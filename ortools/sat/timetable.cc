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

#include "ortools/sat/timetable.h"

#include <algorithm>
#include <functional>
#include <memory>

#include "ortools/base/int_type.h"
#include "ortools/base/logging.h"
#include "ortools/util/sort.h"

namespace operations_research {
namespace sat {

TimeTablingPerTask::TimeTablingPerTask(
    const std::vector<AffineExpression> &demands, AffineExpression capacity,
    IntegerTrail *integer_trail, SchedulingConstraintHelper *helper)
    : num_tasks_(helper->NumTasks()),
      demands_(demands),
      capacity_(capacity),
      integer_trail_(integer_trail),
      helper_(helper) {
  // Each task may create at most two profile rectangles. Such pattern appear if
  // the profile is shaped like the Hanoi tower. The additional space is for
  // both extremities and the sentinels.
  profile_.reserve(2 * num_tasks_ + 4);

  // Reversible set of tasks to consider for propagation.
  forward_num_tasks_to_sweep_ = num_tasks_;
  forward_tasks_to_sweep_.resize(num_tasks_);
  backward_num_tasks_to_sweep_ = num_tasks_;
  backward_tasks_to_sweep_.resize(num_tasks_);

  num_profile_tasks_ = 0;
  profile_tasks_.resize(num_tasks_);
  positions_in_profile_tasks_.resize(num_tasks_);

  // Reversible bounds and starting height of the profile.
  starting_profile_height_ = IntegerValue(0);

  for (int t = 0; t < num_tasks_; ++t) {
    forward_tasks_to_sweep_[t] = t;
    backward_tasks_to_sweep_[t] = t;
    profile_tasks_[t] = t;
    positions_in_profile_tasks_[t] = t;
  }
}

void TimeTablingPerTask::RegisterWith(GenericLiteralWatcher *watcher) {
  const int id = watcher->Register(this);
  helper_->WatchAllTasks(id, watcher);
  watcher->WatchUpperBound(capacity_.var, id);
  for (int t = 0; t < num_tasks_; t++) {
    watcher->WatchLowerBound(demands_[t].var, id);
  }
  watcher->RegisterReversibleInt(id, &forward_num_tasks_to_sweep_);
  watcher->RegisterReversibleInt(id, &backward_num_tasks_to_sweep_);
  watcher->RegisterReversibleInt(id, &num_profile_tasks_);
}

bool TimeTablingPerTask::Propagate() {
  // Repeat until the propagator does not filter anymore.
  profile_changed_ = true;
  while (profile_changed_) {
    profile_changed_ = false;
    // This can fail if the profile exceeds the resource capacity.
    if (!BuildProfile()) return false;
    // Update the minimum start times.
    if (!SweepAllTasks(/*is_forward=*/true)) return false;
    // We reuse the same profile, but reversed, to update the maximum end times.
    ReverseProfile();
    // Update the maximum end times (reversed problem).
    if (!SweepAllTasks(/*is_forward=*/false)) return false;
  }

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

  const auto &by_decreasing_start_max = helper_->TaskByDecreasingStartMax();
  const auto &by_end_min = helper_->TaskByIncreasingEndMin();

  // Build the profile.
  // ------------------
  profile_.clear();

  // Start and height of the highest profile rectangle.
  profile_max_height_ = kMinIntegerValue;
  IntegerValue max_height_start = kMinIntegerValue;

  // Add a sentinel to simplify the algorithm.
  profile_.emplace_back(kMinIntegerValue, IntegerValue(0));

  // Start and height of the currently built profile rectange.
  IntegerValue current_start = kMinIntegerValue;
  IntegerValue current_height = starting_profile_height_;

  // Next start/end of the compulsory parts to be processed. Note that only the
  // task for which IsInProfile() is true must be considered.
  int next_start = num_tasks_ - 1;
  int next_end = 0;
  while (next_end < num_tasks_) {
    const IntegerValue old_height = current_height;

    IntegerValue t = by_end_min[next_end].time;
    if (next_start >= 0) {
      t = std::min(t, by_decreasing_start_max[next_start].time);
    }

    // Process the starting compulsory parts.
    while (next_start >= 0 && by_decreasing_start_max[next_start].time == t) {
      const int task_index = by_decreasing_start_max[next_start].task_index;
      if (IsInProfile(task_index)) current_height += DemandMin(task_index);
      --next_start;
    }

    // Process the ending compulsory parts.
    while (next_end < num_tasks_ && by_end_min[next_end].time == t) {
      const int task_index = by_end_min[next_end].task_index;
      if (IsInProfile(task_index)) current_height -= DemandMin(task_index);
      ++next_end;
    }

    // Insert a new profile rectangle if any.
    if (current_height != old_height) {
      profile_.emplace_back(current_start, old_height);
      if (current_height > profile_max_height_) {
        profile_max_height_ = current_height;
        max_height_start = t;
      }
      current_start = t;
    }
  }

  // Build the last profile rectangle.
  DCHECK_GE(current_height, 0);
  profile_.emplace_back(current_start, IntegerValue(0));

  // Add a sentinel to simplify the algorithm.
  profile_.emplace_back(kMaxIntegerValue, IntegerValue(0));

  // Increase the capacity variable if required.
  return IncreaseCapacity(max_height_start, profile_max_height_);
}

void TimeTablingPerTask::ReverseProfile() {
  helper_->SetTimeDirection(false);  // backward

  // We keep the sentinels inchanged.
  for (int i = 1; i + 1 < profile_.size(); ++i) {
    profile_[i].start = -profile_[i + 1].start;
  }
  std::reverse(profile_.begin() + 1, profile_.end() - 1);
}

bool TimeTablingPerTask::SweepAllTasks(bool is_forward) {
  // Tasks with a lower or equal demand will not be pushed.
  const IntegerValue demand_threshold(
      CapSub(CapacityMax().value(), profile_max_height_.value()));

  // Select the correct members depending on the direction.
  int &num_tasks =
      is_forward ? forward_num_tasks_to_sweep_ : backward_num_tasks_to_sweep_;
  std::vector<int> &tasks =
      is_forward ? forward_tasks_to_sweep_ : backward_tasks_to_sweep_;

  // TODO(user): On some problem, a big chunk of the time is spend just checking
  // these conditions below because it requires indirect memory access to fetch
  // the demand/size/presence/start ...
  for (int i = num_tasks - 1; i >= 0; --i) {
    const int t = tasks[i];
    if (helper_->IsAbsent(t) ||
        (helper_->IsPresent(t) && helper_->StartIsFixed(t))) {
      // This tasks does not have to be considered for propagation in the rest
      // of the sub-tree. Note that StartIsFixed() depends on the time
      // direction, it is why we use two lists.
      std::swap(tasks[i], tasks[--num_tasks]);
      continue;
    }

    // Skip if demand is too low.
    if (DemandMin(t) <= demand_threshold) {
      if (DemandMax(t) == 0) {
        // We can ignore this task for the rest of the subtree like above.
        std::swap(tasks[i], tasks[--num_tasks]);
      }

      // This task does not have to be considered for propagation in this
      // particular iteration, but maybe it does later.
      continue;
    }

    // Skip if size is zero.
    if (helper_->SizeMin(t) == 0) {
      if (helper_->SizeMax(t) == 0) {
        std::swap(tasks[i], tasks[--num_tasks]);
      }
      continue;
    }

    if (!SweepTask(t)) return false;
  }

  return true;
}

bool TimeTablingPerTask::SweepTask(int task_id) {
  const IntegerValue start_max = helper_->StartMax(task_id);
  const IntegerValue size_min = helper_->SizeMin(task_id);
  const IntegerValue initial_start_min = helper_->StartMin(task_id);
  const IntegerValue initial_end_min = helper_->EndMin(task_id);

  IntegerValue new_start_min = initial_start_min;
  IntegerValue new_end_min = initial_end_min;

  // Find the profile rectangle that overlaps the minimum start time of task_id.
  // The sentinel prevents out of bound exceptions.
  DCHECK(std::is_sorted(profile_.begin(), profile_.end()));
  int rec_id =
      std::upper_bound(profile_.begin(), profile_.end(), new_start_min,
                       [&](IntegerValue value, const ProfileRectangle &rect) {
                         return value < rect.start;
                       }) -
      profile_.begin();
  --rec_id;

  // A profile rectangle is in conflict with the task if its height exceeds
  // conflict_height.
  const IntegerValue conflict_height = CapacityMax() - DemandMin(task_id);

  // True if the task is in conflict with at least one profile rectangle.
  bool conflict_found = false;

  // Last time point during which task_id was in conflict with a profile
  // rectangle before being pushed.
  IntegerValue last_initial_conflict = kMinIntegerValue;

  // Push the task from left to right until it does not overlap any conflicting
  // rectangle. Pushing the task may push the end of its compulsory part on the
  // right but will not change its start. The main loop of the propagator will
  // take care of rebuilding the profile with these possible changes and to
  // propagate again in order to reach the timetabling consistency or to fail if
  // the profile exceeds the resource capacity.
  IntegerValue limit = std::min(start_max, new_end_min);
  for (; profile_[rec_id].start < limit; ++rec_id) {
    // If the profile rectangle is not conflicting, go to the next rectangle.
    if (profile_[rec_id].height <= conflict_height) continue;

    conflict_found = true;

    // Compute the next minimum start and end times of task_id. The variables
    // are not updated yet.
    new_start_min = profile_[rec_id + 1].start;  // i.e. profile_[rec_id].end
    if (start_max < new_start_min) {
      if (IsInProfile(task_id)) {
        // Because the task is part of the profile, we cannot push it further.
        new_start_min = start_max;
      } else {
        // We have a conflict or we can push the task absence. In both cases
        // we don't need more than start_max + 1 in the explanation below.
        new_start_min = start_max + 1;
      }
    }

    new_end_min = std::max(new_end_min, new_start_min + size_min);
    limit = std::min(start_max, new_end_min);

    if (profile_[rec_id].start < initial_end_min) {
      last_initial_conflict = std::min(new_start_min, initial_end_min) - 1;
    }
  }

  if (!conflict_found) return true;

  if (initial_start_min != new_start_min &&
      !UpdateStartingTime(task_id, last_initial_conflict, new_start_min)) {
    return false;
  }

  // The profile needs to be recomputed if we pushed something (because it can
  // have side effects). Note that for the case where the interval is optional
  // but not its start, it is possible that UpdateStartingTime() didn't change
  // the start, so we need to test this in order to avoid an infinite loop.
  //
  // TODO(user): find an efficient way to keep the start_max < new_end_min
  // condition. The problem is that ReduceProfile() assumes that by_end_min and
  // by_start_max are up to date (this is not necessarily the case if we use
  // the old condition). A solution is to update those vector before calling
  // ReduceProfile() or to ReduceProfile() directly after BuildProfile() in the
  // main loop.
  if (helper_->StartMin(task_id) != initial_start_min) {
    profile_changed_ = true;
  }

  return true;
}

bool TimeTablingPerTask::UpdateStartingTime(int task_id, IntegerValue left,
                                            IntegerValue right) {
  helper_->ClearReason();

  AddProfileReason(left, right);
  if (capacity_.var != kNoIntegerVariable) {
    helper_->MutableIntegerReason()->push_back(
        integer_trail_->UpperBoundAsLiteral(capacity_.var));
  }

  // State of the task to be pushed.
  helper_->AddEndMinReason(task_id, left + 1);
  helper_->AddSizeMinReason(task_id, IntegerValue(1));
  if (demands_[task_id].var != kNoIntegerVariable) {
    helper_->MutableIntegerReason()->push_back(
        integer_trail_->LowerBoundAsLiteral(demands_[task_id].var));
  }

  // Explain the increase of the minimum start and end times.
  return helper_->IncreaseStartMin(task_id, right);
}

void TimeTablingPerTask::AddProfileReason(IntegerValue left,
                                          IntegerValue right) {
  for (int i = 0; i < num_profile_tasks_; ++i) {
    const int t = profile_tasks_[i];

    // Do not consider the task if it does not overlap for sure (left, right).
    const IntegerValue start_max = helper_->StartMax(t);
    if (right <= start_max) continue;
    const IntegerValue end_min = helper_->EndMin(t);
    if (end_min <= left) continue;

    helper_->AddPresenceReason(t);
    helper_->AddStartMaxReason(t, std::max(left, start_max));
    helper_->AddEndMinReason(t, std::min(right, end_min));
    if (demands_[t].var != kNoIntegerVariable) {
      helper_->MutableIntegerReason()->push_back(
          integer_trail_->LowerBoundAsLiteral(demands_[t].var));
    }
  }
}

bool TimeTablingPerTask::IncreaseCapacity(IntegerValue time,
                                          IntegerValue new_min) {
  if (new_min <= CapacityMin()) return true;

  helper_->ClearReason();
  AddProfileReason(time, time + 1);
  if (capacity_.var == kNoIntegerVariable) {
    return helper_->ReportConflict();
  }

  helper_->MutableIntegerReason()->push_back(
      integer_trail_->UpperBoundAsLiteral(capacity_.var));
  return helper_->PushIntegerLiteral(capacity_.GreaterOrEqual(new_min));
}

}  // namespace sat
}  // namespace operations_research
