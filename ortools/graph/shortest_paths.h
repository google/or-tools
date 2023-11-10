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

// This file contains functions to compute shortest paths on Ebert graphs using
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
// computations (cf. PathContainer).
//
// Usage example computing all-pair shortest paths on a graph:
//     StarGraph graph(...,...);
//     ZVector<uint32_t> arc_lengths(...,...);
//     ... populate graph and arc lengths ...
//     PathContainer container;
//     PathContainer::BuildInMemoryCompactPathContainer(&container);
//     ComputeAllToAllShortestPathsWithMultipleThreads(graph,
//                                                     arc_lengths,
//                                                     /*num_threads=*/4,
//                                                     &container);
//
// Usage example computing shortest paths between a subset of graph nodes:
//     StarGraph graph(...,...);
//     ZVector<uint32_t> arc_lengths(...,...);
//     ... populate graph and arc lengths ...
//     vector<NodeIndex> sources;
//     vector<NodeIndex> sinks;
//     ... fill sources and sinks ...
//     PathContainer container;
//     PathContainer::BuildInMemoryCompactPathContainer(&container);
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
#include <vector>

#include "ortools/base/logging.h"
#include "ortools/base/types.h"
#include "ortools/graph/ebert_graph.h"
#include "ortools/graph/graph.h"
#include "ortools/util/zvector.h"

namespace operations_research {

// Storing distances on 32 bits to limit memory consumption of distance
// matrices. If distances don't fit on 32 bits, scaling and losing a bit of
// precision should be acceptable in practice.
template <class T>
class ZVector;

typedef uint32_t PathDistance;

const PathDistance kDisconnectedPathDistance =
    std::numeric_limits<uint32_t>::max();

class PathContainerImpl;

// Container class storing paths and distances along the paths. It is used in
// shortest path computation functions to store resulting shortest paths.
// Usage example iterating on the path between nodes from and to:
//     PathContainer path_container;
//     PathContainer::BuildInMemoryCompactPathContainer(&path_container);
//     ... fill up container ...
//     const NodeIndex from =...;
//     NodeIndex to =...;
//     while (to != from) {
//       LOG(INFO) << to;
//       to = path_container.GetPenultimateNodeInPath(from, to);
//     }
class PathContainer {
 public:
  PathContainer();

  // This type is neither copyable nor movable.
  PathContainer(const PathContainer&) = delete;
  PathContainer& operator=(const PathContainer&) = delete;

  ~PathContainer();

  // Returns the distance between node 'from' and node 'to' following the path
  // out of 'from' and into 'to'. Note that if 'from' == 'to', the distance is
  // not necessarily 0 if the path out of 'to' and back into 'to' has a distance
  // greater than 0. If you do require the distance to be 0 in this case, add to
  // the graph an arc from 'to' to itself with a length of 0.
  // If nodes are not connected, returns kDisconnectedPathDistance.
  PathDistance GetDistance(NodeIndex from, NodeIndex to) const;

  // Returns the penultimate node on the path out of node 'from' into node 'to'
  // (the direct predecessor of node 'to' on the path).
  // If 'from' == 'to', the penultimate node is 'to' only if the shortest path
  // from 'to' to itself is composed of the arc ('to, 'to'), which might not be
  // the case if either this arc doesn't exist or if the length of this arc is
  // greater than the distance of an alternate path.
  // If nodes are not connected, returns StarGraph::kNilNode.
  NodeIndex GetPenultimateNodeInPath(NodeIndex from, NodeIndex to) const;

  // Returns path nodes from node "from" to node "to" in a ordered vector.
  // The vector starts with 'from' and ends with 'to', if both nodes are
  // connected (otherwise an empty vector is returned).
  void GetPath(NodeIndex from, NodeIndex to,
               std::vector<NodeIndex>* path) const;

  // For internal use only. Returns the internal container implementation.
  PathContainerImpl* GetImplementation() const;

  // Builds a path container which only stores distances between path nodes.
  static void BuildPathDistanceContainer(PathContainer* path_container);

  // Builds a path container which stores explicit paths and distances between
  // path nodes in a memory-compact representation.
  // In this case GetPenultimateNodeInPath() is O(log(path_tree_size)),
  // path_tree_size being the size of a tree of paths from a source node (in
  // practice it is equal to the number of nodes in the graph if all nodes
  // are strongly connected).
  // GetPath is O(log(path_tree_size) + path_size), where path_size is the
  // size of the resulting path; note this is faster than successive calls
  // to GetPenultimateNodeInPath() which would result in
  // O(log(path_tree_size) * path_size).
  static void BuildInMemoryCompactPathContainer(PathContainer* path_container);

  // TODO(user): Add save-to-disk container.
  // TODO(user): Add BuildInMemoryFastPathContainer(), which does
  // GetPenultimateNodeInPath() in O(1).

 private:
  std::unique_ptr<PathContainerImpl> container_;
};

// Utility function which returns a vector containing all nodes of a graph.
template <class GraphType>
void GetGraphNodes(const GraphType& graph, std::vector<NodeIndex>* nodes) {
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
// the arcs of the graph (arc_lengths[arc] is the length of arc).
// Resulting shortest paths are stored in a path container 'path_container'.

// Computes shortest paths from the node 'source' to all nodes in the graph.
template <class GraphType>
void ComputeOneToAllShortestPaths(const GraphType& graph,
                                  const ZVector<PathDistance>& arc_lengths,
                                  NodeIndex source,
                                  PathContainer* const path_container) {
  std::vector<NodeIndex> all_nodes;
  GetGraphNodes<GraphType>(graph, &all_nodes);
  ComputeOneToManyShortestPaths(graph, arc_lengths, source, all_nodes,
                                path_container);
}

template <class GraphType>
void ComputeOneToAllShortestPaths(const GraphType& graph,
                                  const std::vector<PathDistance>& arc_lengths,
                                  typename GraphType::NodeIndex source,
                                  PathContainer* const path_container) {
  std::vector<typename GraphType::NodeIndex> all_nodes;
  GetGraphNodesFromGraph<GraphType>(graph, &all_nodes);
  ComputeOneToManyShortestPaths(graph, arc_lengths, source, all_nodes,
                                path_container);
}

// Computes shortest paths from the node 'source' to nodes in 'destinations'.
template <class GraphType>
void ComputeOneToManyShortestPaths(const GraphType& graph,
                                   const ZVector<PathDistance>& arc_lengths,
                                   NodeIndex source,
                                   const std::vector<NodeIndex>& destinations,
                                   PathContainer* const path_container) {
  std::vector<NodeIndex> sources(1, source);
  ComputeManyToManyShortestPathsWithMultipleThreads(
      graph, arc_lengths, sources, destinations, 1, path_container);
}

template <class GraphType>
void ComputeOneToManyShortestPaths(
    const GraphType& graph, const std::vector<PathDistance>& arc_lengths,
    typename GraphType::NodeIndex source,
    const std::vector<typename GraphType::NodeIndex>& destinations,
    PathContainer* const path_container) {
  std::vector<typename GraphType::NodeIndex> sources(1, source);
  ComputeManyToManyShortestPathsWithMultipleThreads(
      graph, arc_lengths, sources, destinations, 1, path_container);
}

// Computes shortest paths from the nodes in 'sources' to all nodes in the
// graph.
template <class GraphType>
void ComputeManyToAllShortestPathsWithMultipleThreads(
    const GraphType& graph, const ZVector<PathDistance>& arc_lengths,
    const std::vector<NodeIndex>& sources, int num_threads,
    PathContainer* const path_container) {
  std::vector<NodeIndex> all_nodes;
  GetGraphNodes<GraphType>(graph, &all_nodes);
  ComputeManyToManyShortestPathsWithMultipleThreads(
      graph, arc_lengths, sources, all_nodes, num_threads, path_container);
}

template <class GraphType>
void ComputeManyToAllShortestPathsWithMultipleThreads(
    const GraphType& graph, const std::vector<PathDistance>& arc_lengths,
    const std::vector<typename GraphType::NodeIndex>& sources, int num_threads,
    PathContainer* const path_container) {
  std::vector<typename GraphType::NodeIndex> all_nodes;
  GetGraphNodesFromGraph<GraphType>(graph, &all_nodes);
  ComputeManyToManyShortestPathsWithMultipleThreads(
      graph, arc_lengths, sources, all_nodes, num_threads, path_container);
}

// Computes shortest paths from the nodes in 'sources' to the nodes in
// 'destinations'.
template <class GraphType>
void ComputeManyToManyShortestPathsWithMultipleThreads(
    const GraphType& graph, const ZVector<PathDistance>& arc_lengths,
    const std::vector<NodeIndex>& sources,
    const std::vector<NodeIndex>& destinations, int num_threads,
    PathContainer* const path_container) {
  LOG(DFATAL) << "Graph type not supported";
}

template <class GraphType>
void ComputeManyToManyShortestPathsWithMultipleThreads(
    const GraphType& graph, const std::vector<PathDistance>& arc_lengths,
    const std::vector<typename GraphType::NodeIndex>& sources,
    const std::vector<typename GraphType::NodeIndex>& destinations,
    int num_threads, PathContainer* const path_container) {
  LOG(DFATAL) << "Graph type not supported";
}

// Specialization for supported graph classes.

template <>
void ComputeManyToManyShortestPathsWithMultipleThreads(
    const StarGraph& graph, const ZVector<PathDistance>& arc_lengths,
    const std::vector<NodeIndex>& sources,
    const std::vector<NodeIndex>& destinations, int num_threads,
    PathContainer* path_container);

template <>
void ComputeManyToManyShortestPathsWithMultipleThreads(
    const ForwardStarGraph& graph, const ZVector<PathDistance>& arc_lengths,
    const std::vector<NodeIndex>& sources,
    const std::vector<NodeIndex>& destinations, int num_threads,
    PathContainer* path_container);

// Computes shortest paths between all nodes of the graph.
template <class GraphType>
void ComputeAllToAllShortestPathsWithMultipleThreads(
    const GraphType& graph, const ZVector<PathDistance>& arc_lengths,
    int num_threads, PathContainer* const path_container) {
  std::vector<NodeIndex> all_nodes;
  GetGraphNodes<GraphType>(graph, &all_nodes);
  ComputeManyToManyShortestPathsWithMultipleThreads(
      graph, arc_lengths, all_nodes, all_nodes, num_threads, path_container);
}

using ::util::ListGraph;
template <>
void ComputeManyToManyShortestPathsWithMultipleThreads(
    const ListGraph<>& graph, const std::vector<PathDistance>& arc_lengths,
    const std::vector<ListGraph<>::NodeIndex>& sources,
    const std::vector<ListGraph<>::NodeIndex>& destinations, int num_threads,
    PathContainer* path_container);

using ::util::StaticGraph;
template <>
void ComputeManyToManyShortestPathsWithMultipleThreads(
    const StaticGraph<>& graph, const std::vector<PathDistance>& arc_lengths,
    const std::vector<StaticGraph<>::NodeIndex>& sources,
    const std::vector<StaticGraph<>::NodeIndex>& destinations, int num_threads,
    PathContainer* path_container);

using ::util::ReverseArcListGraph;
template <>
void ComputeManyToManyShortestPathsWithMultipleThreads(
    const ReverseArcListGraph<>& graph,
    const std::vector<PathDistance>& arc_lengths,
    const std::vector<ReverseArcListGraph<>::NodeIndex>& sources,
    const std::vector<ReverseArcListGraph<>::NodeIndex>& destinations,
    int num_threads, PathContainer* path_container);

using ::util::ReverseArcStaticGraph;
template <>
void ComputeManyToManyShortestPathsWithMultipleThreads(
    const ReverseArcStaticGraph<>& graph,
    const std::vector<PathDistance>& arc_lengths,
    const std::vector<ReverseArcStaticGraph<>::NodeIndex>& sources,
    const std::vector<ReverseArcStaticGraph<>::NodeIndex>& destinations,
    int num_threads, PathContainer* path_container);

using ::util::ReverseArcMixedGraph;
template <>
void ComputeManyToManyShortestPathsWithMultipleThreads(
    const ReverseArcMixedGraph<>& graph,
    const std::vector<PathDistance>& arc_lengths,
    const std::vector<ReverseArcMixedGraph<>::NodeIndex>& sources,
    const std::vector<ReverseArcMixedGraph<>::NodeIndex>& destinations,
    int num_threads, PathContainer* path_container);

// Computes shortest paths between all nodes of the graph.
template <class GraphType>
void ComputeAllToAllShortestPathsWithMultipleThreads(
    const GraphType& graph, const std::vector<PathDistance>& arc_lengths,
    int num_threads, PathContainer* const path_container) {
  std::vector<typename GraphType::NodeIndex> all_nodes;
  GetGraphNodesFromGraph<GraphType>(graph, &all_nodes);
  ComputeManyToManyShortestPathsWithMultipleThreads(
      graph, arc_lengths, all_nodes, all_nodes, num_threads, path_container);
}

}  // namespace operations_research

#endif  // OR_TOOLS_GRAPH_SHORTEST_PATHS_H_
