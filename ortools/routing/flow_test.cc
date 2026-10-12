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

#include <cstdint>

#include "gtest/gtest.h"
#include "ortools/base/types.h"
#include "ortools/routing/index_manager.h"
#include "ortools/routing/routing.h"
#include "ortools/routing/types.h"

namespace operations_research::routing {
namespace {

TEST(RoutingFlowTest, EmptyModel) {
  IndexManager manager(0, 0, {}, {});
  Model model(manager);
  EXPECT_FALSE(model.IsMatchingModel());
}

TEST(RoutingFlowTest, NoVehicles) {
  IndexManager manager(10, 0, {}, {});
  Model model(manager);
  EXPECT_FALSE(model.IsMatchingModel());
}

TEST(RoutingFlowTest, NoDimension) {
  IndexManager manager(2, 10, NodeIndex(0));
  Model model(manager);
  EXPECT_FALSE(model.IsMatchingModel());
}

TEST(RoutingFlowTest, NoUnaryDimension) {
  IndexManager manager(2, 10, NodeIndex(0));
  Model model(manager);
  model.AddDimension(
      model.RegisterTransitCallback([](int64_t, int64_t) { return 1LL; }), 1, 1,
      true, "stop_count");
  EXPECT_FALSE(model.IsMatchingModel());
}

TEST(RoutingFlowTest, NoUnaryDimensionLimit) {
  IndexManager manager(2, 10, NodeIndex(0));
  Model model(manager);
  model.AddDimension(
      model.RegisterUnaryTransitCallback([](int64_t) { return 1LL; }),
      kint64max, kint64max, true, "stop_count");
  EXPECT_FALSE(model.IsMatchingModel());
}

TEST(RoutingFlowTest, UnaryDimensionLimit) {
  IndexManager manager(2, 10, NodeIndex(0));
  Model model(manager);
  model.AddDimension(
      model.RegisterUnaryTransitCallback([](int64_t) { return 1LL; }), 1, 1,
      true, "stop_count");
  EXPECT_TRUE(model.IsMatchingModel());
}

TEST(RoutingFlowTest, ManyVehicleCostClassesUnaryDimensionLimit) {
  IndexManager manager(2, 10, NodeIndex(0));
  Model model(manager);
  model.AddDimension(
      model.RegisterUnaryTransitCallback([](int64_t) { return 1LL; }), 1, 1,
      true, "stop_count");
  model.SetArcCostEvaluatorOfVehicle(
      model.RegisterTransitCallback([](int64_t, int64_t) { return 1LL; }), 0);
  model.SetArcCostEvaluatorOfVehicle(
      model.RegisterTransitCallback([](int64_t, int64_t) { return 2LL; }), 1);
  EXPECT_TRUE(model.IsMatchingModel());
}

TEST(RoutingFlowTest, ManyVehicleClassesUnaryDimensionLimit) {
  IndexManager manager(2, 2, NodeIndex(0));
  Model model(manager);
  model.AddDimensionWithVehicleTransitAndCapacity(
      {model.RegisterUnaryTransitCallback([](int64_t) { return 2LL; }),
       model.RegisterUnaryTransitCallback([](int64_t) { return 1LL; })},
      2, {2, 2}, true, "stop_count");
  EXPECT_FALSE(model.IsMatchingModel());
}

TEST(RoutingFlowTest, ManyVehicleCapacitiesUnaryDimensionLimit) {
  IndexManager manager(2, 2, NodeIndex(0));
  Model model(manager);
  model.AddDimensionWithVehicleCapacity(
      model.RegisterUnaryTransitCallback([](int64_t) { return 2LL; }), 2,
      {2, 3}, true, "stop_count");
  EXPECT_TRUE(model.IsMatchingModel());
}

TEST(RoutingFlowTest, ManyVehicleHighCapacitiesUnaryDimensionLimit) {
  IndexManager manager(2, 2, NodeIndex(0));
  Model model(manager);
  model.AddDimensionWithVehicleCapacity(
      model.RegisterUnaryTransitCallback([](int64_t) { return 1LL; }), 1,
      {1, 2}, true, "stop_count");
  EXPECT_FALSE(model.IsMatchingModel());
}

TEST(RoutingFlowTest, DimensionAndUnaryDimensionLimit) {
  IndexManager manager(2, 10, NodeIndex(0));
  Model model(manager);
  model.AddDimension(
      model.RegisterUnaryTransitCallback([](int64_t) { return 1LL; }), 1, 1,
      true, "unary_stop_count");
  model.AddDimension(
      model.RegisterTransitCallback([](int64_t, int64_t) { return 1LL; }), 1, 1,
      true, "stop_count");
  EXPECT_TRUE(model.IsMatchingModel());
}

TEST(RoutingFlowTest, UnaryDimensionNoLimitAndUnaryDimensionLimit) {
  IndexManager manager(2, 10, NodeIndex(0));
  Model model(manager);
  model.AddDimension(
      model.RegisterUnaryTransitCallback([](int64_t) { return 1LL; }), 1, 1,
      true, "unary_stop_count");
  model.AddDimension(
      model.RegisterUnaryTransitCallback([](int64_t) { return 1LL; }),
      kint64max, kint64max, true, "stop_count");
  EXPECT_TRUE(model.IsMatchingModel());
}

TEST(RoutingFlowTest, PickupDeliveryUnaryDimensionLimit) {
  IndexManager manager(5, 10, NodeIndex(0));
  Model model(manager);
  model.AddPickupAndDelivery(1, 2);
  model.AddPickupAndDelivery(3, 4);
  model.AddDimension(
      model.RegisterUnaryTransitCallback([](int64_t) { return 1LL; }), 2, 2,
      true, "stop_count");
  EXPECT_TRUE(model.IsMatchingModel());
}

TEST(RoutingFlowTest, PickupDeliverySetUnaryDimensionLimit) {
  IndexManager manager(9, 10, NodeIndex(0));
  Model model(manager);
  model.AddPickupAndDeliverySets(model.AddDisjunction({1, 2}),
                                 model.AddDisjunction({3, 4}));
  model.AddPickupAndDeliverySets(model.AddDisjunction({5, 6}),
                                 model.AddDisjunction({7, 8}));
  model.AddDimension(
      model.RegisterUnaryTransitCallback([](int64_t) { return 1LL; }), 2, 2,
      true, "stop_count");
  EXPECT_TRUE(model.IsMatchingModel());
}

TEST(RoutingFlowTest, MixNodesUnaryDimensionLimit) {
  IndexManager manager(8, 10, NodeIndex(0));
  Model model(manager);
  model.AddPickupAndDeliverySets(model.AddDisjunction({1, 2}),
                                 model.AddDisjunction({3, 4}));
  model.AddPickupAndDelivery(5, 6);
  // 7 is single node
  model.AddDimension(model.RegisterUnaryTransitCallback(
                         [](int64_t i) { return (i == 7) ? 2LL : 1LL; }),
                     2, 2, true, "stop_count");
  EXPECT_TRUE(model.IsMatchingModel());
}

TEST(RoutingFlowTest, SimpleDisjunction) {
  IndexManager manager(8, 10, NodeIndex(0));
  Model model(manager);
  model.AddDisjunction({1, 2});
  model.AddDimension(
      model.RegisterUnaryTransitCallback([](int64_t) { return 2; }), 2, 2, true,
      "stop_count");
  EXPECT_TRUE(model.IsMatchingModel());
}

TEST(RoutingFlowTest, HardMaxDisjunction) {
  IndexManager manager(8, 10, NodeIndex(0));
  Model model(manager);
  model.AddDimension(
      model.RegisterUnaryTransitCallback([](int64_t) { return 2; }), 2, 2, true,
      "stop_count");
  Model::DisjunctionIndex disj_index = model.MakeDisjunction({2, 3, 4, 5});
  model.SetDisjunctionHardMaximum(disj_index, 2);
  EXPECT_FALSE(model.IsMatchingModel());
  model.SetDisjunctionHardMaximum(disj_index, 1);
  EXPECT_TRUE(model.IsMatchingModel());
  model.SetDisjunctionHardMinimum(disj_index, 1);
  EXPECT_TRUE(model.IsMatchingModel());
  model.SetDisjunctionHardMinimum(disj_index, 0);
  model.SetDisjunctionSoftMaximum(disj_index, 0, 3, Model::PENALIZE_ONCE);
  EXPECT_FALSE(model.IsMatchingModel());
  model.SetDisjunctionSoftMaximum(disj_index, 1, 3, Model::PENALIZE_ONCE);
  EXPECT_FALSE(model.IsMatchingModel());
}

}  // namespace
}  // namespace operations_research::routing
