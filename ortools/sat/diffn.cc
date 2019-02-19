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
#include "ortools/base/map_util.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/util/sort.h"

namespace operations_research {
namespace sat {

IntegerValue NonOverlappingRectanglesPropagator::Min(IntegerVariable v) const {
  return integer_trail_->LowerBound(v);
}

IntegerValue NonOverlappingRectanglesPropagator::Max(IntegerVariable v) const {
  return integer_trail_->UpperBound(v);
}

bool NonOverlappingRectanglesPropagator::SetMin(
    IntegerVariable v, IntegerValue val,
    const std::vector<IntegerLiteral>& reason) {
  IntegerValue current_min = Min(v);
  if (val > current_min &&
      !integer_trail_->Enqueue(IntegerLiteral::GreaterOrEqual(v, val),
                               /*literal_reason=*/{}, reason)) {
    return false;
  }
  return true;
}

bool NonOverlappingRectanglesPropagator::SetMax(
    IntegerVariable v, IntegerValue val,
    const std::vector<IntegerLiteral>& reason) {
  const IntegerValue current_max = Max(v);
  if (val < current_max &&
      !integer_trail_->Enqueue(IntegerLiteral::LowerOrEqual(v, val),
                               /*literal_reason=*/{}, reason)) {
    return false;
  }
  return true;
}

NonOverlappingRectanglesPropagator::NonOverlappingRectanglesPropagator(
    const std::vector<IntervalVariable>& x,
    const std::vector<IntervalVariable>& y, bool strict, Model* model,
    IntegerTrail* integer_trail)
    : num_boxes_(x.size()),
      x_(x),
      y_(y),
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

  auto* repository = model->GetOrCreate<IntervalsRepository>();
  start_x_vars_.clear();
  end_x_vars_.clear();
  duration_x_vars_.clear();
  fixed_duration_x_.clear();
  for (const IntervalVariable i : x_) {
    if (repository->SizeVar(i) == kNoIntegerVariable) {
      duration_x_vars_.push_back(kNoIntegerVariable);
      fixed_duration_x_.push_back(repository->MinSize(i));
    } else {
      duration_x_vars_.push_back(repository->SizeVar(i));
      fixed_duration_x_.push_back(IntegerValue(0));
    }
    start_x_vars_.push_back(repository->StartVar(i));
    end_x_vars_.push_back(repository->EndVar(i));
  }

  start_y_vars_.clear();
  end_y_vars_.clear();
  duration_y_vars_.clear();
  fixed_duration_y_.clear();
  for (const IntervalVariable i : y_) {
    if (repository->SizeVar(i) == kNoIntegerVariable) {
      duration_y_vars_.push_back(kNoIntegerVariable);
      fixed_duration_y_.push_back(repository->MinSize(i));
    } else {
      duration_y_vars_.push_back(repository->SizeVar(i));
      fixed_duration_y_.push_back(IntegerValue(0));
    }
    start_y_vars_.push_back(repository->StartVar(i));
    end_y_vars_.push_back(repository->EndVar(i));
  }
}

NonOverlappingRectanglesPropagator::~NonOverlappingRectanglesPropagator() {}

bool NonOverlappingRectanglesPropagator::Propagate() {
  cached_areas_.resize(num_boxes_);
  for (int box = 0; box < num_boxes_; ++box) {
    // We never change the min-size of a box, so this stays valid.
    const IntegerValue duration_x_min =
        duration_x_vars_[box] == kNoIntegerVariable
            ? fixed_duration_x_[box]
            : Min(duration_x_vars_[box]);
    const IntegerValue duration_y_min =
        duration_y_vars_[box] == kNoIntegerVariable
            ? fixed_duration_y_[box]
            : Min(duration_y_vars_[box]);
    cached_areas_[box] = duration_x_min * duration_y_min;
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
  for (int t = 0; t < num_boxes_; ++t) {
    watcher->WatchIntegerVariable(start_x_vars_[t], id);
    watcher->WatchIntegerVariable(end_x_vars_[t], id);
    if (duration_x_vars_[t] != kNoIntegerVariable) {
      watcher->WatchLowerBound(duration_x_vars_[t], id);
    }
    watcher->WatchIntegerVariable(start_y_vars_[t], id);
    watcher->WatchIntegerVariable(end_y_vars_[t], id);
    if (duration_y_vars_[t] != kNoIntegerVariable) {
      watcher->WatchLowerBound(duration_y_vars_[t], id);
    }
    watcher->RegisterReversibleInt(id, &neighbors_ends_[t]);
  }
}

void NonOverlappingRectanglesPropagator::AddBoxReason(int box) {
  integer_reason_.push_back(
      integer_trail_->LowerBoundAsLiteral(start_x_vars_[box]));
  integer_reason_.push_back(
      integer_trail_->UpperBoundAsLiteral(start_x_vars_[box]));
  if (duration_x_vars_[box] != kNoIntegerVariable) {
    integer_reason_.push_back(
        integer_trail_->LowerBoundAsLiteral(duration_x_vars_[box]));
    integer_reason_.push_back(
        integer_trail_->UpperBoundAsLiteral(end_x_vars_[box]));
  } else {
    integer_reason_.push_back(
        integer_trail_->UpperBoundAsLiteral(start_x_vars_[box]));
  }

  integer_reason_.push_back(
      integer_trail_->LowerBoundAsLiteral(start_y_vars_[box]));
  integer_reason_.push_back(
      integer_trail_->UpperBoundAsLiteral(start_y_vars_[box]));
  if (duration_y_vars_[box] != kNoIntegerVariable) {
    integer_reason_.push_back(
        integer_trail_->LowerBoundAsLiteral(duration_y_vars_[box]));
    integer_reason_.push_back(
        integer_trail_->UpperBoundAsLiteral(end_y_vars_[box]));
  } else {
    integer_reason_.push_back(
        integer_trail_->UpperBoundAsLiteral(start_y_vars_[box]));
  }
}

void NonOverlappingRectanglesPropagator::AddBoxInRectangleReason(
    int box, IntegerValue xmin, IntegerValue xmax, IntegerValue ymin,
    IntegerValue ymax) {
  integer_reason_.push_back(
      IntegerLiteral::GreaterOrEqual(start_x_vars_[box], xmin));
  if (duration_x_vars_[box] == kNoIntegerVariable) {
    integer_reason_.push_back(IntegerLiteral::LowerOrEqual(
        start_x_vars_[box], xmax - fixed_duration_x_[box]));
  } else {
    integer_reason_.push_back(
        IntegerLiteral::LowerOrEqual(end_x_vars_[box], xmax));
    integer_reason_.push_back(
        integer_trail_->LowerBoundAsLiteral(duration_y_vars_[box]));
  }

  integer_reason_.push_back(
      IntegerLiteral::GreaterOrEqual(start_y_vars_[box], ymin));
  if (duration_y_vars_[box] == kNoIntegerVariable) {
    integer_reason_.push_back(IntegerLiteral::LowerOrEqual(
        start_y_vars_[box], ymax - fixed_duration_y_[box]));
  } else {
    integer_reason_.push_back(
        IntegerLiteral::LowerOrEqual(end_y_vars_[box], ymax));
    integer_reason_.push_back(
        integer_trail_->LowerBoundAsLiteral(duration_y_vars_[box]));
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
  const IntegerValue box_x_min = Min(start_x_vars_[box]);
  const IntegerValue box_x_max = Max(end_x_vars_[box]);
  const IntegerValue box_y_min = Min(start_y_vars_[box]);
  const IntegerValue box_y_max = Max(end_y_vars_[box]);
  int new_index = neighbors_begins_[box];
  const int end = neighbors_ends_[box];
  for (int i = new_index; i < end; ++i) {
    const int other = neighbors_[i];

    const IntegerValue other_x_min = Min(start_x_vars_[other]);
    const IntegerValue other_x_max = Max(end_x_vars_[other]);
    if (IntervalAreDisjointForSure(box_x_min, box_x_max, other_x_min,
                                   other_x_max)) {
      tmp_removed_.push_back(other);
      continue;
    }

    const IntegerValue other_y_min = Min(start_y_vars_[other]);
    const IntegerValue other_y_max = Max(end_y_vars_[other]);
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
  IntegerValue area_min_x = Min(start_x_vars_[box]);
  IntegerValue area_max_x = Max(end_x_vars_[box]);
  IntegerValue area_min_y = Min(start_y_vars_[box]);
  IntegerValue area_max_y = Max(end_y_vars_[box]);
  IntegerValue sum_of_areas = cached_areas_[box];

  IntegerValue total_sum_of_areas = sum_of_areas;
  const int end = neighbors_ends_[box];
  for (int i = neighbors_begins_[box]; i < end; ++i) {
    const int other = neighbors_[i];
    total_sum_of_areas += cached_areas_[other];
  }

  // TODO(user): Is there a better order, maybe sort by distance
  // with the current box.
  for (int i = neighbors_begins_[box]; i < end; ++i) {
    const int other = neighbors_[i];
    if (cached_areas_[other] == 0) continue;

    // Update Bounding box.
    area_min_x = std::min(area_min_x, Min(start_x_vars_[other]));
    area_max_x = std::max(area_max_x, Max(end_x_vars_[other]));
    area_min_y = std::min(area_min_y, Min(start_y_vars_[other]));
    area_max_y = std::max(area_max_y, Max(end_y_vars_[other]));

    // Update sum of areas.
    sum_of_areas += cached_areas_[other];
    const IntegerValue bounding_area =
        (area_max_x - area_min_x) * (area_max_y - area_min_y);
    if (bounding_area >= total_sum_of_areas) {
      // Nothing will be deduced. Exiting.
      return true;
    }

    if (sum_of_areas > bounding_area) {
      integer_reason_.clear();
      AddBoxInRectangleReason(box, area_min_x, area_max_x, area_min_y,
                              area_max_y);
      for (int j = neighbors_begins_[box]; j <= i; ++j) {
        const int other = neighbors_[j];
        if (cached_areas_[other] == 0) continue;
        AddBoxInRectangleReason(other, area_min_x, area_max_x, area_min_y,
                                area_max_y);
      }
      return integer_trail_->ReportConflict(integer_reason_);
    }
  }
  return true;
}

bool NonOverlappingRectanglesPropagator::FirstBoxIsBeforeSecondBox(
    const std::vector<IntegerVariable>& starts,
    const std::vector<IntegerVariable>& sizes,
    const std::vector<IntegerVariable>& ends,
    const std::vector<IntegerValue>& fixed_sizes, int box, int other) {
  const IntegerValue min_end = Min(ends[box]);
  if (min_end > Min(starts[other])) {
    integer_reason_.clear();
    AddBoxReason(box);
    AddBoxReason(other);
    if (!SetMin(starts[other], min_end, integer_reason_)) return false;
  }

  const IntegerValue other_max_start = Max(starts[other]);
  if (other_max_start < Max(ends[box])) {
    integer_reason_.clear();
    AddBoxReason(box);
    AddBoxReason(other);
    if (sizes[box] == kNoIntegerVariable) {
      if (!SetMax(starts[box], other_max_start - fixed_sizes[box],
                  integer_reason_)) {
        return false;
      }
    } else {
      if (!SetMax(ends[box], other_max_start, integer_reason_)) return false;
    }
  }
  return true;
}

bool NonOverlappingRectanglesPropagator::PushOneBox(int box, int other) {
  if (!strict_ && cached_areas_[other] == 0) return true;

  // For each direction and each order, we test if the boxes can be disjoint.
  const int state = (Min(end_x_vars_[box]) <= Max(start_x_vars_[other])) +
                    2 * (Min(end_x_vars_[other]) <= Max(start_x_vars_[box])) +
                    4 * (Min(end_y_vars_[box]) <= Max(start_y_vars_[other])) +
                    8 * (Min(end_y_vars_[other]) <= Max(start_y_vars_[box]));

  // This is an "hack" to be able to easily test for none or for one
  // and only one of the conditions below.
  integer_reason_.clear();
  switch (state) {
    case 0: {
      AddBoxReason(box);
      AddBoxReason(other);
      return integer_trail_->ReportConflict(integer_reason_);
    }
    case 1:
      return FirstBoxIsBeforeSecondBox(start_x_vars_, duration_x_vars_,
                                       end_x_vars_, fixed_duration_x_, box,
                                       other);
    case 2:
      return FirstBoxIsBeforeSecondBox(start_x_vars_, duration_x_vars_,
                                       end_x_vars_, fixed_duration_x_, other,
                                       box);
    case 4:
      return FirstBoxIsBeforeSecondBox(start_y_vars_, duration_y_vars_,
                                       end_y_vars_, fixed_duration_y_, box,
                                       other);
    case 8:
      return FirstBoxIsBeforeSecondBox(start_y_vars_, duration_y_vars_,
                                       end_y_vars_, fixed_duration_y_, other,
                                       box);
    default:
      break;
  }
  return true;
}

}  // namespace sat
}  // namespace operations_research
