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

#include "ortools/graph/dag_shortest_path.h"

#include <limits>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/graph/graph.h"
#include "ortools/graph/topologicalsorter.h"

namespace operations_research {

namespace {

using GraphType = util::StaticGraph<>;
using NodeIndex = GraphType::NodeIndex;
using ArcIndex = GraphType::ArcIndex;

struct ShortestPathOnDagProblem {
  GraphType graph;
  std::vector<double> arc_lengths;
  std::vector<ArcIndex> original_arc_indices;
  std::vector<NodeIndex> topological_order;
};

ShortestPathOnDagProblem ReadProblem(
    const int num_nodes, absl::Span<const ArcWithLength> arcs_with_length) {
  GraphType graph(num_nodes, arcs_with_length.size());
  std::vector<double> arc_lengths;
  arc_lengths.reserve(arcs_with_length.size());
  for (const auto& arc : arcs_with_length) {
    graph.AddArc(arc.from, arc.to);
    arc_lengths.push_back(arc.length);
  }
  std::vector<ArcIndex> permutation;
  graph.Build(&permutation);
  util::Permute(permutation, &arc_lengths);

  std::vector<ArcIndex> original_arc_indices(permutation.size());
  if (!permutation.empty()) {
    for (ArcIndex i = 0; i < permutation.size(); ++i) {
      original_arc_indices[permutation[i]] = i;
    }
  }

  absl::StatusOr<std::vector<NodeIndex>> topological_order =
      util::graph::FastTopologicalSort(graph);
  CHECK_OK(topological_order) << "arcs_with_length form a cycle.";

  return ShortestPathOnDagProblem{
      .graph = std::move(graph),
      .arc_lengths = std::move(arc_lengths),
      .original_arc_indices = std::move(original_arc_indices),
      .topological_order = std::move(topological_order).value()};
}

void GetOriginalArcPath(absl::Span<const ArcIndex> original_arc_indices,
                        std::vector<ArcIndex>& arc_path) {
  if (original_arc_indices.empty()) {
    return;
  }
  for (int i = 0; i < arc_path.size(); ++i) {
    arc_path[i] = original_arc_indices[arc_path[i]];
  }
}

}  // namespace

PathWithLength ShortestPathsOnDag(
    const int num_nodes, absl::Span<const ArcWithLength> arcs_with_length,
    const int source, const int destination) {
  const ShortestPathOnDagProblem problem =
      ReadProblem(num_nodes, arcs_with_length);

  ShortestPathsOnDagWrapper<util::StaticGraph<>> shortest_path_on_dag(
      &problem.graph, &problem.arc_lengths, problem.topological_order);
  shortest_path_on_dag.RunShortestPathOnDag({source});

  if (!shortest_path_on_dag.IsReachable(destination)) {
    return PathWithLength{.length = std::numeric_limits<double>::infinity()};
  }

  std::vector<int> arc_path = shortest_path_on_dag.ArcPathTo(destination);
  GetOriginalArcPath(problem.original_arc_indices, arc_path);
  return PathWithLength{
      .length = shortest_path_on_dag.LengthTo(destination),
      .arc_path = std::move(arc_path),
      .node_path = shortest_path_on_dag.NodePathTo(destination)};
}

std::vector<PathWithLength> KShortestPathsOnDag(
    const int num_nodes, absl::Span<const ArcWithLength> arcs_with_length,
    const int source, const int destination, const int path_count) {
  const ShortestPathOnDagProblem problem =
      ReadProblem(num_nodes, arcs_with_length);

  KShortestPathsOnDagWrapper<GraphType> shortest_paths_on_dag(
      &problem.graph, &problem.arc_lengths, problem.topological_order,
      path_count);
  shortest_paths_on_dag.RunKShortestPathOnDag({source});

  if (!shortest_paths_on_dag.IsReachable(destination)) {
    return {PathWithLength{.length = std::numeric_limits<double>::infinity()}};
  }

  std::vector<double> lengths = shortest_paths_on_dag.LengthsTo(destination);
  std::vector<std::vector<GraphType::ArcIndex>> arc_paths =
      shortest_paths_on_dag.ArcPathsTo(destination);
  std::vector<std::vector<GraphType::NodeIndex>> node_paths =
      shortest_paths_on_dag.NodePathsTo(destination);
  std::vector<PathWithLength> paths;
  paths.reserve(lengths.size());
  for (int k = 0; k < lengths.size(); ++k) {
    GetOriginalArcPath(problem.original_arc_indices, arc_paths[k]);
    paths.push_back(PathWithLength{.length = lengths[k],
                                   .arc_path = std::move(arc_paths[k]),
                                   .node_path = std::move(node_paths[k])});
  }
  return paths;
}

}  // namespace operations_research
