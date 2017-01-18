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

#include <queue>
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
// in the graph. Memory usage is O(E * log(E)).

// TODO(user): Add a global Minimum Spanning Tree API automatically switching
// between Prim and Kruskal depending on problem size.

// Version taking sorted graph arcs. Allows somewhat incremental recomputation
// of minimum spanning trees as most of the processing time is spent sorting
// arcs.
// Usage:
//  ListGraph<int, int> graph(...);
//  std::vector<int> sorted_arcs = ...;
//  std::vector<int> mst = BuildKruskalMinimumSpanningTreeFromSortedArcs(
//      graph, sorted_arcs);
//
template <typename Graph>
std::vector<typename Graph::ArcIndex>
BuildKruskalMinimumSpanningTreeFromSortedArcs(
    const Graph& graph,
    const std::vector<typename Graph::ArcIndex>& sorted_arcs) {
  using ArcIndex = typename Graph::ArcIndex;
  using NodeIndex = typename Graph::NodeIndex;
  const int num_arcs = graph.num_arcs();
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
    const ArcIndex arc = sorted_arcs[arc_index];
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

// Version taking an arc comparator to sort graph arcs.
// Usage:
//  ListGraph<int, int> graph(...);
//  const auto arc_cost = [&graph](int arc) {
//                           return f(graph.Tail(arc), graph.Head(arc));
//                        };
//  std::vector<int> mst = BuildKruskalMinimumSpanningTree(
//      graph,
//      [&arc_cost](int a, int b) { return arc_cost(a) < arc_cost(b); });
//
template <typename Graph, typename ArcComparator>
std::vector<typename Graph::ArcIndex> BuildKruskalMinimumSpanningTree(
    const Graph& graph, const ArcComparator& arc_comparator) {
  using ArcIndex = typename Graph::ArcIndex;
  std::vector<ArcIndex> sorted_arcs(graph.num_arcs());
  for (const ArcIndex arc : graph.AllForwardArcs()) {
    sorted_arcs[arc] = arc;
  }
  std::sort(sorted_arcs.begin(), sorted_arcs.end(), arc_comparator);
  return BuildKruskalMinimumSpanningTreeFromSortedArcs(graph, sorted_arcs);
}

// Implementation of Prim's mininumum spanning tree algorithm (c.f.
// https://en.wikipedia.org/wiki/Prim's_algorithm) on undirected connected
// graphs.
// Returns the index of the arcs appearing in the tree.
// Complexity of the algorithm is O(E * log(V)) where E is the number of arcs
// in the graph, V is the number of vertices. Memory usage is O(V) + memory
// taken by the graph.
// Usage:
//  ListGraph<int, int> graph(...);
//  const auto arc_cost = [&graph](int arc) -> int64 {
//                           return f(graph.Tail(arc), graph.Head(arc));
//                        };
//  std::vector<int> mst = BuildPrimMinimumSpanningTree(graph, arc_cost);
//
template <typename Graph, typename ArcValue>
std::vector<typename Graph::ArcIndex> BuildPrimMinimumSpanningTree(
    const Graph& graph, const ArcValue& arc_value) {
  using ArcIndex = typename Graph::ArcIndex;
  using NodeIndex = typename Graph::NodeIndex;
  using ArcValueType = decltype(arc_value(0));
  std::vector<ArcIndex> tree_arcs;
  if (graph.num_nodes() == 0) {
    return tree_arcs;
  }
  const int expected_tree_size = graph.num_nodes() - 1;
  tree_arcs.reserve(expected_tree_size);
  std::vector<ArcValueType> node_value(graph.num_nodes(),
                                  std::numeric_limits<ArcValueType>::max());
  std::vector<ArcIndex> node_neighbor(graph.num_nodes(), Graph::kNilArc);
  std::vector<bool> node_active(graph.num_nodes(), true);
  struct Entry {
    NodeIndex node;
    ArcValueType value;
    bool operator<(const Entry& other) const { return value > other.value; }
  };
  std::priority_queue<Entry> pq;
  pq.push({0, 0});
  size_t pq_size = 0;
  while (!pq.empty() && tree_arcs.size() != expected_tree_size) {
    pq_size = std::max(pq_size, pq.size());
    const Entry best = pq.top();
    const NodeIndex node = best.node;
    pq.pop();

    // Checking consistency of the pq element in case it is stale (stale entries
    // are entries for a given node which have been pushed before the latest
    // update to this node); since we are using a normal priority queue instead
    // of an adjustable one (which is much slower) to implement the heap, such
    // entries are just left in the queue instead of calling an increase-key
    // operation.
    // TODO(user): Add a safeguard to make sure the size of the queue
    // remains O(num_nodes).
    if (best.value > node_value[node] || !node_active[node]) {
      continue;
    }
    node_active[node] = false;
    if (node_neighbor[node] != Graph::kNilArc) {
      tree_arcs.push_back(node_neighbor[node]);
    }
    for (const int arc : graph.OutgoingArcs(node)) {
      const NodeIndex neighbor = graph.Head(arc);
      if (node_active[neighbor]) {
        const ArcValueType value = arc_value(arc);
        if (value < node_value[neighbor]) {
          node_value[neighbor] = value;
          node_neighbor[neighbor] = arc;
          pq.push({neighbor, value});
        }
      }
    }
  }
  VLOG(1) << "Prim actual PQ size / nodes: " << pq_size << " / "
          << graph.num_nodes();
  return tree_arcs;
}

}  // namespace operations_research
#endif  // OR_TOOLS_GRAPH_MINIMUM_SPANNING_TREE_H_
