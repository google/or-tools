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

// [START imports]
#include <cstdint>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "ortools/base/init_google.h"
#include "ortools/graph/dag_constrained_shortest_path.h"
#include "ortools/graph/graph.h"
// [END imports]

int main(int argc, char** argv) {
  InitGoogle(argv[0], &argc, &argv, true);

  // [START graph]
  // Create a graph with n + 2 nodes, indexed from 0:
  //   * Node n is `source`
  //   * Node n+1 is `dest`
  //   * Nodes M = [0, 1, ..., n-1]  are in the middle.
  //
  // There is a single resource constraints with limit 1.
  //
  // The graph has 3 * n - 1 arcs (with weights and both resources):
  //   * (source -> i) with weight 100 and no resource use for i in M
  //   * (i -> dest) with weight 100 and no resource use for i in M
  //   * (i -> (i+1)) with weight 1 and resource use of 1 for i = 0, ..., n-2
  //
  // Every path [source, i, dest] for i in M is a constrained shortest path from
  // source to dest with weight 200.
  const int n = 5;
  const int source = n;
  const int dest = n + 1;
  const int num_arcs = 3 * n - 1;
  util::StaticGraph<> graph;
  // There are 3 types of arcs: (1) source to M, (2) M to dest, and (3) within
  // M. This vector stores all of them, first of type (1), then type (2),
  // then type (3). The arcs are ordered by i in M within each type.
  std::vector<double> weights(num_arcs);
  // Resources are first indexed by resource, then by arc.
  std::vector<std::vector<double>> resources(1, std::vector<double>(num_arcs));

  for (int i = 0; i < n; ++i) {
    graph.AddArc(source, i);
    weights[i] = 100.0;
    resources[0][i] = 0.0;
  }
  for (int i = 0; i < n; ++i) {
    graph.AddArc(i, dest);
    weights[n + i] = 100.0;
    resources[0][n + i] = 0.0;
  }
  for (int i = 0; i + 1 < n; ++i) {
    graph.AddArc(i, i + 1);
    weights[2 * n + i] = 1.0;
    resources[0][2 * n + i] = 1.0;
  }

  // Static graph reorders the arcs at Build() time, use permutation to get from
  // the old ordering to the new one.
  std::vector<int32_t> permutation;
  graph.Build(&permutation);
  util::Permute(permutation, &weights);
  util::Permute(permutation, &resources[0]);
  // [END graph]

  // [START first-path]
  // A reusable shortest path calculator.
  // We need a topological order. For this structured graph, we find it by hand
  // instead of using util::graph::FastTopologicalSort().
  std::vector<int32_t> topological_order = {source};
  for (int32_t i = 0; i < n; ++i) {
    topological_order.push_back(i);
  }
  topological_order.push_back(dest);

  const std::vector<int> sources = {source};
  const std::vector<int> destinations = {dest};
  const std::vector<double> max_resources = {1.0};

  operations_research::ConstrainedShortestPathsOnDagWrapper<util::StaticGraph<>>
      constrained_shortest_path_on_dag(&graph, &weights, &resources,
                                       topological_order, sources, destinations,
                                       &max_resources);
  operations_research::GraphPathWithLength<util::StaticGraph<>>
      initial_constrained_shortest_path =
          constrained_shortest_path_on_dag.RunConstrainedShortestPathOnDag();

  std::cout << "Initial distance: " << initial_constrained_shortest_path.length
            << std::endl;
  std::cout << "Initial path: "
            << absl::StrJoin(initial_constrained_shortest_path.node_path, ", ")
            << std::endl;
  // [END first-path]

  // [START more-paths]
  // Now, we make a single arc from source to M free, and a single arc from M
  // to dest free, and resolve. If the free edge from the source hits before
  // the free edge to the dest in M, we use both, walking through M. Otherwise,
  // we use only one free arc.
  std::vector<std::pair<int, int>> fast_paths = {{2, 3}, {8, 1}, {3, 7}};
  for (const auto [free_from_source, free_to_dest] : fast_paths) {
    weights[permutation[free_from_source]] = 0;
    weights[permutation[n + free_to_dest]] = 0;

    operations_research::GraphPathWithLength<util::StaticGraph<>>
        constrained_shortest_path =
            constrained_shortest_path_on_dag.RunConstrainedShortestPathOnDag();
    std::cout << "source -> " << free_from_source << " and " << free_to_dest
              << " -> dest are now free" << std::endl;
    std::string label = absl::StrCat("_", free_from_source, "_", free_to_dest);
    std::cout << "Distance" << label << ": " << constrained_shortest_path.length
              << std::endl;
    std::cout << "Path" << label << ": "
              << absl::StrJoin(constrained_shortest_path.node_path, ", ")
              << std::endl;

    // Restore the old weights
    weights[permutation[free_from_source]] = 100;
    weights[permutation[n + free_to_dest]] = 100;
  }
  // [END more-paths]
  return 0;
}
