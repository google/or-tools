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
#include "ortools/sat/sat_solver.h"
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
  neighbors_.resize(num_boxes_ * (num_boxes_ - 1));
  neighbors_begins_.resize(num_boxes_);
  neighbors_ends_.resize(num_boxes_);
  for (int i = 0; i < num_boxes_; ++i) {
    const int begin = i * (num_boxes_ - 1);
    neighbors_begins_[i] = begin;
    neighbors_ends_[i] = begin + (num_boxes_ - 1);
    for (int j = 0; j < num_boxes_; ++j) {
      if (j == i) continue;
      neighbors_[begin + (j > i ? j - 1 : j)] = j;
    }
  }
}

NonOverlappingRectanglesPropagator::~NonOverlappingRectanglesPropagator() {}

bool NonOverlappingRectanglesPropagator::Propagate() {
  cached_areas_.resize(num_boxes_);
  for (int box = 0; box < num_boxes_; ++box) {
    // We assume that the min-size of a box never changes.
    cached_areas_[box] = x_.DurationMin(box) * y_.DurationMin(box);
  }

  while (true) {
    const int64 saved_stamp = integer_trail_->num_enqueues();
    for (int box = 0; box < num_boxes_; ++box) {
      if (!strict_ && cached_areas_[box] == 0) continue;

      UpdateNeighbors(box);
      if (!FailWhenEnergyIsTooLarge(box)) return false;

      const int end = neighbors_ends_[box];
      for (int i = neighbors_begins_[box]; i < end; ++i) {
        if (!PushOneBox(box, neighbors_[i])) return false;
      }
    }
    if (saved_stamp == integer_trail_->num_enqueues()) break;
  }

  return true;
}

void NonOverlappingRectanglesPropagator::RegisterWith(
    GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  x_.WatchAllTasks(id, watcher);
  y_.WatchAllTasks(id, watcher);
  for (int t = 0; t < num_boxes_; ++t) {
    watcher->RegisterReversibleInt(id, &neighbors_ends_[t]);
  }
}

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

void NonOverlappingRectanglesPropagator::UpdateNeighbors(int box) {
  tmp_removed_.clear();
  cached_distance_to_bounding_box_.resize(num_boxes_);
  const IntegerValue box_x_min = x_.StartMin(box);
  const IntegerValue box_x_max = x_.EndMax(box);
  const IntegerValue box_y_min = y_.StartMin(box);
  const IntegerValue box_y_max = y_.EndMax(box);
  int new_index = neighbors_begins_[box];
  const int end = neighbors_ends_[box];
  for (int i = new_index; i < end; ++i) {
    const int other = neighbors_[i];

    const IntegerValue other_x_min = x_.StartMin(other);
    const IntegerValue other_x_max = x_.EndMax(other);
    if (IntervalAreDisjointForSure(box_x_min, box_x_max, other_x_min,
                                   other_x_max)) {
      tmp_removed_.push_back(other);
      continue;
    }

    const IntegerValue other_y_min = y_.StartMin(other);
    const IntegerValue other_y_max = y_.EndMax(other);
    if (IntervalAreDisjointForSure(box_y_min, box_y_max, other_y_min,
                                   other_y_max)) {
      tmp_removed_.push_back(other);
      continue;
    }

    neighbors_[new_index++] = other;
    cached_distance_to_bounding_box_[other] =
        std::max(DistanceToBoundingInterval(box_x_min, box_x_max, other_x_min,
                                            other_x_max),
                 DistanceToBoundingInterval(box_y_min, box_y_max, other_y_min,
                                            other_y_max));
  }
  neighbors_ends_[box] = new_index;
  for (int i = 0; i < tmp_removed_.size();) {
    neighbors_[new_index++] = tmp_removed_[i++];
  }
  IncrementalSort(neighbors_.begin() + neighbors_begins_[box],
                  neighbors_.begin() + neighbors_ends_[box],
                  [this](int i, int j) {
                    return cached_distance_to_bounding_box_[i] <
                           cached_distance_to_bounding_box_[j];
                  });
}

bool NonOverlappingRectanglesPropagator::FailWhenEnergyIsTooLarge(int box) {
  // Note that we only consider the smallest dimension of each boxes here.
  IntegerValue area_min_x = x_.StartMin(box);
  IntegerValue area_max_x = x_.EndMax(box);
  IntegerValue area_min_y = y_.StartMin(box);
  IntegerValue area_max_y = y_.EndMax(box);

  IntegerValue sum_of_areas = cached_areas_[box];

  IntegerValue total_sum_of_areas = sum_of_areas;
  const int end = neighbors_ends_[box];
  for (int i = neighbors_begins_[box]; i < end; ++i) {
    const int other = neighbors_[i];
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
  for (int i = neighbors_begins_[box]; i < end; ++i) {
    const int other = neighbors_[i];
    if (cached_areas_[other] == 0) continue;

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
      for (int j = neighbors_begins_[box]; j <= i; ++j) {
        const int other = neighbors_[j];
        if (cached_areas_[other] == 0) continue;
        add_box_energy_in_rectangle_reason(other);
      }
      x_.ImportOtherReasons(y_);
      return x_.ReportConflict();
    }
  }
  return true;
}

bool NonOverlappingRectanglesPropagator::PushOneBox(int box, int other) {
  if (!strict_ && cached_areas_[other] == 0) return true;

  // For each direction and each order, we test if the boxes can be disjoint.
  const int state = (x_.EndMin(box) <= x_.StartMax(other)) +
                    2 * (x_.EndMin(other) <= x_.StartMax(box)) +
                    4 * (y_.EndMin(box) <= y_.StartMax(other)) +
                    8 * (y_.EndMin(other) <= y_.StartMax(box));

  if (state != 0 && state != 1 && state != 2 && state != 4 && state != 8) {
    return true;
  }

  const auto left_box_before_right_box =
      [](int left, int right, SchedulingConstraintHelper* helper) {
        // left box pushes right box.
        const IntegerValue left_end_min = helper->EndMin(left);
        if (left_end_min > helper->StartMin(right)) {
          // Store reasons state.
          const int literal_size = helper->MutableLiteralReason()->size();
          const int integer_size = helper->MutableIntegerReason()->size();

          helper->AddEndMinReason(left, left_end_min);
          if (!helper->IncreaseStartMin(right, left_end_min)) {
            return false;
          }

          // Restore the reasons to the state before the increase of the start.
          helper->MutableLiteralReason()->resize(literal_size);
          helper->MutableIntegerReason()->resize(integer_size);
        }

        // right box pushes left box.
        const IntegerValue right_start_max = helper->StartMax(right);
        if (right_start_max < helper->EndMax(left)) {
          helper->AddStartMaxReason(right, right_start_max);
          return helper->DecreaseEndMax(left, right_start_max);
        }

        return true;
      };

  // Clean up reasons.
  x_.ClearReason();
  y_.ClearReason();

  // This is an "hack" to be able to easily test for none or for one
  // and only one of the conditions below.
  switch (state) {
    case 0: {
      x_.AddReasonForBeingBefore(box, other);
      x_.AddReasonForBeingBefore(other, box);
      y_.AddReasonForBeingBefore(box, other);
      y_.AddReasonForBeingBefore(other, box);
      x_.ImportOtherReasons(y_);
      return x_.ReportConflict();
    }
    case 1:
      y_.AddReasonForBeingBefore(box, other);
      y_.AddReasonForBeingBefore(other, box);
      x_.AddReasonForBeingBefore(box, other);
      x_.ImportOtherReasons(y_);
      return left_box_before_right_box(box, other, &x_);
    case 2:
      y_.AddReasonForBeingBefore(box, other);
      y_.AddReasonForBeingBefore(other, box);
      x_.AddReasonForBeingBefore(other, box);
      x_.ImportOtherReasons(y_);
      return left_box_before_right_box(other, box, &x_);
    case 4:
      x_.AddReasonForBeingBefore(box, other);
      x_.AddReasonForBeingBefore(other, box);
      y_.AddReasonForBeingBefore(box, other);
      y_.ImportOtherReasons(x_);
      return left_box_before_right_box(box, other, &y_);
    case 8:
      x_.AddReasonForBeingBefore(box, other);
      x_.AddReasonForBeingBefore(other, box);
      y_.AddReasonForBeingBefore(other, box);
      y_.ImportOtherReasons(x_);
      return left_box_before_right_box(other, box, &y_);
    default:
      break;
  }
  return true;
}

}  // namespace sat
}  // namespace operations_research
