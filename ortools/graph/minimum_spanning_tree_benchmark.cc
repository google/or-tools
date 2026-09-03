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
#include <random>
#include <vector>

#include "absl/log/check.h"
#include "absl/random/distributions.h"
#include "benchmark/benchmark.h"
#include "ortools/graph/minimum_spanning_tree.h"
#include "ortools/graph_base/graph.h"

namespace operations_research {
namespace {

using ::util::CompleteGraph;
using ::util::ListGraph;

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
    CHECK_EQ(num_nodes - 1, mst.size());
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
    CHECK_EQ(num_nodes - 1, mst.size());
  }
}

BENCHMARK_TEMPLATE(BM_PrimMinimimumSpanningTreeOnGrid, ListGraph<>)
    ->Range(2, 1 << 8);

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
    CHECK_EQ(num_nodes - 1, mst.size());
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
    CHECK_EQ(num_nodes - 1, mst.size());
  }
}

BENCHMARK(BM_PrimMinimimumSpanningTreeOnCompleteGraph)->Range(2, 1 << 10);

}  // namespace
}  // namespace operations_research
