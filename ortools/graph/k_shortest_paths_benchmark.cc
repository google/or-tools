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
#include <memory>
#include <random>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/random/distributions.h"
#include "benchmark/benchmark.h"
#include "ortools/graph/k_shortest_paths.h"
#include "ortools/graph/shortest_paths.h"
#include "ortools/graph_base/graph.h"

namespace operations_research {
namespace {

using util::StaticGraph;

namespace internal {

template <typename Graph, typename NodeIndexType, typename ArcIndexType,
          typename URBG, bool IsDirected>
std::unique_ptr<Graph> GenerateUniformGraph(URBG&& urbg,
                                            const NodeIndexType num_nodes,
                                            const ArcIndexType num_edges) {
  // TODO(user): make these utility functions so they can be reused.
  const auto pick_one_node = [&urbg, num_nodes]() -> NodeIndexType {
    const NodeIndexType node = absl::Uniform(urbg, 0, num_nodes);
    CHECK_GE(node, 0);
    CHECK_LT(node, num_nodes);
    return node;
  };
  const auto pick_two_distinct_nodes =
      [&pick_one_node]() -> std::pair<NodeIndexType, NodeIndexType> {
    const NodeIndexType src = pick_one_node();
    NodeIndexType dst;
    do {
      dst = pick_one_node();
    } while (src == dst);
    CHECK_NE(src, dst);
    return {src, dst};
  };

  // Determine the maximum number of arcs in the graph.
  const ArcIndexType max_num_arcs = IsDirected
                                        ? (num_nodes * (num_nodes - 1))
                                        : (num_nodes * (num_nodes - 1)) / 2;

  // Build a random graph (and not multigraph) with `num_arcs` or `max_num_arcs`
  // arcs, whichever is lower. The set is useful to ensure the graph does not
  // contain the same arc more than once (the result would be a multigraph).
  // TODO(user): this is an awful way to generate a complete graph.
  StaticGraph<>::Builder builder;
  builder.AddNode(num_nodes - 1);

  std::set<std::pair<NodeIndexType, NodeIndexType>> arcs;
  for (ArcIndexType i = 0; i < std::min(num_edges, max_num_arcs); ++i) {
    NodeIndexType src, dst;
    std::tie(src, dst) = pick_two_distinct_nodes();
    if (arcs.find({src, dst}) != arcs.end()) continue;
    if (IsDirected && (arcs.find({dst, src}) != arcs.end())) continue;

    arcs.insert({src, dst});
    builder.AddArc(src, dst);

    if (IsDirected) {
      arcs.insert({dst, src});
      builder.AddArc(dst, src);
    }
  }

  // No need to keep the permutation when building, as there are no associated
  // attributes such as lengths in this function.
  return std::move(builder).Build(nullptr);
}

}  // namespace internal

template <typename NodeIndexType, typename ArcIndexType,
          typename Graph = StaticGraph<NodeIndexType, ArcIndexType>,
          typename URBG>
std::unique_ptr<Graph> GenerateUniformDirectedGraph(
    URBG&& urbg, const NodeIndexType num_nodes, const ArcIndexType num_arcs) {
  return internal::GenerateUniformGraph<Graph, NodeIndexType, ArcIndexType,
                                        URBG, /*IsDirected=*/true>(
      urbg, num_nodes, num_arcs);
}

void BM_Yen(benchmark::State& state) {
  const int num_nodes = state.range(0);
  // Use half the maximum number of arcs, so that the graph is a bit sparse.
  const int num_arcs = num_nodes * (num_nodes - 1) / 4;
  // TODO(user): when supported, also benchmark negative weights
  // (separately?).
  constexpr int kMinLength = 0;
  constexpr int kMaxLength = 1'000;

  std::mt19937 random(12345);
  const auto pick_one_node = [&random, num_nodes]() -> int {
    int node = absl::Uniform(random, 0, num_nodes);
    CHECK_GE(node, 0);
    CHECK_LT(node, num_nodes);
    return node;
  };
  const auto pick_two_distinct_nodes =
      [&pick_one_node]() -> std::pair<int, int> {
    int src = pick_one_node();
    int dst;
    do {
      dst = pick_one_node();
    } while (src == dst);
    CHECK_NE(src, dst);
    return {src, dst};
  };

  const auto graph = GenerateUniformDirectedGraph(random, num_nodes, num_arcs);
  std::vector<PathDistance> lengths;
  for (int i = 0; i < graph->num_arcs(); ++i) {
    lengths.push_back(absl::Uniform(random, kMinLength, kMaxLength));
  }

  for (auto unused : state) {
    int src, dst;
    std::tie(src, dst) = pick_two_distinct_nodes();
    YenKShortestPaths(*graph, lengths, src, dst, /*k=*/10);
  }
}

BENCHMARK(BM_Yen)->Range(10, 1'000);

}  // namespace
}  // namespace operations_research
