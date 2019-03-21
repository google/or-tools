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
