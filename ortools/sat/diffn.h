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

#ifndef OR_TOOLS_SAT_DIFFN_H_
#define OR_TOOLS_SAT_DIFFN_H_

#include <functional>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/types/span.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"
#include "ortools/sat/diffn_util.h"
#include "ortools/sat/disjunctive.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/util.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {

// Non overlapping rectangles.
class NonOverlappingRectanglesDisjunctivePropagator
    : public PropagatorInterface {
 public:
  // The strict parameters indicates how to place zero width or zero height
  // boxes. If strict is true, these boxes must not 'cross' another box, and are
  // pushed by the other boxes.
  // The slow_propagators select which disjunctive algorithms to propagate.
  NonOverlappingRectanglesDisjunctivePropagator(bool strict,
                                                SchedulingConstraintHelper* x,
                                                SchedulingConstraintHelper* y,
                                                Model* model);
  ~NonOverlappingRectanglesDisjunctivePropagator() override;

  bool Propagate() final;
  void Register(int fast_priority, int slow_priority);

 private:
  bool PropagateTwoBoxes();
  bool FindBoxesThatMustOverlapAHorizontalLineAndPropagate(
      bool fast_propagation, const SchedulingConstraintHelper& x,
      SchedulingConstraintHelper* y);

  SchedulingConstraintHelper& global_x_;
  SchedulingConstraintHelper& global_y_;
  SchedulingConstraintHelper x_;
  const bool strict_;

  GenericLiteralWatcher* watcher_;
  int fast_id_;  // Propagator id of the "fast" version.

  std::vector<IndexedInterval> indexed_intervals_;
  std::vector<std::vector<int>> events_overlapping_boxes_;

  absl::flat_hash_set<absl::Span<int>> reduced_overlapping_boxes_;
  std::vector<absl::Span<int>> boxes_to_propagate_;
  std::vector<absl::Span<int>> disjoint_boxes_;

  DisjunctiveOverloadChecker overload_checker_;
  DisjunctiveDetectablePrecedences forward_detectable_precedences_;
  DisjunctiveDetectablePrecedences backward_detectable_precedences_;
  DisjunctiveNotLast forward_not_last_;
  DisjunctiveNotLast backward_not_last_;
  DisjunctiveEdgeFinding forward_edge_finding_;
  DisjunctiveEdgeFinding backward_edge_finding_;

  NonOverlappingRectanglesDisjunctivePropagator(
      const NonOverlappingRectanglesDisjunctivePropagator&) = delete;
  NonOverlappingRectanglesDisjunctivePropagator& operator=(
      const NonOverlappingRectanglesDisjunctivePropagator&) = delete;
};

// Add a cumulative relaxation. That is, on one dimension, it does not enforce
// the rectangle aspect, allowing vertical slices to move freely.
void AddDiffnCumulativeRelationOnX(SchedulingConstraintHelper* x,
                                   SchedulingConstraintHelper* y, Model* model);

// Enforces that the boxes with corners in (x, y), (x + dx, y), (x, y + dy)
// and (x + dx, y + dy) do not overlap.
// If strict is true, and if one box has a zero dimension, it still cannot
// intersect another box.
inline std::function<void(Model*)> NonOverlappingRectangles(
    const std::vector<IntervalVariable>& x,
    const std::vector<IntervalVariable>& y, bool is_strict) {
  return [=](Model* model) {
    SchedulingConstraintHelper* x_helper =
        new SchedulingConstraintHelper(x, model);
    SchedulingConstraintHelper* y_helper =
        new SchedulingConstraintHelper(y, model);
    model->TakeOwnership(x_helper);
    model->TakeOwnership(y_helper);

    NonOverlappingRectanglesDisjunctivePropagator* constraint =
        new NonOverlappingRectanglesDisjunctivePropagator(is_strict, x_helper,
                                                          y_helper, model);
    constraint->Register(/*fast_priority=*/3, /*slow_priority=*/4);
    model->TakeOwnership(constraint);

    const SatParameters* params = model->GetOrCreate<SatParameters>();
    const bool add_cumulative_relaxation =
        params->use_timetabling_in_no_overlap_2d() ||
        params->use_energetic_reasoning_in_no_overlap_2d();

    if (add_cumulative_relaxation) {
      // We must first check if the cumulative relaxation is possible.
      bool some_boxes_are_only_optional_on_x = false;
      bool some_boxes_are_only_optional_on_y = false;
      for (int i = 0; i < x.size(); ++i) {
        if (x_helper->IsOptional(i) && y_helper->IsOptional(i) &&
            x_helper->PresenceLiteral(i) != y_helper->PresenceLiteral(i)) {
          // Abort as the task would be conditioned by two literals.
          return;
        }
        if (x_helper->IsOptional(i) && !y_helper->IsOptional(i)) {
          // We cannot use x_size as the demand of the cumulative based on
          // the y_intervals.
          some_boxes_are_only_optional_on_x = true;
        }
        if (y_helper->IsOptional(i) && !x_helper->IsOptional(i)) {
          // We cannot use y_size as the demand of the cumulative based on
          // the y_intervals.
          some_boxes_are_only_optional_on_y = true;
        }
      }
      if (!some_boxes_are_only_optional_on_y) {
        AddDiffnCumulativeRelationOnX(x_helper, y_helper, model);
      }
      if (!some_boxes_are_only_optional_on_x) {
        AddDiffnCumulativeRelationOnX(y_helper, x_helper, model);
      }
    }
  };
}

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_DIFFN_H_
