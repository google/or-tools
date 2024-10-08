// Copyright 2010-2024 Google LLC
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

#include "ortools/graph/shortest_paths.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

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
#include "ortools/graph/ebert_graph.h"

namespace operations_research {

// TODO(user): Currently using StarGraph::kNilNode until the new Ebert
// graphs appear; switch to kNilNode on the base Ebert graph class when
// available.

// Base path container implementation class. Defines virtual functions used to
// fill the container (in particular from the shortest path computation
// function).
class PathContainerImpl {
 public:
  PathContainerImpl() {}
  virtual ~PathContainerImpl() {}

  // Initializes the container on source and destination node vectors (num_nodes
  // is the total number of nodes in the graph containing sources and nodes).
  // Called before adding any paths to the container.
  virtual void Initialize(const std::vector<NodeIndex>& sources,
                          const std::vector<NodeIndex>& destinations,
                          NodeIndex num_nodes) = 0;

  // Called when no more path will be added to the container.
  virtual void Finalize() {}

  // Returns the distance between node 'from' and node 'to' following the path
  // out of 'from' and into 'to'. Note that if 'from' == 'to', the distance is
  // not necessarily 0 if the path out of 'to' and back into 'to' has a distance
  // greater than 0. If you do require the distance to be 0 in this case, add to
  // the graph an arc from 'to' to itself with a length of 0.
  // If nodes are not connected, returns kDisconnectedPathDistance.
  virtual PathDistance GetDistance(NodeIndex from, NodeIndex to) const = 0;

  // Returns the penultimate node on the path out of node 'from' into node 'to'
  // (the direct predecessor of node 'to' on the path).
  // If 'from' == 'to', the penultimate node is 'to' only if the shortest path
  // from 'to' to itself is composed of the arc ('to, 'to'), which might not be
  // the case if either this arc doesn't exist or if the length of this arc is
  // greater than the distance of an alternate path.
  // If nodes are not connected, returns kNilNode.
  virtual NodeIndex GetPenultimateNodeInPath(NodeIndex from,
                                             NodeIndex to) const = 0;

  // Returns path nodes from node "from" to node "to" in a ordered vector.
  virtual void GetPath(NodeIndex from, NodeIndex to,
                       std::vector<NodeIndex>* path) const = 0;

  // Adds a path tree rooted at node 'from', and to a set of implicit
  // destinations:
  // - predecessor_in_path_tree[node] is the predecessor of node 'node' in the
  //   path from 'from' to 'node', or kNilNode if there is no
  //   predecessor (i.e. if 'node' is not in the path tree);
  // - distance_to_destination[i] is the distance from 'from' to the i-th
  //   destination (see Initialize()).
  virtual void StoreSingleSourcePaths(
      NodeIndex from, const std::vector<NodeIndex>& predecessor_in_path_tree,
      const std::vector<PathDistance>& distance_to_destination) = 0;
};

namespace {

// Class designed to store the tree of paths from a root node to a set of nodes
// in a very compact way (over performance).
// Memory consumption is in O(n) (n being the size of the tree) where node
// indices are "very" non-contiguous (extremely sparse node indices). It keeps
// node-sorted arrays of node and parent pairs, which can be accessed in
// O(log(n)) with a binary search.
// The creation of the tree is done in O(n*log(n)) time.
// Note that this class uses temporary memory for each call to Initialize which
// is only an issue for massive parallel calls; in practice for shortest
// paths computation, the number of threads calling Initialize is very small
// compared to the total number of trees created.
class PathTree {
 public:
  PathTree() : nodes_(), parents_() {}

  void Initialize(absl::Span<const NodeIndex> paths,
                  absl::Span<const NodeIndex> destinations);

  // Returns the parent (predecessor) of 'node' in the tree in
  // O(log(path_tree_size)), where path_tree_size is the size of nodes_.
  NodeIndex GetParent(NodeIndex node) const;

  // Returns the path from node 'from' to node 'to' in the tree in
  // O(log(path_tree_size) + path_size), where path_tree_size is the size of
  // nodes_ and path_size is the size of the resulting path.
  void GetPath(NodeIndex from, NodeIndex to,
               std::vector<NodeIndex>* path) const;

 private:
  std::vector<NodeIndex> nodes_;
  std::vector<int> parents_;
};

// Initializes the tree from a non-sparse representation of the path tree
// represented by 'paths'. The tree is reduced to the subtree in which nodes in
// 'destinations' are the leafs.
void PathTree::Initialize(absl::Span<const NodeIndex> paths,
                          absl::Span<const NodeIndex> destinations) {
  const NodeIndex kNilNode = StarGraph::kNilNode;
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
    // Using a dense_hash_map gives a huge speedup over hash_map here, mainly
    // due to better memory management.
    absl::flat_hash_map<NodeIndex, int> node_indices;

    for (int i = 0; i < num_nodes; ++i) {
      node_indices[tree[i].first] = i;
    }
    parents_.resize(num_nodes, -1);
    for (int i = 0; i < num_nodes; ++i) {
      if (tree[i].second == kNilNode) {
        // dense_hash_map does not allow empty key to be used as a key.
        parents_[i] = kNilNode;
      } else {
        parents_[i] =
            gtl::FindWithDefault(node_indices, tree[i].second, kNilNode);
      }
    }
  }
  nodes_.resize(num_nodes, kNilNode);
  for (int i = 0; i < num_nodes; ++i) {
    nodes_[i] = tree[i].first;
  }
}

NodeIndex PathTree::GetParent(NodeIndex node) const {
  std::vector<NodeIndex>::const_iterator node_position =
      std::lower_bound(nodes_.begin(), nodes_.end(), node);
  if (node_position != nodes_.end() && *node_position == node) {
    const int parent = parents_[node_position - nodes_.begin()];
    if (parent != StarGraph::kNilNode) {
      return nodes_[parent];
    }
  }
  return StarGraph::kNilNode;
}

void PathTree::GetPath(NodeIndex from, NodeIndex to,
                       std::vector<NodeIndex>* path) const {
  DCHECK(path != nullptr);
  path->clear();
  std::vector<NodeIndex>::const_iterator to_position =
      std::lower_bound(nodes_.begin(), nodes_.end(), to);
  if (to_position != nodes_.end() && *to_position == to) {
    int current_index = to_position - nodes_.begin();
    NodeIndex current_node = to;
    while (current_node != from) {
      path->push_back(current_node);
      current_index = parents_[current_index];
      // from and to are not connected
      if (current_index == StarGraph::kNilNode) {
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
class DistanceContainer : public PathContainerImpl {
 public:
  DistanceContainer() : reverse_sources_(), distances_() {}

  // This type is neither copyable nor movable.
  DistanceContainer(const DistanceContainer&) = delete;
  DistanceContainer& operator=(const DistanceContainer&) = delete;
  ~DistanceContainer() override {}
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
  NodeIndex GetPenultimateNodeInPath(NodeIndex from,
                                     NodeIndex to) const override {
    (void)from;
    (void)to;

    LOG(FATAL) << "Path not stored.";
    return StarGraph::kNilNode;
  }
  void GetPath(NodeIndex from, NodeIndex to,
               std::vector<NodeIndex>* path) const override {
    (void)from;
    (void)to;
    (void)path;

    LOG(FATAL) << "Path not stored.";
  }
  void StoreSingleSourcePaths(
      NodeIndex from, const std::vector<NodeIndex>& predecessor_in_path_tree,
      const std::vector<PathDistance>& distance_to_destination) override {
    // DistanceContainer only stores distances and not predecessors.
    (void)predecessor_in_path_tree;

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

  std::vector<std::vector<PathDistance> > distances_;
};

// Path container which stores explicit paths and distances between path nodes.
class InMemoryCompactPathContainer : public DistanceContainer {
 public:
  InMemoryCompactPathContainer() : trees_(), destinations_() {}

  // This type is neither copyable nor movable.
  InMemoryCompactPathContainer(const InMemoryCompactPathContainer&) = delete;
  InMemoryCompactPathContainer& operator=(const InMemoryCompactPathContainer&) =
      delete;
  ~InMemoryCompactPathContainer() override {}
  void Initialize(const std::vector<NodeIndex>& sources,
                  const std::vector<NodeIndex>& destinations,
                  NodeIndex num_nodes) override {
    DistanceContainer::Initialize(sources, destinations, num_nodes);
    destinations_ = destinations;
    trees_.clear();
    trees_.resize(sources.size());
  }
  NodeIndex GetPenultimateNodeInPath(NodeIndex from,
                                     NodeIndex to) const override {
    return trees_[reverse_sources_[from]].GetParent(to);
  }
  void GetPath(NodeIndex from, NodeIndex to,
               std::vector<NodeIndex>* path) const override {
    DCHECK(path != nullptr);
    trees_[reverse_sources_[from]].GetPath(from, to, path);
  }
  void StoreSingleSourcePaths(
      NodeIndex from, const std::vector<NodeIndex>& predecessor_in_path_tree,
      const std::vector<PathDistance>& distance_to_destination) override {
    DistanceContainer::StoreSingleSourcePaths(from, predecessor_in_path_tree,
                                              distance_to_destination);
    trees_[reverse_sources_[from]].Initialize(predecessor_in_path_tree,
                                              destinations_);
  }

 private:
  std::vector<PathTree> trees_;
  std::vector<NodeIndex> destinations_;
};

// Priority queue node entry in the boundary of the Dijkstra algorithm.
class NodeEntry {
 public:
  NodeEntry()
      : heap_index_(-1),
        distance_(0),
        node_(StarGraph::kNilNode),
        settled_(false),
        is_destination_(false) {}
  bool operator<(const NodeEntry& other) const {
    return distance_ > other.distance_;
  }
  void SetHeapIndex(int h) { heap_index_ = h; }
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
bool InsertOrUpdateEntry(PathDistance distance, NodeEntry* entry,
                         AdjustablePriorityQueue<NodeEntry>* priority_queue) {
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

// If one wants to use int64_t for either priority or NodeIndex, one should
// consider using packed ints (putting the two bools with heap_index, for
// example) in order to stay at 16 bytes instead of 24.
static_assert(sizeof(NodeEntry) == 16, "node_entry_class_is_not_well_packed");

// Computes shortest paths from node 'source' to nodes in 'destinations'
// using a binary heap-based Dijkstra algorithm.
// TODO(user): Investigate alternate implementation which wouldn't use
// AdjustablePriorityQueue.
template <class GraphType>
void ComputeOneToManyInternalOnGraph(
    const GraphType* const graph,
    const std::vector<PathDistance>* const arc_lengths,
    typename GraphType::NodeIndex source,
    const std::vector<typename GraphType::NodeIndex>* const destinations,
    PathContainerImpl* const paths) {
  CHECK(graph != nullptr);
  CHECK(arc_lengths != nullptr);
  CHECK(destinations != nullptr);
  CHECK(paths != nullptr);
  const int num_nodes = graph->num_nodes();
  std::vector<typename GraphType::NodeIndex> predecessor(num_nodes, -1);
  AdjustablePriorityQueue<NodeEntry> priority_queue;
  std::vector<NodeEntry> entries(num_nodes);
  for (const typename GraphType::NodeIndex node : graph->AllNodes()) {
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
  for (const typename GraphType::ArcIndex arc : graph->OutgoingArcs(source)) {
    const typename GraphType::NodeIndex next = graph->Head(arc);
    if (InsertOrUpdateEntry(arc_lengths->at(arc), &entries[next],
                            &priority_queue)) {
      predecessor[next] = source;
    }
  }
  int destinations_remaining = destinations->size();
  while (!priority_queue.IsEmpty()) {
    NodeEntry* current = priority_queue.Top();
    const typename GraphType::NodeIndex current_node = current->node();
    priority_queue.Pop();
    current->set_settled(true);
    if (current->is_destination()) {
      destinations_remaining--;
      if (destinations_remaining == 0) {
        break;
      }
    }
    const PathDistance current_distance = current->distance();
    for (const typename GraphType::ArcIndex arc :
         graph->OutgoingArcs(current_node)) {
      const typename GraphType::NodeIndex next = graph->Head(arc);
      NodeEntry* const entry = &entries[next];
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
}  // namespace

PathContainer::PathContainer() {}

PathContainer::~PathContainer() {}

PathDistance PathContainer::GetDistance(NodeIndex from, NodeIndex to) const {
  DCHECK(container_ != nullptr);
  return container_->GetDistance(from, to);
}

NodeIndex PathContainer::GetPenultimateNodeInPath(NodeIndex from,
                                                  NodeIndex to) const {
  DCHECK(container_ != nullptr);
  return container_->GetPenultimateNodeInPath(from, to);
}

void PathContainer::GetPath(NodeIndex from, NodeIndex to,
                            std::vector<NodeIndex>* path) const {
  DCHECK(container_ != nullptr);
  DCHECK(path != nullptr);
  container_->GetPath(from, to, path);
}

PathContainerImpl* PathContainer::GetImplementation() const {
  return container_.get();
}

void PathContainer::BuildPathDistanceContainer(
    PathContainer* const path_container) {
  CHECK(path_container != nullptr);
  path_container->container_ = std::make_unique<DistanceContainer>();
}

void PathContainer::BuildInMemoryCompactPathContainer(
    PathContainer* const path_container) {
  CHECK(path_container != nullptr);
  path_container->container_ = std::make_unique<InMemoryCompactPathContainer>();
}

template <class GraphType>
void ComputeManyToManyShortestPathsWithMultipleThreadsInternal(
    const GraphType& graph, const std::vector<PathDistance>& arc_lengths,
    const std::vector<typename GraphType::NodeIndex>& sources,
    const std::vector<typename GraphType::NodeIndex>& destinations,
    int num_threads, PathContainer* const paths) {
  if (graph.num_nodes() > 0) {
    CHECK_EQ(graph.num_arcs(), arc_lengths.size())
        << "Number of arcs in graph must match arc length vector size";
    // Removing duplicate sources to allow mutex-free implementation (and it's
    // more efficient); same with destinations for efficiency reasons.
    std::vector<typename GraphType::NodeIndex> unique_sources = sources;
    gtl::STLSortAndRemoveDuplicates(&unique_sources);
    std::vector<typename GraphType::NodeIndex> unique_destinations =
        destinations;
    gtl::STLSortAndRemoveDuplicates(&unique_destinations);
    WallTimer timer;
    timer.Start();
    PathContainerImpl* container = paths->GetImplementation();
    container->Initialize(unique_sources, unique_destinations,
                          graph.num_nodes());
    {
      std::unique_ptr<ThreadPool> pool(new ThreadPool(num_threads));
      pool->StartWorkers();
      for (int i = 0; i < unique_sources.size(); ++i) {
        pool->Schedule(absl::bind_front(
            &ComputeOneToManyInternalOnGraph<GraphType>, &graph, &arc_lengths,
            unique_sources[i], &unique_destinations, container));
      }
    }
    container->Finalize();
    VLOG(2) << "Elapsed time to compute shortest paths: " << timer.Get() << "s";
  }
}

template <>
void ComputeManyToManyShortestPathsWithMultipleThreads(
    const ListGraph<>& graph, const std::vector<PathDistance>& arc_lengths,
    const std::vector<ListGraph<>::NodeIndex>& sources,
    const std::vector<ListGraph<>::NodeIndex>& destinations, int num_threads,
    PathContainer* const path_container) {
  ComputeManyToManyShortestPathsWithMultipleThreadsInternal(
      graph, arc_lengths, sources, destinations, num_threads, path_container);
}

template <>
void ComputeManyToManyShortestPathsWithMultipleThreads(
    const StaticGraph<>& graph, const std::vector<PathDistance>& arc_lengths,
    const std::vector<StaticGraph<>::NodeIndex>& sources,
    const std::vector<StaticGraph<>::NodeIndex>& destinations, int num_threads,
    PathContainer* const path_container) {
  ComputeManyToManyShortestPathsWithMultipleThreadsInternal(
      graph, arc_lengths, sources, destinations, num_threads, path_container);
}

template <>
void ComputeManyToManyShortestPathsWithMultipleThreads(
    const ReverseArcListGraph<>& graph,
    const std::vector<PathDistance>& arc_lengths,
    const std::vector<ReverseArcListGraph<>::NodeIndex>& sources,
    const std::vector<ReverseArcListGraph<>::NodeIndex>& destinations,
    int num_threads, PathContainer* const path_container) {
  ComputeManyToManyShortestPathsWithMultipleThreadsInternal(
      graph, arc_lengths, sources, destinations, num_threads, path_container);
}

template <>
void ComputeManyToManyShortestPathsWithMultipleThreads(
    const ReverseArcStaticGraph<>& graph,
    const std::vector<PathDistance>& arc_lengths,
    const std::vector<ReverseArcStaticGraph<>::NodeIndex>& sources,
    const std::vector<ReverseArcStaticGraph<>::NodeIndex>& destinations,
    int num_threads, PathContainer* const path_container) {
  ComputeManyToManyShortestPathsWithMultipleThreadsInternal(
      graph, arc_lengths, sources, destinations, num_threads, path_container);
}

template <>
void ComputeManyToManyShortestPathsWithMultipleThreads(
    const ReverseArcMixedGraph<>& graph,
    const std::vector<PathDistance>& arc_lengths,
    const std::vector<ReverseArcMixedGraph<>::NodeIndex>& sources,
    const std::vector<ReverseArcMixedGraph<>::NodeIndex>& destinations,
    int num_threads, PathContainer* const path_container) {
  ComputeManyToManyShortestPathsWithMultipleThreadsInternal(
      graph, arc_lengths, sources, destinations, num_threads, path_container);
}

}  // namespace operations_research
