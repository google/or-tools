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

#include "ortools/graph/dag_connectivity.h"

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

#include "absl/types/span.h"
#include "ortools/base/container_logging.h"
#include "ortools/base/logging.h"
#include "ortools/graph_base/topologicalsorter.h"

namespace operations_research {

// The algorithm is as follows:
//  1. Sort the nodes of the graph topologically.  If a cycle is detected,
//     terminate
//  2. Build the adjacency list for the graph, i.e., adj_list[i] is the list
//     of nodes that can be directly reached from i.
//  3. Create a 2d bool vector x where x[i][j] indicates there is a path from
//     i to j, and for each arc in "arcs", set x[i][j] to true
//  4. In reverse topological order (leaves first) for each node i, for each
//     child j of i, for each node k reachable for j, set k to be reachable
//     from i as well (x[i][k] = true for all k s.t. x[j][k] is true).
//
// The running times of the steps are:
//   1. O(num_arcs)
//   2. O(num_arcs)
//   3. O(num_nodes^2 + num_arcs)
//   4. O(num_nodes*num_arcs)
// Thus the total run time is O(num_nodes^2 + num_nodes*num_arcs).
//
// Implementation note: typically, step 4 will dominate.  To speed up the inner
// loop, we use Bitset64, allowing use to merge 64 x[k][j] values at a time with
// the |= operator.
//
// For graphs where num_arcs is o(num_nodes), a different data structure could
// be used in 3, but this isn't really the interesting case (and prevents |=).
//
// A further improvement on this algorithm is possible, step four can run in
// time O(num_nodes*num_arcs_in_transitive_reduction), and as a by product,
// the transitive reduction can also be produced as output.  For details, see
// "A REDUCT-AND_CLOSURE ALGORITHM FOR GRAPHS" (Alla Goralcikova and
// Vaclav Koubek 1979).  The better typeset paper "AN IMPROVED ALGORITHM FOR
// TRANSITIVE CLOSURE ON ACYCLIC DIGRAPHS" (Klaus Simon 1988) gives a slight
// improvement on the result (less memory, same runtime).
std::vector<Bitset64<int64_t>> ComputeDagConnectivity(
    absl::Span<const std::pair<int, int>> arcs, bool* error_was_cyclic,
    std::vector<int>* error_cycle_out) {
  CHECK(error_was_cyclic != nullptr);
  CHECK(error_cycle_out != nullptr);
  *error_was_cyclic = false;
  error_cycle_out->clear();
  if (arcs.empty()) return {};
  int num_nodes = 0;
  for (const std::pair<int, int>& arc : arcs) {
    CHECK_GE(arc.first, 0);
    CHECK_GE(arc.second, 0);
    num_nodes = std::max(num_nodes, arc.first + 1);
    num_nodes = std::max(num_nodes, arc.second + 1);
  }
  DenseIntStableTopologicalSorter sorter(num_nodes);
  for (const auto& arc : arcs) {
    sorter.AddEdge(arc.first, arc.second);
  }
  std::vector<int> topological_order;
  int next;
  while (sorter.GetNext(&next, error_was_cyclic, error_cycle_out)) {
    topological_order.push_back(next);
  }
  if (*error_was_cyclic) return {};
  std::vector<std::vector<int>> adjacency_list(num_nodes);
  for (const auto& arc : arcs) {
    adjacency_list[arc.first].push_back(arc.second);
  }

  std::vector<Bitset64<int64_t>> connectivity(num_nodes);
  for (Bitset64<int64_t>& bitset : connectivity) {
    bitset.Resize(num_nodes);
  }
  for (const auto& arc : arcs) {
    connectivity[arc.first].Set(arc.second);
  }

  // Iterate over the nodes in reverse topological order.
  std::reverse(topological_order.begin(), topological_order.end());
  // NOTE(user): these two loops visit every arc in the graph, and each
  // union is over a set of size given by the number of nodes.  This gives the
  // runtime in step 4 of O(num_nodes*num_arcs)
  for (const int node : topological_order) {
    for (const int child : adjacency_list[node]) {
      connectivity[node].Union(connectivity[child]);
    }
  }
  return connectivity;
}

std::vector<Bitset64<int64_t>> ComputeDagConnectivityOrDie(
    absl::Span<const std::pair<int, int>> arcs) {
  bool error_was_cyclic = false;
  std::vector<int> error_cycle;
  std::vector<Bitset64<int64_t>> result =
      ComputeDagConnectivity(arcs, &error_was_cyclic, &error_cycle);
  CHECK(!error_was_cyclic) << "Graph should have been acyclic but has cycle: "
                           << gtl::LogContainer(error_cycle);
  return result;
}

}  // namespace operations_research
