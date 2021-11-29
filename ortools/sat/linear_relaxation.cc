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

#include "ortools/sat/linear_relaxation.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "ortools/base/iterator_adaptors.h"
#include "ortools/base/stl_util.h"
#include "ortools/sat/circuit.h"  // for ReindexArcs.
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_mapping.h"
#include "ortools/sat/cuts.h"
#include "ortools/sat/implied_bounds.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_expr.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/linear_programming_constraint.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/scheduling_constraints.h"
#include "ortools/sat/scheduling_cuts.h"

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

// TODO(user): Not super efficient.
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
  const auto& domain = (*domains)[var];
  for (int i = domain.NumIntervals() - 1; i >= 0; --i) {
    const ClosedInterval interval = domain[i];
    for (IntegerValue v(interval.end); v >= interval.start; --v) {
      if (!encoded_values.contains(v)) {
        max = v;
        break;
      }
    }
    if (max != kMinIntegerValue) break;
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

bool IntMaxIsIntAbs(const ConstraintProto& ct) {
  if (ct.constraint_case() != ConstraintProto::ConstraintCase::kIntMax ||
      ct.int_max().vars_size() != 2) {
    return false;
  }
  return ct.int_max().vars(0) == NegatedRef(ct.int_max().vars(1));
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

void AppendPartialEncodingRelaxation(IntegerVariable var, const Model& model,
                                     LinearRelaxation* relaxation) {
  const auto* encoder = model.Get<IntegerEncoder>();
  const auto* integer_trail = model.Get<IntegerTrail>();
  if (encoder == nullptr || integer_trail == nullptr) return;

  const std::vector<IntegerEncoder::ValueLiteralPair>& encoding =
      encoder->PartialDomainEncoding(var);
  if (encoding.empty()) return;

  std::vector<Literal> at_most_one_ct;
  absl::flat_hash_set<IntegerValue> encoded_values;
  for (const auto value_literal : encoding) {
    const Literal literal = value_literal.literal;

    // Note that we skip pairs that do not have an Integer view.
    if (encoder->GetLiteralView(literal) == kNoIntegerVariable &&
        encoder->GetLiteralView(literal.Negated()) == kNoIntegerVariable) {
      continue;
    }

    at_most_one_ct.push_back(literal);
    encoded_values.insert(value_literal.value);
  }
  if (encoded_values.empty()) return;

  // TODO(user): The PartialDomainEncoding() function automatically exclude
  // values that are no longer in the initial domain, so we could be a bit
  // tighter here. That said, this is supposed to be called just after the
  // presolve, so it shouldn't really matter.
  const auto pair = GetMinAndMaxNotEncoded(var, encoded_values, model);
  if (pair.first == kMaxIntegerValue) {
    // TODO(user): try to remove the duplication with
    // AppendFullEncodingRelaxation()? actually I am not sure we need the other
    // function since this one is just more general.
    LinearConstraintBuilder exactly_one_ct(&model, IntegerValue(1),
                                           IntegerValue(1));
    LinearConstraintBuilder encoding_ct(&model, IntegerValue(0),
                                        IntegerValue(0));
    encoding_ct.AddTerm(var, IntegerValue(1));
    for (const auto value_literal : encoding) {
      const Literal lit = value_literal.literal;
      CHECK(exactly_one_ct.AddLiteralTerm(lit, IntegerValue(1)));
      CHECK(
          encoding_ct.AddLiteralTerm(lit, IntegerValue(-value_literal.value)));
    }
    relaxation->linear_constraints.push_back(exactly_one_ct.Build());
    relaxation->linear_constraints.push_back(encoding_ct.Build());
    return;
  }

  // min + sum li * (xi - min) <= var.
  const IntegerValue d_min = pair.first;
  LinearConstraintBuilder lower_bound_ct(&model, d_min, kMaxIntegerValue);
  lower_bound_ct.AddTerm(var, IntegerValue(1));
  for (const auto value_literal : encoding) {
    CHECK(lower_bound_ct.AddLiteralTerm(value_literal.literal,
                                        d_min - value_literal.value));
  }

  // var <= max + sum li * (xi - max).
  const IntegerValue d_max = pair.second;
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
}

void AppendPartialGreaterThanEncodingRelaxation(IntegerVariable var,
                                                const Model& model,
                                                LinearRelaxation* relaxation) {
  const auto* integer_trail = model.Get<IntegerTrail>();
  const auto* encoder = model.Get<IntegerEncoder>();
  if (integer_trail == nullptr || encoder == nullptr) return;

  const std::map<IntegerValue, Literal>& greater_than_encoding =
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
// Adds enforcing_lit => target <= bounding_var to relaxation.
void AppendEnforcedUpperBound(const Literal enforcing_lit,
                              const IntegerVariable target,
                              const IntegerVariable bounding_var, Model* model,
                              LinearRelaxation* relaxation) {
  IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
  const IntegerValue max_target_value = integer_trail->UpperBound(target);
  const IntegerValue min_var_value = integer_trail->LowerBound(bounding_var);
  const IntegerValue max_term_value = max_target_value - min_var_value;
  LinearConstraintBuilder lc(model, kMinIntegerValue, max_term_value);
  lc.AddTerm(target, IntegerValue(1));
  lc.AddTerm(bounding_var, IntegerValue(-1));
  CHECK(lc.AddLiteralTerm(enforcing_lit, max_term_value));
  relaxation->linear_constraints.push_back(lc.Build());
}

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
  const IntegerValue min_expr_value =
      LinExprLowerBound(canonical_expr, *integer_trail);

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
  const IntegerValue max_expr_value =
      LinExprUpperBound(canonical_expr, *integer_trail);
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

namespace {

void AddIntMaxLowerRelaxation(IntegerVariable target,
                              const std::vector<IntegerVariable>& vars,
                              Model* model, LinearRelaxation* relaxation) {
  // Case X = max(X_1, X_2, ..., X_N)
  // Part 1: Encode X >= max(X_1, X_2, ..., X_N)
  for (const IntegerVariable var : vars) {
    // This deal with the corner case X = max(X, Y, Z, ..) !
    // Note that this can be presolved into X >= Y, X >= Z, ...
    if (target == var) continue;
    LinearConstraintBuilder lc(model, kMinIntegerValue, IntegerValue(0));
    lc.AddTerm(var, IntegerValue(1));
    lc.AddTerm(target, IntegerValue(-1));
    relaxation->linear_constraints.push_back(lc.Build());
  }
}

void AddIntAbsUpperRelaxation(IntegerVariable target, IntegerVariable var,
                              Model* model, LinearRelaxation* relaxation) {
  LinearExpression target_expr;
  target_expr.vars.push_back(target);
  target_expr.coeffs.push_back(IntegerValue(1));
  const std::vector<std::pair<IntegerValue, IntegerValue>> affines = {
      {IntegerValue(1), IntegerValue(0)}, {IntegerValue(-1), IntegerValue(0)}};
  relaxation->linear_constraints.push_back(
      BuildMaxAffineUpConstraint(target_expr, var, affines, model));
}

void AddIntMaxUpperRelaxation(IntegerVariable target,
                              const std::vector<IntegerVariable>& vars,
                              Model* model, LinearRelaxation* relaxation) {
  // For each X_i, we encode l_i => X <= X_i. And at least one of the l_i is
  // true. Note that the correct y_i will be chosen because of the first part in
  // linearlization (X >= X_i).
  GenericLiteralWatcher* watcher = model->GetOrCreate<GenericLiteralWatcher>();
  std::vector<Literal> literals =
      CreateAlternativeLiteralsWithView(vars.size(), model, relaxation);
  for (int i = 0; i < vars.size(); ++i) {
    // TODO(user): Only lower bound is needed, experiment.
    //
    // TODO(user): It makes more sense to use ConditionalLowerOrEqual()
    // here since only the lower bounding is needed, but that degrades perf on
    // the road*.fzn problem. Understand why.
    AppendEnforcedUpperBound(literals[i], target, vars[i], model, relaxation);
    IntegerSumLE* upper_bound_constraint = new IntegerSumLE(
        {literals[i]}, {target, vars[i]}, {IntegerValue(1), IntegerValue(-1)},
        IntegerValue(0), model);
    upper_bound_constraint->RegisterWith(watcher);
    model->TakeOwnership(upper_bound_constraint);
  }
}

}  // namespace

// Adds linearization of int max constraints. This can also be used to linearize
// int min with negated variables.
void AppendIntMaxRelaxation(const ConstraintProto& ct, int linearization_level,
                            Model* model, LinearRelaxation* relaxation) {
  if (HasEnforcementLiteral(ct)) return;

  auto* mapping = model->GetOrCreate<CpModelMapping>();
  const IntegerVariable target = mapping->Integer(ct.int_max().target());
  const std::vector<IntegerVariable> vars =
      mapping->Integers(ct.int_max().vars());

  AddIntMaxLowerRelaxation(target, vars, model, relaxation);
  if (IntMaxIsIntAbs(ct)) {
    // TODO(user): consider support for int_abs encoded using int_min.
    AddIntAbsUpperRelaxation(target, PositiveVariable(vars[0]), model,
                             relaxation);
  } else if (linearization_level > 1) {
    AddIntMaxUpperRelaxation(target, vars, model, relaxation);
  }
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
  std::map<int, std::vector<Literal>> incoming_arc_constraints;
  std::map<int, std::vector<Literal>> outgoing_arc_constraints;
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
  std::map<int, std::vector<Literal>> incoming_arc_constraints;
  std::map<int, std::vector<Literal>> outgoing_arc_constraints;
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

void AppendIntervalRelaxation(const ConstraintProto& ct, Model* model,
                              LinearRelaxation* relaxation) {
  // If the interval is using views, then the linear equation is already
  // present in the model.
  if (ct.interval().has_start_view()) return;

  auto* mapping = model->GetOrCreate<CpModelMapping>();
  const IntegerVariable start = mapping->Integer(ct.interval().start());
  const IntegerVariable size = mapping->Integer(ct.interval().size());
  const IntegerVariable end = mapping->Integer(ct.interval().end());
  IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
  const bool size_is_fixed = integer_trail->IsFixed(size);
  const IntegerValue rhs =
      size_is_fixed ? -integer_trail->LowerBound(size) : IntegerValue(0);
  LinearConstraintBuilder lc(model, rhs, rhs);
  lc.AddTerm(start, IntegerValue(1));
  if (!size_is_fixed) {
    lc.AddTerm(size, IntegerValue(1));
  }
  lc.AddTerm(end, IntegerValue(-1));
  if (HasEnforcementLiteral(ct)) {
    LinearConstraint tmp_lc = lc.Build();
    LinearExpression expr;
    expr.coeffs = tmp_lc.coeffs;
    expr.vars = tmp_lc.vars;
    AppendEnforcedLinearExpression(mapping->Literals(ct.enforcement_literal()),
                                   expr, tmp_lc.ub, tmp_lc.ub, *model,
                                   relaxation);
  } else {
    relaxation->linear_constraints.push_back(lc.Build());
  }
}

// TODO(user): Use affine demand.
void AddCumulativeRelaxation(const std::vector<IntervalVariable>& intervals,
                             const std::vector<IntegerVariable>& demands,
                             const std::vector<LinearExpression>& energies,
                             IntegerValue capacity_upper_bound, Model* model,
                             LinearRelaxation* relaxation) {
  // TODO(user): Keep a map intervals -> helper, or ct_index->helper to avoid
  // creating many helpers for the same constraint.
  auto* helper = new SchedulingConstraintHelper(intervals, model);
  model->TakeOwnership(helper);
  const int num_intervals = helper->NumTasks();

  IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();

  IntegerValue min_of_starts = kMaxIntegerValue;
  IntegerValue max_of_ends = kMinIntegerValue;

  int num_variable_sizes = 0;
  int num_optionals = 0;

  for (int index = 0; index < num_intervals; ++index) {
    min_of_starts = std::min(min_of_starts, helper->StartMin(index));
    max_of_ends = std::max(max_of_ends, helper->EndMax(index));

    if (helper->IsOptional(index)) {
      num_optionals++;
    }

    if (!helper->SizeIsFixed(index) ||
        (!demands.empty() && !integer_trail->IsFixed(demands[index]))) {
      num_variable_sizes++;
    }
  }

  VLOG(2) << "Span [" << min_of_starts << ".." << max_of_ends << "] with "
          << num_optionals << " optional intervals, and " << num_variable_sizes
          << " variable size intervals out of " << num_intervals
          << " intervals";

  if (num_variable_sizes + num_optionals == 0) return;

  const IntegerVariable span_start =
      integer_trail->AddIntegerVariable(min_of_starts, max_of_ends);
  const IntegerVariable span_size = integer_trail->AddIntegerVariable(
      IntegerValue(0), max_of_ends - min_of_starts);
  const IntegerVariable span_end =
      integer_trail->AddIntegerVariable(min_of_starts, max_of_ends);

  IntervalVariable span_var;
  if (num_optionals < num_intervals) {
    span_var = model->Add(NewInterval(span_start, span_end, span_size));
  } else {
    const Literal span_lit = Literal(model->Add(NewBooleanVariable()), true);
    span_var = model->Add(
        NewOptionalInterval(span_start, span_end, span_size, span_lit));
  }

  model->Add(SpanOfIntervals(span_var, intervals));

  LinearConstraintBuilder lc(model, kMinIntegerValue, IntegerValue(0));
  lc.AddTerm(span_size, -capacity_upper_bound);
  for (int i = 0; i < num_intervals; ++i) {
    const IntegerValue demand_lower_bound =
        demands.empty() ? IntegerValue(1)
                        : integer_trail->LowerBound(demands[i]);
    const bool demand_is_fixed =
        demands.empty() || integer_trail->IsFixed(demands[i]);
    if (!helper->IsOptional(i)) {
      if (demand_is_fixed) {
        lc.AddTerm(helper->Sizes()[i], demand_lower_bound);
      } else if (!helper->SizeIsFixed(i) && !energies.empty()) {
        // We prefer the energy additional info instead of the McCormick
        // relaxation.
        lc.AddLinearExpression(energies[i]);
      } else {
        lc.AddQuadraticLowerBound(helper->Sizes()[i], demands[i],
                                  integer_trail);
      }
    } else {
      if (!lc.AddLiteralTerm(helper->PresenceLiteral(i),
                             helper->SizeMin(i) * demand_lower_bound)) {
        return;
      }
    }
  }
  relaxation->linear_constraints.push_back(lc.Build());
}

void AppendCumulativeRelaxation(const CpModelProto& model_proto,
                                const ConstraintProto& ct, Model* model,
                                LinearRelaxation* relaxation) {
  CHECK(ct.has_cumulative());
  if (HasEnforcementLiteral(ct)) return;

  auto* mapping = model->GetOrCreate<CpModelMapping>();
  const std::vector<IntegerVariable> demands =
      mapping->Integers(ct.cumulative().demands());
  std::vector<IntervalVariable> intervals =
      mapping->Intervals(ct.cumulative().intervals());
  const IntegerValue capacity_upper_bound =
      model->GetOrCreate<IntegerTrail>()->UpperBound(
          mapping->Integer(ct.cumulative().capacity()));
  std::vector<LinearExpression> energies;
  energies.reserve(ct.cumulative().energies_size());
  for (int i = 0; i < ct.cumulative().energies_size(); ++i) {
    // Note: Cut generator requires all expressions to contain only positive
    // vars.
    energies.push_back(mapping->GetExprFromProto(ct.cumulative().energies(i)));
  }

  AddCumulativeRelaxation(intervals, demands, energies, capacity_upper_bound,
                          model, relaxation);
}

void AppendNoOverlapRelaxation(const CpModelProto& model_proto,
                               const ConstraintProto& ct, Model* model,
                               LinearRelaxation* relaxation) {
  CHECK(ct.has_no_overlap());
  if (HasEnforcementLiteral(ct)) return;

  auto* mapping = model->GetOrCreate<CpModelMapping>();
  std::vector<IntervalVariable> intervals =
      mapping->Intervals(ct.no_overlap().intervals());
  AddCumulativeRelaxation(intervals, /*demands=*/{}, /*energies=*/{},
                          /*capacity_upper_bound=*/IntegerValue(1), model,
                          relaxation);
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
  relaxation->linear_constraints.push_back(
      BuildMaxAffineUpConstraint(target_expr, var, affines, model));
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

  CHECK_EQ(1, ct.lin_max().target().vars_size());
  const LinearExpression target_expr =
      PositiveVarExpr(mapping->GetExprFromProto(ct.lin_max().target()));
  relaxation->cut_generators.push_back(CreateMaxAffineCutGenerator(
      target_expr, var, affines, "AffineMax", model));
}

void AddIntAbsCutGenerator(const ConstraintProto& ct, Model* model,
                           LinearRelaxation* relaxation) {
  auto* mapping = model->GetOrCreate<CpModelMapping>();
  const IntegerVariable var =
      PositiveVariable(mapping->Integer(ct.int_max().vars(0)));

  LinearExpression target_expr;
  target_expr.vars.push_back(mapping->Integer(ct.int_max().target()));
  target_expr.coeffs.push_back(IntegerValue(1));
  const std::vector<std::pair<IntegerValue, IntegerValue>> affines = {
      {IntegerValue(1), IntegerValue(0)}, {IntegerValue(-1), IntegerValue(0)}};

  relaxation->cut_generators.push_back(
      CreateMaxAffineCutGenerator(target_expr, var, affines, "IntAbs", model));
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

// Add a linear relaxation of the CP constraint to the set of linear
// constraints. The highest linearization_level is, the more types of constraint
// we encode. This method should be called only for linearization_level > 0.
//
// Note: IntProd is linearized dynamically using the cut generators.
//
// TODO(user): In full generality, we could encode all the constraint as an LP.
// TODO(user): Add unit tests for this method.
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
    case ConstraintProto::ConstraintCase::kIntMax: {
      AppendIntMaxRelaxation(ct, linearization_level, model, relaxation);
      break;
    }
    case ConstraintProto::ConstraintCase::kLinMax: {
      AppendLinMaxRelaxationPart1(ct, model, relaxation);
      if (LinMaxContainsOnlyOneVarInExpressions(ct)) {
        AppendMaxAffineRelaxation(ct, model, relaxation);
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
      break;
    }
    case ConstraintProto::ConstraintCase::kRoutes: {
      AppendRoutesRelaxation(ct, model, relaxation);
      break;
    }
    case ConstraintProto::ConstraintCase::kInterval: {
      if (linearization_level > 1) {
        AppendIntervalRelaxation(ct, model, relaxation);
      }
      break;
    }
    case ConstraintProto::ConstraintCase::kNoOverlap: {
      if (linearization_level > 1) {
        AppendNoOverlapRelaxation(model_proto, ct, model, relaxation);
      }
      break;
    }
    case ConstraintProto::ConstraintCase::kCumulative: {
      if (linearization_level > 1) {
        AppendCumulativeRelaxation(model_proto, ct, model, relaxation);
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
  if (ct.int_prod().vars_size() != 2) return;
  auto* mapping = m->GetOrCreate<CpModelMapping>();

  // Constraint is z == x * y.

  IntegerVariable z = mapping->Integer(ct.int_prod().target());
  IntegerVariable x = mapping->Integer(ct.int_prod().vars(0));
  IntegerVariable y = mapping->Integer(ct.int_prod().vars(1));

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
      x = NegationOf(x);
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
      x = NegationOf(x);
      z = NegationOf(z);
    }
    if (y_ub <= 0) {
      y = NegationOf(y);
      z = NegationOf(z);
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
  const int num_vars = ct.all_diff().vars_size();
  if (num_vars <= m->GetOrCreate<SatParameters>()->max_all_diff_cut_size()) {
    std::vector<IntegerVariable> vars = mapping->Integers(ct.all_diff().vars());
    relaxation->cut_generators.push_back(
        CreateAllDifferentCutGenerator(vars, m));
  }
}

void AddCumulativeCutGenerator(const ConstraintProto& ct, Model* m,
                               LinearRelaxation* relaxation) {
  if (HasEnforcementLiteral(ct)) return;
  auto* mapping = m->GetOrCreate<CpModelMapping>();

  const std::vector<IntegerVariable> demands =
      mapping->Integers(ct.cumulative().demands());
  const std::vector<IntervalVariable> intervals =
      mapping->Intervals(ct.cumulative().intervals());
  const IntegerVariable capacity = mapping->Integer(ct.cumulative().capacity());
  std::vector<LinearExpression> energies;
  energies.reserve(ct.cumulative().energies_size());
  for (int i = 0; i < ct.cumulative().energies_size(); ++i) {
    // Note: Cut generator requires all expressions to contain only positive
    // vars.
    energies.push_back(mapping->GetExprFromProto(ct.cumulative().energies(i)));
  }

  relaxation->cut_generators.push_back(
      CreateCumulativeTimeTableCutGenerator(intervals, capacity, demands, m));
  relaxation->cut_generators.push_back(CreateCumulativeEnergyCutGenerator(
      intervals, capacity, demands, energies, m));
  relaxation->cut_generators.push_back(
      CreateCumulativeCompletionTimeCutGenerator(intervals, capacity, demands,
                                                 energies, m));
  relaxation->cut_generators.push_back(
      CreateCumulativePrecedenceCutGenerator(intervals, capacity, demands, m));
}

void AddNoOverlapCutGenerator(const ConstraintProto& ct, Model* m,
                              LinearRelaxation* relaxation) {
  if (HasEnforcementLiteral(ct)) return;

  auto* mapping = m->GetOrCreate<CpModelMapping>();
  std::vector<IntervalVariable> intervals =
      mapping->Intervals(ct.no_overlap().intervals());
  relaxation->cut_generators.push_back(
      CreateNoOverlapEnergyCutGenerator(intervals, m));
  relaxation->cut_generators.push_back(
      CreateNoOverlapPrecedenceCutGenerator(intervals, m));
  relaxation->cut_generators.push_back(
      CreateNoOverlapCompletionTimeCutGenerator(intervals, m));
}

void AddNoOverlap2dCutGenerator(const ConstraintProto& ct, Model* m,
                                LinearRelaxation* relaxation) {
  if (HasEnforcementLiteral(ct)) return;

  auto* mapping = m->GetOrCreate<CpModelMapping>();
  std::vector<IntervalVariable> x_intervals =
      mapping->Intervals(ct.no_overlap_2d().x_intervals());
  std::vector<IntervalVariable> y_intervals =
      mapping->Intervals(ct.no_overlap_2d().y_intervals());
  // TODO(user): We can add CumulativeEnergyCuts for no_overlap_2d if boxes
  // do not have a fixed size.
  relaxation->cut_generators.push_back(
      CreateNoOverlap2dCompletionTimeCutGenerator(x_intervals, y_intervals, m));
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

// TODO(user): Remove and merge with model loading.
void TryToAddCutGenerators(const ConstraintProto& ct, int linearization_level,
                           Model* m, LinearRelaxation* relaxation) {
  switch (ct.constraint_case()) {
    case ConstraintProto::ConstraintCase::kCircuit: {
      if (linearization_level > 1) {
        AddCircuitCutGenerator(ct, m, relaxation);
      }
      break;
    }
    case ConstraintProto::ConstraintCase::kRoutes: {
      if (linearization_level > 1) {
        AddRoutesCutGenerator(ct, m, relaxation);
      }
      break;
    }
    case ConstraintProto::ConstraintCase::kIntProd: {
      AddIntProdCutGenerator(ct, linearization_level, m, relaxation);
      break;
    }
    case ConstraintProto::ConstraintCase::kAllDiff: {
      if (linearization_level > 1) {
        AddAllDiffCutGenerator(ct, m, relaxation);
      }
      break;
    }
    case ConstraintProto::ConstraintCase::kCumulative: {
      if (linearization_level > 1) {
        AddCumulativeCutGenerator(ct, m, relaxation);
      }
      break;
    }
    case ConstraintProto::ConstraintCase::kNoOverlap: {
      if (linearization_level > 1) {
        AddNoOverlapCutGenerator(ct, m, relaxation);
      }
      break;
    }
    case ConstraintProto::ConstraintCase::kNoOverlap2D: {
      if (linearization_level > 1) {
        AddNoOverlap2dCutGenerator(ct, m, relaxation);
      }
      break;
    }
    case ConstraintProto::ConstraintCase::kIntMax: {
      if (linearization_level > 1 && IntMaxIsIntAbs(ct)) {
        AddIntAbsCutGenerator(ct, m, relaxation);
      }
      break;
    }
    case ConstraintProto::ConstraintCase::kLinMax: {
      if (linearization_level > 1) {
        if (LinMaxContainsOnlyOneVarInExpressions(ct)) {
          AddMaxAffineCutGenerator(ct, m, relaxation);
        } else {
          AddLinMaxCutGenerator(ct, m, relaxation);
        }
      }
      break;
    }
    default: {
    }
  }
}

// If we have an exactly one between literals l_i, and each l_i => var ==
// value_i, then we can add a strong linear relaxation: var = sum l_i * value_i.
//
// This codes detect this and add the corresponding linear equations.
void AppendElementEncodingRelaxation(const CpModelProto& model_proto, Model* m,
                                     LinearRelaxation* relaxation) {
  auto* implied_bounds = m->GetOrCreate<ImpliedBounds>();
  auto* mapping = m->GetOrCreate<CpModelMapping>();

  for (const ConstraintProto& ct : model_proto.constraints()) {
    if (ct.constraint_case() != ConstraintProto::ConstraintCase::kExactlyOne) {
      continue;
    }

    // Project the implied values onto each integer variable.
    absl::flat_hash_map<IntegerVariable,
                        std::vector<std::pair<Literal, IntegerValue>>>
        var_to_literal_value_list;
    for (const int l : ct.exactly_one().literals()) {
      const Literal literal = mapping->Literal(l);
      for (const auto& var_value : implied_bounds->GetImpliedValues(literal)) {
        var_to_literal_value_list[var_value.first].push_back(
            std::make_pair(literal, var_value.second));
      }
    }

    // Search for variable fully covered by the literals of the exactly_one.
    for (const auto& var_encoding : var_to_literal_value_list) {
      if (var_encoding.second.size() < ct.exactly_one().literals_size()) {
        continue;
      }

      // We only want to deal with the case with duplicate values, because
      // otherwise, the target will be fully encoded, and this is already
      // covered by another function.
      IntegerValue min_value = kMaxIntegerValue;
      {
        absl::flat_hash_set<IntegerValue> values;
        for (const auto& literal_value : var_encoding.second) {
          min_value = std::min(min_value, literal_value.second);
          values.insert(literal_value.second);
        }
        if (values.size() == ct.exactly_one().literals_size()) continue;
      }

      LinearConstraintBuilder linear_encoding(m, -min_value, -min_value);
      linear_encoding.AddTerm(var_encoding.first, IntegerValue(-1));
      for (const auto& literal_value : var_encoding.second) {
        const IntegerValue delta_min = literal_value.second - min_value;
        if (delta_min != 0) {
          if (!linear_encoding.AddLiteralTerm(literal_value.first, delta_min)) {
            return;
          }
        }
      }
      relaxation->linear_constraints.push_back(linear_encoding.Build());
    }
  }
}

void ComputeLinearRelaxation(const CpModelProto& model_proto,
                             int linearization_level, Model* m,
                             LinearRelaxation* relaxation) {
  CHECK(relaxation != nullptr);

  // Linearize the constraints.
  absl::flat_hash_set<int> used_integer_variable;

  auto* mapping = m->GetOrCreate<CpModelMapping>();
  auto* encoder = m->GetOrCreate<IntegerEncoder>();
  for (const auto& ct : model_proto.constraints()) {
    TryToLinearizeConstraint(model_proto, ct, linearization_level, m,
                             relaxation);
    TryToAddCutGenerators(ct, linearization_level, m, relaxation);
  }

  // Linearize the encoding of variable that are fully encoded.
  int num_full_encoding_relaxations = 0;
  int num_partial_encoding_relaxations = 0;
  for (int i = 0; i < model_proto.variables_size(); ++i) {
    if (mapping->IsBoolean(i)) continue;

    const IntegerVariable var = mapping->Integer(i);
    if (m->Get(IsFixed(var))) continue;

    // TODO(user): This different encoding for the partial variable might be
    // better (less LP constraints), but we do need more investigation to
    // decide.
    if (/* DISABLES CODE */ (false)) {
      AppendPartialEncodingRelaxation(var, *m, relaxation);
      continue;
    }

    if (encoder->VariableIsFullyEncoded(var)) {
      if (AppendFullEncodingRelaxation(var, *m, relaxation)) {
        ++num_full_encoding_relaxations;
        continue;
      }
    }

    // Even if the variable is fully encoded, sometimes not all its associated
    // literal have a view (if they are not part of the original model for
    // instance).
    //
    // TODO(user): Should we add them to the LP anyway? this isn't clear as
    // we can sometimes create a lot of Booleans like this.
    const int old = relaxation->linear_constraints.size();
    AppendPartialGreaterThanEncodingRelaxation(var, *m, relaxation);
    if (relaxation->linear_constraints.size() > old) {
      ++num_partial_encoding_relaxations;
    }
  }

  // TODO(user): This is really similar to the AppendFullEncodingRelaxation()
  // above. Investigate if we can merge the code.
  if (linearization_level >= 2) {
    AppendElementEncodingRelaxation(model_proto, m, relaxation);
  }

  if (!m->GetOrCreate<SatSolver>()->FinishPropagation()) return;

  // Linearize the at most one constraints. Note that we transform them
  // into maximum "at most one" first and we removes redundant ones.
  m->GetOrCreate<BinaryImplicationGraph>()->TransformIntoMaxCliques(
      &relaxation->at_most_ones);
  for (const std::vector<Literal>& at_most_one : relaxation->at_most_ones) {
    if (at_most_one.empty()) continue;

    LinearConstraintBuilder lc(m, kMinIntegerValue, IntegerValue(1));
    for (const Literal literal : at_most_one) {
      // Note that it is okay to simply ignore the literal if it has no
      // integer view.
      const bool unused ABSL_ATTRIBUTE_UNUSED =
          lc.AddLiteralTerm(literal, IntegerValue(1));
    }
    relaxation->linear_constraints.push_back(lc.Build());
  }

  // We converted all at_most_one to LP constraints, so we need to clear them
  // so that we don't do extra work in the connected component computation.
  relaxation->at_most_ones.clear();

  // Remove size one LP constraints, they are not useful.
  relaxation->linear_constraints.erase(
      std::remove_if(
          relaxation->linear_constraints.begin(),
          relaxation->linear_constraints.end(),
          [](const LinearConstraint& lc) { return lc.vars.size() <= 1; }),
      relaxation->linear_constraints.end());

  VLOG(3) << "num_full_encoding_relaxations: " << num_full_encoding_relaxations;
  VLOG(3) << "num_partial_encoding_relaxations: "
          << num_partial_encoding_relaxations;
  VLOG(3) << relaxation->linear_constraints.size()
          << " constraints in the LP relaxation.";
  VLOG(3) << relaxation->cut_generators.size() << " cuts generators.";
}

}  // namespace sat
}  // namespace operations_research
