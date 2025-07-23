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

#include "ortools/constraint_solver/routing_breaks.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/algorithms/binary_search.h"
#include "ortools/constraint_solver/routing_filter_committables.h"
#include "ortools/util/saturated_arithmetic.h"

namespace operations_research {

// break_duration_on_transition_ is initialized with an upper bound on the
// number of transitions.
BreakPropagator::BreakPropagator(int num_nodes)
    : break_duration_on_transition_(num_nodes, 0) {}

BreakPropagator::PropagationResult BreakPropagator::FastPropagations(
    int path, DimensionValues& dimension_values,
    const PrePostVisitValues& visits) {
  using VehicleBreak = DimensionValues::VehicleBreak;
  const absl::Span<VehicleBreak> vehicle_breaks =
      absl::MakeSpan(dimension_values.MutableVehicleBreaks(path));
  if (vehicle_breaks.empty()) return kUnchanged;
  const absl::Span<Interval> cumuls = dimension_values.MutableCumuls(path);
  const int num_cumuls = cumuls.size();
  const absl::Span<const int64_t> travels = dimension_values.Travels(path);
  const absl::Span<const int64_t> travel_sums =
      dimension_values.TravelSums(path);
  const absl::Span<const int64_t> pre_visits = visits.PreVisits(path);
  const absl::Span<const int64_t> post_visits = visits.PostVisits(path);
  const auto visit_start_max = [cumuls, pre_visits](int c) -> int64_t {
    return CapSub(cumuls[c].max, pre_visits[c]);
  };
  const auto visit_end_min = [cumuls, post_visits](int c) -> int64_t {
    return CapAdd(cumuls[c].min, post_visits[c]);
  };
  const auto travel_slack = [cumuls, travels](int c) -> int64_t {
    const int64_t slack =
        CapSub(CapSub(cumuls[c + 1].max, cumuls[c].min), travels[c]);
    DCHECK_GE(slack, 0);
    return slack;
  };
  PropagationResult result = kUnchanged;
  // Propagations on cumuls are delayed to not break BinarySearch.
  delayed_propagations_.clear();
  // When breaks must be performed inside the route, accumulate location as a
  // min/max window and their total duration.
  int breaks_window_min = num_cumuls;
  int breaks_window_max = -1;
  int64_t breaks_total_duration = 0;
  break_duration_on_transition_.Revert();
  const int num_breaks = vehicle_breaks.size();
  for (VehicleBreak& br : vehicle_breaks) {
    if (br.is_performed.min == 0) continue;
    if (!IncreaseMin(CapSub(br.end.min, br.start.max), &br.duration, &result)) {
      return result;
    }
    // Find largest c_min such that time windows prevent break br to end
    // before cumul c_min. In that case, all visits up to and including c_min
    // must be performed before the break.
    int c_min = BinarySearch<int, false>(
        -1, num_cumuls, [&visit_start_max, break_end_min = br.end.min](int c) {
          return visit_start_max(c) < break_end_min;
        });
    if (c_min >= 0) {
      while (c_min < num_cumuls - 1 && travel_slack(c_min) < br.duration.min) {
        ++c_min;
      }
      if (!IncreaseMin(visit_end_min(c_min), &br.start, &result) ||
          !IncreaseMin(CapAdd(br.start.min, br.duration.min), &br.end,
                       &result)) {
        return kInfeasible;
      }
      // The min break duration should fit, but in some cases (interbreaks) we
      // may have br.start.min + br.duration.min < br.end.min.
      // If br.end.min - br.start.min > travel_slack:
      // - either the break is during the travel c_min -> c_min+1,
      //   in this case its duration is at most travel_slack,
      //   so that br.end.min - br.start <= travel_slack.
      //   Some of the travel c_min -> c_min+1 may be performed after the break.
      // - or the break is after c_min+1, and all of the travel c_min -> c_min+1
      //   is performed before the break: br.start >= cumul[c_min] + travel.
      // We have to take the weaker of the two alternatives, the first one.
      if (c_min < num_cumuls - 1 &&
          !IncreaseMin(CapSub(br.end.min, travel_slack(c_min)), &br.start,
                       &result)) {
        return kInfeasible;
      }
      // Visit c_min must be before the break.
      const int64_t cumul_ub = CapSub(br.start.max, post_visits[c_min]);
      if (cumuls[c_min].min > cumul_ub) return kInfeasible;
      delayed_propagations_.push_back(
          {.value = cumul_ub, .index = c_min, .is_min = false});
    }
    // Find smallest c_max such that time windows prevent break br to start
    // after cumul c_max. In that case, all visits including and after c_max
    // must be performed after the break.
    int c_max = BinarySearch<int, false>(
        num_cumuls, -1,
        [&visit_end_min, break_start_max = br.start.max](int c) {
          return break_start_max < visit_end_min(c);
        });
    if (c_max < num_cumuls) {
      while (c_max > 0 && travel_slack(c_max - 1) < br.duration.min) --c_max;
      if (!DecreaseMax(visit_start_max(c_max), &br.end, &result) ||
          !DecreaseMax(CapSub(br.end.max, br.duration.min), &br.start,
                       &result)) {
        return kInfeasible;
      }
      // See the comment on the symmetric situation above.
      if (c_max > 0 &&
          !DecreaseMax(CapAdd(br.start.max, travel_slack(c_max - 1)), &br.end,
                       &result)) {
        return kInfeasible;
      }
      // Visit c_max must be after the break, delay to not break BinarySearch.
      const int64_t cumul_lb = CapAdd(br.end.min, pre_visits[c_max]);
      if (cumuls[c_max].max < cumul_lb) return kInfeasible;
      delayed_propagations_.push_back(
          {.value = cumul_lb, .index = c_max, .is_min = true});
    }
    // If the break must be inside the route, it must be inside some cumul
    // window, here [c_min, c_max].
    // Overload checking: if transit + break duration do not fit, the break is
    // infeasible.
    // Edge finding: if it fits, push cumuls c_min/c_max to leave enough room.
    if (0 <= c_min && c_max < num_cumuls) {
      const int64_t transit = CapAdd(
          br.duration.min, CapSub(travel_sums[c_max], travel_sums[c_min]));
      if (CapAdd(cumuls[c_min].min, transit) > cumuls[c_max].max) {
        return kInfeasible;
      }
      delayed_propagations_.push_back(
          {.value = CapAdd(cumuls[c_min].min, transit),
           .index = c_max,
           .is_min = true});
      delayed_propagations_.push_back(
          {.value = CapSub(cumuls[c_max].max, transit),
           .index = c_min,
           .is_min = false});
      breaks_window_min = std::min(breaks_window_min, c_min);
      breaks_window_max = std::max(breaks_window_max, c_max);
      CapAddTo(br.duration.min, &breaks_total_duration);
      // If this break is forced on the transition c_min -> c_min + 1,
      // accumulate its duration to this transition.
      if (num_breaks > 1 && c_min + 1 == c_max) {
        const int64_t total_duration = break_duration_on_transition_.Get(c_min);
        break_duration_on_transition_.Set(
            c_min, CapAdd(total_duration, br.duration.min));
      }
    }
  }
  // After the previous loop, there are no BinarySearch() calls, so there is
  // no need to delay propagations.

  // Per-transition reasoning: total break duration + travel must fit.
  for (const int t : break_duration_on_transition_.ChangedIndices()) {
    const int64_t total =
        CapAdd(travels[t], break_duration_on_transition_.Get(t));
    if (!IncreaseMin(CapAdd(cumuls[t].min, total), &cumuls[t + 1], &result) ||
        !DecreaseMax(CapSub(cumuls[t + 1].max, total), &cumuls[t], &result)) {
      return kInfeasible;
    }
  }
  // Overload checker reasoning on overall break window.
  if (breaks_total_duration > 0) {
    const int64_t window_transit = CapAdd(
        breaks_total_duration,
        CapSub(travel_sums[breaks_window_max], travel_sums[breaks_window_min]));
    if (!IncreaseMin(CapAdd(cumuls[breaks_window_min].min, window_transit),
                     &cumuls[breaks_window_max], &result) ||
        !DecreaseMax(CapSub(cumuls[breaks_window_max].max, window_transit),
                     &cumuls[breaks_window_min], &result)) {
      return kInfeasible;
    }
  }
  for (const auto& [value, index, is_min] : delayed_propagations_) {
    if (is_min && !IncreaseMin(value, &cumuls[index], &result)) {
      return kInfeasible;
    }
    if (!is_min && !DecreaseMax(value, &cumuls[index], &result)) {
      return kInfeasible;
    }
  }
  return result;
}

// Add interbreak reasoning.
BreakPropagator::PropagationResult BreakPropagator::PropagateInterbreak(
    int path, DimensionValues& dimension,
    absl::Span<const std::pair<int64_t, int64_t>> interbreaks) {
  PropagationResult result = kUnchanged;
  absl::Span<Interval> cumuls = dimension.MutableCumuls(path);
  std::vector<VehicleBreak>& vehicle_breaks =
      dimension.MutableVehicleBreaks(path);
  // We use fake breaks for start/end of path:
  // - start break: [kint64min, cumul[0])
  // - end break: [cumul[n-1], kint64max).
  const int64_t kint64min = std::numeric_limits<int64_t>::min();
  const int64_t kint64max = std::numeric_limits<int64_t>::max();
  vehicle_breaks.push_back({.start = {kint64min, kint64min},
                            .end = cumuls.front(),
                            .duration = {0, kint64max},
                            .is_performed = {1, 1}});
  vehicle_breaks.push_back({.start = cumuls.back(),
                            .end = {kint64max, kint64max},
                            .duration = {0, kint64max},
                            .is_performed = {1, 1}});
  const int num_breaks = vehicle_breaks.size();
  for (const auto [limit, min_break_duration] : interbreaks) {
    // Generate and sort events by increasing time. Events have to be
    // regenerated for each interbreak, because end events depend on the limit.
    usage_events_.clear();
    for (int i = 0; i < num_breaks; ++i) {
      const auto& br = vehicle_breaks[i];
      if (br.is_performed.max == 0 || br.duration.max < min_break_duration) {
        continue;
      }
      usage_events_.push_back(
          {.time = br.start.min, .index = i, .is_start = true});
      usage_events_.push_back(
          {.time = CapAdd(br.end.max, limit), .index = i, .is_start = false});
    }
    std::sort(usage_events_.begin(), usage_events_.end());
    // Main loop: sweep over events, maintain max profile height.
    // When sweeping over time, we cross some time intervals of duration > 0:
    // - if profile height is 0, no break can cover the interval. Infeasible.
    // - if profile height is 1, the only active break must cover the interval.
    // When num_active_breaks == 1, the xor of all active breaks is the only
    // active break.
    int num_active_breaks = 0;
    int xor_active_breaks = 0;
    int64_t previous_time = kint64min;
    for (const UsageEvent& event : usage_events_) {
      if (event.time != previous_time) {
        DCHECK_GT(event.time, previous_time);
        // Time changed: check covering condition.
        if (num_active_breaks == 0) return kInfeasible;
        if (num_active_breaks == 1) {
          VehicleBreak& br = vehicle_breaks[xor_active_breaks];
          const int64_t new_start_max =
              std::min(previous_time, CapSub(br.end.max, min_break_duration));
          const int64_t new_end_min =
              std::max(CapSub(event.time, limit),
                       CapAdd(br.start.min, min_break_duration));
          if (!DecreaseMax(new_start_max, &br.start, &result) ||
              !IncreaseMin(new_end_min, &br.end, &result)) {
            return kInfeasible;
          }
          if (xor_active_breaks < num_breaks - 2) {
            const int64_t new_duration_min = std::max(
                min_break_duration, CapSub(new_end_min, new_start_max));
            if (!IncreaseMin(1, &br.is_performed, &result) ||
                !IncreaseMin(new_duration_min, &br.duration, &result)) {
              return kInfeasible;
            }
          }
        }
      }
      // Update the set of active intervals.
      num_active_breaks += event.is_start ? 1 : -1;
      xor_active_breaks ^= event.index;
      previous_time = event.time;
    }
    // Propagate fake start/end information to actual start/end.
    const Interval& new_start = vehicle_breaks[num_breaks - 2].end;
    const Interval& new_end = vehicle_breaks[num_breaks - 1].start;
    if (!IntersectWith(new_start, &cumuls.front(), &result) ||
        !IntersectWith(new_end, &cumuls.back(), &result)) {
      vehicle_breaks.resize(num_breaks - 2);
      return kInfeasible;
    }
  }
  // Remove fake path start/end breaks.
  vehicle_breaks.resize(num_breaks - 2);
  return result;
}

}  // namespace operations_research
