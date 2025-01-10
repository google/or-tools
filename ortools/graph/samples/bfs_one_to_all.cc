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
  // The arcs of this directed graph are encoded as a list of pairs, where
  // .first is the source and .second is the destination of each arc.
  const std::vector<std::pair<int, int>> arcs = {{0, 1}, {1, 2}, {1, 3},
                                                 {2, 3}, {3, 0}, {4, 2}};
  const int num_nodes = 5;

  // Transform the graph to an adjacency_list
  std::vector<std::vector<int>> adjacency_list(num_nodes);
  for (const auto& [start, end] : arcs) {
    adjacency_list[start].push_back(end);
  }

  // Compute the shortest path from 0 to each reachable node.
  const int source = 0;
  ASSIGN_OR_RETURN(
      const std::vector<int> bfs_tree,
      util::graph::GetBFSRootedTree(adjacency_list, num_nodes, source));
  // Runs in O(num nodes). Nodes that are not reachable have distance -1.
  ASSIGN_OR_RETURN(const std::vector<int> node_distances,
                   util::graph::GetBFSDistances(bfs_tree));
  for (int t = 0; t < num_nodes; ++t) {
    if (t == source) {
      continue;
    }
    if (node_distances[t] >= 0) {
      ASSIGN_OR_RETURN(const std::vector<int> shortest_path,
                       util::graph::GetBFSShortestPath(bfs_tree, t));
      std::cout << "Shortest path from 0 to " << t
                << " has length: " << node_distances[t] << std::endl;
      std::cout << "Path is: " << absl::StrJoin(shortest_path, ", ")
                << std::endl;
    } else {
      std::cout << "No path from 0 to " << t << std::endl;
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
