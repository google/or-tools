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

#ifndef OR_TOOLS_SAT_SCHEDULING_CUTS_H_
#define OR_TOOLS_SAT_SCHEDULING_CUTS_H_

#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ortools/sat/cuts.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/intervals.h"

namespace operations_research {
namespace sat {

// For a given set of intervals and demands, we compute the energy of
// each task and make sure their sum fits in the span of the intervals * its
// capacity.
//
// If an interval is optional, it contributes
//    min_demand * min_size * presence_literal
// amount of total energy.
//
// If an interval is performed, we use the linear energy formulation (if
// defined, that is if different from a constant -1), or the McCormick
// relaxation of the product size * demand if not defined.
//
// The maximum energy is capacity * span of intervals at level 0.
CutGenerator CreateCumulativeEnergyCutGenerator(
    SchedulingConstraintHelper* helper, SchedulingDemandHelper* demands_helper,
    const AffineExpression& capacity,
    const std::optional<AffineExpression>& makespan, Model* model);

// For a given set of intervals and demands, we first compute the mandatory part
// of the interval as [start_max , end_min]. We use this to calculate mandatory
// demands for each start_max time points for eligible intervals.
// Since the sum of these mandatory demands must be smaller or equal to the
// capacity, we create a cut representing that.
//
// If an interval is optional, it contributes min_demand * presence_literal
// amount of demand to the mandatory demands sum. So the final cut is generated
// as follows:
//   sum(demands of always present intervals)
//   + sum(presence_literal * min_of_demand) <= capacity.
CutGenerator CreateCumulativeTimeTableCutGenerator(
    SchedulingConstraintHelper* helper, SchedulingDemandHelper* demands_helper,
    const AffineExpression& capacity, Model* model);

// Completion time cuts for the cumulative constraint. It is a simple relaxation
// where we replace a cumulative task with demand k and duration d by a
// no_overlap task with duration d * k / capacity_max.
CutGenerator CreateCumulativeCompletionTimeCutGenerator(
    SchedulingConstraintHelper* helper, SchedulingDemandHelper* demands_helper,
    const AffineExpression& capacity, Model* model);

// For a given set of intervals in a cumulative constraint, we detect violated
// mandatory precedences and create a cut for these.
CutGenerator CreateCumulativePrecedenceCutGenerator(
    SchedulingConstraintHelper* helper, SchedulingDemandHelper* demands_helper,
    const AffineExpression& capacity, Model* model);

// Completion time cuts for the no_overlap_2d constraint. It actually generates
// the completion time cumulative cuts in both axis.
CutGenerator CreateNoOverlap2dCompletionTimeCutGenerator(
    const std::vector<IntervalVariable>& x_intervals,
    const std::vector<IntervalVariable>& y_intervals, Model* model);

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
    const std::vector<IntervalVariable>& x_intervals,
    const std::vector<IntervalVariable>& y_intervals, Model* model);

// For a given set of intervals, we first compute the min and max of all
// intervals. Then we create a cut that indicates that all intervals must fit
// in that span.
//
// If an interval is optional, it contributes min_size * presence_literal
// amount of demand to the mandatory demands sum. So the final cut is generated
// as follows:
//   sum(sizes of always present intervals)
//   + sum(presence_literal * min_of_size) <= span of all intervals.
CutGenerator CreateNoOverlapEnergyCutGenerator(
    SchedulingConstraintHelper* helper,
    const std::optional<AffineExpression>& makespan, Model* model);

// For a given set of intervals in a no_overlap constraint, we detect violated
// mandatory precedences and create a cut for these.
CutGenerator CreateNoOverlapPrecedenceCutGenerator(
    SchedulingConstraintHelper* helper, Model* model);

// For a given set of intervals in a no_overlap constraint, we detect violated
// area based cuts from Queyranne 93 [see note in the code] and create a cut for
// these.
CutGenerator CreateNoOverlapCompletionTimeCutGenerator(
    SchedulingConstraintHelper* helper, Model* model);

// Internal methods and data structures, useful for testing.

// Base event type for scheduling cuts.
struct BaseEvent {
  BaseEvent(int t, SchedulingConstraintHelper* x_helper);

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
struct CtEvent : BaseEvent {
  CtEvent(int t, SchedulingConstraintHelper* x_helper);

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

// Computes the minimum sum of the end min and the minimum sum of the end min
// weighted by y_size_min of all events. It returns false if no permatutation is
// valid w.r.t. the range of x_start.
//
// Note that this is an exhaustive algorithm, so the number of events should be
// small, like <= 10. They should also starts in index order.
//
// Optim: If both sums are proven <= to the corresponding threshold, we abort.
struct PermutableEvent {
  PermutableEvent(int i, CtEvent e)
      : index(i),
        x_start_min(e.x_start_min),
        x_start_max(e.x_start_max),
        x_size_min(e.x_size_min),
        y_size_min(e.y_size_min) {}
  bool operator<(const PermutableEvent& o) const { return index < o.index; }

  int index;  // for < to be used by std::next_permutation().
  IntegerValue x_start_min;
  IntegerValue x_start_max;
  IntegerValue x_size_min;
  IntegerValue y_size_min;
};
bool ComputeMinSumOfWeightedEndMins(std::vector<PermutableEvent>& events,
                                    IntegerValue capacity_max,
                                    IntegerValue& min_sum_of_end_mins,
                                    IntegerValue& min_sum_of_weighted_end_mins,
                                    IntegerValue unweighted_threshold,
                                    IntegerValue weighted_threshold);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_SCHEDULING_CUTS_H_
