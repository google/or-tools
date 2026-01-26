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
struct Arc {
  int start = 0;
  int end = 0;
  int length = 0;
};
}  // namespace

int main(int argc, char** argv) {
  InitGoogle(argv[0], &argc, &argv, true);

  // The input graph, encoded as a list of arcs with distances.
  std::vector<Arc> arcs = {
      {.start = 0, .end = 1, .length = 3}, {.start = 0, .end = 2, .length = 5},
      {.start = 1, .end = 2, .length = 1}, {.start = 1, .end = 3, .length = 4},
      {.start = 1, .end = 4, .length = 0}, {.start = 2, .end = 4, .length = 2},
      {.start = 3, .end = 2, .length = 2}, {.start = 3, .end = 5, .length = 4},
      {.start = 4, .end = 3, .length = 2}, {.start = 4, .end = 5, .length = 5}};

  // Transform the graph.
  std::vector<int> tails;
  std::vector<int> heads;
  std::vector<int> lengths;
  for (const Arc& arc : arcs) {
    tails.push_back(arc.start);
    heads.push_back(arc.end);
    lengths.push_back(arc.length);
  }

  // Solve the shortest path problem from 0 to 5.
  std::pair<int, std::vector<int>> result =
      operations_research::SimpleOneToOneShortestPath<int, int>(0, 5, tails,
                                                                heads, lengths);

  // Print to length of the path and then the nodes in the path.
  std::cout << "Shortest path length: " << result.first << std::endl;
  std::cout << "Shortest path nodes: " << absl::StrJoin(result.second, ", ")
            << std::endl;
  return 0;
}
