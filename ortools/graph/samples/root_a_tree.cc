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
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/strings/str_join.h"
#include "ortools/base/init_google.h"
#include "ortools/base/status_macros.h"
#include "ortools/graph/graph.h"
#include "ortools/graph/rooted_tree.h"

namespace {

absl::Status Main() {
  // Make an undirected tree as a graph using ListGraph (add the arcs in each
  // direction).
  const int32_t num_nodes = 5;
  std::vector<std::pair<int32_t, int32_t>> arcs = {
      {0, 1}, {1, 2}, {2, 3}, {1, 4}};
  util::ListGraph<> graph(num_nodes, 2 * static_cast<int32_t>(arcs.size()));
  for (const auto [s, t] : arcs) {
    graph.AddArc(s, t);
    graph.AddArc(t, s);
  }

  // Root the tree from 2. Save the depth of each node and topological ordering
  int root = 2;
  std::vector<int32_t> topological_order;
  std::vector<int32_t> depth;
  ASSIGN_OR_RETURN(const operations_research::RootedTree<int32_t> tree,
                   operations_research::RootedTreeFromGraph(
                       root, graph, &topological_order, &depth));

  // Parents are:
  //  0 -> 1
  //  1 -> 2
  //  2 is root (returns -1)
  //  3 -> 2
  //  4 -> 1
  std::cout << "Parents:" << std::endl;
  for (int i = 0; i < num_nodes; ++i) {
    std::cout << "  " << i << " -> " << tree.parents()[i] << std::endl;
  }

  // Depths are:
  //   0: 2
  //   1: 1
  //   2: 0
  //   3: 1
  //   4: 2
  std::cout << "Depths:" << std::endl;
  for (int i = 0; i < num_nodes; ++i) {
    std::cout << "  " << i << " -> " << depth[i] << std::endl;
  }

  // Many possible topological orders, including:
  //   [2, 1, 0, 4, 3]
  // all starting with 2.
  std::cout << "Topological order: " << absl::StrJoin(topological_order, ", ")
            << std::endl;

  return absl::OkStatus();
}

}  // namespace

int main(int argc, char** argv) {
  InitGoogle(argv[0], &argc, &argv, true);
  QCHECK_OK(Main());
  return 0;
}
