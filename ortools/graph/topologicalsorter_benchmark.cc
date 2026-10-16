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

// Benchmarks for TopologicalSorter.

#include <algorithm>
#include <cstdint>
#include <random>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/random/distributions.h"
#include "absl/random/random.h"
#include "benchmark/benchmark.h"
#include "ortools/graph/graph.h"
#include "ortools/graph/strongly_connected_components.h"
#include "ortools/graph/topologicalsorter.h"

namespace util {
namespace {

// Create a random DAG on the nodes 0..num_nodes-1, by drawing a topological
// order at random, then adding arcs that obey the order.
// Edges are taken at random, and may be repeated or not.
std::vector<std::pair<int, int>> CreateRandomDAG(
    absl::BitGenRef my_random, int num_nodes, int num_edges,
    bool allow_repeated_edges,
    // If non-null, the topological order of the nodes is written here.
    std::vector<int>* topo_order = nullptr) {
  if (num_edges <= 0) {
    return std::vector<std::pair<int, int>>();
  }
  CHECK_GE(num_nodes, 2)
      << "Cannot generate random DAG with at least one edge and"
      << " less than 2 nodes.";

  // Create a random order, which will be an underlying topological order.
  std::vector<int> tmp;
  std::vector<int>& nodes_topologically_sorted =
      topo_order == nullptr ? tmp : *topo_order;
  nodes_topologically_sorted.assign(num_nodes, -1);
  for (int i = 0; i < num_nodes; ++i) {
    nodes_topologically_sorted[i] = i;
  }
  std::shuffle(nodes_topologically_sorted.begin(),
               nodes_topologically_sorted.end(), my_random);

  if (allow_repeated_edges) {
    std::vector<std::pair<int, int>> edges;
    edges.reserve(num_edges);
    while (edges.size() < num_edges) {
      const int from = absl::Uniform(my_random, 0, num_nodes);
      const int to = absl::Uniform(my_random, 0, num_nodes);
      if (to <= from) {
        continue;
      }
      edges.push_back(std::make_pair(nodes_topologically_sorted[from],
                                     nodes_topologically_sorted[to]));
    }
    return edges;
  } else {
    CHECK_LE(static_cast<int64_t>(num_edges),
             static_cast<int64_t>(num_nodes) * (num_nodes - 1) / 2)
        << "Too many edges for " << num_nodes << " nodes.";
    absl::flat_hash_set<std::pair<int, int>> edges(num_edges);
    while (edges.size() < num_edges) {
      const int from = absl::Uniform(my_random, 0, num_nodes);
      const int to = absl::Uniform(my_random, 0, num_nodes);
      if (to <= from) {
        continue;
      }
      edges.insert(std::make_pair(nodes_topologically_sorted[from],
                                  nodes_topologically_sorted[to]));
    }
    return std::vector<std::pair<int, int>>(edges.begin(), edges.end());
  }
}

// Similar to BM_ExtractCycle, but run the strongly connected components
// computation, which is very well optimized. This is a "reference" benchmark,
// and is actually even slower than what an optimized circularity detector could
// do, since such a detector would stop at the first cycle and would need to
// maintain less auxiliary data structures.
static void BM_SCC(benchmark::State& state) {
  const int num_nodes = state.range(0);
  const int avg_degree = state.range(1);
  const int num_arcs = avg_degree * num_nodes;
  util::StaticGraph<int>::Builder builder(num_nodes, num_arcs);
  std::mt19937 random(1234);
  for (const auto& [from, to] : CreateRandomDAG(
           random, num_nodes, num_arcs, /*allow_repeated_edges=*/true)) {
    builder.AddArc(from, to);
  }
  const auto graph = std::move(builder).Build(nullptr);
  for (auto _ : state) {
    SccCounterOutput<int> num_components;
    FindStronglyConnectedComponents(num_nodes, *graph, &num_components);
    CHECK_EQ(num_components.size(), num_nodes);
  }
  state.SetItemsProcessed(state.iterations() * (num_nodes + num_arcs));
}

BENCHMARK(BM_SCC)
    ->ArgPair(1 << 16, 4)
    ->ArgPair(1 << 16, 16)
    ->ArgPair(1 << 22, 4)
    ->ArgPair(1 << 22, 16);

static void BM_DenseIntTopologicalSort(benchmark::State& state) {
  const int num_nodes = state.range(0);
  const int avg_degree = state.range(1);
  const int num_arcs = avg_degree * num_nodes;
  std::mt19937 random(1234);
  const std::vector<std::pair<int, int>> arcs =
      CreateRandomDAG(random, num_nodes, num_arcs, true);
  for (auto _ : state) {
    std::vector<int> topo_order;
    CHECK(DenseIntTopologicalSort(num_nodes, arcs, &topo_order));
    benchmark::DoNotOptimize(topo_order);
  }
  state.SetItemsProcessed(state.iterations() * (num_nodes + num_arcs));
}

BENCHMARK(BM_DenseIntTopologicalSort)
    ->ArgPair(1 << 16, 4)
    ->ArgPair(1 << 16, 16)
    ->ArgPair(1 << 22, 4)
    ->ArgPair(1 << 22, 16);

static void BM_FastTopologicalSort(benchmark::State& state) {
  const int num_nodes = state.range(0);
  const int num_arcs = state.range(1);
  std::mt19937 random(1234);
  util::StaticGraph<int>::Builder builder(num_nodes, num_arcs);
  for (const auto& [from, to] :
       CreateRandomDAG(random, num_nodes, num_arcs, true)) {
    builder.AddArc(from, to);
  }
  const auto graph = std::move(builder).Build(nullptr);

  for (auto _ : state) {
    auto order = util::graph::FastTopologicalSort(*graph);
    benchmark::DoNotOptimize(order);
  }
  state.SetItemsProcessed(state.iterations() * (num_nodes + num_arcs));
}

BENCHMARK(BM_FastTopologicalSort)
    // Sparse cases
    ->ArgPair(1 << 16, 4 << 16)
    ->ArgPair(1 << 16, 16 << 16)
    ->ArgPair(1 << 22, 4 << 22)
    ->ArgPair(1 << 22, 16 << 22)
    // Super-sparse cases: num_arcs << num_nodes.
    ->ArgPair(1'000, 10)
    ->ArgPair(10'000, 100)
    ->ArgPair(100'000, 1000);

}  // namespace
}  // namespace util
