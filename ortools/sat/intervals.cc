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

#include <algorithm>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/clause.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/integer_expr.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/model.h"
#include "ortools/sat/no_overlap_2d_helper.h"
#include "ortools/sat/precedences.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/scheduling_helpers.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {

IntervalsRepository::IntervalsRepository(Model* model)
    : model_(model),
      assignment_(model->GetOrCreate<Trail>()->Assignment()),
      sat_solver_(model->GetOrCreate<SatSolver>()),
      implications_(model->GetOrCreate<BinaryImplicationGraph>()),
      integer_trail_(model->GetOrCreate<IntegerTrail>()),
      reified_precedences_(model->GetOrCreate<ReifiedLinear2Bounds>()),
      root_level_bounds_(model->GetOrCreate<RootLevelLinear2Bounds>()),
      linear2_bounds_(model->GetOrCreate<Linear2Bounds>()),
      integer_encoder_(model->GetOrCreate<IntegerEncoder>()),
      trivial_literals_(model->GetOrCreate<TrivialLiterals>()) {}

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
  GetOrCreateDisjunctivePrecedenceLiteralIfNonTrivial(
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

LiteralIndex
IntervalsRepository::GetOrCreateDisjunctivePrecedenceLiteralIfNonTrivial(
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

  auto remove_fixed_at_root_level =
      [assignment = &assignment_,
       sat_solver_ = sat_solver_](std::vector<Literal>& literals) {
        int new_size = 0;
        for (const Literal l : literals) {
          if (assignment->LiteralIsAssigned(l)) {
            const bool is_fixed =
                sat_solver_->LiteralTrail().Info(l.Variable()).level == 0;
            // We can ignore always absent interval, and skip the literal of the
            // interval that are now always present.
            if (is_fixed && assignment->LiteralIsTrue(l)) continue;
            if (is_fixed && assignment->LiteralIsFalse(l)) return false;
          }
          literals[new_size++] = l;
        }
        literals.resize(new_size);
        return true;
      };

  if (!remove_fixed_at_root_level(enforcement_literals)) return kNoLiteralIndex;

  // task_a is currently before task_b ?
  // Lets not create a literal that will be propagated right away.
  const auto [expr_b_before_a, ub_b_before_a] =
      EncodeDifferenceLowerThan(b.end, a.start, 0);
  const RelationStatus b_before_a_root_status =
      root_level_bounds_->GetLevelZeroStatus(expr_b_before_a, kMinIntegerValue,
                                             ub_b_before_a);

  // task_b is before task_a ?
  const auto [expr_a_before_b, ub_a_before_b] =
      EncodeDifferenceLowerThan(a.end, b.start, 0);
  const RelationStatus a_before_b_root_status =
      root_level_bounds_->GetLevelZeroStatus(expr_a_before_b, kMinIntegerValue,
                                             ub_a_before_b);

  if (enforcement_literals.empty() &&
      b_before_a_root_status != RelationStatus::IS_UNKNOWN &&
      a_before_b_root_status == b_before_a_root_status) {
    // We have "a before b" and "b before a" at root level (or similarly with
    // "after"). This is UNSAT.
    sat_solver_->NotifyThatModelIsUnsat();
    return kNoLiteralIndex;
  }

  if (b_before_a_root_status == RelationStatus::IS_FALSE) {
    if (!enforcement_literals.empty() ||
        a_before_b_root_status == RelationStatus::IS_UNKNOWN) {
      AddConditionalAffinePrecedence(enforcement_literals, a.end, b.start,
                                     model_);
    }
    return kNoLiteralIndex;
  }
  const RelationStatus b_before_a_status = linear2_bounds_->GetStatus(
      expr_b_before_a, kMinIntegerValue, ub_b_before_a);
  if (b_before_a_status != RelationStatus::IS_UNKNOWN) {
    // Abort if the relation is already known.
    return kNoLiteralIndex;
  }

  if (a_before_b_root_status == RelationStatus::IS_FALSE) {
    if (!enforcement_literals.empty() ||
        b_before_a_root_status == RelationStatus::IS_UNKNOWN) {
      AddConditionalAffinePrecedence(enforcement_literals, b.end, a.start,
                                     model_);
    }
    return kNoLiteralIndex;
  }
  const RelationStatus a_before_b_status = linear2_bounds_->GetStatus(
      expr_a_before_b, kMinIntegerValue, ub_a_before_b);
  if (a_before_b_status != RelationStatus::IS_UNKNOWN) {
    // Abort if the relation is already known.
    return kNoLiteralIndex;
  }

  // Create a new literal.
  //
  // TODO(user): An alternative solution when it is enforced is to get/create
  // - s <=> a.end <= b.start
  // - t <=> b.end <= a.start
  // and have enforcement => s + t == 1. The later might not even be needed
  // though, since interval equation should already enforce it.
  Literal a_before_b;
  if (enforcement_literals.empty()) {
    // We don't have any enforcement literal, so we should use the existing
    // ReifiedLinear2Bounds class.
    LiteralIndex a_before_b_index = GetPrecedenceLiteral(a.end, b.start);
    const LiteralIndex b_before_a_index = GetPrecedenceLiteral(b.end, a.start);
    if (a_before_b_index == kNoLiteralIndex &&
        b_before_a_index == kNoLiteralIndex) {
      CreatePrecedenceLiteralIfNonTrivial(a.end, b.start);
      a_before_b_index = GetPrecedenceLiteral(a.end, b.start);
      DCHECK_NE(a_before_b_index, kNoLiteralIndex);  // We tested not trivial.
      // Now associate its negation with b.end <= a.start.
      reified_precedences_->AddBoundEncodingIfNonTrivial(
          Literal(a_before_b_index).Negated(), expr_b_before_a, ub_b_before_a);
    } else if (a_before_b_index == kNoLiteralIndex &&
               b_before_a_index != kNoLiteralIndex) {
      // We already have a literal for b.end <= a.start.
      // We can just use the negation of that literal.
      a_before_b_index = Literal(b_before_a_index).NegatedIndex();
      reified_precedences_->AddBoundEncodingIfNonTrivial(
          Literal(a_before_b_index), expr_a_before_b, ub_a_before_b);
    } else if (a_before_b_index != kNoLiteralIndex &&
               b_before_a_index == kNoLiteralIndex) {
      reified_precedences_->AddBoundEncodingIfNonTrivial(
          Literal(a_before_b_index).Negated(), expr_b_before_a, ub_b_before_a);
    } else {
      // We have both literals. One must be the negation of the other.
      implications_->AddImplication(Literal(a_before_b_index),
                                    Literal(b_before_a_index).Negated());
      implications_->AddImplication(Literal(a_before_b_index).Negated(),
                                    Literal(b_before_a_index));
    }
    DCHECK_NE(a_before_b_index, kNoLiteralIndex);
    a_before_b = Literal(a_before_b_index);
  } else {
    const BooleanVariable boolean_var = sat_solver_->NewBooleanVariable();
    a_before_b = Literal(boolean_var, true);
  }

  disjunctive_precedences_.insert({{a, b}, a_before_b});
  disjunctive_precedences_.insert({{b, a}, a_before_b.Negated()});

  enforcement_literals.push_back(a_before_b);
  AddConditionalAffinePrecedence(enforcement_literals, a.end, b.start, model_);
  enforcement_literals.pop_back();

  enforcement_literals.push_back(a_before_b.Negated());
  AddConditionalAffinePrecedence(enforcement_literals, b.end, a.start, model_);
  enforcement_literals.pop_back();

  // The calls to AddConditionalAffinePrecedence() might have fixed some of the
  // enforcement literals. Remove them if we are at level zero.
  if (!remove_fixed_at_root_level(enforcement_literals)) return kNoLiteralIndex;

  // Force the value of boolean_var in case the precedence is not active. This
  // avoids duplicate solutions when enumerating all possible solutions.
  for (const Literal l : enforcement_literals) {
    implications_->AddBinaryClause(l, a_before_b);
  }

  return a_before_b;
}

bool IntervalsRepository::CreatePrecedenceLiteralIfNonTrivial(
    AffineExpression x, AffineExpression y) {
  const auto [expr, ub] = EncodeDifferenceLowerThan(x, y, 0);
  auto reified_bound = reified_precedences_->GetEncodedBound(expr, ub);
  if (std::holds_alternative<ReifiedLinear2Bounds::ReifiedBoundType>(
          reified_bound)) {
    const auto bound_type =
        std::get<ReifiedLinear2Bounds::ReifiedBoundType>(reified_bound);
    if (bound_type == ReifiedLinear2Bounds::ReifiedBoundType::kAlwaysTrue ||
        bound_type == ReifiedLinear2Bounds::ReifiedBoundType::kAlwaysFalse) {
      // Nothing to do, precedence is trivial at level zero.
      return false;
    }
  }

  if (std::holds_alternative<Literal>(reified_bound)) {
    // Already created.
    return false;
  }

  if (std::holds_alternative<IntegerLiteral>(reified_bound)) {
    if (integer_encoder_->GetAssociatedLiteral(
            std::get<IntegerLiteral>(reified_bound)) != kNoLiteralIndex) {
      return false;
    }
    // Create a new literal from the IntegerLiteral. This makes sure
    // GetPrecedenceLiteral() always returns something if this function was
    // called on a non-trivial precedence.
    integer_encoder_->GetOrCreateAssociatedLiteral(
        std::get<IntegerLiteral>(reified_bound));
    return true;
  }

  // Create a new literal.
  const BooleanVariable boolean_var = sat_solver_->NewBooleanVariable();
  const Literal x_before_y = Literal(boolean_var, true);
  reified_precedences_->AddBoundEncodingIfNonTrivial(x_before_y, expr, ub);

  AffineExpression y_plus_one = y;
  y_plus_one.constant += 1;
  AddConditionalAffinePrecedence({x_before_y}, x, y, model_);
  AddConditionalAffinePrecedence({x_before_y.Negated()}, y_plus_one, x, model_);
  return true;
}

LiteralIndex IntervalsRepository::GetPrecedenceLiteral(
    AffineExpression x, AffineExpression y) const {
  const auto [expr, ub] = EncodeDifferenceLowerThan(x, y, 0);
  auto reified_bound = reified_precedences_->GetEncodedBound(expr, ub);
  if (std::holds_alternative<IntegerLiteral>(reified_bound)) {
    return integer_encoder_->GetAssociatedLiteral(
        std::get<IntegerLiteral>(reified_bound));
  }
  if (std::holds_alternative<Literal>(reified_bound)) {
    return std::get<Literal>(reified_bound).Index();
  }
  if (std::holds_alternative<ReifiedLinear2Bounds::ReifiedBoundType>(
          reified_bound)) {
    const auto bound_type =
        std::get<ReifiedLinear2Bounds::ReifiedBoundType>(reified_bound);
    if (bound_type == ReifiedLinear2Bounds::ReifiedBoundType::kAlwaysTrue) {
      return trivial_literals_->TrueLiteral().Index();
    }
    if (bound_type == ReifiedLinear2Bounds::ReifiedBoundType::kAlwaysFalse) {
      return trivial_literals_->FalseLiteral().Index();
    }
  }

  return kNoLiteralIndex;
}

Literal IntervalsRepository::GetOrCreatePrecedenceLiteral(AffineExpression x,
                                                          AffineExpression y) {
  {
    const LiteralIndex index = GetPrecedenceLiteral(x, y);
    if (index != kNoLiteralIndex) return Literal(index);
  }

  CHECK(CreatePrecedenceLiteralIfNonTrivial(x, y));
  const LiteralIndex index = GetPrecedenceLiteral(x, y);
  CHECK_NE(index, kNoLiteralIndex);
  return Literal(index);
}

// TODO(user): Ideally we should sort the vector of variables, but right now
// we cannot since we often use this with a parallel vector of demands. So this
// "sorting" should happen in the presolver so we can share as much as possible.
SchedulingConstraintHelper* IntervalsRepository::GetOrCreateHelper(
    std::vector<Literal> enforcement_literals,
    const std::vector<IntervalVariable>& variables,
    bool register_as_disjunctive_helper) {
  std::sort(enforcement_literals.begin(), enforcement_literals.end());
  const auto it = helper_repository_.find({enforcement_literals, variables});
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
  helper->RegisterWith(model_->GetOrCreate<GenericLiteralWatcher>(),
                       enforcement_literals);
  helper_repository_[{enforcement_literals, variables}] = helper;
  model_->TakeOwnership(helper);
  if (register_as_disjunctive_helper) {
    disjunctive_helpers_.push_back(helper);
  }
  return helper;
}

NoOverlap2DConstraintHelper* IntervalsRepository::GetOrCreate2DHelper(
    std::vector<Literal> enforcement_literals,
    const std::vector<IntervalVariable>& x_variables,
    const std::vector<IntervalVariable>& y_variables) {
  std::sort(enforcement_literals.begin(), enforcement_literals.end());
  const auto it = no_overlap_2d_helper_repository_.find(
      {enforcement_literals, x_variables, y_variables});
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
  helper->RegisterWith(model_->GetOrCreate<GenericLiteralWatcher>(),
                       enforcement_literals);
  no_overlap_2d_helper_repository_[{enforcement_literals, x_variables,
                                    y_variables}] = helper;
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
