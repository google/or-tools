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

#include "ortools/sat/timetable.h"

#include <algorithm>
#include <cstdint>
#include <vector>

#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {

void AddReservoirConstraint(std::vector<AffineExpression> times,
                            std::vector<AffineExpression> deltas,
                            std::vector<Literal> presences, int64_t min_level,
                            int64_t max_level, Model* model) {
  // We only create a side if it can fail.
  IntegerValue min_possible(0);
  IntegerValue max_possible(0);
  auto* integer_trail = model->GetOrCreate<IntegerTrail>();
  for (const AffineExpression d : deltas) {
    min_possible += std::min(IntegerValue(0), integer_trail->LowerBound(d));
    max_possible += std::max(IntegerValue(0), integer_trail->UpperBound(d));
  }
  if (max_possible > max_level) {
    model->TakeOwnership(new ReservoirTimeTabling(
        times, deltas, presences, IntegerValue(max_level), model));
  }
  if (min_possible < min_level) {
    for (AffineExpression& ref : deltas) ref = ref.Negated();
    model->TakeOwnership(new ReservoirTimeTabling(
        times, deltas, presences, IntegerValue(-min_level), model));
  }
}

ReservoirTimeTabling::ReservoirTimeTabling(
    const std::vector<AffineExpression>& times,
    const std::vector<AffineExpression>& deltas,
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
    watcher->WatchLowerBound(deltas_[e], id);
    if (integer_trail_->UpperBound(deltas_[e]) > 0) {
      watcher->WatchUpperBound(times_[e].var, id);
      watcher->WatchLiteral(presences_[e], id);
    }
    if (integer_trail_->LowerBound(deltas_[e]) < 0) {
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

    // For positive delta_min, we can maybe increase the min.
    const IntegerValue min_d = integer_trail_->LowerBound(deltas_[e]);
    if (min_d > 0 && !TryToIncreaseMin(e)) return false;

    // For negative delta_min, we can maybe decrease the max.
    if (min_d < 0 && !TryToDecreaseMax(e)) return false;
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
    const IntegerValue min_d = integer_trail_->LowerBound(deltas_[e]);
    if (min_d > 0) {
      // Only consider present event for positive delta.
      if (!assignment_.LiteralIsTrue(presences_[e])) continue;
      const IntegerValue ub = integer_trail_->UpperBound(times_[e]);
      profile_.push_back({ub, min_d});
    } else if (min_d < 0) {
      // Only consider non-absent event for negative delta.
      if (assignment_.LiteralIsFalse(presences_[e])) continue;
      profile_.push_back({integer_trail_->LowerBound(times_[e]), min_d});
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

namespace {

void AddLowerOrEqual(const AffineExpression& expr, IntegerValue bound,
                     std::vector<IntegerLiteral>* reason) {
  if (expr.IsConstant()) return;
  reason->push_back(expr.LowerOrEqual(bound));
}

void AddGreaterOrEqual(const AffineExpression& expr, IntegerValue bound,
                       std::vector<IntegerLiteral>* reason) {
  if (expr.IsConstant()) return;
  reason->push_back(expr.GreaterOrEqual(bound));
}

}  // namespace

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
    const IntegerValue min_d = integer_trail_->LowerBound(deltas_[e]);
    if (min_d > 0) {
      if (!assignment_.LiteralIsTrue(presences_[e])) continue;
      if (integer_trail_->UpperBound(times_[e]) > t) continue;
      AddGreaterOrEqual(deltas_[e], min_d, &integer_reason_);
      AddLowerOrEqual(times_[e], t, &integer_reason_);
      literal_reason_.push_back(presences_[e].Negated());
    } else if (min_d <= 0) {
      if (assignment_.LiteralIsFalse(presences_[e])) {
        literal_reason_.push_back(presences_[e]);
        continue;
      }
      AddGreaterOrEqual(deltas_[e], min_d, &integer_reason_);
      if (min_d < 0 && integer_trail_->LowerBound(times_[e]) > t) {
        AddGreaterOrEqual(times_[e], t + 1, &integer_reason_);
      }
    }
  }
}

// Note that a negative event will always be in the profile, even if its
// presence is still not settled.
bool ReservoirTimeTabling::TryToDecreaseMax(int event) {
  const IntegerValue min_d = integer_trail_->LowerBound(deltas_[event]);
  CHECK_LT(min_d, 0);
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
    if (profile_[rec_id].height - min_d > capacity_) {
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
    AddGreaterOrEqual(times_[event], new_end + 1, &integer_reason_);
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
  const IntegerValue min_d = integer_trail_->LowerBound(deltas_[event]);
  CHECK_GT(min_d, 0);
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
  if (profile_[rec_id].height + min_d > capacity_) {
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
      if (profile_[rec_id - 1].height + min_d > capacity_) {
        push = true;
        new_start = profile_[rec_id].start;
        break;
      }
    }
  }
  if (!push) return true;

  // The reason is simply the capacity at new_start - 1;
  FillReasonForProfileAtGivenTime(new_start - 1, event);
  AddGreaterOrEqual(deltas_[event], min_d, &integer_reason_);
  return integer_trail_->ConditionalEnqueue(
      presences_[event], times_[event].GreaterOrEqual(new_start),
      &literal_reason_, &integer_reason_);
}

TimeTablingPerTask::TimeTablingPerTask(AffineExpression capacity,
                                       SchedulingConstraintHelper* helper,
                                       SchedulingDemandHelper* demands,
                                       Model* model)
    : num_tasks_(helper->NumTasks()),
      capacity_(capacity),
      helper_(helper),
      demands_(demands),
      integer_trail_(model->GetOrCreate<IntegerTrail>()) {
  // Each task may create at most two profile rectangles. Such pattern appear if
  // the profile is shaped like the Hanoi tower. The additional space is for
  // both extremities and the sentinels.
  profile_.reserve(2 * num_tasks_ + 4);

  num_profile_tasks_ = 0;
  profile_tasks_.resize(num_tasks_);

  initial_max_demand_ = IntegerValue(0);
  const bool capa_is_fixed = integer_trail_->IsFixed(capacity_);
  const IntegerValue capa_min = integer_trail_->LowerBound(capacity_);
  for (int t = 0; t < num_tasks_; ++t) {
    profile_tasks_[t] = t;

    if (capa_is_fixed && demands_->DemandMin(t) >= capa_min) {
      // TODO(user): This usually correspond to a makespan interval.
      // We should just detect and propagate it separately as it would result
      // in a faster propagation.
      has_demand_equal_to_capacity_ = true;
      continue;
    }
    initial_max_demand_ = std::max(initial_max_demand_, demands_->DemandMax(t));
  }
}

void TimeTablingPerTask::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  helper_->WatchAllTasks(id, watcher);
  watcher->WatchUpperBound(capacity_.var, id);
  for (int t = 0; t < num_tasks_; t++) {
    watcher->WatchLowerBound(demands_->Demands()[t], id);
  }
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
  if (!SweepAllTasks()) return false;

  // We reuse the same profile, but reversed, to update the maximum end times.
  if (!helper_->SynchronizeAndSetTimeDirection(false)) return false;
  ReverseProfile();

  // Update the maximum end times (reversed problem).
  if (!SweepAllTasks()) return false;

  return true;
}

bool TimeTablingPerTask::BuildProfile() {
  if (!helper_->SynchronizeAndSetTimeDirection(true)) return false;

  // Update the set of tasks that contribute to the profile. Tasks that were
  // contributing are still part of the profile so we only need to check the
  // other tasks.
  for (int i = num_profile_tasks_; i < num_tasks_; ++i) {
    const int t = profile_tasks_[i];
    if (helper_->IsPresent(t) && helper_->StartMax(t) < helper_->EndMin(t)) {
      std::swap(profile_tasks_[i], profile_tasks_[num_profile_tasks_]);
      num_profile_tasks_++;
    }
  }

  // Cache the demand min. If not in profile, it will be zero.
  cached_demands_min_.assign(num_tasks_, 0);
  absl::Span<IntegerValue> demands_min = absl::MakeSpan(cached_demands_min_);
  for (int i = 0; i < num_profile_tasks_; ++i) {
    const int t = profile_tasks_[i];
    demands_min[t] = demands_->DemandMin(t);
  }

  // Build the profile.
  // ------------------
  profile_.clear();

  // Start and height of the highest profile rectangle.
  IntegerValue max_height = 0;
  IntegerValue max_height_start = kMinIntegerValue;

  // Start and height of the currently built profile rectangle.
  IntegerValue current_start = kMinIntegerValue;
  IntegerValue height_at_start = IntegerValue(0);
  IntegerValue current_height = IntegerValue(0);

  // Any profile height <= relevant_height is not really relevant since nothing
  // can be pushed. So we artificially put zero or one (if there is a makespan
  // interval) in the profile instead. This allow to have a lot less
  // "rectangles" in a profile for exactly the same propagation!
  const IntegerValue relevant_height =
      integer_trail_->UpperBound(capacity_) - initial_max_demand_;
  const IntegerValue default_non_relevant_height =
      has_demand_equal_to_capacity_ ? 1 : 0;

  const auto& by_negated_start_max = helper_->TaskByIncreasingNegatedStartMax();
  const auto& by_end_min = helper_->TaskByIncreasingEndMin();

  // Next start/end of the compulsory parts to be processed. Note that only the
  // task for which IsInProfile() is true must be considered.
  int next_start = num_tasks_ - 1;
  int next_end = 0;
  const int num_tasks = num_tasks_;
  while (next_end < num_tasks) {
    IntegerValue time = by_end_min[next_end].time;
    if (next_start >= 0) {
      time = std::min(time, -by_negated_start_max[next_start].time);
    }

    // Process the starting compulsory parts.
    while (next_start >= 0 && -by_negated_start_max[next_start].time == time) {
      const int t = by_negated_start_max[next_start].task_index;
      current_height += demands_min[t];
      --next_start;
    }

    // Process the ending compulsory parts.
    while (next_end < num_tasks && by_end_min[next_end].time == time) {
      const int t = by_end_min[next_end].task_index;
      current_height -= demands_min[t];
      ++next_end;
    }

    if (current_height > max_height) {
      max_height = current_height;
      max_height_start = time;
    }

    IntegerValue effective_height = current_height;
    if (effective_height > 0 && effective_height <= relevant_height) {
      effective_height = default_non_relevant_height;
    }

    // Insert a new profile rectangle if any.
    if (effective_height != height_at_start) {
      profile_.emplace_back(current_start, height_at_start);
      current_start = time;
      height_at_start = effective_height;
    }
  }

  // Build the last profile rectangle.
  DCHECK_GE(current_height, 0);
  profile_.emplace_back(current_start, IntegerValue(0));

  // Add a sentinel to simplify the algorithm.
  profile_.emplace_back(kMaxIntegerValue, IntegerValue(0));

  // Save max_height.
  profile_max_height_ = max_height;

  // Increase the capacity variable if required.
  return IncreaseCapacity(max_height_start, profile_max_height_);
}

void TimeTablingPerTask::ReverseProfile() {
  // We keep the sentinels inchanged.
  std::reverse(profile_.begin() + 1, profile_.end() - 1);
  for (int i = 1; i + 1 < profile_.size(); ++i) {
    profile_[i].start = -profile_[i].start;
    profile_[i].height = profile_[i + 1].height;
  }
}

bool TimeTablingPerTask::SweepAllTasks() {
  // We can start at one since the first sentinel can always be skipped.
  int profile_index = 1;
  const IntegerValue capa_max = CapacityMax();
  for (const auto& [t, time] : helper_->TaskByIncreasingStartMin()) {
    // TODO(user): On some problem, a big chunk of the time is spend just
    // checking these conditions below because it requires indirect memory
    // access to fetch the demand/size/presence/start ...
    if (helper_->IsAbsent(t)) continue;
    if (helper_->SizeMin(t) == 0) continue;

    // A profile rectangle is in conflict with the task if its height exceeds
    // conflict_height.
    const IntegerValue conflict_height = capa_max - demands_->DemandMin(t);

    // TODO(user): This is never true when we have a makespan interval with
    // demand equal to the capacity. Find a simple way to detect when there is
    // no need to scan a task.
    if (conflict_height >= profile_max_height_) continue;

    if (!SweepTask(t, time, conflict_height, &profile_index)) return false;
  }

  return true;
}

bool TimeTablingPerTask::SweepTask(int task_id, IntegerValue initial_start_min,
                                   IntegerValue conflict_height,
                                   int* profile_index) {
  const IntegerValue start_max = helper_->StartMax(task_id);
  const IntegerValue initial_end_min = helper_->EndMin(task_id);

  // Find the profile rectangle that overlaps the minimum start time of task_id.
  // The sentinel prevents out of bound exceptions.
  DCHECK(std::is_sorted(profile_.begin(), profile_.end()));
  while (profile_[*profile_index].start <= initial_start_min) {
    ++*profile_index;
  }
  int rec_id = *profile_index - 1;

  // Last time point during which task_id was in conflict with a profile
  // rectangle before being pushed.
  IntegerValue explanation_start_time = kMinIntegerValue;

  // Push the task from left to right until it does not overlap any conflicting
  // rectangle. Pushing the task may push the end of its compulsory part on the
  // right but will not change its start. The main loop of the propagator will
  // take care of rebuilding the profile with these possible changes and to
  // propagate again in order to reach the timetabling consistency or to fail if
  // the profile exceeds the resource capacity.
  //
  // For optimization purpose we have a separate code if the task is in the
  // profile or not.
  IntegerValue new_start_min = initial_start_min;
  if (IsInProfile(task_id)) {
    DCHECK_LE(start_max, initial_end_min);
    for (; profile_[rec_id].start < start_max; ++rec_id) {
      // If the profile rectangle is not conflicting, go to the next rectangle.
      if (profile_[rec_id].height <= conflict_height) continue;

      // Compute the next minimum start and end times of task_id. The variables
      // are not updated yet.
      new_start_min = profile_[rec_id + 1].start;  // i.e. profile_[rec_id].end
      if (start_max < new_start_min) {
        // Because the task is part of the profile, we cannot push it further.
        new_start_min = start_max;
        explanation_start_time = start_max - 1;
        break;
      }
      explanation_start_time = new_start_min - 1;
    }

    // Since the task is part of the profile, try to lower its demand max
    // if possible.
    const IntegerValue delta =
        demands_->DemandMax(task_id) - demands_->DemandMin(task_id);
    if (delta > 0) {
      const IntegerValue threshold = CapacityMax() - delta;
      if (profile_[rec_id].start > start_max) --rec_id;
      for (; profile_[rec_id].start < initial_end_min; ++rec_id) {
        DCHECK_GT(profile_[rec_id + 1].start, start_max);
        if (profile_[rec_id].height <= threshold) continue;
        const IntegerValue new_max = CapacityMax() - profile_[rec_id].height +
                                     demands_->DemandMin(task_id);

        // Note that the task_id is already part of the profile reason, so
        // there is nothing else needed.
        helper_->ClearReason();
        const IntegerValue time = std::max(start_max, profile_[rec_id].start);
        AddProfileReason(task_id, time, time + 1, CapacityMax());
        if (!helper_->PushIntegerLiteralIfTaskPresent(
                task_id, demands_->Demands()[task_id].LowerOrEqual(new_max))) {
          return false;
        }
      }
    }
  } else {
    IntegerValue limit = initial_end_min;
    const IntegerValue size_min = helper_->SizeMin(task_id);
    for (; profile_[rec_id].start < limit; ++rec_id) {
      // If the profile rectangle is not conflicting, go to the next rectangle.
      if (profile_[rec_id].height <= conflict_height) continue;

      // Compute the next minimum start and end times of task_id. The variables
      // are not updated yet.
      new_start_min = profile_[rec_id + 1].start;  // i.e. profile_[rec_id].end
      limit = std::max(limit, new_start_min + size_min);
      if (profile_[rec_id].start < initial_end_min) {
        explanation_start_time = std::min(new_start_min, initial_end_min) - 1;
      }

      if (new_start_min > start_max) {
        // We have a conflict. We optimize the reason a bit.
        new_start_min = std::max(profile_[rec_id].start + 1, start_max + 1);
        explanation_start_time =
            std::min(explanation_start_time, new_start_min - 1);
        break;
      }
    }
  }

  if (new_start_min == initial_start_min) return true;
  return UpdateStartingTime(task_id, explanation_start_time, new_start_min);
}

bool TimeTablingPerTask::UpdateStartingTime(int task_id, IntegerValue left,
                                            IntegerValue right) {
  DCHECK_LT(left, right);
  helper_->ClearReason();
  AddProfileReason(task_id, left, right,
                   CapacityMax() - demands_->DemandMin(task_id));
  if (capacity_.var != kNoIntegerVariable) {
    helper_->MutableIntegerReason()->push_back(
        integer_trail_->UpperBoundAsLiteral(capacity_.var));
  }

  // State of the task to be pushed.
  helper_->AddEndMinReason(task_id, left + 1);
  helper_->AddSizeMinReason(task_id);
  demands_->AddDemandMinReason(task_id);

  // Explain the increase of the minimum start and end times.
  return helper_->IncreaseStartMin(task_id, right);
}

// TODO(user): there is more room for improvements in the reason.
// Note that compared to the "easiest" reason (mode == 2) this doesn't seems
// to help much. Still the more relaxed the reason, the better it should be.
void TimeTablingPerTask::AddProfileReason(int task_id, IntegerValue left,
                                          IntegerValue right,
                                          IntegerValue capacity_threshold) {
  IntegerValue sum_of_demand(0);

  // We optimize a bit the reason depending on the case.
  int mode;
  DCHECK_GT(right, left);
  DCHECK(task_id >= 0 || left + 1 == right);
  if (left + 1 == right) {
    // Here we can easily remove extra tasks if the demand is already high
    // enough.
    mode = 0;
  } else if (right - left < helper_->SizeMin(task_id) + 2) {
    // In this case, only the profile in [left, left + 1) and [right - 1, right)
    // is enough to push the task. We don't care about what happen in the middle
    // since the task will not fit.
    mode = 1;
  } else {
    mode = 2;
  }

  for (int i = 0; i < num_profile_tasks_; ++i) {
    const int t = profile_tasks_[i];

    // Do not consider the task if it does not overlap for sure (left, right).
    const IntegerValue start_max = helper_->StartMax(t);
    if (right <= start_max) continue;
    const IntegerValue end_min = helper_->EndMin(t);
    if (end_min <= left) continue;

    helper_->AddPresenceReason(t);

    // Note that we exclude the demand min for the task we push.
    // If we push the demand_max, we don't need it. And otherwise the task_id
    // shouldn't be part of the profile anyway.
    if (t != task_id) demands_->AddDemandMinReason(t);

    if (mode == 0) {
      helper_->AddStartMaxReason(t, left);
      helper_->AddEndMinReason(t, right);

      // No need to include more tasks if we have enough demand already.
      //
      // TODO(user): Improve what task we "exclude" instead of always taking
      // the last ones? Note however that profile_tasks_ should be in order in
      // which task have a mandatory part.
      sum_of_demand += demands_->DemandMin(t);
      if (sum_of_demand > capacity_threshold) break;
    } else if (mode == 1) {
      helper_->AddStartMaxReason(t, start_max <= left ? left : right - 1);
      helper_->AddEndMinReason(t, end_min >= right ? right : left + 1);
    } else {
      helper_->AddStartMaxReason(t, std::max(left, start_max));
      helper_->AddEndMinReason(t, std::min(right, end_min));
    }
  }
}

bool TimeTablingPerTask::IncreaseCapacity(IntegerValue time,
                                          IntegerValue new_min) {
  if (new_min <= CapacityMin()) return true;

  // No need to push further than this. It will result in a conflict anyway,
  // and we can optimize the reason a bit.
  new_min = std::min(CapacityMax() + 1, new_min);

  helper_->ClearReason();
  AddProfileReason(-1, time, time + 1, new_min);
  if (capacity_.var == kNoIntegerVariable) {
    return helper_->ReportConflict();
  }
  return helper_->PushIntegerLiteral(capacity_.GreaterOrEqual(new_min));
}

}  // namespace sat
}  // namespace operations_research
