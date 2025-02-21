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

#ifndef OR_TOOLS_GRAPH_DAG_CONSTRAINED_SHORTEST_PATH_H_
#define OR_TOOLS_GRAPH_DAG_CONSTRAINED_SHORTEST_PATH_H_

#include <cmath>
#include <limits>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/base/log_severity.h"
#include "absl/log/check.h"
#include "absl/strings/str_format.h"
#include "absl/types/span.h"
#include "ortools/base/threadpool.h"
#include "ortools/graph/dag_shortest_path.h"
#include "ortools/graph/graph.h"

namespace operations_research {

// This library provides APIs to compute the constrained shortest path (CSP) on
// a given directed acyclic graph (DAG) with resources on each arc. A CSP is a
// shortest path on a DAG which does not exceed a set of maximum resources
// consumption. The algorithm is exponential and has no guarantee to finish. It
// is based on bi-drectionnal search. First is a forward pass from the source to
// nodes “somewhere in the middle” to generate forward labels, just as the
// onedirectional labeling algorithm we discussed; then a symmetric backward
// pass from the destination generates backward labels; and finally at each node
// with both forward and backward labels, it joins any pair of labels to form a
// feasible complete path. Intuitively, the number of labels grows exponentially
// with the number of arcs in the path. The overall number of labels are then
// expected to be smaller with shorter paths. For DAG with a topological
// ordering, we can pick any node (usually right in the middle) as a *midpoint*
// to stop each pass at. Then labels can be joined at only one half of the nodes
// by considering all edges between each half.
//
// In the DAG, multiple arcs between the same pair of nodes is allowed. However,
// self-loop arcs are not allowed.
//
// Note that we use the length formalism here, but the arc lengths can represent
// any numeric physical quantity. A shortest path will just be a path minimizing
// this quantity where the length/resources of a path is the sum of the
// length/resources of its arcs. An arc length can be negative, or +inf
// (indicating that it should not be used). An arc length cannot be -inf or nan.
//
// Resources on each arc must be non-negative and cannot be +inf or nan.

// -----------------------------------------------------------------------------
// Basic API.
// -----------------------------------------------------------------------------

// `tail` and `head` should both be in [0, num_nodes)
// If the length is +inf, then the arc is not used.
struct ArcWithLengthAndResources {
  int from = 0;
  int to = 0;
  double length = 0.0;
  std::vector<double> resources;
};

// Returns {+inf, {}, {}} if there is no path of finite length from the source
// to the destination. Dies if `arcs_with_length_and_resources` has a cycle.
PathWithLength ConstrainedShortestPathsOnDag(
    int num_nodes,
    absl::Span<const ArcWithLengthAndResources> arcs_with_length_and_resources,
    int source, int destination, const std::vector<double>& max_resources);

// -----------------------------------------------------------------------------
// Advanced API.
// -----------------------------------------------------------------------------
// A wrapper that holds the memory needed to run many constrained shortest path
// computations efficiently on the given DAG (on which resources do not change).
// `GraphType` can use one of the interfaces defined in `util/graph/graph.h`.
template <class GraphType>
#if __cplusplus >= 202002L
  requires DagGraphType<GraphType>
#endif
class ConstrainedShortestPathsOnDagWrapper {
 public:
  using NodeIndex = typename GraphType::NodeIndex;
  using ArcIndex = typename GraphType::ArcIndex;

  // IMPORTANT: All arguments must outlive the class.
  //
  // The vectors of `arc_lengths` and `arc_resources[i]` (for all resource i)
  // *must* be of size `graph.num_arcs()` and indexed the same way as in
  // `graph`. The vector `arc_resources` and `max_resources` *must* be of same
  // size.
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
  // Validity of arcs and topological order are DCHECKed.
  //
  // If the number of labels in memory exceeds `max_num_created_labels / 2` at
  // any point in each pass of the algorithm, new labels are not generated
  // anymore and it returns the best path found so far, most particularly the
  // empty path if none were found.
  //
  // IMPORTANT: You cannot modify anything except `arc_lengths` between calls to
  // the `RunConstrainedShortestPathOnDag()` function.
  ConstrainedShortestPathsOnDagWrapper(
      const GraphType* graph, const std::vector<double>* arc_lengths,
      const std::vector<std::vector<double>>* arc_resources,
      absl::Span<const NodeIndex> topological_order,
      absl::Span<const NodeIndex> sources,
      absl::Span<const NodeIndex> destinations,
      const std::vector<double>* max_resources,
      int max_num_created_labels = 1e9);

  // Returns {+inf, {}, {}} if there is no constrained path of finite length
  // wihtin resources constraints from one node in `sources` to one node in
  // `destinations`.
  PathWithLength RunConstrainedShortestPathOnDag();

  // For benchmarking and informational purposes, returns the number of labels
  // generated in the call of `RunConstrainedShortestPathOnDag()`.
  int label_count() const {
    return lengths_from_sources_[FORWARD].size() +
           lengths_from_sources_[BACKWARD].size();
  }

 private:
  enum Direction {
    FORWARD = 0,
    BACKWARD = 1,
  };

  inline static Direction Reverse(Direction d) {
    return d == FORWARD ? BACKWARD : FORWARD;
  }

  // A LabelPair includes the `length` of a path that can be constructed by
  // merging the paths from two *linkable* labels corresponding to
  // `label_index`.
  struct LabelPair {
    double length = 0.0;
    int label_index[2];
  };

  void RunHalfConstrainedShortestPathOnDag(
      const GraphType& reverse_graph, absl::Span<const double> arc_lengths,
      absl::Span<const std::vector<double>> arc_resources,
      absl::Span<const std::vector<double>> min_arc_resources,
      absl::Span<const double> max_resources, int max_num_created_labels,
      std::vector<double>& lengths_from_sources,
      std::vector<std::vector<double>>& resources_from_sources,
      std::vector<ArcIndex>& incoming_arc_indices_from_sources,
      std::vector<int>& incoming_label_indices_from_sources,
      std::vector<int>& first_label, std::vector<int>& num_labels);

  // Returns the arc index linking two nodes from each pass forming the best
  // path. Returns -1 if no better path than the one found from
  // `best_label_pair` is found.
  ArcIndex MergeHalfRuns(
      const GraphType& graph, absl::Span<const double> arc_lengths,
      absl::Span<const std::vector<double>> arc_resources,
      absl::Span<const double> max_resources,
      const std::vector<NodeIndex> sub_node_indices[2],
      const std::vector<double> lengths_from_sources[2],
      const std::vector<std::vector<double>> resources_from_sources[2],
      const std::vector<int> first_label[2],
      const std::vector<int> num_labels[2], LabelPair& best_label_pair);

  // Returns the path as list of arc indices that starts from a node in
  // `sources` (if `direction` iS FORWARD) or `destinations` (if `direction` is
  // BACKWARD) and ends in node represented by `best_label_index`.
  std::vector<ArcIndex> ArcPathTo(
      int best_label_index,
      absl::Span<const ArcIndex> incoming_arc_indices_from_sources,
      absl::Span<const int> incoming_label_indices_from_sources) const;

  // Returns the list of all the nodes implied by a given `arc_path`.
  std::vector<NodeIndex> NodePathImpliedBy(absl::Span<const ArcIndex> arc_path,
                                           const GraphType& graph) const;

  static constexpr double kTolerance = 1e-6;

  const GraphType* const graph_;
  const std::vector<double>* const arc_lengths_;
  const std::vector<std::vector<double>>* const arc_resources_;
  const std::vector<double>* const max_resources_;
  absl::Span<const NodeIndex> sources_;
  absl::Span<const NodeIndex> destinations_;
  const int num_resources_;

  // Data about *reachable* sub-graphs split in two for bidirectional search.
  // Reachable nodes are nodes that can be reached given the resources
  // constraints, i.e., for each resource, the sum of the minimum resource to
  // get to a node from a node in `sources` and to get from a node to a node in
  // `destinations` should be less than the maximum resource. Reachable arcs are
  // arcs linking reachable nodes.
  //
  // `sub_reverse_graph_[dir]` is the reachable sub-graph split in *half* with
  // an additional linked to sources (resp. destinations) for the forward (resp.
  // backward) direction. For the forward (resp. backward) direction, nodes are
  // indexed using the original (resp. reverse) topological order.
  GraphType sub_reverse_graph_[2];
  std::vector<std::vector<double>> sub_arc_resources_[2];
  // `sub_full_arc_indices_[dir]` has size `sub_reverse_graph_[dir].num_arcs()`
  // such that `sub_full_arc_indices_[dir][sub_arc] = arc` where `sub_arc` is
  // the arc in the reachable sub-graph for direction `dir` (i.e.
  // `sub_reverse_graph[dir]`) and `arc` is the arc in the original graph (i.e.
  // `graph`).
  std::vector<NodeIndex> sub_full_arc_indices_[2];
  // `sub_node_indices_[dir]` has size `graph->num_nodes()` such that
  // `sub_node_indices[dir][node] = sub_node` where `node` is the node in the
  // original graph (i.e. `graph`) and `sub_node` is the node in the reachable
  // sub-graph for direction `dir` (i.e. `sub_reverse_graph[dir]`) and -1 if
  // `node` is not present in reachable sub-graph.
  std::vector<NodeIndex> sub_node_indices_[2];
  // `sub_is_source_[dir][sub_dir]` has size
  // `sub_reverse_graph_[dir].num_nodes()` such that
  // `sub_is_source_[dir][sub_dir][sub_node]` is true if `sub_node` is a node in
  // the reachable sub-graph for direction `dir` (i.e. `sub_reverse_graph[dir]`)
  // which is a source (resp. destination) is `sub_dir` is FORWARD (resp.
  // BACKWARD).
  std::vector<bool> sub_is_source_[2][2];
  // `sub_min_arc_resources_[dir]` has size `max_resources->size()` and
  // `sub_min_arc_resources_[dir][r]`, `sub_reverse_graph_[dir].num_nodes()`
  // such that `sub_min_arc_resources_[dir][r][sub_node]` is the minimum of
  // resource r needed to get to a destination (resp. come from a source) if
  // `dir` is FORWARD (resp. BACKWARD).
  std::vector<std::vector<double>> sub_min_arc_resources_[2];
  // Maximum number of labels created for each sub-graph.
  int max_num_created_labels_[2];

  // Data about the last call of the RunConstrainedShortestPathOnDag()
  // function. A path is only added to the following vectors if and only if
  // it is feasible with respect to all resources.
  // A Label includes the cumulative length, resources and the previous arc used
  // in the path to get to this node.
  // Instead of having a single vector of `Label` objects (cl/590819865), we
  // split them into 3 vectors of more fundamental types as this improves
  // push_back operations and memory release.
  std::vector<double> lengths_from_sources_[2];
  std::vector<std::vector<double>> resources_from_sources_[2];
  std::vector<ArcIndex> incoming_arc_indices_from_sources_[2];
  std::vector<int> incoming_label_indices_from_sources_[2];
  std::vector<int> node_first_label_[2];
  std::vector<int> node_num_labels_[2];
};

std::vector<int> GetInversePermutation(absl::Span<const int> permutation);

// -----------------------------------------------------------------------------
// Implementation.
// -----------------------------------------------------------------------------

template <class GraphType>
#if __cplusplus >= 202002L
  requires DagGraphType<GraphType>
#endif
ConstrainedShortestPathsOnDagWrapper<GraphType>::
    ConstrainedShortestPathsOnDagWrapper(
        const GraphType* graph, const std::vector<double>* arc_lengths,
        const std::vector<std::vector<double>>* arc_resources,
        absl::Span<const NodeIndex> topological_order,
        absl::Span<const NodeIndex> sources,
        absl::Span<const NodeIndex> destinations,
        const std::vector<double>* max_resources, int max_num_created_labels)
    : graph_(graph),
      arc_lengths_(arc_lengths),
      arc_resources_(arc_resources),
      max_resources_(max_resources),
      sources_(sources),
      destinations_(destinations),
      num_resources_(max_resources->size()) {
  CHECK(graph_ != nullptr);
  CHECK(arc_lengths_ != nullptr);
  CHECK(arc_resources_ != nullptr);
  CHECK(!sources_.empty());
  CHECK(!destinations_.empty());
  CHECK(max_resources_ != nullptr);
  CHECK(!max_resources_->empty())
      << "max_resources cannot be empty. Use "
         "ortools/graph/dag_shortest_path.h instead";
  if (DEBUG_MODE) {
    CHECK_EQ(arc_lengths->size(), graph->num_arcs());
    CHECK_EQ(arc_resources->size(), max_resources->size());
    for (absl::Span<const double> arcs_resource : *arc_resources) {
      CHECK_EQ(arcs_resource.size(), graph->num_arcs());
      for (const double arc_resource : arcs_resource) {
        CHECK(arc_resource >= 0 &&
              arc_resource != std::numeric_limits<double>::infinity() &&
              !std::isnan(arc_resource))
            << absl::StrFormat("resource cannot be negative nor +inf nor NaN");
      }
    }
    for (const double arc_length : *arc_lengths) {
      CHECK(arc_length != -std::numeric_limits<double>::infinity() &&
            !std::isnan(arc_length))
          << absl::StrFormat("length cannot be -inf nor NaN");
    }
    CHECK_OK(TopologicalOrderIsValid(*graph, topological_order))
        << "Invalid topological order";
    for (const double max_resource : *max_resources) {
      CHECK(max_resource >= 0 &&
            max_resource != std::numeric_limits<double>::infinity() &&
            !std::isnan(max_resource))
          << absl::StrFormat(
                 "max_resource cannot be negative not +inf nor NaN");
    }
    std::vector<bool> is_source(graph->num_nodes(), false);
    for (const NodeIndex source : sources) {
      is_source[source] = true;
    }
    for (const NodeIndex destination : destinations) {
      CHECK(!is_source[destination])
          << "A node cannot be both a source and destination";
    }
  }

  // Full graphs.
  const GraphType* full_graph[2];
  const std::vector<std::vector<double>>* full_arc_resources[2];
  absl::Span<const NodeIndex> full_topological_order[2];
  absl::Span<const NodeIndex> full_sources[2];
  // Forward.
  const int num_nodes = graph->num_nodes();
  const int num_arcs = graph->num_arcs();
  full_graph[FORWARD] = graph;
  full_arc_resources[FORWARD] = arc_resources;
  full_topological_order[FORWARD] = topological_order;
  full_sources[FORWARD] = sources;
  // Backward.
  GraphType full_backward_graph(num_nodes, num_arcs);
  for (ArcIndex arc_index = 0; arc_index < num_arcs; ++arc_index) {
    full_backward_graph.AddArc(graph->Head(arc_index), graph->Tail(arc_index));
  }
  std::vector<ArcIndex> full_permutation;
  full_backward_graph.Build(&full_permutation);
  const std::vector<ArcIndex> full_inverse_arc_indices =
      GetInversePermutation(full_permutation);
  std::vector<std::vector<double>> backward_arc_resources(num_resources_);
  for (int r = 0; r < num_resources_; ++r) {
    backward_arc_resources[r] = (*arc_resources)[r];
    util::Permute(full_permutation, &backward_arc_resources[r]);
  }
  std::vector<NodeIndex> full_backward_topological_order;
  full_backward_topological_order.reserve(num_nodes);
  for (int i = num_nodes - 1; i >= 0; --i) {
    full_backward_topological_order.push_back(topological_order[i]);
  }
  full_graph[BACKWARD] = &full_backward_graph;
  full_arc_resources[BACKWARD] = &backward_arc_resources;
  full_topological_order[BACKWARD] = full_backward_topological_order;
  full_sources[BACKWARD] = destinations;

  // Get the minimum resources sources -> node and node -> destination for each
  // node.
  std::vector<std::vector<double>> full_min_arc_resources[2];
  for (const Direction dir : {FORWARD, BACKWARD}) {
    full_min_arc_resources[dir].reserve(num_resources_);
    std::vector<double> full_arc_resource = full_arc_resources[dir]->front();
    ShortestPathsOnDagWrapper<GraphType> shortest_paths_on_dag(
        full_graph[dir], &full_arc_resource, full_topological_order[dir]);
    for (int r = 0; r < num_resources_; ++r) {
      full_arc_resource = (*(full_arc_resources[dir]))[r];
      shortest_paths_on_dag.RunShortestPathOnDag(full_sources[dir]);
      full_min_arc_resources[dir].push_back(shortest_paths_on_dag.LengthTo());
    }
  }

  // Get reachable subgraph.
  std::vector<bool> is_reachable(num_nodes, true);
  std::vector<NodeIndex> sub_topological_order;
  sub_topological_order.reserve(num_nodes);
  for (const NodeIndex node_index : topological_order) {
    for (int r = 0; r < num_resources_; ++r) {
      if (full_min_arc_resources[FORWARD][r][node_index] +
              full_min_arc_resources[BACKWARD][r][node_index] >
          (*max_resources)[r]) {
        is_reachable[node_index] = false;
        break;
      }
    }
    if (is_reachable[node_index]) {
      sub_topological_order.push_back(node_index);
    }
  }
  const int reachable_node_count = sub_topological_order.size();

  // We split the number of labels evenly between each search (+1 for the
  // additional source node).
  max_num_created_labels_[BACKWARD] = max_num_created_labels / 2 + 1;
  max_num_created_labels_[FORWARD] =
      max_num_created_labels - max_num_created_labels / 2 + 1;

  // Split sub-graphs and related information.
  // The split is based on the number of paths. This is used as a simple proxy
  // for the number of labels.
  int mid_index = 0;
  {
    // We use double to avoid overflow. Note that this is an heuristic, so we
    // don't care too much if we are not precise enough.
    std::vector<double> path_count[2];
    for (const Direction dir : {FORWARD, BACKWARD}) {
      const GraphType& reverse_full_graph = *(full_graph[Reverse(dir)]);
      path_count[dir].resize(num_nodes);
      for (const NodeIndex source : full_sources[dir]) {
        ++path_count[dir][source];
      }
      for (const NodeIndex to : full_topological_order[dir]) {
        if (!is_reachable[to]) continue;
        for (const ArcIndex arc : reverse_full_graph.OutgoingArcs(to)) {
          const NodeIndex from = reverse_full_graph.Head(arc);
          if (!is_reachable[from]) continue;
          path_count[dir][to] += path_count[dir][from];
        }
      }
    }
    for (const NodeIndex node_index : sub_topological_order) {
      if (path_count[FORWARD][node_index] > path_count[BACKWARD][node_index]) {
        break;
      }
      ++mid_index;
    }
    if (mid_index == reachable_node_count) {
      mid_index = reachable_node_count / 2;
    }
  }

  for (const Direction dir : {FORWARD, BACKWARD}) {
    absl::Span<const NodeIndex> const sub_nodes =
        dir == FORWARD
            ? absl::MakeSpan(sub_topological_order).subspan(0, mid_index)
            : absl::MakeSpan(sub_topological_order)
                  .subspan(mid_index, reachable_node_count - mid_index);
    sub_node_indices_[dir].assign(num_nodes, -1);
    sub_min_arc_resources_[dir].resize(num_resources_);
    for (int r = 0; r < num_resources_; ++r) {
      sub_min_arc_resources_[dir][r].resize(sub_nodes.size());
    }
    for (NodeIndex i = 0; i < sub_nodes.size(); ++i) {
      const NodeIndex sub_node_index =
          dir == FORWARD ? i : sub_nodes.size() - 1 - i;
      sub_node_indices_[dir][sub_nodes[i]] = sub_node_index;
      for (int r = 0; r < num_resources_; ++r) {
        sub_min_arc_resources_[dir][r][sub_node_index] =
            full_min_arc_resources[Reverse(dir)][r][sub_nodes[i]];
      }
    }
    // IMPORTANT: The sub-graph has an additional node linked to sources (resp.
    // destinations) for the forward (resp. backward) direction. This additional
    // node is indexed with the last index. All added arcs are given to have an
    // arc index in the original graph of -1.
    const int sub_arcs_count = num_arcs + full_sources[dir].size();
    sub_reverse_graph_[dir] = GraphType(sub_nodes.size() + 1, sub_arcs_count);
    sub_arc_resources_[dir].resize(num_resources_);
    for (int r = 0; r < num_resources_; ++r) {
      sub_arc_resources_[dir][r].reserve(sub_arcs_count);
    }
    sub_full_arc_indices_[dir].reserve(sub_arcs_count);
    const GraphType& reverse_full_graph = *(full_graph[Reverse(dir)]);
    for (ArcIndex arc_index = 0; arc_index < num_arcs; ++arc_index) {
      const NodeIndex from =
          sub_node_indices_[dir][reverse_full_graph.Tail(arc_index)];
      const NodeIndex to =
          sub_node_indices_[dir][reverse_full_graph.Head(arc_index)];
      if (from == -1 || to == -1) {
        continue;
      }
      sub_reverse_graph_[dir].AddArc(from, to);
      ArcIndex sub_full_arc_index;
      if (dir == FORWARD && !full_inverse_arc_indices.empty()) {
        sub_full_arc_index = full_inverse_arc_indices[arc_index];
      } else {
        sub_full_arc_index = arc_index;
      }
      for (int r = 0; r < num_resources_; ++r) {
        sub_arc_resources_[dir][r].push_back(
            (*arc_resources_)[r][sub_full_arc_index]);
      }
      sub_full_arc_indices_[dir].push_back(sub_full_arc_index);
    }
    for (const NodeIndex source : full_sources[dir]) {
      const NodeIndex sub_source = sub_node_indices_[dir][source];
      if (sub_source == -1) {
        continue;
      }
      sub_reverse_graph_[dir].AddArc(sub_source, sub_nodes.size());
      for (int r = 0; r < num_resources_; ++r) {
        sub_arc_resources_[dir][r].push_back(0.0);
      }
      sub_full_arc_indices_[dir].push_back(-1);
    }
    std::vector<ArcIndex> sub_permutation;
    sub_reverse_graph_[dir].Build(&sub_permutation);
    for (int r = 0; r < num_resources_; ++r) {
      util::Permute(sub_permutation, &sub_arc_resources_[dir][r]);
    }
    util::Permute(sub_permutation, &sub_full_arc_indices_[dir]);
  }

  // Memory allocation is done here and only once in order to avoid
  // reallocation at each call of `RunConstrainedShortestPathOnDag()` for
  // better performance.
  for (const Direction dir : {FORWARD, BACKWARD}) {
    resources_from_sources_[dir].resize(num_resources_);
    node_first_label_[dir].resize(sub_reverse_graph_[dir].size());
    node_num_labels_[dir].resize(sub_reverse_graph_[dir].size());
  }
}

template <class GraphType>
#if __cplusplus >= 202002L
  requires DagGraphType<GraphType>
#endif
PathWithLength ConstrainedShortestPathsOnDagWrapper<
    GraphType>::RunConstrainedShortestPathOnDag() {
  // Assign lengths on sub-relevant graphs.
  std::vector<double> sub_arc_lengths[2];
  for (const Direction dir : {FORWARD, BACKWARD}) {
    sub_arc_lengths[dir].reserve(sub_reverse_graph_[dir].num_arcs());
    for (ArcIndex sub_arc_index = 0;
         sub_arc_index < sub_reverse_graph_[dir].num_arcs(); ++sub_arc_index) {
      const ArcIndex arc_index = sub_full_arc_indices_[dir][sub_arc_index];
      if (arc_index == -1) {
        sub_arc_lengths[dir].push_back(0.0);
        continue;
      }
      sub_arc_lengths[dir].push_back((*arc_lengths_)[arc_index]);
    }
  }

  {
    ThreadPool search_threads(2);
    search_threads.StartWorkers();
    for (const Direction dir : {FORWARD, BACKWARD}) {
      search_threads.Schedule([this, dir, &sub_arc_lengths]() {
        RunHalfConstrainedShortestPathOnDag(
            /*reverse_graph=*/sub_reverse_graph_[dir],
            /*arc_lengths=*/sub_arc_lengths[dir],
            /*arc_resources=*/sub_arc_resources_[dir],
            /*min_arc_resources=*/sub_min_arc_resources_[dir],
            /*max_resources=*/*max_resources_,
            /*max_num_created_labels=*/max_num_created_labels_[dir],
            /*lengths_from_sources=*/lengths_from_sources_[dir],
            /*resources_from_sources=*/resources_from_sources_[dir],
            /*incoming_arc_indices_from_sources=*/
            incoming_arc_indices_from_sources_[dir],
            /*incoming_label_indices_from_sources=*/
            incoming_label_indices_from_sources_[dir],
            /*first_label=*/node_first_label_[dir],
            /*num_labels=*/node_num_labels_[dir]);
      });
    }
  }

  // Check destinations within relevant half sub-graphs.
  LabelPair best_label_pair = {
      .length = std::numeric_limits<double>::infinity(),
      .label_index = {-1, -1}};
  for (const Direction dir : {FORWARD, BACKWARD}) {
    absl::Span<const NodeIndex> destinations =
        dir == FORWARD ? destinations_ : sources_;
    for (const NodeIndex dst : destinations) {
      const NodeIndex sub_dst = sub_node_indices_[dir][dst];
      if (sub_dst == -1) {
        continue;
      }
      const int num_labels_dst = node_num_labels_[dir][sub_dst];
      if (num_labels_dst == 0) {
        continue;
      }
      const int first_label_dst = node_first_label_[dir][sub_dst];
      for (int label_index = first_label_dst;
           label_index < first_label_dst + num_labels_dst; ++label_index) {
        const double length_dst = lengths_from_sources_[dir][label_index];
        if (length_dst < best_label_pair.length) {
          best_label_pair.length = length_dst;
          best_label_pair.label_index[dir] = label_index;
        }
      }
    }
  }

  const ArcIndex merging_arc_index = MergeHalfRuns(
      /*graph=*/*graph_, /*arc_lengths=*/*arc_lengths_,
      /*arc_resources=*/*arc_resources_,
      /*max_resources=*/*max_resources_,
      /*sub_node_indices=*/sub_node_indices_,
      /*lengths_from_sources=*/lengths_from_sources_,
      /*resources_from_sources=*/resources_from_sources_,
      /*first_label=*/node_first_label_,
      /*num_labels=*/node_num_labels_, /*best_label_pair=*/best_label_pair);

  std::vector<ArcIndex> arc_path;
  for (const Direction dir : {FORWARD, BACKWARD}) {
    for (const ArcIndex sub_arc_index : ArcPathTo(
             /*best_label_index=*/best_label_pair.label_index[dir],
             /*incoming_arc_indices_from_sources=*/
             incoming_arc_indices_from_sources_[dir],
             /*incoming_label_indices_from_sources=*/
             incoming_label_indices_from_sources_[dir])) {
      const ArcIndex arc_index = sub_full_arc_indices_[dir][sub_arc_index];
      if (arc_index == -1) {
        break;
      }
      arc_path.push_back(arc_index);
    }
    if (dir == FORWARD && merging_arc_index != -1) {
      absl::c_reverse(arc_path);
      arc_path.push_back(merging_arc_index);
    }
  }

  // Clear all labels from the next run.
  for (const Direction dir : {FORWARD, BACKWARD}) {
    lengths_from_sources_[dir].clear();
    for (int r = 0; r < num_resources_; ++r) {
      resources_from_sources_[dir][r].clear();
    }
    incoming_arc_indices_from_sources_[dir].clear();
    incoming_label_indices_from_sources_[dir].clear();
  }
  return {.length = best_label_pair.length,
          .arc_path = arc_path,
          .node_path = NodePathImpliedBy(arc_path, *graph_)};
}

template <class GraphType>
#if __cplusplus >= 202002L
  requires DagGraphType<GraphType>
#endif
void ConstrainedShortestPathsOnDagWrapper<GraphType>::
    RunHalfConstrainedShortestPathOnDag(
        const GraphType& reverse_graph, absl::Span<const double> arc_lengths,
        absl::Span<const std::vector<double>> arc_resources,
        absl::Span<const std::vector<double>> min_arc_resources,
        absl::Span<const double> max_resources,
        const int max_num_created_labels,
        std::vector<double>& lengths_from_sources,
        std::vector<std::vector<double>>& resources_from_sources,
        std::vector<ArcIndex>& incoming_arc_indices_from_sources,
        std::vector<int>& incoming_label_indices_from_sources,
        std::vector<int>& first_label, std::vector<int>& num_labels) {
  // Initialize source node.
  const NodeIndex source_node = reverse_graph.num_nodes() - 1;
  first_label[source_node] = 0;
  num_labels[source_node] = 1;
  lengths_from_sources.push_back(0);
  for (int r = 0; r < num_resources_; ++r) {
    resources_from_sources[r].push_back(0);
  }
  incoming_arc_indices_from_sources.push_back(-1);
  incoming_label_indices_from_sources.push_back(-1);

  std::vector<double> lengths_to;
  std::vector<std::vector<double>> resources_to(num_resources_);
  std::vector<ArcIndex> incoming_arc_indices_to;
  std::vector<int> incoming_label_indices_to;
  std::vector<int> label_indices_to;
  std::vector<double> resources(num_resources_);
  for (NodeIndex to = 0; to < source_node; ++to) {
    lengths_to.clear();
    for (int r = 0; r < num_resources_; ++r) {
      resources_to[r].clear();
    }
    incoming_arc_indices_to.clear();
    incoming_label_indices_to.clear();
    for (const ArcIndex reverse_arc_index : reverse_graph.OutgoingArcs(to)) {
      const NodeIndex from = reverse_graph.Head(reverse_arc_index);
      const double arc_length = arc_lengths[reverse_arc_index];
      DCHECK(arc_length != -std::numeric_limits<double>::infinity());
      if (arc_length == std::numeric_limits<double>::infinity()) {
        continue;
      }
      for (int label_index = first_label[from];
           label_index < first_label[from] + num_labels[from]; ++label_index) {
        bool path_is_feasible = true;
        for (int r = 0; r < num_resources_; ++r) {
          DCHECK_GE(arc_resources[r][reverse_arc_index], 0.0);
          resources[r] = resources_from_sources[r][label_index] +
                         arc_resources[r][reverse_arc_index];
          if (resources[r] + min_arc_resources[r][to] > max_resources[r]) {
            path_is_feasible = false;
            break;
          }
        }
        if (!path_is_feasible) {
          continue;
        }
        lengths_to.push_back(lengths_from_sources[label_index] + arc_length);
        for (int r = 0; r < num_resources_; ++r) {
          resources_to[r].push_back(resources[r]);
        }
        incoming_arc_indices_to.push_back(reverse_arc_index);
        incoming_label_indices_to.push_back(label_index);
      }
    }
    // Sort labels lexicographically with lengths then resources.
    label_indices_to.clear();
    label_indices_to.reserve(lengths_to.size());
    for (int i = 0; i < lengths_to.size(); ++i) {
      label_indices_to.push_back(i);
    }
    absl::c_sort(label_indices_to, [&](const int i, const int j) {
      if (lengths_to[i] < lengths_to[j]) return true;
      if (lengths_to[i] > lengths_to[j]) return false;
      for (int r = 0; r < num_resources_; ++r) {
        if (resources_to[r][i] < resources_to[r][j]) return true;
        if (resources_to[r][i] > resources_to[r][j]) return false;
      }
      return i < j;
    });

    first_label[to] = lengths_from_sources.size();
    int& num_labels_to = num_labels[to];
    // Reset the number of labels to zero otherwise it holds the previous run
    // result.
    num_labels_to = 0;
    for (int i = 0; i < label_indices_to.size(); ++i) {
      // Check if label "i" on node `to` is dominated by any other label.
      const int label_i_index = label_indices_to[i];
      bool label_i_is_dominated = false;
      for (int j = 0; j < i - 1; ++j) {
        const int label_j_index = label_indices_to[j];
        if (lengths_to[label_i_index] <= lengths_to[label_j_index]) continue;
        bool label_j_dominates_label_i = true;
        for (int r = 0; r < num_resources_; ++r) {
          if (resources_to[r][label_i_index] <=
              resources_to[r][label_j_index]) {
            label_j_dominates_label_i = false;
            break;
          }
        }
        if (label_j_dominates_label_i) {
          label_i_is_dominated = true;
          break;
        }
      }
      if (label_i_is_dominated) continue;
      lengths_from_sources.push_back(lengths_to[label_i_index]);
      for (int r = 0; r < num_resources_; ++r) {
        resources_from_sources[r].push_back(resources_to[r][label_i_index]);
      }
      incoming_arc_indices_from_sources.push_back(
          incoming_arc_indices_to[label_i_index]);
      incoming_label_indices_from_sources.push_back(
          incoming_label_indices_to[label_i_index]);
      ++num_labels_to;
      if (lengths_from_sources.size() >= max_num_created_labels) {
        return;
      }
    }
  }
}

template <class GraphType>
#if __cplusplus >= 202002L
  requires DagGraphType<GraphType>
#endif
typename GraphType::ArcIndex
ConstrainedShortestPathsOnDagWrapper<GraphType>::MergeHalfRuns(
    const GraphType& graph, absl::Span<const double> arc_lengths,
    absl::Span<const std::vector<double>> arc_resources,
    absl::Span<const double> max_resources,
    const std::vector<NodeIndex> sub_node_indices[2],
    const std::vector<double> lengths_from_sources[2],
    const std::vector<std::vector<double>> resources_from_sources[2],
    const std::vector<int> first_label[2], const std::vector<int> num_labels[2],
    LabelPair& best_label_pair) {
  const std::vector<NodeIndex>& forward_sub_node_indices =
      sub_node_indices[FORWARD];
  absl::Span<const double> forward_lengths = lengths_from_sources[FORWARD];
  const std::vector<std::vector<double>>& forward_resources =
      resources_from_sources[FORWARD];
  absl::Span<const int> forward_first_label = first_label[FORWARD];
  absl::Span<const int> forward_num_labels = num_labels[FORWARD];
  const std::vector<NodeIndex>& backward_sub_node_indices =
      sub_node_indices[BACKWARD];
  absl::Span<const double> backward_lengths = lengths_from_sources[BACKWARD];
  const std::vector<std::vector<double>>& backward_resources =
      resources_from_sources[BACKWARD];
  absl::Span<const int> backward_first_label = first_label[BACKWARD];
  absl::Span<const int> backward_num_labels = num_labels[BACKWARD];
  ArcIndex merging_arc_index = -1;
  for (ArcIndex arc_index = 0; arc_index < graph.num_arcs(); ++arc_index) {
    const NodeIndex sub_from = forward_sub_node_indices[graph.Tail(arc_index)];
    if (sub_from == -1) {
      continue;
    }
    const NodeIndex sub_to = backward_sub_node_indices[graph.Head(arc_index)];
    if (sub_to == -1) {
      continue;
    }
    const int num_labels_from = forward_num_labels[sub_from];
    if (num_labels_from == 0) {
      continue;
    }
    const int num_labels_to = backward_num_labels[sub_to];
    if (num_labels_to == 0) {
      continue;
    }
    const double arc_length = arc_lengths[arc_index];
    DCHECK(arc_length != -std::numeric_limits<double>::infinity());
    if (arc_length == std::numeric_limits<double>::infinity()) {
      continue;
    }
    const int first_label_from = forward_first_label[sub_from];
    const int first_label_to = backward_first_label[sub_to];
    for (int label_to_index = first_label_to;
         label_to_index < first_label_to + num_labels_to; ++label_to_index) {
      const double length_to = backward_lengths[label_to_index];
      if (arc_length + length_to >= best_label_pair.length) {
        continue;
      }
      for (int label_from_index = first_label_from;
           label_from_index < first_label_from + num_labels_from;
           ++label_from_index) {
        const double length_from = forward_lengths[label_from_index];
        if (length_from + arc_length + length_to >= best_label_pair.length) {
          continue;
        }
        bool path_is_feasible = true;
        for (int r = 0; r < num_resources_; ++r) {
          DCHECK_GE(arc_resources[r][arc_index], 0.0);
          if (forward_resources[r][label_from_index] +
                  arc_resources[r][arc_index] +
                  backward_resources[r][label_to_index] >
              max_resources[r]) {
            path_is_feasible = false;
            break;
          }
        }
        if (!path_is_feasible) {
          continue;
        }
        best_label_pair.length = length_from + arc_length + length_to;
        best_label_pair.label_index[FORWARD] = label_from_index;
        best_label_pair.label_index[BACKWARD] = label_to_index;
        merging_arc_index = arc_index;
      }
    }
  }
  return merging_arc_index;
}

template <class GraphType>
#if __cplusplus >= 202002L
  requires DagGraphType<GraphType>
#endif
std::vector<typename GraphType::ArcIndex>
ConstrainedShortestPathsOnDagWrapper<GraphType>::ArcPathTo(
    const int best_label_index,
    absl::Span<const ArcIndex> incoming_arc_indices_from_sources,
    absl::Span<const int> incoming_label_indices_from_sources) const {
  int current_label_index = best_label_index;
  std::vector<ArcIndex> arc_path;
  for (int i = 0; i < graph_->num_nodes(); ++i) {
    if (current_label_index == -1) {
      break;
    }
    arc_path.push_back(incoming_arc_indices_from_sources[current_label_index]);
    current_label_index =
        incoming_label_indices_from_sources[current_label_index];
  }
  return arc_path;
}

template <typename GraphType>
#if __cplusplus >= 202002L
  requires DagGraphType<GraphType>
#endif
std::vector<typename GraphType::NodeIndex>
ConstrainedShortestPathsOnDagWrapper<GraphType>::NodePathImpliedBy(
    absl::Span<const ArcIndex> arc_path, const GraphType& graph) const {
  if (arc_path.empty()) {
    return {};
  }
  std::vector<NodeIndex> node_path;
  node_path.reserve(arc_path.size() + 1);
  for (const ArcIndex arc_index : arc_path) {
    node_path.push_back(graph.Tail(arc_index));
  }
  node_path.push_back(graph.Head(arc_path.back()));
  return node_path;
}

}  // namespace operations_research

#endif  // OR_TOOLS_GRAPH_DAG_CONSTRAINED_SHORTEST_PATH_H_
