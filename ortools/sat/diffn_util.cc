// Copyright 2010-2022 Google LLC
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

#include "ortools/sat/diffn_util.h"

#include <stddef.h>

#include <algorithm>
#include <cmath>
#include <ostream>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/container/inlined_vector.h"
#include "absl/log/check.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/random/discrete_distribution.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/base/stl_util.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/intervals.h"
#include "ortools/util/integer_pq.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {

bool Rectangle::IsDisjoint(const Rectangle& other) const {
  return x_min >= other.x_max || other.x_min >= x_max || y_min >= other.y_max ||
         other.y_min >= y_max;
}

std::vector<absl::Span<int>> GetOverlappingRectangleComponents(
    const std::vector<Rectangle>& rectangles,
    absl::Span<int> active_rectangles) {
  if (active_rectangles.empty()) return {};

  std::vector<absl::Span<int>> result;
  const int size = active_rectangles.size();
  for (int start = 0; start < size;) {
    // Find the component of active_rectangles[start].
    int end = start + 1;
    for (int i = start; i < end; i++) {
      for (int j = end; j < size; ++j) {
        if (!rectangles[active_rectangles[i]].IsDisjoint(
                rectangles[active_rectangles[j]])) {
          std::swap(active_rectangles[end++], active_rectangles[j]);
        }
      }
    }
    if (end > start + 1) {
      result.push_back(active_rectangles.subspan(start, end - start));
    }
    start = end;
  }
  return result;
}

bool ReportEnergyConflict(Rectangle bounding_box, absl::Span<const int> boxes,
                          SchedulingConstraintHelper* x,
                          SchedulingConstraintHelper* y) {
  x->ClearReason();
  y->ClearReason();
  IntegerValue total_energy(0);
  for (const int b : boxes) {
    const IntegerValue x_min = x->ShiftedStartMin(b);
    const IntegerValue x_max = x->ShiftedEndMax(b);
    if (x_min < bounding_box.x_min || x_max > bounding_box.x_max) continue;
    const IntegerValue y_min = y->ShiftedStartMin(b);
    const IntegerValue y_max = y->ShiftedEndMax(b);
    if (y_min < bounding_box.y_min || y_max > bounding_box.y_max) continue;

    x->AddEnergyMinInIntervalReason(b, bounding_box.x_min, bounding_box.x_max);
    y->AddEnergyMinInIntervalReason(b, bounding_box.y_min, bounding_box.y_max);

    x->AddPresenceReason(b);
    y->AddPresenceReason(b);

    total_energy += x->SizeMin(b) * y->SizeMin(b);

    // We abort early if a subset of boxes is enough.
    // TODO(user): Also relax the box if possible.
    if (total_energy > bounding_box.Area()) break;
  }

  CHECK_GT(total_energy, bounding_box.Area());
  x->ImportOtherReasons(*y);
  return x->ReportConflict();
}

bool BoxesAreInEnergyConflict(const std::vector<Rectangle>& rectangles,
                              const std::vector<IntegerValue>& energies,
                              absl::Span<const int> boxes,
                              Rectangle* conflict) {
  // First consider all relevant intervals along the x axis.
  std::vector<IntegerValue> x_starts;
  std::vector<TaskTime> boxes_by_increasing_x_max;
  for (const int b : boxes) {
    x_starts.push_back(rectangles[b].x_min);
    boxes_by_increasing_x_max.push_back({b, rectangles[b].x_max});
  }
  gtl::STLSortAndRemoveDuplicates(&x_starts);
  std::sort(boxes_by_increasing_x_max.begin(), boxes_by_increasing_x_max.end());

  std::vector<IntegerValue> y_starts;
  std::vector<IntegerValue> energy_sum;
  std::vector<TaskTime> boxes_by_increasing_y_max;

  std::vector<std::vector<int>> stripes(x_starts.size());
  for (int i = 0; i < boxes_by_increasing_x_max.size(); ++i) {
    const int b = boxes_by_increasing_x_max[i].task_index;
    const IntegerValue x_min = rectangles[b].x_min;
    const IntegerValue x_max = rectangles[b].x_max;
    for (int j = 0; j < x_starts.size(); ++j) {
      if (x_starts[j] > x_min) break;
      stripes[j].push_back(b);

      // Redo the same on the y coordinate for the current x interval
      // which is [starts[j], x_max].
      y_starts.clear();
      boxes_by_increasing_y_max.clear();
      for (const int b : stripes[j]) {
        y_starts.push_back(rectangles[b].y_min);
        boxes_by_increasing_y_max.push_back({b, rectangles[b].y_max});
      }
      gtl::STLSortAndRemoveDuplicates(&y_starts);
      std::sort(boxes_by_increasing_y_max.begin(),
                boxes_by_increasing_y_max.end());

      const IntegerValue x_size = x_max - x_starts[j];
      energy_sum.assign(y_starts.size(), IntegerValue(0));
      for (int i = 0; i < boxes_by_increasing_y_max.size(); ++i) {
        const int b = boxes_by_increasing_y_max[i].task_index;
        const IntegerValue y_min = rectangles[b].y_min;
        const IntegerValue y_max = rectangles[b].y_max;
        for (int j = 0; j < y_starts.size(); ++j) {
          if (y_starts[j] > y_min) break;
          energy_sum[j] += energies[b];
          if (energy_sum[j] > x_size * (y_max - y_starts[j])) {
            if (conflict != nullptr) {
              *conflict = rectangles[b];
              for (int k = 0; k < i; ++k) {
                const int task_index = boxes_by_increasing_y_max[k].task_index;
                if (rectangles[task_index].y_min >= y_starts[j]) {
                  conflict->TakeUnionWith(rectangles[task_index]);
                }
              }
            }
            return true;
          }
        }
      }
    }
  }
  return false;
}

bool AnalyzeIntervals(bool transpose, absl::Span<const int> local_boxes,
                      const std::vector<Rectangle>& rectangles,
                      const std::vector<IntegerValue>& rectangle_energies,
                      IntegerValue* x_threshold, IntegerValue* y_threshold,
                      Rectangle* conflict) {
  // First, we compute the possible x_min values (removing duplicates).
  // We also sort the relevant tasks by their x_max.
  //
  // TODO(user): If the number of unique x_max is smaller than the number of
  // unique x_min, it is better to do it the other way around.
  std::vector<IntegerValue> starts;
  std::vector<TaskTime> task_by_increasing_x_max;
  for (const int t : local_boxes) {
    const IntegerValue x_min =
        transpose ? rectangles[t].y_min : rectangles[t].x_min;
    const IntegerValue x_max =
        transpose ? rectangles[t].y_max : rectangles[t].x_max;
    starts.push_back(x_min);
    task_by_increasing_x_max.push_back({t, x_max});
  }
  gtl::STLSortAndRemoveDuplicates(&starts);

  // Note that for the same end_max, the order change our heuristic to
  // evaluate the max_conflict_height.
  std::sort(task_by_increasing_x_max.begin(), task_by_increasing_x_max.end());

  // The maximum y dimension of a bounding area for which there is a potential
  // conflict.
  IntegerValue max_conflict_height(0);

  // This is currently only used for logging.
  absl::flat_hash_set<std::pair<IntegerValue, IntegerValue>> stripes;

  // All quantities at index j correspond to the interval [starts[j], x_max].
  std::vector<IntegerValue> energies(starts.size(), IntegerValue(0));
  std::vector<IntegerValue> y_mins(starts.size(), kMaxIntegerValue);
  std::vector<IntegerValue> y_maxs(starts.size(), -kMaxIntegerValue);
  std::vector<IntegerValue> energy_at_max_y(starts.size(), IntegerValue(0));
  std::vector<IntegerValue> energy_at_min_y(starts.size(), IntegerValue(0));

  // Sentinel.
  starts.push_back(kMaxIntegerValue);

  // Iterate over all boxes by increasing x_max values.
  int first_j = 0;
  const IntegerValue threshold = transpose ? *y_threshold : *x_threshold;
  for (int i = 0; i < task_by_increasing_x_max.size(); ++i) {
    const int t = task_by_increasing_x_max[i].task_index;

    const IntegerValue energy = rectangle_energies[t];
    IntegerValue x_min = rectangles[t].x_min;
    IntegerValue x_max = rectangles[t].x_max;
    IntegerValue y_min = rectangles[t].y_min;
    IntegerValue y_max = rectangles[t].y_max;
    if (transpose) {
      std::swap(x_min, y_min);
      std::swap(x_max, y_max);
    }

    // Add this box contribution to all the [starts[j], x_max] intervals.
    while (first_j + 1 < starts.size() && x_max - starts[first_j] > threshold) {
      ++first_j;
    }
    for (int j = first_j; starts[j] <= x_min; ++j) {
      const IntegerValue old_energy_at_max = energy_at_max_y[j];
      const IntegerValue old_energy_at_min = energy_at_min_y[j];

      energies[j] += energy;

      const bool is_disjoint = y_min >= y_maxs[j] || y_max <= y_mins[j];

      if (y_min <= y_mins[j]) {
        if (y_min < y_mins[j]) {
          y_mins[j] = y_min;
          energy_at_min_y[j] = energy;
        } else {
          energy_at_min_y[j] += energy;
        }
      }

      if (y_max >= y_maxs[j]) {
        if (y_max > y_maxs[j]) {
          y_maxs[j] = y_max;
          energy_at_max_y[j] = energy;
        } else {
          energy_at_max_y[j] += energy;
        }
      }

      // If the new box is disjoint in y from the ones added so far, there
      // cannot be a new conflict involving this box, so we skip until we add
      // new boxes.
      if (is_disjoint) continue;

      const IntegerValue width = x_max - starts[j];
      IntegerValue conflict_height = CeilRatio(energies[j], width) - 1;
      if (y_max - y_min > conflict_height) continue;
      if (conflict_height >= y_maxs[j] - y_mins[j]) {
        // We have a conflict.
        if (conflict != nullptr) {
          *conflict = rectangles[t];
          for (int k = 0; k < i; ++k) {
            const int task_index = task_by_increasing_x_max[k].task_index;
            const IntegerValue task_x_min = transpose
                                                ? rectangles[task_index].y_min
                                                : rectangles[task_index].x_min;
            if (task_x_min < starts[j]) continue;
            conflict->TakeUnionWith(rectangles[task_index]);
          }
        }
        return false;
      }

      // Because we currently do not have a conflict involving the new box, the
      // only way to have one is to remove enough energy to reduce the y domain.
      IntegerValue can_remove = std::min(old_energy_at_min, old_energy_at_max);
      if (old_energy_at_min < old_energy_at_max) {
        if (y_maxs[j] - y_min >=
            CeilRatio(energies[j] - old_energy_at_min, width)) {
          // In this case, we need to remove at least old_energy_at_max to have
          // a conflict.
          can_remove = old_energy_at_max;
        }
      } else if (old_energy_at_max < old_energy_at_min) {
        if (y_max - y_mins[j] >=
            CeilRatio(energies[j] - old_energy_at_max, width)) {
          can_remove = old_energy_at_min;
        }
      }
      conflict_height = CeilRatio(energies[j] - can_remove, width) - 1;

      // If the new box height is above the conflict_height, do not count
      // it now. We only need to consider conflict involving the new box.
      if (y_max - y_min > conflict_height) continue;

      if (VLOG_IS_ON(2)) stripes.insert({starts[j], x_max});
      max_conflict_height = std::max(max_conflict_height, conflict_height);
    }
  }

  VLOG(2) << " num_starts: " << starts.size() - 1 << "/" << local_boxes.size()
          << " conflict_height: " << max_conflict_height
          << " num_stripes:" << stripes.size() << " (<= " << threshold << ")";

  if (transpose) {
    *x_threshold = std::min(*x_threshold, max_conflict_height);
  } else {
    *y_threshold = std::min(*y_threshold, max_conflict_height);
  }
  return true;
}

absl::Span<int> FilterBoxesAndRandomize(
    const std::vector<Rectangle>& cached_rectangles, absl::Span<int> boxes,
    IntegerValue threshold_x, IntegerValue threshold_y,
    absl::BitGenRef random) {
  size_t new_size = 0;
  for (const int b : boxes) {
    const Rectangle& dim = cached_rectangles[b];
    if (dim.x_max - dim.x_min > threshold_x) continue;
    if (dim.y_max - dim.y_min > threshold_y) continue;
    boxes[new_size++] = b;
  }
  if (new_size == 0) return {};
  std::shuffle(&boxes[0], &boxes[0] + new_size, random);
  return {&boxes[0], new_size};
}

absl::Span<int> FilterBoxesThatAreTooLarge(
    const std::vector<Rectangle>& cached_rectangles,
    const std::vector<IntegerValue>& energies, absl::Span<int> boxes) {
  // Sort the boxes by increasing area.
  std::sort(boxes.begin(), boxes.end(), [&cached_rectangles](int a, int b) {
    return cached_rectangles[a].Area() < cached_rectangles[b].Area();
  });

  IntegerValue total_energy(0);
  for (const int box : boxes) total_energy += energies[box];

  // Remove all the large boxes until we have one with area smaller than the
  // energy of the boxes below.
  int new_size = boxes.size();
  while (new_size > 0 &&
         cached_rectangles[boxes[new_size - 1]].Area() >= total_energy) {
    --new_size;
    total_energy -= energies[boxes[new_size]];
  }
  return boxes.subspan(0, new_size);
}

std::ostream& operator<<(std::ostream& out, const IndexedInterval& interval) {
  return out << "[" << interval.start << ".." << interval.end << " (#"
             << interval.index << ")]";
}

void ConstructOverlappingSets(bool already_sorted,
                              std::vector<IndexedInterval>* intervals,
                              std::vector<std::vector<int>>* result) {
  result->clear();
  if (already_sorted) {
    DCHECK(std::is_sorted(intervals->begin(), intervals->end(),
                          IndexedInterval::ComparatorByStart()));
  } else {
    std::sort(intervals->begin(), intervals->end(),
              IndexedInterval::ComparatorByStart());
  }
  IntegerValue min_end_in_set = kMaxIntegerValue;
  intervals->push_back({-1, kMaxIntegerValue, kMaxIntegerValue});  // Sentinel.
  const int size = intervals->size();

  // We do a line sweep. The "current" subset crossing the "line" at
  // (time, time + 1) will be in (*intervals)[start_index, end_index) at the end
  // of the loop block.
  int start_index = 0;
  for (int end_index = 0; end_index < size;) {
    const IntegerValue time = (*intervals)[end_index].start;

    // First, if there is some deletion, we will push the "old" set to the
    // result before updating it. Otherwise, we will have a superset later, so
    // we just continue for now.
    if (min_end_in_set <= time) {
      result->push_back({});
      min_end_in_set = kMaxIntegerValue;
      for (int i = start_index; i < end_index; ++i) {
        result->back().push_back((*intervals)[i].index);
        if ((*intervals)[i].end <= time) {
          std::swap((*intervals)[start_index++], (*intervals)[i]);
        } else {
          min_end_in_set = std::min(min_end_in_set, (*intervals)[i].end);
        }
      }

      // Do not output subset of size one.
      if (result->back().size() == 1) result->pop_back();
    }

    // Add all the new intervals starting exactly at "time".
    do {
      min_end_in_set = std::min(min_end_in_set, (*intervals)[end_index].end);
      ++end_index;
    } while (end_index < size && (*intervals)[end_index].start == time);
  }
}

void GetOverlappingIntervalComponents(
    std::vector<IndexedInterval>* intervals,
    std::vector<std::vector<int>>* components) {
  components->clear();
  if (intervals->empty()) return;
  if (intervals->size() == 1) {
    components->push_back({intervals->front().index});
    return;
  }

  // For correctness, ComparatorByStart is enough, but in unit tests we want to
  // verify this function against another implementation, and fully defined
  // sorting with tie-breaking makes that much easier.
  // If that becomes a performance bottleneck:
  // - One may want to sort the list outside of this function, and simply
  //   have this function DCHECK that it's sorted by start.
  // - One may use stable_sort() with ComparatorByStart().
  std::sort(intervals->begin(), intervals->end(),
            IndexedInterval::ComparatorByStartThenEndThenIndex());

  IntegerValue end_max_so_far = (*intervals)[0].end;
  components->push_back({(*intervals)[0].index});
  for (int i = 1; i < intervals->size(); ++i) {
    const IndexedInterval& interval = (*intervals)[i];
    if (interval.start >= end_max_so_far) {
      components->push_back({interval.index});
    } else {
      components->back().push_back(interval.index);
    }
    end_max_so_far = std::max(end_max_so_far, interval.end);
  }
}

std::vector<int> GetIntervalArticulationPoints(
    std::vector<IndexedInterval>* intervals) {
  std::vector<int> articulation_points;
  if (intervals->size() < 3) return articulation_points;  // Empty.
  if (DEBUG_MODE) {
    for (const IndexedInterval& interval : *intervals) {
      DCHECK_LT(interval.start, interval.end);
    }
  }

  std::sort(intervals->begin(), intervals->end(),
            IndexedInterval::ComparatorByStart());

  IntegerValue end_max_so_far = (*intervals)[0].end;
  int index_of_max = 0;
  IntegerValue prev_end_max = kMinIntegerValue;  // Initialized as a sentinel.
  for (int i = 1; i < intervals->size(); ++i) {
    const IndexedInterval& interval = (*intervals)[i];
    if (interval.start >= end_max_so_far) {
      // New connected component.
      end_max_so_far = interval.end;
      index_of_max = i;
      prev_end_max = kMinIntegerValue;
      continue;
    }
    // Still the same connected component. Was the previous "max" an
    // articulation point ?
    if (prev_end_max != kMinIntegerValue && interval.start >= prev_end_max) {
      // We might be re-inserting the same articulation point: guard against it.
      if (articulation_points.empty() ||
          articulation_points.back() != index_of_max) {
        articulation_points.push_back(index_of_max);
      }
    }
    // Update the max end.
    if (interval.end > end_max_so_far) {
      prev_end_max = end_max_so_far;
      end_max_so_far = interval.end;
      index_of_max = i;
    } else if (interval.end > prev_end_max) {
      prev_end_max = interval.end;
    }
  }
  // Convert articulation point indices to IndexedInterval.index.
  for (int& index : articulation_points) index = (*intervals)[index].index;
  return articulation_points;
}

void CapacityProfile::Clear() {
  events_.clear();
  num_rectangles_added_ = 0;
}

void CapacityProfile::AddRectangle(IntegerValue x_min, IntegerValue x_max,
                                   IntegerValue y_min, IntegerValue y_max) {
  DCHECK_LE(x_min, x_max);
  if (x_min == x_max) return;

  events_.push_back(
      StartRectangleEvent(num_rectangles_added_, x_min, y_min, y_max));
  events_.push_back(EndRectangleEvent(num_rectangles_added_, x_max));
  ++num_rectangles_added_;
}

void CapacityProfile::AddMandatoryConsumption(IntegerValue x_min,
                                              IntegerValue x_max,
                                              IntegerValue y_height) {
  DCHECK_LE(x_min, x_max);
  if (x_min == x_max) return;

  events_.push_back(ChangeMandatoryProfileEvent(x_min, y_height));
  events_.push_back(ChangeMandatoryProfileEvent(x_max, -y_height));
}

void CapacityProfile::BuildResidualCapacityProfile(
    std::vector<CapacityProfile::Rectangle>* result) {
  std::sort(events_.begin(), events_.end());
  IntegerPriorityQueue<QueueElement> min_pq(num_rectangles_added_);
  IntegerPriorityQueue<QueueElement> max_pq(num_rectangles_added_);
  IntegerValue mandatory_capacity(0);

  result->clear();

  result->push_back({kMinIntegerValue, IntegerValue(0)});

  for (int i = 0; i < events_.size();) {
    const IntegerValue current_time = events_[i].time;
    for (; i < events_.size(); ++i) {
      const Event& event = events_[i];
      if (event.time != current_time) break;

      switch (events_[i].type) {
        case START_RECTANGLE: {
          min_pq.Add({event.index, -event.y_min});
          max_pq.Add({event.index, event.y_max});
          break;
        }
        case END_RECTANGLE: {
          min_pq.Remove(event.index);
          max_pq.Remove(event.index);
          break;
        }
        case CHANGE_MANDATORY_PROFILE: {
          mandatory_capacity += event.y_min;
          break;
        }
      }
    }

    DCHECK(!max_pq.IsEmpty() || mandatory_capacity == 0);
    const IntegerValue new_height =
        max_pq.IsEmpty()
            ? IntegerValue(0)
            : max_pq.Top().value + min_pq.Top().value - mandatory_capacity;
    if (new_height != result->back().height) {
      result->push_back({current_time, new_height});
    }
  }
}

IntegerValue CapacityProfile::GetBoundingArea() {
  std::sort(events_.begin(), events_.end());
  IntegerPriorityQueue<QueueElement> min_pq(num_rectangles_added_);
  IntegerPriorityQueue<QueueElement> max_pq(num_rectangles_added_);

  IntegerValue area(0);
  IntegerValue previous_time = kMinIntegerValue;
  IntegerValue previous_height(0);

  for (int i = 0; i < events_.size();) {
    const IntegerValue current_time = events_[i].time;
    for (; i < events_.size(); ++i) {
      const Event& event = events_[i];
      if (event.time != current_time) break;

      switch (event.type) {
        case START_RECTANGLE: {
          min_pq.Add({event.index, -event.y_min});
          max_pq.Add({event.index, event.y_max});
          break;
        }
        case END_RECTANGLE: {
          min_pq.Remove(event.index);
          max_pq.Remove(event.index);
          break;
        }
        case CHANGE_MANDATORY_PROFILE: {
          break;
        }
      }
    }
    const IntegerValue new_height =
        max_pq.IsEmpty() ? IntegerValue(0)
                         : max_pq.Top().value + min_pq.Top().value;
    if (previous_height != 0) {
      area += previous_height * (current_time - previous_time);
    }
    previous_time = current_time;
    previous_height = new_height;
  }
  return area;
}

IntegerValue Smallest1DIntersection(IntegerValue range_min,
                                    IntegerValue range_max, IntegerValue size,
                                    IntegerValue interval_min,
                                    IntegerValue interval_max) {
  // If the item is on the left of the range, we get the intersection between
  // [range_min, range_min + size] and [interval_min, interval_max].
  const IntegerValue overlap_on_left =
      std::min(range_min + size, interval_max) -
      std::max(range_min, interval_min);

  // If the item is on the right of the range, we get the intersection between
  // [range_max - size, range_max] and [interval_min, interval_max].
  const IntegerValue overlap_on_right =
      std::min(range_max, interval_max) -
      std::max(range_max - size, interval_min);

  return std::max(IntegerValue(0), std::min(overlap_on_left, overlap_on_right));
}

ProbingRectangle::ProbingRectangle(
    const std::vector<RectangleInRange>& intervals)
    : intervals_(intervals) {
  minimum_energy_ = 0;
  if (intervals_.empty()) {
    return;
  }
  interval_points_sorted_by_x_.reserve(intervals_.size() * 4);
  interval_points_sorted_by_y_.reserve(intervals_.size() * 4);
  for (int i = 0; i < intervals_.size(); ++i) {
    const RectangleInRange& interval = intervals_[i];
    minimum_energy_ += interval.x_size * interval.y_size;

    interval_points_sorted_by_x_.push_back(
        {interval.bounding_area.x_min, i,
         IntervalPoint::IntervalPointType::START_MIN});
    interval_points_sorted_by_x_.push_back(
        {interval.bounding_area.x_min + interval.x_size, i,
         IntervalPoint::IntervalPointType::END_MIN});
    interval_points_sorted_by_x_.push_back(
        {interval.bounding_area.x_max - interval.x_size, i,
         IntervalPoint::IntervalPointType::START_MAX});
    interval_points_sorted_by_x_.push_back(
        {interval.bounding_area.x_max, i,
         IntervalPoint::IntervalPointType::END_MAX});

    interval_points_sorted_by_y_.push_back(
        {interval.bounding_area.y_min, i,
         IntervalPoint::IntervalPointType::START_MIN});
    interval_points_sorted_by_y_.push_back(
        {interval.bounding_area.y_min + interval.y_size, i,
         IntervalPoint::IntervalPointType::END_MIN});
    interval_points_sorted_by_y_.push_back(
        {interval.bounding_area.y_max - interval.y_size, i,
         IntervalPoint::IntervalPointType::START_MAX});
    interval_points_sorted_by_y_.push_back(
        {interval.bounding_area.y_max, i,
         IntervalPoint::IntervalPointType::END_MAX});
  }

  std::sort(interval_points_sorted_by_x_.begin(),
            interval_points_sorted_by_x_.end(),
            [](const IntervalPoint& a, const IntervalPoint& b) {
              return a.value < b.value;
            });
  std::sort(interval_points_sorted_by_y_.begin(),
            interval_points_sorted_by_y_.end(),
            [](const IntervalPoint& a, const IntervalPoint& b) {
              return a.value < b.value;
            });

  grouped_intervals_sorted_by_x_.reserve(interval_points_sorted_by_x_.size());
  grouped_intervals_sorted_by_y_.reserve(interval_points_sorted_by_y_.size());
  int i = 0;
  while (i < interval_points_sorted_by_x_.size()) {
    int idx_begin = i;
    while (i < interval_points_sorted_by_x_.size() &&
           interval_points_sorted_by_x_[i].value ==
               interval_points_sorted_by_x_[idx_begin].value) {
      i++;
    }
    grouped_intervals_sorted_by_x_.push_back(
        {interval_points_sorted_by_x_[idx_begin].value,
         absl::Span<IntervalPoint>(interval_points_sorted_by_x_)
             .subspan(idx_begin, i - idx_begin)});
  }

  i = 0;
  while (i < interval_points_sorted_by_y_.size()) {
    int idx_begin = i;
    while (i < interval_points_sorted_by_y_.size() &&
           interval_points_sorted_by_y_[i].value ==
               interval_points_sorted_by_y_[idx_begin].value) {
      i++;
    }
    grouped_intervals_sorted_by_y_.push_back(
        {interval_points_sorted_by_y_[idx_begin].value,
         absl::Span<IntervalPoint>(interval_points_sorted_by_y_)
             .subspan(idx_begin, i - idx_begin)});
  }

  left_index_ = 0;
  right_index_ = grouped_intervals_sorted_by_x_.size() - 1;
  bottom_index_ = 0;
  top_index_ = grouped_intervals_sorted_by_y_.size() - 1;

  for (const auto& point : grouped_intervals_sorted_by_x_[left_index_].points) {
    ranges_touching_boundary_[Edge::LEFT].insert(point.index);
  }
  for (const auto& point :
       grouped_intervals_sorted_by_x_[right_index_].points) {
    ranges_touching_boundary_[Edge::RIGHT].insert(point.index);
  }
  for (const auto& point :
       grouped_intervals_sorted_by_y_[bottom_index_].points) {
    ranges_touching_boundary_[Edge::BOTTOM].insert(point.index);
  }
  for (const auto& point : grouped_intervals_sorted_by_y_[top_index_].points) {
    ranges_touching_boundary_[Edge::TOP].insert(point.index);
  }
  probe_area_ = GetCurrentRectangle().Area();
}

Rectangle ProbingRectangle::GetCurrentRectangle() const {
  return {.x_min = grouped_intervals_sorted_by_x_[left_index_].coordinate,
          .x_max = grouped_intervals_sorted_by_x_[right_index_].coordinate,
          .y_min = grouped_intervals_sorted_by_y_[bottom_index_].coordinate,
          .y_max = grouped_intervals_sorted_by_y_[top_index_].coordinate};
}

void ProbingRectangle::Shrink(Edge edge) {
  absl::Span<ProbingRectangle::IntervalPoint> points;

  minimum_energy_ -= GetShrinkDeltaEnergy(edge);
  switch (edge) {
    case Edge::LEFT:
      left_index_++;
      points = grouped_intervals_sorted_by_x_[left_index_].points;
      break;
    case Edge::BOTTOM:
      bottom_index_++;
      points = grouped_intervals_sorted_by_y_[bottom_index_].points;
      break;
    case Edge::RIGHT:
      right_index_--;
      points = grouped_intervals_sorted_by_x_[right_index_].points;
      break;
    case Edge::TOP:
      top_index_--;
      points = grouped_intervals_sorted_by_y_[top_index_].points;
      break;
  }

  for (const auto& point : points) {
    const bool became_outside_probe =
        (point.type == IntervalPoint::IntervalPointType::END_MIN &&
         (edge == Edge::LEFT || edge == Edge::BOTTOM)) ||
        (point.type == IntervalPoint::IntervalPointType::START_MAX &&
         (edge == Edge::RIGHT || edge == Edge::TOP));
    if (became_outside_probe) {
      ranges_touching_boundary_[Edge::LEFT].erase(point.index);
      ranges_touching_boundary_[Edge::BOTTOM].erase(point.index);
      ranges_touching_boundary_[Edge::RIGHT].erase(point.index);
      ranges_touching_boundary_[Edge::TOP].erase(point.index);
    }
  }

  const Rectangle current_rectangle = GetCurrentRectangle();
  auto can_consume_energy = [&current_rectangle](
                                const RectangleInRange& range) {
    // This intersects the current rectangle with the largest rectangle
    // that must intersect with the range in some way. To visualize this
    // largest rectangle, imagine the four possible extreme positions for
    // the item in range (the four corners). This rectangle is the one
    // defined by the interior points of each position.
    // This don't use IsDisjoint() because it also works when the rectangle
    // would be malformed (it's bounding box less than twice the size).
    return !(
        range.bounding_area.x_max - range.x_size >= current_rectangle.x_max ||
        range.bounding_area.y_max - range.y_size >= current_rectangle.y_max ||
        current_rectangle.x_min >= range.bounding_area.x_min + range.x_size ||
        current_rectangle.y_min >= range.bounding_area.y_min + range.y_size);
  };

  switch (edge) {
    case Edge::LEFT:
    case Edge::BOTTOM:
      for (const auto& point : points) {
        if (point.type == IntervalPoint::IntervalPointType::START_MIN) {
          if (can_consume_energy(intervals_[point.index])) {
            ranges_touching_boundary_[edge].insert(point.index);
          }
        }
      }
      break;
    case Edge::RIGHT:
    case Edge::TOP:
      for (const auto& point : points) {
        if (point.type == IntervalPoint::IntervalPointType::END_MAX) {
          if (can_consume_energy(intervals_[point.index])) {
            ranges_touching_boundary_[edge].insert(point.index);
          }
        }
      }
      break;
  }
  probe_area_ = GetCurrentRectangle().Area();
}

IntegerValue ProbingRectangle::GetShrinkDeltaArea(Edge edge) const {
  const Rectangle current_rectangle = GetCurrentRectangle();
  switch (edge) {
    case Edge::LEFT:
      return (grouped_intervals_sorted_by_x_[left_index_ + 1].coordinate -
              current_rectangle.x_min) *
             current_rectangle.SizeY();
    case Edge::BOTTOM:
      return (grouped_intervals_sorted_by_y_[bottom_index_ + 1].coordinate -
              current_rectangle.y_min) *
             current_rectangle.SizeX();
    case Edge::RIGHT:
      return (current_rectangle.x_max -
              grouped_intervals_sorted_by_x_[right_index_ - 1].coordinate) *
             current_rectangle.SizeY();
    case Edge::TOP:
      return (current_rectangle.y_max -
              grouped_intervals_sorted_by_y_[top_index_ - 1].coordinate) *
             current_rectangle.SizeX();
  }
}

IntegerValue ProbingRectangle::GetShrinkDeltaEnergy(Edge edge) const {
  const Rectangle current_rectangle = GetCurrentRectangle();
  Rectangle next_rectangle = current_rectangle;
  IntegerValue step_1d_size;

  switch (edge) {
    case Edge::LEFT:
      next_rectangle.x_min =
          grouped_intervals_sorted_by_x_[left_index_ + 1].coordinate;
      step_1d_size = next_rectangle.x_min - current_rectangle.x_min;
      break;
    case Edge::BOTTOM:
      next_rectangle.y_min =
          grouped_intervals_sorted_by_y_[bottom_index_ + 1].coordinate;
      step_1d_size = next_rectangle.y_min - current_rectangle.y_min;
      break;
    case Edge::RIGHT:
      next_rectangle.x_max =
          grouped_intervals_sorted_by_x_[right_index_ - 1].coordinate;
      step_1d_size = current_rectangle.x_max - next_rectangle.x_max;
      break;
    case Edge::TOP:
      next_rectangle.y_max =
          grouped_intervals_sorted_by_y_[top_index_ - 1].coordinate;
      step_1d_size = current_rectangle.y_max - next_rectangle.y_max;
      break;
  }

  IntegerValue delta_energy = 0;
  IntegerValue units_crossed = 0;
  // Note that the non-deterministic iteration order is fine here.
  for (const int idx : ranges_touching_boundary_[edge]) {
    const RectangleInRange& range = intervals_[idx];
    bool problematic_case_in_two_sides = false;
    IntegerValue opposite_slack;
    switch (edge) {
      case Edge::LEFT:
        opposite_slack = range.bounding_area.x_max - current_rectangle.x_max;
        // First check if we touch the opposite edge to the one we are
        // shrinking.
        if (opposite_slack >= 0) {
          // If it do, it's problematic if it has more slack on the opposite
          // side so it will "jump" to the other side.
          problematic_case_in_two_sides =
              opposite_slack >=
              current_rectangle.x_min - range.bounding_area.x_min;
        }
        break;
      case Edge::BOTTOM:
        opposite_slack = range.bounding_area.y_max - current_rectangle.y_max;
        if (opposite_slack >= 0) {
          problematic_case_in_two_sides =
              opposite_slack >=
              current_rectangle.y_min - range.bounding_area.y_min;
        }
        break;
      case Edge::RIGHT:
        opposite_slack = current_rectangle.x_min - range.bounding_area.x_min;
        if (opposite_slack >= 0) {
          problematic_case_in_two_sides =
              opposite_slack >=
              range.bounding_area.x_max - current_rectangle.x_max;
        }
        break;
      case Edge::TOP:
        opposite_slack = current_rectangle.y_min - range.bounding_area.y_min;
        if (opposite_slack >= 0) {
          problematic_case_in_two_sides =
              opposite_slack >=
              range.bounding_area.y_max - current_rectangle.y_max;
        }
        break;
    }
    if (problematic_case_in_two_sides) {
      // When it touches both sides, reducing the probe on the bottom might
      // make the place with the minimum overlap become the top. It's too
      // complicated to manage, so we fall back on actually computing it from
      // scratch.
      delta_energy += range.GetMinimumIntersectionArea(current_rectangle);
      delta_energy -= range.GetMinimumIntersectionArea(next_rectangle);
    } else {
      IntegerValue intersect_length;
      if (edge == Edge::LEFT || edge == Edge::RIGHT) {
        intersect_length = Smallest1DIntersection(
            range.bounding_area.y_min, range.bounding_area.y_max, range.y_size,
            current_rectangle.y_min, current_rectangle.y_max);
      } else {
        intersect_length = Smallest1DIntersection(
            range.bounding_area.x_min, range.bounding_area.x_max, range.x_size,
            current_rectangle.x_min, current_rectangle.x_max);
      }
      units_crossed += intersect_length;
    }
  }
  delta_energy += units_crossed * step_1d_size;
  return delta_energy;
}

bool ProbingRectangle::CanShrink(Edge edge) const {
  switch (edge) {
    case Edge::LEFT:
    case Edge::RIGHT:
      return (right_index_ - left_index_ > 1);
    case Edge::BOTTOM:
    case Edge::TOP:
      return (top_index_ - bottom_index_ > 1);
  }
}

namespace {
std::vector<double> GetExpTable() {
  std::vector<double> table(101);
  for (int i = 0; i <= 100; ++i) {
    table[i] = std::exp(-(i - 50) / 5.0);
  }
  return table;
}
}  // namespace

std::vector<Rectangle> FindRectanglesWithEnergyConflictMC(
    const std::vector<RectangleInRange>& intervals, absl::BitGenRef random,
    double temperature) {
  std::vector<Rectangle> result;
  ProbingRectangle ranges(intervals);

  static const std::vector<double>* cached_probabilities =
      new std::vector<double>(GetExpTable());

  const double inv_temp = 1.0 / temperature;
  absl::InlinedVector<ProbingRectangle::Edge, 4> candidates;
  absl::InlinedVector<float, 4> weights;
  while (!ranges.IsMinimal()) {
    const IntegerValue rect_area = ranges.GetCurrentRectangleArea();
    const IntegerValue min_energy = ranges.GetMinimumEnergy();
    if (min_energy > rect_area) {
      result.push_back(ranges.GetCurrentRectangle());
    }
    if (min_energy == 0) {
      break;
    }
    candidates.clear();
    weights.clear();

    for (int border_idx = 0; border_idx < 4; ++border_idx) {
      const ProbingRectangle::Edge border =
          static_cast<ProbingRectangle::Edge>(border_idx);
      if (!ranges.CanShrink(border)) {
        continue;
      }
      candidates.push_back(border);
      const IntegerValue delta_area = ranges.GetShrinkDeltaArea(border);
      const IntegerValue delta_energy = ranges.GetShrinkDeltaEnergy(border);
      const IntegerValue delta_slack = delta_energy - delta_area;
      const int table_lookup = std::max(
          0, std::min((int)(delta_slack.value() * 5 * inv_temp + 50), 100));
      weights.push_back(cached_probabilities->at(table_lookup));
    }
    // Pick a change with a probability proportional to exp(- delta_E / Temp)
    absl::discrete_distribution<int> dist(weights.begin(), weights.end());
    ranges.Shrink(candidates.at(dist(random)));
  }
  CHECK_GT(ranges.GetCurrentRectangleArea(), 0);
  if (ranges.GetMinimumEnergy() > ranges.GetCurrentRectangleArea()) {
    result.push_back(ranges.GetCurrentRectangle());
  }
  return result;
}

}  // namespace sat
}  // namespace operations_research
