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

#include "ortools/routing/breaks.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/routing/filter_committables.h"
#include "ortools/routing/filter_test_utils.h"

namespace operations_research::routing {

void ExpectBreaksScenario(
    absl::Span<const DimensionValues::VehicleBreak> expected_breaks, int path,
    const DimensionValues& dimension) {
  const absl::Span<const DimensionValues::VehicleBreak> vehicle_breaks =
      dimension.VehicleBreaks(path);
  EXPECT_EQ(vehicle_breaks.size(), expected_breaks.size());
  for (int i = 0; i < vehicle_breaks.size(); ++i) {
    EXPECT_EQ(vehicle_breaks[i].start, expected_breaks[i].start);
    EXPECT_EQ(vehicle_breaks[i].end, expected_breaks[i].end);
    EXPECT_EQ(vehicle_breaks[i].duration, expected_breaks[i].duration);
    EXPECT_EQ(vehicle_breaks[i].is_performed, expected_breaks[i].is_performed);
  }
}

void MirrorBreaks(std::vector<DimensionValues::VehicleBreak>& breaks,
                  int64_t offset) {
  for (DimensionValues::VehicleBreak& br : breaks) {
    br.start = {.min = offset - br.start.max, .max = offset - br.start.min};
    br.end = {.min = offset - br.end.max, .max = offset - br.end.min};
    std::swap(br.start, br.end);
  }
}

struct VisitScenario {
  std::vector<int64_t> pre_visits;
  std::vector<int64_t> post_visits;
};

void FillPrePostVisitValuesWithVisitScenario(
    const VisitScenario& visit_scenario, int path, PrePostVisitValues& visits) {
  const int num_visits = visit_scenario.pre_visits.size();
  visits.ChangePathSize(path, num_visits);
  const absl::Span<int64_t> pre_visits = visits.MutablePreVisits(path);
  const absl::Span<int64_t> post_visits = visits.MutablePostVisits(path);
  for (int v = 0; v < num_visits; ++v) {
    pre_visits[v] = visit_scenario.pre_visits[v];
    post_visits[v] = visit_scenario.post_visits[v];
  }
}

class BreakPropagatorTest : public testing::TestWithParam<bool> {};

TEST_P(BreakPropagatorTest, PropagateBreakwiseBreaksStartMin) {
  using Interval = DimensionValues::Interval;
  using VehicleBreak = DimensionValues::VehicleBreak;
  const int num_paths = 1;
  const int num_nodes = 10;
  const DimensionValuesPathScenario dim_scenario{
      .nodes = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9},
      .cumuls = std::vector<Interval>(10, {.min = 0, .max = 100}),
      .travels = std::vector<int64_t>(9, 6),
  };
  DimensionValues dimension(num_paths, num_nodes);
  const DimensionValuesPathScenario expected_dim_scenario = {
      .cumuls = {{0, 46},
                 {6, 52},
                 {12, 58},
                 {18, 64},
                 {24, 70},
                 {30, 76},
                 {36, 82},
                 {42, 88},
                 {48, 94},
                 {54, 100}}};
  const VisitScenario visit_scenario = {
      .pre_visits = {0, 0, 2, 0, 0, 0, 0, 1, 0, 0},
      .post_visits = {0, 0, 1, 0, 0, 0, 0, 2, 0, 0},
  };
  PrePostVisitValues visits(num_paths, num_nodes);
  FillPrePostVisitValuesWithVisitScenario(visit_scenario, /*path=*/0, visits);
  struct PropagatorScenario {
    std::vector<VehicleBreak> initial_breaks;
    std::vector<VehicleBreak> expected_breaks;
    BreakPropagator::PropagationResult expected_result;
  };
  std::vector<PropagatorScenario> propagator_scenarios{
      // Break is after visit 2 (br.end_min = 57 > visit[2].start_max = 58 - 2),
      // thus its start is pushed after visit 2 at visit[2].end_min = 12 + 1.
      {.initial_breaks = {{.start = {.min = 0, .max = 90},
                           .end = {.min = 57, .max = 100},
                           .duration = {.min = 10, .max = 100},
                           .is_performed = {.min = 1, .max = 1}}},
       .expected_breaks = {{.start = {.min = 13, .max = 90},
                            .end = {.min = 57, .max = 100},
                            .duration = {.min = 10, .max = 100},
                            .is_performed = {.min = 1, .max = 1}}},
       .expected_result = BreakPropagator::kChanged},
      // Same situation, but the start max blocks the propagation.
      {.initial_breaks = {{.start = {.min = 0, .max = 12},
                           .end = {.min = 57, .max = 100},
                           .duration = {.min = 10, .max = 100},
                           .is_performed = {.min = 1, .max = 1}}},
       .expected_result = BreakPropagator::kInfeasible},
      // Break end min is now 56. Break is:
      // - not forced after visit 2: its start max 58 - 2 = 56 <= 56,
      // - forced after visit 1: its start max is 52 - 0 < 56.
      // Visit 1 end_min is 6, however the slack in transit 1 -> 2 is
      // 58 - 6 - 6 = 46, thus its start is pushed to br.end_min - slack = 10.
      {.initial_breaks = {{.start = {.min = 0, .max = 90},
                           .end = {.min = 56, .max = 100},
                           .duration = {.min = 10, .max = 100},
                           .is_performed = {.min = 1, .max = 1}}},
       .expected_breaks = {{.start = {.min = 10, .max = 90},
                            .end = {.min = 56, .max = 100},
                            .duration = {.min = 10, .max = 100},
                            .is_performed = {.min = 1, .max = 1}}},
       .expected_result = BreakPropagator::kChanged},
      // Break is after visit 1, the slack in transit 1 -> 2 is still 46,
      // thus its start is pushed br.end_min - slack = 7.
      {.initial_breaks = {{.start = {.min = 0, .max = 90},
                           .end = {.min = 53, .max = 100},
                           .duration = {.min = 10, .max = 100},
                           .is_performed = {.min = 1, .max = 1}}},
       .expected_breaks = {{.start = {.min = 7, .max = 90},
                            .end = {.min = 53, .max = 100},
                            .duration = {.min = 10, .max = 100},
                            .is_performed = {.min = 1, .max = 1}}},
       .expected_result = BreakPropagator::kChanged},
      // Same situation, but the start max blocks the propagation.
      {.initial_breaks = {{.start = {.min = 0, .max = 6},
                           .end = {.min = 53, .max = 100},
                           .duration = {.min = 10, .max = 100},
                           .is_performed = {.min = 1, .max = 1}}},
       .expected_result = BreakPropagator::kInfeasible},
      // Break may be before visit 0, it is unchanged.
      {.initial_breaks = {{.start = {.min = 0, .max = 90},
                           .end = {.min = 46, .max = 100},
                           .duration = {.min = 10, .max = 100},
                           .is_performed = {.min = 1, .max = 1}}},
       .expected_result = BreakPropagator::kUnchanged},
  };
  const int num_scenarios = propagator_scenarios.size();
  const bool mirror = GetParam();
  BreakPropagator propagator(num_nodes);
  for (int s = 0; s < num_scenarios; ++s) {
    PropagatorScenario& scenario = propagator_scenarios[s];
    if (mirror) {
      MirrorBreaks(scenario.initial_breaks, 100);
      MirrorBreaks(scenario.expected_breaks, 100);
    }
    dimension.Revert();
    FillDimensionValuesWithPathScenario(dim_scenario, /*path=*/0, dimension);
    EXPECT_TRUE(PropagateTransitAndSpan(0, dimension));
    ExpectDimensionValuesScenario(expected_dim_scenario, /*path=*/0, dimension);
    // Add breaks and propagate.
    dimension.MutableVehicleBreaks(0) = scenario.initial_breaks;
    EXPECT_EQ(propagator.FastPropagations(/*path=*/0, dimension, visits),
              scenario.expected_result);
    if (scenario.expected_result == BreakPropagator::kChanged) {
      ExpectBreaksScenario(scenario.expected_breaks, /*path=*/0, dimension);
    }
  }
}

TEST_P(BreakPropagatorTest, PropagateBreakwiseBreaksEndMin) {
  using VehicleBreak = DimensionValues::VehicleBreak;
  const int num_paths = 1;
  const int num_nodes = 10;
  const DimensionValuesPathScenario dim_scenario{
      .nodes = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9},
      .cumuls = {{0, 100},
                 {0, 100},
                 {0, 100},
                 {0, 100},
                 {45, 100},
                 {0, 55},
                 {0, 100},
                 {0, 100},
                 {0, 100},
                 {0, 100}},
      .travels = std::vector<int64_t>(9, 6),
  };
  DimensionValues dimension(num_paths, num_nodes);
  const DimensionValuesPathScenario expected_dim_scenario = {
      .cumuls = {{0, 25},
                 {6, 31},
                 {12, 37},
                 {18, 43},
                 {45, 49},
                 {51, 55},
                 {57, 82},
                 {63, 88},
                 {69, 94},
                 {75, 100}}};
  const VisitScenario visit_scenario = {
      .pre_visits = {0, 0, 0, 0, 2, 1, 0, 0, 0, 0},
      .post_visits = {0, 0, 0, 0, 1, 2, 0, 0, 0, 0},
  };
  PrePostVisitValues visits(num_paths, num_nodes);
  FillPrePostVisitValuesWithVisitScenario(visit_scenario, /*path=*/0, visits);
  struct PropagatorScenario {
    std::vector<VehicleBreak> initial_breaks;
    std::vector<VehicleBreak> expected_breaks;
    BreakPropagator::PropagationResult expected_result;
  };
  std::vector<PropagatorScenario> propagator_scenarios{
      // Break is after visit 4 (start max = 49 - 2), it fits before visit 5.
      {.initial_breaks = {{.start = {.min = 0, .max = 90},
                           .end = {.min = 48, .max = 100},
                           .duration = {.min = 4, .max = 100},
                           .is_performed = {.min = 1, .max = 1}}},
       .expected_breaks = {{.start = {.min = 45 + 1, .max = 90},
                            .end = {.min = 45 + 1 + 4, .max = 100},
                            .duration = {.min = 4, .max = 100},
                            .is_performed = {.min = 1, .max = 1}}},
       .expected_result = BreakPropagator::kChanged},
      // Same situation, but the start max blocks the propagation.
      {.initial_breaks = {{.start = {.min = 0, .max = 45},
                           .end = {.min = 48, .max = 100},
                           .duration = {.min = 4, .max = 100},
                           .is_performed = {.min = 1, .max = 1}}},
       .expected_breaks = {},
       .expected_result = BreakPropagator::kInfeasible},
      // Same situation, but the end max blocks the propagation.
      {.initial_breaks = {{.start = {.min = 0, .max = 90},
                           .end = {.min = 48, .max = 49},
                           .duration = {.min = 4, .max = 100},
                           .is_performed = {.min = 1, .max = 1}}},
       .expected_breaks = {},
       .expected_result = BreakPropagator::kInfeasible},
      // Same situation, but the break may not be performed.
      {.initial_breaks = {{.start = {.min = 0, .max = 90},
                           .end = {.min = 48, .max = 100},
                           .duration = {.min = 4, .max = 100},
                           .is_performed = {.min = 0, .max = 1}}},
       .expected_breaks = {},
       .expected_result = BreakPropagator::kUnchanged},
      // Break is after visit 4 (start max = 49 - 2), it does not fit before
      // visit 5 because its duration is 5, and the slack in transit 4 -> 5 is
      // 55 - 45 - 6 = 4. The break is pushed after visit 5.
      {.initial_breaks = {{.start = {.min = 0, .max = 90},
                           .end = {.min = 48, .max = 100},
                           .duration = {.min = 5, .max = 100},
                           .is_performed = {.min = 1, .max = 1}}},
       .expected_breaks = {{.start = {.min = 51 + 2, .max = 90},
                            .end = {.min = 51 + 2 + 5, .max = 100},
                            .duration = {.min = 5, .max = 100},
                            .is_performed = {.min = 1, .max = 1}}},
       .expected_result = BreakPropagator::kChanged},
  };
  const int num_scenarios = propagator_scenarios.size();
  const bool mirror = GetParam();
  BreakPropagator propagator(num_nodes);
  for (int s = 0; s < num_scenarios; ++s) {
    PropagatorScenario& scenario = propagator_scenarios[s];
    if (mirror) {
      MirrorBreaks(scenario.initial_breaks, 100);
      MirrorBreaks(scenario.expected_breaks, 100);
    }
    dimension.Revert();
    FillDimensionValuesWithPathScenario(dim_scenario, /*path=*/0, dimension);
    EXPECT_TRUE(PropagateTransitAndSpan(0, dimension));
    ExpectDimensionValuesScenario(expected_dim_scenario, /*path=*/0, dimension);
    // Add breaks and propagate.
    dimension.MutableVehicleBreaks(0) = scenario.initial_breaks;
    EXPECT_EQ(propagator.FastPropagations(/*path=*/0, dimension, visits),
              scenario.expected_result);
    if (scenario.expected_result == BreakPropagator::kChanged) {
      ExpectBreaksScenario(scenario.expected_breaks, /*path=*/0, dimension);
    }
  }
}

TEST(BreakPropagatorTest, EdgeFinding) {
  using Interval = DimensionValues::Interval;
  using VehicleBreak = DimensionValues::VehicleBreak;
  const int num_paths = 1;
  const int num_nodes = 6;
  const DimensionValuesPathScenario dim_scenario{
      .nodes = {0, 1, 2, 3, 4, 5},
      .cumuls = {{0, 9}, {6, 15}, {12, 21}, {18, 27}, {24, 33}, {30, 39}},
      .travels = std::vector<int64_t>(5, 6),
  };
  DimensionValues dimension(num_paths, num_nodes);
  const DimensionValuesPathScenario expected_dim_scenario = {
      .cumuls = {{0, 9}, {6, 15}, {12, 21}, {18, 27}, {24, 33}, {30, 39}}};
  const VisitScenario visit_scenario = {
      .pre_visits = {0, 0, 0, 0, 0, 0},
      .post_visits = {0, 0, 0, 0, 0, 0},
  };
  PrePostVisitValues visits(num_paths, num_nodes);
  FillPrePostVisitValuesWithVisitScenario(visit_scenario, /*path=*/0, visits);
  struct PropagatorScenario {
    std::vector<VehicleBreak> initial_breaks;
    std::vector<VehicleBreak> expected_breaks;
    std::vector<Interval> expected_cumuls;
    BreakPropagator::PropagationResult expected_result;
  };
  std::vector<PropagatorScenario> propagator_scenarios{
      // Break must be between visits 1 and 3.
      // Cumuls[1].max and cumuls[3].min must be widened by break duration.
      {.initial_breaks = {{.start = {.min = 9, .max = 17},
                           .end = {.min = 16, .max = 24},
                           .duration = {.min = 7, .max = 100},
                           .is_performed = {.min = 1, .max = 1}}},
       .expected_cumuls =
           {{0, 9}, {6, 8}, {12, 21}, {25, 27}, {24, 33}, {30, 39}},
       .expected_result = BreakPropagator::kChanged},
      // Break must be between visits 1 and 3.
      // The break duration is too large to fit.
      {.initial_breaks = {{.start = {.min = 9, .max = 17},
                           .end = {.min = 16, .max = 24},
                           .duration = {.min = 10, .max = 100},
                           .is_performed = {.min = 1, .max = 1}}},
       .expected_result = BreakPropagator::kInfeasible},
      // Break must be between visits 2 and 4.
      // Cumuls[2].max and cumuls[4].min must be widened by break duration.
      {.initial_breaks = {{.start = {.min = 20, .max = 23},
                           .end = {.min = 22, .max = 25},
                           .duration = {.min = 2, .max = 100},
                           .is_performed = {.min = 1, .max = 1}}},
       .expected_cumuls =
           {{0, 9}, {6, 15}, {12, 19}, {18, 27}, {26, 33}, {30, 39}},
       .expected_result = BreakPropagator::kChanged},
      // If we set both breaks above at once, the overall cumul window [1, 4]
      // must also be widened.
      {.initial_breaks = {{.start = {.min = 9, .max = 17},
                           .end = {.min = 16, .max = 24},
                           .duration = {.min = 7, .max = 100},
                           .is_performed = {.min = 1, .max = 1}},
                          {.start = {.min = 20, .max = 23},
                           .end = {.min = 22, .max = 25},
                           .duration = {.min = 2, .max = 100},
                           .is_performed = {.min = 1, .max = 1}}},
       .expected_cumuls =
           {{0, 9}, {6, 6}, {12, 19}, {25, 27}, {33, 33}, {30, 39}},
       .expected_result = BreakPropagator::kChanged},
      // If we set both breaks above at once, and the total amount of breaks is
      // too large, the overall cumul window [1, 4] is infeasible.
      {.initial_breaks = {{.start = {.min = 9, .max = 17},
                           .end = {.min = 16, .max = 24},
                           .duration = {.min = 7, .max = 100},
                           .is_performed = {.min = 1, .max = 1}},
                          {.start = {.min = 20, .max = 23},
                           .end = {.min = 22, .max = 25},
                           .duration = {.min = 3, .max = 100},
                           .is_performed = {.min = 1, .max = 1}}},
       .expected_result = BreakPropagator::kInfeasible},
  };
  const int num_scenarios = propagator_scenarios.size();
  BreakPropagator propagator(num_nodes);
  for (int s = 0; s < num_scenarios; ++s) {
    const PropagatorScenario& scenario = propagator_scenarios[s];
    dimension.Revert();
    FillDimensionValuesWithPathScenario(dim_scenario, /*path=*/0, dimension);
    EXPECT_TRUE(PropagateTransitAndSpan(0, dimension));
    ExpectDimensionValuesScenario(expected_dim_scenario, /*path=*/0, dimension);
    // Add breaks and propagate.
    dimension.MutableVehicleBreaks(0) = scenario.initial_breaks;
    EXPECT_EQ(propagator.FastPropagations(/*path=*/0, dimension, visits),
              scenario.expected_result);
    if (scenario.expected_result == BreakPropagator::kChanged) {
      if (scenario.expected_breaks.empty()) {
        ExpectBreaksScenario(scenario.initial_breaks, /*path=*/0, dimension);
      } else {
        ExpectBreaksScenario(scenario.expected_breaks, /*path=*/0, dimension);
      }
      DimensionValuesPathScenario cumuls_post_propagation{
          .cumuls = scenario.expected_cumuls};
      ExpectDimensionValuesScenario(cumuls_post_propagation, 0, dimension);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(MirroredOrNot, BreakPropagatorTest, testing::Bool());

TEST(BreakPropagatorTest, PerTransitionReasoning) {
  const int num_paths = 1;
  const int num_nodes = 4;
  using Interval = DimensionValues::Interval;
  using VehicleBreak = DimensionValues::VehicleBreak;
  // Pre-post visits are the same over all scenarios.
  PrePostVisitValues visits(num_paths, num_nodes);
  FillPrePostVisitValuesWithVisitScenario(
      {.pre_visits = {0, 0, 0, 0}, .post_visits = {0, 0, 0, 0}}, 0, visits);
  // Breaks are the same over all scenarios.
  const std::vector<VehicleBreak> breaks{
      // Must be performed during transition 1 -> 2.
      {.start = {.min = 0, .max = 23},
       .end = {.min = 16, .max = 100},
       .duration = {.min = 4, .max = 4},
       .is_performed = {.min = 1, .max = 1}},
      // Must be performed during transition 1 -> 2.
      {.start = {.min = 0, .max = 23},
       .end = {.min = 16, .max = 100},
       .duration = {.min = 3, .max = 4},
       .is_performed = {.min = 1, .max = 1}},
      // Must be performed during the route, any transition.
      {.start = {.min = 9, .max = 29},
       .end = {.min = 10, .max = 30},
       .duration = {.min = 1, .max = 2},
       .is_performed = {.min = 1, .max = 1}},
  };
  // Cumuls will behave differently depending on where breaks are wrt transits.
  struct PropagatorScenario {
    std::vector<Interval> initial_cumuls;
    std::vector<Interval> expected_cumuls;
    BreakPropagator::PropagationResult expected_result;
  };
  std::vector<PropagatorScenario> scenarios{
      // From time windows, both breaks must be in transition 1 -> 2.
      {.initial_cumuls = {{6, 9}, {12, 15}, {24, 27}, {30, 33}},
       .expected_cumuls = {{6, 7}, {12, 14}, {25, 27}, {32, 33}},
       .expected_result = BreakPropagator::kChanged},
      // Same, but cumuls[1] is too tight.
      {.initial_cumuls = {{6, 9}, {15, 15}, {24, 27}, {30, 33}},
       .expected_result = BreakPropagator::kInfeasible},
      // Same, but cumuls[2] is too tight.
      {.initial_cumuls = {{6, 9}, {12, 15}, {24, 24}, {30, 33}},
       .expected_result = BreakPropagator::kInfeasible},
  };

  const DimensionValuesPathScenario template_dimension_scenario{
      .nodes = {0, 1, 2, 3},
      .travels = {6, 6, 6},
  };
  DimensionValues dimension(num_paths, num_nodes);
  BreakPropagator propagator(num_nodes);
  for (int s = 0; s < scenarios.size(); ++s) {
    const PropagatorScenario& scenario = scenarios[s];
    SCOPED_TRACE(absl::StrCat("Scenario: ", s));
    dimension.Revert();
    // Fill cumuls, propagate transits.
    DimensionValuesPathScenario dimension_scenario =
        template_dimension_scenario;
    dimension_scenario.cumuls = scenario.initial_cumuls;
    FillDimensionValuesWithPathScenario(dimension_scenario, 0, dimension);
    EXPECT_TRUE(PropagateTransitAndSpan(0, dimension));
    ExpectDimensionValuesScenario(dimension_scenario, 0, dimension);
    // Adds breaks, propagate, check result.
    dimension.MutableVehicleBreaks(0) = breaks;
    EXPECT_EQ(propagator.FastPropagations(0, dimension, visits),
              scenario.expected_result);
    if (scenario.expected_result != BreakPropagator::kInfeasible) {
      EXPECT_TRUE(PropagateTransitAndSpan(0, dimension));
      ExpectDimensionValuesScenario({.cumuls = scenario.expected_cumuls}, 0,
                                    dimension);
    }
  }
}

TEST(BreakPropagatorTest, PropagateCumuls) {
  using VehicleBreak = DimensionValues::VehicleBreak;
  const int num_paths = 1;
  const int num_nodes = 5;
  const DimensionValuesPathScenario initial_scenario{
      .nodes = {0, 1, 2, 3, 4},
      .cumuls = {{0, 100}, {0, 51}, {0, 100}, {52, 100}, {0, 100}},
      .travels = std::vector<int64_t>(4, 6),
  };
  DimensionValues dimension(num_paths, num_nodes);
  FillDimensionValuesWithPathScenario(initial_scenario, 0, dimension);
  EXPECT_TRUE(PropagateTransitAndSpan(0, dimension));
  const DimensionValuesPathScenario scenario_post_transit = {
      .cumuls = {{0, 45}, {6, 51}, {12, 88}, {52, 94}, {58, 100}},
  };
  ExpectDimensionValuesScenario(scenario_post_transit, 0, dimension);
  // Add breaks.
  const VisitScenario visit_scenario = {
      .pre_visits = {0, 2, 0, 3, 0},
      .post_visits = {0, 5, 0, 4, 0},
  };
  PrePostVisitValues visits(num_paths, num_nodes);
  FillPrePostVisitValuesWithVisitScenario(visit_scenario, 0, visits);
  std::vector<VehicleBreak> initial_breaks = {
      {.start = {.min = 40, .max = 55},
       .end = {.min = 50, .max = 65},
       .duration = {.min = 10, .max = 100},
       .is_performed = {.min = 1, .max = 1}}};
  dimension.MutableVehicleBreaks(0) = initial_breaks;
  BreakPropagator propagator(num_nodes);
  EXPECT_EQ(propagator.FastPropagations(0, dimension, visits),
            BreakPropagator::kChanged);
  // Visit 1 is before the break: br.end.min = 50 > 51 - 2 = visit[1].start.max.
  // Thus visit[1].end = cumul[1] + 5 <= break.start.max = 55,
  // in other words cumuls[1].end <= 55 - 5.
  // Visit 3 is after the break: visit[3].end.min = 52 + 4 > 55 = br.start.max.
  // Thus cumul[3] >= 50 + 3.
  DimensionValuesPathScenario scenario_post_propagation = scenario_post_transit;
  scenario_post_propagation.cumuls[1].max = 50;
  scenario_post_propagation.cumuls[3].min = 53;
  ExpectDimensionValuesScenario(scenario_post_propagation, 0, dimension);
}

TEST(BreakPropagatorTest, PropagateInterbreak) {
  using Interval = DimensionValues::Interval;
  using VehicleBreak = DimensionValues::VehicleBreak;
  const int num_paths = 1;
  const int num_nodes = 3;
  const DimensionValuesPathScenario initial_scenario{
      .nodes = {0, 1}, .cumuls = {{0, 20}, {80, 100}}, .travels = {0}};
  DimensionValues dimension(num_paths, num_nodes);
  // Add breaks.
  const VisitScenario visit_scenario = {
      .pre_visits = {0, 0, 0},
      .post_visits = {0, 0, 0},
  };
  PrePostVisitValues visits(num_paths, num_nodes);
  FillPrePostVisitValuesWithVisitScenario(visit_scenario, 0, visits);

  struct InterbreakScenario {
    std::vector<VehicleBreak> initial_breaks;
    std::vector<std::pair<int64_t, int64_t>> interbreaks;
    std::vector<VehicleBreak> expected_breaks;
    std::vector<Interval> expected_cumuls;
    BreakPropagator::PropagationResult expected_result;
  };
  std::vector<InterbreakScenario> interbreak_scenarios{
      // No breaks, no cumul tightening.
      {.interbreaks = {{80, 10}},
       .expected_cumuls = {{0, 20}, {80, 100}},
       .expected_result = BreakPropagator::kUnchanged},
      // No breaks, start/end tightening, feasible.
      {.interbreaks = {{70, 10}},
       .expected_cumuls = {{10, 20}, {80, 90}},
       .expected_result = BreakPropagator::kChanged},
      // No breaks, infeasible.
      {.interbreaks = {{59, 10}},
       .expected_result = BreakPropagator::kInfeasible},
      // One break, tightening only break, feasible.
      {.initial_breaks = {{.start = {.min = 0, .max = 90},
                           .end = {.min = 10, .max = 100},
                           .duration = {.min = 10, .max = 100},
                           .is_performed = {.min = 1, .max = 1}}},
       .interbreaks = {{35, 10}},
       .expected_breaks = {{.start = {.min = 0, .max = 55},
                            .end = {.min = 45, .max = 100},
                            .duration = {.min = 10, .max = 100},
                            .is_performed = {.min = 1, .max = 1}}},
       .expected_result = BreakPropagator::kChanged},
      // One break, tighten break and start/end, feasible.
      {.initial_breaks = {{.start = {.min = 0, .max = 90},
                           .end = {.min = 10, .max = 100},
                           .duration = {.min = 0, .max = 100},
                           .is_performed = {.min = 0, .max = 1}}},
       .interbreaks = {{30, 10}},
       .expected_breaks = {{.start = {.min = 0, .max = 50},
                            .end = {.min = 50, .max = 100},
                            .duration = {.min = 10, .max = 100},
                            .is_performed = {.min = 1, .max = 1}}},
       .expected_cumuls = {{0, 20}, {80, 100}},
       .expected_result = BreakPropagator::kChanged},
      // One break, tighten break and start/end, feasible.
      {.initial_breaks = {{.start = {.min = 40, .max = 90},
                           .end = {.min = 10, .max = 60},
                           .duration = {.min = 0, .max = 100},
                           .is_performed = {.min = 0, .max = 1}}},
       .interbreaks = {{30, 10}},
       .expected_breaks = {{.start = {.min = 40, .max = 50},
                            .end = {.min = 50, .max = 60},
                            .duration = {.min = 10, .max = 100},
                            .is_performed = {.min = 1, .max = 1}}},
       .expected_cumuls = {{10, 20}, {80, 90}},
       .expected_result = BreakPropagator::kChanged},
      // One break needed but cannot be performed, infeasible.
      {.initial_breaks = {{.start = {.min = 40, .max = 90},
                           .end = {.min = 10, .max = 60},
                           .duration = {.min = 0, .max = 100},
                           .is_performed = {.min = 0, .max = 0}}},
       .interbreaks = {{30, 10}},
       .expected_result = BreakPropagator::kInfeasible},
      // One break, start gets infeasible.
      {.initial_breaks = {{.start = {.min = 51, .max = 90},
                           .end = {.min = 10, .max = 60},
                           .duration = {.min = 0, .max = 100},
                           .is_performed = {.min = 0, .max = 1}}},
       .interbreaks = {{30, 10}},
       .expected_result = BreakPropagator::kInfeasible},
      // One break, end gets infeasible.
      {.initial_breaks = {{.start = {.min = 40, .max = 90},
                           .end = {.min = 10, .max = 49},
                           .duration = {.min = 0, .max = 100},
                           .is_performed = {.min = 0, .max = 1}}},
       .interbreaks = {{30, 10}},
       .expected_result = BreakPropagator::kInfeasible},
      // One break, duration gets infeasible.
      {.initial_breaks = {{.start = {.min = 40, .max = 90},
                           .end = {.min = 10, .max = 60},
                           .duration = {.min = 0, .max = 9},
                           .is_performed = {.min = 0, .max = 1}}},
       .interbreaks = {{30, 10}},
       .expected_result = BreakPropagator::kInfeasible},
      // Two breaks, both duration-feasible, none should get modified.
      {.initial_breaks = {{.start = {.min = 0, .max = 90},
                           .end = {.min = 5, .max = 100},
                           .duration = {.min = 5, .max = 100},
                           .is_performed = {.min = 0, .max = 1}},
                          {.start = {.min = 0, .max = 90},
                           .end = {.min = 5, .max = 100},
                           .duration = {.min = 5, .max = 100},
                           .is_performed = {.min = 0, .max = 1}}},
       .interbreaks = {{25, 10}},
       .expected_result = BreakPropagator::kUnchanged},
      // Two breaks, one is too short, the other should be forced.
      {.initial_breaks = {{.start = {.min = 0, .max = 90},
                           .end = {.min = 5, .max = 100},
                           .duration = {.min = 5, .max = 9},
                           .is_performed = {.min = 0, .max = 1}},
                          {.start = {.min = 0, .max = 90},
                           .end = {.min = 5, .max = 100},
                           .duration = {.min = 5, .max = 100},
                           .is_performed = {.min = 0, .max = 1}}},
       .interbreaks = {{25, 10}},
       .expected_breaks = {{.start = {.min = 0, .max = 90},
                            .end = {.min = 5, .max = 100},
                            .duration = {.min = 5, .max = 9},
                            .is_performed = {.min = 0, .max = 1}},
                           {.start = {.min = 0, .max = 45},
                            .end = {.min = 55, .max = 100},
                            .duration = {.min = 10, .max = 100},
                            .is_performed = {.min = 1, .max = 1}}},
       .expected_result = BreakPropagator::kChanged},
      // Two breaks, both too short, infeasible.
      {.initial_breaks = {{.start = {.min = 0, .max = 90},
                           .end = {.min = 5, .max = 100},
                           .duration = {.min = 5, .max = 9},
                           .is_performed = {.min = 0, .max = 1}},
                          {.start = {.min = 0, .max = 90},
                           .end = {.min = 5, .max = 100},
                           .duration = {.min = 5, .max = 9},
                           .is_performed = {.min = 0, .max = 1}}},
       .interbreaks = {{25, 10}},
       .expected_result = BreakPropagator::kInfeasible},
  };
  BreakPropagator propagator(num_nodes);
  const int num_scenarios = interbreak_scenarios.size();
  for (int s = 0; s < num_scenarios; ++s) {
    SCOPED_TRACE(absl::StrCat("Scenario ", s));
    const InterbreakScenario& scenario = interbreak_scenarios[s];
    dimension.Revert();
    FillDimensionValuesWithPathScenario(initial_scenario, 0, dimension);
    EXPECT_TRUE(PropagateTransitAndSpan(0, dimension));
    ExpectDimensionValuesScenario(initial_scenario, 0, dimension);
    // Add breaks and propagate.
    dimension.MutableVehicleBreaks(0) = scenario.initial_breaks;
    EXPECT_EQ(
        propagator.PropagateInterbreak(0, dimension, scenario.interbreaks),
        scenario.expected_result);
    if (scenario.expected_result == BreakPropagator::kChanged) {
      ExpectBreaksScenario(scenario.expected_breaks, /*path=*/0, dimension);
      DimensionValuesPathScenario cumuls_post_propagation{
          .cumuls = scenario.expected_cumuls};
      ExpectDimensionValuesScenario(cumuls_post_propagation, 0, dimension);
    }
  }
}

}  // namespace operations_research::routing
