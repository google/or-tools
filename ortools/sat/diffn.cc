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

#include "ortools/sat/diffn.h"

#include <algorithm>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/str_join.h"
#include "ortools/base/iterator_adaptors.h"
#include "ortools/base/map_util.h"
#include "ortools/sat/disjunctive.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/theta_tree.h"
#include "ortools/util/sort.h"

namespace operations_research {
namespace sat {

NonOverlappingRectanglesPropagator::NonOverlappingRectanglesPropagator(
    const std::vector<IntervalVariable>& x,
    const std::vector<IntervalVariable>& y, bool strict, Model* model,
    IntegerTrail* integer_trail)
    : num_boxes_(x.size()),
      x_(x, model),
      y_(y, model),
      strict_(strict),
      integer_trail_(integer_trail) {
  CHECK_GT(num_boxes_, 0);
}

NonOverlappingRectanglesPropagator::~NonOverlappingRectanglesPropagator() {}

#define RETURN_IF_FALSE(f) \
  if (!(f)) return false;

bool NonOverlappingRectanglesPropagator::Propagate() {
  cached_areas_.resize(num_boxes_);
  for (int box = 0; box < num_boxes_; ++box) {
    // We assume that the min-size of a box never changes.
    cached_areas_[box] = x_.DurationMin(box) * y_.DurationMin(box);
  }

  x_.SetTimeDirection(true);
  y_.SetTimeDirection(true);

  for (int box = 0; box < num_boxes_; ++box) {
    if (cached_areas_[box] == 0) continue;
    RETURN_IF_FALSE(FailWhenEnergyIsTooLarge(box));
  }

  RETURN_IF_FALSE(FindBoxesThatMustOverlapAHorizontalLine(&x_, &y_));
  // We can actually swap dimensions to propagate vertically.
  RETURN_IF_FALSE(FindBoxesThatMustOverlapAHorizontalLine(&y_, &x_));

  return true;
}

void NonOverlappingRectanglesPropagator::RegisterWith(
    GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  x_.WatchAllTasks(id, watcher);
  y_.WatchAllTasks(id, watcher);
  watcher->SetPropagatorPriority(id, 3);
}

// ----- Global energetic area reasoning -----

namespace {

// Computes the length of the convex hull of two intervals.
IntegerValue MaxSpan(IntegerValue min_a, IntegerValue max_a, IntegerValue min_b,
                     IntegerValue max_b) {
  return std::max(max_a, max_b) - std::min(min_a, min_b) + 1;
}

}  // namespace

void NonOverlappingRectanglesPropagator::SortNeighbors(int box) {
  cached_distance_to_bounding_box_.resize(num_boxes_);
  neighbors_.clear();
  const IntegerValue box_x_min = x_.StartMin(box);
  const IntegerValue box_x_max = x_.EndMax(box);
  const IntegerValue box_y_min = y_.StartMin(box);
  const IntegerValue box_y_max = y_.EndMax(box);

  for (int other_box = 0; other_box < num_boxes_; ++other_box) {
    if (other_box == box) continue;
    if (cached_areas_[other_box] == 0) continue;

    const IntegerValue other_x_min = x_.StartMin(other_box);
    const IntegerValue other_x_max = x_.EndMax(other_box);
    const IntegerValue other_y_min = y_.StartMin(other_box);
    const IntegerValue other_y_max = y_.EndMax(other_box);

    neighbors_.push_back(other_box);
    cached_distance_to_bounding_box_[other_box] =
        MaxSpan(box_x_min, box_x_max, other_x_min, other_x_max) *
        MaxSpan(box_y_min, box_y_max, other_y_min, other_y_max);
  }
  IncrementalSort(neighbors_.begin(), neighbors_.begin(), [this](int i, int j) {
    return cached_distance_to_bounding_box_[i] <
           cached_distance_to_bounding_box_[j];
  });
}

bool NonOverlappingRectanglesPropagator::FailWhenEnergyIsTooLarge(int box) {
  // Note that we only consider the smallest dimension of each boxes here.

  SortNeighbors(box);
  IntegerValue area_min_x = x_.StartMin(box);
  IntegerValue area_max_x = x_.EndMax(box);
  IntegerValue area_min_y = y_.StartMin(box);
  IntegerValue area_max_y = y_.EndMax(box);

  IntegerValue sum_of_areas = cached_areas_[box];

  IntegerValue total_sum_of_areas = sum_of_areas;
  for (const int other_box : neighbors_) {
    total_sum_of_areas += cached_areas_[other_box];
  }

  const auto add_box_energy_in_rectangle_reason = [&](int b) {
    x_.AddStartMinReason(b, area_min_x);
    x_.AddDurationMinReason(b, x_.DurationMin(b));
    x_.AddEndMaxReason(b, area_max_x);
    y_.AddStartMinReason(b, area_min_y);
    y_.AddDurationMinReason(b, y_.DurationMin(b));
    y_.AddEndMaxReason(b, area_max_y);
  };

  for (int i = 0; i < neighbors_.size(); ++i) {
    const int other_box = neighbors_[i];
    CHECK_GT(cached_areas_[other_box], 0);

    // Update Bounding box.
    area_min_x = std::min(area_min_x, x_.StartMin(other_box));
    area_max_x = std::max(area_max_x, x_.EndMax(other_box));
    area_min_y = std::min(area_min_y, y_.StartMin(other_box));
    area_max_y = std::max(area_max_y, y_.EndMax(other_box));

    // Update sum of areas.
    sum_of_areas += cached_areas_[other_box];
    const IntegerValue bounding_area =
        (area_max_x - area_min_x) * (area_max_y - area_min_y);
    if (bounding_area >= total_sum_of_areas) {
      // Nothing will be deduced. Exiting.
      return true;
    }

    if (sum_of_areas > bounding_area) {
      x_.ClearReason();
      y_.ClearReason();
      add_box_energy_in_rectangle_reason(box);
      for (int j = 0; j <= i; ++j) {
        add_box_energy_in_rectangle_reason(neighbors_[j]);
      }
      x_.ImportOtherReasons(y_);
      return x_.ReportConflict();
    }
  }
  return true;
}

// ----- disjunctive reasoning on overlapping boxes on one dimension -----

namespace {

// TODO(user): When the code is stabilized, reuse disjunctive code.
bool CheckOverload(IntegerValue y_line_for_reason,
                   const absl::flat_hash_set<int>& boxes,
                   SchedulingConstraintHelper* x_dim,
                   SchedulingConstraintHelper* y_dim) {
  ThetaLambdaTree<IntegerValue> theta_tree;
  std::vector<int> task_to_event;
  std::vector<int> event_to_task;
  std::vector<IntegerValue> event_time;
  task_to_event.resize(x_dim->NumTasks());

  x_dim->SetTimeDirection(true);
  y_dim->SetTimeDirection(true);

  // Set up theta tree.
  event_to_task.clear();
  event_time.clear();
  int num_events = 0;

  for (const auto task_time : x_dim->TaskByIncreasingShiftedStartMin()) {
    const int task = task_time.task_index;

    // TODO(user): We need to take into account task with zero duration because
    // in this constraint, such a task cannot be overlapped by other. However,
    // we currently use the fact that the energy min is zero to detect that a
    // task is present and non-optional in the theta_tree. Fix.
    if (x_dim->IsAbsent(task) || x_dim->DurationMin(task) == 0 ||
        !boxes.contains(task)) {
      task_to_event[task] = -1;
      continue;
    }
    event_to_task.push_back(task);
    event_time.push_back(task_time.time);
    task_to_event[task] = num_events;
    num_events++;
  }
  theta_tree.Reset(num_events);

  // Introduce events by non-decreasing end_max, check for overloads.
  // Note, currently, we do not support optional boxes.
  for (const auto task_time :
       ::gtl::reversed_view(x_dim->TaskByDecreasingEndMax())) {
    const int current_task = task_time.task_index;
    if (task_to_event[current_task] == -1) continue;

    {
      const int current_event = task_to_event[current_task];
      const IntegerValue energy_min = x_dim->DurationMin(current_task);
      theta_tree.AddOrUpdateEvent(current_event, event_time[current_event],
                                  energy_min, energy_min);
    }

    const IntegerValue current_end = task_time.time;
    if (theta_tree.GetEnvelope() > current_end) {
      // Explain failure with tasks in critical interval.
      x_dim->ClearReason();
      y_dim->ClearReason();
      const int critical_event =
          theta_tree.GetMaxEventWithEnvelopeGreaterThan(current_end);
      const IntegerValue window_start = event_time[critical_event];
      const IntegerValue window_end =
          theta_tree.GetEnvelopeOf(critical_event) - 1;
      for (int event = critical_event; event < num_events; event++) {
        const IntegerValue energy_min = theta_tree.EnergyMin(event);
        if (energy_min > 0) {
          const int task = event_to_task[event];
          x_dim->AddEnergyAfterReason(task, energy_min, window_start);
          x_dim->AddEndMaxReason(task, window_end);
          y_dim->AddStartMaxReason(task, y_line_for_reason);
          y_dim->AddEndMinReason(task, y_line_for_reason + 1);
        }
      }
      x_dim->ImportOtherReasons(*y_dim);
      return x_dim->ReportConflict();
    }
  }
  return true;
}

bool DetectPrecedences(bool time_direction, IntegerValue y_line_for_reason,
                       const absl::flat_hash_set<int>& active_boxes,
                       SchedulingConstraintHelper* x_dim,
                       SchedulingConstraintHelper* y_dim) {
  x_dim->SetTimeDirection(time_direction);
  y_dim->SetTimeDirection(true);

  const auto& task_by_increasing_end_min = x_dim->TaskByIncreasingEndMin();
  const auto& task_by_decreasing_start_max = x_dim->TaskByDecreasingStartMax();

  const int num_tasks = x_dim->NumTasks();
  int queue_index = num_tasks - 1;
  TaskSet task_set(num_tasks);
  for (const auto task_time : task_by_increasing_end_min) {
    const int t = task_time.task_index;
    const IntegerValue end_min = task_time.time;

    if (x_dim->IsAbsent(t) || !gtl::ContainsKey(active_boxes, t)) continue;

    while (queue_index >= 0) {
      const auto to_insert = task_by_decreasing_start_max[queue_index];
      const int task_index = to_insert.task_index;
      const IntegerValue start_max = to_insert.time;
      if (end_min <= start_max) break;

      if (gtl::ContainsKey(active_boxes, task_index)) {
        CHECK(x_dim->IsPresent(task_index));
        task_set.AddEntry({task_index, x_dim->ShiftedStartMin(task_index),
                           x_dim->DurationMin(task_index)});
      }
      --queue_index;
    }

    // task_set contains all the tasks that must be executed before t.
    // They are in "detectable precedence" because their start_max is smaller
    // than the end-min of t like so:
    //          [(the task t)
    //                     (a task in task_set)]
    // From there, we deduce that the start-min of t is greater or equal to the
    // end-min of the critical tasks.
    int critical_index = 0;
    const IntegerValue end_min_of_critical_tasks =
        task_set.ComputeEndMin(/*task_to_ignore=*/t, &critical_index);
    if (end_min_of_critical_tasks > x_dim->StartMin(t)) {
      const std::vector<TaskSet::Entry>& sorted_tasks = task_set.SortedTasks();
      x_dim->ClearReason();
      y_dim->ClearReason();

      // We need:
      // - StartMax(ct) < EndMin(t) for the detectable precedence.
      // - StartMin(ct) > window_start for the end_min_of_critical_tasks reason.
      const IntegerValue window_start = sorted_tasks[critical_index].start_min;
      std::vector<int> tasks(sorted_tasks.size() - critical_index);
      for (int i = critical_index; i < sorted_tasks.size(); ++i) {
        tasks[i - critical_index] = sorted_tasks[i].task;
      }

      for (int i = critical_index; i < sorted_tasks.size(); ++i) {
        const int ct = sorted_tasks[i].task;
        if (ct == t) continue;
        CHECK(gtl::ContainsKey(active_boxes, ct));
        CHECK(x_dim->IsPresent(ct));
        x_dim->AddEnergyAfterReason(ct, sorted_tasks[i].duration_min,
                                    window_start);
        x_dim->AddStartMaxReason(ct, end_min - 1);
        y_dim->AddStartMaxReason(ct, y_line_for_reason);
        y_dim->AddEndMinReason(ct, y_line_for_reason + 1);
      }

      // Add the reason for t (we only need the end-min).
      x_dim->AddEndMinReason(t, end_min);
      y_dim->AddStartMaxReason(t, y_line_for_reason);
      y_dim->AddEndMinReason(t, y_line_for_reason + 1);

      // Import reasons from the 'y_dim' dimension.
      x_dim->ImportOtherReasons(*y_dim);

      // This augment the start-min of t and subsequently it can augment the
      // next end_min_of_critical_tasks, but our deduction is still valid.
      if (!x_dim->IncreaseStartMin(t, end_min_of_critical_tasks)) {
        return false;
      }

      // We need to reorder t inside task_set. Note that if t is in the set,
      // it means that the task is present and that IncreaseStartMin() did push
      // its start (by opposition to an optional interval where the push might
      // not happen if its start is not optional).
      task_set.NotifyEntryIsNowLastIfPresent(
          {t, x_dim->ShiftedStartMin(t), x_dim->DurationMin(t)});
    }
  }
  return true;
}

bool NotLast(bool time_direction, IntegerValue y_line_for_reason,
             const absl::flat_hash_set<int>& boxes,
             SchedulingConstraintHelper* x_dim,
             SchedulingConstraintHelper* y_dim) {
  x_dim->SetTimeDirection(time_direction);
  y_dim->SetTimeDirection(true);
  const auto& task_by_decreasing_start_max =
      x_dim->TaskByDecreasingStartMax();

  const int num_tasks = x_dim->NumTasks();
  int queue_index = num_tasks - 1;

  TaskSet task_set(num_tasks);
  const auto& task_by_increasing_end_max =
      ::gtl::reversed_view(x_dim->TaskByDecreasingEndMax());
  for (const auto task_time : task_by_increasing_end_max) {
    const int t = task_time.task_index;
    const IntegerValue end_max = task_time.time;

    if (x_dim->IsAbsent(t) || !boxes.contains(t)) continue;

    // task_set contains all the tasks that must start before the end-max of t.
    // These are the only candidates that have a chance to decrease the end-max
    // of t.
    while (queue_index >= 0) {
      const auto to_insert = task_by_decreasing_start_max[queue_index];
      const int task_index = to_insert.task_index;
      const IntegerValue start_max = to_insert.time;
      if (end_max <= start_max) break;
      if (boxes.contains(task_index)) {
        CHECK(x_dim->IsPresent(task_index));
        task_set.AddEntry({task_index, x_dim->ShiftedStartMin(task_index),
                           x_dim->DurationMin(task_index)});
      }
      --queue_index;
    }

    // In the following case, task t cannot be after all the critical tasks
    // (i.e. it cannot be last):
    //
    // [(critical tasks)
    //              | <- t start-max
    //
    // So we can deduce that the end-max of t is smaller than or equal to the
    // largest start-max of the critical tasks.
    //
    // Note that this works as well when task_is_currently_present_[t] is false.
    int critical_index = 0;
    const IntegerValue end_min_of_critical_tasks =
        task_set.ComputeEndMin(/*task_to_ignore=*/t, &critical_index);
    if (end_min_of_critical_tasks <= x_dim->StartMax(t)) continue;

    // Find the largest start-max of the critical tasks (excluding t). The
    // end-max for t need to be smaller than or equal to this.
    IntegerValue largest_ct_start_max = kMinIntegerValue;
    const std::vector<TaskSet::Entry>& sorted_tasks = task_set.SortedTasks();
    const int sorted_tasks_size = sorted_tasks.size();
    for (int i = critical_index; i < sorted_tasks_size; ++i) {
      const int ct = sorted_tasks[i].task;
      if (t == ct) continue;
      const IntegerValue start_max = x_dim->StartMax(ct);
      if (start_max > largest_ct_start_max) {
        largest_ct_start_max = start_max;
      }
    }

    // If we have any critical task, the test will always be true because
    // of the tasks we put in task_set.
    DCHECK(largest_ct_start_max == kMinIntegerValue ||
           end_max > largest_ct_start_max);
    if (end_max > largest_ct_start_max) {
      x_dim->ClearReason();
      y_dim->ClearReason();

      const IntegerValue window_start = sorted_tasks[critical_index].start_min;

      std::vector<int> tasks(sorted_tasks.size() - critical_index);
      for (int i = critical_index; i < sorted_tasks.size(); ++i) {
        tasks[i - critical_index] = sorted_tasks[i].task;
      }

      for (int i = critical_index; i < sorted_tasks_size; ++i) {
        const int ct = sorted_tasks[i].task;

        CHECK(gtl::ContainsKey(boxes, ct));
        CHECK(x_dim->IsPresent(ct));
        if (ct == t) continue;

        x_dim->AddEnergyAfterReason(ct, sorted_tasks[i].duration_min,
                                      window_start);
        x_dim->AddStartMaxReason(ct, largest_ct_start_max);
        y_dim->AddStartMaxReason(ct, y_line_for_reason);
        y_dim->AddEndMinReason(ct, y_line_for_reason + 1);
      }

      // Add the reason for t, we only need the start-max.
      x_dim->AddStartMaxReason(t, end_min_of_critical_tasks - 1);
      y_dim->AddStartMaxReason(t, y_line_for_reason);
      y_dim->AddEndMinReason(t, y_line_for_reason + 1);

      // Import reasons from the 'y_dim' dimension.
      x_dim->ImportOtherReasons(*y_dim);

      // Enqueue the new end-max for t.
      // Note that changing it will not influence the rest of the loop.
      if (!x_dim->DecreaseEndMax(t, largest_ct_start_max)) return false;
    }
  }
  return true;
}

// Specialized propagation on only two boxes that must intersect with the given
// y_line_for_reason.
bool PropagateTwoBoxes(IntegerValue y_line_for_reason, int b1, int b2,
                       SchedulingConstraintHelper* x_dim,
                       SchedulingConstraintHelper* y_dim) {
  // For each direction and each order, we test if the boxes can be disjoint.
  const int state = (x_dim->EndMin(b1) <= x_dim->StartMax(b2)) +
                    2 * (x_dim->EndMin(b2) <= x_dim->StartMax(b1));

  const auto left_box_before_right_box = [](int left, int right,
                                            SchedulingConstraintHelper* x_dim) {
    // left box pushes right box.
    const IntegerValue left_end_min = x_dim->EndMin(left);
    if (left_end_min > x_dim->StartMin(right)) {
      // Store reasons state.
      const int literal_size = x_dim->MutableLiteralReason()->size();
      const int integer_size = x_dim->MutableIntegerReason()->size();

      x_dim->AddEndMinReason(left, left_end_min);
      if (!x_dim->IncreaseStartMin(right, left_end_min)) {
        return false;
      }

      // Restore the reasons to the state before the increase of the start.
      x_dim->MutableLiteralReason()->resize(literal_size);
      x_dim->MutableIntegerReason()->resize(integer_size);
    }

    // right box pushes left box.
    const IntegerValue right_start_max = x_dim->StartMax(right);
    if (right_start_max < x_dim->EndMax(left)) {
      x_dim->AddStartMaxReason(right, right_start_max);
      return x_dim->DecreaseEndMax(left, right_start_max);
    }

    return true;
  };

  // Clean up reasons.
  x_dim->ClearReason();
  y_dim->ClearReason();

  switch (state) {
    case 0: {  // Conflict.
      x_dim->AddReasonForBeingBefore(b1, b2);
      x_dim->AddReasonForBeingBefore(b2, b1);
      y_dim->AddStartMaxReason(b1, y_line_for_reason);
      y_dim->AddEndMinReason(b1, y_line_for_reason + 1);
      y_dim->AddStartMaxReason(b2, y_line_for_reason);
      y_dim->AddEndMinReason(b2, y_line_for_reason + 1);
      x_dim->ImportOtherReasons(*y_dim);
      return x_dim->ReportConflict();
    }
    case 1: {  // b1 is left of b2.
      y_dim->AddStartMaxReason(b1, y_line_for_reason);
      y_dim->AddEndMinReason(b1, y_line_for_reason + 1);
      y_dim->AddStartMaxReason(b2, y_line_for_reason);
      y_dim->AddEndMinReason(b2, y_line_for_reason + 1);
      x_dim->AddReasonForBeingBefore(b1, b2);
      x_dim->ImportOtherReasons(*y_dim);
      return left_box_before_right_box(b1, b2, x_dim);
    }
    case 2: {  // b2 is left of b1.
      y_dim->AddStartMaxReason(b1, y_line_for_reason);
      y_dim->AddEndMinReason(b1, y_line_for_reason + 1);
      y_dim->AddStartMaxReason(b2, y_line_for_reason);
      y_dim->AddEndMinReason(b2, y_line_for_reason + 1);
      x_dim->AddReasonForBeingBefore(b2, b1);
      x_dim->ImportOtherReasons(*y_dim);
      return left_box_before_right_box(b2, b1, x_dim);
    }
    default: {  // Nothing to deduce.
      return true;
    }
  }
}

// We maximize the number of trailing bits set to 0 within a range.
IntegerValue FindCanonicalValue(IntegerValue lb, IntegerValue ub) {
  if (lb == ub) return lb;
  if (lb <= 0 && ub > 0) return IntegerValue(0);
  if (lb < 0 && ub <= 0) {
    return -FindCanonicalValue(-ub, -lb);
  }

  int64 mask = 0;
  IntegerValue candidate = ub;
  for (int o = 0; o < 62; ++o) {
    mask = 2 * mask + 1;
    const IntegerValue masked_ub(ub.value() & ~mask);
    if (masked_ub >= lb) {
      candidate = masked_ub;
    } else {
      break;
    }
  }
  return candidate;
}

}  // namespace

bool NonOverlappingRectanglesPropagator::
    FindBoxesThatMustOverlapAHorizontalLine(SchedulingConstraintHelper* x_dim,
                                            SchedulingConstraintHelper* y_dim) {
  x_dim->SetTimeDirection(true);
  y_dim->SetTimeDirection(true);
  std::map<IntegerValue, std::vector<int>> event_to_overlapping_boxes;
  std::set<IntegerValue> events;

  for (int box = 0; box < num_boxes_; ++box) {
    if (cached_areas_[box] == 0) continue;
    const IntegerValue start_max = y_dim->StartMax(box);
    const IntegerValue end_min = y_dim->EndMin(box);
    if (start_max < end_min) {
      events.insert(start_max);
    }
  }

  for (int box = 0; box < num_boxes_; ++box) {
    if (cached_areas_[box] == 0) continue;
    const IntegerValue start_max = y_dim->StartMax(box);
    const IntegerValue end_min = y_dim->EndMin(box);

    if (start_max < end_min) {
      for (const IntegerValue t : events) {
        if (t < start_max) continue;
        if (t >= end_min) break;
        event_to_overlapping_boxes[t].push_back(box);
      }
    }
  }

  std::set<IntegerValue> events_to_remove;
  std::vector<int> previous_overlapping_boxes;
  IntegerValue previous_event(-1);
  for (const auto& it : event_to_overlapping_boxes) {
    const IntegerValue current_event = it.first;
    const std::vector<int>& current_overlapping_boxes = it.second;
    if (current_overlapping_boxes.size() < 2) {
      events_to_remove.insert(current_event);
    }
    if (!previous_overlapping_boxes.empty()) {
      if (std::includes(previous_overlapping_boxes.begin(),
                        previous_overlapping_boxes.end(),
                        current_overlapping_boxes.begin(),
                        current_overlapping_boxes.end())) {
        events_to_remove.insert(current_event);
      }
      previous_event = current_event;
      previous_overlapping_boxes = current_overlapping_boxes;
    }
  }

  for (const IntegerValue event : events_to_remove) {
    event_to_overlapping_boxes.erase(event);
  }

  for (const auto& it : event_to_overlapping_boxes) {
    const std::vector<int>& boxes = it.second;
    // Collect the common overlapping coordinates of all boxes.
    IntegerValue lb(kint64min);
    IntegerValue ub(kint64max);
    for (const int box : boxes) {
      lb = std::max(lb, y_dim->StartMax(box));
      ub = std::min(ub, y_dim->EndMin(box) - 1);
    }
    CHECK_LE(lb, ub);

    // TODO(user): We should scan the integer trail to find the oldest
    // non-empty common interval. Then we can pick the canonical value within
    // it.

    // We want for different propagation to reuse as much as possible the same
    // line. The idea behind this is to compute the 'canonical' line to use when
    // explaining that boxes overlap on the 'y_dim' dimension. We compute the
    // multiple of the biggest power of two that is common to all boxes.
    const IntegerValue line_to_use_for_reason = FindCanonicalValue(lb, ub);

    RETURN_IF_FALSE(PropagateBoxesOvelappingAHorizontalLine(
        line_to_use_for_reason, it.second, x_dim, y_dim));
  }
  return true;
}

bool NonOverlappingRectanglesPropagator::
    PropagateBoxesOvelappingAHorizontalLine(IntegerValue y_line_for_reason,
                                            const std::vector<int>& boxes,
                                            SchedulingConstraintHelper* x_dim,
                                            SchedulingConstraintHelper* y_dim) {
  if (boxes.size() == 2) {
    // In that case, we can use simpler algorithms.
    // Note that this case happens frequently (~30% of all calls to this method
    // according to our tests).
    return PropagateTwoBoxes(y_line_for_reason, boxes[0], boxes[1], x_dim, y_dim);
  }

  const absl::flat_hash_set<int> active_boxes(boxes.begin(), boxes.end());
  RETURN_IF_FALSE(CheckOverload(y_line_for_reason, active_boxes, x_dim, y_dim));
  RETURN_IF_FALSE(
      DetectPrecedences(true, y_line_for_reason, active_boxes, x_dim, y_dim));
  RETURN_IF_FALSE(
      DetectPrecedences(false, y_line_for_reason, active_boxes, x_dim, y_dim));
  RETURN_IF_FALSE(
      NotLast(false, y_line_for_reason, active_boxes, x_dim, y_dim));
  RETURN_IF_FALSE(
      NotLast(true, y_line_for_reason, active_boxes, x_dim, y_dim));
  return true;
}

#undef RETURN_IF_FALSE

}  // namespace sat
}  // namespace operations_research
