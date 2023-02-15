// Copyright 2010-2022 Google LLC
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

// Utility to build Eulerian paths and tours on a graph. For more information,
// see https://en.wikipedia.org/wiki/Eulerian_path.
// As of 10/2015, only undirected graphs are supported.
//
// Usage:
// - Building an Eulerian tour on a ReverseArcListGraph:
//   ReverseArcListGraph<int, int> graph;
//   // Fill graph
//   std::vector<int> tour = BuildEulerianTour(graph);
//
// - Building an Eulerian path on a ReverseArcListGraph:
//   ReverseArcListGraph<int, int> graph;
//   // Fill graph
//   std::vector<int> tour = BuildEulerianPath(graph);
//
#ifndef OR_TOOLS_GRAPH_EULERIAN_PATH_H_
#define OR_TOOLS_GRAPH_EULERIAN_PATH_H_

#include <vector>

#include "ortools/base/logging.h"

namespace operations_research {

namespace internal {
template <typename Graph>
bool GraphIsConnected(const Graph& graph);
}  // namespace internal

// Returns true if a graph is Eulerian, aka all its nodes are of even degree.
template <typename Graph>
bool IsEulerianGraph(const Graph& graph, bool assume_connectivity = true) {
  typedef typename Graph::NodeIndex NodeIndex;
  for (const NodeIndex node : graph.AllNodes()) {
    if ((graph.OutDegree(node) + graph.InDegree(node)) % 2 != 0) {
      return false;
    }
  }
  return assume_connectivity || internal::GraphIsConnected(graph);
}

// Returns true if a graph is Semi-Eulerian, aka at most two of its nodes are of
// odd degree.
// odd_nodes is filled with odd nodes of the graph.
template <typename NodeIndex, typename Graph>
bool IsSemiEulerianGraph(const Graph& graph, std::vector<NodeIndex>* odd_nodes,
                         bool assume_connectivity = true) {
  CHECK(odd_nodes != nullptr);
  for (const NodeIndex node : graph.AllNodes()) {
    const int degree = graph.OutDegree(node) + graph.InDegree(node);
    if (degree % 2 != 0) {
      odd_nodes->push_back(node);
    }
  }
  if (odd_nodes->size() > 2) return false;
  return assume_connectivity || internal::GraphIsConnected(graph);
}

// Builds an Eulerian path/trail on an undirected graph starting from node root.
// Supposes the graph is connected and is eulerian or semi-eulerian.
// This is an implementation of Hierholzer's algorithm.
// If m is the number of edges in the graph and n the number of nodes, time
// and memory complexity is O(n + m).
template <typename NodeIndex, typename Graph>
std::vector<NodeIndex> BuildEulerianPathFromNode(const Graph& graph,
                                                 NodeIndex root) {
  typedef typename Graph::ArcIndex ArcIndex;
  std::vector<bool> unvisited_edges(graph.num_arcs(), true);
  std::vector<NodeIndex> tour;
  if (graph.IsNodeValid(root)) {
    std::vector<NodeIndex> tour_stack = {root};
    std::vector<ArcIndex> active_arcs(graph.num_nodes());
    for (const NodeIndex node : graph.AllNodes()) {
      active_arcs[node] = *(graph.OutgoingOrOppositeIncomingArcs(node)).begin();
    }
    while (!tour_stack.empty()) {
      const NodeIndex node = tour_stack.back();
      bool has_unvisited_edges = false;
      for (const ArcIndex arc :
           graph.OutgoingOrOppositeIncomingArcsStartingFrom(
               node, active_arcs[node])) {
        const ArcIndex edge = arc < 0 ? graph.OppositeArc(arc) : arc;
        if (unvisited_edges[edge]) {
          has_unvisited_edges = true;
          active_arcs[node] = arc;
          tour_stack.push_back(graph.Head(arc));
          unvisited_edges[edge] = false;
          break;
        }
      }
      if (!has_unvisited_edges) {
        tour.push_back(node);
        tour_stack.pop_back();
      }
    }
  }
  return tour;
}

// Builds an Eulerian tour/circuit/cycle starting and ending at node root on an
// undirected graph.
// This function works only on Reverse graphs
// (cf. ortools/graph/graph.h).
// Returns an empty tour if either root is invalid or if a tour cannot be built.
template <typename NodeIndex, typename Graph>
std::vector<NodeIndex> BuildEulerianTourFromNode(
    const Graph& graph, NodeIndex root, bool assume_connectivity = true) {
  std::vector<NodeIndex> tour;
  if (IsEulerianGraph(graph, assume_connectivity)) {
    tour = BuildEulerianPathFromNode(graph, root);
  }
  return tour;
}

// Same as above but without specifying a start/end root node (node 0 is taken
// as default root).
template <typename Graph>
std::vector<typename Graph::NodeIndex> BuildEulerianTour(
    const Graph& graph, bool assume_connectivity = true) {
  return BuildEulerianTourFromNode(graph, 0, assume_connectivity);
}

// Builds an Eulerian path/trail on an undirected graph.
// This function works only on Reverse graphs
// (cf. ortools/graph/graph.h).
// Returns an empty tour if a tour cannot be built.
template <typename Graph>
std::vector<typename Graph::NodeIndex> BuildEulerianPath(
    const Graph& graph, bool assume_connectivity = true) {
  typedef typename Graph::NodeIndex NodeIndex;
  std::vector<NodeIndex> path;
  std::vector<NodeIndex> roots;
  if (IsSemiEulerianGraph(graph, &roots, assume_connectivity)) {
    const NodeIndex root = roots.empty() ? 0 : roots.back();
    path = BuildEulerianPathFromNode(graph, root);
  }
  return path;
}

namespace internal {
template <typename Graph>
bool GraphIsConnected(const Graph& graph) {
  typedef typename Graph::NodeIndex NodeIndex;
  const NodeIndex n = graph.num_nodes();
  if (n <= 1) return true;
  // We use iterative DFS, which is probably the fastest.
  NodeIndex num_visited = 1;
  std::vector<NodeIndex> stack = {0};
  std::vector<bool> visited(n, false);
  while (!stack.empty()) {
    const NodeIndex node = stack.back();
    stack.pop_back();
    for (auto arc : graph.OutgoingOrOppositeIncomingArcs(node)) {
      const NodeIndex neigh = graph.Head(arc);
      if (!visited[neigh]) {
        visited[neigh] = true;
        stack.push_back(neigh);
        if (++num_visited == n) return true;
      }
    }
  }
  return false;
}
}  // namespace internal

}  // namespace operations_research

#endif  // OR_TOOLS_GRAPH_EULERIAN_PATH_H_
