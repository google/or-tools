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

#ifndef UTIL_GRAPH_BFS_H_
#define UTIL_GRAPH_BFS_H_

#include <cstddef>
#include <limits>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"

// These 3 functions give the full functionality of a BFS (Breadth-First-Search)
// on any type of Graph on dense integers that implements the [] operator to
// yield the adjacency list: graph[i] should yield a vector<int>-like object
// that lists all the (outgoing) neighbors of node #i.
//
// If your graph is undirected, it means that for each edge (i,j), graph[i] must
// contain j and graph[j] must contain i.
//
// Self-arcs and multi-arcs are supported, since they don't affect BFS.
//
// These functions are fast: they have the optimal asymptotic complexity, and
// are reasonably optimized. You may still get performance gains if you
// implement your own BFS for your application. In particular, this API is
// optimized for repeated calls to GetBFSShortestPath(); if you only care about
// GetBFSDistances() there exists more optimized implementations.
//
// ERRORS:
// This library does perform many checks at runtime, and returns an error Status
// if it detects a problem, but it doesn't fully protect you from crashes if the
// input is ill-formed in some ways this library can't check. For example, if
// calling graph[i] crashes for some i in 0..num_nodes-1, this won't detect it.
//
// Example:
//   const int num_nodes = 3;
//   vector<vector<int>> graph(num_nodes) = {{1}, {0}, {1, 2}};  // 0↔1←2⟲
//   const int source = 1;
//   vector<int> bfs_tree = GetBFSRootedTree(graph, num_nodes, source).value();
//   LOG(INFO) << DUMP_VARS(GetBFSDistances(bfs_tree));
//   for (int target : {0, 1, 2}) {
//     vector<int> path = GetBFSShortestPath(bfs_tree, target);
//     LOG(INFO) << DUMP_VARS(path);
//   }
//
// Would log this: GetBFSDistances(bfs_tree) = [1, 0, -1]
//                 path = [1, 0]
//                 path = [1]
//                 path = []  // no path from 1 to 2.

namespace util {
namespace graph {

// Runs a BFS (Breadth First Search) in O(num_nodes + num_arcs), and returns the
// BFS tree rooted at the source: returned element #i is either:
// - the parent of node #i, i.e. the node that precedes it in the shortest path
//   from the source to i;
// - or -1, if the node wasn't reached;
// - or itself, i.e. i, if #i is the source.
//
// TIE BREAKING: This implementation always breaks tie the same way, by order
// in the adjacency lists. E.g. if all the adjacency lists are sorted, then the
// parent of a node in the BFS tree is always the smalled possible parent.
template <class Graph, class NodeIndex = int>
absl::StatusOr<std::vector<NodeIndex>> GetBFSRootedTree(const Graph& graph,
                                                        NodeIndex num_nodes,
                                                        NodeIndex source);

// Returns the distances of all nodes from the source, in O(num_nodes).
// `bfs_tree` must be exactly as returned by GetBFSRootedTree().
// NOTE: Supports BFS forests, i.e. the result of a BFS from multiple sources.
template <class NodeIndex>
absl::StatusOr<std::vector<NodeIndex>> GetBFSDistances(
    const std::vector<NodeIndex>& bfs_tree);

// Returns the shortest path from the source to `target`, in O(path length).
// `bfs_tree` must be exactly as returned by GetBFSRootedTree().
// If `target` wasn't reached in the BFS, returns the empty vector. Else the
// returned path always starts with the source and ends with the target (if
// source=target, returns [source]).
template <class NodeIndex>
absl::StatusOr<std::vector<NodeIndex>> GetBFSShortestPath(
    const std::vector<NodeIndex>& bfs_tree, NodeIndex target);

// _____________________________________________________________________________
// Implementation of the templates.

template <class Graph, class NodeIndex>
absl::StatusOr<std::vector<NodeIndex>> GetBFSRootedTree(const Graph& graph,
                                                        NodeIndex num_nodes,
                                                        NodeIndex source) {
  if (source < 0 || source >= num_nodes) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Invalid graph: source=%d is not in [0, num_nodes=%d)",
                        source, num_nodes));
  }
  // NOTE(user): -1 works fine for unsigned integers because 'num_nodes' is
  // already an exclusive upper bound for node indices (and -1 unsigned can only
  // be ≥ num_nodes).
  constexpr NodeIndex kNone = -1;  // NOLINT
  std::vector<NodeIndex> bfs_tree(num_nodes, kNone);
  bfs_tree[source] = source;
  std::vector<NodeIndex> bfs_queue = {source};
  size_t num_visited = 0;
  while (num_visited < bfs_queue.size()) {
    const NodeIndex node = bfs_queue[num_visited++];
    for (const NodeIndex child : graph[node]) {
      if (child < 0 || child >= num_nodes) {
        return absl::InvalidArgumentError(
            absl::StrFormat("Invalid graph: graph[%d] contains %d, which is "
                            "not a valid node index in [0, num_nodes=%d)",
                            node, child, num_nodes));
      }
      if (bfs_tree[child] != kNone) continue;  // Already visited.
      bfs_tree[child] = node;
      bfs_queue.push_back(child);
    }
  }
  return bfs_tree;
}

template <class NodeIndex>
absl::StatusOr<std::vector<NodeIndex>> GetBFSDistances(
    const std::vector<NodeIndex>& bfs_tree) {
  const NodeIndex n = bfs_tree.size();
  constexpr NodeIndex kNone = -1;  // NOLINT

  // Run a few checks on the input first.
  constexpr NodeIndex kMaxNodeIndex = std::numeric_limits<NodeIndex>::max();
  if (bfs_tree.size() > kMaxNodeIndex) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "bfs_tree.size()=%d is too large for its integer node type (max=%d)",
        bfs_tree.size(), kMaxNodeIndex));
  }
  for (NodeIndex i = 0; i < n; ++i) {
    const NodeIndex parent = bfs_tree[i];
    if (parent != kNone && (parent < 0 || parent >= n)) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "bfs_tree[%d]=%d is outside [0, bfs_tree.size()=%d)", i, parent, n));
    }
  }

  // The algorithm starts.
  std::vector<NodeIndex> distance(n, kNone);
  for (NodeIndex i = 0; i < n; ++i) {
    if (bfs_tree[i] == kNone) continue;
    // Ascend the parent tree until we reach either the root (the BFS source),
    // or an already-marked node (whose distance we already know).
    NodeIndex p = i;
    NodeIndex dist = 0;
    while (bfs_tree[p] != p && distance[p] == kNone) {
      p = bfs_tree[p];
      ++dist;
      if (dist >= n) {
        return absl::InvalidArgumentError(
            absl::StrFormat("bfs_tree isn't a BFS tree: detected a cycle in"
                            " the ascendance of node %d",
                            i));
      }
      if (p == kNone) {
        return absl::InvalidArgumentError(
            absl::StrFormat("bfs_tree isn't a BFS tree: detected an"
                            " interrupted ascendance from %d",
                            i));
      }
    }
    // We've reached the root or an already-marked node. Update our distance.
    if (bfs_tree[p] == p) distance[p] = 0;
    dist += distance[p];
    // Now ascend the path a second time, to mark the distances of all nodes on
    // the path.
    const NodeIndex known_node = p;
    p = i;
    while (p != known_node) {
      distance[p] = dist;
      --dist;
      p = bfs_tree[p];
    }
  }
  return distance;
}

template <class NodeIndex>
absl::StatusOr<std::vector<NodeIndex>> GetBFSShortestPath(
    const std::vector<NodeIndex>& bfs_tree, NodeIndex target) {
  const NodeIndex n = bfs_tree.size();
  if (target < 0 || target >= n) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "target=%d is outside [0, bfs_tree.size()=%d)", target, n));
  }

  std::vector<NodeIndex> path;
  constexpr NodeIndex kNone = -1;  // NOLINT
  if (bfs_tree[target] == kNone) return path;
  while (true) {
    if (path.size() >= bfs_tree.size()) {
      return absl::InvalidArgumentError(
          absl::StrFormat("bfs_tree isn't a BFS tree: detected a cycle in"
                          " the ascendance of node %d",
                          target));
    }
    path.push_back(target);
    const NodeIndex new_target = bfs_tree[target];
    if (new_target == target) break;
    if (new_target < 0 || new_target >= n) {
      return absl::InvalidArgumentError(
          absl::StrFormat("bfs_tree[%d]=%d is outside [0, bfs_tree.size()=%d)",
                          target, new_target, n));
    }
    target = new_target;
  }
  std::reverse(path.begin(), path.end());
  return path;
}

}  // namespace graph
}  // namespace util

#endif  // UTIL_GRAPH_BFS_H_
