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

#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/base/log_severity.h"
#include "absl/base/optimization.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/types/span.h"
#include "ortools/sat/2d_rectangle_presolve.h"
#include "ortools/sat/debug_solution.h"
#include "ortools/sat/diffn_util.h"
#include "ortools/sat/enforcement.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/scheduling_helpers.h"
#include "ortools/sat/util.h"

namespace operations_research {
namespace sat {

bool NoOverlap2DConstraintHelper::SynchronizeAndSetDirection(
    bool x_is_forward_after_swap, bool y_is_forward_after_swap,
    bool swap_x_and_y) {
  if (axes_are_swapped_ != swap_x_and_y) {
    std::swap(x_helper_, y_helper_);
    std::swap(x_demands_helper_, y_demands_helper_);
    axes_are_swapped_ = !axes_are_swapped_;
  }
  if (!x_helper_->SynchronizeAndSetTimeDirection(x_is_forward_after_swap))
    return false;
  if (!y_helper_->SynchronizeAndSetTimeDirection(y_is_forward_after_swap))
    return false;
  return true;
}

SchedulingDemandHelper& NoOverlap2DConstraintHelper::x_demands_helper() {
  if (x_demands_helper_ == nullptr) {
    x_demands_helper_ = std::make_unique<SchedulingDemandHelper>(
        x_helper_->Sizes(), y_helper_.get(), model_);
  }
  return *x_demands_helper_;
}

SchedulingDemandHelper& NoOverlap2DConstraintHelper::y_demands_helper() {
  if (y_demands_helper_ == nullptr) {
    y_demands_helper_ = std::make_unique<SchedulingDemandHelper>(
        y_helper_->Sizes(), x_helper_.get(), model_);
  }
  return *y_demands_helper_;
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
  y->ResetReason();
  y->AddPresenceReason(box1);
  y->AddPresenceReason(box2);
  y->AddReasonForBeingBeforeAssumingNoOverlap(box1, box2);
  y->AddReasonForBeingBeforeAssumingNoOverlap(box2, box1);
}
}  // namespace

bool NoOverlap2DConstraintHelper::ReportConflictFromTwoBoxes(int box1,
                                                             int box2) {
  DCHECK_NE(box1, box2);
  if (DEBUG_MODE) {
    std::vector<PairwiseRestriction> restrictions;
    AppendPairwiseRestrictions({GetItemWithVariableSize(box1)},
                               {GetItemWithVariableSize(box2)}, &restrictions);
    DCHECK_EQ(restrictions.size(), 1);
    DCHECK(restrictions[0].type ==
           PairwiseRestriction::PairwiseRestrictionType::CONFLICT);
  }
  ClearAndAddMandatoryOverlapReason(box1, box2, x_helper_.get());
  ClearAndAddMandatoryOverlapReason(box1, box2, y_helper_.get());
  x_helper_->ImportReasonsFromOther(*y_helper_);
  return x_helper_->ReportConflict();
}

bool NoOverlap2DConstraintHelper::ReportConflictFromInfeasibleBoxRanges(
    absl::Span<const RectangleInRange> ranges) {
  if (ranges.size() == 2) {
    return ReportConflictFromTwoBoxes(ranges[0].box_index, ranges[1].box_index);
  }
  x_helper_->ResetReason();
  y_helper_->ResetReason();
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
  x_helper_->ImportReasonsFromOther(*y_helper_);
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
  x->ResetReason();
  x->AddPresenceReason(left);
  x->AddPresenceReason(right);
  x->AddReasonForBeingBeforeAssumingNoOverlap(left, right);
  // left and right must overlap on y.
  ClearAndAddMandatoryOverlapReason(left, right, y);
  // Propagate with the complete reason.
  x->ImportReasonsFromOther(*y);
  return x->PushTaskOrderWhenPresent(left, right);
}

}  // namespace

bool NoOverlap2DConstraintHelper::PropagateRelativePosition(
    int first, int second, PairwiseRestriction::PairwiseRestrictionType type) {
  switch (type) {
    case PairwiseRestriction::PairwiseRestrictionType::CONFLICT:
      return ReportConflictFromTwoBoxes(first, second);
    case PairwiseRestriction::PairwiseRestrictionType::FIRST_LEFT_OF_SECOND:
      return LeftBoxBeforeRightBoxOnFirstDimension(
          first, second, x_helper_.get(), y_helper_.get());
    case PairwiseRestriction::PairwiseRestrictionType::FIRST_RIGHT_OF_SECOND:
      return LeftBoxBeforeRightBoxOnFirstDimension(
          second, first, x_helper_.get(), y_helper_.get());
    case PairwiseRestriction::PairwiseRestrictionType::FIRST_BELOW_SECOND:
      return LeftBoxBeforeRightBoxOnFirstDimension(
          first, second, y_helper_.get(), x_helper_.get());
    case PairwiseRestriction::PairwiseRestrictionType::FIRST_ABOVE_SECOND:
      return LeftBoxBeforeRightBoxOnFirstDimension(
          second, first, y_helper_.get(), x_helper_.get());
  }
  ABSL_UNREACHABLE();
}

void NoOverlap2DConstraintHelper::Reset(
    absl::Span<const Rectangle> fixed_boxes,
    absl::Span<const int> non_fixed_box_indexes) {
  inprocessing_count_++;
  std::vector<AffineExpression> x_starts;
  std::vector<AffineExpression> x_ends;
  std::vector<AffineExpression> x_sizes;
  std::vector<LiteralIndex> x_reason_for_presence;
  std::vector<AffineExpression> y_starts;
  std::vector<AffineExpression> y_ends;
  std::vector<AffineExpression> y_sizes;
  std::vector<LiteralIndex> y_reason_for_presence;

  auto add_non_fixed_box = [&](int box_index) {
    x_starts.push_back(x_helper_->Starts()[box_index]);
    x_ends.push_back(x_helper_->Ends()[box_index]);
    x_sizes.push_back(x_helper_->Sizes()[box_index]);
    if (x_helper_->IsOptional(box_index)) {
      x_reason_for_presence.push_back(x_helper_->PresenceLiteral(box_index));
    } else {
      x_reason_for_presence.push_back(kNoLiteralIndex);
    }

    y_starts.push_back(y_helper_->Starts()[box_index]);
    y_ends.push_back(y_helper_->Ends()[box_index]);
    y_sizes.push_back(y_helper_->Sizes()[box_index]);
    if (y_helper_->IsOptional(box_index)) {
      y_reason_for_presence.push_back(y_helper_->PresenceLiteral(box_index));
    } else {
      y_reason_for_presence.push_back(kNoLiteralIndex);
    }

    return x_starts.size() - 1;
  };

  auto add_fixed_box = [&](const Rectangle& box) {
    x_starts.push_back(AffineExpression(box.x_min));
    x_ends.push_back(AffineExpression(box.x_max));
    x_sizes.push_back(box.SizeX());
    x_reason_for_presence.push_back(kNoLiteralIndex);

    y_starts.push_back(AffineExpression(box.y_min));
    y_ends.push_back(AffineExpression(box.y_max));
    y_sizes.push_back(box.SizeY());
    y_reason_for_presence.push_back(kNoLiteralIndex);

    return x_starts.size() - 1;
  };

  std::vector<Rectangle> active_bounding_boxes;
  std::vector<std::tuple<int, bool>> active_box_indexes;
  int new_num_boxes = fixed_boxes.size() + non_fixed_box_indexes.size();
  active_bounding_boxes.reserve(new_num_boxes);
  active_box_indexes.reserve(new_num_boxes);
  DCHECK_EQ(x_helper_->CurrentDecisionLevel(), 0);
  for (int box : non_fixed_box_indexes) {
    if (IsAbsent(box)) continue;
    active_bounding_boxes.push_back(GetBoundingRectangle(box));
    // At level zero we can do a stronger check whether a box is fixed, since
    // we can see use IsPresent() instead of !IsOptional().
    const bool is_fixed = IsPresent(box) && x_helper_->StartIsFixed(box) &&
                          x_helper_->EndIsFixed(box) &&
                          y_helper_->StartIsFixed(box) &&
                          y_helper_->EndIsFixed(box);
    active_box_indexes.push_back({box, is_fixed});
  }
  for (int box = 0; box < fixed_boxes.size(); ++box) {
    active_bounding_boxes.push_back(fixed_boxes[box]);
    active_box_indexes.push_back({box, true});
  }
  CompactVectorVector<int> components =
      GetOverlappingRectangleComponents(absl::MakeSpan(active_bounding_boxes));
  connected_components_.clear();
  for (absl::Span<const int> component : components.AsVectorOfSpan()) {
    if (component.size() < 2) continue;
    connected_components_.Add({});
    for (int idx : component) {
      const auto [box_idx, is_fixed] = active_box_indexes[idx];
      int new_index;
      if (is_fixed) {
        new_index = add_fixed_box(fixed_boxes[box_idx]);
      } else {
        new_index = add_non_fixed_box(box_idx);
      }
      connected_components_.AppendToLastVector(new_index);
    }
  }
  const int old_num_boxes = NumBoxes();

  if (old_num_boxes != connected_components_.num_entries()) {
    VLOG_EVERY_N_SEC(1, 1) << "Total boxes: "
                           << connected_components_.num_entries()
                           << " connected components: "
                           << connected_components_.size()
                           << " dropped single box components: "
                           << old_num_boxes -
                                  connected_components_.num_entries();
  }
  VLOG_EVERY_N_SEC(1, 2) << "No_overlap_2d helper inprocessing: "
                         << connected_components_.size() << " components and "
                         << connected_components_.num_entries() << " boxes";

  x_helper_ = std::make_unique<SchedulingConstraintHelper>(
      std::move(x_starts), std::move(x_ends), std::move(x_sizes),
      std::move(x_reason_for_presence), model_);
  y_helper_ = std::make_unique<SchedulingConstraintHelper>(
      std::move(y_starts), std::move(y_ends), std::move(y_sizes),
      std::move(y_reason_for_presence), model_);
  x_helper_->SetEnforcementId(enforcement_id_);
  y_helper_->SetEnforcementId(enforcement_id_);
  x_demands_helper_ = nullptr;
  y_demands_helper_ = nullptr;
}

bool NoOverlap2DConstraintHelper::IsEnforced() const {
  return enforcement_helper_.Status(enforcement_id_) ==
         EnforcementStatus::IS_ENFORCED;
}

bool NoOverlap2DConstraintHelper::Propagate() {
  if (!IsEnforced()) return true;
  for (const int id : propagators_watching_) {
    watcher_->CallOnNextPropagate(id);
  }
  if (!x_helper_->Propagate() || !y_helper_->Propagate()) return false;

  if (x_helper_->CurrentDecisionLevel() == 0) {
    ++level_zero_bound_change_idx_;
    SynchronizeAndSetDirection();
    int num_boxes = NumBoxes();

    // Subtle: it is tempting to run this logic once for every connected
    // component. However, this would be buggy, since the
    // PresolveFixed2dRectangles() might grow a fixed box making it overlap with
    // something on another component.
    std::vector<Rectangle> fixed_boxes;
    std::vector<int> non_fixed_box_indexes;
    std::vector<RectangleInRange> non_fixed_boxes;
    fixed_boxes.reserve(num_boxes);
    non_fixed_box_indexes.reserve(num_boxes);
    non_fixed_boxes.reserve(num_boxes);
    bool has_zero_area_boxes = false;
    for (int box_index = 0; box_index < num_boxes; ++box_index) {
      if (x_helper_->IsAbsent(box_index) || y_helper_->IsAbsent(box_index)) {
        continue;
      }
      if (x_helper_->SizeMin(box_index) == 0 ||
          y_helper_->SizeMin(box_index) == 0) {
        has_zero_area_boxes = true;
      }
      if (IsOptional(box_index) || !IsFixed(box_index)) {
        non_fixed_boxes.push_back(
            {.bounding_area = GetBoundingRectangle(box_index),
             .x_size = x_helper_->SizeMin(box_index),
             .y_size = y_helper_->SizeMin(box_index)});
        non_fixed_box_indexes.push_back(box_index);
      } else {
        fixed_boxes.push_back(GetItemRangeForSizeMin(box_index).bounding_area);
      }
    }

    const int original_num_fixed_boxes = fixed_boxes.size();
    if (!non_fixed_boxes.empty() && !has_zero_area_boxes) {
      if (FindPartialRectangleIntersections(fixed_boxes).empty()) {
        PresolveFixed2dRectangles(non_fixed_boxes, &fixed_boxes);
      }
    }

    if (fixed_boxes.size() != original_num_fixed_boxes) {
      VLOG_EVERY_N_SEC(1, 1) << "Num_boxes: " << num_boxes
                             << " fixed_before: " << original_num_fixed_boxes
                             << " fixed_after: " << fixed_boxes.size();
    }
    Reset(fixed_boxes, non_fixed_box_indexes);
  }
  return true;
}

void NoOverlap2DConstraintHelper::RegisterWith(
    GenericLiteralWatcher* watcher,
    absl::Span<const Literal> enforcement_literals) {
  const int id = watcher->Register(this);
  const int num_boxes = NumBoxes();
  for (int b = 0; b < num_boxes; ++b) {
    if (x_helper_->IsOptional(b)) {
      watcher->WatchLiteral(x_helper_->PresenceLiteral(b), id);
    }
    if (y_helper_->IsOptional(b)) {
      watcher->WatchLiteral(y_helper_->PresenceLiteral(b), id);
    }
    watcher->WatchIntegerVariable(x_helper_->Sizes()[b].var, id);
    watcher->WatchIntegerVariable(x_helper_->Starts()[b].var, id);
    watcher->WatchIntegerVariable(x_helper_->Ends()[b].var, id);
    watcher->WatchIntegerVariable(y_helper_->Sizes()[b].var, id);
    watcher->WatchIntegerVariable(y_helper_->Starts()[b].var, id);
    watcher->WatchIntegerVariable(y_helper_->Ends()[b].var, id);
  }
  watcher->SetPropagatorPriority(id, 0);
  enforcement_id_ =
      enforcement_helper_.Register(enforcement_literals, watcher, id);
  x_helper_->SetEnforcementId(enforcement_id_);
  y_helper_->SetEnforcementId(enforcement_id_);
}

Rectangle NoOverlap2DConstraintHelper::GetBoxInDebugSolution(int index) const {
  DebugSolution* debug_solution = model_->GetOrCreate<DebugSolution>();
  const auto& ivar_values = debug_solution->IntegerVariableValues();
  if (ivar_values.empty()) {
    return {};
  }
  const AffineExpression& x_start = x_helper_->Starts()[index];
  const AffineExpression& x_size = x_helper_->Sizes()[index];
  const AffineExpression& y_start = y_helper_->Starts()[index];
  const AffineExpression& y_size = y_helper_->Sizes()[index];
  const IntegerValue x_min = x_start.IsConstant()
                                 ? x_start.constant
                                 : x_start.ValueAt(ivar_values[x_start.var]);
  const IntegerValue x_size_val = x_size.IsConstant()
                                      ? x_size.constant
                                      : x_size.ValueAt(ivar_values[x_size.var]);
  const IntegerValue y_min = y_start.IsConstant()
                                 ? y_start.constant
                                 : y_start.ValueAt(ivar_values[y_start.var]);
  const IntegerValue y_size_val = y_size.IsConstant()
                                      ? y_size.constant
                                      : y_size.ValueAt(ivar_values[y_size.var]);
  return {.x_min = x_min,
          .x_max = x_min + x_size_val,
          .y_min = y_min,
          .y_max = y_min + y_size_val};
}

}  // namespace sat
}  // namespace operations_research
