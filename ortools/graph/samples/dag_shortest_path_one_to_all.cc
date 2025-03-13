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
#include <iostream>
#include <vector>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/strings/str_join.h"
#include "ortools/base/init_google.h"
#include "ortools/base/status_macros.h"
#include "ortools/graph/dag_shortest_path.h"
#include "ortools/graph/graph.h"
#include "ortools/graph/topologicalsorter.h"

namespace {

absl::Status Main() {
  util::StaticGraph<> graph;
  std::vector<double> weights;
  graph.AddArc(0, 2);
  weights.push_back(5.0);
  graph.AddArc(0, 3);
  weights.push_back(4.0);
  graph.AddArc(1, 3);
  weights.push_back(1.0);
  graph.AddArc(2, 4);
  weights.push_back(-3.0);
  graph.AddArc(3, 4);
  weights.push_back(0.0);

  // Static graph reorders the arcs at Build() time, use permutation to get
  // from the old ordering to the new one.
  std::vector<int32_t> permutation;
  graph.Build(&permutation);
  util::Permute(permutation, &weights);

  // We need a topological order. We can find it by hand on this small graph,
  // e.g., {0, 1, 2, 3, 4}, but we demonstrate how to compute one instead.
  ASSIGN_OR_RETURN(const std::vector<int32_t> topological_order,
                   util::graph::FastTopologicalSort(graph));

  operations_research::ShortestPathsOnDagWrapper<util::StaticGraph<>>
      shortest_path_on_dag(&graph, &weights, topological_order);
  const int source = 0;
  shortest_path_on_dag.RunShortestPathOnDag({source});

  // For each node other than 0, print its distance and the shortest path.
  for (int i = 1; i < 5; ++i) {
    if (shortest_path_on_dag.IsReachable(i)) {
      std::cout << "Length of shortest path to node " << i << ": "
                << shortest_path_on_dag.LengthTo(i) << std::endl;
      std::cout << "Shortest path to node " << i << ": "
                << absl::StrJoin(shortest_path_on_dag.NodePathTo(i), ", ")
                << std::endl;

    } else {
      std::cout << "No path to node: " << i << std::endl;
    }
  }
  return absl::OkStatus();
}

}  // namespace

int main(int argc, char** argv) {
  InitGoogle(argv[0], &argc, &argv, true);
  QCHECK_OK(Main());
  return 0;
}
