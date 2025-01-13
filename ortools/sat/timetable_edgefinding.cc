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

#include "ortools/sat/timetable_edgefinding.h"

#include <algorithm>
#include <limits>
#include <vector>

#include "absl/log/check.h"
#include "ortools/base/iterator_adaptors.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/model.h"
#include "ortools/sat/scheduling_helpers.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {

TimeTableEdgeFinding::TimeTableEdgeFinding(AffineExpression capacity,
                                           SchedulingConstraintHelper* helper,
                                           SchedulingDemandHelper* demands,
                                           Model* model)
    : num_tasks_(helper->NumTasks()),
      capacity_(capacity),
      helper_(helper),
      demands_(demands),
      integer_trail_(model->GetOrCreate<IntegerTrail>()) {
  // Edge finding structures.
  mandatory_energy_before_end_max_.resize(num_tasks_);
  mandatory_energy_before_start_min_.resize(num_tasks_);

  // Energy of free parts.
  size_free_.resize(num_tasks_);
  energy_free_.resize(num_tasks_);
}

void TimeTableEdgeFinding::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  watcher->WatchUpperBound(capacity_, id);
  helper_->WatchAllTasks(id, watcher);
  for (int t = 0; t < num_tasks_; t++) {
    watcher->WatchLowerBound(demands_->Demands()[t], id);
  }
  watcher->SetPropagatorPriority(id, 3);
  watcher->NotifyThatPropagatorMayNotReachFixedPointInOnePass(id);
}

bool TimeTableEdgeFinding::Propagate() {
  if (!helper_->SynchronizeAndSetTimeDirection(true)) return false;
  if (!TimeTableEdgeFindingPass()) return false;
  if (!helper_->SynchronizeAndSetTimeDirection(false)) return false;
  if (!TimeTableEdgeFindingPass()) return false;
  return true;
}

void TimeTableEdgeFinding::BuildTimeTable() {
  scp_.clear();
  ecp_.clear();

  // Build start of compulsory part events.
  const auto by_negated_smax = helper_->TaskByIncreasingNegatedStartMax();
  for (const auto [t, negated_smax] : ::gtl::reversed_view(by_negated_smax)) {
    if (!helper_->IsPresent(t)) continue;
    const IntegerValue start_max = -negated_smax;
    if (start_max < helper_->EndMin(t)) {
      scp_.push_back({t, start_max});
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

  const auto by_decreasing_end_max = helper_->TaskByDecreasingEndMax();
  const auto by_start_min = helper_->TaskByIncreasingStartMin();

  IntegerValue height = IntegerValue(0);
  IntegerValue energy = IntegerValue(0);

  // We don't care since at the beginning height is zero, and previous_time will
  // be correct after the first iteration.
  IntegerValue previous_time = IntegerValue(0);

  int index_scp = 0;                // index of the next value in scp
  int index_ecp = 0;                // index of the next value in ecp
  int index_smin = 0;               // index of the next value in by_start_min_
  int index_emax = num_tasks_ - 1;  // index of the next value in by_end_max_

  while (index_emax >= 0) {
    // Next time point.
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
      height += demands_->DemandMin(scp_[index_scp].task_index);
      index_scp++;
    }

    // Process the ending compulsory parts.
    while (index_ecp < ecp_.size() && ecp_[index_ecp].time == time) {
      height -= demands_->DemandMin(ecp_[index_ecp].task_index);
      index_ecp++;
    }
  }
}

bool TimeTableEdgeFinding::TimeTableEdgeFindingPass() {
  if (!demands_->CacheAllEnergyValues()) return true;

  IntegerValue earliest_start_min = std::numeric_limits<IntegerValue>::max();
  IntegerValue latest_end_max = std::numeric_limits<IntegerValue>::min();
  IntegerValue maximum_demand_min = IntegerValue(0);
  // Initialize the data structures and build the free parts.
  // --------------------------------------------------------
  for (int t = 0; t < num_tasks_; ++t) {
    // If the task has no mandatory part, then its free part is the task itself.
    const IntegerValue start_max = helper_->StartMax(t);
    const IntegerValue end_min = helper_->EndMin(t);
    const IntegerValue demand_min = demands_->DemandMin(t);
    IntegerValue mandatory_energy(0);

    earliest_start_min = std::min(earliest_start_min, helper_->StartMin(t));
    latest_end_max = std::max(latest_end_max, helper_->EndMax(t));
    maximum_demand_min = std::max(maximum_demand_min, demand_min);

    if (start_max >= end_min) {
      size_free_[t] = helper_->SizeMin(t);
    } else {
      const IntegerValue mandatory_size = end_min - start_max;
      size_free_[t] = helper_->SizeMin(t) - mandatory_size;
      mandatory_energy = mandatory_size * demand_min;
    }

    const IntegerValue min_energy = demands_->EnergyMin(t);
    energy_free_[t] = min_energy - mandatory_energy;
    DCHECK_GE(energy_free_[t], 0);
  }

  if (AtMinOrMaxInt64I(CapProdI(CapSubI(latest_end_max, earliest_start_min),
                                maximum_demand_min))) {
    // Avoid possible overflow.
    return true;
  }

  // TODO(user): Is it possible to have a 'higher' mandatory profile using
  // the min energy instead of the demand_min * size_min? How can we incorporate
  // this extra energy in the mandatory profile ?
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

    // Energy of the free parts contained in the interval
    // [window_min, window_max].
    IntegerValue energy_free_parts = IntegerValue(0);
    reason_tasks_fully_included_in_window_.clear();
    reason_tasks_partially_included_in_window_.clear();

    // Task that requires the biggest additional amount of energy to be
    // scheduled at its minimum start time in the task interval
    // [window_min, window_max].
    int max_task = -1;
    IntegerValue free_energy_of_max_task_in_window(0);
    IntegerValue extra_energy_required_by_max_task = kMinIntegerValue;

    // Process task by decreasing start min.
    const IntegerValue window_max = end_task_time.time;
    for (const TaskTime begin_task_time : gtl::reversed_view(by_start_min)) {
      const int begin_task = begin_task_time.task_index;

      // The considered time window. Note that we use the "cached" values so
      // that our mandatory energy before computation is correct.
      const IntegerValue window_min = begin_task_time.time;

      // Not a valid time window.
      if (window_max <= window_min) continue;

      // TODO(user): consider optional tasks for additional propagation.
      if (!helper_->IsPresent(begin_task)) continue;
      if (energy_free_[begin_task] == 0) continue;

      // We consider two different cases: either the free part overlaps the
      // window_max of the interval (right) or it does not (inside).
      //
      //            window_min  window_max
      //                   v     v
      // right:            ======|===
      //
      //      window_min     window_max
      //            v            v
      // inside:    ==========   |
      //
      // In the inside case, the additional amount of energy required to
      // schedule the task at its minimum start time is equal to the whole
      // energy of the free part. In the right case, the additional energy is
      // equal to the largest part of the free part that can fit in the task
      // interval.
      const IntegerValue end_max = helper_->EndMax(begin_task);
      if (end_max <= window_max) {
        // The whole task energy is contained in the window.
        reason_tasks_fully_included_in_window_.push_back(begin_task);
        energy_free_parts += energy_free_[begin_task];
      } else {
        const IntegerValue demand_min = demands_->DemandMin(begin_task);
        const IntegerValue extra_energy =
            std::min(size_free_[begin_task], (window_max - window_min)) *
            demand_min;

        // This is not in the paper, but it is almost free for us to account for
        // the free energy of this task that must be present in the window.
        const IntegerValue free_energy_in_window =
            std::max(IntegerValue(0),
                     size_free_[begin_task] - (end_max - window_max)) *
            demand_min;

        // TODO(user): There is no point setting max_task if its start min
        // is already bigger that what we can push. Maybe we can exploit that?
        if (extra_energy > extra_energy_required_by_max_task) {
          if (max_task != -1 && free_energy_of_max_task_in_window > 0) {
            reason_tasks_partially_included_in_window_.push_back(max_task);
          }

          max_task = begin_task;
          extra_energy_required_by_max_task = extra_energy;

          // Account for the free energy of the old max task, and cache the
          // new one for later.
          energy_free_parts += free_energy_of_max_task_in_window;
          free_energy_of_max_task_in_window = free_energy_in_window;
        } else if (free_energy_in_window > 0) {
          reason_tasks_partially_included_in_window_.push_back(begin_task);
          energy_free_parts += free_energy_in_window;
        }
      }

      // No task to push. This happens if all the tasks that overlap the task
      // interval are entirely contained in it.
      // TODO(user): check that we should not fail if the interval is
      // overloaded, i.e., available_energy < 0.
      //
      // We also defensively abort if the demand_min is 0.
      // This may happen along a energy_min > 0 if the literals in the
      // decomposed_energy have been fixed, and not yet propagated to the demand
      // affine expression.
      if (max_task == -1 || demands_->DemandMin(max_task) == 0) continue;

      // Compute the amount of energy available to schedule max_task.
      const IntegerValue window_energy =
          CapacityMax() * (window_max - window_min);
      const IntegerValue energy_mandatory =
          mandatory_energy_before_end_max_[end_task] -
          mandatory_energy_before_start_min_[begin_task];
      const IntegerValue available_energy =
          window_energy - energy_free_parts - energy_mandatory;

      // Enough energy to schedule max_task at its minimum start time?
      //
      // TODO(user): In case of alternatives, for each fixed
      // size/demand pair, we can compute a new_start and use the min of them.
      if (extra_energy_required_by_max_task <= available_energy) {
        // If the test below is true, we know the max_task cannot fully
        // fit in the time window, so at least end_min > window_max.
        //
        // TODO(user): We currently only do that if we are not about to push the
        // start as we assume the start push is just stronger. Maybe we should
        // do it in more situation?
        if (energy_free_[max_task] > available_energy &&
            helper_->EndMin(max_task) <= window_max) {
          FillEnergyInWindowReason(window_min, window_max, max_task);
          demands_->AddEnergyMinReason(max_task);
          helper_->AddStartMinReason(max_task, window_min);
          if (!helper_->IncreaseEndMin(max_task, window_max + 1)) return false;
        }
        continue;
      }

      // Compute the length of the mandatory subpart of max_task that should be
      // considered as available.
      //
      // TODO(user): Because this use updated bounds, it might be more than what
      // we accounted for in the precomputation. This is correct but could be
      // improved upon.
      const IntegerValue mandatory_size_in_window =
          std::max(IntegerValue(0),
                   std::min(window_max, helper_->EndMin(max_task)) -
                       std::max(window_min, helper_->StartMax(max_task)));

      // Compute the new minimum start time of max_task.
      const IntegerValue max_free_size_that_fit =
          available_energy / demands_->DemandMin(max_task);
      const IntegerValue new_start =
          window_max - mandatory_size_in_window - max_free_size_that_fit;

      // Push and explain only if the new start is bigger than the current one.
      if (helper_->StartMin(max_task) < new_start) {
        FillEnergyInWindowReason(window_min, window_max, max_task);

        // Reason needed for task_index.
        // We only need start_min and demand_min to push the start.
        helper_->AddStartMinReason(max_task, window_min);
        demands_->AddDemandMinReason(max_task);

        if (!helper_->IncreaseStartMin(max_task, new_start)) return false;
      }
    }
  }

  return true;
}

void TimeTableEdgeFinding::FillEnergyInWindowReason(IntegerValue window_min,
                                                    IntegerValue window_max,
                                                    int task_index) {
  helper_->ClearReason();

  // Capacity of the resource.
  if (capacity_.var != kNoIntegerVariable) {
    helper_->MutableIntegerReason()->push_back(
        integer_trail_->UpperBoundAsLiteral(capacity_.var));
  }

  // Tasks contributing to the mandatory energy in the interval.
  for (int t = 0; t < num_tasks_; ++t) {
    if (t == task_index) continue;
    if (!helper_->IsPresent(t)) continue;
    const IntegerValue smax = helper_->StartMax(t);
    const IntegerValue emin = helper_->EndMin(t);
    if (smax >= emin) continue;
    if (emin <= window_min) continue;
    if (smax >= window_max) continue;
    helper_->AddStartMaxReason(t, std::max(smax, window_min));
    helper_->AddEndMinReason(t, std::min(emin, window_max));
    helper_->AddPresenceReason(t);
    demands_->AddDemandMinReason(t);
  }

  // Tasks contributing to the free energy in [window_min, window_max].
  //
  // TODO(user): If a task appears in both, we could avoid adding twice the
  // same things, but the core solver should merge duplicates anyway.
  for (const int t : reason_tasks_fully_included_in_window_) {
    DCHECK_NE(t, task_index);
    DCHECK(helper_->IsPresent(t));
    DCHECK_GT(helper_->EndMax(t), window_min);
    DCHECK_LT(helper_->StartMin(t), window_max);
    DCHECK_GE(helper_->StartMin(t), window_min);

    helper_->AddStartMinReason(t, helper_->StartMin(t));
    helper_->AddEndMaxReason(t, std::max(window_max, helper_->EndMax(t)));
    helper_->AddPresenceReason(t);
    demands_->AddEnergyMinReason(t);
  }
  for (const int t : reason_tasks_partially_included_in_window_) {
    DCHECK_NE(t, task_index);
    DCHECK(helper_->IsPresent(t));
    DCHECK_GT(helper_->EndMax(t), window_min);
    DCHECK_LT(helper_->StartMin(t), window_max);
    DCHECK_GE(helper_->StartMin(t), window_min);

    helper_->AddStartMinReason(t, helper_->StartMin(t));
    helper_->AddEndMaxReason(t, std::max(window_max, helper_->EndMax(t)));
    helper_->AddPresenceReason(t);

    helper_->AddSizeMinReason(t);
    demands_->AddDemandMinReason(t);
  }
}

}  // namespace sat
}  // namespace operations_research
