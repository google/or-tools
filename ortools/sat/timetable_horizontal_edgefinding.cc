// Copyright 2010-2024 Google LLC
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

#include "ortools/sat/timetable_horizontal_edgefinding.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "absl/log/check.h"
#include "ortools/base/iterator_adaptors.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/model.h"
#include "ortools/util/sort.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {

HorizontallyElasticOverloadChecker::HorizontallyElasticOverloadChecker(
    AffineExpression capacity, SchedulingConstraintHelper* helper,
    SchedulingDemandHelper* demands, Model* model)
    : num_tasks_(helper->NumTasks()),
      capacity_(capacity),
      helper_(helper),
      demands_(demands),
      integer_trail_(model->GetOrCreate<IntegerTrail>()) {
  task_types_.resize(num_tasks_);
  profile_events_.reserve(4 * num_tasks_ + 1);
}

void HorizontallyElasticOverloadChecker::RegisterWith(
    GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  watcher->WatchUpperBound(capacity_, id);
  helper_->WatchAllTasks(id, watcher);
  for (int t = 0; t < num_tasks_; t++) {
    watcher->WatchLowerBound(demands_->Demands()[t], id);
  }
  watcher->SetPropagatorPriority(id, 3);
  watcher->NotifyThatPropagatorMayNotReachFixedPointInOnePass(id);
}

bool HorizontallyElasticOverloadChecker::Propagate() {
  if (!helper_->SynchronizeAndSetTimeDirection(true)) return false;
  if (!OverloadCheckerPass()) return false;
  if (!helper_->SynchronizeAndSetTimeDirection(false)) return false;
  if (!OverloadCheckerPass()) return false;
  return true;
}

bool HorizontallyElasticOverloadChecker::OverloadCheckerPass() {
  // Prepate the profile events which will be used during ScheduleTasks to
  // dynamically compute the profile. The events are valid for the entire
  // function and do not need to be recomputed.
  //
  // TODO: This datastructure contains everything we need to compute the
  // "classic" profile used in Time-Tabling.
  profile_events_.clear();
  for (int i = 0; i < num_tasks_; i++) {
    if (helper_->IsPresent(i) && demands_->DemandMin(i) > 0) {
      profile_events_.emplace_back(i, helper_->StartMin(i),
                                   demands_->DemandMin(i),
                                   ProfileEventType::kStartMin);
      profile_events_.emplace_back(i, helper_->StartMax(i),
                                   demands_->DemandMin(i),
                                   ProfileEventType::kStartMax);
      profile_events_.emplace_back(i, helper_->EndMin(i),
                                   demands_->DemandMin(i),
                                   ProfileEventType::kEndMin);
      profile_events_.emplace_back(i, helper_->EndMax(i),
                                   demands_->DemandMin(i),
                                   ProfileEventType::kEndMax);
    }
  }
  profile_events_.emplace_back(-1, kMaxIntegerValue, IntegerValue(0),
                               ProfileEventType::kSentinel);
  IncrementalSort(profile_events_.begin(), profile_events_.end());

  // Iterate on all the windows [-inf, window_end] where window_end is the end
  // max of a task.
  IntegerValue window_end = kMinIntegerValue;
  for (const ProfileEvent event : profile_events_) {
    if (event.event_type != ProfileEventType::kEndMax ||
        event.time <= window_end) {
      continue;
    }
    window_end = event.time;

    const IntegerValue capacity = integer_trail_->UpperBound(capacity_);
    if (window_end < ScheduleTasks(window_end, capacity)) {
      AddScheduleTaskReason(window_end);
      return helper_->ReportConflict();
    }
  }

  return true;
}

// ScheduleTasks returns a lower bound of the earliest time at which a group
// of tasks will complete. The group of tasks is all the tasks finishing before
// the end of the window + the fixed part of the tasks having a mandatory part
// that overlaps with the window.
IntegerValue HorizontallyElasticOverloadChecker::ScheduleTasks(
    IntegerValue window_end, IntegerValue capacity) {
  // TODO: If we apply this only by increasing window_end, then there is no
  // need to re-process the FULL tasks. Only the fixed-part and ignored might
  // change type. Specifically, FIXED-PART can become FULL while IGNORE can
  // either become FIXED-PART or FULL. Said otherwise, FULL is the terminal
  // state.
  for (int i = 0; i < num_tasks_; ++i) {
    if (helper_->EndMax(i) <= window_end) {
      task_types_[i] = TaskType::kFull;
      continue;
    }
    // If the task is external but has a compulsory part that starts before
    // window_end, then we can process it partially.
    if (helper_->StartMax(i) < window_end &&
        helper_->StartMax(i) < helper_->EndMin(i)) {
      task_types_[i] = TaskType::kFixedPart;
      continue;
    }
    // Otherwise, simply mark the task to be ignored during sweep.
    task_types_[i] = TaskType::kIgnore;
  }

  int next_event = 0;
  IntegerValue time = profile_events_[0].time;

  // Estimation of the earliest time at which all the processed tasks can be
  // scheduled.
  IntegerValue new_window_end = kMinIntegerValue;

  // Overload represents the accumulated quantity of energy that could not be
  // consumed before time.
  IntegerValue overload = 0;

  // Total demand required at time if all processed tasks were starting at
  // their start min.
  IntegerValue demand_req = 0;

  // Total demand required at time if all processed tasks that could overlap
  // time were. This is used to avoid placing overload in places were no task
  // would actually be.
  IntegerValue demand_max = 0;

  while (time < window_end) {
    // Aggregate the changes of all events happening at time. How to process
    // an event depends on its type ("full" vs "fixed-part").
    IntegerValue delta_max = 0;
    IntegerValue delta_req = 0;
    while (profile_events_[next_event].time == time) {
      const ProfileEvent event = profile_events_[next_event];
      switch (task_types_[event.task_id]) {
        case TaskType::kIgnore:
          break;  // drop the event
        case TaskType::kFull:
          switch (event.event_type) {
            case ProfileEventType::kStartMin:
              delta_max += event.height;
              delta_req += event.height;
              break;
            case ProfileEventType::kEndMin:
              delta_req -= event.height;
              break;
            case ProfileEventType::kEndMax:
              delta_max -= event.height;
              break;
            default:
              break;
          }
          break;
        case TaskType::kFixedPart:
          switch (event.event_type) {
            case ProfileEventType::kStartMax:
              delta_max += event.height;
              delta_req += event.height;
              break;
            case ProfileEventType::kEndMin:
              delta_req -= event.height;
              delta_max -= event.height;
              break;
            default:
              break;
          }
          break;
      }
      next_event++;
    }

    // Should always be safe thanks to the sentinel.
    DCHECK_LT(next_event, profile_events_.size());

    IntegerValue next_time = profile_events_[next_event].time;
    const IntegerValue length = next_time - time;

    demand_max += delta_max;
    demand_req += delta_req;

    DCHECK_LE(demand_req, demand_max);
    DCHECK_GE(overload, 0);

    // The maximum amount of resource that could be consumed if all non-ignored
    // tasks that could be scheduled at the current time were.
    const IntegerValue true_capa = std::min(demand_max, capacity);

    // Indicate whether we're using some capacity at this time point. This is
    // used to decide later on how to update new_window_end.
    const IntegerValue capa_used = std::min(demand_req + overload, true_capa);

    // Amount of resource available to potentially place some overload from
    // previous time points.
    const IntegerValue overload_delta = demand_req - true_capa;

    if (overload_delta > 0) {  // adding overload
      overload += length * overload_delta;
    } else if (overload_delta < 0 && overload > 0) {  // remove overload
      const IntegerValue used = std::min(-overload_delta, overload);
      const IntegerValue removable = used * length;
      if (removable < overload) {
        overload -= removable;
      } else {
        // Adjust next_time to indicate that the true "next event" in terms of
        // a change in resource consumption is happening before the next event
        // in the profile. This is important to guarantee that new_window_end
        // is properly adjusted below.
        next_time = time + (overload + used - 1) / used;  // ceil(overload/used)
        overload = 0;
      }
    }

    if (capa_used > 0) {
      // Note that next_time might be earlier than the time of the next event if
      // all the overload was consumed. See comment above.
      new_window_end = next_time;
    }

    time = profile_events_[next_event].time;
  }

  if (overload > 0) {
    return kMaxIntegerValue;
  }
  return new_window_end;
}

void HorizontallyElasticOverloadChecker::AddScheduleTaskReason(
    IntegerValue window_end) {
  helper_->ClearReason();

  // Capacity of the resource.
  if (capacity_.var != kNoIntegerVariable) {
    helper_->MutableIntegerReason()->push_back(
        integer_trail_->UpperBoundAsLiteral(capacity_.var));
  }

  // TODO: There's an opportunity to further generalize the reason if
  // demand_max and overload are set to 0 before the end of the window.
  // This can happen if the resources consumption has "humps" though it
  // is unclear whether this pattern is likely in practice or not.
  for (int t = 0; t < num_tasks_; ++t) {
    switch (task_types_[t]) {
      case TaskType::kFull:
        helper_->AddStartMinReason(t, helper_->StartMin(t));
        helper_->AddEndMaxReason(t, helper_->EndMax(t));
        break;
      case TaskType::kFixedPart:
        helper_->AddEndMinReason(t, std::min(helper_->EndMin(t), window_end));
        helper_->AddStartMaxReason(t, helper_->StartMax(t));
        break;
      case TaskType::kIgnore:
        continue;
    }

    helper_->AddPresenceReason(t);
    helper_->AddSizeMinReason(t);
    demands_->AddDemandMinReason(t);
  }
}

}  // namespace sat
}  // namespace operations_research
