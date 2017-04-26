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

#include "ortools/sat/timetable.h"

#include <algorithm>

#include "ortools/sat/overload_checker.h"
#include "ortools/sat/precedences.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/util/sort.h"

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

  // Reversible set of tasks to consider for propagation.
  num_tasks_to_sweep_ = num_tasks_;
  tasks_to_sweep_.resize(num_tasks_);
  num_active_tasks_ = num_tasks_;
  active_tasks_.resize(num_tasks_);
  num_profile_tasks_ = 0;
  profile_tasks_.resize(num_tasks_);
  positions_in_profile_tasks_.resize(num_tasks_);

  // Reversible bounds and starting height of the profile.
  starting_profile_height_ = IntegerValue(0);
  left_start_ = 0;
  left_end_ = 0;
  right_start_ = num_tasks_;
  right_end_ = num_tasks_;

  // Vector of tasks to sort to build the profile.
  by_start_max_.resize(num_tasks_);
  by_end_min_.resize(num_tasks_);

  for (int t = 0; t < num_tasks_; ++t) {
    tasks_to_sweep_[t] = t;
    active_tasks_[t] = t;
    profile_tasks_[t] = t;
    positions_in_profile_tasks_[t] = t;
    by_start_max_[t].task_index = t;
    by_end_min_[t].task_index = t;
  }
}

void TimeTablingPerTask::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  helper_->WatchAllTasks(id, watcher);
  watcher->WatchUpperBound(capacity_var_, id);
  for (int t = 0; t < num_tasks_; t++) {
    watcher->WatchLowerBound(demand_vars_[t], id);
  }
  // Repositories responsible for restoring the reversible values.
  watcher->RegisterReversibleClass(id, &rev_repository_int_);
  watcher->RegisterReversibleClass(id, &rev_repository_integer_value_);
}

bool TimeTablingPerTask::Propagate() {
  // TODO(user): understand why the following line creates a bug.
  // if (num_tasks_to_sweep_ == 0) return true;

  // Save the current state of the set of tasks.
  rev_repository_int_.SaveState(&num_tasks_to_sweep_);
  rev_repository_int_.SaveState(&num_active_tasks_);
  rev_repository_int_.SaveState(&num_profile_tasks_);

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

  // Reduce the profile now that we know that it is stable.
  // TODO(user): something seems to be wrong we the reduction. To reactivate
  // once we'll have understood (and fixed) the behavior.
  if (false) ReduceProfile();

  return true;
}

bool TimeTablingPerTask::BuildProfile() {
  helper_->SetTimeDirection(true);  // forward

  // Update the set of tasks that contribute to the profile. Tasks that were
  // contributing are still part of the profile so we only need to check the
  // other tasks.
  for (int i = num_profile_tasks_; i < num_tasks_; ++i) {
    const int t1 = profile_tasks_[i];
    if (helper_->IsPresent(t1) && helper_->StartMax(t1) < helper_->EndMin(t1)) {
      // Swap values and positions.
      const int t2 = profile_tasks_[num_profile_tasks_];
      profile_tasks_[i] = t2;
      profile_tasks_[num_profile_tasks_] = t1;
      positions_in_profile_tasks_[t1] = num_profile_tasks_;
      positions_in_profile_tasks_[t2] = i;
      num_profile_tasks_++;
    }
  }

  // Update start value of active tasks.
  for (int i = left_start_; i < right_start_; ++i) {
    by_start_max_[i].time = helper_->StartMax(by_start_max_[i].task_index);
  }
  // Likely to be already or almost sorted.
  IncrementalSort(by_start_max_.begin() + left_start_,
                  by_start_max_.begin() + right_start_);
  // Build start of compulsory part events.
  scp_.clear();
  for (int i = left_start_; i < right_start_; ++i) {
    if (IsInProfile(by_start_max_[i].task_index)) {
      scp_.push_back(by_start_max_[i]);
    }
  }

  // Update end value of active tasks.
  for (int i = left_end_; i < right_end_; ++i) {
    by_end_min_[i].time = helper_->EndMin(by_end_min_[i].task_index);
  }
  // Likely to be already or almost sorted.
  IncrementalSort(by_end_min_.begin() + left_end_,
                  by_end_min_.begin() + right_end_);
  // Build end of compulsory part events.
  ecp_.clear();
  for (int i = left_end_; i < right_end_; ++i) {
    if (IsInProfile(by_end_min_[i].task_index)) {
      ecp_.push_back(by_end_min_[i]);
    }
  }

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
  IntegerValue current_height = starting_profile_height_;

  // Next scp and ecp events to be processed.
  int next_scp = 0;
  int next_ecp = 0;

  while (next_ecp < ecp_.size()) {
    const IntegerValue old_height = current_height;

    // Next time point.
    IntegerValue t = ecp_[next_ecp].time;
    if (next_scp < scp_.size()) {
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
  DCHECK_GE(current_height, 0);
  profile_.push_back(
      ProfileRectangle(current_start, kMaxIntegerValue, IntegerValue(0)));

  // Add a sentinel to simplify the algorithm.
  profile_.push_back(
      ProfileRectangle(kMaxIntegerValue, kMaxIntegerValue, IntegerValue(0)));

  // Increase the capacity variable if required.
  return IncreaseCapacity(max_height_start, profile_max_height_);
}

// TODO(user): calling this function currently have an impact on the search.
// This should not be the case and might be the symptom of a larger bug.
void TimeTablingPerTask::ReduceProfile() {
  helper_->SetTimeDirection(true);  // forward

  IntegerValue min_start_min = kMaxIntegerValue;
  IntegerValue max_end_max = kMinIntegerValue;

  // TODO(user): we could probably go a step further by considering tasks
  // that only have a fixed demand and start (resp. end) variables. The idea is
  // that the start of their mandatory part is not going to change and could
  // already be aggregated in starting_profile_height_.
  for (int i = num_active_tasks_ - 1; i >= 0; --i) {
    const int t = active_tasks_[i];
    // Discard absent and fixed tasks.
    if (helper_->IsAbsent(t) ||
        (helper_->StartMin(t) == helper_->StartMax(t) &&
         helper_->EndMin(t) == helper_->EndMax(t) &&
         DemandMin(t) == DemandMax(t) && helper_->IsPresent(t))) {
      std::swap(active_tasks_[i], active_tasks_[--num_active_tasks_]);
      continue;
    }
    min_start_min = std::min(min_start_min, helper_->StartMin(t));
    max_end_max = std::max(max_end_max, helper_->EndMax(t));
  }

  // Save the current state of the reversible values.
  rev_repository_integer_value_.SaveState(&starting_profile_height_);
  rev_repository_int_.SaveState(&left_start_);
  rev_repository_int_.SaveState(&left_end_);
  rev_repository_int_.SaveState(&right_start_);
  rev_repository_int_.SaveState(&right_end_);

  // Update the range of active tasks in their corresponding sorted vectors.
  while (left_start_ < right_start_ &&
         by_start_max_[left_start_].time < min_start_min) {
    const int t = by_start_max_[left_start_].task_index;
    if (helper_->IsPresent(t)) starting_profile_height_ += DemandMin(t);
    left_start_++;
  }

  while (left_end_ < right_end_ &&
         by_end_min_[left_end_].time < min_start_min) {
    const int t = by_end_min_[left_end_].task_index;
    if (helper_->IsPresent(t)) starting_profile_height_ -= DemandMin(t);
    left_end_++;
  }

  while (left_start_ < right_start_ &&
         max_end_max < by_start_max_[right_start_ - 1].time) {
    right_start_--;
  }

  while (left_end_ < right_end_ &&
         max_end_max < by_end_min_[right_end_ - 1].time) {
    right_end_--;
  }

  DCHECK_GE(starting_profile_height_, 0);
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
      std::swap(tasks_to_sweep_[i], tasks_to_sweep_[--num_tasks_to_sweep_]);
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
  int rec_id = 1;
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
      overload = !IsInProfile(task_id);
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
  // TODO(user): find an efficient way to keep the start_max < new_end_min
  // condition. The problem is that ReduceProfile() assumes that by_end_min and
  // by_start_max are up to date (this is not necessarily the case if we use
  // the old condition). A solution is to update those vector before calling
  // ReduceProfile() or to ReduceProfile() directly after BuildProfile() in the
  // main loop.
  profile_changed_ |= true;

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
  for (int i = 0; i < num_profile_tasks_; ++i) {
    const int t = profile_tasks_[i];
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
