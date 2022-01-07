// Copyright 2010-2021 Google LLC
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
#include <string>
#include <utility>
#include <vector>

#include "ortools/base/int_type.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/cuts.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/linear_constraint_manager.h"
#include "ortools/sat/model.h"
#include "ortools/util/time_limit.h"

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
    const std::vector<IntervalVariable>& intervals,
    const AffineExpression& capacity,
    const std::vector<AffineExpression>& demands,
    const std::vector<LinearExpression>& energies, Model* model);

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
    const std::vector<IntervalVariable>& intervals,
    const AffineExpression& capacity,
    const std::vector<AffineExpression>& demands, Model* model);

// Completion time cuts for the cumulative constraint. It is a simple relaxation
// where we replace a cumulative task with demand k and duration d by a
// no_overlap task with duration d * k / capacity_max.
CutGenerator CreateCumulativeCompletionTimeCutGenerator(
    const std::vector<IntervalVariable>& intervals,
    const AffineExpression& capacity,
    const std::vector<AffineExpression>& demands,
    const std::vector<LinearExpression>& energies, Model* model);

// For a given set of intervals in a cumulative constraint, we detect violated
// mandatory precedences and create a cut for these.
CutGenerator CreateCumulativePrecedenceCutGenerator(
    const std::vector<IntervalVariable>& intervals,
    const AffineExpression& capacity,
    const std::vector<AffineExpression>& demands, Model* model);

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
    const std::vector<IntervalVariable>& intervals, Model* model);

// For a given set of intervals in a no_overlap constraint, we detect violated
// mandatory precedences and create a cut for these.
CutGenerator CreateNoOverlapPrecedenceCutGenerator(
    const std::vector<IntervalVariable>& intervals, Model* model);

// For a given set of intervals in a no_overlap constraint, we detect violated
// area based cuts from Queyranne 93 [see note in the code] and create a cut for
// these.
CutGenerator CreateNoOverlapCompletionTimeCutGenerator(
    const std::vector<IntervalVariable>& intervals, Model* model);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_SCHEDULING_CUTS_H_
