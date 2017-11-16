// Copyright 2010-2017 Google
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

#include "ortools/sat/cp_constraints.h"

#include <algorithm>
#include <unordered_map>

#include "ortools/base/map_util.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/util/sort.h"

namespace operations_research {
namespace sat {

bool BooleanXorPropagator::Propagate() {
  bool sum = false;
  int unassigned_index = -1;
  for (int i = 0; i < literals_.size(); ++i) {
    const Literal l = literals_[i];
    if (trail_->Assignment().LiteralIsFalse(l)) {
      sum ^= false;
    } else if (trail_->Assignment().LiteralIsTrue(l)) {
      sum ^= true;
    } else {
      // If we have more than one unassigned literal, we can't deduce anything.
      if (unassigned_index != -1) return true;
      unassigned_index = i;
    }
  }

  // Propagates?
  if (unassigned_index != -1) {
    literal_reason_.clear();
    for (int i = 0; i < literals_.size(); ++i) {
      if (i == unassigned_index) continue;
      const Literal l = literals_[i];
      literal_reason_.push_back(
          trail_->Assignment().LiteralIsFalse(l) ? l : l.Negated());
    }
    const Literal u = literals_[unassigned_index];
    integer_trail_->EnqueueLiteral(sum == value_ ? u.Negated() : u,
                                   literal_reason_, {});
    return true;
  }

  // Ok.
  if (sum == value_) return true;

  // Conflict.
  std::vector<Literal>* conflict = trail_->MutableConflict();
  conflict->clear();
  for (int i = 0; i < literals_.size(); ++i) {
    const Literal l = literals_[i];
    conflict->push_back(trail_->Assignment().LiteralIsFalse(l) ? l
                                                               : l.Negated());
  }
  return false;
}

void BooleanXorPropagator::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  for (const Literal& l : literals_) {
    watcher->WatchLiteral(l, id);
    watcher->WatchLiteral(l.Negated(), id);
  }
}

// ----- CpPropagator -----

CpPropagator::CpPropagator(IntegerTrail* integer_trail)
    : integer_trail_(integer_trail) {}

CpPropagator::~CpPropagator() {}

IntegerValue CpPropagator::Min(IntegerVariable v) const {
  return integer_trail_->LowerBound(v);
}

IntegerValue CpPropagator::Max(IntegerVariable v) const {
  return integer_trail_->UpperBound(v);
}

bool CpPropagator::SetMin(IntegerVariable v, IntegerValue val,
                          const std::vector<IntegerLiteral>& reason) {
  IntegerValue current_min = Min(v);
  if (val > current_min &&
      !integer_trail_->Enqueue(IntegerLiteral::GreaterOrEqual(v, val),
                               /*literal_reason=*/{}, reason)) {
    return false;
  }
  return true;
}

bool CpPropagator::SetMax(IntegerVariable v, IntegerValue val,
                          const std::vector<IntegerLiteral>& reason) {
  const IntegerValue current_max = Max(v);
  if (val < current_max &&
      !integer_trail_->Enqueue(IntegerLiteral::LowerOrEqual(v, val),
                               /*literal_reason=*/{}, reason)) {
    return false;
  }
  return true;
}

bool CpPropagator::SetMin(IntegerValue v, IntegerValue val,
                          const std::vector<IntegerLiteral>& reason) {
  if (val > v) {
    return integer_trail_->ReportConflict(reason);
  }
  return true;
}

bool CpPropagator::SetMax(IntegerValue v, IntegerValue val,
                          const std::vector<IntegerLiteral>& reason) {
  if (val < v) {
    return integer_trail_->ReportConflict(reason);
  }
  return true;
}

void CpPropagator::AddLowerBoundReason(
    IntegerVariable v, std::vector<IntegerLiteral>* reason) const {
  reason->push_back(integer_trail_->LowerBoundAsLiteral(v));
}

void CpPropagator::AddUpperBoundReason(
    IntegerVariable v, std::vector<IntegerLiteral>* reason) const {
  reason->push_back(integer_trail_->UpperBoundAsLiteral(v));
}

void CpPropagator::AddBoundsReason(IntegerVariable v,
                                   std::vector<IntegerLiteral>* reason) const {
  reason->push_back(integer_trail_->LowerBoundAsLiteral(v));
  reason->push_back(integer_trail_->UpperBoundAsLiteral(v));
}

template <class S>
NonOverlappingRectanglesPropagator<S>::NonOverlappingRectanglesPropagator(
    const std::vector<IntegerVariable>& x,
    const std::vector<IntegerVariable>& y, const std::vector<S>& dx,
    const std::vector<S>& dy, bool strict, IntegerTrail* integer_trail)
    : CpPropagator(integer_trail),
      x_(x),
      y_(y),
      dx_(dx),
      dy_(dy),
      strict_(strict) {
  const int n = x.size();
  CHECK_GT(n, 0);
  neighbors_.resize(n * (n - 1));
  neighbors_begins_.resize(n);
  neighbors_ends_.resize(n);
  for (int i = 0; i < n; ++i) {
    const int begin = i * (n - 1);
    neighbors_begins_[i] = begin;
    neighbors_ends_[i] = begin + (n - 1);
    for (int j = 0; j < n; ++j) {
      if (j == i) continue;
      neighbors_[begin + (j > i ? j - 1 : j)] = j;
    }
  }
}

template <class S>
NonOverlappingRectanglesPropagator<S>::~NonOverlappingRectanglesPropagator() {}

template <class S>
bool NonOverlappingRectanglesPropagator<S>::Propagate() {
  const int n = x_.size();
  cached_areas_.resize(n);
  cached_min_end_x_.resize(n);
  cached_min_end_y_.resize(n);
  for (int box = 0; box < n; ++box) {
    // We never change the min-size of a box, so this stays valid.
    cached_areas_[box] = Min(dx_[box]) * Min(dy_[box]);

    // These will be update on each push.
    cached_min_end_x_[box] = Min(x_[box]) + Min(dx_[box]);
    cached_min_end_y_[box] = Min(y_[box]) + Min(dy_[box]);
  }

  while (true) {
    const int64 saved_stamp = integer_trail_->num_enqueues();
    for (int box = 0; box < n; ++box) {
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

template <class S>
void NonOverlappingRectanglesPropagator<S>::RegisterWith(
    GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  for (const IntegerVariable& var : x_) watcher->WatchIntegerVariable(var, id);
  for (const IntegerVariable& var : y_) watcher->WatchIntegerVariable(var, id);
  for (const S& var : dx_) watcher->WatchLowerBound(var, id);
  for (const S& var : dy_) watcher->WatchLowerBound(var, id);
  for (int i = 0; i < x_.size(); ++i) {
    watcher->RegisterReversibleInt(id, &neighbors_ends_[i]);
  }
}

template <class S>
void NonOverlappingRectanglesPropagator<S>::AddBoxReason(int box) {
  AddBoundsReason(x_[box], &integer_reason_);
  AddBoundsReason(y_[box], &integer_reason_);
  AddLowerBoundReason(dx_[box], &integer_reason_);
  AddLowerBoundReason(dy_[box], &integer_reason_);
}

template <class S>
void NonOverlappingRectanglesPropagator<S>::AddBoxReason(int box,
                                                         IntegerValue xmin,
                                                         IntegerValue xmax,
                                                         IntegerValue ymin,
                                                         IntegerValue ymax) {
  integer_reason_.push_back(IntegerLiteral::GreaterOrEqual(x_[box], xmin));
  integer_reason_.push_back(
      IntegerLiteral::LowerOrEqual(x_[box], xmax - Min(dx_[box])));
  integer_reason_.push_back(IntegerLiteral::GreaterOrEqual(y_[box], ymin));
  integer_reason_.push_back(
      IntegerLiteral::LowerOrEqual(y_[box], ymax - Min(dy_[box])));
  AddLowerBoundReason(dx_[box], &integer_reason_);
  AddLowerBoundReason(dy_[box], &integer_reason_);
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

template <class S>
void NonOverlappingRectanglesPropagator<S>::UpdateNeighbors(int box) {
  tmp_removed_.clear();
  cached_distance_to_bounding_box_.resize(x_.size());
  const IntegerValue box_x_min = Min(x_[box]);
  const IntegerValue box_x_max = Max(x_[box]) + Max(dx_[box]);
  const IntegerValue box_y_min = Min(y_[box]);
  const IntegerValue box_y_max = Max(y_[box]) + Max(dy_[box]);
  int new_index = neighbors_begins_[box];
  const int end = neighbors_ends_[box];
  for (int i = new_index; i < end; ++i) {
    const int other = neighbors_[i];

    const IntegerValue other_x_min = Min(x_[other]);
    const IntegerValue other_x_max = Max(x_[other]) + Max(dx_[other]);
    if (IntervalAreDisjointForSure(box_x_min, box_x_max, other_x_min,
                                   other_x_max)) {
      tmp_removed_.push_back(other);
      continue;
    }

    const IntegerValue other_y_min = Min(y_[other]);
    const IntegerValue other_y_max = Max(y_[other]) + Max(dy_[other]);
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

template <class S>
bool NonOverlappingRectanglesPropagator<S>::FailWhenEnergyIsTooLarge(int box) {
  // Note that we only consider the smallest dimension of each boxes here.
  IntegerValue area_min_x = Min(x_[box]);
  IntegerValue area_max_x = Max(x_[box]) + Min(dx_[box]);
  IntegerValue area_min_y = Min(y_[box]);
  IntegerValue area_max_y = Max(y_[box]) + Min(dy_[box]);
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
    area_min_x = std::min(area_min_x, Min(x_[other]));
    area_max_x = std::max(area_max_x, Max(x_[other]) + Min(dx_[other]));
    area_min_y = std::min(area_min_y, Min(y_[other]));
    area_max_y = std::max(area_max_y, Max(y_[other]) + Min(dy_[other]));

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
      AddBoxReason(box, area_min_x, area_max_x, area_min_y, area_max_y);
      for (int j = neighbors_begins_[box]; j <= i; ++j) {
        const int other = neighbors_[j];
        if (cached_areas_[other] == 0) continue;
        AddBoxReason(other, area_min_x, area_max_x, area_min_y, area_max_y);
      }
      return integer_trail_->ReportConflict(integer_reason_);
    }
  }
  return true;
}

template <class S>
bool NonOverlappingRectanglesPropagator<S>::FirstBoxIsBeforeSecondBox(
    const std::vector<IntegerVariable>& pos, const std::vector<S>& size,
    int box, int other, std::vector<IntegerValue>* min_end) {
  const IntegerValue other_min_pos = (*min_end)[box];
  if (other_min_pos > Min(pos[other])) {
    if (integer_reason_.empty()) {
      AddBoxReason(box);
      AddBoxReason(other);
    }
    (*min_end)[other] = other_min_pos + Min(size[other]);
    if (!SetMin(pos[other], other_min_pos, integer_reason_)) return false;
  }
  const IntegerValue box_max_pos = Max(pos[other]) - Min(size[box]);
  if (box_max_pos < Max(pos[box])) {
    if (integer_reason_.empty()) {
      AddBoxReason(box);
      AddBoxReason(other);
    }
    if (!SetMax(pos[box], box_max_pos, integer_reason_)) return false;
  }
  const IntegerValue box_max_size = Max(pos[other]) - Min(pos[box]);
  if (box_max_size < Max(size[box])) {
    if (integer_reason_.empty()) {
      AddBoxReason(box);
      AddBoxReason(other);
    }
    if (!SetMax(size[box], box_max_size, integer_reason_)) return false;
  }
  return true;
}

template <class S>
bool NonOverlappingRectanglesPropagator<S>::PushOneBox(int box, int other) {
  if (!strict_ && cached_areas_[other] == 0) return true;

  // For each direction and each order, we test if the boxes can be disjoint.
  const int state = (cached_min_end_x_[box] <= Max(x_[other])) +
                    2 * (cached_min_end_x_[other] <= Max(x_[box])) +
                    4 * (cached_min_end_y_[box] <= Max(y_[other])) +
                    8 * (cached_min_end_y_[other] <= Max(y_[box]));

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
      return FirstBoxIsBeforeSecondBox(x_, dx_, box, other, &cached_min_end_x_);
    case 2:
      return FirstBoxIsBeforeSecondBox(x_, dx_, other, box, &cached_min_end_x_);
    case 4:
      return FirstBoxIsBeforeSecondBox(y_, dy_, box, other, &cached_min_end_y_);
    case 8:
      return FirstBoxIsBeforeSecondBox(y_, dy_, other, box, &cached_min_end_y_);
    default:
      break;
  }
  return true;
}

template class NonOverlappingRectanglesPropagator<IntegerVariable>;
template class NonOverlappingRectanglesPropagator<IntegerValue>;

GreaterThanAtLeastOneOfPropagator::GreaterThanAtLeastOneOfPropagator(
    IntegerVariable target_var, const std::vector<IntegerVariable>& vars,
    const std::vector<IntegerValue>& offsets,
    const std::vector<Literal>& selectors, Model* model)
    : target_var_(target_var),
      vars_(vars),
      offsets_(offsets),
      selectors_(selectors),
      trail_(model->GetOrCreate<Trail>()),
      integer_trail_(model->GetOrCreate<IntegerTrail>()) {}

bool GreaterThanAtLeastOneOfPropagator::Propagate() {
  // Compute the min of the lower-bound for the still possible variables.
  // TODO(user): This could be optimized by keeping more info from the last
  // Propagate() calls.
  IntegerValue target_min = kMaxIntegerValue;
  const IntegerValue current_min = integer_trail_->LowerBound(target_var_);
  for (int i = 0; i < vars_.size(); ++i) {
    if (trail_->Assignment().LiteralIsTrue(selectors_[i])) return true;
    if (trail_->Assignment().LiteralIsFalse(selectors_[i])) continue;
    target_min = std::min(target_min,
                          integer_trail_->LowerBound(vars_[i]) + offsets_[i]);

    // Abort if we can't get a better bound.
    if (target_min <= current_min) return true;
  }
  if (target_min == kMaxIntegerValue) {
    // All false, conflit.
    *(trail_->MutableConflict()) = selectors_;
    return false;
  }

  literal_reason_.clear();
  integer_reason_.clear();
  for (int i = 0; i < vars_.size(); ++i) {
    if (trail_->Assignment().LiteralIsFalse(selectors_[i])) {
      literal_reason_.push_back(selectors_[i]);
    } else {
      integer_reason_.push_back(
          IntegerLiteral::GreaterOrEqual(vars_[i], target_min - offsets_[i]));
    }
  }
  return integer_trail_->Enqueue(
      IntegerLiteral::GreaterOrEqual(target_var_, target_min), literal_reason_,
      integer_reason_);
}

void GreaterThanAtLeastOneOfPropagator::RegisterWith(
    GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  for (const Literal l : selectors_) watcher->WatchLiteral(l.Negated(), id);
  for (const IntegerVariable v : vars_) watcher->WatchLowerBound(v, id);
}

}  // namespace sat
}  // namespace operations_research
