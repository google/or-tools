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
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "ortools/base/init_google.h"
#include "ortools/graph/bounded_dijkstra.h"
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
  // The graph has 3 * n arcs (with weights):
  //   * (source -> i) with weight 100 for i in M
  //   * (i -> (i+1) % n) with weight 1 for i in M
  //   * (i -> dest) with weight 100 for i in M
  //
  // Every path [source, i, dest] for i in M is a shortest path from source to
  // dest with weight 200.
  const int n = 10;
  const int source = n;
  const int dest = n + 1;
  util::StaticGraph<> graph;
  // There are 3 types of arcs: (1) source to M, (2) within M, and (3) M to
  // dest. This vector stores all of them, first of type (1), then type (2),
  // then type (3). The arcs are ordered by i in M within each type.
  std::vector<int> weights(3 * n);

  for (int i = 0; i < n; ++i) {
    graph.AddArc(source, i);
    weights[i] = 100;
  }
  for (int i = 0; i < n; ++i) {
    graph.AddArc(i, (i + 1) % n);
    weights[n + i] = 1;
  }
  for (int i = 0; i < n; ++i) {
    graph.AddArc(i, dest);
    weights[2 * n + i] = 100;
  }

  // Static graph reorders the arcs at Build() time, use permutation to get from
  // the old ordering to the new one.
  std::vector<int32_t> permutation;
  graph.Build(&permutation);
  util::Permute(permutation, &weights);
  // [END graph]

  // [START first-path]
  // A reusable shortest path calculator.
  operations_research::BoundedDijkstraWrapper<util::StaticGraph<>, int>
      dijkstra(&graph, &weights);

  // This function returns false if there is no path from `source` to `dest`
  // of length at most `distance_limit`. Avoid CHECK when you cannot prove a
  // path exists.
  CHECK(dijkstra.OneToOneShortestPath(
      source, dest, /*distance_limit=*/std::numeric_limits<int>::max()));
  std::cout << "Initial distance: " << dijkstra.distances()[dest] << std::endl;
  std::cout << "Initial path: "
            << absl::StrJoin(dijkstra.NodePathTo(dest), ", ") << std::endl;
  // [END first-path]

  // [START more-paths]
  // Now, we make a single arc from source to M free, and a single arc from M
  // to dest free, and resolve. The shortest path is now to use these free arcs,
  // walking through M to connect them.
  std::vector<std::pair<int, int>> fast_paths = {{2, 4}, {8, 1}, {3, 7}};
  for (const auto [free_from_source, free_to_dest] : fast_paths) {
    weights[permutation[free_from_source]] = 0;
    weights[permutation[2 * n + free_to_dest]] = 0;

    CHECK(dijkstra.OneToOneShortestPath(
        source, dest, /*distance_limit=*/std::numeric_limits<int>::max()));
    std::cout << "source -> " << free_from_source << " and " << free_to_dest
              << " -> dest are now free" << std::endl;
    std::string label = absl::StrCat("_", free_from_source, "_", free_to_dest);
    std::cout << "Distance" << label << ": " << dijkstra.distances()[dest]
              << std::endl;
    std::cout << "Path" << label << ": "
              << absl::StrJoin(dijkstra.NodePathTo(dest), ", ") << std::endl;

    // Restore the old weights
    weights[permutation[free_from_source]] = 100;
    weights[permutation[2 * n + free_to_dest]] = 100;
  }
  // [END more-paths]
  return 0;
}
