// Copyright 2010-2024 Google LLC
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

#include <climits>
#include <cstdint>
#include <memory>
#include <numeric>
#include <random>
#include <utility>
#include <vector>

#include "absl/random/distributions.h"
#include "benchmark/benchmark.h"
#include "isp/fiber/auto_design/utils/parallelizer.h"
#include "ortools/base/logging.h"
#include "ortools/base/threadlocal.h"
#include "ortools/graph/bounded_dijkstra.h"
#include "ortools/graph/graph.h"
#include "ortools/graph/shortest_paths.h"
#include "ortools/graph/test_util.h"

namespace operations_research {
namespace {

using Graph = ::util::StaticGraph<>;

enum Implementation {
  BOUNDED_DIJKSTRA = 1,
  SHORTEST_PATHS = 2,
};

template <Implementation implementation>
std::vector<std::vector<uint32_t>> ManyToManyShortestPaths(
    const Graph& graph, const std::vector<uint32_t>& arc_costs,
    const std::vector<int>& srcs, const std::vector<int>& dsts,
    int num_threads) {
  LOG(FATAL) << "Default implementation of ManyToManyShortestPaths(); use a "
                "specialization [Implementation="
             << implementation << "].";
}

template <Implementation implementation>
std::vector<std::vector<uint32_t>> AllPairsShortestPaths(
    const Graph& graph, const std::vector<uint32_t>& arc_costs,
    int num_threads) {
  std::vector<int> all_nodes(graph.num_nodes(), -1);
  std::iota(all_nodes.begin(), all_nodes.end(), 0);
  return ManyToManyShortestPaths<implementation>(graph, arc_costs, all_nodes,
                                                 all_nodes, num_threads);
}

template <>
std::vector<std::vector<uint32_t>> ManyToManyShortestPaths<BOUNDED_DIJKSTRA>(
    const Graph& graph, const std::vector<uint32_t>& arc_costs,
    const std::vector<int>& srcs, const std::vector<int>& dsts,
    int num_threads) {
  using Dijkstra = BoundedDijkstraWrapper<Graph, uint32_t>;
  Dijkstra base_dijkstra(&graph, &arc_costs);
  std::vector<std::pair<int, uint32_t>> dsts_with_offsets;
  for (const int dst : dsts) {
    dsts_with_offsets.push_back({dst, 0});
  }
  ThreadLocal<Dijkstra> thread_local_dijkstra(base_dijkstra);
  std::vector<std::vector<uint32_t>> distances(
      srcs.size(), std::vector<uint32_t>(dsts.size(), INT_MAX));
  std::vector<int> src_to_src_index(graph.num_nodes(), -1);
  for (int i = 0; i < srcs.size(); ++i) {
    src_to_src_index[srcs[i]] = i;
  }
  std::vector<int> dst_to_dst_index(graph.num_nodes(), -1);
  for (int i = 0; i < dsts.size(); ++i) {
    dst_to_dst_index[dsts[i]] = i;
  }
  // clang-format off
  fiber_auto_design::Parallelizer(num_threads).Apply(
      [&thread_local_dijkstra, &dsts_with_offsets, &src_to_src_index,
       &dst_to_dst_index, &distances](const int* src_ptr) {
        const int src = *src_ptr;
        Dijkstra* const dij = thread_local_dijkstra.pointer();
        for (const int destination :
             dij->RunBoundedDijkstraFromMultipleSourcesToMultipleDestinations(
                 {{src, 0}}, dsts_with_offsets,
                 /*num_destinations_to_reach=*/dsts_with_offsets.size(),
                 /*distance_limit=*/INT_MAX)) {
          distances[src_to_src_index[src]][dst_to_dst_index[destination]] =
              dij->distances()[destination];
        }
      },
      &srcs);
  // clang-format on
  return distances;
}

template <>
std::vector<std::vector<uint32_t>> ManyToManyShortestPaths<SHORTEST_PATHS>(
    const Graph& graph, const std::vector<uint32_t>& arc_costs,
    const std::vector<int>& srcs, const std::vector<int>& dsts,
    int num_threads) {
  auto path_container =
      GenericPathContainer<Graph>::BuildPathDistanceContainer();
  ComputeManyToManyShortestPathsWithMultipleThreads(
      graph, arc_costs, srcs, dsts, num_threads, &path_container);
  std::vector<std::vector<uint32_t>> distances(
      srcs.size(), std::vector<uint32_t>(dsts.size(), INT_MAX));
  std::vector<int> src_to_src_index(graph.num_nodes(), -1);
  for (int i = 0; i < srcs.size(); ++i) {
    src_to_src_index[srcs[i]] = i;
  }
  std::vector<int> dst_to_dst_index(graph.num_nodes(), -1);
  for (int i = 0; i < dsts.size(); ++i) {
    dst_to_dst_index[dsts[i]] = i;
  }
  for (const int src : srcs) {
    for (const int dst : dsts) {
      distances[src_to_src_index[src]][dst_to_dst_index[dst]] =
          path_container.GetDistance(src, dst);
    }
  }
  return distances;
}

template <Implementation implementation>
static void BM_MultiThreadAllPairsOn2DGrid(benchmark::State& state) {
  // Benchmark arguments: grid size and number of threads.
  const int grid_size = state.range(0);
  const int num_threads = state.range(1);

  std::unique_ptr<Graph> graph =
      util::Create2DGridGraph<Graph>(grid_size, grid_size);
  std::vector<uint32_t> arc_costs(graph->num_arcs(), 0);
  std::mt19937 random(12345);
  for (uint32_t& cost : arc_costs) {
    cost = absl::Uniform(random, 0, 100000);
  }
  for (auto _ : state) {
    ::benchmark::DoNotOptimize(
        AllPairsShortestPaths<implementation>(*graph, arc_costs, num_threads));
  }
  // "byte" = pair of nodes for which we computed the shortest path distance.
  state.SetBytesProcessed(state.iterations() * graph->num_nodes() *
                          graph->num_nodes());
}

// NOTE(user): Sadly, RangePair() doesn't give us the range we want, and
// there's no easy way to avoid duplicating the big list of ArgPair().
BENCHMARK(BM_MultiThreadAllPairsOn2DGrid<BOUNDED_DIJKSTRA>)
    ->ArgPair(/*grid_size*/ 8, /*num_threads*/ 1)
    ->ArgPair(/*grid_size*/ 8, /*num_threads*/ 8)
    ->ArgPair(/*grid_size*/ 8, /*num_threads*/ 16)
    ->ArgPair(/*grid_size*/ 16, /*num_threads*/ 1)
    ->ArgPair(/*grid_size*/ 16, /*num_threads*/ 8)
    ->ArgPair(/*grid_size*/ 16, /*num_threads*/ 16)
    ->ArgPair(/*grid_size*/ 64, /*num_threads*/ 1)
    ->ArgPair(/*grid_size*/ 64, /*num_threads*/ 8)
    ->ArgPair(/*grid_size*/ 64, /*num_threads*/ 16)
    // For the larger size, just run with 8 threads: 1 thread is too slow.
    ->ArgPair(/*grid_size*/ 128, /*num_threads*/ 8);
BENCHMARK(BM_MultiThreadAllPairsOn2DGrid<SHORTEST_PATHS>)
    ->ArgPair(/*grid_size*/ 8, /*num_threads*/ 1)
    ->ArgPair(/*grid_size*/ 8, /*num_threads*/ 8)
    ->ArgPair(/*grid_size*/ 8, /*num_threads*/ 16)
    ->ArgPair(/*grid_size*/ 16, /*num_threads*/ 1)
    ->ArgPair(/*grid_size*/ 16, /*num_threads*/ 8)
    ->ArgPair(/*grid_size*/ 16, /*num_threads*/ 16)
    ->ArgPair(/*grid_size*/ 64, /*num_threads*/ 1)
    ->ArgPair(/*grid_size*/ 64, /*num_threads*/ 8)
    ->ArgPair(/*grid_size*/ 64, /*num_threads*/ 16)
    ->ArgPair(/*grid_size*/ 128, /*num_threads*/ 8);

template <Implementation implementation, int num_threads>
static void BM_WindowedAllPairsOn2DGrid(benchmark::State& state) {
  // Benchmark arguments: total grid size and "window" size, i.e. the
  // size of the sub-square within which we'll pick all of our sources and
  // destinations (all nodes in the window).
  const int grid_size = state.range(0);
  const int window_size = state.range(1);

  std::unique_ptr<Graph> graph =
      util::Create2DGridGraph<Graph>(grid_size, grid_size);
  std::vector<uint32_t> arc_costs(graph->num_arcs(), 0);
  std::mt19937 random(12345);
  for (uint32_t& cost : arc_costs) {
    cost = absl::Uniform(random, 0, 100000);
  }
  std::vector<int> window_nodes;
  const int window_start = (grid_size - window_size) / 2;
  const int window_end = window_start + window_size;
  for (int i = window_start; i < window_end; ++i) {
    for (int j = window_start; j < window_end; ++j) {
      window_nodes.push_back(i * grid_size + j);
    }
  }
  for (auto _ : state) {
    ::benchmark::DoNotOptimize(ManyToManyShortestPaths<implementation>(
        *graph, arc_costs, window_nodes, window_nodes, num_threads));
  }
  // "byte" = pair of nodes for which we computed the shortest path distance.
  state.SetBytesProcessed(state.iterations() * window_size * window_size);
}

BENCHMARK(BM_WindowedAllPairsOn2DGrid<BOUNDED_DIJKSTRA, 1>)
    ->ArgPair(100, 10)
    ->ArgPair(1000, 10)
    ->ArgPair(500, 50);
BENCHMARK(BM_WindowedAllPairsOn2DGrid<SHORTEST_PATHS, 1>)
    ->ArgPair(100, 10)
    ->ArgPair(1000, 10)
    ->ArgPair(500, 50);
BENCHMARK(BM_WindowedAllPairsOn2DGrid<BOUNDED_DIJKSTRA, 8>)
    ->ArgPair(100, 10)
    ->ArgPair(1000, 10)
    ->ArgPair(500, 50);
BENCHMARK(BM_WindowedAllPairsOn2DGrid<SHORTEST_PATHS, 8>)
    ->ArgPair(100, 10)
    ->ArgPair(1000, 10)
    ->ArgPair(500, 50);
BENCHMARK(BM_WindowedAllPairsOn2DGrid<BOUNDED_DIJKSTRA, 16>)
    ->ArgPair(100, 10)
    ->ArgPair(1000, 10)
    ->ArgPair(500, 50);
BENCHMARK(BM_WindowedAllPairsOn2DGrid<SHORTEST_PATHS, 16>)
    ->ArgPair(100, 10)
    ->ArgPair(1000, 10)
    ->ArgPair(500, 50);

}  // namespace
}  // namespace operations_research
