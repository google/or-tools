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

#ifndef ORTOOLS_ROUTING_FILTER_TEST_UTILS_H_
#define ORTOOLS_ROUTING_FILTER_TEST_UTILS_H_

#include <cstdint>
#include <optional>
#include <vector>

#include "absl/strings/str_format.h"
#include "ortools/routing/filter_committables.h"

namespace operations_research::routing {

// Pretty-print Intervals in EXPECT/ASSERT failures.
template <typename Sink>
void AbslStringify(Sink& sink, const DimensionValues::Interval& interval) {
  absl::Format(&sink, "{.min=%d, .max=%d)", interval.min, interval.max);
}
// Pretty-print VehicleBreaks in EXPECT/ASSERT failures.
template <typename Sink>
void AbslStringify(Sink& sink, const DimensionValues::VehicleBreak& br) {
  absl::Format(&sink, "Break<start=%v, end=%v, duration=%v, is_performed=%v>",
               br.start, br.end, br.duration, br.is_performed);
}

// Defines the (non-breaks) internal state of a DimensionValues object.
struct DimensionValuesPathScenario {
  using Interval = DimensionValues::Interval;
  std::vector<int> nodes;
  std::vector<Interval> cumuls;
  std::vector<Interval> transits;
  std::vector<int64_t> travels;
  std::vector<int64_t> travel_sums;
  std::optional<Interval> span;
};

// Fills the internal state of a DimensionValues object with the given scenario.
void FillDimensionValuesWithPathScenario(
    const DimensionValuesPathScenario& scenario, int path,
    DimensionValues& dimension);

// Expects the internal state of a DimensionValues object to match the given
// scenario.
void ExpectDimensionValuesScenario(
    const DimensionValuesPathScenario& expected_scenario, int path,
    const DimensionValues& dimension);

}  // namespace operations_research::routing

#endif  // ORTOOLS_ROUTING_FILTER_TEST_UTILS_H_
