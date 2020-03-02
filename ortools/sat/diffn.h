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

#ifndef OR_TOOLS_SAT_DIFFN_H_
#define OR_TOOLS_SAT_DIFFN_H_

#include <vector>

#include "ortools/base/int_type.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"
#include "ortools/sat/disjunctive.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"

namespace operations_research {
namespace sat {

// Propagates using a box energy reasoning.
class NonOverlappingRectanglesEnergyPropagator : public PropagatorInterface {
 public:
  // The strict parameters indicates how to place zero width or zero height
  // boxes. If strict is true, these boxes must not 'cross' another box, and are
  // pushed by the other boxes.
  NonOverlappingRectanglesEnergyPropagator(SchedulingConstraintHelper* x,
                                           SchedulingConstraintHelper* y)
      : x_(*x), y_(*y) {}
  ~NonOverlappingRectanglesEnergyPropagator() override;

  bool Propagate() final;
  int RegisterWith(GenericLiteralWatcher* watcher);

 private:
  void SortBoxesIntoNeighbors(int box, absl::Span<const int> local_boxes,
                              IntegerValue total_sum_of_areas);
  bool FailWhenEnergyIsTooLarge(int box, absl::Span<const int> local_boxes,
                                IntegerValue total_sum_of_areas);

  SchedulingConstraintHelper& x_;
  SchedulingConstraintHelper& y_;

  std::vector<absl::Span<int>> x_split_;
  std::vector<absl::Span<int>> y_split_;

  std::vector<int> active_boxes_;
  std::vector<IntegerValue> cached_areas_;

  struct Dimension {
    IntegerValue x_min;
    IntegerValue x_max;
    IntegerValue y_min;
    IntegerValue y_max;

    void TakeUnionWith(const Dimension& other) {
      x_min = std::min(x_min, other.x_min);
      y_min = std::min(y_min, other.y_min);
      x_max = std::max(x_max, other.x_max);
      y_max = std::max(y_max, other.y_max);
    }
  };
  std::vector<Dimension> cached_dimensions_;

  struct Neighbor {
    int box;
    IntegerValue distance_to_bounding_box;
    bool operator<(const Neighbor& o) const {
      return distance_to_bounding_box < o.distance_to_bounding_box;
    }
  };
  std::vector<Neighbor> neighbors_;

  NonOverlappingRectanglesEnergyPropagator(
      const NonOverlappingRectanglesEnergyPropagator&) = delete;
  NonOverlappingRectanglesEnergyPropagator& operator=(
      const NonOverlappingRectanglesEnergyPropagator&) = delete;
};

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
      const SchedulingConstraintHelper& x, const SchedulingConstraintHelper& y,
      std::function<bool()> inner_propagate);

  SchedulingConstraintHelper& global_x_;
  SchedulingConstraintHelper& global_y_;
  SchedulingConstraintHelper x_;
  SchedulingConstraintHelper y_;
  const bool strict_;

  GenericLiteralWatcher* watcher_;
  int fast_id_;  // Propagator id of the "fast" version.

  std::vector<int> active_boxes_;
  std::vector<IntegerValue> events_time_;
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
void AddCumulativeRelaxation(const std::vector<IntervalVariable>& x_intervals,
                             SchedulingConstraintHelper* x,
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

    NonOverlappingRectanglesEnergyPropagator* energy_constraint =
        new NonOverlappingRectanglesEnergyPropagator(x_helper, y_helper);
    GenericLiteralWatcher* const watcher =
        model->GetOrCreate<GenericLiteralWatcher>();
    watcher->SetPropagatorPriority(energy_constraint->RegisterWith(watcher), 3);
    model->TakeOwnership(energy_constraint);

    NonOverlappingRectanglesDisjunctivePropagator* constraint =
        new NonOverlappingRectanglesDisjunctivePropagator(is_strict, x_helper,
                                                          y_helper, model);
    constraint->Register(/*fast_priority=*/3, /*slow_priority=*/4);
    model->TakeOwnership(constraint);

    AddCumulativeRelaxation(x, x_helper, y_helper, model);
    AddCumulativeRelaxation(y, y_helper, x_helper, model);
  };
}

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_DIFFN_H_
