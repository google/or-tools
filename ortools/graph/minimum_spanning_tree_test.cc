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

#include "ortools/graph/minimum_spanning_tree.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <random>
#include <vector>

#include "absl/base/macros.h"
#include "absl/random/distributions.h"
#include "absl/types/span.h"
#include "benchmark/benchmark.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/types.h"
#include "ortools/graph/graph.h"

namespace operations_research {
namespace {

using ::testing::UnorderedElementsAreArray;
using ::util::CompleteGraph;
using ::util::ListGraph;

TEST(MSTTest, EmptyGraph) {
  ListGraph<int, int> graph(0, 0);
  std::vector<int64_t> costs;
  const std::vector<int> mst =
      BuildKruskalMinimumSpanningTree<ListGraph<int, int>>(
          graph, [&costs](int a, int b) { return costs[a] < costs[b]; });
  EXPECT_EQ(0, mst.size());
}

TEST(MSTTest, NoArcGraph) {
  ListGraph<int, int> graph(5, 0);
  std::vector<int64_t> costs;
  const std::vector<int> mst =
      BuildKruskalMinimumSpanningTree<ListGraph<int, int>>(
          graph, [&costs](int a, int b) { return costs[a] < costs[b]; });
  EXPECT_EQ(0, mst.size());
}

// Helper function to check the expected MST is obtained with Kruskal.
void CheckMSTWithKruskal(const ListGraph<int, int>& graph,
                         absl::Span<const int64_t> costs,
                         const std::vector<int>& expected_arcs) {
  const auto ByCost = [costs](int a, int b) {
    if (costs[a] != costs[b]) {
      return costs[a] < costs[b];
    }
    // for the sake of stability, preserve the order of arcs with the same cost
    return a < b;
  };
  const std::vector<int> mst = BuildKruskalMinimumSpanningTree(graph, ByCost);
  EXPECT_THAT(expected_arcs, UnorderedElementsAreArray(mst));
  std::vector<int> sorted_arcs(graph.num_arcs());
  for (const int arc : graph.AllForwardArcs()) {
    sorted_arcs[arc] = arc;
  }
  std::sort(sorted_arcs.begin(), sorted_arcs.end(), ByCost);
  EXPECT_THAT(mst, UnorderedElementsAreArray(
                       BuildKruskalMinimumSpanningTreeFromSortedArcs(
                           graph, sorted_arcs)));
}

// Helper function to check the expected MST is obtained with Prim.
void CheckMSTWithPrim(const ListGraph<int, int>& graph,
                      absl::Span<const int64_t> costs,
                      const std::vector<int>& expected_arcs) {
  const std::vector<int> prim_mst = BuildPrimMinimumSpanningTree(
      graph, [costs](int arc) { return costs[arc]; });
  EXPECT_THAT(expected_arcs, UnorderedElementsAreArray(prim_mst));
}

// Testing Kruskal MST on a small undirectedgraph:
// - original graph:
// 0 -(1)- 1 -(2)- 2
//         |       |
//        (1)     (1)
//         |       |
//         4 -(4)- 3
//
// - minimum spanning tree:
// 0 ----> 1 ----> 2
//         |       |
//         |       |
//         v       v
//         4       3
//
TEST(MSTTest, SmallGraph) {
  const int kArcs[][2] = {{0, 1}, {1, 2}, {1, 4}, {2, 3}, {3, 4}};
  const int64_t kCosts[] = {1, 2, 1, 1, 4};
  const int kNodes = 5;
  ListGraph<int, int> graph(kNodes, ABSL_ARRAYSIZE(kArcs) * 2);
  std::vector<int64_t> costs(ABSL_ARRAYSIZE(kArcs) * 2, 0);
  for (int i = 0; i < ABSL_ARRAYSIZE(kArcs); ++i) {
    costs[graph.AddArc(kArcs[i][0], kArcs[i][1])] = kCosts[i];
    costs[graph.AddArc(kArcs[i][1], kArcs[i][0])] = kCosts[i];
  }
  CheckMSTWithKruskal(graph, costs, {0, 4, 6, 2});
  CheckMSTWithPrim(graph, costs, {0, 4, 6, 2});
}

// Testing on a small graph with kint64max as value for arcs.
TEST(MSTTest, SmallGraphWithMaxValueArcs) {
  const int kArcs[][2] = {{0, 1}, {1, 2}};
  const int kNodes = 3;
  const int64_t kCosts[] = {std::numeric_limits<int64_t>::max(),
                            std::numeric_limits<int64_t>::max()};
  ListGraph<int, int> graph(kNodes, ABSL_ARRAYSIZE(kArcs) * 2);
  std::vector<int64_t> costs(ABSL_ARRAYSIZE(kArcs) * 2, 0);
  for (int i = 0; i < ABSL_ARRAYSIZE(kArcs); ++i) {
    costs[graph.AddArc(kArcs[i][0], kArcs[i][1])] = kCosts[i];
    costs[graph.AddArc(kArcs[i][1], kArcs[i][0])] = kCosts[i];
  }
  CheckMSTWithKruskal(graph, costs, {0, 2});
  CheckMSTWithPrim(graph, costs, {0, 2});
}

// Testing Kruskal MST on a small directed graph:
// - original graph:
// 0 <-(1)- 1 <-(2)- 2
//          ^ \      |
//         (1) (0)  (1)
//          |     \  |
//          |      > v
//          4 -(4)-> 3
//
// - minimum spanning tree:
// 0 <---- 1     2
//         ^ \   |
//         |  \  |
//         |   \ |
//         |    >v
//         4     3
//
TEST(MSTTest, SmallDirectedGraph) {
  const int kArcs[][2] = {{1, 0}, {2, 1}, {4, 1}, {2, 3}, {4, 3}, {1, 3}};
  const int64_t kCosts[] = {1, 2, 1, 1, 4, 0};
  const int kNodes = 5;
  ListGraph<int, int> graph(kNodes, ABSL_ARRAYSIZE(kArcs));
  std::vector<int64_t> costs(ABSL_ARRAYSIZE(kArcs), 0);
  for (int i = 0; i < ABSL_ARRAYSIZE(kArcs); ++i) {
    costs[graph.AddArc(kArcs[i][0], kArcs[i][1])] = kCosts[i];
  }
  CheckMSTWithKruskal(graph, costs, {5, 0, 2, 3});
}

// Testing Kruskal MST on a small disconnected graph:
// - original graph:
// 0 -(1)- 1    2
//         |    |
//        (1)  (1)
//         |    |
//         4    3
//
// - minimum spanning tree:
// 0 ----> 1    2
//         |    |
//         |    |
//         v    v
//         4    3
//
TEST(MSTTest, SmallDisconnectedGraph) {
  const int kArcs[][2] = {{0, 1}, {1, 4}, {2, 3}};
  const int64_t kCosts[] = {1, 1, 1};
  const int kNodes = 5;
  ListGraph<int, int> graph(kNodes, ABSL_ARRAYSIZE(kArcs) * 2);
  std::vector<int64_t> costs(ABSL_ARRAYSIZE(kArcs) * 2, 0);
  for (int i = 0; i < ABSL_ARRAYSIZE(kArcs); ++i) {
    costs[graph.AddArc(kArcs[i][0], kArcs[i][1])] = kCosts[i];
    costs[graph.AddArc(kArcs[i][1], kArcs[i][0])] = kCosts[i];
  }
  CheckMSTWithKruskal(graph, costs, {0, 2, 4});
}

// Benchmark on a grid graph with random arc costs; 'size' corresponds to the
// number of nodes on a row/column of the grid.
template <typename GraphType>
void BM_KruskalMinimimumSpanningTreeOnGrid(benchmark::State& state) {
  int size = state.range(0);
  const int64_t kCostLimit = 1000000;
  std::mt19937 randomizer(0);
  const int num_nodes = size * size;
  const int num_edges = 2 * (2 * size * (size - 1) + 2 * size - 4);
  std::vector<int64_t> edge_costs(num_edges, 0);
  ListGraph<int, int> graph(num_nodes, num_edges);
  for (int i = 0; i < size; ++i) {
    for (int j = 0; j < size; ++j) {
      if (j < size - 1) {
        const int64_t cost = absl::Uniform(randomizer, 0, kCostLimit);
        edge_costs[graph.AddArc(i * size + j, i * size + j + 1)] = cost;
        edge_costs[graph.AddArc(i * size + j + 1, i * size + j)] = cost;
      }
      if (i < size - 1) {
        const int64_t cost = absl::Uniform(randomizer, 0, kCostLimit);
        edge_costs[graph.AddArc(i * size + j, (i + 1) * size + j)] = cost;
        edge_costs[graph.AddArc((i + 1) * size + j, i * size + j)] = cost;
      }
    }
  }
  for (int i = 1; i < size - 1; ++i) {
    const int64_t cost = absl::Uniform(randomizer, 0, kCostLimit);
    edge_costs[graph.AddArc(i * size, i * size + size - 1)] = cost;
    edge_costs[graph.AddArc(i * size + size - 1, i * size)] = cost;
  }
  for (int i = 1; i < size - 1; ++i) {
    const int64_t cost = absl::Uniform(randomizer, 0, kCostLimit);
    edge_costs[graph.AddArc(i, (size - 1) * size + i)] = cost;
    edge_costs[graph.AddArc((size - 1) * size + i, i)] = cost;
  }
  for (auto _ : state) {
    const std::vector<int> mst = BuildKruskalMinimumSpanningTree(
        graph,
        [&edge_costs](int a, int b) { return edge_costs[a] < edge_costs[b]; });
    EXPECT_EQ(num_nodes - 1, mst.size());
  }
}

BENCHMARK_TEMPLATE(BM_KruskalMinimimumSpanningTreeOnGrid, ListGraph<>)
    ->Range(2, 1 << 8);

template <typename GraphType>
void BM_PrimMinimimumSpanningTreeOnGrid(benchmark::State& state) {
  int size = state.range(0);
  const int64_t kCostLimit = 1000000;
  std::mt19937 randomizer(0);
  const int num_nodes = size * size;
  const int num_edges = 2 * (2 * size * (size - 1) + 2 * size - 4);
  std::vector<int64_t> edge_costs(num_edges, 0);
  ListGraph<int, int> graph(num_nodes, num_edges);
  for (int i = 0; i < size; ++i) {
    for (int j = 0; j < size; ++j) {
      if (j < size - 1) {
        const int64_t cost = absl::Uniform(randomizer, 0, kCostLimit);
        edge_costs[graph.AddArc(i * size + j, i * size + j + 1)] = cost;
        edge_costs[graph.AddArc(i * size + j + 1, i * size + j)] = cost;
      }
      if (i < size - 1) {
        const int64_t cost = absl::Uniform(randomizer, 0, kCostLimit);
        edge_costs[graph.AddArc(i * size + j, (i + 1) * size + j)] = cost;
        edge_costs[graph.AddArc((i + 1) * size + j, i * size + j)] = cost;
      }
    }
  }
  for (int i = 1; i < size - 1; ++i) {
    const int64_t cost = absl::Uniform(randomizer, 0, kCostLimit);
    edge_costs[graph.AddArc(i * size, i * size + size - 1)] = cost;
    edge_costs[graph.AddArc(i * size + size - 1, i * size)] = cost;
  }
  for (int i = 1; i < size - 1; ++i) {
    const int64_t cost = absl::Uniform(randomizer, 0, kCostLimit);
    edge_costs[graph.AddArc(i, (size - 1) * size + i)] = cost;
    edge_costs[graph.AddArc((size - 1) * size + i, i)] = cost;
  }
  for (auto _ : state) {
    const std::vector<int> mst = BuildPrimMinimumSpanningTree(
        graph, [&edge_costs](int arc) { return edge_costs[arc]; });
    EXPECT_EQ(num_nodes - 1, mst.size());
  }
}

BENCHMARK_TEMPLATE(BM_PrimMinimimumSpanningTreeOnGrid, ListGraph<>)
    ->Range(2, 1 << 8);

// Benchmark on complete graph with random arc costs.
void BM_KruskalMinimimumSpanningTreeOnCompleteGraph(benchmark::State& state) {
  int num_nodes = state.range(0);
  const int64_t kCostLimit = 1000000;
  std::mt19937 randomizer(0);
  CompleteGraph<int, int> graph(num_nodes);
  std::vector<int64_t> edge_costs(graph.num_arcs(), 0);
  for (const int node : graph.AllNodes()) {
    for (const auto arc : graph.OutgoingArcs(node)) {
      const int64_t cost = absl::Uniform(randomizer, 0, kCostLimit);
      edge_costs[arc] = cost;
    }
  }
  for (auto _ : state) {
    const std::vector<int> mst = BuildKruskalMinimumSpanningTree(
        graph,
        [&edge_costs](int a, int b) { return edge_costs[a] < edge_costs[b]; });
    EXPECT_EQ(num_nodes - 1, mst.size());
  }
}

BENCHMARK(BM_KruskalMinimimumSpanningTreeOnCompleteGraph)->Range(2, 1 << 10);

void BM_PrimMinimimumSpanningTreeOnCompleteGraph(benchmark::State& state) {
  int num_nodes = state.range(0);
  const int64_t kCostLimit = 1000000;
  std::mt19937 randomizer(0);
  CompleteGraph<int, int> graph(num_nodes);
  std::vector<int64_t> edge_costs(graph.num_arcs(), 0);
  for (const int node : graph.AllNodes()) {
    for (const auto arc : graph.OutgoingArcs(node)) {
      const int64_t cost = absl::Uniform(randomizer, 0, kCostLimit);
      edge_costs[arc] = cost;
    }
  }
  for (auto _ : state) {
    const std::vector<int> mst = BuildPrimMinimumSpanningTree(
        graph, [&edge_costs](int arc) { return edge_costs[arc]; });
    EXPECT_EQ(num_nodes - 1, mst.size());
  }
}

BENCHMARK(BM_PrimMinimimumSpanningTreeOnCompleteGraph)->Range(2, 1 << 10);

}  // namespace
}  // namespace operations_research
