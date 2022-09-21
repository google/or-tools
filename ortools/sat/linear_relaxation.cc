// Copyright 2010-2022 Google LLC
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

#include "ortools/sat/linear_relaxation.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_set.h"
#include "ortools/base/logging.h"
#include "ortools/base/stl_util.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/circuit.h"  // for ReindexArcs.
#include "ortools/sat/clause.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_mapping.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/cuts.h"
#include "ortools/sat/implied_bounds.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_expr.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/linear_programming_constraint.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/scheduling_constraints.h"
#include "ortools/sat/scheduling_cuts.h"
#include "ortools/util/logging.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {

bool AppendFullEncodingRelaxation(IntegerVariable var, const Model& model,
                                  LinearRelaxation* relaxation) {
  const auto* encoder = model.Get<IntegerEncoder>();
  if (encoder == nullptr) return false;
  if (!encoder->VariableIsFullyEncoded(var)) return false;

  const auto& encoding = encoder->FullDomainEncoding(var);
  const IntegerValue var_min = model.Get<IntegerTrail>()->LowerBound(var);

  LinearConstraintBuilder at_least_one(&model, IntegerValue(1),
                                       kMaxIntegerValue);
  LinearConstraintBuilder encoding_ct(&model, var_min, var_min);
  encoding_ct.AddTerm(var, IntegerValue(1));

  // Create the constraint if all literal have a view.
  std::vector<Literal> at_most_one;

  for (const auto value_literal : encoding) {
    const Literal lit = value_literal.literal;
    const IntegerValue delta = value_literal.value - var_min;
    DCHECK_GE(delta, IntegerValue(0));
    at_most_one.push_back(lit);
    if (!at_least_one.AddLiteralTerm(lit, IntegerValue(1))) return false;
    if (delta != IntegerValue(0)) {
      if (!encoding_ct.AddLiteralTerm(lit, -delta)) return false;
    }
  }

  relaxation->linear_constraints.push_back(at_least_one.Build());
  relaxation->linear_constraints.push_back(encoding_ct.Build());
  relaxation->at_most_ones.push_back(at_most_one);
  return true;
}

namespace {

std::pair<IntegerValue, IntegerValue> GetMinAndMaxNotEncoded(
    IntegerVariable var,
    const absl::flat_hash_set<IntegerValue>& encoded_values,
    const Model& model) {
  const auto* domains = model.Get<IntegerDomains>();
  if (domains == nullptr || var >= domains->size()) {
    return {kMaxIntegerValue, kMinIntegerValue};
  }

  // The domain can be large, but the list of values shouldn't, so this
  // runs in O(encoded_values.size());
  IntegerValue min = kMaxIntegerValue;
  for (const int64_t v : (*domains)[var].Values()) {
    if (!encoded_values.contains(IntegerValue(v))) {
      min = IntegerValue(v);
      break;
    }
  }

  IntegerValue max = kMinIntegerValue;
  for (const int64_t v : (*domains)[NegationOf(var)].Values()) {
    if (!encoded_values.contains(IntegerValue(-v))) {
      max = IntegerValue(-v);
      break;
    }
  }

  return {min, max};
}

bool LinMaxContainsOnlyOneVarInExpressions(const ConstraintProto& ct) {
  CHECK_EQ(ct.constraint_case(), ConstraintProto::ConstraintCase::kLinMax);
  int current_var = -1;
  for (const LinearExpressionProto& expr : ct.lin_max().exprs()) {
    if (expr.vars().empty()) continue;
    if (expr.vars().size() > 1) return false;
    const int var = PositiveRef(expr.vars(0));
    if (current_var == -1) {
      current_var = var;
    } else if (var != current_var) {
      return false;
    }
  }
  return true;
}

// Collect all the affines expressions in a LinMax constraint.
// It checks that these are indeed affine expressions, and that they all share
// the same variable.
// It returns the shared variable, as well as a vector of pairs
// (coefficient, offset) when each affine is coefficient * shared_var + offset.
void CollectAffineExpressionWithSingleVariable(
    const ConstraintProto& ct, CpModelMapping* mapping, IntegerVariable* var,
    std::vector<std::pair<IntegerValue, IntegerValue>>* affines) {
  DCHECK(LinMaxContainsOnlyOneVarInExpressions(ct));
  CHECK_EQ(ct.constraint_case(), ConstraintProto::ConstraintCase::kLinMax);
  *var = kNoIntegerVariable;
  affines->clear();
  for (const LinearExpressionProto& expr : ct.lin_max().exprs()) {
    if (expr.vars().empty()) {
      affines->push_back({IntegerValue(0), IntegerValue(expr.offset())});
    } else {
      CHECK_EQ(expr.vars().size(), 1);
      const IntegerVariable affine_var = mapping->Integer(expr.vars(0));
      if (*var == kNoIntegerVariable) {
        *var = PositiveVariable(affine_var);
      }
      if (VariableIsPositive(affine_var)) {
        CHECK_EQ(affine_var, *var);
        affines->push_back(
            {IntegerValue(expr.coeffs(0)), IntegerValue(expr.offset())});
      } else {
        CHECK_EQ(NegationOf(affine_var), *var);
        affines->push_back(
            {IntegerValue(-expr.coeffs(0)), IntegerValue(expr.offset())});
      }
    }
  }
}

}  // namespace

void AppendRelaxationForEqualityEncoding(IntegerVariable var,
                                         const Model& model,
                                         LinearRelaxation* relaxation,
                                         int* num_tight, int* num_loose) {
  const auto* encoder = model.Get<IntegerEncoder>();
  const auto* integer_trail = model.Get<IntegerTrail>();
  if (encoder == nullptr || integer_trail == nullptr) return;

  std::vector<Literal> at_most_one_ct;
  absl::flat_hash_set<IntegerValue> encoded_values;
  std::vector<ValueLiteralPair> encoding;
  {
    const std::vector<ValueLiteralPair>& initial_encoding =
        encoder->PartialDomainEncoding(var);
    if (initial_encoding.empty()) return;
    for (const auto value_literal : initial_encoding) {
      const Literal literal = value_literal.literal;

      // Note that we skip pairs that do not have an Integer view.
      if (encoder->GetLiteralView(literal) == kNoIntegerVariable &&
          encoder->GetLiteralView(literal.Negated()) == kNoIntegerVariable) {
        continue;
      }

      encoding.push_back(value_literal);
      at_most_one_ct.push_back(literal);
      encoded_values.insert(value_literal.value);
    }
  }
  if (encoded_values.empty()) return;

  // TODO(user): PartialDomainEncoding() filter pair corresponding to literal
  // set to false, however the initial variable Domain is not always updated. As
  // a result, these min/max can be larger than in reality. Try to fix this even
  // if in practice this is a rare occurrence, as the presolve should have
  // propagated most of what we can.
  const auto [min_not_encoded, max_not_encoded] =
      GetMinAndMaxNotEncoded(var, encoded_values, model);

  // This means that there are no non-encoded value and we have a full encoding.
  // We substract the minimum value to reduce its size.
  if (min_not_encoded == kMaxIntegerValue) {
    const IntegerValue rhs = encoding[0].value;
    LinearConstraintBuilder at_least_one(&model, IntegerValue(1),
                                         kMaxIntegerValue);
    LinearConstraintBuilder encoding_ct(&model, rhs, rhs);
    encoding_ct.AddTerm(var, IntegerValue(1));
    for (const auto value_literal : encoding) {
      const Literal lit = value_literal.literal;
      CHECK(at_least_one.AddLiteralTerm(lit, IntegerValue(1)));

      const IntegerValue delta = value_literal.value - rhs;
      if (delta != IntegerValue(0)) {
        CHECK_GE(delta, IntegerValue(0));
        CHECK(encoding_ct.AddLiteralTerm(lit, -delta));
      }
    }

    relaxation->linear_constraints.push_back(at_least_one.Build());
    relaxation->linear_constraints.push_back(encoding_ct.Build());
    relaxation->at_most_ones.push_back(at_most_one_ct);
    ++*num_tight;
    return;
  }

  // In this special case, the two constraints below can be merged into an
  // equality: var = rhs + sum l_i * (value_i - rhs).
  if (min_not_encoded == max_not_encoded) {
    const IntegerValue rhs = min_not_encoded;
    LinearConstraintBuilder encoding_ct(&model, rhs, rhs);
    encoding_ct.AddTerm(var, IntegerValue(1));
    for (const auto value_literal : encoding) {
      CHECK(encoding_ct.AddLiteralTerm(value_literal.literal,
                                       rhs - value_literal.value));
    }
    relaxation->at_most_ones.push_back(at_most_one_ct);
    relaxation->linear_constraints.push_back(encoding_ct.Build());
    ++*num_tight;
    return;
  }

  // min + sum l_i * (value_i - min) <= var.
  const IntegerValue d_min = min_not_encoded;
  LinearConstraintBuilder lower_bound_ct(&model, d_min, kMaxIntegerValue);
  lower_bound_ct.AddTerm(var, IntegerValue(1));
  for (const auto value_literal : encoding) {
    CHECK(lower_bound_ct.AddLiteralTerm(value_literal.literal,
                                        d_min - value_literal.value));
  }

  // var <= max + sum l_i * (value_i - max).
  const IntegerValue d_max = max_not_encoded;
  LinearConstraintBuilder upper_bound_ct(&model, kMinIntegerValue, d_max);
  upper_bound_ct.AddTerm(var, IntegerValue(1));
  for (const auto value_literal : encoding) {
    CHECK(upper_bound_ct.AddLiteralTerm(value_literal.literal,
                                        d_max - value_literal.value));
  }

  // Note that empty/trivial constraints will be filtered later.
  relaxation->at_most_ones.push_back(at_most_one_ct);
  relaxation->linear_constraints.push_back(lower_bound_ct.Build());
  relaxation->linear_constraints.push_back(upper_bound_ct.Build());
  ++*num_loose;
}

void AppendPartialGreaterThanEncodingRelaxation(IntegerVariable var,
                                                const Model& model,
                                                LinearRelaxation* relaxation) {
  const auto* integer_trail = model.Get<IntegerTrail>();
  const auto* encoder = model.Get<IntegerEncoder>();
  if (integer_trail == nullptr || encoder == nullptr) return;

  const absl::btree_map<IntegerValue, Literal>& greater_than_encoding =
      encoder->PartialGreaterThanEncoding(var);
  if (greater_than_encoding.empty()) return;

  // Start by the var >= side.
  // And also add the implications between used literals.
  {
    IntegerValue prev_used_bound = integer_trail->LowerBound(var);
    LinearConstraintBuilder lb_constraint(&model, prev_used_bound,
                                          kMaxIntegerValue);
    lb_constraint.AddTerm(var, IntegerValue(1));
    LiteralIndex prev_literal_index = kNoLiteralIndex;
    for (const auto entry : greater_than_encoding) {
      if (entry.first <= prev_used_bound) continue;

      const LiteralIndex literal_index = entry.second.Index();
      const IntegerValue diff = prev_used_bound - entry.first;

      // Skip the entry if the literal doesn't have a view.
      if (!lb_constraint.AddLiteralTerm(entry.second, diff)) continue;
      if (prev_literal_index != kNoLiteralIndex) {
        // Add var <= prev_var, which is the same as var + not(prev_var) <= 1
        relaxation->at_most_ones.push_back(
            {Literal(literal_index), Literal(prev_literal_index).Negated()});
      }
      prev_used_bound = entry.first;
      prev_literal_index = literal_index;
    }
    relaxation->linear_constraints.push_back(lb_constraint.Build());
  }

  // Do the same for the var <= side by using NegationOfVar().
  // Note that we do not need to add the implications between literals again.
  {
    IntegerValue prev_used_bound = integer_trail->LowerBound(NegationOf(var));
    LinearConstraintBuilder lb_constraint(&model, prev_used_bound,
                                          kMaxIntegerValue);
    lb_constraint.AddTerm(var, IntegerValue(-1));
    for (const auto entry :
         encoder->PartialGreaterThanEncoding(NegationOf(var))) {
      if (entry.first <= prev_used_bound) continue;
      const IntegerValue diff = prev_used_bound - entry.first;

      // Skip the entry if the literal doesn't have a view.
      if (!lb_constraint.AddLiteralTerm(entry.second, diff)) continue;
      prev_used_bound = entry.first;
    }
    relaxation->linear_constraints.push_back(lb_constraint.Build());
  }
}

namespace {

// Adds {enforcing_lits} => rhs_domain_min <= expr <= rhs_domain_max.
// Requires expr offset to be 0.
void AppendEnforcedLinearExpression(
    const std::vector<Literal>& enforcing_literals,
    const LinearExpression& expr, const IntegerValue rhs_domain_min,
    const IntegerValue rhs_domain_max, const Model& model,
    LinearRelaxation* relaxation) {
  CHECK_EQ(expr.offset, IntegerValue(0));
  const LinearExpression canonical_expr = CanonicalizeExpr(expr);
  const IntegerTrail* integer_trail = model.Get<IntegerTrail>();
  const IntegerValue min_expr_value = canonical_expr.Min(*integer_trail);

  if (rhs_domain_min > min_expr_value) {
    // And(ei) => terms >= rhs_domain_min
    // <=> Sum_i (~ei * (rhs_domain_min - min_expr_value)) + terms >=
    // rhs_domain_min
    LinearConstraintBuilder lc(&model, rhs_domain_min, kMaxIntegerValue);
    for (const Literal& literal : enforcing_literals) {
      CHECK(lc.AddLiteralTerm(literal.Negated(),
                              rhs_domain_min - min_expr_value));
    }
    for (int i = 0; i < canonical_expr.vars.size(); i++) {
      lc.AddTerm(canonical_expr.vars[i], canonical_expr.coeffs[i]);
    }
    relaxation->linear_constraints.push_back(lc.Build());
  }
  const IntegerValue max_expr_value = canonical_expr.Max(*integer_trail);
  if (rhs_domain_max < max_expr_value) {
    // And(ei) => terms <= rhs_domain_max
    // <=> Sum_i (~ei * (rhs_domain_max - max_expr_value)) + terms <=
    // rhs_domain_max
    LinearConstraintBuilder lc(&model, kMinIntegerValue, rhs_domain_max);
    for (const Literal& literal : enforcing_literals) {
      CHECK(lc.AddLiteralTerm(literal.Negated(),
                              rhs_domain_max - max_expr_value));
    }
    for (int i = 0; i < canonical_expr.vars.size(); i++) {
      lc.AddTerm(canonical_expr.vars[i], canonical_expr.coeffs[i]);
    }
    relaxation->linear_constraints.push_back(lc.Build());
  }
}

bool AllLiteralsHaveViews(const IntegerEncoder& encoder,
                          const std::vector<Literal>& literals) {
  for (const Literal lit : literals) {
    if (!encoder.LiteralOrNegationHasView(lit)) return false;
  }
  return true;
}

}  // namespace

void AppendBoolOrRelaxation(const ConstraintProto& ct, Model* model,
                            LinearRelaxation* relaxation) {
  auto* mapping = model->GetOrCreate<CpModelMapping>();
  LinearConstraintBuilder lc(model, IntegerValue(1), kMaxIntegerValue);
  for (const int enforcement_ref : ct.enforcement_literal()) {
    CHECK(lc.AddLiteralTerm(mapping->Literal(NegatedRef(enforcement_ref)),
                            IntegerValue(1)));
  }
  for (const int ref : ct.bool_or().literals()) {
    CHECK(lc.AddLiteralTerm(mapping->Literal(ref), IntegerValue(1)));
  }
  relaxation->linear_constraints.push_back(lc.Build());
}

void AppendBoolAndRelaxation(const ConstraintProto& ct, Model* model,
                             LinearRelaxation* relaxation) {
  // TODO(user): These constraints can be many, and if they are not regrouped
  // in big at most ones, then they should probably only added lazily as cuts.
  // Regroup this with future clique-cut separation logic.
  if (!HasEnforcementLiteral(ct)) return;

  auto* mapping = model->GetOrCreate<CpModelMapping>();
  if (ct.enforcement_literal().size() == 1) {
    const Literal enforcement = mapping->Literal(ct.enforcement_literal(0));
    for (const int ref : ct.bool_and().literals()) {
      relaxation->at_most_ones.push_back(
          {enforcement, mapping->Literal(ref).Negated()});
    }
    return;
  }

  // Andi(e_i) => Andj(x_j)
  // <=> num_rhs_terms <= Sum_j(x_j) + num_rhs_terms * Sum_i(~e_i)
  int num_literals = ct.bool_and().literals_size();
  LinearConstraintBuilder lc(model, IntegerValue(num_literals),
                             kMaxIntegerValue);
  for (const int ref : ct.bool_and().literals()) {
    CHECK(lc.AddLiteralTerm(mapping->Literal(ref), IntegerValue(1)));
  }
  for (const int enforcement_ref : ct.enforcement_literal()) {
    CHECK(lc.AddLiteralTerm(mapping->Literal(NegatedRef(enforcement_ref)),
                            IntegerValue(num_literals)));
  }
  relaxation->linear_constraints.push_back(lc.Build());
}

void AppendAtMostOneRelaxation(const ConstraintProto& ct, Model* model,
                               LinearRelaxation* relaxation) {
  if (HasEnforcementLiteral(ct)) return;

  auto* mapping = model->GetOrCreate<CpModelMapping>();
  relaxation->at_most_ones.push_back(
      mapping->Literals(ct.at_most_one().literals()));
}

void AppendExactlyOneRelaxation(const ConstraintProto& ct, Model* model,
                                LinearRelaxation* relaxation) {
  if (HasEnforcementLiteral(ct)) return;
  auto* mapping = model->GetOrCreate<CpModelMapping>();
  auto* encoder = model->GetOrCreate<IntegerEncoder>();

  const std::vector<Literal> literals =
      mapping->Literals(ct.exactly_one().literals());
  if (AllLiteralsHaveViews(*encoder, literals)) {
    LinearConstraintBuilder lc(model, IntegerValue(1), IntegerValue(1));
    for (const Literal lit : literals) {
      CHECK(lc.AddLiteralTerm(lit, IntegerValue(1)));
    }
    relaxation->linear_constraints.push_back(lc.Build());
  } else {
    // We just encode the at most one part that might be partially linearized
    // later.
    relaxation->at_most_ones.push_back(literals);
  }
}

std::vector<Literal> CreateAlternativeLiteralsWithView(
    int num_literals, Model* model, LinearRelaxation* relaxation) {
  auto* encoder = model->GetOrCreate<IntegerEncoder>();

  if (num_literals == 1) {
    // This is not supposed to happen, but it is easy enough to cover, just
    // in case. We might however want to use encoder->GetTrueLiteral().
    const IntegerVariable var = model->Add(NewIntegerVariable(1, 1));
    const Literal lit =
        encoder->GetOrCreateLiteralAssociatedToEquality(var, IntegerValue(1));
    return {lit};
  }

  if (num_literals == 2) {
    const IntegerVariable var = model->Add(NewIntegerVariable(0, 1));
    const Literal lit =
        encoder->GetOrCreateLiteralAssociatedToEquality(var, IntegerValue(1));

    // TODO(user): We shouldn't need to create this view ideally. Even better,
    // we should be able to handle Literal natively in the linear relaxation,
    // but that is a lot of work.
    const IntegerVariable var2 = model->Add(NewIntegerVariable(0, 1));
    encoder->AssociateToIntegerEqualValue(lit.Negated(), var2, IntegerValue(1));

    return {lit, lit.Negated()};
  }

  std::vector<Literal> literals;
  LinearConstraintBuilder lc_builder(model, IntegerValue(1), IntegerValue(1));
  for (int i = 0; i < num_literals; ++i) {
    const IntegerVariable var = model->Add(NewIntegerVariable(0, 1));
    const Literal lit =
        encoder->GetOrCreateLiteralAssociatedToEquality(var, IntegerValue(1));
    literals.push_back(lit);
    CHECK(lc_builder.AddLiteralTerm(lit, IntegerValue(1)));
  }
  model->Add(ExactlyOneConstraint(literals));
  relaxation->linear_constraints.push_back(lc_builder.Build());
  return literals;
}

void AppendCircuitRelaxation(const ConstraintProto& ct, Model* model,
                             LinearRelaxation* relaxation) {
  if (HasEnforcementLiteral(ct)) return;
  auto* mapping = model->GetOrCreate<CpModelMapping>();
  const int num_arcs = ct.circuit().literals_size();
  CHECK_EQ(num_arcs, ct.circuit().tails_size());
  CHECK_EQ(num_arcs, ct.circuit().heads_size());

  // Each node must have exactly one incoming and one outgoing arc (note
  // that it can be the unique self-arc of this node too).
  absl::btree_map<int, std::vector<Literal>> incoming_arc_constraints;
  absl::btree_map<int, std::vector<Literal>> outgoing_arc_constraints;
  for (int i = 0; i < num_arcs; i++) {
    const Literal arc = mapping->Literal(ct.circuit().literals(i));
    const int tail = ct.circuit().tails(i);
    const int head = ct.circuit().heads(i);

    // Make sure this literal has a view.
    model->Add(NewIntegerVariableFromLiteral(arc));
    outgoing_arc_constraints[tail].push_back(arc);
    incoming_arc_constraints[head].push_back(arc);
  }
  for (const auto* node_map :
       {&outgoing_arc_constraints, &incoming_arc_constraints}) {
    for (const auto& entry : *node_map) {
      const std::vector<Literal>& exactly_one = entry.second;
      if (exactly_one.size() > 1) {
        LinearConstraintBuilder at_least_one_lc(model, IntegerValue(1),
                                                kMaxIntegerValue);
        for (const Literal l : exactly_one) {
          CHECK(at_least_one_lc.AddLiteralTerm(l, IntegerValue(1)));
        }

        // We separate the two constraints.
        relaxation->at_most_ones.push_back(exactly_one);
        relaxation->linear_constraints.push_back(at_least_one_lc.Build());
      }
    }
  }
}

void AppendRoutesRelaxation(const ConstraintProto& ct, Model* model,
                            LinearRelaxation* relaxation) {
  if (HasEnforcementLiteral(ct)) return;
  auto* mapping = model->GetOrCreate<CpModelMapping>();
  const int num_arcs = ct.routes().literals_size();
  CHECK_EQ(num_arcs, ct.routes().tails_size());
  CHECK_EQ(num_arcs, ct.routes().heads_size());

  // Each node except node zero must have exactly one incoming and one outgoing
  // arc (note that it can be the unique self-arc of this node too). For node
  // zero, the number of incoming arcs should be the same as the number of
  // outgoing arcs.
  absl::btree_map<int, std::vector<Literal>> incoming_arc_constraints;
  absl::btree_map<int, std::vector<Literal>> outgoing_arc_constraints;
  for (int i = 0; i < num_arcs; i++) {
    const Literal arc = mapping->Literal(ct.routes().literals(i));
    const int tail = ct.routes().tails(i);
    const int head = ct.routes().heads(i);

    // Make sure this literal has a view.
    model->Add(NewIntegerVariableFromLiteral(arc));
    outgoing_arc_constraints[tail].push_back(arc);
    incoming_arc_constraints[head].push_back(arc);
  }
  for (const auto* node_map :
       {&outgoing_arc_constraints, &incoming_arc_constraints}) {
    for (const auto& entry : *node_map) {
      if (entry.first == 0) continue;
      const std::vector<Literal>& exactly_one = entry.second;
      if (exactly_one.size() > 1) {
        LinearConstraintBuilder at_least_one_lc(model, IntegerValue(1),
                                                kMaxIntegerValue);
        for (const Literal l : exactly_one) {
          CHECK(at_least_one_lc.AddLiteralTerm(l, IntegerValue(1)));
        }

        // We separate the two constraints.
        relaxation->at_most_ones.push_back(exactly_one);
        relaxation->linear_constraints.push_back(at_least_one_lc.Build());
      }
    }
  }
  LinearConstraintBuilder zero_node_balance_lc(model, IntegerValue(0),
                                               IntegerValue(0));
  for (const Literal& incoming_arc : incoming_arc_constraints[0]) {
    CHECK(zero_node_balance_lc.AddLiteralTerm(incoming_arc, IntegerValue(1)));
  }
  for (const Literal& outgoing_arc : outgoing_arc_constraints[0]) {
    CHECK(zero_node_balance_lc.AddLiteralTerm(outgoing_arc, IntegerValue(-1)));
  }
  relaxation->linear_constraints.push_back(zero_node_balance_lc.Build());
}

// Scan the intervals of a cumulative/no_overlap constraint, and its capacity (1
// for the no_overlap). It returns the index of the makespan interval if found,
// or -1 otherwise.
//
// Currently, this requires the capacity to be fixed in order to scan for a
// makespan interval.
//
// The makespan interval has the following property:
//   - its end is fixed at the horizon
//   - it is always present
//   - its demand is the capacity of the cumulative/no_overlap.
//   - its size is > 0.
//
// These property ensures that all other intervals ends before the start of
// the makespan interval.
int DetectMakespan(const std::vector<IntervalVariable>& intervals,
                   const std::vector<AffineExpression>& demands,
                   const AffineExpression& capacity, Model* model) {
  IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
  IntervalsRepository* repository = model->GetOrCreate<IntervalsRepository>();

  // TODO(user): Supports variable capacity.
  if (!integer_trail->IsFixed(capacity)) {
    return -1;
  }

  // Detect the horizon (max of all end max of all intervals).
  IntegerValue horizon = kMinIntegerValue;
  for (int i = 0; i < intervals.size(); ++i) {
    if (repository->IsAbsent(intervals[i])) continue;
    horizon = std::max(
        horizon, integer_trail->UpperBound(repository->End(intervals[i])));
  }

  const IntegerValue capacity_value = integer_trail->FixedValue(capacity);
  for (int i = 0; i < intervals.size(); ++i) {
    if (repository->IsAbsent(intervals[i])) continue;
    const AffineExpression& end = repository->End(intervals[i]);
    if (integer_trail->IsFixed(demands[i]) &&
        integer_trail->FixedValue(demands[i]) == capacity_value &&
        integer_trail->IsFixed(end) &&
        integer_trail->FixedValue(end) == horizon &&
        integer_trail->LowerBound(repository->Size(intervals[i])) > 0 &&
        repository->IsPresent(intervals[i])) {
      return i;
    }
  }
  return -1;
}

void AppendNoOverlapRelaxationAndCutGenerator(const ConstraintProto& ct,
                                              Model* model,
                                              LinearRelaxation* relaxation) {
  if (HasEnforcementLiteral(ct)) return;
  auto* mapping = model->GetOrCreate<CpModelMapping>();
  std::vector<IntervalVariable> intervals =
      mapping->Intervals(ct.no_overlap().intervals());
  const IntegerValue one(1);
  std::vector<AffineExpression> demands(intervals.size(), one);
  const int makespan_index =
      DetectMakespan(intervals, demands, /*capacity=*/one, model);
  std::optional<AffineExpression> makespan;
  IntervalsRepository* repository = model->GetOrCreate<IntervalsRepository>();

  if (makespan_index != -1) {
    makespan = repository->Start(intervals[makespan_index]);
    demands.pop_back();  // the vector is filled with ones.
    intervals.erase(intervals.begin() + makespan_index);
  }

  SchedulingConstraintHelper* helper = repository->GetOrCreateHelper(intervals);
  if (!helper->SynchronizeAndSetTimeDirection(true)) return;

  SchedulingDemandHelper* demands_helper =
      new SchedulingDemandHelper(demands, helper, model);
  model->TakeOwnership(demands_helper);

  AddCumulativeRelaxation(/*capacity=*/one, helper, demands_helper, makespan,
                          model, relaxation);
  if (model->GetOrCreate<SatParameters>()->linearization_level() > 1) {
    AddNoOverlapCutGenerator(helper, makespan, model, relaxation);
  }
}

void AppendCumulativeRelaxationAndCutGenerator(const ConstraintProto& ct,
                                               Model* model,
                                               LinearRelaxation* relaxation) {
  if (HasEnforcementLiteral(ct)) return;
  auto* mapping = model->GetOrCreate<CpModelMapping>();
  std::vector<IntervalVariable> intervals =
      mapping->Intervals(ct.cumulative().intervals());
  std::vector<AffineExpression> demands =
      mapping->Affines(ct.cumulative().demands());
  const AffineExpression capacity = mapping->Affine(ct.cumulative().capacity());
  const int makespan_index =
      DetectMakespan(intervals, demands, capacity, model);
  std::optional<AffineExpression> makespan;
  IntervalsRepository* repository = model->GetOrCreate<IntervalsRepository>();
  if (makespan_index != -1) {
    // We remove the makespan data from the intervals the demands vector.
    makespan = repository->Start(intervals[makespan_index]);
    demands.erase(demands.begin() + makespan_index);
    intervals.erase(intervals.begin() + makespan_index);
  }

  // We try to linearize the energy of each task (size * demand).
  SchedulingConstraintHelper* helper = repository->GetOrCreateHelper(intervals);
  if (!helper->SynchronizeAndSetTimeDirection(true)) return;
  SchedulingDemandHelper* demands_helper =
      new SchedulingDemandHelper(demands, helper, model);
  model->TakeOwnership(demands_helper);

  // We can now add the relaxation and the cut generators.
  AddCumulativeRelaxation(capacity, helper, demands_helper, makespan, model,
                          relaxation);
  if (model->GetOrCreate<SatParameters>()->linearization_level() > 1) {
    AddCumulativeCutGenerator(capacity, helper, demands_helper, makespan, model,
                              relaxation);
  }
}

// This relaxation will compute the bounding box of all tasks in the cumulative,
// and add the constraint that the sum of energies of each task must fit in the
// capacity * span area.
void AddCumulativeRelaxation(const AffineExpression& capacity,
                             SchedulingConstraintHelper* helper,
                             SchedulingDemandHelper* demands_helper,
                             const std::optional<AffineExpression>& makespan,
                             Model* model, LinearRelaxation* relaxation) {
  const int num_intervals = helper->NumTasks();
  demands_helper->CacheAllEnergyValues();

  std::vector<Literal> presence_literals;
  std::vector<AffineExpression> starts;
  std::vector<AffineExpression> ends;
  std::vector<Literal> clause;
  bool at_least_one_interval_is_present = false;
  IntegerValue min_of_starts = kMaxIntegerValue;
  IntegerValue max_of_ends = kMinIntegerValue;
  int num_variable_energies = 0;
  int num_optionals = 0;
  IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
  for (int index = 0; index < num_intervals; ++index) {
    if (helper->IsAbsent(index)) continue;

    min_of_starts = std::min(min_of_starts, helper->StartMin(index));
    max_of_ends = std::max(max_of_ends, helper->EndMax(index));

    if (helper->IsOptional(index)) {
      num_optionals++;
      const Literal task_lit = helper->PresenceLiteral(index);
      presence_literals.push_back(task_lit);
      clause.push_back(task_lit);
    } else {
      at_least_one_interval_is_present = true;
      presence_literals.push_back(
          model->GetOrCreate<IntegerEncoder>()->GetTrueLiteral());
    }

    if (!helper->SizeIsFixed(index) || !demands_helper->DemandIsFixed(index)) {
      num_variable_energies++;
    }
    starts.push_back(helper->Starts()[index]);
    ends.push_back(helper->Ends()[index]);
  }

  VLOG(2) << "Span [" << min_of_starts << ".." << max_of_ends << "] with "
          << num_optionals << " optional intervals, and "
          << num_variable_energies << " variable energy tasks out of "
          << num_intervals << " intervals";

  // If nothing is variable, the linear relaxation will already be enforced by
  // the scheduling propagators.
  if (num_variable_energies + num_optionals == 0) return;

  const IntegerVariable span_start =
      integer_trail->AddIntegerVariable(min_of_starts, max_of_ends);
  const AffineExpression span_end =
      makespan.has_value()
          ? makespan.value()
          : integer_trail->AddIntegerVariable(min_of_starts, max_of_ends);

  auto* sat_solver = model->GetOrCreate<SatSolver>();
  const Literal cumulative_is_not_empty =
      at_least_one_interval_is_present
          ? model->GetOrCreate<IntegerEncoder>()->GetTrueLiteral()
          : Literal(model->Add(NewBooleanVariable()), true);
  if (!at_least_one_interval_is_present) {
    for (const Literal task_lit : clause) {
      sat_solver->AddBinaryClause(task_lit.Negated(), cumulative_is_not_empty);
    }
    clause.push_back(cumulative_is_not_empty.Negated());
    sat_solver->AddProblemClause(clause, /*is_safe=*/false);
  }

  // Link span_start and span_end to the starts and ends of the tasks.
  model->Add(EqualMinOfSelectedVariables(cumulative_is_not_empty, span_start,
                                         starts, presence_literals));
  if (!makespan.has_value()) {
    model->Add(EqualMaxOfSelectedVariables(cumulative_is_not_empty, span_end,
                                           ends, presence_literals));
  }

  LinearConstraintBuilder lc(model, kMinIntegerValue, IntegerValue(0));
  lc.AddTerm(span_end, -integer_trail->UpperBound(capacity));
  lc.AddTerm(span_start, integer_trail->UpperBound(capacity));
  for (int i = 0; i < num_intervals; ++i) {
    if (helper->IsAbsent(i)) continue;

    if (!helper->IsOptional(i)) {
      const std::vector<LiteralValueValue>& product =
          demands_helper->DecomposedEnergies()[i];
      if (!product.empty()) {
        // The energy is defined if the vector is not empty.
        if (!lc.AddDecomposedProduct(product)) return;
      } else {
        // The energy is not a decomposed product, but it could still be
        // constant or linear. If not, a McCormick relaxation will be
        // introduced.  AddQuadraticLowerBound() supports all cases.
        lc.AddQuadraticLowerBound(helper->Sizes()[i],
                                  demands_helper->Demands()[i], integer_trail);
      }
    } else {
      const IntegerValue energy_min = demands_helper->EnergyMin(i);
      if (!lc.AddLiteralTerm(helper->PresenceLiteral(i), energy_min)) {
        return;
      }
    }
  }
  relaxation->linear_constraints.push_back(lc.Build());
}

// Adds the energetic relaxation sum(areas) <= bounding box area.
void AppendNoOverlap2dRelaxation(const ConstraintProto& ct, Model* model,
                                 LinearRelaxation* relaxation) {
  CHECK(ct.has_no_overlap_2d());
  if (HasEnforcementLiteral(ct)) return;

  auto* mapping = model->GetOrCreate<CpModelMapping>();
  std::vector<IntervalVariable> x_intervals =
      mapping->Intervals(ct.no_overlap_2d().x_intervals());
  std::vector<IntervalVariable> y_intervals =
      mapping->Intervals(ct.no_overlap_2d().y_intervals());

  auto* integer_trail = model->GetOrCreate<IntegerTrail>();
  auto* intervals_repository = model->GetOrCreate<IntervalsRepository>();

  IntegerValue x_min = kMaxIntegerValue;
  IntegerValue x_max = kMinIntegerValue;
  IntegerValue y_min = kMaxIntegerValue;
  IntegerValue y_max = kMinIntegerValue;
  std::vector<AffineExpression> x_sizes;
  std::vector<AffineExpression> y_sizes;
  for (int i = 0; i < ct.no_overlap_2d().x_intervals_size(); ++i) {
    x_sizes.push_back(intervals_repository->Size(x_intervals[i]));
    y_sizes.push_back(intervals_repository->Size(y_intervals[i]));
    x_min = std::min(x_min, integer_trail->LevelZeroLowerBound(
                                intervals_repository->Start(x_intervals[i])));
    x_max = std::max(x_max, integer_trail->LevelZeroUpperBound(
                                intervals_repository->End(x_intervals[i])));
    y_min = std::min(y_min, integer_trail->LevelZeroLowerBound(
                                intervals_repository->Start(y_intervals[i])));
    y_max = std::max(y_max, integer_trail->LevelZeroUpperBound(
                                intervals_repository->End(y_intervals[i])));
  }

  const IntegerValue max_area =
      IntegerValue(CapProd(CapSub(x_max.value(), x_min.value()),
                           CapSub(y_max.value(), y_min.value())));
  if (max_area == kMaxIntegerValue) return;

  LinearConstraintBuilder lc(model, IntegerValue(0), max_area);
  for (int i = 0; i < ct.no_overlap_2d().x_intervals_size(); ++i) {
    if (intervals_repository->IsPresent(x_intervals[i]) &&
        intervals_repository->IsPresent(y_intervals[i])) {
      const std::vector<LiteralValueValue> energy =
          TryToDecomposeProduct(x_sizes[i], y_sizes[i], model);
      if (!energy.empty()) {
        if (!lc.AddDecomposedProduct(energy)) return;
      } else {
        lc.AddQuadraticLowerBound(x_sizes[i], y_sizes[i], integer_trail);
      }
    } else if (intervals_repository->IsPresent(x_intervals[i]) ||
               intervals_repository->IsPresent(y_intervals[i]) ||
               (intervals_repository->PresenceLiteral(x_intervals[i]) ==
                intervals_repository->PresenceLiteral(y_intervals[i]))) {
      // We have only one active literal.
      const Literal presence_literal =
          intervals_repository->IsPresent(x_intervals[i])
              ? intervals_repository->PresenceLiteral(y_intervals[i])
              : intervals_repository->PresenceLiteral(x_intervals[i]);
      const IntegerValue area_min =
          integer_trail->LevelZeroLowerBound(x_sizes[i]) *
          integer_trail->LevelZeroLowerBound(y_sizes[i]);
      if (area_min != 0) {
        // Not including the term if we don't have a view is ok.
        (void)lc.AddLiteralTerm(presence_literal, area_min);
      }
    }
  }
  relaxation->linear_constraints.push_back(lc.Build());
}

void AppendLinMaxRelaxationPart1(const ConstraintProto& ct, Model* model,
                                 LinearRelaxation* relaxation) {
  auto* mapping = model->GetOrCreate<CpModelMapping>();

  // We want to linearize target = max(exprs[1], exprs[2], ..., exprs[d]).
  // Part 1: Encode target >= max(exprs[1], exprs[2], ..., exprs[d])
  const LinearExpression negated_target =
      NegationOf(mapping->GetExprFromProto(ct.lin_max().target()));
  for (int i = 0; i < ct.lin_max().exprs_size(); ++i) {
    const LinearExpression expr =
        mapping->GetExprFromProto(ct.lin_max().exprs(i));
    LinearConstraintBuilder lc(model, kMinIntegerValue, IntegerValue(0));
    lc.AddLinearExpression(negated_target);
    lc.AddLinearExpression(expr);
    relaxation->linear_constraints.push_back(lc.Build());
  }
}

// TODO(user): experiment with:
//   1) remove this code
//   2) keep this code
//   3) remove this code and create the cut generator at level 1.
void AppendMaxAffineRelaxation(const ConstraintProto& ct, Model* model,
                               LinearRelaxation* relaxation) {
  IntegerVariable var;
  std::vector<std::pair<IntegerValue, IntegerValue>> affines;
  auto* mapping = model->GetOrCreate<CpModelMapping>();
  CollectAffineExpressionWithSingleVariable(ct, mapping, &var, &affines);
  if (var == kNoIntegerVariable ||
      model->GetOrCreate<IntegerTrail>()->IsFixed(var)) {
    return;
  }

  CHECK(VariableIsPositive(var));
  const LinearExpression target_expr =
      PositiveVarExpr(mapping->GetExprFromProto(ct.lin_max().target()));
  LinearConstraintBuilder builder(model);
  if (BuildMaxAffineUpConstraint(target_expr, var, affines, model, &builder)) {
    relaxation->linear_constraints.push_back(builder.Build());
  }
}

void AddMaxAffineCutGenerator(const ConstraintProto& ct, Model* model,
                              LinearRelaxation* relaxation) {
  IntegerVariable var;
  std::vector<std::pair<IntegerValue, IntegerValue>> affines;
  auto* mapping = model->GetOrCreate<CpModelMapping>();
  CollectAffineExpressionWithSingleVariable(ct, mapping, &var, &affines);
  if (var == kNoIntegerVariable ||
      model->GetOrCreate<IntegerTrail>()->IsFixed(var)) {
    return;
  }

  // If the target is constant, propagation is enough.
  if (ct.lin_max().target().vars().empty()) return;

  const LinearExpression target_expr =
      PositiveVarExpr(mapping->GetExprFromProto(ct.lin_max().target()));
  relaxation->cut_generators.push_back(CreateMaxAffineCutGenerator(
      target_expr, var, affines, "AffineMax", model));
}

// Part 2: Encode upper bound on X.
//
// Add linking constraint to the CP solver
// sum zi = 1 and for all i, zi => max = expr_i.
void AppendLinMaxRelaxationPart2(
    IntegerVariable target, const std::vector<Literal>& alternative_literals,
    const std::vector<LinearExpression>& exprs, Model* model,
    LinearRelaxation* relaxation) {
  const int num_exprs = exprs.size();
  GenericLiteralWatcher* watcher = model->GetOrCreate<GenericLiteralWatcher>();

  // First add the CP constraints.
  for (int i = 0; i < num_exprs; ++i) {
    LinearExpression local_expr;
    local_expr.vars = NegationOf(exprs[i].vars);
    local_expr.vars.push_back(target);
    local_expr.coeffs = exprs[i].coeffs;
    local_expr.coeffs.push_back(IntegerValue(1));
    IntegerSumLE* upper_bound =
        new IntegerSumLE({alternative_literals[i]}, local_expr.vars,
                         local_expr.coeffs, exprs[i].offset, model);
    upper_bound->RegisterWith(watcher);
    model->TakeOwnership(upper_bound);
  }

  // For the relaxation, we use different constraints with a stronger linear
  // relaxation as explained in the .h
  //
  // TODO(user): Consider passing the x_vars to this method instead of
  // computing it here.
  std::vector<IntegerVariable> x_vars;
  for (int i = 0; i < num_exprs; ++i) {
    x_vars.insert(x_vars.end(), exprs[i].vars.begin(), exprs[i].vars.end());
  }
  gtl::STLSortAndRemoveDuplicates(&x_vars);

  // All expressions should only contain positive variables.
  DCHECK(std::all_of(x_vars.begin(), x_vars.end(), [](IntegerVariable var) {
    return VariableIsPositive(var);
  }));

  std::vector<std::vector<IntegerValue>> sum_of_max_corner_diff(
      num_exprs, std::vector<IntegerValue>(num_exprs, IntegerValue(0)));

  IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
  for (int i = 0; i < num_exprs; ++i) {
    for (int j = 0; j < num_exprs; ++j) {
      if (i == j) continue;
      for (const IntegerVariable x_var : x_vars) {
        const IntegerValue lb = integer_trail->LevelZeroLowerBound(x_var);
        const IntegerValue ub = integer_trail->LevelZeroUpperBound(x_var);
        const IntegerValue diff =
            GetCoefficient(x_var, exprs[j]) - GetCoefficient(x_var, exprs[i]);
        sum_of_max_corner_diff[i][j] += std::max(diff * lb, diff * ub);
      }
    }
  }
  for (int i = 0; i < num_exprs; ++i) {
    LinearConstraintBuilder lc(model, kMinIntegerValue, IntegerValue(0));
    lc.AddTerm(target, IntegerValue(1));
    for (int j = 0; j < exprs[i].vars.size(); ++j) {
      lc.AddTerm(exprs[i].vars[j], -exprs[i].coeffs[j]);
    }
    for (int j = 0; j < num_exprs; ++j) {
      CHECK(lc.AddLiteralTerm(alternative_literals[j],
                              -exprs[j].offset - sum_of_max_corner_diff[i][j]));
    }
    relaxation->linear_constraints.push_back(lc.Build());
  }
}

void AppendLinearConstraintRelaxation(const ConstraintProto& ct,
                                      bool linearize_enforced_constraints,
                                      Model* model,
                                      LinearRelaxation* relaxation) {
  auto* mapping = model->Get<CpModelMapping>();

  // Note that we ignore the holes in the domain.
  //
  // TODO(user): In LoadLinearConstraint() we already created intermediate
  // Booleans for each disjoint interval, we should reuse them here if
  // possible.
  //
  // TODO(user): process the "at most one" part of a == 1 separately?
  const IntegerValue rhs_domain_min = IntegerValue(ct.linear().domain(0));
  const IntegerValue rhs_domain_max =
      IntegerValue(ct.linear().domain(ct.linear().domain_size() - 1));
  if (rhs_domain_min == std::numeric_limits<int64_t>::min() &&
      rhs_domain_max == std::numeric_limits<int64_t>::max())
    return;

  if (!HasEnforcementLiteral(ct)) {
    LinearConstraintBuilder lc(model, rhs_domain_min, rhs_domain_max);
    for (int i = 0; i < ct.linear().vars_size(); i++) {
      const int ref = ct.linear().vars(i);
      const int64_t coeff = ct.linear().coeffs(i);
      lc.AddTerm(mapping->Integer(ref), IntegerValue(coeff));
    }
    relaxation->linear_constraints.push_back(lc.Build());
    return;
  }

  // Reified version.
  if (!linearize_enforced_constraints) return;

  // We linearize fully reified constraints of size 1 all together for a given
  // variable. But we need to process half-reified ones.
  if (!mapping->IsHalfEncodingConstraint(&ct) && ct.linear().vars_size() <= 1) {
    return;
  }

  std::vector<Literal> enforcing_literals;
  enforcing_literals.reserve(ct.enforcement_literal_size());
  for (const int enforcement_ref : ct.enforcement_literal()) {
    enforcing_literals.push_back(mapping->Literal(enforcement_ref));
  }
  LinearExpression expr;
  expr.vars.reserve(ct.linear().vars_size());
  expr.coeffs.reserve(ct.linear().vars_size());
  for (int i = 0; i < ct.linear().vars_size(); i++) {
    int ref = ct.linear().vars(i);
    IntegerValue coeff(ct.linear().coeffs(i));
    if (!RefIsPositive(ref)) {
      ref = PositiveRef(ref);
      coeff = -coeff;
    }
    const IntegerVariable int_var = mapping->Integer(ref);
    expr.vars.push_back(int_var);
    expr.coeffs.push_back(coeff);
  }
  AppendEnforcedLinearExpression(enforcing_literals, expr, rhs_domain_min,
                                 rhs_domain_max, *model, relaxation);
}

// Add a static and a dynamic linear relaxation of the CP constraint to the set
// of linear constraints. The highest linearization_level is, the more types of
// constraint we encode. This method should be called only for
// linearization_level > 0. The static part is just called a relaxation and is
// called at the root node of the search. The dynamic part is implemented
// through a set of linear cut generators that will be called throughout the
// search.
//
// TODO(user): In full generality, we could encode all the constraint as an LP.
// TODO(user): Add unit tests for this method.
// TODO(user): Remove and merge with model loading.
void TryToLinearizeConstraint(const CpModelProto& model_proto,
                              const ConstraintProto& ct,
                              int linearization_level, Model* model,
                              LinearRelaxation* relaxation) {
  CHECK_EQ(model->GetOrCreate<SatSolver>()->CurrentDecisionLevel(), 0);
  DCHECK_GT(linearization_level, 0);

  switch (ct.constraint_case()) {
    case ConstraintProto::ConstraintCase::kBoolOr: {
      if (linearization_level > 1) {
        AppendBoolOrRelaxation(ct, model, relaxation);
      }
      break;
    }
    case ConstraintProto::ConstraintCase::kBoolAnd: {
      if (linearization_level > 1) {
        AppendBoolAndRelaxation(ct, model, relaxation);
      }
      break;
    }
    case ConstraintProto::ConstraintCase::kAtMostOne: {
      AppendAtMostOneRelaxation(ct, model, relaxation);
      break;
    }
    case ConstraintProto::ConstraintCase::kExactlyOne: {
      AppendExactlyOneRelaxation(ct, model, relaxation);
      break;
    }
    case ConstraintProto::ConstraintCase::kIntProd: {
      // No relaxation, just a cut generator .
      AddIntProdCutGenerator(ct, linearization_level, model, relaxation);
      break;
    }
    case ConstraintProto::ConstraintCase::kLinMax: {
      AppendLinMaxRelaxationPart1(ct, model, relaxation);
      const bool is_affine_max = LinMaxContainsOnlyOneVarInExpressions(ct);
      if (is_affine_max) {
        AppendMaxAffineRelaxation(ct, model, relaxation);
      }

      // Add cut generators.
      if (linearization_level > 1) {
        if (is_affine_max) {
          AddMaxAffineCutGenerator(ct, model, relaxation);
        } else {
          AddLinMaxCutGenerator(ct, model, relaxation);
        }
      }
      break;
    }
    case ConstraintProto::ConstraintCase::kAllDiff: {
      if (linearization_level > 1) {
        AddAllDiffCutGenerator(ct, model, relaxation);
      }
      break;
    }
    case ConstraintProto::ConstraintCase::kLinear: {
      AppendLinearConstraintRelaxation(
          ct, /*linearize_enforced_constraints=*/linearization_level > 1, model,
          relaxation);
      break;
    }
    case ConstraintProto::ConstraintCase::kCircuit: {
      AppendCircuitRelaxation(ct, model, relaxation);
      if (linearization_level > 1) {
        AddCircuitCutGenerator(ct, model, relaxation);
      }
      break;
    }
    case ConstraintProto::ConstraintCase::kRoutes: {
      AppendRoutesRelaxation(ct, model, relaxation);
      if (linearization_level > 1) {
        AddRoutesCutGenerator(ct, model, relaxation);
      }
      break;
    }
    case ConstraintProto::ConstraintCase::kNoOverlap: {
      AppendNoOverlapRelaxationAndCutGenerator(ct, model, relaxation);
      break;
    }
    case ConstraintProto::ConstraintCase::kCumulative: {
      AppendCumulativeRelaxationAndCutGenerator(ct, model, relaxation);
      break;
    }
    case ConstraintProto::ConstraintCase::kNoOverlap2D: {
      // TODO(user): Use the same pattern as the other 2 scheduling methods:
      //   - single function
      //   - generate helpers once
      //
      // Adds an energetic relaxation (sum of areas fits in bounding box).
      AppendNoOverlap2dRelaxation(ct, model, relaxation);
      if (linearization_level > 1) {
        // Adds a completion time cut generator and an energetic cut generator.
        AddNoOverlap2dCutGenerator(ct, model, relaxation);
      }
      break;
    }
    default: {
    }
  }
}

// Cut generators.

void AddCircuitCutGenerator(const ConstraintProto& ct, Model* m,
                            LinearRelaxation* relaxation) {
  std::vector<int> tails(ct.circuit().tails().begin(),
                         ct.circuit().tails().end());
  std::vector<int> heads(ct.circuit().heads().begin(),
                         ct.circuit().heads().end());
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  std::vector<Literal> literals = mapping->Literals(ct.circuit().literals());
  const int num_nodes = ReindexArcs(&tails, &heads);

  relaxation->cut_generators.push_back(CreateStronglyConnectedGraphCutGenerator(
      num_nodes, tails, heads, literals, m));
}

void AddRoutesCutGenerator(const ConstraintProto& ct, Model* m,
                           LinearRelaxation* relaxation) {
  std::vector<int> tails(ct.routes().tails().begin(),
                         ct.routes().tails().end());
  std::vector<int> heads(ct.routes().heads().begin(),
                         ct.routes().heads().end());
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  std::vector<Literal> literals = mapping->Literals(ct.routes().literals());

  int num_nodes = 0;
  for (int i = 0; i < ct.routes().tails_size(); ++i) {
    num_nodes = std::max(num_nodes, 1 + ct.routes().tails(i));
    num_nodes = std::max(num_nodes, 1 + ct.routes().heads(i));
  }
  if (ct.routes().demands().empty() || ct.routes().capacity() == 0) {
    relaxation->cut_generators.push_back(
        CreateStronglyConnectedGraphCutGenerator(num_nodes, tails, heads,
                                                 literals, m));
  } else {
    const std::vector<int64_t> demands(ct.routes().demands().begin(),
                                       ct.routes().demands().end());
    relaxation->cut_generators.push_back(CreateCVRPCutGenerator(
        num_nodes, tails, heads, literals, demands, ct.routes().capacity(), m));
  }
}

void AddIntProdCutGenerator(const ConstraintProto& ct, int linearization_level,
                            Model* m, LinearRelaxation* relaxation) {
  if (HasEnforcementLiteral(ct)) return;
  if (ct.int_prod().exprs_size() != 2) return;
  auto* mapping = m->GetOrCreate<CpModelMapping>();

  // Constraint is z == x * y.

  AffineExpression z = mapping->Affine(ct.int_prod().target());
  AffineExpression x = mapping->Affine(ct.int_prod().exprs(0));
  AffineExpression y = mapping->Affine(ct.int_prod().exprs(1));

  IntegerTrail* const integer_trail = m->GetOrCreate<IntegerTrail>();
  IntegerValue x_lb = integer_trail->LowerBound(x);
  IntegerValue x_ub = integer_trail->UpperBound(x);
  IntegerValue y_lb = integer_trail->LowerBound(y);
  IntegerValue y_ub = integer_trail->UpperBound(y);

  if (x == y) {
    // We currently only support variables with non-negative domains.
    if (x_lb < 0 && x_ub > 0) return;

    // Change the sigh of x if its domain is non-positive.
    if (x_ub <= 0) {
      x = x.Negated();
    }

    relaxation->cut_generators.push_back(
        CreateSquareCutGenerator(z, x, linearization_level, m));
  } else {
    // We currently only support variables with non-negative domains.
    if (x_lb < 0 && x_ub > 0) return;
    if (y_lb < 0 && y_ub > 0) return;

    // Change signs to return to the case where all variables are a domain
    // with non negative values only.
    if (x_ub <= 0) {
      x = x.Negated();
      z = z.Negated();
    }
    if (y_ub <= 0) {
      y = y.Negated();
      z = z.Negated();
    }

    relaxation->cut_generators.push_back(
        CreatePositiveMultiplicationCutGenerator(z, x, y, linearization_level,
                                                 m));
  }
}

void AddAllDiffCutGenerator(const ConstraintProto& ct, Model* m,
                            LinearRelaxation* relaxation) {
  if (HasEnforcementLiteral(ct)) return;
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  const int num_exprs = ct.all_diff().exprs_size();

  if (num_exprs <= m->GetOrCreate<SatParameters>()->max_all_diff_cut_size()) {
    std::vector<AffineExpression> exprs(num_exprs);
    for (const LinearExpressionProto& expr : ct.all_diff().exprs()) {
      exprs.push_back(mapping->Affine(expr));
    }
    relaxation->cut_generators.push_back(
        CreateAllDifferentCutGenerator(exprs, m));
  }
}

bool IntervalIsVariable(const IntervalVariable interval,
                        IntervalsRepository* intervals_repository) {
  // Ignore absent rectangles.
  if (intervals_repository->IsAbsent(interval)) {
    return false;
  }

  // Checks non-present intervals.
  if (!intervals_repository->IsPresent(interval)) {
    return true;
  }

  // Checks variable sized intervals.
  if (intervals_repository->MinSize(interval) !=
      intervals_repository->MaxSize(interval)) {
    return true;
  }

  return false;
}

void AddCumulativeCutGenerator(const AffineExpression& capacity,
                               SchedulingConstraintHelper* helper,
                               SchedulingDemandHelper* demands_helper,
                               const std::optional<AffineExpression>& makespan,
                               Model* m, LinearRelaxation* relaxation) {
  relaxation->cut_generators.push_back(CreateCumulativeTimeTableCutGenerator(
      helper, demands_helper, capacity, m));
  relaxation->cut_generators.push_back(
      CreateCumulativeCompletionTimeCutGenerator(helper, demands_helper,
                                                 capacity, m));
  relaxation->cut_generators.push_back(CreateCumulativePrecedenceCutGenerator(
      helper, demands_helper, capacity, m));

  // Checks if at least one rectangle has a variable size, is optional, or if
  // the demand or the capacity are variable.
  bool has_variable_part = false;
  IntegerTrail* integer_trail = m->GetOrCreate<IntegerTrail>();
  for (int i = 0; i < helper->NumTasks(); ++i) {
    if (!helper->SizeIsFixed(i)) {
      has_variable_part = true;
      break;
    }
    // Checks variable demand.
    if (!demands_helper->DemandIsFixed(i)) {
      has_variable_part = true;
      break;
    }
  }
  if (has_variable_part || !integer_trail->IsFixed(capacity)) {
    relaxation->cut_generators.push_back(CreateCumulativeEnergyCutGenerator(
        helper, demands_helper, capacity, makespan, m));
  }
}

void AddNoOverlapCutGenerator(SchedulingConstraintHelper* helper,
                              const std::optional<AffineExpression>& makespan,
                              Model* m, LinearRelaxation* relaxation) {
  relaxation->cut_generators.push_back(
      CreateNoOverlapPrecedenceCutGenerator(helper, m));
  relaxation->cut_generators.push_back(
      CreateNoOverlapCompletionTimeCutGenerator(helper, m));

  // Checks if at least one rectangle has a variable size or is optional.
  bool has_variable_or_optional_part = false;
  for (int i = 0; i < helper->NumTasks(); ++i) {
    if (helper->IsAbsent(i)) continue;
    if (!helper->SizeIsFixed(i) || !helper->IsPresent(i)) {
      has_variable_or_optional_part = true;
      break;
    }
  }
  if (has_variable_or_optional_part) {
    relaxation->cut_generators.push_back(
        CreateNoOverlapEnergyCutGenerator(helper, makespan, m));
  }
}

void AddNoOverlap2dCutGenerator(const ConstraintProto& ct, Model* m,
                                LinearRelaxation* relaxation) {
  if (HasEnforcementLiteral(ct)) return;

  auto* mapping = m->GetOrCreate<CpModelMapping>();
  std::vector<IntervalVariable> x_intervals =
      mapping->Intervals(ct.no_overlap_2d().x_intervals());
  std::vector<IntervalVariable> y_intervals =
      mapping->Intervals(ct.no_overlap_2d().y_intervals());
  relaxation->cut_generators.push_back(
      CreateNoOverlap2dCompletionTimeCutGenerator(x_intervals, y_intervals, m));

  // Checks if at least one rectangle has a variable dimension or is optional.
  IntervalsRepository* intervals_repository =
      m->GetOrCreate<IntervalsRepository>();
  bool has_variable_part = false;
  for (int i = 0; i < x_intervals.size(); ++i) {
    // Ignore absent rectangles.
    if (intervals_repository->IsAbsent(x_intervals[i]) ||
        intervals_repository->IsAbsent(y_intervals[i])) {
      continue;
    }

    // Checks non-present intervals.
    if (!intervals_repository->IsPresent(x_intervals[i]) ||
        !intervals_repository->IsPresent(y_intervals[i])) {
      has_variable_part = true;
      break;
    }

    // Checks variable sized intervals.
    if (intervals_repository->MinSize(x_intervals[i]) !=
            intervals_repository->MaxSize(x_intervals[i]) ||
        intervals_repository->MinSize(y_intervals[i]) !=
            intervals_repository->MaxSize(y_intervals[i])) {
      has_variable_part = true;
      break;
    }
  }
  if (has_variable_part) {
    relaxation->cut_generators.push_back(
        CreateNoOverlap2dEnergyCutGenerator(x_intervals, y_intervals, m));
  }
}

void AddLinMaxCutGenerator(const ConstraintProto& ct, Model* m,
                           LinearRelaxation* relaxation) {
  if (!m->GetOrCreate<SatParameters>()->add_lin_max_cuts()) return;
  if (HasEnforcementLiteral(ct)) return;

  // TODO(user): Support linearization of general target expression.
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  if (ct.lin_max().target().vars_size() != 1) return;
  if (ct.lin_max().target().coeffs(0) != 1) return;
  if (ct.lin_max().target().offset() != 0) return;

  const IntegerVariable target =
      mapping->Integer(ct.lin_max().target().vars(0));
  std::vector<LinearExpression> exprs;
  exprs.reserve(ct.lin_max().exprs_size());
  for (int i = 0; i < ct.lin_max().exprs_size(); ++i) {
    // Note: Cut generator requires all expressions to contain only positive
    // vars.
    exprs.push_back(
        PositiveVarExpr(mapping->GetExprFromProto(ct.lin_max().exprs(i))));
  }

  const std::vector<Literal> alternative_literals =
      CreateAlternativeLiteralsWithView(exprs.size(), m, relaxation);

  // TODO(user): Move this out of here.
  //
  // Add initial big-M linear relaxation.
  // z_vars[i] == 1 <=> target = exprs[i].
  AppendLinMaxRelaxationPart2(target, alternative_literals, exprs, m,
                              relaxation);

  std::vector<IntegerVariable> z_vars;
  auto* encoder = m->GetOrCreate<IntegerEncoder>();
  for (const Literal lit : alternative_literals) {
    z_vars.push_back(encoder->GetLiteralView(lit));
    CHECK_NE(z_vars.back(), kNoIntegerVariable);
  }
  relaxation->cut_generators.push_back(
      CreateLinMaxCutGenerator(target, exprs, z_vars, m));
}

// If we have an exactly one between literals l_i, and each l_i => var ==
// value_i, then we can add a strong linear relaxation: var = sum l_i * value_i.
//
// This codes detect this and add the corresponding linear equations.
//
// TODO(user): We can do something similar with just an at most one, however
// it is harder to detect that if all literal are false then none of the implied
// value can be taken.
void AppendElementEncodingRelaxation(Model* m, LinearRelaxation* relaxation) {
  auto* implied_bounds = m->GetOrCreate<ImpliedBounds>();

  int num_exactly_one_elements = 0;

  for (const IntegerVariable var :
       implied_bounds->GetElementEncodedVariables()) {
    for (const auto& [index, literal_value_list] :
         implied_bounds->GetElementEncodings(var)) {
      // We only want to deal with the case with duplicate values, because
      // otherwise, the target will be fully encoded, and this is already
      // covered by another function.
      IntegerValue min_value = kMaxIntegerValue;
      {
        absl::flat_hash_set<IntegerValue> values;
        for (const auto& literal_value : literal_value_list) {
          min_value = std::min(min_value, literal_value.value);
          values.insert(literal_value.value);
        }
        if (values.size() == literal_value_list.size()) continue;
      }

      LinearConstraintBuilder linear_encoding(m, -min_value, -min_value);
      linear_encoding.AddTerm(var, IntegerValue(-1));
      for (const auto& [value, literal] : literal_value_list) {
        const IntegerValue delta_min = value - min_value;
        if (delta_min != 0) {
          // If the term has no view, we abort.
          if (!linear_encoding.AddLiteralTerm(literal, delta_min)) {
            return;
          }
        }
      }
      ++num_exactly_one_elements;
      relaxation->linear_constraints.push_back(linear_encoding.Build());
    }
  }

  if (num_exactly_one_elements != 0) {
    auto* logger = m->GetOrCreate<SolverLogger>();
    SOLVER_LOG(logger,
               "[ElementLinearRelaxation]"
               " #from_exactly_one:",
               num_exactly_one_elements);
  }
}

LinearRelaxation ComputeLinearRelaxation(const CpModelProto& model_proto,
                                         Model* m) {
  LinearRelaxation relaxation;

  // Linearize the constraints.
  const SatParameters& params = *m->GetOrCreate<SatParameters>();
  for (const auto& ct : model_proto.constraints()) {
    TryToLinearizeConstraint(model_proto, ct, params.linearization_level(), m,
                             &relaxation);
  }

  // Linearize the encoding of variable that are fully encoded.
  int num_loose_equality_encoding_relaxations = 0;
  int num_tight_equality_encoding_relaxations = 0;
  int num_inequality_encoding_relaxations = 0;
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  for (int i = 0; i < model_proto.variables_size(); ++i) {
    if (mapping->IsBoolean(i)) continue;

    const IntegerVariable var = mapping->Integer(i);
    if (m->Get(IsFixed(var))) continue;

    // We first try to linerize the values encoding.
    AppendRelaxationForEqualityEncoding(
        var, *m, &relaxation, &num_tight_equality_encoding_relaxations,
        &num_loose_equality_encoding_relaxations);

    // The we try to linearize the inequality encoding. Note that on some
    // problem like pizza27i.mps.gz, adding both equality and inequality
    // encoding is a must.
    //
    // Even if the variable is fully encoded, sometimes not all its associated
    // literal have a view (if they are not part of the original model for
    // instance).
    //
    // TODO(user): Should we add them to the LP anyway? this isn't clear as
    // we can sometimes create a lot of Booleans like this.
    const int old = relaxation.linear_constraints.size();
    AppendPartialGreaterThanEncodingRelaxation(var, *m, &relaxation);
    if (relaxation.linear_constraints.size() > old) {
      ++num_inequality_encoding_relaxations;
    }
  }

  // TODO(user): This is similar to AppendRelaxationForEqualityEncoding() above.
  // Investigate if we can merge the code.
  if (params.linearization_level() >= 2) {
    AppendElementEncodingRelaxation(m, &relaxation);
  }

  // TODO(user): I am not sure this is still needed. Investigate and explain why
  // or remove.
  if (!m->GetOrCreate<SatSolver>()->FinishPropagation()) {
    return relaxation;
  }

  // We display the stats before linearizing the at most ones.
  auto* logger = m->GetOrCreate<SolverLogger>();
  if (num_tight_equality_encoding_relaxations != 0 ||
      num_loose_equality_encoding_relaxations != 0 ||
      num_inequality_encoding_relaxations != 0) {
    SOLVER_LOG(logger,
               "[EncodingLinearRelaxation]"
               " #tight_equality:",
               num_tight_equality_encoding_relaxations,
               " #loose_equality:", num_loose_equality_encoding_relaxations,
               " #inequality:", num_inequality_encoding_relaxations);
  }
  if (!relaxation.linear_constraints.empty() ||
      !relaxation.at_most_ones.empty()) {
    SOLVER_LOG(logger,
               "[LinearRelaxationBeforeCliqueExpansion]"
               " #linear:",
               relaxation.linear_constraints.size(),
               " #at_most_ones:", relaxation.at_most_ones.size());
  }

  // Linearize the at most one constraints. Note that we transform them
  // into maximum "at most one" first and we removes redundant ones.
  m->GetOrCreate<BinaryImplicationGraph>()->TransformIntoMaxCliques(
      &relaxation.at_most_ones,
      SafeDoubleToInt64(params.merge_at_most_one_work_limit()));
  for (const std::vector<Literal>& at_most_one : relaxation.at_most_ones) {
    if (at_most_one.empty()) continue;

    LinearConstraintBuilder lc(m, kMinIntegerValue, IntegerValue(1));
    for (const Literal literal : at_most_one) {
      // Note that it is okay to simply ignore the literal if it has no
      // integer view.
      const bool unused ABSL_ATTRIBUTE_UNUSED =
          lc.AddLiteralTerm(literal, IntegerValue(1));
    }
    relaxation.linear_constraints.push_back(lc.Build());
  }

  // We converted all at_most_one to LP constraints, so we need to clear them
  // so that we don't do extra work in the connected component computation.
  relaxation.at_most_ones.clear();

  // Remove size one LP constraints, they are not useful.
  relaxation.linear_constraints.erase(
      std::remove_if(
          relaxation.linear_constraints.begin(),
          relaxation.linear_constraints.end(),
          [](const LinearConstraint& lc) { return lc.vars.size() <= 1; }),
      relaxation.linear_constraints.end());

  // We add a clique cut generation over all Booleans of the problem.
  // Note that in practice this might regroup independent LP together.
  //
  // TODO(user): compute connected components of the original problem and
  // split these cuts accordingly.
  if (params.linearization_level() > 1 && params.add_clique_cuts()) {
    LinearConstraintBuilder builder(m);
    for (int i = 0; i < model_proto.variables_size(); ++i) {
      if (!mapping->IsBoolean(i)) continue;

      // Note that it is okay to simply ignore the literal if it has no
      // integer view.
      const bool unused ABSL_ATTRIBUTE_UNUSED =
          builder.AddLiteralTerm(mapping->Literal(i), IntegerValue(1));
    }

    // We add a generator touching all the variable in the builder.
    const LinearExpression& expr = builder.BuildExpression();
    if (!expr.vars.empty()) {
      relaxation.cut_generators.push_back(
          CreateCliqueCutGenerator(expr.vars, m));
    }
  }

  if (!relaxation.linear_constraints.empty() ||
      !relaxation.cut_generators.empty()) {
    SOLVER_LOG(logger,
               "[FinalLinearRelaxation]"
               " #linear:",
               relaxation.linear_constraints.size(),
               " #cut_generators:", relaxation.cut_generators.size());
  }

  return relaxation;
}

}  // namespace sat
}  // namespace operations_research
