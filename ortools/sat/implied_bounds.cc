// Copyright 2010-2021 Google LLC
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

#include <algorithm>
#include <limits>

#include "ortools/sat/integer.h"
#include "ortools/sat/linear_constraint.h"

namespace operations_research {
namespace sat {

// Just display some global statistics on destruction.
ImpliedBounds::~ImpliedBounds() {
  VLOG(1) << num_deductions_ << " enqueued deductions.";
  VLOG(1) << bounds_.size() << " implied bounds stored.";
  VLOG(1) << num_enqueued_in_var_to_bounds_
          << " implied bounds with view stored.";
}

void ImpliedBounds::Add(Literal literal, IntegerLiteral integer_literal) {
  if (!parameters_.use_implied_bounds()) return;
  const IntegerVariable var = integer_literal.var;

  // Update our local level-zero bound.
  if (var >= level_zero_lower_bounds_.size()) {
    level_zero_lower_bounds_.resize(var.value() + 1, kMinIntegerValue);
    new_level_zero_bounds_.Resize(var + 1);
  }
  level_zero_lower_bounds_[var] = std::max(
      level_zero_lower_bounds_[var], integer_trail_->LevelZeroLowerBound(var));

  // Ignore any Add() with a bound worse than the level zero one.
  // TODO(user): Check that this never happen? it shouldn't.
  if (integer_literal.bound <= level_zero_lower_bounds_[var]) {
    return;
  }

  // We skip any IntegerLiteral referring to a variable with only two
  // consecutive possible values. This is because, once shifted this will
  // already be a variable in [0, 1] so we shouldn't gain much by substituing
  // it.
  if (integer_trail_->LevelZeroLowerBound(var) + 1 >=
      integer_trail_->LevelZeroUpperBound(var)) {
    return;
  }

  // Add or update the current bound.
  const auto key = std::make_pair(literal.Index(), var);
  auto insert_result = bounds_.insert({key, integer_literal.bound});
  if (!insert_result.second) {
    if (insert_result.first->second < integer_literal.bound) {
      insert_result.first->second = integer_literal.bound;
    } else {
      // No new info.
      return;
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
  //
  // TODO(user): Like in probing, we can also create hole in the domain if there
  // is some implied bounds for (literal.NegatedIndex, NegagtionOf(var)) that
  // crosses integer_literal.bound.
  const auto it = bounds_.find(std::make_pair(literal.NegatedIndex(), var));
  if (it != bounds_.end()) {
    if (it->second <= level_zero_lower_bounds_[var]) {
      // The other bounds is worse than the new level-zero bound which can
      // happen because of lazy update, so here we just remove it.
      bounds_.erase(it);
    } else {
      const IntegerValue deduction =
          std::min(integer_literal.bound, it->second);
      DCHECK_GT(deduction, level_zero_lower_bounds_[var]);
      DCHECK_GT(deduction, integer_trail_->LevelZeroLowerBound(var));

      // TODO(user): support Enqueueing level zero fact at a positive level.
      // That is, do not loose the info on backtrack. This should be doable. It
      // is also why we return a bool in case of conflict when pushing
      // deduction.
      ++num_deductions_;
      level_zero_lower_bounds_[var] = deduction;
      new_level_zero_bounds_.Set(var);

      VLOG(1) << "Deduction old: "
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
        return;
      }
    }
  }

  // While the code above deal correctly with optionality, we cannot just
  // register a literal => bound for an optional variable, because the equation
  // might end up in the LP which do not handle them correctly.
  //
  // TODO(user): Maybe we can handle this case somehow, as long as every
  // constraint using this bound is protected by the variable optional literal.
  // Alternativelly we could disable optional variable when we are at
  // linearization level 2.
  if (integer_trail_->IsOptional(var)) return;

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
    var_to_bounds_[var].push_back({integer_encoder_->GetLiteralView(literal),
                                   integer_literal.bound, true});
  } else if (integer_encoder_->GetLiteralView(literal.Negated()) !=
             kNoIntegerVariable) {
    if (var_to_bounds_.size() <= var) {
      var_to_bounds_.resize(var.value() + 1);
      has_implied_bounds_.Resize(var + 1);
    }
    ++num_enqueued_in_var_to_bounds_;
    has_implied_bounds_.Set(var);
    var_to_bounds_[var].push_back(
        {integer_encoder_->GetLiteralView(literal.Negated()),
         integer_literal.bound, false});
  }
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
  const IntegerValue level_zero_lb = std::max(
      level_zero_lower_bounds_[var], integer_trail_->LevelZeroLowerBound(var));
  level_zero_lower_bounds_[var] = level_zero_lb;
  for (const ImpliedBoundEntry& entry : ref) {
    if (entry.lower_bound <= level_zero_lb) continue;
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

void ImpliedBounds::ProcessIntegerTrail(Literal first_decision) {
  if (!parameters_.use_implied_bounds()) return;

  CHECK_EQ(sat_solver_->CurrentDecisionLevel(), 1);
  tmp_integer_literals_.clear();
  integer_trail_->AppendNewBounds(&tmp_integer_literals_);
  for (const IntegerLiteral lit : tmp_integer_literals_) {
    Add(first_decision, lit);
  }
}

void ImpliedBounds::AddElementEncoding(
    IntegerVariable var, const std::vector<ValueLiteralPair>& encoding,
    int exactly_one_index) {
  var_to_index_to_element_encodings_[var][exactly_one_index] = encoding;
}

const absl::flat_hash_map<int, std::vector<ValueLiteralPair>>&
ImpliedBounds::GetElementEncodings(IntegerVariable var) {
  const auto& it = var_to_index_to_element_encodings_.find(var);
  if (it == var_to_index_to_element_encodings_.end()) {
    return empty_element_encoding_;
  } else {
    return it->second;
  }
}

const std::vector<IntegerVariable>& ImpliedBounds::GetElementEncodedVariables()
    const {
  return element_encoded_variables_;
}

bool ImpliedBounds::EnqueueNewDeductions() {
  CHECK_EQ(sat_solver_->CurrentDecisionLevel(), 0);
  for (const IntegerVariable var :
       new_level_zero_bounds_.PositionsSetAtLeastOnce()) {
    if (!integer_trail_->Enqueue(
            IntegerLiteral::GreaterOrEqual(var, level_zero_lower_bounds_[var]),
            {}, {})) {
      return false;
    }
  }
  new_level_zero_bounds_.SparseClearAll();
  return sat_solver_->FinishPropagation();
}

std::string EncodingStr(const std::vector<ValueLiteralPair>& enc) {
  std::string result;
  for (const ValueLiteralPair& term : enc) {
    absl::StrAppend(&result, term.literal.DebugString(), ":",
                    term.value.value(), " ");
  }
  return result;
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
bool TryToReconcileEncodings(
    const AffineExpression& size2_affine, const AffineExpression& affine,
    const std::vector<ValueLiteralPair>& affine_var_encoding, Model* model,
    LinearConstraintBuilder* builder) {
  IntegerEncoder* integer_encoder = model->GetOrCreate<IntegerEncoder>();
  IntegerVariable binary = size2_affine.var;
  if (!integer_encoder->VariableIsFullyEncoded(binary)) return false;
  const std::vector<ValueLiteralPair>& size2_enc =
      integer_encoder->FullDomainEncoding(binary);
  CHECK_EQ(2, size2_enc.size());
  Literal lit0 = size2_enc[0].literal;
  IntegerValue value0 =
      size2_enc[0].value * size2_affine.coeff + size2_affine.constant;
  Literal lit1 = size2_enc[1].literal;
  IntegerValue value1 =
      size2_enc[1].value * size2_affine.coeff + size2_affine.constant;
  for (const auto& [unused, candidate_literal] : affine_var_encoding) {
    if (candidate_literal == lit1) {
      std::swap(lit0, lit1);
      std::swap(value0, value1);
    }
    if (candidate_literal != lit0) continue;

    // Compute the minimum energy.
    IntegerValue min_energy = kMaxIntegerValue;
    for (const auto& [value, literal] : affine_var_encoding) {
      const IntegerValue energy = literal == lit0
                                      ? value0 * affine.ValueAt(value)
                                      : value1 * affine.ValueAt(value);
      min_energy = std::min(energy, min_energy);
    }

    // Build the energy expression.
    builder->Clear();
    builder->AddConstant(min_energy);
    for (const auto& [value, literal] : affine_var_encoding) {
      const IntegerValue energy = literal == lit0
                                      ? value0 * affine.ValueAt(value)
                                      : value1 * affine.ValueAt(value);
      if (energy > min_energy) {
        if (!builder->AddLiteralTerm(literal, energy - min_energy)) {
          return false;
        }
      }
    }
    return true;
  }

  return false;
}

// TODO(user): Experiment with x * x where constants = 0, x is
// fully encoded, and the domain is small.
bool DetectLinearEncodingOfProducts(const AffineExpression& left,
                                    const AffineExpression& right, Model* model,
                                    LinearConstraintBuilder* builder) {
  CHECK(builder != nullptr);
  builder->Clear();

  IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
  ImpliedBounds* implied_bounds = model->GetOrCreate<ImpliedBounds>();

  if (integer_trail->IsFixed(left)) {
    const IntegerValue value = integer_trail->FixedValue(left);
    builder->AddTerm(right, value);
    return true;
  }

  if (integer_trail->IsFixed(right)) {
    const IntegerValue value = integer_trail->FixedValue(right);
    builder->AddTerm(left, value);
    return true;
  }

  // Linearization is possible if both left and right have the same Boolean
  // variable.
  if (PositiveVariable(left.var) == PositiveVariable(right.var) &&
      integer_trail->LowerBound(PositiveVariable(left.var)) == 0 &&
      integer_trail->UpperBound(PositiveVariable(left.var)) == 1) {
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

  // Fill in the encodings for the left variable.
  const absl::flat_hash_map<int, std::vector<ValueLiteralPair>>&
      left_encodings = implied_bounds->GetElementEncodings(left.var);

  // Fill in the encodings for the right variable.
  const absl::flat_hash_map<int, std::vector<ValueLiteralPair>>&
      right_encodings = implied_bounds->GetElementEncodings(right.var);

  std::vector<int> compatible_keys;
  for (const auto& [index, encoding] : left_encodings) {
    if (right_encodings.contains(index)) {
      compatible_keys.push_back(index);
    }
  }

  if (compatible_keys.empty()) {
    if (integer_trail->InitialVariableDomain(left.var).Size() == 2) {
      for (const auto& [index, right_encoding] : right_encodings) {
        if (TryToReconcileEncodings(left, right, right_encoding, model,
                                    builder)) {
          return true;
        }
      }
    }
    if (integer_trail->InitialVariableDomain(right.var).Size() == 2) {
      for (const auto& [index, left_encoding] : left_encodings) {
        if (TryToReconcileEncodings(right, left, left_encoding, model,
                                    builder)) {
          return true;
        }
      }
    }
    return false;
  }

  if (compatible_keys.size() > 1) {
    VLOG(1) << "More than one exactly_one involved in the encoding of the two "
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

  // Compute the min energy.
  IntegerValue min_energy = kMaxIntegerValue;
  for (int i = 0; i < left_encoding.size(); ++i) {
    const IntegerValue energy = left.ValueAt(left_encoding[i].value) *
                                right.ValueAt(right_encoding[i].value);
    min_energy = std::min(min_energy, energy);
  }

  // Build the linear formulation of the energy.
  for (int i = 0; i < left_encoding.size(); ++i) {
    const IntegerValue energy = left.ValueAt(left_encoding[i].value) *
                                right.ValueAt(right_encoding[i].value);
    if (energy == min_energy) continue;
    DCHECK_GT(energy, min_energy);
    const Literal lit = left_encoding[i].literal;
    DCHECK_EQ(lit, right_encoding[i].literal);

    if (!builder->AddLiteralTerm(lit, energy - min_energy)) {
      return false;
    }
  }
  builder->AddConstant(min_energy);
  return true;
}

LinearExpression NotLinearizedEnergy() {
  LinearExpression result;
  result.offset = std::numeric_limits<int64_t>::min();
  return result;
}

bool ProductIsLinearized(const LinearExpression& expr) {
  return !expr.vars.empty() ||
         expr.offset >= std::numeric_limits<int64_t>::min();
}

void LinearizeInnerProduct(const std::vector<AffineExpression>& left,
                           const std::vector<AffineExpression>& right,
                           Model* model,
                           std::vector<LinearExpression>* energies) {
  auto* integer_trail = model->GetOrCreate<IntegerTrail>();
  for (int i = 0; i < left.size(); ++i) {
    LinearConstraintBuilder builder(model);
    if (DetectLinearEncodingOfProducts(left[i], right[i], model, &builder)) {
      VLOG(3) << "linearized energy: "
              << builder.BuildExpression().DebugString();
      energies->push_back(builder.BuildExpression());
    } else {
      VLOG(2) << "Product is not linearizable: demands "
              << left[i].DebugString() << " with var domain "
              << integer_trail->InitialVariableDomain(left[i].var)
              << ", size = " << right[i].DebugString() << " with var domain "
              << integer_trail->InitialVariableDomain(right[i].var);
      energies->push_back(NotLinearizedEnergy());
    }
  }
}

}  // namespace sat
}  // namespace operations_research
