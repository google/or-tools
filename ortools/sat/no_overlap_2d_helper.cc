// Copyright 2010-2025 Google LLC
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

#include "ortools/sat/no_overlap_2d_helper.h"

#include <utility>

#include "absl/types/span.h"
#include "ortools/sat/diffn_util.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/scheduling_helpers.h"

namespace operations_research {
namespace sat {

bool NoOverlap2DConstraintHelper::SynchronizeAndSetDirection(
    bool x_is_forward_after_swap, bool y_is_forward_after_swap,
    bool swap_x_and_y) {
  if (axes_are_swapped_ != swap_x_and_y) {
    std::swap(x_helper_, y_helper_);
    axes_are_swapped_ = !axes_are_swapped_;
  }
  if (!x_helper_->SynchronizeAndSetTimeDirection(x_is_forward_after_swap))
    return false;
  if (!y_helper_->SynchronizeAndSetTimeDirection(y_is_forward_after_swap))
    return false;
  return true;
}

RectangleInRange NoOverlap2DConstraintHelper::GetItemRangeForSizeMin(
    int index) const {
  return RectangleInRange{
      .box_index = index,
      .bounding_area = {.x_min = x_helper_->StartMin(index),
                        .x_max = x_helper_->StartMax(index) +
                                 x_helper_->SizeMin(index),
                        .y_min = y_helper_->StartMin(index),
                        .y_max = y_helper_->StartMax(index) +
                                 y_helper_->SizeMin(index)},
      .x_size = x_helper_->SizeMin(index),
      .y_size = y_helper_->SizeMin(index)};
}

ItemWithVariableSize NoOverlap2DConstraintHelper::GetItemWithVariableSize(
    int index) const {
  return ItemWithVariableSize{.index = index,
                              .x = {.start_min = x_helper_->StartMin(index),
                                    .start_max = x_helper_->StartMax(index),
                                    .end_min = x_helper_->EndMin(index),
                                    .end_max = x_helper_->EndMax(index)},
                              .y = {.start_min = y_helper_->StartMin(index),
                                    .start_max = y_helper_->StartMax(index),
                                    .end_min = y_helper_->EndMin(index),
                                    .end_max = y_helper_->EndMax(index)}};
}

namespace {
void ClearAndAddMandatoryOverlapReason(int box1, int box2,
                                       SchedulingConstraintHelper* y) {
  y->ClearReason();
  y->AddPresenceReason(box1);
  y->AddPresenceReason(box2);
  y->AddReasonForBeingBefore(box1, box2);
  y->AddReasonForBeingBefore(box2, box1);
}
}  // namespace

bool NoOverlap2DConstraintHelper::ReportConflictFromTwoBoxes(int box1,
                                                             int box2) {
  ClearAndAddMandatoryOverlapReason(box1, box2, x_helper_);
  ClearAndAddMandatoryOverlapReason(box1, box2, y_helper_);
  x_helper_->ImportOtherReasons(*y_helper_);
  return x_helper_->ReportConflict();
}

bool NoOverlap2DConstraintHelper::ReportConflictFromInfeasibleBoxRanges(
    absl::Span<const RectangleInRange> ranges) {
  if (ranges.size() == 2) {
    return ReportConflictFromTwoBoxes(ranges[0].box_index, ranges[1].box_index);
  }
  x_helper_->ClearReason();
  y_helper_->ClearReason();
  for (const auto& range : ranges) {
    const int b = range.box_index;

    x_helper_->AddStartMinReason(b, range.bounding_area.x_min);
    y_helper_->AddStartMinReason(b, range.bounding_area.y_min);

    x_helper_->AddStartMaxReason(b, range.bounding_area.x_max - range.x_size);
    y_helper_->AddStartMaxReason(b, range.bounding_area.y_max - range.y_size);

    x_helper_->AddSizeMinReason(b);
    y_helper_->AddSizeMinReason(b);

    x_helper_->AddPresenceReason(b);
    y_helper_->AddPresenceReason(b);
  }
  x_helper_->ImportOtherReasons(*y_helper_);
  return x_helper_->ReportConflict();
}

namespace {
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
    // left and right must overlap on y.
    ClearAndAddMandatoryOverlapReason(left, right, y);
    // Propagate with the complete reason.
    x->ImportOtherReasons(*y);
    if (!x->IncreaseStartMin(right, left_end_min)) return false;
  }

  // right box2 pushes left box2.
  const IntegerValue right_start_max = x->StartMax(right);
  if (right_start_max < x->EndMax(left)) {
    x->ClearReason();
    x->AddPresenceReason(left);
    x->AddPresenceReason(right);
    x->AddReasonForBeingBefore(left, right);
    x->AddStartMaxReason(right, right_start_max);
    // left and right must overlap on y.
    ClearAndAddMandatoryOverlapReason(left, right, y);
    // Propagate with the complete reason.
    x->ImportOtherReasons(*y);
    if (!x->DecreaseEndMax(left, right_start_max)) return false;
  }

  return true;
}

}  // namespace

bool NoOverlap2DConstraintHelper::PropagateRelativePosition(
    int first, int second, PairwiseRestriction::PairwiseRestrictionType type) {
  switch (type) {
    case PairwiseRestriction::PairwiseRestrictionType::CONFLICT:
      return ReportConflictFromTwoBoxes(first, second);
    case PairwiseRestriction::PairwiseRestrictionType::FIRST_LEFT_OF_SECOND:
      return LeftBoxBeforeRightBoxOnFirstDimension(first, second, x_helper_,
                                                   y_helper_);
    case PairwiseRestriction::PairwiseRestrictionType::FIRST_RIGHT_OF_SECOND:
      return LeftBoxBeforeRightBoxOnFirstDimension(second, first, x_helper_,
                                                   y_helper_);
    case PairwiseRestriction::PairwiseRestrictionType::FIRST_BELOW_SECOND:
      return LeftBoxBeforeRightBoxOnFirstDimension(first, second, y_helper_,
                                                   x_helper_);
    case PairwiseRestriction::PairwiseRestrictionType::FIRST_ABOVE_SECOND:
      return LeftBoxBeforeRightBoxOnFirstDimension(second, first, y_helper_,
                                                   x_helper_);
  }
}

}  // namespace sat
}  // namespace operations_research
