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

  return PropagateOnProjections();
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

// Returns true iff the 2 given intervals are disjoint. If their union is one
// point, this also returns true.
bool IntervalAreDisjointForSure(IntegerValue min_a, IntegerValue max_a,
                                IntegerValue min_b, IntegerValue max_b) {
  return min_a >= max_b || min_b >= max_a;
}

// Returns the distance from interval a to the "bounding interval" of a and b.
IntegerValue DistanceToBoundingInterval(IntegerValue min_a, IntegerValue max_a,
                                        IntegerValue min_b,
                                        IntegerValue max_b) {
  return std::max(min_a - min_b, max_b - max_a);
}

}  // namespace

void NonOverlappingRectanglesPropagator::SortNeighbors(int box) {
  cached_distance_to_bounding_box_.resize(num_boxes_);
  neighbors_.clear();
  const IntegerValue box_x_min = x_.StartMin(box);
  const IntegerValue box_x_max = x_.EndMax(box);
  const IntegerValue box_y_min = y_.StartMin(box);
  const IntegerValue box_y_max = y_.EndMax(box);

  for (int other = 0; other < num_boxes_; ++other) {
    if (other == box) continue;
    if (cached_areas_[other] == 0) continue;

    const IntegerValue other_x_min = x_.StartMin(other);
    const IntegerValue other_x_max = x_.EndMax(other);
    const IntegerValue other_y_min = y_.StartMin(other);
    const IntegerValue other_y_max = y_.EndMax(other);

    neighbors_.push_back(other);
    cached_distance_to_bounding_box_[other] =
        std::max(DistanceToBoundingInterval(box_x_min, box_x_max, other_x_min,
                                            other_x_max),
                 DistanceToBoundingInterval(box_y_min, box_y_max, other_y_min,
                                            other_y_max));
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
  for (const int other : neighbors_) {
    total_sum_of_areas += cached_areas_[other];
  }

  const auto add_box_energy_in_rectangle_reason = [&](int b) {
    x_.AddStartMinReason(b, area_min_x);
    x_.AddDurationMinReason(b, x_.DurationMin(b));
    x_.AddEndMaxReason(b, area_max_x);
    y_.AddStartMinReason(b, area_min_y);
    y_.AddDurationMinReason(b, y_.DurationMin(b));
    y_.AddEndMaxReason(b, area_max_y);
  };

  // TODO(user): Is there a better order, maybe sort by distance
  // with the current box.
  for (int i = 0; i < neighbors_.size(); ++i) {
    const int other = neighbors_[i];
    CHECK_GT(cached_areas_[other], 0);

    // Update Bounding box.
    area_min_x = std::min(area_min_x, x_.StartMin(other));
    area_max_x = std::max(area_max_x, x_.EndMax(other));
    area_min_y = std::min(area_min_y, y_.StartMin(other));
    area_max_y = std::max(area_max_y, y_.EndMax(other));

    // Update sum of areas.
    sum_of_areas += cached_areas_[other];
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

// ----- disjunctive reasoning on mandatory boxes on one dimension -----

namespace {
bool CheckOverload(bool time_direction, IntegerValue other_time,
                   const absl::flat_hash_set<int>& boxes,
                   SchedulingConstraintHelper* helper,
                   SchedulingConstraintHelper* other) {
  ThetaLambdaTree<IntegerValue> theta_tree;
  std::vector<int> task_to_event;
  std::vector<int> event_to_task;
  std::vector<IntegerValue> event_time;
  task_to_event.resize(helper->NumTasks());

  helper->SetTimeDirection(time_direction);
  other->SetTimeDirection(true);

  // Set up theta tree.
  event_to_task.clear();
  event_time.clear();
  int num_events = 0;

  for (const auto task_time : helper->TaskByIncreasingShiftedStartMin()) {
    const int task = task_time.task_index;

    // TODO(user): We need to take into account task with zero duration because
    // in this constraint, such a task cannot be overlapped by other. However,
    // we currently use the fact that the energy min is zero to detect that a
    // task is present and non-optional in the theta_tree. Fix.
    if (helper->IsAbsent(task) || helper->DurationMin(task) == 0 ||
        !gtl::ContainsKey(boxes, task)) {
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
       ::gtl::reversed_view(helper->TaskByDecreasingEndMax())) {
    const int current_task = task_time.task_index;
    if (task_to_event[current_task] == -1) continue;

    {
      const int current_event = task_to_event[current_task];
      const IntegerValue energy_min = helper->DurationMin(current_task);
      theta_tree.AddOrUpdateEvent(current_event, event_time[current_event],
                                  energy_min, energy_min);
    }

    const IntegerValue current_end = task_time.time;
    if (theta_tree.GetEnvelope() > current_end) {
      // Explain failure with tasks in critical interval.
      helper->ClearReason();
      const int critical_event =
          theta_tree.GetMaxEventWithEnvelopeGreaterThan(current_end);
      const IntegerValue window_start = event_time[critical_event];
      const IntegerValue window_end =
          theta_tree.GetEnvelopeOf(critical_event) - 1;
      for (int event = critical_event; event < num_events; event++) {
        const IntegerValue energy_min = theta_tree.EnergyMin(event);
        if (energy_min > 0) {
          const int task = event_to_task[event];
          helper->AddEnergyAfterReason(task, energy_min, window_start);
          helper->AddEndMaxReason(task, window_end);
          other->AddStartMaxReason(task, other_time);
          other->AddEndMinReason(task, other_time + 1);
        }
      }
      helper->ImportOtherReasons(*other);
      return helper->ReportConflict();
    }
  }
  return true;
}

void AddOtherReasons(const std::vector<int>& tasks, IntegerValue other_time,
                     int main_task, SchedulingConstraintHelper* other) {
  other->ClearReason();
  bool main_task_seen = false;
  for (const int task : tasks) {
    other->AddStartMaxReason(task, other_time);
    other->AddEndMinReason(task, other_time + 1);
    if (task == main_task) {
      main_task_seen = true;
    }
  }
  if (!main_task_seen) {
    other->AddStartMaxReason(main_task, other_time);
    other->AddEndMinReason(main_task, other_time + 1);
  }
}

bool DetectPrecedences(bool time_direction, IntegerValue other_time,
                       const absl::flat_hash_set<int>& active_boxes,
                       SchedulingConstraintHelper* helper,
                       SchedulingConstraintHelper* other) {
  helper->SetTimeDirection(time_direction);
  other->SetTimeDirection(true);

  const auto& task_by_increasing_end_min = helper->TaskByIncreasingEndMin();
  const auto& task_by_decreasing_start_max = helper->TaskByDecreasingStartMax();

  const int num_tasks = helper->NumTasks();
  int queue_index = num_tasks - 1;
  TaskSet task_set(num_tasks);
  for (const auto task_time : task_by_increasing_end_min) {
    const int t = task_time.task_index;
    const IntegerValue end_min = task_time.time;

    if (helper->IsAbsent(t) || !gtl::ContainsKey(active_boxes, t)) continue;

    while (queue_index >= 0) {
      const auto to_insert = task_by_decreasing_start_max[queue_index];
      const int task_index = to_insert.task_index;
      const IntegerValue start_max = to_insert.time;
      if (end_min <= start_max) break;

      if (gtl::ContainsKey(active_boxes, task_index)) {
        CHECK(helper->IsPresent(task_index));
        task_set.AddEntry({task_index, helper->ShiftedStartMin(task_index),
                           helper->DurationMin(task_index)});
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
    if (end_min_of_critical_tasks > helper->StartMin(t)) {
      const std::vector<TaskSet::Entry>& sorted_tasks = task_set.SortedTasks();
      helper->ClearReason();

      // We need:
      // - StartMax(ct) < EndMin(t) for the detectable precedence.
      // - StartMin(ct) > window_start for the end_min_of_critical_tasks reason.
      const IntegerValue window_start = sorted_tasks[critical_index].start_min;
      std::vector<int> tasks(sorted_tasks.size() - critical_index);
      for (int i = critical_index; i < sorted_tasks.size(); ++i) {
        tasks[i - critical_index] = sorted_tasks[i].task;
      }

      AddOtherReasons(tasks, other_time, t, other);

      for (int i = critical_index; i < sorted_tasks.size(); ++i) {
        const int ct = sorted_tasks[i].task;
        if (ct == t) continue;
        CHECK(gtl::ContainsKey(active_boxes, ct));
        CHECK(helper->IsPresent(ct));
        helper->AddEnergyAfterReason(ct, sorted_tasks[i].duration_min,
                                     window_start);
        helper->AddStartMaxReason(ct, end_min - 1);
      }

      // Add the reason for t (we only need the end-min).
      helper->AddEndMinReason(t, end_min);

      // Import reasons from the 'other' dimension.
      helper->ImportOtherReasons(*other);

      // This augment the start-min of t and subsequently it can augment the
      // next end_min_of_critical_tasks, but our deduction is still valid.
      if (!helper->IncreaseStartMin(t, end_min_of_critical_tasks)) {
        return false;
      }

      // We need to reorder t inside task_set. Note that if t is in the set,
      // it means that the task is present and that IncreaseStartMin() did push
      // its start (by opposition to an optional interval where the push might
      // not happen if its start is not optional).
      task_set.NotifyEntryIsNowLastIfPresent(
          {t, helper->ShiftedStartMin(t), helper->DurationMin(t)});
    }
  }
  return true;
}
}  // namespace

bool NonOverlappingRectanglesPropagator::PropagateOnProjections() {
  RETURN_IF_FALSE(FindMandatoryBoxesOnOneDimension(&x_, &y_));
  RETURN_IF_FALSE(FindMandatoryBoxesOnOneDimension(&y_, &x_));
  return true;
}

bool NonOverlappingRectanglesPropagator::FindMandatoryBoxesOnOneDimension(
    SchedulingConstraintHelper* helper, SchedulingConstraintHelper* other) {
  helper->SetTimeDirection(true);
  other->SetTimeDirection(true);
  std::map<IntegerValue, std::vector<int>> mandatory_boxes;
  std::set<IntegerValue> events;

  for (int box = 0; box < num_boxes_; ++box) {
    if (cached_areas_[box] == 0) continue;
    const IntegerValue start_max = other->StartMax(box);
    const IntegerValue end_min = other->EndMin(box);
    if (start_max < end_min) {
      events.insert(start_max);
    }
  }

  for (int box = 0; box < num_boxes_; ++box) {
    if (cached_areas_[box] == 0) continue;
    const IntegerValue start_max = other->StartMax(box);
    const IntegerValue end_min = other->EndMin(box);

    if (start_max < end_min) {
      for (const IntegerValue t : events) {
        if (t < start_max) continue;
        if (t >= end_min) break;
        mandatory_boxes[t].push_back(box);
      }
    }
  }

  std::set<IntegerValue> to_remove;
  std::vector<int> previous;
  IntegerValue previous_event(-1);
  for (const auto& it : mandatory_boxes) {
    if (it.second.size() < 2) to_remove.insert(it.first);
    if (!previous.empty()) {
      if (std::includes(previous.begin(), previous.end(), it.second.begin(),
                        it.second.end())) {
        to_remove.insert(it.first);
      }
      if (std::includes(it.second.begin(), it.second.end(), previous.begin(),
                        previous.end())) {
        to_remove.insert(previous_event);
      }
      previous_event = it.first;
      previous = it.second;
    }
  }

  for (IntegerValue event : to_remove) {
    mandatory_boxes.erase(event);
  }

  for (const auto& it : mandatory_boxes) {
    // Compute the 'canonical' line to use when explaining that boxes overlap
    // on the 'other' dimension.
    IntegerValue lb(kint64min);
    IntegerValue ub(kint64max);
    for (const int task : it.second) {
      lb = std::max(lb, other->StartMax(task));
      ub = std::min(ub, other->EndMin(task));
    }

    const IntegerValue span = ub - lb + 1;
    IntegerValue selected = lb;
    for (int shift = 30; shift >= 0; --shift) {
      const IntegerValue mask(static_cast<int64>(1) >> shift);
      if (mask <= span) {
        selected = (ub / mask) * mask;
        break;
      }
    }
    // And propagate.
    RETURN_IF_FALSE(PropagateMandatoryBoxesOnOneDimension(selected, it.second,
                                                          helper, other));
  }
  return true;
}

bool NonOverlappingRectanglesPropagator::PropagateMandatoryBoxesOnOneDimension(
    IntegerValue event, const std::vector<int>& boxes,
    SchedulingConstraintHelper* helper, SchedulingConstraintHelper* other) {
  const absl::flat_hash_set<int> active_boxes(boxes.begin(), boxes.end());
  RETURN_IF_FALSE(CheckOverload(true, event, active_boxes, helper, other));
  RETURN_IF_FALSE(DetectPrecedences(true, event, active_boxes, helper, other));
  RETURN_IF_FALSE(DetectPrecedences(false, event, active_boxes, helper, other));
  return true;
}

#undef RETURN_IF_FALSE

}  // namespace sat
}  // namespace operations_research
