// Copyright 2010-2025 Google LLC
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "ortools/sat/implied_bounds.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <array>
#include <bitset>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/log/vlog_is_on.h"
#include "absl/meta/type_traits.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/base/strong_vector.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/sat/clause.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/synchronization.h"
#include "ortools/util/bitset.h"
#include "ortools/util/sorted_interval_list.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {

// Just display some global statistics on destruction.
ImpliedBounds::~ImpliedBounds() {
  if (!VLOG_IS_ON(1)) return;
  if (shared_stats_ == nullptr) return;
  std::vector<std::pair<std::string, int64_t>> stats;
  stats.push_back({"implied_bound/num_deductions", num_deductions_});
  stats.push_back({"implied_bound/num_stored", bounds_.size()});
  stats.push_back({"implied_bound/num_promoted_to_equivalence",
                   num_promoted_to_equivalence_});
  stats.push_back(
      {"implied_bound/num_stored_with_view", num_enqueued_in_var_to_bounds_});
  stats.push_back({"implied_bound/max_changed_domain_complexity",
                   max_changed_domain_complexity_});
  shared_stats_->AddStats(stats);
}

bool ImpliedBounds::Add(Literal literal, IntegerLiteral integer_literal) {
  if (!parameters_.use_implied_bounds()) return true;
  const IntegerVariable var = integer_literal.var;

  // Ignore any Add() with a bound worse than the level zero one.
  // TODO(user): Check that this never happen? it shouldn't.
  const IntegerValue root_lb = integer_trail_->LevelZeroLowerBound(var);
  if (integer_literal.bound <= root_lb) return true;

  if (integer_literal.bound > integer_trail_->LevelZeroUpperBound(var)) {
    // The literal being true is incompatible with the root level bounds.
    sat_solver_->AddClauseDuringSearch({literal.Negated()});
    return true;
  }

  // We skip any IntegerLiteral referring to a variable with only two
  // consecutive possible values. This is because, once shifted this will
  // already be a variable in [0, 1] so we shouldn't gain much by substituing
  // it.
  if (root_lb + 1 >= integer_trail_->LevelZeroUpperBound(var)) return true;

  // Add or update the current bound.
  const auto key = std::make_pair(literal.Index(), var);
  auto insert_result = bounds_.insert({key, integer_literal.bound});
  if (!insert_result.second) {
    if (insert_result.first->second < integer_literal.bound) {
      insert_result.first->second = integer_literal.bound;
    } else {
      // No new info.
      return true;
    }
  }

  // Checks if the variable is now fixed.
  if (integer_trail_->LevelZeroUpperBound(var) == integer_literal.bound) {
    AddLiteralImpliesVarEqValue(literal, var, integer_literal.bound);
  } else {
    const auto it =
        bounds_.find(std::make_pair(literal.Index(), NegationOf(var)));
    if (it != bounds_.end() && it->second == -integer_literal.bound) {
      AddLiteralImpliesVarEqValue(literal, var, integer_literal.bound);
    }
  }

  // Check if we have any deduction. Since at least one of (literal,
  // literal.Negated()) must be true, we can take the min bound as valid at
  // level zero.
  const auto it = bounds_.find(std::make_pair(literal.NegatedIndex(), var));
  if (it != bounds_.end()) {
    if (it->second <= root_lb) {
      // The other bounds is worse than the new level-zero bound which can
      // happen because of lazy update, so here we just remove it.
      bounds_.erase(it);
    } else {
      const IntegerValue deduction =
          std::min(integer_literal.bound, it->second);
      DCHECK_GT(deduction, root_lb);

      ++num_deductions_;
      if (!integer_trail_->RootLevelEnqueue(
              IntegerLiteral::GreaterOrEqual(var, deduction))) {
        return false;
      }

      VLOG(2) << "Deduction old: "
              << IntegerLiteral::GreaterOrEqual(
                     var, integer_trail_->LevelZeroLowerBound(var))
              << " new: " << IntegerLiteral::GreaterOrEqual(var, deduction);

      // The entries that are equal to the min no longer need to be stored once
      // the level zero bound is enqueued.
      if (it->second == deduction) {
        bounds_.erase(it);
      }
      if (integer_literal.bound == deduction) {
        bounds_.erase(std::make_pair(literal.Index(), var));

        // No need to update var_to_bounds_ in this case.
        return true;
      }
      // We already tested this, but enqueueing at root level can make this true
      // again if there are holes in the domain.
      if (integer_literal.bound <= integer_trail_->LevelZeroLowerBound(var)) {
        return true;
      }
    }
  }

  const auto it2 =
      bounds_.find(std::make_pair(literal.NegatedIndex(), NegationOf(var)));
  // If we have "l => (x >= 9)" and "~l => (x <= 6)" we can push
  // "l <=> (x <= 6)" to the encoded integer literals and deduce that [7, 8] is
  // a hole in the domain. More generally, if we have:
  //
  //    l => (x >= a)
  //   ~l => (x <= b)
  //
  // And if moreover b < a, we have the following truth table:
  //
  //   l |   x <= b  |   b < x < a   |   x >= a
  //   --+-----------+---------------+----------
  //   0 |    true   |     false     |   false   (from "~l => (x <= b)")
  //   1 |    false  |     false     |   true    (from " l => (x >= a)")
  //
  //  So we can generalize the expressions to equivalences:
  //    l <=> (x >= a)
  //   ~l <=> (x <= b)
  //    (b < x < a) is impossible (a hole in the domain).
  //
  // TODO(user): understand why we need to restrict to level zero.
  if (it2 != bounds_.end() && -it2->second < integer_literal.bound &&
      sat_solver_->CurrentDecisionLevel() == 0) {
    const IntegerLiteral other_integer_literal =
        IntegerLiteral::GreaterOrEqual(NegationOf(var), it2->second);
    if (integer_encoder_->GetAssociatedLiteral(integer_literal) !=
            literal.Index() ||
        integer_encoder_->GetAssociatedLiteral(other_integer_literal) !=
            literal.NegatedIndex()) {
      ++num_promoted_to_equivalence_;
      integer_encoder_->AssociateToIntegerLiteral(literal, integer_literal);
      integer_encoder_->AssociateToIntegerLiteral(literal.Negated(),
                                                  other_integer_literal);
      const IntegerValue other_bound = -it2->second;
      if (integer_literal.bound - other_bound > 1) {
        const Domain old_domain = integer_trail_->InitialVariableDomain(var);
        const Domain new_domain = old_domain.IntersectionWith(
            Domain(other_bound.value() + 1, integer_literal.bound.value() - 1)
                .Complement());
        max_changed_domain_complexity_ =
            std::max(max_changed_domain_complexity_, new_domain.NumIntervals());
        if (!integer_trail_->UpdateInitialDomain(var, new_domain)) {
          return false;
        }
      }
    }
  }

  // The information below is currently only used for cuts.
  // So no need to store it if we aren't going to use it.
  if (parameters_.linearization_level() == 0) return true;
  if (parameters_.cut_level() == 0) return true;

  // If we have a new implied bound and the literal has a view, add it to
  // var_to_bounds_. Note that we might add more than one entry with the same
  // literal_view, and we will later need to lazily clean the vector up.
  if (integer_encoder_->GetLiteralView(literal) != kNoIntegerVariable) {
    if (var_to_bounds_.size() <= var) {
      var_to_bounds_.resize(var.value() + 1);
      has_implied_bounds_.Resize(var + 1);
    }
    ++num_enqueued_in_var_to_bounds_;
    has_implied_bounds_.Set(var);
    var_to_bounds_[var].emplace_back(integer_encoder_->GetLiteralView(literal),
                                     integer_literal.bound);
  } else if (integer_encoder_->GetLiteralView(literal.Negated()) !=
             kNoIntegerVariable) {
    if (var_to_bounds_.size() <= var) {
      var_to_bounds_.resize(var.value() + 1);
      has_implied_bounds_.Resize(var + 1);
    }
    ++num_enqueued_in_var_to_bounds_;
    has_implied_bounds_.Set(var);
    var_to_bounds_[var].emplace_back(
        NegationOf(integer_encoder_->GetLiteralView(literal.Negated())),
        integer_literal.bound);
  }
  return true;
}

const std::vector<ImpliedBoundEntry>& ImpliedBounds::GetImpliedBounds(
    IntegerVariable var) {
  if (var >= var_to_bounds_.size()) return empty_implied_bounds_;

  // Lazily remove obsolete entries from the vector.
  //
  // TODO(user): Check no duplicate and remove old entry if the enforcement
  // is tighter.
  int new_size = 0;
  std::vector<ImpliedBoundEntry>& ref = var_to_bounds_[var];
  const IntegerValue root_lb = integer_trail_->LevelZeroLowerBound(var);
  for (const ImpliedBoundEntry& entry : ref) {
    if (entry.lower_bound <= root_lb) continue;
    ref[new_size++] = entry;
  }
  ref.resize(new_size);

  return ref;
}

void ImpliedBounds::AddLiteralImpliesVarEqValue(Literal literal,
                                                IntegerVariable var,
                                                IntegerValue value) {
  if (!VariableIsPositive(var)) {
    var = NegationOf(var);
    value = -value;
  }
  literal_to_var_to_value_[literal.Index()][var] = value;
}

bool ImpliedBounds::ProcessIntegerTrail(Literal first_decision) {
  if (!parameters_.use_implied_bounds()) return true;

  CHECK_EQ(sat_solver_->CurrentDecisionLevel(), 1);
  tmp_integer_literals_.clear();
  integer_trail_->AppendNewBounds(&tmp_integer_literals_);
  for (const IntegerLiteral lit : tmp_integer_literals_) {
    if (!Add(first_decision, lit)) return false;
  }
  return true;
}

void ElementEncodings::Add(IntegerVariable var,
                           const std::vector<ValueLiteralPair>& encoding,
                           int exactly_one_index) {
  if (!var_to_index_to_element_encodings_.contains(var)) {
    element_encoded_variables_.push_back(var);
  }
  var_to_index_to_element_encodings_[var][exactly_one_index] = encoding;
}

const absl::btree_map<int, std::vector<ValueLiteralPair>>&
ElementEncodings::Get(IntegerVariable var) {
  const auto& it = var_to_index_to_element_encodings_.find(var);
  if (it == var_to_index_to_element_encodings_.end()) {
    return empty_element_encoding_;
  } else {
    return it->second;
  }
}

const std::vector<IntegerVariable>&
ElementEncodings::GetElementEncodedVariables() const {
  return element_encoded_variables_;
}

// If a variable has a size of 2, it is most likely reduced to an affine
// expression pointing to a variable with domain [0,1] or [-1,0].
// If the original variable has been removed from the model, then there are no
// implied values from any exactly_one constraint to its domain.
// If we are lucky, one of the literal of the exactly_one constraints, and its
// negation are used to encode the Boolean variable of the affine.
//
// This may fail if exactly_one(l0, l1, l2, l3); l0 and l1 imply x = 0,
// l2 and l3 imply x = 1. In that case, one must look at the binary
// implications to find the missing link.
//
// TODO(user): Consider removing this once we are more complete in our implied
// bounds repository. Because if we can reconcile an encoding, then any of the
// literal in the at most one should imply a value on the boolean view use in
// the size2 affine.
std::vector<LiteralValueValue> TryToReconcileEncodings(
    const AffineExpression& size2_affine, const AffineExpression& affine,
    absl::Span<const ValueLiteralPair> affine_var_encoding,
    bool put_affine_left_in_result, IntegerEncoder* integer_encoder) {
  IntegerVariable binary = size2_affine.var;
  std::vector<LiteralValueValue> terms;
  if (!integer_encoder->VariableIsFullyEncoded(binary)) return terms;
  const std::vector<ValueLiteralPair>& size2_enc =
      integer_encoder->FullDomainEncoding(binary);

  // TODO(user): I am not sure how this can happen since size2_affine is
  // supposed to be non-fixed. Maybe we miss some propag. Investigate.
  if (size2_enc.size() != 2) return terms;

  Literal lit0 = size2_enc[0].literal;
  IntegerValue value0 = size2_affine.ValueAt(size2_enc[0].value);
  Literal lit1 = size2_enc[1].literal;
  IntegerValue value1 = size2_affine.ValueAt(size2_enc[1].value);

  for (const auto& [unused, candidate_literal] : affine_var_encoding) {
    if (candidate_literal == lit1) {
      std::swap(lit0, lit1);
      std::swap(value0, value1);
    }
    if (candidate_literal != lit0) continue;

    // Build the decomposition.
    for (const auto& [value, literal] : affine_var_encoding) {
      const IntegerValue size_2_value = literal == lit0 ? value0 : value1;
      const IntegerValue affine_value = affine.ValueAt(value);
      if (put_affine_left_in_result) {
        terms.push_back({literal, affine_value, size_2_value});
      } else {
        terms.push_back({literal, size_2_value, affine_value});
      }
    }
    break;
  }

  return terms;
}

// Specialized case of encoding reconciliation when both variables have a domain
// of size of 2.
std::vector<LiteralValueValue> TryToReconcileSize2Encodings(
    const AffineExpression& left, const AffineExpression& right,
    IntegerEncoder* integer_encoder) {
  std::vector<LiteralValueValue> terms;
  if (!integer_encoder->VariableIsFullyEncoded(left.var) ||
      !integer_encoder->VariableIsFullyEncoded(right.var)) {
    return terms;
  }
  const std::vector<ValueLiteralPair> left_enc =
      integer_encoder->FullDomainEncoding(left.var);
  const std::vector<ValueLiteralPair> right_enc =
      integer_encoder->FullDomainEncoding(right.var);
  if (left_enc.size() != 2 || right_enc.size() != 2) {
    VLOG(2) << "encodings are not fully propagated";
    return terms;
  }

  const Literal left_lit0 = left_enc[0].literal;
  const IntegerValue left_value0 = left.ValueAt(left_enc[0].value);
  const Literal left_lit1 = left_enc[1].literal;
  const IntegerValue left_value1 = left.ValueAt(left_enc[1].value);

  const Literal right_lit0 = right_enc[0].literal;
  const IntegerValue right_value0 = right.ValueAt(right_enc[0].value);
  const Literal right_lit1 = right_enc[1].literal;
  const IntegerValue right_value1 = right.ValueAt(right_enc[1].value);

  if (left_lit0 == right_lit0 || left_lit0 == right_lit1.Negated()) {
    terms.push_back({left_lit0, left_value0, right_value0});
    terms.push_back({left_lit0.Negated(), left_value1, right_value1});
  } else if (left_lit0 == right_lit1 || left_lit0 == right_lit0.Negated()) {
    terms.push_back({left_lit0, left_value0, right_value1});
    terms.push_back({left_lit0.Negated(), left_value1, right_value0});
  } else if (left_lit1 == right_lit1 || left_lit1 == right_lit0.Negated()) {
    terms.push_back({left_lit1.Negated(), left_value0, right_value0});
    terms.push_back({left_lit1, left_value1, right_value1});
  } else if (left_lit1 == right_lit0 || left_lit1 == right_lit1.Negated()) {
    terms.push_back({left_lit1.Negated(), left_value0, right_value1});
    terms.push_back({left_lit1, left_value1, right_value0});
  } else {
    VLOG(3) << "Complex size 2 encoding case, need to scan exactly_ones";
  }

  return terms;
}

std::vector<LiteralValueValue> ProductDecomposer::TryToDecompose(
    const AffineExpression& left, const AffineExpression& right) {
  if (integer_trail_->IsFixed(left) || integer_trail_->IsFixed(right)) {
    return {};
  }

  // Fill in the encodings for the left variable.
  const absl::btree_map<int, std::vector<ValueLiteralPair>>& left_encodings =
      element_encodings_->Get(left.var);

  // Fill in the encodings for the right variable.
  const absl::btree_map<int, std::vector<ValueLiteralPair>>& right_encodings =
      element_encodings_->Get(right.var);

  std::vector<int> compatible_keys;
  for (const auto& [index, encoding] : left_encodings) {
    if (right_encodings.contains(index)) {
      compatible_keys.push_back(index);
    }
  }

  if (compatible_keys.empty()) {
    if (integer_trail_->InitialVariableDomain(left.var).Size() == 2) {
      for (const auto& [index, right_encoding] : right_encodings) {
        const std::vector<LiteralValueValue> result = TryToReconcileEncodings(
            left, right, right_encoding,
            /*put_affine_left_in_result=*/false, integer_encoder_);
        if (!result.empty()) {
          return result;
        }
      }
    }
    if (integer_trail_->InitialVariableDomain(right.var).Size() == 2) {
      for (const auto& [index, left_encoding] : left_encodings) {
        const std::vector<LiteralValueValue> result = TryToReconcileEncodings(
            right, left, left_encoding,
            /*put_affine_left_in_result=*/true, integer_encoder_);
        if (!result.empty()) {
          return result;
        }
      }
    }
    if (integer_trail_->InitialVariableDomain(left.var).Size() == 2 &&
        integer_trail_->InitialVariableDomain(right.var).Size() == 2) {
      const std::vector<LiteralValueValue> result =
          TryToReconcileSize2Encodings(left, right, integer_encoder_);
      if (!result.empty()) {
        return result;
      }
    }
    return {};
  }

  if (compatible_keys.size() > 1) {
    VLOG(3) << "More than one exactly_one involved in the encoding of the two "
               "variables";
  }

  // Select the compatible encoding with the minimum index.
  const int min_index =
      *std::min_element(compatible_keys.begin(), compatible_keys.end());
  // By construction, encodings follow the order of literals in the exactly_one
  // constraint.
  const std::vector<ValueLiteralPair>& left_encoding =
      left_encodings.at(min_index);
  const std::vector<ValueLiteralPair>& right_encoding =
      right_encodings.at(min_index);
  DCHECK_EQ(left_encoding.size(), right_encoding.size());

  // Build decomposition of the product.
  std::vector<LiteralValueValue> terms;
  for (int i = 0; i < left_encoding.size(); ++i) {
    const Literal literal = left_encoding[i].literal;
    DCHECK_EQ(literal, right_encoding[i].literal);
    terms.push_back({literal, left.ValueAt(left_encoding[i].value),
                     right.ValueAt(right_encoding[i].value)});
  }

  return terms;
}

// TODO(user): Experiment with x * x where constants = 0, x is
// fully encoded, and the domain is small.
bool ProductDecomposer::TryToLinearize(const AffineExpression& left,
                                       const AffineExpression& right,
                                       LinearConstraintBuilder* builder) {
  DCHECK(builder != nullptr);
  builder->Clear();

  if (integer_trail_->IsFixed(left)) {
    if (integer_trail_->IsFixed(right)) {
      builder->AddConstant(integer_trail_->FixedValue(left) *
                           integer_trail_->FixedValue(right));
      return true;
    }
    builder->AddTerm(right, integer_trail_->FixedValue(left));
    return true;
  }

  if (integer_trail_->IsFixed(right)) {
    builder->AddTerm(left, integer_trail_->FixedValue(right));
    return true;
  }

  // Linearization is possible if both left and right have the same Boolean
  // variable.
  if (PositiveVariable(left.var) == PositiveVariable(right.var) &&
      integer_trail_->LowerBound(PositiveVariable(left.var)) == 0 &&
      integer_trail_->UpperBound(PositiveVariable(left.var)) == 1) {
    const IntegerValue left_coeff =
        VariableIsPositive(left.var) ? left.coeff : -left.coeff;
    const IntegerValue right_coeff =
        VariableIsPositive(right.var) ? right.coeff : -right.coeff;
    builder->AddTerm(PositiveVariable(left.var),
                     left_coeff * right_coeff + left.constant * right_coeff +
                         left_coeff * right.constant);
    builder->AddConstant(left.constant * right.constant);
    return true;
  }

  const std::vector<LiteralValueValue> decomposition =
      TryToDecompose(left, right);
  if (decomposition.empty()) return false;

  IntegerValue min_coefficient = kMaxIntegerValue;
  for (const LiteralValueValue& term : decomposition) {
    min_coefficient =
        std::min(min_coefficient, term.left_value * term.right_value);
  }
  for (const LiteralValueValue& term : decomposition) {
    const IntegerValue coefficient =
        term.left_value * term.right_value - min_coefficient;
    if (coefficient == 0) continue;
    if (!builder->AddLiteralTerm(term.literal, coefficient)) {
      return false;
    }
  }
  builder->AddConstant(min_coefficient);
  return true;
}

ProductDetector::ProductDetector(Model* model)
    : enabled_(
          model->GetOrCreate<SatParameters>()->detect_linearized_product() &&
          model->GetOrCreate<SatParameters>()->linearization_level() > 1),
      rlt_enabled_(model->GetOrCreate<SatParameters>()->add_rlt_cuts() &&
                   model->GetOrCreate<SatParameters>()->linearization_level() >
                       1),
      sat_solver_(model->GetOrCreate<SatSolver>()),
      trail_(model->GetOrCreate<Trail>()),
      integer_trail_(model->GetOrCreate<IntegerTrail>()),
      integer_encoder_(model->GetOrCreate<IntegerEncoder>()),
      shared_stats_(model->GetOrCreate<SharedStatistics>()) {}

ProductDetector::~ProductDetector() {
  if (!VLOG_IS_ON(1)) return;
  if (shared_stats_ == nullptr) return;
  std::vector<std::pair<std::string, int64_t>> stats;
  stats.push_back(
      {"product_detector/num_processed_binary", num_processed_binary_});
  stats.push_back(
      {"product_detector/num_processed_exactly_one", num_processed_exo_});
  stats.push_back(
      {"product_detector/num_processed_ternary", num_processed_ternary_});
  stats.push_back({"product_detector/num_trail_updates", num_trail_updates_});
  stats.push_back({"product_detector/num_products", num_products_});
  stats.push_back({"product_detector/num_conditional_equalities",
                   num_conditional_equalities_});
  stats.push_back(
      {"product_detector/num_conditional_zeros", num_conditional_zeros_});
  stats.push_back({"product_detector/num_int_products", num_int_products_});
  shared_stats_->AddStats(stats);
}

void ProductDetector::ProcessTernaryClause(
    absl::Span<const Literal> ternary_clause) {
  if (ternary_clause.size() != 3) return;
  ++num_processed_ternary_;

  if (rlt_enabled_) ProcessTernaryClauseForRLT(ternary_clause);
  if (!enabled_) return;

  candidates_[GetKey(ternary_clause[0].Index(), ternary_clause[1].Index())]
      .push_back(ternary_clause[2].Index());
  candidates_[GetKey(ternary_clause[0].Index(), ternary_clause[2].Index())]
      .push_back(ternary_clause[1].Index());
  candidates_[GetKey(ternary_clause[1].Index(), ternary_clause[2].Index())]
      .push_back(ternary_clause[0].Index());

  // We mark the literal of the ternary clause as seen.
  // Only a => b with a seen need to be looked at.
  for (const Literal l : ternary_clause) {
    if (l.Index() >= seen_.size()) seen_.resize(l.Index() + 1);
    seen_[l.Index()] = true;
  }
}

// If all have view, add to flat representation.
void ProductDetector::ProcessTernaryClauseForRLT(
    absl::Span<const Literal> ternary_clause) {
  const int old_size = ternary_clauses_with_view_.size();
  for (const Literal l : ternary_clause) {
    const IntegerVariable var =
        integer_encoder_->GetLiteralView(Literal(l.Variable(), true));
    if (var == kNoIntegerVariable || !VariableIsPositive(var)) {
      ternary_clauses_with_view_.resize(old_size);
      return;
    }
    ternary_clauses_with_view_.push_back(l.IsPositive() ? var
                                                        : NegationOf(var));
  }
}

void ProductDetector::ProcessTernaryExactlyOne(
    absl::Span<const Literal> ternary_exo) {
  if (ternary_exo.size() != 3) return;
  ++num_processed_exo_;

  if (rlt_enabled_) ProcessTernaryClauseForRLT(ternary_exo);
  if (!enabled_) return;

  ProcessNewProduct(ternary_exo[0].Index(), ternary_exo[1].NegatedIndex(),
                    ternary_exo[2].NegatedIndex());
  ProcessNewProduct(ternary_exo[1].Index(), ternary_exo[0].NegatedIndex(),
                    ternary_exo[2].NegatedIndex());
  ProcessNewProduct(ternary_exo[2].Index(), ternary_exo[0].NegatedIndex(),
                    ternary_exo[1].NegatedIndex());
}

// TODO(user): As product are discovered, we could remove entries from our
// hash maps!
void ProductDetector::ProcessBinaryClause(
    absl::Span<const Literal> binary_clause) {
  if (!enabled_) return;
  if (binary_clause.size() != 2) return;
  ++num_processed_binary_;
  const std::array<LiteralIndex, 2> key =
      GetKey(binary_clause[0].NegatedIndex(), binary_clause[1].NegatedIndex());
  std::array<LiteralIndex, 3> ternary;
  for (const LiteralIndex l : candidates_[key]) {
    ternary[0] = key[0];
    ternary[1] = key[1];
    ternary[2] = l;
    std::sort(ternary.begin(), ternary.end());
    const int l_index = ternary[0] == l ? 0 : ternary[1] == l ? 1 : 2;
    std::bitset<3>& bs = detector_[ternary];
    if (bs[l_index]) continue;
    bs[l_index] = true;
    if (bs[0] && bs[1] && l_index != 2) {
      ProcessNewProduct(ternary[2], Literal(ternary[0]).NegatedIndex(),
                        Literal(ternary[1]).NegatedIndex());
    }
    if (bs[0] && bs[2] && l_index != 1) {
      ProcessNewProduct(ternary[1], Literal(ternary[0]).NegatedIndex(),
                        Literal(ternary[2]).NegatedIndex());
    }
    if (bs[1] && bs[2] && l_index != 0) {
      ProcessNewProduct(ternary[0], Literal(ternary[1]).NegatedIndex(),
                        Literal(ternary[2]).NegatedIndex());
    }
  }
}

void ProductDetector::ProcessImplicationGraph(BinaryImplicationGraph* graph) {
  if (!enabled_) return;
  for (LiteralIndex a(0); a < seen_.size(); ++a) {
    if (!seen_[a]) continue;
    if (trail_->Assignment().LiteralIsAssigned(Literal(a))) continue;
    const Literal not_a = Literal(a).Negated();
    for (const Literal b : graph->DirectImplications(Literal(a))) {
      ProcessBinaryClause({not_a, b});  // a => b;
    }
  }
}

void ProductDetector::ProcessTrailAtLevelOne() {
  if (!enabled_) return;
  if (trail_->CurrentDecisionLevel() != 1) return;
  ++num_trail_updates_;

  const LiteralWithTrailIndex decision = trail_->Decisions()[0];
  if (decision.literal.Index() >= seen_.size() ||
      !seen_[decision.literal.Index()]) {
    return;
  }
  const Literal not_a = decision.literal.Negated();
  const int current_index = trail_->Index();
  for (int i = decision.trail_index + 1; i < current_index; ++i) {
    const Literal b = (*trail_)[i];
    ProcessBinaryClause({not_a, b});
  }
}

LiteralIndex ProductDetector::GetProduct(Literal a, Literal b) const {
  const auto it = products_.find(GetKey(a.Index(), b.Index()));
  if (it == products_.end()) return kNoLiteralIndex;
  return it->second;
}

std::array<LiteralIndex, 2> ProductDetector::GetKey(LiteralIndex a,
                                                    LiteralIndex b) const {
  std::array<LiteralIndex, 2> key{a, b};
  if (key[0] > key[1]) std::swap(key[0], key[1]);
  return key;
}

void ProductDetector::ProcessNewProduct(LiteralIndex p, LiteralIndex a,
                                        LiteralIndex b) {
  // If many literal correspond to the same product, we just keep one.
  ++num_products_;
  products_[GetKey(a, b)] = p;

  // This is used by ProductIsLinearizable().
  has_product_.insert(
      GetKey(Literal(a).IsPositive() ? a : Literal(a).NegatedIndex(),
             Literal(b).IsPositive() ? b : Literal(b).NegatedIndex()));
}

bool ProductDetector::ProductIsLinearizable(IntegerVariable a,
                                            IntegerVariable b) const {
  if (a == b) return true;
  if (a == NegationOf(b)) return true;

  // Otherwise, we need both a and b to be expressible as linear expression
  // involving Booleans whose product is also expressible.
  if (integer_trail_->InitialVariableDomain(a).Size() != 2) return false;
  if (integer_trail_->InitialVariableDomain(b).Size() != 2) return false;

  const LiteralIndex la =
      integer_encoder_->GetAssociatedLiteral(IntegerLiteral::GreaterOrEqual(
          a, integer_trail_->LevelZeroUpperBound(a)));
  if (la == kNoLiteralIndex) return false;

  const LiteralIndex lb =
      integer_encoder_->GetAssociatedLiteral(IntegerLiteral::GreaterOrEqual(
          b, integer_trail_->LevelZeroUpperBound(b)));
  if (lb == kNoLiteralIndex) return false;

  // Any product involving la/not(la) * lb/not(lb) can be used.
  return has_product_.contains(
      GetKey(Literal(la).IsPositive() ? la : Literal(la).NegatedIndex(),
             Literal(lb).IsPositive() ? lb : Literal(lb).NegatedIndex()));
}

IntegerVariable ProductDetector::GetProduct(Literal a,
                                            IntegerVariable b) const {
  const auto it = int_products_.find({a.Index(), PositiveVariable(b)});
  if (it == int_products_.end()) return kNoIntegerVariable;
  return VariableIsPositive(b) ? it->second : NegationOf(it->second);
}

void ProductDetector::ProcessNewProduct(IntegerVariable p, Literal l,
                                        IntegerVariable x) {
  if (!VariableIsPositive(x)) {
    x = NegationOf(x);
    p = NegationOf(p);
  }

  // We only store one product if there are many.
  ++num_int_products_;
  int_products_[{l.Index(), x}] = p;
}

void ProductDetector::ProcessConditionalEquality(Literal l, IntegerVariable x,
                                                 IntegerVariable y) {
  ++num_conditional_equalities_;
  if (x == y) return;

  // We process both possibilities (product = x or product = y).
  for (int i = 0; i < 2; ++i) {
    if (!VariableIsPositive(x)) {
      x = NegationOf(x);
      y = NegationOf(y);
    }
    bool seen = false;

    // TODO(user): Linear scan can be bad if b => X = many other variables.
    // Hopefully this will not be common.
    std::vector<IntegerVariable>& others =
        conditional_equalities_[{l.Index(), x}];
    for (const IntegerVariable o : others) {
      if (o == y) {
        seen = true;
        break;
      }
    }

    if (!seen) {
      others.push_back(y);
      if (conditional_zeros_.contains({l.NegatedIndex(), x})) {
        ProcessNewProduct(/*p=*/x, l, y);
      }
    }
    std::swap(x, y);
  }
}

void ProductDetector::ProcessConditionalZero(Literal l, IntegerVariable p) {
  ++num_conditional_zeros_;
  p = PositiveVariable(p);
  auto [_, inserted] = conditional_zeros_.insert({l.Index(), p});
  if (inserted) {
    const auto it = conditional_equalities_.find({l.NegatedIndex(), p});
    if (it != conditional_equalities_.end()) {
      for (const IntegerVariable x : it->second) {
        ProcessNewProduct(p, l.Negated(), x);
      }
    }
  }
}

namespace {

std::pair<IntegerVariable, IntegerVariable> Canonicalize(IntegerVariable a,
                                                         IntegerVariable b) {
  if (a < b) return {a, b};
  return {b, a};
}

double GetLiteralLpValue(
    IntegerVariable var,
    const util_intops::StrongVector<IntegerVariable, double>& lp_values) {
  return VariableIsPositive(var) ? lp_values[var]
                                 : 1.0 - lp_values[PositiveVariable(var)];
}

}  // namespace

void ProductDetector::UpdateRLTMaps(
    const util_intops::StrongVector<IntegerVariable, double>& lp_values,
    IntegerVariable var1, double lp1, IntegerVariable var2, double lp2,
    IntegerVariable bound_var, double bound_lp) {
  // we have var1 * var2 <= bound_var, and this is only useful if it is better
  // than the trivial bound <= var1 or <= var2.
  if (bound_lp > lp1 && bound_lp > lp2) return;

  const auto [it, inserted] =
      bool_rlt_ubs_.insert({Canonicalize(var1, var2), bound_var});

  // Replace if better.
  if (!inserted && bound_lp < GetLiteralLpValue(it->second, lp_values)) {
    it->second = bound_var;
  }

  // This will increase a RLT cut violation and is a good candidate.
  if (lp1 * lp2 > bound_lp + 1e-4) {
    bool_rlt_candidates_[var1].push_back(var2);
    bool_rlt_candidates_[var2].push_back(var1);
  }
}

// TODO(user): limit work if too many ternary.
void ProductDetector::InitializeBooleanRLTCuts(
    absl::Span<const IntegerVariable> lp_vars,
    const util_intops::StrongVector<IntegerVariable, double>& lp_values) {
  // TODO(user): Maybe we shouldn't reconstruct this every time, but it is hard
  // in case of multiple lps to make sure we don't use variables not in the lp
  // otherwise.
  bool_rlt_ubs_.clear();

  // If we transform a linear constraint to sum positive_coeff * bool <= rhs.
  // We will list all interesting multiplicative candidate for each variable.
  bool_rlt_candidates_.clear();
  const int size = ternary_clauses_with_view_.size();
  if (size == 0) return;

  is_in_lp_vars_.resize(integer_trail_->NumIntegerVariables().value());
  for (const IntegerVariable var : lp_vars) is_in_lp_vars_.Set(var);

  for (int i = 0; i < size; i += 3) {
    const IntegerVariable var1 = ternary_clauses_with_view_[i];
    const IntegerVariable var2 = ternary_clauses_with_view_[i + 1];
    const IntegerVariable var3 = ternary_clauses_with_view_[i + 2];

    if (!is_in_lp_vars_[PositiveVariable(var1)]) continue;
    if (!is_in_lp_vars_[PositiveVariable(var2)]) continue;
    if (!is_in_lp_vars_[PositiveVariable(var3)]) continue;

    // If we have l1 + l2 + l3 >= 1, then for all (i, j) pair we have
    // !li * !lj <= lk. We are looking for violation like this.
    const double lp1 = GetLiteralLpValue(var1, lp_values);
    const double lp2 = GetLiteralLpValue(var2, lp_values);
    const double lp3 = GetLiteralLpValue(var3, lp_values);

    UpdateRLTMaps(lp_values, NegationOf(var1), 1.0 - lp1, NegationOf(var2),
                  1.0 - lp2, var3, lp3);
    UpdateRLTMaps(lp_values, NegationOf(var1), 1.0 - lp1, NegationOf(var3),
                  1.0 - lp3, var2, lp2);
    UpdateRLTMaps(lp_values, NegationOf(var2), 1.0 - lp2, NegationOf(var3),
                  1.0 - lp3, var1, lp1);
  }

  // Clear.
  // TODO(user): Just switch to memclear() when dense.
  for (const IntegerVariable var : lp_vars) is_in_lp_vars_.ClearBucket(var);
}

}  // namespace sat
}  // namespace operations_research
