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

#ifndef OR_TOOLS_GRAPH_BIDIRECTIONAL_DIJKSTRA_H_
#define OR_TOOLS_GRAPH_BIDIRECTIONAL_DIJKSTRA_H_

#include <algorithm>
#include <limits>
#include <queue>
#include <string>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/log/log.h"
#include "absl/log/vlog_is_on.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "ortools/base/iterator_adaptors.h"
#include "ortools/base/logging.h"
#include "ortools/base/threadpool.h"

namespace operations_research {

// Run a bi-directional Dijkstra search, which can be much faster than
// a typical Dijkstra, depending on the structure of the underlying graph.
// It should be at least 2x faster when using 2 threads, but in practice
// it can be much faster.
// Eg. if the graph represents 3D points in the space and the distance is
// the Euclidian distance, the search space grows like the cubic power of
// the search radius, so the bi-directional Dijkstra can be expected to be
// 2^3 = 8 times faster than the standard one.
template <typename GraphType, typename DistanceType>
class BidirectionalDijkstra {
 public:
  typedef typename GraphType::NodeIndex NodeIndex;
  typedef typename GraphType::ArcIndex ArcIndex;

  // IMPORTANT: Both arguments must outlive the class. The arc lengths cannot be
  // negative and the vector must be of the correct size (both preconditions are
  // CHECKed).
  // Two graphs are needed, for the forward and backward searches. Both graphs
  // must have the same number of nodes.
  // If you want to perform the search on a symmetric graph, you can simply
  // provide it twice here, ditto for the arc lengths.
  BidirectionalDijkstra(const GraphType* forward_graph,
                        const std::vector<DistanceType>* forward_arc_lengths,
                        const GraphType* backward_graph,
                        const std::vector<DistanceType>* backward_arc_lengths);

  // Represents a node with a distance (typically from one end of the search,
  // either the source or the destination).
  struct NodeDistance {
    NodeIndex node;
    DistanceType distance;
    // We inverse the < operator to easily use this node within priority queues
    // where the closest node comes first.
    bool operator<(const NodeDistance& other) const {
      return distance > other.distance;
    }
    std::string DebugString() const {
      return absl::StrCat(node, ", d=", (distance));
    }
  };

  // Represents a bidirectional path. See SetToSetShortestPath() to understand
  // why this data structure is like this.
  struct Path {
    // The node where the two half-paths meet. -1 if no path exists.
    NodeIndex meeting_point;

    // The forward arc path from a source to "meeting_point". Might be empty
    // if no path is found, or if "meeting_point" is a source (the reverse
    // implication doesn't work: even if meeting_point is a source Sa, there
    // might be another source Sb != Sa such that the path [Sb....Sa] is shorter
    // than [Sa], because of the initial distances).
    std::vector<ArcIndex> forward_arc_path;

    // Ditto, but those are arcs in the backwards graph, from a destination to
    // the meeting point.
    std::vector<ArcIndex> backward_arc_path;
  };

  // Returns a very nice debug string of the bidirectional path, eg:
  // 0 --(#4:3.2)--> 1 --(#2:1.3)--> [5] <--(#8:5.6)-- 9 <--(#0:1.3)-- 3
  // where the text in () is an arc's index followed by its length.
  // Returns "<NO PATH>" for empty paths.
  std::string PathDebugString(const Path& path) const;

  // Converts the rich 'Path' structure into a simple node path, where
  // the nodes go from the source to the destination (i.e. the backward
  // path is reversed).
  std::vector<NodeIndex> PathToNodePath(const Path& path) const;

  // Finds the shortest path between two sets of nodes with costs, and returns
  // a description of it as two half-paths of arcs (one in the forward graph,
  // one in the backward graph) meeting at a "meeting point" node.
  //
  // When choosing the shortest path, the source and destination
  // "initial distances" are taken into account: the overall path length is
  // the sum of those and of the arc lengths. Note that this supports negative
  // initial distances, as opposed to arc lengths which must be non-negative.
  //
  // Corner case: if a node is present several times in "sources" or in
  // "destinations", only the entry with the smallest distance is taken into
  // account.
  Path SetToSetShortestPath(const std::vector<NodeDistance>& sources,
                            const std::vector<NodeDistance>& destinations);

  // Shortcut for the common case when there is a single source and a single
  // destination: in that case, source and destination cost don't matter.
  Path OneToOneShortestPath(NodeIndex from, NodeIndex to) {
    return SetToSetShortestPath({{from, 0.0}}, {{to, 0.0}});
  }

 private:
  enum Direction {
    FORWARD = 0,
    BACKWARD = 1,
  };

  inline static Direction Reverse(Direction d) {
    return d == FORWARD ? BACKWARD : FORWARD;
  }

  inline static DistanceType infinity() {
    return std::numeric_limits<DistanceType>::infinity();
  }

  template <Direction dir>
  void PerformHalfSearch(absl::Notification* search_has_ended);

  // Input forward/backward graphs with their arc lengths.
  const GraphType* graph_[2];
  const std::vector<DistanceType>* arc_lengths_[2];

  // Priority queue of nodes, ordered by their distance to the source.
  std::priority_queue<NodeDistance> queue_[2];

  std::vector<bool> is_source_[2];
  std::vector<bool> is_reached_[2];
  // NOTE(user): is_settled is functionally vector<bool>, but we use
  // vector<char> because the locking that it's involved in
  // (via the per-node mutex, see below) works on entire memory bytes.
  std::vector<char> is_settled_[2];

  std::vector<DistanceType> distances_[2];
  std::vector<ArcIndex> parent_arc_[2];
  std::vector<NodeIndex> reached_nodes_[2];

  // The per-node information shared by the two half searches are
  // "is_settled_" and "distances_". Each direction exclusively writes in its
  // own data, and reads the other direction's data.
  // To avoid too much contention, we use a vector of one mutex per node.
  //
  // NOTE(user): There was no visible performance gain when using
  // vector<bool> and, for example, one mutex for each group of 8 nodes
  // spanning a byte (or for 64 nodes spanning 8 bytes).
  //
  // NOTE(user): There are arguments to simply remove the node Mutexes and
  // the corresponding locks: the measured performance gain was 20%-30%, and
  // the randomized correctness tests were passing (but TSAN was complaining).
  // To be investigated if/when needed.
  std::vector<absl::Mutex> node_mutex_;

  absl::Mutex search_mutex_;
  NodeDistance best_meeting_point_ ABSL_GUARDED_BY(search_mutex_);
  DistanceType current_search_radius_[2] ABSL_GUARDED_BY(search_mutex_);

  ThreadPool search_threads_;
};

template <typename GraphType, typename DistanceType>
BidirectionalDijkstra<GraphType, DistanceType>::BidirectionalDijkstra(
    const GraphType* forward_graph,
    const std::vector<DistanceType>* forward_arc_lengths,
    const GraphType* backward_graph,
    const std::vector<DistanceType>* backward_arc_lengths)
    : node_mutex_(forward_graph->num_nodes()), search_threads_(2) {
  CHECK_EQ(forward_graph->num_nodes(), backward_graph->num_nodes());
  const int num_nodes = forward_graph->num_nodes();
  // Quickly verify that the arc lengths are non-negative.
  for (int i = 0; i < num_nodes; ++i) {
    CHECK_GE((*forward_arc_lengths)[i], 0) << i;
    CHECK_GE((*backward_arc_lengths)[i], 0) << i;
  }
  graph_[FORWARD] = forward_graph;
  graph_[BACKWARD] = backward_graph;
  arc_lengths_[FORWARD] = forward_arc_lengths;
  arc_lengths_[BACKWARD] = backward_arc_lengths;
  for (const Direction dir : {FORWARD, BACKWARD}) {
    current_search_radius_[dir] = -infinity();
    is_source_[dir].assign(num_nodes, false);
    is_reached_[dir].assign(num_nodes, false);
    is_settled_[dir].assign(num_nodes, false);
    distances_[dir].assign(num_nodes, infinity());
    parent_arc_[dir].assign(num_nodes, -1);
  }
  search_threads_.StartWorkers();
}

template <typename GraphType, typename DistanceType>
std::string BidirectionalDijkstra<GraphType, DistanceType>::PathDebugString(
    const Path& path) const {
  if (path.meeting_point == -1) return "<NO PATH>";
  std::string out;
  for (const int arc : path.forward_arc_path) {
    absl::StrAppend(&out, graph_[FORWARD]->Tail(arc), " --(#", arc, ":",
                    ((*arc_lengths_[FORWARD])[arc]), ")--> ");
  }
  absl::StrAppend(&out, "[", path.meeting_point, "]");
  for (const int arc : gtl::reversed_view(path.backward_arc_path)) {
    absl::StrAppend(&out, " <--(#", arc, ":", ((*arc_lengths_[BACKWARD])[arc]),
                    ")-- ", graph_[BACKWARD]->Tail(arc));
  }
  return out;
}

template <typename GraphType, typename DistanceType>
std::vector<typename GraphType::NodeIndex>
BidirectionalDijkstra<GraphType, DistanceType>::PathToNodePath(
    const typename BidirectionalDijkstra<GraphType, DistanceType>::Path& path)
    const {
  if (path.meeting_point == -1) return {};
  std::vector<int> nodes;
  for (const int arc : path.forward_arc_path) {
    nodes.push_back(graph_[FORWARD]->Tail(arc));
  }
  nodes.push_back(path.meeting_point);
  for (const int arc : gtl::reversed_view(path.backward_arc_path)) {
    nodes.push_back(graph_[BACKWARD]->Tail(arc));
  }
  return nodes;
}

template <typename GraphType, typename DistanceType>
typename BidirectionalDijkstra<GraphType, DistanceType>::Path
BidirectionalDijkstra<GraphType, DistanceType>::SetToSetShortestPath(
    const std::vector<NodeDistance>& sources,
    const std::vector<NodeDistance>& destinations)
    // Disable thread safety analysis in this function, because there's no
    // multi-threading within its body, per se: the multi-threading work
    // is solely within PerformHalfSearch().
    ABSL_NO_THREAD_SAFETY_ANALYSIS {
  if (VLOG_IS_ON(2)) {
    VLOG(2) << "Starting search with " << sources.size() << " sources and "
            << destinations.size() << " destinations. Sources:";
    for (const NodeDistance& src : sources) VLOG(2) << src.DebugString();
    VLOG(2) << "Destinations:";
    for (const NodeDistance& dst : destinations) VLOG(2) << dst.DebugString();
  }
  if (sources.empty() || destinations.empty()) return {-1, {}, {}};
  // Initialize the fields that must be ready before both searches start.
  for (const Direction dir : {FORWARD, BACKWARD}) {
    const std::vector<NodeDistance>& srcs =
        dir == FORWARD ? sources : destinations;
    CHECK(queue_[dir].empty());
    QCHECK_EQ(reached_nodes_[dir].size(), 0);
    if (DEBUG_MODE) {
      for (bool b : is_reached_[dir]) QCHECK(!b);
      for (bool b : is_settled_[dir]) QCHECK(!b);
    }
    for (const NodeDistance& src : srcs) {
      CHECK_GE(src.node, 0);
      CHECK_LT(src.node, graph_[dir]->num_nodes());
      is_source_[dir][src.node] = true;
      if (!is_reached_[dir][src.node]) {
        is_reached_[dir][src.node] = true;
        reached_nodes_[dir].push_back(src.node);
        parent_arc_[dir][src.node] = -1;
      } else if (src.distance >= distances_[dir][src.node]) {
        continue;
      }
      // If we're here, we have a new best distance for the current source.
      // We also need to re-push it in the queue, since the distance changed.
      distances_[dir][src.node] = src.distance;
      queue_[dir].push(src);
    }
  }

  // Start the Dijkstras!
  best_meeting_point_ = {-1, infinity()};
  absl::Notification search_has_ended[2];
  search_threads_.Schedule([this, &search_has_ended, &sources]() {
    PerformHalfSearch<FORWARD>(&search_has_ended[FORWARD]);
  });
  search_threads_.Schedule([this, &search_has_ended, &destinations]() {
    PerformHalfSearch<BACKWARD>(&search_has_ended[BACKWARD]);
  });

  // Wait for the two searches to finish.
  search_has_ended[FORWARD].WaitForNotification();
  search_has_ended[BACKWARD].WaitForNotification();

  // Clean up the rest of the search, sparsely. "is_settled" can't be cleaned
  // in PerformHalfSearch() because it is needed by the other half-search
  // (which might be still ongoing when the first half-search finishes), so
  // we have to do it when both searches have ended.
  // So we also clean the auxiliary field "reached_nodes" and the sibling field
  // "is_reached" here too.
  // Ditto for "is_source".
  for (const Direction dir : {FORWARD, BACKWARD}) {
    current_search_radius_[dir] = -infinity();
    for (const int node : reached_nodes_[dir]) {
      is_reached_[dir][node] = false;
      is_settled_[dir][node] = false;
    }
    reached_nodes_[dir].clear();
  }
  for (const NodeDistance& src : sources) {
    is_source_[FORWARD][src.node] = false;
  }
  for (const NodeDistance& dst : destinations) {
    is_source_[BACKWARD][dst.node] = false;
  }

  // Extract the shortest path from the meeting point.
  Path path = {best_meeting_point_.node, {}, {}};
  if (path.meeting_point == -1) return path;  // No path.

  for (const Direction dir : {FORWARD, BACKWARD}) {
    int node = path.meeting_point;
    std::vector<int>* arc_path =
        dir == FORWARD ? &path.forward_arc_path : &path.backward_arc_path;
    while (true) {
      const int arc = parent_arc_[dir][node];
      if (arc < 0) break;
      arc_path->push_back(arc);
      node = graph_[dir]->Tail(arc);
    }
    std::reverse(arc_path->begin(), arc_path->end());
  }
  return path;
}

template <typename GraphType, typename DistanceType>
template <
    typename BidirectionalDijkstra<GraphType, DistanceType>::Direction dir>
void BidirectionalDijkstra<GraphType, DistanceType>::PerformHalfSearch(
    absl::Notification* search_has_ended) {
  while (!queue_[dir].empty()) {
    const NodeDistance top = queue_[dir].top();
    queue_[dir].pop();

    // The queue may contain the same node more than once, skip irrelevant
    // entries.
    if (is_settled_[dir][top.node]) continue;
    DVLOG(2) << (dir ? "BACKWARD" : "FORWARD") << ": Popped "
             << top.DebugString();

    // Mark the node as settled. Since the "is_settled" might be read by the
    // other search thread when updating the same node, we use a Mutex on that
    // node.
    {
      node_mutex_[top.node].Lock();
      is_settled_[dir][top.node] = true;  // It's important to do this early.
      // Most meeting points are caught by the logic below (in the arc
      // relaxation loop), but not the meeting points that are on the sources
      // or destinations. So we need this special case here.
      if (is_source_[Reverse(dir)][top.node]) {
        const DistanceType meeting_distance =
            top.distance + distances_[Reverse(dir)][top.node];
        // Release the node mutex, now that we can, to prevent deadlocks when
        // we try acquiring the global search mutex.
        node_mutex_[top.node].Unlock();
        absl::MutexLock search_lock(&search_mutex_);
        if (meeting_distance < best_meeting_point_.distance) {
          best_meeting_point_ = {top.node, meeting_distance};
          DVLOG(2) << (dir ? "BACKWARD" : "FORWARD")
                   << ": New best: " << best_meeting_point_.DebugString();
        }
      } else {
        node_mutex_[top.node].Unlock();
      }
    }

    // Update the current search radius in this direction, and see whether we
    // should stop the search, based on the other radius.
    DistanceType potentially_interesting_distance_upper_bound;
    {
      absl::MutexLock lock(&search_mutex_);
      current_search_radius_[dir] = top.distance;
      potentially_interesting_distance_upper_bound =
          best_meeting_point_.distance - current_search_radius_[Reverse(dir)];
    }
    if (top.distance >= potentially_interesting_distance_upper_bound) {
      DVLOG(2) << (dir ? "BACKWARD" : "FORWARD") << ": Stopping.";
      break;
    }

    // Visit the neighbors.
    for (const int arc : graph_[dir]->OutgoingArcs(top.node)) {
      const DistanceType candidate_distance =
          top.distance + (*arc_lengths_[dir])[arc];
      const int head = graph_[dir]->Head(arc);
      if (!is_reached_[dir][head] ||
          candidate_distance < distances_[dir][head]) {
        DVLOG(2) << (dir ? "BACKWARD" : "FORWARD") << ": Pushing: "
                 << NodeDistance({head, candidate_distance}).DebugString();
        if (!is_reached_[dir][head]) {
          is_reached_[dir][head] = true;
          reached_nodes_[dir].push_back(head);
        }
        parent_arc_[dir][head] = arc;

        // SUBTLE: A simple performance optimization that speeds up the search
        // (especially towards the end) is to avoid enqueuing nodes that can't
        // possibly improve the current best meeting point.
        // We still need to process them normally, though, including the
        // meeting point logic below.
        // TODO(user): Explain why.
        if (candidate_distance < potentially_interesting_distance_upper_bound) {
          queue_[dir].push({head, candidate_distance});
        }

        // Update the node distance and check for meeting points with the
        // protection of a Mutex.
        DistanceType meeting_distance = infinity();
        {
          absl::MutexLock node_lock(&node_mutex_[head]);
          distances_[dir][head] = candidate_distance;
          // Did we reach a meeting point?
          if (is_settled_[Reverse(dir)][head]) {
            meeting_distance =
                candidate_distance + distances_[Reverse(dir)][head];
            DVLOG(2) << (dir ? "BACKWARD" : "FORWARD")
                     << ": Found meeting point!";
          }
        }
        // Process the meeting point with the protection of the global search
        // Mutex -- this is fine performance-wise because it happens rarely.
        // To avoid deadlocks, we make sure that 'node_mutex' is no longer held.
        if (meeting_distance != infinity()) {
          absl::MutexLock search_lock(&search_mutex_);
          if (meeting_distance < best_meeting_point_.distance) {
            best_meeting_point_ = {head, meeting_distance};
            DVLOG(2) << (dir ? "BACKWARD" : "FORWARD")
                     << ": New best: " << best_meeting_point_.DebugString();
          }
        }
      }
    }
  }
  DVLOG(2) << (dir ? "BACKWARD" : "FORWARD") << ": Done. Cleaning up...";

  // Empty the queue.
  while (!queue_[dir].empty()) queue_[dir].pop();

  // We're done. Notify!
  search_has_ended->Notify();
}

}  // namespace operations_research
#endif  // OR_TOOLS_GRAPH_BIDIRECTIONAL_DIJKSTRA_H_
