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

#include "ortools/routing/filter_test_utils.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/types.h"
#include "ortools/routing/filter_committables.h"
#include "ortools/util/saturated_arithmetic.h"

namespace operations_research::routing {

void FillDimensionValuesWithPathScenario(
    const DimensionValuesPathScenario& scenario, int path,
    DimensionValues& dimension) {
  using Interval = DimensionValues::Interval;
  const size_t num_nodes = scenario.nodes.size();
  ASSERT_GE(num_nodes, 2);
  // Copy path nodes, make path.
  for (const int node : scenario.nodes) dimension.PushNode(node);
  dimension.MakePathFromNewNodes(path);
  // Copy cumuls.
  ASSERT_EQ(scenario.cumuls.size(), num_nodes);
  absl::Span<Interval> cumuls = dimension.MutableCumuls(path);
  for (int r = 0; r < num_nodes; ++r) {
    cumuls[r] = scenario.cumuls[r];
  }
  // Copy transits if present, otherwise fill with all-integers interval.
  absl::Span<Interval> transits = dimension.MutableTransits(path);
  if (!scenario.transits.empty()) {
    ASSERT_EQ(scenario.transits.size(), num_nodes - 1);
    for (int r = 0; r < num_nodes - 1; ++r) transits[r] = scenario.transits[r];
  } else if (!scenario.travels.empty()) {
    for (int r = 0; r < num_nodes - 1; ++r) {
      transits[r] = {.min = scenario.travels[r], .max = kint64max};
    }
  } else {
    for (auto& transit : transits) transit = Interval::AllIntegers();
  }
  // Copy travels if present, otherwise copy from transits min.
  absl::Span<int64_t> travels = dimension.MutableTravels(path);
  if (!scenario.travels.empty()) {
    ASSERT_EQ(scenario.travels.size(), num_nodes - 1);
    for (int r = 0; r < num_nodes - 1; ++r) travels[r] = scenario.travels[r];
  } else {
    for (int r = 0; r < num_nodes - 1; ++r) travels[r] = transits[r].min;
  }
  // Copy travel sums if present, otherwise compute travel sums from travels.
  if (!scenario.travel_sums.empty()) {
    ASSERT_EQ(scenario.travel_sums.size(), num_nodes);
    absl::Span<int64_t> sums = dimension.MutableTravelSums(path);
    for (int r = 0; r < num_nodes; ++r) sums[r] = scenario.travel_sums[r];
  } else {
    absl::Span<int64_t> travel_sums = dimension.MutableTravelSums(path);
    travel_sums.front() = 0;
    int64_t sum = 0;
    for (int r = 1; r < num_nodes; ++r) {
      CapAddTo(travels[r - 1], &sum);
      travel_sums[r] = sum;
    }
  }
  // Set span if present, otherwise set to [0, kint64max].
  if (scenario.span.has_value()) {
    dimension.MutableSpan(path) = *scenario.span;
  } else {
    dimension.MutableSpan(path) = {.min = 0, .max = kint64max};
  }
}

void ExpectDimensionValuesScenario(
    const DimensionValuesPathScenario& expected_scenario, int path,
    const DimensionValues& dimension) {
  // Check nodes only if they are specified.
  if (!expected_scenario.nodes.empty()) {
    const int num_nodes = expected_scenario.nodes.size();
    ASSERT_EQ(dimension.Nodes(path).size(), num_nodes);
    for (int r = 0; r < num_nodes; ++r) {
      EXPECT_EQ(dimension.Nodes(path)[r], expected_scenario.nodes[r]);
    }
  }
  // Check cumuls only if they are specified.
  if (!expected_scenario.cumuls.empty()) {
    const int num_nodes = expected_scenario.cumuls.size();
    ASSERT_EQ(dimension.Cumuls(path).size(), num_nodes);
    for (int r = 0; r < num_nodes; ++r) {
      EXPECT_EQ(dimension.Cumuls(path)[r], expected_scenario.cumuls[r]);
    }
  }
  // Check transits only if they are specified.
  if (!expected_scenario.transits.empty()) {
    const int num_transits = expected_scenario.transits.size();
    ASSERT_EQ(dimension.Transits(path).size(), num_transits);
    for (int r = 0; r < num_transits; ++r) {
      EXPECT_EQ(dimension.Transits(path)[r], expected_scenario.transits[r]);
    }
  }
  // Check travels only if they are specified.
  if (!expected_scenario.travels.empty()) {
    const int num_travels = expected_scenario.travels.size();
    ASSERT_EQ(dimension.Travels(path).size(), num_travels);
    for (int r = 0; r < num_travels; ++r) {
      EXPECT_EQ(dimension.Travels(path)[r], expected_scenario.travels[r]);
    }
  }
  // Check travel sums only if they are specified.
  if (!expected_scenario.travel_sums.empty()) {
    const int num_nodes = expected_scenario.travel_sums.size();
    ASSERT_EQ(dimension.TravelSums(path).size(), num_nodes);
    for (int r = 0; r < num_nodes; ++r) {
      EXPECT_EQ(dimension.TravelSums(path)[r],
                expected_scenario.travel_sums[r]);
    }
  }
  // Check span only if it is specified.
  if (expected_scenario.span.has_value()) {
    EXPECT_EQ(dimension.Span(path), *expected_scenario.span);
  }
}

}  // namespace operations_research::routing
