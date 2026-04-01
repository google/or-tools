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

#include "ortools/graph/min_cost_flow.h"

#include <algorithm>  // For max.
#include <cstdint>
#include <limits>
#include <memory>
#include <random>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/random/distributions.h"
#include "absl/strings/str_format.h"
#include "absl/types/span.h"
#include "benchmark/benchmark.h"
#include "gtest/gtest.h"
#include "ortools/algorithms/binary_search.h"
#include "ortools/graph_base/graph.h"
#include "ortools/linear_solver/linear_solver.h"

namespace operations_research {
namespace {

using FlowQuantity = int64_t;
using CostValue = int64_t;

TEST(SolveTest, CapacityTooLarge) {
  using Graph = ::util::ReverseArcListGraph<int64_t, int64_t>;
  using Solver =
      ::operations_research::GenericMinCostFlow<Graph, int64_t, int64_t>;

  const int num_nodes = 6;
  const int num_arcs = 10;
  auto graph = std::make_unique<Graph>(num_nodes, num_arcs);
  auto solver = std::make_unique<Solver>(graph.get());
  const std::vector<int64_t> tails = {1, 2, 3, 4, 5, 0, 1, 2, 3, 4};
  const std::vector<int64_t> heads = {0, 1, 2, 3, 4, 5, 5, 5, 5, 5};
  const std::vector<int64_t> capacities = {
      3184525836262886912, 3184525836262886912, 3184525836262886912,
      3184525836262886912, 3184525836262886912, 1025,
      3184525836262886914, 3184525836262886914, 3184525836262886914,
      3184525836262886914,
  };
  const std::vector<int64_t> objectives(num_arcs, 0);
  const std::vector<int64_t> supplies = {-3184525836262885888, 1, 1, 1, 1,
                                         3184525836262885884};
  for (int64_t e = 0; e < num_arcs; ++e) {
    graph->AddArc(/*tail=*/tails[e], /*head=*/heads[e]);
    solver->SetArcCapacity(/*arc=*/e, /*new_capacity=*/capacities[e]);
    solver->SetArcUnitCost(/*arc=*/e, /*unit_cost=*/objectives[e]);
  }
  for (int64_t n = 0; n < num_nodes; ++n) {
    solver->SetNodeSupply(/*node=*/n, /*supply=*/supplies[n]);
  }

  // This one can actually be "corrected" by our simple heuristic.
  EXPECT_TRUE(solver->Solve());
  EXPECT_EQ(solver->status(), Solver::OPTIMAL);
}

TEST(SolveTest, CapacityTooLarge2) {
  using Graph = ::util::ReverseArcListGraph<int64_t, int64_t>;
  using Solver =
      ::operations_research::GenericMinCostFlow<Graph, int64_t, int64_t>;

  const int num_nodes = 3;
  const int num_arcs = 6;
  auto graph = std::make_unique<Graph>(num_nodes, num_arcs);
  auto solver = std::make_unique<Solver>(graph.get());

  // We construct a double cycle so that the incoming/outgoing flow cannot be
  // easily bounded.
  const int64_t kint64max = std::numeric_limits<int64_t>::max();
  const std::vector<int64_t> tails = {0, 0, 1, 1, 2, 2};
  const std::vector<int64_t> heads = {1, 1, 2, 2, 0, 0};
  const std::vector<int64_t> capacities(num_arcs, kint64max - 10);
  const std::vector<int64_t> objectives(num_arcs, 0);
  const std::vector<int64_t> supplies = {-(kint64max - 10), kint64max - 10, 0};

  for (int64_t e = 0; e < num_arcs; ++e) {
    graph->AddArc(/*tail=*/tails[e], /*head=*/heads[e]);
    solver->SetArcCapacity(/*arc=*/e, /*new_capacity=*/capacities[e]);
    solver->SetArcUnitCost(/*arc=*/e, /*unit_cost=*/objectives[e]);
  }
  for (int64_t n = 0; n < num_nodes; ++n) {
    solver->SetNodeSupply(/*node=*/n, /*supply=*/supplies[n]);
  }

  EXPECT_FALSE(solver->Solve());
  EXPECT_EQ(solver->status(), Solver::BAD_CAPACITY_RANGE);
}

template <typename Graph>
void GenericMinCostFlowTester(
    const typename Graph::NodeIndex num_nodes,
    const typename Graph::ArcIndex num_arcs, const FlowQuantity* node_supply,
    const typename Graph::NodeIndex* tail,
    const typename Graph::NodeIndex* head, const CostValue* cost,
    const FlowQuantity* capacity, const CostValue expected_flow_cost,
    const FlowQuantity* expected_flow,
    const typename GenericMinCostFlow<Graph>::Status expected_status) {
  Graph graph(num_nodes, num_arcs);
  for (int arc = 0; arc < num_arcs; ++arc) {
    graph.AddArc(tail[arc], head[arc]);
  }
  std::vector<typename Graph::ArcIndex> permutation;
  graph.Build(&permutation);
  EXPECT_TRUE(permutation.empty());

  GenericMinCostFlow<Graph> min_cost_flow(&graph);
  for (int arc = 0; arc < num_arcs; ++arc) {
    min_cost_flow.SetArcUnitCost(arc, cost[arc]);
    min_cost_flow.SetArcCapacity(arc, capacity[arc]);
    EXPECT_EQ(min_cost_flow.UnitCost(arc), cost[arc]);
    EXPECT_EQ(min_cost_flow.Capacity(arc), capacity[arc]);
  }
  for (int i = 0; i < num_nodes; ++i) {
    min_cost_flow.SetNodeSupply(i, node_supply[i]);
    EXPECT_EQ(min_cost_flow.Supply(i), node_supply[i]);
  }
  for (int options = 0; options < 2; ++options) {
    min_cost_flow.SetUseUpdatePrices(options & 1);
    bool ok = min_cost_flow.Solve();
    typename GenericMinCostFlow<Graph>::Status status = min_cost_flow.status();
    EXPECT_EQ(expected_status, status);
    if (expected_status == GenericMinCostFlow<Graph>::OPTIMAL) {
      EXPECT_TRUE(ok);
      CostValue total_flow_cost = min_cost_flow.GetOptimalCost();
      EXPECT_EQ(expected_flow_cost, total_flow_cost);
      for (int i = 0; i < num_arcs; ++i) {
        EXPECT_EQ(expected_flow[i], min_cost_flow.Flow(i)) << " i = " << i;
      }
    } else if (expected_status == GenericMinCostFlow<Graph>::INFEASIBLE) {
      EXPECT_FALSE(ok);
    }
  }
}

template <typename Graph>
class GenericMinCostFlowTest : public ::testing::Test {};

typedef ::testing::Types<util::ReverseArcListGraph<>,
                         util::ReverseArcStaticGraph<>>
    GraphTypes;

TYPED_TEST_SUITE(GenericMinCostFlowTest, GraphTypes);

TYPED_TEST(GenericMinCostFlowTest, CapacityRange) {
  // Check that we can set capacities to large numbers.
  const int kNumNodes = 7;
  const int kNumArcs = 12;
  const FlowQuantity kNodeSupply[kNumNodes] = {20, 10, 25, -11, -13, -17, -14};
  const typename TypeParam::NodeIndex kTail[kNumArcs] = {0, 0, 0, 0, 1, 1,
                                                         1, 1, 2, 2, 2, 2};
  const typename TypeParam::NodeIndex kHead[kNumArcs] = {3, 4, 5, 6, 3, 4,
                                                         5, 6, 3, 4, 5, 6};
  const CostValue kCost[kNumArcs] = {1, 6, 3, 5, 7, 3, 1, 6, 9, 4, 5, 3};
  // Since MinCostFlow stores node excess as a FlowQuantity, one must take care
  // to check that the total flow in/out of a node is less than kint64max.  To
  // guarantee this here, we set kCapMax to kint64max / 4 since the maximum
  // degree of a node is 4.
  const int64_t kCapMax = std::numeric_limits<int64_t>::max() / 4;
  const FlowQuantity kCapacity[kNumArcs] = {kCapMax, kCapMax, kCapMax, kCapMax,
                                            kCapMax, kCapMax, kCapMax, kCapMax,
                                            kCapMax, kCapMax, kCapMax, kCapMax};
  const CostValue kExpectedFlowCost = 138;
  const FlowQuantity kExpectedFlow[kNumArcs] = {11, 0, 9, 0,  0, 2,
                                                8,  0, 0, 11, 0, 14};
  GenericMinCostFlowTester<TypeParam>(
      kNumNodes, kNumArcs, kNodeSupply, kTail, kHead, kCost, kCapacity,
      kExpectedFlowCost, kExpectedFlow, GenericMinCostFlow<TypeParam>::OPTIMAL);
}

TYPED_TEST(GenericMinCostFlowTest, Test1) {
  const int kNumNodes = 2;
  const int kNumArcs = 1;
  const FlowQuantity kNodeSupply[kNumNodes] = {12, -12};
  const typename TypeParam::NodeIndex kTail[kNumArcs] = {0};
  const typename TypeParam::NodeIndex kHead[kNumArcs] = {1};
  const CostValue kCost[kNumArcs] = {10};
  const FlowQuantity kCapacity[kNumArcs] = {20};
  const CostValue kExpectedFlowCost = 120;
  const FlowQuantity kExpectedFlow[kNumArcs] = {12};
  GenericMinCostFlowTester<TypeParam>(
      kNumNodes, kNumArcs, kNodeSupply, kTail, kHead, kCost, kCapacity,
      kExpectedFlowCost, kExpectedFlow, GenericMinCostFlow<TypeParam>::OPTIMAL);
}

TYPED_TEST(GenericMinCostFlowTest, Test2) {
  const int kNumNodes = 7;
  const int kNumArcs = 12;
  const FlowQuantity kNodeSupply[kNumNodes] = {20, 10, 25, -11, -13, -17, -14};
  const typename TypeParam::NodeIndex kTail[kNumArcs] = {0, 0, 0, 0, 1, 1,
                                                         1, 1, 2, 2, 2, 2};
  const typename TypeParam::NodeIndex kHead[kNumArcs] = {3, 4, 5, 6, 3, 4,
                                                         5, 6, 3, 4, 5, 6};
  const CostValue kCost[kNumArcs] = {1, 6, 3, 5, 7, 3, 1, 6, 9, 4, 5, 3};
  const FlowQuantity kCapacity[kNumArcs] = {100, 100, 100, 100, 100, 100,
                                            100, 100, 100, 100, 100, 100};
  const CostValue kExpectedFlowCost = 138;
  const FlowQuantity kExpectedFlow[kNumArcs] = {11, 0, 9, 0,  0, 2,
                                                8,  0, 0, 11, 0, 14};
  GenericMinCostFlowTester<TypeParam>(
      kNumNodes, kNumArcs, kNodeSupply, kTail, kHead, kCost, kCapacity,
      kExpectedFlowCost, kExpectedFlow, GenericMinCostFlow<TypeParam>::OPTIMAL);
}

TYPED_TEST(GenericMinCostFlowTest, Test3) {
  const int kNumNodes = 7;
  const int kNumArcs = 12;
  const FlowQuantity kNodeSupply[kNumNodes] = {20, 10, 25, -11, -13, -17, -14};
  const typename TypeParam::NodeIndex kTail[kNumArcs] = {0, 0, 0, 0, 1, 1,
                                                         1, 1, 2, 2, 2, 2};
  const typename TypeParam::NodeIndex kHead[kNumArcs] = {3, 4, 5, 6, 3, 4,
                                                         5, 6, 3, 4, 5, 6};
  const CostValue kCost[kNumArcs] = {0, 0, 1, 1, 1, 1, 0, 1, 0, 1, 0, 0};
  const FlowQuantity kCapacity[kNumArcs] = {100, 100, 100, 100, 100, 100,
                                            100, 100, 100, 100, 100, 100};
  const CostValue kExpectedFlowCost = 0;
  const FlowQuantity kExpectedFlow[kNumArcs] = {7,  13, 0, 0, 0, 0,
                                                10, 0,  4, 0, 7, 14};
  GenericMinCostFlowTester<TypeParam>(
      kNumNodes, kNumArcs, kNodeSupply, kTail, kHead, kCost, kCapacity,
      kExpectedFlowCost, kExpectedFlow, GenericMinCostFlow<TypeParam>::OPTIMAL);
}

// Test on a 4x4 matrix. Example taken from
// http://www.ee.oulu.fi/~mpa/matreng/eem1_2-1.htm
TYPED_TEST(GenericMinCostFlowTest, Small4x4Matrix) {
  const int kNumSources = 4;
  const int kNumTargets = 4;
  const CostValue kCost[kNumSources][kNumTargets] = {{90, 75, 75, 80},
                                                     {35, 85, 55, 65},
                                                     {125, 95, 90, 105},
                                                     {45, 110, 95, 115}};
  const CostValue kExpectedCost = 275;
  TypeParam graph(kNumSources + kNumTargets, kNumSources * kNumTargets);
  for (typename TypeParam::NodeIndex source = 0; source < kNumSources;
       ++source) {
    for (typename TypeParam::NodeIndex target = 0; target < kNumTargets;
         ++target) {
      graph.AddArc(source, kNumSources + target);
    }
  }
  std::vector<typename TypeParam::ArcIndex> permutation;
  graph.Build(&permutation);
  EXPECT_TRUE(permutation.empty());

  GenericMinCostFlow<TypeParam> min_cost_flow(&graph);
  int arc = 0;
  for (typename TypeParam::NodeIndex source = 0; source < kNumSources;
       ++source) {
    for (typename TypeParam::NodeIndex target = 0; target < kNumTargets;
         ++target) {
      min_cost_flow.SetArcUnitCost(arc, kCost[source][target]);
      min_cost_flow.SetArcCapacity(arc, 1);
      ++arc;
    }
  }
  for (typename TypeParam::NodeIndex source = 0; source < kNumSources;
       ++source) {
    min_cost_flow.SetNodeSupply(source, 1);
  }
  for (typename TypeParam::NodeIndex target = 0; target < kNumTargets;
       ++target) {
    min_cost_flow.SetNodeSupply(kNumSources + target, -1);
  }
  EXPECT_TRUE(min_cost_flow.Solve());
  EXPECT_EQ(GenericMinCostFlow<TypeParam>::OPTIMAL, min_cost_flow.status());
  CostValue total_flow_cost = min_cost_flow.GetOptimalCost();
  EXPECT_EQ(kExpectedCost, total_flow_cost);
}

// Test that very large flow quantities do not overflow and that the total flow
// cost in cases of overflows stays capped at INT64_MAX.
TYPED_TEST(GenericMinCostFlowTest, TotalFlowCostOverflow) {
  const int kNumNodes = 2;
  const int kNumArcs = 1;
  const FlowQuantity kNodeSupply[kNumNodes] = {1LL << 61, -(1LL << 61)};
  const typename TypeParam::NodeIndex kTail[kNumArcs] = {0};
  const typename TypeParam::NodeIndex kHead[kNumArcs] = {1};
  const CostValue kCost[kNumArcs] = {10};
  const FlowQuantity kCapacity[kNumArcs] = {1LL << 61};
  const CostValue kExpectedFlowCost = std::numeric_limits<int64_t>::max();
  const FlowQuantity kExpectedFlow[kNumArcs] = {1LL << 61};
  GenericMinCostFlowTester<TypeParam>(
      kNumNodes, kNumArcs, kNodeSupply, kTail, kHead, kCost, kCapacity,
      kExpectedFlowCost, kExpectedFlow, GenericMinCostFlow<TypeParam>::OPTIMAL);
}

TEST(GenericMinCostFlowTest, OverflowPrevention1) {
  util::ReverseArcListGraph<> graph;
  const int arc = graph.AddArc(0, 1);

  GenericMinCostFlow<util::ReverseArcListGraph<>> mcf(&graph);
  mcf.SetArcCapacity(arc, std::numeric_limits<int64_t>::max() - 1);
  mcf.SetArcUnitCost(arc, -std::numeric_limits<int64_t>::max() + 1);
  mcf.SetNodeSupply(0, std::numeric_limits<int64_t>::max());
  mcf.SetNodeSupply(1, -std::numeric_limits<int64_t>::max());

  EXPECT_FALSE(mcf.Solve());
  EXPECT_EQ(mcf.status(), MinCostFlowBase::BAD_CAPACITY_RANGE);
}

TEST(GenericMinCostFlowTest, OverflowPrevention2) {
  util::ReverseArcListGraph<> graph;
  const int arc = graph.AddArc(0, 0);

  GenericMinCostFlow<util::ReverseArcListGraph<>> mcf(&graph);
  mcf.SetArcCapacity(arc, std::numeric_limits<int64_t>::max() - 1);
  mcf.SetArcUnitCost(arc, -std::numeric_limits<int64_t>::max() + 1);

  EXPECT_FALSE(mcf.Solve());
  EXPECT_EQ(mcf.status(), MinCostFlowBase::BAD_COST_RANGE);
}

TEST(GenericMinCostFlowTest, SelfLoop) {
  util::ReverseArcListGraph<> graph;
  const int arc = graph.AddArc(0, 0);

  GenericMinCostFlow<util::ReverseArcListGraph<>> mcf(&graph);
  const int64_t kMaxCost = std::numeric_limits<int64_t>::max();
  mcf.SetArcCapacity(arc, kMaxCost - 1);
  mcf.SetArcUnitCost(arc, -kMaxCost / 4);

  EXPECT_TRUE(mcf.Solve());
  EXPECT_EQ(mcf.status(), MinCostFlowBase::OPTIMAL);
  EXPECT_EQ(mcf.GetOptimalCost(), kMaxCost);  // Indicate overflow.
  EXPECT_EQ(mcf.Flow(arc), kMaxCost - 1);
}

TEST(SimpleMinCostFlowTest, Empty) {
  SimpleMinCostFlow min_cost_flow;
  EXPECT_EQ(SimpleMinCostFlow::OPTIMAL, min_cost_flow.Solve());
  EXPECT_EQ(0, min_cost_flow.NumNodes());
  EXPECT_EQ(0, min_cost_flow.NumArcs());
  EXPECT_EQ(0, min_cost_flow.OptimalCost());
  EXPECT_EQ(0, min_cost_flow.MaximumFlow());
}

TEST(SimpleMinCostFlowTest, NegativeCost) {
  SimpleMinCostFlow min_cost_flow;
  min_cost_flow.AddArcWithCapacityAndUnitCost(0, 1, 10, -10);
  min_cost_flow.AddArcWithCapacityAndUnitCost(1, 2, 10, -10);
  min_cost_flow.SetNodeSupply(0, 8);
  min_cost_flow.SetNodeSupply(2, -8);
  EXPECT_EQ(SimpleMinCostFlow::OPTIMAL, min_cost_flow.Solve());
  EXPECT_EQ(-160, min_cost_flow.OptimalCost());
  EXPECT_EQ(8, min_cost_flow.MaximumFlow());
}

TEST(SimpleMinCostFlowTest, NegativeCostWithLoop) {
  SimpleMinCostFlow min_cost_flow;
  // We have a loop 0 -> 1 -> 2 -> 0 with negative cost (but capacity bounded).
  min_cost_flow.AddArcWithCapacityAndUnitCost(0, 1, 10, -10);
  min_cost_flow.AddArcWithCapacityAndUnitCost(1, 2, 10, -10);
  min_cost_flow.AddArcWithCapacityAndUnitCost(2, 0, 10, -10);
  min_cost_flow.AddArcWithCapacityAndUnitCost(0, 3, 10, -10);
  min_cost_flow.SetNodeSupply(0, 8);
  min_cost_flow.SetNodeSupply(3, -8);
  EXPECT_EQ(SimpleMinCostFlow::OPTIMAL, min_cost_flow.Solve());
  EXPECT_EQ(-300 - 80, min_cost_flow.OptimalCost());
  EXPECT_EQ(8, min_cost_flow.MaximumFlow());
}

TEST(SimpleMinCostFlowTest, SelfLoop) {
  SimpleMinCostFlow min_cost_flow;
  min_cost_flow.AddArcWithCapacityAndUnitCost(0, 0, 10, 0);
  min_cost_flow.AddArcWithCapacityAndUnitCost(0, 1, 10, 0);
  min_cost_flow.AddArcWithCapacityAndUnitCost(1, 1, 10, 0);
  min_cost_flow.AddArcWithCapacityAndUnitCost(1, 2, 10, 0);
  min_cost_flow.AddArcWithCapacityAndUnitCost(2, 2, 10, 0);
  min_cost_flow.SetNodeSupply(0, 8);
  min_cost_flow.SetNodeSupply(2, -8);
  EXPECT_EQ(SimpleMinCostFlow::OPTIMAL, min_cost_flow.Solve());
  EXPECT_EQ(0, min_cost_flow.OptimalCost());
  EXPECT_EQ(8, min_cost_flow.MaximumFlow());
  EXPECT_EQ(8, min_cost_flow.Flow(1));
  EXPECT_EQ(8, min_cost_flow.Flow(3));
}

TEST(SimpleMinCostFlowTest, SelfLoopWithNegativeCost) {
  SimpleMinCostFlow min_cost_flow;
  min_cost_flow.AddArcWithCapacityAndUnitCost(0, 0, 10, 0);
  min_cost_flow.AddArcWithCapacityAndUnitCost(0, 1, 10, 0);
  min_cost_flow.AddArcWithCapacityAndUnitCost(1, 1, 10, -10);
  min_cost_flow.AddArcWithCapacityAndUnitCost(1, 2, 10, 0);
  min_cost_flow.AddArcWithCapacityAndUnitCost(2, 2, 10, 0);
  min_cost_flow.SetNodeSupply(0, 8);
  min_cost_flow.SetNodeSupply(2, -8);
  EXPECT_EQ(SimpleMinCostFlow::OPTIMAL, min_cost_flow.Solve());
  EXPECT_EQ(-100, min_cost_flow.OptimalCost());
  EXPECT_EQ(8, min_cost_flow.MaximumFlow());
  EXPECT_EQ(8, min_cost_flow.Flow(1));
  EXPECT_EQ(10, min_cost_flow.Flow(2));
  EXPECT_EQ(8, min_cost_flow.Flow(3));
}

TEST(SimpleMinCostFlowTest, FeasibleProblem) {
  const int kNumNodes = 7;
  const int kNumArcs = 12;
  const FlowQuantity kNodeSupply[kNumNodes] = {20, 10, 25, -11, -13, -17, -14};
  const SimpleMinCostFlow::NodeIndex kTail[kNumArcs] = {0, 0, 0, 0, 1, 1,
                                                        1, 1, 2, 2, 2, 2};
  const SimpleMinCostFlow::NodeIndex kHead[kNumArcs] = {3, 4, 5, 6, 3, 4,
                                                        5, 6, 3, 4, 5, 6};
  const CostValue kCost[kNumArcs] = {0, 0, 1, 1, 1, 1, 0, 1, 0, 1, 0, 0};
  const FlowQuantity kCapacity[kNumArcs] = {100, 100, 100, 100, 100, 100,
                                            100, 100, 100, 100, 100, 100};
  const CostValue kExpectedFlowCost = 0;
  const CostValue kExpectedFlowVolume = 55;
  const FlowQuantity kExpectedFlow[kNumArcs] = {7,  13, 0, 0, 0, 0,
                                                10, 0,  4, 0, 7, 14};

  SimpleMinCostFlow min_cost_flow;
  for (SimpleMinCostFlow::NodeIndex node = 0; node < kNumNodes; ++node) {
    min_cost_flow.SetNodeSupply(node, kNodeSupply[node]);
  }
  for (SimpleMinCostFlow::ArcIndex arc = 0; arc < kNumArcs; ++arc) {
    EXPECT_EQ(arc, min_cost_flow.AddArcWithCapacityAndUnitCost(
                       kTail[arc], kHead[arc], kCapacity[arc], kCost[arc]));
  }
  SimpleMinCostFlow::Status status = min_cost_flow.Solve();
  EXPECT_EQ(SimpleMinCostFlow::OPTIMAL, status);
  EXPECT_EQ(kExpectedFlowCost, min_cost_flow.OptimalCost());
  EXPECT_EQ(kExpectedFlowVolume, min_cost_flow.MaximumFlow());
  for (SimpleMinCostFlow::ArcIndex arc = 0; arc < kNumArcs; ++arc) {
    EXPECT_EQ(kExpectedFlow[arc], min_cost_flow.Flow(arc))
        << " for Arc #" << arc << ": " << kTail[arc] << "->" << kHead[arc];
  }
}

TEST(SimpleMinCostFlowTest, InfeasibleProblem) {
  const int kNumNodes = 7;
  const int kNumArcs = 12;
  const FlowQuantity kNodeSupply[kNumNodes] = {20, 10, 25, -11, -13, -17, -14};
  const SimpleMinCostFlow::NodeIndex kTail[kNumArcs] = {0, 0, 0, 0, 1, 1,
                                                        1, 1, 2, 2, 2, 2};
  const SimpleMinCostFlow::NodeIndex kHead[kNumArcs] = {3, 4, 5, 6, 3, 4,
                                                        5, 6, 3, 4, 5, 6};
  const CostValue kCost[kNumArcs] = {0, 0, 1, 1, 1, 1, 0, 1, 0, 1, 0, 0};

  SimpleMinCostFlow min_cost_flow;
  for (SimpleMinCostFlow::NodeIndex node = 0; node < kNumNodes; ++node) {
    min_cost_flow.SetNodeSupply(node, kNodeSupply[node]);
  }
  for (SimpleMinCostFlow::ArcIndex arc = 0; arc < kNumArcs; ++arc) {
    min_cost_flow.AddArcWithCapacityAndUnitCost(kTail[arc], kHead[arc], 1.0,
                                                kCost[arc]);
  }
  EXPECT_EQ(SimpleMinCostFlow::INFEASIBLE, min_cost_flow.Solve());
  EXPECT_EQ(SimpleMinCostFlow::OPTIMAL,
            min_cost_flow.SolveMaxFlowWithMinCost());
  // There should be unit flow through all the arcs we added.
  EXPECT_EQ(6, min_cost_flow.OptimalCost());
  EXPECT_EQ(12, min_cost_flow.MaximumFlow());
  for (SimpleMinCostFlow::ArcIndex arc = 0; arc < kNumArcs; ++arc) {
    EXPECT_EQ(1, min_cost_flow.Flow(arc))
        << " for Arc #" << arc << ": " << kTail[arc] << "->" << kHead[arc];
  }
}

// Create a single path graph with large arc unit cost.
// Note that the capacity does not directly influence the max usable cost.
TEST(SimpleMinCostFlowTest, OverflowCostBound) {
  const int n = 100;
  const int64_t kCapacity = 1'000'000;
  const int64_t kint64Max = std::numeric_limits<int64_t>::max();

  const int64_t safe_divisor =
      BinarySearch<int64_t>(kint64Max, 1, [](int64_t divisor) {
        SimpleMinCostFlow min_cost_flow;
        const int64_t kMaxCost = kint64Max / divisor;
        for (int i = 0; i + 1 < n; ++i) {
          min_cost_flow.AddArcWithCapacityAndUnitCost(i, i + 1, kCapacity,
                                                      kMaxCost);
        }
        min_cost_flow.SetNodeSupply(0, kCapacity);
        min_cost_flow.SetNodeSupply(n - 1, -kCapacity);
        const auto status = min_cost_flow.Solve();
        if (status == SimpleMinCostFlow::OPTIMAL) return true;
        CHECK_EQ(status, SimpleMinCostFlow::BAD_COST_RANGE);
        return false;
      });

  // On a single path graph, the threshold is around n ^ 2.
  EXPECT_EQ(safe_divisor, 11009);
}

template <typename Graph>
void GenerateCompleteGraph(const typename Graph::NodeIndex num_sources,
                           const typename Graph::NodeIndex num_targets,
                           Graph* graph) {
  const typename Graph::NodeIndex num_nodes = num_sources + num_targets;
  const typename Graph::ArcIndex num_arcs = num_sources * num_targets;
  graph->Reserve(num_nodes, num_arcs);
  for (typename Graph::NodeIndex source = 0; source < num_sources; ++source) {
    for (typename Graph::NodeIndex target = 0; target < num_targets; ++target) {
      graph->AddArc(source, target + num_sources);
    }
  }
}

template <typename Graph>
void GeneratePartialRandomGraph(const typename Graph::NodeIndex num_sources,
                                const typename Graph::NodeIndex num_targets,
                                const typename Graph::NodeIndex degree,
                                Graph* graph) {
  const typename Graph::NodeIndex num_nodes = num_sources + num_targets;
  const typename Graph::ArcIndex num_arcs = num_sources * degree;
  graph->Reserve(num_nodes, num_arcs);
  std::mt19937 randomizer(12345);
  for (typename Graph::NodeIndex source = 0; source < num_sources; ++source) {
    // For each source, we create degree - 1 random arcs.
    for (typename Graph::NodeIndex d = 0; d < degree - 1; ++d) {
      typename Graph::NodeIndex target =
          absl::Uniform(randomizer, 0, num_targets);
      graph->AddArc(source, target + num_sources);
    }
  }
  // Make sure that each target has at least one corresponding source.
  for (typename Graph::NodeIndex target = 0; target < num_targets; ++target) {
    typename Graph::NodeIndex source =
        absl::Uniform(randomizer, 0, num_sources);
    graph->AddArc(source, target + num_sources);
  }
}

template <typename Graph>
void GenerateRandomSupply(const typename Graph::NodeIndex num_sources,
                          const typename Graph::NodeIndex num_targets,
                          const typename Graph::NodeIndex num_generations,
                          const int64_t range, std::vector<int64_t>* supply) {
  const typename Graph::NodeIndex num_nodes = num_sources + num_targets;
  supply->resize(num_nodes, 0);
  std::mt19937 randomizer(12345);
  for (int64_t i = 0; i < num_sources * num_generations; ++i) {
    FlowQuantity q = absl::Uniform(randomizer, 0, range);
    int supply_index = absl::Uniform(randomizer, 0, num_sources);
    int demand_index = absl::Uniform(randomizer, 0, num_targets) + num_sources;
    (*supply)[supply_index] += q;
    (*supply)[demand_index] -= q;
  }
}

template <typename Graph>
void GenerateAssignmentSupply(const typename Graph::NodeIndex num_sources,
                              const typename Graph::NodeIndex num_targets,
                              std::vector<int64_t>* supply) {
  supply->resize(num_sources + num_targets);
  for (int i = 0; i < num_sources; ++i) {
    (*supply)[i] = 1;
  }
  for (int i = 0; i < num_targets; ++i) {
    (*supply)[i + num_sources] = -1;
  }
}

template <typename Graph>
void GenerateRandomArcValuations(const typename Graph::ArcIndex num_arcs,
                                 const int64_t max_range,
                                 std::vector<int64_t>* arc_valuation) {
  arc_valuation->resize(num_arcs);
  std::mt19937 randomizer(12345);
  for (typename Graph::ArcIndex arc = 0; arc < num_arcs; ++arc) {
    (*arc_valuation)[arc] = absl::Uniform(randomizer, 0, max_range);
  }
}

template <typename Graph>
void SetUpNetworkData(absl::Span<const typename Graph::ArcIndex> permutation,
                      absl::Span<const int64_t> supply,
                      absl::Span<const int64_t> arc_cost,
                      absl::Span<const int64_t> arc_capacity, Graph* graph,
                      GenericMinCostFlow<Graph>* min_cost_flow) {
  for (typename Graph::NodeIndex node = 0; node < graph->num_nodes(); ++node) {
    min_cost_flow->SetNodeSupply(node, supply[node]);
  }
  for (typename Graph::ArcIndex arc = 0; arc < graph->num_arcs(); ++arc) {
    typename Graph::ArcIndex permuted_arc =
        arc < permutation.size() ? permutation[arc] : arc;
    min_cost_flow->SetArcUnitCost(permuted_arc, arc_cost[arc]);
    min_cost_flow->SetArcCapacity(permuted_arc, arc_capacity[arc]);
  }
}

template <typename Graph>
CostValue SolveMinCostFlow(GenericMinCostFlow<Graph>* min_cost_flow) {
  bool ok = min_cost_flow->Solve();
  if (ok && min_cost_flow->status() == GenericMinCostFlow<Graph>::OPTIMAL) {
    CostValue cost = min_cost_flow->GetOptimalCost();
    CostValue computed_cost = 0;
    for (typename Graph::ArcIndex arc = 0;
         arc < min_cost_flow->graph()->num_arcs(); ++arc) {
      const FlowQuantity flow = min_cost_flow->Flow(arc);
      EXPECT_GE(min_cost_flow->Capacity(arc), flow);
      computed_cost += min_cost_flow->UnitCost(arc) * flow;
    }
    EXPECT_EQ(cost, computed_cost);
    return cost;
  } else {
    return 0;
  }
}

template <typename Graph>
CostValue SolveMinCostFlowWithLP(GenericMinCostFlow<Graph>* min_cost_flow) {
  MPSolver solver("LPSolver", MPSolver::GLOP_LINEAR_PROGRAMMING);
  const Graph* graph = min_cost_flow->graph();
  const typename Graph::NodeIndex num_nodes = graph->num_nodes();
  const typename Graph::ArcIndex num_arcs = graph->num_arcs();
  std::unique_ptr<MPConstraint*[]> constraint(new MPConstraint*[num_nodes]);
  for (typename Graph::NodeIndex node = 0; node < graph->num_nodes(); ++node) {
    constraint[node] = solver.MakeRowConstraint();
    FlowQuantity supply = min_cost_flow->Supply(node);
    constraint[node]->SetBounds(supply, supply);
  }
  std::unique_ptr<MPVariable*[]> var(new MPVariable*[num_arcs]);
  MPObjective* const objective = solver.MutableObjective();
  for (typename Graph::ArcIndex arc = 0; arc < graph->num_arcs(); ++arc) {
    var[arc] = solver.MakeNumVar(0.0, min_cost_flow->Capacity(arc),
                                 absl::StrFormat("v%d", arc));
    constraint[graph->Tail(arc)]->SetCoefficient(var[arc], 1.0);
    constraint[graph->Head(arc)]->SetCoefficient(var[arc], -1.0);
    objective->SetCoefficient(var[arc], min_cost_flow->UnitCost(arc));
  }
  solver.Solve();
  return static_cast<CostValue>(objective->Value() + .5);
}

template <typename Graph>
bool CheckAssignmentFeasibility(const Graph& graph,
                                absl::Span<const int64_t> supply) {
  for (typename Graph::NodeIndex node = 0; node < graph.num_nodes(); ++node) {
    if (supply[node] != 0) {
      EXPECT_FALSE(graph.OutgoingOrOppositeIncomingArcs(node).empty())
          << node << " has no incident arc";
    }
  }
  return true;
}

template <typename Graph>
struct MinCostFlowSolver {
  typedef FlowQuantity (*Solver)(GenericMinCostFlow<Graph>* min_cost_flow);
};

template <typename Graph>
void FullRandomAssignment(typename MinCostFlowSolver<Graph>::Solver f,
                          typename Graph::NodeIndex num_sources,
                          typename Graph::NodeIndex num_targets,
                          CostValue expected_cost1,
                          CostValue /*expected_cost2*/) {
  const CostValue kCostRange = 1000;
  Graph graph;
  GenerateCompleteGraph(num_sources, num_targets, &graph);
  std::vector<typename Graph::ArcIndex> permutation;
  graph.Build(&permutation);

  std::vector<int64_t> supply;
  GenerateAssignmentSupply<Graph>(num_sources, num_targets, &supply);
  EXPECT_TRUE(CheckAssignmentFeasibility(graph, supply));
  std::vector<int64_t> arc_capacity(graph.num_arcs(), 1);
  std::vector<int64_t> arc_cost(graph.num_arcs());
  GenerateRandomArcValuations<Graph>(graph.num_arcs(), kCostRange, &arc_cost);
  GenericMinCostFlow<Graph> min_cost_flow(&graph);
  SetUpNetworkData(permutation, supply, arc_cost, arc_capacity, &graph,
                   &min_cost_flow);

  CostValue cost = f(&min_cost_flow);
  EXPECT_EQ(expected_cost1, cost);
}

template <typename Graph>
void PartialRandomAssignment(typename MinCostFlowSolver<Graph>::Solver f,
                             typename Graph::NodeIndex num_sources,
                             typename Graph::NodeIndex num_targets,
                             CostValue expected_cost1,
                             CostValue /*expected_cost2*/) {
  const typename Graph::NodeIndex kDegree = 10;
  const CostValue kCostRange = 1000;
  Graph graph;
  GeneratePartialRandomGraph(num_sources, num_targets, kDegree, &graph);
  std::vector<typename Graph::ArcIndex> permutation;
  graph.Build(&permutation);

  std::vector<int64_t> supply;
  GenerateAssignmentSupply<Graph>(num_sources, num_targets, &supply);
  EXPECT_TRUE(CheckAssignmentFeasibility(graph, supply));

  EXPECT_EQ(graph.num_arcs(), num_sources * kDegree);
  std::vector<int64_t> arc_capacity(graph.num_arcs(), 1);
  std::vector<int64_t> arc_cost(graph.num_arcs());
  GenerateRandomArcValuations<Graph>(graph.num_arcs(), kCostRange, &arc_cost);
  GenericMinCostFlow<Graph> min_cost_flow(&graph);
  SetUpNetworkData(permutation, supply, arc_cost, arc_capacity, &graph,
                   &min_cost_flow);
  CostValue cost = f(&min_cost_flow);
  EXPECT_EQ(expected_cost1, cost);
}

template <typename Graph>
void ChangeCapacities(absl::Span<const typename Graph::ArcIndex> permutation,
                      absl::Span<const int64_t> arc_capacity,
                      FlowQuantity delta,
                      GenericMinCostFlow<Graph>* min_cost_flow,
                      float probability) {
  std::mt19937 randomizer(12345);
  const typename Graph::ArcIndex num_arcs = min_cost_flow->graph()->num_arcs();
  for (typename Graph::ArcIndex arc = 0; arc < num_arcs; ++arc) {
    typename Graph::ArcIndex permuted_arc =
        arc < permutation.size() ? permutation[arc] : arc;
    if (absl::Bernoulli(randomizer, probability)) {
      min_cost_flow->SetArcCapacity(
          permuted_arc, std::max(arc_capacity[arc] - delta, int64_t{0}));
    } else {
      min_cost_flow->SetArcCapacity(permuted_arc, arc_capacity[arc]);
    }
  }
}

template <typename Graph>
void PartialRandomFlow(typename MinCostFlowSolver<Graph>::Solver f,
                       typename Graph::NodeIndex num_sources,
                       typename Graph::NodeIndex num_targets,
                       CostValue expected_cost1, CostValue expected_cost2) {
  const typename Graph::NodeIndex kDegree = 15;
  const FlowQuantity kSupplyRange = 500;
  const int64_t kSupplyGens = 15;
  const FlowQuantity kCapacityRange = 10000;
  const CostValue kCostRange = 1000;
  const FlowQuantity kCapacityDelta = 500;
  const float kProbability = 0.9;
  Graph graph;
  GeneratePartialRandomGraph(num_sources, num_targets, kDegree, &graph);
  std::vector<typename Graph::ArcIndex> permutation;
  graph.Build(&permutation);

  std::vector<int64_t> supply;
  GenerateRandomSupply<Graph>(num_sources, num_targets, kSupplyGens,
                              kSupplyRange, &supply);
  EXPECT_TRUE(CheckAssignmentFeasibility(graph, supply));
  std::vector<int64_t> arc_capacity(graph.num_arcs());
  GenerateRandomArcValuations<Graph>(graph.num_arcs(), kCapacityRange,
                                     &arc_capacity);
  std::vector<int64_t> arc_cost(graph.num_arcs());
  GenerateRandomArcValuations<Graph>(graph.num_arcs(), kCostRange, &arc_cost);
  GenericMinCostFlow<Graph> min_cost_flow(&graph);
  SetUpNetworkData(permutation, supply, arc_cost, arc_capacity, &graph,
                   &min_cost_flow);

  CostValue cost = f(&min_cost_flow);
  EXPECT_EQ(expected_cost1, cost);
  ChangeCapacities(permutation, arc_capacity, kCapacityDelta, &min_cost_flow,
                   kProbability);
  cost = f(&min_cost_flow);
  EXPECT_EQ(expected_cost2, cost);
  ChangeCapacities(permutation, arc_capacity, 0, &min_cost_flow, 1.0);
  cost = f(&min_cost_flow);
  EXPECT_EQ(expected_cost1, cost);
}

template <typename Graph>
void FullRandomFlow(typename MinCostFlowSolver<Graph>::Solver f,
                    typename Graph::NodeIndex num_sources,
                    typename Graph::NodeIndex num_targets,
                    CostValue expected_cost1, CostValue expected_cost2) {
  const FlowQuantity kSupplyRange = 1000;
  const int64_t kSupplyGens = 10;
  const FlowQuantity kCapacityRange = 10000;
  const CostValue kCostRange = 1000;
  const FlowQuantity kCapacityDelta = 1000;
  const float kProbability = 0.9;
  Graph graph;
  GenerateCompleteGraph(num_sources, num_targets, &graph);
  std::vector<typename Graph::ArcIndex> permutation;
  graph.Build(&permutation);

  std::vector<int64_t> supply;
  GenerateRandomSupply<Graph>(num_sources, num_targets, kSupplyGens,
                              kSupplyRange, &supply);
  EXPECT_TRUE(CheckAssignmentFeasibility(graph, supply));
  std::vector<int64_t> arc_capacity(graph.num_arcs());
  GenerateRandomArcValuations<Graph>(graph.num_arcs(), kCapacityRange,
                                     &arc_capacity);
  std::vector<int64_t> arc_cost(graph.num_arcs());
  GenerateRandomArcValuations<Graph>(graph.num_arcs(), kCostRange, &arc_cost);
  GenericMinCostFlow<Graph> min_cost_flow(&graph);

  SetUpNetworkData(permutation, supply, arc_cost, arc_capacity, &graph,
                   &min_cost_flow);

  CostValue cost = f(&min_cost_flow);
  EXPECT_EQ(expected_cost1, cost);
  ChangeCapacities(permutation, arc_capacity, kCapacityDelta, &min_cost_flow,
                   kProbability);
  cost = f(&min_cost_flow);
  EXPECT_EQ(expected_cost2, cost);
  ChangeCapacities(permutation, arc_capacity, 0, &min_cost_flow, 1.0);
  cost = f(&min_cost_flow);
  EXPECT_EQ(expected_cost1, cost);
}

#define LP_AND_FLOW_TEST(test_name, size, expected_cost1, expected_cost2) \
  LP_ONLY_TEST(test_name, size, expected_cost1, expected_cost2)           \
  FLOW_ONLY_TEST(test_name, size, expected_cost1, expected_cost2)         \
  FLOW_ONLY_TEST_SG(test_name, size, expected_cost1, expected_cost2)

#define LP_ONLY_TEST(test_name, size, expected_cost1, expected_cost2)          \
  TEST(LPMinCostFlowTest, test_name##size) {                                   \
    test_name<util::ReverseArcListGraph<>>(SolveMinCostFlowWithLP, size, size, \
                                           expected_cost1, expected_cost2);    \
  }

#define FLOW_ONLY_TEST(test_name, size, expected_cost1, expected_cost2)     \
  TEST(MinCostFlowTest, test_name##size) {                                  \
    test_name<util::ReverseArcListGraph<>>(SolveMinCostFlow, size, size,    \
                                           expected_cost1, expected_cost2); \
  }

#define FLOW_ONLY_TEST_SG(test_name, size, expected_cost1, expected_cost2)    \
  TEST(MinCostFlowTestStaticGraph, test_name##size) {                         \
    test_name<util::ReverseArcStaticGraph<>>(SolveMinCostFlow, size, size,    \
                                             expected_cost1, expected_cost2); \
  }

// The times indicated below are in opt mode.
// The figures indicate the time with the LP solver and with MinCostFlow,
// respectively. _ indicates "N/A".

LP_AND_FLOW_TEST(FullRandomAssignment, 100, 1653, 0);  //  0.070s / 0.007s
LP_AND_FLOW_TEST(FullRandomAssignment, 300, 1487, 0);  //  0.5s / 0.121s

LP_AND_FLOW_TEST(PartialRandomFlow, 10, 9195615, 10720774);
LP_AND_FLOW_TEST(PartialRandomFlow, 100, 80098192, 95669398);  // 12ms / 8ms
LP_AND_FLOW_TEST(PartialRandomFlow, 1000, 770743566, 936886845);
// 1.6s / 0.094s

LP_AND_FLOW_TEST(FullRandomFlow, 100, 40998962, 81814978);   // 0.085s / 0.025s
LP_AND_FLOW_TEST(FullRandomFlow, 300, 67301515, 173406965);  // 0.7s / 0.412s

LP_AND_FLOW_TEST(PartialRandomAssignment, 100, 15418, 0);    // 0.012s/0.003s
LP_AND_FLOW_TEST(PartialRandomAssignment, 1000, 155105, 0);  // 0.416s/0.041s

// LARGE must be defined from the build command line to test larger instances.
#ifdef LARGE

LP_AND_FLOW_TEST(FullRandomAssignment, 1000, 1142, 0);  //  7.2s / 5.809s
FLOW_ONLY_TEST(FullRandomAssignment, 3000, 392, 0);     // 800s / 93.9s
FLOW_ONLY_TEST_SG(FullRandomAssignment, 3000, 392, 0);  // 40s

LP_AND_FLOW_TEST(PartialRandomAssignment, 10000, 3649506, 0);  // 22s / 0.953s
FLOW_ONLY_TEST(PartialRandomAssignment, 100000, 36722363, 0);  // 4740s / 23s
FLOW_ONLY_TEST_SG(PartialRandomAssignment, 100000, 36722363, 0);  // 4740s / 23s
FLOW_ONLY_TEST(PartialRandomAssignment, 1000000, 367732438, 0);   // _ / 430s
FLOW_ONLY_TEST_SG(PartialRandomAssignment, 1000000, 367732438, 0);  // 336s

LP_AND_FLOW_TEST(PartialRandomFlow, 2000, 3040966812, 3072394992);
// 7.15s / 0.269s
LP_AND_FLOW_TEST(FullRandomFlow, 800, 10588600, 12057369);
LP_AND_FLOW_TEST(FullRandomFlow, 1000, 9491720, 10994039);  // 14.4s / 13.183s
FLOW_ONLY_TEST(FullRandomFlow, 3000, 5588622, 7140712);     // 1460s / 488s
FLOW_ONLY_TEST_SG(FullRandomFlow, 3000, 5588622, 7140712);  // 230s

#endif  // LARGE

#undef LP_AND_FLOW_TEST
#undef LP_ONLY_TEST
#undef FLOW_ONLY_TEST
#undef FLOW_ONLY_TEST_SG

// Benchmark inspired from the existing problem of matching Youtube ads channels
// to Youtube users, maximizing the expected revenue:
// - Each channel needs an exact number of users assigned to it.
// - Each user has an upper limit on the number of channels they can be assigned
//   to, with a guarantee that this upper limit won't prevent the channels to
//   get their required number of users.
// - Each pair (user, channel) has a known expected revenue, which is modeled
//   as a small-ish integer (<3K). Using larger ranges can slightly impact
//   performance, and you should look for a good trade-off with the accuracy.
//
// IMPORTANT: don't run this with default flags! Use:
//   blaze run -c opt --linkopt=-static [--run_under=perflab] --
//   ortools/graph/min_cost_flow_test --benchmarks=all
//   --benchmark_min_iters=1 --heap_check= --benchmark_memory_usage
template <typename Graph, typename ArcFlowType, typename ArcScaledCostType,
          int kNumChannels, int kNumUsers>
void BM_MinCostFlowOnMultiMatchingProblem(benchmark::State& state) {
  std::mt19937 my_random(12345);
  // Average probability of a user-channel pair being matched.
  const double kDensity = 1.0 / 200;
  const int kMaxChannelsPerUser = 5 * static_cast<int>(kDensity * kNumChannels);
  const int kAverageNumUsersPerChannels =
      static_cast<int>(kDensity * kNumUsers);
  std::vector<int> num_users_per_channel(kNumChannels, -1);
  int total_demand = 0;
  for (int i = 0; i < kNumChannels; ++i) {
    num_users_per_channel[i] =
        1 + absl::Uniform(my_random, 0, 2 * kAverageNumUsersPerChannels - 1);
    total_demand += num_users_per_channel[i];
  }
  // User #j, when assigned to channel #i, is expected to generate
  // -expected_cost_per_channel_user[kNumUsers * i + j]: since MinCostFlow
  // only *minimizes* costs, and doesn't maximizes revenue, we just set
  // cost = -revenue.
  // To stress the algorithm, we generate a cost matrix that is highly skewed
  // and that would probably challenge greedy approaches: each user gets
  // a random coefficient, each channel as well, and then the expected revenue
  // of a (user, channel) pairing is the product of these coefficient, plus a
  // small (per-cell) random perturbation.
  std::vector<int16_t> expected_cost_per_channel_user(kNumChannels * kNumUsers);
  {
    std::vector<int16_t> channel_coeff(kNumChannels);
    for (int i = 0; i < kNumChannels; ++i) {
      channel_coeff[i] = absl::Uniform(my_random, 0, 48);
    }
    std::vector<int16_t> user_coeff(kNumUsers);
    for (int j = 0; j < kNumUsers; ++j) {
      user_coeff[j] = absl::Uniform(my_random, 0, 48);
    }
    for (int i = 0; i < kNumChannels; ++i) {
      for (int j = 0; j < kNumUsers; ++j) {
        expected_cost_per_channel_user[kNumUsers * i + j] =
            -channel_coeff[i] * user_coeff[j] - absl::Uniform(my_random, 0, 10);
      }
    }
  }
  CHECK_LE(total_demand, kNumUsers * kMaxChannelsPerUser);

  for (auto _ : state) {
    Graph graph(/*num_nodes=*/kNumUsers + kNumChannels + 1,
                /*arc_capacity=*/kNumChannels * kNumUsers + kNumUsers);
    // We model this problem as a graph (on which we'll do a min-cost flow):
    // - Each channel #i is a source node (index #i + 1) with supply
    //   num_users_per_channel[i].
    // - There is a global sink node (index 0) with a demand equal to the
    //   sum of num_users_per_channel.
    // - Each user #j is an intermediate node (index 1 + kNumChannels + j)
    //   with no supply or demand, but with an arc of capacity
    //   kMaxChannelsPerUser towards the global sink node (and of unit cost 0).
    // - There is an arc from each channel #i to each user #j, with capacity 1
    //   and unit cost expected_cost_per_channel_user[kNumUsers * i + j].
    for (int i = 0; i < kNumChannels; ++i) {
      for (int j = 0; j < kNumUsers; ++j) {
        graph.AddArc(/*tail=*/i + 1, /*head=*/kNumChannels + 1 + j);
      }
    }
    for (int j = 0; j < kNumUsers; ++j) {
      graph.AddArc(/*tail=*/kNumChannels + 1 + j, /*head=*/0);
    }
    std::vector<typename Graph::ArcIndex> permutation;
    graph.Build(&permutation);
    // To spare memory, we added arcs in the right order, so that no permutation
    // is needed. See graph.h.
    CHECK(permutation.empty());

    // To spare memory, the types are chosen as small as possible.
    GenericMinCostFlow<Graph, ArcFlowType, ArcScaledCostType> min_cost_flow(
        &graph);

    // We also disable the feasibility check which takes a huge amount of
    // memory.
    min_cost_flow.SetCheckFeasibility(false);

    min_cost_flow.SetNodeSupply(/*node=*/0, /*supply=*/-total_demand);
    // Now, set the arcs capacity and cost, in the same order as we created them
    // above.
    typename Graph::ArcIndex arc_index = 0;
    for (int i = 0; i < kNumChannels; ++i) {
      min_cost_flow.SetNodeSupply(
          /*node=*/i + 1, /*supply=*/num_users_per_channel[i]);
      for (int j = 0; j < kNumUsers; ++j) {
        min_cost_flow.SetArcUnitCost(
            arc_index, expected_cost_per_channel_user[kNumUsers * i + j]);
        min_cost_flow.SetArcCapacity(arc_index, 1);
        arc_index++;
      }
    }
    for (int j = 0; j < kNumUsers; ++j) {
      min_cost_flow.SetArcUnitCost(arc_index, 0);
      min_cost_flow.SetArcCapacity(arc_index, kMaxChannelsPerUser);
      arc_index++;
    }
    const bool solved_ok = min_cost_flow.Solve();
    CHECK(solved_ok);
    LOG(INFO) << "Maximum revenue: " << -min_cost_flow.GetOptimalCost();
  }
}

// We don't have more than 65536 nodes, so we use 16-bit integers to spare
// memory (and potentially speed up the code). Arc indices must be 32 bits
// though, since we have much more.
BENCHMARK(BM_MinCostFlowOnMultiMatchingProblem<
          util::ReverseArcStaticGraph<uint16_t, int32_t>, int16_t, int32_t,
          /*kNumChannels=*/20000, /*kNumUsers=*/20000>);
// We also benchmark with default parameter types for reference.
// We use fewer channels and users to avoid running out of memory.
BENCHMARK(BM_MinCostFlowOnMultiMatchingProblem<
          ::util::ReverseArcListGraph<>, int64_t, int64_t,
          /*kNumChannels=*/5000, /*kNumUsers=*/5000>);

}  // namespace
}  // namespace operations_research
