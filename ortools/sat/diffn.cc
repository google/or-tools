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
#include "ortools/base/stl_util.h"
#include "ortools/sat/cumulative.h"
#include "ortools/sat/disjunctive.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/theta_tree.h"
#include "ortools/util/sort.h"

namespace operations_research {
namespace sat {

void AddCumulativeRelaxation(const std::vector<IntervalVariable>& x,
                             const std::vector<IntervalVariable>& y,
                             Model* model) {
  IntervalsRepository* const repository =
      model->GetOrCreate<IntervalsRepository>();
  std::vector<IntegerVariable> starts;
  std::vector<IntegerVariable> sizes;
  std::vector<IntegerVariable> ends;
  int64 min_starts = kint64max;
  int64 max_ends = kint64min;

  for (const IntervalVariable& interval : y) {
    starts.push_back(repository->StartVar(interval));
    IntegerVariable s_var = repository->SizeVar(interval);
    if (s_var == kNoIntegerVariable) {
      s_var = model->Add(
          ConstantIntegerVariable(repository->MinSize(interval).value()));
    }
    sizes.push_back(s_var);
    ends.push_back(repository->EndVar(interval));
    min_starts = std::min(min_starts, model->Get(LowerBound(starts.back())));
    max_ends = std::max(max_ends, model->Get(UpperBound(ends.back())));
  }

  const IntegerVariable min_start_var =
      model->Add(NewIntegerVariable(min_starts, max_ends));
  model->Add(IsEqualToMinOf(min_start_var, starts));

  const IntegerVariable max_end_var =
      model->Add(NewIntegerVariable(min_starts, max_ends));
  model->Add(IsEqualToMaxOf(max_end_var, ends));

  const IntegerVariable capacity =
      model->Add(NewIntegerVariable(0, CapSub(max_ends, min_starts)));
  const std::vector<int64> coeffs = {-1, -1, 1};
  model->Add(WeightedSumGreaterOrEqual({capacity, min_start_var, max_end_var},
                                       coeffs, 0));

  model->Add(Cumulative(x, sizes, capacity));
}

namespace {

// We want for different propagation to reuse as much as possible the same
// line. The idea behind this is to compute the 'canonical' line to use
// when explaining that boxes overlap on the 'y_dim' dimension. We compute
// the multiple of the biggest power of two that is common to all boxes.
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

std::vector<absl::Span<int>> SplitDisjointBoxes(
    const SchedulingConstraintHelper& x, absl::Span<int> boxes) {
  std::vector<absl::Span<int>> result;
  std::sort(boxes.begin(), boxes.end(),
            [&x](int a, int b) { return x.StartMin(a) < x.StartMin(b); });
  int current_start = 0;
  std::size_t current_length = 1;
  IntegerValue current_max_end = x.EndMax(boxes[0]);

  for (int b = 1; b < boxes.size(); ++b) {
    const int box = boxes[b];
    if (x.StartMin(box) < current_max_end) {
      // Merge.
      current_length++;
      current_max_end = std::max(current_max_end, x.EndMax(box));
    } else {
      if (current_length > 1) {  // Ignore lists of size 1.
        result.push_back({&boxes[current_start], current_length});
      }
      current_start = b;
      current_length = 1;
      current_max_end = x.EndMax(box);
    }
  }

  // Push last span.
  if (current_length > 1) {
    result.push_back({&boxes[current_start], current_length});
  }
  return result;
}

}  // namespace

#define RETURN_IF_FALSE(f) \
  if (!(f)) return false;

NonOverlappingRectanglesEnergyPropagator::
    NonOverlappingRectanglesEnergyPropagator(
        const std::vector<IntervalVariable>& x,
        const std::vector<IntervalVariable>& y, Model* model)
    : x_(x, model), y_(y, model) {}

NonOverlappingRectanglesEnergyPropagator::
    ~NonOverlappingRectanglesEnergyPropagator() {}

bool NonOverlappingRectanglesEnergyPropagator::Propagate() {
  cached_areas_.resize(x_.NumTasks());

  active_boxes_.clear();
  for (int box = 0; box < x_.NumTasks(); ++box) {
    cached_areas_[box] = x_.DurationMin(box) * y_.DurationMin(box);
    if (cached_areas_[box] == 0) continue;
    active_boxes_.push_back(box);
  }
  if (active_boxes_.size() <= 1) return true;

  const std::vector<absl::Span<int>> x_split =
      SplitDisjointBoxes(x_, absl::MakeSpan(active_boxes_));
  std::vector<absl::Span<int>> y_split;
  for (absl::Span<int> x_boxes : x_split) {
    y_split = SplitDisjointBoxes(y_, x_boxes);
    for (absl::Span<int> y_boxes : y_split) {
      for (const int box : y_boxes) {
        RETURN_IF_FALSE(FailWhenEnergyIsTooLarge(box, y_boxes));
      }
    }
  }

  return true;
}

int NonOverlappingRectanglesEnergyPropagator::RegisterWith(
    GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  x_.WatchAllTasks(id, watcher, /*watch_start_max=*/false,
                   /*watch_end_max=*/true);
  y_.WatchAllTasks(id, watcher, /*watch_start_max=*/false,
                   /*watch_end_max=*/true);
  return id;
}

void NonOverlappingRectanglesEnergyPropagator::SortBoxesIntoNeighbors(
    int box, absl::Span<const int> local_boxes) {
  const IntegerValue box_x_min = x_.StartMin(box);
  const IntegerValue box_x_max = x_.EndMax(box);
  const IntegerValue box_y_min = y_.StartMin(box);
  const IntegerValue box_y_max = y_.EndMax(box);

  neighbors_.clear();
  for (const int other_box : local_boxes) {
    if (other_box == box) continue;

    const IntegerValue other_x_min = x_.StartMin(other_box);
    const IntegerValue other_x_max = x_.EndMax(other_box);
    const IntegerValue other_y_min = y_.StartMin(other_box);
    const IntegerValue other_y_max = y_.EndMax(other_box);

    const IntegerValue span_x =
        std::max(box_x_max, other_x_max) - std::min(box_x_min, other_x_min) + 1;
    const IntegerValue span_y =
        std::max(box_y_max, other_y_max) - std::min(box_y_min, other_y_min) + 1;
    neighbors_.push_back({other_box, span_x * span_y});
  }
  std::sort(neighbors_.begin(), neighbors_.end());
}

bool NonOverlappingRectanglesEnergyPropagator::FailWhenEnergyIsTooLarge(
    int box, absl::Span<const int> local_boxes) {
  // Note that we only consider the smallest dimension of each boxes here.
  SortBoxesIntoNeighbors(box, local_boxes);

  IntegerValue area_min_x = x_.StartMin(box);
  IntegerValue area_max_x = x_.EndMax(box);
  IntegerValue area_min_y = y_.StartMin(box);
  IntegerValue area_max_y = y_.EndMax(box);

  IntegerValue sum_of_areas = cached_areas_[box];

  IntegerValue total_sum_of_areas = sum_of_areas;
  for (const Neighbor n : neighbors_) {
    total_sum_of_areas += cached_areas_[n.box];
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
    const int other_box = neighbors_[i].box;
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
        add_box_energy_in_rectangle_reason(neighbors_[j].box);
      }
      x_.ImportOtherReasons(y_);
      return x_.ReportConflict();
    }
  }
  return true;
}

// Note that x_ and y_ must be initialized with enough intervals when passed
// to the disjunctive propagators.
NonOverlappingRectanglesDisjunctivePropagator::
    NonOverlappingRectanglesDisjunctivePropagator(
        const std::vector<IntervalVariable>& x,
        const std::vector<IntervalVariable>& y, bool strict, Model* model)
    : global_x_(x, model),
      global_y_(y, model),
      x_(x, model),
      y_(y, model),
      strict_(strict),
      watcher_(model->GetOrCreate<GenericLiteralWatcher>()),
      overload_checker_(true, &x_),
      forward_detectable_precedences_(true, &x_),
      backward_detectable_precedences_(false, &x_),
      forward_not_last_(true, &x_),
      backward_not_last_(false, &x_),
      forward_edge_finding_(true, &x_),
      backward_edge_finding_(false, &x_) {}

NonOverlappingRectanglesDisjunctivePropagator::
    ~NonOverlappingRectanglesDisjunctivePropagator() {}

void NonOverlappingRectanglesDisjunctivePropagator::RegisterWith(
    GenericLiteralWatcher* watcher, int fast_priority, int slow_priority) {
  fast_id_ = watcher->Register(this);
  watcher->SetPropagatorPriority(fast_id_, fast_priority);
  global_x_.WatchAllTasks(fast_id_, watcher);
  global_y_.WatchAllTasks(fast_id_, watcher);

  const int slow_id = watcher->Register(this);
  watcher->SetPropagatorPriority(slow_id, slow_priority);
  global_x_.WatchAllTasks(slow_id, watcher);
  global_y_.WatchAllTasks(slow_id, watcher);
}

bool NonOverlappingRectanglesDisjunctivePropagator::
    FindBoxesThatMustOverlapAHorizontalLineAndPropagate(
        const SchedulingConstraintHelper& x,
        const SchedulingConstraintHelper& y,
        std::function<bool()> inner_propagate) {
  // Compute relevant events (line in the y dimension).
  active_boxes_.clear();
  events_time_.clear();
  for (int box = 0; box < x.NumTasks(); ++box) {
    if (!strict_ && (x.DurationMin(box) == 0 || y.DurationMin(box) == 0)) {
      continue;
    }

    const IntegerValue start_max = y.StartMax(box);
    const IntegerValue end_min = y.EndMin(box);
    if (start_max < end_min) {
      events_time_.push_back(start_max);
      active_boxes_.push_back(box);
    }
  }

  // Less than 2 boxes, no propagation.
  if (active_boxes_.size() < 2) return true;

  // Add boxes to the event lists they always overlap with.
  gtl::STLSortAndRemoveDuplicates(&events_time_);
  events_overlapping_boxes_.assign(events_time_.size(), {});
  for (const int box : active_boxes_) {
    const IntegerValue start_max = y.StartMax(box);
    const IntegerValue end_min = y.EndMin(box);

    for (int i = 0; i < events_time_.size(); ++i) {
      const IntegerValue t = events_time_[i];
      if (t < start_max) continue;
      if (t >= end_min) break;
      events_overlapping_boxes_[i].push_back(box);
    }
  }

  // Scan events chronologically to remove events where there is only one
  // mandatory box, or dominated events lists.
  {
    int new_size = 0;
    for (std::vector<int>& overlapping_boxes : events_overlapping_boxes_) {
      if (overlapping_boxes.size() < 2) {
        continue;  // Remove current event.
      }
      if (new_size > 0) {
        const std::vector<int>& previous_overlapping_boxes =
            events_overlapping_boxes_[new_size - 1];

        // If the previous set of boxes is included in the current one, replace
        // the old one by the new one.
        //
        // Note that because the events correspond to new boxes, there is no
        // need to check for the other side (current set included in previous
        // set).
        if (std::includes(overlapping_boxes.begin(), overlapping_boxes.end(),
                          previous_overlapping_boxes.begin(),
                          previous_overlapping_boxes.end())) {
          --new_size;
        }
      }

      std::swap(events_overlapping_boxes_[new_size], overlapping_boxes);
      ++new_size;
    }
    events_overlapping_boxes_.resize(new_size);
  }

  // Split lists of boxes into disjoint set of boxes (w.r.t. overlap).
  boxes_to_propagate_.clear();
  reduced_overlapping_boxes_.clear();
  for (std::vector<int>& overlapping_boxes : events_overlapping_boxes_) {
    disjoint_boxes_ = SplitDisjointBoxes(x, absl::MakeSpan(overlapping_boxes));
    for (absl::Span<int> sub_boxes : disjoint_boxes_) {
      // Boxes are sorted in a stable manner in the Split method.
      // Note that we do not use reduced_overlapping_boxes_ directly so that
      // the order of iteration is deterministic.
      const auto& insertion = reduced_overlapping_boxes_.insert(sub_boxes);
      if (insertion.second) boxes_to_propagate_.push_back(sub_boxes);
    }
  }

  // And finally propagate.
  // TODO(user): Sorting of boxes seems influential on the performance. Test.
  for (const absl::Span<const int> boxes : boxes_to_propagate_) {
    reduced_x_.clear();
    reduced_y_.clear();
    for (const int box : boxes) {
      reduced_x_.push_back(x.Intervals()[box]);
      reduced_y_.push_back(y.Intervals()[box]);
    }
    x_.Init(reduced_x_);
    y_.Init(reduced_y_);

    // Collect the common overlapping coordinates of all boxes.
    IntegerValue lb(kint64min);
    IntegerValue ub(kint64max);
    for (int i = 0; i < reduced_x_.size(); ++i) {
      lb = std::max(lb, y_.StartMax(i));
      ub = std::min(ub, y_.EndMin(i) - 1);
    }
    CHECK_LE(lb, ub);

    // TODO(user): We should scan the integer trail to find the oldest
    // non-empty common interval. Then we can pick the canonical value within
    // it.

    // We want for different propagation to reuse as much as possible the same
    // line. The idea behind this is to compute the 'canonical' line to use
    // when explaining that boxes overlap on the 'y_dim' dimension. We compute
    // the multiple of the biggest power of two that is common to all boxes.
    const IntegerValue line_to_use_for_reason = FindCanonicalValue(lb, ub);

    // Setup x_dim for propagation.
    x_.SetOtherHelper(&y_, line_to_use_for_reason);

    RETURN_IF_FALSE(inner_propagate());
  }

  return true;
}

bool NonOverlappingRectanglesDisjunctivePropagator::Propagate() {
  std::function<bool()> inner_propagate;
  if (watcher_->GetCurrentId() == fast_id_) {
    inner_propagate = [this]() {
      if (x_.NumTasks() == 2) {
        // In that case, we can use simpler algorithms.
        // Note that this case happens frequently (~30% of all calls to this
        // method according to our tests).
        RETURN_IF_FALSE(PropagateTwoBoxes());
      } else {
        RETURN_IF_FALSE(overload_checker_.Propagate());
        RETURN_IF_FALSE(forward_detectable_precedences_.Propagate());
        RETURN_IF_FALSE(backward_detectable_precedences_.Propagate());
      }
      return true;
    };
  } else {
    inner_propagate = [this]() {
      if (x_.NumTasks() <= 2) return true;
      RETURN_IF_FALSE(forward_not_last_.Propagate());
      RETURN_IF_FALSE(backward_not_last_.Propagate());
      RETURN_IF_FALSE(backward_edge_finding_.Propagate());
      RETURN_IF_FALSE(forward_edge_finding_.Propagate());
      return true;
    };
  }

  RETURN_IF_FALSE(FindBoxesThatMustOverlapAHorizontalLineAndPropagate(
      global_x_, global_y_, inner_propagate));

  // We can actually swap dimensions to propagate vertically.
  RETURN_IF_FALSE(FindBoxesThatMustOverlapAHorizontalLineAndPropagate(
      global_y_, global_x_, inner_propagate));

  return true;
}

// Specialized propagation on only two boxes that must intersect with the
// given y_line_for_reason.
bool NonOverlappingRectanglesDisjunctivePropagator::PropagateTwoBoxes() {
  // For each direction and each order, we test if the boxes can be disjoint.
  const int state =
      (x_.EndMin(0) <= x_.StartMax(1)) + 2 * (x_.EndMin(1) <= x_.StartMax(0));

  const auto left_box_before_right_box = [this](int left, int right) {
    // left box pushes right box.
    const IntegerValue left_end_min = x_.EndMin(left);
    if (left_end_min > x_.StartMin(right)) {
      x_.ClearReason();
      x_.AddReasonForBeingBefore(left, right);
      x_.AddEndMinReason(left, left_end_min);
      RETURN_IF_FALSE(x_.IncreaseStartMin(right, left_end_min));
    }

    // right box pushes left box.
    const IntegerValue right_start_max = x_.StartMax(right);
    if (right_start_max < x_.EndMax(left)) {
      x_.ClearReason();
      x_.AddReasonForBeingBefore(left, right);
      x_.AddStartMaxReason(right, right_start_max);
      RETURN_IF_FALSE(x_.DecreaseEndMax(left, right_start_max));
    }

    return true;
  };

  switch (state) {
    case 0: {  // Conflict.
      x_.ClearReason();
      x_.AddReasonForBeingBefore(0, 1);
      x_.AddReasonForBeingBefore(1, 0);
      return x_.ReportConflict();
    }
    case 1: {  // b1 is left of b2.
      return left_box_before_right_box(0, 1);
    }
    case 2: {  // b2 is left of b1.
      return left_box_before_right_box(1, 0);
    }
    default: {  // Nothing to deduce.
      return true;
    }
  }
}

#undef RETURN_IF_FALSE
}  // namespace sat
}  // namespace operations_research
