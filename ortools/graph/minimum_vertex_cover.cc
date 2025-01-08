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

#include "ortools/graph/minimum_vertex_cover.h"

#include <vector>

#include "absl/log/check.h"
#include "ortools/graph/max_flow.h"

namespace operations_research {

std::vector<bool> BipartiteMinimumVertexCover(
    const std::vector<std::vector<int>>& left_to_right_arcs, int num_right) {
  // This algorithm first uses the maximum flow to find a maximum matching. Then
  // it uses the same method outlined in the proof of Konig's theorem to
  // transform the maximum matching into a minimum vertex cover.
  //
  // More concretely, it uses a DFS starting with unmatched nodes and
  // alternating matched/unmatched edges to find a minimum vertex cover.
  SimpleMaxFlow max_flow;
  const int num_left = left_to_right_arcs.size();
  std::vector<SimpleMaxFlow::ArcIndex> arcs;
  for (int i = 0; i < num_left; ++i) {
    for (const int right_node : left_to_right_arcs[i]) {
      DCHECK_GE(right_node, num_left);
      DCHECK_LT(right_node, num_right + num_left);
      arcs.push_back(max_flow.AddArcWithCapacity(i, right_node, 1));
    }
  }
  std::vector<std::vector<int>> adj_list = left_to_right_arcs;
  adj_list.resize(num_left + num_right);
  for (int i = 0; i < num_left; ++i) {
    for (const int right_node : left_to_right_arcs[i]) {
      adj_list[right_node].push_back(i);
    }
  }
  const int sink = num_left + num_right;
  const int source = num_left + num_right + 1;
  for (int i = 0; i < num_left; ++i) {
    max_flow.AddArcWithCapacity(source, i, 1);
  }
  for (int i = 0; i < num_right; ++i) {
    max_flow.AddArcWithCapacity(i + num_left, sink, 1);
  }
  CHECK(max_flow.Solve(source, sink) == SimpleMaxFlow::OPTIMAL);
  std::vector<int> maximum_matching(num_left + num_right, -1);
  for (const SimpleMaxFlow::ArcIndex arc : arcs) {
    if (max_flow.Flow(arc) > 0) {
      maximum_matching[max_flow.Tail(arc)] = max_flow.Head(arc);
      maximum_matching[max_flow.Head(arc)] = max_flow.Tail(arc);
    }
  }
  // We do a DFS starting with unmatched nodes and alternating matched/unmatched
  // edges.
  std::vector<bool> in_alternating_path(num_left + num_right, false);
  std::vector<int> to_visit;
  for (int i = 0; i < num_left; ++i) {
    if (maximum_matching[i] == -1) {
      to_visit.push_back(i);
    }
  }
  while (!to_visit.empty()) {
    const int current = to_visit.back();
    to_visit.pop_back();
    if (in_alternating_path[current]) {
      continue;
    }
    in_alternating_path[current] = true;
    for (const int j : adj_list[current]) {
      if (current < num_left && maximum_matching[current] != j) {
        to_visit.push_back(j);
      } else if (current >= num_left && maximum_matching[current] == j) {
        to_visit.push_back(j);
      }
    }
  }
  std::vector<bool> minimum_vertex_cover(num_left + num_right, false);
  for (int i = 0; i < num_left; ++i) {
    if (!in_alternating_path[i]) {
      minimum_vertex_cover[i] = true;
    }
  }
  for (int i = num_left; i < num_left + num_right; ++i) {
    if (in_alternating_path[i]) {
      minimum_vertex_cover[i] = true;
    }
  }
  return minimum_vertex_cover;
}

}  // namespace operations_research
