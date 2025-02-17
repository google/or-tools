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

// Algorithms to compute k-shortest paths. Currently, only Yen's algorithm is
// implemented.
//
// TODO(user): implement Lawler's modification:
// https://pubsonline.informs.org/doi/abs/10.1287/mnsc.18.7.401
//
// | Algo. | Neg. weights | Neg.-weight loops | Graph type   | Loopless paths |
// |-------|--------------|-------------------|--------------|----------------|
// | Yen   | No           | No                | (Un)directed | Yes            |
//
//
// Loopless path: path not going through the same node more than once. Also
// called simple path.
//
//
// Design choices
// ==============
//
// The design takes some inspiration from `shortest_paths.h` and
// `bounded_dijkstra.h`, but the shortest-path and k-shortest-path problems have
// vastly different structures.
// For instance, a path container that only stores distances, like
// `DistanceContainer` in `shortest_paths.h`, is irrelevant as an output for
// this problem: it can only characterize one path, the shortest one.
// This is why the results are stored in an intermediate structure, containing
// the paths (as a sequence of nodes, just like `PathContainerImpl` subclasses)
// and their distance.
//
// Only the one-to-one k-shortest-path problem is well-defined. Variants with
// multiple sources and/or destinations pose representational challenges whose
// solution is likely to be algorithm-dependent.
// Optimizations of path storage such as `PathTree` are not general enough to
// store k shortest paths: the set of paths for a given index for many
// sources/destinations is not ensured to form a set for each index. (While the
// first paths will form such a tree, storing *different* second paths for each
// source-destination pair may be impossible to do in a tree.)
//
// Unlike the functions in `shortest_paths.h`, the functions in this file
// directly return their result, to follow the current best practices.

#ifndef OR_TOOLS_GRAPH_K_SHORTEST_PATHS_H_
#define OR_TOOLS_GRAPH_K_SHORTEST_PATHS_H_

#include <algorithm>
#include <iterator>
#include <limits>
#include <queue>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/base/optimization.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/strings/str_join.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/graph/bounded_dijkstra.h"
#include "ortools/graph/shortest_paths.h"

namespace operations_research {

// Stores the solution to a k-shortest path problem. `paths` contains up to `k`
// paths from `source` to `destination` (these nodes are arguments to the
// algorithm), each having a distance stored in `distances`.
//
// The paths in `paths` start with `origin` and end at `destination`.
//
// If the computations are unsuccessful for any reason, the vectors are empty.
template <class GraphType>
struct KShortestPaths {
  // The paths are stored as vectors of nodes, like the other graph algorithms.
  // TODO(user): what about vectors of arcs? That might be faster
  // (potentially, add a function to transform it into a vector of nodes if the
  // user really needs it). It would also have the nice benefit of removing the
  // need for `distances` (compute it on the fly), with a reference to the graph
  // and the costs.
  std::vector<std::vector<typename GraphType::NodeIndex>> paths;
  std::vector<PathDistance> distances;
};

// Computes up to k shortest paths from the node `source` to the node
// `destination` in the given directed `graph`. The paths are guaranteed not to
// have loops.
//
// Hypotheses on input (which are not checked at runtime):
// - No multigraphs (more than one edge or a pair of nodes). The behavior is
//   undefined otherwise.
// - The `arc_lengths` are supposed to be nonnegative. The behavior is
//   undefined otherwise.
//   TODO(user): relax to "no negative-weight cycles" (no Dijkstra).
// - The graphs might have loops.
//
// This function uses Yen's algorithm, which guarantees to find the first k
// shortest paths in O(k n (m + n log n)) for n nodes and m edges. This
// algorithm is an implementation of the idea of detours.
//
// Yen, Jin Y. "Finding the k Shortest Loopless Paths in a Network". Management
// Science. 17 (11): 712–716, 1971.
// https://doi.org/10.1287%2Fmnsc.17.11.712
template <class GraphType>
KShortestPaths<GraphType> YenKShortestPaths(
    const GraphType& graph, const std::vector<PathDistance>& arc_lengths,
    typename GraphType::NodeIndex source,
    typename GraphType::NodeIndex destination, unsigned k);

// End of the interface. Below is the implementation.

// TODO(user): introduce an enum to choose the algorithm. It's useless as
// long as this file only provides Yen.

namespace internal {

const PathDistance kMaxDistance = std::numeric_limits<PathDistance>::max() - 1;
const PathDistance kDisconnectedDistance =
    std::numeric_limits<PathDistance>::max();

// Determines the arc index from a source to a destination.
//
// This operation requires iterating through the set of outgoing arcs from the
// source node, which might be expensive.
//
// In a multigraph, this function returns an index for one of the edges between
// the source and the destination.
template <class GraphType>
typename GraphType::ArcIndex FindArcIndex(
    const GraphType& graph, const typename GraphType::NodeIndex source,
    const typename GraphType::NodeIndex destination) {
  const auto outgoing_arcs_iter = graph.OutgoingArcs(source);
  const auto arc = std::find_if(
      outgoing_arcs_iter.begin(), outgoing_arcs_iter.end(),
      [&graph, destination](const typename GraphType::ArcIndex arc) {
        return graph.Head(arc) == destination;
      });
  return (arc != outgoing_arcs_iter.end()) ? *arc : GraphType::kNilArc;
}

// Determines the shortest path from the given source and destination, returns a
// tuple with the path (as a vector of node indices) and its cost.
template <class GraphType>
std::tuple<std::vector<typename GraphType::NodeIndex>, PathDistance>
ComputeShortestPath(const GraphType& graph,
                    const std::vector<PathDistance>& arc_lengths,
                    const typename GraphType::NodeIndex source,
                    const typename GraphType::NodeIndex destination) {
  BoundedDijkstraWrapper<GraphType, PathDistance> dijkstra(&graph,
                                                           &arc_lengths);
  dijkstra.RunBoundedDijkstra(source, kMaxDistance);
  const PathDistance path_length = dijkstra.distances()[destination];

  if (path_length >= kMaxDistance) {
    // There are shortest paths in this graph, just not from the source to this
    // destination.
    // This case only happens when some arcs have an infinite length (i.e.
    // larger than `kMaxDistance`): `BoundedDijkstraWrapper::NodePathTo` fails
    // to return a path, even empty.
    return {std::vector<typename GraphType::NodeIndex>{},
            kDisconnectedDistance};
  }

  if (std::vector<typename GraphType::NodeIndex> path =
          std::move(dijkstra.NodePathTo(destination));
      !path.empty()) {
    return {std::move(path), path_length};
  } else {
    return {std::vector<typename GraphType::NodeIndex>{},
            kDisconnectedDistance};
  }
}

// Computes the total length of a path.
template <class GraphType>
PathDistance ComputePathLength(
    const GraphType& graph, const absl::Span<const PathDistance> arc_lengths,
    const absl::Span<const typename GraphType::NodeIndex> path) {
  PathDistance distance = 0;
  for (typename GraphType::NodeIndex i = 0; i < path.size() - 1; ++i) {
    const typename GraphType::ArcIndex arc =
        internal::FindArcIndex(graph, path[i], path[i + 1]);
    DCHECK_NE(arc, GraphType::kNilArc);
    distance += arc_lengths[arc];
  }
  return distance;
}

// Stores a path with a priority (typically, the distance), with a comparison
// operator that operates on the priority.
template <class GraphType>
class PathWithPriority {
 public:
  using NodeIndex = typename GraphType::NodeIndex;

  PathWithPriority(PathDistance priority, std::vector<NodeIndex> path)
      : path_(std::move(path)), priority_(priority) {}
  bool operator<(const PathWithPriority& other) const {
    return priority_ < other.priority_;
  }

  [[nodiscard]] const std::vector<NodeIndex>& path() const { return path_; }
  [[nodiscard]] PathDistance priority() const { return priority_; }

 private:
  std::vector<NodeIndex> path_;
  PathDistance priority_;
};

// Container adapter to be used with STL container adapters such as
// std::priority_queue. It gives access to the underlying container, which is a
// protected member in a standard STL container adapter.
template <class Container>
class UnderlyingContainerAdapter : public Container {
 public:
  typedef typename Container::container_type container_type;
  // No mutable version of `container`, so that the user cannot change the data
  // within the container: they might destroy the container's invariants.
  [[nodiscard]] const container_type& container() const { return this->c; }
};

}  // namespace internal

// TODO(user): Yen's algorithm can work with negative weights, but
// Dijkstra cannot.
//
// Yen, Jin Y. "Finding the k Shortest Loopless Paths in a Network". Management
// Science. 17 (11): 712–716, 1971.
// https://doi.org/10.1287%2Fmnsc.17.11.712
//
// Yen's notations:
// - Source node: (1).
// - Destination node: (N).
// - Path from (1) to (j): (1) - (i) - ... - (j).
// - Cost for following the arc from (i) to (j), potentially negative: d_ij.
// - k-th shortest path: A^k == (1) - (2^k) - (3^k) - ... - (Q_k^k) - (N).
// - Deviation from A^k-1 at (i): A_i^k. This is the shortest path from (1) to
//   (N) that is identical to A^k-1 from (1) to (i^k-1), then different from all
//   the first k-1 shortest paths {A^1, A^2, ..., A^k-1}.
// - Root of A_i^k: R_i^k. This is the first subpath of A_i^k that coincides
//   with A^k-1, i.e. A_i^k until i^k-1.
// - Spur of A_i^k: S_i^k. This is the last subpart of A_i^k with only one node
//   coinciding with A_i^k, (i^k-1), i.e. A_i^k from i^k-1 onwards.
//
// Example graph, paths from A to H (more classical notations):
//       C - D
//      /   / \
// A - B   /  G - H
//      \ /   /
//       E - F
// Source node: A. Destination node: H.
// Three paths from A to H, say they are ordered from the cheapest to the most
// expensive:
// - 1st path: A - B - C - D - G - H
// - 2nd path: A - B - E - F - G - H
// - 3rd path: A - B - E - D - G - H
// To start with, Yen's algorithm uses the shortest path:
//   A^1 = A - B - C - D - G - H
// To compute the second path A^2, compute a detour around A^1. Consider the
// iteration where B is the spur node.
// - Spur node:     2^1 = B.
// - Root of A^1_2: R_1^2 = A - B (including the spur node 2^1 = B).
// - Spur path S_1^2 starts at the spur node 2^1 = B. There are two possible
//   spur paths, the cheapest being:
//     S_1^2 = B - E - F - G - H
template <class GraphType>
KShortestPaths<GraphType> YenKShortestPaths(
    const GraphType& graph, const std::vector<PathDistance>& arc_lengths,
    typename GraphType::NodeIndex source,
    typename GraphType::NodeIndex destination, unsigned k) {
  using NodeIndex = typename GraphType::NodeIndex;

  CHECK_GT(internal::kDisconnectedDistance, internal::kMaxDistance);

  CHECK_GE(k, 0) << "k must be nonnegative. Input value: " << k;
  CHECK_NE(k, 0) << "k cannot be zero: you are requesting zero paths!";

  CHECK_GT(graph.num_nodes(), 0) << "The graph is empty: it has no nodes";
  CHECK_GT(graph.num_arcs(), 0) << "The graph is empty: it has no arcs";

  CHECK_GE(source, 0) << "The source node must be nonnegative. Input value: "
                      << source;
  CHECK_LT(source, graph.num_nodes())
      << "The source node must be a valid node. Input value: " << source
      << ". Number of nodes in the input graph: " << graph.num_nodes();
  CHECK_GE(destination, 0)
      << "The source node must be nonnegative. Input value: " << destination;
  CHECK_LT(destination, graph.num_nodes())
      << "The destination node must be a valid node. Input value: "
      << destination
      << ". Number of nodes in the input graph: " << graph.num_nodes();

  KShortestPaths<GraphType> paths;

  // First step: compute the shortest path.
  {
    std::tuple<std::vector<NodeIndex>, PathDistance> first_path =
        internal::ComputeShortestPath(graph, arc_lengths, source, destination);
    if (std::get<0>(first_path).empty()) return paths;
    paths.paths.push_back(std::move(std::get<0>(first_path)));
    paths.distances.push_back(std::get<1>(first_path));
  }

  if (k == 1) {
    return paths;
  }

  // Generate variant paths.
  internal::UnderlyingContainerAdapter<
      std::priority_queue<internal::PathWithPriority<GraphType>>>
      variant_path_queue;

  // One path has already been generated (the shortest one). Only k-1 more
  // paths need to be generated.
  for (; k > 1; --k) {
    VLOG(1) << "k: " << k;

    // Generate variant paths from the last shortest path.
    const absl::Span<NodeIndex> last_shortest_path =
        absl::MakeSpan(paths.paths.back());

    // TODO(user): think about adding parallelism for this loop to improve
    // running times. This is not a priority as long as the algorithm is
    // faster than the one in `shortest_paths.h`.
    for (int spur_node_position = 0;
         spur_node_position < last_shortest_path.size() - 1;
         ++spur_node_position) {
      VLOG(4) << "  spur_node_position: " << spur_node_position;
      VLOG(4) << "  last_shortest_path: "
              << absl::StrJoin(last_shortest_path, " - ") << " ("
              << last_shortest_path.size() << ")";
      if (spur_node_position > 0) {
        DCHECK_NE(last_shortest_path[spur_node_position], source);
      }
      DCHECK_NE(last_shortest_path[spur_node_position], destination);

      const NodeIndex spur_node = last_shortest_path[spur_node_position];
      // Consider the part of the last shortest path up to and excluding the
      // spur node. If spur_node_position == 0, this span only contains the
      // source node.
      const absl::Span<NodeIndex> root_path =
          last_shortest_path.subspan(0, spur_node_position + 1);
      DCHECK_GE(root_path.length(), 1);
      DCHECK_NE(root_path.back(), destination);

      // Simplify the graph to have different paths using infinite lengths:
      // copy the weights, set some of them to infinity. There is no need to
      // restore the graph to its previous state in this case.
      //
      // This trick is used in the original article (it's old-fashioned), but
      // not in Wikipedia's pseudocode (it prefers mutating the graph, which is
      // harder to do without copying the whole graph structure).
      // Copying the whole graph might be quite expensive, especially as it is
      // not useful for long (computing one shortest path).
      std::vector<PathDistance> arc_lengths_for_detour = arc_lengths;
      for (absl::Span<const NodeIndex> previous_path : paths.paths) {
        // Check among the previous paths: if part of the path coincides with
        // the first few nodes up to the spur node (included), forbid this part
        // of the path in the search for the next shortest path. More
        // precisely, in that case, avoid the arc from the spur node to the
        // next node in the path.
        if (previous_path.size() <= root_path.length()) continue;
        const bool has_same_prefix_as_root_path = std::equal(
            root_path.begin(), root_path.end(), previous_path.begin(),
            previous_path.begin() + root_path.length());
        if (!has_same_prefix_as_root_path) continue;

        const typename GraphType::ArcIndex after_spur_node_arc =
            internal::FindArcIndex(graph, previous_path[spur_node_position],
                                   previous_path[spur_node_position + 1]);
        VLOG(4) << "  after_spur_node_arc: " << graph.Tail(after_spur_node_arc)
                << " - " << graph.Head(after_spur_node_arc) << " (" << source
                << " - " << destination << ")";
        arc_lengths_for_detour[after_spur_node_arc] =
            internal::kDisconnectedDistance;
      }
      // Ensure that the path computed from the new weights is loopless by
      // "removing" the nodes of the root path from the graph (by tweaking the
      // weights, again). The previous operation only disallows the arc from the
      // spur node (at the end of the root path) to the next node in the
      // previously found paths.
      for (int node_position = 0; node_position < spur_node_position;
           ++node_position) {
        for (const int arc : graph.OutgoingArcs(root_path[node_position])) {
          arc_lengths_for_detour[arc] = internal::kDisconnectedDistance;
        }
      }
      VLOG(3) << "  arc_lengths_for_detour: "
              << absl::StrJoin(arc_lengths_for_detour, " - ");

      // Generate a new candidate path from the spur node to the destination
      // without using the forbidden arcs.
      {
        std::tuple<std::vector<NodeIndex>, PathDistance> detour_path =
            internal::ComputeShortestPath(graph, arc_lengths_for_detour,
                                          spur_node, destination);

        if (std::get<0>(detour_path).empty()) {
          // Node unreachable after some arcs are forbidden.
          continue;
        }
        VLOG(2) << "  detour_path: "
                << absl::StrJoin(std::get<0>(detour_path), " - ") << " ("
                << std::get<0>(detour_path).size()
                << "): " << std::get<1>(detour_path);
        std::vector<NodeIndex> spur_path = std::move(std::get<0>(detour_path));
        if (ABSL_PREDICT_FALSE(spur_path.empty())) continue;

#ifndef NDEBUG
        CHECK_EQ(root_path.back(), spur_path.front());
        CHECK_EQ(spur_node, spur_path.front());

        if (spur_path.size() == 1) {
          CHECK_EQ(spur_path.front(), destination);
        } else {
          // Ensure there is an edge between the end of the root path
          // and the beginning of the spur path (knowing that both subpaths
          // coincide at the spur node).
          const bool root_path_leads_to_spur_path = absl::c_any_of(
              graph.OutgoingArcs(root_path.back()),
              [&graph, node_after_spur_in_spur_path = *(spur_path.begin() + 1)](
                  const typename GraphType::ArcIndex arc_index) {
                return graph.Head(arc_index) == node_after_spur_in_spur_path;
              });
          CHECK(root_path_leads_to_spur_path);
        }

        // Ensure the forbidden arc is not present in any previously generated
        // path.
        for (absl::Span<const NodeIndex> previous_path : paths.paths) {
          if (previous_path.size() <= spur_node_position + 1) continue;
          const bool has_same_prefix_as_root_path = std::equal(
              root_path.begin(), root_path.end(), previous_path.begin(),
              previous_path.begin() + root_path.length());
          if (has_same_prefix_as_root_path) {
            CHECK_NE(spur_path[1], previous_path[spur_node_position + 1])
                << "Forbidden arc " << previous_path[spur_node_position]
                << " - " << previous_path[spur_node_position + 1]
                << " is present in the spur path "
                << absl::StrJoin(spur_path, " - ");
          }
        }
#endif  // !defined(NDEBUG)

        // Assemble the new path.
        std::vector<NodeIndex> new_path;
        absl::c_copy(root_path.subspan(0, spur_node_position),
                     std::back_inserter(new_path));
        absl::c_copy(spur_path, std::back_inserter(new_path));

        DCHECK_EQ(new_path.front(), source);
        DCHECK_EQ(new_path.back(), destination);

#ifndef NDEBUG
        // Ensure the assembled path is loopless, i.e. no node is repeated.
        {
          absl::flat_hash_set<NodeIndex> visited_nodes(new_path.begin(),
                                                       new_path.end());
          CHECK_EQ(visited_nodes.size(), new_path.size());
        }
#endif  // !defined(NDEBUG)

        // Ensure the new path is not one of the previously known ones. This
        // operation is required, as there are two sources of paths from the
        // source to the destination:
        // - `paths`, the list of paths that is output by the function: there
        //   is no possible duplicate due to `arc_lengths_for_detour`, where
        //   edges that might generate a duplicate path are forbidden.
        // - `variant_path_queue`, the list of potential paths, ordered by
        //   their cost, with no impact on `arc_lengths_for_detour`.
        // TODO(user): would it be faster to fingerprint the paths and
        // filter by fingerprints? Due to the probability of error with
        // fingerprints, still use this slow-but-exact code, but after
        // filtering.
        const bool is_new_path_already_known = std::any_of(
            variant_path_queue.container().cbegin(),
            variant_path_queue.container().cend(),
            [&new_path](const internal::PathWithPriority<GraphType>& element) {
              return element.path() == new_path;
            });
        if (is_new_path_already_known) continue;

        const PathDistance path_length =
            internal::ComputePathLength(graph, arc_lengths, new_path);
        VLOG(5) << "  New potential path generated: "
                << absl::StrJoin(new_path, " - ") << " (" << new_path.size()
                << ")";
        VLOG(5) << "    Root: " << absl::StrJoin(root_path, " - ") << " ("
                << root_path.size() << ")";
        VLOG(5) << "    Spur: " << absl::StrJoin(spur_path, " - ") << " ("
                << spur_path.size() << ")";
        variant_path_queue.emplace(
            /*priority=*/path_length, /*path=*/std::move(new_path));
      }
    }

    // Add the shortest spur path ever found that has not yet been added. This
    // can be a spur path that has just been generated or a previous one, if
    // this iteration found no shorter one.
    if (variant_path_queue.empty()) break;

    const internal::PathWithPriority<GraphType>& next_shortest_path =
        variant_path_queue.top();
    VLOG(5) << "> New path generated: "
            << absl::StrJoin(next_shortest_path.path(), " - ") << " ("
            << next_shortest_path.path().size() << ")";
    paths.paths.emplace_back(next_shortest_path.path());
    paths.distances.push_back(next_shortest_path.priority());
    variant_path_queue.pop();
  }

  return paths;
}

}  // namespace operations_research

#endif  // OR_TOOLS_GRAPH_K_SHORTEST_PATHS_H_
