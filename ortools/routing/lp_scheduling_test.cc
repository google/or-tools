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

#include "ortools/routing/lp_scheduling.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <numeric>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/base/macros.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/inlined_vector.h"
#include "absl/log/check.h"
#include "absl/numeric/int128.h"
#include "absl/random/distributions.h"
#include "absl/random/random.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "google/protobuf/duration.pb.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/log_severity.h"
#include "ortools/base/strong_vector.h"
#include "ortools/base/types.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/glop/parameters.pb.h"
#include "ortools/routing/index_manager.h"
#include "ortools/routing/lp_scheduling_test_utils.h"
#include "ortools/routing/parameters.h"
#include "ortools/routing/parameters.pb.h"
#include "ortools/routing/routing.h"
#include "ortools/routing/types.h"
#include "ortools/util/piecewise_linear_function.h"
#include "ortools/util/piecewise_linear_function_test_utils.h"
#include "ortools/util/random_engine.h"
#include "ortools/util/random_subset.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/util/sorted_interval_list.h"
#include "ortools/util/testing_utils.h"

namespace operations_research::routing {
namespace {

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

using Attributes = Model::ResourceGroup::Attributes;
using ResourceGroup = Model::ResourceGroup;
using RCIndex = ResourceClassIndex;

TEST(PropagateCumulBoundsTest, PropagateBoundsBasicExample) {
  const int kNodes = 3;
  const int kVehicles = 1;
  const IndexManager manager(kNodes, kVehicles, NodeIndex(0));
  const std::string dim_name = "time";

  const std::vector<std::pair<int, int>> node_cumul_bounds = {{4, 7}, {7, 7}};

  for (int offset : {0, 5, 1000000}) {
    Model model(manager);
    model.AddConstantDimensionWithSlack(1, kint64max, kint64max, false,
                                        dim_name);
    Dimension* const dimension = model.GetMutableDimension(dim_name);

    dimension->CumulVar(model.Start(0))->SetMin(offset);
    for (int node = 0; node < node_cumul_bounds.size(); node++) {
      dimension->SetCumulVarRange(node + 1,
                                  node_cumul_bounds[node].first + offset,
                                  node_cumul_bounds[node].second + offset);
    }

    CumulBoundsPropagator propagator(dimension);

    // First case: The route is START-->1-->2-->END
    std::function<int(int)> next_accessor = [](int i) { return i + 1; };
    ASSERT_TRUE(propagator.PropagateCumulBounds(next_accessor, offset));
    const std::vector<std::pair<int64_t, int64_t>> expected_bounds = {
        {0, 5}, {4, 6}, {7, 7}, {8, kint64max}};
    ASSERT_EQ(expected_bounds.size(), dimension->cumuls().size());
    for (int i = 0; i < expected_bounds.size(); i++) {
      EXPECT_EQ(expected_bounds[i].first, propagator.CumulMin(i));
      EXPECT_EQ(expected_bounds[i].second, propagator.CumulMax(i));
    }

    // Second case: START-->2-->1-->END
    // Impossible because 1 cannot be reached in time from 2.
    const std::vector<int> next_index = {2, 3, 1};
    next_accessor = [&next_index](int i) { return next_index[i]; };
    EXPECT_FALSE(propagator.PropagateCumulBounds(next_accessor, offset));
  }
}

// Node1 can be in [20, 30] or [40, 50], Node2 in [10, 20] or [30, 40].
TEST(PropagateCumulBoundsTest, PropagateBoundsWithForbiddenIntervals) {
  const int kNodes = 3;
  const int kVehicles = 1;
  const IndexManager manager(kNodes, kVehicles, NodeIndex(0));
  const std::string kDimName = "time";

  const std::vector<std::pair<int, int>> node_cumul_bounds = {{20, 50},
                                                              {10, 40}};

  for (int offset : {0, 5, 1000000}) {
    Model model(manager);
    model.AddConstantDimensionWithSlack(5, kint64max, kint64max, false,
                                        kDimName);
    Dimension* const dimension = model.GetMutableDimension(kDimName);

    const std::vector<std::pair<std::vector<int64_t>, std::vector<int64_t>>>
        forbidden_start_ends = {
            {{offset + 0, offset + 31}, {offset + 19, offset + 39}},
            {{offset + 0, offset + 21}, {offset + 9, offset + 29}}};

    dimension->CumulVar(model.Start(0))->SetMin(offset);
    Solver* const solver = model.solver();
    for (int node = 0; node < node_cumul_bounds.size(); node++) {
      dimension->SetCumulVarRange(node + 1,
                                  node_cumul_bounds[node].first + offset,
                                  node_cumul_bounds[node].second + offset);
      solver->AddConstraint(solver->MakeNotMemberCt(
          dimension->CumulVar(node + 1), forbidden_start_ends[node].first,
          forbidden_start_ends[node].second));
    }

    model.CloseModel();

    CumulBoundsPropagator propagator(dimension);

    // First case: The route is START-->1-->2-->END
    std::function<int(int)> next_accessor = [](int i) { return i + 1; };
    ASSERT_TRUE(propagator.PropagateCumulBounds(next_accessor, offset));
    std::vector<std::pair<int64_t, int64_t>> expected_bounds = {
        {0, 25}, {20, 30}, {30, 40}, {35, kint64max}};
    ASSERT_EQ(expected_bounds.size(), dimension->cumuls().size());
    for (int i = 0; i < expected_bounds.size(); i++) {
      EXPECT_EQ(expected_bounds[i].first, propagator.CumulMin(i));
      EXPECT_EQ(expected_bounds[i].second, propagator.CumulMax(i));
    }

    // Second case: START-->2-->1-->END
    const std::vector<int> next_index = {2, 3, 1};
    next_accessor = [&next_index](int i) { return next_index[i]; };
    ASSERT_TRUE(propagator.PropagateCumulBounds(next_accessor, offset));
    expected_bounds = {{0, 35}, {20, 50}, {10, 40}, {25, kint64max}};
    for (int i = 0; i < expected_bounds.size(); i++) {
      EXPECT_EQ(expected_bounds[i].first, propagator.CumulMin(i));
      EXPECT_EQ(expected_bounds[i].second, propagator.CumulMax(i));
    }
  }
}

TEST(PropagateCumulBoundsTest, PropagateBoundsWithPrecedences) {
  const int kNodes = 4;
  const int kVehicles = 2;
  const IndexManager manager(kNodes, kVehicles, NodeIndex(0));
  const std::string dim_name = "time";

  for (int cumul_offset : {0, 5, 1000000}) {
    Model model(manager);
    model.AddConstantDimensionWithSlack(1, kint64max, kint64max, false,
                                        dim_name);
    Dimension* const dimension = model.GetMutableDimension(dim_name);

    for (IntVar* cumul_var : dimension->cumuls()) {
      cumul_var->SetMin(cumul_offset);
    }
    // First vehicle must end before 3, second vehicle must start after 5.
    dimension->CumulVar(model.End(0))->SetMax(cumul_offset + 3);
    dimension->CumulVar(model.Start(1))->SetMin(cumul_offset + 5);

    // Precedences: Node1 + 2 <= Node2, Node2 + 1 <= Node3, Node3 - 5 <= Node1.
    // The precedence graph has a negative cycle, so not necessarily infeasible.
    dimension->AddNodePrecedence(1, 2, /*offset*/ 2);
    dimension->AddNodePrecedence(2, 3, /*offset*/ 1);
    dimension->AddNodePrecedence(3, 1, /*offset*/ -5);

    CumulBoundsPropagator propagator(dimension);

    // First route: START-->1-->END
    // Second route: START-->2-->3-->END
    // The only feasible solution is to visit Node1 at 2, Node2 at 6, and Node3
    // at 7.
    std::vector<int> next_index = {1, 5, 3, 6, 2};
    const auto next_accessor = [&next_index](int i) { return next_index[i]; };
    ASSERT_TRUE(propagator.PropagateCumulBounds(next_accessor, cumul_offset));
    std::vector<std::pair<int64_t, int64_t>> expected_bounds = {
        {0, 1}, {2, 2}, {6, 6}, {7, 7}, {5, 5}, {3, 3}, {8, kint64max}};
    ASSERT_EQ(expected_bounds.size(), dimension->cumuls().size());
    for (int i = 0; i < expected_bounds.size(); i++) {
      EXPECT_EQ(expected_bounds[i].first, propagator.CumulMin(i));
      EXPECT_EQ(expected_bounds[i].second, propagator.CumulMax(i));
    }

    // We switch the two routes, i.e.
    // First route: START-->2-->3-->END
    // Second route: START-->1-->END
    // Impossible because Node1 will be reached after 5 and Node2 before 1, so
    // the first precedence can't be respected.
    next_index = {2, 6, 3, 5, 1};
    EXPECT_FALSE(propagator.PropagateCumulBounds(next_accessor, cumul_offset));

    // We now make Node1 unperformed, so its 2 precedences don't take effect and
    // the solution should be acceptable.
    next_index = {2, 1, 3, 5, 6};
    ASSERT_TRUE(propagator.PropagateCumulBounds(next_accessor, cumul_offset));
    expected_bounds = {{0, 0},        {kint64min, kint64max}, {1, 1},
                       {2, 2},        {5, kint64max},         {3, 3},
                       {6, kint64max}};
    for (int i = 0; i < expected_bounds.size(); i++) {
      EXPECT_EQ(expected_bounds[i].first, propagator.CumulMin(i));
      EXPECT_EQ(expected_bounds[i].second, propagator.CumulMax(i));
    }
  }
}

TEST(PropagateCumulBoundsTest, PropagateBoundsWithPickupDeliveryLimits) {
  const int kNodes = 5;
  const int kVehicles = 1;
  const IndexManager manager(kNodes, kVehicles, NodeIndex(0));
  const std::string dim_name = "time";

  const std::vector<std::pair<int64_t, int64_t>> node_cumul_bounds = {
      /*pickup*/ {1, 2}, /*pickup*/ {3, 4}, /*delivery*/ {1, 3},
      /*delivery*/ {5, kint64max}};

  for (int offset : {0, 5, 1000000}) {
    Model model(manager);
    const DisjunctionIndex pickup_disjunction = model.AddDisjunction({1, 2});
    const DisjunctionIndex delivery_disjunction = model.AddDisjunction({3, 4});
    model.AddPickupAndDeliverySets(pickup_disjunction, delivery_disjunction);

    model.AddConstantDimensionWithSlack(1, kint64max, kint64max, false,
                                        dim_name);
    Dimension* const dimension = model.GetMutableDimension(dim_name);

    dimension->CumulVar(model.Start(0))->SetMin(offset);
    for (int node = 0; node < node_cumul_bounds.size(); node++) {
      dimension->SetCumulVarRange(
          node + 1, node_cumul_bounds[node].first + offset,
          CapAdd(node_cumul_bounds[node].second, offset));
    }

    // Pickup-delivery limits: delivery <= pickup + 1;
    dimension->SetPickupToDeliveryLimitFunctionForPair(
        [](int, int) { return 1; }, 0);

    CumulBoundsPropagator propagator(dimension);

    // Route: START-->1-->3-->END
    std::vector<int> next_index = {1, 3, 2, 5, 4};
    const auto next_accessor = [&next_index](int i) { return next_index[i]; };
    ASSERT_TRUE(propagator.PropagateCumulBounds(next_accessor, offset));
    std::vector<std::pair<int64_t, int64_t>> expected_bounds = {
        {0, 1},
        {1, 2},
        {kint64min, kint64max},
        {2, 3},
        {kint64min, kint64max},
        {3, kint64max}};
    ASSERT_EQ(expected_bounds.size(), dimension->cumuls().size());
    for (int i = 0; i < expected_bounds.size(); i++) {
      EXPECT_EQ(expected_bounds[i].first, propagator.CumulMin(i));
      EXPECT_EQ(expected_bounds[i].second, propagator.CumulMax(i));
    }

    // We change the delivery, i.e.
    // START-->1-->4-->END
    // Impossible because the pickup/delivery limit would not be respected.
    next_index = {1, 4, 2, 3, 5};
    EXPECT_FALSE(propagator.PropagateCumulBounds(next_accessor, offset));

    // Second pickup -> first delivery is also impossible because of the bounds.
    // START-->2-->3-->END
    next_index = {2, 1, 3, 5, 4};
    EXPECT_FALSE(propagator.PropagateCumulBounds(next_accessor, offset));

    // Second pickup -> second delivery is OK.
    // START-->2-->4-->END
    next_index = {2, 1, 4, 3, 5};
    ASSERT_TRUE(propagator.PropagateCumulBounds(next_accessor, offset));
    expected_bounds = {{0, 3}, {kint64min, kint64max},
                       {4, 4}, {kint64min, kint64max},
                       {5, 5}, {6, kint64max}};
    for (int i = 0; i < expected_bounds.size(); i++) {
      EXPECT_EQ(expected_bounds[i].first, propagator.CumulMin(i));
      EXPECT_EQ(expected_bounds[i].second, propagator.CumulMax(i));
    }
  }
}

TEST(PropagateCumulBoundsTest, PickupDeliveriesWithAndWithoutLimits) {
  const int kNodes = 5;
  const int kVehicles = 1;
  const IndexManager manager(kNodes, kVehicles, NodeIndex(0));
  const std::string dim_name = "time";

  for (int offset : {0, 5, 1000000}) {
    Model model(manager);
    model.AddConstantDimensionWithSlack(1, kint64max, kint64max, false,
                                        dim_name);
    Dimension* const dimension = model.GetMutableDimension(dim_name);
    dimension->CumulVar(model.Start(0))->SetMin(offset);

    // Pickup/delivery pairs: {{{1}, {2}}, {{3}, {4}}}.
    for (int pickup_index = 1; pickup_index < kNodes; pickup_index += 2) {
      model.AddPickupAndDelivery(pickup_index, pickup_index + 1);
    }
    // Pickup-delivery limit for first pair: delivery <= pickup + 2;
    dimension->SetPickupToDeliveryLimitFunctionForPair(
        [](int, int) { return 2; }, 0);

    CumulBoundsPropagator propagator(dimension);

    // Route: START-->1-->2-->3-->4-->END, OK
    std::vector<int> next_index = {1, 2, 3, 4, 5};
    const auto next_accessor = [&next_index](int i) { return next_index[i]; };
    EXPECT_TRUE(propagator.PropagateCumulBounds(next_accessor, offset));

    // Route: START-->1-->3-->2-->4-->END, OK
    next_index = {1, 3, 4, 2, 5};
    EXPECT_TRUE(propagator.PropagateCumulBounds(next_accessor, offset));

    // Route: START-->1-->3-->4-->2-->END, NOT OK
    next_index = {1, 3, 5, 4, 2};
    EXPECT_FALSE(propagator.PropagateCumulBounds(next_accessor, offset));

    // Route: START-->3-->1-->4-->2-->END, OK
    next_index = {3, 4, 5, 1, 2};
    EXPECT_TRUE(propagator.PropagateCumulBounds(next_accessor, offset));

    // Route: START-->3-->1-->2-->4-->END, OK
    next_index = {3, 2, 4, 1, 5};
    EXPECT_TRUE(propagator.PropagateCumulBounds(next_accessor, offset));
  }
}

TEST(PropagateCumulBoundsTest, PropagateBoundsWithSlacks) {
  const int kNodes = 4;
  const int kVehicles = 1;
  const IndexManager manager(kNodes, kVehicles, NodeIndex(0));
  const std::string dim_name = "time";
  const int slack_max = 15;

  for (int offset : {0, 5, 1000000}) {
    Model model(manager);
    model.AddConstantDimensionWithSlack(5, kint64max, slack_max, false,
                                        dim_name);
    Dimension* dimension = model.GetMutableDimension(dim_name);

    dimension->CumulVar(model.Start(0))->SetMin(offset);
    const std::vector<std::pair<int64_t, int64_t>> cumul_ranges = {
        {25, 30}, {51, kint64max - offset}, {0, 30}};
    for (int node = 0; node < cumul_ranges.size(); node++) {
      dimension->CumulVar(node + 1)->SetRange(
          cumul_ranges[node].first + offset,
          cumul_ranges[node].second + offset);
    }

    CumulBoundsPropagator propagator(dimension);

    // Route: START-->1-->2-->END.
    // Impossible because of the slack max.
    std::vector<int> next_index = {1, 2, 4, 3};
    const auto next_accessor = [&next_index](int i) { return next_index[i]; };
    EXPECT_FALSE(propagator.PropagateCumulBounds(next_accessor, offset));

    // Route: START-->1-->3-->END. OK
    next_index = {1, 3, 2, 4};
    ASSERT_TRUE(propagator.PropagateCumulBounds(next_accessor, offset));
    std::vector<std::pair<int64_t, int64_t>> expected_bounds = {
        {5, 20}, {25, 25}, {kint64min, kint64max}, {30, 30}, {35, 50}};
    for (int i = 0; i < expected_bounds.size(); i++) {
      EXPECT_EQ(expected_bounds[i].first, propagator.CumulMin(i));
      EXPECT_EQ(expected_bounds[i].second, propagator.CumulMax(i));
    }

    // Route: START-->1-->3-->END, but with slack[1].min == 1.
    // Not OK because 3 can't be reached before 31.
    dimension->SlackVar(1)->SetMin(1);
    EXPECT_FALSE(propagator.PropagateCumulBounds(next_accessor, offset));
  }
}

TEST(PropagateCumulBoundsTest, WithVehicleSpanUpperBound) {
  const int kNodes = 4;
  const int kVehicles = 1;
  const IndexManager manager(kNodes, kVehicles, NodeIndex(0));
  const std::string dim_name = "time";
  const int kVehicleSpanUpperBound = 3;
  const int kMaxStart = 20;
  const int kExpectedMaxEnd = kMaxStart + kVehicleSpanUpperBound;

  for (int offset : {0, 5, 1000000}) {
    Model model(manager);
    model.AddConstantDimensionWithSlack(1, kint64max, kint64max, false,
                                        dim_name);
    Dimension* dimension = model.GetMutableDimension(dim_name);

    dimension->CumulVar(model.Start(0))->SetRange(offset, offset + kMaxStart);
    dimension->SetSpanUpperBoundForVehicle(kVehicleSpanUpperBound, 0);

    CumulBoundsPropagator propagator(dimension);

    // Route: START-->2-->END. OK
    std::vector<int> next_index = {2, 1, 4, 3};
    const auto next_accessor = [&next_index](int i) { return next_index[i]; };
    ASSERT_TRUE(propagator.PropagateCumulBounds(next_accessor, offset));
    std::vector<std::pair<int64_t, int64_t>> expected_bounds = {
        {0, kMaxStart},
        {kint64min, kint64max},
        {1, kExpectedMaxEnd - 1},
        {kint64min, kint64max},
        {2, kExpectedMaxEnd}};
    for (int i = 0; i < expected_bounds.size(); i++) {
      EXPECT_EQ(expected_bounds[i].first, propagator.CumulMin(i));
      EXPECT_EQ(expected_bounds[i].second, propagator.CumulMax(i));
    }

    // Route: START-->1-->3-->END. OK
    next_index = {1, 3, 2, 4};
    ASSERT_TRUE(propagator.PropagateCumulBounds(next_accessor, offset));
    expected_bounds = {{0, kMaxStart},
                       {1, kExpectedMaxEnd - 2},
                       {kint64min, kint64max},
                       {2, kExpectedMaxEnd - 1},
                       {3, kExpectedMaxEnd}};
    for (int i = 0; i < expected_bounds.size(); i++) {
      EXPECT_EQ(expected_bounds[i].first, propagator.CumulMin(i));
      EXPECT_EQ(expected_bounds[i].second, propagator.CumulMax(i));
    }

    // Route: START-->1-->2-->3-->END. Violates vehicle span upper bound.
    next_index = {1, 2, 3, 4};
    EXPECT_FALSE(propagator.PropagateCumulBounds(next_accessor, offset));
  }
}

// Tests potential overflow issues with quadratic costs. This test fails with
// sat::SatParameters.mip_max_bound = 1e8.
TEST(QuadraticCostTest, SimpleModel) {
  const IndexManager manager(2, 1, NodeIndex(0));
  Model model(manager);
  model.AddConstantDimension(30000, /*capacity*/ 1e7,
                             /*fix_start_cumul_to_zero*/ false, "dim");
  Dimension* const dimension = model.GetMutableDimension("dim");
  dimension->SetQuadraticCostSoftSpanUpperBoundForVehicle({/*bound*/ 1000,
                                                           /*cost*/ 1},
                                                          /*vehicle*/ 0);
  model.CloseModel();

  DimensionCumulOptimizerCore optimizer(dimension,
                                        /*use_precedence_propagator*/ false);
  SearchStats stats;
  CPSatWrapper solver(&stats);

  // Routes: Start(v0) -> 1 -> End(v0).
  const std::vector<int> kNexts = {1, 2};
  const auto next_accessor = [&kNexts](int index) { return kNexts[index]; };

  std::vector<int64_t> cumul_values;
  std::vector<int64_t> break_values;
  int64_t cost = -1;
  int64_t transit_cost = -1;
  const DimensionSchedulingStatus status =
      optimizer.OptimizeSingleRouteWithResource(
          /*vehicle*/ 0, /*solve_duration_ratio=*/1.0, next_accessor, {},
          /*resource=*/nullptr, /*optimize_vehicle_costs=*/true, &solver,
          &cumul_values, &break_values, &cost, &transit_cost);
  EXPECT_EQ(DimensionSchedulingStatus::OPTIMAL, status);
  EXPECT_EQ(3481000000, cost);
  EXPECT_EQ(0, transit_cost);
  EXPECT_FALSE(cumul_values.empty());
  EXPECT_TRUE(break_values.empty());
  EXPECT_EQ(stats.num_cp_sat_calls_in_lp_scheduling, 1);
  EXPECT_EQ(stats.num_glop_calls_in_lp_scheduling, 0);
  EXPECT_EQ(stats.num_min_cost_flow_calls, 0);
}

// Make sure breaks necessarily outside the route are handled properly in the
// model. In this test, the second break starts at the end of the vehicle end
// time.
TEST(BreakDistanceDurationTest, BreaksWithDistanceBreakDurationOutsideRoute) {
  const IndexManager manager(1, 1, NodeIndex(0));
  Model model(manager);
  model.AddConstantDimensionWithSlack(0, /*capacity*/ 59557,
                                      /*slack_max*/ 60000,
                                      /*fix_start_cumul_to_zero*/ false, "dim");
  Dimension* const dimension = model.GetMutableDimension("dim");
  dimension->CumulVar(model.Start(0))->SetMin(1000);
  IntervalVar* const break_before =
      model.solver()->MakeFixedDurationIntervalVar(42, 232, 31, false,
                                                   "break_before");
  IntervalVar* const break_inside =
      model.solver()->MakeFixedDurationIntervalVar(16557, 36432, 531, false,
                                                   "break_inside");
  IntervalVar* const break_after = model.solver()->MakeFixedDurationIntervalVar(
      59557, 59588, 129, false, "break_after");
  dimension->SetBreakIntervalsOfVehicle(
      {break_before, break_inside, break_after},
      /*vehicle*/ 0, {0, 0, 0});
  dimension->SetBreakDistanceDurationOfVehicle(
      /*distance*/ 4920, /*duration*/ 40160, /*vehicle*/ 0);
  model.SetVehicleUsedWhenEmpty(true, 0);
  model.CloseModel();

  DimensionCumulOptimizerCore optimizer(dimension,
                                        /*use_precedence_propagator*/ false);
  SearchStats stats;
  CPSatWrapper solver(&stats);

  // Routes: Start(v0) -> End(v0).
  const std::vector<int> kNexts = {1};
  const auto next_accessor = [&kNexts](int index) { return kNexts[index]; };

  std::vector<int64_t> cumul_values;
  std::vector<int64_t> break_values;
  int64_t cost = -1;
  int64_t transit_cost = -1;
  const DimensionSchedulingStatus status =
      optimizer.OptimizeSingleRouteWithResource(
          /*vehicle*/ 0, /*solve_duration_ratio=*/1.0, next_accessor, {},
          /*resource=*/nullptr, /*optimize_vehicle_costs=*/true, &solver,
          &cumul_values, &break_values, &cost, &transit_cost);
  EXPECT_EQ(DimensionSchedulingStatus::OPTIMAL, status);
  EXPECT_EQ(0, cost);
  EXPECT_EQ(0, transit_cost);
  EXPECT_FALSE(cumul_values.empty());
  EXPECT_FALSE(break_values.empty());
  EXPECT_EQ(stats.num_cp_sat_calls_in_lp_scheduling, 1);
  EXPECT_EQ(stats.num_glop_calls_in_lp_scheduling, 0);
  EXPECT_EQ(stats.num_min_cost_flow_calls, 0);
}

// Making sure infeasible models get handled properly. The model below has a
// single vehicle, with a single resource and a single node to visit. The
// vehicle has a break which makes the route infeasible. This break requires
// the CP-SAT solver to be run as the infeasibility cannot be detected without
// it.
TEST(ResourceDimensionCumulOptimizerTest, InfeasibleResourcesTest) {
  const int kNodes = 2;
  const int kVehicles = 1;
  const IndexManager manager(kNodes, kVehicles, NodeIndex(0));

  Model model(manager);
  const std::string kDimName = "time";
  model.AddConstantDimensionWithSlack(10,
                                      /*capacity*/ 1000, /*slack_max*/ 1000,
                                      /*fix_start_cumul_to_zero*/ false,
                                      kDimName);
  Dimension* const dimension = model.GetMutableDimension(kDimName);

  // Add a resource with start/end domains for the dimension.
  Model::ResourceGroup& resource_group = *model.AddResourceGroup();
  const int resource = resource_group.AddResource(
      Attributes(Domain(0, 20), Domain(0, 22)), dimension);
  // The vehicle requires a resource from the group and a break.
  resource_group.NotifyVehicleRequiresAResource(/*vehicle*/ 0);
  // The break has to happen in the middle of the route (due to its window).
  IntervalVar* const break_interval =
      model.solver()->MakeFixedDurationIntervalVar(12, 15, 5, false, "b");
  dimension->SetBreakIntervalsOfVehicle({break_interval}, /*vehicle*/ 0,
                                        {0, 0, 0});
  model.CloseModel();

  DimensionCumulOptimizerCore optimizer(dimension,
                                        /*use_precedence_propagator*/ false);
  SearchStats stats;
  CPSatWrapper solver(&stats);

  // Routes: Start(v0) -> 1 -> End(v0).
  const std::vector<int> kNexts = {1, 2};
  const auto next_accessor = [&kNexts](int index) { return kNexts[index]; };

  int64_t cost = 0;
  int64_t transit_cost;
  std::vector<int64_t> cumul_values;
  std::vector<int64_t> break_values;
  const DimensionSchedulingStatus status =
      optimizer.OptimizeSingleRouteWithResource(
          /*vehicle=*/0, /*solve_duration_ratio=*/1.0, next_accessor, {},
          &resource_group.GetResources()[resource],
          /*optimize_vehicle_costs=*/true, &solver, &cumul_values,
          &break_values, &cost, &transit_cost);
  EXPECT_EQ(status, DimensionSchedulingStatus::INFEASIBLE);
  EXPECT_EQ(cost, -1);
  EXPECT_TRUE(cumul_values.empty());
  EXPECT_TRUE(break_values.empty());
  EXPECT_EQ(stats.num_cp_sat_calls_in_lp_scheduling, 1);
  EXPECT_EQ(stats.num_glop_calls_in_lp_scheduling, 0);
  EXPECT_EQ(stats.num_min_cost_flow_calls, 0);
}

// Similar to the previous test, but the infeasibility is caused by the span
// upper bound of the resource.
TEST(ResourceDimensionCumulOptimizerTest, InfeasibleDueToSpanUBResourcesTest) {
  const int kNodes = 2;
  const int kVehicles = 1;
  const IndexManager manager(kNodes, kVehicles, NodeIndex(0));

  for (int64_t span_upper_bound : {19, 1000}) {
    Model model(manager);
    const std::string kDimName = "time";
    model.AddConstantDimensionWithSlack(10,
                                        /*capacity*/ 1000, /*slack_max*/ 1000,
                                        /*fix_start_cumul_to_zero*/ false,
                                        kDimName);
    Dimension* const dimension = model.GetMutableDimension(kDimName);

    // Add a resource with start/end domains for the dimension.
    Model::ResourceGroup& resource_group = *model.AddResourceGroup();
    const int resource = resource_group.AddResource(
        Attributes(Domain(0, 1000), Domain(0, 1000), span_upper_bound),
        dimension);
    // The vehicle requires a resource from the group and a break.
    resource_group.NotifyVehicleRequiresAResource(/*vehicle*/ 0);
    // The break has to happen in the middle of the route (due to its window).
    IntervalVar* const break_interval =
        model.solver()->MakeFixedDurationIntervalVar(12, 15, 5, false, "b");
    dimension->SetBreakIntervalsOfVehicle({break_interval}, /*vehicle*/ 0,
                                          {0, 0, 0});
    model.CloseModel();

    DimensionCumulOptimizerCore optimizer(dimension,
                                          /*use_precedence_propagator*/ false);
    SearchStats stats;
    CPSatWrapper solver(&stats);

    // Routes: Start(v0) -> 1 -> End(v0).
    const std::vector<int> kNexts = {1, 2};
    const auto next_accessor = [&kNexts](int index) { return kNexts[index]; };

    int64_t cost = 0;
    int64_t transit_cost;
    std::vector<int64_t> cumul_values;
    std::vector<int64_t> break_values;
    const DimensionSchedulingStatus status =
        optimizer.OptimizeSingleRouteWithResource(
            /*vehicle=*/0, /*solve_duration_ratio=*/1.0, next_accessor, {},
            &resource_group.GetResources()[resource],
            /*optimize_vehicle_costs=*/true, &solver, &cumul_values,
            &break_values, &cost, &transit_cost);
    if (span_upper_bound == 19) {
      EXPECT_EQ(status, DimensionSchedulingStatus::INFEASIBLE);
      EXPECT_EQ(cost, -1);
      EXPECT_TRUE(cumul_values.empty());
      EXPECT_TRUE(break_values.empty());
      EXPECT_EQ(stats.num_cp_sat_calls_in_lp_scheduling, 1);
      EXPECT_EQ(stats.num_glop_calls_in_lp_scheduling, 0);
      EXPECT_EQ(stats.num_min_cost_flow_calls, 0);
    } else {
      EXPECT_EQ(status, DimensionSchedulingStatus::OPTIMAL);
      EXPECT_EQ(cost, 0);
      EXPECT_FALSE(cumul_values.empty());
      EXPECT_FALSE(break_values.empty());
      EXPECT_EQ(stats.num_cp_sat_calls_in_lp_scheduling, 1);
      EXPECT_EQ(stats.num_glop_calls_in_lp_scheduling, 0);
      EXPECT_EQ(stats.num_min_cost_flow_calls, 0);
    }
  }
}

// Tests that there's no overflow with a high coefficient in the objective
// (see b/206758911).
TEST(RoutingCPSatWrapperTest, NoOverflowWithInfiniteObjectiveCoefficient) {
  const int kNodes = 2;
  const int kVehicles = 1;
  const IndexManager manager(kNodes, kVehicles, NodeIndex(0));

  Model model(manager);
  const std::string kDimName = "time";
  model.AddConstantDimensionWithSlack(10,
                                      /*capacity*/ 1000, /*slack_max*/ 1000,
                                      /*fix_start_cumul_to_zero*/ false,
                                      kDimName);
  Dimension* const dimension = model.GetMutableDimension(kDimName);
  // Set a high span cost on the vehicle.
  dimension->SetSpanCostCoefficientForVehicle(kint64max, 0);
  model.CloseModel();

  DimensionCumulOptimizerCore optimizer(dimension,
                                        /*use_precedence_propagator*/ false);
  SearchStats stats;
  CPSatWrapper solver(&stats);

  // Routes: Start(v0) -> 1 -> End(v0).
  const std::vector<int> kNexts = {1, 2};
  const auto next_accessor = [&kNexts](int index) { return kNexts[index]; };

  // NOTE: The call should go through OK (i.e. no crash), and handle the large
  // cost coefficients properly.
  EXPECT_EQ(
      optimizer.OptimizeAndPack(next_accessor, {}, &solver, nullptr, nullptr),
      DimensionSchedulingStatus::OPTIMAL);
  EXPECT_EQ(stats.num_cp_sat_calls_in_lp_scheduling, 3);
  EXPECT_EQ(stats.num_glop_calls_in_lp_scheduling, 0);
  EXPECT_EQ(stats.num_min_cost_flow_calls, 0);
}

// Tests that the optimizer tries to reapply previous schedules to the
// current problem.
TEST(RoutingCPSatWrapperTest, ScheduleHint) {
  SearchStats stats;
  CPSatWrapper solver(&stats);
  // Schedule a two-node route, with singleton domains to force the times.
  {
    const int start_var = solver.CreateNewPositiveVariable();
    solver.SetVariableBounds(start_var, 42, 42);
    const int end_var = solver.CreateNewPositiveVariable();
    solver.SetVariableBounds(end_var, 123, 123);
    solver.AddLinearConstraint(0, 100, {{start_var, -1}, {end_var, 1}});
    solver.AddRoute({0, 1}, {start_var, end_var});
    EXPECT_EQ(solver.Solve(absl::Seconds(1)),
              DimensionSchedulingStatus::OPTIMAL);
    EXPECT_EQ(solver.GetVariableValue(start_var), 42);
    EXPECT_EQ(solver.GetVariableValue(end_var), 123);
    EXPECT_EQ(stats.num_cp_sat_calls_in_lp_scheduling, 1);
    EXPECT_EQ(stats.num_glop_calls_in_lp_scheduling, 0);
    EXPECT_EQ(stats.num_min_cost_flow_calls, 0);
  }

  // We disable exploit_integer_lp_solution, otherwise the root LP just give us
  // a feasible solution right away.
  solver.MutableParameters()->set_exploit_integer_lp_solution(false);

  // Schedule a three-node route, with large domains, and check that previous
  // nodes are scheduled at the same time as in the first problem.
  {
    solver.Clear();
    const int start_var = solver.CreateNewPositiveVariable();
    solver.SetVariableBounds(start_var, 0, 1000);
    const int middle1_var = solver.CreateNewPositiveVariable();
    solver.SetVariableBounds(middle1_var, 0, 1000);
    solver.AddLinearConstraint(10, 1000, {{start_var, -1}, {middle1_var, 1}});
    const int middle2_var = solver.CreateNewPositiveVariable();
    solver.SetVariableBounds(middle2_var, 0, 1000);
    solver.AddLinearConstraint(10, 1000, {{middle1_var, -1}, {middle2_var, 1}});
    const int end_var = solver.CreateNewPositiveVariable();
    solver.SetVariableBounds(end_var, 0, 1000);
    solver.AddLinearConstraint(10, 1000, {{middle2_var, -1}, {end_var, 1}});

    // Add some constraints that prevent the presolve from solving the problem
    // without the hint.
    solver.AddLinearConstraint(
        1, 1, {{start_var, 1}, {middle1_var, -2}, {end_var, 1}});
    solver.AddRoute({0, 2, 3, 1},
                    {start_var, middle1_var, middle2_var, end_var});
    EXPECT_EQ(solver.Solve(absl::Seconds(1)),
              DimensionSchedulingStatus::OPTIMAL);

    // If these fail due to progress in the CPSAT presolve, find another way
    // to test the hint.
    EXPECT_EQ(solver.GetVariableValue(start_var), 42);
    EXPECT_EQ(solver.GetVariableValue(end_var), 123);
    EXPECT_EQ(stats.num_cp_sat_calls_in_lp_scheduling, 2);
    EXPECT_EQ(stats.num_glop_calls_in_lp_scheduling, 0);
    EXPECT_EQ(stats.num_min_cost_flow_calls, 0);
  }
}

std::unique_ptr<Model> CreateSimpleModelWithResources(
    const std::string& dim_name, bool add_span_upper_bound,
    bool duplicate_last_resource = false) {
  const int kNodes = 3;
  const int kVehicles = 2;
  const IndexManager manager(kNodes, kVehicles, NodeIndex(0));

  auto model = std::make_unique<Model>(manager);
  model->AddConstantDimensionWithSlack(1, /*capacity*/ 1000, /*slack_max*/ 1000,
                                       /*fix_start_cumul_to_zero*/ false,
                                       dim_name);
  Dimension* const dimension = model->GetMutableDimension(dim_name);

  // Add a soft upper bound on all start times.
  for (int index = 0; index < dimension->cumuls().size(); index++) {
    dimension->SetCumulVarSoftUpperBound(index, /*upper_bound*/ 0,
                                         /*coefficient*/ index + 1);
  }

  // Add resources with start/end domains and span upper bounds for the
  // dimension.
  Model::ResourceGroup& resource_group = *model->AddResourceGroup();
  resource_group.AddResource(Attributes(Domain(10, 20), Domain(20, 30)),
                             dimension);
  resource_group.AddResource(Attributes(Domain(5, 100), Domain(5, kint64max)),
                             dimension);
  for (int i = 0; i < (duplicate_last_resource ? 2 : 1); i++) {
    resource_group.AddResource(Attributes(Domain(-1, 10), Domain(6, 12),
                                          add_span_upper_bound ? 3 : kint64max),
                               dimension);
  }
  // Both vehicles require a resource from the group.
  for (int v = 0; v < model->vehicles(); v++) {
    resource_group.NotifyVehicleRequiresAResource(v);
  }

  model->CloseModel();
  return model;
}

TEST(GlobalDimensionCumulOptimizerTest, SimpleResourcesTest) {
  const std::string kDimName = "time";
  for (bool add_span_upper_bound : {false, true}) {
    std::unique_ptr<Model> model =
        CreateSimpleModelWithResources(kDimName, add_span_upper_bound);
    Dimension* const dimension = model->GetMutableDimension(kDimName);

    SearchStats stats;
    GlobalDimensionCumulOptimizer optimizer(
        dimension, RoutingSearchParameters::SCHEDULING_CP_SAT, &stats);

    // Routes: Start(v0) -> 1 -> End(v0). Start(v1) -> 2 -> End(v1).
    const std::vector<int> kNexts = {1, 4, 5, 2};
    const auto next_accessor = [&kNexts](int index) { return kNexts[index]; };

    std::vector<int64_t> cumuls;
    std::vector<std::vector<int>> resource_indices_per_group;
    EXPECT_EQ(DimensionSchedulingStatus::OPTIMAL,
              optimizer.ComputeCumuls(next_accessor, {}, &cumuls, nullptr,
                                      &resource_indices_per_group));
    if (add_span_upper_bound) {
      // Expected times:
      // Start(v0): 5 -> Node1: 6 -> End(v0): 7 - Resource #1
      // Start(v1): 3 -> Node2: 4 -> End(v1): 6 - Resource #2
      // Second route is compressed to fit the span upper bound.
      EXPECT_THAT(cumuls, ElementsAre(5, 6, 4, 3, 7, 6));
    } else {
      // Expected times:
      // Start(v0): 5 -> Node1: 6 -> End(v0): 7 - Resource #1
      // Start(v1): 0 -> Node2: 1 -> End(v1): 6 - Resource #2
      EXPECT_THAT(cumuls, ElementsAre(5, 6, 1, 0, 7, 6));
    }
    ASSERT_EQ(resource_indices_per_group.size(), 1);
    EXPECT_THAT(resource_indices_per_group[0], ElementsAre(1, 2));
    EXPECT_EQ(stats.num_cp_sat_calls_in_lp_scheduling, 1);
    EXPECT_EQ(stats.num_glop_calls_in_lp_scheduling, 0);
    EXPECT_EQ(stats.num_min_cost_flow_calls, 0);
  }
}

// Same as above but only resource #2 is allowed for vehicle #0.
TEST(GlobalDimensionCumulOptimizerTest, HolesInResourceVarDomainsTest) {
  const std::string kDimName = "time";
  for (bool add_span_upper_bound : {false, true}) {
    for (bool duplicate_last_resource : {false, true}) {
      std::unique_ptr<Model> model = CreateSimpleModelWithResources(
          kDimName, add_span_upper_bound, duplicate_last_resource);
      Dimension* const dimension = model->GetMutableDimension(kDimName);

      // Mark resource #2 as the only possible resource for vehicle #0.
      model->ResourceVar(0, 0)->SetValue(2);

      SearchStats stats;
      GlobalDimensionCumulOptimizer optimizer(
          dimension, RoutingSearchParameters::SCHEDULING_CP_SAT, &stats);

      // Routes: Start(v0) -> 1 -> End(v0). Start(v1) -> 2 -> End(v1).
      const std::vector<int> kNexts = {1, 4, 5, 2};
      const auto next_accessor = [&kNexts](int index) { return kNexts[index]; };

      std::vector<int64_t> cumuls;
      std::vector<std::vector<int>> resource_indices_per_group;
      EXPECT_EQ(DimensionSchedulingStatus::OPTIMAL,
                optimizer.ComputeCumuls(next_accessor, {}, &cumuls, nullptr,
                                        &resource_indices_per_group));
      ASSERT_EQ(resource_indices_per_group.size(), 1);
      if (add_span_upper_bound) {
        if (duplicate_last_resource) {
          // Expected times:
          // Start(v0): 3 -> Node1: 4 -> End(v0): 6  - Resource #2
          // Start(v1): 3 -> Node2: 4 -> End(v1): 6  - Resource #3
          EXPECT_THAT(cumuls, ElementsAre(3, 4, 4, 3, 6, 6));
          EXPECT_THAT(resource_indices_per_group[0], ElementsAre(2, 3));
        } else {
          // Expected times:
          // Start(v0): 3 -> Node1: 4 -> End(v0): 6  - Resource #2
          // Start(v1): 5 -> Node2: 6 -> End(v1): 7  - Resource #1
          EXPECT_THAT(cumuls, ElementsAre(3, 4, 6, 5, 6, 7));
          EXPECT_THAT(resource_indices_per_group[0], ElementsAre(2, 1));
        }
      } else {
        if (duplicate_last_resource) {
          // Expected times:
          // Start(v0): 0 -> Node1: 1 -> End(v0): 6  - Resource #2
          // Start(v1): 0 -> Node2: 1 -> End(v1): 6  - Resource #3
          EXPECT_THAT(cumuls, ElementsAre(0, 1, 1, 0, 6, 6));
          EXPECT_THAT(resource_indices_per_group[0], ElementsAre(2, 3));
        } else {
          // Expected times:
          // Start(v0): 0 -> Node1: 1 -> End(v0): 6  - Resource #2
          // Start(v1): 5 -> Node2: 6 -> End(v1): 7  - Resource #1
          EXPECT_THAT(cumuls, ElementsAre(0, 1, 6, 5, 6, 7));
          EXPECT_THAT(resource_indices_per_group[0], ElementsAre(2, 1));
        }
      }
      EXPECT_EQ(stats.num_cp_sat_calls_in_lp_scheduling, 1);
      EXPECT_EQ(stats.num_glop_calls_in_lp_scheduling, 0);
      EXPECT_EQ(stats.num_min_cost_flow_calls, 0);
    }
  }
}

TEST(GlobalDimensionCumulOptimizerTest, NotifyVehicleRequiresAResourceTest) {
  const int kNodes = 3;
  const int kVehicles = 2;
  const IndexManager manager(kNodes, kVehicles, NodeIndex(0));

  const bool kVehicleRequiresResource[][kVehicles] = {
      {true, true}, {false, true}, {true, false}, {false, false}};

  for (int i = 0; i < ABSL_ARRAYSIZE(kVehicleRequiresResource); i++) {
    Model model(manager);
    const std::string kDimName = "time";
    model.AddConstantDimensionWithSlack(
        1, /*capacity*/ 1000, /*slack_max*/ 1000,
        /*fix_start_cumul_to_zero*/ false, kDimName);
    Dimension* const dimension = model.GetMutableDimension(kDimName);

    // Add a single resource.
    Model::ResourceGroup& resource_group = *model.AddResourceGroup();
    resource_group.AddResource(Attributes(), dimension);

    int num_vehicles_requiring_resource = 0;
    for (int v = 0; v < model.vehicles(); v++) {
      // Make vehicles used even when empty.
      model.SetVehicleUsedWhenEmpty(true, v);
      if (kVehicleRequiresResource[i][v]) {
        num_vehicles_requiring_resource++;
        resource_group.NotifyVehicleRequiresAResource(v);
      }
    }

    model.CloseModel();

    SearchStats stats;
    GlobalDimensionCumulOptimizer optimizer(
        dimension, RoutingSearchParameters::SCHEDULING_CP_SAT, &stats);

    const std::vector<std::vector<int>> kNextsScenarios = {
        // Routes: Start(v0) -> 1 -> End(v0). Start(v1) -> 2 -> End(v1).
        {1, 4, 5, 2},
        // Routes empty. Vehicles are still used, so they need a resource.
        {4, 1, 2, 5}};
    const DimensionSchedulingStatus expected_status =
        num_vehicles_requiring_resource <= 1
            ? DimensionSchedulingStatus::OPTIMAL
            : DimensionSchedulingStatus::INFEASIBLE;
    for (const std::vector<int>& kNexts : kNextsScenarios) {
      std::vector<std::vector<int>> resource_indices_per_group;
      EXPECT_EQ(optimizer.ComputeCumuls(
                    [&kNexts](int index) { return kNexts[index]; }, {}, nullptr,
                    nullptr, &resource_indices_per_group),
                expected_status);
      if (expected_status == DimensionSchedulingStatus::INFEASIBLE) {
        EXPECT_TRUE(resource_indices_per_group.empty());
        continue;
      }
      if (num_vehicles_requiring_resource == 0) {
        EXPECT_TRUE(resource_indices_per_group.empty());
        continue;
      }
      ASSERT_EQ(resource_indices_per_group.size(), 1);
      const std::vector<int>& resource_indices = resource_indices_per_group[0];
      ASSERT_EQ(resource_indices.size(), model.vehicles());
      for (int v = 0; v < model.vehicles(); v++) {
        EXPECT_EQ(resource_indices[v], kVehicleRequiresResource[i][v] ? 0 : -1);
      }
    }
    EXPECT_EQ(
        stats.num_cp_sat_calls_in_lp_scheduling,
        kVehicleRequiresResource[i][0] + kVehicleRequiresResource[i][1] <= 1
            ? 2
            : 0);
    EXPECT_EQ(stats.num_glop_calls_in_lp_scheduling, 0);
    EXPECT_EQ(stats.num_min_cost_flow_calls, 0);
  }
}

TEST(GlobalDimensionCumulOptimizerTest, ResourcesWithDimensionOffsetTest) {
  const int kNodes = 3;
  const int kVehicles = 2;
  const IndexManager manager(kNodes, kVehicles, NodeIndex(0));
  const std::string kDimName = "time";

  for (int64_t span_upper_bound :
       {int64_t{1}, int64_t{2}, int64_t{3}, kint64max}) {
    for (int64_t offset : {10, 100, 200, 500, 1000}) {
      Model model(manager);
      model.AddConstantDimensionWithSlack(
          1, /*capacity*/ 1e6, /*slack_max*/ 1e6,
          /*fix_start_cumul_to_zero*/ false, kDimName);
      Dimension* const dimension = model.GetMutableDimension(kDimName);

      // Add 2 resources.
      Model::ResourceGroup& resource_group = *model.AddResourceGroup();
      resource_group.AddResource(
          Attributes(Domain::AllValues(), Domain(50, 200), span_upper_bound),
          dimension);
      resource_group.AddResource(
          Attributes(Domain(150, 500), Domain::AllValues(), span_upper_bound),
          dimension);

      // Set vehicle min start times based on offset.
      for (int v = 0; v < model.vehicles(); v++) {
        dimension->CumulVar(model.Start(v))->SetMin(offset);
        resource_group.NotifyVehicleRequiresAResource(v);
      }

      model.CloseModel();

      SearchStats stats;
      GlobalDimensionCumulOptimizer optimizer(
          dimension, RoutingSearchParameters::SCHEDULING_CP_SAT, &stats);

      // Routes: Start(v0) -> 1 -> End(v0). Start(v1) -> 2 -> End(v1).
      std::vector<int> kNexts = {1, 4, 5, 2};
      EXPECT_EQ(optimizer.ComputeCumuls(
                    [&kNexts](int index) { return kNexts[index]; }, {}, nullptr,
                    nullptr, nullptr),
                span_upper_bound >= 2 && offset <= 200 - 2
                    ? DimensionSchedulingStatus::OPTIMAL
                    : DimensionSchedulingStatus::INFEASIBLE);

      // Nodes on first vehicle: Start(v0) -> 1 -> 2 -> End(v0).
      kNexts = {1, 2, 4, 5};
      EXPECT_EQ(optimizer.ComputeCumuls(
                    [&kNexts](int index) { return kNexts[index]; }, {}, nullptr,
                    nullptr, nullptr),
                span_upper_bound >= 3 && offset <= 500
                    ? DimensionSchedulingStatus::OPTIMAL
                    : DimensionSchedulingStatus::INFEASIBLE);

      // Nodes on second vehicle: Start(v1) -> 1 -> 2 -> End(v1).
      kNexts = {4, 2, 5, 1};
      EXPECT_EQ(optimizer.ComputeCumuls(
                    [&kNexts](int index) { return kNexts[index]; }, {}, nullptr,
                    nullptr, nullptr),
                span_upper_bound >= 3 && offset <= 500
                    ? DimensionSchedulingStatus::OPTIMAL
                    : DimensionSchedulingStatus::INFEASIBLE);
      EXPECT_EQ(stats.num_cp_sat_calls_in_lp_scheduling, 3);
      EXPECT_EQ(stats.num_glop_calls_in_lp_scheduling, 0);
      EXPECT_EQ(stats.num_min_cost_flow_calls, 0);
    }
  }
}

TEST(GlobalDimensionCumulOptimizerTest, TwoResourceGroupsForSameDimensionTest) {
  const int kNodes = 3;
  const int kVehicles = 2;
  const IndexManager manager(kNodes, kVehicles, NodeIndex(0));

  for (int64_t span_upper_bound : {int64_t{1}, int64_t{2}, kint64max}) {
    Model model(manager);
    const std::string kDimName = "time";
    model.AddConstantDimension(1, /*capacity*/ 1000,
                               /*fix_start_cumul_to_zero*/ false, kDimName);
    Dimension* const dimension = model.GetMutableDimension(kDimName);

    // First vehicle can start between 8 and 10, second vehicle from 4 to 5.
    const std::vector<std::pair<int, int>> start_cumul_bounds = {{8, 10},
                                                                 {4, 5}};
    for (int v = 0; v < kVehicles; v++) {
      dimension->CumulVar(model.Start(v))
          ->SetRange(start_cumul_bounds[v].first, start_cumul_bounds[v].second);
    }

    // First ResourceGroup.
    // The first resource can only be assigned to the first vehicle.
    Model::ResourceGroup* resource_group = model.AddResourceGroup();
    resource_group->AddResource(
        Attributes(Domain(10, 13), Domain::AllValues(), span_upper_bound),
        dimension);
    resource_group->AddResource(Attributes(), dimension);
    for (int v = 0; v < model.vehicles(); v++) {
      resource_group->NotifyVehicleRequiresAResource(v);
    }

    // Second ResourceGroup.
    // The first resource can only be assigned to the second vehicle.
    resource_group = model.AddResourceGroup();
    resource_group->AddResource(Attributes(Domain::AllValues(), Domain(7, 11)),
                                dimension);
    resource_group->AddResource(Attributes(), dimension);
    for (int v = 0; v < model.vehicles(); v++) {
      resource_group->NotifyVehicleRequiresAResource(v);
    }

    model.CloseModel();

    SearchStats stats;
    GlobalDimensionCumulOptimizer optimizer(
        dimension, RoutingSearchParameters::SCHEDULING_CP_SAT, &stats);

    // Routes: Start(v0) -> 1 -> End(v0). Start(v1) -> 2 -> End(v1).
    const std::vector<int> kNexts = {1, 4, 5, 2};
    const auto next_accessor = [&kNexts](int index) { return kNexts[index]; };

    if (span_upper_bound < 2) {
      EXPECT_EQ(
          optimizer.ComputeCumuls(next_accessor, {}, nullptr, nullptr, nullptr),
          DimensionSchedulingStatus::INFEASIBLE);
      continue;
    }

    std::vector<int64_t> cumuls;
    std::vector<std::vector<int>> resource_indices_per_resource_group;
    EXPECT_EQ(DimensionSchedulingStatus::OPTIMAL,
              optimizer.ComputeCumuls(next_accessor, {}, &cumuls, nullptr,
                                      &resource_indices_per_resource_group));
    // Expected times:
    // Start(v0): 10 -> Node1: 11 -> End(v0): 12
    // Start(v1):  5 -> Node2:  6 -> End(v1):  7
    EXPECT_THAT(cumuls, ElementsAre(10, 11, 6, 5, 12, 7));
    ASSERT_EQ(resource_indices_per_resource_group.size(), 2);
    EXPECT_THAT(resource_indices_per_resource_group[0], ElementsAre(0, 1));
    EXPECT_THAT(resource_indices_per_resource_group[1], ElementsAre(1, 0));
    EXPECT_EQ(stats.num_cp_sat_calls_in_lp_scheduling, 1);
    EXPECT_EQ(stats.num_glop_calls_in_lp_scheduling, 0);
    EXPECT_EQ(stats.num_min_cost_flow_calls, 0);
  }
}

util_intops::StrongVector<RCIndex, std::vector<int>>
CreateResourceIndicesPerClass(int num_resource_classes,
                              int num_resources_per_class) {
  util_intops::StrongVector<RCIndex, std::vector<int>>
      resource_indices_per_class(num_resource_classes);
  // Resource class index RC contains resource indices in
  // [num_resources_per_class*RC, num_resources_per_class*(RC + 1)[.
  for (RCIndex rc(0); rc < num_resource_classes; rc++) {
    for (int r_i = 0; r_i < num_resources_per_class; r_i++) {
      resource_indices_per_class[rc].push_back(
          num_resources_per_class * rc.value() + r_i);
    }
  }
  return resource_indices_per_class;
}

util_intops::StrongVector<RCIndex, absl::flat_hash_set<int>>
CreateIgnoredResourcesPerClass(
    const util_intops::StrongVector<RCIndex, std::vector<int>>&
        resource_indices_per_class,
    int num_ignored_resources_per_class) {
  util_intops::StrongVector<RCIndex, absl::flat_hash_set<int>>
      ignored_resources_per_class(resource_indices_per_class.size());
  for (int rc = 0; rc < resource_indices_per_class.size(); ++rc) {
    RCIndex rci(rc);
    const int num_resources_in_class = resource_indices_per_class[rci].size();
    DCHECK_LE(num_ignored_resources_per_class, num_resources_in_class);
    for (int r_i = 0; r_i < num_ignored_resources_per_class; ++r_i) {
      ignored_resources_per_class[rci].insert(
          resource_indices_per_class[rci][r_i]);
    }
  }
  return ignored_resources_per_class;
}

TEST(ComputeBestVehicleToResourceAssignmentTest, NoVehicles) {
  std::vector<int> resource_indices(42, /*some junk value*/ 567);
  const auto resource_indices_per_class = CreateResourceIndicesPerClass(10, 10);
  const auto ignored_resources_per_class =
      CreateIgnoredResourcesPerClass(resource_indices_per_class, 0);
  EXPECT_EQ(ComputeBestVehicleToResourceAssignment(
                /*vehicles=*/
                {}, resource_indices_per_class, ignored_resources_per_class,
                /*vehicle_to_resource_class_assignment_costs=*/
                [](int) { return nullptr; }, &resource_indices),
            0);
  EXPECT_THAT(resource_indices, ElementsAreArray(std::vector<int>(42, -1)));
}

TEST(ComputeBestVehicleToResourceAssignmentTest, ZeroVehicleInModel) {
  std::vector<int> resource_indices;
  const auto resource_indices_per_class = CreateResourceIndicesPerClass(10, 10);
  const auto ignored_resources_per_class =
      CreateIgnoredResourcesPerClass(resource_indices_per_class, 0);
  EXPECT_EQ(ComputeBestVehicleToResourceAssignment(
                /*vehicles=*/
                {}, resource_indices_per_class, ignored_resources_per_class,
                /*vehicle_to_resource_class_assignment_costs=*/
                [](int) { return nullptr; }, &resource_indices),
            0);
  EXPECT_THAT(resource_indices, IsEmpty());
}

TEST(ComputeBestVehicleToResourceAssignmentTest, SomeVehiclesButNoneAreUsed) {
  std::vector<int> resource_indices(7, 56789);
  const auto resource_indices_per_class = CreateResourceIndicesPerClass(10, 10);
  const auto ignored_resources_per_class =
      CreateIgnoredResourcesPerClass(resource_indices_per_class, 0);
  const std::vector<int64_t> empty_vector;
  std::vector<int> cost_callback_args;
  EXPECT_EQ(ComputeBestVehicleToResourceAssignment(
                /*vehicles=*/
                {2, 6}, resource_indices_per_class, ignored_resources_per_class,
                /*vehicle_to_resource_class_assignment_costs=*/
                [&cost_callback_args, &empty_vector](int v) {
                  // Register v, we'll test it later.
                  cost_callback_args.push_back(v);
                  return &empty_vector;
                },
                &resource_indices),
            0);
  EXPECT_THAT(resource_indices, ElementsAreArray(std::vector<int>(7, -1)));
  EXPECT_THAT(cost_callback_args, UnorderedElementsAre(2, 6));
}

TEST(ComputeBestVehicleToResourceAssignmentTest, SimpleCase) {
  const int kNumResourceClasses = 3;
  // Cost matrix.
  const std::vector<std::vector<int64_t>> costs_per_class = {
      {33, 13, 45},  // v0
      {44, 30, 60},  // v1
      {},            // v2
      {38, 25, 55},  // v3
  };
  struct ResourceScenario {
    int num_resources_per_class;
    int num_ignored_resources_per_class;
  };
  const std::vector<ResourceScenario> kResourceScenarios = {
      {1, 1}, {1, 0}, {2, 1}, {2, 0}, {3, 1}, {3, 0}, {10, 7}, {10, 0}};
  const std::vector<std::vector<int>> kExpectedResourceIndices = {
      // No resource available in any of the classes.
      {},
      // 1 resource per class. Optimal assignment: v0-rc1, v1-rc2, v3-rc0.
      {1, 2, -1, 0},
      // 2 resources per class but only one available. Same as above.
      {3, 5, -1, 1},
      // 2 resources per class. Optimal assignment: v0-rc1, v1-rc1, v3-rc0.
      {2, 3, -1, 0},
      // 3 resources per class but only 2 available. Same as above.
      {4, 5, -1, 1},
      // 3 resources per class. Optimal assignment: v0-rc1, v1-rc1, v3-rc1.
      {3, 4, -1, 5},
      // 10 resources per class and only 3 available. Same as before.
      {17, 18, -1, 19},
      // 10 resources per class. Same as before.
      {10, 11, -1, 12},
  };
  ASSERT_EQ(kExpectedResourceIndices.size(), kResourceScenarios.size());
  const std::vector<int64_t> kExpectedCosts = {
      -1,           13 + 60 + 38, 13 + 60 + 38, 13 + 30 + 38,
      13 + 30 + 38, 13 + 30 + 25, 13 + 30 + 25, 13 + 30 + 25};
  ASSERT_EQ(kExpectedCosts.size(), kResourceScenarios.size());

  for (int i = 0; i < kResourceScenarios.size(); i++) {
    auto [num_res_per_class, num_ignored_res_per_class] = kResourceScenarios[i];
    const auto resource_indices_per_class =
        CreateResourceIndicesPerClass(kNumResourceClasses, num_res_per_class);
    const auto ignored_resources_per_class = CreateIgnoredResourcesPerClass(
        resource_indices_per_class, num_ignored_res_per_class);

    std::vector<int> resource_indices(4);
    EXPECT_EQ(ComputeBestVehicleToResourceAssignment(
                  /*vehicles=*/
                  {0, 1, 2, 3}, resource_indices_per_class,
                  ignored_resources_per_class,
                  /*vehicle_to_resource_class_assignment_costs=*/
                  [&costs_per_class](int v) { return &costs_per_class[v]; },
                  &resource_indices),
              kExpectedCosts[i]);
    EXPECT_THAT(resource_indices,
                ElementsAreArray(kExpectedResourceIndices[i]));

    // Try a call with resource_indices=nullptr.
    EXPECT_EQ(
        ComputeBestVehicleToResourceAssignment(
            /*vehicles=*/
            {0, 1, 2, 3}, resource_indices_per_class,
            ignored_resources_per_class,
            /*vehicle_to_resource_class_assignment_costs=*/
            [&costs_per_class](int v) { return &costs_per_class[v]; }, nullptr),
        kExpectedCosts[i]);
  }
}

TEST(ComputeBestVehicleToResourceAssignmentTest, InfeasibleTooFewResources) {
  const std::vector<int> kVehicles = {0, 1, 2, 3};
  struct ResourceScenario {
    int num_classes;
    int num_resources_per_class;
    int num_ignored_resources_per_class;
  };
  const std::vector<ResourceScenario> kResourceScenarios = {
      {1, 2, 0}, {1, 3, 0}, {1, 4, 1}, {2, 1, 0},
      {2, 2, 1}, {3, 1, 0}, {3, 2, 1}, {10, 20, 20}};
  for (const auto& [num_classes, num_res_per_class, num_ignored_res_per_class] :
       kResourceScenarios) {
    const auto resource_indices_per_class =
        CreateResourceIndicesPerClass(num_classes, num_res_per_class);
    const auto ignored_resources_per_class = CreateIgnoredResourcesPerClass(
        resource_indices_per_class, num_ignored_res_per_class);
    const std::vector<int64_t> dummy_costs(num_classes, 1);
    std::vector<int> resource_indices(4, 1234);
    EXPECT_EQ(
        ComputeBestVehicleToResourceAssignment(
            kVehicles, resource_indices_per_class, ignored_resources_per_class,
            /*vehicle_to_resource_class_assignment_costs=*/
            [&dummy_costs](int) { return &dummy_costs; }, &resource_indices),
        -1);
    EXPECT_THAT(resource_indices, IsEmpty());
  }
}

TEST(ComputeBestVehicleToResourceAssignmentTest,
     InfeasibleSomeVehicleHasNoUsableResources) {
  for (auto [num_classes, num_res_per_class] :
       std::vector<std::pair<int, int>>{{2, 2}, {3, 1}, {1, 3}}) {
    const auto resource_indices_per_class =
        CreateResourceIndicesPerClass(num_classes, num_res_per_class);
    // No resources ignored.
    const auto ignored_resources_per_class =
        CreateIgnoredResourcesPerClass(resource_indices_per_class, 0);
    const std::vector<int64_t> dummy_costs(num_classes, 1);
    const std::vector<int64_t> negative_costs(num_classes, -1);
    std::vector<int> resource_indices(4, 1234);
    EXPECT_EQ(
        ComputeBestVehicleToResourceAssignment(
            /*vehicles=*/
            {0, 2}, resource_indices_per_class, ignored_resources_per_class,
            /*vehicle_to_resource_class_assignment_costs=*/
            [&](int v) { return v == 2 ? &negative_costs : &dummy_costs; },
            &resource_indices),
        -1);
    EXPECT_THAT(resource_indices, IsEmpty());
  }
}

TEST(ComputeBestVehicleToResourceAssignmentTest, InfeasibleNonTrivialCase) {
  // Vehicle i can only use resources 0..i.
  // Except the last vehicle #n-1 which can only use resources 0..n-2.
  constexpr int n = 1000;
  std::vector<std::vector<int64_t>> costs(n);
  for (int i = 0; i < n; ++i) {
    costs[i].assign(n, -1);
    for (int j = 0; j <= i; ++j) costs[i][j] = 0;
  }
  costs[n - 1][n - 1] = -1;
  std::vector<int> resource_indices(n);
  std::vector<int> all_indices(n);
  for (int i = 0; i < n; ++i) all_indices[i] = i;
  const auto resource_indices_per_class = CreateResourceIndicesPerClass(n, 1);
  // No resources ignored.
  const auto ignored_resources_per_class =
      CreateIgnoredResourcesPerClass(resource_indices_per_class, 0);
  EXPECT_EQ(
      ComputeBestVehicleToResourceAssignment(
          /*vehicles=*/
          all_indices, resource_indices_per_class, ignored_resources_per_class,
          /*vehicle_to_resource_class_assignment_costs=*/
          [&costs](int v) { return &costs[v]; }, &resource_indices),
      -1);
  EXPECT_THAT(resource_indices, IsEmpty());
}

TEST(ComputeBestVehicleToResourceAssignmentTest, RandomizedStressTest) {
  constexpr int kNumTests = 30000;  // RUNTIME: 8s average, forge, fastbuild.
  absl::BitGen random;
  for (int t = 0; t < kNumTests; ++t) {
    // Prepare the test case.
    const int total_num_vehicles = absl::LogUniform(random, 0, 20);
    const int num_vehicles_passed =
        absl::Uniform(random, 0, total_num_vehicles);
    const int num_vehicles_to_assign =
        absl::Uniform(random, 0, num_vehicles_passed);
    // Make either a feasible or infeasible model.
    const int num_ignored_resources = absl::Uniform(random, 0, 20);
    const int num_resources =
        num_ignored_resources +
        (num_vehicles_to_assign > 1 && absl::Bernoulli(random, 0.5)
             ? absl::Uniform(random, 1, num_vehicles_to_assign - 1)  // Infeas.
             : absl::Uniform(random,                                 // Feas.
                             std::max(1, num_vehicles_to_assign),
                             2 * num_vehicles_to_assign + 1));
    const int num_res_classes = absl::Uniform(random, 1, num_resources);
    util_intops::StrongVector<RCIndex, std::vector<int>>
        resource_indices_per_class(num_res_classes);
    std::vector<int> resource_class_index(num_resources);
    for (int res = 0; res < num_resources; res++) {
      const RCIndex rci = RCIndex(res % num_res_classes);
      resource_indices_per_class[rci].push_back(res);
      resource_class_index[res] = rci.value();
    }
    util_intops::StrongVector<RCIndex, absl::flat_hash_set<int>>
        ignored_resources_per_class(num_res_classes);
    for (int ignored_res :
         RandomSubset(num_resources, num_ignored_resources, random)) {
      ignored_resources_per_class[RCIndex(resource_class_index[ignored_res])]
          .insert(ignored_res);
    }
    std::vector<int> available_resources;
    for (int res = 0; res < num_resources; res++) {
      if (!ignored_resources_per_class[RCIndex(resource_class_index[res])]
               .contains(res)) {
        available_resources.push_back(res);
      }
    }
    ASSERT_EQ(available_resources.size(),
              num_resources - num_ignored_resources);
    const int64_t max_arc_cost =
        absl::LogUniform<int64_t>(random, 0, kint64max);
    std::vector<int> vehicles =
        RandomSubset(total_num_vehicles, num_vehicles_passed, random);
    absl::Span<int> vehicles_to_assign =
        RandomSubsetOf(num_vehicles_to_assign, &vehicles, random);
    std::vector<std::vector<int64_t>> costs(total_num_vehicles);
    int64_t actual_max_arc_cost = 0;
    for (int v : vehicles_to_assign) {
      costs[v].reserve(num_res_classes);
      for (int rc = 0; rc < num_res_classes; ++rc) {
        costs[v].push_back(absl::Uniform<int64_t>(absl::IntervalClosed, random,
                                                  0, max_arc_cost));
        actual_max_arc_cost = std::max(actual_max_arc_cost, costs[v].back());
      }
    }
    // Since the cost returned is only approximate in case of really high arc
    // costs, we need to compute the expected right shift applied to the costs
    // by the function.
    const int num_flow_nodes = num_res_classes + num_vehicles_passed + 4;
    const int expected_cost_right_shift = std::ceil(
        std::log2(1 + static_cast<int64_t>(absl::int128(actual_max_arc_cost) *
                                           4 * num_flow_nodes / kint64max)));
    ASSERT_LE(expected_cost_right_shift, 16);

    // CORNER CASES: ComputeBestVehicleToResourceAssignment() does some internal
    // down-scaling of the arc costs if necessary to avoid overflows within
    // SimpleMinCostFlow(), but it scales the returned total cost back up, which
    // can result in an overflow. To prevent this here, we create an assignment
    // whose total cost is ≤ kint64max, which should guarantee that the total
    // cost doesn't overflow.
    const int num_available_resources = available_resources.size();
    if (num_vehicles_to_assign <= num_available_resources &&
        num_vehicles_to_assign > 0 &&
        actual_max_arc_cost >= kint64max / num_vehicles_to_assign) {
      absl::Span<int> assignment =
          RandomSubsetOf(num_vehicles_to_assign, &available_resources, random);
      int64_t cost_left = CapProd(num_vehicles_to_assign, actual_max_arc_cost);
      // The following draw isn't uniform, but it does guarantee that each
      // cost will be in [0..actual_max_arc_cost], with a global sum
      // equal to the initial value of 'cost_left'. Moreover, to preserve the
      // fact that actual_max_arc_cost remains the actual max arc cost, despite
      // our 'assigned' costs potentially overwriting the arc realizing the max,
      // we always choose actual_max_arc_cost as the first arc cost.
      for (int i = 0; i < num_vehicles_to_assign; ++i) {
        const int64_t num_after = num_vehicles_to_assign - 1 - i;
        const int64_t arc_cost =
            i == 0 ? actual_max_arc_cost
                   : absl::Uniform(
                         absl::IntervalClosed, random,
                         std::max<int64_t>(
                             0, cost_left -
                                    CapProd(num_after, actual_max_arc_cost)),
                         std::min(actual_max_arc_cost, cost_left));
        cost_left -= arc_cost;
        costs[vehicles[i]][resource_class_index[assignment[i]]] = arc_cost;
      }
      ASSERT_EQ(cost_left, 0);
    }

    // Run the assignment optimizer.
    std::vector<int> assignment(total_num_vehicles, -12345);
    const int64_t cost = ComputeBestVehicleToResourceAssignment(
        vehicles, resource_indices_per_class, ignored_resources_per_class,
        [&costs](int v) { return &costs[v]; }, &assignment);
    ASSERT_EQ(cost == -1, num_available_resources < num_vehicles_to_assign);
    if (cost < 0) {
      ASSERT_EQ(cost, -1);
      ASSERT_THAT(assignment, IsEmpty());
      continue;
    }
    // For feasible assignments, verify that the assignment that was output
    // is valid, and that it's consistent with the returned cost.
    ASSERT_EQ(assignment.size(), total_num_vehicles);
    std::vector<bool> vehicle_should_be_assigned(total_num_vehicles, false);
    for (int v : vehicles_to_assign) vehicle_should_be_assigned[v] = true;
    for (int v = 0; v < total_num_vehicles; ++v) {
      ASSERT_EQ(assignment[v] == -1, !vehicle_should_be_assigned[v]) << v;
    }

    int64_t recomputed_shifted_cost = 0;
    std::vector<bool> resource_was_assigned(num_resources, false);
    for (int v : vehicles_to_assign) {
      const int r = assignment[v];
      ASSERT_GE(r, 0);
      ASSERT_LT(r, num_resources);
      const int rc_idx = resource_class_index[r];
      ASSERT_FALSE(ignored_resources_per_class[RCIndex(rc_idx)].contains(r));
      ASSERT_FALSE(resource_was_assigned[r]);
      resource_was_assigned[r] = true;
      recomputed_shifted_cost += costs[v][rc_idx] >> expected_cost_right_shift;
    }
    ASSERT_EQ(cost >> expected_cost_right_shift, recomputed_shifted_cost);
  }
}

TEST(ComputeBestVehicleToResourceAssignmentTest, CostOverflow) {
  // Cost matrix.
  const std::vector<std::vector<int64_t>> costs = {
      {kint64max - 20, kint64max - 1},  // v0
      {0, 1},                           // v1
  };
  const auto resource_indices_per_class = CreateResourceIndicesPerClass(2, 1);
  const auto ignored_resources_per_class =
      CreateIgnoredResourcesPerClass(resource_indices_per_class, 0);
  // Because we need to shift the costs to not overflow in min cost flow, we
  // underestimate the final cost and loose some bit of precision.
  constexpr int64_t kExpectedCost =
      (kint64max - 19) & ~static_cast<uint64_t>(63);
  std::vector<int> resource_indices(2);
  EXPECT_EQ(ComputeBestVehicleToResourceAssignment(
                /*vehicles=*/
                {0, 1}, resource_indices_per_class, ignored_resources_per_class,
                /*vehicle_to_resource_class_assignment_costs=*/
                [&costs](int v) { return &costs[v]; }, &resource_indices),
            kExpectedCost);
  EXPECT_THAT(resource_indices, ElementsAre(0, 1));

  // Try a call with resource_indices=nullptr.
  EXPECT_EQ(ComputeBestVehicleToResourceAssignment(
                /*vehicles=*/
                {0, 1}, resource_indices_per_class, ignored_resources_per_class,
                /*vehicle_to_resource_class_assignment_costs=*/
                [&costs](int v) { return &costs[v]; }, nullptr),
            kExpectedCost);
}

// Test to check that constraints are considered in ascending order when adding
// variable coefficients, especially in the case of breaks.
// Without cl/461658813 this test will fail when NotifyThatColumnsAreClean() is
// called.
TEST(GlopWrapperTest, LinearProgramIsCleanWithBreaks) {
  IndexManager manager(2, 1, NodeIndex(0));
  Model model(manager);
  const int callback =
      model.RegisterTransitCallback([&manager](int64_t i, int64_t j) {
        return manager.IndexToNode(i) != manager.IndexToNode(j) ? 10 : 0;
      });
  model.AddDimension(callback, 70, 100, false, "dim");
  Dimension* dim = model.GetMutableDimension("dim");
  dim->CumulVar(model.Start(0))->SetMax(4);
  dim->CumulVar(model.End(0))->SetMin(80);
  std::vector<IntervalVar*> break_intervals;
  break_intervals.push_back(
      model.solver()->MakeFixedDurationIntervalVar(0, 79, 5, false, ""));
  break_intervals.push_back(
      model.solver()->MakeFixedDurationIntervalVar(0, 79, 5, false, ""));
  std::vector<int64_t> visits(dim->cumuls().size(), 0);
  dim->SetBreakIntervalsOfVehicle(break_intervals, 0, visits);
  model.CloseModel();
  DimensionCumulOptimizerCore optimizer(dim, false);
  glop::GlopParameters parameters;
  parameters.set_use_dual_simplex(true);
  SearchStats stats;
  GlopWrapper solver(false, parameters, &stats);
  std::vector<int64_t> nexts = {1, 2};
  Model::RouteDimensionTravelInfo dimension_travel_info;
  std::vector<int64_t> cumul_values;
  std::vector<int64_t> break_values;
  int64_t cost = -1;
  int64_t transit_cost = -1;
  DimensionSchedulingStatus status = optimizer.OptimizeSingleRouteWithResource(
      /*vehicle=*/0, /*solve_duration_ratio=*/1.0,
      [&nexts](int64_t i) { return nexts[i]; }, &dimension_travel_info,
      /*resource=*/nullptr, /*optimize_vehicle_costs=*/true, &solver,
      &cumul_values, &break_values, &cost, &transit_cost);
  EXPECT_EQ(status, DimensionSchedulingStatus::OPTIMAL);
  EXPECT_EQ(stats.num_cp_sat_calls_in_lp_scheduling, 0);
  EXPECT_EQ(stats.num_glop_calls_in_lp_scheduling, 1);
  EXPECT_EQ(stats.num_min_cost_flow_calls, 0);
}

struct TransitTargetsSchedulingParams {
  bool use_lp;
  bool use_optimizer_core;
};

class ScheduleRouteWithTransitTargets
    : public testing::TestWithParam<TransitTargetsSchedulingParams> {
 public:
  void SetUp() override {
    use_lp_ = GetParam().use_lp;
    use_optimizer_core_ = GetParam().use_optimizer_core;

    lp_solver_ =
        std::make_unique<GlopWrapper>(false, glop::GlopParameters(), nullptr);
    mp_solver_ = std::make_unique<CPSatWrapper>(nullptr);
  }

  DimensionSchedulingStatus Optimize(double transit_target_threshold_ratio,
                                     std::vector<int64_t>* cumuls,
                                     std::vector<int64_t>* transits) {
    DCHECK_NE(dim_, nullptr);
    DCHECK_NE(transits, nullptr);
    const DimensionSchedulingStatus status =
        use_optimizer_core_
            ? OptimizeWithOptimizerCore(transit_target_threshold_ratio, cumuls,
                                        transits)
            : OptimizeWithLocalDimensionCumulOptimizer(
                  transit_target_threshold_ratio, cumuls, transits);
    const auto transit_evaluator = dim_->transit_evaluator(0);
    for (int i = 0; i < transits->size(); ++i) {
      transits->at(i) += transit_evaluator(i, next_accessor_(i));
    }
    return status;
  }

 protected:
  Dimension* dim_;
  std::vector<int64_t> transit_targets_;

 private:
  static constexpr int64_t kTransitSoftCostAboveThreshold = 10;
  static constexpr int64_t kTransitSoftCostBelowThreshold = 20;

  DimensionSchedulingStatus OptimizeWithOptimizerCore(
      double transit_target_threshold_ratio, std::vector<int64_t>* cumuls,
      std::vector<int64_t>* variable_transits) {
    DimensionCumulOptimizerCore optimizer(dim_, false);
    LinearSolverWrapper* solver;
    if (use_lp_) {
      solver = lp_solver_.get();
    } else {
      solver = mp_solver_.get();
    }
    return optimizer.OptimizeSingleRouteWithTransitTargets(
        0, /*solve_duration_ratio=*/1.0, next_accessor_, transit_targets_,
        {transit_target_threshold_ratio, kTransitSoftCostBelowThreshold,
         kTransitSoftCostAboveThreshold},
        solver, variable_transits, cumuls, nullptr);
  }

  DimensionSchedulingStatus OptimizeWithLocalDimensionCumulOptimizer(
      double transit_target_threshold_ratio, std::vector<int64_t>* cumuls,
      std::vector<int64_t>* variable_transits) {
    LocalDimensionCumulOptimizer optimizer(
        dim_,
        use_lp_ ? RoutingSearchParameters::SCHEDULING_GLOP
                : RoutingSearchParameters::SCHEDULING_CP_SAT,
        nullptr);
    return optimizer.ComputeRouteCumulsWithTransitTargets(
        0, /*solve_duration_ratio=*/1.0, next_accessor_, transit_targets_,
        {transit_target_threshold_ratio, kTransitSoftCostBelowThreshold,
         kTransitSoftCostAboveThreshold},
        variable_transits, cumuls, nullptr);
  }

  bool use_lp_;
  bool use_optimizer_core_;

  const std::function<int64_t(int64_t)> next_accessor_ = [](int64_t i) {
    return i + 1;
  };
  std::unique_ptr<GlopWrapper> lp_solver_;
  std::unique_ptr<CPSatWrapper> mp_solver_;
};

TEST_P(ScheduleRouteWithTransitTargets, TransitTargetsFeasible) {
  const IndexManager manager(3, 1, NodeIndex(0));
  Model model(manager);
  // "Regular" transit for each node is 10 * node.
  const int transit_callback =
      model.RegisterTransitCallback([](int64_t i, int64_t) { return 10 * i; });
  model.AddDimension(transit_callback, 100, 100,
                     /*fix_start_cumul_to_zero=*/true, "dim");
  dim_ = model.GetMutableDimension("dim");
  // Force the scheduler to minimize the span of the vehicle -> no slack.
  dim_->SetSpanCostCoefficientForAllVehicles(10);

  model.CloseModel();

  transit_targets_ = {10, 10, 21};
  std::vector<int64_t> transit_values;
  std::vector<int64_t> cumul_values;
  for (const double transit_target_threshold_ratio : {0.1, 0.5, 0.9}) {
    const DimensionSchedulingStatus status = Optimize(
        transit_target_threshold_ratio, &cumul_values, &transit_values);
    ASSERT_EQ(status, DimensionSchedulingStatus::OPTIMAL);
    EXPECT_THAT(transit_values, ElementsAre(10, 10, 21));
    EXPECT_THAT(cumul_values, ElementsAre(0, 10, 20, 41));
  }
}

TEST_P(ScheduleRouteWithTransitTargets, TransitTargetsViolateSpanLimit) {
  const IndexManager manager(3, 1, NodeIndex(0));
  Model model(manager);
  // "Regular" transit for each node is 5.
  model.AddConstantDimensionWithSlack(5, 100, 100,
                                      /*fix_start_cumul_to_zero=*/true, "dim");
  dim_ = model.GetMutableDimension("dim");
  dim_->SetSpanCostCoefficientForAllVehicles(10);
  // The span is limited to 24, so with transit targets of 10 we have a total
  // delta from the targets of 30-24 = 6, so ideally we'd like to have a
  // transit of 8 per arc for a perfect balance of the delta across the arcs.
  const int64_t kSpanUpperBound = 24;
  dim_->SetSpanUpperBoundForVehicle(kSpanUpperBound, 0);
  transit_targets_ = {10, 10, 10};
  const int64_t kPerfectBalanceTransit =
      kSpanUpperBound / transit_targets_.size();  // 8

  model.CloseModel();

  std::vector<int64_t> transit_values;
  std::vector<int64_t> cumul_values;
  for (const double transit_target_threshold_ratio : {0.1, 0.5, 0.8, 0.9}) {
    const DimensionSchedulingStatus status = Optimize(
        transit_target_threshold_ratio, &cumul_values, &transit_values);

    ASSERT_EQ(status, DimensionSchedulingStatus::OPTIMAL);
    EXPECT_EQ(absl::c_accumulate(transit_values, 0), kSpanUpperBound);

    ASSERT_EQ(cumul_values.size(), transit_values.size() + 1);
    EXPECT_EQ(cumul_values[0], 0);
    for (int i = 0; i < transit_values.size(); ++i) {
      EXPECT_EQ(cumul_values[i + 1], cumul_values[i] + transit_values[i]);
    }

    const int64_t threshold =
        transit_target_threshold_ratio * transit_targets_[0];
    if (threshold == kPerfectBalanceTransit) {
      // Since the threshold where we go from the lower-sloped cost to the
      // higher-sloped cost is equal to the perfect-balance transit, we
      // should have a perfect balance of the delta across the arcs in order
      // to minimize the cost of violating the transit targets.
      EXPECT_THAT(transit_values,
                  ElementsAre(kPerfectBalanceTransit, kPerfectBalanceTransit,
                              kPerfectBalanceTransit));
    } else if (threshold > kPerfectBalanceTransit) {
      // The perfect-balance transit is below the threshold of the
      // lower-sloped cost, so every transit should be less than or equal to
      // the threshold.
      // Below the threshold, there's no guarantee on how the transit will
      // be distributed across the arcs, since the violation will cost the
      // same regardless of how it's spread across the arcs.
      EXPECT_THAT(transit_values, testing::Each(testing::Le(threshold)));
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    ScheduleRouteWithTransitTargets, ScheduleRouteWithTransitTargets,
    testing::Values(TransitTargetsSchedulingParams{.use_lp = true,
                                                   .use_optimizer_core = true},
                    TransitTargetsSchedulingParams{.use_lp = true,
                                                   .use_optimizer_core = false},
                    TransitTargetsSchedulingParams{.use_lp = false,
                                                   .use_optimizer_core = true},
                    TransitTargetsSchedulingParams{
                        .use_lp = false, .use_optimizer_core = false}));

// New traffic module.

constexpr double kLowerBoundCompressedTravelFactor = 0.7;
constexpr double kUpperBoundTravelFactor = 3.0;
constexpr int64_t kCostRef = 1 << 12;

void BuildTestModel(int kNodes, int64_t cumul_max, int64_t span_cost,
                    int64_t slack_cost, int time_limit_seconds,
                    std::unique_ptr<Model>& model,
                    std::unique_ptr<LocalDimensionCumulOptimizer>& optimizer) {
  const int kVehicles = 1;
  const IndexManager manager(kNodes, kVehicles, NodeIndex(0));

  model = std::make_unique<Model>(manager);
  // Add time dimension.
  const std::string kDimName = "time";
  model->AddConstantDimensionWithSlack(
      1, /*capacity*/ 10'000, /*slack_max*/ 10'000,
      /*fix_start_cumul_to_zero*/ false, kDimName);
  Dimension* const time = model->GetMutableDimension(kDimName);
  time->SetSpanCostCoefficientForVehicle(span_cost, 0);
  time->SetSlackCostCoefficientForVehicle(slack_cost, 0);
  time->CumulVar(kNodes)->SetMax(cumul_max);

  // HACK(user): This permits to set a time limit.
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.mutable_time_limit()->set_seconds(time_limit_seconds);
  model->SolveFromAssignmentWithParameters(nullptr, parameters, nullptr);

  optimizer = std::make_unique<LocalDimensionCumulOptimizer>(
      time, RoutingSearchParameters::SCHEDULING_CP_SAT, nullptr);
}

DimensionSchedulingStatus ComputeRouteCumuls(
    std::unique_ptr<LocalDimensionCumulOptimizer>& optimizer,
    Model::RouteDimensionTravelInfo& travel_info,
    std::vector<int64_t>* optimal_cumuls,
    std::vector<int64_t>* optimal_breaks = nullptr,
    int64_t* optimal_cost_without_fixed_transits = nullptr) {
  // Builds the next_accessor such as i -> i+1
  const int kNodes = travel_info.transition_info.size();
  std::vector<int64_t> nexts;
  for (int node = 0; node < kNodes; ++node) nexts.push_back(1 + node);
  const auto next_accessor = [&nexts](int index) { return nexts[index]; };

  return optimizer->ComputeRouteCumulsAndCostWithoutFixedTransits(
      /*vehicle=*/0, /*solve_duration_ratio=*/1.0, next_accessor, &travel_info,
      optimal_cumuls, optimal_breaks, optimal_cost_without_fixed_transits);
}

DimensionSchedulingStatus ComputeSolutionCost(
    std::unique_ptr<LocalDimensionCumulOptimizer>& optimizer,
    Model::RouteDimensionTravelInfo& travel_info,
    absl::Span<const int64_t> solution_cumul_values,
    absl::Span<const int64_t> solution_break_values, int64_t* solution_cost) {
  // Builds the next_accessor such that i -> i+1.
  const int kNodes = travel_info.transition_info.size();
  std::vector<int64_t> nexts;
  for (int node = 0; node < kNodes; ++node) nexts.push_back(1 + node);
  const auto next_accessor = [&nexts](int index) { return nexts[index]; };

  return optimizer->ComputeRouteSolutionCostWithoutFixedTransits(
      /*vehicle=*/0, /*solve_duration_ratio=*/1.0, next_accessor, &travel_info,
      solution_cumul_values, solution_break_values, solution_cost);
}

std::vector<int64_t> FillRandomTrafficInfo(
    random_engine_t random, int kNodes, int64_t dT, int nb_segments,
    Model::RouteDimensionTravelInfo& travel_info,
    double compression_factor = 0.8, double play = 0.25,
    int n_points_lin_cost = 30) {
  const std::vector<int64_t> initial_cumuls =
      RandomDistribution(dT, 1 + kNodes, random);
  const std::vector<int64_t> new_cumuls = RandomDistribution(
      std::ceil(dT / compression_factor), 1 + kNodes, random);
  std::mt19937 eng(absl::Uniform(random, kint32min, kint32max));
  travel_info.transition_info.resize(kNodes);

  std::vector<int64_t> static_travel_durations(kNodes);
  for (int k = 0; k < kNodes; ++k) {
    Model::RouteDimensionTravelInfo::TransitionInfo transition;
    const int64_t initial_dt = initial_cumuls[k + 1] - initial_cumuls[k];
    const int64_t expanded_dt = new_cumuls[k + 1] - new_cumuls[k];
    const int64_t ref_dt = std::min(initial_dt, expanded_dt);

    // Pre travel is a binomial variable of mean kPreFactor * initial_dt, such
    // that it is in [0, initial_dt * (1 - kPostFactor - kMinTravelFactor)].
    constexpr double kPreFactor = 0.15;
    constexpr double kPostFactor = 0.15;
    constexpr double kMinTravelFactor = 0.4;

    // Pre travel
    const double mult_pre = 1 - kPostFactor - kMinTravelFactor;
    std::binomial_distribution<int64_t> binomial_pre(ref_dt * mult_pre,
                                                     kPreFactor / mult_pre);
    transition.pre_travel_transit_value = binomial_pre(eng);

    // Post travel
    const double mult_post = 1 - kMinTravelFactor;
    const int64_t n = ref_dt * mult_post - transition.pre_travel_transit_value;
    std::binomial_distribution<int64_t> binomial_post(
        n, kPreFactor * ref_dt / static_cast<double>(n));
    transition.post_travel_transit_value = binomial_post(eng);

    // Travel
    const int64_t static_travel = initial_dt -
                                  transition.pre_travel_transit_value -
                                  transition.post_travel_transit_value;
    static_travel_durations[k] = static_travel;
    const int64_t avg_traffic_travel = expanded_dt -
                                       transition.pre_travel_transit_value -
                                       transition.post_travel_transit_value;
    const int64_t min_traffic_travel =
        std::max((1 - play) * avg_traffic_travel,
                 kLowerBoundCompressedTravelFactor * static_travel);
    const int64_t max_traffic_travel =
        std::max<int64_t>(1 + min_traffic_travel,
                          std::min((1 + play) * avg_traffic_travel,
                                   kUpperBoundTravelFactor * static_travel));
    transition.travel_start_dependent_travel =
        RandomPiecewiseLinearFunctionFixedNbSegments(
            0, dT, min_traffic_travel, max_traffic_travel, nb_segments, random);

    travel_info.transition_info[k] = std::move(transition);
  }
  PopulateTravelInfoCompressionCost(
      travel_info, kLowerBoundCompressedTravelFactor, kUpperBoundTravelFactor,
      kCostRef, static_travel_durations, n_points_lin_cost);
  return static_travel_durations;
}

void FillBlankTrafficInfo(int kNodes,
                          Model::RouteDimensionTravelInfo& travel_info) {
  travel_info.travel_cost_coefficient = 0;  // No cost
  travel_info.transition_info.resize(kNodes);

  for (int k = 0; k < kNodes; ++k) {
    auto& transition = travel_info.transition_info[k];
    transition.pre_travel_transit_value = 0;
    transition.post_travel_transit_value = 0;
  }
}

TEST(ComputeSingleRouteSolutionCost, CheckCostOfOptimalSolutions) {
  random_engine_t random;
  constexpr int kNumTests = DEBUG_MODE ? 5 : 10;
  const int64_t cost_per_unit_max = kCostRef;

  for (int i = 0; i < kNumTests; ++i) {
    // Get a random route with multiple transits.
    Model::RouteDimensionTravelInfo travel_info;
    const int kNodes = absl::Uniform(random, 2, 5);
    const int64_t span_cost = absl::Uniform(random, 1, cost_per_unit_max);
    const int64_t slack_cost = absl::Uniform(random, 1, cost_per_unit_max);
    const int64_t dT = absl::Uniform(random, 20 * kNodes, 100 * kNodes);
    const int nb_segments = absl::Uniform(random, 1, 8);

    travel_info.travel_cost_coefficient =
        absl::Uniform(random, 1, cost_per_unit_max);
    const std::vector<int64_t> static_travel_durations =
        FillRandomTrafficInfo(random, kNodes, dT, nb_segments, travel_info,
                              /*compression_factor=*/0.9, /*play=*/0.25);

    // Find an optimal scheduling.
    std::unique_ptr<Model> model;
    std::unique_ptr<LocalDimensionCumulOptimizer> optimizer;
    BuildTestModel(kNodes, /*cumul_max=*/dT, span_cost, slack_cost,
                   /*time_limit_seconds=*/100, model, optimizer);
    int64_t optimal_cost, solution_cost;
    std::vector<int64_t> optimal_breaks, optimal_cumuls;
    EXPECT_EQ(ComputeRouteCumuls(optimizer, travel_info, &optimal_cumuls,
                                 &optimal_breaks, &optimal_cost),
              DimensionSchedulingStatus::OPTIMAL);
    EXPECT_EQ(ComputeSolutionCost(optimizer, travel_info, optimal_cumuls,
                                  optimal_breaks, &solution_cost),
              DimensionSchedulingStatus::OPTIMAL);
    EXPECT_EQ(solution_cost, optimal_cost);
  }
}

TEST(SetRouteTravelInfoConstraints, FindOptimalSpanWithoutCompression) {
  // This test checks that the traffic module, without compression, optimizes
  // the span if only the span cost is set.
  Model::RouteDimensionTravelInfo travel_info;
  FillBlankTrafficInfo(/*kNodes=*/2, travel_info);
  travel_info.transition_info[0].travel_start_dependent_travel =
      FloatSlopePiecewiseLinearFunction(/*x_anchors=*/{0, 1000, 2000, 2200},
                                        /*y_anchors=*/{100, 500, 100, 100});
  travel_info.transition_info[1].travel_start_dependent_travel =
      FloatSlopePiecewiseLinearFunction(/*x_anchors=*/{0, 1000, 1800, 2200},
                                        /*y_anchors=*/{500, 500, 100, 300});
  PopulateTravelInfoCompressionCost(travel_info,
                                    kLowerBoundCompressedTravelFactor,
                                    kUpperBoundTravelFactor, kCostRef,
                                    /*static_travel_durations=*/{100, 100},
                                    /*num_points_lin_cost=*/30);

  // Find an optimal scheduling.
  std::unique_ptr<Model> model;
  std::unique_ptr<LocalDimensionCumulOptimizer> optimizer;
  BuildTestModel(/*kNodes=*/2, /*cumul_max=*/2350, /*span_cost=*/10,
                 /*slack_cost=*/5,
                 /*time_limit_seconds=*/10, model, optimizer);
  std::vector<int64_t> optimal_cumuls;
  ASSERT_EQ(ComputeRouteCumuls(optimizer, travel_info, &optimal_cumuls),
            DimensionSchedulingStatus::OPTIMAL);

  // There is no cost per hour but just a global span cost, so the optimal is
  // obtained for cumul[2] - cumul[0] minimum.
  // std::vector<int64_t> expected_cumuls = {2000, 2100, 2350};
  // EXPECT_EQ(optimal_cumuls, expected_cumuls);
  EXPECT_LE(optimal_cumuls[2] - optimal_cumuls[0], 350);
  // Checks that the compression is equal to zero.
  for (int i = 0; i < 2; ++i) {
    EXPECT_LE(travel_info.transition_info[i]
                      .travel_start_dependent_travel.ComputeConvexValue(
                          optimal_cumuls[i]) -
                  1,
              optimal_cumuls[i + 1] - optimal_cumuls[i]);
  }
}

TEST(SetRouteTravelInfoConstraints, FindOptimalPerHourWithoutCompression) {
  // This test checks that the traffic module, without compression, optizes
  // the total travel duration if only the cost per hour is set.
  Model::RouteDimensionTravelInfo travel_info;
  FillBlankTrafficInfo(/*kNodes=*/2, travel_info);
  travel_info.transition_info[0].travel_start_dependent_travel =
      FloatSlopePiecewiseLinearFunction(/*x_anchors=*/{0, 1000, 2000, 2200},
                                        /*y_anchors=*/{100, 500, 100, 100});
  travel_info.transition_info[1].travel_start_dependent_travel =
      FloatSlopePiecewiseLinearFunction(/*x_anchors=*/{0, 1000, 1800, 2200},
                                        /*y_anchors=*/{500, 500, 100, 300});
  travel_info.travel_cost_coefficient = 10;
  PopulateTravelInfoCompressionCost(travel_info,
                                    kLowerBoundCompressedTravelFactor,
                                    kUpperBoundTravelFactor, kCostRef,
                                    /*static_travel_durations=*/{100, 100},
                                    /*num_points_lin_cost=*/30);

  // Find an optimal scheduling.
  std::unique_ptr<Model> model;
  std::unique_ptr<LocalDimensionCumulOptimizer> optimizer;
  BuildTestModel(/*kNodes=*/2, /*cumul_max=*/2350, /*span_cost=*/0,
                 /*slack_cost=*/0,
                 /*time_limit_seconds=*/5, model, optimizer);
  std::vector<int64_t> optimal_cumuls;
  ASSERT_EQ(ComputeRouteCumuls(optimizer, travel_info, &optimal_cumuls),
            DimensionSchedulingStatus::OPTIMAL);

  // There is a per hour but no global span cost, so the optimal is obtained
  // for travel_duration[0] + travel_duration[1] minimum. So the result should
  // be [0: 0s] - Travel: 100s + Slack: 1700s - [1: 1800s] - Travel: 100s Due
  // to some rounding issue, the cummuls might not be exactly {0, 1800, 1900},
  // therefore we only check that the travel durations are 100s.
  EXPECT_LE(travel_info.transition_info[0]
                    .travel_start_dependent_travel.ComputeConvexValue(
                        optimal_cumuls[0]) -
                1,
            100);
  EXPECT_LE(travel_info.transition_info[1]
                    .travel_start_dependent_travel.ComputeConvexValue(
                        optimal_cumuls[1]) -
                1,
            100);
  // Checks that the compression is equal to zero.
  for (int i = 0; i < 2; ++i) {
    EXPECT_LE(travel_info.transition_info[i]
                      .travel_start_dependent_travel.ComputeConvexValue(
                          optimal_cumuls[i]) -
                  1,
              optimal_cumuls[i + 1] - optimal_cumuls[i]);
  }
}

TEST(SetRouteTravelInfoConstraints, CompressionSmallManualTestTwoTransits) {
  Model::RouteDimensionTravelInfo travel_info;
  FillBlankTrafficInfo(/*kNodes=*/2, travel_info);
  travel_info.transition_info[0].travel_start_dependent_travel =
      FloatSlopePiecewiseLinearFunction({0, 50, 100}, {70, 50, 30});
  travel_info.transition_info[1].travel_start_dependent_travel =
      FloatSlopePiecewiseLinearFunction({0, 80, 120}, {80, 120, 80});
  PopulateTravelInfoCompressionCost(travel_info,
                                    kLowerBoundCompressedTravelFactor,
                                    kUpperBoundTravelFactor, kCostRef,
                                    /*static_travel_durations=*/{40, 60},
                                    /*num_points_lin_cost=*/30);

  // Find an optimal scheduling.
  std::unique_ptr<Model> model;
  std::unique_ptr<LocalDimensionCumulOptimizer> optimizer;
  BuildTestModel(/*kNodes=*/2, /*cumul_max=*/100, /*span_cost=*/10,
                 /*slack_cost=*/5,
                 /*time_limit_seconds=*/5, model, optimizer);
  std::vector<int64_t> optimal_cumuls;
  EXPECT_EQ(ComputeRouteCumuls(optimizer, travel_info, &optimal_cumuls),
            DimensionSchedulingStatus::OPTIMAL);

  // Since the time window to come back is before 100, these times should get
  // compressed "evenly".
  EXPECT_THAT(optimal_cumuls, ElementsAre(0, 37, 100));
}

TEST(SetRouteTravelInfoConstraints, CompressionStressTestTwoTransits) {
  // This test checks that the solution obtained by the optimizer has a lower
  // cost than a solution obtained with a greddy search. The parameters used
  // (set as random variables) are:
  // - span_cost
  // - travel_cost_coefficient
  // - pre, post transit
  // - travel_start_dependent_travel
  // - Time window to come back to the depot
  random_engine_t random;
  const int kNodes = 2;
  std::vector<int64_t> nexts;
  for (int node = 0; node < kNodes; ++node) nexts.push_back(1 + node);
  const auto next_accessor = [&nexts](int index) { return nexts[index]; };
  constexpr int kNumTests = DEBUG_MODE ? 10 : 20;

  for (int i = 0; i < kNumTests; ++i) {
    // Get a random route with constant transits.
    Model::RouteDimensionTravelInfo travel_info;
    const int64_t span_cost = absl::Uniform(random, 0, kCostRef);
    const int64_t dT = absl::Uniform(random, 10 * kNodes, 50 * kNodes);
    const int64_t slack_cost = absl::Uniform(random, 0, kCostRef);
    const int nb_segments = absl::Uniform(random, 2, 6);

    travel_info.travel_cost_coefficient = absl::Uniform(random, 0, kCostRef);
    const std::vector<int64_t> static_durations =
        FillRandomTrafficInfo(random, kNodes, dT, nb_segments, travel_info,
                              /*compression_factor=*/0.8, /*play=*/0.25);

    // Find an optimal scheduling.
    std::unique_ptr<Model> model;
    std::unique_ptr<LocalDimensionCumulOptimizer> optimizer;
    BuildTestModel(kNodes, /*cumul_max=*/dT, span_cost, slack_cost,
                   /*time_limit_seconds=*/5, model, optimizer);
    int64_t optimal_cost;
    std::vector<int64_t> optimal_breaks, optimal_cumuls;
    ASSERT_EQ(ComputeRouteCumuls(optimizer, travel_info, &optimal_cumuls,
                                 &optimal_breaks, &optimal_cost),
              DimensionSchedulingStatus::OPTIMAL);

    std::vector<int64_t> cumuls = {0, 1, dT};
    std::vector<int64_t> breaks;
    int64_t cost_offset;
    absl::Duration solve_duration;
    for (cumuls[1] = 1; cumuls[1] < dT - 1; ++cumuls[1]) {
      // Compute the cost.
      int64_t cost;
      DimensionSchedulingStatus status =
          optimizer->ComputeRouteSolutionCostWithoutFixedTransits(
              /*vehicle=*/0, /*solve_duration_ratio=*/1.0, next_accessor,
              &travel_info, cumuls, breaks, &cost, &cost_offset,
              /*reuse_previous_model_if_possible=*/true, /*clear_lp=*/false,
              &solve_duration);
      if (status == DimensionSchedulingStatus::OPTIMAL) {
        EXPECT_GE(cost, optimal_cost);
      } else {
        EXPECT_EQ(status, DimensionSchedulingStatus::INFEASIBLE);
      }
    }
  }
}

TEST(SetRouteTravelInfoConstraints, CompressionStressTestManyConstantTransits) {
  random_engine_t random;
  constexpr int kNumTests = DEBUG_MODE ? 5 : 10;

  for (int i = 0; i < kNumTests; ++i) {
    // Get a random route with constant transits.
    Model::RouteDimensionTravelInfo travel_info;
    const int kNodes = absl::Uniform(random, 5, 10);
    const int64_t span_cost = 0;
    const int64_t dT = absl::Uniform(random, 30 * kNodes, 100 * kNodes);
    const int64_t slack_cost = 0;
    const int nb_segments = 0;

    travel_info.travel_cost_coefficient = 0;
    const std::vector<int64_t> static_durations = FillRandomTrafficInfo(
        random, kNodes, dT, nb_segments, travel_info,
        /*compression_factor=*/0.8, /*play=*/0.25, /*n_points_lin_cost=*/100);

    // Find an optimal scheduling.
    std::unique_ptr<Model> model;
    std::unique_ptr<LocalDimensionCumulOptimizer> optimizer;
    BuildTestModel(kNodes, /*cumul_max=*/dT, span_cost, slack_cost,
                   /*time_limit_seconds=*/5, model, optimizer);
    std::vector<int64_t> optimal_cumuls;
    ASSERT_EQ(ComputeRouteCumuls(optimizer, travel_info, &optimal_cumuls),
              DimensionSchedulingStatus::OPTIMAL);
    std::vector<Model::RouteDimensionTravelInfo::TransitionInfo>& transitions =
        travel_info.transition_info;

    // Checks that all compressed travel durations are positive.
    double compression_factor_max = 0;
    std::vector<double> compression_factors;
    const int size = optimal_cumuls.size();
    for (int path = 0; path < size - 1; ++path) {
      const int64_t transit = optimal_cumuls[path + 1] - optimal_cumuls[path];
      const int64_t compressed_travel_duration =
          transit - transitions[path].pre_travel_transit_value -
          transitions[path].post_travel_transit_value;
      EXPECT_GE(compressed_travel_duration, 0);
      const int64_t departure_time =
          optimal_cumuls[path] + transitions[path].pre_travel_transit_value;
      const int64_t travel_duration =
          transitions[path].travel_start_dependent_travel.ComputeConvexValue(
              departure_time);
      const double compression_factor = std::max(
          0.0,
          static_cast<double>(travel_duration - compressed_travel_duration) /
              static_durations[path]);
      compression_factors.push_back(compression_factor);
      compression_factor_max =
          std::max(compression_factor_max, compression_factor);
    }

    // Check that all compression factors are nearly equal.
    double compression_factor_avg =
        std::accumulate(compression_factors.begin(), compression_factors.end(),
                        0.0) /
        compression_factors.size();
    double sq_sum = std::inner_product(compression_factors.begin(),
                                       compression_factors.end(),
                                       compression_factors.begin(), 0.0);
    double stdev = std::sqrt(sq_sum / compression_factors.size() -
                             compression_factor_avg * compression_factor_avg);
    EXPECT_LE(stdev, 0.45 * compression_factor_max);
    EXPECT_LE(stdev, 0.70 * compression_factor_avg);
  }
}

TEST(SetRouteTravelInfoConstraints, TooCompressedInfeasibility) {
  random_engine_t random;
  constexpr int kNumTests = DEBUG_MODE ? 2 : 5;
  const int64_t cost_per_unit_max = kCostRef;

  for (int i = 0; i < kNumTests; ++i) {
    // Get a random route with multiple transits
    Model::RouteDimensionTravelInfo travel_info;
    const int kNodes = absl::Uniform(random, 8, 10);
    const int64_t span_cost = absl::Uniform(random, 1, cost_per_unit_max);
    const int64_t slack_cost = absl::Uniform(random, 1, cost_per_unit_max);
    const int64_t dT = absl::Uniform(random, 20 * kNodes, 100 * kNodes);
    const int nb_segments = absl::Uniform(random, 1, 8);

    travel_info.travel_cost_coefficient =
        absl::Uniform(random, 1, cost_per_unit_max);
    FillRandomTrafficInfo(random, kNodes, dT, nb_segments, travel_info,
                          /*compression_factor=*/0.3, /*play=*/0.2);

    // Find an optimal scheduling.
    std::unique_ptr<Model> model;
    std::unique_ptr<LocalDimensionCumulOptimizer> optimizer;
    BuildTestModel(kNodes, /*cumul_max=*/dT, span_cost, slack_cost,
                   /*time_limit_seconds=*/2, model, optimizer);
    std::vector<int64_t> optimal_cumuls;
    ASSERT_EQ(ComputeRouteCumuls(optimizer, travel_info, &optimal_cumuls),
              DimensionSchedulingStatus::INFEASIBLE);
  }
}

TEST(SetRouteTravelInfoConstraints, CompressionStressTestManyTransits) {
  // This test takes a general RouteDimensionTravelInfo and solves the
  // scheduling problem. As this problem is complex, the only things we can
  // test are the fact that the solution is OPTIMAL and that it respects basic
  // constraints.
  random_engine_t random;
  constexpr int kNumTests = DEBUG_MODE ? 2 : 5;
  const int64_t cost_per_unit_max = kCostRef;
  int64_t num_non_optimal = 0;

  for (int i = 0; i < kNumTests; ++i) {
    // Get a random route with multiple transits
    Model::RouteDimensionTravelInfo travel_info;
    const int kNodes = absl::Uniform(random, 8, 10);
    const int64_t span_cost = absl::Uniform(random, 1, cost_per_unit_max);
    const int64_t slack_cost = absl::Uniform(random, 1, cost_per_unit_max);
    const int64_t dT = absl::Uniform(random, 20 * kNodes, 100 * kNodes);
    const int nb_segments = absl::Uniform(random, 1, 8);

    travel_info.travel_cost_coefficient =
        absl::Uniform(random, 1, cost_per_unit_max);
    const std::vector<int64_t> static_travel_durations =
        FillRandomTrafficInfo(random, kNodes, dT, nb_segments, travel_info,
                              /*compression_factor=*/0.9, /*play=*/0.25);

    // Find an optimal scheduling.
    std::unique_ptr<Model> model;
    std::unique_ptr<LocalDimensionCumulOptimizer> optimizer;
    BuildTestModel(
        kNodes, /*cumul_max=*/dT, span_cost, slack_cost,
        /*time_limit_seconds=*/(DEBUG_MODE || kAnyXsanEnabled) ? 70 : 20, model,
        optimizer);

    // The solution should be optimal.
    std::vector<int64_t> optimal_cumuls;
    if (ComputeRouteCumuls(optimizer, travel_info, &optimal_cumuls) !=
        DimensionSchedulingStatus::OPTIMAL) {
      ++num_non_optimal;
      continue;
    }

    std::vector<Model::RouteDimensionTravelInfo::TransitionInfo>& transitions =
        travel_info.transition_info;
    const int size = optimal_cumuls.size();
    for (int path = 0; path < size - 1; ++path) {
      // "Real" travel duration.
      const int64_t departure_time =
          optimal_cumuls[path] + transitions[path].pre_travel_transit_value;
      const int64_t travel_duration =
          transitions[path].travel_start_dependent_travel.ComputeConvexValue(
              departure_time);

      // Compressed travel duration.
      const int64_t transit = optimal_cumuls[path + 1] - optimal_cumuls[path];
      const int64_t compressed_travel_duration =
          std::min(travel_duration,
                   transit - transitions[path].pre_travel_transit_value -
                       transitions[path].post_travel_transit_value);

      // Static travel duration.
      const int64_t& static_travel_duration = static_travel_durations[path];

      // Check global invariants on the compressed travel duration.
      // This also checks that cumul[i+1] - cumul[i] ≥ pre_travel +
      // post_travel. Since kLowerBoundCompressedTravelFactor *
      // static_travel_duration ≥ 0.
      EXPECT_GE(compressed_travel_duration,
                static_cast<int64_t>(kLowerBoundCompressedTravelFactor *
                                     static_travel_duration));
    }
  }
  const int64_t max_num_non_optimal =
      HACK_CodeCoverageIsEnabled() || kAnyXsanEnabled ? 1 : 0;
  EXPECT_LE(num_non_optimal, max_num_non_optimal);
}

}  // namespace
}  // namespace operations_research::routing
