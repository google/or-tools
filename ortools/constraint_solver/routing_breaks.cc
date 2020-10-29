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

#include <algorithm>

#include "ortools/constraint_solver/routing.h"

namespace operations_research {

bool DisjunctivePropagator::Propagate(Tasks* tasks) {
  DCHECK_LE(tasks->num_chain_tasks, tasks->start_min.size());
  DCHECK_EQ(tasks->start_min.size(), tasks->start_max.size());
  DCHECK_EQ(tasks->start_min.size(), tasks->duration_min.size());
  DCHECK_EQ(tasks->start_min.size(), tasks->duration_max.size());
  DCHECK_EQ(tasks->start_min.size(), tasks->end_min.size());
  DCHECK_EQ(tasks->start_min.size(), tasks->end_max.size());
  DCHECK_EQ(tasks->start_min.size(), tasks->is_preemptible.size());
  // Do forward deductions, then backward deductions.
  // All propagators are followed by Precedences(),
  // except MirrorTasks() after which Precedences() would make no deductions,
  // and DetectablePrecedencesWithChain() which is stronger than Precedences().
  // Precedences() is a propagator that does obvious deductions quickly (O(n)),
  // so interleaving Precedences() speeds up the propagation fixed point.
  if (!Precedences(tasks) || !EdgeFinding(tasks) || !Precedences(tasks) ||
      !DetectablePrecedencesWithChain(tasks)) {
    return false;
  }
  if (!tasks->forbidden_intervals.empty()) {
    if (!ForbiddenIntervals(tasks) || !Precedences(tasks)) return false;
  }
  if (!tasks->distance_duration.empty()) {
    if (!DistanceDuration(tasks) || !Precedences(tasks)) return false;
  }
  if (!MirrorTasks(tasks) || !EdgeFinding(tasks) || !Precedences(tasks) ||
      !DetectablePrecedencesWithChain(tasks) || !MirrorTasks(tasks)) {
    return false;
  }
  return true;
}

bool DisjunctivePropagator::Precedences(Tasks* tasks) {
  const int num_chain_tasks = tasks->num_chain_tasks;
  if (num_chain_tasks > 0) {
    // Propagate forwards.
    int64 time = tasks->start_min[0];
    for (int task = 0; task < num_chain_tasks; ++task) {
      time = std::max(tasks->start_min[task], time);
      tasks->start_min[task] = time;
      time = CapAdd(time, tasks->duration_min[task]);
      if (tasks->end_max[task] < time) return false;
      time = std::max(time, tasks->end_min[task]);
      tasks->end_min[task] = time;
    }
    // Propagate backwards.
    time = tasks->end_max[num_chain_tasks - 1];
    for (int task = num_chain_tasks - 1; task >= 0; --task) {
      time = std::min(tasks->end_max[task], time);
      tasks->end_max[task] = time;
      time = CapSub(time, tasks->duration_min[task]);
      if (time < tasks->start_min[task]) return false;
      time = std::min(time, tasks->start_max[task]);
      tasks->start_max[task] = time;
    }
  }
  const int num_tasks = tasks->start_min.size();
  for (int task = 0; task < num_tasks; ++task) {
    // Enforce start + duration <= end.
    tasks->end_min[task] =
        std::max(tasks->end_min[task],
                 CapAdd(tasks->start_min[task], tasks->duration_min[task]));
    tasks->start_max[task] =
        std::min(tasks->start_max[task],
                 CapSub(tasks->end_max[task], tasks->duration_min[task]));
    tasks->duration_max[task] =
        std::min(tasks->duration_max[task],
                 CapSub(tasks->end_max[task], tasks->start_min[task]));
    if (!tasks->is_preemptible[task]) {
      // Enforce start + duration == end for nonpreemptibles.
      tasks->end_max[task] =
          std::min(tasks->end_max[task],
                   CapAdd(tasks->start_max[task], tasks->duration_max[task]));
      tasks->start_min[task] =
          std::max(tasks->start_min[task],
                   CapSub(tasks->end_min[task], tasks->duration_max[task]));
      tasks->duration_min[task] =
          std::max(tasks->duration_min[task],
                   CapSub(tasks->end_min[task], tasks->start_max[task]));
    }
    if (tasks->duration_min[task] > tasks->duration_max[task]) return false;
    if (tasks->end_min[task] > tasks->end_max[task]) return false;
    if (tasks->start_min[task] > tasks->start_max[task]) return false;
  }
  return true;
}

bool DisjunctivePropagator::MirrorTasks(Tasks* tasks) {
  const int num_tasks = tasks->start_min.size();
  // For all tasks, start_min := -end_max and end_max := -start_min.
  for (int task = 0; task < num_tasks; ++task) {
    const int64 t = -tasks->start_min[task];
    tasks->start_min[task] = -tasks->end_max[task];
    tasks->end_max[task] = t;
  }
  // For all tasks, start_max := -end_min and end_min := -start_max.
  for (int task = 0; task < num_tasks; ++task) {
    const int64 t = -tasks->start_max[task];
    tasks->start_max[task] = -tasks->end_min[task];
    tasks->end_min[task] = t;
  }
  // In the mirror problem, tasks linked by precedences are in reversed order.
  const int num_chain_tasks = tasks->num_chain_tasks;
  for (const auto it :
       {tasks->start_min.begin(), tasks->start_max.begin(),
        tasks->duration_min.begin(), tasks->duration_max.begin(),
        tasks->end_min.begin(), tasks->end_max.begin()}) {
    std::reverse(it, it + num_chain_tasks);
    std::reverse(it + num_chain_tasks, it + num_tasks);
  }
  std::reverse(tasks->is_preemptible.begin(),
               tasks->is_preemptible.begin() + num_chain_tasks);
  std::reverse(tasks->is_preemptible.begin() + num_chain_tasks,
               tasks->is_preemptible.begin() + num_tasks);
  return true;
}

bool DisjunctivePropagator::EdgeFinding(Tasks* tasks) {
  const int num_tasks = tasks->start_min.size();
  // Prepare start_min events for tree.
  tasks_by_start_min_.resize(num_tasks);
  std::iota(tasks_by_start_min_.begin(), tasks_by_start_min_.end(), 0);
  std::sort(
      tasks_by_start_min_.begin(), tasks_by_start_min_.end(),
      [&](int i, int j) { return tasks->start_min[i] < tasks->start_min[j]; });
  event_of_task_.resize(num_tasks);
  for (int event = 0; event < num_tasks; ++event) {
    event_of_task_[tasks_by_start_min_[event]] = event;
  }
  // Tasks will be browsed according to end_max order.
  tasks_by_end_max_.resize(num_tasks);
  std::iota(tasks_by_end_max_.begin(), tasks_by_end_max_.end(), 0);
  std::sort(
      tasks_by_end_max_.begin(), tasks_by_end_max_.end(),
      [&](int i, int j) { return tasks->end_max[i] < tasks->end_max[j]; });

  // Generic overload checking: insert tasks by end_max,
  // fail if envelope > end_max.
  theta_lambda_tree_.Reset(num_tasks);
  for (const int task : tasks_by_end_max_) {
    theta_lambda_tree_.AddOrUpdateEvent(
        event_of_task_[task], tasks->start_min[task], tasks->duration_min[task],
        tasks->duration_min[task]);
    if (theta_lambda_tree_.GetEnvelope() > tasks->end_max[task]) {
      return false;
    }
  }

  // Generic edge finding: from full set of tasks, at each end_max event in
  // decreasing order, check lambda feasibility, then move end_max task from
  // theta to lambda.
  for (int i = num_tasks - 1; i >= 0; --i) {
    const int task = tasks_by_end_max_[i];
    const int64 envelope = theta_lambda_tree_.GetEnvelope();
    // If a nonpreemptible optional would overload end_max, push to envelope.
    while (theta_lambda_tree_.GetOptionalEnvelope() > tasks->end_max[task]) {
      int critical_event;  // Dummy value.
      int optional_event;
      int64 available_energy;  // Dummy value.
      theta_lambda_tree_.GetEventsWithOptionalEnvelopeGreaterThan(
          tasks->end_max[task], &critical_event, &optional_event,
          &available_energy);
      const int optional_task = tasks_by_start_min_[optional_event];
      tasks->start_min[optional_task] =
          std::max(tasks->start_min[optional_task], envelope);
      theta_lambda_tree_.RemoveEvent(optional_event);
    }
    if (!tasks->is_preemptible[task]) {
      theta_lambda_tree_.AddOrUpdateOptionalEvent(event_of_task_[task],
                                                  tasks->start_min[task],
                                                  tasks->duration_min[task]);
    } else {
      theta_lambda_tree_.RemoveEvent(event_of_task_[task]);
    }
  }
  return true;
}

bool DisjunctivePropagator::DetectablePrecedencesWithChain(Tasks* tasks) {
  const int num_tasks = tasks->start_min.size();
  // Prepare start_min events for tree.
  tasks_by_start_min_.resize(num_tasks);
  std::iota(tasks_by_start_min_.begin(), tasks_by_start_min_.end(), 0);
  std::sort(
      tasks_by_start_min_.begin(), tasks_by_start_min_.end(),
      [&](int i, int j) { return tasks->start_min[i] < tasks->start_min[j]; });
  event_of_task_.resize(num_tasks);
  for (int event = 0; event < num_tasks; ++event) {
    event_of_task_[tasks_by_start_min_[event]] = event;
  }
  theta_lambda_tree_.Reset(num_tasks);

  // Sort nonchain tasks by start max = end_max - duration_min.
  const int num_chain_tasks = tasks->num_chain_tasks;
  nonchain_tasks_by_start_max_.resize(num_tasks - num_chain_tasks);
  std::iota(nonchain_tasks_by_start_max_.begin(),
            nonchain_tasks_by_start_max_.end(), num_chain_tasks);
  std::sort(nonchain_tasks_by_start_max_.begin(),
            nonchain_tasks_by_start_max_.end(), [&tasks](int i, int j) {
              return tasks->end_max[i] - tasks->duration_min[i] <
                     tasks->end_max[j] - tasks->duration_min[j];
            });

  // Detectable precedences, specialized for routes: for every task on route,
  // put all tasks before it in the tree, then push with envelope.
  int index_nonchain = 0;
  for (int i = 0; i < num_chain_tasks; ++i) {
    if (!tasks->is_preemptible[i]) {
      // Add all nonchain tasks detected before i.
      while (index_nonchain < nonchain_tasks_by_start_max_.size()) {
        const int task = nonchain_tasks_by_start_max_[index_nonchain];
        if (tasks->end_max[task] - tasks->duration_min[task] >=
            tasks->start_min[i] + tasks->duration_min[i])
          break;
        theta_lambda_tree_.AddOrUpdateEvent(
            event_of_task_[task], tasks->start_min[task],
            tasks->duration_min[task], tasks->duration_min[task]);
        index_nonchain++;
      }
    }
    // All chain and nonchain tasks before i are now in the tree, push i.
    const int64 new_start_min = theta_lambda_tree_.GetEnvelope();
    // Add i to the tree before updating it.
    theta_lambda_tree_.AddOrUpdateEvent(event_of_task_[i], tasks->start_min[i],
                                        tasks->duration_min[i],
                                        tasks->duration_min[i]);
    tasks->start_min[i] = std::max(tasks->start_min[i], new_start_min);
  }
  return true;
}

bool DisjunctivePropagator::ForbiddenIntervals(Tasks* tasks) {
  if (tasks->forbidden_intervals.empty()) return true;
  const int num_tasks = tasks->start_min.size();
  for (int task = 0; task < num_tasks; ++task) {
    if (tasks->duration_min[task] == 0) continue;
    if (tasks->forbidden_intervals[task] == nullptr) continue;
    // If start_min forbidden, push to next feasible value.
    {
      const auto& interval =
          tasks->forbidden_intervals[task]->FirstIntervalGreaterOrEqual(
              tasks->start_min[task]);
      if (interval == tasks->forbidden_intervals[task]->end()) continue;
      if (interval->start <= tasks->start_min[task]) {
        tasks->start_min[task] = CapAdd(interval->end, 1);
      }
    }
    // If end_max forbidden, push to next feasible value.
    {
      const int64 start_max =
          CapSub(tasks->end_max[task], tasks->duration_min[task]);
      const auto& interval =
          tasks->forbidden_intervals[task]->LastIntervalLessOrEqual(start_max);
      if (interval == tasks->forbidden_intervals[task]->end()) continue;
      if (interval->end >= start_max) {
        tasks->end_max[task] =
            CapAdd(interval->start, tasks->duration_min[task] - 1);
      }
    }
    if (CapAdd(tasks->start_min[task], tasks->duration_min[task]) >
        tasks->end_max[task]) {
      return false;
    }
  }
  return true;
}

bool DisjunctivePropagator::DistanceDuration(Tasks* tasks) {
  if (tasks->distance_duration.empty()) return true;
  if (tasks->num_chain_tasks == 0) return true;
  const int route_start = 0;
  const int route_end = tasks->num_chain_tasks - 1;
  const int num_tasks = tasks->start_min.size();
  for (int i = 0; i < tasks->distance_duration.size(); ++i) {
    const int64 max_distance = tasks->distance_duration[i].first;
    const int64 minimum_break_duration = tasks->distance_duration[i].second;

    // This is a sweeping algorithm that looks whether the union of intervals
    // defined by breaks and route start/end is (-infty, +infty).
    // Those intervals are:
    // - route start: (-infty, start_max + distance]
    // - route end: [end_min, +infty)
    // - breaks: [start_min, end_max + distance) if their duration_max
    //   is >= min_duration, empty set otherwise.
    // If sweeping finds that a time point can be covered by only one interval,
    // it will force the corresponding break or route start/end to cover this
    // point, which can force a break to be above minimum_break_duration.

    // We suppose break tasks are ordered, so the algorithm supposes that
    // start_min(task_n) <= start_min(task_{n+1}) and
    // end_max(task_n) <= end_max(task_{n+1}).
    for (int task = tasks->num_chain_tasks + 1; task < num_tasks; ++task) {
      tasks->start_min[task] =
          std::max(tasks->start_min[task], tasks->start_min[task - 1]);
    }
    for (int task = num_tasks - 2; task >= tasks->num_chain_tasks; --task) {
      tasks->end_max[task] =
          std::min(tasks->end_max[task], tasks->end_max[task + 1]);
    }
    // Skip breaks that cannot be performed after start.
    int index_break_by_emax = tasks->num_chain_tasks;
    while (index_break_by_emax < num_tasks &&
           tasks->end_max[index_break_by_emax] <= tasks->end_max[route_start]) {
      ++index_break_by_emax;
    }
    // Special case: no breaks after start.
    if (index_break_by_emax == num_tasks) {
      tasks->end_min[route_start] =
          std::max(tasks->end_min[route_start],
                   CapSub(tasks->start_min[route_end], max_distance));
      tasks->start_max[route_end] =
          std::min(tasks->start_max[route_end],
                   CapAdd(tasks->end_max[route_start], max_distance));
      continue;
    }
    // There will be a break after start, so route_start coverage is tested.
    // Initial state: start at -inf with route_start in task_set.
    // Sweep over profile, looking for time points where the number of
    // covering breaks is <= 1. If it is 0, fail, otherwise force the
    // unique break to cover it.
    // Route start and end get a special treatment, not sure generalizing
    // would be better.
    int64 xor_active_tasks = route_start;
    int num_active_tasks = 1;
    int64 previous_time = kint64min;
    const int64 route_start_time =
        CapAdd(tasks->end_max[route_start], max_distance);
    const int64 route_end_time = tasks->start_min[route_end];
    int index_break_by_smin = tasks->num_chain_tasks;
    while (index_break_by_emax < num_tasks) {
      // Find next time point among start/end of covering intervals.
      int64 current_time =
          CapAdd(tasks->end_max[index_break_by_emax], max_distance);
      if (index_break_by_smin < num_tasks) {
        current_time =
            std::min(current_time, tasks->start_min[index_break_by_smin]);
      }
      if (previous_time < route_start_time && route_start_time < current_time) {
        current_time = route_start_time;
      }
      if (previous_time < route_end_time && route_end_time < current_time) {
        current_time = route_end_time;
      }
      // If num_active_tasks was 1, the unique active task must cover from
      // previous_time to current_time.
      if (num_active_tasks == 1) {
        // xor_active_tasks is the unique task that can cover [previous_time,
        // current_time).
        if (xor_active_tasks != route_end) {
          tasks->end_min[xor_active_tasks] =
              std::max(tasks->end_min[xor_active_tasks],
                       CapSub(current_time, max_distance));
          if (xor_active_tasks != route_start) {
            tasks->duration_min[xor_active_tasks] = std::max(
                tasks->duration_min[xor_active_tasks],
                std::max(
                    minimum_break_duration,
                    CapSub(CapSub(current_time, max_distance), previous_time)));
          }
        }
      }
      // Process covering intervals that start or end at current_time.
      while (index_break_by_smin < num_tasks &&
             current_time == tasks->start_min[index_break_by_smin]) {
        if (tasks->duration_max[index_break_by_smin] >=
            minimum_break_duration) {
          xor_active_tasks ^= index_break_by_smin;
          ++num_active_tasks;
        }
        ++index_break_by_smin;
      }
      while (index_break_by_emax < num_tasks &&
             current_time ==
                 CapAdd(tasks->end_max[index_break_by_emax], max_distance)) {
        if (tasks->duration_max[index_break_by_emax] >=
            minimum_break_duration) {
          xor_active_tasks ^= index_break_by_emax;
          --num_active_tasks;
        }
        ++index_break_by_emax;
      }
      if (current_time == route_start_time) {
        xor_active_tasks ^= route_start;
        --num_active_tasks;
      }
      if (current_time == route_end_time) {
        xor_active_tasks ^= route_end;
        ++num_active_tasks;
      }
      // If num_active_tasks becomes 1, the unique active task must cover from
      // current_time.
      if (num_active_tasks <= 0) return false;
      if (num_active_tasks == 1) {
        if (xor_active_tasks != route_start) {
          // xor_active_tasks is the unique task that can cover from
          // current_time to the next time point.
          tasks->start_max[xor_active_tasks] =
              std::min(tasks->start_max[xor_active_tasks], current_time);
          if (xor_active_tasks != route_end) {
            tasks->duration_min[xor_active_tasks] = std::max(
                tasks->duration_min[xor_active_tasks], minimum_break_duration);
          }
        }
      }
      previous_time = current_time;
    }
  }
  return true;
}

bool DisjunctivePropagator::ChainSpanMin(Tasks* tasks) {
  const int num_chain_tasks = tasks->num_chain_tasks;
  if (num_chain_tasks < 1) return true;
  // TODO(user): add stronger bounds.
  // The duration of the chain plus that of nonchain tasks that must be
  // performed during the chain is a lower bound of the chain span.
  {
    int64 sum_chain_durations = 0;
    const auto duration_start = tasks->duration_min.begin();
    const auto duration_end = tasks->duration_min.begin() + num_chain_tasks;
    for (auto it = duration_start; it != duration_end; ++it) {
      sum_chain_durations = CapAdd(sum_chain_durations, *it);
    }
    int64 sum_forced_nonchain_durations = 0;
    for (int i = num_chain_tasks; i < tasks->start_min.size(); ++i) {
      // Tasks that can be executed before or after are skipped.
      if (tasks->end_min[i] <= tasks->start_max[0] ||
          tasks->end_min[num_chain_tasks - 1] <= tasks->start_max[i]) {
        continue;
      }
      sum_forced_nonchain_durations =
          CapAdd(sum_forced_nonchain_durations, tasks->duration_min[i]);
    }
    tasks->span_min =
        std::max(tasks->span_min,
                 CapAdd(sum_chain_durations, sum_forced_nonchain_durations));
  }
  // The difference end of the chain - start of the chain is a lower bound.
  {
    const int64 end_minus_start =
        CapSub(tasks->end_min[num_chain_tasks - 1], tasks->start_max[0]);
    tasks->span_min = std::max(tasks->span_min, end_minus_start);
  }

  return tasks->span_min <= tasks->span_max;
}

// Computes a lower bound of the span of the chain, taking into account only
// the first nonchain task.
// TODO(user): extend to arbitrary number of nonchain tasks.
bool DisjunctivePropagator::ChainSpanMinDynamic(Tasks* tasks) {
  // Do nothing if there are no chain tasks or no nonchain tasks.
  const int num_chain_tasks = tasks->num_chain_tasks;
  if (num_chain_tasks < 1) return true;
  if (num_chain_tasks == tasks->start_min.size()) return true;
  const int task_index = num_chain_tasks;
  if (!Precedences(tasks)) return false;
  const int64 min_possible_chain_end = tasks->end_min[num_chain_tasks - 1];
  const int64 max_possible_chain_start = tasks->start_max[0];
  // For each chain task i, compute cumulated duration of chain tasks before it.
  int64 total_duration = 0;
  {
    total_duration_before_.resize(num_chain_tasks);
    for (int i = 0; i < num_chain_tasks; ++i) {
      total_duration_before_[i] = total_duration;
      total_duration = CapAdd(total_duration, tasks->duration_min[i]);
    }
  }
  // Estimate span min of chain tasks. Use the schedule that ends at
  // min_possible_chain_end and starts at smallest of start_max[0] or the
  // threshold where pushing start[0] later does not make a difference to the
  // chain span because of chain precedence constraints,
  // i.e. min_possible_chain_end - total_duration.
  {
    const int64 chain_span_min =
        min_possible_chain_end -
        std::min(tasks->start_max[0], min_possible_chain_end - total_duration);
    if (chain_span_min > tasks->span_max) {
      return false;
    } else {
      tasks->span_min = std::max(tasks->span_min, chain_span_min);
    }
    // If task can be performed before or after the chain,
    // span_min is chain_span_min.
    if (tasks->end_min[task_index] <= tasks->start_max[0] ||
        tasks->end_min[num_chain_tasks - 1] <= tasks->start_max[task_index]) {
      return true;
    }
  }
  // Scan all possible preemption positions of the nontask chain,
  // keep the one that yields the minimum span.
  int64 span_min = kint64max;
  bool schedule_is_feasible = false;
  for (int i = 0; i < num_chain_tasks; ++i) {
    if (!tasks->is_preemptible[i]) continue;
    // Estimate span min if tasks is performed during i.
    // For all possible minimal-span schedules, there is a schedule where task i
    // and nonchain task form a single block. Thus, we only consider those.
    const int64 block_start_min =
        std::max(tasks->start_min[i],
                 tasks->start_min[task_index] - tasks->duration_min[i]);
    const int64 block_start_max =
        std::min(tasks->start_max[task_index],
                 tasks->start_max[i] - tasks->duration_min[task_index]);
    if (block_start_min > block_start_max) continue;

    // Compute the block start that yields the minimal span.
    // Given a feasible block start, a chain of minimum span constrained to
    // this particular block start can be obtained by scheduling all tasks after
    // the block at their earliest, and all tasks before it at their latest.
    // The span can be decomposed into two parts: the head, which are the
    // tasks that are before the block, and the tail, which are the block and
    // the tasks after it.
    // When the block start varies, the head length of the optimal schedule
    // described above decreases as much as the block start decreases, until
    // an inflection point at which it stays constant. That inflection value
    // is the one where the precedence constraints force the chain start to
    // decrease because of durations.
    const int64 head_inflection =
        max_possible_chain_start + total_duration_before_[i];
    // The map from block start to minimal tail length also has an inflection
    // point, that additionally depends on the nonchain task's duration.
    const int64 tail_inflection = min_possible_chain_end -
                                  (total_duration - total_duration_before_[i]) -
                                  tasks->duration_min[task_index];
    // All block start values between these two yield the same minimal span.
    // Indeed, first, mind that the inflection points might be in any order.
    // - if head_inflection < tail_inflection, then inside the interval
    //   [head_inflection, tail_inflection], increasing the block start by delta
    //   decreases the tail length by delta and increases the head length by
    //   delta too.
    // - if tail_inflection < head_inflection, then inside the interval
    //   [tail_inflection, head_inflection], head length is constantly at
    //   total_duration_before_[i], and tail length is also constant.
    // In both cases, outside of the interval, one part is constant and the
    // other increases as much as the distance to the interval.
    // We can abstract inflection point to the interval they form.
    const int64 optimal_interval_min_start =
        std::min(head_inflection, tail_inflection);
    const int64 optimal_interval_max_start =
        std::max(head_inflection, tail_inflection);
    // If the optimal interval for block start intersects the feasible interval,
    // we can select any point within it, for instance the earliest one.
    int64 block_start = std::max(optimal_interval_min_start, block_start_min);
    // If the intervals do not intersect, the feasible value closest to the
    // optimal interval has the minimal span, because the span increases as
    // much as the distance to the optimal interval.
    if (optimal_interval_max_start < block_start_min) {
      // Optimal interval is before feasible interval, closest is feasible min.
      block_start = block_start_min;
    } else if (block_start_max < optimal_interval_min_start) {
      // Optimal interval is after feasible interval, closest is feasible max.
      block_start = block_start_max;
    }
    // Compute span for the chosen block start.
    const int64 head_duration =
        std::max(block_start, head_inflection) - max_possible_chain_start;
    const int64 tail_duration =
        min_possible_chain_end - std::min(block_start, tail_inflection);
    const int64 optimal_span_at_i = head_duration + tail_duration;
    span_min = std::min(span_min, optimal_span_at_i);
    schedule_is_feasible = true;
  }
  if (!schedule_is_feasible || span_min > tasks->span_max) {
    return false;
  } else {
    tasks->span_min = std::max(tasks->span_min, span_min);
    return true;
  }
}

void AppendTasksFromPath(const std::vector<int64>& path,
                         const TravelBounds& travel_bounds,
                         const RoutingDimension& dimension,
                         DisjunctivePropagator::Tasks* tasks) {
  const int num_nodes = path.size();
  DCHECK_EQ(travel_bounds.pre_travels.size(), num_nodes - 1);
  DCHECK_EQ(travel_bounds.post_travels.size(), num_nodes - 1);
  for (int i = 0; i < num_nodes; ++i) {
    const int64 cumul_min = dimension.CumulVar(path[i])->Min();
    const int64 cumul_max = dimension.CumulVar(path[i])->Max();
    // Add task associated to visit i.
    // Visits start at Cumul(path[i]) - before_visit
    // and end at Cumul(path[i]) + after_visit
    {
      const int64 before_visit =
          (i == 0) ? 0 : travel_bounds.post_travels[i - 1];
      const int64 after_visit =
          (i == num_nodes - 1) ? 0 : travel_bounds.pre_travels[i];

      tasks->start_min.push_back(CapSub(cumul_min, before_visit));
      tasks->start_max.push_back(CapSub(cumul_max, before_visit));
      tasks->duration_min.push_back(CapAdd(before_visit, after_visit));
      tasks->duration_max.push_back(CapAdd(before_visit, after_visit));
      tasks->end_min.push_back(CapAdd(cumul_min, after_visit));
      tasks->end_max.push_back(CapAdd(cumul_max, after_visit));
      tasks->is_preemptible.push_back(false);
    }
    if (i == num_nodes - 1) break;

    // Tasks from travels.
    // A travel task starts at Cumul(path[i]) + pre_travel,
    // last for FixedTransitVar(path[i]) - pre_travel - post_travel,
    // and must end at the latest at Cumul(path[i+1]) - post_travel.
    {
      const int64 pre_travel = travel_bounds.pre_travels[i];
      const int64 post_travel = travel_bounds.post_travels[i];
      tasks->start_min.push_back(CapAdd(cumul_min, pre_travel));
      tasks->start_max.push_back(CapAdd(cumul_max, pre_travel));
      tasks->duration_min.push_back(
          std::max<int64>(0, CapSub(travel_bounds.min_travels[i],
                                    CapAdd(pre_travel, post_travel))));
      tasks->duration_max.push_back(
          travel_bounds.max_travels[i] == kint64max
              ? kint64max
              : std::max<int64>(0, CapSub(travel_bounds.max_travels[i],
                                          CapAdd(pre_travel, post_travel))));
      tasks->end_min.push_back(
          CapSub(dimension.CumulVar(path[i + 1])->Min(), post_travel));
      tasks->end_max.push_back(
          CapSub(dimension.CumulVar(path[i + 1])->Max(), post_travel));
      tasks->is_preemptible.push_back(true);
    }
  }
}

void FillTravelBoundsOfVehicle(int vehicle, const std::vector<int64>& path,
                               const RoutingDimension& dimension,
                               TravelBounds* travel_bounds) {
  // Fill path and min/max/pre/post travel bounds.
  FillPathEvaluation(path, dimension.transit_evaluator(vehicle),
                     &travel_bounds->min_travels);
  const int num_travels = travel_bounds->min_travels.size();
  travel_bounds->max_travels.assign(num_travels, kint64max);
  {
    const int index = dimension.GetPreTravelEvaluatorOfVehicle(vehicle);
    if (index == -1) {
      travel_bounds->pre_travels.assign(num_travels, 0);
    } else {
      FillPathEvaluation(path, dimension.model()->TransitCallback(index),
                         &travel_bounds->pre_travels);
    }
  }
  {
    const int index = dimension.GetPostTravelEvaluatorOfVehicle(vehicle);
    if (index == -1) {
      travel_bounds->post_travels.assign(num_travels, 0);
    } else {
      FillPathEvaluation(path, dimension.model()->TransitCallback(index),
                         &travel_bounds->post_travels);
    }
  }
}

void AppendTasksFromIntervals(const std::vector<IntervalVar*>& intervals,
                              DisjunctivePropagator::Tasks* tasks) {
  for (IntervalVar* interval : intervals) {
    if (!interval->MustBePerformed()) continue;
    tasks->start_min.push_back(interval->StartMin());
    tasks->start_max.push_back(interval->StartMax());
    tasks->duration_min.push_back(interval->DurationMin());
    tasks->duration_max.push_back(interval->DurationMax());
    tasks->end_min.push_back(interval->EndMin());
    tasks->end_max.push_back(interval->EndMax());
    tasks->is_preemptible.push_back(false);
  }
}

GlobalVehicleBreaksConstraint::GlobalVehicleBreaksConstraint(
    const RoutingDimension* dimension)
    : Constraint(dimension->model()->solver()),
      model_(dimension->model()),
      dimension_(dimension) {
  vehicle_demons_.resize(model_->vehicles());
}

void GlobalVehicleBreaksConstraint::Post() {
  for (int vehicle = 0; vehicle < model_->vehicles(); vehicle++) {
    if (dimension_->GetBreakIntervalsOfVehicle(vehicle).empty() &&
        dimension_->GetBreakDistanceDurationOfVehicle(vehicle).empty()) {
      continue;
    }
    vehicle_demons_[vehicle] = MakeDelayedConstraintDemon1(
        solver(), this, &GlobalVehicleBreaksConstraint::PropagateVehicle,
        "PropagateVehicle", vehicle);
    for (IntervalVar* interval :
         dimension_->GetBreakIntervalsOfVehicle(vehicle)) {
      interval->WhenAnything(vehicle_demons_[vehicle]);
    }
  }
  const int num_cumuls = dimension_->cumuls().size();
  const int num_nexts = model_->Nexts().size();
  for (int node = 0; node < num_cumuls; node++) {
    Demon* dimension_demon = MakeConstraintDemon1(
        solver(), this, &GlobalVehicleBreaksConstraint::PropagateNode,
        "PropagateNode", node);
    if (node < num_nexts) {
      model_->NextVar(node)->WhenBound(dimension_demon);
      dimension_->SlackVar(node)->WhenRange(dimension_demon);
    }
    model_->VehicleVar(node)->WhenBound(dimension_demon);
    dimension_->CumulVar(node)->WhenRange(dimension_demon);
  }
}

void GlobalVehicleBreaksConstraint::InitialPropagate() {
  for (int vehicle = 0; vehicle < model_->vehicles(); vehicle++) {
    if (!dimension_->GetBreakIntervalsOfVehicle(vehicle).empty() ||
        !dimension_->GetBreakDistanceDurationOfVehicle(vehicle).empty()) {
      PropagateVehicle(vehicle);
    }
  }
}

// This dispatches node events to the right vehicle propagator.
// It also filters out a part of uninteresting events, on which the vehicle
// propagator will not find anything new.
void GlobalVehicleBreaksConstraint::PropagateNode(int node) {
  if (!model_->VehicleVar(node)->Bound()) return;
  const int vehicle = model_->VehicleVar(node)->Min();
  if (vehicle < 0 || vehicle_demons_[vehicle] == nullptr) return;
  EnqueueDelayedDemon(vehicle_demons_[vehicle]);
}

void GlobalVehicleBreaksConstraint::FillPartialPathOfVehicle(int vehicle) {
  path_.clear();
  int current = model_->Start(vehicle);
  while (!model_->IsEnd(current)) {
    path_.push_back(current);
    current = model_->NextVar(current)->Bound()
                  ? model_->NextVar(current)->Min()
                  : model_->End(vehicle);
  }
  path_.push_back(current);
}

void GlobalVehicleBreaksConstraint::FillPathTravels(
    const std::vector<int64>& path) {
  const int num_travels = path.size() - 1;
  travel_bounds_.min_travels.resize(num_travels);
  travel_bounds_.max_travels.resize(num_travels);
  for (int i = 0; i < num_travels; ++i) {
    travel_bounds_.min_travels[i] = dimension_->FixedTransitVar(path[i])->Min();
    travel_bounds_.max_travels[i] = dimension_->FixedTransitVar(path[i])->Max();
  }
}

// First, perform energy-based reasoning on intervals and cumul variables.
// Then, perform reasoning on slack variables.
void GlobalVehicleBreaksConstraint::PropagateVehicle(int vehicle) {
  // Fill path and pre/post travel information.
  FillPartialPathOfVehicle(vehicle);
  const int num_nodes = path_.size();
  FillPathTravels(path_);
  {
    const int index = dimension_->GetPreTravelEvaluatorOfVehicle(vehicle);
    if (index == -1) {
      travel_bounds_.pre_travels.assign(num_nodes - 1, 0);
    } else {
      FillPathEvaluation(path_, model_->TransitCallback(index),
                         &travel_bounds_.pre_travels);
    }
  }
  {
    const int index = dimension_->GetPostTravelEvaluatorOfVehicle(vehicle);
    if (index == -1) {
      travel_bounds_.post_travels.assign(num_nodes - 1, 0);
    } else {
      FillPathEvaluation(path_, model_->TransitCallback(index),
                         &travel_bounds_.post_travels);
    }
  }
  // The last travel might not be fixed: in that case, relax its information.
  if (!model_->NextVar(path_[num_nodes - 2])->Bound()) {
    travel_bounds_.min_travels.back() = 0;
    travel_bounds_.max_travels.back() = kint64max;
    travel_bounds_.pre_travels.back() = 0;
    travel_bounds_.post_travels.back() = 0;
  }

  // Fill tasks from path, break intervals, and break constraints.
  tasks_.Clear();
  AppendTasksFromPath(path_, travel_bounds_, *dimension_, &tasks_);
  tasks_.num_chain_tasks = tasks_.start_min.size();
  AppendTasksFromIntervals(dimension_->GetBreakIntervalsOfVehicle(vehicle),
                           &tasks_);
  tasks_.distance_duration =
      dimension_->GetBreakDistanceDurationOfVehicle(vehicle);

  // Do the actual reasoning, no need to continue if infeasible.
  if (!disjunctive_propagator_.Propagate(&tasks_)) solver()->Fail();

  // Make task translators to help set new bounds of CP variables.
  task_translators_.clear();
  for (int i = 0; i < num_nodes; ++i) {
    const int64 before_visit =
        (i == 0) ? 0 : travel_bounds_.post_travels[i - 1];
    const int64 after_visit =
        (i == num_nodes - 1) ? 0 : travel_bounds_.pre_travels[i];
    task_translators_.emplace_back(dimension_->CumulVar(path_[i]), before_visit,
                                   after_visit);
    if (i == num_nodes - 1) break;
    task_translators_.emplace_back();  // Dummy translator for travel tasks.
  }
  for (IntervalVar* interval :
       dimension_->GetBreakIntervalsOfVehicle(vehicle)) {
    if (!interval->MustBePerformed()) continue;
    task_translators_.emplace_back(interval);
  }

  // Push new bounds to CP variables.
  const int num_tasks = tasks_.start_min.size();
  for (int task = 0; task < num_tasks; ++task) {
    task_translators_[task].SetStartMin(tasks_.start_min[task]);
    task_translators_[task].SetStartMax(tasks_.start_max[task]);
    task_translators_[task].SetDurationMin(tasks_.duration_min[task]);
    task_translators_[task].SetEndMin(tasks_.end_min[task]);
    task_translators_[task].SetEndMax(tasks_.end_max[task]);
  }

  // Reasoning on slack variables: when intervals must be inside an arc,
  // that arc's slack must be large enough to accommodate for those.
  // TODO(user): Make a version more efficient than O(n^2).
  if (dimension_->GetBreakIntervalsOfVehicle(vehicle).empty()) return;
  // If the last arc of the path was not bound, do not change slack.
  const int64 last_bound_arc =
      num_nodes - 2 - (model_->NextVar(path_[num_nodes - 2])->Bound() ? 0 : 1);
  for (int i = 0; i <= last_bound_arc; ++i) {
    const int64 arc_start_max =
        CapSub(dimension_->CumulVar(path_[i])->Max(),
               i > 0 ? travel_bounds_.post_travels[i - 1] : 0);
    const int64 arc_end_min =
        CapAdd(dimension_->CumulVar(path_[i + 1])->Min(),
               i < num_nodes - 2 ? travel_bounds_.pre_travels[i + 1] : 0);
    int64 total_break_inside_arc = 0;
    for (IntervalVar* interval :
         dimension_->GetBreakIntervalsOfVehicle(vehicle)) {
      if (!interval->MustBePerformed()) continue;
      const int64 interval_start_max = interval->StartMax();
      const int64 interval_end_min = interval->EndMin();
      const int64 interval_duration_min = interval->DurationMin();
      // If interval cannot end before the arc's from node and
      // cannot start after the 'to' node, then it must be inside the arc.
      if (arc_start_max < interval_end_min &&
          interval_start_max < arc_end_min) {
        total_break_inside_arc += interval_duration_min;
      }
    }
    dimension_->SlackVar(path_[i])->SetMin(total_break_inside_arc);
  }
  // Reasoning on optional intervals.
  // TODO(user): merge this with energy-based reasoning.
  // If there is no optional interval, skip the rest of this function.
  {
    bool has_optional = false;
    for (const IntervalVar* interval :
         dimension_->GetBreakIntervalsOfVehicle(vehicle)) {
      if (interval->MayBePerformed() && !interval->MustBePerformed()) {
        has_optional = true;
        break;
      }
    }
    if (!has_optional) return;
  }
  const std::vector<IntervalVar*>& break_intervals =
      dimension_->GetBreakIntervalsOfVehicle(vehicle);
  for (int pos = 0; pos < num_nodes - 1; ++pos) {
    const int64 current_slack_max = dimension_->SlackVar(path_[pos])->Max();
    const int64 visit_start_offset =
        pos > 0 ? travel_bounds_.post_travels[pos - 1] : 0;
    const int64 visit_start_max =
        CapSub(dimension_->CumulVar(path_[pos])->Max(), visit_start_offset);
    const int64 visit_end_offset =
        (pos < num_nodes - 1) ? travel_bounds_.pre_travels[pos] : 0;
    const int64 visit_end_min =
        CapAdd(dimension_->CumulVar(path_[pos])->Min(), visit_end_offset);

    for (IntervalVar* interval : break_intervals) {
      if (!interval->MayBePerformed()) continue;
      const bool interval_is_performed = interval->MustBePerformed();
      const int64 interval_start_max = interval->StartMax();
      const int64 interval_end_min = interval->EndMin();
      const int64 interval_duration_min = interval->DurationMin();
      // When interval cannot fit inside current arc,
      // do disjunctive reasoning on full arc.
      if (pos < num_nodes - 1 && interval_duration_min > current_slack_max) {
        // The arc lasts from CumulVar(path_[pos]) - post_travel_[pos] to
        // CumulVar(path_[pos+1]) + pre_travel_[pos+1].
        const int64 arc_start_offset =
            pos > 0 ? travel_bounds_.post_travels[pos - 1] : 0;
        const int64 arc_start_max = visit_start_max;
        const int64 arc_end_offset =
            (pos < num_nodes - 2) ? travel_bounds_.pre_travels[pos + 1] : 0;
        const int64 arc_end_min =
            CapAdd(dimension_->CumulVar(path_[pos + 1])->Min(), arc_end_offset);
        // Interval not before.
        if (arc_start_max < interval_end_min) {
          interval->SetStartMin(arc_end_min);
          if (interval_is_performed) {
            dimension_->CumulVar(path_[pos + 1])
                ->SetMax(CapSub(interval_start_max, arc_end_offset));
          }
        }
        // Interval not after.
        if (interval_start_max < arc_end_min) {
          interval->SetEndMax(arc_start_max);
          if (interval_is_performed) {
            dimension_->CumulVar(path_[pos])
                ->SetMin(CapSub(interval_end_min, arc_start_offset));
          }
        }
        continue;
      }
      // Interval could fit inside arc: do disjunctive reasoning between
      // interval and visit.
      // Interval not before.
      if (visit_start_max < interval_end_min) {
        interval->SetStartMin(visit_end_min);
        if (interval_is_performed) {
          dimension_->CumulVar(path_[pos])
              ->SetMax(CapSub(interval_start_max, visit_end_offset));
        }
      }
      // Interval not after.
      if (interval_start_max < visit_end_min) {
        interval->SetEndMax(visit_start_max);
        if (interval_is_performed) {
          dimension_->CumulVar(path_[pos])
              ->SetMin(CapAdd(interval_end_min, visit_start_offset));
        }
      }
    }
  }
}

namespace {
class VehicleBreaksFilter : public BasePathFilter {
 public:
  VehicleBreaksFilter(const RoutingModel& routing_model,
                      const RoutingDimension& dimension);
  std::string DebugString() const override { return "VehicleBreaksFilter"; }
  bool AcceptPath(int64 path_start, int64 chain_start,
                  int64 chain_end) override;

 private:
  // Fills path_ with the path of vehicle, start to end.
  void FillPathOfVehicle(int64 vehicle);
  std::vector<int64> path_;
  // Handles to model.
  const RoutingModel& model_;
  const RoutingDimension& dimension_;
  // Strong energy-based filtering algorithm.
  DisjunctivePropagator disjunctive_propagator_;
  DisjunctivePropagator::Tasks tasks_;
  // Used to check whether propagation changed a vector.
  std::vector<int64> old_start_min_;
  std::vector<int64> old_start_max_;
  std::vector<int64> old_end_min_;
  std::vector<int64> old_end_max_;

  std::vector<int> start_to_vehicle_;
  TravelBounds travel_bounds_;
};

VehicleBreaksFilter::VehicleBreaksFilter(const RoutingModel& routing_model,
                                         const RoutingDimension& dimension)
    : BasePathFilter(routing_model.Nexts(),
                     routing_model.Size() + routing_model.vehicles()),
      model_(routing_model),
      dimension_(dimension) {
  DCHECK(dimension_.HasBreakConstraints());
  start_to_vehicle_.resize(Size(), -1);
  for (int i = 0; i < routing_model.vehicles(); ++i) {
    start_to_vehicle_[routing_model.Start(i)] = i;
  }
}

void VehicleBreaksFilter::FillPathOfVehicle(int64 vehicle) {
  path_.clear();
  int current = model_.Start(vehicle);
  while (!model_.IsEnd(current)) {
    path_.push_back(current);
    current = GetNext(current);
  }
  path_.push_back(current);
}

bool VehicleBreaksFilter::AcceptPath(int64 path_start, int64 chain_start,
                                     int64 chain_end) {
  const int vehicle = start_to_vehicle_[path_start];
  if (dimension_.GetBreakIntervalsOfVehicle(vehicle).empty() &&
      dimension_.GetBreakDistanceDurationOfVehicle(vehicle).empty()) {
    return true;
  }
  // Fill path and pre/post travel information.
  FillPathOfVehicle(vehicle);
  FillTravelBoundsOfVehicle(vehicle, path_, dimension_, &travel_bounds_);
  // Fill tasks from path, forbidden intervals, breaks and break constraints.
  tasks_.Clear();
  AppendTasksFromPath(path_, travel_bounds_, dimension_, &tasks_);
  tasks_.num_chain_tasks = tasks_.start_min.size();
  AppendTasksFromIntervals(dimension_.GetBreakIntervalsOfVehicle(vehicle),
                           &tasks_);
  // Add forbidden intervals only if a node has some.
  tasks_.forbidden_intervals.clear();
  if (std::any_of(path_.begin(), path_.end(), [this](int64 node) {
        return dimension_.forbidden_intervals()[node].NumIntervals() > 0;
      })) {
    tasks_.forbidden_intervals.assign(tasks_.start_min.size(), nullptr);
    for (int i = 0; i < path_.size(); ++i) {
      tasks_.forbidden_intervals[2 * i] =
          &(dimension_.forbidden_intervals()[path_[i]]);
    }
  }
  // Max distance duration constraint.
  tasks_.distance_duration =
      dimension_.GetBreakDistanceDurationOfVehicle(vehicle);

  // Reduce bounds until failure or fixed point is reached.
  // We set a maximum amount of iterations to avoid slow propagation.
  bool is_feasible = true;
  int maximum_num_iterations = 8;
  while (--maximum_num_iterations >= 0) {
    old_start_min_ = tasks_.start_min;
    old_start_max_ = tasks_.start_max;
    old_end_min_ = tasks_.end_min;
    old_end_max_ = tasks_.end_max;
    is_feasible = disjunctive_propagator_.Propagate(&tasks_);
    if (!is_feasible) break;
    // If fixed point reached, stop.
    if ((old_start_min_ == tasks_.start_min) &&
        (old_start_max_ == tasks_.start_max) &&
        (old_end_min_ == tasks_.end_min) && (old_end_max_ == tasks_.end_max)) {
      break;
    }
  }
  return is_feasible;
}

}  // namespace

IntVarLocalSearchFilter* MakeVehicleBreaksFilter(
    const RoutingModel& routing_model, const RoutingDimension& dimension) {
  return routing_model.solver()->RevAlloc(
      new VehicleBreaksFilter(routing_model, dimension));
}

}  // namespace operations_research
