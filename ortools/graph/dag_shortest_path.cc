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

#include "ortools/graph/dag_shortest_path.h"

#include <limits>
#include <vector>

#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/graph/graph.h"
#include "ortools/graph/topologicalsorter.h"

namespace operations_research {

PathWithLength ShortestPathsOnDag(
    const int num_nodes, absl::Span<const ArcWithLength> arcs_with_length,
    int source, int destination) {
  using GraphType = util::StaticGraph<>;
  using NodeIndex = GraphType::NodeIndex;
  using ArcIndex = GraphType::ArcIndex;

  GraphType graph(num_nodes, arcs_with_length.size());
  std::vector<double> arc_lengths;
  arc_lengths.reserve(arcs_with_length.size());
  for (const auto& arc : arcs_with_length) {
    graph.AddArc(arc.tail, arc.head);
    arc_lengths.push_back(arc.length);
  }
  std::vector<ArcIndex> permutation;
  graph.Build(&permutation);
  util::Permute(permutation, &arc_lengths);

  std::vector<ArcIndex> inverse_permutation(permutation.size());
  if (!permutation.empty()) {
    for (ArcIndex i = 0; i < permutation.size(); ++i) {
      inverse_permutation[permutation[i]] = i;
    }
  }

  const absl::StatusOr<std::vector<NodeIndex>> topological_order =
      util::graph::FastTopologicalSort(graph);
  CHECK_OK(topological_order) << "arcs_with_length form a cycle.";

  ShortestPathsOnDagWrapper<util::StaticGraph<>> shortest_path_on_dag(
      &graph, &arc_lengths, *topological_order);

  shortest_path_on_dag.RunShortestPathOnDag({source});

  if (!shortest_path_on_dag.IsReachable(destination)) {
    return PathWithLength{.length = std::numeric_limits<double>::infinity(),
                          .arc_path = std::vector<int>(),
                          .node_path = std::vector<int>()};
  }

  std::vector<int> arc_path = shortest_path_on_dag.ArcPathTo(destination);
  if (!inverse_permutation.empty()) {
    for (int i = 0; i < arc_path.size(); ++i) {
      arc_path[i] = inverse_permutation[arc_path[i]];
    }
  }
  return PathWithLength{
      .length = shortest_path_on_dag.LengthTo(destination),
      .arc_path = arc_path,
      .node_path = shortest_path_on_dag.NodePathTo(destination)};
}

}  // namespace operations_research
