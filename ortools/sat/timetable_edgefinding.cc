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

#include "ortools/sat/timetable_edgefinding.h"

#include <algorithm>
#include <vector>

#include "ortools/base/int_type.h"
#include "ortools/util/sort.h"

namespace operations_research {
namespace sat {

TimeTableEdgeFinding::TimeTableEdgeFinding(
    const std::vector<IntervalVariable>& interval_vars,
    const std::vector<IntegerVariable>& demand_vars, IntegerVariable capacity,
    Trail* trail, IntegerTrail* integer_trail,
    IntervalsRepository* intervals_repository)
    : num_tasks_(interval_vars.size()),
      interval_vars_(interval_vars),
      demand_vars_(demand_vars),
      capacity_var_(capacity),
      trail_(trail),
      integer_trail_(integer_trail),
      intervals_repository_(intervals_repository) {
  // Cached domains.
  start_min_.resize(num_tasks_);
  start_max_.resize(num_tasks_);
  end_min_.resize(num_tasks_);
  end_max_.resize(num_tasks_);
  duration_min_.resize(num_tasks_);
  demand_min_.resize(num_tasks_);

  // Edge finding structures.
  by_start_min_.reserve(num_tasks_);
  by_end_max_.reserve(num_tasks_);
  mandatory_energy_before_end_max_.resize(num_tasks_);
  mandatory_energy_before_start_min_.resize(num_tasks_);

  // Energy of free parts.
  energy_free_.resize(num_tasks_);

  // Collect the variables.
  start_vars_.resize(num_tasks_);
  end_vars_.resize(num_tasks_);
  mirror_start_vars_.resize(num_tasks_);
  mirror_end_vars_.resize(num_tasks_);
  duration_vars_.resize(num_tasks_);
  for (int t = 0; t < num_tasks_; ++t) {
    const IntervalVariable i = interval_vars[t];
    start_vars_[t] = intervals_repository->StartVar(i);
    end_vars_[t] = intervals_repository->EndVar(i);
    mirror_start_vars_[t] = NegationOf(end_vars_[t]);
    mirror_end_vars_[t] = NegationOf(start_vars_[t]);
    duration_vars_[t] = intervals_repository->SizeVar(i);
    by_start_min_.push_back(TaskTime(t, IntegerValue(0)));
    by_start_max_.push_back(TaskTime(t, IntegerValue(0)));
    by_end_min_.push_back(TaskTime(t, IntegerValue(0)));
    by_end_max_.push_back(TaskTime(t, IntegerValue(0)));
  }
}

void TimeTableEdgeFinding::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  watcher->WatchUpperBound(capacity_var_, id);
  for (int t = 0; t < num_tasks_; t++) {
    watcher->WatchIntegerVariable(start_vars_[t], id);
    watcher->WatchIntegerVariable(end_vars_[t], id);
    watcher->WatchLowerBound(demand_vars_[t], id);
    if (duration_vars_[t] != kNoIntegerVariable) {
      watcher->WatchLowerBound(duration_vars_[t], id);
    }
    if (!IsPresent(t) && !IsAbsent(t)) {
      const Literal is_present =
          intervals_repository_->IsPresentLiteral(interval_vars_[t]);
      watcher->WatchLiteral(is_present, id);
    }
  }
}

bool TimeTableEdgeFinding::IsPresent(int task_id) const {
  if (intervals_repository_->IsOptional(interval_vars_[task_id])) {
    const Literal is_present =
        intervals_repository_->IsPresentLiteral(interval_vars_[task_id]);
    return trail_->Assignment().LiteralIsTrue(is_present);
  }
  return true;
}

bool TimeTableEdgeFinding::IsAbsent(int task_id) const {
  if (intervals_repository_->IsOptional(interval_vars_[task_id])) {
    const Literal is_present =
        intervals_repository_->IsPresentLiteral(interval_vars_[task_id]);
    return trail_->Assignment().LiteralIsFalse(is_present);
  }
  return false;
}

bool TimeTableEdgeFinding::Propagate() {
  while (true) {
    const int64 old_timestamp = integer_trail_->num_enqueues();
    // Update the start variables.
    if (!TimeTableEdgeFindingPass()) return false;
    SwitchToMirrorProblem();
    // Update the end variables.
    if (!TimeTableEdgeFindingPass()) return false;
    SwitchToMirrorProblem();
    // Stop if no propagation.
    if (old_timestamp == integer_trail_->num_enqueues()) break;
  }
  return true;
}

void TimeTableEdgeFinding::SwitchToMirrorProblem() {
  // Swap variables.
  std::swap(start_vars_, mirror_start_vars_);
  std::swap(end_vars_, mirror_end_vars_);
  // Swap sorted vectors for incrementality.
  std::swap(by_start_min_, by_end_max_);
  std::swap(by_end_min_, by_start_max_);
}

void TimeTableEdgeFinding::BuildTimeTable() {
  scp_.clear();
  ecp_.clear();

  // Build start of compulsory part events.
  for (int i = 0; i < num_tasks_; ++i) {
    const int t = by_start_max_[i].task_id;
    if (!IsPresent(t)) continue;
    if (start_max_[t] < end_min_[t]) {
      scp_.push_back(by_start_max_[i]);
    }
  }

  // Build end of compulsory part events.
  for (int i = 0; i < num_tasks_; ++i) {
    const int t = by_end_min_[i].task_id;
    if (!IsPresent(t)) continue;
    if (start_max_[t] < end_min_[t]) {
      ecp_.push_back(by_end_min_[i]);
    }
  }

  DCHECK_EQ(scp_.size(), ecp_.size());

  IntegerValue height = IntegerValue(0);
  IntegerValue energy = IntegerValue(0);
  IntegerValue previous_time = by_start_min_[0].time;

  int index_scp = 0;   // index of the next value in scp
  int index_ecp = 0;   // index of the next value in ecp
  int index_smin = 0;  // index of the next value in by_start_min_
  int index_emax = 0;  // index of the next value in by_end_max_

  while (index_emax < num_tasks_) {
    // Next time point.
    // TODO(user): could be simplified with a sentinel.
    IntegerValue time = by_end_max_[index_emax].time;
    if (index_smin < num_tasks_) {
      time = std::min(time, by_start_min_[index_smin].time);
    }
    if (index_scp < scp_.size()) {
      time = std::min(time, scp_[index_scp].time);
    }
    if (index_ecp < ecp_.size()) {
      time = std::min(time, ecp_[index_ecp].time);
    }

    // Total amount of energy contained in the timetable until time.
    energy += (time - previous_time) * height;

    // Store the energy contained in the timetable just before those events.
    while (index_smin < num_tasks_ && by_start_min_[index_smin].time == time) {
      mandatory_energy_before_start_min_[by_start_min_[index_smin].task_id] =
          energy;
      index_smin++;
    }

    // Store the energy contained in the timetable just before those events.
    while (index_emax < num_tasks_ && by_end_max_[index_emax].time == time) {
      mandatory_energy_before_end_max_[by_end_max_[index_emax].task_id] =
          energy;
      index_emax++;
    }

    // Process the starting compulsory parts.
    while (index_scp < scp_.size() && scp_[index_scp].time == time) {
      height += demand_min_[scp_[index_scp].task_id];
      index_scp++;
    }

    // Process the ending compulsory parts.
    while (index_ecp < ecp_.size() && ecp_[index_ecp].time == time) {
      height -= demand_min_[ecp_[index_ecp].task_id];
      index_ecp++;
    }

    previous_time = time;
  }
}

bool TimeTableEdgeFinding::TimeTableEdgeFindingPass() {
  // Initialize the data structures and build the free parts.
  // --------------------------------------------------------
  for (int t = 0; t < num_tasks_; ++t) {
    // Cache the variable bounds. Note that those values are not updated by the
    // propagation loop.
    start_min_[t] = StartMin(t);
    start_max_[t] = StartMax(t);
    end_min_[t] = EndMin(t);
    end_max_[t] = EndMax(t);
    demand_min_[t] = DemandMin(t);
    // TODO(user): improve filtering for variable durations.
    duration_min_[t] = DurationMin(t);

    // If the task has no mandatory part, then its free part is the task itself.
    if (start_max_[t] >= end_min_[t]) {
      energy_free_[t] = demand_min_[t] * duration_min_[t];
    } else {
      energy_free_[t] =
          demand_min_[t] * (duration_min_[t] - end_min_[t] + start_max_[t]);
    }
  }

  // Update the sorted vectors.
  // --------------------------
  for (int i = 0; i < num_tasks_; ++i) {
    by_start_min_[i].time = start_min_[by_start_min_[i].task_id];
    by_start_max_[i].time = start_max_[by_start_max_[i].task_id];
    by_end_min_[i].time = end_min_[by_end_min_[i].task_id];
    by_end_max_[i].time = end_max_[by_end_max_[i].task_id];
  }
  // Likely to be already or almost sorted.
  IncrementalSort(by_start_min_.begin(), by_start_min_.end());
  IncrementalSort(by_start_max_.begin(), by_start_max_.end());
  IncrementalSort(by_end_min_.begin(), by_end_min_.end());
  IncrementalSort(by_end_max_.begin(), by_end_max_.end());

  BuildTimeTable();

  // Apply the Timetabling Edge Finding filtering rule.
  // --------------------------------------------------
  for (int end_task = 0; end_task < num_tasks_; ++end_task) {
    // TODO(user): consider optional tasks for additional propagation.
    if (!IsPresent(end_task) || energy_free_[end_task] == 0) continue;

    // Energy of the free parts contained in the interval [begin, end).
    IntegerValue energy_free_parts = IntegerValue(0);

    // Task that requires the biggest additional amount of energy to be
    // scheduled at its minimum start time in the task interval [begin, end).
    int max_task = -1;
    IntegerValue extra_energy_required_by_max_task = kMinIntegerValue;

    for (int j = num_tasks_ - 1; j >= 0; --j) {
      const int begin_task = by_start_min_[j].task_id;
      // TODO(user): consider optional tasks for additional propagation.
      if (!IsPresent(begin_task) || energy_free_[begin_task] == 0) continue;

      // The considered task interval.
      const IntegerValue end = end_max_[end_task];
      const IntegerValue begin = start_min_[begin_task];

      // Not a valid task interval.
      if (end <= begin) continue;

      if (end_max_[begin_task] <= end) {
        // The whole task energy is contained in the task interval.
        energy_free_parts += energy_free_[begin_task];
      } else {
        // We consider two different cases: either the free part overlaps the
        // end of the interval (right) or it does not (inside).
        //
        //                 begin  end
        //                   v     v
        // right:            ======|===
        //
        //          begin         end
        //            v            v
        // inside:    ==========   |
        //
        // In the inside case, the additional amount of energy required to
        // schedule the task at its minimum start time is equal to the whole
        // energy of the free part. In the right case, the additional energy is
        // equal to the largest part of the free part that can fit in the task
        // interval.
        const IntegerValue extra_energy = std::min(
            energy_free_[begin_task], demand_min_[begin_task] * (end - begin));

        if (extra_energy > extra_energy_required_by_max_task) {
          max_task = begin_task;
          extra_energy_required_by_max_task = extra_energy;
        }
      }

      // No task to push. This happens if all the tasks that overlap the task
      // interval are entirely contained in it.
      // TODO(user): check that we should not fail if the interval is
      // overloaded, i.e., available_energy < 0.
      if (max_task == -1) continue;

      // Compute the amount of energy available to schedule max_task.
      const IntegerValue interval_energy = CapacityMax() * (end - begin);
      const IntegerValue energy_mandatory =
          mandatory_energy_before_end_max_[end_task] -
          mandatory_energy_before_start_min_[begin_task];
      const IntegerValue available_energy =
          interval_energy - energy_free_parts - energy_mandatory;

      // Enough energy to schedule max_task at its minimum start time.
      if (extra_energy_required_by_max_task <= available_energy) continue;

      // Compute the length of the mandatory subpart of max_task that should be
      // considered as available.
      const IntegerValue mandatory_in =
          std::max(IntegerValue(0),
                   std::min(end, end_min_[max_task]) -
                       std::max(begin, start_max_[max_task]));

      // Compute the new minimum start time of max_task.
      const IntegerValue new_start =
          end - mandatory_in - (available_energy / demand_min_[max_task]);

      // Push and explain only if the new start is bigger than the current one.
      if (StartMin(max_task) < new_start) {
        if (!IncreaseStartMin(begin, end, max_task, new_start)) return false;
      }
    }
  }

  return true;
}

void TimeTableEdgeFinding::AddPresenceReasonIfNeeded(int task_id) {
  if (intervals_repository_->IsOptional(interval_vars_[task_id])) {
    literal_reason_.push_back(
        intervals_repository_->IsPresentLiteral(interval_vars_[task_id])
            .Negated());
  }
}

// TODO(user): generalize the explanation.
bool TimeTableEdgeFinding::IncreaseStartMin(IntegerValue begin,
                                            IntegerValue end, int task_id,
                                            IntegerValue new_start) {
  reason_.clear();
  literal_reason_.clear();

  // Capacity of the resource.
  reason_.push_back(integer_trail_->UpperBoundAsLiteral(capacity_var_));

  // Variables of the task to be pushed.
  reason_.push_back(integer_trail_->LowerBoundAsLiteral(demand_vars_[task_id]));
  reason_.push_back(integer_trail_->LowerBoundAsLiteral(start_vars_[task_id]));
  reason_.push_back(integer_trail_->UpperBoundAsLiteral(end_vars_[task_id]));
  if (duration_vars_[task_id] != kNoIntegerVariable) {
    reason_.push_back(
        integer_trail_->LowerBoundAsLiteral(duration_vars_[task_id]));
  }

  // Task contributing to the energy in the interval.
  for (int t = 0; t < num_tasks_; ++t) {
    if (begin < end_max_[t] && start_min_[t] < end && IsPresent(t)) {
      reason_.push_back(integer_trail_->LowerBoundAsLiteral(demand_vars_[t]));
      reason_.push_back(integer_trail_->LowerBoundAsLiteral(start_vars_[t]));
      reason_.push_back(integer_trail_->UpperBoundAsLiteral(end_vars_[t]));
      if (duration_vars_[t] != kNoIntegerVariable) {
        reason_.push_back(
            integer_trail_->LowerBoundAsLiteral(duration_vars_[t]));
      }
      AddPresenceReasonIfNeeded(t);
    }
  }

  // Explain the increase of the start min.
  if (!integer_trail_->Enqueue(
          IntegerLiteral::GreaterOrEqual(start_vars_[task_id], new_start),
          literal_reason_, reason_)) {
    return false;
  }

  // Check that we need to push the end min.
  const IntegerValue new_end = new_start + duration_min_[task_id];
  if (new_end <= end_min_[task_id]) return true;

  // Build the reason to increase the end min.
  reason_.clear();
  reason_.push_back(integer_trail_->LowerBoundAsLiteral(start_vars_[task_id]));
  // Only use the duration variable if it is defined.
  if (duration_vars_[task_id] != kNoIntegerVariable) {
    reason_.push_back(
        integer_trail_->LowerBoundAsLiteral(duration_vars_[task_id]));
  }

  // Explain the increase of the end min.
  if (!integer_trail_->Enqueue(
          IntegerLiteral::GreaterOrEqual(end_vars_[task_id], new_end), {},
          reason_)) {
    return false;
  }

  return true;
}

}  // namespace sat
}  // namespace operations_research
