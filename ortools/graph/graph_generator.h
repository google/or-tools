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

#ifndef UTIL_GRAPH_GRAPH_GENERATOR_H_
#define UTIL_GRAPH_GRAPH_GENERATOR_H_

// Generates a complete undirected graph with `num_nodes` nodes.
//
// A complete graph is a graph in which all pairs of distinct nodes are
// connected by an edge.
// The graph is represented using the provided `Graph` template.
// If the chosen graph type requires a call to `Build()`, the user is expected
// to perform this call, possibly after tweaking the graph.
//
// Consider using `CompleteGraph` (ortools/graph/graph.h) instead of this
// function in production code, as it uses constant memory to store the graph.
// Instead, this function explicitly creates the graph using the template type,
// which is mostly useful for tests or when you have to tweak the graph after
// creation (i.e. a complete graph is just the core of your final graph).
//
// Args:
//   num_nodes: The number of nodes in the graph.
//
// Returns:
//   A complete undirected graph.
template <typename Graph>
Graph GenerateCompleteUndirectedGraph(
    const typename Graph::NodeIndex num_nodes) {
  Graph graph;
  graph.Reserve(num_nodes, num_nodes * (num_nodes - 1));
  graph.AddNode(num_nodes - 1);  // Only for degenerate cases with no arcs.
  for (typename Graph::NodeIndex src = 0; src < num_nodes; ++src) {
    for (typename Graph::NodeIndex dst = src + 1; dst < num_nodes; ++dst) {
      graph.AddArc(src, dst);
      graph.AddArc(dst, src);
    }
  }
  return graph;
}

// Generates a complete undirected bipartite graph with `num_nodes_1` and
// `num_nodes_2` nodes in each part.
//
// A complete bipartite graph is a graph in which all pairs of distinct nodes,
// one in each part, are connected by an edge.
// The graph is represented using the provided `Graph` template.
// If the chosen graph type requires a call to `Build()`, the user is expected
// to perform this call, possibly after tweaking the graph.
//
// Args:
//   num_nodes_1: The number of nodes in the first part of the graph.
//   num_nodes_2: The number of nodes in the second part of the graph.
//
// Returns:
//   A complete undirected bipartite graph.
template <typename Graph>
Graph GenerateCompleteUndirectedBipartiteGraph(
    const typename Graph::NodeIndex num_nodes_1,
    const typename Graph::NodeIndex num_nodes_2) {
  Graph graph;
  graph.Reserve(num_nodes_1 + num_nodes_2, 2 * num_nodes_1 * num_nodes_2);
  graph.AddNode(num_nodes_1 + num_nodes_2 - 1);  // Only for degenerate cases.
  for (typename Graph::NodeIndex src = 0; src < num_nodes_1; ++src) {
    for (typename Graph::NodeIndex dst = 0; dst < num_nodes_2; ++dst) {
      graph.AddArc(src, num_nodes_1 + dst);
      graph.AddArc(num_nodes_1 + dst, src);
    }
  }
  return graph;
}

// Generates a complete directed bipartite graph with `num_nodes_1` and
// `num_nodes_2` nodes in each part.
//
// A complete bipartite graph is a graph in which all pairs of distinct nodes,
// one in each part, are connected by an edge. Edges are directed from the first
// part towards the second part.
// The graph is represented using the provided `Graph` template.
// If the chosen graph type requires a call to `Build()`, the user is expected
// to perform this call, possibly after tweaking the graph.
//
// Consider using `CompleteBipartiteGraph` (ortools/graph/graph.h) instead of
// this function in production code, as it uses constant memory to store the
// graph. Instead, this function explicitly creates the graph using the template
// type, which is mostly useful for tests or when you have to tweak the graph
// after creation (i.e. a complete graph is just the core of your final graph).
//
// Args:
//   num_nodes_1: The number of nodes in the first part of the graph.
//   num_nodes_2: The number of nodes in the second part of the graph.
//
// Returns:
//   A complete directed bipartite graph.
template <typename Graph>
Graph GenerateCompleteDirectedBipartiteGraph(
    const typename Graph::NodeIndex num_nodes_1,
    const typename Graph::NodeIndex num_nodes_2) {
  Graph graph;
  graph.Reserve(num_nodes_1 + num_nodes_2, num_nodes_1 * num_nodes_2);
  graph.AddNode(num_nodes_1 + num_nodes_2 - 1);  // Only for degenerate cases.
  for (typename Graph::NodeIndex src = 0; src < num_nodes_1; ++src) {
    for (typename Graph::NodeIndex dst = 0; dst < num_nodes_2; ++dst) {
      graph.AddArc(src, num_nodes_1 + dst);
    }
  }
  return graph;
}

#endif  // UTIL_GRAPH_GRAPH_GENERATOR_H_
