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

#include <iostream>
#include <utility>
#include <vector>

#include "absl/strings/str_join.h"
#include "ortools/base/init_google.h"
#include "ortools/graph/bounded_dijkstra.h"

namespace {
// An edge in an undirected graph, the order of the endpoints does not matter.
struct Edge {
  int endpoint1 = 0;
  int endpoint2 = 0;
  int length = 0;
};
}  // namespace

int main(int argc, char** argv) {
  InitGoogle(argv[0], &argc, &argv, true);

  // The input graph, encoded as a list of edges with distances.
  std::vector<Edge> edges = {
      {.endpoint1 = 0, .endpoint2 = 1, .length = 8},
      {.endpoint1 = 0, .endpoint2 = 2, .length = 1},
      {.endpoint1 = 1, .endpoint2 = 2, .length = 0},
      {.endpoint1 = 1, .endpoint2 = 3, .length = 1},
      {.endpoint1 = 1, .endpoint2 = 4, .length = 4},
      {.endpoint1 = 2, .endpoint2 = 4, .length = 5},
      {.endpoint1 = 3, .endpoint2 = 4, .length = 2},
  };

  // Transform the graph.
  std::vector<int> tails;
  std::vector<int> heads;
  std::vector<int> lengths;
  for (const Edge& edge : edges) {
    // The "forward" directed edge
    tails.push_back(edge.endpoint1);
    heads.push_back(edge.endpoint2);
    lengths.push_back(edge.length);
    // The "backward" directed edge
    tails.push_back(edge.endpoint2);
    heads.push_back(edge.endpoint1);
    lengths.push_back(edge.length);
  }

  // Solve the shortest path problem from 0 to 4.
  std::pair<int, std::vector<int>> result =
      operations_research::SimpleOneToOneShortestPath<int, int>(0, 4, tails,
                                                                heads, lengths);

  // Print to length of the path and then the nodes in the path.
  std::cout << "Shortest path length: " << result.first << std::endl;
  std::cout << "Shortest path nodes: " << absl::StrJoin(result.second, ", ")
            << std::endl;
  return 0;
}
