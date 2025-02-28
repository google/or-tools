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

#include "ortools/sat/routing_cuts.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/log.h"
#include "absl/random/distributions.h"
#include "absl/random/random.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/parse_test_proto.h"
#include "ortools/base/strong_vector.h"
#include "ortools/graph/max_flow.h"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/cuts.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/linear_constraint_manager.h"
#include "ortools/sat/model.h"
#include "ortools/sat/precedences.h"
#include "ortools/sat/sat_base.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {
namespace {

using ::google::protobuf::contrib::parse_proto::ParseTestProto;
using ::testing::AnyOf;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::EqualsProto;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;
using Relation = RouteRelationsHelper::Relation;

TEST(MinOutgoingFlowHelperTest, TwoNodesWithoutConstraints) {
  Model model;
  const std::vector<int> tails = {0, 1};
  const std::vector<int> heads = {1, 0};
  const std::vector<Literal> literals = {
      Literal(model.Add(NewBooleanVariable()), true),
      Literal(model.Add(NewBooleanVariable()), true)};
  MinOutgoingFlowHelper helper(2, tails, heads, literals, &model);

  const int min_flow = helper.ComputeMinOutgoingFlow({0, 1});
  const int tight_min_flow = helper.ComputeTightMinOutgoingFlow({0, 1});

  EXPECT_EQ(min_flow, 1);
  EXPECT_EQ(tight_min_flow, 1);
}

TEST(MinOutgoingFlowHelperTest, CapacityConstraints) {
  Model model;
  const int num_nodes = 5;
  // A complete graph with num_nodes.
  std::vector<int> tails;
  std::vector<int> heads;
  std::vector<Literal> literals;
  absl::flat_hash_map<std::pair<int, int>, Literal> literal_by_arc;
  for (int tail = 0; tail < num_nodes; ++tail) {
    for (int head = 0; head < num_nodes; ++head) {
      if (tail == head) continue;
      tails.push_back(tail);
      heads.push_back(head);
      literals.push_back(Literal(model.Add(NewBooleanVariable()), true));
      literal_by_arc[{tail, head}] = literals.back();
    }
  }
  // For each node, the load of the vehicle leaving it.
  std::vector<IntegerVariable> loads;
  const int max_capacity = 30;
  for (int n = 0; n < num_nodes; ++n) {
    loads.push_back(model.Add(NewIntegerVariable(0, max_capacity)));
  }
  // Capacity constraints.
  auto* repository = model.GetOrCreate<BinaryRelationRepository>();
  for (const auto& [arc, literal] : literal_by_arc) {
    const auto& [tail, head] = arc;
    // We consider that, at each node n other than the depot, n+10 items must be
    // picked up by the vehicle leaving n.
    const int head_load = head == 0 ? 0 : head + 10;
    // loads[head] - loads[tail] >= head_load
    repository->Add(literal, {loads[head], 1}, {loads[tail], -1}, head_load,
                    1000);
  }
  repository->Build();
  // Subject under test.
  MinOutgoingFlowHelper helper(num_nodes, tails, heads, literals, &model);

  const int min_flow = helper.ComputeMinOutgoingFlow({1, 2, 3, 4});
  const int tight_min_flow = helper.ComputeTightMinOutgoingFlow({1, 2, 3, 4});

  // Due to the capacity constraints, a feasible path can have at most 3 nodes,
  // hence at least two paths are needed. The lower bound of the vehicle load
  // at each node n appearing at position i should be computed as follows:
  //
  //            1  2  3  4  (position)
  //          -------------
  //   node 1 | 0 11 23  -
  //        2 | 0 12 23  -
  //        3 | 0 13 24  -
  //        4 | 0 14 24  -
  EXPECT_EQ(min_flow, 2);
  EXPECT_EQ(tight_min_flow, 2);
}

class DemandBasedMinOutgoingFlowHelperTest
    : public testing::TestWithParam<std::pair<bool, bool>> {};

TEST_P(DemandBasedMinOutgoingFlowHelperTest, BasicCapacities) {
  // If true, the load variables are the load of the vehicle leaving each node,
  // otherwise they are the load of the vehicle arriving at each node.
  const bool use_outgoing_load = GetParam().first;
  // If true, vehicles pickup items at each node, otherwise they deliver items.
  const bool pickup = GetParam().second;

  Model model;
  const int num_nodes = 5;
  // A complete graph with num_nodes.
  std::vector<int> tails;
  std::vector<int> heads;
  std::vector<Literal> literals;
  absl::flat_hash_map<std::pair<int, int>, Literal> literal_by_arc;
  for (int tail = 0; tail < num_nodes; ++tail) {
    for (int head = 0; head < num_nodes; ++head) {
      if (tail == head) continue;
      tails.push_back(tail);
      heads.push_back(head);
      literals.push_back(Literal(model.Add(NewBooleanVariable()), true));
      literal_by_arc[{tail, head}] = literals.back();
    }
  }
  const std::vector<int> demands = {0, 11, 12, 13, 14};
  std::vector<IntegerVariable> loads;
  const int max_capacity = 49;
  for (int n = 0; n < num_nodes; ++n) {
    if (pickup == use_outgoing_load) {
      loads.push_back(model.Add(NewIntegerVariable(demands[n], max_capacity)));
    } else {
      loads.push_back(
          model.Add(NewIntegerVariable(0, max_capacity - demands[n])));
    }
  }
  // Capacity constraints.
  auto* repository = model.GetOrCreate<BinaryRelationRepository>();
  for (const auto& [arc, literal] : literal_by_arc) {
    const auto& [tail, head] = arc;
    if (tail == 0 || head == 0) continue;
    if (pickup) {
      // loads[head] - loads[tail] >= demand
      repository->Add(literal, {loads[head], 1}, {loads[tail], -1},
                      demands[use_outgoing_load ? head : tail], 1000);
    } else {
      // loads[tail] - loads[head] >= demand
      repository->Add(literal, {loads[tail], 1}, {loads[head], -1},
                      demands[use_outgoing_load ? head : tail], 1000);
    }
  }
  repository->Build();
  std::unique_ptr<RouteRelationsHelper> route_relations_helper =
      RouteRelationsHelper::Create(num_nodes, tails, heads, literals, {},
                                   *repository);
  ASSERT_NE(route_relations_helper, nullptr);
  // Subject under test.
  MinOutgoingFlowHelper helper(num_nodes, tails, heads, literals, &model);

  const int min_flow = helper.ComputeDemandBasedMinOutgoingFlow(
      {1, 2, 3, 4}, *route_relations_helper);

  // The total demand is 50, and the maximum capacity is 49.
  EXPECT_EQ(min_flow, 2);
}

TEST_P(DemandBasedMinOutgoingFlowHelperTest,
       NodesWithoutIncomingOrOutgoingArc) {
  // If true, the load variables are the load of the vehicle leaving each node,
  // otherwise they are the load of the vehicle arriving at each node.
  const bool use_outgoing_load = GetParam().first;
  // If true, vehicles pickup items at each node, otherwise they deliver items.
  const bool pickup = GetParam().second;

  Model model;
  // A graph with 4 nodes and 4 arcs, with 1 node without incoming arc and 1
  // node without outgoing arc:
  //
  // --> 1 --> 2  -->
  //     ^     |
  //     |     v
  // --> 0 --> 3  -->
  //
  // We use "outside" arcs from/to node 4 otherwise the problem will be
  // infeasible.
  const int num_nodes = 5;
  const std::vector<int> tails = {0, 0, 1, 2, 4, 4, 2, 3};
  const std::vector<int> heads = {1, 3, 2, 3, 0, 1, 4, 4};
  std::vector<Literal> literals(tails.size());
  for (int i = 0; i < literals.size(); ++i) {
    literals[i] = Literal(model.Add(NewBooleanVariable()), true);
  }
  const std::vector<int> demands = {11, 12, 13, 14};
  std::vector<IntegerVariable> loads;
  const int max_capacity = 49;
  for (int n = 0; n < demands.size(); ++n) {
    if (pickup == use_outgoing_load) {
      loads.push_back(model.Add(NewIntegerVariable(demands[n], max_capacity)));
    } else {
      loads.push_back(
          model.Add(NewIntegerVariable(0, max_capacity - demands[n])));
    }
  }
  // Capacity constraints.
  auto* repository = model.GetOrCreate<BinaryRelationRepository>();
  for (int i = 0; i < 4; ++i) {
    const int head = heads[i];
    const int tail = tails[i];
    if (pickup) {
      // loads[head] - loads[tail] >= demand
      repository->Add(literals[i], {loads[head], 1}, {loads[tail], -1},
                      demands[use_outgoing_load ? head : tail], 1000);
    } else {
      // loads[tail] - loads[head] >= demand
      repository->Add(literals[i], {loads[tail], 1}, {loads[head], -1},
                      demands[use_outgoing_load ? head : tail], 1000);
    }
  }
  repository->Build();
  std::unique_ptr<RouteRelationsHelper> route_relations_helper =
      RouteRelationsHelper::Create(num_nodes, tails, heads, literals, {},
                                   *repository);
  ASSERT_NE(route_relations_helper, nullptr);
  // Subject under test.
  MinOutgoingFlowHelper helper(num_nodes, tails, heads, literals, &model);

  const int min_flow = helper.ComputeDemandBasedMinOutgoingFlow(
      {0, 1, 2, 3}, *route_relations_helper);

  // The total demand is 50, and the maximum capacity is 49.
  EXPECT_EQ(min_flow, 2);
}

INSTANTIATE_TEST_SUITE_P(AllCombinations, DemandBasedMinOutgoingFlowHelperTest,
                         testing::Values(std::make_pair(true, true),
                                         std::make_pair(true, false),
                                         std::make_pair(false, true),
                                         std::make_pair(false, false)));

TEST(MinOutgoingFlowHelperTest, NodeExpressionWithConstant) {
  // A graph with 3 nodes: 0 <--> 1 -(demand1)-> 2 <-(demand2)-> 0
  Model model;
  const int num_nodes = 3;
  std::vector<int> tails = {1, 0, 0, 1, 2};
  std::vector<int> heads = {2, 1, 2, 0, 0};
  std::vector<Literal> literals;
  for (int i = 0; i < tails.size(); ++i) {
    literals.push_back(Literal(model.Add(NewBooleanVariable()), true));
  }
  // The vehicle capacity and the demand at each node.
  const int capacity = 100;
  const int demand1 = 70;
  const int demand2 = 40;
  // The load of the vehicle arriving at node 1.
  const IntegerVariable load1 =
      model.Add(NewIntegerVariable(0, capacity - demand1));
  // The load of the vehicle arriving at node 2, minus `offset`.
  const int offset = 30;
  const IntegerVariable offset_load2 =
      model.Add(NewIntegerVariable(-offset, capacity - demand2 - offset));

  auto* repository = model.GetOrCreate<BinaryRelationRepository>();
  // Capacity constraint: (offset_load2 + offset) - load1 >= demand1
  repository->Add(literals[0], {offset_load2, 1}, {load1, -1}, demand1 - offset,
                  1000);
  repository->Build();
  std::unique_ptr<RouteRelationsHelper> route_relations_helper =
      RouteRelationsHelper::Create(num_nodes, tails, heads, literals,
                                   {AffineExpression(), AffineExpression(load1),
                                    AffineExpression(offset_load2, 1, offset)},
                                   *repository);
  ASSERT_NE(route_relations_helper, nullptr);

  MinOutgoingFlowHelper helper(num_nodes, tails, heads, literals, &model);
  const int min_flow =
      helper.ComputeDemandBasedMinOutgoingFlow({1, 2}, *route_relations_helper);

  // The total demand exceeds the capacity.
  EXPECT_EQ(min_flow, 2);
}

TEST(MinOutgoingFlowHelperTest, NodeMustBeInnerNode) {
  // when considering subset {1, 2, 3}, knowing that 2 cannot be reached
  // from outside can lead to better bound. The non zero-demands are in () on
  // the arcs.
  //
  // 0 --> 1 -(5)-> 2 -(5)-> 3 --> 0
  //       1 <-(3)- 2 -----------> 0
  //       1 -----(4)------> 3
  // 0 --------------------> 3
  for (const bool can_enter_at_2 : {true, false}) {
    Model model;
    const int num_nodes = 4;
    std::vector<int> tails = {0, 1, 2, 3, 2, 2, 1, 0};
    std::vector<int> heads = {1, 2, 3, 0, 0, 1, 3, 3};
    std::vector<int> demands = {0, 5, 5, 0, 0, 4, 4, 0};
    if (can_enter_at_2) {
      tails.push_back(0);
      heads.push_back(2);
      demands.push_back(0);
    }
    std::vector<Literal> literals;
    const int num_arcs = demands.size();
    for (int i = 0; i < num_arcs; ++i) {
      literals.push_back(Literal(model.Add(NewBooleanVariable()), true));
    }

    std::vector<IntegerVariable> loads;
    for (int i = 0; i < num_nodes; ++i) {
      loads.push_back(model.Add(NewIntegerVariable(0, 8)));
    }

    // Capacity constraints.
    auto* repository = model.GetOrCreate<BinaryRelationRepository>();
    for (int i = 0; i < num_arcs; ++i) {
      // loads[head] - loads[tail] >= demand[arc]
      repository->Add(literals[i], {loads[heads[i]], 1}, {loads[tails[i]], -1},
                      demands[i], 1000);
    }
    repository->Build();
    std::unique_ptr<RouteRelationsHelper> route_relations_helper =
        RouteRelationsHelper::Create(num_nodes, tails, heads, literals, {},
                                     *repository);
    ASSERT_NE(route_relations_helper, nullptr);

    MinOutgoingFlowHelper helper(num_nodes, tails, heads, literals, &model);
    const int min_flow = helper.ComputeDemandBasedMinOutgoingFlow(
        {1, 2, 3}, *route_relations_helper);

    // If we cannot enter at 2, the only possibility is 0->1->2->0 and 0->3->0.
    // Otherwise 0->2->1->3->0 is just under the capacity of 8.
    EXPECT_EQ(min_flow, can_enter_at_2 ? 1 : 2);
  }
}

TEST(MinOutgoingFlowHelperTest, BetterUseOfUpperBound) {
  // The non-zero demands are in () on the arcs.
  // when considering subset {1, 2}:
  //
  // 0 --> 1 -(8)-> 2 --> 0
  // 0 --> 2 -(8)-> 1 --> 0
  for (const bool bounds_forces_two_path : {true, false}) {
    Model model;
    std::vector<int> tails = {0, 1, 2, 0, 2, 1};
    std::vector<int> heads = {1, 2, 0, 2, 1, 0};
    std::vector<int> demands = {0, 8, 0, 0, 8, 0};
    std::vector<Literal> literals;
    const int num_arcs = demands.size();
    for (int i = 0; i < num_arcs; ++i) {
      literals.push_back(Literal(model.Add(NewBooleanVariable()), true));
    }

    std::vector<IntegerVariable> loads;
    loads.push_back(model.Add(NewIntegerVariable(0, 10)));  // depot.
    if (bounds_forces_two_path) {
      // Here if we exploit the bound properly, we can see that both possible
      // paths are invalid.
      loads.push_back(model.Add(NewIntegerVariable(0, 10)));
      loads.push_back(model.Add(NewIntegerVariable(5, 5)));
    } else {
      // Here the path 0->1->2->0 is fine.
      loads.push_back(model.Add(NewIntegerVariable(0, 10)));
      loads.push_back(model.Add(NewIntegerVariable(5, 10)));
    }

    // Capacity constraints.
    auto* repository = model.GetOrCreate<BinaryRelationRepository>();
    for (int i = 0; i < num_arcs; ++i) {
      // loads[head] - loads[tail] >= demand[arc]
      repository->Add(literals[i], {loads[heads[i]], 1}, {loads[tails[i]], -1},
                      demands[i], 1000);
    }
    repository->Build();
    std::unique_ptr<RouteRelationsHelper> route_relations_helper =
        RouteRelationsHelper::Create(loads.size(), tails, heads, literals, {},
                                     *repository);
    ASSERT_NE(route_relations_helper, nullptr);

    MinOutgoingFlowHelper helper(loads.size(), tails, heads, literals, &model);
    const int min_flow = helper.ComputeDemandBasedMinOutgoingFlow(
        {1, 2}, *route_relations_helper);

    EXPECT_EQ(min_flow, bounds_forces_two_path ? 2 : 1);
  }
}

TEST(MinOutgoingFlowHelperTest, DemandBasedMinOutgoingFlow_IsolatedNodes) {
  Model model;
  const int num_nodes = 5;
  // A star graph with num_nodes-1 nodes and a depot.
  std::vector<int> tails;
  std::vector<int> heads;
  std::vector<Literal> literals;
  std::vector<IntegerVariable> variables;
  auto* repository = model.GetOrCreate<BinaryRelationRepository>();
  // The depot variable.
  variables.push_back(model.Add(NewIntegerVariable(0, 100)));
  for (int head = 1; head < num_nodes; ++head) {
    tails.push_back(0);
    heads.push_back(head);
    literals.push_back(Literal(model.Add(NewBooleanVariable()), true));
    variables.push_back(model.Add(NewIntegerVariable(0, 100)));
    // Dummy relation, used only to associate a variable with each node.
    repository->Add(literals.back(), {variables[head], 1}, {variables[0], -1},
                    1, 100);
  }
  repository->Build();
  std::unique_ptr<RouteRelationsHelper> route_relations_helper =
      RouteRelationsHelper::Create(num_nodes, tails, heads, literals, {},
                                   *repository);
  ASSERT_NE(route_relations_helper, nullptr);
  // Subject under test.
  MinOutgoingFlowHelper helper(num_nodes, tails, heads, literals, &model);

  const int min_flow = helper.ComputeDemandBasedMinOutgoingFlow(
      {1, 2, 3, 4}, *route_relations_helper);

  EXPECT_EQ(min_flow, 4);
}

TEST(MinOutgoingFlowHelperTest, TimeWindows) {
  Model model;
  const int num_nodes = 5;
  // A complete graph with num_nodes.
  std::vector<int> tails;
  std::vector<int> heads;
  std::vector<Literal> literals;
  absl::flat_hash_map<std::pair<int, int>, Literal> literal_by_arc;
  for (int tail = 0; tail < num_nodes; ++tail) {
    for (int head = 0; head < num_nodes; ++head) {
      if (tail == head) continue;
      tails.push_back(tail);
      heads.push_back(head);
      literals.push_back(Literal(model.Add(NewBooleanVariable()), true));
      literal_by_arc[{tail, head}] = literals.back();
    }
  }
  // For each node, the time at which a vehicle leaves this node.
  std::vector<IntegerVariable> times;
  times.push_back(model.Add(NewIntegerVariable(0, 100)));  // Depot.
  times.push_back(model.Add(NewIntegerVariable(8, 12)));   // Node 1.
  times.push_back(model.Add(NewIntegerVariable(18, 22)));  // Node 2.
  times.push_back(model.Add(NewIntegerVariable(18, 22)));  // Node 3.
  times.push_back(model.Add(NewIntegerVariable(28, 32)));  // Node 4.
  // Travel time constraints.
  auto* repository = model.GetOrCreate<BinaryRelationRepository>();
  for (const auto& [arc, literal] : literal_by_arc) {
    const auto& [tail, head] = arc;
    const int travel_time = 10 - tail;
    // times[head] - times[tail] >= travel_time
    repository->Add(literal, {times[head], 1}, {times[tail], -1}, travel_time,
                    1000);
  }
  repository->Build();
  // Subject under test.
  MinOutgoingFlowHelper helper(num_nodes, tails, heads, literals, &model);

  const int min_flow = helper.ComputeMinOutgoingFlow({1, 2, 3, 4});
  const int tight_min_flow = helper.ComputeTightMinOutgoingFlow({1, 2, 3, 4});

  // Due to the time window constraints, a feasible path can have at most 3
  // nodes, hence at least two paths are needed. The earliest departure time
  // from each node n appearing at position i should be computed as follows:
  //
  //            1  2  3  4  (position)
  //          -------------
  //   node 1 | 8  -  -  -
  //        2 | 18 18 -  -
  //        3 | 18 18 -  -
  //        4 | 28 28 28 -
  EXPECT_EQ(min_flow, 2);
  EXPECT_EQ(tight_min_flow, 2);
}

std::vector<absl::flat_hash_map<int, AffineExpression>>
GetNodeExpressionsByDimension(const RouteRelationsHelper& helper) {
  std::vector<absl::flat_hash_map<int, AffineExpression>> result(
      helper.num_dimensions());
  for (int n = 0; n < helper.num_nodes(); ++n) {
    for (int d = 0; d < helper.num_dimensions(); ++d) {
      if (!helper.GetNodeExpression(n, d).IsConstant()) {
        result[d][n] = helper.GetNodeExpression(n, d);
      }
    }
  }
  return result;
}

int SolveTwoDimensionBinPacking(int capacity, absl::Span<const int> load1,
                                absl::Span<const int> load2) {
  // Lets generate a quick cp-sat model.
  const int num_items = load1.size();
  const int num_bins = num_items;

  CpModelBuilder cp_model;

  // x[i][b] == item i in bin b.
  std::vector<std::vector<BoolVar>> x(num_items);
  for (int i = 0; i < num_items; ++i) {
    x[i].resize(num_bins);
    for (int b = 0; b < num_bins; ++b) {
      x[i][b] = cp_model.NewBoolVar();
    }
  }

  // Place all items.
  for (int i = 0; i < num_items; ++i) {
    cp_model.AddExactlyOne(x[i]);
  }

  // Respect capacity.
  for (int b = 0; b < num_bins; ++b) {
    LinearExpr sum1;
    LinearExpr sum2;
    for (int i = 0; i < num_items; ++i) {
      sum1 += load1[i] * x[i][b];
      sum2 += load2[i] * x[i][b];
    }
    cp_model.AddLessOrEqual(sum1, capacity);
    cp_model.AddLessOrEqual(sum2, capacity);
  }

  // Bin used variables.
  std::vector<BoolVar> is_used(num_bins);
  for (int b = 0; b < num_bins; ++b) {
    is_used[b] = cp_model.NewBoolVar();
    for (int i = 0; i < num_items; ++i) {
      cp_model.AddImplication(x[i][b], is_used[b]);
    }
  }

  // Objective
  cp_model.Minimize(LinearExpr::Sum(is_used));

  // Solving part.
  const CpSolverResponse response = Solve(cp_model.Build());
  return static_cast<int>(response.objective_value());
}

// We test a simple example with 2 dimensions and 4 nodes with demands
// (7, 3) (3, 7) and (3, 1), (1, 3).
TEST(MinOutgoingFlowHelperTest, SubsetMightBeServedWithKRoutes) {
  Model model;
  const int num_nodes = 5;

  // A complete graph with num_nodes.
  std::vector<int> tails;
  std::vector<int> heads;
  std::vector<Literal> literals;
  absl::flat_hash_map<std::pair<int, int>, Literal> literal_by_arc;
  for (int tail = 0; tail < num_nodes; ++tail) {
    for (int head = 0; head < num_nodes; ++head) {
      if (tail == head) continue;
      tails.push_back(tail);
      heads.push_back(head);
      literals.push_back(Literal(model.Add(NewBooleanVariable()), true));
      literal_by_arc[{tail, head}] = literals.back();
    }
  }

  // Load of each node on both dimensions.
  std::vector<int> load1 = {0, 7, 3, 3, 1};
  std::vector<int> load2 = {0, 3, 7, 1, 3};

  // For each node, one cumul variable per dimension.
  std::vector<IntegerVariable> cumul_vars_1;
  std::vector<IntegerVariable> cumul_vars_2;
  const int64_t capacity(10);
  for (int n = 0; n < num_nodes; ++n) {
    cumul_vars_1.push_back(model.Add(NewIntegerVariable(load1[n], capacity)));
    cumul_vars_2.push_back(model.Add(NewIntegerVariable(load2[n], capacity)));
  }

  // Capacity constraints on two dimensions.
  auto* repository = model.GetOrCreate<BinaryRelationRepository>();
  for (const auto& [arc, literal] : literal_by_arc) {
    const auto& [tail, head] = arc;

    // vars[head] >= vars[tail] + load[head];
    repository->Add(literal, {cumul_vars_1[head], 1}, {cumul_vars_1[tail], -1},
                    load1[head], 10000);
    repository->Add(literal, {cumul_vars_2[head], 1}, {cumul_vars_2[tail], -1},
                    load2[head], 10000);
  }
  repository->Build();

  const int optimal = SolveTwoDimensionBinPacking(capacity, load1, load2);
  EXPECT_EQ(optimal, 2);

  // Subject under test.
  MinOutgoingFlowHelper helper(num_nodes, tails, heads, literals, &model);

  std::vector<int> subset = {1, 2, 3, 4};
  for (int k = 0; k < subset.size(); ++k) {
    if (k < optimal) {
      EXPECT_FALSE(helper.SubsetMightBeServedWithKRoutes(k, subset));
    } else {
      EXPECT_TRUE(helper.SubsetMightBeServedWithKRoutes(k, subset));
    }
  }
}

// Same as above but with randomization.
// I kept the "golden" test just to make sure things looks reasonable.
TEST(MinOutgoingFlowHelperTest, SubsetMightBeServedWithKRoutesRandom) {
  Model model;
  absl::BitGen random;
  const int num_nodes = 8;
  const int capacity = 20;

  // A complete graph with num_nodes.
  std::vector<int> tails;
  std::vector<int> heads;
  std::vector<Literal> literals;
  absl::flat_hash_map<std::pair<int, int>, Literal> literal_by_arc;
  for (int tail = 0; tail < num_nodes; ++tail) {
    for (int head = 0; head < num_nodes; ++head) {
      if (tail == head) continue;
      tails.push_back(tail);
      heads.push_back(head);
      literals.push_back(Literal(model.Add(NewBooleanVariable()), true));
      literal_by_arc[{tail, head}] = literals.back();
    }
  }

  // Load of each node on both dimensions.
  std::vector<int> load1(num_nodes, 0);
  std::vector<int> load2(num_nodes, 0);
  for (int n = 0; n < num_nodes; ++n) {
    load1[n] = absl::Uniform(random, 0, capacity);
    load2[n] = absl::Uniform(random, 0, capacity);
  }

  // For each node, one cumul variable per dimension.
  std::vector<IntegerVariable> cumul_vars_1;
  std::vector<IntegerVariable> cumul_vars_2;
  for (int n = 0; n < num_nodes; ++n) {
    cumul_vars_1.push_back(model.Add(NewIntegerVariable(load1[n], capacity)));
    cumul_vars_2.push_back(model.Add(NewIntegerVariable(load2[n], capacity)));
  }

  // Capacity constraints on two dimensions.
  auto* repository = model.GetOrCreate<BinaryRelationRepository>();
  for (const auto& [arc, literal] : literal_by_arc) {
    const auto& [tail, head] = arc;

    // vars[head] >= vars[tail] + load[head];
    repository->Add(literal, {cumul_vars_1[head], 1}, {cumul_vars_1[tail], -1},
                    load1[head], 10000);
    repository->Add(literal, {cumul_vars_2[head], 1}, {cumul_vars_2[tail], -1},
                    load2[head], 10000);
  }
  repository->Build();

  // To check our indices mapping, lets remove a random nodes from the subset
  std::vector<int> subset;
  for (int i = 0; i < num_nodes; ++i) subset.push_back(i);
  const int to_remove = absl::Uniform(random, 0, num_nodes);
  std::swap(subset[to_remove], subset.back());
  subset.pop_back();

  // We set the load to zero to have the proper optimal.
  load1[to_remove] = 0;
  load2[to_remove] = 0;
  const int optimal = SolveTwoDimensionBinPacking(capacity, load1, load2);
  LOG(INFO) << "random problem optimal = " << optimal;

  // Subject under test.
  MinOutgoingFlowHelper helper(num_nodes, tails, heads, literals, &model);

  for (int k = 0; k < subset.size(); ++k) {
    if (k < optimal) {
      EXPECT_FALSE(helper.SubsetMightBeServedWithKRoutes(k, subset));
    } else {
      EXPECT_TRUE(helper.SubsetMightBeServedWithKRoutes(k, subset));
    }
  }
}

int SolveSpecialBinPackingWithCpSat(absl::Span<const ItemOrBin> objects) {
  CpModelBuilder cp_model;

  const int n = objects.size();
  std::vector<BoolVar> item_is_bin(n);
  for (int i = 0; i < n; ++i) {
    if (objects[i].type == ItemOrBin::MUST_BE_BIN) {
      item_is_bin[i] = cp_model.TrueVar();
    } else if (objects[i].type == ItemOrBin::MUST_BE_ITEM) {
      item_is_bin[i] = cp_model.FalseVar();
    } else {
      item_is_bin[i] = cp_model.NewBoolVar();
    }
  }

  // x[i][b] == item i in bin b.
  std::vector<std::vector<BoolVar>> x(n);
  for (int i = 0; i < n; ++i) {
    x[i].resize(n);
    for (int b = 0; b < n; ++b) {
      if (i == b) {
        // We always place a bin into itself in this model.
        x[i][b] = item_is_bin[b];
      } else {
        x[i][b] = cp_model.NewBoolVar();
        cp_model.AddImplication(x[i][b], item_is_bin[b]);
      }
    }
  }

  // Place all items.
  for (int i = 0; i < n; ++i) {
    cp_model.AddExactlyOne(x[i]);
  }

  // Respect capacity.
  for (int b = 0; b < n; ++b) {
    LinearExpr demands;
    for (int i = 0; i < n; ++i) {
      if (i == b) continue;
      demands += objects[i].demand.value() * x[i][b];
    }
    // We shift by the bin demand since we always have x[b][b] at true if the
    // bin is used as such.
    cp_model.AddLessOrEqual(demands, objects[b].capacity.value())
        .OnlyEnforceIf(item_is_bin[b]);
  }

  // Objective
  cp_model.Minimize(LinearExpr::Sum(item_is_bin));

  // Solving part.
  SatParameters params;
  params.set_log_search_progress(false);
  const CpSolverResponse response =
      SolveWithParameters(cp_model.Build(), params);

  // This is the convention used in our bound computation function.
  if (response.status() == INFEASIBLE) return n + 1;
  return static_cast<int>(response.objective_value());
}

// Generate a random problem and make sure our bound is always valid.
// These problems are a bit easy, but with --runs_per_test 1000 there are a few
// instances where our lower bound is strictly worse than the true optimal.
TEST(SpecialBinPackingProblemTest, ComputeMinNumberOfBins) {
  Model model;
  absl::BitGen random;
  const int num_objects = 20;

  std::vector<ItemOrBin> objects;
  for (int i = 0; i < num_objects; ++i) {
    ItemOrBin o;
    o.capacity = absl::Uniform(random, 0, 100);
    o.demand = absl::Uniform(random, 0, 50);
    const int type = absl::Uniform(random, 0, 2);
    if (type == 0) o.type = ItemOrBin::MUST_BE_ITEM;
    if (type == 1) o.type = ItemOrBin::ITEM_OR_BIN;
    if (type == 2) o.type = ItemOrBin::MUST_BE_BIN;
    objects.push_back(o);
  }

  bool gcd_was_used;
  const int obj_lb =
      ComputeMinNumberOfBins(absl::MakeSpan(objects), &gcd_was_used);
  const int optimal = SolveSpecialBinPackingWithCpSat(objects);
  EXPECT_LE(obj_lb, optimal);
  if (obj_lb != optimal) {
    LOG(INFO) << "bound " << obj_lb << " optimal " << optimal;
  }
}

std::vector<absl::flat_hash_map<int, Relation>> GetRelationByDimensionAndArc(
    const RouteRelationsHelper& helper) {
  std::vector<absl::flat_hash_map<int, Relation>> result(
      helper.num_dimensions());
  for (int i = 0; i < helper.num_arcs(); ++i) {
    for (int d = 0; d < helper.num_dimensions(); ++d) {
      if (!helper.GetArcRelation(i, d).empty()) {
        result[d][i] = helper.GetArcRelation(i, d);
      }
    }
  }
  return result;
}

TEST(RouteRelationsHelperTest, Basic) {
  Model model;
  // A graph with 6 nodes and the following arcs:
  //
  // l0 --->0<--- l1
  //    |       |
  //    1--l2-->2--l3-->3     4--l4-->5
  //
  const int num_nodes = 6;
  const std::vector<int> tails = {1, 2, 1, 2, 4};
  const std::vector<int> heads = {0, 0, 2, 3, 5};
  const std::vector<Literal> literals = {
      Literal(model.Add(NewBooleanVariable()), true),
      Literal(model.Add(NewBooleanVariable()), true),
      Literal(model.Add(NewBooleanVariable()), true),
      Literal(model.Add(NewBooleanVariable()), true),
      Literal(model.Add(NewBooleanVariable()), true)};
  // Add relations with "time" variables A, B, C intended to be associated with
  // nodes 0, 1, 2 respectively, and "load" variables U, V, W, X, Y, Z intended
  // to be associated with nodes 0, 1, 2, 3, 4, 5 respectively.
  const IntegerVariable a = model.Add(NewIntegerVariable(0, 100));
  const IntegerVariable b = model.Add(NewIntegerVariable(0, 100));
  const IntegerVariable c = model.Add(NewIntegerVariable(0, 100));
  const IntegerVariable u = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable v = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable w = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable x = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable y = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable z = model.Add(NewIntegerVariable(0, 10));
  BinaryRelationRepository repository;
  repository.Add(literals[0], {a, 1}, {b, -1}, 50, 1000);
  repository.Add(literals[1], {a, 1}, {c, -1}, 70, 1000);
  repository.Add(literals[2], {c, 1}, {b, -1}, 40, 1000);
  repository.Add(literals[0], {NegationOf(u), -1}, {NegationOf(v), 1}, 4, 100);
  repository.Add(literals[1], {u, 1}, {w, -1}, 4, 100);
  repository.Add(literals[2], {w, -1}, {v, 1}, -100, -3);
  repository.Add(literals[3], {x, 1}, {w, -1}, 5, 100);
  repository.Add(literals[4], {z, 1}, {y, -1}, 7, 100);
  repository.Build();

  std::unique_ptr<RouteRelationsHelper> helper = RouteRelationsHelper::Create(
      num_nodes, tails, heads, literals, {}, repository);

  ASSERT_NE(helper, nullptr);
  // Two dimensions (time and load) on the first connected component, and one
  // dimension (load) on the second component.
  EXPECT_EQ(helper->num_dimensions(), 3);
  EXPECT_EQ(helper->num_nodes(), num_nodes);
  EXPECT_EQ(helper->num_arcs(), 5);
  // Check the node variables.
  EXPECT_THAT(
      GetNodeExpressionsByDimension(*helper),
      UnorderedElementsAre(
          UnorderedElementsAre(Pair(0, a), Pair(1, b), Pair(2, c)),
          UnorderedElementsAre(Pair(0, u), Pair(1, v), Pair(2, w), Pair(3, x)),
          // Variables y and z cannot be unambiguously associated with nodes.
          IsEmpty()));
  // Check the arc relations.
  EXPECT_THAT(GetRelationByDimensionAndArc(*helper),
              UnorderedElementsAre(
                  UnorderedElementsAre(Pair(0, Relation{-1, 1, 50, 1000}),
                                       Pair(1, Relation{-1, 1, 70, 1000}),
                                       Pair(2, Relation{-1, 1, 40, 1000})),
                  UnorderedElementsAre(Pair(0, Relation{-1, 1, 4, 100}),
                                       Pair(1, Relation{-1, 1, 4, 100}),
                                       Pair(2, Relation{-1, 1, 3, 100}),
                                       Pair(3, Relation{-1, 1, 5, 100})),
                  // The relation for the arc 4->5 is not recovered since its
                  // variables cannot be unambiguously associated with nodes.
                  IsEmpty()));

  helper->RemoveArcs({0, 2});

  EXPECT_EQ(helper->num_nodes(), num_nodes);
  EXPECT_EQ(helper->num_arcs(), 3);
  EXPECT_THAT(GetRelationByDimensionAndArc(*helper),
              UnorderedElementsAre(
                  UnorderedElementsAre(Pair(0, Relation{-1, 1, 70, 1000})),
                  UnorderedElementsAre(Pair(0, Relation{-1, 1, 4, 100}),
                                       Pair(1, Relation{-1, 1, 5, 100})),
                  IsEmpty()));
}

TEST(RouteRelationsHelperTest, UnenforcedRelations) {
  Model model;
  // Graph:  0--l0-->1
  //         ^\      |
  //      l3 | \_l4_ | l1
  //         |      \v
  //         3<--l2--2
  //
  const int num_nodes = 4;
  const std::vector<int> tails = {0, 1, 2, 3, 0};
  const std::vector<int> heads = {1, 2, 3, 0, 2};
  const std::vector<Literal> literals = {
      Literal(model.Add(NewBooleanVariable()), true),
      Literal(model.Add(NewBooleanVariable()), true),
      Literal(model.Add(NewBooleanVariable()), true),
      Literal(model.Add(NewBooleanVariable()), true),
      Literal(model.Add(NewBooleanVariable()), true)};
  // Add relations with "time" variables A, B, C, D intended to be associated
  // with nodes 0, 1, 2, 3 respectively.
  const IntegerVariable a = model.Add(NewIntegerVariable(0, 100));
  const IntegerVariable b = model.Add(NewIntegerVariable(0, 100));
  const IntegerVariable c = model.Add(NewIntegerVariable(0, 100));
  const IntegerVariable d = model.Add(NewIntegerVariable(0, 100));
  BinaryRelationRepository repository;
  repository.Add(literals[0], {b, 1}, {a, -1}, 1, 1);
  repository.Add(literals[1], {c, 1}, {b, -1}, 2, 2);
  repository.Add(literals[2], {d, 1}, {c, -1}, 3, 3);
  repository.Add(literals[3], {a, 1}, {d, -1}, 4, 4);
  // Several unenforced relations on the diagonal arc. The one with the +/-1
  // coefficients should be preferred.
  repository.Add(Literal(kNoLiteralIndex), {c, 3}, {a, -2}, 1, 9);
  repository.Add(Literal(kNoLiteralIndex), {c, 1}, {a, -1}, 5, 5);
  repository.Add(Literal(kNoLiteralIndex), {c, 2}, {a, -3}, 3, 8);
  repository.Build();

  std::unique_ptr<RouteRelationsHelper> helper = RouteRelationsHelper::Create(
      num_nodes, tails, heads, literals, {}, repository);

  ASSERT_NE(helper, nullptr);
  EXPECT_THAT(GetNodeExpressionsByDimension(*helper),
              UnorderedElementsAre(UnorderedElementsAre(
                  Pair(0, a), Pair(1, b), Pair(2, c), Pair(3, d))));
  // The unenforced relation is taken into account.
  EXPECT_THAT(
      GetRelationByDimensionAndArc(*helper),
      UnorderedElementsAre(UnorderedElementsAre(
          Pair(0, Relation{-1, 1, 1, 1}), Pair(1, Relation{-1, 1, 2, 2}),
          Pair(2, Relation{-1, 1, 3, 3}), Pair(3, Relation{-1, 1, 4, 4}),
          Pair(4, Relation{-1, 1, 5, 5}))));
}

TEST(RouteRelationsHelperTest, SeveralVariablesPerNode) {
  Model model;
  // A graph with 3 nodes and the following arcs: 0--l0-->1--l2-->2
  const int num_nodes = 3;
  const std::vector<int> tails = {0, 1};
  const std::vector<int> heads = {1, 2};
  const std::vector<Literal> literals = {
      Literal(model.Add(NewBooleanVariable()), true),
      Literal(model.Add(NewBooleanVariable()), true)};
  // Add relations with "time" variables A, B, C and "load" variables X, Y, Z,
  // intended to be associated with nodes 0, 1, 2 respectively.
  const IntegerVariable a = model.Add(NewIntegerVariable(0, 100));
  const IntegerVariable b = model.Add(NewIntegerVariable(0, 100));
  const IntegerVariable c = model.Add(NewIntegerVariable(0, 100));
  const IntegerVariable x = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable y = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable z = model.Add(NewIntegerVariable(0, 10));
  BinaryRelationRepository repository;
  repository.Add(literals[0], {b, 1}, {a, -1}, 50, 1000);
  repository.Add(literals[1], {c, 1}, {b, -1}, 70, 1000);
  repository.Add(literals[0], {z, 1}, {y, -1}, 5, 100);
  repository.Add(literals[1], {y, 1}, {x, -1}, 7, 100);
  // Weird relation linking time and load variables, causing all the variables
  // to be in a single "dimension".
  repository.Add(literals[0], {x, 1}, {a, -1}, 0, 100);
  repository.Build();

  std::unique_ptr<RouteRelationsHelper> helper = RouteRelationsHelper::Create(
      num_nodes, tails, heads, literals, {}, repository);

  EXPECT_EQ(helper, nullptr);
}

TEST(RouteRelationsHelperTest, SeveralRelationsPerArc) {
  Model model;
  // A graph with 3 nodes and the following arcs: 0--l0-->1--l1-->2
  const int num_nodes = 3;
  const std::vector<int> tails = {0, 1};
  const std::vector<int> heads = {1, 2};
  const std::vector<Literal> literals = {
      Literal(model.Add(NewBooleanVariable()), true),
      Literal(model.Add(NewBooleanVariable()), true)};
  // Add relations with "time" variables A, B, C intended to be associated with
  // nodes 0, 1, 2 respectively.
  const IntegerVariable a = model.Add(NewIntegerVariable(0, 100));
  const IntegerVariable b = model.Add(NewIntegerVariable(0, 100));
  const IntegerVariable c = model.Add(NewIntegerVariable(0, 100));
  BinaryRelationRepository repository;
  repository.Add(literals[0], {b, 1}, {a, -1}, 50, 1000);
  repository.Add(literals[1], {c, 1}, {b, -1}, 70, 1000);
  // Add a second relation for some arc.
  repository.Add(literals[1], {c, 2}, {b, -3}, 100, 200);
  repository.Build();

  using Relation = RouteRelationsHelper::Relation;
  std::unique_ptr<RouteRelationsHelper> helper = RouteRelationsHelper::Create(
      num_nodes, tails, heads, literals, {}, repository);

  ASSERT_NE(helper, nullptr);
  EXPECT_EQ(helper->num_dimensions(), 1);
  EXPECT_EQ(helper->GetNodeExpression(0, 0), a);
  EXPECT_EQ(helper->GetNodeExpression(1, 0), b);
  EXPECT_EQ(helper->GetNodeExpression(2, 0), c);
  EXPECT_EQ(helper->GetArcRelation(0, 0), (Relation{-1, 1, 50, 1000}));
  EXPECT_THAT(
      helper->GetArcRelation(1, 0),
      AnyOf(Eq(Relation{-1, 1, 70, 1000}), Eq(Relation{-3, 2, 100, 200})));
}

TEST(RouteRelationsHelperTest, SeveralArcsPerLiteral) {
  // A graph with 3 nodes and the following arcs: 0--l0-->1--l0-->2, both
  // enforced by the same literal l0.
  Model model;
  const int num_nodes = 3;
  const std::vector<int> tails = {0, 1};
  const std::vector<int> heads = {1, 2};
  const Literal literal(model.Add(NewBooleanVariable()), true);
  const std::vector<Literal> literals = {literal, literal};
  // Add relations with "time" variables A, B, C intended to be associated with
  // nodes 0, 1, 2 respectively.
  const IntegerVariable a = model.Add(NewIntegerVariable(0, 100));
  const IntegerVariable b = model.Add(NewIntegerVariable(0, 100));
  const IntegerVariable c = model.Add(NewIntegerVariable(0, 100));
  BinaryRelationRepository repository;
  repository.Add(literals[0], {b, 1}, {a, -1}, 50, 1000);
  repository.Add(literals[0], {c, 1}, {b, -1}, 40, 1000);
  repository.Build();

  std::unique_ptr<RouteRelationsHelper> helper = RouteRelationsHelper::Create(
      num_nodes, tails, heads, literals, {}, repository);

  // No variable should be associated with any node, since there is no unique
  // way to do this ([A, B, C] or [C, B, A], for nodes [0, 1, 2] respectively).
  // As a consequence, no relation should be recovered either.
  EXPECT_EQ(helper, nullptr);
}

TEST(RouteRelationsHelperTest, InconsistentRelationIsSkipped) {
  // Graph:   0--l0-->1--l1-->2--l3-->3--l4-->4
  //                  |               ^
  //                  |               |
  //               l3 ------->5-------- l5
  //
  Model model;
  const int num_nodes = 6;
  const std::vector<int> tails = {0, 1, 2, 3, 1, 5};
  const std::vector<int> heads = {1, 2, 3, 4, 5, 3};
  const std::vector<Literal> literals = {
      Literal(model.Add(NewBooleanVariable()), true),
      Literal(model.Add(NewBooleanVariable()), true),
      Literal(model.Add(NewBooleanVariable()), true),
      Literal(model.Add(NewBooleanVariable()), true),
      Literal(model.Add(NewBooleanVariable()), true),
      Literal(model.Add(NewBooleanVariable()), true)};
  // Variables a, b, c, d, e, f are supposed to be associated with nodes 0, 1,
  // 2, 3, 4, 5 respectively.
  const IntegerVariable a = model.Add(NewIntegerVariable(0, 100));
  const IntegerVariable b = model.Add(NewIntegerVariable(0, 100));
  const IntegerVariable c = model.Add(NewIntegerVariable(0, 100));
  const IntegerVariable d = model.Add(NewIntegerVariable(0, 100));
  const IntegerVariable e = model.Add(NewIntegerVariable(0, 100));
  const IntegerVariable f = model.Add(NewIntegerVariable(0, 100));
  BinaryRelationRepository repository;
  repository.Add(literals[0], {b, 1}, {a, -1}, 0, 0);
  repository.Add(literals[1], {c, 1}, {b, -1}, 1, 1);
  repository.Add(literals[2], {d, 1}, {c, -1}, 2, 2);
  repository.Add(literals[3], {e, 1}, {d, -1}, 3, 3);
  repository.Add(literals[4], {f, 1}, {b, -1}, 4, 4);
  // Inconsistent relation for arc 5->3 (should be between f and d).
  repository.Add(literals[5], {f, 2}, {b, -1}, 5, 5);
  repository.Build();

  std::unique_ptr<RouteRelationsHelper> helper = RouteRelationsHelper::Create(
      num_nodes, tails, heads, literals, {}, repository);

  ASSERT_NE(helper, nullptr);
  EXPECT_THAT(GetNodeExpressionsByDimension(*helper),
              UnorderedElementsAre(
                  UnorderedElementsAre(Pair(0, a), Pair(1, b), Pair(2, c),
                                       Pair(3, d), Pair(4, e), Pair(5, f))));
  // The relation for arc 5->3 is filtered out because it is inconsistent.
  EXPECT_THAT(
      GetRelationByDimensionAndArc(*helper),
      UnorderedElementsAre(UnorderedElementsAre(
          Pair(0, Relation{-1, 1, 0, 0}), Pair(1, Relation{-1, 1, 1, 1}),
          Pair(2, Relation{-1, 1, 2, 2}), Pair(3, Relation{-1, 1, 3, 3}),
          Pair(4, Relation{-1, 1, 4, 4}))));
}

TEST(RouteRelationsHelperTest, InconsistentRelationWithMultipleArcsPerLiteral) {
  // Graph:  0--l0-->1<---
  //         ^       |   |
  //        l3      l1   |
  //         |       v   l4
  //         3<--l2--2   |
  //         |           |
  //         ----l4----->4
  Model model;
  const int num_nodes = 5;
  const std::vector<int> tails = {0, 1, 2, 3, 4, 3};
  const std::vector<int> heads = {1, 2, 3, 0, 1, 4};
  const Literal l4 = Literal(model.Add(NewBooleanVariable()), true);
  const std::vector<Literal> literals = {
      Literal(model.Add(NewBooleanVariable()), true),
      Literal(model.Add(NewBooleanVariable()), true),
      Literal(model.Add(NewBooleanVariable()), true),
      Literal(model.Add(NewBooleanVariable()), true),
      l4,
      l4};
  // Variables a, b, c, d, e are supposed to be associated with nodes 0, 1, 2,
  // 3, 4 respectively.
  const IntegerVariable a = model.Add(NewIntegerVariable(0, 100));
  const IntegerVariable b = model.Add(NewIntegerVariable(0, 100));
  const IntegerVariable c = model.Add(NewIntegerVariable(0, 100));
  const IntegerVariable d = model.Add(NewIntegerVariable(0, 100));
  const IntegerVariable e = model.Add(NewIntegerVariable(0, 100));
  BinaryRelationRepository repository;
  repository.Add(literals[0], {b, 1}, {a, -1}, 0, 0);
  repository.Add(literals[1], {c, 1}, {b, -1}, 1, 1);
  repository.Add(literals[2], {d, 1}, {c, -1}, 2, 2);
  repository.Add(literals[3], {a, 1}, {d, -1}, 3, 3);
  // Inconsistent relation for arc 4->1 (should be between e and b). Note that
  // arcs 4->1 and 4->3 are enforced by the same literal.
  repository.Add(literals[4], {e, 1}, {d, -1}, 4, 4);
  repository.Add(literals[5], {e, 1}, {d, -1}, 5, 5);
  repository.Build();

  std::unique_ptr<RouteRelationsHelper> helper = RouteRelationsHelper::Create(
      num_nodes, tails, heads, literals, {}, repository);

  ASSERT_NE(helper, nullptr);
  EXPECT_THAT(GetNodeExpressionsByDimension(*helper),
              UnorderedElementsAre(UnorderedElementsAre(
                  Pair(0, a), Pair(1, b), Pair(2, c), Pair(3, d), Pair(4, e))));
  // The relation for arc 4->1 is filtered out because it is inconsistent.
  EXPECT_THAT(
      GetRelationByDimensionAndArc(*helper),
      UnorderedElementsAre(UnorderedElementsAre(
          Pair(0, Relation{-1, 1, 0, 0}), Pair(1, Relation{-1, 1, 1, 1}),
          Pair(2, Relation{-1, 1, 2, 2}), Pair(3, Relation{-1, 1, 3, 3}),
          Pair(5, Relation{-1, 1, 5, 5}))));
}

TEST(MaybeFillMissingRoutesConstraintNodeExpressions,
     FillsNodeVariablesIfNotPresent) {
  // A graph with 4 nodes and the following arcs, with relations implying that
  // variables 4, 5, 6, 7 should be associated with nodes 0, 1, 2, 3
  // respectively.
  //
  // l0 --->0<--- l1
  //    |       |
  //    1--l2-->2--l3-->3
  //
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    constraints {
      routes {
        tails: [ 1, 2, 1, 2 ]
        heads: [ 0, 0, 2, 3 ]
        literals: [ 0, 1, 2, 3 ]
      }
    }
    constraints {
      enforcement_literal: 0
      linear {
        vars: [ 4, 5 ]
        coeffs: [ 1, -1 ]
        domain: [ 0, 10 ]
      }
    }
    constraints {
      enforcement_literal: 1
      linear {
        vars: [ 4, 6 ]
        coeffs: [ 1, -1 ]
        domain: [ 0, 10 ]
      }
    }
    constraints {
      enforcement_literal: 2
      linear {
        vars: [ 5, 6 ]
        coeffs: [ 1, -1 ]
        domain: [ 0, 10 ]
      }
    }
    constraints {
      enforcement_literal: 3
      linear {
        vars: [ 6, 7 ]
        coeffs: [ 1, -1 ]
        domain: [ 0, 10 ]
      }
    }
  )pb");
  CpModelProto new_cp_model = initial_model;
  const auto [num_routes, num_dimensions] =
      MaybeFillMissingRoutesConstraintNodeExpressions(initial_model,
                                                      new_cp_model);

  EXPECT_EQ(num_routes, 1);
  EXPECT_EQ(num_dimensions, 1);
  const ConstraintProto expected_constraint = ParseTestProto(R"pb(
                routes {
                  tails: [ 1, 2, 1, 2 ]
                  heads: [ 0, 0, 2, 3 ]
                  literals: [ 0, 1, 2, 3 ]
                  dimensions {
                    exprs {
                      vars: [ 4 ]
                      coeffs: [ 1 ]
                    }
                    exprs {
                      vars: [ 5 ]
                      coeffs: [ 1 ]
                    }
                    exprs {
                      vars: [ 6 ]
                      coeffs: [ 1 ]
                    }
                    exprs {
                      vars: [ 7 ]
                      coeffs: [ 1 ]
                    }
                  }
                }
              )pb");
  EXPECT_THAT(new_cp_model.constraints(0), EqualsProto(expected_constraint));
}

TEST(MaybeFillMissingRoutesConstraintNodeExpressions,
     KeepsNodeVariablesIfPresent) {
  // A graph with 4 nodes and the following arcs, with relations implying that
  // variables 4, 5, 6, 7 should be associated with nodes 0, 1, 2, 3
  // respectively (but the user provided 7, 6, 5, 4 instead, respectively).
  //
  // l0 --->0<--- l1
  //    |       |
  //    1--l2-->2--l3-->3
  //
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    constraints {
      routes {
        tails: [ 1, 2, 1, 2 ]
        heads: [ 0, 0, 2, 3 ]
        literals: [ 0, 1, 2, 3 ]
        dimensions {
          exprs {
            vars: [ 7 ]
            coeffs: [ 1 ]
          }
          exprs {
            vars: [ 6 ]
            coeffs: [ 1 ]
          }
          exprs {
            vars: [ 5 ]
            coeffs: [ 1 ]
          }
          exprs {
            vars: [ 4 ]
            coeffs: [ 1 ]
          }
        }
      }
    }
    constraints {
      enforcement_literal: 0
      linear {
        vars: [ 4, 5 ]
        coeffs: [ 1, -1 ]
        domain: [ 0, 10 ]
      }
    }
    constraints {
      enforcement_literal: 1
      linear {
        vars: [ 4, 6 ]
        coeffs: [ 1, -1 ]
        domain: [ 0, 10 ]
      }
    }
    constraints {
      enforcement_literal: 2
      linear {
        vars: [ 5, 6 ]
        coeffs: [ 1, -1 ]
        domain: [ 0, 10 ]
      }
    }
    constraints {
      enforcement_literal: 3
      linear {
        vars: [ 6, 7 ]
        coeffs: [ 1, -1 ]
        domain: [ 0, 10 ]
      }
    }
  )pb");
  CpModelProto new_cp_model = initial_model;
  const auto [num_routes, num_dimensions] =
      MaybeFillMissingRoutesConstraintNodeExpressions(initial_model,
                                                      new_cp_model);

  EXPECT_EQ(num_routes, 0);
  EXPECT_EQ(num_dimensions, 0);
  EXPECT_THAT(new_cp_model, EqualsProto(initial_model));
}

TEST(ExtractAllSubsetsFromForestTest, Basic) {
  std::vector<int> parents = {3, 3, 1, 3, 1, 3};

  std::vector<int> buffer;
  std::vector<absl::Span<const int>> subsets;
  ExtractAllSubsetsFromForest(parents, &buffer, &subsets);

  // Post order but we explore high number first.
  // Alternatively, we could use unordered here, but the order is stable.
  EXPECT_THAT(buffer, ElementsAre(5, 4, 2, 1, 0, 3));
  EXPECT_THAT(subsets,
              ElementsAre(ElementsAre(5), ElementsAre(4), ElementsAre(2),
                          ElementsAre(4, 2, 1), ElementsAre(0),
                          ElementsAre(5, 4, 2, 1, 0, 3)));
}

//
//   0     3   4
//  / \        |
// 1   2       5
TEST(ExtractAllSubsetsFromForestTest, BasicForest) {
  std::vector<int> parents = {0, 0, 0, 3, 4, 4};

  std::vector<int> buffer;
  std::vector<absl::Span<const int>> subsets;
  ExtractAllSubsetsFromForest(parents, &buffer, &subsets);

  // Post order but we explore high number first.
  // Alternatively, we could use unordered here, but the order is stable.
  EXPECT_THAT(buffer, ElementsAre(2, 1, 0, 3, 5, 4));
  EXPECT_THAT(subsets,
              ElementsAre(ElementsAre(2), ElementsAre(1), ElementsAre(2, 1, 0),
                          ElementsAre(3), ElementsAre(5), ElementsAre(5, 4)));
}

TEST(ExtractAllSubsetsFromForestTest, Random) {
  const int num_nodes = 20;
  absl::BitGen random;

  // Create a random tree rooted at zero.
  std::vector<int> parents(num_nodes, 0);
  for (int i = 2; i < num_nodes; ++i) {
    parents[i] = absl::Uniform<int>(random, 0, i);  // in [0, i - 1].
  }

  std::vector<int> buffer;
  std::vector<absl::Span<const int>> subsets;
  ExtractAllSubsetsFromForest(parents, &buffer, &subsets);

  // We don't test that we are exhaustive, but we check basic property.
  std::vector<int> in_subset(num_nodes, false);
  for (const auto subset : subsets) {
    for (const int n : subset) in_subset[n] = true;

    // There should be at most one out edge.
    int root = -1;
    for (const int n : subset) {
      if (in_subset[parents[n]]) continue;
      if (root != -1) EXPECT_EQ(parents[n], root);
      root = parents[n];
    }

    // No node outside should point inside.
    for (int n = 0; n < num_nodes; ++n) {
      if (in_subset[n]) continue;
      EXPECT_TRUE(!in_subset[parents[n]]);
    }

    for (const int n : subset) in_subset[n] = false;
  }
}

TEST(SymmetrizeArcsTest, BasicTest) {
  std::vector<ArcWithLpValue> arcs{{.tail = 0, .head = 1, .lp_value = 0.5},
                                   {.tail = 2, .head = 0, .lp_value = 0.5},
                                   {.tail = 1, .head = 0, .lp_value = 0.5}};
  SymmetrizeArcs(&arcs);
  EXPECT_THAT(
      arcs, ElementsAre(ArcWithLpValue{.tail = 0, .head = 1, .lp_value = 1.0},
                        ArcWithLpValue{.tail = 0, .head = 2, .lp_value = 0.5}));
}

TEST(ComputeGomoryHuTreeTest, Random) {
  absl::BitGen random;

  // Lets generate a random graph on a small number of nodes.
  const int num_nodes = 10;
  const int num_arcs = 100;
  std::vector<ArcWithLpValue> arcs;
  for (int i = 0; i < num_arcs; ++i) {
    const int tail = absl::Uniform<int>(random, 0, num_nodes);
    const int head = absl::Uniform<int>(random, 0, num_nodes);
    if (tail == head) continue;
    const double lp_value = absl::Uniform<double>(random, 0, 1);
    arcs.push_back({tail, head, lp_value});
  }

  // Get all cut from Gomory-Hu tree.
  const std::vector<int> parents = ComputeGomoryHuTree(num_nodes, arcs);
  std::vector<int> buffer;
  std::vector<absl::Span<const int>> subsets;
  ExtractAllSubsetsFromForest(parents, &buffer, &subsets);

  // Compute the cost of entering (resp. leaving) each subset.
  // TODO(user): We need the same scaling as in ComputeGomoryHu(), not super
  // clean. We might want an integer input to the function, but ok for now.
  std::vector<bool> in_subset(num_nodes, false);
  std::vector<int64_t> out_costs(subsets.size(), 0);
  std::vector<int64_t> in_costs(subsets.size(), 0);
  for (int i = 0; i < subsets.size(); ++i) {
    for (const int n : subsets[i]) in_subset[n] = true;
    for (const auto& arc : arcs) {
      if (in_subset[arc.tail] && !in_subset[arc.head]) {
        out_costs[i] += std::round(1.0e6 * arc.lp_value);
      }
      if (!in_subset[arc.tail] && in_subset[arc.head]) {
        in_costs[i] += std::round(1.0e6 * arc.lp_value);
      }
    }
    for (const int n : subsets[i]) in_subset[n] = false;
  }

  // We will test with an exhaustive comparison. We are in n ^ 3 !
  // For all (s,t) pair, get the actual max-flow on the scaled graph.
  // Check than one of the cuts separate s and t, with this exact weight.
  SimpleMaxFlow max_flow;
  for (const auto& [tail, head, lp_value] : arcs) {
    // TODO(user): the algo only seems to work on an undirected graph, or
    // equivalently when we always have a reverse arc with the same weight.
    // Note that you can see below that we compute "min" cut for the sum of
    // outgoing + incoming arcs this way.
    max_flow.AddArcWithCapacity(tail, head, std::round(1.0e6 * lp_value));
    max_flow.AddArcWithCapacity(head, tail, std::round(1.0e6 * lp_value));
  }
  for (int s = 0; s < num_nodes; ++s) {
    for (int t = s + 1; t < num_nodes; ++t) {
      ASSERT_EQ(max_flow.Solve(s, t), SimpleMaxFlow::OPTIMAL);
      const int64_t flow = max_flow.OptimalFlow();
      bool found = false;
      for (int i = 0; i < subsets.size(); ++i) {
        bool s_out = true;
        bool t_out = true;
        for (const int n : subsets[i]) {
          if (n == s) s_out = false;
          if (n == t) t_out = false;
        }
        if (!s_out && t_out && out_costs[i] + in_costs[i] == flow) found = true;
        if (s_out && !t_out && in_costs[i] + out_costs[i] == flow) found = true;
        if (found) break;
      }

      // Debug.
      if (!found) {
        LOG(INFO) << s << " -> " << t << " flow= " << flow;
        for (int i = 0; i < subsets.size(); ++i) {
          bool s_out = true;
          bool t_out = true;
          for (const int n : subsets[i]) {
            if (n == s) s_out = false;
            if (n == t) t_out = false;
          }
          if (!s_out && t_out) {
            LOG(INFO) << i << " out= " << out_costs[i] + in_costs[i];
          }
          if (s_out && !t_out) {
            LOG(INFO) << i << " in= " << in_costs[i] + out_costs[i];
          }
        }
      }
      ASSERT_TRUE(found);
    }
  }
}

TEST(CreateStronglyConnectedGraphCutGeneratorTest, BasicExample) {
  Model model;

  // Lets create a simple square graph with arcs in both directions:
  //
  // 0 ---- 1
  // |      |
  // |      |
  // 2 ---- 3
  const int num_nodes = 4;
  std::vector<int> tails{0, 1, 1, 3, 3, 2, 2, 0};
  std::vector<int> heads{1, 0, 3, 1, 2, 3, 0, 2};
  std::vector<Literal> literals;
  std::vector<IntegerVariable> vars;
  for (int i = 0; i < 2 * num_nodes; ++i) {
    literals.push_back(Literal(model.Add(NewBooleanVariable()), true));
    vars.push_back(model.Add(NewIntegerVariableFromLiteral(literals.back())));
  }

  CutGenerator generator = CreateStronglyConnectedGraphCutGenerator(
      num_nodes, tails, heads, literals, &model);

  // Suppose only 0-1 and 2-3 are in the lp solution (values do not matter).
  auto& lp_values = *model.GetOrCreate<ModelLpValues>();
  lp_values.resize(16, 0.0);
  lp_values[vars[0]] = 0.5;
  lp_values[vars[1]] = 0.5;
  lp_values[vars[4]] = 1.0;
  lp_values[vars[5]] = 1.0;
  LinearConstraintManager manager(&model);
  generator.generate_cuts(&manager);

  // We should get two cuts.
  EXPECT_EQ(manager.num_cuts(), 2);
  EXPECT_THAT(manager.AllConstraints().front().constraint.VarsAsSpan(),
              ElementsAre(vars[3], vars[6]));
  EXPECT_THAT(manager.AllConstraints().back().constraint.VarsAsSpan(),
              ElementsAre(vars[2], vars[7]));
}

TEST(CreateStronglyConnectedGraphCutGeneratorTest, AnotherExample) {
  // This time, the graph is fully connected, but we still detect that {1, 2,
  // 3} do not have enough outgoing flow:
  //
  //           0.5
  //        0 <--> 1
  //        ^      |               0.5
  //   0.5  |      |  1     and  2 ----> 1
  //        v      v
  //        2 <--- 3
  //            1
  const int num_nodes = 4;
  std::vector<int> tails{0, 1, 0, 2, 1, 3, 2};
  std::vector<int> heads{1, 0, 2, 0, 3, 2, 1};
  std::vector<double> values{0.5, 0.0, 0.5, 0.0, 1.0, 1.0, 0.5};

  Model model;
  std::vector<Literal> literals;
  auto& lp_values = *model.GetOrCreate<ModelLpValues>();
  lp_values.resize(16, 0.0);
  for (int i = 0; i < values.size(); ++i) {
    literals.push_back(Literal(model.Add(NewBooleanVariable()), true));
    lp_values[model.Add(NewIntegerVariableFromLiteral(literals.back()))] =
        values[i];
  }

  CutGenerator generator = CreateStronglyConnectedGraphCutGenerator(
      num_nodes, tails, heads, literals, &model);

  LinearConstraintManager manager(&model);
  generator.generate_cuts(&manager);

  // The sets {2, 3} and {1, 2, 3} will generate cuts.
  // However as an heuristic, we will wait another round to generate {1, 2, 3}.
  EXPECT_EQ(manager.num_cuts(), 1);
  EXPECT_THAT(manager.AllConstraints().back().constraint.DebugString(),
              ::testing::StartsWith("1 <= 1*X3 1*X6"));
}

TEST(GenerateInterestingSubsetsTest, BasicExample) {
  const int num_nodes = 6;
  const std::vector<std::pair<int, int>> arcs = {{0, 5}, {2, 3}, {3, 4}};

  // Note that the order is not important, but is currently fixed.
  // This document the actual order.
  std::vector<int> subset_data;
  std::vector<absl::Span<const int>> subsets;
  GenerateInterestingSubsets(num_nodes, arcs,
                             /*stop_at_num_components=*/2, &subset_data,
                             &subsets);
  EXPECT_THAT(
      subsets,
      ElementsAre(ElementsAre(1), ElementsAre(5), ElementsAre(0),
                  ElementsAre(5, 0), ElementsAre(3), ElementsAre(2),
                  ElementsAre(3, 2), ElementsAre(4), ElementsAre(3, 2, 4)));

  // We can call it more than once.
  GenerateInterestingSubsets(num_nodes, arcs,
                             /*stop_at_num_components=*/2, &subset_data,
                             &subsets);
  EXPECT_THAT(
      subsets,
      ElementsAre(ElementsAre(1), ElementsAre(5), ElementsAre(0),
                  ElementsAre(5, 0), ElementsAre(3), ElementsAre(2),
                  ElementsAre(3, 2), ElementsAre(4), ElementsAre(3, 2, 4)));
}

TEST(CreateFlowCutGeneratorTest, BasicExample) {
  //
  //            /---> 2
  //    0 ---> 1      ^
  //            \---> 3
  //
  // With a flow of 2 leaving 0 and a flow of 1 requested at 2 and 3.
  // On each arc the flow <= max_flow * arc_indicator where max_flow = 2.
  const int num_nodes = 4;
  std::vector<int> tails{0, 1, 1, 3};
  std::vector<int> heads{1, 2, 3, 2};
  std::vector<double> values{1.0, 0.5, 0.5, 0.0};

  Model model;
  std::vector<AffineExpression> capacities;
  auto& lp_values = *model.GetOrCreate<ModelLpValues>();
  lp_values.resize(16, 0.0);
  for (int i = 0; i < values.size(); ++i) {
    AffineExpression expr;
    expr.var = model.Add(NewIntegerVariable(0, 1));
    expr.coeff = 2;
    expr.constant = 0;
    capacities.emplace_back(expr);
    lp_values[capacities.back().var] = values[i];
  }

  const auto get_flows = [](const std::vector<bool>& in_subset,
                            IntegerValue* min_incoming_flow,
                            IntegerValue* min_outgoing_flow) {
    IntegerValue demand(0);
    if (in_subset[0]) demand -= 2;
    if (in_subset[2]) demand += 1;
    if (in_subset[3]) demand += 1;
    *min_incoming_flow = std::max(IntegerValue(0), demand);
    *min_outgoing_flow = std::max(IntegerValue(0), -demand);
  };
  CutGenerator generator = CreateFlowCutGenerator(
      num_nodes, tails, heads, capacities, get_flows, &model);

  LinearConstraintManager manager(&model);
  generator.generate_cuts(&manager);

  // The sets {2} and {3} will generate incoming flow cuts.
  EXPECT_EQ(manager.num_cuts(), 2);
  EXPECT_THAT(manager.AllConstraints().front().constraint.DebugString(),
              ::testing::StartsWith("1 <= 1*X2"));
  EXPECT_THAT(manager.AllConstraints().back().constraint.DebugString(),
              ::testing::StartsWith("1 <= 1*X1 1*X3"));
}

TEST(CreateFlowCutGeneratorTest, WithMinusOneArcs) {
  //    0 ---> 1 -->
  //           |
  //           \ -->
  const int num_nodes = 2;
  std::vector<int> tails{0, 1, 1};
  std::vector<int> heads{1, -1, -1};
  std::vector<double> values{1.0, 0.5, 0.0};

  Model model;
  std::vector<AffineExpression> capacities;
  auto& lp_values = *model.GetOrCreate<ModelLpValues>();
  lp_values.resize(16, 0.0);
  for (int i = 0; i < values.size(); ++i) {
    AffineExpression expr;
    expr.var = model.Add(NewIntegerVariable(0, 1));
    expr.coeff = 2;
    expr.constant = 0;
    capacities.emplace_back(expr);
    lp_values[capacities.back().var] = values[i];
  }

  const auto get_flows = [](const std::vector<bool>& in_subset,
                            IntegerValue* min_incoming_flow,
                            IntegerValue* min_outgoing_flow) {
    IntegerValue demand(0);
    if (in_subset[0]) demand -= 2;
    *min_incoming_flow = std::max(IntegerValue(0), demand);
    *min_outgoing_flow = std::max(IntegerValue(0), -demand);
  };
  CutGenerator generator = CreateFlowCutGenerator(
      num_nodes, tails, heads, capacities, get_flows, &model);

  LinearConstraintManager manager(&model);
  generator.generate_cuts(&manager);

  // We artificially put bad LP values so that {1} generate outgoing flow cut.
  EXPECT_EQ(manager.num_cuts(), 1);
  EXPECT_THAT(manager.AllConstraints().front().constraint.DebugString(),
              ::testing::StartsWith("1 <= 1*X1 1*X2"));
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
