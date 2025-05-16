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

#ifndef OR_TOOLS_SAT_SCHEDULING_CUTS_H_
#define OR_TOOLS_SAT_SCHEDULING_CUTS_H_

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/types/span.h"
#include "ortools/sat/cuts.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/model.h"
#include "ortools/sat/scheduling_helpers.h"
#include "ortools/sat/util.h"

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

// Stores the event for a task (interval, demand).
// For a no_overlap constraint, demand is always between 0 and 1.
// For a cumulative constraint, demand must be between 0 and capacity_max.
struct CompletionTimeEvent {
  CompletionTimeEvent(int t, SchedulingConstraintHelper* x_helper,
                      SchedulingDemandHelper* demands_helper);

  // The index of the task in the helper.
  int task_index;

  // Cache of the bounds of the interval.
  IntegerValue start_min;
  IntegerValue start_max;
  IntegerValue end_min;
  IntegerValue end_max;
  IntegerValue size_min;

  // Start and end affine expressions and lp value of the end of the interval.
  AffineExpression start;
  AffineExpression end;
  double lp_end = 0.0;

  // Cache of the bounds of the demand.
  IntegerValue demand_min;

  // If we know that the size on y is fixed, we can use some heuristic to
  // compute the maximum subset sums under the capacity and use that instead
  // of the full capacity.
  bool demand_is_fixed = false;

  // The energy min of this event.
  IntegerValue energy_min;

  // If non empty, a decomposed view of the energy of this event.
  // First value in each pair is x_size, second is y_size.
  std::vector<LiteralValueValue> decomposed_energy;

  // Indicates if the events used the optional energy information from the
  // model.
  bool use_decomposed_energy_min = false;

  // Indicates if the cut is lifted, that is if it includes tasks that are not
  // strictly contained in the current time window.
  bool lifted = false;

  std::string DebugString() const;
};

class CtExhaustiveHelper {
 public:
  int max_task_index() const { return max_task_index_; }
  const CompactVectorVector<int>& predecessors() const { return predecessors_; }

  // Temporary data.
  std::vector<std::pair<IntegerValue, IntegerValue>> profile_;
  std::vector<std::pair<IntegerValue, IntegerValue>> new_profile_;
  std::vector<IntegerValue> assigned_ends_;

  // Collect precedences, set max_task_index.
  // TODO(user): Do some transitive closure.
  void Init(absl::Span<const CompletionTimeEvent> events, Model* model);

  bool PermutationIsCompatibleWithPrecedences(
      absl::Span<const CompletionTimeEvent> events,
      absl::Span<const int> permutation);

 private:
  CompactVectorVector<int> predecessors_;
  int max_task_index_ = 0;
  std::vector<bool> visited_;
};

// Computes the minimum sum of the end min and the minimum sum of the end min
// weighted by weight of all events. It returns false if no permutation is
// valid w.r.t. the range of starts.
//
// Note that this is an exhaustive algorithm, so the number of events should be
// small, like <= 10. They should also starts in index order.
//
// Optim: If both sums are proven <= to the corresponding threshold, we abort.
bool ComputeMinSumOfWeightedEndMins(
    absl::Span<const CompletionTimeEvent> events, IntegerValue capacity_max,
    double unweighted_threshold, double weighted_threshold,
    CtExhaustiveHelper& helper, double& min_sum_of_ends,
    double& min_sum_of_weighted_ends, bool& cut_use_precedences);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_SCHEDULING_CUTS_H_
