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

// This file contains functions to compute shortest paths on graphs using
// Dijkstra's algorithm,
// E.W. Dijkstra, "A note on two problems in connexion with graphs". Numerische
// Mathematik 1:269–271, 1959. See for example:
// http://www.springerlink.com/content/uu8608u0u27k7256/fulltext.pdf.
// More information can also be found on Wikipedia:
// http://en.wikipedia.org/wiki/Dijkstra's_algorithm
//
// This is a unidirectional implementation of Dijkstra's algorithm. A
// bidirectional is available in bidirectional_dijkstra.h for specific use
// cases.
//
// Each 1-to-many shortest path computation is run in a separate thread. Users
// should select the number of threads to use according to the number of cores
// available (each thread will use up one core). However, increasing the number
// of threads also increases temporary memory used by each 1-to-many
// computation.
//
// Also included are classes to store path data resulting from shortest path
// computations (cf. GenericPathContainer).
//
// Usage example computing all-pair shortest paths on a graph:
//     StaticGraph<> graph(...,...);
//     std::vector<uint32_t> arc_lengths(...,...);
//     ... populate graph and arc lengths ...
//     GenericPathContainer<StaticGraph<>> container =
//       GenericPathContainer<
//         StaticGraph<>>::BuildInMemoryCompactPathContainer();
//     ComputeAllToAllShortestPathsWithMultipleThreads(graph,
//                                                     arc_lengths,
//                                                     /*num_threads=*/4,
//                                                     &container);
//
// Usage example computing shortest paths between a subset of graph nodes:
//     StaticGraph<> graph(...,...);
//     std::vector<uint32_t> arc_lengths(...,...);
//     ... populate graph and arc lengths ...
//     vector<NodeIndex> sources;
//     vector<NodeIndex> sinks;
//     ... fill sources and sinks ...
//     GenericPathContainer<StaticGraph<>> container =
//       GenericPathContainer<
//         StaticGraph<>>::BuildInMemoryCompactPathContainer();
//     ComputeManyToManyShortestPathsWithMultipleThreads(graph,
//                                                       arc_lengths,
//                                                       sources,
//                                                       sinks,
//                                                       /*num_threads=*/4,
//                                                       &container);

#ifndef OR_TOOLS_GRAPH_SHORTEST_PATHS_H_
#define OR_TOOLS_GRAPH_SHORTEST_PATHS_H_

#include <cstdint>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/container/flat_hash_map.h"
#include "absl/functional/bind_front.h"
#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/base/adjustable_priority_queue-inl.h"
#include "ortools/base/adjustable_priority_queue.h"
#include "ortools/base/logging.h"
#include "ortools/base/map_util.h"
#include "ortools/base/stl_util.h"
#include "ortools/base/threadpool.h"
#include "ortools/base/timer.h"
namespace operations_research {

// Storing distances on 32 bits to limit memory consumption of distance
// matrices. If distances don't fit on 32 bits, scaling and losing a bit of
// precision should be acceptable in practice.
typedef uint32_t PathDistance;

const PathDistance kDisconnectedPathDistance =
    std::numeric_limits<uint32_t>::max();

namespace internal {
template <class NodeIndex, NodeIndex kNilNode>
class PathContainerImpl;
}  // namespace internal

// Container class storing paths and distances along the paths. It is used in
// shortest path computation functions to store resulting shortest paths.
// Usage example iterating on the path between nodes `from` and `to`:
//     GenericPathContainer<StaticGraph<>> container =
//       GenericPathContainer<
//         StaticGraph<>>::BuildInMemoryCompactPathContainer();
//     // ... fill up container ...
//     const GenericPathContainer::NodeIndex from =...;
//     GenericPathContainer::NodeIndex to =...;
//     while (to != from) {
//       LOG(INFO) << to;
//       to = container.GetPenultimateNodeInPath(from, to);
//     }
template <class GraphType>
class GenericPathContainer {
 public:
  using NodeIndex = typename GraphType::NodeIndex;
  using Impl = internal::PathContainerImpl<NodeIndex, GraphType::kNilNode>;

  // TODO(b/385094969): Remove this when all clients are migrated, and use
  // factory functions instead.
  GenericPathContainer();

  // This type is neither copyable nor movable.
  GenericPathContainer(const GenericPathContainer&) = delete;
  GenericPathContainer& operator=(const GenericPathContainer&) = delete;

  ~GenericPathContainer();

  // Returns the distance between node `from` and node `to` following the path
  // out of `from` and into `to`. Note that if `from` == `to`, the distance is
  // not necessarily 0 if the path out of `to` and back into `to` has a distance
  // greater than 0. If you do require the distance to be 0 in this case, add to
  // the graph an arc from `to` to itself with a length of 0.
  // If nodes are not connected, returns `kDisconnectedPathDistance`.
  PathDistance GetDistance(NodeIndex from, NodeIndex to) const;

  // Returns the penultimate node on the path out of node `from` into node `to`
  // (the direct predecessor of node `to` on the path).
  // If `from` == `to`, the penultimate node is `to` only if the shortest path
  // from `to` to itself is composed of the arc (`to, `to`), which might not be
  // the case if either this arc doesn't exist or if the length of this arc is
  // greater than the distance of an alternate path.
  // If nodes are not connected, returns `GraphType::kNilNode`.
  NodeIndex GetPenultimateNodeInPath(NodeIndex from, NodeIndex to) const;

  // Returns path nodes from node `from` to node `to` in the order in which they
  // appear along the path.
  // The vector starts with `from` and ends with `to`, if both nodes are
  // connected (otherwise an empty vector is returned).
  void GetPath(NodeIndex from, NodeIndex to,
               std::vector<NodeIndex>* path) const;

  // Builds a path container which only stores distances between path nodes.
  static GenericPathContainer BuildPathDistanceContainer();

  ABSL_DEPRECATED("Use factory function BuildPathDistanceContainer instead.")
  static void BuildPathDistanceContainer(GenericPathContainer* path_container);

  // Builds a path container which stores explicit paths and distances between
  // path nodes in a memory-compact representation.
  // In this case `GetPenultimateNodeInPath()` is `O(log(path_tree_size))`,
  // `path_tree_size` being the size of a tree of paths from a source node (in
  // practice it is equal to the number of nodes in the graph if all nodes
  // are strongly connected).
  // `GetPath` is `O(log(path_tree_size) + path_size)`, where `path_size` is the
  // size of the resulting path; note this is faster than successive calls
  // to `GetPenultimateNodeInPath()` which would result in
  // `O(log(path_tree_size) * path_size)`.
  static GenericPathContainer BuildInMemoryCompactPathContainer();

  ABSL_DEPRECATED(
      "Use factory function BuildInMemoryCompactPathContainer instead.")
  static void BuildInMemoryCompactPathContainer(
      GenericPathContainer* path_container);

  // TODO(user): Add save-to-disk container.
  // TODO(user): Add `BuildInMemoryFastPathContainer()`, which does
  // `GetPenultimateNodeInPath()` in `O(1)`.

  // For internal use only. Returns the internal container implementation.
  Impl* GetImplementation() const { return container_.get(); }

 private:
  explicit GenericPathContainer(std::unique_ptr<Impl> impl)
      : container_(std::move(impl)) {}

  std::unique_ptr<Impl> container_;
};

// Utility function which returns a vector containing all nodes of a graph.
template <class GraphType>
void GetGraphNodes(const GraphType& graph,
                   std::vector<typename GraphType::NodeIndex>* nodes) {
  CHECK(nodes != nullptr);
  nodes->clear();
  nodes->reserve(graph.num_nodes());
  for (typename GraphType::NodeIterator iterator(graph); iterator.Ok();
       iterator.Next()) {
    nodes->push_back(iterator.Index());
  }
}

template <class GraphType>
void GetGraphNodesFromGraph(const GraphType& graph,
                            std::vector<typename GraphType::NodeIndex>* nodes) {
  CHECK(nodes != nullptr);
  nodes->clear();
  nodes->reserve(graph.num_nodes());
  for (const typename GraphType::NodeIndex node : graph.AllNodes()) {
    nodes->push_back(node);
  }
}

// In all the functions below the arc_lengths vector represents the lengths of
// the arcs of the graph (`arc_lengths[arc]` is the length of `arc`).
// Resulting shortest paths are stored in a path container `path_container`.

// Computes shortest paths from the node `source` to all nodes in the graph.
template <class GraphType>
void ComputeOneToAllShortestPaths(
    const GraphType& graph, const std::vector<PathDistance>& arc_lengths,
    typename GraphType::NodeIndex source,
    GenericPathContainer<GraphType>* const path_container) {
  std::vector<typename GraphType::NodeIndex> all_nodes;
  GetGraphNodesFromGraph<GraphType>(graph, &all_nodes);
  ComputeOneToManyShortestPaths(graph, arc_lengths, source, all_nodes,
                                path_container);
}

// Computes shortest paths from the node `source` to nodes in `destinations`.
// TODO(b/385094969): Remove second template parameter when all clients are
// migrated.
template <class GraphType, class PathContainerGraphType>
void ComputeOneToManyShortestPaths(
    const GraphType& graph, const std::vector<PathDistance>& arc_lengths,
    typename GraphType::NodeIndex source,
    const std::vector<typename GraphType::NodeIndex>& destinations,
    GenericPathContainer<PathContainerGraphType>* const path_container) {
  std::vector<typename GraphType::NodeIndex> sources(1, source);
  ComputeManyToManyShortestPathsWithMultipleThreads(
      graph, arc_lengths, sources, destinations, 1, path_container);
}

// Computes the shortest path from the node `source` to the node `destination`
// and returns that path as a vector of nodes. If there is no path from `source`
// to `destination`, the returned vector is empty.
//
// To get distance information, use `ComputeOneToManyShortestPaths` with a
// single destination and a `PathContainer` built with
// `BuildPathDistanceContainer` (if you just need the distance) or
// `BuildInMemoryCompactPathContainer` (otherwise).
template <class GraphType>
std::vector<typename GraphType::NodeIndex> ComputeOneToOneShortestPath(
    const GraphType& graph, const std::vector<PathDistance>& arc_lengths,
    typename GraphType::NodeIndex source,
    typename GraphType::NodeIndex destination) {
  std::vector<typename GraphType::NodeIndex> sources(1, source);
  std::vector<typename GraphType::NodeIndex> destinations(1, destination);

  auto path_container =
      GenericPathContainer<GraphType>::BuildInMemoryCompactPathContainer();

  ComputeManyToManyShortestPathsWithMultipleThreads(
      graph, arc_lengths, sources, destinations, 1, &path_container);

  std::vector<typename GraphType::NodeIndex> path;
  path_container.GetPath(source, destination, &path);
  return path;
}

// Computes shortest paths from the nodes in `sources` to all nodes in the
// graph.
template <class GraphType>
void ComputeManyToAllShortestPathsWithMultipleThreads(
    const GraphType& graph, const std::vector<PathDistance>& arc_lengths,
    const std::vector<typename GraphType::NodeIndex>& sources, int num_threads,
    GenericPathContainer<GraphType>* const path_container) {
  std::vector<typename GraphType::NodeIndex> all_nodes;
  GetGraphNodesFromGraph<GraphType>(graph, &all_nodes);
  ComputeManyToManyShortestPathsWithMultipleThreads(
      graph, arc_lengths, sources, all_nodes, num_threads, path_container);
}

// Computes shortest paths between all nodes of the graph.
// TODO(b/385094969): Remove second template parameter when all clients are
// migrated.
template <class GraphType, class PathContainerGraphType>
void ComputeAllToAllShortestPathsWithMultipleThreads(
    const GraphType& graph, const std::vector<PathDistance>& arc_lengths,
    int num_threads,
    GenericPathContainer<PathContainerGraphType>* const path_container) {
  std::vector<typename GraphType::NodeIndex> all_nodes;
  GetGraphNodesFromGraph<GraphType>(graph, &all_nodes);
  ComputeManyToManyShortestPathsWithMultipleThreads(
      graph, arc_lengths, all_nodes, all_nodes, num_threads, path_container);
}

// =============================================================================
// Implementation.
// =============================================================================

namespace internal {

// Base path container implementation class. Defines virtual functions used to
// fill the container (in particular from the shortest path computation
// function).
template <class NodeIndex, NodeIndex kNilNode>
class PathContainerImpl {
 public:
  PathContainerImpl() = default;
  virtual ~PathContainerImpl() = default;

  // Initializes the container on source and destination node vectors
  // (`num_nodes` is the total number of nodes in the graph containing source
  // and destination nodes).
  // Called before adding any paths to the container.
  virtual void Initialize(const std::vector<NodeIndex>& sources,
                          const std::vector<NodeIndex>& destinations,
                          NodeIndex num_nodes) = 0;

  // Called when no more path will be added to the container.
  virtual void Finalize() {}

  // Returns the distance between node `from` and node `to` following the path
  // out of `from` and into `to`. Note that if `from` == `to`, the distance is
  // not necessarily 0 if the path out of `to` and back into `to` has a distance
  // greater than 0. If you do require the distance to be 0 in this case, add to
  // the graph an arc from `to` to itself with a length of 0.
  // If nodes are not connected, returns `kDisconnectedPathDistance`.
  virtual PathDistance GetDistance(NodeIndex from, NodeIndex to) const = 0;

  // Returns the penultimate node on the path out of node `from` into node `to`
  // (the direct predecessor of node `to` on the path).
  // If `from` == `to`, the penultimate node is `to` only if the shortest path
  // from `to` to itself is composed of the arc (`to, `to`), which might not be
  // the case if either this arc doesn't exist or if the length of this arc is
  // greater than the distance of an alternate path.
  // If nodes are not connected, returns `kNilNode`.
  virtual NodeIndex GetPenultimateNodeInPath(NodeIndex from,
                                             NodeIndex to) const = 0;

  // Returns path nodes from node `from` to node `to` in a ordered vector.
  virtual void GetPath(NodeIndex from, NodeIndex to,
                       std::vector<NodeIndex>* path) const = 0;

  // Adds a path tree rooted at node `from`, and to a set of implicit
  // destinations:
  // - `predecessor_in_path_tree[node]` is the predecessor of node `node` in the
  //   path from `from` to `node`, or `kNilNode` if there is no
  //   predecessor (i.e. if `node` is not in the path tree);
  // - `distance_to_destination[i]` is the distance from `from` to the i-th
  //   destination (see `Initialize()`).
  virtual void StoreSingleSourcePaths(
      NodeIndex from, const std::vector<NodeIndex>& predecessor_in_path_tree,
      const std::vector<PathDistance>& distance_to_destination) = 0;
};

// Class designed to store the tree of paths from a root node to a set of nodes
// in a very compact way (over performance).
// Memory consumption is in `O(n)` (`n` being the size of the tree) where node
// indices are "very" non-contiguous (extremely sparse node indices). It keeps
// node-sorted arrays of node and parent pairs, which can be accessed in
// `O(log(n))` with a binary search.
// The creation of the tree is done in `O(n*log(n))` time.
// Note that this class uses temporary memory for each call to `Initialize`
// which is only an issue for massive parallel calls; in practice for shortest
// paths computation, the number of threads calling `Initialize` is very small
// compared to the total number of trees created.
template <class NodeIndex, NodeIndex kNilNode>
class PathTree {
 public:
  PathTree() : nodes_(), parents_() {}

  void Initialize(absl::Span<const NodeIndex> paths,
                  absl::Span<const NodeIndex> destinations);

  // Returns the parent (predecessor) of `node` in the tree in
  // `O(log(path_tree_size))`, where `path_tree_size` is the size of `nodes_`.
  NodeIndex GetParent(NodeIndex node) const;

  // Returns the path from node `from` to node `to` in the tree in
  // `O(log(path_tree_size) + path_size)`, where `path_tree_size` is the size of
  // `nodes_` and `path_size` is the size of the resulting path.
  void GetPath(NodeIndex from, NodeIndex to,
               std::vector<NodeIndex>* path) const;

 private:
  std::vector<NodeIndex> nodes_;
  std::vector<int> parents_;
};

// Initializes the tree from a non-sparse representation of the path tree
// represented by `paths`. The tree is reduced to the subtree in which nodes in
// `destinations` are the leafs.
template <class NodeIndex, NodeIndex kNilNode>
void PathTree<NodeIndex, kNilNode>::Initialize(
    absl::Span<const NodeIndex> paths,
    absl::Span<const NodeIndex> destinations) {
  std::vector<bool> node_explored(paths.size(), false);
  const int destination_size = destinations.size();
  typedef std::pair<NodeIndex, NodeIndex> NodeParent;
  std::vector<NodeParent> tree;
  for (int i = 0; i < destination_size; ++i) {
    NodeIndex destination = destinations[i];
    while (!node_explored[destination]) {
      node_explored[destination] = true;
      tree.push_back(std::make_pair(destination, paths[destination]));
      if (paths[destination] != kNilNode) {
        destination = paths[destination];
      }
    }
  }
  std::sort(tree.begin(), tree.end());
  const int num_nodes = tree.size();
  {
    absl::flat_hash_map<NodeIndex, int> node_indices;

    for (int i = 0; i < num_nodes; ++i) {
      node_indices[tree[i].first] = i;
    }
    parents_.resize(num_nodes, -1);
    for (int i = 0; i < num_nodes; ++i) {
      parents_[i] =
          ::gtl::FindWithDefault(node_indices, tree[i].second, kNilNode);
    }
  }
  nodes_.resize(num_nodes, kNilNode);
  for (int i = 0; i < num_nodes; ++i) {
    nodes_[i] = tree[i].first;
  }
}

template <class NodeIndex, NodeIndex kNilNode>
NodeIndex PathTree<NodeIndex, kNilNode>::GetParent(NodeIndex node) const {
  const auto node_position = absl::c_lower_bound(nodes_, node);
  if (node_position != nodes_.end() && *node_position == node) {
    const int parent = parents_[node_position - nodes_.begin()];
    if (parent != kNilNode) {
      return nodes_[parent];
    }
  }
  return kNilNode;
}

template <class NodeIndex, NodeIndex kNilNode>
void PathTree<NodeIndex, kNilNode>::GetPath(
    NodeIndex from, NodeIndex to, std::vector<NodeIndex>* path) const {
  DCHECK(path != nullptr);
  path->clear();
  const auto to_position = absl::c_lower_bound(nodes_, to);
  if (to_position != nodes_.end() && *to_position == to) {
    int current_index = to_position - nodes_.begin();
    NodeIndex current_node = to;
    while (current_node != from) {
      path->push_back(current_node);
      current_index = parents_[current_index];
      // `from` and `to` are not connected.
      if (current_index == kNilNode) {
        path->clear();
        return;
      }
      current_node = nodes_[current_index];
    }
    path->push_back(current_node);
    std::reverse(path->begin(), path->end());
  }
}

// Path container which only stores distances between path nodes.
template <class NodeIndex, NodeIndex kNilNode>
class DistanceContainer : public PathContainerImpl<NodeIndex, kNilNode> {
 public:
  DistanceContainer() : reverse_sources_(), distances_() {}

  // This type is neither copyable nor movable.
  DistanceContainer(const DistanceContainer&) = delete;
  DistanceContainer& operator=(const DistanceContainer&) = delete;
  ~DistanceContainer() override = default;
  void Initialize(const std::vector<NodeIndex>& sources,
                  const std::vector<NodeIndex>& destinations,
                  NodeIndex num_nodes) override {
    ComputeReverse(sources, num_nodes, &reverse_sources_);
    ComputeReverse(destinations, num_nodes, &reverse_destinations_);
    distances_.clear();
    distances_.resize(sources.size());
  }
  PathDistance GetDistance(NodeIndex from, NodeIndex to) const override {
    return distances_[reverse_sources_[from]][reverse_destinations_[to]];
  }
  NodeIndex GetPenultimateNodeInPath(NodeIndex, NodeIndex) const override {
    LOG(FATAL) << "Path not stored.";
    return kNilNode;
  }
  void GetPath(NodeIndex, NodeIndex, std::vector<NodeIndex>*) const override {
    LOG(FATAL) << "Path not stored.";
  }
  void StoreSingleSourcePaths(
      NodeIndex from,
      // `DistanceContainer` only stores distances and not predecessors.
      const std::vector<NodeIndex>&,
      const std::vector<PathDistance>& distance_to_destination) override {
    distances_[reverse_sources_[from]] = distance_to_destination;
  }

 protected:
  std::vector<int> reverse_sources_;
  std::vector<int> reverse_destinations_;

 private:
  static void ComputeReverse(absl::Span<const NodeIndex> nodes,
                             NodeIndex num_nodes,
                             std::vector<int>* reverse_nodes) {
    CHECK(reverse_nodes != nullptr);
    const int kUnassignedIndex = -1;
    reverse_nodes->clear();
    reverse_nodes->resize(num_nodes, kUnassignedIndex);
    for (int i = 0; i < nodes.size(); ++i) {
      reverse_nodes->at(nodes[i]) = i;
    }
  }

  std::vector<std::vector<PathDistance>> distances_;
};

// Path container which stores explicit paths and distances between path nodes.
template <class NodeIndex, NodeIndex kNilNode>
class InMemoryCompactPathContainer
    : public DistanceContainer<NodeIndex, kNilNode> {
 public:
  using Base = DistanceContainer<NodeIndex, kNilNode>;

  InMemoryCompactPathContainer() : trees_(), destinations_() {}

  // This type is neither copyable nor movable.
  InMemoryCompactPathContainer(const InMemoryCompactPathContainer&) = delete;
  InMemoryCompactPathContainer& operator=(const InMemoryCompactPathContainer&) =
      delete;
  ~InMemoryCompactPathContainer() override = default;
  void Initialize(const std::vector<NodeIndex>& sources,
                  const std::vector<NodeIndex>& destinations,
                  NodeIndex num_nodes) override {
    Base::Initialize(sources, destinations, num_nodes);
    destinations_ = destinations;
    trees_.clear();
    trees_.resize(sources.size());
  }
  NodeIndex GetPenultimateNodeInPath(NodeIndex from,
                                     NodeIndex to) const override {
    return trees_[Base::reverse_sources_[from]].GetParent(to);
  }
  void GetPath(NodeIndex from, NodeIndex to,
               std::vector<NodeIndex>* path) const override {
    DCHECK(path != nullptr);
    trees_[Base::reverse_sources_[from]].GetPath(from, to, path);
  }
  void StoreSingleSourcePaths(
      NodeIndex from, const std::vector<NodeIndex>& predecessor_in_path_tree,
      const std::vector<PathDistance>& distance_to_destination) override {
    Base::StoreSingleSourcePaths(from, predecessor_in_path_tree,
                                 distance_to_destination);
    trees_[Base::reverse_sources_[from]].Initialize(predecessor_in_path_tree,
                                                    destinations_);
  }

 private:
  std::vector<PathTree<NodeIndex, kNilNode>> trees_;
  std::vector<NodeIndex> destinations_;
};

// Priority queue node entry in the boundary of the Dijkstra algorithm.
template <class NodeIndex, NodeIndex kNilNode>
class NodeEntry {
 public:
  NodeEntry()
      : heap_index_(-1),
        distance_(0),
        node_(kNilNode),
        settled_(false),
        is_destination_(false) {}
  bool operator<(const NodeEntry& other) const {
    return distance_ > other.distance_;
  }
  void SetHeapIndex(int h) {
    DCHECK_GE(h, 0);
    heap_index_ = h;
  }
  int GetHeapIndex() const { return heap_index_; }
  void set_distance(PathDistance distance) { distance_ = distance; }
  PathDistance distance() const { return distance_; }
  void set_node(NodeIndex node) { node_ = node; }
  NodeIndex node() const { return node_; }
  void set_settled(bool settled) { settled_ = settled; }
  bool settled() const { return settled_; }
  void set_is_destination(bool is_destination) {
    is_destination_ = is_destination;
  }
  bool is_destination() const { return is_destination_; }

 private:
  int heap_index_;
  PathDistance distance_;
  NodeIndex node_;
  bool settled_;
  bool is_destination_;
};

// Updates an entry with the given distance if it's shorter, and then inserts it
// in the priority queue (or updates it if it's there already), if needed.
// Returns true if the entry was modified, false otherwise.
template <class NodeIndex, NodeIndex kNilNode>
bool InsertOrUpdateEntry(
    PathDistance distance, NodeEntry<NodeIndex, kNilNode>* entry,
    AdjustablePriorityQueue<NodeEntry<NodeIndex, kNilNode>>* priority_queue) {
  // If one wants to use int64_t for either priority or NodeIndex, one should
  // consider using packed ints (putting the two bools with heap_index, for
  // example) in order to stay at 16 bytes instead of 24.
  static_assert(sizeof(NodeEntry<NodeIndex, kNilNode>) == 16,
                "node_entry_class_is_not_well_packed");

  DCHECK(priority_queue != nullptr);
  DCHECK(entry != nullptr);
  if (!priority_queue->Contains(entry)) {
    entry->set_distance(distance);
    priority_queue->Add(entry);
    return true;
  } else if (distance < entry->distance()) {
    entry->set_distance(distance);
    priority_queue->NoteChangedPriority(entry);
    return true;
  }
  return false;
}

// Computes shortest paths from node `source` to nodes in `destinations`
// using a binary heap-based Dijkstra algorithm.
// TODO(user): Investigate alternate implementation which wouldn't use
// AdjustablePriorityQueue.
// TODO(b/385094969): Remove second template parameter when all clients are
// migrated.
template <class GraphType, class PathContainerGraphType>
void ComputeOneToManyOnGraph(
    const GraphType* const graph,
    const std::vector<PathDistance>* const arc_lengths,
    typename GraphType::NodeIndex source,
    const std::vector<typename GraphType::NodeIndex>* const destinations,
    typename GenericPathContainer<PathContainerGraphType>::Impl* const paths) {
  using NodeIndex = typename GraphType::NodeIndex;
  using ArcIndex = typename GraphType::ArcIndex;
  using NodeEntryT = NodeEntry<NodeIndex, GraphType::kNilNode>;
  CHECK(graph != nullptr);
  CHECK(arc_lengths != nullptr);
  CHECK(destinations != nullptr);
  CHECK(paths != nullptr);
  const int num_nodes = graph->num_nodes();
  std::vector<NodeIndex> predecessor(num_nodes, GraphType::kNilNode);
  AdjustablePriorityQueue<NodeEntryT> priority_queue;
  std::vector<NodeEntryT> entries(num_nodes);
  for (const NodeIndex node : graph->AllNodes()) {
    entries[node].set_node(node);
  }
  // Marking destination node. This is an optimization stopping the search
  // when all destinations have been reached.
  for (int i = 0; i < destinations->size(); ++i) {
    entries[(*destinations)[i]].set_is_destination(true);
  }
  // In this implementation the distance of a node to itself isn't necessarily
  // 0.
  // So we push successors of source in the queue instead of the source
  // directly which will avoid marking the source.
  for (const ArcIndex arc : graph->OutgoingArcs(source)) {
    const NodeIndex next = graph->Head(arc);
    if (InsertOrUpdateEntry(arc_lengths->at(arc), &entries[next],
                            &priority_queue)) {
      predecessor[next] = source;
    }
  }
  int destinations_remaining = destinations->size();
  while (!priority_queue.IsEmpty()) {
    NodeEntryT* current = priority_queue.Top();
    const NodeIndex current_node = current->node();
    priority_queue.Pop();
    current->set_settled(true);
    if (current->is_destination()) {
      destinations_remaining--;
      if (destinations_remaining == 0) {
        break;
      }
    }
    const PathDistance current_distance = current->distance();
    for (const ArcIndex arc : graph->OutgoingArcs(current_node)) {
      const NodeIndex next = graph->Head(arc);
      NodeEntryT* const entry = &entries[next];
      if (!entry->settled()) {
        DCHECK_GE(current_distance, 0);
        const PathDistance arc_length = arc_lengths->at(arc);
        DCHECK_LE(current_distance, kDisconnectedPathDistance - arc_length);
        if (InsertOrUpdateEntry(current_distance + arc_length, entry,
                                &priority_queue)) {
          predecessor[next] = current_node;
        }
      }
    }
  }
  const int destinations_size = destinations->size();
  std::vector<PathDistance> distances(destinations_size,
                                      kDisconnectedPathDistance);
  for (int i = 0; i < destinations_size; ++i) {
    NodeIndex node = destinations->at(i);
    if (entries[node].settled()) {
      distances[i] = entries[node].distance();
    }
  }
  paths->StoreSingleSourcePaths(source, predecessor, distances);
}

}  // namespace internal

template <class GraphType>
GenericPathContainer<GraphType>::GenericPathContainer() = default;

template <class GraphType>
GenericPathContainer<GraphType>::~GenericPathContainer() = default;

template <class GraphType>
PathDistance GenericPathContainer<GraphType>::GetDistance(NodeIndex from,
                                                          NodeIndex to) const {
  DCHECK(container_ != nullptr);
  return container_->GetDistance(from, to);
}

template <class GraphType>
typename GenericPathContainer<GraphType>::NodeIndex
GenericPathContainer<GraphType>::GetPenultimateNodeInPath(NodeIndex from,
                                                          NodeIndex to) const {
  DCHECK(container_ != nullptr);
  return container_->GetPenultimateNodeInPath(from, to);
}

template <class GraphType>
void GenericPathContainer<GraphType>::GetPath(
    NodeIndex from, NodeIndex to, std::vector<NodeIndex>* path) const {
  DCHECK(container_ != nullptr);
  DCHECK(path != nullptr);
  container_->GetPath(from, to, path);
}

template <class GraphType>
void GenericPathContainer<GraphType>::BuildPathDistanceContainer(
    GenericPathContainer* const path_container) {
  CHECK(path_container != nullptr);
  path_container->container_ = std::make_unique<
      internal::DistanceContainer<NodeIndex, GraphType::kNilNode>>();
}

template <class GraphType>
void GenericPathContainer<GraphType>::BuildInMemoryCompactPathContainer(
    GenericPathContainer* const path_container) {
  CHECK(path_container != nullptr);
  path_container->container_ = std::make_unique<
      internal::InMemoryCompactPathContainer<NodeIndex, GraphType::kNilNode>>();
}

template <class GraphType>
GenericPathContainer<GraphType>
GenericPathContainer<GraphType>::BuildPathDistanceContainer() {
  return GenericPathContainer(
      std::make_unique<
          internal::DistanceContainer<NodeIndex, GraphType::kNilNode>>());
}

template <class GraphType>
GenericPathContainer<GraphType>
GenericPathContainer<GraphType>::BuildInMemoryCompactPathContainer() {
  return GenericPathContainer(
      std::make_unique<internal::InMemoryCompactPathContainer<
          NodeIndex, GraphType::kNilNode>>());
}

// TODO(b/385094969): Remove second template parameter when all clients are
// migrated.
template <class GraphType, class PathContainerGraphType>
void ComputeManyToManyShortestPathsWithMultipleThreads(
    const GraphType& graph, const std::vector<PathDistance>& arc_lengths,
    const std::vector<typename GraphType::NodeIndex>& sources,
    const std::vector<typename GraphType::NodeIndex>& destinations,
    int num_threads,
    GenericPathContainer<PathContainerGraphType>* const paths) {
  static_assert(std::is_same_v<typename GraphType::NodeIndex,
                               typename PathContainerGraphType::NodeIndex>,
                "use an explicit `GenericPathContainer<T>` instead of using "
                "`PathContainer`");
  static_assert(GraphType::kNilNode == PathContainerGraphType::kNilNode,
                "use an explicit `GenericPathContainer<T>` instead of using "
                "`PathContainer`");
  if (graph.num_nodes() > 0) {
    CHECK_EQ(graph.num_arcs(), arc_lengths.size())
        << "Number of arcs in graph must match arc length vector size";
    // Removing duplicate sources to allow mutex-free implementation (and it's
    // more efficient); same with destinations for efficiency reasons.
    std::vector<typename GraphType::NodeIndex> unique_sources = sources;
    ::gtl::STLSortAndRemoveDuplicates(&unique_sources);
    std::vector<typename GraphType::NodeIndex> unique_destinations =
        destinations;
    ::gtl::STLSortAndRemoveDuplicates(&unique_destinations);
    WallTimer timer;
    timer.Start();
    auto* const container = paths->GetImplementation();
    container->Initialize(unique_sources, unique_destinations,
                          graph.num_nodes());
    {
      std::unique_ptr<ThreadPool> pool(new ThreadPool(num_threads));
      pool->StartWorkers();
      for (int i = 0; i < unique_sources.size(); ++i) {
        pool->Schedule(absl::bind_front(
            &internal::ComputeOneToManyOnGraph<GraphType,
                                               PathContainerGraphType>,
            &graph, &arc_lengths, unique_sources[i], &unique_destinations,
            container));
      }
    }
    container->Finalize();
    VLOG(2) << "Elapsed time to compute shortest paths: " << timer.Get() << "s";
  }
}

}  // namespace operations_research

#endif  // OR_TOOLS_GRAPH_SHORTEST_PATHS_H_
