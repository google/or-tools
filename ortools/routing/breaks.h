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

#ifndef ORTOOLS_ROUTING_BREAKS_H_
#define ORTOOLS_ROUTING_BREAKS_H_

#include <cstdint>
#include <utility>
#include <vector>

#include "absl/types/span.h"
#include "ortools/routing/filter_committables.h"

namespace operations_research::routing {

class BreakPropagator {
 public:
  explicit BreakPropagator(int num_nodes);

  // Result of a propagation: kInfeasible means some infeasibility was found,
  // kChanged means that the propagation tightened the bounds of some intervals,
  // kUnchanged means that the propagation did not change anything.
  enum class PropagationResult { kInfeasible, kChanged, kUnchanged };
  // TODO(user): when the OSS version is at C++20, replace this by
  // using enum PropagationResult;
  static constexpr PropagationResult kInfeasible =
      PropagationResult::kInfeasible;
  static constexpr PropagationResult kChanged = PropagationResult::kChanged;
  static constexpr PropagationResult kUnchanged = PropagationResult::kUnchanged;

  // Applies fast propagations, O(log |path|) per break, to the given path.
  PropagationResult FastPropagations(int path,
                                     DimensionValues& dimension_values,
                                     const PrePostVisitValues& visits);
  // Propagates interbreak rules on a given path, with a covering reasoning.
  // Each interbreak is a pair (interbreak_limit, min_break_duration).
  PropagationResult PropagateInterbreak(
      int path, DimensionValues& dimension,
      absl::Span<const std::pair<int64_t, int64_t>> interbreaks);

 private:
  using Interval = DimensionValues::Interval;
  using VehicleBreak = DimensionValues::VehicleBreak;

  static bool IncreaseMin(int64_t new_min, Interval* interval,
                          PropagationResult* propagation_result) {
    if (interval->min >= new_min) return true;
    if (!interval->IncreaseMin(new_min)) {
      *propagation_result = kInfeasible;
      return false;
    }
    *propagation_result = kChanged;
    return true;
  }
  static bool DecreaseMax(int64_t new_max, Interval* interval,
                          PropagationResult* propagation_result) {
    if (interval->max <= new_max) return true;
    if (!interval->DecreaseMax(new_max)) {
      *propagation_result = kInfeasible;
      return false;
    }
    *propagation_result = kChanged;
    return true;
  }
  static bool IntersectWith(Interval source, Interval* target,
                            PropagationResult* propagation_result) {
    if (!source.IntersectWith(*target)) {
      *propagation_result = kInfeasible;
    } else if (source != *target) {
      *propagation_result = kChanged;
    }
    *target = source;
    return *propagation_result != kInfeasible;
  }
  // In cases where propagators expect some property of variables to hold,
  // for instance "cumuls[i].min should be weakly increasing in i",
  // it is necessary to delay modification of the variables until after all
  // propagations are done.
  // This struct can be used to store such delayed propagations.
  struct DelayedPropagation {
    int64_t value;  // New bound of the variable.
    int index;      // Some information on which variable to modify.
    bool is_min;    // The bound is a min if this is true, otherwise a max.
  };
  std::vector<DelayedPropagation> delayed_propagations_;
  // Events used in PropagateInterbreak().
  struct UsageEvent {
    int64_t time;
    int index;
    bool is_start;
    bool operator<(const UsageEvent& other) const { return time < other.time; }
  };
  std::vector<UsageEvent> usage_events_;
  // Per-transition reasoning.
  CommittableArray<int64_t> break_duration_on_transition_;
};

}  // namespace operations_research::routing

#endif  // ORTOOLS_ROUTING_BREAKS_H_
