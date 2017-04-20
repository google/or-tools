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

#include "sat/timetable.h"

#include <algorithm>

#include "sat/overload_checker.h"
#include "sat/precedences.h"
#include "sat/sat_solver.h"
#include "util/sort.h"

namespace operations_research {
namespace sat {

TimeTablingPerTask::TimeTablingPerTask(
    const std::vector<IntegerVariable>& demand_vars, IntegerVariable capacity,
    IntegerTrail* integer_trail, SchedulingConstraintHelper* helper)
    : num_tasks_(helper->NumTasks()),
      demand_vars_(demand_vars),
      capacity_var_(capacity),
      integer_trail_(integer_trail),
      helper_(helper) {
  // Each task may create at most two profile rectangles. Such pattern appear if
  // the profile is shaped like the Hanoi tower. The additional space is for
  // both extremities and the sentinels.
  profile_.reserve(2 * num_tasks_ + 4);
  scp_.reserve(num_tasks_);
  ecp_.reserve(num_tasks_);
  is_in_profile_.resize(num_tasks_);
  in_profile_.reserve(num_tasks_);
  // Reversible set of tasks to consider for propagation.
  num_tasks_to_sweep_ = num_tasks_;
  tasks_to_sweep_.resize(num_tasks_);
  for (int t = 0; t < num_tasks_; ++t) {
    tasks_to_sweep_[t] = t;
  }
}

void TimeTablingPerTask::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  helper_->WatchAllTasks(id, watcher);
  watcher->WatchUpperBound(capacity_var_, id);
  for (int t = 0; t < num_tasks_; t++) {
    watcher->WatchLowerBound(demand_vars_[t], id);
  }
  // Reversible number of tasks to consider for propagation.
  watcher->RegisterReversibleInt(id, &num_tasks_to_sweep_);
}

bool TimeTablingPerTask::Propagate() {
  // TODO(user): understand why the following line creates a bug.
  // if (num_tasks_to_sweep_ == 0) return true;
  // Repeat until the propagator does not filter anymore.
  profile_changed_ = true;
  while (profile_changed_) {
    profile_changed_ = false;
    // This can fail if the profile exceeds the resource capacity.
    if (!BuildProfile()) return false;
    // Update the minimum start times.
    if (!SweepAllTasks()) return false;
    // We reuse the same profile, but reversed, to update the maximum end times.
    ReverseProfile();
    // Update the maximum end times (reversed problem).
    if (!SweepAllTasks()) return false;
  }
  return true;
}

bool TimeTablingPerTask::BuildProfile() {
  helper_->SetTimeDirection(true);  // forward

  // Rely on the incremental sort algorithm of helper_.
  const auto& by_decreasing_start_max = helper_->TaskByDecreasingStartMax();
  const auto& by_increasing_end_min = helper_->TaskByIncreasingEndMin();

  scp_.clear();
  ecp_.clear();
  in_profile_.clear();

  // Build start of compulsory part events.
  for (int i = num_tasks_ - 1; i >= 0; --i) {
    const auto task_time = by_decreasing_start_max[i];
    const int t = task_time.task_index;
    is_in_profile_[t] =
        helper_->IsPresent(t) && task_time.time < helper_->EndMin(t);
    if (is_in_profile_[t]) {
      in_profile_.push_back(t);
      scp_.push_back(task_time);
    }
  }

  // Build end of compulsory part events.
  for (int i = 0; i < num_tasks_; ++i) {
    if (is_in_profile_[by_increasing_end_min[i].task_index]) {
      ecp_.push_back(by_increasing_end_min[i]);
    }
  }

  DCHECK_EQ(scp_.size(), ecp_.size());

  // Build the profile.
  // ------------------
  profile_.clear();

  // Start and height of the highest profile rectangle.
  profile_max_height_ = kMinIntegerValue;
  IntegerValue max_height_start = kMinIntegerValue;

  // Add a sentinel to simplify the algorithm.
  profile_.push_back(
      ProfileRectangle(kMinIntegerValue, kMinIntegerValue, IntegerValue(0)));

  // Start and height of the currently built profile rectange.
  IntegerValue current_start = kMinIntegerValue;
  IntegerValue current_height = IntegerValue(0);

  // Next scp and ecp events to be processed.
  int next_scp = 0;
  int next_ecp = 0;

  while (next_ecp < ecp_.size()) {
    const IntegerValue old_height = current_height;

    // Next time point.
    IntegerValue t = ecp_[next_ecp].time;
    if (next_scp < ecp_.size()) {
      t = std::min(t, scp_[next_scp].time);
    }

    // Process the starting compulsory parts.
    while (next_scp < scp_.size() && scp_[next_scp].time == t) {
      current_height += DemandMin(scp_[next_scp].task_index);
      next_scp++;
    }

    // Process the ending compulsory parts.
    while (next_ecp < ecp_.size() && ecp_[next_ecp].time == t) {
      current_height -= DemandMin(ecp_[next_ecp].task_index);
      next_ecp++;
    }

    // Insert a new profile rectangle if any.
    if (current_height != old_height) {
      profile_.push_back(ProfileRectangle(current_start, t, old_height));
      if (current_height > profile_max_height_) {
        profile_max_height_ = current_height;
        max_height_start = t;
      }
      current_start = t;
    }
  }
  // Build the last profile rectangle.
  DCHECK_EQ(current_height, 0);
  profile_.push_back(
      ProfileRectangle(current_start, kMaxIntegerValue, IntegerValue(0)));

  // Add a sentinel to simplify the algorithm.
  profile_.push_back(
      ProfileRectangle(kMaxIntegerValue, kMaxIntegerValue, IntegerValue(0)));

  // Increase the capacity variable if required.
  return IncreaseCapacity(max_height_start, profile_max_height_);
}

void TimeTablingPerTask::ReverseProfile() {
  helper_->SetTimeDirection(false);  // backward
  // Do not swap sentinels.
  int left = 1;
  int right = profile_.size() - 2;
  // Swap and reverse profile rectangles.
  while (left < right) {
    IntegerValue tmp = profile_[left].start;
    profile_[left].start = -profile_[right].end;
    profile_[right].end = -tmp;

    tmp = profile_[left].end;
    profile_[left].end = -profile_[right].start;
    profile_[right].start = -tmp;

    std::swap(profile_[left].height, profile_[right].height);

    left++;
    right--;
  }
  // Reverse the profile rectangle in the middle if any.
  if (left == right) {
    const IntegerValue tmp = profile_[left].start;
    profile_[left].start = -profile_[left].end;
    profile_[left].end = -tmp;
  }
}

bool TimeTablingPerTask::SweepAllTasks() {
  // Tasks with a lower or equal demand will not be pushed.
  const IntegerValue min_demand = CapacityMax() - profile_max_height_;

  for (int i = num_tasks_to_sweep_ - 1; i >= 0; --i) {
    const int t = tasks_to_sweep_[i];
    const bool fixed_start = helper_->StartMin(t) == helper_->StartMax(t);
    const bool fixed_end = helper_->EndMin(t) == helper_->EndMax(t);
    // A task does not have to be considered for propagation in the rest of the
    // sub-tree if it is absent or if it is present and respects one of these
    // conditions:
    // - its start and end variables are fixed;
    // - it has a maximum duration of 0;
    // - it has a maximum demand of 0;
    if (helper_->IsAbsent(t) ||
        (helper_->IsPresent(t) &&
         ((fixed_start && fixed_end) || helper_->DurationMax(t) == 0 ||
          DemandMax(t) == 0))) {
      // Remove the task from the set of tasks to sweep.
      num_tasks_to_sweep_--;
      tasks_to_sweep_[i] = tasks_to_sweep_[num_tasks_to_sweep_];
      tasks_to_sweep_[num_tasks_to_sweep_] = t;
      continue;
    }
    // A task does not have to be considered for propagation in this particular
    // iteration if it respects one of these conditions:
    // - it is present and its start variable is fixed;
    // - its minimum demand cannot lead to a resource overload;
    // - its minimum duration is 0.
    if ((helper_->IsPresent(t) && fixed_start) || DemandMin(t) <= min_demand ||
        helper_->DurationMin(t) == 0) {
      continue;
    }

    if (!SweepTask(t)) return false;
  }

  return true;
}

bool TimeTablingPerTask::SweepTask(int task_id) {
  const IntegerValue start_max = helper_->StartMax(task_id);
  const IntegerValue duration_min = helper_->DurationMin(task_id);
  const IntegerValue initial_start_min = helper_->StartMin(task_id);
  const IntegerValue initial_end_min = helper_->EndMin(task_id);

  IntegerValue new_start_min = initial_start_min;
  IntegerValue new_end_min = initial_end_min;

  // Find the profile rectangle that overlpas the minimum start time of task_id.
  // The sentinel prevents out of bound exceptions.
  int rec_id = 0;
  while (profile_[rec_id].end <= new_start_min) rec_id++;

  // A profile rectangle is in conflict with the task if its height exceeds
  // conflict_height.
  const IntegerValue conflict_height = CapacityMax() - DemandMin(task_id);

  // True if the task is in conflict with at least one profile rectangle.
  bool conflict_found = false;

  // Last time point during which task_id was in conflict with a profile
  // rectangle before being pushed.
  IntegerValue last_initial_conflict = kMinIntegerValue;

  // True if the task has been scheduled during a conflicting profile rectangle.
  // This means that the task is either part of the profile rectangle or that we
  // have an overload in which case we remove the case if it is optional.
  bool overload = false;

  // Push the task from left to right until it does not overlap any conflicting
  // rectangle. Pushing the task may push the end of its compulsory part on the
  // right but will not change its start. The main loop of the propagator will
  // take care of rebuilding the profile with these possible changes and to
  // propagate again in order to reach the timetabling consistency or to fail if
  // the profile exceeds the resource capacity.
  for (; profile_[rec_id].start < std::min(start_max, new_end_min); ++rec_id) {
    // If the profile rectangle is not conflicting, go to the next rectangle.
    if (profile_[rec_id].height <= conflict_height) continue;

    conflict_found = true;

    // Compute the next minimum start and end times of task_id. The variables
    // are not updated yet.
    new_start_min = profile_[rec_id].end;
    if (start_max < new_start_min) {
      new_start_min = start_max;
      overload = !is_in_profile_[task_id];
    }
    new_end_min = std::max(new_end_min, new_start_min + duration_min);

    if (profile_[rec_id].start < initial_end_min) {
      last_initial_conflict = std::min(new_start_min, initial_end_min) - 1;
    }
  }

  if (!conflict_found) return true;

  if (initial_start_min != new_start_min &&
      !UpdateStartingTime(task_id, last_initial_conflict, new_start_min)) {
    return false;
  }

  // The profile needs to be recomputed if the task has a mandatory part.
  profile_changed_ |= start_max < new_end_min;

  // Explain that the task should be absent or explain the resource overload.
  if (overload) return OverloadOrRemove(task_id, start_max);

  return true;
}

bool TimeTablingPerTask::UpdateStartingTime(int task_id, IntegerValue left,
                                            IntegerValue right) {
  helper_->ClearReason();

  AddProfileReason(left, right);

  helper_->MutableIntegerReason()->push_back(
      integer_trail_->UpperBoundAsLiteral(capacity_var_));

  // State of the task to be pushed.
  helper_->AddEndMinReason(task_id, left + 1);
  helper_->AddDurationMinReason(task_id, IntegerValue(1));
  helper_->MutableIntegerReason()->push_back(
      integer_trail_->LowerBoundAsLiteral(demand_vars_[task_id]));

  // Explain the increase of the minimum start and end times.
  return helper_->IncreaseStartMin(task_id, right);
}

void TimeTablingPerTask::AddProfileReason(IntegerValue left,
                                          IntegerValue right) {
  for (const int t : in_profile_) {
    const IntegerValue start_max = helper_->StartMax(t);
    const IntegerValue end_min = helper_->EndMin(t);

    // Do not consider the task if it does not overlap [left, right).
    if (end_min <= left || right <= start_max) continue;

    helper_->AddPresenceReason(t);
    helper_->AddStartMaxReason(t, std::max(left, start_max));
    helper_->AddEndMinReason(t, std::min(right, end_min));
    helper_->MutableIntegerReason()->push_back(
        integer_trail_->LowerBoundAsLiteral(demand_vars_[t]));
  }
}

bool TimeTablingPerTask::IncreaseCapacity(IntegerValue time,
                                          IntegerValue new_min) {
  if (new_min <= CapacityMin()) return true;
  helper_->ClearReason();
  helper_->MutableIntegerReason()->push_back(
      integer_trail_->UpperBoundAsLiteral(capacity_var_));
  AddProfileReason(time, time + 1);
  return helper_->PushIntegerLiteral(
      IntegerLiteral::GreaterOrEqual(capacity_var_, new_min));
}

bool TimeTablingPerTask::OverloadOrRemove(int task_id, IntegerValue time) {
  helper_->ClearReason();

  helper_->MutableIntegerReason()->push_back(
      integer_trail_->UpperBoundAsLiteral(capacity_var_));

  AddProfileReason(time, time + 1);

  // We know that task_id was not part of the profile we it was built. We thus
  // have to add it manualy since it will not be added by AddProfileReason.
  helper_->AddStartMaxReason(task_id, time);
  helper_->AddEndMinReason(task_id, time + 1);
  helper_->MutableIntegerReason()->push_back(
      integer_trail_->LowerBoundAsLiteral(demand_vars_[task_id]));

  // Explain the resource overload if the task cannot be removed.
  if (helper_->IsPresent(task_id)) {
    helper_->AddPresenceReason(task_id);
    return helper_->PushIntegerLiteral(
        IntegerLiteral::GreaterOrEqual(capacity_var_, CapacityMax() + 1));
  }

  // Remove the task to prevent the overload.
  helper_->PushTaskAbsence(task_id);

  return true;
}

}  // namespace sat
}  // namespace operations_research
