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
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/meta/type_traits.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/implied_bounds.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/integer_expr.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/model.h"
#include "ortools/sat/precedences.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/util/sort.h"
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
  if (disjunctive_precedences_.contains({a, b})) return;

  std::vector<Literal> enforcement_literals;
  if (IsOptional(a)) enforcement_literals.push_back(PresenceLiteral(a));
  if (IsOptional(b)) enforcement_literals.push_back(PresenceLiteral(b));
  if (sat_solver_->CurrentDecisionLevel() == 0) {
    int new_size = 0;
    for (const Literal l : enforcement_literals) {
      // We can ignore always absent interval, and skip the literal of the
      // interval that are now always present.
      if (assignment_.LiteralIsTrue(l)) continue;
      if (assignment_.LiteralIsFalse(l)) return;
      enforcement_literals[new_size++] = l;
    }
    enforcement_literals.resize(new_size);
  }

  const AffineExpression start_a = Start(a);
  const AffineExpression end_a = End(a);
  const AffineExpression start_b = Start(b);
  const AffineExpression end_b = End(b);

  // task_a is always before task_b ?
  if (integer_trail_->UpperBound(start_a) < integer_trail_->LowerBound(end_b)) {
    AddConditionalAffinePrecedence(enforcement_literals, end_a, start_b,
                                   model_);
    return;
  }

  // task_b is always before task_a ?
  if (integer_trail_->UpperBound(start_b) < integer_trail_->LowerBound(end_a)) {
    AddConditionalAffinePrecedence(enforcement_literals, end_b, start_a,
                                   model_);
    return;
  }

  // Create a new literal.
  const BooleanVariable boolean_var = sat_solver_->NewBooleanVariable();
  const Literal a_before_b = Literal(boolean_var, true);
  disjunctive_precedences_.insert({{a, b}, a_before_b});
  disjunctive_precedences_.insert({{b, a}, a_before_b.Negated()});

  // Also insert it in precedences.
  // TODO(user): also add the reverse like start_b + 1 <= end_a if negated?
  precedences_.insert({{end_a, start_b}, a_before_b});
  precedences_.insert({{end_b, start_a}, a_before_b.Negated()});

  enforcement_literals.push_back(a_before_b);
  AddConditionalAffinePrecedence(enforcement_literals, end_a, start_b, model_);
  enforcement_literals.pop_back();

  enforcement_literals.push_back(a_before_b.Negated());
  AddConditionalAffinePrecedence(enforcement_literals, end_b, start_a, model_);
  enforcement_literals.pop_back();

  // Force the value of boolean_var in case the precedence is not active. This
  // avoids duplicate solutions when enumerating all possible solutions.
  for (const Literal l : enforcement_literals) {
    implications_->AddBinaryClause(l, a_before_b);
  }
}

bool IntervalsRepository::CreatePrecedenceLiteral(IntervalVariable a,
                                                  IntervalVariable b) {
  const AffineExpression x = End(a);
  const AffineExpression y = Start(b);
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
    IntervalVariable a, IntervalVariable b) const {
  const AffineExpression x = End(a);
  const AffineExpression y = Start(b);
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

  SchedulingConstraintHelper* helper =
      new SchedulingConstraintHelper(variables, model_);
  helper_repository_[variables] = helper;
  model_->TakeOwnership(helper);
  if (register_as_disjunctive_helper) {
    disjunctive_helpers_.push_back(helper);
  }
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

SchedulingConstraintHelper::SchedulingConstraintHelper(
    const std::vector<IntervalVariable>& tasks, Model* model)
    : model_(model),
      trail_(model->GetOrCreate<Trail>()),
      sat_solver_(model->GetOrCreate<SatSolver>()),
      integer_trail_(model->GetOrCreate<IntegerTrail>()),
      watcher_(model->GetOrCreate<GenericLiteralWatcher>()),
      precedence_relations_(model->GetOrCreate<PrecedenceRelations>()),
      interval_variables_(tasks),
      capacity_(tasks.size()),
      cached_size_min_(new IntegerValue[capacity_]),
      cached_start_min_(new IntegerValue[capacity_]),
      cached_end_min_(new IntegerValue[capacity_]),
      cached_negated_start_max_(new IntegerValue[capacity_]),
      cached_negated_end_max_(new IntegerValue[capacity_]),
      cached_shifted_start_min_(new IntegerValue[capacity_]),
      cached_negated_shifted_end_max_(new IntegerValue[capacity_]) {
  starts_.clear();
  ends_.clear();
  minus_ends_.clear();
  minus_starts_.clear();
  sizes_.clear();
  reason_for_presence_.clear();

  auto* repository = model->GetOrCreate<IntervalsRepository>();
  for (const IntervalVariable i : tasks) {
    if (repository->IsOptional(i)) {
      reason_for_presence_.push_back(repository->PresenceLiteral(i).Index());
    } else {
      reason_for_presence_.push_back(kNoLiteralIndex);
    }
    sizes_.push_back(repository->Size(i));
    starts_.push_back(repository->Start(i));
    ends_.push_back(repository->End(i));
    minus_starts_.push_back(repository->Start(i).Negated());
    minus_ends_.push_back(repository->End(i).Negated());
  }

  RegisterWith(model->GetOrCreate<GenericLiteralWatcher>());
  InitSortedVectors();
  if (!SynchronizeAndSetTimeDirection(true)) {
    model->GetOrCreate<SatSolver>()->NotifyThatModelIsUnsat();
  }
}

SchedulingConstraintHelper::SchedulingConstraintHelper(int num_tasks,
                                                       Model* model)
    : model_(model),
      trail_(model->GetOrCreate<Trail>()),
      sat_solver_(model->GetOrCreate<SatSolver>()),
      integer_trail_(model->GetOrCreate<IntegerTrail>()),
      precedence_relations_(model->GetOrCreate<PrecedenceRelations>()),
      capacity_(num_tasks),
      cached_size_min_(new IntegerValue[capacity_]),
      cached_start_min_(new IntegerValue[capacity_]),
      cached_end_min_(new IntegerValue[capacity_]),
      cached_negated_start_max_(new IntegerValue[capacity_]),
      cached_negated_end_max_(new IntegerValue[capacity_]),
      cached_shifted_start_min_(new IntegerValue[capacity_]),
      cached_negated_shifted_end_max_(new IntegerValue[capacity_]) {
  starts_.resize(num_tasks);
  CHECK_EQ(NumTasks(), num_tasks);
}

bool SchedulingConstraintHelper::Propagate() {
  recompute_all_cache_ = true;
  for (const int id : propagator_ids_) watcher_->CallOnNextPropagate(id);
  return true;
}

bool SchedulingConstraintHelper::IncrementalPropagate(
    const std::vector<int>& watch_indices) {
  for (const int t : watch_indices) recompute_cache_.Set(t);
  for (const int id : propagator_ids_) watcher_->CallOnNextPropagate(id);
  return true;
}

void SchedulingConstraintHelper::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  const int num_tasks = starts_.size();
  for (int t = 0; t < num_tasks; ++t) {
    watcher->WatchIntegerVariable(sizes_[t].var, id, t);
    watcher->WatchIntegerVariable(starts_[t].var, id, t);
    watcher->WatchIntegerVariable(ends_[t].var, id, t);
  }
  watcher->SetPropagatorPriority(id, 0);
}

bool SchedulingConstraintHelper::UpdateCachedValues(int t) {
  if (IsAbsent(t)) return true;

  IntegerValue smin = integer_trail_->LowerBound(starts_[t]);
  IntegerValue smax = integer_trail_->UpperBound(starts_[t]);
  IntegerValue emin = integer_trail_->LowerBound(ends_[t]);
  IntegerValue emax = integer_trail_->UpperBound(ends_[t]);

  // We take the max for the corner case where the size of an optional interval
  // is used elsewhere and has a domain with negative value.
  //
  // TODO(user): maybe we should just disallow size with a negative domain, but
  // is is harder to enforce if we have a linear expression for size.
  IntegerValue dmin =
      std::max(IntegerValue(0), integer_trail_->LowerBound(sizes_[t]));
  IntegerValue dmax = integer_trail_->UpperBound(sizes_[t]);

  // Detect first if we have a conflict using the relation start + size = end.
  if (dmax < 0) {
    ClearReason();
    AddSizeMaxReason(t, dmax);
    return PushTaskAbsence(t);
  }
  if (smin + dmin - emax > 0) {
    ClearReason();
    AddStartMinReason(t, smin);
    AddSizeMinReason(t, dmin);
    AddEndMaxReason(t, emax);
    return PushTaskAbsence(t);
  }
  if (smax + dmax - emin < 0) {
    ClearReason();
    AddStartMaxReason(t, smax);
    AddSizeMaxReason(t, dmax);
    AddEndMinReason(t, emin);
    return PushTaskAbsence(t);
  }

  // Sometimes, for optional interval with non-optional bounds, this propagation
  // give tighter bounds. We always consider the value assuming
  // the interval is present.
  //
  // Note that this is also useful in case not everything was propagated. Note
  // also that since there is no conflict, we reach the fix point in one pass.
  smin = std::max(smin, emin - dmax);
  smax = std::min(smax, emax - dmin);
  dmin = std::max(dmin, emin - smax);
  emin = std::max(emin, smin + dmin);
  emax = std::min(emax, smax + dmax);

  if (emin != cached_end_min_[t]) {
    recompute_energy_profile_ = true;
  }

  // We might only want to do that if the value changed, but I am not sure it
  // is worth the test.
  recompute_by_start_max_ = true;
  recompute_by_end_min_ = true;

  cached_start_min_[t] = smin;
  cached_end_min_[t] = emin;
  cached_negated_start_max_[t] = -smax;
  cached_negated_end_max_[t] = -emax;
  cached_size_min_[t] = dmin;

  // Note that we use the cached value here for EndMin()/StartMax().
  const IntegerValue new_shifted_start_min = emin - dmin;
  if (new_shifted_start_min != cached_shifted_start_min_[t]) {
    recompute_energy_profile_ = true;
    recompute_shifted_start_min_ = true;
    cached_shifted_start_min_[t] = new_shifted_start_min;
  }
  const IntegerValue new_negated_shifted_end_max = -(smax + dmin);
  if (new_negated_shifted_end_max != cached_negated_shifted_end_max_[t]) {
    recompute_negated_shifted_end_max_ = true;
    cached_negated_shifted_end_max_[t] = new_negated_shifted_end_max;
  }
  return true;
}

bool SchedulingConstraintHelper::ResetFromSubset(
    const SchedulingConstraintHelper& other, absl::Span<const int> tasks) {
  current_time_direction_ = other.current_time_direction_;

  const int num_tasks = tasks.size();
  interval_variables_.resize(num_tasks);
  starts_.resize(num_tasks);
  ends_.resize(num_tasks);
  minus_ends_.resize(num_tasks);
  minus_starts_.resize(num_tasks);
  sizes_.resize(num_tasks);
  reason_for_presence_.resize(num_tasks);
  for (int i = 0; i < num_tasks; ++i) {
    const int t = tasks[i];
    interval_variables_[i] = other.interval_variables_[t];
    starts_[i] = other.starts_[t];
    ends_[i] = other.ends_[t];
    minus_ends_[i] = other.minus_ends_[t];
    minus_starts_[i] = other.minus_starts_[t];
    sizes_[i] = other.sizes_[t];
    reason_for_presence_[i] = other.reason_for_presence_[t];
  }

  InitSortedVectors();
  return SynchronizeAndSetTimeDirection(true);
}

void SchedulingConstraintHelper::InitSortedVectors() {
  const int num_tasks = starts_.size();

  recompute_all_cache_ = true;
  recompute_cache_.Resize(num_tasks);
  for (int t = 0; t < num_tasks; ++t) {
    recompute_cache_.Set(t);
  }

  // Make sure all the cached_* arrays can hold enough data.
  CHECK_LE(num_tasks, capacity_);

  task_by_increasing_start_min_.resize(num_tasks);
  task_by_increasing_end_min_.resize(num_tasks);
  task_by_increasing_negated_start_max_.resize(num_tasks);
  task_by_decreasing_end_max_.resize(num_tasks);
  task_by_increasing_shifted_start_min_.resize(num_tasks);
  task_by_negated_shifted_end_max_.resize(num_tasks);
  for (int t = 0; t < num_tasks; ++t) {
    task_by_increasing_start_min_[t].task_index = t;
    task_by_increasing_end_min_[t].task_index = t;
    task_by_increasing_negated_start_max_[t].task_index = t;
    task_by_decreasing_end_max_[t].task_index = t;

    task_by_increasing_shifted_start_min_[t].task_index = t;
    task_by_increasing_shifted_start_min_[t].presence_lit =
        reason_for_presence_[t];
    task_by_negated_shifted_end_max_[t].task_index = t;
    task_by_negated_shifted_end_max_[t].presence_lit = reason_for_presence_[t];
  }

  recompute_by_start_max_ = true;
  recompute_by_end_min_ = true;
  recompute_energy_profile_ = true;
  recompute_shifted_start_min_ = true;
  recompute_negated_shifted_end_max_ = true;
}

void SchedulingConstraintHelper::SetTimeDirection(bool is_forward) {
  if (current_time_direction_ != is_forward) {
    current_time_direction_ = is_forward;

    std::swap(starts_, minus_ends_);
    std::swap(ends_, minus_starts_);

    std::swap(task_by_increasing_start_min_, task_by_decreasing_end_max_);
    std::swap(task_by_increasing_end_min_,
              task_by_increasing_negated_start_max_);
    std::swap(recompute_by_end_min_, recompute_by_start_max_);
    std::swap(task_by_increasing_shifted_start_min_,
              task_by_negated_shifted_end_max_);

    recompute_energy_profile_ = true;
    std::swap(cached_start_min_, cached_negated_end_max_);
    std::swap(cached_end_min_, cached_negated_start_max_);
    std::swap(cached_shifted_start_min_, cached_negated_shifted_end_max_);
    std::swap(recompute_shifted_start_min_, recompute_negated_shifted_end_max_);
  }
}

bool SchedulingConstraintHelper::SynchronizeAndSetTimeDirection(
    bool is_forward) {
  SetTimeDirection(is_forward);

  // If there was any backtracks since the last time this was called, we
  // recompute our cache.
  if (sat_solver_->num_backtracks() != saved_num_backtracks_) {
    recompute_all_cache_ = true;
    saved_num_backtracks_ = sat_solver_->num_backtracks();
  }

  if (recompute_all_cache_) {
    for (int t = 0; t < recompute_cache_.size(); ++t) {
      if (!UpdateCachedValues(t)) return false;
    }
  } else {
    for (const int t : recompute_cache_) {
      if (!UpdateCachedValues(t)) return false;
    }
  }
  recompute_cache_.ClearAll();
  recompute_all_cache_ = false;
  return true;
}

// TODO(user): be more precise when we know a and b are in disjunction.
// we really just need start_b > start_a, or even >= if duration is non-zero.
IntegerValue SchedulingConstraintHelper::GetCurrentMinDistanceBetweenTasks(
    int a, int b, bool add_reason_if_after) {
  const AffineExpression before = ends_[a];
  const AffineExpression after = starts_[b];
  if (before.var == kNoIntegerVariable || before.coeff != 1 ||
      after.var == kNoIntegerVariable || after.coeff != 1) {
    return kMinIntegerValue;
  }

  // We take the max of the level zero offset and the one coming from a
  // conditional precedence at true.
  const IntegerValue conditional_offset =
      precedence_relations_->GetConditionalOffset(before.var, after.var);
  const IntegerValue known = integer_trail_->LevelZeroLowerBound(after.var) -
                             integer_trail_->LevelZeroUpperBound(before.var);
  const IntegerValue offset = std::max(conditional_offset, known);

  const IntegerValue needed_offset = before.constant - after.constant;
  const IntegerValue distance = offset - needed_offset;
  if (add_reason_if_after && distance >= 0 && known < conditional_offset) {
    for (const Literal l : precedence_relations_->GetConditionalEnforcements(
             before.var, after.var)) {
      literal_reason_.push_back(l.Negated());
    }
  }
  return distance;
}

// Note that we could call this at a positive level to propagate any literal
// associated to task a before task b. However we only call this for task that
// are in detectable precedence, which means the normal precedence or linear
// propagator should have already propagated that Boolean too.
bool SchedulingConstraintHelper::PropagatePrecedence(int a, int b) {
  CHECK(IsPresent(a));
  CHECK(IsPresent(b));
  CHECK_EQ(trail_->CurrentDecisionLevel(), 0);

  const AffineExpression before = ends_[a];
  const AffineExpression after = starts_[b];
  if (after.coeff != 1) return true;
  if (before.coeff != 1) return true;
  if (after.var == kNoIntegerVariable) return true;
  if (before.var == kNoIntegerVariable) return true;
  const IntegerValue offset = before.constant - after.constant;
  if (precedence_relations_->Add(before.var, after.var, offset)) {
    VLOG(2) << "new relation " << TaskDebugString(a)
            << " <= " << TaskDebugString(b);

    // TODO(user): Adding new constraint during propagation might not be the
    // best idea as it can create some complication.
    AddWeightedSumLowerOrEqual({}, {before.var, after.var},
                               {int64_t{1}, int64_t{-1}}, -offset.value(),
                               model_);
    if (model_->GetOrCreate<SatSolver>()->ModelIsUnsat()) return false;
  }
  return true;
}

absl::Span<const TaskTime>
SchedulingConstraintHelper::TaskByIncreasingStartMin() {
  for (TaskTime& ref : task_by_increasing_start_min_) {
    ref.time = StartMin(ref.task_index);
  }
  IncrementalSort(task_by_increasing_start_min_.begin(),
                  task_by_increasing_start_min_.end());
  return task_by_increasing_start_min_;
}

absl::Span<const TaskTime>
SchedulingConstraintHelper::TaskByIncreasingEndMin() {
  if (!recompute_by_end_min_) return task_by_increasing_end_min_;
  for (TaskTime& ref : task_by_increasing_end_min_) {
    ref.time = EndMin(ref.task_index);
  }
  IncrementalSort(task_by_increasing_end_min_.begin(),
                  task_by_increasing_end_min_.end());
  recompute_by_end_min_ = false;
  return task_by_increasing_end_min_;
}

absl::Span<const TaskTime>
SchedulingConstraintHelper::TaskByIncreasingNegatedStartMax() {
  if (!recompute_by_start_max_) return task_by_increasing_negated_start_max_;
  for (TaskTime& ref : task_by_increasing_negated_start_max_) {
    ref.time = cached_negated_start_max_[ref.task_index];
  }
  IncrementalSort(task_by_increasing_negated_start_max_.begin(),
                  task_by_increasing_negated_start_max_.end());
  recompute_by_start_max_ = false;
  return task_by_increasing_negated_start_max_;
}

absl::Span<const TaskTime>
SchedulingConstraintHelper::TaskByDecreasingEndMax() {
  for (TaskTime& ref : task_by_decreasing_end_max_) {
    ref.time = EndMax(ref.task_index);
  }
  IncrementalSort(task_by_decreasing_end_max_.begin(),
                  task_by_decreasing_end_max_.end(), std::greater<TaskTime>());
  return task_by_decreasing_end_max_;
}

absl::Span<const CachedTaskBounds>
SchedulingConstraintHelper::TaskByIncreasingShiftedStartMin() {
  if (recompute_shifted_start_min_) {
    recompute_shifted_start_min_ = false;
    bool is_sorted = true;
    IntegerValue previous = kMinIntegerValue;
    for (CachedTaskBounds& ref : task_by_increasing_shifted_start_min_) {
      ref.time = ShiftedStartMin(ref.task_index);
      is_sorted = is_sorted && ref.time >= previous;
      previous = ref.time;
    }
    if (is_sorted) return task_by_increasing_shifted_start_min_;
    IncrementalSort(task_by_increasing_shifted_start_min_.begin(),
                    task_by_increasing_shifted_start_min_.end());
  }
  return task_by_increasing_shifted_start_min_;
}

// TODO(user): Avoid recomputing it if nothing changed.
const std::vector<SchedulingConstraintHelper::ProfileEvent>&
SchedulingConstraintHelper::GetEnergyProfile() {
  if (energy_profile_.empty()) {
    const int num_tasks = NumTasks();
    for (int t = 0; t < num_tasks; ++t) {
      energy_profile_.push_back(
          {cached_shifted_start_min_[t], t, /*is_first=*/true});
      energy_profile_.push_back({cached_end_min_[t], t, /*is_first=*/false});
    }
  } else {
    if (!recompute_energy_profile_) return energy_profile_;
    for (ProfileEvent& ref : energy_profile_) {
      const int t = ref.task;
      if (ref.is_first) {
        ref.time = cached_shifted_start_min_[t];
      } else {
        ref.time = cached_end_min_[t];
      }
    }
  }
  IncrementalSort(energy_profile_.begin(), energy_profile_.end());
  recompute_energy_profile_ = false;
  return energy_profile_;
}

// Produces a relaxed reason for StartMax(before) < EndMin(after).
void SchedulingConstraintHelper::AddReasonForBeingBefore(int before,
                                                         int after) {
  AddOtherReason(before);
  AddOtherReason(after);

  // The reason will be a linear expression greater than a value. Note that all
  // coeff must be positive, and we will use the variable lower bound.
  std::vector<IntegerVariable> vars;
  std::vector<IntegerValue> coeffs;

  // Reason for StartMax(before).
  const IntegerValue smax_before = StartMax(before);
  if (smax_before >= integer_trail_->UpperBound(starts_[before])) {
    if (starts_[before].var != kNoIntegerVariable) {
      vars.push_back(NegationOf(starts_[before].var));
      coeffs.push_back(starts_[before].coeff);
    }
  } else {
    if (ends_[before].var != kNoIntegerVariable) {
      vars.push_back(NegationOf(ends_[before].var));
      coeffs.push_back(ends_[before].coeff);
    }
    if (sizes_[before].var != kNoIntegerVariable) {
      vars.push_back(sizes_[before].var);
      coeffs.push_back(sizes_[before].coeff);
    }
  }

  // Reason for EndMin(after);
  const IntegerValue emin_after = EndMin(after);
  if (emin_after <= integer_trail_->LowerBound(ends_[after])) {
    if (ends_[after].var != kNoIntegerVariable) {
      vars.push_back(ends_[after].var);
      coeffs.push_back(ends_[after].coeff);
    }
  } else {
    if (starts_[after].var != kNoIntegerVariable) {
      vars.push_back(starts_[after].var);
      coeffs.push_back(starts_[after].coeff);
    }
    if (sizes_[after].var != kNoIntegerVariable) {
      vars.push_back(sizes_[after].var);
      coeffs.push_back(sizes_[after].coeff);
    }
  }

  DCHECK_LT(smax_before, emin_after);
  const IntegerValue slack = emin_after - smax_before - 1;
  integer_trail_->AppendRelaxedLinearReason(slack, coeffs, vars,
                                            &integer_reason_);
}

bool SchedulingConstraintHelper::PushIntegerLiteral(IntegerLiteral lit) {
  CHECK(other_helper_ == nullptr);
  return integer_trail_->Enqueue(lit, literal_reason_, integer_reason_);
}

bool SchedulingConstraintHelper::PushIntegerLiteralIfTaskPresent(
    int t, IntegerLiteral lit) {
  if (IsAbsent(t)) return true;
  AddOtherReason(t);
  ImportOtherReasons();
  if (IsOptional(t)) {
    return integer_trail_->ConditionalEnqueue(
        PresenceLiteral(t), lit, &literal_reason_, &integer_reason_);
  }
  return integer_trail_->Enqueue(lit, literal_reason_, integer_reason_);
}

// We also run directly the precedence propagator for this variable so that when
// we push an interval start for example, we have a chance to push its end.
bool SchedulingConstraintHelper::PushIntervalBound(int t, IntegerLiteral lit) {
  if (!PushIntegerLiteralIfTaskPresent(t, lit)) return false;
  if (IsAbsent(t)) return true;
  if (!UpdateCachedValues(t)) return false;
  recompute_cache_.Clear(t);
  return true;
}

bool SchedulingConstraintHelper::IncreaseStartMin(int t, IntegerValue value) {
  if (starts_[t].var == kNoIntegerVariable) {
    if (value > starts_[t].constant) return PushTaskAbsence(t);
    return true;
  }
  return PushIntervalBound(t, starts_[t].GreaterOrEqual(value));
}

bool SchedulingConstraintHelper::IncreaseEndMin(int t, IntegerValue value) {
  if (ends_[t].var == kNoIntegerVariable) {
    if (value > ends_[t].constant) return PushTaskAbsence(t);
    return true;
  }
  return PushIntervalBound(t, ends_[t].GreaterOrEqual(value));
}

bool SchedulingConstraintHelper::DecreaseEndMax(int t, IntegerValue value) {
  if (ends_[t].var == kNoIntegerVariable) {
    if (value < ends_[t].constant) return PushTaskAbsence(t);
    return true;
  }
  return PushIntervalBound(t, ends_[t].LowerOrEqual(value));
}

bool SchedulingConstraintHelper::PushLiteral(Literal l) {
  integer_trail_->EnqueueLiteral(l, literal_reason_, integer_reason_);
  return true;
}

bool SchedulingConstraintHelper::PushTaskAbsence(int t) {
  if (IsAbsent(t)) return true;
  if (!IsOptional(t)) return ReportConflict();

  AddOtherReason(t);

  if (IsPresent(t)) {
    literal_reason_.push_back(Literal(reason_for_presence_[t]).Negated());
    return ReportConflict();
  }
  ImportOtherReasons();
  integer_trail_->EnqueueLiteral(Literal(reason_for_presence_[t]).Negated(),
                                 literal_reason_, integer_reason_);
  return true;
}

bool SchedulingConstraintHelper::PushTaskPresence(int t) {
  DCHECK_NE(reason_for_presence_[t], kNoLiteralIndex);
  DCHECK(!IsPresent(t));

  AddOtherReason(t);

  if (IsAbsent(t)) {
    literal_reason_.push_back(Literal(reason_for_presence_[t]));
    return ReportConflict();
  }
  ImportOtherReasons();
  integer_trail_->EnqueueLiteral(Literal(reason_for_presence_[t]),
                                 literal_reason_, integer_reason_);
  return true;
}

bool SchedulingConstraintHelper::ReportConflict() {
  ImportOtherReasons();
  return integer_trail_->ReportConflict(literal_reason_, integer_reason_);
}

void SchedulingConstraintHelper::WatchAllTasks(int id, bool watch_max_side) {
  // In all cases, we watch presence literals since this class is not waked up
  // when those changes.
  const int num_tasks = starts_.size();
  for (int t = 0; t < num_tasks; ++t) {
    if (!IsPresent(t) && !IsAbsent(t)) {
      watcher_->WatchLiteral(Literal(reason_for_presence_[t]), id);
    }
  }

  // If everything is watched, it is slighlty more efficient to enqueue the
  // propagator when the helper Propagate() is called. This result in less
  // entries in our watched lists.
  if (watch_max_side) {
    propagator_ids_.push_back(id);
    return;
  }

  // We only watch "min" side.
  for (int t = 0; t < num_tasks; ++t) {
    watcher_->WatchLowerBound(starts_[t], id);
    watcher_->WatchLowerBound(ends_[t], id);
    watcher_->WatchLowerBound(sizes_[t], id);
  }
}

void SchedulingConstraintHelper::AddOtherReason(int t) {
  if (other_helper_ == nullptr || already_added_to_other_reasons_[t]) return;
  already_added_to_other_reasons_[t] = true;
  const int mapped_t = map_to_other_helper_[t];
  other_helper_->AddStartMaxReason(mapped_t, event_for_other_helper_);
  other_helper_->AddEndMinReason(mapped_t, event_for_other_helper_ + 1);
}

void SchedulingConstraintHelper::ImportOtherReasons() {
  if (other_helper_ != nullptr) ImportOtherReasons(*other_helper_);
}

void SchedulingConstraintHelper::ImportOtherReasons(
    const SchedulingConstraintHelper& other_helper) {
  literal_reason_.insert(literal_reason_.end(),
                         other_helper.literal_reason_.begin(),
                         other_helper.literal_reason_.end());
  integer_reason_.insert(integer_reason_.end(),
                         other_helper.integer_reason_.begin(),
                         other_helper.integer_reason_.end());
}

std::string SchedulingConstraintHelper::TaskDebugString(int t) const {
  return absl::StrCat("t=", t, " is_present=",
                      (IsPresent(t)  ? "1"
                       : IsAbsent(t) ? "0"
                                     : "?"),
                      " size=[", SizeMin(t).value(), ",", SizeMax(t).value(),
                      "]", " start=[", StartMin(t).value(), ",",
                      StartMax(t).value(), "]", " end=[", EndMin(t).value(),
                      ",", EndMax(t).value(), "]");
}

IntegerValue SchedulingConstraintHelper::GetMinOverlap(int t,
                                                       IntegerValue start,
                                                       IntegerValue end) const {
  return std::min(std::min(end - start, SizeMin(t)),
                  std::min(EndMin(t) - start, end - StartMax(t)));
}

IntegerValue ComputeEnergyMinInWindow(
    IntegerValue start_min, IntegerValue start_max, IntegerValue end_min,
    IntegerValue end_max, IntegerValue size_min, IntegerValue demand_min,
    absl::Span<const LiteralValueValue> filtered_energy,
    IntegerValue window_start, IntegerValue window_end) {
  if (window_end <= window_start) return IntegerValue(0);

  // Returns zero if the interval do not necessarily overlap.
  if (end_min <= window_start) return IntegerValue(0);
  if (start_max >= window_end) return IntegerValue(0);
  const IntegerValue window_size = window_end - window_start;
  const IntegerValue simple_energy_min =
      demand_min * std::min({end_min - window_start, window_end - start_max,
                             size_min, window_size});
  if (filtered_energy.empty()) return simple_energy_min;

  IntegerValue result = kMaxIntegerValue;
  for (const auto [lit, fixed_size, fixed_demand] : filtered_energy) {
    const IntegerValue alt_end_min = std::max(end_min, start_min + fixed_size);
    const IntegerValue alt_start_max =
        std::min(start_max, end_max - fixed_size);
    const IntegerValue energy_min =
        fixed_demand *
        std::min({alt_end_min - window_start, window_end - alt_start_max,
                  fixed_size, window_size});
    result = std::min(result, energy_min);
  }
  if (result == kMaxIntegerValue) return simple_energy_min;
  return std::max(simple_energy_min, result);
}

SchedulingDemandHelper::SchedulingDemandHelper(
    absl::Span<const AffineExpression> demands,
    SchedulingConstraintHelper* helper, Model* model)
    : integer_trail_(model->GetOrCreate<IntegerTrail>()),
      product_decomposer_(model->GetOrCreate<ProductDecomposer>()),
      sat_solver_(model->GetOrCreate<SatSolver>()),
      assignment_(model->GetOrCreate<SatSolver>()->Assignment()),
      demands_(demands.begin(), demands.end()),
      helper_(helper) {
  const int num_tasks = helper->NumTasks();
  linearized_energies_.resize(num_tasks);
  decomposed_energies_.resize(num_tasks);
  cached_energies_min_.resize(num_tasks, kMinIntegerValue);
  cached_energies_max_.resize(num_tasks, kMaxIntegerValue);
  energy_is_quadratic_.resize(num_tasks, false);

  // We try to init decomposed energies. This is needed for the cuts that are
  // created after we call InitAllDecomposedEnergies().
  InitDecomposedEnergies();
}

void SchedulingDemandHelper::InitDecomposedEnergies() {
  // For the special case were demands is empty.
  const int num_tasks = helper_->NumTasks();
  if (demands_.size() != num_tasks) return;
  for (int t = 0; t < num_tasks; ++t) {
    const AffineExpression size = helper_->Sizes()[t];
    const AffineExpression demand = demands_[t];
    decomposed_energies_[t] = product_decomposer_->TryToDecompose(size, demand);
  }
}

IntegerValue SchedulingDemandHelper::SimpleEnergyMin(int t) const {
  if (demands_.empty()) return kMinIntegerValue;
  return CapProdI(DemandMin(t), helper_->SizeMin(t));
}

IntegerValue SchedulingDemandHelper::LinearEnergyMin(int t) const {
  if (!linearized_energies_[t].has_value()) return kMinIntegerValue;
  return linearized_energies_[t]->Min(*integer_trail_);
}

IntegerValue SchedulingDemandHelper::DecomposedEnergyMin(int t) const {
  if (decomposed_energies_[t].empty()) return kMinIntegerValue;
  IntegerValue result = kMaxIntegerValue;
  for (const auto [lit, fixed_size, fixed_demand] : decomposed_energies_[t]) {
    if (assignment_.LiteralIsTrue(lit)) {
      return fixed_size * fixed_demand;
    }
    if (assignment_.LiteralIsFalse(lit)) continue;
    result = std::min(result, fixed_size * fixed_demand);
  }
  DCHECK_NE(result, kMaxIntegerValue);
  return result;
}

IntegerValue SchedulingDemandHelper::SimpleEnergyMax(int t) const {
  if (demands_.empty()) return kMaxIntegerValue;
  return CapProdI(DemandMax(t), helper_->SizeMax(t));
}

IntegerValue SchedulingDemandHelper::LinearEnergyMax(int t) const {
  if (!linearized_energies_[t].has_value()) return kMaxIntegerValue;
  return linearized_energies_[t]->Max(*integer_trail_);
}

IntegerValue SchedulingDemandHelper::DecomposedEnergyMax(int t) const {
  if (decomposed_energies_[t].empty()) return kMaxIntegerValue;
  IntegerValue result = kMinIntegerValue;
  for (const auto [lit, fixed_size, fixed_demand] : decomposed_energies_[t]) {
    if (assignment_.LiteralIsTrue(lit)) {
      return fixed_size * fixed_demand;
    }
    if (assignment_.LiteralIsFalse(lit)) continue;
    result = std::max(result, fixed_size * fixed_demand);
  }
  DCHECK_NE(result, kMinIntegerValue);
  return result;
}

bool SchedulingDemandHelper::CacheAllEnergyValues() {
  const int num_tasks = cached_energies_min_.size();
  const bool is_at_level_zero = sat_solver_->CurrentDecisionLevel() == 0;
  for (int t = 0; t < num_tasks; ++t) {
    // Try to reduce the size of the decomposed energy vector.
    if (is_at_level_zero) {
      int new_size = 0;
      for (int i = 0; i < decomposed_energies_[t].size(); ++i) {
        if (assignment_.LiteralIsFalse(decomposed_energies_[t][i].literal)) {
          continue;
        }
        decomposed_energies_[t][new_size++] = decomposed_energies_[t][i];
      }
      decomposed_energies_[t].resize(new_size);
    }

    cached_energies_min_[t] = std::max(
        {SimpleEnergyMin(t), LinearEnergyMin(t), DecomposedEnergyMin(t)});
    if (cached_energies_min_[t] <= kMinIntegerValue) return false;
    energy_is_quadratic_[t] =
        decomposed_energies_[t].empty() && !demands_.empty() &&
        !integer_trail_->IsFixed(demands_[t]) && !helper_->SizeIsFixed(t);
    cached_energies_max_[t] = std::min(
        {SimpleEnergyMax(t), LinearEnergyMax(t), DecomposedEnergyMax(t)});
    if (cached_energies_max_[t] >= kMaxIntegerValue) return false;
  }

  return true;
}

IntegerValue SchedulingDemandHelper::DemandMin(int t) const {
  DCHECK_LT(t, demands_.size());
  return integer_trail_->LowerBound(demands_[t]);
}

IntegerValue SchedulingDemandHelper::DemandMax(int t) const {
  DCHECK_LT(t, demands_.size());
  return integer_trail_->UpperBound(demands_[t]);
}

bool SchedulingDemandHelper::DemandIsFixed(int t) const {
  return integer_trail_->IsFixed(demands_[t]);
}

bool SchedulingDemandHelper::DecreaseEnergyMax(int t, IntegerValue value) {
  if (value < EnergyMin(t)) {
    if (helper_->IsOptional(t)) {
      return helper_->PushTaskAbsence(t);
    } else {
      return helper_->ReportConflict();
    }
  } else if (!decomposed_energies_[t].empty()) {
    for (const auto [lit, fixed_size, fixed_demand] : decomposed_energies_[t]) {
      if (fixed_size * fixed_demand > value) {
        if (assignment_.LiteralIsTrue(lit)) return helper_->ReportConflict();
        if (assignment_.LiteralIsFalse(lit)) continue;
        if (!helper_->PushLiteral(lit.Negated())) return false;
      }
    }
  } else if (linearized_energies_[t].has_value() &&
             linearized_energies_[t]->vars.size() == 1) {
    const LinearExpression& e = linearized_energies_[t].value();
    const AffineExpression affine_energy(e.vars[0], e.coeffs[0], e.offset);
    const IntegerLiteral deduction = affine_energy.LowerOrEqual(value);
    if (!helper_->PushIntegerLiteralIfTaskPresent(t, deduction)) {
      return false;
    }
  } else {
    // TODO(user): Propagate if possible.
    VLOG(3) << "Cumulative energy missed propagation";
  }
  return true;
}

void SchedulingDemandHelper::AddDemandMinReason(int t) {
  DCHECK_LT(t, demands_.size());
  if (demands_[t].var != kNoIntegerVariable) {
    helper_->MutableIntegerReason()->push_back(
        integer_trail_->LowerBoundAsLiteral(demands_[t].var));
  }
}

void SchedulingDemandHelper::AddDemandMinReason(int t,
                                                IntegerValue min_demand) {
  DCHECK_LT(t, demands_.size());
  if (demands_[t].var != kNoIntegerVariable) {
    helper_->MutableIntegerReason()->push_back(
        demands_[t].GreaterOrEqual(min_demand));
  }
}

void SchedulingDemandHelper::AddEnergyMinReason(int t) {
  // We prefer these reason in order.
  const IntegerValue value = cached_energies_min_[t];
  if (DecomposedEnergyMin(t) >= value) {
    auto* reason = helper_->MutableLiteralReason();
    const int old_size = reason->size();
    for (const auto [lit, fixed_size, fixed_demand] : decomposed_energies_[t]) {
      if (assignment_.LiteralIsTrue(lit)) {
        reason->resize(old_size);
        reason->push_back(lit.Negated());
        return;
      } else if (fixed_size * fixed_demand < value &&
                 assignment_.LiteralIsFalse(lit)) {
        reason->push_back(lit);
      }
    }
  } else if (SimpleEnergyMin(t) >= value) {
    AddDemandMinReason(t);
    helper_->AddSizeMinReason(t);
  } else {
    DCHECK_GE(LinearEnergyMin(t), value);
    for (const IntegerVariable var : linearized_energies_[t]->vars) {
      helper_->MutableIntegerReason()->push_back(
          integer_trail_->LowerBoundAsLiteral(var));
    }
  }
}

bool SchedulingDemandHelper::AddLinearizedDemand(
    int t, LinearConstraintBuilder* builder) const {
  if (helper_->IsPresent(t)) {
    if (!decomposed_energies_[t].empty()) {
      for (const LiteralValueValue& entry : decomposed_energies_[t]) {
        if (!builder->AddLiteralTerm(entry.literal, entry.right_value)) {
          return false;
        }
      }
    } else {
      builder->AddTerm(demands_[t], IntegerValue(1));
    }
  } else if (!helper_->IsAbsent(t)) {
    return builder->AddLiteralTerm(helper_->PresenceLiteral(t), DemandMin(t));
  }
  return true;
}

void SchedulingDemandHelper::OverrideLinearizedEnergies(
    absl::Span<const LinearExpression> energies) {
  const int num_tasks = energies.size();
  DCHECK_EQ(num_tasks, helper_->NumTasks());
  linearized_energies_.resize(num_tasks);
  for (int t = 0; t < num_tasks; ++t) {
    linearized_energies_[t] = energies[t];
    if (DEBUG_MODE) {
      for (const IntegerValue coeff : linearized_energies_[t]->coeffs) {
        DCHECK_GE(coeff, 0);
      }
    }
  }
}

std::vector<LiteralValueValue> SchedulingDemandHelper::FilteredDecomposedEnergy(
    int index) {
  if (decomposed_energies_[index].empty()) return {};
  if (sat_solver_->CurrentDecisionLevel() == 0) {
    // CacheAllEnergyValues has already filtered false literals.
    return decomposed_energies_[index];
  }

  // Scan and filter false literals.
  std::vector<LiteralValueValue> result;
  for (const auto& e : decomposed_energies_[index]) {
    if (assignment_.LiteralIsFalse(e.literal)) continue;
    result.push_back(e);
  }
  return result;
}

void SchedulingDemandHelper::OverrideDecomposedEnergies(
    const std::vector<std::vector<LiteralValueValue>>& energies) {
  DCHECK_EQ(energies.size(), helper_->NumTasks());
  decomposed_energies_ = energies;
}

IntegerValue SchedulingDemandHelper::EnergyMinInWindow(
    int t, IntegerValue window_start, IntegerValue window_end) {
  return ComputeEnergyMinInWindow(
      helper_->StartMin(t), helper_->StartMax(t), helper_->EndMin(t),
      helper_->EndMax(t), helper_->SizeMin(t), DemandMin(t),
      FilteredDecomposedEnergy(t), window_start, window_end);
}

// Since we usually ask way less often for the reason, we redo the computation
// here.
void SchedulingDemandHelper::AddEnergyMinInWindowReason(
    int t, IntegerValue window_start, IntegerValue window_end) {
  const IntegerValue actual_energy_min =
      EnergyMinInWindow(t, window_start, window_end);
  if (actual_energy_min == 0) return;

  // Return simple reason right away if there is no decomposition or the simple
  // energy is enough.
  const IntegerValue start_max = helper_->StartMax(t);
  const IntegerValue end_min = helper_->EndMin(t);
  const IntegerValue min_overlap =
      helper_->GetMinOverlap(t, window_start, window_end);
  const IntegerValue simple_energy_min = DemandMin(t) * min_overlap;
  if (simple_energy_min == actual_energy_min) {
    AddDemandMinReason(t);
    helper_->AddSizeMinReason(t);
    helper_->AddStartMaxReason(t, start_max);
    helper_->AddEndMinReason(t, end_min);
    return;
  }

  // TODO(user): only include the one we need?
  const IntegerValue start_min = helper_->StartMin(t);
  const IntegerValue end_max = helper_->EndMax(t);
  DCHECK(!decomposed_energies_[t].empty());
  helper_->AddStartMinReason(t, start_min);
  helper_->AddStartMaxReason(t, start_max);
  helper_->AddEndMinReason(t, end_min);
  helper_->AddEndMaxReason(t, end_max);

  auto* literal_reason = helper_->MutableLiteralReason();
  const int old_size = literal_reason->size();

  DCHECK(!decomposed_energies_[t].empty());
  for (const auto [lit, fixed_size, fixed_demand] : decomposed_energies_[t]) {
    // Should be the same in most cases.
    if (assignment_.LiteralIsTrue(lit)) {
      literal_reason->resize(old_size);
      literal_reason->push_back(lit.Negated());
      return;
    }
    if (assignment_.LiteralIsFalse(lit)) {
      const IntegerValue alt_em = std::max(end_min, start_min + fixed_size);
      const IntegerValue alt_sm = std::min(start_max, end_max - fixed_size);
      const IntegerValue energy_min =
          fixed_demand *
          std::min({alt_em - window_start, window_end - alt_sm, fixed_size});
      if (energy_min >= actual_energy_min) continue;
      literal_reason->push_back(lit);
    }
  }
}

void AddIntegerVariableFromIntervals(SchedulingConstraintHelper* helper,
                                     Model* model,
                                     std::vector<IntegerVariable>* vars) {
  IntegerEncoder* encoder = model->GetOrCreate<IntegerEncoder>();
  for (int t = 0; t < helper->NumTasks(); ++t) {
    if (helper->Starts()[t].var != kNoIntegerVariable) {
      vars->push_back(helper->Starts()[t].var);
    }
    if (helper->Sizes()[t].var != kNoIntegerVariable) {
      vars->push_back(helper->Sizes()[t].var);
    }
    if (helper->Ends()[t].var != kNoIntegerVariable) {
      vars->push_back(helper->Ends()[t].var);
    }
    if (helper->IsOptional(t) && !helper->IsAbsent(t) &&
        !helper->IsPresent(t)) {
      const Literal l = helper->PresenceLiteral(t);
      IntegerVariable view = kNoIntegerVariable;
      if (!encoder->LiteralOrNegationHasView(l, &view)) {
        view = model->Add(NewIntegerVariableFromLiteral(l));
      }
      vars->push_back(view);
    }
  }
}

void AppendVariablesFromCapacityAndDemands(
    const AffineExpression& capacity, SchedulingDemandHelper* demands_helper,
    Model* model, std::vector<IntegerVariable>* vars) {
  auto* integer_trail = model->GetOrCreate<IntegerTrail>();
  for (const AffineExpression& demand_expr : demands_helper->Demands()) {
    if (!integer_trail->IsFixed(demand_expr)) {
      vars->push_back(demand_expr.var);
    }
  }
  IntegerEncoder* encoder = model->GetOrCreate<IntegerEncoder>();
  for (const auto& product : demands_helper->DecomposedEnergies()) {
    for (const auto& lit_val_val : product) {
      IntegerVariable view = kNoIntegerVariable;
      if (!encoder->LiteralOrNegationHasView(lit_val_val.literal, &view)) {
        view = model->Add(NewIntegerVariableFromLiteral(lit_val_val.literal));
      }
      vars->push_back(view);
    }
  }

  if (!integer_trail->IsFixed(capacity)) {
    vars->push_back(capacity.var);
  }
}

}  // namespace sat
}  // namespace operations_research
