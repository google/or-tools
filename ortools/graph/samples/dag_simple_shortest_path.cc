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

#include <iostream>
#include <vector>

#include "absl/strings/str_join.h"
#include "ortools/base/init_google.h"
#include "ortools/graph/dag_shortest_path.h"

int main(int argc, char** argv) {
  InitGoogle(argv[0], &argc, &argv, true);

  // The input graph, encoded as a list of arcs with distances.
  std::vector<operations_research::ArcWithLength> arcs = {
      {.tail = 0, .head = 2, .length = 5},
      {.tail = 0, .head = 3, .length = 4},
      {.tail = 1, .head = 3, .length = 1},
      {.tail = 2, .head = 4, .length = -3},
      {.tail = 3, .head = 4, .length = 0}};
  const int num_nodes = 5;

  const int source = 0;
  const int destination = 4;
  const operations_research::PathWithLength path_with_length =
      operations_research::ShortestPathsOnDag(num_nodes, arcs, source,
                                              destination);

  // Print to length of the path and then the nodes in the path.
  std::cout << "Shortest path length: " << path_with_length.length << std::endl;
  std::cout << "Shortest path nodes: "
            << absl::StrJoin(path_with_length.node_path, ", ") << std::endl;
  return 0;
}
