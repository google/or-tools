// Copyright 2010-2018 Google LLC
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

// A collections of utilities for the Graph classes in ./graph.h.

#ifndef UTIL_GRAPH_UTIL_H_
#define UTIL_GRAPH_UTIL_H_

#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "ortools/base/hash.h"
#include "ortools/base/map_util.h"
#include "ortools/graph/connected_components.h"
#include "ortools/graph/graph.h"
#include "ortools/graph/iterators.h"

namespace util {

// Here's a set of simple diagnosis tools. Notes:
// - A self-arc is an arc from a node to itself.
// - We say that an arc A->B is duplicate when there is another arc A->B in the
//   same graph.
// - A graph is said "weakly connected" if it is connected when considering all
//   arcs as undirected edges.
// - A graph is said "symmetric" iff for all (a, b), the number of arcs a->b
//   is equal to the number of arcs b->a.
//
// All these diagnosis work in O(graph size), since the inverse Ackerman
// function is <= 5 for all practical instances, and are very fast.
//
// If the graph is a "static" kind, they must be finalized, except for
// GraphHasSelfArcs() and GraphIsWeaklyConnected() which also support
// non-finalized StaticGraph<>.
template <class Graph>
bool GraphHasSelfArcs(const Graph& graph);
template <class Graph>
bool GraphHasDuplicateArcs(const Graph& graph);
template <class Graph>
bool GraphIsSymmetric(const Graph& graph);
template <class Graph>
bool GraphIsWeaklyConnected(const Graph& graph);

// Returns a fresh copy of a given graph.
template <class Graph>
std::unique_ptr<Graph> CopyGraph(const Graph& graph);

// Creates a remapped copy of graph "graph", where node i becomes node
// new_node_index[i].
// "new_node_index" must be a valid permutation of [0..num_nodes-1] or the
// behavior is undefined (it may die).
// Note that you can call IsValidPermutation() to check it yourself.
template <class Graph>
std::unique_ptr<Graph> RemapGraph(const Graph& graph,
                                  const std::vector<int>& new_node_index);

// Gets the induced subgraph of "graph" restricted to the nodes in "nodes":
// the resulting graph will have exactly nodes.size() nodes, and its
// node #0 will be the former graph's node #nodes[0], etc.
// See https://en.wikipedia.org/wiki/Induced_subgraph .
// The "nodes" must be a valid subset (no repetitions) of
// [0..graph.num_nodes()-1], or the behavior is undefined (it may die).
// Note that you can call IsSubsetOf0N() to check it yourself.
//
// Current complexity: O(num old nodes + num new arcs). It could easily
// be done in O(num new nodes + num new arcs) but with a higher constant.
template <class Graph>
std::unique_ptr<Graph> GetSubgraphOfNodes(const Graph& graph,
                                          const std::vector<int>& nodes);

// This can be used to view a directed graph (that supports reverse arcs)
// from graph.h as un undirected graph: operator[](node) returns a
// pseudo-container that iterates over all nodes adjacent to "node" (from
// outgoing or incoming arcs).
// CAVEAT: Self-arcs (aka loops) will appear twice.
//
// Example:
// ReverseArcsStaticGraph<> dgraph;
// ...
// UndirectedAdjacencyListsOfDirectedGraph<decltype(dgraph)> ugraph(dgraph);
// for (int neighbor_of_node_42 : ugraph[42]) { ... }
template <class Graph>
class UndirectedAdjacencyListsOfDirectedGraph {
 public:
  explicit UndirectedAdjacencyListsOfDirectedGraph(const Graph& graph)
      : graph_(graph) {}

  typedef typename Graph::OutgoingOrOppositeIncomingArcIterator ArcIterator;
  class AdjacencyListIterator : public ArcIterator {
   public:
    explicit AdjacencyListIterator(const Graph& graph, ArcIterator&& arc_it)
        : ArcIterator(arc_it), graph_(graph) {}
    // Overwrite operator* to return the heads of the arcs.
    typename Graph::NodeIndex operator*() const {
      return graph_.Head(ArcIterator::operator*());
    }

   private:
    const Graph& graph_;
  };

  // Returns a pseudo-container of all the nodes adjacent to "node".
  BeginEndWrapper<AdjacencyListIterator> operator[](int node) const {
    const auto& arc_range = graph_.OutgoingOrOppositeIncomingArcs(node);
    return {AdjacencyListIterator(graph_, arc_range.begin()),
            AdjacencyListIterator(graph_, arc_range.end())};
  }

 private:
  const Graph& graph_;
};

// Computes the weakly connected components of a directed graph that
// provides the OutgoingOrOppositeIncomingArcs() API, and returns them
// as a mapping from node to component index. See GetConnectedComponens().
template <class Graph>
std::vector<int> GetWeaklyConnectedComponents(const Graph& graph) {
  return GetConnectedComponents(
      graph.num_nodes(), UndirectedAdjacencyListsOfDirectedGraph<Graph>(graph));
}

// Returns true iff the given vector is a subset of [0..n-1], i.e.
// all elements i are such that 0 <= i < n and no two elements are equal.
// "n" must be >= 0 or the result is undefined.
bool IsSubsetOf0N(const std::vector<int>& v, int n);

// Returns true iff the given vector is a permutation of [0..size()-1].
inline bool IsValidPermutation(const std::vector<int>& v) {
  return IsSubsetOf0N(v, v.size());
}

// Returns a copy of "graph", without self-arcs and duplicate arcs.
template <class Graph>
std::unique_ptr<Graph> RemoveSelfArcsAndDuplicateArcs(const Graph& graph);

// Given an arc path, changes it to a sub-path with the same source and
// destination but without any cycle. Nothing happen if the path was already
// without cycle.
//
// The graph class should support Tail(arc) and Head(arc). They should both
// return an integer representing the corresponding tail/head of the passed arc.
//
// TODO(user): In some cases, there is more than one possible solution. We could
// take some arc costs and return the cheapest path instead. Or return the
// shortest path in term of number of arcs.
template <class Graph>
void RemoveCyclesFromPath(const Graph& graph, std::vector<int>* arc_path);

// Returns true iff the given path contains a cycle.
template <class Graph>
bool PathHasCycle(const Graph& graph, const std::vector<int>& arc_path);

// Returns a vector representing a mapping from arcs to arcs such that each arc
// is mapped to another arc with its (tail, head) flipped, if such an arc
// exists (otherwise it is mapped to -1).
// If the graph is symmetric, the returned mapping is bijective and reflexive,
// i.e. out[out[arc]] = arc for all "arc", where "out" is the returned vector.
// If "die_if_not_symmetric" is true, this function CHECKs() that the graph
// is symmetric.
//
// Self-arcs are always mapped to themselves.
//
// Note that since graphs may have multi-arcs, the mapping isn't necessarily
// unique, hence the function name.
template <class Graph>
std::vector<int> ComputeOnePossibleReverseArcMapping(const Graph& graph,
                                                     bool die_if_not_symmetric);

// Implementations of the templated methods.

template <class Graph>
bool GraphHasSelfArcs(const Graph& graph) {
  for (const auto arc : graph.AllForwardArcs()) {
    if (graph.Tail(arc) == graph.Head(arc)) return true;
  }
  return false;
}

template <class Graph>
bool GraphHasDuplicateArcs(const Graph& graph) {
  typedef typename Graph::ArcIndex ArcIndex;
  typedef typename Graph::NodeIndex NodeIndex;
  std::vector<bool> tmp_node_mask(graph.num_nodes(), false);
  for (const NodeIndex tail : graph.AllNodes()) {
    for (const ArcIndex arc : graph.OutgoingArcs(tail)) {
      const NodeIndex head = graph.Head(arc);
      if (tmp_node_mask[head]) return true;
      tmp_node_mask[head] = true;
    }
    for (const ArcIndex arc : graph.OutgoingArcs(tail)) {
      tmp_node_mask[graph.Head(arc)] = false;
    }
  }
  return false;
}

template <class Graph>
bool GraphIsSymmetric(const Graph& graph) {
  typedef typename Graph::NodeIndex NodeIndex;
  typedef typename Graph::ArcIndex ArcIndex;
  // Create a reverse copy of the graph.
  StaticGraph<NodeIndex, ArcIndex> reverse_graph(graph.num_nodes(),
                                                 graph.num_arcs());
  for (const NodeIndex node : graph.AllNodes()) {
    for (const ArcIndex arc : graph.OutgoingArcs(node)) {
      reverse_graph.AddArc(graph.Head(arc), node);
    }
  }
  reverse_graph.Build();
  // Compare the graph to its reverse, one adjacency list at a time.
  std::vector<ArcIndex> count(graph.num_nodes(), 0);
  for (const NodeIndex node : graph.AllNodes()) {
    for (const ArcIndex arc : graph.OutgoingArcs(node)) {
      ++count[graph.Head(arc)];
    }
    for (const ArcIndex arc : reverse_graph.OutgoingArcs(node)) {
      if (--count[reverse_graph.Head(arc)] < 0) return false;
    }
    for (const ArcIndex arc : graph.OutgoingArcs(node)) {
      if (count[graph.Head(arc)] != 0) return false;
    }
  }
  return true;
}

template <class Graph>
bool GraphIsWeaklyConnected(const Graph& graph) {
  typedef typename Graph::NodeIndex NodeIndex;
  static_assert(std::numeric_limits<NodeIndex>::max() <= INT_MAX,
                "GraphIsWeaklyConnected() isn't yet implemented for graphs"
                " that support more than INT_MAX nodes. Reach out to"
                " or-core-team@ if you need this.");
  if (graph.num_nodes() == 0) return true;
  DenseConnectedComponentsFinder union_find;
  union_find.SetNumberOfNodes(graph.num_nodes());
  for (typename Graph::ArcIndex arc = 0; arc < graph.num_arcs(); ++arc) {
    union_find.AddEdge(graph.Tail(arc), graph.Head(arc));
  }
  return union_find.GetNumberOfComponents() == 1;
}

template <class Graph>
std::unique_ptr<Graph> CopyGraph(const Graph& graph) {
  std::unique_ptr<Graph> new_graph(
      new Graph(graph.num_nodes(), graph.num_arcs()));
  for (const auto node : graph.AllNodes()) {
    for (const auto arc : graph.OutgoingArcs(node)) {
      new_graph->AddArc(node, graph.Head(arc));
    }
  }
  new_graph->Build();
  return new_graph;
}

template <class Graph>
std::unique_ptr<Graph> RemapGraph(const Graph& old_graph,
                                  const std::vector<int>& new_node_index) {
  DCHECK(IsValidPermutation(new_node_index)) << "Invalid permutation";
  const int num_nodes = old_graph.num_nodes();
  CHECK_EQ(new_node_index.size(), num_nodes);
  std::unique_ptr<Graph> new_graph(new Graph(num_nodes, old_graph.num_arcs()));
  typedef typename Graph::NodeIndex NodeIndex;
  typedef typename Graph::ArcIndex ArcIndex;
  for (const NodeIndex node : old_graph.AllNodes()) {
    for (const ArcIndex arc : old_graph.OutgoingArcs(node)) {
      new_graph->AddArc(new_node_index[node],
                        new_node_index[old_graph.Head(arc)]);
    }
  }
  new_graph->Build();
  return new_graph;
}

template <class Graph>
std::unique_ptr<Graph> GetSubgraphOfNodes(const Graph& old_graph,
                                          const std::vector<int>& nodes) {
  typedef typename Graph::NodeIndex NodeIndex;
  typedef typename Graph::ArcIndex ArcIndex;
  DCHECK(IsSubsetOf0N(nodes, old_graph.num_nodes())) << "Invalid subset";
  std::vector<NodeIndex> new_node_index(old_graph.num_nodes(), -1);
  for (NodeIndex new_index = 0; new_index < nodes.size(); ++new_index) {
    new_node_index[nodes[new_index]] = new_index;
  }
  // Do a first pass to count the arcs, so that we don't allocate more memory
  // than needed.
  ArcIndex num_arcs = 0;
  for (const NodeIndex node : nodes) {
    for (const ArcIndex arc : old_graph.OutgoingArcs(node)) {
      if (new_node_index[old_graph.Head(arc)] != -1) ++num_arcs;
    }
  }
  // A second pass where we actually copy the subgraph.
  // NOTE(user): there might seem to be a bit of duplication with RemapGraph(),
  // but there is a key difference: the loop below only iterates on "nodes",
  // which could be much smaller than all the graph's nodes.
  std::unique_ptr<Graph> new_graph(new Graph(nodes.size(), num_arcs));
  for (NodeIndex new_tail = 0; new_tail < nodes.size(); ++new_tail) {
    const NodeIndex old_tail = nodes[new_tail];
    for (const ArcIndex arc : old_graph.OutgoingArcs(old_tail)) {
      const NodeIndex new_head = new_node_index[old_graph.Head(arc)];
      if (new_head != -1) new_graph->AddArc(new_tail, new_head);
    }
  }
  new_graph->Build();
  return new_graph;
}

template <class Graph>
std::unique_ptr<Graph> RemoveSelfArcsAndDuplicateArcs(const Graph& graph) {
  std::unique_ptr<Graph> g(new Graph(graph.num_nodes(), graph.num_arcs()));
  typedef typename Graph::ArcIndex ArcIndex;
  typedef typename Graph::NodeIndex NodeIndex;
  std::vector<bool> tmp_node_mask(graph.num_nodes(), false);
  for (const NodeIndex tail : graph.AllNodes()) {
    for (const ArcIndex arc : graph.OutgoingArcs(tail)) {
      const NodeIndex head = graph.Head(arc);
      if (head != tail && !tmp_node_mask[head]) {
        tmp_node_mask[head] = true;
        g->AddArc(tail, head);
      }
    }
    for (const ArcIndex arc : graph.OutgoingArcs(tail)) {
      tmp_node_mask[graph.Head(arc)] = false;
    }
  }
  g->Build();
  return g;
}

template <class Graph>
void RemoveCyclesFromPath(const Graph& graph, std::vector<int>* arc_path) {
  if (arc_path->empty()) return;

  // This maps each node to the latest arc in the given path that leaves it.
  std::map<int, int> last_arc_leaving_node;
  for (const int arc : *arc_path) last_arc_leaving_node[graph.Tail(arc)] = arc;

  // Special case for the destination.
  // Note that this requires that -1 is not a valid arc of Graph.
  last_arc_leaving_node[graph.Head(arc_path->back())] = -1;

  // Reconstruct the path by starting at the source and then following the
  // "next" arcs. We override the given arc_path at the same time.
  int node = graph.Tail(arc_path->front());
  int new_size = 0;
  while (new_size < arc_path->size()) {  // To prevent cycle on bad input.
    const int arc = gtl::FindOrDie(last_arc_leaving_node, node);
    if (arc == -1) break;
    (*arc_path)[new_size++] = arc;
    node = graph.Head(arc);
  }
  arc_path->resize(new_size);
}

template <class Graph>
bool PathHasCycle(const Graph& graph, const std::vector<int>& arc_path) {
  if (arc_path.empty()) return false;
  std::set<int> seen;
  seen.insert(graph.Tail(arc_path.front()));
  for (const int arc : arc_path) {
    if (!gtl::InsertIfNotPresent(&seen, graph.Head(arc))) return true;
  }
  return false;
}

template <class Graph>
std::vector<int> ComputeOnePossibleReverseArcMapping(
    const Graph& graph, bool die_if_not_symmetric) {
  std::vector<int> reverse_arc(graph.num_arcs(), -1);
  std::unordered_multimap<std::pair</*tail*/ int, /*head*/ int>,
                          /*arc index*/ int>
      arc_map;
  for (int arc = 0; arc < graph.num_arcs(); ++arc) {
    const int tail = graph.Tail(arc);
    const int head = graph.Head(arc);
    if (tail == head) {
      // Special case: directly map any self-arc to itself.
      reverse_arc[arc] = arc;
      continue;
    }
    // Lookup for the reverse arc of the current one...
    auto it = arc_map.find({head, tail});
    if (it != arc_map.end()) {
      // Found a reverse arc! Store the mapping and remove the
      // reverse arc from the map.
      reverse_arc[arc] = it->second;
      reverse_arc[it->second] = arc;
      arc_map.erase(it);
    } else {
      // Reverse arc not in the map. Add the current arc to the map.
      arc_map.insert({{tail, head}, arc});
    }
  }
  // Algorithm check, for debugging.
  DCHECK_EQ(std::count(reverse_arc.begin(), reverse_arc.end(), -1),
            arc_map.size());
  if (die_if_not_symmetric) {
    CHECK_EQ(arc_map.size(), 0)
        << "The graph is not symmetric: " << arc_map.size() << " of "
        << graph.num_arcs() << " arcs did not have a reverse.";
  }
  return reverse_arc;
}

}  // namespace util

#endif  // UTIL_GRAPH_UTIL_H_
