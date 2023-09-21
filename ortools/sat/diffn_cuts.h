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

#ifndef OR_TOOLS_SAT_DIFFN_CUTS_H_
#define OR_TOOLS_SAT_DIFFN_CUTS_H_

#include <string>
#include <vector>

#include "ortools/sat/cuts.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/model.h"

namespace operations_research {
namespace sat {

// Completion time cuts for the no_overlap_2d constraint. It actually generates
// the completion time cumulative cuts in both axis.
CutGenerator CreateNoOverlap2dCompletionTimeCutGenerator(
    SchedulingConstraintHelper* x_helper, SchedulingConstraintHelper* y_helper,
    Model* model);

// Energetic cuts for the no_overlap_2d constraint.
//
// For a given set of rectangles, we compute the area of each rectangle
// and make sure their sum is less than the area of the bounding interval.
//
// If an interval is optional, it contributes
//    min_size_x * min_size_y * presence_literal
// amount of total area.
//
// If an interval is performed, we use the linear area formulation (if
// possible), or the McCormick relaxation of the size_x * size_y.
//
// The maximum area is the area of the bounding rectangle of each intervals
// at level 0.
CutGenerator CreateNoOverlap2dEnergyCutGenerator(
    SchedulingConstraintHelper* x_helper, SchedulingConstraintHelper* y_helper,
    SchedulingDemandHelper* x_demands_helper,
    SchedulingDemandHelper* y_demands_helper,
    const std::vector<std::vector<LiteralValueValue>>& energies, Model* model);

// Internal methods and data structures, useful for testing.

// Base event type for scheduling cuts.
struct DiffnBaseEvent {
  DiffnBaseEvent(int t, SchedulingConstraintHelper* x_helper);

  // Cache of the intervals bound on the x direction.
  IntegerValue x_start_min;
  IntegerValue x_start_max;
  IntegerValue x_end_min;
  IntegerValue x_end_max;
  IntegerValue x_size_min;
  // Useful for no_overlap_2d or cumulative.
  IntegerValue y_min = IntegerValue(0);
  IntegerValue y_max = IntegerValue(0);
  IntegerValue y_size_min;

  // The energy min of this event.
  IntegerValue energy_min;

  // If non empty, a decomposed view of the energy of this event.
  // First value in each pair is x_size, second is y_size.
  std::vector<LiteralValueValue> decomposed_energy;
};

// Stores the event for a rectangle along the two axis x and y.
//   For a no_overlap constraint, y is always of size 1 between 0 and 1.
//   For a cumulative constraint, y is the demand that must be between 0 and
//       capacity_max.
//   For a no_overlap_2d constraint, y the other dimension of the rect.
struct DiffnCtEvent : DiffnBaseEvent {
  DiffnCtEvent(int t, SchedulingConstraintHelper* x_helper);

  // The lp value of the end of the x interval.
  AffineExpression x_end;
  double x_lp_end;

  // Indicates if the events used the optional energy information from the
  // model.
  bool use_energy = false;

  // Indicates if the cut is lifted, that is if it includes tasks that are not
  // strictly contained in the current time window.
  bool lifted = false;

  // If we know that the size on y is fixed, we can use some heuristic to
  // compute the maximum subset sums under the capacity and use that instead
  // of the full capacity.
  bool y_size_is_fixed = false;

  std::string DebugString() const;
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_DIFFN_CUTS_H_
