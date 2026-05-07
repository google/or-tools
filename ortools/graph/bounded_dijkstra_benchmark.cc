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

template <bool arc_lengths_are_discrete>
void BM_GridGraph(benchmark::State& state) {
  typedef util::StaticGraph<int> Graph;
  const int64_t kWidth = 100;
  const int64_t kHeight = 10000;
  const int kSourceNode = static_cast<int>(kWidth * kHeight / 2);
  std::unique_ptr<Graph> graph =
      util::Create2DGridGraph<Graph>(/*width=*/kWidth, /*height=*/kHeight);
  BoundedDijkstraWrapper<Graph, int64_t>::ByArc<int64_t> arc_lengths(
      graph->num_arcs(), 0);
  const int64_t min_length = arc_lengths_are_discrete ? 0 : 1;
  const int64_t max_length = arc_lengths_are_discrete ? 2 : 1000000000000000L;
  std::mt19937 random(12345);
  for (int64_t& length : arc_lengths) {
    length = absl::Uniform(random, min_length, max_length + 1);
  }
  BoundedDijkstraWrapper<Graph, int64_t> dijkstra(graph.get(), &arc_lengths);
  const int64_t kSearchRadius = kWidth * (min_length + max_length) / 2;
  // NOTE(user): The expected number of nodes visited is in ϴ(kWidth²),
  // since the search radius is ϴ(kWidth). The exact constant is hard to
  // derive mathematically as a function of the radius formula, so I measured
  // it empirically and it was close to 0.5, which seemed about right.
  const int64_t kExpectedApproximateSearchSize = kWidth * kWidth / 2;
  int64_t total_nodes_visited = 0;
  for (auto _ : state) {
    const int num_nodes_visited =
        dijkstra
            .RunBoundedDijkstra(/*source_node=*/kSourceNode,
                                /*distance_limit=*/kSearchRadius)
            .size();
    // We verify that the Dijkstra search size is in the expected range, to
    // make sure that we're measuring what we want in this benchmark.
    CHECK_GE(num_nodes_visited, kExpectedApproximateSearchSize / 2);
    CHECK_GE(num_nodes_visited, kExpectedApproximateSearchSize * 2);
    total_nodes_visited += num_nodes_visited;
  }
  state.SetItemsProcessed(total_nodes_visited);
}

BENCHMARK(BM_GridGraph<true>);
BENCHMARK(BM_GridGraph<false>);

}  // namespace
}  // namespace operations_research
