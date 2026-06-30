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

#include <utility>

// Generates a complete undirected graph with `num_nodes` nodes.
//
// A complete graph is a graph in which all pairs of distinct nodes are
// connected by an edge.
// The graph is represented using the provided `Graph` template.
//
// Consider using `CompleteGraph` (util/graph/graph.h) instead of this function
// in production code, as it uses constant memory to store the graph. Instead,
// this function explicitly creates the graph using the template type, which is
// mostly useful for tests or when you have to tweak the graph after creation
// (i.e. a complete graph is just the core of your final graph).
//
// Args:
//   num_nodes: The number of nodes in the graph.
//
// Returns:
//   A complete undirected graph.
template <typename Graph>
Graph GenerateCompleteUndirectedGraph(
    const typename Graph::NodeIndex num_nodes) {
  typename Graph::Builder builder;
  builder.Reserve(num_nodes, num_nodes * (num_nodes - 1));
  builder.AddNode(num_nodes - 1);  // Only for degenerate cases with no arcs.
  for (typename Graph::NodeIndex src = 0; src < num_nodes; ++src) {
    for (typename Graph::NodeIndex dst = src + 1; dst < num_nodes; ++dst) {
      builder.AddArc(src, dst);
      builder.AddArc(dst, src);
    }
  }
  return std::move(*std::move(builder).Build(nullptr));
}

// Generates a complete undirected bipartite graph with `num_nodes_1` and
// `num_nodes_2` nodes in each part.
//
// A complete bipartite graph is a graph in which all pairs of distinct nodes,
// one in each part, are connected by an edge.
// The graph is represented using the provided `Graph` template.
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
  typename Graph::Builder builder;
  builder.Reserve(num_nodes_1 + num_nodes_2, 2 * num_nodes_1 * num_nodes_2);
  builder.AddNode(num_nodes_1 + num_nodes_2 - 1);  // Only for degenerate cases.
  for (typename Graph::NodeIndex src = 0; src < num_nodes_1; ++src) {
    for (typename Graph::NodeIndex dst = 0; dst < num_nodes_2; ++dst) {
      builder.AddArc(src, num_nodes_1 + dst);
      builder.AddArc(num_nodes_1 + dst, src);
    }
  }
  return std::move(*std::move(builder).Build(nullptr));
}

// Generates a complete directed bipartite graph with `num_nodes_1` and
// `num_nodes_2` nodes in each part.
//
// A complete bipartite graph is a graph in which all pairs of distinct nodes,
// one in each part, are connected by an edge. Edges are directed from the first
// part towards the second part.
// The graph is represented using the provided `Graph` template.
//
// Consider using `CompleteBipartiteGraph` (util/graph/graph.h) instead of this
// function in production code, as it uses constant memory to store the graph.
// Instead, this function explicitly creates the graph using the template type,
// which is mostly useful for tests or when you have to tweak the graph after
// creation (i.e. a complete graph is just the core of your final graph).
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
  typename Graph::Builder builder;
  builder.Reserve(num_nodes_1 + num_nodes_2, num_nodes_1 * num_nodes_2);
  builder.AddNode(num_nodes_1 + num_nodes_2 - 1);  // Only for degenerate cases.
  for (typename Graph::NodeIndex src = 0; src < num_nodes_1; ++src) {
    for (typename Graph::NodeIndex dst = 0; dst < num_nodes_2; ++dst) {
      builder.AddArc(src, num_nodes_1 + dst);
    }
  }
  return std::move(*std::move(builder).Build(nullptr));
}

#endif  // UTIL_GRAPH_GRAPH_GENERATOR_H_
