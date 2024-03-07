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

#ifndef OR_TOOLS_GRAPH_DAG_CONSTRAINED_SHORTEST_PATH_H_
#define OR_TOOLS_GRAPH_DAG_CONSTRAINED_SHORTEST_PATH_H_

#include <cmath>
#include <concepts>
#include <limits>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/base/log_severity.h"
#include "absl/log/check.h"
#include "absl/strings/str_format.h"
#include "absl/types/span.h"
#include "ortools/graph/dag_shortest_path.h"

namespace operations_research {

// This library provides APIs to compute the constrained shortest path (CSP) on
// a given directed acyclic graph (DAG) with resources on each arc. A CSP is a
// shortest path on a DAG which does not exceed a set of maximum resources
// consumption. The algorithm is exponential and has no guarantee to finish.
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
};
#endif

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
  // If the number of labels in memory exceeds `max_num_created_labels` at any
  // point in the algorithm, it returns the best path found so far, most
  // particularly the empty path if none were found.
  //
  // SUBTLE: You can modify the graph, the arc lengths and resources, the
  // topological order, sources, destinations or the maximum resource between
  // calls to the `RunConstrainedShortestPathOnDag()` function. That's fine.
  // Doing so will obviously invalidate the result API of the last constrained
  // shortest path run, which could return an upper bound, junk, or crash.
  ConstrainedShortestPathsOnDagWrapper(
      const GraphType* graph, const std::vector<double>* arc_lengths,
      const std::vector<std::vector<double>>* arc_resources,
      const std::vector<NodeIndex>* topological_order,
      const std::vector<NodeIndex>* sources,
      const std::vector<NodeIndex>* destinations,
      const std::vector<double>* max_resources,
      int max_num_created_labels = 1e9);

  // Returns {+inf, {}, {}} if there is no constrained path of finite length
  // from one node in `sources` to one node in `destinations`.
  PathWithLength RunConstrainedShortestPathOnDag();

 private:
  // Returns the list of all the arcs of the shortest path from one node in
  // `sources` ending by the arc from a given `label_index` if and only if
  // `label_index` is between 0 and `labels_from_sources_.size() - 1`.
  std::vector<ArcIndex> BestArcPathEndingWith(int label_index) const;
  // Returns the list of all the nodes implied by a given `arc_path`.
  std::vector<NodeIndex> NodePathImpliedBy(
      const std::vector<ArcIndex>& arc_path) const;

  const GraphType* const graph_;
  const std::vector<double>* const arc_lengths_;
  const std::vector<std::vector<double>>* const arc_resources_;
  const std::vector<NodeIndex>* const topological_order_;
  const std::vector<NodeIndex>* const sources_;
  const std::vector<NodeIndex>* const destinations_;
  const std::vector<double>* const max_resources_;
  int max_num_created_labels_;

  std::vector<bool> node_is_source_;
  std::vector<bool> node_is_destination_;
  // Using the fact that the graph is a DAG, we can disregard any node that
  // comes after the last destination (based on the topological order).
  std::vector<bool> node_is_after_last_destination_;

  // Data for reverse graph.
  GraphType reverse_graph_;
  std::vector<ArcIndex> reverse_inverse_arc_permutation_;

  // Data about the last call of the RunConstrainedShortestPathOnDag()
  // function. A Label includes the cumulative length, resources and the
  // previous arc used in the path to get to this node.
  struct Label {
    double length;
    // TODO(b/315786885): Optimize resources in Label struct.
    std::vector<double> resources;
    ArcIndex incoming_arc;
  };
  // A label is present in `labels_from_sources_` if and only if it is feasible
  // with respect to all resources.
  std::vector<Label> labels_from_sources_;
  std::vector<int> node_first_label_;
  std::vector<int> node_num_labels_;
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
        const std::vector<NodeIndex>* topological_order,
        const std::vector<NodeIndex>* sources,
        const std::vector<NodeIndex>* destinations,
        const std::vector<double>* max_resources, int max_num_created_labels)
    : graph_(graph),
      arc_lengths_(arc_lengths),
      arc_resources_(arc_resources),
      topological_order_(topological_order),
      sources_(sources),
      destinations_(destinations),
      max_resources_(max_resources),
      max_num_created_labels_(max_num_created_labels) {
  CHECK(graph_ != nullptr);
  CHECK(arc_lengths_ != nullptr);
  CHECK(arc_resources_ != nullptr);
  CHECK(topological_order_ != nullptr);
  CHECK(sources_ != nullptr);
  CHECK(destinations_ != nullptr);
  CHECK(max_resources_ != nullptr);
  if (DEBUG_MODE) {
    CHECK_EQ(arc_lengths_->size(), graph_->num_arcs());
    CHECK_EQ(arc_resources_->size(), max_resources_->size());
    for (const std::vector<double>& arcs_resource : *arc_resources_) {
      CHECK_EQ(arcs_resource.size(), graph_->num_arcs());
      for (const double arc_resource : arcs_resource) {
        CHECK(arc_resource >= 0 &&
              arc_resource != std::numeric_limits<double>::infinity() &&
              !std::isnan(arc_resource))
            << absl::StrFormat("resource cannot be negative nor +inf nor NaN");
      }
    }
    for (const double arc_length : *arc_lengths_) {
      CHECK(arc_length != -std::numeric_limits<double>::infinity() &&
            !std::isnan(arc_length))
          << absl::StrFormat("length cannot be -inf nor NaN");
    }
    CHECK_OK(TopologicalOrderIsValid(*graph_, *topological_order_))
        << "Invalid topological order";
    for (const double max_resource : *max_resources_) {
      CHECK(max_resource >= 0 &&
            max_resource != std::numeric_limits<double>::infinity() &&
            !std::isnan(max_resource))
          << absl::StrFormat(
                 "max_resource cannot be negative not +inf nor NaN");
    }
  }

  node_is_source_.resize(graph_->num_nodes());
  for (const NodeIndex source : *sources_) {
    node_is_source_[source] = true;
  }
  node_is_destination_.resize(graph_->num_nodes());
  for (const NodeIndex destination : *destinations_) {
    node_is_destination_[destination] = true;
  }
  node_is_after_last_destination_.resize(graph_->num_nodes());
  bool last_destination_found = false;
  for (int i = graph_->num_nodes() - 1; i >= 0; --i) {
    const NodeIndex node = (*topological_order_)[i];
    if (node_is_destination_[node]) {
      last_destination_found = true;
    }
    node_is_after_last_destination_[node] = !last_destination_found;
  }

  // Reverse graph.
  reverse_graph_ = GraphType(graph_->num_nodes(), graph_->num_arcs());
  for (ArcIndex arc_index = 0; arc_index < graph_->num_arcs(); ++arc_index) {
    reverse_graph_.AddArc(graph_->Head(arc_index), graph_->Tail(arc_index));
  }
  std::vector<ArcIndex> permutation;
  reverse_graph_.Build(&permutation);
  reverse_inverse_arc_permutation_ = GetInversePermutation(permutation);

  // Memory allocation is done here and only once in order to avoid reallocation
  // at each call of `RunConstrainedShortestPathOnDag()` for better performance.
  node_first_label_.resize(graph_->num_nodes());
  node_num_labels_.resize(graph_->num_nodes());
}

template <class GraphType>
#if __cplusplus >= 202002L
  requires DagGraphType<GraphType>
#endif
PathWithLength ConstrainedShortestPathsOnDagWrapper<
    GraphType>::RunConstrainedShortestPathOnDag() {
  // Caching the vector addresses allow to not fetch it on each access.
  const absl::Span<const double> arc_lengths = *arc_lengths_;
  const absl::Span<const double> max_resources = *max_resources_;

  // Clear all labels from previous run.
  labels_from_sources_.clear();

  double best_length = std::numeric_limits<double>::infinity();
  int best_label_index = -1;
  for (const NodeIndex to : *topological_order_) {
    node_first_label_[to] = labels_from_sources_.size();
    int& num_labels = node_num_labels_[to];
    num_labels = 0;
    if (node_is_after_last_destination_[to]) {
      break;
    }
    if (node_is_source_[to]) {
      labels_from_sources_.push_back(
          {.length = 0,
           .resources = std::vector<double>(max_resources.size()),
           .incoming_arc = -1});
      ++num_labels;
      if (labels_from_sources_.size() >= max_num_created_labels_) {
        const std::vector<ArcIndex> arc_path =
            BestArcPathEndingWith(best_label_index);
        return {.length = best_length,
                .arc_path = arc_path,
                .node_path = NodePathImpliedBy(arc_path)};
      }
    }
    for (const ArcIndex reverse_arc_index : reverse_graph_.OutgoingArcs(to)) {
      const NodeIndex from = reverse_graph_.Head(reverse_arc_index);
      const ArcIndex arc_index =
          reverse_inverse_arc_permutation_.empty()
              ? reverse_arc_index
              : reverse_inverse_arc_permutation_[reverse_arc_index];
      const double arc_length = arc_lengths[arc_index];
      DCHECK(arc_length != -std::numeric_limits<double>::infinity());
      if (arc_length == std::numeric_limits<double>::infinity()) {
        continue;
      }
      for (int i = node_first_label_[from];
           i < node_first_label_[from] + node_num_labels_[from]; ++i) {
        const Label& label_from = labels_from_sources_[i];
        bool path_is_feasible = true;
        for (int r = 0; r < max_resources.size(); ++r) {
          DCHECK_GE((*arc_resources_)[r][arc_index], 0.0);
          // TODO(b/314756645): Include lower bound on the resources to
          // increase pruning.
          if (label_from.resources[r] + (*arc_resources_)[r][arc_index] >
              max_resources[r]) {
            path_is_feasible = false;
            break;
          }
        }
        if (!path_is_feasible) {
          continue;
        }
        Label label_to = label_from;
        label_to.length += arc_length;
        for (int r = 0; r < max_resources.size(); ++r) {
          label_to.resources[r] += (*arc_resources_)[r][arc_index];
        }
        label_to.incoming_arc = arc_index;
        labels_from_sources_.push_back(label_to);
        ++num_labels;
        if (node_is_destination_[to]) {
          if (best_length > label_to.length) {
            best_length = label_to.length;
            best_label_index = labels_from_sources_.size() - 1;
          }
        }
        if (labels_from_sources_.size() >= max_num_created_labels_) {
          const std::vector<ArcIndex> arc_path =
              BestArcPathEndingWith(best_label_index);
          return {.length = best_length,
                  .arc_path = arc_path,
                  .node_path = NodePathImpliedBy(arc_path)};
        }
      }
    }
  }
  const std::vector<ArcIndex> arc_path =
      BestArcPathEndingWith(best_label_index);
  return {.length = best_length,
          .arc_path = arc_path,
          .node_path = NodePathImpliedBy(arc_path)};
}

template <typename GraphType>
#if __cplusplus >= 202002L
  requires DagGraphType<GraphType>
#endif
std::vector<typename GraphType::ArcIndex>
ConstrainedShortestPathsOnDagWrapper<GraphType>::BestArcPathEndingWith(
    const int label_index) const {
  if (label_index < 0 || label_index >= labels_from_sources_.size()) {
    return {};
  }
  int current_label_index = label_index;
  std::vector<ArcIndex> arc_path;
  while (labels_from_sources_[current_label_index].incoming_arc != -1) {
    const Label& label = labels_from_sources_[current_label_index];
    const NodeIndex node = graph_->Tail(label.incoming_arc);
    arc_path.push_back(label.incoming_arc);
    for (int i = node_first_label_[node];
         i < node_first_label_[node] + node_num_labels_[node]; ++i) {
      // Since all labels of `labels_from_sources_` are feasible, we can pick
      // any previous label which satisfies the length equality (irrespective of
      // the resources it consumes).
      if (labels_from_sources_[i].length +
              (*arc_lengths_)[label.incoming_arc] ==
          label.length) {
        current_label_index = i;
        break;
      }
    }
  }
  absl::c_reverse(arc_path);
  return arc_path;
}

template <typename GraphType>
#if __cplusplus >= 202002L
  requires DagGraphType<GraphType>
#endif
std::vector<typename GraphType::NodeIndex>
ConstrainedShortestPathsOnDagWrapper<GraphType>::NodePathImpliedBy(
    const std::vector<ArcIndex>& arc_path) const {
  if (arc_path.empty()) {
    return {};
  }
  std::vector<NodeIndex> node_path;
  node_path.reserve(arc_path.size() + 1);
  for (const ArcIndex arc_index : arc_path) {
    node_path.push_back(graph_->Tail(arc_index));
  }
  node_path.push_back(graph_->Head(arc_path.back()));
  return node_path;
}

}  // namespace operations_research

#endif  // OR_TOOLS_GRAPH_DAG_CONSTRAINED_SHORTEST_PATH_H_
