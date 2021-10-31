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

#include "ortools/sat/cumulative_energy.h"

#include <memory>
#include <utility>

#include "ortools/base/int_type.h"
#include "ortools/base/iterator_adaptors.h"
#include "ortools/base/logging.h"
#include "ortools/sat/sat_base.h"

namespace operations_research {
namespace sat {

void AddCumulativeEnergyConstraint(std::vector<AffineExpression> energies,
                                   AffineExpression capacity,
                                   SchedulingConstraintHelper* helper,
                                   Model* model) {
  auto* watcher = model->GetOrCreate<GenericLiteralWatcher>();
  auto* integer_trail = model->GetOrCreate<IntegerTrail>();

  CumulativeEnergyConstraint* constraint = new CumulativeEnergyConstraint(
      std::move(energies), capacity, integer_trail, helper);
  constraint->RegisterWith(watcher);
  model->TakeOwnership(constraint);
}

void AddCumulativeOverloadChecker(const std::vector<AffineExpression>& demands,
                                  AffineExpression capacity,
                                  SchedulingConstraintHelper* helper,
                                  Model* model) {
  auto* watcher = model->GetOrCreate<GenericLiteralWatcher>();
  auto* integer_trail = model->GetOrCreate<IntegerTrail>();

  std::vector<AffineExpression> energies;
  const int num_tasks = helper->NumTasks();
  CHECK_EQ(demands.size(), num_tasks);
  for (int t = 0; t < num_tasks; ++t) {
    const AffineExpression size = helper->Sizes()[t];
    const AffineExpression demand = demands[t];

    if (demand.var == kNoIntegerVariable && size.var == kNoIntegerVariable) {
      CHECK_GE(demand.constant, 0);
      CHECK_GE(size.constant, 0);
      energies.emplace_back(demand.constant * size.constant);
    } else if (demand.var == kNoIntegerVariable) {
      CHECK_GE(demand.constant, 0);
      energies.push_back(size.MultipliedBy(demand.constant));
    } else if (size.var == kNoIntegerVariable) {
      CHECK_GE(size.constant, 0);
      energies.push_back(demand.MultipliedBy(size.constant));
    } else {
      // The case where both demand and size are variable should be rare.
      //
      // TODO(user): Handle when needed by creating an intermediate product
      // variable equal to demand * size. Note that because of the affine
      // expression, we do need some custom code for this.
      LOG(INFO) << "Overload checker with variable demand and variable size "
                   "is currently not implemented. Skipping.";
      return;
    }
  }

  CumulativeEnergyConstraint* constraint =
      new CumulativeEnergyConstraint(energies, capacity, integer_trail, helper);
  constraint->RegisterWith(watcher);
  model->TakeOwnership(constraint);
}

CumulativeEnergyConstraint::CumulativeEnergyConstraint(
    std::vector<AffineExpression> energies, AffineExpression capacity,
    IntegerTrail* integer_trail, SchedulingConstraintHelper* helper)
    : energies_(std::move(energies)),
      capacity_(capacity),
      integer_trail_(integer_trail),
      helper_(helper),
      theta_tree_() {
  const int num_tasks = helper_->NumTasks();
  CHECK_EQ(energies_.size(), num_tasks);
  task_to_start_event_.resize(num_tasks);
}

void CumulativeEnergyConstraint::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  helper_->WatchAllTasks(id, watcher);
  watcher->NotifyThatPropagatorMayNotReachFixedPointInOnePass(id);
}

bool CumulativeEnergyConstraint::Propagate() {
  // This only uses one time direction, but the helper might be used elsewhere.
  // TODO(user): just keep the current direction?
  if (!helper_->SynchronizeAndSetTimeDirection(true)) return false;

  const IntegerValue capacity_max = integer_trail_->UpperBound(capacity_);
  // TODO(user): force capacity_max >= 0, fail/remove optionals when 0.
  if (capacity_max <= 0) return true;

  // Set up theta tree.
  start_event_task_time_.clear();
  int num_events = 0;
  for (const auto task_time : helper_->TaskByIncreasingStartMin()) {
    const int task = task_time.task_index;
    if (helper_->IsAbsent(task) ||
        integer_trail_->UpperBound(energies_[task]) == 0) {
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
        theta_tree_.AddOrUpdateEvent(
            current_event, start_min * capacity_max,
            integer_trail_->LowerBound(energies_[current_task]),
            integer_trail_->UpperBound(energies_[current_task]));
      } else {
        theta_tree_.AddOrUpdateOptionalEvent(
            current_event, start_min * capacity_max,
            integer_trail_->UpperBound(energies_[current_task]));
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
            if (energies_[task].var != kNoIntegerVariable) {
              helper_->MutableIntegerReason()->push_back(
                  integer_trail_->LowerBoundAsLiteral(energies_[task].var));
            }
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
          if (energies_[task].var != kNoIntegerVariable) {
            helper_->MutableIntegerReason()->push_back(
                integer_trail_->LowerBoundAsLiteral(energies_[task].var));
          }
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

      if (new_energy_max <
          integer_trail_->LowerBound(energies_[task_with_new_energy_max])) {
        if (helper_->IsOptional(task_with_new_energy_max)) {
          return helper_->PushTaskAbsence(task_with_new_energy_max);
        } else {
          return helper_->ReportConflict();
        }
      } else {
        const IntegerLiteral deduction =
            energies_[task_with_new_energy_max].LowerOrEqual(new_energy_max);
        if (!helper_->PushIntegerLiteralIfTaskPresent(task_with_new_energy_max,
                                                      deduction)) {
          return false;
        }
      }

      if (helper_->IsPresent(task_with_new_energy_max)) {
        theta_tree_.AddOrUpdateEvent(
            task_to_start_event_[task_with_new_energy_max],
            start_event_task_time_[event_with_new_energy_max].time *
                capacity_max,
            integer_trail_->LowerBound(energies_[task_with_new_energy_max]),
            new_energy_max);
      } else {
        theta_tree_.RemoveEvent(event_with_new_energy_max);
      }
    }
  }
  return true;
}

CumulativeIsAfterSubsetConstraint::CumulativeIsAfterSubsetConstraint(
    IntegerVariable var, IntegerValue offset, AffineExpression capacity,
    const std::vector<AffineExpression> demands,
    const std::vector<int> subtasks, IntegerTrail* integer_trail,
    SchedulingConstraintHelper* helper)
    : var_to_push_(var),
      offset_(offset),
      capacity_(capacity),
      demands_(demands),
      subtasks_(subtasks),
      integer_trail_(integer_trail),
      helper_(helper) {
  is_in_subtasks_.assign(helper->NumTasks(), false);
  for (const int t : subtasks) is_in_subtasks_[t] = true;
}

bool CumulativeIsAfterSubsetConstraint::Propagate() {
  const IntegerValue capacity_max = integer_trail_->UpperBound(capacity_);

  if (!helper_->SynchronizeAndSetTimeDirection(true)) {
    return false;
  }

  // Compute the total energy.
  // Compute the profile deltas in energy if all task are packed left.
  IntegerValue energy_after_time(0);
  std::vector<std::pair<IntegerValue, IntegerValue>> energy_changes;
  for (int t = 0; t < helper_->NumTasks(); ++t) {
    if (!is_in_subtasks_[t]) continue;
    if (!helper_->IsPresent(t)) continue;
    if (helper_->SizeMin(t) == 0) continue;

    const IntegerValue demand = integer_trail_->LowerBound(demands_[t]);
    if (demand == 0) continue;

    const IntegerValue size_min = helper_->SizeMin(t);
    const IntegerValue end_min = helper_->EndMin(t);
    energy_changes.push_back({end_min - size_min, demand});
    energy_changes.push_back({end_min, -demand});
    energy_after_time += size_min * demand;
  }

  IntegerValue best_time = kMinIntegerValue;
  IntegerValue best_end_min = kMinIntegerValue;

  IntegerValue previous_time = kMinIntegerValue;
  IntegerValue profile_height(0);

  // We consider the energy after a given time.
  // From that we derive a bound on the end_min of the subtasks.
  std::sort(energy_changes.begin(), energy_changes.end());
  for (int i = 0; i < energy_changes.size();) {
    const IntegerValue time = energy_changes[i].first;
    if (profile_height > 0) {
      energy_after_time -= profile_height * (time - previous_time);
    }
    previous_time = time;

    while (i < energy_changes.size() && energy_changes[i].first == time) {
      profile_height += energy_changes[i].second;
      ++i;
    }

    // We prefer higher time in case of ties since that should reduce the
    // explanation size.
    const IntegerValue end_min =
        time + CeilRatio(energy_after_time, capacity_max);
    if (end_min >= best_end_min) {
      best_time = time;
      best_end_min = end_min;
    }
  }
  CHECK_EQ(profile_height, 0);
  CHECK_EQ(energy_after_time, 0);

  if (best_end_min + offset_ > integer_trail_->LowerBound(var_to_push_)) {
    // Compute the reason.
    // It is just the reason for the energy after time.
    helper_->ClearReason();
    for (int t = 0; t < helper_->NumTasks(); ++t) {
      if (!is_in_subtasks_[t]) continue;
      if (!helper_->IsPresent(t)) continue;
      if (helper_->SizeMin(t) == 0) continue;

      const IntegerValue demand = integer_trail_->LowerBound(demands_[t]);
      if (demand == 0) continue;

      const IntegerValue size_min = helper_->SizeMin(t);
      const IntegerValue end_min = helper_->EndMin(t);
      helper_->AddEndMinReason(t, std::min(best_time + size_min, end_min));
      helper_->AddSizeMinReason(t);
      helper_->AddPresenceReason(t);
      if (demands_[t].var != kNoIntegerVariable) {
        helper_->MutableIntegerReason()->push_back(
            integer_trail_->LowerBoundAsLiteral(demands_[t].var));
      }
    }
    if (capacity_.var != kNoIntegerVariable) {
      helper_->MutableIntegerReason()->push_back(
          integer_trail_->UpperBoundAsLiteral(capacity_.var));
    }

    // Propagate.
    if (!helper_->PushIntegerLiteral(IntegerLiteral::GreaterOrEqual(
            var_to_push_, best_end_min + offset_))) {
      return false;
    }
  }

  return true;
}

void CumulativeIsAfterSubsetConstraint::RegisterWith(
    GenericLiteralWatcher* watcher) {
  helper_->SetTimeDirection(true);
  const int id = watcher->Register(this);
  watcher->WatchUpperBound(capacity_, id);
  for (const int t : subtasks_) {
    watcher->WatchLowerBound(helper_->Starts()[t], id);
    watcher->WatchLowerBound(helper_->Ends()[t], id);
    watcher->WatchLowerBound(helper_->Sizes()[t], id);
    watcher->WatchLowerBound(demands_[t], id);
    if (!helper_->IsPresent(t) && !helper_->IsAbsent(t)) {
      watcher->WatchLiteral(helper_->PresenceLiteral(t), id);
    }
  }
}

}  // namespace sat
}  // namespace operations_research
