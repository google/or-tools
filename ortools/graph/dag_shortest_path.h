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

#ifndef OR_TOOLS_GRAPH_DAG_SHORTEST_PATH_H_
#define OR_TOOLS_GRAPH_DAG_SHORTEST_PATH_H_

#include <cmath>
#include <limits>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/base/log_severity.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"

namespace operations_research {
// TODO(user): extend to non-floating lengths.
// TODO(user): extend to allow for length functor.
// TODO(user): extend to k-shortest paths for k > 1.

// This library provides a few APIs to compute the shortest path on a given
// directed acyclic graph (DAG).
//
// In the DAG, multiple arcs between the same pair of nodes is allowed. However,
// self-loop arcs are not allowed.
//
// Note that we use the length formalism here, but the arc lengths can represent
// any numeric physical quantity. A shortest path will just be a path minimizing
// this quantity where the length of a path is the sum of the length of its
// arcs. An arc length can be negative, or +inf (indicating that it should not
// be used). An arc length cannot be -inf or nan.

// -----------------------------------------------------------------------------
// Basic API.
// -----------------------------------------------------------------------------

// `tail` and `head` should both be in [0, num_nodes)
// If the length is +inf, then the arc should not be used.
struct ArcWithLength {
  int tail = 0;
  int head = 0;
  double length = 0.0;
};

struct PathWithLength {
  double length = 0.0;
  // The returned arc indices points into the `arcs_with_length` passed to the
  // function below.
  std::vector<int> arc_path;
  std::vector<int> node_path;  // includes the source node.
};

// Returns {+inf, {}} if there is no path of finite length from the source to
// the destination. Dies if `arcs_with_length` has a cycle.
PathWithLength ShortestPathsOnDag(
    int num_nodes, absl::Span<const ArcWithLength> arcs_with_length, int source,
    int destination);

// -----------------------------------------------------------------------------
// Advanced API.
// -----------------------------------------------------------------------------

// A wrapper that holds the memory needed to run many shortest path computations
// efficiently on the given DAG. `GraphType` *must* implement the following
// types and functions: `GraphType::NodeIndex`, `GraphType::ArcIndex`,
// `num_nodes()`, `num_arcs()`, `OutgoingArcs(tail)`, `Tail(arc)` and
// `Head(arc)`. You can use on of the interfaces defined in
// `util/graph/graph.h`.
template <class GraphType>
class ShortestPathsOnDagWrapper {
 public:
  using NodeIndex = typename GraphType::NodeIndex;
  using ArcIndex = typename GraphType::ArcIndex;

  // IMPORTANT: All arguments must outlive the class.
  //
  // The vector of `arc_lengths` *must* be of size `graph.num_arcs()` and
  // indexed the same way as in `graph`.
  //
  // You *must* provide a topological order. You can use
  // `util::graph::FastTopologicalSort(graph)` to compute one if you don't
  // already have one. An invalid topological order results in an upper bound
  // for all shortest path computations. For maximum performance, you can
  // further reindex the nodes under the topological order so that the memory
  // access pattern is generally forward instead of random. For example, if the
  // topological order for a graph with 4 nodes is [2,1,0,3], you can re-label
  // the nodes 2, 1, and 0 to 0, 1, and 2 (and updates arcs accordingly).
  //
  // Validity of arcs and topological order are CHECKed if compiled in DEBUG
  // mode.
  //
  // SUBTLE: You can modify the graph, the arc lengths or the topological order
  // between calls to the `RunShortestPathOnDag()` function. That's fine. Doing
  // so will obviously invalidate the result API of the last shortest path run,
  // which could return an upper bound, junk, or crash.
  ShortestPathsOnDagWrapper(const GraphType* graph,
                            const std::vector<double>* arc_lengths,
                            absl::Span<const NodeIndex> topological_order);

  // Computes the shortest path to all reachable nodes from the given sources.
  // This must be called before any of the query functions below.
  void RunShortestPathOnDag(absl::Span<const NodeIndex> sources);

  // Returns true if `node` is reachable from at least one source, i.e., the
  // length from at least one source is finite.
  bool IsReachable(NodeIndex node) const {
    return length_from_sources_[node] < std::numeric_limits<double>::infinity();
  }
  const std::vector<NodeIndex>& reached_nodes() const { return reached_nodes_; }

  // Returns the length of the shortest path from `node`'s source to `node`.
  double LengthTo(NodeIndex node) const { return length_from_sources_[node]; }

  // Returns the list of all the arcs in the shortest path from `node`'s
  // source to `node`. CHECKs if the node is reachable.
  std::vector<ArcIndex> ArcPathTo(NodeIndex node) const;

  // Returns the list of all the nodes in the shortest path from `node`'s
  // source to `node` (including the source). CHECKs if the node is reachable.
  std::vector<NodeIndex> NodePathTo(NodeIndex node) const;

  // Accessors to the underlying graph and arc lengths.
  const GraphType& graph() const { return *graph_; }
  const std::vector<double>& arc_lengths() const { return *arc_lengths_; }

 private:
  const GraphType* const graph_;
  const std::vector<double>* const arc_lengths_;
  absl::Span<const NodeIndex> const topological_order_;

  // Data about the last call of the RunShortestPathOnDag() function.
  std::vector<double> length_from_sources_;
  std::vector<ArcIndex> incoming_shortest_path_arc_;
  std::vector<NodeIndex> reached_nodes_;
};

template <typename GraphType>
absl::Status TopologicalOrderIsValid(
    const GraphType& graph,
    absl::Span<const typename GraphType::NodeIndex> topological_order);

// -----------------------------------------------------------------------------
// Implementation.
// -----------------------------------------------------------------------------

template <class GraphType>
ShortestPathsOnDagWrapper<GraphType>::ShortestPathsOnDagWrapper(
    const GraphType* graph, const std::vector<double>* arc_lengths,
    absl::Span<const NodeIndex> topological_order)
    : graph_(graph),
      arc_lengths_(arc_lengths),
      topological_order_(topological_order) {
  CHECK(graph_ != nullptr);
  CHECK(arc_lengths_ != nullptr);
  if (DEBUG_MODE) {
    CHECK_EQ(arc_lengths_->size(), graph_->num_arcs());
    for (const double arc_length : *arc_lengths_) {
      CHECK(arc_length != -std::numeric_limits<double>::infinity() &&
            !std::isnan(arc_length))
          << absl::StrFormat("length cannot be -inf nor NaN");
    }
    CHECK_OK(TopologicalOrderIsValid(*graph_, topological_order_))
        << "Invalid topological order";
  }

  // Memory allocation is done here and only once in order to avoid reallocation
  // at each call of `RunShortestPathOnDag()` for better performance.
  length_from_sources_.resize(graph_->num_nodes(),
                              std::numeric_limits<double>::infinity());
  incoming_shortest_path_arc_.resize(graph_->num_nodes(), -1);
  reached_nodes_.reserve(graph_->num_nodes());
}

template <class GraphType>
void ShortestPathsOnDagWrapper<GraphType>::RunShortestPathOnDag(
    absl::Span<const NodeIndex> sources) {
  // Caching the vector addresses allow to not fetch it on each access.
  const absl::Span<double> length_from_sources =
      absl::MakeSpan(length_from_sources_);
  const absl::Span<const double> arc_lengths = *arc_lengths_;

  // Note that we do not re-assign `incoming_shortest_path_arc_` at every call
  // for better performance, so it only makes sense for nodes that are reachable
  // from at least one source, the other ones will contain junk.
  for (const NodeIndex node : reached_nodes_) {
    length_from_sources[node] = std::numeric_limits<double>::infinity();
  }
  DCHECK(std::all_of(
      length_from_sources.begin(), length_from_sources.end(),
      [](double l) { return l == std::numeric_limits<double>::infinity(); }));
  reached_nodes_.clear();

  for (const NodeIndex source : sources) {
    length_from_sources[source] = 0.0;
  }

  for (const NodeIndex tail : topological_order_) {
    const double length_to_tail = length_from_sources[tail];
    // Stop exploring a node as soon as its length to all sources is +inf.
    if (length_to_tail == std::numeric_limits<double>::infinity()) {
      continue;
    }
    reached_nodes_.push_back(tail);
    for (const ArcIndex arc : graph_->OutgoingArcs(tail)) {
      const NodeIndex head = graph_->Head(arc);
      DCHECK(arc_lengths[arc] != -std::numeric_limits<double>::infinity());
      const double length_to_head = arc_lengths[arc] + length_to_tail;
      if (length_to_head < length_from_sources[head]) {
        length_from_sources[head] = length_to_head;
        incoming_shortest_path_arc_[head] = arc;
      }
    }
  }
}

// TODO(user): If `ArcPathTo` and/or `NodePathTo` functions become
// bottlenecks:
//    (1) have the class preallocate a buffer of size `num_nodes`
//    (2) assign into an index rather than with push_back
//    (3) return by absl::Span (or return a copy) with known size.
template <typename GraphType>
std::vector<typename GraphType::ArcIndex>
ShortestPathsOnDagWrapper<GraphType>::ArcPathTo(NodeIndex node) const {
  CHECK(IsReachable(node));
  std::vector<ArcIndex> arc_path;
  ArcIndex current_arc = incoming_shortest_path_arc_[node];
  while (current_arc != -1) {
    arc_path.push_back(current_arc);
    current_arc = incoming_shortest_path_arc_[graph_->Tail(current_arc)];
  }
  absl::c_reverse(arc_path);
  return arc_path;
}

template <typename GraphType>
std::vector<typename GraphType::NodeIndex>
ShortestPathsOnDagWrapper<GraphType>::NodePathTo(NodeIndex node) const {
  CHECK(IsReachable(node));
  std::vector<NodeIndex> node_path;
  NodeIndex current_node = node;
  node_path.push_back(current_node);
  ArcIndex current_arc = incoming_shortest_path_arc_[current_node];
  while (current_arc != -1) {
    current_node = graph_->Tail(current_arc);
    node_path.push_back(current_node);
    current_arc = incoming_shortest_path_arc_[current_node];
  }
  absl::c_reverse(node_path);
  return node_path;
}

template <typename GraphType>
absl::Status TopologicalOrderIsValid(
    const GraphType& graph,
    absl::Span<const typename GraphType::NodeIndex> topological_order) {
  using NodeIndex = typename GraphType::NodeIndex;
  using ArcIndex = typename GraphType::ArcIndex;
  const NodeIndex num_nodes = graph.num_nodes();
  if (topological_order.size() != num_nodes) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "topological_order.size() = %i, != graph.num_nodes() = %i",
        topological_order.size(), num_nodes));
  }
  std::vector<NodeIndex> inverse_topology(num_nodes, -1);
  for (NodeIndex node = 0; node < topological_order.size(); ++node) {
    if (inverse_topology[topological_order[node]] >= 0) {
      return absl::InvalidArgumentError(
          absl::StrFormat("node % i appears twice in topological order",
                          topological_order[node]));
    }
    inverse_topology[topological_order[node]] = node;
  }
  for (NodeIndex tail = 0; tail < num_nodes; ++tail) {
    for (const ArcIndex arc : graph.OutgoingArcs(tail)) {
      const NodeIndex head = graph.Head(arc);
      if (inverse_topology[tail] >= inverse_topology[head]) {
        return absl::InvalidArgumentError(absl::StrFormat(
            "arc (%i, %i) is inconsistent with topological order", tail, head));
      }
    }
  }
  return absl::OkStatus();
}

}  // namespace operations_research
#endif  // OR_TOOLS_GRAPH_DAG_SHORTEST_PATH_H_
