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

// Non overlapping rectangles.
class NonOverlappingRectanglesBasePropagator : public PropagatorInterface {
 public:
  // The strict parameters indicates how to place zero width or zero height
  // boxes. If strict is true, these boxes must not 'cross' another box, and are
  // pushed by the other boxes.
  NonOverlappingRectanglesBasePropagator(const std::vector<IntervalVariable>& x,
                                         const std::vector<IntervalVariable>& y,
                                         bool strict, Model* model,
                                         IntegerTrail* integer_trail);
  ~NonOverlappingRectanglesBasePropagator() override;

 protected:
  void FillCachedAreas();
  IntegerValue FindCanonicalValue(IntegerValue lb, IntegerValue ub);
  bool FindBoxesThatMustOverlapAHorizontalLineAndPropagate(
      SchedulingConstraintHelper* x_dim, SchedulingConstraintHelper* y_dim,
      std::function<bool(const std::vector<int>& boxes)> inner_propagate);
  std::vector<std::vector<int>> SplitDisjointBoxes(
      std::vector<int> boxes, SchedulingConstraintHelper* x_dim);

  const int num_boxes_;
  SchedulingConstraintHelper x_;
  SchedulingConstraintHelper y_;
  const bool strict_;
  IntegerTrail* integer_trail_;
  std::vector<IntegerValue> cached_areas_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NonOverlappingRectanglesBasePropagator);
};

// Propagates using a box energy reasoning.
class NonOverlappingRectanglesEnergyPropagator
    : public NonOverlappingRectanglesBasePropagator {
 public:
  // The strict parameters indicates how to place zero width or zero height
  // boxes. If strict is true, these boxes must not 'cross' another box, and are
  // pushed by the other boxes.
  NonOverlappingRectanglesEnergyPropagator(
      const std::vector<IntervalVariable>& x,
      const std::vector<IntervalVariable>& y, bool strict, Model* model,
      IntegerTrail* integer_trail);
  ~NonOverlappingRectanglesEnergyPropagator() override;

  bool Propagate() final;
  void RegisterWith(GenericLiteralWatcher* watcher);

 private:
  void SortNeighbors(int box, const std::vector<int>& local_boxes);
  bool FailWhenEnergyIsTooLarge(int box, const std::vector<int>& local_boxes);

  std::vector<int> neighbors_;
  std::vector<IntegerValue> cached_distance_to_bounding_box_;

  DISALLOW_COPY_AND_ASSIGN(NonOverlappingRectanglesEnergyPropagator);
};

// Embeds the overload checker and the detectable precedences propagators from
// the disjunctive constraint.
class NonOverlappingRectanglesFastPropagator
    : public NonOverlappingRectanglesBasePropagator {
 public:
  // The strict parameters indicates how to place zero width or zero height
  // boxes. If strict is true, these boxes must not 'cross' another box, and are
  // pushed by the other boxes.
  NonOverlappingRectanglesFastPropagator(const std::vector<IntervalVariable>& x,
                                         const std::vector<IntervalVariable>& y,
                                         bool strict, Model* model,
                                         IntegerTrail* integer_trail);
  ~NonOverlappingRectanglesFastPropagator() override;

  bool Propagate() final;
  void RegisterWith(GenericLiteralWatcher* watcher);

 private:
  bool PropagateTwoBoxes(int b1, int b2, SchedulingConstraintHelper* x_dim);

  DisjunctiveOverloadChecker x_overload_checker_;
  DisjunctiveOverloadChecker y_overload_checker_;
  DisjunctiveDetectablePrecedences forward_x_detectable_precedences_;
  DisjunctiveDetectablePrecedences backward_x_detectable_precedences_;
  DisjunctiveDetectablePrecedences forward_y_detectable_precedences_;
  DisjunctiveDetectablePrecedences backward_y_detectable_precedences_;

  DISALLOW_COPY_AND_ASSIGN(NonOverlappingRectanglesFastPropagator);
};

// Embeds the not last and edge finder propagators from the disjunctive
// constraint.
class NonOverlappingRectanglesSlowPropagator
    : public NonOverlappingRectanglesBasePropagator {
 public:
  // The strict parameters indicates how to place zero width or zero height
  // boxes. If strict is true, these boxes must not 'cross' another box, and are
  // pushed by the other boxes.
  NonOverlappingRectanglesSlowPropagator(const std::vector<IntervalVariable>& x,
                                         const std::vector<IntervalVariable>& y,
                                         bool strict, Model* model,
                                         IntegerTrail* integer_trail);
  ~NonOverlappingRectanglesSlowPropagator() override;

  bool Propagate() final;
  void RegisterWith(GenericLiteralWatcher* watcher);

 private:
  DisjunctiveNotLast forward_x_not_last_;
  DisjunctiveNotLast backward_x_not_last_;
  DisjunctiveNotLast forward_y_not_last_;
  DisjunctiveNotLast backward_y_not_last_;
  DisjunctiveEdgeFinding forward_x_edge_finding_;
  DisjunctiveEdgeFinding backward_x_edge_finding_;
  DisjunctiveEdgeFinding forward_y_edge_finding_;
  DisjunctiveEdgeFinding backward_y_edge_finding_;

  DISALLOW_COPY_AND_ASSIGN(NonOverlappingRectanglesSlowPropagator);
};

// Add a cumulative relaxation. That is, on one direction, it does not enforce
// the rectangle aspect, allowing vertical slices to move freely.
void AddCumulativeRelaxation(const std::vector<IntervalVariable>& x,
                             const std::vector<IntervalVariable>& y,
                             Model* model);

// Enforces that the boxes with corners in (x, y), (x + dx, y), (x, y + dy)
// and (x + dx, y + dy) do not overlap.
// If one box has a zero dimension, then it can be placed anywhere.
inline std::function<void(Model*)> NonOverlappingRectangles(
    const std::vector<IntervalVariable>& x,
    const std::vector<IntervalVariable>& y) {
  return [=](Model* model) {
    NonOverlappingRectanglesEnergyPropagator* energy_constraint =
        new NonOverlappingRectanglesEnergyPropagator(
            x, y, false, model, model->GetOrCreate<IntegerTrail>());
    energy_constraint->RegisterWith(
        model->GetOrCreate<GenericLiteralWatcher>());
    model->TakeOwnership(energy_constraint);

    NonOverlappingRectanglesFastPropagator* fast_constraint =
        new NonOverlappingRectanglesFastPropagator(
            x, y, false, model, model->GetOrCreate<IntegerTrail>());
    fast_constraint->RegisterWith(model->GetOrCreate<GenericLiteralWatcher>());
    model->TakeOwnership(fast_constraint);

    NonOverlappingRectanglesSlowPropagator* slow_constraint =
        new NonOverlappingRectanglesSlowPropagator(
            x, y, false, model, model->GetOrCreate<IntegerTrail>());
    slow_constraint->RegisterWith(model->GetOrCreate<GenericLiteralWatcher>());

    model->TakeOwnership(slow_constraint);

    AddCumulativeRelaxation(x, y, model);
    AddCumulativeRelaxation(y, x, model);
  };
}

// Enforces that the boxes with corners in (x, y), (x + dx, y), (x, y + dy)
// and (x + dx, y + dy) do not overlap.
// If one box has a zero dimension, it still cannot intersect another box.
inline std::function<void(Model*)> StrictNonOverlappingRectangles(
    const std::vector<IntervalVariable>& x,
    const std::vector<IntervalVariable>& y) {
  return [=](Model* model) {
    NonOverlappingRectanglesEnergyPropagator* energy_constraint =
        new NonOverlappingRectanglesEnergyPropagator(
            x, y, true, model, model->GetOrCreate<IntegerTrail>());
    energy_constraint->RegisterWith(
        model->GetOrCreate<GenericLiteralWatcher>());
    model->TakeOwnership(energy_constraint);

    NonOverlappingRectanglesFastPropagator* fast_constraint =
        new NonOverlappingRectanglesFastPropagator(
            x, y, true, model, model->GetOrCreate<IntegerTrail>());
    fast_constraint->RegisterWith(model->GetOrCreate<GenericLiteralWatcher>());
    model->TakeOwnership(fast_constraint);

    NonOverlappingRectanglesSlowPropagator* slow_constraint =
        new NonOverlappingRectanglesSlowPropagator(
            x, y, true, model, model->GetOrCreate<IntegerTrail>());
    slow_constraint->RegisterWith(model->GetOrCreate<GenericLiteralWatcher>());
    model->TakeOwnership(slow_constraint);

    AddCumulativeRelaxation(x, y, model);
    AddCumulativeRelaxation(y, x, model);
  };
}

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_DIFFN_H_
