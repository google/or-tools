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
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/log/check.h"
#include "absl/random/random.h"
#include "absl/types/span.h"
#include "benchmark/benchmark.h"
#include "ortools/graph/dag_shortest_path.h"
#include "ortools/graph_base/graph.h"

namespace operations_research {
namespace {

// Builds a random DAG with a given number of nodes and arcs where 0 is always
// the first and num_nodes-1 the last element in the topological order. Note
// that the graph always include at least one arc from 0 to num_nodes-1.
std::pair<std::unique_ptr<const util::StaticGraph<>>, std::vector<int>>
BuildRandomDag(const int64_t num_nodes, const int64_t num_arcs) {
  absl::BitGen bit_gen;
  CHECK_GE(num_nodes, 2);
  CHECK_GE(num_arcs, 1);
  CHECK_LE(num_arcs, (num_nodes * (num_nodes - 1)) / 2);
  std::vector<int> topological_order(num_nodes);
  topological_order.back() = num_nodes - 1;
  absl::Span<int> non_start_end =
      absl::MakeSpan(topological_order).subspan(1, num_nodes - 2);
  absl::c_iota(non_start_end, 1);
  absl::c_shuffle(non_start_end, bit_gen);
  int edges_added = 0;
  util::StaticGraph<>::Builder builder(num_nodes, num_arcs);
  builder.AddArc(0, num_nodes - 1);
  while (edges_added < num_arcs - 1) {
    int start_index = absl::Uniform(bit_gen, 0, num_nodes - 1);
    int end_index = absl::Uniform(bit_gen, start_index + 1, num_nodes);
    builder.AddArc(topological_order[start_index],
                   topological_order[end_index]);
    edges_added++;
  }
  return {std::move(builder).Build(nullptr), topological_order};
}

// The length of each arc is drawn uniformly at random within a given interval
// except if the first arc from 0 to num_nodes-1 where it is set to a large
// length.
std::vector<double> GenerateRandomLengths(const util::StaticGraph<>& graph,
                                          const double min_length = 0.0,
                                          const double max_length = 10.0,
                                          const double large_length = 10000.0) {
  absl::BitGen bit_gen;
  std::vector<double> arc_lengths;
  arc_lengths.reserve(graph.num_arcs());
  bool large_length_set = false;
  for (util::StaticGraph<>::ArcIndex arc = 0; arc < graph.num_arcs(); ++arc) {
    if (!large_length_set && graph.Tail(arc) == 0 &&
        graph.Head(arc) == graph.num_nodes() - 1) {
      arc_lengths.push_back(large_length);
      large_length_set = true;
      continue;
    }
    arc_lengths.push_back(static_cast<double>(
        absl::Uniform<int>(bit_gen, min_length, max_length)));
  }
  return arc_lengths;
}

void BM_RandomDag(benchmark::State& state) {
  absl::BitGen bit_gen;
  // Generate a fixed random DAG.
  const int num_nodes = state.range(0);
  const int num_arcs = num_nodes * state.range(1);
  const auto [graph, topological_order] = BuildRandomDag(num_nodes, num_arcs);
  // Generate 10 scenarios of random arc lengths.
  const int num_scenarios = 10;
  std::vector<std::vector<double>> arc_lengths_scenarios;
  for (int _ = 0; _ < num_scenarios; ++_) {
    arc_lengths_scenarios.push_back(GenerateRandomLengths(*graph));
  }
  std::vector<double> arc_lengths = arc_lengths_scenarios.front();
  ShortestPathsOnDagWrapper<util::StaticGraph<>> shortest_path_on_dag(
      graph.get(), &arc_lengths, topological_order);
  for (auto _ : state) {
    // Pick a arc lengths scenario at random.
    arc_lengths =
        arc_lengths_scenarios[absl::Uniform(bit_gen, 0, num_scenarios)];
    shortest_path_on_dag.RunShortestPathOnDag({0});
    CHECK(shortest_path_on_dag.IsReachable(num_nodes - 1));
    const double minimum_length = shortest_path_on_dag.LengthTo(num_nodes - 1);
    CHECK_GE(minimum_length, 0.0);
    CHECK_LE(minimum_length, 10000.0);
  }
  state.SetItemsProcessed(state.iterations() * (num_nodes + num_arcs));
}

BENCHMARK(BM_RandomDag)
    ->ArgPair(1000, 10)
    ->ArgPair(1 << 16, 4)
    ->ArgPair(1 << 16, 16)
    ->ArgPair(1 << 22, 4)
    ->ArgPair(1 << 22, 16)
    ->ArgPair(1 << 26, 4);  // Don't go bigger, can't run on work station.

void BM_LineDag(benchmark::State& state) {
  const int num_nodes = state.range(0);
  const int num_edges = num_nodes - 1;
  std::vector<int> topological_order(num_nodes);
  util::StaticGraph<>::Builder builder(num_nodes, num_edges);
  absl::c_iota(topological_order, 0);
  for (int i = 0; i < num_nodes - 1; ++i) {
    builder.AddArc(i, i + 1);
  }
  const auto graph = std::move(builder).Build(nullptr);
  std::vector<double> arc_lengths(num_edges, 1);
  ShortestPathsOnDagWrapper<util::StaticGraph<>> shortest_path_on_dag(
      graph.get(), &arc_lengths, topological_order);
  for (auto _ : state) {
    shortest_path_on_dag.RunShortestPathOnDag({0});
    CHECK(shortest_path_on_dag.IsReachable(num_nodes - 1));
    CHECK_EQ(shortest_path_on_dag.LengthTo(num_nodes - 1), num_nodes - 1);
    CHECK_EQ(shortest_path_on_dag.ArcPathTo(num_nodes - 1).size(),
             num_nodes - 1);
    CHECK_EQ(shortest_path_on_dag.NodePathTo(num_nodes - 1).size(), num_nodes);
  }
  state.SetItemsProcessed(state.iterations() * num_nodes);
}

BENCHMARK(BM_LineDag)->Arg(1 << 16)->Arg(1 << 22)->Arg(1 << 24);

void BM_RandomDag_K(benchmark::State& state) {
  absl::BitGen bit_gen;
  // Generate a fixed random DAG.
  const int num_nodes = state.range(0);
  const int num_arcs = num_nodes * state.range(1);
  const int path_count = state.range(2);
  const auto [graph, topological_order] = BuildRandomDag(num_nodes, num_arcs);
  // Generate 10 scenarios of random arc lengths.
  const int num_scenarios = 10;
  std::vector<std::vector<double>> arc_lengths_scenarios;
  for (int _ = 0; _ < num_scenarios; ++_) {
    arc_lengths_scenarios.push_back(GenerateRandomLengths(*graph));
  }
  std::vector<double> arc_lengths = arc_lengths_scenarios.front();
  KShortestPathsOnDagWrapper<util::StaticGraph<>> shortest_paths_on_dag(
      graph.get(), &arc_lengths, topological_order, path_count);
  for (auto _ : state) {
    // Pick a arc lengths scenario at random.
    arc_lengths =
        arc_lengths_scenarios[absl::Uniform(bit_gen, 0, num_scenarios)];
    shortest_paths_on_dag.RunKShortestPathOnDag({0});
    CHECK(shortest_paths_on_dag.IsReachable(num_nodes - 1));
    const std::vector<double> lengths =
        shortest_paths_on_dag.LengthsTo(num_nodes - 1);
    CHECK_GE(lengths[0], 0.0);
    CHECK_LE(lengths[0], 10000.0);
  }
  state.SetItemsProcessed(state.iterations() * (num_nodes + num_arcs));
}

BENCHMARK(BM_RandomDag_K)
    ->Args({1000, 10, 4})
    ->Args({1 << 16, 4, 4})
    ->Args({1 << 16, 4, 16})
    ->Args({1 << 16, 16, 4})
    ->Args({1 << 16, 16, 16})
    ->Args({1 << 22, 4, 4})
    ->Args({1 << 22, 4, 16});

}  // namespace
}  // namespace operations_research
