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

#include <algorithm>

#include "sat/overload_checker.h"
#include "sat/precedences.h"
#include "sat/sat_solver.h"
#include "sat/timetabling.h"

namespace operations_research {
namespace sat {

std::function<void(Model*)> Cumulative(
    const std::vector<IntervalVariable>& vars,
    const std::vector<IntegerVariable>& demands,
    const IntegerVariable& capacity) {
  return [=](Model* model) {
    if (vars.empty()) return;
    IntervalsRepository* intervals = model->GetOrCreate<IntervalsRepository>();
    Trail* trail = model->GetOrCreate<Trail>();
    IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();

    if (vars.size() == 1) {
      if (intervals->IsOptional(vars[0])) {
        model->Add(ConditionalLowerOrEqualWithOffset(
            demands[0], capacity, 0, intervals->IsPresentLiteral(vars[0])));
      } else {
        model->Add(LowerOrEqual(demands[0], capacity));
      }
      return;
    }

    // Propagator responsible for applying the Overload Checking filtering rule.
    // This propagator increases the minimum of the capacity variable.
    OverloadChecker* overload_checker = new OverloadChecker(
        vars, demands, capacity, trail, integer_trail, intervals);
    overload_checker->RegisterWith(model->GetOrCreate<GenericLiteralWatcher>());
    model->TakeOwnership(overload_checker);

    // Propagator responsible for applying Timetabling filtering rule. This
    // propagator increases the minimum of the start variables, decrease the
    // maximum of the end variables, and increasing the minimum of the capacity
    // variable.
    TimeTablingPerTask* time_tabling = new TimeTablingPerTask(
        vars, demands, capacity, trail, integer_trail, intervals);
    time_tabling->RegisterWith(model->GetOrCreate<GenericLiteralWatcher>());
    model->TakeOwnership(time_tabling);
  };
}

TimeTablingPerTask::TimeTablingPerTask(
    const std::vector<IntervalVariable>& interval_vars,
    const std::vector<IntegerVariable>& demand_vars, IntegerVariable capacity,
    Trail* trail, IntegerTrail* integer_trail,
    IntervalsRepository* intervals_repository)
    : num_tasks_(interval_vars.size()),
      interval_vars_(interval_vars),
      demand_vars_(demand_vars),
      capacity_var_(capacity),
      trail_(trail),
      integer_trail_(integer_trail),
      intervals_repository_(intervals_repository) {
  start_min_.resize(num_tasks_);
  start_max_.resize(num_tasks_);
  end_min_.resize(num_tasks_);
  end_max_.resize(num_tasks_);
  duration_min_.resize(num_tasks_);
  demand_min_.resize(num_tasks_);
  scp_.reserve(num_tasks_);
  ecp_.reserve(num_tasks_);
  // Each task may create at most two profile rectangles. Such pattern appear if
  // the profile is shaped like the Hanoi tower. The additional space is for
  // both extremities and the sentinels.
  profile_.reserve(2 * num_tasks_ + 4);
  // Collect the variables.
  start_vars_.resize(num_tasks_);
  end_vars_.resize(num_tasks_);
  duration_vars_.resize(num_tasks_);
  for (int t = 0; t < num_tasks_; ++t) {
    const IntervalVariable i = interval_vars[t];
    start_vars_[t] = intervals_repository->StartVar(i);
    end_vars_[t] = intervals_repository->EndVar(i);
    duration_vars_[t] = intervals_repository->SizeVar(i);
  }
}

void TimeTablingPerTask::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  watcher->WatchUpperBound(capacity_var_, id);
  for (int t = 0; t < num_tasks_; t++) {
    watcher->WatchIntegerVariable(start_vars_[t], id);
    watcher->WatchIntegerVariable(end_vars_[t], id);
    watcher->WatchLowerBound(demand_vars_[t], id);
    if (duration_vars_[t] != kNoIntegerVariable) {
      watcher->WatchLowerBound(duration_vars_[t], id);
    }
    if (!IsAlwaysPresent(t)) {
      const Literal is_present =
          intervals_repository_->IsPresentLiteral(interval_vars_[t]);
      watcher->WatchLiteral(is_present, id);
    }
  }
}

bool TimeTablingPerTask::IsAlwaysPresent(int task_id) const {
  if (intervals_repository_->IsOptional(interval_vars_[task_id])) {
    const Literal is_present =
        intervals_repository_->IsPresentLiteral(interval_vars_[task_id]);
    return trail_->Assignment().LiteralIsTrue(is_present);
  }
  return true;
}

bool TimeTablingPerTask::Propagate() {
  // Repeat until the propagator does not filter anymore.
  profile_changed_ = true;
  while (profile_changed_) {
    profile_changed_ = false;

    // Rebuild compulsory part events.
    // -------------------------------
    scp_.clear();
    ecp_.clear();
    // TODO(user): exclude extreme tasks during the search.
    for (int t = 0; t < num_tasks_; ++t) {
      start_min_[t] = StartMin(t);
      start_max_[t] = StartMax(t);
      end_min_[t] = EndMin(t);
      end_max_[t] = EndMax(t);
      demand_min_[t] = DemandMin(t);
      duration_min_[t] = DurationMin(t);
      if (start_max_[t] < end_min_[t] && IsAlwaysPresent(t)) {
        scp_.push_back(Event(start_max_[t], t));
        ecp_.push_back(Event(end_min_[t], t));
      }
    }

    // No filtering is possible.
    if (scp_.empty()) return true;

    // TODO(user): use insertion sort with a fall back to std::sort.
    // Sort compulsory part events.
    std::sort(scp_.begin(), scp_.end());
    std::sort(ecp_.begin(), ecp_.end());

    // Build the profile.
    // ------------------
    profile_.clear();

    // Add a sentinel to simplify the algorithm.
    profile_.push_back(
        ProfileRectangle(kMinIntegerValue, kMinIntegerValue, IntegerValue(0)));

    // Start and height of the currently built profile rectange.
    IntegerValue current_start = kMinIntegerValue;
    IntegerValue current_height = IntegerValue(0);

    // Start and height of the highest profile rectangle.
    IntegerValue max_height_start = kMinIntegerValue;
    IntegerValue max_height = IntegerValue(0);

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
        current_height += demand_min_[scp_[next_scp].task_id];
        next_scp++;
      }

      // Process the ending compulsory parts.
      while (next_ecp < ecp_.size() && ecp_[next_ecp].time == t) {
        current_height -= demand_min_[ecp_[next_ecp].task_id];
        next_ecp++;
      }

      // Insert a new profile rectangle if any.
      if (current_height != old_height) {
        profile_.push_back(ProfileRectangle(current_start, t, old_height));
        if (current_height > max_height) {
          max_height = current_height;
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

    // Filter the capacity variable.
    // -----------------------------
    if (max_height > CapacityMin()) {
      reason_.clear();
      literal_reason_.clear();
      ExplainProfileHeight(max_height_start);
      if (!integer_trail_->Enqueue(
              IntegerLiteral::GreaterOrEqual(capacity_var_, max_height),
              literal_reason_, reason_)) {
        return false;
      }
    }

    // Update the start and end variables.
    // -----------------------------------
    // Tasks with a lower or equal demand will not be pushed.
    const IntegerValue min_demand = CapacityMax() - max_height;

    for (int t = 0; t < num_tasks_; ++t) {
      // The task cannot be pushed.
      // TODO(user): exclude fixed tasks for all the subtree.
      //
      // Note: We do not check that the task t is optional.
      // It is OK to propagate the bounds of optional variables. They should
      // become unperformed if the bounds are no longer consistent.
      if (demand_min_[t] <= min_demand || duration_min_[t] == 0) continue;

      // Increase the start min of task t.
      if (start_min_[t] != start_max_[t] && !SweepTaskRight(t)) return false;
      // Decrease the end max of task t.
      if (end_min_[t] != end_max_[t] && !SweepTaskLeft(t)) return false;
    }
  }
  return true;
}

bool TimeTablingPerTask::SweepTaskRight(int task_id) {
  // Find the profile rectangle that overlaps the start min of the task.
  // The sentinel prevents out of bound exceptions.
  int rec_id = 0;
  while (profile_[rec_id].end <= start_min_[task_id]) {
    rec_id++;
    DCHECK_LT(rec_id, profile_.size());
  }
  // Push the task from left to right until it does not overlap any conflicting
  // rectangle. Pushing the task may push the end of its compulsory part on the
  // right but will not change its start. The main loop of the propagator will
  // take care of rebuilding the profile with these possible changes and to
  // propagate again in order to reach the timetabling consistency or to fail if
  // the profile exceeds the resource capacity.
  const IntegerValue conflict_height = CapacityMax() - demand_min_[task_id];
  const IntegerValue s_max = start_max_[task_id];
  while (profile_[rec_id].start < std::min(s_max, end_min_[task_id])) {
    // If the profile rectangle is not conflicting, go to the next rectangle.
    if (profile_[rec_id].height <= conflict_height) {
      rec_id++;
      continue;
    }
    // If the task cannot be scheduled after the conflicting profile rectangle,
    // we explain all the intermediate pushes to schedule the task to its
    // start max. Scheduling the task to its start max may result in a capacity
    // overload that will be detected once the profile is rebuilt.
    if (s_max < profile_[rec_id].end) {
      while (end_min_[task_id] < s_max) {
        if (!UpdateStartingTime(task_id, end_min_[task_id])) return false;
      }
      if (start_min_[task_id] < s_max) {
        if (!UpdateStartingTime(task_id, s_max)) return false;
      }
      profile_changed_ = true;
      return true;
    }
    // If the task can be scheduled after the conflicting profile rectangle, we
    // explain all the intermediate pushes to push the task after this profile
    // rectangle. We then consider the next profile rectangle in the profile.
    while (end_min_[task_id] < profile_[rec_id].end) {
      if (!UpdateStartingTime(task_id, end_min_[task_id])) return false;
    }
    if (start_min_[task_id] < profile_[rec_id].end) {
      if (!UpdateStartingTime(task_id, profile_[rec_id].end)) return false;
    }
    profile_changed_ |= s_max < end_min_[task_id];
    rec_id++;
  }
  return true;
}

bool TimeTablingPerTask::SweepTaskLeft(int task_id) {
  // Find the profile rectangle that overlaps the end max of the task.
  // The sentinel prevents out of bound exceptions.
  int rec_id = profile_.size() - 1;
  while (end_max_[task_id] <= profile_[rec_id].start) {
    rec_id--;
    DCHECK_GE(rec_id, 0);
  }
  // Push the task from right to left until it does not overlap any conflicting
  // rectangle. Pushing the task may push the start of its compulsory part on
  // the left but will not change its end. The main loop of the propagator will
  // take care of rebuilding the profile with these possible changes and to
  // propagate again in order to reach the timetabling consistency or to fail if
  // the profile exceeds the resource capacity.
  const IntegerValue conflict_height = CapacityMax() - demand_min_[task_id];
  const IntegerValue e_min = end_min_[task_id];
  while (std::max(e_min, start_max_[task_id]) < profile_[rec_id].end) {
    // If the profile rectangle is not conflicting, go to the next rectangle.
    if (profile_[rec_id].height <= conflict_height) {
      rec_id--;
      continue;
    }
    // If the task cannot be scheduled before the conflicting profile rectangle,
    // we explain all the intermediate pushes to schedule the task to its
    // end min. Scheduling the task to its end min may result in a capacity
    // overload that will be detected once the profile is rebuilt.
    if (profile_[rec_id].start < e_min) {
      while (e_min < start_max_[task_id]) {
        if (!UpdateEndingTime(task_id, start_max_[task_id])) return false;
      }
      if (e_min < end_max_[task_id]) {
        if (!UpdateEndingTime(task_id, e_min)) return false;
      }
      profile_changed_ = true;
      return true;
    }
    // If the task can be scheduled before the conflicting profile rectangle, we
    // explain all the intermediate pushes to push the task before this profile
    // rectangle. We then consider the next profile rectangle in the profile.
    while (profile_[rec_id].start < start_max_[task_id]) {
      if (!UpdateEndingTime(task_id, start_max_[task_id])) return false;
    }
    if (profile_[rec_id].start < end_max_[task_id]) {
      if (!UpdateEndingTime(task_id, profile_[rec_id].start)) return false;
    }
    profile_changed_ |= start_max_[task_id] < e_min;
    rec_id--;
  }
  return true;
}

// TODO(user): Test the code on tasks with variable duration.
bool TimeTablingPerTask::UpdateStartingTime(int task_id,
                                            IntegerValue new_start) {
  reason_.clear();
  literal_reason_.clear();
  ExplainProfileHeight(new_start - 1);
  reason_.push_back(integer_trail_->UpperBoundAsLiteral(capacity_var_));
  reason_.push_back(integer_trail_->LowerBoundAsLiteral(demand_vars_[task_id]));
  reason_.push_back(
      IntegerLiteral::GreaterOrEqual(end_vars_[task_id], new_start));

  // Explain the increase of the start min.
  if (!integer_trail_->Enqueue(
          IntegerLiteral::GreaterOrEqual(start_vars_[task_id], new_start),
          literal_reason_, reason_)) {
    return false;
  }

  // Update the cached start min.
  start_min_[task_id] = new_start;

  // Check that we need to push the end min.
  const IntegerValue new_end =
      std::max(end_min_[task_id], new_start + duration_min_[task_id]);
  if (new_end == end_min_[task_id]) return true;

  // Build the reason to increase the end min.
  reason_.clear();
  literal_reason_.clear();
  reason_.push_back(integer_trail_->LowerBoundAsLiteral(start_vars_[task_id]));
  // Only use the duration variable if it is defined.
  if (duration_vars_[task_id] != kNoIntegerVariable) {
    reason_.push_back(
        integer_trail_->LowerBoundAsLiteral(duration_vars_[task_id]));
  }

  // Explain the increase of the end min.
  if (!integer_trail_->Enqueue(
          IntegerLiteral::GreaterOrEqual(end_vars_[task_id], new_end),
          literal_reason_, reason_)) {
    return false;
  }

  // Update the cached end min.
  end_min_[task_id] = new_end;

  return true;
}

bool TimeTablingPerTask::UpdateEndingTime(int task_id, IntegerValue new_end) {
  reason_.clear();
  ExplainProfileHeight(new_end);
  reason_.push_back(integer_trail_->UpperBoundAsLiteral(capacity_var_));
  reason_.push_back(integer_trail_->LowerBoundAsLiteral(demand_vars_[task_id]));
  reason_.push_back(
      IntegerLiteral::LowerOrEqual(start_vars_[task_id], new_end));

  // Explain the decrease of the end max.
  if (!integer_trail_->Enqueue(
          IntegerLiteral::LowerOrEqual(end_vars_[task_id], new_end),
          literal_reason_, reason_)) {
    return false;
  }

  // Update the cached end max.
  end_max_[task_id] = new_end;

  // Check that we need to push the start max.
  const IntegerValue new_start =
      std::min(start_max_[task_id], new_end - duration_min_[task_id]);
  if (new_start == start_max_[task_id]) return true;

  // Build the reason to decrease the start max.
  reason_.clear();
  reason_.push_back(integer_trail_->UpperBoundAsLiteral(end_vars_[task_id]));
  // Only use the duration variable if it is defined.
  if (duration_vars_[task_id] != kNoIntegerVariable) {
    reason_.push_back(
        integer_trail_->LowerBoundAsLiteral(duration_vars_[task_id]));
  }

  // Explain the decrease of the start max.
  if (!integer_trail_->Enqueue(
          IntegerLiteral::LowerOrEqual(start_vars_[task_id], new_start),
          literal_reason_, reason_)) {
    return false;
  }

  // Update the cached start max.
  start_max_[task_id] = new_start;

  return true;
}

void TimeTablingPerTask::AddPresenceReasonIfNeeded(int task_id) {
  if (intervals_repository_->IsOptional(interval_vars_[task_id])) {
    literal_reason_.push_back(
        intervals_repository_->IsPresentLiteral(interval_vars_[task_id])
            .Negated());
  }
}

void TimeTablingPerTask::ExplainProfileHeight(IntegerValue time) {
  for (int t = 0; t < num_tasks_; ++t) {
    // Tasks need to overlap the time point, i.e., start_max <= time < end_min.
    if (start_max_[t] <= time && time < end_min_[t]) {
      reason_.push_back(integer_trail_->LowerBoundAsLiteral(demand_vars_[t]));
      reason_.push_back(IntegerLiteral::LowerOrEqual(start_vars_[t], time));
      reason_.push_back(IntegerLiteral::GreaterOrEqual(end_vars_[t], time + 1));
      AddPresenceReasonIfNeeded(t);
    }
  }
}

}  // namespace sat
}  // namespace operations_research
