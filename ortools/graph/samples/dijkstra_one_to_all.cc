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

// [START program]
// [START imports]
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

#include "absl/strings/str_join.h"
#include "ortools/base/init_google.h"
#include "ortools/graph/bounded_dijkstra.h"
#include "ortools/graph_base/graph.h"
// [END imports]

int main(int argc, char** argv) {
  InitGoogle(argv[0], &argc, &argv, true);

  // Create the graph
  util::StaticGraph<> graph;
  std::vector<int> weights;
  graph.AddArc(0, 1);
  weights.push_back(2);
  graph.AddArc(1, 2);
  weights.push_back(4);
  graph.AddArc(1, 3);
  weights.push_back(0);
  graph.AddArc(2, 3);
  weights.push_back(6);
  graph.AddArc(3, 0);
  weights.push_back(8);
  graph.AddArc(4, 2);
  weights.push_back(1);

  // Static graph reorders the arcs at Build() time, use permutation to get
  // from the old ordering to the new one.
  std::vector<int32_t> permutation;
  graph.Build(&permutation);
  util::Permute(permutation, &weights);

  // Compute the shortest path to each reachable node.
  operations_research::BoundedDijkstraWrapper<util::StaticGraph<>, int>
      dijkstra(&graph, &weights);
  const std::vector<int> reachable_from_zero = dijkstra.RunBoundedDijkstra(
      /*source_node=*/0, /*distance_limit=*/std::numeric_limits<int>::max());

  // Print paths from zero to the reachable nodes ordered by distance.
  for (const int dest : reachable_from_zero) {
    const int distance = dijkstra.distances()[dest];
    const std::vector<int32_t> path = dijkstra.NodePathTo(dest);
    std::cout << "Distance to " << dest << ": " << distance << std::endl;
    std::cout << "Path to " << dest << ": " << absl::StrJoin(path, ", ")
              << std::endl;
  }
}
// [END program]
