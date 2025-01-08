// Copyright 2010-2024 Google LLC
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

#include <cstdint>
#include <optional>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/types/span.h"
#include "ortools/sat/2d_orthogonal_packing.h"
#include "ortools/sat/diffn_util.h"
#include "ortools/sat/disjunctive.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/synchronization.h"
#include "ortools/sat/util.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

// Propagates using a box energy reasoning.
class NonOverlappingRectanglesEnergyPropagator : public PropagatorInterface {
 public:
  NonOverlappingRectanglesEnergyPropagator(SchedulingConstraintHelper* x,
                                           SchedulingConstraintHelper* y,
                                           Model* model)
      : x_(*x),
        y_(*y),
        random_(model->GetOrCreate<ModelRandomGenerator>()),
        shared_stats_(model->GetOrCreate<SharedStatistics>()),
        orthogonal_packing_checker_(*random_, shared_stats_) {}

  ~NonOverlappingRectanglesEnergyPropagator() override;

  bool Propagate() final;
  int RegisterWith(GenericLiteralWatcher* watcher);

 private:
  struct Conflict {
    // The Orthogonal Packing subproblem we used.
    std::vector<RectangleInRange> items_for_opp;
    Rectangle rectangle_with_too_much_energy;
    OrthogonalPackingResult opp_result;
  };
  std::optional<Conflict> FindConflict(
      std::vector<RectangleInRange> active_box_ranges);

  std::vector<RectangleInRange> GeneralizeExplanation(const Conflict& conflict);

  bool BuildAndReportEnergyTooLarge(absl::Span<const RectangleInRange> ranges);

  SchedulingConstraintHelper& x_;
  SchedulingConstraintHelper& y_;
  ModelRandomGenerator* random_;
  SharedStatistics* shared_stats_;
  OrthogonalPackingInfeasibilityDetector orthogonal_packing_checker_;

  int64_t num_calls_ = 0;
  int64_t num_conflicts_ = 0;
  int64_t num_conflicts_two_boxes_ = 0;
  int64_t num_refined_conflicts_ = 0;
  int64_t num_conflicts_with_slack_ = 0;

  NonOverlappingRectanglesEnergyPropagator(
      const NonOverlappingRectanglesEnergyPropagator&) = delete;
  NonOverlappingRectanglesEnergyPropagator& operator=(
      const NonOverlappingRectanglesEnergyPropagator&) = delete;
};

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
  bool FindBoxesThatMustOverlapAHorizontalLineAndPropagate(
      bool fast_propagation, SchedulingConstraintHelper* x,
      SchedulingConstraintHelper* y);

  SchedulingConstraintHelper& global_x_;
  SchedulingConstraintHelper& global_y_;
  SchedulingConstraintHelper x_;

  GenericLiteralWatcher* watcher_;
  TimeLimit* time_limit_;
  int fast_id_;  // Propagator id of the "fast" version.

  // Temporary data.
  std::vector<IndexedInterval> indexed_boxes_;
  std::vector<Rectangle> rectangles_;
  std::vector<int> order_;
  CompactVectorVector<int> events_overlapping_boxes_;

  // List of box that are fully fixed in the current dive, and for which we
  // know they are no conflict between them.
  bool rev_is_in_dive_ = false;
  Bitset64<int> already_checked_fixed_boxes_;

  absl::flat_hash_set<absl::Span<const int>> reduced_overlapping_boxes_;
  std::vector<absl::Span<const int>> boxes_to_propagate_;
  std::vector<absl::Span<const int>> disjoint_boxes_;
  std::vector<int> non_zero_area_boxes_;

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

// Propagator that compares the boxes pairwise.
class RectanglePairwisePropagator : public PropagatorInterface {
 public:
  RectanglePairwisePropagator(SchedulingConstraintHelper* x,
                              SchedulingConstraintHelper* y, Model* model)
      : global_x_(*x),
        global_y_(*y),
        shared_stats_(model->GetOrCreate<SharedStatistics>()),
        params_(model->GetOrCreate<SatParameters>()) {}

  ~RectanglePairwisePropagator() override;

  bool Propagate() final;
  int RegisterWith(GenericLiteralWatcher* watcher);

 private:
  RectanglePairwisePropagator(const RectanglePairwisePropagator&) = delete;
  RectanglePairwisePropagator& operator=(const RectanglePairwisePropagator&) =
      delete;

  // Return false if a conflict is found.
  bool FindRestrictionsAndPropagateConflict(
      absl::Span<const ItemWithVariableSize> items,
      std::vector<PairwiseRestriction>* restrictions);

  bool FindRestrictionsAndPropagateConflict(
      absl::Span<const ItemWithVariableSize> items1,
      absl::Span<const ItemWithVariableSize> items2,
      std::vector<PairwiseRestriction>* restrictions);

  bool PropagateTwoBoxes(const PairwiseRestriction& restriction);

  SchedulingConstraintHelper& global_x_;
  SchedulingConstraintHelper& global_y_;
  SharedStatistics* shared_stats_;
  const SatParameters* params_;

  int64_t num_calls_ = 0;
  int64_t num_pairwise_conflicts_ = 0;
  int64_t num_pairwise_propagations_ = 0;

  std::vector<ItemWithVariableSize> non_zero_area_boxes_;
  std::vector<ItemWithVariableSize> horizontal_zero_area_boxes_;
  std::vector<ItemWithVariableSize> vertical_zero_area_boxes_;
  std::vector<ItemWithVariableSize> point_zero_area_boxes_;
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_DIFFN_H_
