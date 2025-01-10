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

#include "ortools/graph/dag_constrained_shortest_path.h"

#include <vector>

#include "absl/log/check.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "ortools/graph/dag_shortest_path.h"
#include "ortools/graph/graph.h"
#include "ortools/graph/topologicalsorter.h"

namespace operations_research {

namespace {

void ApplyMapping(absl::Span<const int> mapping, std::vector<int>& values) {
  if (!mapping.empty()) {
    for (int i = 0; i < values.size(); ++i) {
      values[i] = mapping[values[i]];
    }
  }
}

}  // namespace

PathWithLength ConstrainedShortestPathsOnDag(
    const int num_nodes,
    absl::Span<const ArcWithLengthAndResources> arcs_with_length_and_resources,
    int source, int destination, const std::vector<double>& max_resources) {
  using GraphType = util::StaticGraph<>;
  using NodeIndex = GraphType::NodeIndex;
  using ArcIndex = GraphType::ArcIndex;

  const int num_arcs = arcs_with_length_and_resources.size();
  GraphType graph(num_nodes, num_arcs);
  std::vector<double> arc_lengths;
  arc_lengths.reserve(num_arcs);
  std::vector<std::vector<double>> arc_resources(max_resources.size());
  for (int i = 0; i < max_resources.size(); ++i) {
    arc_resources[i].reserve(num_arcs);
  }
  for (const auto& arc : arcs_with_length_and_resources) {
    graph.AddArc(arc.from, arc.to);
    arc_lengths.push_back(arc.length);
    for (int i = 0; i < arc.resources.size(); ++i) {
      arc_resources[i].push_back(arc.resources[i]);
    }
  }

  std::vector<ArcIndex> permutation;
  graph.Build(&permutation);
  util::Permute(permutation, &arc_lengths);
  for (int i = 0; i < max_resources.size(); ++i) {
    util::Permute(permutation, &arc_resources[i]);
  }

  std::vector<ArcIndex> inverse_permutation =
      GetInversePermutation(permutation);

  const absl::StatusOr<std::vector<NodeIndex>> topological_order =
      util::graph::FastTopologicalSort(graph);
  CHECK_OK(topological_order) << "arcs_with_length form a cycle.";

  std::vector<NodeIndex> sources = {source};
  std::vector<NodeIndex> destinations = {destination};
  ConstrainedShortestPathsOnDagWrapper<util::StaticGraph<>>
      constrained_shortest_path_on_dag(&graph, &arc_lengths, &arc_resources,
                                       *topological_order, sources,
                                       destinations, &max_resources);

  PathWithLength path_with_length =
      constrained_shortest_path_on_dag.RunConstrainedShortestPathOnDag();

  ApplyMapping(inverse_permutation, path_with_length.arc_path);

  return path_with_length;
}

std::vector<int> GetInversePermutation(absl::Span<const int> permutation) {
  std::vector<int> inverse_permutation(permutation.size());
  if (!permutation.empty()) {
    for (int i = 0; i < permutation.size(); ++i) {
      inverse_permutation[permutation[i]] = i;
    }
  }
  return inverse_permutation;
}

}  // namespace operations_research
