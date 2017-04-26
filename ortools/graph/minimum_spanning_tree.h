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

#include "ortools/base/integral_types.h"
#include "ortools/graph/connectivity.h"
#include "ortools/graph/graph.h"
#include "ortools/util/vector_or_function.h"
#include "ortools/base/adjustable_priority_queue-inl.h"
#include "ortools/base/adjustable_priority_queue.h"

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
  std::vector<ArcIndex> node_neighbor(graph.num_nodes(), Graph::kNilArc);
  std::vector<bool> node_active(graph.num_nodes(), true);

  // This struct represents entries in the adjustable priority queue which
  // maintains active nodes (not added to the tree yet) in decreasing insertion
  // cost order. AdjustablePriorityQueue requires the existence of the
  // SetHeapIndex and GetHeapIndex methods.
  struct Entry {
    void SetHeapIndex(int index) { heap_index = index; }
    int GetHeapIndex() const { return heap_index; }
    bool operator<(const Entry& other) const { return value > other.value; }

    NodeIndex node;
    ArcValueType value;
    int heap_index;
  };

  AdjustablePriorityQueue<Entry> pq;
  std::vector<Entry> entries;
  for (NodeIndex node : graph.AllNodes()) {
    entries.push_back({node, std::numeric_limits<ArcValueType>::max(), -1});
  }
  entries[0].value = 0;
  pq.Add(&entries[0]);
  while (!pq.IsEmpty() && tree_arcs.size() != expected_tree_size) {
    const Entry* best = pq.Top();
    const NodeIndex node = best->node;
    pq.Pop();
    node_active[node] = false;
    if (node_neighbor[node] != Graph::kNilArc) {
      tree_arcs.push_back(node_neighbor[node]);
    }
    for (const ArcIndex arc : graph.OutgoingArcs(node)) {
      const NodeIndex neighbor = graph.Head(arc);
      if (node_active[neighbor]) {
        const ArcValueType value = arc_value(arc);
        Entry& entry = entries[neighbor];
        if (value < entry.value) {
          node_neighbor[neighbor] = arc;
          entry.value = value;
          if (pq.Contains(&entry)) {
            pq.NoteChangedPriority(&entry);
          } else {
            pq.Add(&entry);
          }
        }
      }
    }
  }
  return tree_arcs;
}

}  // namespace operations_research
#endif  // OR_TOOLS_GRAPH_MINIMUM_SPANNING_TREE_H_
