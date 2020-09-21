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

#include "ortools/sat/scheduling_constraints.h"

#include "ortools/sat/integer.h"

namespace operations_research {
namespace sat {

bool ConvexHullPropagator::Propagate() {
  const int num_tasks = helper_->NumTasks() - 1;  // Ignore the target.
  const int target = helper_->NumTasks() - 1;     // Last interval.

  const IntegerValue target_min_start = helper_->StartMin(target);
  const IntegerValue target_max_start = helper_->StartMax(target);
  const IntegerValue target_max_end = helper_->EndMax(target);
  const IntegerValue target_min_end = helper_->EndMin(target);

  // Propagate target absence to all tasks.
  if (helper_->IsAbsent(target)) {
    for (int t = 0; t < num_tasks; ++t) {
      if (!helper_->IsAbsent(t)) {
        helper_->ClearReason();
        helper_->AddAbsenceReason(target);
        if (!helper_->PushTaskAbsence(t)) {
          return false;
        }
      }
    }
    return true;
  }

  // Propagate task presence to target.
  if (!helper_->IsPresent(target)) {
    for (int t = 0; t < num_tasks; ++t) {
      if (helper_->IsPresent(t)) {
        helper_->ClearReason();
        helper_->AddPresenceReason(t);
        if (!helper_->PushTaskPresence(target)) {
          return false;
        }
        break;
      }
    }
  }

  // Count absent tasks, eject incompatible tasks.
  int num_absent_tasks = 0;
  int first_non_absent_task = -1;
  for (int t = 0; t < num_tasks; ++t) {
    if (helper_->IsAbsent(t)) {
      num_absent_tasks++;
    } else if (helper_->StartMin(t) > target_max_end &&
               helper_->IsPresent(target)) {
      helper_->ClearReason();
      helper_->AddPresenceReason(target);
      helper_->AddEndMaxReason(target, target_max_end);
      helper_->AddStartMinReason(t, target_max_end + 1);
      if (!helper_->PushTaskAbsence(t)) {
        return false;
      }
      num_absent_tasks++;
    } else if (helper_->EndMax(t) < target_min_start &&
               helper_->IsPresent(target)) {
      helper_->ClearReason();
      helper_->AddPresenceReason(target);
      helper_->AddStartMinReason(target, target_min_start);
      helper_->AddEndMaxReason(t, target_min_start - 1);
      if (!helper_->PushTaskAbsence(t)) {
        return false;
      }
      num_absent_tasks++;
    } else if (first_non_absent_task == -1) {
      first_non_absent_task = t;
    }
  }

  // No active tasks left, then the target must be absent too.
  if (num_absent_tasks == num_tasks && !helper_->IsAbsent(target)) {
    helper_->ClearReason();
    for (int t = 0; t < num_tasks; ++t) {
      helper_->AddAbsenceReason(t);
    }
    if (!helper_->PushTaskAbsence(target)) {
      return false;
    }
    return true;
  }

  // Target is present, and one task left, it must be present too.
  if (num_absent_tasks == num_tasks - 1 && helper_->IsPresent(target) &&
      !helper_->IsPresent(first_non_absent_task)) {
    DCHECK_NE(first_non_absent_task, -1);
    helper_->ClearReason();
    for (int t = 0; t < num_tasks; ++t) {
      if (t == first_non_absent_task) continue;
      helper_->AddAbsenceReason(t);
    }
    helper_->AddPresenceReason(target);
    if (!helper_->PushTaskPresence(first_non_absent_task)) {
      return false;
    }
  }

  IntegerValue min_of_start_mins(kMaxIntegerValue);
  IntegerValue min_of_present_start_maxes(kMaxIntegerValue);
  IntegerValue max_of_possible_start_maxes(kMinIntegerValue);
  IntegerValue max_of_present_end_mins(kMinIntegerValue);
  IntegerValue min_of_possible_end_mins(kMaxIntegerValue);
  IntegerValue max_of_end_maxes(kMinIntegerValue);
  int start_max_support = -1;
  int end_min_support = -1;
  const bool target_is_present = helper_->IsPresent(target);
  int num_possible_tasks = 0;
  int num_present_tasks = 0;

  // Loop through tasks, collect their convex hull.
  for (int t = 0; t < num_tasks; ++t) {
    if (helper_->IsAbsent(t)) continue;

    min_of_start_mins = std::min(min_of_start_mins, helper_->StartMin(t));
    max_of_end_maxes = std::max(max_of_end_maxes, helper_->EndMax(t));

    if (helper_->IsPresent(t)) {
      DCHECK(target_is_present);
      num_present_tasks++;
      if (helper_->StartMax(t) < min_of_present_start_maxes) {
        min_of_present_start_maxes = helper_->StartMax(t);
        start_max_support = t;
      }
      if (helper_->EndMin(t) > max_of_present_end_mins) {
        max_of_present_end_mins = helper_->EndMin(t);
        end_min_support = t;
      }

      // Push the task inside the convex hull described by the target.
      if (helper_->EndMax(t) > target_max_end) {
        helper_->ClearReason();
        helper_->AddPresenceReason(t);
        helper_->AddPresenceReason(target);
        helper_->AddEndMaxReason(target, target_max_end);
        if (!helper_->DecreaseEndMax(t, target_max_end)) {
          return false;
        }
      }
      if (helper_->StartMin(t) < target_min_start) {
        helper_->ClearReason();
        helper_->AddPresenceReason(t);
        helper_->AddPresenceReason(target);
        helper_->AddStartMinReason(target, target_min_start);
        if (!helper_->IncreaseStartMin(t, target_min_start)) {
          return false;
        }
      }
    } else {
      DCHECK(helper_->IsOptional(t));
      num_possible_tasks++;
      max_of_possible_start_maxes =
          std::max(max_of_possible_start_maxes, helper_->StartMax(t));
      min_of_possible_end_mins =
          std::min(min_of_possible_end_mins, helper_->EndMin(t));
    }
  }

  if (min_of_start_mins > target_min_start) {
    helper_->ClearReason();
    for (int t = 0; t < num_tasks; ++t) {
      if (helper_->IsAbsent(t)) {
        helper_->AddAbsenceReason(t);
      } else {
        helper_->AddStartMinReason(t, min_of_start_mins);
      }
    }
    if (!helper_->IncreaseStartMin(target, min_of_start_mins)) {
      return false;
    }
  }

  if (num_present_tasks > 0 && min_of_present_start_maxes < target_max_start) {
    DCHECK(target_is_present);
    DCHECK_NE(start_max_support, -1);
    helper_->ClearReason();
    helper_->AddPresenceReason(start_max_support);
    helper_->AddStartMaxReason(start_max_support, min_of_present_start_maxes);
    if (!helper_->DecreaseStartMax(target, min_of_present_start_maxes)) {
      return false;
    }
  }

  if (num_present_tasks > 0 && max_of_present_end_mins > target_min_end) {
    DCHECK(target_is_present);
    DCHECK_NE(end_min_support, -1);
    helper_->ClearReason();
    helper_->AddPresenceReason(end_min_support);
    helper_->AddEndMinReason(end_min_support, max_of_present_end_mins);
    if (!helper_->IncreaseEndMin(target, max_of_present_end_mins)) {
      return false;
    }
  }

  if (max_of_end_maxes < target_max_end) {
    helper_->ClearReason();
    for (int t = 0; t < num_tasks; ++t) {
      if (helper_->IsAbsent(t)) {
        helper_->AddAbsenceReason(t);
      } else {
        helper_->AddEndMaxReason(t, max_of_end_maxes);
      }
    }
    if (!helper_->DecreaseEndMax(target, max_of_end_maxes)) {
      return false;
    }
  }

  // All propagations and checks belows rely of the presence of the target.
  if (!target_is_present) return true;

  // Propagates in case every tasks are still optional.
  if (num_possible_tasks > 0 && num_present_tasks == 0) {
    if (helper_->StartMax(target) > max_of_possible_start_maxes) {
      helper_->ClearReason();
      for (int t = 0; t < num_tasks; ++t) {
        if (helper_->IsAbsent(t)) {
          helper_->AddAbsenceReason(t);
        } else {
          helper_->AddStartMaxReason(t, max_of_possible_start_maxes);
        }
      }
      helper_->AddPresenceReason(target);
      if (!helper_->DecreaseStartMax(target, max_of_possible_start_maxes)) {
        return false;
      }
    }

    if (helper_->EndMin(target) < min_of_possible_end_mins) {
      helper_->ClearReason();
      for (int t = 0; t < num_tasks; ++t) {
        if (helper_->IsAbsent(t)) {
          helper_->AddAbsenceReason(t);
        } else {
          helper_->AddEndMinReason(t, min_of_possible_end_mins);
        }
      }
      helper_->AddPresenceReason(target);
      if (!helper_->IncreaseEndMin(target, min_of_possible_end_mins)) {
        return false;
      }
    }
  }

  DCHECK_GE(helper_->StartMin(target), min_of_start_mins);
  DCHECK_LE(helper_->EndMax(target), max_of_end_maxes);

  // If there is only one task left. It is equal to the target.
  if (num_possible_tasks + num_present_tasks > 1) return true;

  DCHECK_EQ(num_possible_tasks, 0);
  DCHECK_EQ(num_present_tasks, 1);

  // Propagate bound from target to the only present task.
  DCHECK_EQ(min_of_start_mins, helper_->StartMin(first_non_absent_task));
  if (target_min_start > min_of_start_mins) {
    helper_->ClearReason();
    for (int t = 0; t < num_tasks; ++t) {
      if (t != first_non_absent_task) {
        helper_->AddAbsenceReason(t);
      } else {
        helper_->AddPresenceReason(t);
      }
    }
    helper_->AddStartMinReason(target, target_min_start);
    helper_->AddPresenceReason(target);
    if (!helper_->IncreaseStartMin(first_non_absent_task, target_min_start)) {
      return false;
    }
  }

  if (target_max_start < helper_->StartMax(first_non_absent_task)) {
    helper_->ClearReason();
    for (int t = 0; t < num_tasks; ++t) {
      if (t == first_non_absent_task) {
        helper_->AddPresenceReason(t);
      } else {
        helper_->AddAbsenceReason(t);
      }
    }
    helper_->AddStartMaxReason(target, target_max_start);
    if (!helper_->DecreaseStartMax(first_non_absent_task, target_max_start)) {
      return false;
    }
  }

  if (target_min_end > helper_->EndMin(first_non_absent_task)) {
    helper_->ClearReason();
    for (int t = 0; t < num_tasks; ++t) {
      if (t == first_non_absent_task) {
        helper_->AddPresenceReason(t);
      } else {
        helper_->AddAbsenceReason(t);
      }
    }
    helper_->AddStartMaxReason(target, target_min_end);
    if (!helper_->IncreaseEndMin(first_non_absent_task, target_min_end)) {
      return false;
    }
  }

  DCHECK_EQ(max_of_end_maxes, helper_->EndMax(first_non_absent_task));
  if (target_max_end < max_of_end_maxes) {
    for (int t = 0; t < num_tasks; ++t) {
      if (t != first_non_absent_task) {
        helper_->AddAbsenceReason(t);
      } else {
        helper_->AddPresenceReason(t);
      }
    }
    helper_->AddPresenceReason(target);
    helper_->AddEndMaxReason(target, target_max_end);
    if (!helper_->DecreaseEndMax(first_non_absent_task, target_max_end)) {
      return false;
    }
  }

  return true;
}

int ConvexHullPropagator::RegisterWith(GenericLiteralWatcher* watcher) {
  // This propagator reach the fix point in one pass.
  const int id = watcher->Register(this);
  helper_->WatchAllTasks(id, watcher);
  return id;
}

}  // namespace sat
}  // namespace operations_research
