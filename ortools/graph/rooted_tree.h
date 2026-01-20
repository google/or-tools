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

// Find paths and compute path distances between nodes on a rooted tree.
//
// A tree is a connected undirected graph with no cycles. A rooted tree is a
// directed graph derived from a tree, where a node is designated as the root,
// and then all edges are directed towards the root.
//
// This file provides the class RootedTree, which stores a rooted tree on dense
// integer nodes a single vector, and a function RootedTreeFromGraph(), which
// converts the adjacency list of a an undirected tree to a RootedTree.

#ifndef ORTOOLS_GRAPH_ROOTED_TREE_H_
#define ORTOOLS_GRAPH_ROOTED_TREE_H_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/types/span.h"
#include "ortools/base/status_builder.h"
#include "ortools/base/status_macros.h"

namespace operations_research {

// A tree is an undirected graph with no cycles, n nodes, and n-1 undirected
// edges. Consequently, a tree is connected. Given a tree on the nodes [0..n),
// a RootedTree picks any node to be the root, and then converts all edges into
// (directed) arcs pointing at the root. Each node has one outgoing edge, so we
// can store the adjacency list of this directed view of the graph as a single
// vector of integers with length equal to the number of nodes. At the root
// index, we store RootedTree::kNullParent=-1, and at every other index, we
// store the next node towards the root (the parent in the tree).
//
// This class is templated on the NodeType, which must be an integer type, e.g.,
// int or int32_t (signed and unsigned types both work).
//
// The following operations are supported:
//  * Path from node to root in O(path length to root)
//  * Lowest Common Ancestor (LCA) of two nodes in O(path length between nodes)
//  * Depth of all nodes in O(num nodes)
//  * Topological sort in O(num nodes)
//  * Path between any two nodes in O(path length between nodes)
//
// Users can provide a vector<double> of arc lengths (indexed by source) to get:
//  * Distance from node to root in O(path length to root)
//  * Distance from all nodes to root in O(num nodes)
//  * Distance between any two nodes in O(path length between nodes)
//
// Operations on rooted trees are generally more efficient than on adjacency
// list representations because the entire tree is in one contiguous allocation.
// There is also an asymptotic advantage on path finding problems.
//
// Two methods for finding the LCA are provided. The first requires the depth of
// every node ahead of time. The second requires a workspace of n bools, all
// starting at false. These values are modified and restored to false when the
// LCA computation finishes. In both cases, if the depths/workspace allocation
// is an O(n) precomputation, then the LCA runs in O(path length).
// Non-asymptotically, the depth method requires more precomputation, but the
// LCA is faster and does not require the user to manage mutable state (i.e.,
// may be better for multi-threaded computation).
//
// An operation that is missing is bulk LCA, see
// https://en.wikipedia.org/wiki/Tarjan%27s_off-line_lowest_common_ancestors_algorithm.
template <typename NodeType = int32_t>
class RootedTree {
 public:
  static constexpr NodeType kNullParent = static_cast<NodeType>(-1);
  // Like the constructor but checks that the tree is valid. Uses O(num nodes)
  // temporary space with O(log(n)) allocations.
  //
  // If the input is cyclic, an InvalidArgument error will be returned with
  // "cycle" as a substring. Further, if error_cycle is not null, it will be
  // cleared and then set to contain the cycle. We will not modify error cycle
  // or return an error message containing the string cycle if there is no
  // cycle. The cycle output will always begin with its smallest element.
  static absl::StatusOr<RootedTree> Create(
      NodeType root, std::vector<NodeType> parents,
      std::vector<NodeType>* error_cycle = nullptr,
      std::vector<NodeType>* topological_order = nullptr);

  // Like Create(), but data is not validated (UB on bad input).
  explicit RootedTree(NodeType root, std::vector<NodeType> parents)
      : root_(root), parents_(std::move(parents)) {}

  // The root node of this rooted tree.
  NodeType root() const { return root_; }

  // The number of nodes in this rooted tree.
  NodeType num_nodes() const { return static_cast<NodeType>(parents_.size()); }

  // A vector that holds the parent of each non root node, and kNullParent at
  // the root.
  absl::Span<const NodeType> parents() const { return parents_; }

  // Returns the path from `node` to `root()` as a vector of nodes starting with
  // `node`.
  std::vector<NodeType> PathToRoot(NodeType node) const;

  // Returns the path from `root()` to `node` as a vector of nodes starting with
  // `node`.
  std::vector<NodeType> PathFromRoot(NodeType node) const;

  // Returns the sum of the arc lengths of the arcs in the path from `start` to
  // `root()`.
  //
  // `arc_lengths[i]` is the length of the arc from node i to `parents()[i]`.
  // `arc_lengths` must have size equal to `num_nodes()` or else we CHECK fail.
  // The value of `arc_lengths[root()]` is unused.
  double DistanceToRoot(NodeType start,
                        absl::Span<const double> arc_lengths) const;

  // Returns the path from `start` to `root()` as a vector of nodes starting
  // with `start`, and the sum of the arc lengths of the arcs in the path.
  //
  // `arc_lengths[i]` is the length of the arc from node i to `parents()[i]`.
  // `arc_lengths` must have size equal to `num_nodes()` or else we CHECK fail.
  // The value of `arc_lengths[root()]` is unused.
  std::pair<double, std::vector<NodeType>> DistanceAndPathToRoot(
      NodeType start, absl::Span<const double> arc_lengths) const;

  // Returns the path from `start` to `end` as a vector of nodes starting with
  // `start` and ending with `end`.
  //
  // `lca` is the lowest common ancestor of `start` and `end`. This can be
  // computed using LowestCommonAncestorByDepth() or
  // LowestCommonAncestorByDepth(), both defined on this class.
  //
  // Runs in time O(path length).
  std::vector<NodeType> Path(NodeType start, NodeType end, NodeType lca) const;

  // Returns the sum of the arc lengths of the arcs in the path from `start` to
  // `end`.
  //
  // `lca` is the lowest common ancestor of `start` and `end`. This can be
  // computed using LowestCommonAncestorByDepth() or
  // LowestCommonAncestorByDepth(), both defined on this class.
  //
  // `arc_lengths[i]` is the length of the arc from node i to `parents()[i]`.
  // `arc_lengths` must have size equal to `num_nodes()` or else we CHECK fail.
  // The value of `arc_lengths[root()]` is unused.
  //
  // Runs in time O(number of edges connecting start to end).
  double Distance(NodeType start, NodeType end, NodeType lca,
                  absl::Span<const double> arc_lengths) const;

  // Returns the path from `start` to `end` as a vector of nodes starting with
  // `start`, and the sum of the arc lengths of the arcs in the path.
  //
  // `lca` is the lowest common ancestor of `start` and `end`. This can be
  // computed using LowestCommonAncestorByDepth() or
  // LowestCommonAncestorByDepth(), both defined on this class.
  //
  // `arc_lengths[i]` is the length of the arc from node i to `parents()[i]`.
  // `arc_lengths` must have size equal to `num_nodes()` or else we CHECK fail.
  // The value of `arc_lengths[root()]` is unused.
  //
  // Runs in time O(number of edges connecting start to end).
  std::pair<double, std::vector<NodeType>> DistanceAndPath(
      NodeType start, NodeType end, NodeType lca,
      absl::Span<const double> arc_lengths) const;

  // Given a path of nodes, returns the sum of the length of the arcs connecting
  // them.
  //
  // `path` must be a list of nodes in the tree where
  // path[i+1] == parents()[path[i]].
  //
  // `arc_lengths[i]` is the length of the arc from node i to `parents()[i]`.
  // `arc_lengths` must have size equal to `num_nodes()` or else we CHECK fail.
  // The value of `arc_lengths[root()]` is unused.
  double DistanceOfPath(absl::Span<const NodeType> path,
                        absl::Span<const double> arc_lengths) const;

  // Returns a topological ordering of the nodes where the root is first and
  // every other node appears after its parent.
  std::vector<NodeType> TopologicalSort() const;

  // Returns the distance of every node from `root()`, if the edge leaving node
  // i has length costs[i].
  //
  // `arc_lengths[i]` is the length of the arc from node i to `parents()[i]`.
  // `arc_lengths` must have size equal to `num_nodes()` or else we CHECK fail.
  // The value of `arc_lengths[root()]` is unused.
  //
  // If you already have a topological order, prefer
  // `AllDistances(absl::Span<const double> costs,
  //               absl::Span<const int>& topological_order)`.
  template <typename T>
  std::vector<T> AllDistancesToRoot(absl::Span<const T> arc_lengths) const;

  // Returns the distance from every node to root().
  //
  // `arc_lengths[i]` is the length of the arc from node i to `parents()[i]`.
  // `arc_lengths` must have size equal to `num_nodes()` or else we CHECK fail.
  // The value of `arc_lengths[root()]` is unused.
  //
  // `topological_order` must have size equal to `num_nodes()` and start with
  // `root()`, or else we CHECK fail. It can be any topological over nodes when
  // the orientation of the arcs from rooting the tree is reversed.
  template <typename T>
  std::vector<T> AllDistancesToRoot(
      absl::Span<const T> arc_lengths,
      absl::Span<const NodeType> topological_order) const;

  // Returns the distance (arcs to move over) from every node to the root.
  //
  // If you already have a topological order, prefer
  // AllDepths(absl::Span<const NodeType>).
  std::vector<NodeType> AllDepths() const {
    return AllDepths(TopologicalSort());
  }

  // Returns the distance (arcs to move over) from every node to the root.
  //
  // `topological_order` must have size equal to `num_nodes()` and start with
  // `root()`, or else we CHECK fail. It can be any topological over nodes when
  // the orientation of the arcs from rooting the tree is reversed.
  std::vector<NodeType> AllDepths(
      absl::Span<const NodeType> topological_order) const;

  // Returns the lowest common ancestor of n1 and n2.
  //
  // `depths` must have size equal to `num_nodes()`, or else we CHECK fail.
  // Values must be the distance of each node to the root in arcs (see
  // AllDepths()).
  NodeType LowestCommonAncestorByDepth(NodeType n1, NodeType n2,
                                       absl::Span<const NodeType> depths) const;

  // Returns the lowest common ancestor of n1 and n2.
  //
  // `visited_workspace` must be a vector with num_nodes() size, or else we
  // CHECK fail. All values of `visited_workspace` should be false. It will be
  // modified and then restored to its starting state.
  NodeType LowestCommonAncestorBySearch(
      NodeType n1, NodeType n2, std::vector<bool>& visited_workspace) const;

  // Modifies the tree in place to change the root. Runs in
  // O(path length from root() to new_root).
  void Evert(NodeType new_root);

 private:
  static_assert(std::is_integral_v<NodeType>,
                "NodeType must be an integral type.");
  static_assert(sizeof(NodeType) <= sizeof(std::size_t),
                "NodeType cannot be larger than size_t, because NodeType is "
                "used to index into std::vector.");

  // Returns the number of nodes appended.
  NodeType AppendToPath(NodeType start, NodeType end,
                        std::vector<NodeType>& path) const;

  // Returns the number of nodes appended.
  NodeType ReverseAppendToPath(NodeType start, NodeType end,
                               std::vector<NodeType>& path) const;

  // Like AllDistancestoRoot(), but the input arc_lengths is mutated to hold
  // the output, instead of just returning the output as a new vector.
  template <typename T>
  void AllDistancesToRootInPlace(
      absl::Span<const NodeType> topological_order,
      absl::Span<T> arc_lengths_in_distances_out) const;

  // Returns the cost of the path from start to end.
  //
  // end must be either equal to an or ancestor of start in the tree (otherwise
  // DCHECK/UB).
  //
  // `arc_lengths[i]` is the length of the arc from node i to `parents()[i]`.
  // `arc_lengths` must have size equal to `num_nodes()` or else we CHECK fail.
  // The value of `arc_lengths[root()]` is unused.
  double DistanceOfUpwardPath(NodeType start, NodeType end,
                              absl::Span<const double> arc_lengths) const;

  int root_;
  std::vector<NodeType> parents_;  // kNullParent=-1 if root
};

////////////////////////////////////////////////////////////////////////////////
// Graph API
////////////////////////////////////////////////////////////////////////////////

// Converts an adjacency list representation of an undirected tree into a rooted
// tree.
//
// Graph must meet the API defined in ortools/graph_base/graph.h, e.g.,
// StaticGraph or ListGraph. Note that these are directed graph APIs, so they
// must have both forward and backward arcs for each edge in the tree.
//
// Graph must be a tree when viewed as an undirected graph.
//
// If topological_order is not null, it is set to a vector with one entry for
// each node giving a topological ordering over the nodes of the graph, with the
// root first.
//
// If depths is not null, it is set to a vector with one entry for each node,
// giving the depth in the tree of that node (the root has depth zero).
template <typename Graph>
absl::StatusOr<RootedTree<typename Graph::NodeType>> RootedTreeFromGraph(
    typename Graph::NodeType root, const Graph& graph,
    std::vector<typename Graph::NodeType>* topological_order = nullptr,
    std::vector<typename Graph::NodeType>* depths = nullptr);

////////////////////////////////////////////////////////////////////////////////
// Template implementations
////////////////////////////////////////////////////////////////////////////////

namespace internal {

template <typename NodeType>
bool IsValidParent(const NodeType node, const NodeType num_tree_nodes) {
  return node == RootedTree<NodeType>::kNullParent ||
         (node >= NodeType{0} && node < num_tree_nodes);
}

template <typename NodeType>
absl::Status IsValidNode(const NodeType node, const NodeType num_tree_nodes) {
  if (node < NodeType{0} || node >= num_tree_nodes) {
    return util::InvalidArgumentErrorBuilder()
           << "nodes must be in [0.." << num_tree_nodes
           << "), found bad node: " << node;
  }
  return absl::OkStatus();
}

template <typename NodeType>
std::vector<NodeType> ExtractCycle(absl::Span<const NodeType> parents,
                                   const NodeType node_in_cycle) {
  std::vector<NodeType> cycle;
  cycle.push_back(node_in_cycle);
  for (NodeType i = parents[node_in_cycle]; i != node_in_cycle;
       i = parents[i]) {
    CHECK_NE(i, RootedTree<NodeType>::kNullParent)
        << "node_in_cycle: " << node_in_cycle
        << " not in cycle, reached the root";
    cycle.push_back(i);
    CHECK_LE(cycle.size(), parents.size())
        << "node_in_cycle: " << node_in_cycle
        << " not in cycle, just (transitively) leads to a cycle";
  }
  absl::c_rotate(cycle, absl::c_min_element(cycle));
  cycle.push_back(cycle[0]);
  return cycle;
}

template <typename NodeType>
std::string CycleErrorMessage(absl::Span<const NodeType> cycle) {
  CHECK_GT(cycle.size(), 0);
  const NodeType start = cycle[0];
  std::string cycle_string;
  if (cycle.size() > 10) {
    cycle_string = absl::StrCat(
        absl::StrJoin(absl::MakeConstSpan(cycle).subspan(0, 8), ", "),
        ", ..., ", start);
  } else {
    cycle_string = absl::StrJoin(cycle, ", ");
  }
  return absl::StrCat("found cycle of size: ", cycle.size(),
                      " with nodes: ", cycle_string);
}

// Every element of parents must be in {kNullParent} union [0..parents.size()),
// otherwise UB.
template <typename NodeType>
std::vector<NodeType> CheckForCycle(absl::Span<const NodeType> parents,
                                    std::vector<NodeType>* topological_order) {
  const NodeType n = static_cast<NodeType>(parents.size());
  if (topological_order != nullptr) {
    topological_order->clear();
    topological_order->reserve(n);
  }
  std::vector<bool> visited(n);
  std::vector<NodeType> dfs_stack;
  for (NodeType i = 0; i < n; ++i) {
    if (visited[i]) {
      continue;
    }
    NodeType next = i;
    while (next != RootedTree<NodeType>::kNullParent && !visited[next]) {
      dfs_stack.push_back(next);
      if (dfs_stack.size() > n) {
        if (topological_order != nullptr) {
          topological_order->clear();
        }
        return ExtractCycle(parents, next);
      }
      next = parents[next];
      DCHECK(IsValidParent(next, n)) << "next: " << next << ", n: " << n;
    }
    absl::c_reverse(dfs_stack);
    for (const NodeType j : dfs_stack) {
      visited[j] = true;
      if (topological_order != nullptr) {
        topological_order->push_back(j);
      }
    }
    dfs_stack.clear();
  }
  return {};
}

}  // namespace internal

template <typename NodeType>
NodeType RootedTree<NodeType>::AppendToPath(const NodeType start,
                                            const NodeType end,
                                            std::vector<NodeType>& path) const {
  NodeType num_new = 0;
  for (NodeType node = start; node != end; node = parents_[node]) {
    DCHECK_NE(node, kNullParent);
    path.push_back(node);
    num_new++;
  }
  path.push_back(end);
  return num_new + 1;
}

template <typename NodeType>
NodeType RootedTree<NodeType>::ReverseAppendToPath(
    NodeType start, NodeType end, std::vector<NodeType>& path) const {
  NodeType result = AppendToPath(start, end, path);
  std::reverse(path.end() - result, path.end());
  return result;
}

template <typename NodeType>
double RootedTree<NodeType>::DistanceOfUpwardPath(
    const NodeType start, const NodeType end,
    absl::Span<const double> arc_lengths) const {
  CHECK_EQ(num_nodes(), arc_lengths.size());
  double distance = 0.0;
  for (NodeType next = start; next != end; next = parents_[next]) {
    DCHECK_NE(next, root_);
    distance += arc_lengths[next];
  }
  return distance;
}

template <typename NodeType>
absl::StatusOr<RootedTree<NodeType>> RootedTree<NodeType>::Create(
    const NodeType root, std::vector<NodeType> parents,
    std::vector<NodeType>* error_cycle,
    std::vector<NodeType>* topological_order) {
  const NodeType num_nodes = static_cast<NodeType>(parents.size());
  RETURN_IF_ERROR(internal::IsValidNode(root, num_nodes)) << "invalid root";
  if (parents[root] != kNullParent) {
    return util::InvalidArgumentErrorBuilder()
           << "root should have no parent (-1), but found parent of: "
           << parents[root];
  }
  for (NodeType i = 0; i < num_nodes; ++i) {
    if (i == root) {
      continue;
    }
    RETURN_IF_ERROR(internal::IsValidNode(parents[i], num_nodes))
        << "invalid value for parent of node: " << i;
  }
  std::vector<NodeType> cycle =
      internal::CheckForCycle(absl::MakeConstSpan(parents), topological_order);
  if (!cycle.empty()) {
    std::string error_message =
        internal::CycleErrorMessage(absl::MakeConstSpan(cycle));
    if (error_cycle != nullptr) {
      *error_cycle = std::move(cycle);
    }
    return absl::InvalidArgumentError(std::move(error_message));
  }
  return RootedTree(root, std::move(parents));
}

template <typename NodeType>
std::vector<NodeType> RootedTree<NodeType>::PathToRoot(
    const NodeType node) const {
  std::vector<NodeType> path;
  for (NodeType next = node; next != root_; next = parents_[next]) {
    path.push_back(next);
  }
  path.push_back(root_);
  return path;
}

template <typename NodeType>
std::vector<NodeType> RootedTree<NodeType>::PathFromRoot(
    const NodeType node) const {
  std::vector<NodeType> result = PathToRoot(node);
  absl::c_reverse(result);
  return result;
}

template <typename NodeType>
std::vector<NodeType> RootedTree<NodeType>::TopologicalSort() const {
  std::vector<NodeType> result;
  const std::vector<NodeType> cycle =
      internal::CheckForCycle(absl::MakeConstSpan(parents_), &result);
  CHECK(cycle.empty()) << internal::CycleErrorMessage(
      absl::MakeConstSpan(cycle));
  return result;
}

template <typename NodeType>
double RootedTree<NodeType>::DistanceToRoot(
    const NodeType start, absl::Span<const double> arc_lengths) const {
  return DistanceOfUpwardPath(start, root_, arc_lengths);
}

template <typename NodeType>
std::pair<double, std::vector<NodeType>>
RootedTree<NodeType>::DistanceAndPathToRoot(
    const NodeType start, absl::Span<const double> arc_lengths) const {
  CHECK_EQ(num_nodes(), arc_lengths.size());
  double distance = 0.0;
  std::vector<NodeType> path;
  for (NodeType next = start; next != root_; next = parents_[next]) {
    path.push_back(next);
    distance += arc_lengths[next];
  }
  path.push_back(root_);
  return {distance, path};
}

template <typename NodeType>
std::vector<NodeType> RootedTree<NodeType>::Path(const NodeType start,
                                                 const NodeType end,
                                                 const NodeType lca) const {
  std::vector<NodeType> result;
  if (start == end) {
    result.push_back(start);
    return result;
  }
  if (start == lca) {
    ReverseAppendToPath(end, lca, result);
    return result;
  }
  if (end == lca) {
    AppendToPath(start, lca, result);
    return result;
  }
  AppendToPath(start, lca, result);
  result.pop_back();  // Don't include the LCA twice
  ReverseAppendToPath(end, lca, result);
  return result;
}

template <typename NodeType>
double RootedTree<NodeType>::Distance(
    const NodeType start, const NodeType end, const NodeType lca,
    absl::Span<const double> arc_lengths) const {
  return DistanceOfUpwardPath(start, lca, arc_lengths) +
         DistanceOfUpwardPath(end, lca, arc_lengths);
}

template <typename NodeType>
std::pair<double, std::vector<NodeType>> RootedTree<NodeType>::DistanceAndPath(
    const NodeType start, const NodeType end, const NodeType lca,
    absl::Span<const double> arc_lengths) const {
  std::vector<NodeType> path = Path(start, end, lca);
  const double dist = DistanceOfPath(path, arc_lengths);
  return {dist, std::move(path)};
}

template <typename NodeType>
double RootedTree<NodeType>::DistanceOfPath(
    absl::Span<const NodeType> path,
    absl::Span<const double> arc_lengths) const {
  CHECK_EQ(num_nodes(), arc_lengths.size());
  double distance = 0.0;
  for (int i = 0; i + 1 < path.size(); ++i) {
    if (parents_[path[i]] != path[i + 1]) {
      distance += arc_lengths[path[i]];
    } else if (parents_[path[i + 1]] == path[i]) {
      distance += arc_lengths[path[i + 1]];
    } else {
      LOG(FATAL) << "bad edge in path from " << path[i] << " to "
                 << path[i + 1];
    }
  }
  return distance;
}

template <typename NodeType>
NodeType RootedTree<NodeType>::LowestCommonAncestorByDepth(
    const NodeType n1, const NodeType n2,
    absl::Span<const NodeType> depths) const {
  CHECK_EQ(num_nodes(), depths.size());
  const NodeType n = num_nodes();
  CHECK_OK(internal::IsValidNode(n1, n));
  CHECK_OK(internal::IsValidNode(n2, n));
  CHECK_EQ(depths.size(), n);
  if (n1 == root_ || n2 == root_) {
    return root_;
  }
  if (n1 == n2) {
    return n1;
  }
  NodeType next1 = n1;
  NodeType next2 = n2;
  while (depths[next1] > depths[next2]) {
    next1 = parents_[next1];
  }
  while (depths[next2] > depths[next1]) {
    next2 = parents_[next2];
  }
  while (next1 != next2) {
    next1 = parents_[next1];
    next2 = parents_[next2];
  }
  return next1;
}

template <typename NodeType>
NodeType RootedTree<NodeType>::LowestCommonAncestorBySearch(
    const NodeType n1, const NodeType n2,
    std::vector<bool>& visited_workspace) const {
  const NodeType n = num_nodes();
  CHECK_OK(internal::IsValidNode(n1, n));
  CHECK_OK(internal::IsValidNode(n2, n));
  CHECK_EQ(visited_workspace.size(), n);
  if (n1 == root_ || n2 == root_) {
    return root_;
  }
  if (n1 == n2) {
    return n1;
  }
  NodeType next1 = n1;
  NodeType next2 = n2;
  visited_workspace[n1] = true;
  visited_workspace[n2] = true;
  NodeType lca = kNullParent;
  NodeType lca_distance =
      1;  // used only for cleanup purposes, can over estimate
  while (true) {
    lca_distance++;
    if (next1 != root_) {
      next1 = parents_[next1];
      if (visited_workspace[next1]) {
        lca = next1;
        break;
      }
    }
    if (next2 != root_) {
      visited_workspace[next1] = true;
      next2 = parents_[next2];
      if (visited_workspace[next2]) {
        lca = next2;
        break;
      }
      visited_workspace[next2] = true;
    }
  }
  CHECK_OK(internal::IsValidNode(lca, n));
  auto cleanup = [this, lca_distance, &visited_workspace](NodeType next) {
    for (NodeType i = 0; i < lca_distance && next != kNullParent; ++i) {
      visited_workspace[next] = false;
      next = parents_[next];
    }
  };
  cleanup(n1);
  cleanup(n2);
  return lca;
}

template <typename NodeType>
void RootedTree<NodeType>::Evert(const NodeType new_root) {
  NodeType previous_node = kNullParent;
  for (NodeType node = new_root; node != kNullParent;) {
    NodeType next_node = parents_[node];
    parents_[node] = previous_node;
    previous_node = node;
    node = next_node;
  }
  root_ = new_root;
}

template <typename NodeType>
template <typename T>
void RootedTree<NodeType>::AllDistancesToRootInPlace(
    absl::Span<const NodeType> topological_order,
    absl::Span<T> arc_lengths_in_distances_out) const {
  CHECK_EQ(num_nodes(), arc_lengths_in_distances_out.size());
  CHECK_EQ(num_nodes(), topological_order.size());
  if (!topological_order.empty()) {
    CHECK_EQ(topological_order[0], root_);
  }
  for (const NodeType node : topological_order) {
    if (parents_[node] == kNullParent) {
      arc_lengths_in_distances_out[node] = 0;
    } else {
      arc_lengths_in_distances_out[node] +=
          arc_lengths_in_distances_out[parents_[node]];
    }
  }
}

template <typename NodeType>
std::vector<NodeType> RootedTree<NodeType>::AllDepths(
    absl::Span<const NodeType> topological_order) const {
  std::vector<NodeType> arc_length_in_distance_out(num_nodes(), 1);
  AllDistancesToRootInPlace(topological_order,
                            absl::MakeSpan(arc_length_in_distance_out));
  return arc_length_in_distance_out;
}

template <typename NodeType>
template <typename T>
std::vector<T> RootedTree<NodeType>::AllDistancesToRoot(
    absl::Span<const T> arc_lengths) const {
  return AllDistancesToRoot(arc_lengths, TopologicalSort());
}

template <typename NodeType>
template <typename T>
std::vector<T> RootedTree<NodeType>::AllDistancesToRoot(
    absl::Span<const T> arc_lengths,
    absl::Span<const NodeType> topological_order) const {
  std::vector<T> distances(arc_lengths.begin(), arc_lengths.end());
  AllDistancesToRootInPlace(topological_order, absl::MakeSpan(distances));
  return distances;
}

template <typename Graph>
absl::StatusOr<RootedTree<typename Graph::NodeIndex>> RootedTreeFromGraph(
    const typename Graph::NodeIndex root, const Graph& graph,
    std::vector<typename Graph::NodeIndex>* const topological_order,
    std::vector<typename Graph::NodeIndex>* const depths) {
  using NodeIndex = typename Graph::NodeIndex;
  const NodeIndex num_nodes = graph.num_nodes();
  RETURN_IF_ERROR(internal::IsValidNode(root, num_nodes))
      << "invalid root node";
  if (topological_order != nullptr) {
    topological_order->clear();
    topological_order->reserve(num_nodes);
    topological_order->push_back(root);
  }
  if (depths != nullptr) {
    depths->clear();
    depths->resize(num_nodes, 0);
  }
  std::vector<NodeIndex> tree(num_nodes, RootedTree<NodeIndex>::kNullParent);
  auto visited = [&tree, root](const NodeIndex node) {
    if (node == root) {
      return true;
    }
    return tree[node] != RootedTree<NodeIndex>::kNullParent;
  };
  std::vector<NodeIndex> must_search_children = {root};
  while (!must_search_children.empty()) {
    NodeIndex next = must_search_children.back();
    must_search_children.pop_back();
    for (const NodeIndex neighbor : graph[next]) {
      if (visited(neighbor)) {
        if (tree[next] == neighbor) {
          continue;
        } else {
          // NOTE: this will also catch nodes with self loops.
          return util::InvalidArgumentErrorBuilder()
                 << "graph has cycle containing arc from " << next << " to "
                 << neighbor;
        }
      }
      tree[neighbor] = next;
      if (topological_order != nullptr) {
        topological_order->push_back(neighbor);
      }
      if (depths != nullptr) {
        (*depths)[neighbor] = (*depths)[next] + 1;
      }
      must_search_children.push_back(neighbor);
    }
  }
  for (NodeIndex i = 0; i < num_nodes; ++i) {
    if (!visited(i)) {
      return util::InvalidArgumentErrorBuilder()
             << "graph is not connected, no path to " << i;
    }
  }
  return RootedTree<NodeIndex>(root, tree);
}

}  // namespace operations_research

#endif  // ORTOOLS_GRAPH_ROOTED_TREE_H_
