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

#include "ortools/sat/intervals.h"

#include <optional>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/meta/type_traits.h"
#include "absl/types/span.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/integer_expr.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/model.h"
#include "ortools/sat/no_overlap_2d_helper.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/scheduling_helpers.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {

IntervalVariable IntervalsRepository::CreateInterval(IntegerVariable start,
                                                     IntegerVariable end,
                                                     IntegerVariable size,
                                                     IntegerValue fixed_size,
                                                     LiteralIndex is_present) {
  return CreateInterval(AffineExpression(start), AffineExpression(end),
                        size == kNoIntegerVariable
                            ? AffineExpression(fixed_size)
                            : AffineExpression(size),
                        is_present, /*add_linear_relation=*/true);
}

IntervalVariable IntervalsRepository::CreateInterval(AffineExpression start,
                                                     AffineExpression end,
                                                     AffineExpression size,
                                                     LiteralIndex is_present,
                                                     bool add_linear_relation) {
  // Create the interval.
  const IntervalVariable i(starts_.size());
  starts_.push_back(start);
  ends_.push_back(end);
  sizes_.push_back(size);
  is_present_.push_back(is_present);

  std::vector<Literal> enforcement_literals;
  if (is_present != kNoLiteralIndex) {
    enforcement_literals.push_back(Literal(is_present));
  }

  if (add_linear_relation) {
    LinearConstraintBuilder builder(model_, IntegerValue(0), IntegerValue(0));
    builder.AddTerm(Start(i), IntegerValue(1));
    builder.AddTerm(Size(i), IntegerValue(1));
    builder.AddTerm(End(i), IntegerValue(-1));
    LoadConditionalLinearConstraint(enforcement_literals, builder.Build(),
                                    model_);
  }

  return i;
}

void IntervalsRepository::CreateDisjunctivePrecedenceLiteral(
    IntervalVariable a, IntervalVariable b) {
  GetOrCreateDisjunctivePrecedenceLiteral(
      IntervalDefinition{.start = Start(a),
                         .end = End(a),
                         .size = Size(a),
                         .is_present = IsOptional(a)
                                           ? std::optional(PresenceLiteral(a))
                                           : std::nullopt},
      IntervalDefinition{.start = Start(b),
                         .end = End(b),
                         .size = Size(b),
                         .is_present = IsOptional(b)
                                           ? std::optional(PresenceLiteral(b))
                                           : std::nullopt});
}

LiteralIndex IntervalsRepository::GetOrCreateDisjunctivePrecedenceLiteral(
    const IntervalDefinition& a, const IntervalDefinition& b) {
  auto it = disjunctive_precedences_.find({a, b});
  if (it != disjunctive_precedences_.end()) return it->second.Index();

  std::vector<Literal> enforcement_literals;
  if (a.is_present.has_value()) {
    enforcement_literals.push_back(a.is_present.value());
  }
  if (b.is_present.has_value()) {
    enforcement_literals.push_back(b.is_present.value());
  }

  auto remove_fixed = [assignment =
                           &assignment_](std::vector<Literal>& literals) {
    int new_size = 0;
    for (const Literal l : literals) {
      // We can ignore always absent interval, and skip the literal of the
      // interval that are now always present.
      if (assignment->LiteralIsTrue(l)) continue;
      if (assignment->LiteralIsFalse(l)) return false;
      literals[new_size++] = l;
    }
    literals.resize(new_size);
    return true;
  };

  if (sat_solver_->CurrentDecisionLevel() == 0) {
    if (!remove_fixed(enforcement_literals)) return kNoLiteralIndex;
  }

  // task_a is currently before task_b ?
  // Lets not create a literal that will be propagated right away.
  if (integer_trail_->UpperBound(a.start) < integer_trail_->LowerBound(b.end)) {
    if (sat_solver_->CurrentDecisionLevel() == 0) {
      AddConditionalAffinePrecedence(enforcement_literals, a.end, b.start,
                                     model_);
    }
    return kNoLiteralIndex;
  }

  // task_b is before task_a ?
  if (integer_trail_->UpperBound(b.start) < integer_trail_->LowerBound(a.end)) {
    if (sat_solver_->CurrentDecisionLevel() == 0) {
      AddConditionalAffinePrecedence(enforcement_literals, b.end, a.start,
                                     model_);
    }
    return kNoLiteralIndex;
  }

  // Create a new literal.
  const BooleanVariable boolean_var = sat_solver_->NewBooleanVariable();
  const Literal a_before_b = Literal(boolean_var, true);
  disjunctive_precedences_.insert({{a, b}, a_before_b});
  disjunctive_precedences_.insert({{b, a}, a_before_b.Negated()});

  // Also insert it in precedences.
  if (enforcement_literals.empty()) {
    // TODO(user): also add the reverse like start_b + 1 <= end_a if negated?
    precedences_.insert({{a.end, b.start}, a_before_b});
    precedences_.insert({{b.end, a.start}, a_before_b.Negated()});
  }

  enforcement_literals.push_back(a_before_b);
  AddConditionalAffinePrecedence(enforcement_literals, a.end, b.start, model_);
  enforcement_literals.pop_back();

  enforcement_literals.push_back(a_before_b.Negated());
  AddConditionalAffinePrecedence(enforcement_literals, b.end, a.start, model_);
  enforcement_literals.pop_back();

  // The calls to AddConditionalAffinePrecedence() might have fixed some of the
  // enforcement literals. Remove them if we are at level zero.
  if (sat_solver_->CurrentDecisionLevel() == 0) {
    if (!remove_fixed(enforcement_literals)) return kNoLiteralIndex;
  }

  // Force the value of boolean_var in case the precedence is not active. This
  // avoids duplicate solutions when enumerating all possible solutions.
  for (const Literal l : enforcement_literals) {
    implications_->AddBinaryClause(l, a_before_b);
  }

  return a_before_b;
}

bool IntervalsRepository::CreatePrecedenceLiteral(AffineExpression x,
                                                  AffineExpression y) {
  if (precedences_.contains({x, y})) return false;

  // We want l => x <= y and not(l) => x > y <=> y + 1 <= x
  // Do not create l if the relation is always true or false.
  if (integer_trail_->UpperBound(x) <= integer_trail_->LowerBound(y)) {
    return false;
  }
  if (integer_trail_->LowerBound(x) > integer_trail_->UpperBound(y)) {
    return false;
  }

  // Create a new literal.
  const BooleanVariable boolean_var = sat_solver_->NewBooleanVariable();
  const Literal x_before_y = Literal(boolean_var, true);

  // TODO(user): Also add {{y_plus_one, x}, x_before_y.Negated()} ?
  precedences_.insert({{x, y}, x_before_y});

  AffineExpression y_plus_one = y;
  y_plus_one.constant += 1;
  AddConditionalAffinePrecedence({x_before_y}, x, y, model_);
  AddConditionalAffinePrecedence({x_before_y.Negated()}, y_plus_one, x, model_);
  return true;
}

LiteralIndex IntervalsRepository::GetPrecedenceLiteral(
    AffineExpression x, AffineExpression y) const {
  const auto it = precedences_.find({x, y});
  if (it != precedences_.end()) return it->second.Index();
  return kNoLiteralIndex;
}

// TODO(user): Ideally we should sort the vector of variables, but right now
// we cannot since we often use this with a parallel vector of demands. So this
// "sorting" should happen in the presolver so we can share as much as possible.
SchedulingConstraintHelper* IntervalsRepository::GetOrCreateHelper(
    const std::vector<IntervalVariable>& variables,
    bool register_as_disjunctive_helper) {
  const auto it = helper_repository_.find(variables);
  if (it != helper_repository_.end()) return it->second;
  std::vector<AffineExpression> starts;
  std::vector<AffineExpression> ends;
  std::vector<AffineExpression> sizes;
  std::vector<LiteralIndex> reason_for_presence;

  const int num_variables = variables.size();
  starts.reserve(num_variables);
  ends.reserve(num_variables);
  sizes.reserve(num_variables);
  reason_for_presence.reserve(num_variables);

  for (const IntervalVariable i : variables) {
    if (IsOptional(i)) {
      reason_for_presence.push_back(PresenceLiteral(i).Index());
    } else {
      reason_for_presence.push_back(kNoLiteralIndex);
    }
    sizes.push_back(Size(i));
    starts.push_back(Start(i));
    ends.push_back(End(i));
  }

  SchedulingConstraintHelper* helper = new SchedulingConstraintHelper(
      std::move(starts), std::move(ends), std::move(sizes),
      std::move(reason_for_presence), model_);
  helper->RegisterWith(model_->GetOrCreate<GenericLiteralWatcher>());
  helper_repository_[variables] = helper;
  model_->TakeOwnership(helper);
  if (register_as_disjunctive_helper) {
    disjunctive_helpers_.push_back(helper);
  }
  return helper;
}

NoOverlap2DConstraintHelper* IntervalsRepository::GetOrCreate2DHelper(
    const std::vector<IntervalVariable>& x_variables,
    const std::vector<IntervalVariable>& y_variables) {
  const auto it =
      no_overlap_2d_helper_repository_.find({x_variables, y_variables});
  if (it != no_overlap_2d_helper_repository_.end()) return it->second;

  std::vector<AffineExpression> x_starts;
  std::vector<AffineExpression> x_ends;
  std::vector<AffineExpression> x_sizes;
  std::vector<LiteralIndex> x_reason_for_presence;

  for (const IntervalVariable i : x_variables) {
    if (IsOptional(i)) {
      x_reason_for_presence.push_back(PresenceLiteral(i).Index());
    } else {
      x_reason_for_presence.push_back(kNoLiteralIndex);
    }
    x_sizes.push_back(Size(i));
    x_starts.push_back(Start(i));
    x_ends.push_back(End(i));
  }
  std::vector<AffineExpression> y_starts;
  std::vector<AffineExpression> y_ends;
  std::vector<AffineExpression> y_sizes;
  std::vector<LiteralIndex> y_reason_for_presence;

  for (const IntervalVariable i : y_variables) {
    if (IsOptional(i)) {
      y_reason_for_presence.push_back(PresenceLiteral(i).Index());
    } else {
      y_reason_for_presence.push_back(kNoLiteralIndex);
    }
    y_sizes.push_back(Size(i));
    y_starts.push_back(Start(i));
    y_ends.push_back(End(i));
  }
  NoOverlap2DConstraintHelper* helper = new NoOverlap2DConstraintHelper(
      std::move(x_starts), std::move(x_ends), std::move(x_sizes),
      std::move(x_reason_for_presence), std::move(y_starts), std::move(y_ends),
      std::move(y_sizes), std::move(y_reason_for_presence), model_);
  helper->RegisterWith(model_->GetOrCreate<GenericLiteralWatcher>());
  no_overlap_2d_helper_repository_[{x_variables, y_variables}] = helper;
  model_->TakeOwnership(helper);
  return helper;
}

SchedulingDemandHelper* IntervalsRepository::GetOrCreateDemandHelper(
    SchedulingConstraintHelper* helper,
    absl::Span<const AffineExpression> demands) {
  const std::pair<SchedulingConstraintHelper*, std::vector<AffineExpression>>
      key = {helper,
             std::vector<AffineExpression>(demands.begin(), demands.end())};
  const auto it = demand_helper_repository_.find(key);
  if (it != demand_helper_repository_.end()) return it->second;

  SchedulingDemandHelper* demand_helper =
      new SchedulingDemandHelper(demands, helper, model_);
  model_->TakeOwnership(demand_helper);
  demand_helper_repository_[key] = demand_helper;
  return demand_helper;
}

void IntervalsRepository::InitAllDecomposedEnergies() {
  for (const auto& it : demand_helper_repository_) {
    it.second->InitDecomposedEnergies();
  }
}

}  // namespace sat
}  // namespace operations_research
