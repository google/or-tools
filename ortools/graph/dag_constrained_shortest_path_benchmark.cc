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

#include <algorithm>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/log/check.h"
#include "absl/random/distributions.h"
#include "absl/random/random.h"
#include "absl/types/span.h"
#include "benchmark/benchmark.h"
#include "ortools/graph/dag_constrained_shortest_path.h"
#include "ortools/graph_base/graph.h"

namespace operations_research {
namespace {

// Builds a random DAG with a given number of nodes and arcs where 0 is always
// the first and num_nodes-1 the last element in the topological order. Note
// that the graph always include at least one arc from 0 to num_nodes-1.
template <typename NodeIndex = int32_t, typename ArcIndex = int32_t>
std::pair<std::unique_ptr<util::StaticGraph<NodeIndex, ArcIndex>>,
          std::vector<NodeIndex>>
BuildRandomDag(const NodeIndex num_nodes, const ArcIndex num_arcs) {
  absl::BitGen bit_gen;
  CHECK_GE(num_nodes, 2);
  CHECK_GE(num_arcs, 1);
  CHECK_LE(num_arcs, (num_nodes * (num_nodes - 1)) / 2);
  std::vector<NodeIndex> topological_order(num_nodes);
  topological_order.back() = num_nodes - 1;
  absl::Span<NodeIndex> non_start_end =
      absl::MakeSpan(topological_order).subspan(1, num_nodes - 2);
  absl::c_iota(non_start_end, 1);
  absl::c_shuffle(non_start_end, bit_gen);
  ArcIndex edges_added = 0;
  typename util::StaticGraph<NodeIndex, ArcIndex>::Builder builder(num_nodes,
                                                                   num_arcs);
  builder.AddArc(0, num_nodes - 1);
  while (edges_added < num_arcs - 1) {
    NodeIndex start_index = absl::Uniform(bit_gen, 0, num_nodes - 1);
    NodeIndex end_index = absl::Uniform(bit_gen, start_index + 1, num_nodes);
    builder.AddArc(topological_order[start_index],
                   topological_order[end_index]);
    edges_added++;
  }
  auto graph = std::move(builder).Build(nullptr);
  return {std::move(graph), topological_order};
}

// The length of each arc is drawn uniformly at random within a given interval
// except if the first arc from 0 to num_nodes-1 where it is set to
// `start_to_end_value`.
template <class GraphType>
std::vector<double> GenerateRandomIntegerValues(
    const GraphType& graph, const double min_value = 0.0,
    const double max_value = 10.0, const double start_to_end_value = 10000.0) {
  absl::BitGen bit_gen;
  std::vector<double> arc_values;
  arc_values.reserve(graph.num_arcs());
  bool start_to_end_value_set = false;
  for (typename GraphType::ArcIndex arc = 0; arc < graph.num_arcs(); ++arc) {
    if (!start_to_end_value_set && graph.Tail(arc) == 0 &&
        graph.Head(arc) == graph.num_nodes() - 1) {
      arc_values.push_back(start_to_end_value);
      start_to_end_value_set = true;
      continue;
    }
    arc_values.push_back(
        static_cast<double>(absl::Uniform<int>(bit_gen, min_value, max_value)));
  }
  return arc_values;
}

template <typename NodeIndex = int32_t, typename ArcIndex = int32_t>
void BM_RandomDag(benchmark::State& state) {
  absl::BitGen bit_gen;
  // Generate a fixed random DAG.
  const NodeIndex num_nodes = state.range(0);
  const ArcIndex num_arcs = num_nodes * state.range(1);
  const auto [graph, topological_order] = BuildRandomDag(num_nodes, num_arcs);
  // Generate 20 scenarios of random arc lengths.
  const int num_scenarios = 20;
  std::vector<std::vector<double>> arc_lengths_scenarios;
  for (int _ = 0; _ < num_scenarios; ++_) {
    arc_lengths_scenarios.push_back(GenerateRandomIntegerValues(*graph));
  }
  std::vector<double> arc_lengths(num_arcs);
  std::vector<std::vector<double>> arc_resources(1);
  arc_resources[0] =
      GenerateRandomIntegerValues(*graph, /*min_value=*/1.0,
                                  /*max_value=*/10.0,
                                  /*start_to_end_value=*/num_nodes * 0.2);
  const std::vector<NodeIndex> sources = {0};
  const std::vector<NodeIndex> destinations = {num_nodes - 1};
  const std::vector<double> max_resources = {num_nodes * 0.2};
  ConstrainedShortestPathsOnDagWrapper<util::StaticGraph<NodeIndex, ArcIndex>>
      constrained_shortest_path_on_dag(graph.get(), &arc_lengths,
                                       &arc_resources, topological_order,
                                       sources, destinations, &max_resources);

  int total_label_count = 0;
  for (auto _ : state) {
    // Pick a arc lengths scenario at random.
    arc_lengths =
        arc_lengths_scenarios[absl::Uniform(bit_gen, 0, num_scenarios)];
    const GraphPathWithLength<util::StaticGraph<NodeIndex, ArcIndex>>
        path_with_length =
            constrained_shortest_path_on_dag.RunConstrainedShortestPathOnDag();
    total_label_count += constrained_shortest_path_on_dag.label_count();
    CHECK_GE(path_with_length.length, 0.0);
    CHECK_LE(path_with_length.length, 10000.0);
  }
  state.SetItemsProcessed(std::max(1, total_label_count));
}

BENCHMARK(BM_RandomDag<int32_t, int32_t>)
    ->ArgPair(1 << 10, 16)
    ->ArgPair(1 << 16, 4)
    ->ArgPair(1 << 16, 16)
    ->ArgPair(1 << 19, 4);
BENCHMARK(BM_RandomDag<int64_t, int64_t>)
    ->ArgPair(1 << 19, 16)
    ->ArgPair(1 << 22, 4);

// Generate a 2-dimensional grid DAG.
// Eg. for width=3, height=2, it generates this:
// 0 ----> 1 ----> 2
// |       |       |
// |       |       |
// v       v       v
// 3 ----> 4 ----> 5
void BM_GridDAG(benchmark::State& state) {
  const int64_t width = state.range(0);
  const int64_t height = state.range(1);
  const int num_resources = state.range(2);
  const int num_nodes = width * height;
  const int num_arcs = 2 * num_nodes - width - height;
  util::StaticGraph<>::Builder builder(num_nodes, num_arcs);
  // Add horizontal edges.
  for (int i = 0; i < height; ++i) {
    for (int j = 1; j < width; ++j) {
      const int left = i * width + (j - 1);
      const int right = i * width + j;
      builder.AddArc(left, right);
    }
  }
  // Add vertical edges.
  for (int i = 1; i < height; ++i) {
    for (int j = 0; j < width; ++j) {
      const int up = (i - 1) * width + j;
      const int down = i * width + j;
      builder.AddArc(up, down);
    }
  }
  auto graph = std::move(builder).Build(nullptr);
  std::vector<int> topological_order(num_nodes);
  absl::c_iota(topological_order, 0);

  // Generate 20 scenarios of random arc lengths.
  absl::BitGen bit_gen;
  const int kNumScenarios = 20;
  std::vector<std::vector<double>> arc_lengths_scenarios;
  for (int unused = 0; unused < kNumScenarios; ++unused) {
    std::vector<double> arc_lengths(graph->num_arcs());
    for (int i = 0; i < graph->num_arcs(); ++i) {
      arc_lengths[i] = absl::Uniform<double>(bit_gen, 0, 1);
    }
    arc_lengths_scenarios.push_back(arc_lengths);
  }

  std::vector<std::vector<double>> arc_resources(num_resources);
  for (int r = 0; r < num_resources; ++r) {
    arc_resources[r].resize(graph->num_arcs());
    for (int i = 0; i < graph->num_arcs(); ++i) {
      arc_resources[r][i] = absl::Uniform<double>(bit_gen, 0, 1);
    }
  }

  std::vector<double> arc_lengths(num_arcs);
  const std::vector<int> sources = {0};
  const std::vector<int> destinations = {num_nodes - 1};
  std::vector<double> max_resources(num_resources);
  // Each path from source to destination has `(width + height - 2)` arcs. Each
  // arc has mean resource(s) 0.5. We want to consider paths with half (0.5) the
  // mean resource(s).
  const double max_resource = (width + height - 2) * 0.5 * 0.5;
  for (int r = 0; r < num_resources; ++r) {
    max_resources[r] = max_resource;
  }

  ConstrainedShortestPathsOnDagWrapper<util::StaticGraph<>>
      constrained_shortest_path_on_dag(graph.get(), &arc_lengths,
                                       &arc_resources, topological_order,
                                       sources, destinations, &max_resources);

  int total_label_count = 0;
  for (auto _ : state) {
    // Pick a arc lengths scenario at random.
    arc_lengths =
        arc_lengths_scenarios[absl::Uniform<int>(bit_gen, 0, kNumScenarios)];
    const GraphPathWithLength<util::StaticGraph<>> path_with_length =
        constrained_shortest_path_on_dag.RunConstrainedShortestPathOnDag();
    total_label_count += constrained_shortest_path_on_dag.label_count();
    CHECK_GE(path_with_length.length, 0.0);
  }
  state.SetItemsProcessed(std::max(1, total_label_count));
}

BENCHMARK(BM_GridDAG)
    ->Args({100, 100, 1})
    ->Args({100, 100, 2})
    ->Args({1000, 100, 1})
    ->Args({1000, 100, 2});

}  // namespace
}  // namespace operations_research
