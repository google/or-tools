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

#include "ortools/sat/diffn.h"

#include <stddef.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/sat/cumulative_energy.h"
#include "ortools/sat/diffn_util.h"
#include "ortools/sat/disjunctive.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_expr.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/timetable.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/util/strong_integers.h"

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

void AddDiffnCumulativeRelationOnX(SchedulingConstraintHelper* x,
                                   SchedulingConstraintHelper* y,
                                   Model* model) {
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

  auto* watcher = model->GetOrCreate<GenericLiteralWatcher>();

  const SatParameters* params = model->GetOrCreate<SatParameters>();
  const bool add_timetabling_relaxation =
      params->use_timetabling_in_no_overlap_2d();
  bool add_energetic_relaxation =
      params->use_energetic_reasoning_in_no_overlap_2d();

  // Needed if we use one of the relaxation below.
  SchedulingDemandHelper* demands;
  if (add_timetabling_relaxation || add_energetic_relaxation) {
    demands =
        model->GetOrCreate<IntervalsRepository>()->GetOrCreateDemandHelper(
            x, sizes);
  }

  // Propagator responsible for applying Timetabling filtering rule. It
  // increases the minimum of the start variables, decrease the maximum of the
  // end variables, and increase the minimum of the capacity variable.
  if (add_timetabling_relaxation) {
    DCHECK(demands != nullptr);
    TimeTablingPerTask* time_tabling =
        new TimeTablingPerTask(capacity, x, demands, model);
    time_tabling->RegisterWith(watcher);
    model->TakeOwnership(time_tabling);
  }

  // Propagator responsible for applying the Overload Checking filtering rule.
  // It increases the minimum of the capacity variable.
  if (add_energetic_relaxation) {
    DCHECK(demands != nullptr);
    AddCumulativeOverloadChecker(capacity, x, demands, model);
  }
}

#define RETURN_IF_FALSE(f) \
  if (!(f)) return false;

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

// This function will fill the helper why the two boxes always overlap on that
// dimension.
void ClearAndAddMandatoryOverlapReason(int box1, int box2,
                                       SchedulingConstraintHelper* helper) {
  helper->ClearReason();
  helper->AddPresenceReason(box1);
  helper->AddPresenceReason(box2);
  helper->AddReasonForBeingBefore(box1, box2);
  helper->AddReasonForBeingBefore(box2, box1);
}

// This function assumes that the left and right boxes overlap on the second
// dimension, and that left cannot be after right.
// It checks and pushes the lower bound of the right box and the upper bound
// of the left box if need.
//
// If y is not null, it import the mandatory reason for the overlap on y in
// the x helper.
bool LeftBoxBeforeRightBoxOnFirstDimension(int left, int right,
                                           SchedulingConstraintHelper* x,
                                           SchedulingConstraintHelper* y) {
  // left box2 pushes right box2.
  const IntegerValue left_end_min = x->EndMin(left);
  if (left_end_min > x->StartMin(right)) {
    x->ClearReason();
    x->AddPresenceReason(left);
    x->AddPresenceReason(right);
    x->AddReasonForBeingBefore(left, right);
    x->AddEndMinReason(left, left_end_min);
    if (y != nullptr) {
      // left and right must overlap on y.
      ClearAndAddMandatoryOverlapReason(left, right, y);
      // Propagate with the complete reason.
      x->ImportOtherReasons(*y);
    }
    RETURN_IF_FALSE(x->IncreaseStartMin(right, left_end_min));
  }

  // right box2 pushes left box2.
  const IntegerValue right_start_max = x->StartMax(right);
  if (right_start_max < x->EndMax(left)) {
    x->ClearReason();
    x->AddPresenceReason(left);
    x->AddPresenceReason(right);
    x->AddReasonForBeingBefore(left, right);
    x->AddStartMaxReason(right, right_start_max);
    if (y != nullptr) {
      // left and right must overlap on y.
      ClearAndAddMandatoryOverlapReason(left, right, y);
      // Propagate with the complete reason.
      x->ImportOtherReasons(*y);
    }
    RETURN_IF_FALSE(x->DecreaseEndMax(left, right_start_max));
  }

  return true;
}

}  // namespace

// Note that x_ and y_ must be initialized with enough intervals when passed
// to the disjunctive propagators.
NonOverlappingRectanglesDisjunctivePropagator::
    NonOverlappingRectanglesDisjunctivePropagator(SchedulingConstraintHelper* x,
                                                  SchedulingConstraintHelper* y,
                                                  Model* model)
    : global_x_(*x),
      global_y_(*y),
      x_(x->NumTasks(), model),
      watcher_(model->GetOrCreate<GenericLiteralWatcher>()),
      overload_checker_(&x_),
      forward_detectable_precedences_(true, &x_),
      backward_detectable_precedences_(false, &x_),
      forward_not_last_(true, &x_),
      backward_not_last_(false, &x_),
      forward_edge_finding_(true, &x_),
      backward_edge_finding_(false, &x_),
      pairwise_propagation_(model->GetOrCreate<SatParameters>()
                                ->use_pairwise_reasoning_in_no_overlap_2d()) {
  // Checks the presence of potential zero area boxes.
  for (int b = 0; b < global_x_.NumTasks(); ++b) {
    if (global_x_.SizeMin(b) == 0 || global_y_.SizeMin(b) == 0) {
      has_zero_area_boxes_ = true;
      break;
    }
  }
}

NonOverlappingRectanglesDisjunctivePropagator::
    ~NonOverlappingRectanglesDisjunctivePropagator() = default;

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
        bool fast_propagation, const SchedulingConstraintHelper& x,
        SchedulingConstraintHelper* y) {
  // Note that since we only push bounds on x, we cache the value for y just
  // once.
  if (!y->SynchronizeAndSetTimeDirection(true)) return false;

  // Compute relevant boxes, the one with a mandatory part of y. Because we will
  // need to sort it this way, we consider them by increasing start max.
  indexed_boxes_.clear();
  const std::vector<TaskTime>& temp = y->TaskByDecreasingStartMax();
  for (int i = temp.size(); --i >= 0;) {
    const int box = temp[i].task_index;
    // Ignore absent boxes.
    if (x.IsAbsent(box) || y->IsAbsent(box)) continue;

    // Ignore boxes where the relevant presence literal is only on the y
    // dimension, or if both intervals are optionals with different literals.
    if (x.IsPresent(box) && !y->IsPresent(box)) continue;
    if (!x.IsPresent(box) && !y->IsPresent(box) &&
        x.PresenceLiteral(box) != y->PresenceLiteral(box)) {
      continue;
    }

    const IntegerValue start_max = temp[i].time;
    const IntegerValue end_min = y->EndMin(box);
    if (start_max < end_min) {
      indexed_boxes_.push_back({box, start_max, end_min});
    }
  }

  // Less than 2 boxes, no propagation.
  if (indexed_boxes_.size() < 2) return true;
  ConstructOverlappingSets(/*already_sorted=*/true, &indexed_boxes_,
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
    // The case of two boxes should be taken care of during "fast" propagation,
    // so we can skip it here.
    if (!fast_propagation && boxes.size() <= 2) continue;

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

    // We want for different propagation to reuse as much as possible the same
    // line. The idea behind this is to compute the 'canonical' line to use
    // when explaining that boxes overlap on the 'y_dim' dimension. We compute
    // the multiple of the biggest power of two that is common to all boxes.
    //
    // TODO(user): We should scan the integer trail to find the oldest
    // non-empty common interval. Then we can pick the canonical value within
    // it.
    const IntegerValue line_to_use_for_reason = FindCanonicalValue(lb, ub);

    // Setup x_dim for propagation.
    x_.SetOtherHelper(y, boxes, line_to_use_for_reason);

    if (fast_propagation) {
      if (x_.NumTasks() == 2) {
        // In that case, we can use simpler algorithms.
        // Note that this case happens frequently (~30% of all calls to this
        // method according to our tests).
        RETURN_IF_FALSE(PropagateOnXWhenOnlyTwoBoxes());
      } else {
        RETURN_IF_FALSE(overload_checker_.Propagate());
        RETURN_IF_FALSE(forward_detectable_precedences_.Propagate());
        RETURN_IF_FALSE(backward_detectable_precedences_.Propagate());
      }
    } else {
      DCHECK_GT(x_.NumTasks(), 2);
      RETURN_IF_FALSE(forward_not_last_.Propagate());
      RETURN_IF_FALSE(backward_not_last_.Propagate());
      RETURN_IF_FALSE(backward_edge_finding_.Propagate());
      RETURN_IF_FALSE(forward_edge_finding_.Propagate());
    }
  }

  return true;
}

bool NonOverlappingRectanglesDisjunctivePropagator::Propagate() {
  global_x_.SetTimeDirection(true);
  global_y_.SetTimeDirection(true);

  // Note that the code assumes that this was registered twice in fast and slow
  // mode. So we will not redo some propagation in slow mode that was already
  // done by the fast mode.
  const bool fast_propagation = watcher_->GetCurrentId() == fast_id_;
  RETURN_IF_FALSE(FindBoxesThatMustOverlapAHorizontalLineAndPropagate(
      fast_propagation, global_x_, &global_y_));

  // We can actually swap dimensions to propagate vertically.
  RETURN_IF_FALSE(FindBoxesThatMustOverlapAHorizontalLineAndPropagate(
      fast_propagation, global_y_, &global_x_));

  if (!fast_propagation && (has_zero_area_boxes_ || pairwise_propagation_)) {
    RETURN_IF_FALSE(PropagateAllPairsOfBoxes());
  }

  return true;
}

// Specialized propagation on only two boxes that must intersect with the
// given y_line_for_reason.
bool NonOverlappingRectanglesDisjunctivePropagator::
    PropagateOnXWhenOnlyTwoBoxes() {
  if (!x_.IsPresent(0) || !x_.IsPresent(1)) return true;

  // For each direction and each order, we test if the boxes can be disjoint.
  const int state =
      (x_.EndMin(0) <= x_.StartMax(1)) + 2 * (x_.EndMin(1) <= x_.StartMax(0));
  switch (state) {
    case 0: {  // Conflict.
      ClearAndAddMandatoryOverlapReason(0, 1, &x_);
      // Note that the secondary helper is set on x.
      return x_.ReportConflict();
    }
    case 1: {  // b1 is left of b2.
      return LeftBoxBeforeRightBoxOnFirstDimension(0, 1, &x_, /*y=*/nullptr);
    }
    case 2: {  // b2 is left of b1.
      return LeftBoxBeforeRightBoxOnFirstDimension(1, 0, &x_, /*y=*/nullptr);
    }
    default: {  // Nothing to deduce.
      return true;
    }
  }
}

// TODO(user): Use box splitting to speed up.
bool NonOverlappingRectanglesDisjunctivePropagator::PropagateAllPairsOfBoxes() {
  // Extra propagation code for zero-area boxes, and for all pairs of boxes in
  // pairwise_propagation_ mode.
  DCHECK(pairwise_propagation_ || has_zero_area_boxes_);
  horizontal_zero_area_boxes_.clear();
  vertical_zero_area_boxes_.clear();
  point_zero_area_boxes_.clear();
  non_zero_area_boxes_.clear();
  for (int b = 0; b < global_x_.NumTasks(); ++b) {
    if (!global_x_.IsPresent(b) || !global_y_.IsPresent(b)) continue;
    const IntegerValue x_size_max = global_x_.SizeMax(b);
    const IntegerValue y_size_max = global_y_.SizeMax(b);
    if (x_size_max == 0) {
      if (y_size_max == 0) {
        point_zero_area_boxes_.push_back(b);
      } else {
        vertical_zero_area_boxes_.push_back(b);
      }
    } else if (y_size_max == 0) {
      horizontal_zero_area_boxes_.push_back(b);
    } else {
      non_zero_area_boxes_.push_back(b);
    }
  }

  if (pairwise_propagation_) {
    for (int i1 = 0; i1 + 1 < non_zero_area_boxes_.size(); ++i1) {
      for (int i2 = i1 + 1; i2 < non_zero_area_boxes_.size(); ++i2) {
        RETURN_IF_FALSE(PropagateTwoBoxes(non_zero_area_boxes_[i1],
                                          non_zero_area_boxes_[i2]));
      }
    }
  }

  // Propagates zero area boxes against non-zero area boxes.
  for (const int nz : non_zero_area_boxes_) {
    for (const int z : horizontal_zero_area_boxes_) {
      RETURN_IF_FALSE(PropagateTwoBoxes(z, nz));
    }
    for (const int z : vertical_zero_area_boxes_) {
      RETURN_IF_FALSE(PropagateTwoBoxes(z, nz));
    }
    for (const int z : point_zero_area_boxes_) {
      RETURN_IF_FALSE(PropagateTwoBoxes(z, nz));
    }
  }

  // Propagates vertical zero area boxes against horizontal zero area boxes.
  for (const int i1 : horizontal_zero_area_boxes_) {
    for (const int i2 : vertical_zero_area_boxes_) {
      RETURN_IF_FALSE(PropagateTwoBoxes(i1, i2));
    }
  }

  return true;
}

bool NonOverlappingRectanglesDisjunctivePropagator::PropagateTwoBoxes(
    int box1, int box2) {
  // We use the global helpers, without the mechanism of sub-helpers.
  // So any reason will need to be computed on both dimensions, and imported in
  // the helper doing the modification.
  DCHECK(!global_x_.HasOtherHelper());
  DCHECK(!global_y_.HasOtherHelper());

  const int state =
      // box1 can be left of box2.
      (global_x_.EndMin(box1) <= global_x_.StartMax(box2)) +
      // box1 can be right of box2.
      2 * (global_x_.EndMin(box2) <= global_x_.StartMax(box1)) +
      // box1 can be below box2.
      4 * (global_y_.EndMin(box1) <= global_y_.StartMax(box2)) +
      // box1 can be up of box2.
      8 * (global_y_.EndMin(box2) <= global_y_.StartMax(box1));

  switch (state) {
    case 0: {  // Conflict. The two boxes must overlap in both dimensions.
      ClearAndAddMandatoryOverlapReason(box1, box2, &global_x_);
      ClearAndAddMandatoryOverlapReason(box1, box2, &global_y_);
      global_x_.ImportOtherReasons(global_y_);
      return global_x_.ReportConflict();
    }
    case 1: {  // box2 can only be after box1 on x.
      return LeftBoxBeforeRightBoxOnFirstDimension(box1, box2, &global_x_,
                                                   &global_y_);
    }
    case 2: {  // box1 an only be after box2 on x.
      return LeftBoxBeforeRightBoxOnFirstDimension(box2, box1, &global_x_,
                                                   &global_y_);
    }
    case 4: {  // box2 an only be after box1 on y.
      return LeftBoxBeforeRightBoxOnFirstDimension(box1, box2, &global_y_,
                                                   &global_x_);
    }
    case 8: {  // box1 an only be after box2 on y.
      return LeftBoxBeforeRightBoxOnFirstDimension(box2, box1, &global_y_,
                                                   &global_x_);
    }
    default: {
      break;
    }
  }
  return true;
}

#undef RETURN_IF_FALSE
}  // namespace sat
}  // namespace operations_research
