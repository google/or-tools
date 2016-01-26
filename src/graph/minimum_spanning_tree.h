// Copyright 2010-2014 Google
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

#ifndef OR_TOOLS_GRAPH_MINIMUM_SPANNING_TREE_H_
#define OR_TOOLS_GRAPH_MINIMUM_SPANNING_TREE_H_

#include <vector>

#include "base/integral_types.h"
#include "graph/connectivity.h"
#include "graph/graph.h"
#include "util/vector_or_function.h"

namespace operations_research {

// Implementation of Kruskal's mininumum spanning tree algorithm (c.f.
// https://en.wikipedia.org/wiki/Kruskal%27s_algorithm).
// Returns the index of the arcs appearing in the tree; will return a forest if
// the graph is disconnected. Nodes without any arcs will be ignored.
// Each arc of the graph is interpreted as an undirected arc.
// Complexity of the algorithm is O(E * log(E)) where E is the number of arcs
// in the graph.
template <typename Graph, typename ArcComparator>
std::vector<typename Graph::ArcIndex> BuildKruskalMinimumSpanningTree(
    const Graph& graph, const ArcComparator& arc_comparator) {
  using ArcIndex = typename Graph::ArcIndex;
  using NodeIndex = typename Graph::NodeIndex;
  const int num_arcs = graph.num_arcs();
  std::vector<ArcIndex> ordered_arcs(num_arcs);
  for (const ArcIndex arc : graph.AllForwardArcs()) {
    ordered_arcs[arc] = arc;
  }
  std::sort(ordered_arcs.begin(), ordered_arcs.end(), arc_comparator);
  int arc_index = 0;
  std::vector<ArcIndex> tree_arcs;
  if (graph.num_nodes() == 0) {
    return tree_arcs;
  }
  const int expected_tree_size = graph.num_nodes() - 1;
  tree_arcs.reserve(expected_tree_size);
  ConnectedComponents<NodeIndex, ArcIndex> components;
  components.Init(graph.num_nodes());
  while (tree_arcs.size() != expected_tree_size && arc_index < num_arcs) {
    const ArcIndex arc = ordered_arcs[arc_index];
    const NodeIndex tail_class =
        components.GetClassRepresentative(graph.Tail(arc));
    const NodeIndex head_class =
        components.GetClassRepresentative(graph.Head(arc));
    if (tail_class != head_class) {
      components.MergeClasses(tail_class, head_class);
      tree_arcs.push_back(arc);
    }
    ++arc_index;
  }
  return tree_arcs;
}

}  // namespace operations_research
#endif  // OR_TOOLS_GRAPH_MINIMUM_SPANNING_TREE_H_
