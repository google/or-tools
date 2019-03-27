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
    absl::Span<int> boxes, SchedulingConstraintHelper* x_dim) {
  std::vector<absl::Span<int>> result;
  std::sort(boxes.begin(), boxes.end(), [x_dim](int a, int b) {
    return x_dim->StartMin(a) < x_dim->StartMin(b) ||
           (x_dim->StartMin(a) == x_dim->StartMin(b) && a < b);
  });
  int current_start = 0;
  std::size_t current_length = 1;
  IntegerValue current_max_end = x_dim->EndMax(boxes[0]);

  for (int b = 1; b < boxes.size(); ++b) {
    const int box = boxes[b];
    if (x_dim->StartMin(box) < current_max_end) {
      // Merge.
      current_length++;
      current_max_end = std::max(current_max_end, x_dim->EndMax(box));
    } else {
      if (current_length > 1) {  // Ignore lists of size 1.
        result.push_back({&boxes[current_start], current_length});
      }
      current_start = b;
      current_length = 1;
      current_max_end = x_dim->EndMax(box);
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

  if (active_boxes_.empty()) return true;

  // const std::vector<absl::Span<int>> x_split =
  //     SplitDisjointBoxes({&active_boxes_[0], active_boxes_.size()}, &x_);
  const std::vector<absl::Span<int>> x_split =
      SplitDisjointBoxes(absl::MakeSpan(active_boxes_), &x_);
  for (absl::Span<int> x_boxes : x_split) {
    if (x_boxes.size() <= 1) continue;
    const std::vector<absl::Span<int>> y_split =
        SplitDisjointBoxes(x_boxes, &y_);
    for (absl::Span<int> y_boxes : y_split) {
      if (y_boxes.size() <= 1) continue;
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
  x_.WatchAllTasks(id, watcher);
  y_.WatchAllTasks(id, watcher);
  return id;
}

void NonOverlappingRectanglesEnergyPropagator::SortBoxesIntoNeighbors(
    int box, absl::Span<int> local_boxes) {
  auto max_span = [](IntegerValue min_a, IntegerValue max_a, IntegerValue min_b,
                     IntegerValue max_b) {
    return std::max(max_a, max_b) - std::min(min_a, min_b) + 1;
  };

  cached_distance_to_bounding_box_.assign(x_.NumTasks(), IntegerValue(0));
  neighbors_.clear();
  const IntegerValue box_x_min = x_.StartMin(box);
  const IntegerValue box_x_max = x_.EndMax(box);
  const IntegerValue box_y_min = y_.StartMin(box);
  const IntegerValue box_y_max = y_.EndMax(box);

  for (const int other_box : local_boxes) {
    if (other_box == box) continue;
    if (cached_areas_[other_box] == 0) continue;

    const IntegerValue other_x_min = x_.StartMin(other_box);
    const IntegerValue other_x_max = x_.EndMax(other_box);
    const IntegerValue other_y_min = y_.StartMin(other_box);
    const IntegerValue other_y_max = y_.EndMax(other_box);

    neighbors_.push_back(other_box);
    cached_distance_to_bounding_box_[other_box] =
        max_span(box_x_min, box_x_max, other_x_min, other_x_max) *
        max_span(box_y_min, box_y_max, other_y_min, other_y_max);
  }
  std::sort(neighbors_.begin(), neighbors_.begin(), [this](int i, int j) {
    return cached_distance_to_bounding_box_[i] <
           cached_distance_to_bounding_box_[j];
  });
}

bool NonOverlappingRectanglesEnergyPropagator::FailWhenEnergyIsTooLarge(
    int box, absl::Span<int> local_boxes) {
  // Note that we only consider the smallest dimension of each boxes here.
  SortBoxesIntoNeighbors(box, local_boxes);

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

NonOverlappingRectanglesDisjunctivePropagator::
    NonOverlappingRectanglesDisjunctivePropagator(
        const std::vector<IntervalVariable>& x,
        const std::vector<IntervalVariable>& y, bool strict,
        bool slow_propagators, Model* model)
    : x_intervals_(x),
      y_intervals_(y),
      x_(x, model),
      y_(y, model),
      strict_(strict),
      slow_propagators_(slow_propagators),
      overload_checker_(true, &x_),
      forward_detectable_precedences_(true, &x_),
      backward_detectable_precedences_(false, &x_),
      forward_not_last_(true, &x_),
      backward_not_last_(false, &x_),
      forward_edge_finding_(true, &x_),
      backward_edge_finding_(false, &x_) {
  CHECK_GT(x_.NumTasks(), 0);
}

NonOverlappingRectanglesDisjunctivePropagator::
    ~NonOverlappingRectanglesDisjunctivePropagator() {}

int NonOverlappingRectanglesDisjunctivePropagator::RegisterWith(
    GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  x_.WatchAllTasks(id, watcher);
  y_.WatchAllTasks(id, watcher);
  return id;
}

bool NonOverlappingRectanglesDisjunctivePropagator::
    FindBoxesThatMustOverlapAHorizontalLineAndPropagate(
        const std::vector<IntervalVariable>& x_intervals,
        const std::vector<IntervalVariable>& y_intervals,
        std::function<bool()> inner_propagate) {
  // Restore the two dimensions in a sane state.
  x_.SetTimeDirection(true);
  x_.ClearOtherHelper();
  x_.Init(x_intervals);

  y_.SetTimeDirection(true);
  y_.ClearOtherHelper();
  y_.Init(y_intervals);

  // Compute relevant events (line in the y dimension).
  absl::flat_hash_map<IntegerValue, std::vector<int>>
      event_to_overlapping_boxes;
  std::set<IntegerValue> events;

  std::vector<int> active_boxes;

  for (int box = 0; box < x_intervals.size(); ++box) {
    if ((x_.DurationMin(box) == 0 || y_.DurationMin(box) == 0) && !strict_) {
      continue;
    }

    const IntegerValue start_max = y_.StartMax(box);
    const IntegerValue end_min = y_.EndMin(box);
    if (start_max < end_min) {
      events.insert(start_max);
      active_boxes.push_back(box);
    }
  }

  // Less than 2 boxes, no propagation.
  if (active_boxes.size() < 2) return true;

  // Add boxes to the event lists they always overlap with.
  for (const int box : active_boxes) {
    const IntegerValue start_max = y_.StartMax(box);
    const IntegerValue end_min = y_.EndMin(box);

    for (const IntegerValue t : events) {
      if (t < start_max) continue;
      if (t >= end_min) break;
      event_to_overlapping_boxes[t].push_back(box);
    }
  }

  // Scan events chronologically to remove events where there is only one
  // mandatory box, or dominated events lists.
  std::vector<IntegerValue> events_to_remove;
  std::vector<int> previous_overlapping_boxes;
  IntegerValue previous_event(-1);
  for (const IntegerValue current_event : events) {
    const std::vector<int>& current_overlapping_boxes =
        event_to_overlapping_boxes[current_event];
    if (current_overlapping_boxes.size() < 2) {
      events_to_remove.push_back(current_event);
      continue;
    }
    if (!previous_overlapping_boxes.empty()) {
      // In case we just add one box to the previous event.
      if (std::includes(current_overlapping_boxes.begin(),
                        current_overlapping_boxes.end(),
                        previous_overlapping_boxes.begin(),
                        previous_overlapping_boxes.end())) {
        events_to_remove.push_back(previous_event);
        continue;
      }
    }

    previous_event = current_event;
    previous_overlapping_boxes = current_overlapping_boxes;
  }

  for (const IntegerValue event : events_to_remove) {
    events.erase(event);
  }

  // Split lists of boxes into disjoint set of boxes (w.r.t. overlap).
  absl::flat_hash_set<absl::Span<int>> reduced_overlapping_boxes;
  std::vector<absl::Span<int>> boxes_to_propagate;
  std::vector<absl::Span<int>> disjoint_boxes;
  for (const IntegerValue event : events) {
    disjoint_boxes = SplitDisjointBoxes(
        absl::MakeSpan(event_to_overlapping_boxes[event]), &x_);
    for (absl::Span<int> sub_boxes : disjoint_boxes) {
      if (sub_boxes.size() > 1) {
        // Boxes are sorted in a stable manner in the Split method.
        const auto& insertion = reduced_overlapping_boxes.insert(sub_boxes);
        if (insertion.second) boxes_to_propagate.push_back(sub_boxes);
      }
    }
  }

  // And finally propagate.
  // TODO(user): Sorting of boxes seems influential on the performance. Test.
  for (const absl::Span<int> boxes : boxes_to_propagate) {
    std::vector<IntervalVariable> reduced_x;
    std::vector<IntervalVariable> reduced_y;
    for (const int box : boxes) {
      reduced_x.push_back(x_intervals[box]);
      reduced_y.push_back(y_intervals[box]);
    }

    x_.Init(reduced_x);
    y_.Init(reduced_y);

    // Collect the common overlapping coordinates of all boxes.
    IntegerValue lb(kint64min);
    IntegerValue ub(kint64max);
    for (int i = 0; i < reduced_x.size(); ++i) {
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
  const auto slow_propagate = [this]() {
    if (x_.NumTasks() <= 2) return true;
    RETURN_IF_FALSE(forward_not_last_.Propagate());
    RETURN_IF_FALSE(backward_not_last_.Propagate());
    RETURN_IF_FALSE(backward_edge_finding_.Propagate());
    RETURN_IF_FALSE(forward_edge_finding_.Propagate());
    return true;
  };

  const auto fast_propagate = [this]() {
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

  if (slow_propagators_) {
    RETURN_IF_FALSE(FindBoxesThatMustOverlapAHorizontalLineAndPropagate(
        x_intervals_, y_intervals_, slow_propagate));

    // We can actually swap dimensions to propagate vertically.
    RETURN_IF_FALSE(FindBoxesThatMustOverlapAHorizontalLineAndPropagate(
        y_intervals_, x_intervals_, slow_propagate));
  } else {
    RETURN_IF_FALSE(FindBoxesThatMustOverlapAHorizontalLineAndPropagate(
        x_intervals_, y_intervals_, fast_propagate));

    // We can actually swap dimensions to propagate vertically.
    RETURN_IF_FALSE(FindBoxesThatMustOverlapAHorizontalLineAndPropagate(
        y_intervals_, x_intervals_, fast_propagate));
  }
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
