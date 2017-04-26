// Copyright 2010-2014 Google
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

  // Link properly all its components.
  if (SizeVar(i) != kNoIntegerVariable) {
    precedences_->AddPrecedenceWithVariableOffset(StartVar(i), EndVar(i),
                                                  SizeVar(i));
    precedences_->AddPrecedenceWithVariableOffset(EndVar(i), StartVar(i),
                                                  NegationOf(SizeVar(i)));
  } else {
    precedences_->AddPrecedenceWithOffset(StartVar(i), EndVar(i), fixed_size);
    precedences_->AddPrecedenceWithOffset(EndVar(i), StartVar(i), -fixed_size);
  }
  if (IsOptional(i)) {
    const Literal literal(is_present);
    precedences_->MarkIntegerVariableAsOptional(StartVar(i), literal);
    integer_trail_->MarkIntegerVariableAsOptional(StartVar(i), literal);
    precedences_->MarkIntegerVariableAsOptional(EndVar(i), literal);
    integer_trail_->MarkIntegerVariableAsOptional(EndVar(i), literal);
    if (SizeVar(i) != kNoIntegerVariable) {
      // TODO(user): This is not currently fully supported in precedences_
      // if the size is not a constant variable.
      CHECK_EQ(integer_trail_->LowerBound(SizeVar(i)),
               integer_trail_->UpperBound(SizeVar(i)));
      precedences_->MarkIntegerVariableAsOptional(SizeVar(i), literal);
      integer_trail_->MarkIntegerVariableAsOptional(SizeVar(i), literal);
    }
  }
  return i;
}

SchedulingConstraintHelper::SchedulingConstraintHelper(
    const std::vector<IntervalVariable>& tasks, Trail* trail,
    IntegerTrail* integer_trail, IntervalsRepository* repository)
    : trail_(trail),
      integer_trail_(integer_trail),
      current_time_direction_(true) {
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
  for (int t = 0; t < num_tasks; ++t) {
    task_by_increasing_min_start_[t].task_index = t;
    task_by_increasing_min_end_[t].task_index = t;
    task_by_decreasing_max_start_[t].task_index = t;
    task_by_decreasing_max_end_[t].task_index = t;
  }
}

void SchedulingConstraintHelper::SetTimeDirection(bool is_forward) {
  if (current_time_direction_ == is_forward) return;
  current_time_direction_ = is_forward;

  std::swap(start_vars_, minus_end_vars_);
  std::swap(end_vars_, minus_start_vars_);
  std::swap(task_by_increasing_min_start_, task_by_decreasing_max_end_);
  std::swap(task_by_increasing_min_end_, task_by_decreasing_max_start_);
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

bool SchedulingConstraintHelper::PushIntegerLiteral(IntegerLiteral bound) {
  return integer_trail_->Enqueue(bound, literal_reason_, integer_reason_);
}

bool SchedulingConstraintHelper::IncreaseStartMin(int t,
                                                  IntegerValue new_min_start) {
  if (!integer_trail_->Enqueue(
          IntegerLiteral::GreaterOrEqual(start_vars_[t], new_min_start),
          literal_reason_, integer_reason_)) {
    return false;
  }

  // Skip if the task is now absent.
  if (IsAbsent(t)) return true;

  // We propagate right away the new end-min lower-bound we have.
  const IntegerValue min_end_lb = new_min_start + DurationMin(t);
  if (EndMin(t) < min_end_lb) {
    ClearReason();
    AddStartMinReason(t, new_min_start);
    AddDurationMinReason(t);
    if (!integer_trail_->Enqueue(
            IntegerLiteral::GreaterOrEqual(end_vars_[t], min_end_lb),
            literal_reason_, integer_reason_)) {
      return false;
    }
  }

  return true;
}

bool SchedulingConstraintHelper::DecreaseEndMax(int t,
                                                IntegerValue new_max_end) {
  if (!integer_trail_->Enqueue(
          IntegerLiteral::LowerOrEqual(end_vars_[t], new_max_end),
          literal_reason_, integer_reason_)) {
    return false;
  }

  // Skip if the task is now absent.
  if (IsAbsent(t)) return true;

  // We propagate right away the new start-max upper-bound we have.
  const IntegerValue max_start_ub = new_max_end - DurationMin(t);
  if (StartMax(t) > max_start_ub) {
    ClearReason();
    AddEndMaxReason(t, new_max_end);
    AddDurationMinReason(t);
    if (!integer_trail_->Enqueue(
            IntegerLiteral::LowerOrEqual(start_vars_[t], max_start_ub),
            literal_reason_, integer_reason_)) {
      return false;
    }
  }

  return true;
}

void SchedulingConstraintHelper::PushTaskAbsence(int t) {
  DCHECK_NE(reason_for_presence_[t], kNoLiteralIndex);
  DCHECK(!IsPresent(t));
  DCHECK(!IsAbsent(t));
  integer_trail_->EnqueueLiteral(Literal(reason_for_presence_[t]).Negated(),
                                 literal_reason_, integer_reason_);
}

bool SchedulingConstraintHelper::ReportConflict() {
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

}  // namespace sat
}  // namespace operations_research
