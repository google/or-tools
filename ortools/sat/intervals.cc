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

#include "ortools/sat/intervals.h"

#include <memory>

#include "ortools/util/sort.h"

namespace operations_research {
namespace sat {

IntervalVariable IntervalsRepository::CreateInterval(IntegerVariable start,
                                                     IntegerVariable end,
                                                     IntegerVariable size,
                                                     IntegerValue fixed_size,
                                                     LiteralIndex is_present) {
  // Create the interval.
  const IntervalVariable i(start_vars_.size());
  start_vars_.push_back(start);
  end_vars_.push_back(end);
  size_vars_.push_back(size);
  fixed_sizes_.push_back(fixed_size);
  is_present_.push_back(is_present);

  std::vector<Literal> enforcement_literals;
  if (is_present != kNoLiteralIndex) {
    enforcement_literals.push_back(Literal(is_present));
  }

  // Link properly all its components.
  precedences_->AddPrecedenceWithAllOptions(StartVar(i), EndVar(i), fixed_size,
                                            SizeVar(i), enforcement_literals);
  precedences_->AddPrecedenceWithAllOptions(EndVar(i), StartVar(i), -fixed_size,
                                            SizeVar(i) == kNoIntegerVariable
                                                ? kNoIntegerVariable
                                                : NegationOf(SizeVar(i)),
                                            enforcement_literals);
  return i;
}

SchedulingConstraintHelper::SchedulingConstraintHelper(
    const std::vector<IntervalVariable>& tasks, Model* model)
    : trail_(model->GetOrCreate<Trail>()),
      integer_trail_(model->GetOrCreate<IntegerTrail>()),
      precedences_(model->GetOrCreate<PrecedencesPropagator>()),
      current_time_direction_(true),
      visible_intervals_(tasks.size(), true) {
  auto* repository = model->GetOrCreate<IntervalsRepository>();
  start_vars_.clear();
  end_vars_.clear();
  minus_end_vars_.clear();
  minus_start_vars_.clear();
  duration_vars_.clear();
  fixed_durations_.clear();
  reason_for_presence_.clear();
  for (const IntervalVariable i : tasks) {
    if (repository->IsOptional(i)) {
      reason_for_presence_.push_back(repository->IsPresentLiteral(i).Index());
    } else {
      reason_for_presence_.push_back(kNoLiteralIndex);
    }
    if (repository->SizeVar(i) == kNoIntegerVariable) {
      duration_vars_.push_back(kNoIntegerVariable);
      fixed_durations_.push_back(repository->MinSize(i));
    } else {
      duration_vars_.push_back(repository->SizeVar(i));
      fixed_durations_.push_back(IntegerValue(0));
    }
    start_vars_.push_back(repository->StartVar(i));
    end_vars_.push_back(repository->EndVar(i));
    minus_start_vars_.push_back(NegationOf(repository->StartVar(i)));
    minus_end_vars_.push_back(NegationOf(repository->EndVar(i)));
  }

  const int num_tasks = start_vars_.size();
  task_by_increasing_min_start_.resize(num_tasks);
  task_by_increasing_min_end_.resize(num_tasks);
  task_by_decreasing_max_start_.resize(num_tasks);
  task_by_decreasing_max_end_.resize(num_tasks);
  task_by_increasing_shifted_start_min_.resize(num_tasks);
  task_by_decreasing_shifted_end_max_.resize(num_tasks);
  for (int t = 0; t < num_tasks; ++t) {
    task_by_increasing_min_start_[t].task_index = t;
    task_by_increasing_min_end_[t].task_index = t;
    task_by_decreasing_max_start_[t].task_index = t;
    task_by_decreasing_max_end_[t].task_index = t;
    task_by_increasing_shifted_start_min_[t].task_index = t;
    task_by_decreasing_shifted_end_max_[t].task_index = t;
  }
}

void SchedulingConstraintHelper::SetTimeDirection(bool is_forward) {
  if (current_time_direction_ == is_forward) return;
  current_time_direction_ = is_forward;

  std::swap(start_vars_, minus_end_vars_);
  std::swap(end_vars_, minus_start_vars_);
  std::swap(task_by_increasing_min_start_, task_by_decreasing_max_end_);
  std::swap(task_by_increasing_min_end_, task_by_decreasing_max_start_);
  std::swap(task_by_increasing_shifted_start_min_,
            task_by_decreasing_shifted_end_max_);
}

const std::vector<SchedulingConstraintHelper::TaskTime>&
SchedulingConstraintHelper::TaskByIncreasingStartMin() {
  const int num_tasks = NumTasks();
  for (int i = 0; i < num_tasks; ++i) {
    TaskTime& ref = task_by_increasing_min_start_[i];
    ref.time = StartMin(ref.task_index);
  }
  IncrementalSort(task_by_increasing_min_start_.begin(),
                  task_by_increasing_min_start_.end());
  return task_by_increasing_min_start_;
}

const std::vector<SchedulingConstraintHelper::TaskTime>&
SchedulingConstraintHelper::TaskByIncreasingEndMin() {
  const int num_tasks = NumTasks();
  for (int i = 0; i < num_tasks; ++i) {
    TaskTime& ref = task_by_increasing_min_end_[i];
    ref.time = EndMin(ref.task_index);
  }
  IncrementalSort(task_by_increasing_min_end_.begin(),
                  task_by_increasing_min_end_.end());
  return task_by_increasing_min_end_;
}

const std::vector<SchedulingConstraintHelper::TaskTime>&
SchedulingConstraintHelper::TaskByDecreasingStartMax() {
  const int num_tasks = NumTasks();
  for (int i = 0; i < num_tasks; ++i) {
    TaskTime& ref = task_by_decreasing_max_start_[i];
    ref.time = StartMax(ref.task_index);
  }
  IncrementalSort(task_by_decreasing_max_start_.begin(),
                  task_by_decreasing_max_start_.end(),
                  std::greater<TaskTime>());
  return task_by_decreasing_max_start_;
}

const std::vector<SchedulingConstraintHelper::TaskTime>&
SchedulingConstraintHelper::TaskByDecreasingEndMax() {
  const int num_tasks = NumTasks();
  for (int i = 0; i < num_tasks; ++i) {
    TaskTime& ref = task_by_decreasing_max_end_[i];
    ref.time = EndMax(ref.task_index);
  }
  IncrementalSort(task_by_decreasing_max_end_.begin(),
                  task_by_decreasing_max_end_.end(), std::greater<TaskTime>());
  return task_by_decreasing_max_end_;
}

const std::vector<SchedulingConstraintHelper::TaskTime>&
SchedulingConstraintHelper::TaskByIncreasingShiftedStartMin() {
  const int num_tasks = NumTasks();
  for (int i = 0; i < num_tasks; ++i) {
    TaskTime& ref = task_by_increasing_shifted_start_min_[i];
    ref.time = ShiftedStartMin(ref.task_index);
  }
  IncrementalSort(task_by_increasing_shifted_start_min_.begin(),
                  task_by_increasing_shifted_start_min_.end());
  return task_by_increasing_shifted_start_min_;
}

// Produces a relaxed reason for StartMax(before) < EndMin(after).
void SchedulingConstraintHelper::AddReasonForBeingBefore(int before,
                                                         int after) {
  AddOtherReason(before);
  AddOtherReason(after);

  const IntegerLiteral end_min_lit =
      integer_trail_->LowerBoundAsLiteral(end_vars_[after]);
  const IntegerLiteral start_max_lit =
      integer_trail_->UpperBoundAsLiteral(start_vars_[before]);

  DCHECK_LT(StartMax(before), EndMin(after));
  DCHECK_EQ(EndMin(after), end_min_lit.bound);
  DCHECK_EQ(StartMax(before), -start_max_lit.bound);

  const IntegerValue slack = end_min_lit.bound + start_max_lit.bound - 1;
  if (slack == 0) {
    integer_reason_.push_back(end_min_lit);
    integer_reason_.push_back(start_max_lit);
    return;
  }
  integer_trail_->AppendRelaxedLinearReason(
      slack, {IntegerValue(1), IntegerValue(1)},
      {end_min_lit.var, start_max_lit.var}, &integer_reason_);
}

bool SchedulingConstraintHelper::PushIntegerLiteral(IntegerLiteral bound) {
  CHECK(other_helper_ == nullptr);
  return integer_trail_->Enqueue(bound, literal_reason_, integer_reason_);
}

bool SchedulingConstraintHelper::PushIntervalBound(int t, IntegerLiteral lit) {
  if (IsAbsent(t)) return true;
  AddOtherReason(t);

  // TODO(user): we can also push lit.var if its presence implies the interval
  // presence.
  if (IsOptional(t) && integer_trail_->OptionalLiteralIndex(lit.var) !=
                           reason_for_presence_[t]) {
    if (IsPresent(t)) {
      // We can still push, but we do need the presence reason.
      AddPresenceReason(t);
    } else {
      // In this case we cannot push lit.var, but we may detect the interval
      // absence.
      if (lit.bound > integer_trail_->UpperBound(lit.var)) {
        integer_reason_.push_back(
            IntegerLiteral::LowerOrEqual(lit.var, lit.bound - 1));
        if (!PushTaskAbsence(t)) return false;
      }
      return true;
    }
  }

  ImportOtherReasons();
  if (!integer_trail_->Enqueue(lit, literal_reason_, integer_reason_)) {
    return false;
  }

  if (IsAbsent(t)) return true;
  return precedences_->PropagateOutgoingArcs(lit.var);
}

bool SchedulingConstraintHelper::IncreaseStartMin(int t,
                                                  IntegerValue new_min_start) {
  return PushIntervalBound(
      t, IntegerLiteral::GreaterOrEqual(start_vars_[t], new_min_start));
}

bool SchedulingConstraintHelper::DecreaseEndMax(int t,
                                                IntegerValue new_max_end) {
  return PushIntervalBound(
      t, IntegerLiteral::LowerOrEqual(end_vars_[t], new_max_end));
}

bool SchedulingConstraintHelper::PushTaskAbsence(int t) {
  DCHECK_NE(reason_for_presence_[t], kNoLiteralIndex);
  DCHECK(!IsAbsent(t));

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

bool SchedulingConstraintHelper::ReportConflict() {
  ImportOtherReasons();
  return integer_trail_->ReportConflict(literal_reason_, integer_reason_);
}

void SchedulingConstraintHelper::WatchAllTasks(
    int id, GenericLiteralWatcher* watcher) const {
  const int num_tasks = start_vars_.size();
  for (int t = 0; t < num_tasks; ++t) {
    watcher->WatchIntegerVariable(start_vars_[t], id);
    watcher->WatchIntegerVariable(end_vars_[t], id);
    if (duration_vars_[t] != kNoIntegerVariable) {
      watcher->WatchLowerBound(duration_vars_[t], id);
    }
    if (!IsPresent(t) && !IsAbsent(t)) {
      watcher->WatchLiteral(Literal(reason_for_presence_[t]), id);
    }
  }
}

void SchedulingConstraintHelper::AddOtherReason(int t) {
  if (other_helper_ == nullptr || already_added_to_other_reasons_[t]) return;
  already_added_to_other_reasons_[t] = true;
  other_helper_->AddStartMaxReason(t, event_for_other_helper_);
  other_helper_->AddEndMinReason(t, event_for_other_helper_ + 1);
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

void SchedulingConstraintHelper::SetAllIntervalsVisible() {
  visible_intervals_.assign(NumTasks(), true);
}

void SchedulingConstraintHelper::SetVisibleIntervals(
    const std::vector<int>& visible_intervals) {
  visible_intervals_.assign(NumTasks(), false);
  for (const int t : visible_intervals) {
    visible_intervals_[t] = true;
  }
}

}  // namespace sat
}  // namespace operations_research
