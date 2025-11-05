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

#ifndef ORTOOLS_SAT_NO_OVERLAP_2D_HELPER_H_
#define ORTOOLS_SAT_NO_OVERLAP_2D_HELPER_H_

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "absl/types/span.h"
#include "ortools/sat/diffn_util.h"
#include "ortools/sat/enforcement.h"
#include "ortools/sat/enforcement_helper.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/scheduling_helpers.h"
#include "ortools/sat/util.h"

namespace operations_research {
namespace sat {

// Helper class shared by the propagators that handle no_overlap_2d constraints.
//
// Having a helper class like this one makes much easier to do in-processing and
// to share pre-computed data between the two propagators.
class NoOverlap2DConstraintHelper : public PropagatorInterface {
 public:
  NoOverlap2DConstraintHelper(std::vector<AffineExpression> x_starts,
                              std::vector<AffineExpression> x_ends,
                              std::vector<AffineExpression> x_sizes,
                              std::vector<LiteralIndex> x_reason_for_presence,
                              std::vector<AffineExpression> y_starts,
                              std::vector<AffineExpression> y_ends,
                              std::vector<AffineExpression> y_sizes,
                              std::vector<LiteralIndex> y_reason_for_presence,
                              Model* model)
      : axes_are_swapped_(false),
        x_helper_(std::make_unique<SchedulingConstraintHelper>(
            x_starts, x_ends, x_sizes, x_reason_for_presence, model)),
        y_helper_(std::make_unique<SchedulingConstraintHelper>(
            y_starts, y_ends, y_sizes, y_reason_for_presence, model)),
        enforcement_helper_(*model->GetOrCreate<EnforcementHelper>()),
        enforcement_id_(-1),
        model_(model),
        watcher_(model->GetOrCreate<GenericLiteralWatcher>()) {
    const int num_boxes = x_helper_->NumTasks();
    connected_components_.reserve(1, num_boxes);
    connected_components_.Add({});
    for (int i = 0; i < x_helper_->NumTasks(); ++i) {
      if (!x_helper_->IsAbsent(i) && !y_helper_->IsAbsent(i)) {
        connected_components_.AppendToLastVector(i);
      }
    }
  }

  void RegisterWith(GenericLiteralWatcher* watcher,
                    absl::Span<const Literal> enforcement_literals);

  bool SynchronizeAndSetDirection(bool x_is_forward_after_swap = true,
                                  bool y_is_forward_after_swap = true,
                                  bool swap_x_and_y = false);

  bool IsOptional(int index) const {
    return x_helper_->IsOptional(index) || y_helper_->IsOptional(index);
  }

  bool IsPresent(int index) const {
    return x_helper_->IsPresent(index) && y_helper_->IsPresent(index);
  }

  bool IsAbsent(int index) const {
    return x_helper_->IsAbsent(index) || y_helper_->IsAbsent(index);
  }

  Rectangle GetBoundingRectangle(int index) const {
    return {.x_min = x_helper_->StartMin(index),
            .x_max = x_helper_->EndMax(index),
            .y_min = y_helper_->StartMin(index),
            .y_max = y_helper_->EndMax(index)};
  }

  Rectangle GetLevelZeroBoundingRectangle(int index) const {
    return {.x_min = x_helper_->LevelZeroStartMin(index),
            .x_max = x_helper_->LevelZeroEndMax(index),
            .y_min = y_helper_->LevelZeroStartMin(index),
            .y_max = y_helper_->LevelZeroEndMax(index)};
  }

  bool IsFixed(int index) const {
    return x_helper_->StartIsFixed(index) && x_helper_->EndIsFixed(index) &&
           y_helper_->StartIsFixed(index) && y_helper_->EndIsFixed(index);
  }

  std::pair<IntegerValue, IntegerValue> GetBoxSizesMax(int index) const {
    return {x_helper_->SizeMax(index), y_helper_->SizeMax(index)};
  }

  std::pair<IntegerValue, IntegerValue> GetLevelZeroBoxSizesMin(
      int index) const {
    return {x_helper_->LevelZeroSizeMin(index),
            y_helper_->LevelZeroSizeMin(index)};
  }

  bool IsEnforced() const;

  void ResetReason() {
    x_helper_->ResetReason();
    y_helper_->ResetReason();
  }

  void WatchAllBoxes(int id) { propagators_watching_.push_back(id); }

  // Propagate a relationship between two boxes (ie., first must be to the left
  // of the second, etc).
  bool PropagateRelativePosition(
      int first, int second, PairwiseRestriction::PairwiseRestrictionType type);

  // Returns a "fixed size projection" of the item of the item `index`. More
  // precisely, returns item of index `index` with its sizes fixed to their
  // minimum value alongside a bounding box that contains the item.
  RectangleInRange GetItemRangeForSizeMin(int index) const;

  // Returns a {start_min, start_max, end_min, end_max} view of the item of
  // the index `index`.
  ItemWithVariableSize GetItemWithVariableSize(int index) const;

  // If there is no possible placement for the two mandatory boxes (they will
  // necessarily overlap), call this function to report this as a conflict.
  // Returns true.
  bool ReportConflictFromTwoBoxes(int box1, int box2);

  // Reports a conflict due to a (potentially relaxed) infeasible subproblem of
  // the no_overlap_2d constraint. More concretely, this function takes a list
  // of fixed-size rectangles and their placement domains in `ranges` that
  // satisfy:
  //   - the problem of placing all the rectangles in their domain is
  //     infeasible;
  //   - the x and y sizes of each box in `ranges` are smaller or equal than
  //     the corresponding current minimum sizes of the boxes;
  //   - for each range in `ranges`, range.box_index.bounding_box is fully
  //     contained inside GetItemRangeForSizeMin(range.box_index).bounding_box.
  //     In other terms, each element is infeasible in a domain at least as
  //     large as the current one.
  bool ReportConflictFromInfeasibleBoxRanges(
      absl::Span<const RectangleInRange> ranges);

  void AddXSizeMinReason(int index) { x_helper_->AddSizeMinReason(index); }
  void AddYSizeMinReason(int index) { y_helper_->AddSizeMinReason(index); }
  void AddSizeMinReason(int index) {
    AddXSizeMinReason(index);
    AddYSizeMinReason(index);
  }

  // Push the explanation that the left edge of this box is to the right of the
  // vertical line x=lower_bound.
  //
  //  |  =>   |
  //  |  =>  \/
  //  |  =>   +---+
  //  |  =>   |   |
  //  |  =>   +---+
  //  |
  void AddLeftMinReason(int index, IntegerValue lower_bound) {
    x_helper_->AddStartMinReason(index, lower_bound);
  }

  // Push the explanation that the left edge of this box is to the left of the
  // vertical line x=upper_bound.
  //
  //   |       <= |
  //   \/      <= |
  //    +---------|---+
  //    |      <= |   |
  //    |      <= |   |
  //    +---------|---|
  //              |
  void AddLeftMaxReason(int index, IntegerValue upper_bound) {
    x_helper_->AddStartMaxReason(index, upper_bound);
  }

  // Push the explanation that the bottom edge of this box is to the top of the
  // horizontal line y=lower_bound.
  void AddBottomMinReason(int index, IntegerValue lower_bound) {
    y_helper_->AddStartMinReason(index, lower_bound);
  }

  // Push the explanation that the bottom edge of this box is to the bottom of
  // the vertical line y=upper_bound.
  void AddBottomMaxReason(int index, IntegerValue upper_bound) {
    y_helper_->AddStartMaxReason(index, upper_bound);
  }

  void AddPresenceReason(int index) {
    x_helper_->AddPresenceReason(index);
    y_helper_->AddPresenceReason(index);
  }

  bool IncreaseLeftMin(int index, IntegerValue new_lower_bound) {
    x_helper_->ImportReasonsFromOther(*y_helper_);
    return x_helper_->IncreaseStartMin(index, new_lower_bound);
  }

  bool ReportConflict() {
    x_helper_->ImportReasonsFromOther(*y_helper_);
    return x_helper_->ReportConflict();
  }

  int NumBoxes() const { return x_helper_->NumTasks(); }

  bool Propagate() override;

  // Note that the helpers are only valid until the next call to
  // Propagate().
  SchedulingConstraintHelper& x_helper() const { return *x_helper_; }
  SchedulingConstraintHelper& y_helper() const { return *y_helper_; }

  SchedulingDemandHelper& x_demands_helper();
  SchedulingDemandHelper& y_demands_helper();

  const CompactVectorVector<int>& connected_components() const {
    return connected_components_;
  }

  int64_t InProcessingCount() const { return inprocessing_count_; }
  int64_t LastLevelZeroChangeIdx() const {
    return level_zero_bound_change_idx_;
  }

 private:
  void Reset(absl::Span<const Rectangle> fixed_boxes,
             absl::Span<const int> non_fixed_box_indexes);

  CompactVectorVector<int> connected_components_;

  bool axes_are_swapped_;
  std::unique_ptr<SchedulingConstraintHelper> x_helper_;
  std::unique_ptr<SchedulingConstraintHelper> y_helper_;
  std::unique_ptr<SchedulingDemandHelper> x_demands_helper_;
  std::unique_ptr<SchedulingDemandHelper> y_demands_helper_;
  EnforcementHelper& enforcement_helper_;
  EnforcementId enforcement_id_;
  Model* model_;
  GenericLiteralWatcher* watcher_;
  std::vector<int> propagators_watching_;
  int64_t inprocessing_count_ = 0;
  int64_t level_zero_bound_change_idx_ = 0;
};

}  // namespace sat
}  // namespace operations_research

#endif  // ORTOOLS_SAT_NO_OVERLAP_2D_HELPER_H_
