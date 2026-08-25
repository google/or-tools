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

// Generic graph helper, when your nodes are not dense integers.
// Helps using a few of the libraries in this directory. Examples:
//
//   using Graph = util::graph::GenericGraph<std::string>;
//   Graph::Builder builder;
//   builder.AddEdge("foo", "bar");
//   ...
//   const Graph graph = std::move(builder).Build();
//   const std::vector<std::string> cycle =
//       graph.Nodes(util::graph::FindCycleInGraph(graph.Graph()));
//
// It holds a *directed* graph where nodes can be any type supporting either
// hash or <. It is a thin wrapper around StaticGraph (see util/graph/graph.h)
// and does the bidirectional mapping from nodes objects to dense integer
// indices for you.
// Use this when you need to invoke an algorithm on StaticGraph but your nodes
// are not already dense integers.
// Note the arcs are permuted a construction time: if you re-access them via,
// e.g., graph.Graph().AllForwardArcs(), they won't be in insertion order.
//
// The API can be extended as needed, contact util-graph@.

#ifndef UTIL_GRAPH_GENERIC_GRAPH_H_
#define UTIL_GRAPH_GENERIC_GRAPH_H_

#include <optional>
#include <utility>
#include <vector>

#include "absl/types/span.h"
#include "ortools/graph_base/graph.h"
#include "ortools/graph_base/hash_or_tree_container.h"
#include "ortools/graph_base/index.h"

namespace util {
namespace graph {

// NodeT must be movable.
template <typename NodeT,
          typename CompareOrHashNodeT = PreferHashOrCompare<NodeT>>
class GenericGraph {
 public:
  class Builder;  // Defined below.
  // Direct builder from edges. Else, use GenericGraph::Builder.
  static GenericGraph FromEdges(absl::Span<const std::pair<NodeT, NodeT>> arcs);

  const StaticGraph<>& Graph() const { return graph_; }

  absl::Span<const NodeT> AllNodes() const { return nodes_.span(); }

  // Converts 1 or several node indices to the corresponding Node(s).
  const NodeT& Node(int node_id) const { return nodes_[node_id]; }
  std::vector<NodeT> Nodes(absl::Span<const int> node_ids) const;

  // Returns std::nullopt if the node is not in the graph.
  std::optional<int> LookupNode(const NodeT& node) const;

 private:
  util::StaticGraph<> graph_;
  Index<NodeT, CompareOrHashNodeT> nodes_;

  GenericGraph() = default;
};

template <typename NodeT, typename CompareOrHashNodeT>
class GenericGraph<NodeT, CompareOrHashNodeT>::Builder {
 public:
  Builder() = default;
  // If you know the number of nodes and edges in advance (or a somewhat tight
  // upper bound), this is faster thanks to reserve(). Adding more nodes
  // DCHECK-fails. Adding fewer is fine. For edges, no hard constraints.
  Builder(int num_nodes, int num_edges)
      : nodes_(num_nodes), builder_(num_nodes, num_edges) {}

  // Returns the internal node id (a dense index in 0..num_nodes-1). Nodes are
  // unique: duplicate nodes get the same id and correspond to a single node.
  template <typename U>
  int LookupOrAddNode(U&& node);

  // Automatically adds the nodes if they have not been added yet. If both are
  // new, then 'from' is added before 'to'.
  template <typename U, typename V>
  void AddEdge(U&& from, V&& to);

  // You must std::move() the Builder to call this. Returns the immutable graph.
  GenericGraph<NodeT, CompareOrHashNodeT> Build() &&;

  // Convenient reader functions, available mid-build.
  absl::Span<const NodeT> AllNodes() const { return nodes_.span(); }
  std::optional<int> LookupNode(const NodeT& node) const;

 private:
  Index<NodeT, CompareOrHashNodeT> nodes_;
  util::StaticGraph<>::Builder builder_;
};

template <typename NodeT, typename CompareOrHashNodeT>
template <typename U>
int GenericGraph<NodeT, CompareOrHashNodeT>::Builder::LookupOrAddNode(
    U&& node) {
  const auto [node_id, inserted] = nodes_.TryEmplace(node);
  if (inserted) builder_.AddNode(node_id);
  return node_id;
}

template <typename NodeT, typename CompareOrHashNodeT>
std::optional<int> GenericGraph<NodeT, CompareOrHashNodeT>::Builder::LookupNode(
    const NodeT& node) const {
  return nodes_.Lookup(node);
}

template <typename NodeT, typename CompareOrHashNodeT>
template <typename U, typename V>
void GenericGraph<NodeT, CompareOrHashNodeT>::Builder::AddEdge(U&& from,
                                                               V&& to) {
  // Don't inline the calls to LookupOrAddNode(), so that we determinisitically
  // always insert 'from' before 'to'.
  const int from_id = LookupOrAddNode(from);
  const int to_id = LookupOrAddNode(to);
  builder_.AddArc(from_id, to_id);
}

template <typename NodeT, typename CompareOrHashNodeT>
GenericGraph<NodeT, CompareOrHashNodeT>
GenericGraph<NodeT, CompareOrHashNodeT>::Builder::Build() && {
  GenericGraph<NodeT, CompareOrHashNodeT> graph;
  graph.graph_ = std::move(builder_).BuildGraph(/*permutation=*/nullptr);
  graph.nodes_ = std::move(nodes_);
  return graph;
}

template <typename NodeT, typename CompareOrHashNodeT>
std::optional<int> GenericGraph<NodeT, CompareOrHashNodeT>::LookupNode(
    const NodeT& node) const {
  return nodes_.Lookup(node);
}

template <typename NodeT, typename CompareOrHashNodeT>
std::vector<NodeT> GenericGraph<NodeT, CompareOrHashNodeT>::Nodes(
    absl::Span<const int> node_ids) const {
  std::vector<NodeT> nodes;
  nodes.reserve(node_ids.size());
  for (const int node_id : node_ids) {
    nodes.push_back(nodes_[node_id]);
  }
  return nodes;
}

// static
template <typename NodeT, typename CompareOrHashNodeT>
GenericGraph<NodeT, CompareOrHashNodeT>
GenericGraph<NodeT, CompareOrHashNodeT>::FromEdges(
    absl::Span<const std::pair<NodeT, NodeT>> arcs) {
  GenericGraph<NodeT, CompareOrHashNodeT>::Builder builder;
  for (const auto& [from, to] : arcs) {
    builder.AddEdge(from, to);
  }
  return std::move(builder).Build();
}

template <typename NodeT,
          typename CompareOrHashNodeT = PreferHashOrCompare<NodeT>>
using GenericGraphBuilder =
    typename GenericGraph<NodeT, CompareOrHashNodeT>::Builder;

}  // namespace graph
}  // namespace util

#endif  // UTIL_GRAPH_GENERIC_GRAPH_H_
