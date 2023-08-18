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

// Runs multiple Dijkstra searches simultaneously (single-threaded, but growing
// their search radii at the same time) on the same graph.
// Supports custom arc length functors, and custom stopping criterion and
// tracking.
//
// EXAMPLE:
// With two sources and a custom stopping criterion that stops the first
// Dijkstra when it has settled 1000 nodes and the second when it has reached
// the search radius 123.45:
//
//  ListGraph<> graph;  // From ortools/graph/graph.h
//  ... build the graph ...
//  int source1 = ... , source2 = ...;
//  vector<double> arc_lengths(graph.num_arcs(), 0);
//  ... set the arc lengths ...
//  int num_nodes_to_settle_in_first_search = 1000;
//  std::vector<absl::flat_hash_map<int, DistanceAndParentArc<double>>>
//      reached_nodes;  // One map per source: node -> DistanceAndParentArc.
//  reached_nodes = MultiDijkstra<double>(
//      graph,
//      [&arc_lengths](int a) { return arc_lengths[a]; },
//      {{source1}, {source2}},
//      [&num_nodes_to_settle_in_first_search](
//          int settled_node, int source_index, double settled_distance) {
//            if (source_index == 0) {
//              return --num_nodes_to_settle_in_first_search == 0;
//            } else {
//              return settled_distance >= 123.45;
//            }
//          });

#ifndef OR_TOOLS_GRAPH_MULTI_DIJKSTRA_H_
#define OR_TOOLS_GRAPH_MULTI_DIJKSTRA_H_

#include <iostream>
#include <ostream>
#include <queue>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "ortools/base/map_util.h"
#include "ortools/base/types.h"

namespace operations_research {

template <class DistanceType>
struct DistanceAndParentArc {
  DistanceType distance;
  int parent_arc;  // -1 means "no parent" (i.e. we're at the root).
  bool operator==(const DistanceAndParentArc& other) const {
    return distance == other.distance && parent_arc == other.parent_arc;
  }
};

template <class DistanceType>
std::ostream& operator<<(
    std::ostream& out,
    DistanceAndParentArc<DistanceType> distance_and_parent_arc);

// Runs multiple Dijkstra searches simultaneously on the same graph, in a single
// thread. All the Dijkstras share the same priority queue: their search radius
// will grow "simulatenously".
//
// Moreover, each individual Dijkstra search can have several nodes as its
// "source", and the stopping criterion for each Dijkstra search is highly
// customizable: the user controls it via a "settled node callback",
// called every time a node is settled. See the API below.
//
// The Dijkstra are sparse: the global space complexity will be linear in the
// number of search states explored. Ditto for the time complexity, with an
// additional logarithmic factor caused by the priority queue.
//
// This class has many similarities with BoundedDijkstraWrapper from
// ./bounded_dijkstra.h but adds the overhead of tracking the source index for
// every search state, and of the sparse (but slower) node hash maps.
//
// Arguments:
// - "graph": the graph. The Graph class must support the interface of
//   ortools/graph/graph.h: OutgoingArcs(), Head(), Tail().
// - "arc_length_functor": we will call arc_length_functor(a) on every
//   arc "a" explored, where "a" is cast to an int. It should return the arc's
//   length, which is statically cast to DistanceType.
// - "source_sets" contains the sources. Each source is itself a set of nodes.
// - "settled_node_callback" will be called every time we settled a node, with 3
//   arguments (node, source index, distance of node from that source).
//   If it returns true, the Dijkstra search from that source will stop.
//
// Returns the list of Dijkstra search: for each source #s, the element #s of
// the returned vector will map every node explored in the Dijkstra from source
// #s to its distance and parent arc.
template <class DistanceType, class Graph, class ArcLengthFunctor,
          class SettledNodeCallbackType>
std::vector<absl::flat_hash_map<int, DistanceAndParentArc<DistanceType>>>
MultiDijkstra(const Graph& graph, ArcLengthFunctor arc_length_functor,
              const std::vector<std::vector<int>>& source_sets,
              SettledNodeCallbackType settled_node_callback);

// =============================================================================
// Implementation of the function templates
// =============================================================================
template <class DistanceType>
std::ostream& operator<<(
    std::ostream& out,
    DistanceAndParentArc<DistanceType> distance_and_parent_arc) {
  return out << "{distance=" << distance_and_parent_arc.distance
             << ", parent_arc=" << distance_and_parent_arc.parent_arc << "}";
}

template <class DistanceType, class Graph, class ArcLengthFunctor,
          class SettledNodeCallbackType>
std::vector<absl::flat_hash_map<int, DistanceAndParentArc<DistanceType>>>
MultiDijkstra(const Graph& graph, ArcLengthFunctor arc_length_functor,
              const std::vector<std::vector<int>>& source_sets,
              SettledNodeCallbackType settled_node_callback) {
  // Initialize the return data structure and the priority queue.
  const int num_sources = source_sets.size();
  std::vector<absl::flat_hash_map<int, DistanceAndParentArc<DistanceType>>>
      reached_from(num_sources);  // Also the returned output!
  struct State {
    DistanceType distance;
    int node;
    int source_index;
  };
  struct StateComparator {
    bool operator()(const State& s0, const State& s1) const {
      if (s0.distance != s1.distance) return s0.distance > s1.distance;
      if (s0.node != s1.node) return s0.node > s1.node;
      return s0.source_index > s1.source_index;
    }
  };
  StateComparator pq_state_comparator;
  std::priority_queue<State, std::vector<State>, StateComparator> pq(
      pq_state_comparator);
  std::vector<bool> dijkstra_is_active(num_sources, false);
  int num_active_dijkstras = 0;
  for (int source_index = 0; source_index < num_sources; ++source_index) {
    if (!source_sets[source_index].empty()) {
      dijkstra_is_active[source_index] = true;
      ++num_active_dijkstras;
    }
    for (const int node : source_sets[source_index]) {
      if (gtl::InsertIfNotPresent(
              &reached_from[source_index],
              {node, {/*distance=*/0, /*parent_arc=*/-1}})) {
        pq.push({/*distance=*/0, /*node=*/node, /*source_index=*/source_index});
      }
    }
  }

  // Main Dijkstra loop.
  while (!pq.empty() && num_active_dijkstras > 0) {
    const State state = pq.top();
    pq.pop();

    if (!dijkstra_is_active[state.source_index]) continue;

    // Dijkstra optimization: ignore states that don't correspond to the optimal
    // (such states have been preceded by better states in the queue order,
    // without being deleted since priority_queue doesn't have erase()).
    if (gtl::FindOrDie(reached_from[state.source_index], state.node).distance <
        state.distance) {
      continue;
    }

    if (settled_node_callback(state.node, state.source_index, state.distance)) {
      dijkstra_is_active[state.source_index] = false;
      --num_active_dijkstras;
      continue;
    }

    for (const int arc : graph.OutgoingArcs(state.node)) {
      const DistanceType distance = arc_length_functor(arc) + state.distance;
      const int head_node = graph.Head(arc);
      auto insertion_result = reached_from[state.source_index].insert(
          {head_node, {/*distance=*/distance, /*parent_arc=*/arc}});
      if (!insertion_result.second) {  // Already reached...
        if (insertion_result.first->second.distance <= distance) continue;
        insertion_result.first->second = {/*distance=*/distance,
                                          /*parent_arc=*/arc};
      }
      pq.push(State{/*distance=*/distance, /*node=*/head_node,
                    /*source_index=*/state.source_index});
    }
  }
  return reached_from;
}

}  // namespace operations_research

#endif  // OR_TOOLS_GRAPH_MULTI_DIJKSTRA_H_
