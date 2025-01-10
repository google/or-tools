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

#include "ortools/graph/perfect_matching.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <random>
#include <vector>

#include "absl/random/random.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/linear_solver/solve_mp_model.h"

namespace operations_research {
namespace {

TEST(MinCostPerfectMatchingTest, Empty) {
  MinCostPerfectMatching matcher(0);
  ASSERT_EQ(matcher.Solve(), MinCostPerfectMatching::OPTIMAL);
  EXPECT_EQ(matcher.OptimalCost(), 0);
  EXPECT_EQ(matcher.Matches().size(), 0);
}

TEST(MinCostPerfectMatchingTest, OptimumMatching) {
  MinCostPerfectMatching matcher(4);
  matcher.AddEdgeWithCost(0, 2, 0);
  matcher.AddEdgeWithCost(0, 3, 2);
  matcher.AddEdgeWithCost(1, 2, 3);
  matcher.AddEdgeWithCost(1, 3, 4);
  ASSERT_EQ(matcher.Solve(), MinCostPerfectMatching::OPTIMAL);
  EXPECT_EQ(matcher.OptimalCost(), 4);
  EXPECT_EQ(matcher.Matches().size(), 4);
  EXPECT_EQ(matcher.Match(0), 2);
  EXPECT_EQ(matcher.Match(1), 3);
  EXPECT_EQ(matcher.Match(2), 0);
  EXPECT_EQ(matcher.Match(3), 1);
}

TEST(MinCostPerfectMatchingTest, BipartiteInfeasibleProblem) {
  MinCostPerfectMatching matcher(4);
  matcher.AddEdgeWithCost(0, 3, 2);
  matcher.AddEdgeWithCost(0, 3, 10);
  matcher.AddEdgeWithCost(1, 3, 3);
  matcher.AddEdgeWithCost(1, 3, 20);
  ASSERT_EQ(matcher.Solve(), MinCostPerfectMatching::INFEASIBLE);
}

TEST(MinCostPerfectMatchingTest, LargerBipartiteInfeasibleProblem) {
  MinCostPerfectMatching matcher(10);
  matcher.AddEdgeWithCost(0, 5, 0);
  matcher.AddEdgeWithCost(0, 6, 2);
  matcher.AddEdgeWithCost(1, 5, 3);
  matcher.AddEdgeWithCost(1, 6, 4);
  matcher.AddEdgeWithCost(2, 5, 4);
  matcher.AddEdgeWithCost(2, 6, 4);
  matcher.AddEdgeWithCost(3, 7, 4);
  matcher.AddEdgeWithCost(3, 8, 4);
  matcher.AddEdgeWithCost(3, 9, 4);
  matcher.AddEdgeWithCost(4, 7, 4);
  matcher.AddEdgeWithCost(4, 8, 4);
  matcher.AddEdgeWithCost(4, 9, 4);
  ASSERT_EQ(matcher.Solve(), MinCostPerfectMatching::INFEASIBLE);
}

TEST(MinCostPerfectMatchingTest, IntegerOverflow) {
  MinCostPerfectMatching matcher(4);
  matcher.AddEdgeWithCost(0, 2, std::numeric_limits<int64_t>::max());
  matcher.AddEdgeWithCost(0, 3, std::numeric_limits<int64_t>::max());
  matcher.AddEdgeWithCost(1, 2, std::numeric_limits<int64_t>::max());
  matcher.AddEdgeWithCost(1, 3, std::numeric_limits<int64_t>::max());
  ASSERT_EQ(matcher.Solve(), MinCostPerfectMatching::INTEGER_OVERFLOW);
}

TEST(MinCostPerfectMatchingTest, CostOverflow) {
  MinCostPerfectMatching matcher(4);
  matcher.AddEdgeWithCost(0, 2, std::numeric_limits<int64_t>::max() / 3);
  matcher.AddEdgeWithCost(0, 3, std::numeric_limits<int64_t>::max() / 3);
  matcher.AddEdgeWithCost(1, 2, std::numeric_limits<int64_t>::max() / 3);
  matcher.AddEdgeWithCost(1, 3, std::numeric_limits<int64_t>::max() / 3);
  ASSERT_EQ(matcher.Solve(), MinCostPerfectMatching::COST_OVERFLOW);
  EXPECT_EQ(matcher.OptimalCost(), std::numeric_limits<int64_t>::max());
}

class MacholWienTest : public ::testing::TestWithParam<int> {};

// The following test computes bi-partite assignments on the instances described
// in Robert E. Machol and Michael Wien, "Errata: A Hard Assignment
// Problem" Operations Research, vol. 25, p. 364, 1977.
// http://www.jstor.org/stable/169842
//
// Such instances are proven difficult for the Hungarian method. Note that since
// this is a bi-partite problem, this doesn't exercise the Shrink()/Expand()
// methods.
TEST_P(MacholWienTest, SolveHardProblem) {
  const int n = GetParam();
  MinCostPerfectMatching matcher(2 * n);
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < n; ++j) {
      matcher.AddEdgeWithCost(i, n + j, /*cost=*/i * j);
    }
  }
  ASSERT_EQ(matcher.Solve(), MinCostPerfectMatching::OPTIMAL);

  int64_t cost = 0;
  for (int i = 0; i < n; ++i) {
    cost += i * (n - 1 - i);
    EXPECT_EQ(matcher.Match(i), 2 * n - 1 - i);
  }
  EXPECT_EQ(matcher.OptimalCost(), cost);
}

// Even with -c opt, a 1000x1000 Machol-Wien problem currently takes too long to
// solve.
#ifdef NDEBUG
INSTANTIATE_TEST_SUITE_P(MacholWienProblems, MacholWienTest,
                         ::testing::Values(10, 50, 100, 200));
#else
INSTANTIATE_TEST_SUITE_P(MacholWienProblems, MacholWienTest,
                         ::testing::Values(10, 50));
#endif

using NodeIndex = BlossomGraph::NodeIndex;
using EdgeIndex = BlossomGraph::EdgeIndex;
using CostValue = BlossomGraph::CostValue;

// Tests on a basic complete graph on 4 nodes.
TEST(BlossomGraphTest, Initialization) {
  const int num_nodes = 4;
  BlossomGraph graph(num_nodes);
  CostValue increasing_cost;
  for (NodeIndex a(0); a < num_nodes; ++a) {
    for (NodeIndex b(a + 1); b < num_nodes; ++b) {
      graph.AddEdge(a, b, ++increasing_cost);
    }
  }
  CHECK(graph.Initialize());
  CHECK(graph.DebugDualsAreFeasible());

  EXPECT_EQ(graph.Dual(graph.GetNode(0)), CostValue(2));
  EXPECT_EQ(graph.Dual(graph.GetNode(1)), CostValue(0));
  EXPECT_EQ(graph.Dual(graph.GetNode(2)), CostValue(2));
  EXPECT_EQ(graph.Dual(graph.GetNode(3)), CostValue(4));

  // We don't have a perfect matching yet. Only 1 pair is matched.
  EXPECT_EQ(graph.Match(NodeIndex(0)), NodeIndex(1));
  EXPECT_EQ(graph.Match(NodeIndex(1)), NodeIndex(0));
  EXPECT_EQ(graph.Slack(graph.GetEdge(0)), CostValue(0));  // edge 0 <-> 1.
  EXPECT_FALSE(!graph.NodeIsMatched(NodeIndex(0)));
  EXPECT_FALSE(!graph.NodeIsMatched(NodeIndex(1)));

  // We have two unmatched nodes, which are tree roots.
  EXPECT_EQ(graph.Match(NodeIndex(2)), NodeIndex(2));
  EXPECT_EQ(graph.Match(NodeIndex(3)), NodeIndex(3));
  EXPECT_TRUE(!graph.NodeIsMatched(NodeIndex(2)));
  EXPECT_TRUE(!graph.NodeIsMatched(NodeIndex(3)));

  // The edge 2 <-> 3 is not tight.
  // Is slack is cost = 6 - dual(2) - dual(3) == 3.
  EXPECT_EQ(graph.Slack(graph.GetEdge(5)), CostValue(6));

  // There is still some operation we can do, and we can't increase
  EXPECT_EQ(graph.ComputeMaxCommonTreeDualDeltaAndResetPrimalEdgeQueue(),
            CostValue(0));

  graph.PrimalUpdates();
  VLOG(2) << graph.DebugString();

  const CostValue delta =
      graph.ComputeMaxCommonTreeDualDeltaAndResetPrimalEdgeQueue();
  EXPECT_EQ(delta, 3);
  graph.UpdateAllTrees(delta);

  EXPECT_EQ(graph.Dual(graph.GetNode(0)), CostValue(-1));
  EXPECT_EQ(graph.Dual(graph.GetNode(1)), CostValue(3));
  EXPECT_EQ(graph.Dual(graph.GetNode(2)), CostValue(5));
  EXPECT_EQ(graph.Dual(graph.GetNode(3)), CostValue(7));

  VLOG(2) << graph.DebugString();
  graph.PrimalUpdates();
}

struct Edge {
  int node1;
  int node2;
  int64_t cost;
};

std::vector<Edge> GenerateAndLoadRandomProblem(
    int num_nodes, int num_arcs, MinCostPerfectMatching* matcher) {
  CHECK_EQ(num_nodes % 2, 0);

  absl::BitGen random;
  std::uniform_int_distribution<int> random_cost(0, 1000);
  std::vector<Edge> all_edges;

  // Starts by making sure there is a matching.
  std::vector<int> all_nodes;
  for (int i = 0; i < num_nodes; ++i) all_nodes.push_back(i);
  while (!all_nodes.empty()) {
    std::vector<int> edge_nodes;
    for (int i = 0; i < 2; ++i) {
      const int index =
          absl::Uniform(random, 0, static_cast<int>(all_nodes.size() - 1));
      edge_nodes.push_back(all_nodes[index]);
      all_nodes[index] = all_nodes.back();
      all_nodes.pop_back();
    }
    all_edges.push_back({edge_nodes[0], edge_nodes[1], random_cost(random)});
  }

  // Now just add random arcs.
  for (int i = num_nodes / 2; i < num_arcs; ++i) {
    const int node1 = absl::Uniform(random, 0, num_nodes);
    const int node2 = absl::Uniform(random, 0, num_nodes);
    if (node1 != node2) {
      all_edges.push_back({node1, node2, random_cost(random)});
    }
  }

  matcher->Reset(num_nodes);
  for (const Edge edge : all_edges) {
    matcher->AddEdgeWithCost(edge.node1, edge.node2, edge.cost);
  }

  return all_edges;
}

// We check that the returned matching is a valid matching with the correct
// costs.
//
// TODO(user): We could theoretically recover the dual and check the optimality
// condition if really needed. This is a bit involved though, and with the MIP
// tests below, we should have a good enough confidence in the code already.
void CheckOptimalSolution(const MinCostPerfectMatching& matcher,
                          absl::Span<const Edge> edges) {
  const std::vector<int>& matches = matcher.Matches();
  std::vector<bool> seen(matches.size(), false);
  int num_seen = 0;
  for (int i = 0; i < matches.size(); ++i) {
    const int m = matches[i];
    ASSERT_NE(m, i);
    ASSERT_GE(m, 0);
    ASSERT_LT(m, matches.size());
    ASSERT_EQ(matches[m], i);
    if (m < i) continue;

    ASSERT_FALSE(seen[i]);
    ASSERT_FALSE(seen[m]);
    seen[i] = true;
    seen[m] = true;
    num_seen += 2;
  }
  EXPECT_EQ(num_seen, matches.size());

  // Check that the matching returned has the correct cost.
  std::vector<int64_t> costs(matches.size(),
                             std::numeric_limits<int64_t>::max());
  for (const Edge e : edges) {
    if (matches[e.node1] == e.node2) {
      const int rep = std::min(e.node1, e.node2);
      const int other = std::max(e.node1, e.node2);
      costs[rep] = std::min(costs[rep], e.cost);
      costs[other] = 0;
    }
  }
  int64_t actual_cost = 0;
  for (int i = 0; i < costs.size(); ++i) {
    CHECK_NE(costs[i], std::numeric_limits<int64_t>::max());
    actual_cost += costs[i];
  }
  EXPECT_EQ(matcher.OptimalCost(), actual_cost);
}

TEST(BlossomGraphTest, RandomSmallGraph) {
  for (const int size : {10, 40, 100, 1000}) {
    for (const int edge_factor : {1, 10, 100}) {
      MinCostPerfectMatching matcher;
      const std::vector<Edge> edges =
          GenerateAndLoadRandomProblem(size, size * edge_factor, &matcher);
      ASSERT_EQ(matcher.Solve(), MinCostPerfectMatching::OPTIMAL)
          << "Size: " << size << ", Edge factor: " << edge_factor;
      CheckOptimalSolution(matcher, edges);
    }
  }
}

TEST(BlossomGraphTest, RandomLargeGraph) {
  if (DEBUG_MODE) GTEST_SKIP() << "Too slow in non-opt";
  MinCostPerfectMatching matcher;
  const std::vector<Edge> edges =
      GenerateAndLoadRandomProblem(10000, 100000, &matcher);
  ASSERT_EQ(matcher.Solve(), MinCostPerfectMatching::OPTIMAL);
  CheckOptimalSolution(matcher, edges);
}

int64_t SolveWithMip(absl::Span<const Edge> edges) {
  MPModelRequest request;
  request.set_solver_type(MPModelRequest::SAT_INTEGER_PROGRAMMING);

  std::vector<MPConstraintProto*> exactly_ones;
  for (int i = 0; i < edges.size(); ++i) {
    auto* var_proto = request.mutable_model()->add_variable();
    var_proto->set_lower_bound(0.0);
    var_proto->set_upper_bound(1.0);
    var_proto->set_is_integer(true);
    var_proto->set_objective_coefficient(edges[i].cost);

    for (const int node : {edges[i].node1, edges[i].node2}) {
      // Create constraint if needed.
      while (node >= exactly_ones.size()) {
        exactly_ones.push_back(request.mutable_model()->add_constraint());
        exactly_ones.back()->set_lower_bound(1.0);
        exactly_ones.back()->set_upper_bound(1.0);
      }

      // Add edge to it.
      exactly_ones[node]->add_var_index(i);
      exactly_ones[node]->add_coefficient(1.0);
    }
  }

  const MPSolutionResponse response = SolveMPModel(request);
  return static_cast<int64_t>(std::round(response.objective_value()));
}

class AgreeWithMipTest : public ::testing::TestWithParam<int> {};

TEST_P(AgreeWithMipTest, CompareOptimalObjective) {
  MinCostPerfectMatching matcher;
  const std::vector<Edge> edges =
      GenerateAndLoadRandomProblem(50, 100, &matcher);
  ASSERT_EQ(matcher.Solve(), MinCostPerfectMatching::OPTIMAL);
  EXPECT_EQ(matcher.OptimalCost(), SolveWithMip(edges));
}

INSTANTIATE_TEST_SUITE_P(RandomInstances, AgreeWithMipTest,
                         ::testing::Range(0, 10));

}  // namespace
}  // namespace operations_research
