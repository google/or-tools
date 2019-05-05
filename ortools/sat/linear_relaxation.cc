// Copyright 2010-2018 Google LLC
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

#include "absl/container/flat_hash_set.h"
#include "ortools/base/iterator_adaptors.h"
#include "ortools/sat/cp_model_loader.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/linear_programming_constraint.h"

namespace operations_research {
namespace sat {

bool AppendFullEncodingRelaxation(IntegerVariable var, const Model& model,
                                  LinearRelaxation* relaxation) {
  const auto* encoder = model.Get<IntegerEncoder>();
  if (encoder == nullptr) return false;
  if (!encoder->VariableIsFullyEncoded(var)) return false;

  LinearConstraintBuilder at_least_one(&model, IntegerValue(1),
                                       kMaxIntegerValue);
  LinearConstraintBuilder encoding_ct(&model, IntegerValue(0), IntegerValue(0));
  encoding_ct.AddTerm(var, IntegerValue(1));

  // Create the constraint if all literal have a view.
  std::vector<Literal> at_most_one;
  for (const auto value_literal : encoder->FullDomainEncoding(var)) {
    const Literal lit = value_literal.literal;
    at_most_one.push_back(lit);
    if (!at_least_one.AddLiteralTerm(lit, IntegerValue(1))) return false;
    if (!encoding_ct.AddLiteralTerm(lit, -value_literal.value)) return false;
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
  for (const ClosedInterval interval : (*domains)[var]) {
    for (IntegerValue v(interval.start); v <= interval.end; ++v) {
      if (!gtl::ContainsKey(encoded_values, v)) {
        min = v;
        break;
      }
    }
    if (min != kMaxIntegerValue) break;
  }

  IntegerValue max = kMinIntegerValue;
  const auto& domain = (*domains)[var];
  for (int i = domain.NumIntervals() - 1; i >= 0; --i) {
    const ClosedInterval interval = domain[i];
    for (IntegerValue v(interval.end); v >= interval.start; --v) {
      if (!gtl::ContainsKey(encoded_values, v)) {
        max = v;
        break;
      }
    }
    if (max != kMinIntegerValue) break;
  }

  return {min, max};
}

}  // namespace

void AppendPartialEncodingRelaxation(IntegerVariable var, const Model& model,
                                     LinearRelaxation* relaxation) {
  const auto* encoder = model.Get<IntegerEncoder>();
  const auto* integer_trail = model.Get<IntegerTrail>();
  if (encoder == nullptr || integer_trail == nullptr) return;

  const std::vector<IntegerEncoder::ValueLiteralPair> encoding =
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

// Add a linear relaxation of the CP constraint to the set of linear
// constraints. The highest linearization_level is, the more types of constraint
// we encode. This method should be called only for linearization_level > 0.
//
// TODO(user): In full generality, we could encode all the constraint as an LP.
// TODO(user,user): Add unit tests for this method.
void TryToLinearizeConstraint(const CpModelProto& model_proto,
                              const ConstraintProto& ct, Model* model,
                              int linearization_level,
                              LinearRelaxation* relaxation) {
  DCHECK_GT(linearization_level, 0);
  auto* mapping = model->GetOrCreate<CpModelMapping>();
  if (ct.constraint_case() == ConstraintProto::ConstraintCase::kBoolOr) {
    if (linearization_level < 2) return;
    LinearConstraintBuilder lc(model, IntegerValue(1), kMaxIntegerValue);
    for (const int enforcement_ref : ct.enforcement_literal()) {
      CHECK(lc.AddLiteralTerm(mapping->Literal(NegatedRef(enforcement_ref)),
                              IntegerValue(1)));
    }
    for (const int ref : ct.bool_or().literals()) {
      CHECK(lc.AddLiteralTerm(mapping->Literal(ref), IntegerValue(1)));
    }
    relaxation->linear_constraints.push_back(lc.Build());
  } else if (ct.constraint_case() ==
             ConstraintProto::ConstraintCase::kBoolAnd) {
    // TODO(user): These constraints can be many, and if they are not regrouped
    // in big at most ones, then they should probably only added lazily as cuts.
    // Regroup this with future clique-cut separation logic.
    if (linearization_level < 2) return;
    if (!HasEnforcementLiteral(ct)) return;
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
  } else if (ct.constraint_case() ==
             ConstraintProto::ConstraintCase::kAtMostOne) {
    if (HasEnforcementLiteral(ct)) return;
    std::vector<Literal> at_most_one;
    for (const int ref : ct.at_most_one().literals()) {
      at_most_one.push_back(mapping->Literal(ref));
    }
    relaxation->at_most_ones.push_back(at_most_one);
  } else if (ct.constraint_case() == ConstraintProto::ConstraintCase::kIntMax) {
    if (HasEnforcementLiteral(ct)) return;
    const IntegerVariable target = mapping->Integer(ct.int_max().target());
    const std::vector<IntegerVariable> vars =
        mapping->Integers(ct.int_max().vars());
    AppendMaxRelaxation(target, vars, linearization_level, model, relaxation);

  } else if (ct.constraint_case() == ConstraintProto::ConstraintCase::kIntMin) {
    if (HasEnforcementLiteral(ct)) return;
    const IntegerVariable negative_target =
        NegationOf(mapping->Integer(ct.int_min().target()));
    const std::vector<IntegerVariable> negative_vars =
        NegationOf(mapping->Integers(ct.int_min().vars()));
    AppendMaxRelaxation(negative_target, negative_vars, linearization_level,
                        model, relaxation);

  } else if (ct.constraint_case() ==
             ConstraintProto::ConstraintCase::kIntProd) {
    if (HasEnforcementLiteral(ct)) return;
    const int target = ct.int_prod().target();
    const int size = ct.int_prod().vars_size();

    // We just linearize x = y^2 by x >= y which is far from ideal but at
    // least pushes x when y moves away from zero. Note that if y is negative,
    // we should probably also add x >= -y, but then this do not happen in
    // our test set.
    if (size == 2 && ct.int_prod().vars(0) == ct.int_prod().vars(1)) {
      LinearConstraintBuilder lc(model, kMinIntegerValue, IntegerValue(0));
      lc.AddTerm(mapping->Integer(ct.int_prod().vars(0)), IntegerValue(1));
      lc.AddTerm(mapping->Integer(target), IntegerValue(-1));
      relaxation->linear_constraints.push_back(lc.Build());
    }
  } else if (ct.constraint_case() == ConstraintProto::ConstraintCase::kLinear) {
    AppendLinearConstraintRelaxation(ct, linearization_level, *model,
                                     relaxation);
  } else if (ct.constraint_case() ==
             ConstraintProto::ConstraintCase::kCircuit) {
    if (HasEnforcementLiteral(ct)) return;
    const int num_arcs = ct.circuit().literals_size();
    CHECK_EQ(num_arcs, ct.circuit().tails_size());
    CHECK_EQ(num_arcs, ct.circuit().heads_size());

    // Each node must have exactly one incoming and one outgoing arc (note that
    // it can be the unique self-arc of this node too).
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
  } else if (ct.constraint_case() ==
             ConstraintProto::ConstraintCase::kElement) {
    const IntegerVariable index = mapping->Integer(ct.element().index());
    const IntegerVariable target = mapping->Integer(ct.element().target());
    const std::vector<IntegerVariable> vars =
        mapping->Integers(ct.element().vars());

    // We only relax the case where all the vars are constant.
    // target = sum (index == i) * fixed_vars[i].
    LinearConstraintBuilder constraint(model, IntegerValue(0), IntegerValue(0));
    constraint.AddTerm(target, IntegerValue(-1));
    IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
    for (const auto literal_value : model->Add(FullyEncodeVariable((index)))) {
      const IntegerVariable var = vars[literal_value.value.value()];
      if (!model->Get(IsFixed(var))) return;

      // Make sure this literal has a view.
      model->Add(NewIntegerVariableFromLiteral(literal_value.literal));
      CHECK(constraint.AddLiteralTerm(literal_value.literal,
                                      integer_trail->LowerBound(var)));
    }

    relaxation->linear_constraints.push_back(constraint.Build());
  }
}

void AppendMaxRelaxation(IntegerVariable target,
                         const std::vector<IntegerVariable>& vars,
                         int linearization_level, Model* model,
                         LinearRelaxation* relaxation) {
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

  // Part 2: Encode upper bound on X.
  // NOTE(user) for size = 2, we can do this with 1 less variable.
  if (vars.size() == 2 || linearization_level >= 2) {
    IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
    const IntegerValue max_target_value = integer_trail->UpperBound(target);

    // For each X_i, we encode y_i => X <= X_i. And at least one of the y_i
    // is true. Note that the correct y_i will be chosen because of the first
    // part in linearlization (X >= X_i).
    // TODO(user): Only lower bound is needed, experiment.
    LinearConstraintBuilder lc_exactly_one(model, IntegerValue(1),
                                           IntegerValue(1));
    for (const IntegerVariable var : vars) {
      if (target == var) continue;
      // y => X <= X_i.
      // <=> max_term_value * y + X - X_i <= max_term_value.
      // where max_tern_value is X_ub - X_i_lb.
      IntegerVariable y = model->Add(NewIntegerVariable(0, 1));
      const IntegerValue min_var_value = integer_trail->LowerBound(var);
      const IntegerValue max_term_value = max_target_value - min_var_value;
      LinearConstraintBuilder lc(model, kMinIntegerValue, max_term_value);
      lc.AddTerm(target, IntegerValue(1));
      lc.AddTerm(var, IntegerValue(-1));
      lc.AddTerm(y, max_term_value);
      relaxation->linear_constraints.push_back(lc.Build());

      lc_exactly_one.AddTerm(y, IntegerValue(1));
    }
    relaxation->linear_constraints.push_back(lc_exactly_one.Build());
  }
}

void AppendLinearConstraintRelaxation(const ConstraintProto& constraint_proto,
                                      const int linearization_level,
                                      const Model& model,
                                      LinearRelaxation* relaxation) {
  auto* mapping = model.Get<CpModelMapping>();

  // Note that we ignore the holes in the domain.
  //
  // TODO(user): In LoadLinearConstraint() we already created intermediate
  // Booleans for each disjoint interval, we should reuse them here if
  // possible.
  //
  // TODO(user): process the "at most one" part of a == 1 separately?
  const IntegerValue rhs_domain_min =
      IntegerValue(constraint_proto.linear().domain(0));
  const IntegerValue rhs_domain_max =
      IntegerValue(constraint_proto.linear().domain(
          constraint_proto.linear().domain_size() - 1));
  if (rhs_domain_min == kint64min && rhs_domain_max == kint64max) return;

  if (!HasEnforcementLiteral(constraint_proto)) {
    LinearConstraintBuilder lc(&model, rhs_domain_min, rhs_domain_max);
    for (int i = 0; i < constraint_proto.linear().vars_size(); i++) {
      const int ref = constraint_proto.linear().vars(i);
      const int64 coeff = constraint_proto.linear().coeffs(i);
      lc.AddTerm(mapping->Integer(ref), IntegerValue(coeff));
    }
    relaxation->linear_constraints.push_back(lc.Build());
    return;
  }

  // Reified version.
  if (linearization_level < 2) return;

  // We linearize fully reified constraints of size 1 all together for a given
  // variable. But we need to process half-reified ones.
  if (!mapping->IsHalfEncodingConstraint(&constraint_proto) &&
      constraint_proto.linear().vars_size() <= 1) {
    return;
  }

  // Compute the implied bounds on the linear expression.
  IntegerValue min_sum(0);
  IntegerValue max_sum(0);
  for (int i = 0; i < constraint_proto.linear().vars_size(); i++) {
    int ref = constraint_proto.linear().vars(i);
    IntegerValue coeff(constraint_proto.linear().coeffs(i));
    if (!RefIsPositive(ref)) {
      ref = PositiveRef(ref);
      coeff = -coeff;
    }
    const IntegerVariable int_var = mapping->Integer(ref);
    const auto* integer_trail = model.Get<IntegerTrail>();
    integer_trail->LowerBound(int_var);
    if (coeff > 0.0) {
      min_sum += coeff * integer_trail->LowerBound(int_var);
      max_sum += coeff * integer_trail->UpperBound(int_var);
    } else {
      min_sum += coeff * integer_trail->UpperBound(int_var);
      max_sum += coeff * integer_trail->LowerBound(int_var);
    }
  }

  if (rhs_domain_min != kint64min) {
    // And(ei) => terms >= rhs_domain_min
    // <=> Sum_i (~ei * (rhs_domain_min - min_sum)) + terms >= rhs_domain_min
    LinearConstraintBuilder lc(&model, rhs_domain_min, kMaxIntegerValue);
    for (const int enforcement_ref : constraint_proto.enforcement_literal()) {
      CHECK(lc.AddLiteralTerm(mapping->Literal(NegatedRef(enforcement_ref)),
                              rhs_domain_min - min_sum));
    }
    for (int i = 0; i < constraint_proto.linear().vars_size(); i++) {
      const int ref = constraint_proto.linear().vars(i);
      lc.AddTerm(mapping->Integer(ref),
                 IntegerValue(constraint_proto.linear().coeffs(i)));
    }
    relaxation->linear_constraints.push_back(lc.Build());
  }
  if (rhs_domain_max != kint64max) {
    // And(ei) => terms <= rhs_domain_max
    // <=> Sum_i (~ei * (rhs_domain_max - max_sum)) + terms <= rhs_domain_max
    LinearConstraintBuilder lc(&model, kMinIntegerValue, rhs_domain_max);
    for (const int enforcement_ref : constraint_proto.enforcement_literal()) {
      CHECK(lc.AddLiteralTerm(mapping->Literal(NegatedRef(enforcement_ref)),
                              rhs_domain_max - max_sum));
    }
    for (int i = 0; i < constraint_proto.linear().vars_size(); i++) {
      const int ref = constraint_proto.linear().vars(i);
      lc.AddTerm(mapping->Integer(ref),
                 IntegerValue(constraint_proto.linear().coeffs(i)));
    }
    relaxation->linear_constraints.push_back(lc.Build());
  }
}

}  // namespace sat
}  // namespace operations_research
