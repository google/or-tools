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

#include "ortools/sat/timetable_edgefinding.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include "ortools/base/int_type.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/iterator_adaptors.h"
#include "ortools/base/logging.h"
#include "ortools/util/sort.h"

namespace operations_research {
namespace sat {

TimeTableEdgeFinding::TimeTableEdgeFinding(
    const std::vector<AffineExpression>& demands, AffineExpression capacity,
    SchedulingConstraintHelper* helper, IntegerTrail* integer_trail)
    : num_tasks_(helper->NumTasks()),
      demands_(demands),
      capacity_(capacity),
      helper_(helper),
      integer_trail_(integer_trail) {
  // Edge finding structures.
  mandatory_energy_before_end_max_.resize(num_tasks_);
  mandatory_energy_before_start_min_.resize(num_tasks_);

  // Energy of free parts.
  size_free_.resize(num_tasks_);
  energy_free_.resize(num_tasks_);
}

void TimeTableEdgeFinding::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  watcher->WatchUpperBound(capacity_.var, id);
  helper_->WatchAllTasks(id, watcher);
  for (int t = 0; t < num_tasks_; t++) {
    watcher->WatchLowerBound(demands_[t].var, id);
  }
}

bool TimeTableEdgeFinding::Propagate() {
  while (true) {
    const int64_t old_timestamp = integer_trail_->num_enqueues();

    if (!helper_->SynchronizeAndSetTimeDirection(true)) return false;
    if (!TimeTableEdgeFindingPass()) return false;

    if (!helper_->SynchronizeAndSetTimeDirection(false)) return false;
    if (!TimeTableEdgeFindingPass()) return false;

    // Stop if no propagation.
    if (old_timestamp == integer_trail_->num_enqueues()) break;
  }
  return true;
}

void TimeTableEdgeFinding::BuildTimeTable() {
  scp_.clear();
  ecp_.clear();

  // Build start of compulsory part events.
  for (const auto task_time :
       ::gtl::reversed_view(helper_->TaskByDecreasingStartMax())) {
    const int t = task_time.task_index;
    if (!helper_->IsPresent(t)) continue;
    if (task_time.time < helper_->EndMin(t)) {
      scp_.push_back(task_time);
    }
  }

  // Build end of compulsory part events.
  for (const auto task_time : helper_->TaskByIncreasingEndMin()) {
    const int t = task_time.task_index;
    if (!helper_->IsPresent(t)) continue;
    if (helper_->StartMax(t) < task_time.time) {
      ecp_.push_back(task_time);
    }
  }

  DCHECK_EQ(scp_.size(), ecp_.size());

  const std::vector<TaskTime>& by_decreasing_end_max =
      helper_->TaskByDecreasingEndMax();
  const std::vector<TaskTime>& by_start_min =
      helper_->TaskByIncreasingStartMin();

  IntegerValue height = IntegerValue(0);
  IntegerValue energy = IntegerValue(0);

  // We don't care since at the beginning heigh is zero, and previous_time will
  // be correct after the first iteration.
  IntegerValue previous_time = IntegerValue(0);

  int index_scp = 0;                // index of the next value in scp
  int index_ecp = 0;                // index of the next value in ecp
  int index_smin = 0;               // index of the next value in by_start_min_
  int index_emax = num_tasks_ - 1;  // index of the next value in by_end_max_

  while (index_emax >= 0) {
    // Next time point.
    // TODO(user): could be simplified with a sentinel.
    IntegerValue time = by_decreasing_end_max[index_emax].time;
    if (index_smin < num_tasks_) {
      time = std::min(time, by_start_min[index_smin].time);
    }
    if (index_scp < scp_.size()) {
      time = std::min(time, scp_[index_scp].time);
    }
    if (index_ecp < ecp_.size()) {
      time = std::min(time, ecp_[index_ecp].time);
    }

    // Total amount of energy contained in the timetable until time.
    energy += (time - previous_time) * height;
    previous_time = time;

    // Store the energy contained in the timetable just before those events.
    while (index_smin < num_tasks_ && by_start_min[index_smin].time == time) {
      mandatory_energy_before_start_min_[by_start_min[index_smin].task_index] =
          energy;
      index_smin++;
    }

    // Store the energy contained in the timetable just before those events.
    while (index_emax >= 0 && by_decreasing_end_max[index_emax].time == time) {
      mandatory_energy_before_end_max_[by_decreasing_end_max[index_emax]
                                           .task_index] = energy;
      index_emax--;
    }

    // Process the starting compulsory parts.
    while (index_scp < scp_.size() && scp_[index_scp].time == time) {
      height += DemandMin(scp_[index_scp].task_index);
      index_scp++;
    }

    // Process the ending compulsory parts.
    while (index_ecp < ecp_.size() && ecp_[index_ecp].time == time) {
      height -= DemandMin(ecp_[index_ecp].task_index);
      index_ecp++;
    }
  }
}

bool TimeTableEdgeFinding::TimeTableEdgeFindingPass() {
  // Initialize the data structures and build the free parts.
  // --------------------------------------------------------
  for (int t = 0; t < num_tasks_; ++t) {
    // If the task has no mandatory part, then its free part is the task itself.
    const IntegerValue start_max = helper_->StartMax(t);
    const IntegerValue end_min = helper_->EndMin(t);
    if (start_max >= end_min) {
      size_free_[t] = helper_->SizeMin(t);
    } else {
      size_free_[t] = helper_->SizeMin(t) + start_max - end_min;
    }
    energy_free_[t] = size_free_[t] * DemandMin(t);
  }

  BuildTimeTable();
  const auto& by_start_min = helper_->TaskByIncreasingStartMin();

  IntegerValue previous_end = kMaxIntegerValue;

  // Apply the Timetabling Edge Finding filtering rule.
  // --------------------------------------------------
  // The loop order is not important for correctness.
  for (const TaskTime end_task_time : helper_->TaskByDecreasingEndMax()) {
    const int end_task = end_task_time.task_index;

    // TODO(user): consider optional tasks for additional propagation.
    if (!helper_->IsPresent(end_task)) continue;
    if (energy_free_[end_task] == 0) continue;

    // We only need to consider each time point once.
    if (end_task_time.time == previous_end) continue;
    previous_end = end_task_time.time;

    // Energy of the free parts contained in the interval [begin, end).
    IntegerValue energy_free_parts = IntegerValue(0);

    // Task that requires the biggest additional amount of energy to be
    // scheduled at its minimum start time in the task interval [begin, end).
    int max_task = -1;
    IntegerValue free_energy_of_max_task_in_window(0);
    IntegerValue extra_energy_required_by_max_task = kMinIntegerValue;

    // Process task by decreasing start min.
    for (const TaskTime begin_task_time : gtl::reversed_view(by_start_min)) {
      const int begin_task = begin_task_time.task_index;

      // TODO(user): consider optional tasks for additional propagation.
      if (!helper_->IsPresent(begin_task)) continue;
      if (energy_free_[begin_task] == 0) continue;

      // The considered time window. Note that we use the "cached" values so
      // that our mandatory energy before computation is correct.
      const IntegerValue begin = begin_task_time.time;  // Start min.
      const IntegerValue end = end_task_time.time;      // End max.

      // Not a valid time window.
      if (end <= begin) continue;

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
      const IntegerValue end_max = helper_->EndMax(begin_task);
      if (end_max <= end) {
        // The whole task energy is contained in the task interval.
        energy_free_parts += energy_free_[begin_task];
      } else {
        const IntegerValue demand_min = DemandMin(begin_task);
        const IntegerValue extra_energy =
            std::min(size_free_[begin_task], (end - begin)) * demand_min;

        // This is not in the paper, but it is almost free for us to account for
        // the free energy of this task that must be present in the window.
        const IntegerValue free_energy_in_window =
            std::max(IntegerValue(0),
                     size_free_[begin_task] - (end_max - end)) *
            demand_min;

        if (extra_energy > extra_energy_required_by_max_task) {
          max_task = begin_task;
          extra_energy_required_by_max_task = extra_energy;

          // Account for the free energy of the old max task, and cache the
          // new one for later.
          energy_free_parts += free_energy_of_max_task_in_window;
          free_energy_of_max_task_in_window = free_energy_in_window;
        } else {
          energy_free_parts += free_energy_in_window;
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
      //
      // TODO(user): Because this use updated bounds, it might be more than what
      // we accounted for in the precomputation. This is correct but could be
      // improved uppon.
      const IntegerValue mandatory_in = std::max(
          IntegerValue(0), std::min(end, helper_->EndMin(max_task)) -
                               std::max(begin, helper_->StartMax(max_task)));

      // Compute the new minimum start time of max_task.
      const IntegerValue new_start =
          end - mandatory_in - (available_energy / DemandMin(max_task));

      // Push and explain only if the new start is bigger than the current one.
      if (helper_->StartMin(max_task) < new_start) {
        if (!IncreaseStartMin(begin, end, max_task, new_start)) return false;
      }
    }
  }

  return true;
}

bool TimeTableEdgeFinding::IncreaseStartMin(IntegerValue begin,
                                            IntegerValue end, int task_index,
                                            IntegerValue new_start) {
  helper_->ClearReason();
  std::vector<IntegerLiteral>* mutable_reason = helper_->MutableIntegerReason();

  // Capacity of the resource.
  if (capacity_.var != kNoIntegerVariable) {
    mutable_reason->push_back(
        integer_trail_->UpperBoundAsLiteral(capacity_.var));
  }

  // Variables of the task to be pushed. We do not need the end max for this
  // task and we only need for it to begin in the time window.
  if (demands_[task_index].var != kNoIntegerVariable) {
    mutable_reason->push_back(
        integer_trail_->LowerBoundAsLiteral(demands_[task_index].var));
  }
  helper_->AddStartMinReason(task_index, begin);
  helper_->AddSizeMinReason(task_index);

  // Task contributing to the energy in the interval.
  for (int t = 0; t < num_tasks_; ++t) {
    if (t == task_index) continue;
    if (!helper_->IsPresent(t)) continue;
    if (helper_->EndMax(t) <= begin) continue;
    if (helper_->StartMin(t) >= end) continue;

    if (demands_[t].var != kNoIntegerVariable) {
      mutable_reason->push_back(
          integer_trail_->LowerBoundAsLiteral(demands_[t].var));
    }

    // We need the reason for the energy contribution of this interval into
    // [begin, end].
    //
    // TODO(user): Since we actually do not account fully for this energy, we
    // could relax the reason more.
    //
    // TODO(user): This reason might not be enough in the presence of variable
    // size intervals where StartMax and EndMin give rise to more energy
    // that just using size min and these bounds. Fix.
    helper_->AddStartMinReason(t, std::min(begin, helper_->StartMin(t)));
    helper_->AddEndMaxReason(t, std::max(end, helper_->EndMax(t)));
    helper_->AddSizeMinReason(t);
    helper_->AddPresenceReason(t);
  }

  return helper_->IncreaseStartMin(task_index, new_start);
}

}  // namespace sat
}  // namespace operations_research
