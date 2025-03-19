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

#ifndef OR_TOOLS_GRAPH_DAG_SHORTEST_PATH_H_
#define OR_TOOLS_GRAPH_DAG_SHORTEST_PATH_H_

#include <cmath>
#if __cplusplus >= 202002L
#include <concepts>
#endif
#include <functional>
#include <limits>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "absl/types/span.h"

namespace operations_research {
// TODO(b/332475231): extend to non-floating lengths.
// TODO(b/332476147): extend to allow for length functor.

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

// `from` and `to` should both be in [0, num_nodes).
// If the length is +inf, then the arc should not be used.
struct ArcWithLength {
  int from = 0;
  int to = 0;
  double length = 0.0;
};

struct PathWithLength {
  double length = 0.0;
  // The returned arc indices points into the `arcs_with_length` passed to the
  // function below.
  std::vector<int> arc_path;
  std::vector<int> node_path;  // includes the source node.
};

// Returns {+inf, {}, {}} if there is no path of finite length from the source
// to the destination. Dies if `arcs_with_length` has a cycle.
PathWithLength ShortestPathsOnDag(
    int num_nodes, absl::Span<const ArcWithLength> arcs_with_length, int source,
    int destination);

// Returns the k-shortest paths by increasing length. Returns fewer than k paths
// if there are fewer than k paths from the source to the destination. Returns
// {{+inf, {}, {}}} if there is no path of finite length from the source to the
// destination. Dies if `arcs_with_length` has a cycle.
std::vector<PathWithLength> KShortestPathsOnDag(
    int num_nodes, absl::Span<const ArcWithLength> arcs_with_length, int source,
    int destination, int path_count);

// -----------------------------------------------------------------------------
// Advanced API.
// -----------------------------------------------------------------------------
// This concept only enforces the standard graph API needed for all algorithms
// on DAGs. One could add the requirement of being a DAG wihtin this concept
// (which is done before running the algorithm).
#if __cplusplus >= 202002L
template <class GraphType>
concept DagGraphType = requires(GraphType graph) {
  { typename GraphType::NodeIndex{} };
  { typename GraphType::ArcIndex{} };
  { graph.num_nodes() } -> std::same_as<typename GraphType::NodeIndex>;
  { graph.num_arcs() } -> std::same_as<typename GraphType::ArcIndex>;
  { graph.OutgoingArcs(typename GraphType::NodeIndex{}) };
  {
    graph.Tail(typename GraphType::ArcIndex{})
  } -> std::same_as<typename GraphType::NodeIndex>;
  {
    graph.Head(typename GraphType::ArcIndex{})
  } -> std::same_as<typename GraphType::NodeIndex>;
  { graph.Build() };
};
#endif

// A wrapper that holds the memory needed to run many shortest path computations
// efficiently on the given DAG. One call of `RunShortestPathOnDag()` has time
// complexity O(|E| + |V|) and space complexity O(|V|).
// `GraphType` can use any of the interfaces defined in `util/graph/graph.h`.
template <class GraphType>
#if __cplusplus >= 202002L
  requires DagGraphType<GraphType>
#endif
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
  bool IsReachable(NodeIndex node) const;
  const std::vector<NodeIndex>& reached_nodes() const { return reached_nodes_; }

  // Returns the length of the shortest path from `node`'s source to `node`.
  double LengthTo(NodeIndex node) const { return length_from_sources_[node]; }
  std::vector<double> LengthTo() const { return length_from_sources_; }

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
  static constexpr double kInf = std::numeric_limits<double>::infinity();
  const GraphType* const graph_;
  const std::vector<double>* const arc_lengths_;
  absl::Span<const NodeIndex> const topological_order_;

  // Data about the last call of the RunShortestPathOnDag() function.
  std::vector<double> length_from_sources_;
  std::vector<ArcIndex> incoming_shortest_path_arc_;
  std::vector<NodeIndex> reached_nodes_;
};

// A wrapper that holds the memory needed to run many k-shortest paths
// computations efficiently on the given DAG. One call of
// `RunKShortestPathOnDag()` has time complexity O(|E| + k|V|log(d)) where d is
// the mean degree of the graph and space complexity O(k|V|).
// `GraphType` can use any of the interfaces defined in `util/graph/graph.h`.
// IMPORTANT: Only use if `path_count > 1` (k > 1) otherwise use
// `ShortestPathsOnDagWrapper`.
template <class GraphType>
#if __cplusplus >= 202002L
  requires DagGraphType<GraphType>
#endif
class KShortestPathsOnDagWrapper {
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
  // between calls to the `RunKShortestPathOnDag()` function. That's fine. Doing
  // so will obviously invalidate the result API of the last shortest path run,
  // which could return an upper bound, junk, or crash.
  KShortestPathsOnDagWrapper(const GraphType* graph,
                             const std::vector<double>* arc_lengths,
                             absl::Span<const NodeIndex> topological_order,
                             int path_count);

  // Computes the shortest path to all reachable nodes from the given sources.
  // This must be called before any of the query functions below.
  void RunKShortestPathOnDag(absl::Span<const NodeIndex> sources);

  // Returns true if `node` is reachable from at least one source, i.e., the
  // length of the shortest path from at least one source is finite.
  bool IsReachable(NodeIndex node) const;
  const std::vector<NodeIndex>& reached_nodes() const { return reached_nodes_; }

  // Returns the lengths of the k-shortest paths from `node`'s source to `node`
  // in increasing order. If there are less than k paths, return all path
  // lengths.
  std::vector<double> LengthsTo(NodeIndex node) const;

  // Returns the list of all the arcs of the k-shortest paths from `node`'s
  // source to `node`.
  std::vector<std::vector<ArcIndex>> ArcPathsTo(NodeIndex node) const;

  // Returns the list of all the nodes of the k-shortest paths from `node`'s
  // source to `node` (including the source). CHECKs if the node is reachable.
  std::vector<std::vector<NodeIndex>> NodePathsTo(NodeIndex node) const;

  // Accessors to the underlying graph and arc lengths.
  const GraphType& graph() const { return *graph_; }
  const std::vector<double>& arc_lengths() const { return *arc_lengths_; }
  int path_count() const { return path_count_; }

 private:
  static constexpr double kInf = std::numeric_limits<double>::infinity();

  const GraphType* const graph_;
  const std::vector<double>* const arc_lengths_;
  absl::Span<const NodeIndex> const topological_order_;
  const int path_count_;

  GraphType reverse_graph_;
  // Maps reverse arc indices to indices in the original graph.
  std::vector<ArcIndex> arc_indices_;

  // Data about the last call of the `RunKShortestPathOnDag()` function. The
  // first dimension is the index of the path (1st being the shortest). The
  // second dimension are nodes.
  std::vector<std::vector<double>> lengths_from_sources_;
  std::vector<std::vector<ArcIndex>> incoming_shortest_paths_arc_;
  std::vector<std::vector<int>> incoming_shortest_paths_index_;
  std::vector<bool> is_source_;
  std::vector<NodeIndex> reached_nodes_;
};

template <class GraphType>
#if __cplusplus >= 202002L
  requires DagGraphType<GraphType>
#endif
absl::Status TopologicalOrderIsValid(
    const GraphType& graph,
    absl::Span<const typename GraphType::NodeIndex> topological_order);

// -----------------------------------------------------------------------------
// Implementations.
// -----------------------------------------------------------------------------
// TODO(b/332475804): If `ArcPathTo` and/or `NodePathTo` functions become
// bottlenecks:
//    (1) have the class preallocate a buffer of size `num_nodes`
//    (2) assign into an index rather than with push_back
//    (3) return by absl::Span (or return a copy) with known size.
template <typename GraphType>
#if __cplusplus >= 202002L
  requires DagGraphType<GraphType>
#endif
std::vector<typename GraphType::NodeIndex> NodePathImpliedBy(
    absl::Span<const typename GraphType::ArcIndex> arc_path,
    const GraphType& graph) {
  CHECK(!arc_path.empty());
  std::vector<typename GraphType::NodeIndex> node_path;
  node_path.reserve(arc_path.size() + 1);
  for (const typename GraphType::ArcIndex arc_index : arc_path) {
    node_path.push_back(graph.Tail(arc_index));
  }
  node_path.push_back(graph.Head(arc_path.back()));
  return node_path;
}

template <class GraphType>
#if __cplusplus >= 202002L
  requires DagGraphType<GraphType>
#endif
void CheckNodeIsValid(typename GraphType::NodeIndex node,
                      const GraphType& graph) {
  CHECK_GE(node, 0) << "Node must be nonnegative. Input value: " << node;
  CHECK_LT(node, graph.num_nodes())
      << "Node must be a valid node. Input value: " << node
      << ". Number of nodes in the input graph: " << graph.num_nodes();
}

template <class GraphType>
#if __cplusplus >= 202002L
  requires DagGraphType<GraphType>
#endif
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

// -----------------------------------------------------------------------------
// ShortestPathsOnDagWrapper implementation.
// -----------------------------------------------------------------------------
template <class GraphType>
#if __cplusplus >= 202002L
  requires DagGraphType<GraphType>
#endif
ShortestPathsOnDagWrapper<GraphType>::ShortestPathsOnDagWrapper(
    const GraphType* graph, const std::vector<double>* arc_lengths,
    absl::Span<const NodeIndex> topological_order)
    : graph_(graph),
      arc_lengths_(arc_lengths),
      topological_order_(topological_order) {
  CHECK(graph_ != nullptr);
  CHECK(arc_lengths_ != nullptr);
  CHECK_GT(graph_->num_nodes(), 0) << "The graph is empty: it has no nodes";
#ifndef NDEBUG
  CHECK_EQ(arc_lengths_->size(), graph_->num_arcs());
  for (const double arc_length : *arc_lengths_) {
    CHECK(arc_length != -kInf && !std::isnan(arc_length))
        << absl::StrFormat("length cannot be -inf nor NaN");
  }
  CHECK_OK(TopologicalOrderIsValid(*graph_, topological_order_))
      << "Invalid topological order";
#endif

  // Memory allocation is done here and only once in order to avoid reallocation
  // at each call of `RunShortestPathOnDag()` for better performance.
  length_from_sources_.resize(graph_->num_nodes(), kInf);
  incoming_shortest_path_arc_.resize(graph_->num_nodes(), -1);
  reached_nodes_.reserve(graph_->num_nodes());
}

template <class GraphType>
#if __cplusplus >= 202002L
  requires DagGraphType<GraphType>
#endif
void ShortestPathsOnDagWrapper<GraphType>::RunShortestPathOnDag(
    absl::Span<const NodeIndex> sources) {
  // Caching the vector addresses allow to not fetch it on each access.
  const absl::Span<double> length_from_sources =
      absl::MakeSpan(length_from_sources_);
  const absl::Span<const double> arc_lengths = *arc_lengths_;

  // Avoid reassigning `incoming_shortest_path_arc_` at every call for better
  // performance, so it only makes sense for nodes that are reachable from at
  // least one source, the other ones will contain junk.
  for (const NodeIndex node : reached_nodes_) {
    length_from_sources[node] = kInf;
  }
  DCHECK(std::all_of(length_from_sources.begin(), length_from_sources.end(),
                     [](double l) { return l == kInf; }));
  reached_nodes_.clear();

  for (const NodeIndex source : sources) {
    CheckNodeIsValid(source, *graph_);
    length_from_sources[source] = 0.0;
  }

  for (const NodeIndex tail : topological_order_) {
    const double length_to_tail = length_from_sources[tail];
    // Stop exploring a node as soon as its length to all sources is +inf.
    if (length_to_tail == kInf) {
      continue;
    }
    reached_nodes_.push_back(tail);
    for (const ArcIndex arc : graph_->OutgoingArcs(tail)) {
      const NodeIndex head = graph_->Head(arc);
      DCHECK(arc_lengths[arc] != -kInf);
      const double length_to_head = arc_lengths[arc] + length_to_tail;
      if (length_to_head < length_from_sources[head]) {
        length_from_sources[head] = length_to_head;
        incoming_shortest_path_arc_[head] = arc;
      }
    }
  }
}

template <class GraphType>
#if __cplusplus >= 202002L
  requires DagGraphType<GraphType>
#endif
bool ShortestPathsOnDagWrapper<GraphType>::IsReachable(NodeIndex node) const {
  CheckNodeIsValid(node, *graph_);
  return length_from_sources_[node] < kInf;
}

template <class GraphType>
#if __cplusplus >= 202002L
  requires DagGraphType<GraphType>
#endif
std::vector<typename GraphType::ArcIndex>
ShortestPathsOnDagWrapper<GraphType>::ArcPathTo(NodeIndex node) const {
  CHECK(IsReachable(node));
  std::vector<ArcIndex> arc_path;
  NodeIndex current_node = node;
  for (int i = 0; i < graph_->num_nodes(); ++i) {
    ArcIndex current_arc = incoming_shortest_path_arc_[current_node];
    if (current_arc == -1) {
      break;
    }
    arc_path.push_back(current_arc);
    current_node = graph_->Tail(current_arc);
  }
  absl::c_reverse(arc_path);
  return arc_path;
}

template <class GraphType>
#if __cplusplus >= 202002L
  requires DagGraphType<GraphType>
#endif
std::vector<typename GraphType::NodeIndex>
ShortestPathsOnDagWrapper<GraphType>::NodePathTo(NodeIndex node) const {
  const std::vector<typename GraphType::ArcIndex> arc_path = ArcPathTo(node);
  if (arc_path.empty()) {
    return {node};
  }
  return NodePathImpliedBy(ArcPathTo(node), *graph_);
}

// -----------------------------------------------------------------------------
// KShortestPathsOnDagWrapper implementation.
// -----------------------------------------------------------------------------
template <class GraphType>
#if __cplusplus >= 202002L
  requires DagGraphType<GraphType>
#endif
KShortestPathsOnDagWrapper<GraphType>::KShortestPathsOnDagWrapper(
    const GraphType* graph, const std::vector<double>* arc_lengths,
    absl::Span<const NodeIndex> topological_order, const int path_count)
    : graph_(graph),
      arc_lengths_(arc_lengths),
      topological_order_(topological_order),
      path_count_(path_count) {
  CHECK(graph_ != nullptr);
  CHECK(arc_lengths_ != nullptr);
  CHECK_GT(graph_->num_nodes(), 0) << "The graph is empty: it has no nodes";
  CHECK_GT(path_count_, 0) << "path_count must be greater than 0";
#ifndef NDEBUG
  CHECK_EQ(arc_lengths_->size(), graph_->num_arcs());
  for (const double arc_length : *arc_lengths_) {
    CHECK(arc_length != -kInf && !std::isnan(arc_length))
        << absl::StrFormat("length cannot be -inf nor NaN");
  }
  CHECK_OK(TopologicalOrderIsValid(*graph_, topological_order_))
      << "Invalid topological order";
#endif

  // TODO(b/332475713): Optimize if reverse graph is already provided in
  // `GraphType`.
  const int num_arcs = graph_->num_arcs();
  reverse_graph_ = GraphType(graph_->num_nodes(), num_arcs);
  for (ArcIndex arc_index = 0; arc_index < num_arcs; ++arc_index) {
    reverse_graph_.AddArc(graph->Head(arc_index), graph->Tail(arc_index));
  }
  std::vector<ArcIndex> permutation;
  reverse_graph_.Build(&permutation);
  arc_indices_.resize(permutation.size());
  if (!permutation.empty()) {
    for (int i = 0; i < permutation.size(); ++i) {
      arc_indices_[permutation[i]] = i;
    }
  }

  // Memory allocation is done here and only once in order to avoid reallocation
  // at each call of `RunKShortestPathOnDag()` for better performance.
  lengths_from_sources_.resize(path_count_);
  incoming_shortest_paths_arc_.resize(path_count_);
  incoming_shortest_paths_index_.resize(path_count_);
  for (int k = 0; k < path_count_; ++k) {
    lengths_from_sources_[k].resize(graph_->num_nodes(), kInf);
    incoming_shortest_paths_arc_[k].resize(graph_->num_nodes(), -1);
    incoming_shortest_paths_index_[k].resize(graph_->num_nodes(), -1);
  }
  is_source_.resize(graph_->num_nodes(), false);
  reached_nodes_.reserve(graph_->num_nodes());
}

template <class GraphType>
#if __cplusplus >= 202002L
  requires DagGraphType<GraphType>
#endif
void KShortestPathsOnDagWrapper<GraphType>::RunKShortestPathOnDag(
    absl::Span<const NodeIndex> sources) {
  // Caching the vector addresses allow to not fetch it on each access.
  const absl::Span<const double> arc_lengths = *arc_lengths_;
  const absl::Span<const ArcIndex> arc_indices = arc_indices_;

  // Avoid reassigning `incoming_shortest_path_arc_` at every call for better
  // performance, so it only makes sense for nodes that are reachable from at
  // least one source, the other ones will contain junk.

  for (const NodeIndex node : reached_nodes_) {
    is_source_[node] = false;
    for (int k = 0; k < path_count_; ++k) {
      lengths_from_sources_[k][node] = kInf;
    }
  }
  reached_nodes_.clear();
#ifndef NDEBUG
  for (int k = 0; k < path_count_; ++k) {
    CHECK(std::all_of(lengths_from_sources_[k].begin(),
                      lengths_from_sources_[k].end(),
                      [](double l) { return l == kInf; }));
  }
#endif

  for (const NodeIndex source : sources) {
    CheckNodeIsValid(source, *graph_);
    is_source_[source] = true;
  }

  struct IncomingArcPath {
    double path_length = 0.0;
    ArcIndex arc_index = 0;
    double arc_length = 0.0;
    NodeIndex from = 0;
    int path_index = 0;

    bool operator<(const IncomingArcPath& other) const {
      return std::tie(path_length, from) <
             std::tie(other.path_length, other.from);
    }
    bool operator>(const IncomingArcPath& other) const { return other < *this; }
  };
  std::vector<IncomingArcPath> min_heap;
  auto comp = std::greater<IncomingArcPath>();
  for (const NodeIndex to : topological_order_) {
    min_heap.clear();
    if (is_source_[to]) {
      min_heap.push_back({.arc_index = -1});
    }
    for (const ArcIndex reverse_arc_index : reverse_graph_.OutgoingArcs(to)) {
      const ArcIndex arc_index = arc_indices.empty()
                                     ? reverse_arc_index
                                     : arc_indices[reverse_arc_index];
      const NodeIndex from = graph_->Tail(arc_index);
      const double arc_length = arc_lengths[arc_index];
      DCHECK(arc_length != -kInf);
      const double path_length =
          lengths_from_sources_.front()[from] + arc_length;
      if (path_length == kInf) {
        continue;
      }
      min_heap.push_back({.path_length = path_length,
                          .arc_index = arc_index,
                          .arc_length = arc_length,
                          .from = from});
      std::push_heap(min_heap.begin(), min_heap.end(), comp);
    }
    if (min_heap.empty()) {
      continue;
    }
    reached_nodes_.push_back(to);
    for (int k = 0; k < path_count_; ++k) {
      std::pop_heap(min_heap.begin(), min_heap.end(), comp);
      IncomingArcPath& incoming_arc_path = min_heap.back();
      lengths_from_sources_[k][to] = incoming_arc_path.path_length;
      incoming_shortest_paths_arc_[k][to] = incoming_arc_path.arc_index;
      incoming_shortest_paths_index_[k][to] = incoming_arc_path.path_index;
      if (incoming_arc_path.arc_index != -1 &&
          incoming_arc_path.path_index < path_count_ - 1 &&
          lengths_from_sources_[incoming_arc_path.path_index + 1]
                               [incoming_arc_path.from] < kInf) {
        ++incoming_arc_path.path_index;
        incoming_arc_path.path_length =
            lengths_from_sources_[incoming_arc_path.path_index]
                                 [incoming_arc_path.from] +
            incoming_arc_path.arc_length;
        std::push_heap(min_heap.begin(), min_heap.end(), comp);
      } else {
        min_heap.pop_back();
        if (min_heap.empty()) {
          break;
        }
      }
    }
  }
}

template <class GraphType>
#if __cplusplus >= 202002L
  requires DagGraphType<GraphType>
#endif
bool KShortestPathsOnDagWrapper<GraphType>::IsReachable(NodeIndex node) const {
  CheckNodeIsValid(node, *graph_);
  return lengths_from_sources_.front()[node] < kInf;
}

template <class GraphType>
#if __cplusplus >= 202002L
  requires DagGraphType<GraphType>
#endif
std::vector<double> KShortestPathsOnDagWrapper<GraphType>::LengthsTo(
    NodeIndex node) const {
  std::vector<double> lengths_to;
  lengths_to.reserve(path_count_);
  for (int k = 0; k < path_count_; ++k) {
    const double length_to = lengths_from_sources_[k][node];
    if (length_to == kInf) {
      break;
    }
    lengths_to.push_back(length_to);
  }
  return lengths_to;
}

template <class GraphType>
#if __cplusplus >= 202002L
  requires DagGraphType<GraphType>
#endif
std::vector<std::vector<typename GraphType::ArcIndex>>
KShortestPathsOnDagWrapper<GraphType>::ArcPathsTo(NodeIndex node) const {
  std::vector<std::vector<ArcIndex>> arc_paths;
  arc_paths.reserve(path_count_);
  for (int k = 0; k < path_count_; ++k) {
    if (lengths_from_sources_[k][node] == kInf) {
      break;
    }
    std::vector<ArcIndex> arc_path;
    int current_path_index = k;
    NodeIndex current_node = node;
    for (int i = 0; i < graph_->num_nodes(); ++i) {
      ArcIndex current_arc =
          incoming_shortest_paths_arc_[current_path_index][current_node];
      if (current_arc == -1) {
        break;
      }
      arc_path.push_back(current_arc);
      current_path_index =
          incoming_shortest_paths_index_[current_path_index][current_node];
      current_node = graph_->Tail(current_arc);
    }
    absl::c_reverse(arc_path);
    arc_paths.push_back(arc_path);
  }
  return arc_paths;
}

template <class GraphType>
#if __cplusplus >= 202002L
  requires DagGraphType<GraphType>
#endif
std::vector<std::vector<typename GraphType::NodeIndex>>
KShortestPathsOnDagWrapper<GraphType>::NodePathsTo(NodeIndex node) const {
  const std::vector<std::vector<ArcIndex>> arc_paths = ArcPathsTo(node);
  std::vector<std::vector<NodeIndex>> node_paths(arc_paths.size());
  for (int k = 0; k < arc_paths.size(); ++k) {
    if (arc_paths[k].empty()) {
      node_paths[k] = {node};
    } else {
      node_paths[k] = NodePathImpliedBy(arc_paths[k], *graph_);
    }
  }
  return node_paths;
}

}  // namespace operations_research
#endif  // OR_TOOLS_GRAPH_DAG_SHORTEST_PATH_H_
