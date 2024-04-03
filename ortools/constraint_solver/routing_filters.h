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

#ifndef OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_FILTERS_H_
#define OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_FILTERS_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "ortools/constraint_solver/routing.h"
#include "ortools/constraint_solver/routing_lp_scheduling.h"
#include "ortools/constraint_solver/routing_types.h"
#include "ortools/routing/parameters.pb.h"
#include "ortools/util/bitset.h"

namespace operations_research {

/// Returns a filter ensuring that max active vehicles constraints are enforced.
IntVarLocalSearchFilter* MakeMaxActiveVehiclesFilter(
    const RoutingModel& routing_model);

/// Returns a filter ensuring that node disjunction constraints are enforced.
IntVarLocalSearchFilter* MakeNodeDisjunctionFilter(
    const RoutingModel& routing_model, bool filter_cost);

/// Returns a filter computing vehicle amortized costs.
IntVarLocalSearchFilter* MakeVehicleAmortizedCostFilter(
    const RoutingModel& routing_model);

/// Returns a filter ensuring type regulation constraints are enforced.
IntVarLocalSearchFilter* MakeTypeRegulationsFilter(
    const RoutingModel& routing_model);

/// Returns a filter enforcing pickup and delivery constraints for the given
/// pair of nodes and given policies.
IntVarLocalSearchFilter* MakePickupDeliveryFilter(
    const RoutingModel& routing_model,
    const std::vector<PickupDeliveryPair>& pairs,
    const std::vector<RoutingModel::PickupAndDeliveryPolicy>& vehicle_policies);

/// Returns a filter checking that vehicle variable domains are respected.
IntVarLocalSearchFilter* MakeVehicleVarFilter(
    const RoutingModel& routing_model);

/// Returns a filter handling dimension costs and constraints.
IntVarLocalSearchFilter* MakePathCumulFilter(const RoutingDimension& dimension,
                                             bool propagate_own_objective_value,
                                             bool filter_objective_cost,
                                             bool can_use_lp);

/// Returns a filter handling dimension cumul bounds.
IntVarLocalSearchFilter* MakeCumulBoundsPropagatorFilter(
    const RoutingDimension& dimension);

/// Returns a filter checking global linear constraints and costs.
IntVarLocalSearchFilter* MakeGlobalLPCumulFilter(
    GlobalDimensionCumulOptimizer* optimizer,
    GlobalDimensionCumulOptimizer* mp_optimizer, bool filter_objective_cost);

/// Returns a filter checking the feasibility and cost of the resource
/// assignment.
LocalSearchFilter* MakeResourceAssignmentFilter(
    LocalDimensionCumulOptimizer* optimizer,
    LocalDimensionCumulOptimizer* mp_optimizer,
    bool propagate_own_objective_value, bool filter_objective_cost);

/// Returns a filter checking the current solution using CP propagation.
IntVarLocalSearchFilter* MakeCPFeasibilityFilter(RoutingModel* routing_model);

// This class allows making fast range queries on sequences of elements.
// * Main characteristics.
// - queries on sequences of elements {height, weight},
//   parametrized by (begin, end, T), returning
//   sum_{i \in [begin, end), S[i].height >= T} S[i].weight
// - O(log (#different heights)) time complexity thanks to an underlying
//   wavelet tree (https://en.wikipedia.org/wiki/Wavelet_Tree)
// - holds several sequences at once, can be cleared while still keeping
//   allocated memory to avoid allocations.
// More details on these points follow.
//
// * Query complexity.
// The time complexity of a query in S is O(log H), where H is the number of
// different heights appearing in S.
// The particular implementation guarantees that queries that are trivial in
// the .height dimension, that is if threshold_height is <= or >= all heights
// in the range, are O(1).
//
// * Initialization complexity.
// The time complexity of filling the underlying data structures,
// which is done by running MakeTreeFromNewElements(),
// is O(N log N) where N is the number of new elements.
// The space complexity is a O(N log H).
//
// * Usage.
// Given Histogram holding elements with fields {.height, .weight},
// Histogram hist1 {{2, 3}, {1, 4}, {4, 1}, {2, 2}, {3, 1}, {0, 4}};
// Histogram hist2 {{-2, -3}, {-1, -4}, {-4, -1}, {-2, -2}};
// WeightedWaveletTree tree;
//
// for (const auto [height, weight] : hist1]) {
//   tree.PushBack(height, weight);
// }
// const int begin1 = tree.TreeSize();
// tree.MakeTreeFromNewElements();
// const int end1 = tree.TreeSize();
// const int begin2 = tree.TreeSize();  // begin2 == end1.
// for (const auto [height, weight] : hist2]) {
//   tree.PushBack(height, weight);
// }
// tree.MakeTreeFromNewElements();
// const int end2 = tree.TreeSize();
//
// // Sum of weights on whole first sequence, == 3 + 4 + 1 + 2 + 1 + 4
// tree.RangeSumWithThreshold(/*threshold=*/0, /*begin=*/begin1, /*end=*/end1);
// // Sum of weights on whole second sequence, all heights are negative,
// // so the result is 0.
// tree.RangeSumWithThreshold(/*threshold=*/0, /*begin=*/begin2, /*end=*/end2);
// // This is forbidden, because the range overlaps two sequences.
// tree.RangeSumWithThreshold(/*threshold=*/0, /*begin=*/2, /*end=*/10);
// // Returns 2 = 0 + 1 + 0 + 1.
// tree.RangeSumWithThreshold(/*threshold=*/3, /*begin=*/1, /*end=*/5);
// // Returns -6 = -4 + 0 + -2.
// tree.RangeSumWithThreshold(/*threshold=*/-2, /*begin=*/1, /*end=*/4);
// // Add another sequence.
// Histogram hist3 {{1, 1}, {3, 4}};
// const int begin3 = tree.TreeSize();
// for (const auto [height, weight] : hist3) {
//   tree.PushBack(height, weight);
// }
// tree.MakeTreeFromNewElements();
// const int end3 = tree.TreeSize();
// // Returns 4 = 0 + 4.
// tree.RangeSumWithThreshold(/*threshold=*/2, /*begin=*/begin3, /*end=*/end3);
// // Clear the tree, this invalidates all range queries.
// tree.Clear();
// // Forbidden!
// tree.RangeSumWithThreshold(/*threshold=*/2, /*begin=*/begin3, /*end=*/end3);
//
// * Implementation.
// This data structure uses two main techniques of the wavelet tree:
// - a binary search tree in the height dimension.
// - nodes only hold information about elements in their height range,
//   keeping selected elements in the same order as the full sequence,
//   and can map the index of its elements to their left and right child.
// The layout of the tree is packed by separating the tree navigation
// information from the (prefix sum + mapping) information.
// Here is how the tree for heights 6 4 1 3 6 1 7 4 2 is laid out in memory:
// tree_layers_         // nodes_
// 6 4 1 3 6 1 7 4 2    //        4
// 1 3 1 2|6 4 6 7 4    //    2       6
// 1 1|3 2|4 4|6 6 7    //  _   3   _   7
// _ _|2|3|_ _|6 6|7    // Dummy information is used to pad holes in nodes_.
// In addition to the mapping information of each element, each node holds
// the prefix sum of weights up to each element, to be able to compute the sum
// of S[i].weight of elements in its height range, for any range, in O(1).
// The data structure does not actually need height information inside the tree
// nodes, and does not store them.
class WeightedWaveletTree {
 public:
  WeightedWaveletTree() {}

  // Clears all trees, which invalidates all further range queries on currently
  // existing trees. This does *not* release memory held by this object.
  void Clear();

  // Returns the total number of elements in trees.
  int TreeSize() const { return tree_location_.size(); }

  // Adds an element at index this->Size().
  void PushBack(int64_t height, int64_t weight) {
    elements_.push_back({.height = height, .weight = weight});
  }

  // Generates the wavelet tree for all new elements, i.e. elements that were
  // added with PushBack() since the latest of these events: construction of
  // this object, a previous call to MakeTreeFromNewElements(), or a call to
  // Clear().
  // The range of new elements [begin, end), with begin the Size() at the
  // latest event, and end the current Size().
  void MakeTreeFromNewElements();

  // Returns sum_{begin_index <= i < end_index,
  //              S[i].height >= threshold_height} S[i].weight.
  // The range [begin_index, end_index) can only cover elements that were new
  // at the same call to MakeTreeFromNewElements().
  // When calling this method, there must be no pending new elements,
  // i.e. the last method called must not have been PushBack() or TreeSize().
  int64_t RangeSumWithThreshold(int64_t threshold_height, int begin_index,
                                int end_index) const;

 private:
  // Internal copy of an element.
  struct Element {
    int64_t height;
    int64_t weight;
  };
  // Elements are stored in a vector, they are only used during the
  // initialization of the data structure.
  std::vector<Element> elements_;

  // Maps the index of an element to the location of its tree.
  // Elements of the same sequence have the same TreeLocation value.
  struct TreeLocation {
    int node_begin;  // index of the first node in the tree in nodes_.
    int node_end;    // index of the last node in the tree in nodes_, plus 1.
    int sequence_first;  // index of the first element in all layers.
  };
  std::vector<TreeLocation> tree_location_;

  // A node of the tree is represented by the height of its pivot element and
  // the index of its pivot in the layer below, or -1 if the node is a leaf.
  struct Node {
    int64_t pivot_height;
    int pivot_index;
    bool operator<(const Node& other) const {
      return pivot_height < other.pivot_height;
    }
    bool operator==(const Node& other) const {
      return pivot_height == other.pivot_height;
    }
  };
  std::vector<Node> nodes_;

  // Holds range sum query and mapping information of each element
  // in each layer.
  // - prefix_sum: sum of weights in this node up to this element, included.
  // - left_index: number of elements in the same layer that are either:
  //   - in a node on the left of this node, or
  //   - in the same node, preceding this element, mapped to the left subtree.
  //   Coincides with this element's index in the left subtree if is_left = 1.
  // - is_left: 1 if the element is in the left subtree, otherwise 0.
  struct ElementInfo {
    int64_t prefix_sum;
    int left_index : 31;
    unsigned int is_left : 1;
  };
  // Contains range sum query and mapping data of all elements in their
  // respective tree, arranged by layer (depth) in the tree.
  // Layer 0 has root data, layer 1 has information of the left child
  // then the right child, layer 2 has left-left, left-right, right-left,
  // then right-right, etc.
  // Trees are stored consecutively, e.g. in each layer, the tree resulting
  // from the second MakeTreeFromNewElements() has its root information
  // after that of the tree resulting from the first MakeTreeFromNewElements().
  // If a node does not exist, some padding is stored instead.
  // Padding allows all layers to store the same number of element information,
  // which is one ElementInfo per element of the original sequence.
  // The values necessary to navigate the tree are stored in a separate
  // structure, in tree_location_ and nodes_.
  std::vector<std::vector<ElementInfo>> tree_layers_;

  // Represents a range of elements inside a node of a wavelet tree.
  // Also provides methods to compute the range sum query corresponding to
  // the range, and to project the range to left and right children.
  struct ElementRange {
    int range_first_index;
    int range_last_index;  // Last element of the range, inclusive.
    // True when the first element of this range is the first element of the
    // node. This is tracked to avoid out-of-bounds indices when computing range
    // sum queries from prefix sums.
    bool range_first_is_node_first;

    bool Empty() const { return range_first_index > range_last_index; }

    int64_t Sum(const ElementInfo* elements) const {
      return elements[range_last_index].prefix_sum -
             (range_first_is_node_first
                  ? 0
                  : elements[range_first_index - 1].prefix_sum);
    }

    ElementRange RightSubRange(const ElementInfo* els, int pivot_index) const {
      ElementRange right = {
          .range_first_index =
              pivot_index +
              (range_first_index - els[range_first_index].left_index),
          .range_last_index =
              pivot_index +
              (range_last_index - els[range_last_index].left_index) -
              els[range_last_index].is_left,
          .range_first_is_node_first = false};
      right.range_first_is_node_first = right.range_first_index == pivot_index;
      return right;
    }

    ElementRange LeftSubRange(const ElementInfo* els) const {
      return {.range_first_index = els[range_first_index].left_index,
              .range_last_index = els[range_last_index].left_index -
                                  !els[range_last_index].is_left,
              .range_first_is_node_first = range_first_is_node_first};
    }
  };
};

class PathEnergyCostChecker {
 public:
  struct EnergyCost {
    int64_t threshold;
    int64_t cost_per_unit_below_threshold;
    int64_t cost_per_unit_above_threshold;
    bool IsNull() const {
      return (cost_per_unit_below_threshold == 0 || threshold == 0) &&
             (cost_per_unit_above_threshold == 0 || threshold == kint64max);
    }
  };
  PathEnergyCostChecker(
      const PathState* path_state, std::vector<int64_t> force_start_min,
      std::vector<int64_t> force_end_min, std::vector<int> force_class,
      std::vector<const std::function<int64_t(int64_t)>*> force_per_class,
      std::vector<int> distance_class,
      std::vector<const std::function<int64_t(int64_t, int64_t)>*>
          distance_per_class,
      std::vector<EnergyCost> path_energy_cost,
      std::vector<bool> path_has_cost_when_empty);
  bool Check();
  void Commit();
  int64_t CommittedCost() const { return committed_total_cost_; }
  int64_t AcceptedCost() const { return accepted_total_cost_; }

 private:
  int64_t ComputePathCost(int64_t path) const;
  const PathState* const path_state_;
  const std::vector<int64_t> force_start_min_;
  const std::vector<int64_t> force_end_min_;
  const std::vector<int> force_class_;
  const std::vector<int> distance_class_;
  const std::vector<const std::function<int64_t(int64_t)>*> force_per_class_;
  const std::vector<const std::function<int64_t(int64_t, int64_t)>*>
      distance_per_class_;
  const std::vector<EnergyCost> path_energy_cost_;
  const std::vector<bool> path_has_cost_when_empty_;

  // Incremental cost computation.
  int64_t committed_total_cost_;
  int64_t accepted_total_cost_;
  std::vector<int64_t> committed_path_cost_;
};

LocalSearchFilter* MakePathEnergyCostFilter(
    Solver* solver, std::unique_ptr<PathEnergyCostChecker> checker,
    absl::string_view dimension_name);

/// Appends dimension-based filters to the given list of filters using a path
/// state.
void AppendLightWeightDimensionFilters(
    const PathState* path_state,
    const std::vector<RoutingDimension*>& dimensions,
    std::vector<LocalSearchFilterManager::FilterEvent>* filters);

void AppendDimensionCumulFilters(
    const std::vector<RoutingDimension*>& dimensions,
    const RoutingSearchParameters& parameters, bool filter_objective_cost,
    bool use_chain_cumul_filter,
    std::vector<LocalSearchFilterManager::FilterEvent>* filters);

/// Generic path-based filter class.

class BasePathFilter : public IntVarLocalSearchFilter {
 public:
  BasePathFilter(const std::vector<IntVar*>& nexts, int next_domain_size,
                 const PathsMetadata& paths_metadata);
  ~BasePathFilter() override {}
  bool Accept(const Assignment* delta, const Assignment* deltadelta,
              int64_t objective_min, int64_t objective_max) override;
  void OnSynchronize(const Assignment* delta) override;

 protected:
  static const int64_t kUnassigned;

  int64_t GetNext(int64_t node) const {
    return (new_nexts_[node] == kUnassigned)
               ? (IsVarSynced(node) ? Value(node) : kUnassigned)
               : new_nexts_[node];
  }
  bool HasAnySyncedPath() const {
    for (int64_t start : paths_metadata_.Starts()) {
      if (IsVarSynced(start)) return true;
    }
    return false;
  }
  int NumPaths() const { return paths_metadata_.NumPaths(); }
  int64_t Start(int i) const { return paths_metadata_.Start(i); }
  int64_t End(int i) const { return paths_metadata_.End(i); }
  int GetPath(int64_t node) const { return paths_metadata_.GetPath(node); }
  int Rank(int64_t node) const { return ranks_[node]; }
  bool IsDisabled() const { return status_ == DISABLED; }
  const std::vector<int64_t>& GetTouchedPathStarts() const {
    return touched_paths_.PositionsSetAtLeastOnce();
  }
  bool PathStartTouched(int64_t start) const { return touched_paths_[start]; }
  const std::vector<int64_t>& GetNewSynchronizedUnperformedNodes() const {
    return new_synchronized_unperformed_nodes_.PositionsSetAtLeastOnce();
  }

  bool lns_detected() const { return lns_detected_; }

 private:
  enum Status { UNKNOWN, ENABLED, DISABLED };

  virtual bool DisableFiltering() const { return false; }
  virtual void OnBeforeSynchronizePaths() {}
  virtual void OnAfterSynchronizePaths() {}
  virtual void OnSynchronizePathFromStart(int64_t) {}
  virtual bool InitializeAcceptPath() { return true; }
  virtual bool AcceptPath(int64_t path_start, int64_t chain_start,
                          int64_t chain_end) = 0;
  virtual bool FinalizeAcceptPath(int64_t, int64_t) { return true; }
  /// Detects path starts, used to track which node belongs to which path.
  void ComputePathStarts(std::vector<int64_t>* path_starts,
                         std::vector<int>* index_to_path);
  bool HavePathsChanged();
  void SynchronizeFullAssignment();
  void UpdateAllRanks();
  void UpdatePathRanksFromStart(int start);

  const PathsMetadata& paths_metadata_;
  std::vector<int64_t> node_path_starts_;
  SparseBitset<int64_t> new_synchronized_unperformed_nodes_;
  std::vector<int64_t> new_nexts_;
  std::vector<int> delta_touched_;
  SparseBitset<> touched_paths_;
  // clang-format off
  std::vector<std::pair<int64_t, int64_t> > touched_path_chain_start_ends_;
  // clang-format on
  std::vector<int> ranks_;

  Status status_;
  bool lns_detected_;
};

}  // namespace operations_research

#endif  // OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_FILTERS_H_
