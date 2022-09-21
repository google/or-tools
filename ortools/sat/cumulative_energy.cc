// Copyright 2010-2022 Google LLC
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

#include "ortools/sat/cumulative_energy.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "ortools/base/iterator_adaptors.h"
#include "ortools/base/logging.h"
#include "ortools/sat/implied_bounds.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/model.h"
#include "ortools/sat/theta_tree.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {

void AddCumulativeOverloadChecker(AffineExpression capacity,
                                  SchedulingConstraintHelper* helper,
                                  SchedulingDemandHelper* demands,
                                  Model* model) {
  auto* watcher = model->GetOrCreate<GenericLiteralWatcher>();
  CumulativeEnergyConstraint* constraint =
      new CumulativeEnergyConstraint(capacity, helper, demands, model);
  constraint->RegisterWith(watcher);
  model->TakeOwnership(constraint);
}

CumulativeEnergyConstraint::CumulativeEnergyConstraint(
    AffineExpression capacity, SchedulingConstraintHelper* helper,
    SchedulingDemandHelper* demands, Model* model)
    : capacity_(capacity),
      integer_trail_(model->GetOrCreate<IntegerTrail>()),
      helper_(helper),
      demands_(demands) {
  const int num_tasks = helper_->NumTasks();
  task_to_start_event_.resize(num_tasks);
}

void CumulativeEnergyConstraint::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  helper_->WatchAllTasks(id, watcher);
  watcher->SetPropagatorPriority(id, 2);
  watcher->NotifyThatPropagatorMayNotReachFixedPointInOnePass(id);
}

bool CumulativeEnergyConstraint::Propagate() {
  // This only uses one time direction, but the helper might be used elsewhere.
  // TODO(user): just keep the current direction?
  if (!helper_->SynchronizeAndSetTimeDirection(true)) return false;
  demands_->CacheAllEnergyValues();

  const IntegerValue capacity_max = integer_trail_->UpperBound(capacity_);
  // TODO(user): force capacity_max >= 0, fail/remove optionals when 0.
  if (capacity_max <= 0) return true;

  // Set up theta tree.
  start_event_task_time_.clear();
  int num_events = 0;
  for (const auto task_time : helper_->TaskByIncreasingStartMin()) {
    const int task = task_time.task_index;
    if (helper_->IsAbsent(task) || demands_->EnergyMax(task) == 0) {
      task_to_start_event_[task] = -1;
      continue;
    }
    start_event_task_time_.emplace_back(task_time);
    task_to_start_event_[task] = num_events;
    num_events++;
  }
  start_event_is_present_.assign(num_events, false);
  theta_tree_.Reset(num_events);

  bool tree_has_mandatory_intervals = false;

  // Main loop: insert tasks by increasing end_max, check for overloads.
  for (const auto task_time :
       ::gtl::reversed_view(helper_->TaskByDecreasingEndMax())) {
    const int current_task = task_time.task_index;
    const IntegerValue current_end = task_time.time;
    if (task_to_start_event_[current_task] == -1) continue;

    // Add the current task to the tree.
    {
      const int current_event = task_to_start_event_[current_task];
      const IntegerValue start_min = start_event_task_time_[current_event].time;
      const bool is_present = helper_->IsPresent(current_task);
      start_event_is_present_[current_event] = is_present;
      if (is_present) {
        tree_has_mandatory_intervals = true;
        theta_tree_.AddOrUpdateEvent(current_event, start_min * capacity_max,
                                     demands_->EnergyMin(current_task),
                                     demands_->EnergyMax(current_task));
      } else {
        theta_tree_.AddOrUpdateOptionalEvent(current_event,
                                             start_min * capacity_max,
                                             demands_->EnergyMax(current_task));
      }
    }

    if (tree_has_mandatory_intervals) {
      // Find the critical interval.
      const IntegerValue envelope = theta_tree_.GetEnvelope();
      const int critical_event =
          theta_tree_.GetMaxEventWithEnvelopeGreaterThan(envelope - 1);
      const IntegerValue window_start =
          start_event_task_time_[critical_event].time;
      const IntegerValue window_end = current_end;
      const IntegerValue window_size = window_end - window_start;
      if (window_size == 0) continue;
      const IntegerValue new_capacity_min =
          CeilRatio(envelope - window_start * capacity_max, window_size);

      // Push the new capacity min, note that this can fail if it go above the
      // maximum capacity.
      //
      // TODO(user): We do not need the capacity max in the reason, but by using
      // a lower one, we could maybe have propagated more the minimum capacity.
      // investigate.
      if (new_capacity_min > integer_trail_->LowerBound(capacity_)) {
        helper_->ClearReason();
        for (int event = critical_event; event < num_events; event++) {
          if (start_event_is_present_[event]) {
            const int task = start_event_task_time_[event].task_index;
            helper_->AddPresenceReason(task);
            demands_->AddEnergyMinReason(task);
            helper_->AddStartMinReason(task, window_start);
            helper_->AddEndMaxReason(task, window_end);
          }
        }
        if (capacity_.var == kNoIntegerVariable) {
          return helper_->ReportConflict();
        } else {
          if (!helper_->PushIntegerLiteral(
                  capacity_.GreaterOrEqual(new_capacity_min))) {
            return false;
          }
        }
      }
    }

    // Reduce energy of all tasks whose max energy would exceed an interval
    // ending at current_end.
    while (theta_tree_.GetOptionalEnvelope() > current_end * capacity_max) {
      // Some task's max energy is too high, reduce its maximal energy.
      // Explain with tasks present in the critical interval.
      // If it is optional, it might get excluded, in that case,
      // remove it from the tree.
      // TODO(user): This could be done lazily.
      // TODO(user): the same required task can have its energy pruned
      // several times, making this algorithm O(n^2 log n). Is there a way
      // to get the best pruning in one go? This looks like edge-finding not
      // being able to converge in one pass, so it might not be easy.
      helper_->ClearReason();
      int critical_event;
      int event_with_new_energy_max;
      IntegerValue new_energy_max;
      theta_tree_.GetEventsWithOptionalEnvelopeGreaterThan(
          current_end * capacity_max, &critical_event,
          &event_with_new_energy_max, &new_energy_max);

      const IntegerValue window_start =
          start_event_task_time_[critical_event].time;

      // TODO(user): Improve window_end using envelope of critical event.
      const IntegerValue window_end = current_end;
      for (int event = critical_event; event < num_events; event++) {
        if (start_event_is_present_[event]) {
          if (event == event_with_new_energy_max) continue;
          const int task = start_event_task_time_[event].task_index;
          helper_->AddPresenceReason(task);
          helper_->AddStartMinReason(task, window_start);
          helper_->AddEndMaxReason(task, window_end);
          demands_->AddEnergyMinReason(task);
        }
      }
      if (capacity_.var != kNoIntegerVariable) {
        helper_->MutableIntegerReason()->push_back(
            integer_trail_->UpperBoundAsLiteral(capacity_.var));
      }

      const int task_with_new_energy_max =
          start_event_task_time_[event_with_new_energy_max].task_index;
      helper_->AddStartMinReason(task_with_new_energy_max, window_start);
      helper_->AddEndMaxReason(task_with_new_energy_max, window_end);
      if (!demands_->DecreaseEnergyMax(task_with_new_energy_max,
                                       new_energy_max)) {
        return false;
      }

      if (helper_->IsPresent(task_with_new_energy_max)) {
        theta_tree_.AddOrUpdateEvent(
            task_to_start_event_[task_with_new_energy_max],
            start_event_task_time_[event_with_new_energy_max].time *
                capacity_max,
            demands_->EnergyMin(task_with_new_energy_max), new_energy_max);
      } else {
        theta_tree_.RemoveEvent(event_with_new_energy_max);
      }
    }
  }
  return true;
}

CumulativeIsAfterSubsetConstraint::CumulativeIsAfterSubsetConstraint(
    IntegerVariable var, AffineExpression capacity,
    const std::vector<int>& subtasks, const std::vector<IntegerValue>& offsets,
    SchedulingConstraintHelper* helper, SchedulingDemandHelper* demands,
    Model* model)
    : var_to_push_(var),
      capacity_(capacity),
      subtasks_(subtasks),
      integer_trail_(model->GetOrCreate<IntegerTrail>()),
      helper_(helper),
      demands_(demands) {
  is_in_subtasks_.assign(helper->NumTasks(), false);
  task_offsets_.assign(helper->NumTasks(), 0);
  for (int i = 0; i < subtasks.size(); ++i) {
    is_in_subtasks_[subtasks[i]] = true;
    task_offsets_[subtasks[i]] = offsets[i];
  }
}

bool CumulativeIsAfterSubsetConstraint::Propagate() {
  if (!helper_->SynchronizeAndSetTimeDirection(true)) return false;

  IntegerValue best_time = kMaxIntegerValue;
  IntegerValue best_bound = kMinIntegerValue;

  IntegerValue previous_time = kMaxIntegerValue;
  IntegerValue energy_after_time(0);
  IntegerValue profile_height(0);

  // If the capacity_max is low enough, we compute the exact possible subset
  // of reachable "sum of demands" of all tasks used in the energy. We will use
  // the highest reachable as the capacity max.
  const IntegerValue capacity_max = integer_trail_->UpperBound(capacity_);
  dp_.Reset(capacity_max.value());

  // We consider the energy after a given time.
  // From that we derive a bound on the end_min of the subtasks.
  const auto& profile = helper_->GetEnergyProfile();
  IntegerValue min_offset = kMaxIntegerValue;
  for (int i = profile.size() - 1; i >= 0;) {
    // Skip tasks not relevant for this propagator.
    {
      const int t = profile[i].task;
      if (!helper_->IsPresent(t) || !is_in_subtasks_[t]) {
        --i;
        continue;
      }
    }

    const IntegerValue time = profile[i].time;
    if (profile_height > 0) {
      energy_after_time += profile_height * (previous_time - time);
    }
    previous_time = time;

    // Any newly introduced tasks will only change the reachable capa max or
    // the min_offset on the next time point.
    const IntegerValue saved_capa_max = dp_.CurrentMax();
    const IntegerValue saved_min_offset = min_offset;

    for (; i >= 0 && profile[i].time == time; --i) {
      // Skip tasks not relevant for this propagator.
      const int t = profile[i].task;
      if (!helper_->IsPresent(t) || !is_in_subtasks_[t]) continue;

      min_offset = std::min(min_offset, task_offsets_[t]);
      const IntegerValue demand_min = demands_->DemandMin(t);
      if (profile[i].is_first) {
        profile_height -= demand_min;
      } else {
        profile_height += demand_min;
        if (demands_->Demands()[t].IsConstant()) {
          dp_.Add(demand_min.value());
        } else {
          dp_.Add(capacity_max.value());  // Abort DP.
        }
      }
    }

    // We prefer higher time in case of ties since that should reduce the
    // explanation size.
    //
    // Note that if the energy is zero, we don't push anything. Other propagator
    // will make sure that the end_min is greater than the end_min of any of
    // the task considered here. TODO(user): actually, we will push using the
    // last task, and the reason will be non-optimal, fix.
    if (energy_after_time == 0) continue;
    DCHECK_GT(saved_capa_max, 0);
    DCHECK_LT(saved_min_offset, kMaxIntegerValue);
    const IntegerValue end_min_with_offset =
        time + CeilRatio(energy_after_time, saved_capa_max) + saved_min_offset;
    if (end_min_with_offset > best_bound) {
      best_time = time;
      best_bound = end_min_with_offset;
    }
  }
  DCHECK_EQ(profile_height, 0);

  if (best_bound == kMinIntegerValue) return true;
  if (best_bound > integer_trail_->LowerBound(var_to_push_)) {
    // Compute the reason.
    // It is just the reason for the energy after time.
    helper_->ClearReason();
    for (int t = 0; t < helper_->NumTasks(); ++t) {
      if (!is_in_subtasks_[t]) continue;
      if (!helper_->IsPresent(t)) continue;

      const IntegerValue size_min = helper_->SizeMin(t);
      if (size_min == 0) continue;

      const IntegerValue demand_min = demands_->DemandMin(t);
      if (demand_min == 0) continue;

      const IntegerValue end_min = helper_->EndMin(t);
      if (end_min <= best_time) continue;

      helper_->AddEndMinReason(t, std::min(best_time + size_min, end_min));
      helper_->AddSizeMinReason(t);
      helper_->AddPresenceReason(t);
      demands_->AddDemandMinReason(t);
    }
    if (capacity_.var != kNoIntegerVariable) {
      helper_->MutableIntegerReason()->push_back(
          integer_trail_->UpperBoundAsLiteral(capacity_.var));
    }

    // Propagate.
    if (!helper_->PushIntegerLiteral(
            IntegerLiteral::GreaterOrEqual(var_to_push_, best_bound))) {
      return false;
    }
  }

  return true;
}

void CumulativeIsAfterSubsetConstraint::RegisterWith(
    GenericLiteralWatcher* watcher) {
  helper_->SetTimeDirection(true);
  const int id = watcher->Register(this);
  watcher->SetPropagatorPriority(id, 2);
  watcher->WatchUpperBound(capacity_, id);
  for (const int t : subtasks_) {
    watcher->WatchLowerBound(helper_->Starts()[t], id);
    watcher->WatchLowerBound(helper_->Ends()[t], id);
    watcher->WatchLowerBound(helper_->Sizes()[t], id);
    watcher->WatchLowerBound(demands_->Demands()[t], id);
    if (!helper_->IsPresent(t) && !helper_->IsAbsent(t)) {
      watcher->WatchLiteral(helper_->PresenceLiteral(t), id);
    }
  }
}

}  // namespace sat
}  // namespace operations_research
