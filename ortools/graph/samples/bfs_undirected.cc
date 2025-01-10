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

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/strings/str_join.h"
#include "ortools/base/init_google.h"
#include "ortools/base/status_macros.h"
#include "ortools/graph/bfs.h"

namespace {

absl::Status Main() {
  // The edges of this undirected graph encoded as a list of pairs, where .first
  // and .second are the endpoints of each edge (the order does not matter).
  const std::vector<std::pair<int, int>> edges = {
      {0, 1}, {0, 2}, {1, 2}, {2, 3}};
  const int num_nodes = 4;

  // Transform the graph to an adjacency_list
  std::vector<std::vector<int>> adjacency_list(num_nodes);
  for (const auto& [node1, node2] : edges) {
    // Include both orientations of the edge
    adjacency_list[node1].push_back(node2);
    adjacency_list[node2].push_back(node1);
  }

  // Solve the shortest path problem from 0 to 3.
  const int source = 0;
  const int terminal = 3;
  ASSIGN_OR_RETURN(
      const std::vector<int> bfs_tree,
      util::graph::GetBFSRootedTree(adjacency_list, num_nodes, source));
  ASSIGN_OR_RETURN(const std::vector<int> shortest_path,
                   util::graph::GetBFSShortestPath(bfs_tree, terminal));

  // Print to length of the path and then the nodes in the path.
  std::cout << "Shortest path length (in arcs): " << shortest_path.size() - 1
            << std::endl;
  std::cout << "Shortest path nodes: " << absl::StrJoin(shortest_path, ", ")
            << std::endl;

  return absl::OkStatus();
}

}  // namespace

int main(int argc, char** argv) {
  InitGoogle(argv[0], &argc, &argv, true);
  QCHECK_OK(Main());
  return 0;
}
