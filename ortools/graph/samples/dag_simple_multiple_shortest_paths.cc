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
#include <vector>

#include "absl/strings/str_join.h"
#include "ortools/base/init_google.h"
#include "ortools/graph/dag_shortest_path.h"

int main(int argc, char** argv) {
  InitGoogle(argv[0], &argc, &argv, true);

  // The input graph, encoded as a list of arcs with distances.
  std::vector<operations_research::ArcWithLength> arcs = {
      {.from = 0, .to = 1, .length = 2},  {.from = 0, .to = 2, .length = 5},
      {.from = 0, .to = 3, .length = 4},  {.from = 1, .to = 4, .length = 1},
      {.from = 2, .to = 4, .length = -3}, {.from = 3, .to = 4, .length = 0}};
  const int num_nodes = 5;

  const int source = 0;
  const int destination = 4;
  const int path_count = 2;
  const std::vector<operations_research::PathWithLength> paths_with_length =
      operations_research::KShortestPathsOnDag(num_nodes, arcs, source,
                                               destination, path_count);

  for (int path_index = 0; path_index < paths_with_length.size();
       ++path_index) {
    std::cout << "#" << (path_index + 1) << " shortest path has length: "
              << paths_with_length[path_index].length << std::endl;
    std::cout << "#" << (path_index + 1) << " shortest path is: "
              << absl::StrJoin(paths_with_length[path_index].node_path, ", ")
              << std::endl;
  }
  return 0;
}
