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

#define RETURN_IF_FALSE(f) \
  if (!(f)) return false;

// ------ Base class -----

NonOverlappingRectanglesBasePropagator::NonOverlappingRectanglesBasePropagator(
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

NonOverlappingRectanglesBasePropagator::
    ~NonOverlappingRectanglesBasePropagator() {}

void NonOverlappingRectanglesBasePropagator::FillCachedAreas() {
  cached_areas_.resize(num_boxes_);
  for (int box = 0; box < num_boxes_; ++box) {
    // We assume that the min-size of a box never changes.
    cached_areas_[box] = x_.DurationMin(box) * y_.DurationMin(box);
  }
}

// We maximize the number of trailing bits set to 0 within a range.
IntegerValue NonOverlappingRectanglesBasePropagator::FindCanonicalValue(
    IntegerValue lb, IntegerValue ub) {
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

std::vector<std::vector<int>>
NonOverlappingRectanglesBasePropagator::SplitDisjointBoxes(
    std::vector<int> boxes, SchedulingConstraintHelper* x_dim) {
  std::vector<std::vector<int>> result(1);
  std::sort(boxes.begin(), boxes.end(), [x_dim](int a, int b) {
    return x_dim->StartMin(a) < x_dim->StartMin(b);
  });
  result.back().push_back(boxes[0]);
  IntegerValue current_max_end = x_dim->EndMax(boxes[0]);

  for (int b = 1; b < boxes.size(); ++b) {
    const int box = boxes[b];
    if (x_dim->StartMin(box) < current_max_end) {
      // Merge.
      result.back().push_back(box);
      current_max_end = std::max(current_max_end, x_dim->EndMax(box));
    } else {
      if (result.back().size() == 1) {
        // Overwrite
        result.back().clear();
      } else {
        result.push_back(std::vector<int>());
      }
      result.back().push_back(box);
      current_max_end = x_dim->EndMax(box);
    }
  }

  return result;
}

bool NonOverlappingRectanglesBasePropagator::
    FindBoxesThatMustOverlapAHorizontalLineAndPropagate(
        SchedulingConstraintHelper* x_dim, SchedulingConstraintHelper* y_dim,
        std::function<bool(const std::vector<int>& boxes)> inner_propagate) {
  // Restore the two dimensions in a sane state.
  x_dim->SetTimeDirection(true);
  x_dim->ClearOtherHelper();
  x_dim->SetAllIntervalsVisible();
  y_dim->SetTimeDirection(true);
  y_dim->ClearOtherHelper();
  y_dim->SetAllIntervalsVisible();

  std::map<IntegerValue, std::vector<int>> event_to_overlapping_boxes;
  std::set<IntegerValue> events;

  std::vector<int> active_boxes;

  for (int box = 0; box < num_boxes_; ++box) {
    if (cached_areas_[box] == 0 && !strict_) continue;
    const IntegerValue start_max = y_dim->StartMax(box);
    const IntegerValue end_min = y_dim->EndMin(box);
    if (start_max < end_min) {
      events.insert(start_max);
      active_boxes.push_back(box);
    }
  }

  if (active_boxes.size() < 2) return true;

  for (const int box : active_boxes) {
    const IntegerValue start_max = y_dim->StartMax(box);
    const IntegerValue end_min = y_dim->EndMin(box);

    for (const IntegerValue t : events) {
      if (t < start_max) continue;
      if (t >= end_min) break;
      event_to_overlapping_boxes[t].push_back(box);
    }
  }

  std::vector<IntegerValue> events_to_remove;
  std::vector<int> previous_overlapping_boxes;
  IntegerValue previous_event(-1);
  for (const auto& it : event_to_overlapping_boxes) {
    const IntegerValue current_event = it.first;
    const std::vector<int>& current_overlapping_boxes = it.second;
    if (current_overlapping_boxes.size() < 2) {
      events_to_remove.push_back(current_event);
      continue;
    }
    if (!previous_overlapping_boxes.empty()) {
      if (std::includes(previous_overlapping_boxes.begin(),
                        previous_overlapping_boxes.end(),
                        current_overlapping_boxes.begin(),
                        current_overlapping_boxes.end())) {
        events_to_remove.push_back(current_event);
      }
    }
    previous_event = current_event;
    previous_overlapping_boxes = current_overlapping_boxes;
  }

  for (const IntegerValue event : events_to_remove) {
    event_to_overlapping_boxes.erase(event);
  }

  std::set<std::vector<int>> reduced_overlapping_boxes;
  for (const auto& it : event_to_overlapping_boxes) {
    std::vector<std::vector<int>> disjoint_boxes =
        SplitDisjointBoxes(it.second, x_dim);
    for (std::vector<int>& sub_boxes : disjoint_boxes) {
      if (sub_boxes.size() > 1) {
        std::sort(sub_boxes.begin(), sub_boxes.end());
        reduced_overlapping_boxes.insert(sub_boxes);
      }
    }
  }

  for (const std::vector<int>& boxes : reduced_overlapping_boxes) {
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
    // line. The idea behind this is to compute the 'canonical' line to use
    // when explaining that boxes overlap on the 'y_dim' dimension. We compute
    // the multiple of the biggest power of two that is common to all boxes.
    const IntegerValue line_to_use_for_reason = FindCanonicalValue(lb, ub);

    // Setup x_dim for propagation.
    x_dim->SetOtherHelper(y_dim, line_to_use_for_reason);
    x_dim->SetVisibleIntervals(boxes);

    RETURN_IF_FALSE(inner_propagate(boxes));
  }
  return true;
}

NonOverlappingRectanglesEnergyPropagator::
    NonOverlappingRectanglesEnergyPropagator(
        const std::vector<IntervalVariable>& x,
        const std::vector<IntervalVariable>& y, bool strict, Model* model,
        IntegerTrail* integer_trail)
    : NonOverlappingRectanglesBasePropagator(x, y, strict, model,
                                             integer_trail) {}

NonOverlappingRectanglesEnergyPropagator::
    ~NonOverlappingRectanglesEnergyPropagator() {}

bool NonOverlappingRectanglesEnergyPropagator::Propagate() {
  FillCachedAreas();

  std::vector<int> all_boxes;
  for (int box = 0; box < num_boxes_; ++box) {
    if (cached_areas_[box] == 0) continue;
    all_boxes.push_back(box);
  }

  if (all_boxes.empty()) return true;

  const std::vector<std::vector<int>> x_split =
      SplitDisjointBoxes(all_boxes, &x_);
  for (const std::vector<int>& x_boxes : x_split) {
    if (x_boxes.size() <= 1) continue;
    const std::vector<std::vector<int>> y_split =
        SplitDisjointBoxes(x_boxes, &y_);
    for (const std::vector<int>& y_boxes : y_split) {
      if (y_boxes.size() <= 1) continue;
      for (const int box : y_boxes) {
        RETURN_IF_FALSE(FailWhenEnergyIsTooLarge(box, y_boxes));
      }
    }
  }

  return true;
}

void NonOverlappingRectanglesEnergyPropagator::RegisterWith(
    GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  x_.WatchAllTasks(id, watcher);
  y_.WatchAllTasks(id, watcher);
  watcher->SetPropagatorPriority(id, 2);
}

void NonOverlappingRectanglesEnergyPropagator::SortNeighbors(
    int box, const std::vector<int>& local_boxes) {
  auto max_span = [](IntegerValue min_a, IntegerValue max_a, IntegerValue min_b,
                     IntegerValue max_b) {
    return std::max(max_a, max_b) - std::min(min_a, min_b) + 1;
  };

  cached_distance_to_bounding_box_.assign(num_boxes_, IntegerValue(0));
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
    int box, const std::vector<int>& local_boxes) {
  // Note that we only consider the smallest dimension of each boxes here.

  SortNeighbors(box, local_boxes);
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

NonOverlappingRectanglesFastPropagator::NonOverlappingRectanglesFastPropagator(
    const std::vector<IntervalVariable>& x,
    const std::vector<IntervalVariable>& y, bool strict, Model* model,
    IntegerTrail* integer_trail)
    : NonOverlappingRectanglesBasePropagator(x, y, strict, model,
                                             integer_trail),
      x_overload_checker_(true, &x_),
      y_overload_checker_(true, &y_),
      forward_x_detectable_precedences_(true, &x_),
      backward_x_detectable_precedences_(false, &x_),
      forward_y_detectable_precedences_(true, &y_),
      backward_y_detectable_precedences_(false, &y_) {}

NonOverlappingRectanglesFastPropagator::
    ~NonOverlappingRectanglesFastPropagator() {}

bool NonOverlappingRectanglesFastPropagator::Propagate() {
  FillCachedAreas();

  // Reach fix-point on fast propagators.
  RETURN_IF_FALSE(FindBoxesThatMustOverlapAHorizontalLineAndPropagate(
      &x_, &y_, [this](const std::vector<int>& boxes) {
        if (boxes.size() == 2) {
          // In that case, we can use simpler algorithms.
          // Note that this case happens frequently (~30% of all calls to this
          // method according to our tests).
          RETURN_IF_FALSE(PropagateTwoBoxes(boxes[0], boxes[1], &x_));
        } else {
          RETURN_IF_FALSE(x_overload_checker_.Propagate());
          RETURN_IF_FALSE(forward_x_detectable_precedences_.Propagate());
          RETURN_IF_FALSE(backward_x_detectable_precedences_.Propagate());
        }
        return true;
      }));

  // We can actually swap dimensions to propagate vertically.
  RETURN_IF_FALSE(FindBoxesThatMustOverlapAHorizontalLineAndPropagate(
      &y_, &x_, [this](const std::vector<int>& boxes) {
        if (boxes.size() == 2) {
          // In that case, we can use simpler algorithms.
          // Note that this case happens frequently (~30% of all calls to this
          // method according to our tests).
          RETURN_IF_FALSE(PropagateTwoBoxes(boxes[0], boxes[1], &y_));
        } else {
          RETURN_IF_FALSE(y_overload_checker_.Propagate());
          RETURN_IF_FALSE(forward_y_detectable_precedences_.Propagate());
          RETURN_IF_FALSE(backward_y_detectable_precedences_.Propagate());
        }
        return true;
      }));
  return true;
}

void NonOverlappingRectanglesFastPropagator::RegisterWith(
    GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  x_.WatchAllTasks(id, watcher);
  y_.WatchAllTasks(id, watcher);
  watcher->SetPropagatorPriority(id, 3);
}

// Specialized propagation on only two boxes that must intersect with the
// given y_line_for_reason.
bool NonOverlappingRectanglesFastPropagator::PropagateTwoBoxes(
    int b1, int b2, SchedulingConstraintHelper* x_dim) {
  // For each direction and each order, we test if the boxes can be disjoint.
  const int state = (x_dim->EndMin(b1) <= x_dim->StartMax(b2)) +
                    2 * (x_dim->EndMin(b2) <= x_dim->StartMax(b1));

  const auto left_box_before_right_box = [](int left, int right,
                                            SchedulingConstraintHelper* x_dim) {
    // left box pushes right box.
    const IntegerValue left_end_min = x_dim->EndMin(left);
    if (left_end_min > x_dim->StartMin(right)) {
      x_dim->ClearReason();
      x_dim->AddReasonForBeingBefore(left, right);
      x_dim->AddEndMinReason(left, left_end_min);
      RETURN_IF_FALSE(x_dim->IncreaseStartMin(right, left_end_min));
    }

    // right box pushes left box.
    const IntegerValue right_start_max = x_dim->StartMax(right);
    if (right_start_max < x_dim->EndMax(left)) {
      x_dim->ClearReason();
      x_dim->AddReasonForBeingBefore(left, right);
      x_dim->AddStartMaxReason(right, right_start_max);
      RETURN_IF_FALSE(x_dim->DecreaseEndMax(left, right_start_max));
    }

    return true;
  };

  switch (state) {
    case 0: {  // Conflict.
      x_dim->ClearReason();
      x_dim->AddReasonForBeingBefore(b1, b2);
      x_dim->AddReasonForBeingBefore(b2, b1);
      return x_dim->ReportConflict();
    }
    case 1: {  // b1 is left of b2.
      return left_box_before_right_box(b1, b2, x_dim);
    }
    case 2: {  // b2 is left of b1.
      return left_box_before_right_box(b2, b1, x_dim);
    }
    default: {  // Nothing to deduce.
      return true;
    }
  }
}

NonOverlappingRectanglesSlowPropagator::NonOverlappingRectanglesSlowPropagator(
    const std::vector<IntervalVariable>& x,
    const std::vector<IntervalVariable>& y, bool strict, Model* model,
    IntegerTrail* integer_trail)
    : NonOverlappingRectanglesBasePropagator(x, y, strict, model,
                                             integer_trail),
      forward_x_not_last_(true, &x_),
      backward_x_not_last_(false, &x_),
      forward_y_not_last_(true, &y_),
      backward_y_not_last_(false, &y_),
      forward_x_edge_finding_(true, &x_),
      backward_x_edge_finding_(false, &x_),
      forward_y_edge_finding_(true, &y_),
      backward_y_edge_finding_(false, &y_) {}

NonOverlappingRectanglesSlowPropagator::
    ~NonOverlappingRectanglesSlowPropagator() {}

bool NonOverlappingRectanglesSlowPropagator::Propagate() {
  FillCachedAreas();

  RETURN_IF_FALSE(FindBoxesThatMustOverlapAHorizontalLineAndPropagate(
      &x_, &y_, [this](const std::vector<int>& boxes) {
        if (boxes.size() <= 2) return true;
        RETURN_IF_FALSE(forward_x_not_last_.Propagate());
        RETURN_IF_FALSE(backward_x_not_last_.Propagate());
        RETURN_IF_FALSE(backward_x_edge_finding_.Propagate());
        RETURN_IF_FALSE(forward_x_edge_finding_.Propagate());
        return true;
      }));

  // We can actually swap dimensions to propagate vertically.
  RETURN_IF_FALSE(FindBoxesThatMustOverlapAHorizontalLineAndPropagate(
      &y_, &x_, [this](const std::vector<int>& boxes) {
        if (boxes.size() <= 2) return true;
        RETURN_IF_FALSE(forward_y_not_last_.Propagate());
        RETURN_IF_FALSE(backward_y_not_last_.Propagate());
        RETURN_IF_FALSE(backward_y_edge_finding_.Propagate());
        RETURN_IF_FALSE(forward_y_edge_finding_.Propagate());
        return true;
      }));

  return true;
}

void NonOverlappingRectanglesSlowPropagator::RegisterWith(
    GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  x_.WatchAllTasks(id, watcher);
  y_.WatchAllTasks(id, watcher);
  watcher->SetPropagatorPriority(id, 4);
}

#undef RETURN_IF_FALSE
}  // namespace sat
}  // namespace operations_research
