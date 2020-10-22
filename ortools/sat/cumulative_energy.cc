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
                                   SchedulingConstraintHelper *helper,
                                   Model *model) {
  auto *watcher = model->GetOrCreate<GenericLiteralWatcher>();
  auto *integer_trail = model->GetOrCreate<IntegerTrail>();

  CumulativeEnergyConstraint *constraint = new CumulativeEnergyConstraint(
      std::move(energies), capacity, integer_trail, helper);
  constraint->RegisterWith(watcher);
  model->TakeOwnership(constraint);
}

void AddCumulativeOverloadChecker(const std::vector<AffineExpression> &demands,
                                  AffineExpression capacity,
                                  SchedulingConstraintHelper *helper,
                                  Model *model) {
  auto *watcher = model->GetOrCreate<GenericLiteralWatcher>();
  auto *integer_trail = model->GetOrCreate<IntegerTrail>();

  std::vector<AffineExpression> energies;
  const int num_tasks = helper->NumTasks();
  CHECK_EQ(demands.size(), num_tasks);
  for (int t = 0; t < num_tasks; ++t) {
    // TODO(user): Remove once helper->Sizes()[t] is an expression.
    const AffineExpression size =
        helper->SizeVars()[t] == kNoIntegerVariable ||
                integer_trail->IsFixed(helper->SizeVars()[t])
            ? AffineExpression(helper->SizeMin(t))
            : AffineExpression(helper->SizeVars()[t]);
    const AffineExpression demand = demands[t];

    if (demand.var == kNoIntegerVariable && size.var == kNoIntegerVariable) {
      CHECK_GE(demand.constant, 0);
      CHECK_GE(size.constant, 0);
      energies.emplace_back(demand.constant * size.constant);
    } else if (demand.var == kNoIntegerVariable) {
      CHECK_GE(demand.constant, 0);
      energies.push_back(size);
      energies.back().coeff *= demand.constant;
      energies.back().constant *= demand.constant;
    } else if (size.var == kNoIntegerVariable) {
      CHECK_GE(size.constant, 0);
      energies.push_back(demand);
      energies.back().coeff *= size.constant;
      energies.back().constant *= size.constant;
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

  CumulativeEnergyConstraint *constraint =
      new CumulativeEnergyConstraint(energies, capacity, integer_trail, helper);
  constraint->RegisterWith(watcher);
  model->TakeOwnership(constraint);
}

CumulativeEnergyConstraint::CumulativeEnergyConstraint(
    std::vector<AffineExpression> energies, AffineExpression capacity,
    IntegerTrail *integer_trail, SchedulingConstraintHelper *helper)
    : energies_(std::move(energies)),
      capacity_(capacity),
      integer_trail_(integer_trail),
      helper_(helper),
      theta_tree_() {
  const int num_tasks = helper_->NumTasks();
  CHECK_EQ(energies_.size(), num_tasks);
  task_to_start_event_.resize(num_tasks);
}

void CumulativeEnergyConstraint::RegisterWith(GenericLiteralWatcher *watcher) {
  const int id = watcher->Register(this);
  helper_->WatchAllTasks(id, watcher);
  watcher->NotifyThatPropagatorMayNotReachFixedPointInOnePass(id);
}

bool CumulativeEnergyConstraint::Propagate() {
  // This only uses one time direction, but the helper might be used elsewhere.
  // TODO(user): just keep the current direction?
  helper_->SetTimeDirection(true);

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

}  // namespace sat
}  // namespace operations_research
