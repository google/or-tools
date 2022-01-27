// Copyright 2010-2021 Google LLC
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
#include <cstdint>
#include <limits>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/str_join.h"
#include "ortools/base/iterator_adaptors.h"
#include "ortools/base/map_util.h"
#include "ortools/base/stl_util.h"
#include "ortools/sat/cumulative.h"
#include "ortools/sat/diffn_util.h"
#include "ortools/sat/disjunctive.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/theta_tree.h"
#include "ortools/util/sort.h"

namespace operations_research {
namespace sat {

namespace {

// TODO(user): Use the faster variable only version if all expressions reduce
// to a single variable?
void AddIsEqualToMinOf(IntegerVariable min_var,
                       const std::vector<AffineExpression>& exprs,
                       Model* model) {
  std::vector<LinearExpression> converted;
  for (const AffineExpression& affine : exprs) {
    LinearExpression e;
    e.offset = affine.constant;
    if (affine.var != kNoIntegerVariable) {
      e.vars.push_back(affine.var);
      e.coeffs.push_back(affine.coeff);
    }
    converted.push_back(e);
  }
  LinearExpression target;
  target.vars.push_back(min_var);
  target.coeffs.push_back(IntegerValue(1));
  model->Add(IsEqualToMinOf(target, converted));
}

void AddIsEqualToMaxOf(IntegerVariable max_var,
                       const std::vector<AffineExpression>& exprs,
                       Model* model) {
  std::vector<LinearExpression> converted;
  for (const AffineExpression& affine : exprs) {
    LinearExpression e;
    e.offset = affine.constant;
    if (affine.var != kNoIntegerVariable) {
      e.vars.push_back(affine.var);
      e.coeffs.push_back(affine.coeff);
    }
    converted.push_back(NegationOf(e));
  }
  LinearExpression target;
  target.vars.push_back(NegationOf(max_var));
  target.coeffs.push_back(IntegerValue(1));
  model->Add(IsEqualToMinOf(target, converted));
}

}  // namespace

void AddCumulativeRelaxation(const std::vector<IntervalVariable>& x_intervals,
                             SchedulingConstraintHelper* x,
                             SchedulingConstraintHelper* y, Model* model) {
  int64_t min_starts = std::numeric_limits<int64_t>::max();
  int64_t max_ends = std::numeric_limits<int64_t>::min();
  std::vector<AffineExpression> sizes;
  for (int box = 0; box < y->NumTasks(); ++box) {
    min_starts = std::min(min_starts, y->StartMin(box).value());
    max_ends = std::max(max_ends, y->EndMax(box).value());
    sizes.push_back(y->Sizes()[box]);
  }

  const IntegerVariable min_start_var =
      model->Add(NewIntegerVariable(min_starts, max_ends));
  AddIsEqualToMinOf(min_start_var, y->Starts(), model);

  const IntegerVariable max_end_var =
      model->Add(NewIntegerVariable(min_starts, max_ends));
  AddIsEqualToMaxOf(max_end_var, y->Ends(), model);

  // (max_end - min_start) >= capacity.
  const AffineExpression capacity(
      model->Add(NewIntegerVariable(0, CapSub(max_ends, min_starts))));
  const std::vector<int64_t> coeffs = {-capacity.coeff.value(), -1, 1};
  model->Add(
      WeightedSumGreaterOrEqual({capacity.var, min_start_var, max_end_var},
                                coeffs, capacity.constant.value()));

  model->Add(Cumulative(x_intervals, sizes, capacity, x));
}

#define RETURN_IF_FALSE(f) \
  if (!(f)) return false;

NonOverlappingRectanglesEnergyPropagator::
    ~NonOverlappingRectanglesEnergyPropagator() {}

bool NonOverlappingRectanglesEnergyPropagator::Propagate() {
  const int num_boxes = x_.NumTasks();
  if (!x_.SynchronizeAndSetTimeDirection(true)) return false;
  if (!y_.SynchronizeAndSetTimeDirection(true)) return false;

  active_boxes_.clear();
  cached_energies_.resize(num_boxes);
  cached_rectangles_.resize(num_boxes);
  for (int box = 0; box < num_boxes; ++box) {
    cached_energies_[box] = x_.SizeMin(box) * y_.SizeMin(box);
    if (cached_energies_[box] == 0) continue;
    if (!x_.IsPresent(box) || !y_.IsPresent(box)) continue;

    // The code needs the size min to be larger or equal to the mandatory part
    // for it to works correctly. This is always enforced by the helper.
    DCHECK_GE(x_.SizeMin(box), x_.EndMin(box) - x_.StartMax(box));
    DCHECK_GE(y_.SizeMin(box), y_.EndMin(box) - y_.StartMax(box));

    Rectangle& rectangle = cached_rectangles_[box];
    rectangle.x_min = x_.ShiftedStartMin(box);
    rectangle.x_max = x_.ShiftedEndMax(box);
    rectangle.y_min = y_.ShiftedStartMin(box);
    rectangle.y_max = y_.ShiftedEndMax(box);

    active_boxes_.push_back(box);
  }

  absl::Span<int> initial_boxes = FilterBoxesThatAreTooLarge(
      cached_rectangles_, cached_energies_, absl::MakeSpan(active_boxes_));
  if (initial_boxes.size() <= 1) return true;

  Rectangle conflicting_rectangle;
  std::vector<absl::Span<int>> components =
      GetOverlappingRectangleComponents(cached_rectangles_, initial_boxes);
  for (absl::Span<int> boxes : components) {
    // Computes the size on x and y past which there is no point doing any
    // energetic reasonning. We do a few iterations since as we filter one size,
    // we can potentially filter more on the transposed dimension.
    threshold_x_ = kMaxIntegerValue;
    threshold_y_ = kMaxIntegerValue;
    for (int i = 0; i < 3; ++i) {
      if (!AnalyzeIntervals(/*transpose=*/i == 1, boxes, cached_rectangles_,
                            cached_energies_, &threshold_x_, &threshold_y_,
                            &conflicting_rectangle)) {
        return ReportEnergyConflict(conflicting_rectangle, boxes, &x_, &y_);
      }
      boxes = FilterBoxesAndRandomize(cached_rectangles_, boxes, threshold_x_,
                                      threshold_y_, *random_);
      if (boxes.size() <= 1) break;
    }
    if (boxes.size() <= 1) continue;

    // Now that we removed boxes with a large domain, resplit everything.
    const std::vector<absl::Span<int>> local_components =
        GetOverlappingRectangleComponents(cached_rectangles_, boxes);
    for (absl::Span<int> local_boxes : local_components) {
      IntegerValue total_sum_of_areas(0);
      for (const int box : local_boxes) {
        total_sum_of_areas += cached_energies_[box];
      }
      for (const int box : local_boxes) {
        RETURN_IF_FALSE(
            FailWhenEnergyIsTooLarge(box, local_boxes, total_sum_of_areas));
      }
    }
  }

  return true;
}

int NonOverlappingRectanglesEnergyPropagator::RegisterWith(
    GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  x_.WatchAllTasks(id, watcher, /*watch_start_max=*/true,
                   /*watch_end_max=*/true);
  y_.WatchAllTasks(id, watcher, /*watch_start_max=*/true,
                   /*watch_end_max=*/true);
  return id;
}

void NonOverlappingRectanglesEnergyPropagator::SortBoxesIntoNeighbors(
    int box, absl::Span<const int> local_boxes,
    IntegerValue total_sum_of_areas) {
  const Rectangle& box_dim = cached_rectangles_[box];

  neighbors_.clear();
  for (const int other_box : local_boxes) {
    if (other_box == box) continue;
    const Rectangle& other_dim = cached_rectangles_[other_box];
    const IntegerValue span_x = std::max(box_dim.x_max, other_dim.x_max) -
                                std::min(box_dim.x_min, other_dim.x_min);
    if (span_x > threshold_x_) continue;
    const IntegerValue span_y = std::max(box_dim.y_max, other_dim.y_max) -
                                std::min(box_dim.y_min, other_dim.y_min);
    if (span_y > threshold_y_) continue;
    const IntegerValue bounding_area = span_x * span_y;
    if (bounding_area < total_sum_of_areas) {
      neighbors_.push_back({other_box, bounding_area});
    }
  }
  std::sort(neighbors_.begin(), neighbors_.end());
}

bool NonOverlappingRectanglesEnergyPropagator::FailWhenEnergyIsTooLarge(
    int box, absl::Span<const int> local_boxes,
    IntegerValue total_sum_of_areas) {
  SortBoxesIntoNeighbors(box, local_boxes, total_sum_of_areas);

  Rectangle area = cached_rectangles_[box];
  IntegerValue sum_of_areas = cached_energies_[box];

  const auto add_box_energy_in_rectangle_reason = [&](int b) {
    x_.AddEnergyMinInIntervalReason(b, area.x_min, area.x_max);
    y_.AddEnergyMinInIntervalReason(b, area.y_min, area.y_max);
    x_.AddPresenceReason(b);
    y_.AddPresenceReason(b);
  };

  for (int i = 0; i < neighbors_.size(); ++i) {
    const int other_box = neighbors_[i].box;
    CHECK_GT(cached_energies_[other_box], 0);

    // Update Bounding box.
    area.TakeUnionWith(cached_rectangles_[other_box]);
    if (area.x_max - area.x_min > threshold_x_) break;
    if (area.y_max - area.y_min > threshold_y_) break;

    // Update sum of areas.
    sum_of_areas += cached_energies_[other_box];
    const IntegerValue bounding_area =
        (area.x_max - area.x_min) * (area.y_max - area.y_min);
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

  int64_t mask = 0;
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

void SplitDisjointBoxes(const SchedulingConstraintHelper& x,
                        absl::Span<int> boxes,
                        std::vector<absl::Span<int>>* result) {
  result->clear();
  std::sort(boxes.begin(), boxes.end(), [&x](int a, int b) {
    return x.ShiftedStartMin(a) < x.ShiftedStartMin(b);
  });
  int current_start = 0;
  std::size_t current_length = 1;
  IntegerValue current_max_end = x.EndMax(boxes[0]);

  for (int b = 1; b < boxes.size(); ++b) {
    const int box = boxes[b];
    if (x.ShiftedStartMin(box) < current_max_end) {
      // Merge.
      current_length++;
      current_max_end = std::max(current_max_end, x.EndMax(box));
    } else {
      if (current_length > 1) {  // Ignore lists of size 1.
        result->emplace_back(&boxes[current_start], current_length);
      }
      current_start = b;
      current_length = 1;
      current_max_end = x.EndMax(box);
    }
  }

  // Push last span.
  if (current_length > 1) {
    result->emplace_back(&boxes[current_start], current_length);
  }
}

}  // namespace

// Note that x_ and y_ must be initialized with enough intervals when passed
// to the disjunctive propagators.
NonOverlappingRectanglesDisjunctivePropagator::
    NonOverlappingRectanglesDisjunctivePropagator(bool strict,
                                                  SchedulingConstraintHelper* x,
                                                  SchedulingConstraintHelper* y,
                                                  Model* model)
    : global_x_(*x),
      global_y_(*y),
      x_(x->NumTasks(), model),
      strict_(strict),
      watcher_(model->GetOrCreate<GenericLiteralWatcher>()),
      overload_checker_(&x_),
      forward_detectable_precedences_(true, &x_),
      backward_detectable_precedences_(false, &x_),
      forward_not_last_(true, &x_),
      backward_not_last_(false, &x_),
      forward_edge_finding_(true, &x_),
      backward_edge_finding_(false, &x_) {}

NonOverlappingRectanglesDisjunctivePropagator::
    ~NonOverlappingRectanglesDisjunctivePropagator() {}

void NonOverlappingRectanglesDisjunctivePropagator::Register(
    int fast_priority, int slow_priority) {
  fast_id_ = watcher_->Register(this);
  watcher_->SetPropagatorPriority(fast_id_, fast_priority);
  global_x_.WatchAllTasks(fast_id_, watcher_);
  global_y_.WatchAllTasks(fast_id_, watcher_);

  // This propagator is the one making sure our propagation is complete, so
  // we do need to make sure it is called again if it modified some bounds.
  watcher_->NotifyThatPropagatorMayNotReachFixedPointInOnePass(fast_id_);

  const int slow_id = watcher_->Register(this);
  watcher_->SetPropagatorPriority(slow_id, slow_priority);
  global_x_.WatchAllTasks(slow_id, watcher_);
  global_y_.WatchAllTasks(slow_id, watcher_);
}

bool NonOverlappingRectanglesDisjunctivePropagator::
    FindBoxesThatMustOverlapAHorizontalLineAndPropagate(
        const SchedulingConstraintHelper& x, SchedulingConstraintHelper* y,
        std::function<bool()> inner_propagate) {
  // Note that since we only push bounds on x, we cache the value for y just
  // once.
  if (!y->SynchronizeAndSetTimeDirection(true)) return false;

  // Compute relevant boxes, the one with a mandatory part of y. Because we will
  // need to sort it this way, we consider them by increasing start max.
  indexed_intervals_.clear();
  const std::vector<TaskTime>& temp = y->TaskByDecreasingStartMax();
  for (int i = temp.size(); --i >= 0;) {
    const int box = temp[i].task_index;
    if (!strict_ && (x.SizeMin(box) == 0 || y->SizeMin(box) == 0)) continue;

    // Ignore a box if its x interval and its y interval are not present at
    // the same time.
    if (!x.IsPresent(box) || !y->IsPresent(box)) continue;

    const IntegerValue start_max = temp[i].time;
    const IntegerValue end_min = y->EndMin(box);
    if (start_max < end_min) {
      indexed_intervals_.push_back({box, start_max, end_min});
    }
  }

  // Less than 2 boxes, no propagation.
  if (indexed_intervals_.size() < 2) return true;
  ConstructOverlappingSets(/*already_sorted=*/true, &indexed_intervals_,
                           &events_overlapping_boxes_);

  // Split lists of boxes into disjoint set of boxes (w.r.t. overlap).
  boxes_to_propagate_.clear();
  reduced_overlapping_boxes_.clear();
  for (int i = 0; i < events_overlapping_boxes_.size(); ++i) {
    SplitDisjointBoxes(x, absl::MakeSpan(events_overlapping_boxes_[i]),
                       &disjoint_boxes_);
    for (absl::Span<int> sub_boxes : disjoint_boxes_) {
      // Boxes are sorted in a stable manner in the Split method.
      // Note that we do not use reduced_overlapping_boxes_ directly so that
      // the order of iteration is deterministic.
      const auto& insertion = reduced_overlapping_boxes_.insert(sub_boxes);
      if (insertion.second) boxes_to_propagate_.push_back(sub_boxes);
    }
  }

  // And finally propagate.
  //
  // TODO(user): Sorting of boxes seems influential on the performance. Test.
  for (const absl::Span<const int> boxes : boxes_to_propagate_) {
    x_.ClearOtherHelper();
    if (!x_.ResetFromSubset(x, boxes)) return false;

    // Collect the common overlapping coordinates of all boxes.
    IntegerValue lb(std::numeric_limits<int64_t>::min());
    IntegerValue ub(std::numeric_limits<int64_t>::max());
    for (const int b : boxes) {
      lb = std::max(lb, y->StartMax(b));
      ub = std::min(ub, y->EndMin(b) - 1);
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
    x_.SetOtherHelper(y, boxes, line_to_use_for_reason);

    RETURN_IF_FALSE(inner_propagate());
  }

  return true;
}

bool NonOverlappingRectanglesDisjunctivePropagator::Propagate() {
  global_x_.SetTimeDirection(true);
  global_y_.SetTimeDirection(true);

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
      global_x_, &global_y_, inner_propagate));

  // We can actually swap dimensions to propagate vertically.
  RETURN_IF_FALSE(FindBoxesThatMustOverlapAHorizontalLineAndPropagate(
      global_y_, &global_x_, inner_propagate));

  // If two boxes must overlap but do not have a mandatory line/column that
  // crosses both of them, then the code above do not see it. So we manually
  // propagate this case.
  //
  // TODO(user): Since we are at it, do more propagation even if no conflict?
  // This rarely propagate, so disabled for now. Investigate if it is worth
  // it.
  if (/*DISABLES CODE*/ (false) && watcher_->GetCurrentId() == fast_id_) {
    const int num_boxes = global_x_.NumTasks();
    for (int box1 = 0; box1 < num_boxes; ++box1) {
      if (!global_x_.IsPresent(box1)) continue;
      for (int box2 = box1 + 1; box2 < num_boxes; ++box2) {
        if (!global_x_.IsPresent(box2)) continue;
        if (global_x_.EndMin(box1) <= global_x_.StartMax(box2)) continue;
        if (global_x_.EndMin(box2) <= global_x_.StartMax(box1)) continue;
        if (global_y_.EndMin(box1) <= global_y_.StartMax(box2)) continue;
        if (global_y_.EndMin(box2) <= global_y_.StartMax(box1)) continue;

        // X and Y must overlap. This is a conflict.
        global_x_.ClearReason();
        global_x_.AddPresenceReason(box1);
        global_x_.AddPresenceReason(box2);
        global_x_.AddReasonForBeingBefore(box1, box2);
        global_x_.AddReasonForBeingBefore(box2, box1);
        global_y_.ClearReason();
        global_y_.AddPresenceReason(box1);
        global_y_.AddPresenceReason(box2);
        global_y_.AddReasonForBeingBefore(box1, box2);
        global_y_.AddReasonForBeingBefore(box2, box1);
        global_x_.ImportOtherReasons(global_y_);
        return global_x_.ReportConflict();
      }
    }
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
      x_.AddPresenceReason(left);
      x_.AddPresenceReason(right);
      x_.AddReasonForBeingBefore(left, right);
      x_.AddEndMinReason(left, left_end_min);
      RETURN_IF_FALSE(x_.IncreaseStartMin(right, left_end_min));
    }

    // right box pushes left box.
    const IntegerValue right_start_max = x_.StartMax(right);
    if (right_start_max < x_.EndMax(left)) {
      x_.ClearReason();
      x_.AddPresenceReason(left);
      x_.AddPresenceReason(right);
      x_.AddReasonForBeingBefore(left, right);
      x_.AddStartMaxReason(right, right_start_max);
      RETURN_IF_FALSE(x_.DecreaseEndMax(left, right_start_max));
    }

    return true;
  };

  switch (state) {
    case 0: {  // Conflict.
      x_.ClearReason();
      x_.AddPresenceReason(0);
      x_.AddPresenceReason(1);
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
