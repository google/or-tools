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
#include <functional>
#include <memory>
#include <random>
#include <vector>

#include "absl/log/check.h"
#include "absl/random/distributions.h"
#include "absl/random/random.h"
#include "benchmark/benchmark.h"
#include "ortools/graph/bounded_dijkstra.h"
#include "ortools/graph_base/graph.h"
#include "ortools/graph_base/test_util.h"

namespace operations_research {
namespace {

constexpr int64_t kBenchmarkWidth = 100;
constexpr int64_t kBenchmarkHeight = 10000;
using Graph = util::StaticGraph<int>;

template <typename T>
void RunBenchmark(benchmark::State& state, T& dijkstra,
                  const int64_t search_radius) {
  const int kSourceNode =
      static_cast<int>(kBenchmarkWidth * kBenchmarkHeight / 2);
  // NOTE(user): The expected number of nodes visited is in ϴ(kWidth²),
  // since the search radius is ϴ(kWidth). The exact constant is hard to
  // derive mathematically as a function of the radius formula, so I measured
  // it empirically and it was close to 0.5, which seemed about right.
  const int64_t kExpectedApproximateSearchSize =
      kBenchmarkWidth * kBenchmarkWidth / 2;
  int64_t total_nodes_visited = 0;
  for (auto _ : state) {
    const int num_nodes_visited =
        dijkstra
            .RunBoundedDijkstra(/*source_node=*/kSourceNode,
                                /*distance_limit=*/search_radius)
            .size();
    // We verify that the Dijkstra search size is in the expected range, to
    // make sure that we're measuring what we want in this benchmark.
    CHECK_GE(num_nodes_visited, kExpectedApproximateSearchSize / 2);
    CHECK_GE(num_nodes_visited, kExpectedApproximateSearchSize * 2);
    total_nodes_visited += num_nodes_visited;
  }
  state.SetItemsProcessed(total_nodes_visited);
}

template <bool arc_lengths_are_discrete>
void BM_GridGraph(benchmark::State& state) {
  std::unique_ptr<Graph> graph =
      util::Create2DGridGraph<Graph>(kBenchmarkWidth, kBenchmarkHeight);
  BoundedDijkstraWrapper<Graph, int64_t>::ByArc<int64_t> arc_lengths(
      graph->num_arcs(), 0);
  const int64_t min_length = arc_lengths_are_discrete ? 0 : 1;
  const int64_t max_length = arc_lengths_are_discrete ? 2 : 1000000000000000L;
  std::mt19937 random(12345);
  for (int64_t& length : arc_lengths) {
    length = absl::Uniform(random, min_length, max_length + 1);
  }
  const int64_t kSearchRadius = kBenchmarkWidth * (min_length + max_length) / 2;
  BoundedDijkstraWrapper<Graph, int64_t> dijkstra(graph.get(), &arc_lengths);
  RunBenchmark(state, dijkstra, kSearchRadius);
}

BENCHMARK(BM_GridGraph<true>);
BENCHMARK(BM_GridGraph<false>);

void BM_GridGraph_ArcLengthFunctor(benchmark::State& state) {
  std::unique_ptr<Graph> graph =
      util::Create2DGridGraph<Graph>(kBenchmarkWidth, kBenchmarkHeight);
  BoundedDijkstraWrapper dijkstra(
      graph.get(), [](int node_id) { return 1 + (node_id % 3); });
  RunBenchmark(state, dijkstra, 2 * kBenchmarkWidth);
}
BENCHMARK(BM_GridGraph_ArcLengthFunctor);

void BM_GridGraph_ArcLengthStdFunction(benchmark::State& state) {
  std::unique_ptr<Graph> graph =
      util::Create2DGridGraph<Graph>(kBenchmarkWidth, kBenchmarkHeight);
  std::function<int64_t(int)> get_arc_length = [](int node_id) {
    return 1 + (node_id % 3);
  };
  BoundedDijkstraWrapper dijkstra(graph.get(), get_arc_length);
  RunBenchmark(state, dijkstra, 2 * kBenchmarkWidth);
}
BENCHMARK(BM_GridGraph_ArcLengthStdFunction);

}  // namespace
}  // namespace operations_research
