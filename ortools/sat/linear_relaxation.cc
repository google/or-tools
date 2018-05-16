// Copyright 2010-2017 Google
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

#include <unordered_set>
#include "ortools/base/iterator_adaptors.h"

namespace operations_research {
namespace sat {

bool AppendFullEncodingRelaxation(IntegerVariable var, const Model& model,
                                  std::vector<LinearConstraint>* constraints) {
  const auto* encoder = model.Get<IntegerEncoder>();
  if (encoder == nullptr) return false;
  if (!encoder->VariableIsFullyEncoded(var)) return false;

  LinearConstraintBuilder exactly_one_ct(&model, 1.0, 1.0);
  LinearConstraintBuilder encoding_ct(&model, 0.0, 0.0);
  encoding_ct.AddTerm(var, 1.0);

  // Create the constraint if all literal have a view.
  for (const auto value_literal : encoder->FullDomainEncoding(var)) {
    const Literal lit = value_literal.literal;
    const double coeff = static_cast<double>(value_literal.value.value());
    if (!exactly_one_ct.AddLiteralTerm(lit, 1)) return false;
    if (!encoding_ct.AddLiteralTerm(lit, -coeff)) return false;
  }

  // TODO(user): also skip if var is fixed and there is just one term. Or more
  // generally, always add all constraints, but do not add the trivial ones to
  // the LP.
  if (!exactly_one_ct.IsEmpty()) constraints->push_back(exactly_one_ct.Build());
  if (!encoding_ct.IsEmpty()) constraints->push_back(encoding_ct.Build());
  return true;
}

namespace {

// TODO(user): Not super efficient.
std::pair<IntegerValue, IntegerValue> GetMinAndMaxNotEncoded(
    IntegerVariable var, const std::unordered_set<IntegerValue>& encoded_values,
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
  for (const ClosedInterval interval : gtl::reversed_view((*domains)[var])) {
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

void AppendPartialEncodingRelaxation(
    IntegerVariable var, const Model& model,
    std::vector<LinearConstraint>* constraints) {
  const auto* encoder = model.Get<IntegerEncoder>();
  const auto* integer_trail = model.Get<IntegerTrail>();
  if (encoder == nullptr || integer_trail == nullptr) return;

  const std::vector<IntegerEncoder::ValueLiteralPair> encoding =
      encoder->PartialDomainEncoding(var);
  if (encoding.empty()) return;

  const double kInfinity = std::numeric_limits<double>::infinity();
  LinearConstraintBuilder at_most_one_ct(&model, -kInfinity, 1.0);
  std::unordered_set<IntegerValue> encoded_values;
  for (const auto value_literal : encoding) {
    // Note that we skip pairs that do not have an Integer view.
    if (at_most_one_ct.AddLiteralTerm(value_literal.literal, 1)) {
      encoded_values.insert(value_literal.value);
    }
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
    LinearConstraintBuilder exactly_one_ct(&model, 1.0, 1.0);
    LinearConstraintBuilder encoding_ct(&model, 0.0, 0.0);
    encoding_ct.AddTerm(var, 1.0);
    for (const auto value_literal : encoding) {
      const Literal lit = value_literal.literal;
      const double coeff = static_cast<double>(value_literal.value.value());
      CHECK(exactly_one_ct.AddLiteralTerm(lit, 1));
      CHECK(encoding_ct.AddLiteralTerm(lit, -coeff));
    }
    if (exactly_one_ct.size() > 1) {
      constraints->push_back(exactly_one_ct.Build());
    }
    if (encoding_ct.size() > 1) {
      constraints->push_back(encoding_ct.Build());
    }
    return;
  }

  // min + sum li * (xi - min) <= var.
  const double d_min = static_cast<double>(pair.first.value());
  LinearConstraintBuilder lower_bound_ct(&model, d_min, kInfinity);
  lower_bound_ct.AddTerm(var, 1);
  for (const auto value_literal : encoding) {
    const double value = static_cast<double>(value_literal.value.value());
    CHECK(lower_bound_ct.AddLiteralTerm(value_literal.literal, d_min - value));
  }

  // var <= max + sum li * (xi - max).
  const double d_max = static_cast<double>(pair.second.value());
  LinearConstraintBuilder upper_bound_ct(&model, -kInfinity, d_max);
  upper_bound_ct.AddTerm(var, 1);
  for (const auto value_literal : encoding) {
    const double value = static_cast<double>(value_literal.value.value());
    CHECK(upper_bound_ct.AddLiteralTerm(value_literal.literal, d_max - value));
  }

  if (at_most_one_ct.size() > 1 && encoded_values.size() > 1) {
    constraints->push_back(at_most_one_ct.Build());
  }
  if (lower_bound_ct.size() > 1) constraints->push_back(lower_bound_ct.Build());
  if (upper_bound_ct.size() > 1) constraints->push_back(upper_bound_ct.Build());
}

void AppendPartialGreaterThanEncodingRelaxation(
    IntegerVariable var, const Model& model,
    std::vector<LinearConstraint>* constraints) {
  const auto* integer_trail = model.Get<IntegerTrail>();
  const auto* encoder = model.Get<IntegerEncoder>();
  if (integer_trail == nullptr || encoder == nullptr) return;

  const std::map<IntegerValue, Literal>& greater_than_encoding =
      encoder->PartialGreaterThanEncoding(var);
  if (greater_than_encoding.empty()) return;

  // Start by the var >= side.
  // And also add the implications between used literals.
  const double kInfinity = std::numeric_limits<double>::infinity();
  {
    IntegerValue prev_used_bound = integer_trail->LowerBound(var);
    const double lb = static_cast<double>(prev_used_bound.value());
    LinearConstraintBuilder lb_constraint(&model, lb, kInfinity);
    lb_constraint.AddTerm(var, 1.0);
    LiteralIndex prev_literal_index = kNoLiteralIndex;
    for (const auto entry : greater_than_encoding) {
      if (entry.first <= prev_used_bound) continue;

      const LiteralIndex literal_index = entry.second.Index();
      const double diff =
          static_cast<double>((prev_used_bound - entry.first).value());

      // Skip the entry if the literal doesn't have a view.
      if (!lb_constraint.AddLiteralTerm(entry.second, diff)) continue;
      if (prev_literal_index != kNoLiteralIndex) {
        // Add var <= prev_var.
        LinearConstraintBuilder lower_than(
            &model, -std::numeric_limits<double>::infinity(), 0);
        CHECK(lower_than.AddLiteralTerm(Literal(literal_index), 1.0));
        CHECK(lower_than.AddLiteralTerm(Literal(prev_literal_index), -1.0));
        if (!lower_than.IsEmpty()) constraints->push_back(lower_than.Build());
      }
      prev_used_bound = entry.first;
      prev_literal_index = literal_index;
    }
    if (!lb_constraint.IsEmpty()) constraints->push_back(lb_constraint.Build());
  }

  // Do the same for the var <= side by using NegationOfVar().
  // Note that we do not need to add the implications between literals again.
  {
    IntegerValue prev_used_bound = integer_trail->LowerBound(NegationOf(var));
    const double lb = static_cast<double>(prev_used_bound.value());
    LinearConstraintBuilder lb_constraint(&model, lb, kInfinity);
    lb_constraint.AddTerm(var, -1.0);
    for (const auto entry :
         encoder->PartialGreaterThanEncoding(NegationOf(var))) {
      if (entry.first <= prev_used_bound) continue;
      const double diff =
          static_cast<double>((prev_used_bound - entry.first).value());

      // Skip the entry if the literal doesn't have a view.
      if (!lb_constraint.AddLiteralTerm(entry.second, diff)) continue;
      prev_used_bound = entry.first;
    }
    if (!lb_constraint.IsEmpty()) constraints->push_back(lb_constraint.Build());
  }
}

}  // namespace sat
}  // namespace operations_research
