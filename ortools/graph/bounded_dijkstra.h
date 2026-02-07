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

#ifndef ORTOOLS_GRAPH_BOUNDED_DIJKSTRA_H_
#define ORTOOLS_GRAPH_BOUNDED_DIJKSTRA_H_

#include <algorithm>
#include <cstddef>
#include <functional>
#include <limits>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/base/attributes.h"
#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/base/strong_int.h"
#include "ortools/base/strong_vector.h"
#include "ortools/base/top_n.h"
#include "ortools/graph_base/graph.h"

namespace operations_research {

// Computes a shortest path from source to destination in a weighted directed
// graph, specified as an arc list.
//
// This function also demonstrates how to use the more feature rich
// BoundedDijkstraWrapper (defined below) in the simple case, see the
// implementation at the bottom of this file.
//
// We take a sparse directed input graph with nodes indexed in [0, num_nodes).
// Each arcs goes from a tail node to a head node (tail -> head) and must have a
// NON-NEGATIVE length. Self-arc or duplicate arcs are supported. This is
// provided as 3 parallel vectors of the same size. Note that we validate the
// input consistency with checks.
//
// If your graph is undirected, you can easily transform it by adding two arcs
// (a -> b and b -> a) for each edge (a <-> b).
//
// This returns a pair (path length, node-path from source to destination)
// corresponding to a shortest path. Both source and destination will be
// included in the path.
//
// If destination is not reachable from source, or if the shortest path length
// is >= limit we will return {limit, {}}. As a consequence any arc length >=
// limit is the same as no arc. The code is also overflow-safe and will behave
// correctly if the limit is int64max or infinity.
template <typename NodeIndex, typename DistanceType>
std::pair<DistanceType, std::vector<NodeIndex>> SimpleOneToOneShortestPath(
    NodeIndex source, NodeIndex destination, absl::Span<const NodeIndex> tails,
    absl::Span<const NodeIndex> heads, absl::Span<const DistanceType> lengths,
    DistanceType limit = std::numeric_limits<DistanceType>::max());

namespace internal {

template <typename IndexType, typename ValueType>
using IndexedVector =
    std::conditional_t<util_intops::IsStrongInt<IndexType>::value,
                       ::util_intops::StrongVector<IndexType, ValueType>,
                       std::vector<ValueType>>;

template <class T, typename ArcIndex>
class ElementGetter {
 public:
  explicit ElementGetter(const IndexedVector<ArcIndex, T>& c) : c_(c) {}
  const T& operator()(ArcIndex index) const { return c_[index]; }

 private:
  const IndexedVector<ArcIndex, T>& c_;
};

}  // namespace internal

// A wrapper that holds the memory needed to run many bounded shortest path
// computations on the given graph. The graph must implement the
// interface described in graph.h (without the need for reverse arcs).
//
// We use the length and distance formalism here, but the arc lengths can
// represent any numeric physical quantity. A shortest path will just be a path
// minimizing this quantity. Arc length MUST be non-negative though. The code
// should work with both signed and unsigned integer, or floating point
// DistanceType.
//
// If one do not use source/destination distance offset, this class is
// integer-overflow safe, and one can safely use distance_limit =
// std::numeric_limits<int64_t>::max() for instance. Any arcs with a distance >=
// distance_limit will then be the same as a non-existent arc.
//
// With source/destination offsets, the potential integer overflow situation is
// trickier: one need to make sure that the range (distance_limit -
// source_offset) do not overflow in case of negative source_offset. And also
// that (distance_limit + destination_offset) do not overflow. Note that with
// negative source_offset, arc with a length greater than the distance_limit can
// still be considered!
template <class GraphType, class DistanceType,
          class ArcLengthFunctor = internal::ElementGetter<
              DistanceType, typename GraphType::ArcIndex>>
class BoundedDijkstraWrapper {
 public:
  typedef typename GraphType::NodeIndex NodeIndex;
  typedef typename GraphType::ArcIndex ArcIndex;
  typedef DistanceType distance_type;

  // A vector of T, indexed by NodeIndex/ArcIndex.
  template <typename T>
  using ByNode = internal::IndexedVector<NodeIndex, T>;
  template <typename T>
  using ByArc = internal::IndexedVector<ArcIndex, T>;

  // IMPORTANT: Both arguments must outlive the class. The arc lengths cannot be
  // negative and the vector must be of the correct size (both preconditions are
  // CHECKed).
  //
  // SUBTLE: The client can modify the graph and the arc length between calls to
  // RunBoundedDijkstra(). That's fine. Doing so will obviously invalidate the
  // reader API of the last Dijkstra run, which could return junk, or crash.
  BoundedDijkstraWrapper(const GraphType* graph,
                         const ByArc<DistanceType>* arc_lengths);

  // Variant that takes a custom arc length functor and copies it locally.
  BoundedDijkstraWrapper(const GraphType* graph,
                         ArcLengthFunctor arc_length_functor);

  // The typical Dijkstra, from a single source with distance zero, to all nodes
  // of the graph within the distance limit (exclusive). The first element of
  // the returned vector will always be the source_node with a distance of zero.
  // See RunBoundedDijkstraFromMultipleSources() for more information.
  const std::vector<NodeIndex>& RunBoundedDijkstra(
      NodeIndex source_node, DistanceType distance_limit) {
    return RunBoundedDijkstraFromMultipleSources({{source_node, 0}},
                                                 distance_limit);
  }

  // Finds the shortest path between two nodes, subject to the distance limit.
  // Returns true iff it exists and its length is < distance_limit.
  //
  // If this returns true, you can get the path distance with distances()[to]
  // and the path with ArcPathTo(to) or NodePathTo(to).
  bool OneToOneShortestPath(NodeIndex from, NodeIndex to,
                            DistanceType distance_limit);

  // Returns the list of all the nodes which are under the given distance limit
  // (exclusive) from at least one of the given source nodes (which also have
  // an initial distance offset, to be added to the distance).
  // The nodes are sorted by increasing distance.
  // By "distance", we mean the length of the shortest path from any source
  // plus the source's distance offset, where the length of a path is the
  // sum of the length of its arcs
  const std::vector<NodeIndex>& RunBoundedDijkstraFromMultipleSources(
      const std::vector<std::pair<NodeIndex, DistanceType>>&
          sources_with_distance_offsets,
      DistanceType distance_limit);

  // Like RunBoundedDijkstraFromMultipleSources(), but this one stops as soon as
  // it has determined the shortest path from any of the sources to the closest
  // "num_destinations_to_reach" destinations, and returns those destinations,
  // sorted by overall distance (also counting the destination offsets).
  //
  // If num_destinations_to_reach is non-positive, returns the empty vector, if
  // it is greater than the number of distinct destination nodes, it has no
  // effect (it's safe to do so).
  //
  // If the limit is reached before, the returned vector may have smaller size,
  // in particular it may be empty if none of the destinations was reachable.
  //
  // Both the sources and the destinations have a "distance offset", which is
  // added to the path length to determine the distance between them.
  //
  // The rest of the reader API below is available; with the caveat that you
  // should only try to access the nodes that have been reached by the search.
  // That's true for the returned node index (if not -1) and its ancestors.
  //
  // Note that the distances() will take the source offsets into account,
  // but not the destination offsets.
  std::vector<NodeIndex>
  RunBoundedDijkstraFromMultipleSourcesToMultipleDestinations(
      const std::vector<std::pair<NodeIndex, DistanceType>>&
          sources_with_distance_offsets,
      const std::vector<std::pair<NodeIndex, DistanceType>>&
          destinations_with_distance_offsets,
      int num_destinations_to_reach, DistanceType distance_limit);

  // Like RunBoundedDijkstraFromMultipleSources(), but will call a user-provided
  // callback "settled_node_callback" when settling each node ('settling' a node
  // happens at most once per node, when popping it from the Dijkstra queue,
  // meaning that the node has been fully 'processed'). This callback may modify
  // the distance limit dynamically, thus affecting the stopping criterion.
  const std::vector<NodeIndex>& RunBoundedDijkstraWithSettledNodeCallback(
      const std::vector<std::pair<NodeIndex, DistanceType>>&
          sources_with_distance_offsets,
      std::function<void(NodeIndex settled_node, DistanceType settled_distance,
                         DistanceType* distance_limit)>
          settled_node_callback,
      DistanceType distance_limit);

  // Returns true if `node` was reached by the last Run*() call.
  bool IsReachable(NodeIndex node) const { return is_reached_[node]; }

  // Returns all the reached nodes form the previous Run*() call.
  const ByNode<NodeIndex>& reached_nodes() const { return reached_nodes_; }

  // The following vectors are all indexed by graph node indices.
  //
  // IMPORTANT: after each Run*() function, only the positions of the
  // reached nodes are updated, the others will contain junk.

  // The distance of the nodes from their source.
  const ByNode<DistanceType>& distances() const { return distances_; }

  // The parent of the nodes in the shortest path from their source.
  // When a node doesn't have any parent (it has to be a source), its parent
  // is itself.
  // Note that a path will never contain the same node twice, even if some
  // arcs have a length of zero.
  // Note also that some sources may have parents, because of the initial
  // distances.
  const ByNode<NodeIndex>& parents() const { return parents_; }

  // The arc reaching a given node in the path from their source.
  // arc_from_source()[x] is undefined (i.e. junk) when parents()[x] == x.
  const ByNode<ArcIndex>& arc_from_source() const { return arc_from_source_; }

  // Returns the list of all the arcs in the shortest path from the node's
  // source to the node.
  std::vector<ArcIndex> ArcPathTo(NodeIndex node) const;

  ABSL_DEPRECATED("Use ArcPathTo() instead.")
  std::vector<ArcIndex> ArcPathToNode(NodeIndex node) const {
    return ArcPathTo(node);
  }

  // Returns the list of all the nodes in the shortest path from the node's
  // source to the node. This always start by the node's source, and end by
  // the given node. In the case that source == node, returns {node}.
  std::vector<NodeIndex> NodePathTo(NodeIndex node) const;

  // Returns the node's source. This is especially useful when running
  // Dijkstras from multiple sources.
  NodeIndex SourceOfShortestPathToNode(NodeIndex node) const;

  // Original Source/Destination index extraction, after a call to the
  // multi-source and/or multi-destination variants:
  // Retrieves the original index of the source or destination node in the
  // source/destination lists given in the method calls. Eg. if you called
  // RunBoundedDijkstraFromMultipleSourcesToMultipleDestinations(srcs, dsts),
  // then srcs[GetSourceIndex(node)] = (node, ...), for all "node" that appear
  // in "srcs". Ditto for dsts and GetDestinationIndex().
  //
  // If the node was listed several times as a source (or destinations), it'll
  // pick the listing with the smallest distance offset.
  // If the node is not a source (or destination), this returns junk: you can't
  // rely on the value.
  //
  // These methods are invalidated by the next RunBoundedDijkstra*() call.
  int GetSourceIndex(NodeIndex node) const;
  int GetDestinationIndex(NodeIndex node) const;

  // Trivial accessors to the underlying graph and arc lengths.
  const GraphType& graph() const { return *graph_; }
  const ByArc<DistanceType>& arc_lengths() const {
    CHECK(arc_lengths_);
    return *arc_lengths_;
  }
  DistanceType GetArcLength(ArcIndex arc) const {
    const DistanceType length = arc_length_functor_(arc);
    DCHECK_GE(length, 0);
    return length;
  }

 private:
  // The default copy constructor is problematic in a multi-threaded
  // environment.
  BoundedDijkstraWrapper(const BoundedDijkstraWrapper& other);

  // The Graph and length of each arc.
  const GraphType* const graph_;
  ArcLengthFunctor arc_length_functor_;
  const ByArc<DistanceType>* const arc_lengths_;

  // Data about the last Dijkstra run.
  ByNode<DistanceType> distances_;
  ByNode<NodeIndex> parents_;
  ByNode<ArcIndex> arc_from_source_;
  ByNode<bool> is_reached_;
  std::vector<NodeIndex> reached_nodes_;

  // Priority queue of nodes, ordered by their distance to the source.
  struct NodeDistance {
    NodeIndex node;         // The target node.
    DistanceType distance;  // Its distance from the source.

    bool operator<(const NodeDistance& other) const {
      // PERFORMANCE(user): Here are some versions of the comparison operator:
      // 0) distance != other.distance ? distance < other.distance
      //                               : node < other.node
      // 1) Same, with ABSL_PREDICT_TRUE() around the first test.
      // 2) std::tie(distance, node) < std::tie(other.distance, other.node)
      // 3) __int128 comparison with *reinterpret_cast<const __int128*>(this).
      //    (note: this only works when the node and distance types are integer
      //     or ieee754 floating-point, when the machine is little endian, and
      //     when the total size of NodeDistance equals 16 bytes).
      // And here are the speeds of the BM_GridGraph benchmark (in which
      // DistanceType=int64_t and NodeIndex=int32_t), done with benchy
      // --runs=20: 0) BM_GridGraph<true> 9.22ms ± 5% BM_GridGraph<false> 3.19ms
      // ± 6% 1) BM_GridGraph<true> 8.89ms ± 4%   BM_GridGraph<false> 3.07ms ±
      // 3% 2) BM_GridGraph<true> 8.61ms ± 3%   BM_GridGraph<false> 3.13ms ± 6%
      // 3) BM_GridGraph<true> 7.85ms ± 2%   BM_GridGraph<false> 3.29ms ± 2%
      return std::tie(distance, node) < std::tie(other.distance, other.node);
    }
    bool operator>(const NodeDistance& other) const { return other < *this; }
  };
  std::vector<NodeDistance> queue_;

  // These are used by some of the Run...() variants, and are kept as data
  // members to avoid reallocation upon multiple calls.
  // The vectors are only allocated after they are first used.
  // Between calls, is_destination_ is all false, and the rest is junk.
  std::vector<bool> is_destination_;
  ByNode<int> node_to_source_index_;
  ByNode<int> node_to_destination_index_;
};

// -----------------------------------------------------------------------------
// Implementation.
// -----------------------------------------------------------------------------

template <class GraphType, class DistanceType, class ArcLengthFunctor>
BoundedDijkstraWrapper<GraphType, DistanceType, ArcLengthFunctor>::
    BoundedDijkstraWrapper(const GraphType* graph,
                           const ByArc<DistanceType>* arc_lengths)
    : graph_(graph),
      arc_length_functor_(*arc_lengths),
      arc_lengths_(arc_lengths) {
  CHECK(arc_lengths_ != nullptr);
  CHECK_EQ(ArcIndex(arc_lengths_->size()), graph->num_arcs());
  for (const DistanceType length : *arc_lengths) {
    CHECK_GE(length, 0);
  }
}

template <class GraphType, class DistanceType, class ArcLengthFunctor>
BoundedDijkstraWrapper<GraphType, DistanceType, ArcLengthFunctor>::
    BoundedDijkstraWrapper(const GraphType* graph,
                           ArcLengthFunctor arc_length_functor)
    : graph_(graph),
      arc_length_functor_(std::move(arc_length_functor)),
      arc_lengths_(nullptr) {}

template <class GraphType, class DistanceType, class ArcLengthFunctor>
BoundedDijkstraWrapper<GraphType, DistanceType, ArcLengthFunctor>::
    BoundedDijkstraWrapper(const BoundedDijkstraWrapper& other)
    : graph_(other.graph_),
      arc_length_functor_(other.arc_length_functor_),
      arc_lengths_(other.arc_lengths_) {}

template <class GraphType, class DistanceType, class ArcLengthFunctor>
const std::vector<typename GraphType::NodeIndex>&
BoundedDijkstraWrapper<GraphType, DistanceType, ArcLengthFunctor>::
    RunBoundedDijkstraFromMultipleSources(
        const std::vector<std::pair<NodeIndex, DistanceType>>&
            sources_with_distance_offsets,
        DistanceType distance_limit) {
  return RunBoundedDijkstraWithSettledNodeCallback(
      sources_with_distance_offsets, nullptr, distance_limit);
}

template <class GraphType, class DistanceType, class ArcLengthFunctor>
std::vector<typename GraphType::NodeIndex>
BoundedDijkstraWrapper<GraphType, DistanceType, ArcLengthFunctor>::
    RunBoundedDijkstraFromMultipleSourcesToMultipleDestinations(
        const std::vector<std::pair<NodeIndex, DistanceType>>&
            sources_with_distance_offsets,
        const std::vector<std::pair<NodeIndex, DistanceType>>&
            destinations_with_distance_offsets,
        int num_destinations_to_reach, DistanceType distance_limit) {
  if (destinations_with_distance_offsets.empty()) return {};
  if (num_destinations_to_reach <= 0) return {};

  // Initialize the destinations. We'll adapt the distance limit according to
  // the minimal destination distance offset. This is an optional optimization,
  // to reduce the search space.
  DCHECK_GE(num_destinations_to_reach, 0);
  int num_destinations = 0;
  is_destination_.resize(static_cast<size_t>(graph_->num_nodes()), false);
  node_to_destination_index_.resize(graph_->num_nodes(), -1);
  DistanceType min_destination_distance_offset =
      destinations_with_distance_offsets[0].second;
  for (int i = 0; i < destinations_with_distance_offsets.size(); ++i) {
    const NodeIndex node = destinations_with_distance_offsets[i].first;
    const DistanceType distance = destinations_with_distance_offsets[i].second;
    if (!is_destination_[static_cast<size_t>(node)]) ++num_destinations;
    // Skip useless repetitions.
    if (is_destination_[static_cast<size_t>(node)] &&
        distance >=
            destinations_with_distance_offsets[node_to_destination_index_[node]]
                .second) {
      continue;
    }
    is_destination_[static_cast<size_t>(node)] = true;
    node_to_destination_index_[node] = i;
    min_destination_distance_offset =
        std::min(min_destination_distance_offset, distance);
  }
  distance_limit -= min_destination_distance_offset;
  if (num_destinations_to_reach > num_destinations) {
    num_destinations_to_reach = num_destinations;
  }
  gtl::TopN<NodeDistance, std::less<NodeDistance>> closest_destinations(
      /*limit=*/num_destinations_to_reach);

  std::function<void(NodeIndex, DistanceType, DistanceType*)>
      settled_node_callback =
          [this, num_destinations_to_reach, min_destination_distance_offset,
           &destinations_with_distance_offsets, &closest_destinations](
              NodeIndex settled_node, DistanceType settled_distance,
              DistanceType* distance_limit) {
            if (!is_destination_[static_cast<size_t>(settled_node)]) return;
            const DistanceType distance =
                settled_distance +
                destinations_with_distance_offsets
                    [node_to_destination_index_[settled_node]]
                        .second -
                min_destination_distance_offset;
            if (distance >= *distance_limit) return;
            closest_destinations.push(NodeDistance{settled_node, distance});
            if (closest_destinations.size() == num_destinations_to_reach) {
              const DistanceType new_distance_limit =
                  closest_destinations.peek_bottom().distance;
              DCHECK_LE(new_distance_limit, *distance_limit);
              *distance_limit = new_distance_limit;
            }
          };

  RunBoundedDijkstraWithSettledNodeCallback(
      sources_with_distance_offsets, settled_node_callback, distance_limit);

  // Clean up, sparsely, for the next call.
  for (const auto& [node, _] : destinations_with_distance_offsets) {
    is_destination_[static_cast<size_t>(node)] = false;
  }

  // Return the closest "num_destinations_to_reach" reached destinations,
  // sorted by distance.
  std::vector<NodeIndex> sorted_destinations;
  sorted_destinations.reserve(closest_destinations.size());
  for (const NodeDistance& d : closest_destinations.Take()) {
    sorted_destinations.push_back(d.node);
  }
  return sorted_destinations;
}

template <class GraphType, class DistanceType, class ArcLengthFunctor>
bool BoundedDijkstraWrapper<GraphType, DistanceType, ArcLengthFunctor>::
    OneToOneShortestPath(NodeIndex from, NodeIndex to,
                         DistanceType distance_limit) {
  bool reached = false;
  std::function<void(NodeIndex, DistanceType, DistanceType*)>
      settled_node_callback = [to, &reached](NodeIndex node,
                                             DistanceType distance,
                                             DistanceType* distance_limit) {
        if (node != to) return;
        if (distance >= *distance_limit) return;
        reached = true;
        // Stop the search, by setting the distance limit to 0.
        *distance_limit = 0;
      };
  RunBoundedDijkstraWithSettledNodeCallback({{from, 0}}, settled_node_callback,
                                            distance_limit);
  return reached;
}

template <class GraphType, class DistanceType, class ArcLengthFunctor>
const std::vector<typename GraphType::NodeIndex>&
BoundedDijkstraWrapper<GraphType, DistanceType, ArcLengthFunctor>::
    RunBoundedDijkstraWithSettledNodeCallback(
        const std::vector<std::pair<NodeIndex, DistanceType>>&
            sources_with_distance_offsets,
        std::function<void(NodeIndex settled_node,
                           DistanceType settled_distance,
                           DistanceType* distance_limit)>
            settled_node_callback,
        DistanceType distance_limit) {
  // Sparse clear is_reached_ from the last call.
  for (const NodeIndex node : reached_nodes_) {
    is_reached_[node] = false;
  }
  reached_nodes_.clear();
  DCHECK(!absl::c_linear_search(is_reached_, true));

  is_reached_.resize(graph_->num_nodes(), false);
  distances_.resize(graph_->num_nodes(), distance_limit);
  parents_.resize(graph_->num_nodes(), std::numeric_limits<NodeIndex>::min());
  arc_from_source_.resize(graph_->num_nodes(), GraphType::kNilArc);

  // Initialize sources.
  CHECK(queue_.empty());
  node_to_source_index_.resize(graph_->num_nodes(), -1);
  for (int i = 0; i < sources_with_distance_offsets.size(); ++i) {
    const NodeIndex node = sources_with_distance_offsets[i].first;
    DCHECK_GE(node, NodeIndex(0));
    DCHECK_LT(node, graph_->num_nodes());
    const DistanceType distance = sources_with_distance_offsets[i].second;
    // Sources with an initial distance ≥ limit are *not* reached.
    if (distance >= distance_limit) continue;
    // Skip useless repetitions.
    if (is_reached_[node] && distance >= distances_[node]) continue;
    if (!is_reached_[node]) {
      is_reached_[node] = true;
      reached_nodes_.push_back(node);
      parents_[node] = node;  // Set the parent to itself.
    }
    node_to_source_index_[node] = i;
    distances_[node] = distance;
  }
  for (const NodeIndex source : reached_nodes_) {
    queue_.push_back({source, distances_[source]});
  }
  std::make_heap(queue_.begin(), queue_.end(), std::greater<NodeDistance>());

  // Dijkstra loop.
  while (!queue_.empty()) {
    const NodeDistance top = queue_.front();
    std::pop_heap(queue_.begin(), queue_.end(), std::greater<NodeDistance>());
    queue_.pop_back();

    // The queue may contain the same node more than once, skip irrelevant
    // entries.
    if (distances_[top.node] < top.distance) continue;

    if (settled_node_callback) {
      // We usually never enqueue anything >= distance_limit, but if
      // settled_node_callback is not nullptr, the limit might have been changed
      // after the enqueue were done. So we re-test it here to make sure we
      // never call the callback on such node.
      if (top.distance < distance_limit) {
        settled_node_callback(top.node, top.distance, &distance_limit);
      }

      // If we are over the distance, we can empty the queue and abort.
      if (top.distance >= distance_limit) {
        queue_.clear();
        break;
      }
    } else {
      DCHECK_LT(top.distance, distance_limit);
    }

    // Visit the neighbors.
    const DistanceType limit = distance_limit - top.distance;
    for (const typename GraphType::ArcIndex arc :
         graph_->OutgoingArcs(top.node)) {
      // Overflow-safe check of top.distance + arc_length >= distance_limit.
      // This works since we know top.distance < distance_limit, as long as we
      // don't have negative top.distance (which might happen with negative
      // source offset). Note that for floating point, it is not exactly the
      // same as (top_distance + arc_length < distance_limit) though.
      const DistanceType arc_length = GetArcLength(arc);
      if (arc_length >= limit) continue;
      const DistanceType candidate_distance = top.distance + arc_length;

      const NodeIndex head = graph_->Head(arc);
      if (is_reached_[head]) {
        if (candidate_distance >= distances_[head]) continue;
      } else {
        is_reached_[head] = true;
        reached_nodes_.push_back(head);
      }
      distances_[head] = candidate_distance;
      parents_[head] = top.node;
      arc_from_source_[head] = arc;
      queue_.push_back({head, candidate_distance});
      std::push_heap(queue_.begin(), queue_.end(),
                     std::greater<NodeDistance>());
    }
  }

  return reached_nodes_;
}

template <class GraphType, class DistanceType, class ArcLengthFunctor>
std::vector<typename GraphType::ArcIndex>
BoundedDijkstraWrapper<GraphType, DistanceType, ArcLengthFunctor>::ArcPathTo(
    NodeIndex node) const {
  std::vector<typename GraphType::ArcIndex> output;
  int loop_detector = 0;
  while (true) {
    DCHECK_GE(node, NodeIndex(0));
    DCHECK_LT(node, NodeIndex(parents_.size()));
    CHECK_LT(loop_detector++, parents_.size());
    if (parents_[node] == node) break;
    output.push_back(arc_from_source_[node]);
    node = parents_[node];
  }
  std::reverse(output.begin(), output.end());
  return output;
}

template <class GraphType, class DistanceType, class ArcLengthFunctor>
std::vector<typename GraphType::NodeIndex>
BoundedDijkstraWrapper<GraphType, DistanceType, ArcLengthFunctor>::NodePathTo(
    NodeIndex node) const {
  std::vector<NodeIndex> output;
  int loop_detector = 0;
  while (true) {
    DCHECK_GE(node, NodeIndex(0));
    DCHECK_LT(node, NodeIndex(parents_.size()));
    CHECK_LT(loop_detector++, parents_.size());
    output.push_back(node);
    if (parents_[node] == node) break;
    node = parents_[node];
  }
  std::reverse(output.begin(), output.end());
  return output;
}

template <class GraphType, class DistanceType, class ArcLengthFunctor>
typename GraphType::NodeIndex BoundedDijkstraWrapper<
    GraphType, DistanceType,
    ArcLengthFunctor>::SourceOfShortestPathToNode(NodeIndex node) const {
  NodeIndex parent = node;
  while (parents_[parent] != parent) parent = parents_[parent];
  return parent;
}

template <class GraphType, class DistanceType, class ArcLengthFunctor>
int BoundedDijkstraWrapper<GraphType, DistanceType,
                           ArcLengthFunctor>::GetSourceIndex(NodeIndex node)
    const {
  DCHECK_GE(node, NodeIndex(0));
  DCHECK_LT(node, NodeIndex(node_to_source_index_.size()));
  return node_to_source_index_[node];
}

template <class GraphType, class DistanceType, class ArcLengthFunctor>
int BoundedDijkstraWrapper<GraphType, DistanceType, ArcLengthFunctor>::
    GetDestinationIndex(NodeIndex node) const {
  DCHECK_GE(node, NodeIndex(0));
  DCHECK_LT(node, NodeIndex(node_to_destination_index_.size()));
  return node_to_destination_index_[node];
}

// -----------------------------------------------------------------------------
// Example usage.
// -----------------------------------------------------------------------------

template <typename NodeIndex, typename DistanceType>
std::pair<DistanceType, std::vector<NodeIndex>> SimpleOneToOneShortestPath(
    NodeIndex source, NodeIndex destination, absl::Span<const NodeIndex> tails,
    absl::Span<const NodeIndex> heads, absl::Span<const DistanceType> lengths,
    DistanceType limit) {
  using ArcIndex = NodeIndex;
  // Compute the number of nodes.
  //
  // This is not necessary, but is a good practice to allocate the graph size in
  // one go. We also do some basic validation.
  CHECK_GE(source, 0);
  CHECK_GE(destination, 0);
  NodeIndex num_nodes = std::max(source + 1, destination + 1);
  for (const NodeIndex tail : tails) {
    CHECK_GE(tail, 0);
    num_nodes = std::max(tail + 1, num_nodes);
  }
  for (const NodeIndex head : heads) {
    CHECK_GE(head, 0);
    num_nodes = std::max(head + 1, num_nodes);
  }

  // The number of arcs.
  const ArcIndex num_arcs = tails.size();
  CHECK_EQ(num_arcs, heads.size());
  CHECK_EQ(num_arcs, lengths.size());

  // Build the graph. Note that this permutes arc indices for speed, but we
  // don't care here since we will return a node path.
  util::StaticGraph<NodeIndex, ArcIndex> graph(num_nodes, num_arcs);
  std::vector<DistanceType> arc_lengths(lengths.begin(), lengths.end());
  for (ArcIndex a = 0; a < num_arcs; ++a) {
    // Negative length can cause the algo to loop forever and/or use a lot of
    // memory. So it should be validated.
    CHECK_GE(lengths[a], 0);
    graph.AddArc(tails[a], heads[a]);
  }
  std::vector<int> permutation;
  graph.Build(&permutation);
  util::Permute(permutation, &arc_lengths);

  // Compute a shortest path.
  // This should work for both float/double or integer distances.
  BoundedDijkstraWrapper<util::StaticGraph<>, DistanceType> wrapper(
      &graph, &arc_lengths);
  if (!wrapper.OneToOneShortestPath(source, destination, limit)) {
    // No path exists, or shortest_distance >= limit.
    return {limit, {}};
  }

  // A path exist, returns it.
  return {wrapper.distances()[destination], wrapper.NodePathTo(destination)};
}

}  // namespace operations_research

#endif  // ORTOOLS_GRAPH_BOUNDED_DIJKSTRA_H_
