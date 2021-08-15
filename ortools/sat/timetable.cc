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

#include "ortools/sat/timetable.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>

#include "ortools/base/int_type.h"
#include "ortools/base/logging.h"
#include "ortools/util/sort.h"

namespace operations_research {
namespace sat {

void AddReservoirConstraint(std::vector<AffineExpression> times,
                            std::vector<IntegerValue> deltas,
                            std::vector<Literal> presences, int64_t min_level,
                            int64_t max_level, Model* model) {
  // We only create a side if it can fail.
  IntegerValue min_possible(0);
  IntegerValue max_possible(0);
  for (const IntegerValue d : deltas) {
    if (d > 0) {
      max_possible += d;
    } else {
      min_possible += d;
    }
  }
  if (max_possible > max_level) {
    model->TakeOwnership(new ReservoirTimeTabling(
        times, deltas, presences, IntegerValue(max_level), model));
  }
  if (min_possible < min_level) {
    for (IntegerValue& ref : deltas) ref = -ref;
    model->TakeOwnership(new ReservoirTimeTabling(
        times, deltas, presences, IntegerValue(-min_level), model));
  }
}

ReservoirTimeTabling::ReservoirTimeTabling(
    const std::vector<AffineExpression>& times,
    const std::vector<IntegerValue>& deltas,
    const std::vector<Literal>& presences, IntegerValue capacity, Model* model)
    : times_(times),
      deltas_(deltas),
      presences_(presences),
      capacity_(capacity),
      assignment_(model->GetOrCreate<Trail>()->Assignment()),
      integer_trail_(model->GetOrCreate<IntegerTrail>()) {
  auto* watcher = model->GetOrCreate<GenericLiteralWatcher>();
  const int id = watcher->Register(this);
  const int num_events = times.size();
  for (int e = 0; e < num_events; e++) {
    if (deltas_[e] > 0) {
      watcher->WatchUpperBound(times_[e].var, id);
      watcher->WatchLiteral(presences_[e], id);
    }
    if (deltas_[e] < 0) {
      watcher->WatchLowerBound(times_[e].var, id);
      watcher->WatchLiteral(presences_[e].Negated(), id);
    }
  }
  watcher->NotifyThatPropagatorMayNotReachFixedPointInOnePass(id);
}

bool ReservoirTimeTabling::Propagate() {
  const int num_events = times_.size();
  if (!BuildProfile()) return false;
  for (int e = 0; e < num_events; e++) {
    if (assignment_.LiteralIsFalse(presences_[e])) continue;

    // For positive delta, we can maybe increase the min.
    if (deltas_[e] > 0 && !TryToIncreaseMin(e)) return false;

    // For negative delta, we can maybe decrease the max.
    if (deltas_[e] < 0 && !TryToDecreaseMax(e)) return false;
  }
  return true;
}

// We compute the lowest possible profile at time t.
//
// TODO(user): If we have precedences between events, we should be able to do
// more.
bool ReservoirTimeTabling::BuildProfile() {
  // Starts by copying the "events" in the profile and sort them by time.
  profile_.clear();
  const int num_events = times_.size();
  profile_.emplace_back(kMinIntegerValue, IntegerValue(0));  // Sentinel.
  for (int e = 0; e < num_events; e++) {
    if (deltas_[e] > 0) {
      // Only consider present event for positive delta.
      if (!assignment_.LiteralIsTrue(presences_[e])) continue;
      const IntegerValue ub = integer_trail_->UpperBound(times_[e]);
      profile_.push_back({ub, deltas_[e]});
    } else if (deltas_[e] < 0) {
      // Only consider non-absent event for negative delta.
      if (assignment_.LiteralIsFalse(presences_[e])) continue;
      profile_.push_back({integer_trail_->LowerBound(times_[e]), deltas_[e]});
    }
  }
  profile_.emplace_back(kMaxIntegerValue, IntegerValue(0));  // Sentinel.
  std::sort(profile_.begin(), profile_.end());

  // Accumulate delta and collapse entries.
  int last = 0;
  for (const ProfileRectangle& rect : profile_) {
    if (rect.start == profile_[last].start) {
      profile_[last].height += rect.height;
    } else {
      ++last;
      profile_[last].start = rect.start;
      profile_[last].height = rect.height + profile_[last - 1].height;
    }
  }
  profile_.resize(last + 1);

  // Conflict?
  for (const ProfileRectangle& rect : profile_) {
    if (rect.height <= capacity_) continue;
    FillReasonForProfileAtGivenTime(rect.start);
    return integer_trail_->ReportConflict(literal_reason_, integer_reason_);
  }

  return true;
}

// TODO(user): Minimize with how high the profile needs to be. We can also
// remove from the reason the absence of a negative event provided that the
// level zero min of the event is greater than t anyway.
//
// TODO(user): Make sure the code work with fixed time since pushing always
// true/false literal to the reason is not completely supported.
void ReservoirTimeTabling::FillReasonForProfileAtGivenTime(
    IntegerValue t, int event_to_ignore) {
  integer_reason_.clear();
  literal_reason_.clear();
  const int num_events = times_.size();
  for (int e = 0; e < num_events; e++) {
    if (e == event_to_ignore) continue;
    if (deltas_[e] > 0) {
      if (!assignment_.LiteralIsTrue(presences_[e])) continue;
      if (integer_trail_->UpperBound(times_[e]) > t) continue;
      integer_reason_.push_back(times_[e].LowerOrEqual(t));
      literal_reason_.push_back(presences_[e].Negated());
    } else if (deltas_[e] < 0) {
      if (assignment_.LiteralIsFalse(presences_[e])) {
        literal_reason_.push_back(presences_[e]);
      } else if (integer_trail_->LowerBound(times_[e]) > t) {
        integer_reason_.push_back(times_[e].GreaterOrEqual(t + 1));
      }
    }
  }
}

// Note that a negative event will always be in the profile, even if its
// presence is still not settled.
bool ReservoirTimeTabling::TryToDecreaseMax(int event) {
  CHECK_LT(deltas_[event], 0);
  const IntegerValue start = integer_trail_->LowerBound(times_[event]);
  const IntegerValue end = integer_trail_->UpperBound(times_[event]);

  // We already tested for conflict in BuildProfile().
  if (start == end) return true;

  // Find the profile rectangle that overlaps the start of the given event.
  // The sentinel prevents out of bound exceptions.
  DCHECK(std::is_sorted(profile_.begin(), profile_.end()));
  int rec_id =
      std::upper_bound(profile_.begin(), profile_.end(), start,
                       [&](IntegerValue value, const ProfileRectangle& rect) {
                         return value < rect.start;
                       }) -
      profile_.begin();
  --rec_id;

  bool push = false;
  IntegerValue new_end = end;
  for (; profile_[rec_id].start < end; ++rec_id) {
    if (profile_[rec_id].height - deltas_[event] > capacity_) {
      new_end = profile_[rec_id].start;
      push = true;
      break;
    }
  }
  if (!push) return true;

  // The reason is simply why the capacity at new_end (without the event)
  // would overflow.
  FillReasonForProfileAtGivenTime(new_end, event);

  // Note(user): I don't think this is possible since it would have been
  // detected at profile construction, but then, since the bound might have been
  // updated, better be defensive.
  if (new_end < start) {
    integer_reason_.push_back(times_[event].GreaterOrEqual(new_end + 1));
    return integer_trail_->ReportConflict(literal_reason_, integer_reason_);
  }

  // First, the task MUST be present, otherwise we have a conflict.
  //
  // TODO(user): We actually need to look after 'end' to potentially push the
  // presence in more situation.
  if (!assignment_.LiteralIsTrue(presences_[event])) {
    integer_trail_->EnqueueLiteral(presences_[event], literal_reason_,
                                   integer_reason_);
  }

  // Push new_end too. Note that we don't need the presence reason.
  return integer_trail_->Enqueue(times_[event].LowerOrEqual(new_end),
                                 literal_reason_, integer_reason_);
}

bool ReservoirTimeTabling::TryToIncreaseMin(int event) {
  CHECK_GT(deltas_[event], 0);
  const IntegerValue start = integer_trail_->LowerBound(times_[event]);
  const IntegerValue end = integer_trail_->UpperBound(times_[event]);

  // We already tested for conflict in BuildProfile().
  if (start == end) return true;

  // Find the profile rectangle containing the end of the given event.
  // The sentinel prevents out of bound exceptions.
  //
  // TODO(user): If the task is no present, we should actually look at the
  // maximum profile after end to maybe push its absence.
  DCHECK(std::is_sorted(profile_.begin(), profile_.end()));
  int rec_id =
      std::upper_bound(profile_.begin(), profile_.end(), end,
                       [&](IntegerValue value, const ProfileRectangle& rect) {
                         return value < rect.start;
                       }) -
      profile_.begin();
  --rec_id;

  bool push = false;
  IntegerValue new_start = start;
  if (profile_[rec_id].height + deltas_[event] > capacity_) {
    if (!assignment_.LiteralIsTrue(presences_[event])) {
      // Push to false since it wasn't part of the profile and cannot fit.
      push = true;
      new_start = end + 1;
    } else if (profile_[rec_id].start < end) {
      // It must be at end in this case.
      push = true;
      new_start = end;
    }
  }
  if (!push) {
    for (; profile_[rec_id].start > start; --rec_id) {
      if (profile_[rec_id - 1].height + deltas_[event] > capacity_) {
        push = true;
        new_start = profile_[rec_id].start;
        break;
      }
    }
  }
  if (!push) return true;

  // The reason is simply the capacity at new_start - 1;
  FillReasonForProfileAtGivenTime(new_start - 1, event);
  return integer_trail_->ConditionalEnqueue(
      presences_[event], times_[event].GreaterOrEqual(new_start),
      &literal_reason_, &integer_reason_);
}

TimeTablingPerTask::TimeTablingPerTask(
    const std::vector<AffineExpression>& demands, AffineExpression capacity,
    IntegerTrail* integer_trail, SchedulingConstraintHelper* helper)
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

void TimeTablingPerTask::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  helper_->WatchAllTasks(id, watcher);
  watcher->WatchUpperBound(capacity_.var, id);
  for (int t = 0; t < num_tasks_; t++) {
    watcher->WatchLowerBound(demands_[t].var, id);
  }
  watcher->RegisterReversibleInt(id, &forward_num_tasks_to_sweep_);
  watcher->RegisterReversibleInt(id, &backward_num_tasks_to_sweep_);
  watcher->RegisterReversibleInt(id, &num_profile_tasks_);

  // Changing the times or pushing task absence migth have side effects on the
  // other intervals, so we would need to be called again in this case.
  watcher->NotifyThatPropagatorMayNotReachFixedPointInOnePass(id);
}

// Note that we relly on being called again to reach a fixed point.
bool TimeTablingPerTask::Propagate() {
  // This can fail if the profile exceeds the resource capacity.
  if (!BuildProfile()) return false;

  // Update the minimum start times.
  if (!SweepAllTasks(/*is_forward=*/true)) return false;

  // We reuse the same profile, but reversed, to update the maximum end times.
  if (!helper_->SynchronizeAndSetTimeDirection(false)) return false;
  ReverseProfile();

  // Update the maximum end times (reversed problem).
  if (!SweepAllTasks(/*is_forward=*/false)) return false;

  return true;
}

bool TimeTablingPerTask::BuildProfile() {
  if (!helper_->SynchronizeAndSetTimeDirection(true)) return false;

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

  const auto& by_decreasing_start_max = helper_->TaskByDecreasingStartMax();
  const auto& by_end_min = helper_->TaskByIncreasingEndMin();

  // Build the profile.
  // ------------------
  profile_.clear();

  // Start and height of the highest profile rectangle.
  profile_max_height_ = kMinIntegerValue;
  IntegerValue max_height_start = kMinIntegerValue;

  // Add a sentinel to simplify the algorithm.
  profile_.emplace_back(kMinIntegerValue, IntegerValue(0));

  // Start and height of the currently built profile rectangle.
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
  int& num_tasks =
      is_forward ? forward_num_tasks_to_sweep_ : backward_num_tasks_to_sweep_;
  std::vector<int>& tasks =
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
                       [&](IntegerValue value, const ProfileRectangle& rect) {
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
