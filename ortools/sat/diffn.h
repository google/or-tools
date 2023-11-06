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
#include "ortools/sat/diffn_util.h"
#include "ortools/sat/disjunctive.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"

namespace operations_research {
namespace sat {

// Enforces that the boxes with corners in (x, y), (x + dx, y), (x, y + dy)
// and (x + dx, y + dy) do not overlap.
void AddNonOverlappingRectangles(const std::vector<IntervalVariable>& x,
                                 const std::vector<IntervalVariable>& y,
                                 Model* model);

// Non overlapping rectangles. This includes box with zero-areas.
// The following is forbidden:
//   - a point box inside a box with a non zero area
//   - a line box overlapping a box with a non zero area
//   - one vertical line box crossing an horizontal line box.
class NonOverlappingRectanglesDisjunctivePropagator
    : public PropagatorInterface {
 public:
  // The slow_propagators select which disjunctive algorithms to propagate.
  NonOverlappingRectanglesDisjunctivePropagator(SchedulingConstraintHelper* x,
                                                SchedulingConstraintHelper* y,
                                                Model* model);
  ~NonOverlappingRectanglesDisjunctivePropagator() override;

  bool Propagate() final;
  void Register(int fast_priority, int slow_priority);

 private:
  bool PropagateOnXWhenOnlyTwoBoxes();
  // If two boxes must overlap but do not have a mandatory line/column that
  // crosses both of them, then the code above do not see it. So we manually
  // propagate this case.
  // This also propagates boxes with null area against other boxes (with a non
  // zero area, and with a zero area in the other dimension).
  bool PropagateAllPairsOfBoxes();
  bool PropagateTwoBoxes(int box1, int box2);
  bool FindBoxesThatMustOverlapAHorizontalLineAndPropagate(
      bool fast_propagation, const SchedulingConstraintHelper& x,
      SchedulingConstraintHelper* y);

  SchedulingConstraintHelper& global_x_;
  SchedulingConstraintHelper& global_y_;
  SchedulingConstraintHelper x_;

  GenericLiteralWatcher* watcher_;
  int fast_id_;  // Propagator id of the "fast" version.

  std::vector<IndexedInterval> indexed_boxes_;
  std::vector<std::vector<int>> events_overlapping_boxes_;

  absl::flat_hash_set<absl::Span<int>> reduced_overlapping_boxes_;
  std::vector<absl::Span<int>> boxes_to_propagate_;
  std::vector<absl::Span<int>> disjoint_boxes_;
  std::vector<int> horizontal_zero_area_boxes_;
  std::vector<int> vertical_zero_area_boxes_;
  std::vector<int> point_zero_area_boxes_;
  std::vector<int> non_zero_area_boxes_;

  DisjunctiveOverloadChecker overload_checker_;
  DisjunctiveDetectablePrecedences forward_detectable_precedences_;
  DisjunctiveDetectablePrecedences backward_detectable_precedences_;
  DisjunctiveNotLast forward_not_last_;
  DisjunctiveNotLast backward_not_last_;
  DisjunctiveEdgeFinding forward_edge_finding_;
  DisjunctiveEdgeFinding backward_edge_finding_;

  bool has_zero_area_boxes_ = false;
  const bool pairwise_propagation_ = false;

  NonOverlappingRectanglesDisjunctivePropagator(
      const NonOverlappingRectanglesDisjunctivePropagator&) = delete;
  NonOverlappingRectanglesDisjunctivePropagator& operator=(
      const NonOverlappingRectanglesDisjunctivePropagator&) = delete;
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_DIFFN_H_
