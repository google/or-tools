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

#ifndef ORTOOLS_ROUTING_FILTERS_H_
#define ORTOOLS_ROUTING_FILTERS_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/base/types.h"
#include "ortools/constraint_solver/assignment.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/local_search.h"
#include "ortools/routing/filter_committables.h"
#include "ortools/routing/lp_scheduling.h"
#include "ortools/routing/parameters.pb.h"
#include "ortools/routing/path_state.h"
#include "ortools/routing/routing.h"
#include "ortools/routing/types.h"
#include "ortools/util/bitset.h"
#include "ortools/util/range_minimum_query.h"

namespace operations_research::routing {

// Given a DimensionValues whose path has changed nodes, fills the travels,
// travel_sums, transits, cumuls, and span of the new path.
// This only sets the initial values at each node, and does not propagate
// the transit constraint cumul[i+1] = cumul[i] + transits[i].
// Returns false if some cumul.min exceeds the capacity, or if the sum of
// travels exceeds the span_upper_bound.
bool FillDimensionValuesFromDimension(
    int path, int64_t capacity, int64_t span_upper_bound,
    absl::Span<const DimensionValues::Interval> cumul_of_node,
    absl::Span<const DimensionValues::Interval> slack_of_node,
    absl::AnyInvocable<int64_t(int64_t, int64_t) const> evaluator,
    DimensionValues& dimension_values);

void FillPrePostVisitValues(
    int path, const DimensionValues& dimension_values,
    std::optional<absl::AnyInvocable<int64_t(int64_t, int64_t) const>>
        pre_travel_evaluator,
    std::optional<absl::AnyInvocable<int64_t(int64_t, int64_t) const>>
        post_travel_evaluator,
    PrePostVisitValues& visit_values);

// Propagates vehicle break constraints in dimension_values.
// This returns false if breaks cannot fit the path.
// Otherwise, this returns true, and modifies the start cumul, end cumul and the
// span of the given path.
// This applies light reasoning, and runs in O(#breaks * #interbreak rules).
bool PropagateLightweightVehicleBreaks(
    int path, DimensionValues& dimension_values,
    absl::Span<const std::pair<int64_t, int64_t>> interbreaks,
    bool breaks_are_ordered = false);

/// Returns a filter tracking route constraints.
IntVarLocalSearchFilter* MakeRouteConstraintFilter(const Model& routing_model);

/// Returns a filter ensuring that max active vehicles constraints are enforced.
IntVarLocalSearchFilter* MakeMaxActiveVehiclesFilter(
    const Model& routing_model);

/// Returns a filter ensuring that all nodes in a same activity group have the
/// same activity.
IntVarLocalSearchFilter* MakeActiveNodeGroupFilter(const Model& routing_model);

/// Returns a filter ensuring that for each ordered activity group,
/// if nodes[i] is active then nodes[i-1] is active.
IntVarLocalSearchFilter* MakeOrderedActivityGroupFilter(
    const Model& routing_model);

/// Returns a filter ensuring that node disjunction constraints are enforced.
IntVarLocalSearchFilter* MakeNodeDisjunctionFilter(const Model& routing_model,
                                                   bool filter_cost);

/// Returns a filter computing vehicle amortized costs.
IntVarLocalSearchFilter* MakeVehicleAmortizedCostFilter(
    const Model& routing_model);

/// Returns a filter computing same vehicle costs.
IntVarLocalSearchFilter* MakeSameVehicleCostFilter(const Model& routing_model);

/// Returns a filter ensuring type regulation constraints are enforced.
IntVarLocalSearchFilter* MakeTypeRegulationsFilter(const Model& routing_model);

/// Returns a filter handling dimension costs and constraints.
IntVarLocalSearchFilter* MakePathCumulFilter(const Dimension& dimension,
                                             bool propagate_own_objective_value,
                                             bool filter_objective_cost,
                                             bool may_use_optimizers);

/// Version of dimension path filter in O(length of moved chains). Currently
/// only supports dimensions without costs (global and local span cost, soft
/// bounds) and with unconstrained cumul variables except overall capacity and
/// cumul variables of path ends.
IntVarLocalSearchFilter* MakeChainCumulFilter(const Dimension& dimension);

/// Returns a filter handling dimension cumul bounds.
IntVarLocalSearchFilter* MakeCumulBoundsPropagatorFilter(
    const Dimension& dimension);

/// Returns a filter checking global linear constraints and costs.
IntVarLocalSearchFilter* MakeGlobalLPCumulFilter(
    GlobalDimensionCumulOptimizer* absl_nonnull lp_optimizer,
    GlobalDimensionCumulOptimizer* absl_nonnull mp_optimizer,
    bool filter_objective_cost);

/// Returns a filter checking the feasibility and cost of the resource
/// assignment.
LocalSearchFilter* MakeResourceAssignmentFilter(
    LocalDimensionCumulOptimizer* absl_nonnull optimizer,
    LocalDimensionCumulOptimizer* absl_nonnull mp_optimizer,
    bool propagate_own_objective_value, bool filter_objective_cost);

/// Returns a filter checking the current solution using CP propagation.
IntVarLocalSearchFilter* MakeCPFeasibilityFilter(Model* routing_model);

#if !defined(SWIG)
// Make a filter that takes ownership of a PathState and synchronizes it with
// solver events. The solver represents a graph with array of variables 'nexts'.
// Solver events are embodied by Assignment* deltas, that are translated to node
// changes during Relax(), committed during Synchronize(), and reverted on
// Revert().
LocalSearchFilter* MakePathStateFilter(Solver* solver,
                                       std::unique_ptr<PathState> path_state,
                                       const std::vector<IntVar*>& nexts);

/// Returns a filter checking that vehicle variable domains are respected.
LocalSearchFilter* MakeVehicleVarFilter(const Model& routing_model,
                                        const PathState* path_state);

/// Returns a filter enforcing pickup and delivery constraints for the given
/// pair of nodes and given policies.
LocalSearchFilter* MakePickupDeliveryFilter(
    const Model& routing_model, const PathState* path_state,
    absl::Span<const PickupDeliveryPair> pairs,
    const std::vector<Model::PickupAndDeliveryPolicy>& vehicle_policies);

// This checker enforces dimension requirements.
// A dimension requires that there is some valuation of
// cumul and demand such that for all paths:
// - cumul[A] is in interval node_capacity[A]
// - if arc A -> B is on a path of path_class p,
//   then cumul[A] + demand[p](A, B) = cumul[B].
// - if A is on a path of class p, then
//   cumul[A] must be inside interval path_capacity[path].
class DimensionChecker {
 public:
  struct Interval {
    int64_t min;
    int64_t max;
  };

  struct ExtendedInterval {
    int64_t min;
    int64_t max;
    int64_t num_negative_infinity;
    int64_t num_positive_infinity;
  };

  // TODO(user): the addition of kMinRangeSizeForRIQ slowed down Check().
  // See if using a template parameter makes it faster.
  DimensionChecker(const PathState* path_state,
                   std::vector<Interval> path_capacity,
                   std::vector<int> path_class,
                   std::vector<std::function<Interval(int64_t, int64_t)>>
                       demand_per_path_class,
                   std::vector<Interval> node_capacity,
                   int min_range_size_for_riq = kOptimalMinRangeSizeForRIQ);

  // Given the change made in PathState, checks that the dimension
  // constraint is still feasible.
  bool Check() const;

  // Commits to the changes made in PathState,
  // must be called before PathState::Commit().
  void Commit();

  static constexpr int kOptimalMinRangeSizeForRIQ = 4;

 private:
  inline void UpdateCumulUsingChainRIQ(int first_index, int last_index,
                                       const ExtendedInterval& path_capacity,
                                       ExtendedInterval& cumul) const;

  // Commits to the current solution and rebuilds structures from scratch.
  void FullCommit();
  // Commits to the current solution and only build structures for paths that
  // changed, using additional space to do so in a time-memory tradeoff.
  void IncrementalCommit();
  // Adds sums of given path to the bottom layer of the Range Intersection Query
  // structure, updates index_ and previous_nontrivial_index_.
  void AppendPathDemandsToSums(int path);
  // Updates the Range Intersection Query structure from its bottom layer,
  // with [begin_index, end_index) the range of the change,
  // which must be at the end of the bottom layer.
  // Supposes that requests overlapping the range will be inside the range,
  // to avoid updating all layers.
  void UpdateRIQStructure(int begin_index, int end_index);

  const PathState* const path_state_;
  const std::vector<ExtendedInterval> path_capacity_;
  const std::vector<int> path_class_;
  const std::vector<std::function<Interval(int64_t, int64_t)>>
      demand_per_path_class_;
  std::vector<ExtendedInterval> cached_demand_;
  const std::vector<ExtendedInterval> node_capacity_;

  // Precomputed data.
  // Maps nodes to their pre-computed data, except for isolated nodes,
  // which do not have precomputed data.
  // Only valid for nodes that are in some path in the committed state.
  std::vector<int> index_;
  // Range intersection query in <O(n log n), O(1)>, with n = #nodes.
  // Let node be in a path, i = index_[node], start the start of node's path.
  // Let l such that index_[start] <= i - 2**l.
  // - riq_[l][i].tsum_at_lst contains the sum of demands from start to node.
  // - riq_[l][i].tsum_at_fst contains the sum of demands from start to the
  //   node at i - 2**l.
  // - riq_[l][i].tightest_tsum contains the intersection of
  //   riq_[0][j].tsum_at_lst for all j in (i - 2**l, i].
  // - riq_[0][i].cumuls_to_lst and riq_[0][i].cumuls_to_fst contain
  //   the node's capacity.
  // - riq_[l][i].cumuls_to_lst is the intersection, for j in (i - 2**l, i], of
  //   riq_[0][j].cumuls_to_lst + sum_{k in [j, i)} demand(k, k+1)
  // - riq_[l][i].cumuls_to_fst is the intersection, for j in (i - 2**l, i], of
  //   riq_[0][j].cumuls_to_fst - sum_{k in (i-2**l, j)} demand(k, k+1)
  struct RIQNode {
    ExtendedInterval cumuls_to_fst;
    ExtendedInterval tightest_tsum;
    ExtendedInterval cumuls_to_lst;
    ExtendedInterval tsum_at_fst;
    ExtendedInterval tsum_at_lst;
  };
  std::vector<std::vector<RIQNode>> riq_;
  // The incremental branch of Commit() may waste space in the layers of the
  // RIQ structure. This is the upper limit of a layer's size.
  const int maximum_riq_layer_size_;
  // Range queries are used on a chain only if the range is larger than this.
  const int min_range_size_for_riq_;
};

// Make a filter that translates solver events to the input checker's interface.
// Since DimensionChecker has a PathState, the filter returned by this
// must be synchronized to the corresponding PathStateFilter:
// - Relax() must be called after the PathStateFilter's.
// - Accept() must be called after.
// - Synchronize() must be called before.
// - Revert() must be called before.
LocalSearchFilter* MakeDimensionFilter(
    Solver* solver, std::unique_ptr<DimensionChecker> checker,
    absl::string_view dimension_name);
#endif  // !defined(SWIG)

class LightVehicleBreaksChecker {
 public:
  struct VehicleBreak {
    int64_t start_min;
    int64_t start_max;
    int64_t end_min;
    int64_t end_max;
    int64_t duration_min;
    bool is_performed_min;
    bool is_performed_max;
  };
  struct InterbreakLimit {
    int64_t max_interbreak_duration;
    int64_t min_break_duration;
  };

  struct PathData {
    std::vector<VehicleBreak> vehicle_breaks;
    std::vector<InterbreakLimit> interbreak_limits;
    LocalSearchState::Variable start_cumul;
    LocalSearchState::Variable end_cumul;
    LocalSearchState::Variable total_transit;
    LocalSearchState::Variable span;
  };

  LightVehicleBreaksChecker(PathState* path_state,
                            std::vector<PathData> path_data);

  void Relax() const;

  bool Check() const;

 private:
  PathState* path_state_;
  std::vector<PathData> path_data_;
};

LocalSearchFilter* MakeLightVehicleBreaksFilter(
    Solver* solver, std::unique_ptr<LightVehicleBreaksChecker> checker,
    absl::string_view dimension_name);

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
  WeightedWaveletTree() = default;

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
              static_cast<int>(els[range_last_index].is_left),
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

// TODO(user): improve this class by:
// - using WeightedWaveletTree to get the amount of energy above the threshold.
// - detect when costs above and below are the same, to avoid correcting for
//   energy above the threshold and get O(1) time per chain.
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
  void CacheAndPrecomputeRangeQueriesOfPath(int path);
  void IncrementalCacheAndPrecompute();
  void FullCacheAndPrecompute();

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

  // Range queries.
  const int maximum_range_query_size_;
  // Allows to compute the minimum total_force over any chain,
  // supposing the starting force is 0.
  RangeMinimumQuery<int64_t> force_rmq_;
  std::vector<int> force_rmq_index_of_node_;
  // Allows to compute the sum of energies of transitions whose total_force is
  // above a threshold over any chain. Supposes total_force at start is 0.
  WeightedWaveletTree energy_query_;
  // Allows to compute the sum of distances of transitions whose total_force is
  // above a threshold over any chain. Supposes total_force at start is 0.
  WeightedWaveletTree distance_query_;
  // Maps nodes to their common index in both threshold queries.
  std::vector<int> threshold_query_index_of_node_;

  std::vector<int64_t> cached_force_;
  std::vector<int64_t> cached_distance_;

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
    const PathState* path_state, const std::vector<Dimension*>& dimensions,
    std::vector<LocalSearchFilterManager::FilterEvent>* filters);

void AppendDimensionCumulFilters(
    const std::vector<Dimension*>& dimensions,
    const RoutingSearchParameters& parameters, bool filter_objective_cost,
    bool use_chain_cumul_filter,
    std::vector<LocalSearchFilterManager::FilterEvent>* filters);

/// Generic path-based filter class.

class BasePathFilter : public IntVarLocalSearchFilter {
 public:
  BasePathFilter(const std::vector<IntVar*>& nexts, int next_domain_size,
                 const PathsMetadata& paths_metadata);
  ~BasePathFilter() override = default;
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
  const std::vector<int64_t>& GetTouchedPathStarts() const {
    return touched_paths_.PositionsSetAtLeastOnce();
  }
  bool PathStartTouched(int64_t start) const { return touched_paths_[start]; }
  const std::vector<int64_t>& GetNewSynchronizedUnperformedNodes() const {
    return new_synchronized_unperformed_nodes_.PositionsSetAtLeastOnce();
  }

  bool lns_detected() const { return lns_detected_; }

 private:
  virtual void OnBeforeSynchronizePaths(bool) {}
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
  std::vector<std::pair<int64_t, int64_t>> touched_path_chain_start_ends_;
  std::vector<int> ranks_;

  bool lns_detected_;
};

}  // namespace operations_research::routing

#endif  // ORTOOLS_ROUTING_FILTERS_H_
