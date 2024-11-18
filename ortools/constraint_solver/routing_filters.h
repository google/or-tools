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

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <memory>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/base/types.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "ortools/constraint_solver/routing.h"
#include "ortools/constraint_solver/routing_lp_scheduling.h"
#include "ortools/constraint_solver/routing_parameters.pb.h"
#include "ortools/constraint_solver/routing_types.h"
#include "ortools/util/bitset.h"
#include "ortools/util/range_minimum_query.h"
#include "ortools/util/saturated_arithmetic.h"

namespace operations_research {

// A vector that allows to revert back to a previously committed state,
// get the set of changed indices, and get current and committed values.
template <typename T>
class CommittableValue {
 public:
  explicit CommittableValue(const T& value)
      : current_(value), committed_(value) {}

  const T& Get() const { return current_; }
  const T& GetCommitted() const { return committed_; }

  void Set(const T& value) { current_ = value; }

  void SetAndCommit(const T& value) {
    Set(value);
    Commit();
  }

  void Revert() { current_ = committed_; }

  void Commit() { committed_ = current_; }

 private:
  T current_;
  T committed_;
};

template <typename T>
class CommittableVector {
 public:
  // Makes a vector with initial elements all committed to value.
  CommittableVector<T>(size_t num_elements, const T& value)
      : elements_(num_elements, {value, value}), changed_(num_elements) {}

  // Return the size of the vector.
  size_t Size() const { return elements_.size(); }

  // Returns a copy of the value stored at index in the current state.
  // Does not return a reference, because the class needs to know when elements
  // are modified.
  T Get(size_t index) const {
    DCHECK_LT(index, elements_.size());
    return elements_[index].current;
  }

  // Set the value stored at index in the current state to given value.
  void Set(size_t index, const T& value) {
    DCHECK_GE(index, 0);
    DCHECK_LT(index, elements_.size());
    changed_.Set(index);
    elements_[index].current = value;
  }

  // Changes the values of the vector to those in the last Commit().
  void Revert() {
    for (const size_t index : changed_.PositionsSetAtLeastOnce()) {
      elements_[index].current = elements_[index].committed;
    }
    changed_.ClearAll();
  }

  // Makes the current state committed, clearing all changes.
  void Commit() {
    for (const size_t index : changed_.PositionsSetAtLeastOnce()) {
      elements_[index].committed = elements_[index].current;
    }
    changed_.ClearAll();
  }

  // Sets all elements of this vector to given value, and commits to this state.
  // Supposes that there are no changes since the last Commit() or Revert().
  void SetAllAndCommit(const T& value) {
    DCHECK_EQ(0, changed_.NumberOfSetCallsWithDifferentArguments());
    elements_.assign(elements_.size(), {value, value});
  }

  // Returns a copy of the value stored at index in the last committed state.
  T GetCommitted(size_t index) const {
    DCHECK_LT(index, elements_.size());
    return elements_[index].committed;
  }

  // Return true iff the value at index has been Set() since the last Commit()
  // or Revert(), even if the current value is the same as the committed value.
  bool HasChanged(size_t index) const { return changed_[index]; }

  // Returns the set of indices that have been Set() since the last Commit() or
  // Revert().
  const std::vector<size_t>& ChangedIndices() const {
    return changed_.PositionsSetAtLeastOnce();
  }

 private:
  struct VersionedElement {
    T current;
    T committed;
  };
  // Holds current and committed versions of values of this vector.
  std::vector<VersionedElement> elements_;
  // Holds indices that were Set() since the last Commit() or Revert().
  SparseBitset<size_t> changed_;
};

// This class allows to represent a state of dimension values for all paths of
// a vehicle routing problem. Values of interest for each path are:
// - nodes,
// - cumuls (min/max),
// - transit times,
// - sum of transit times since the beginning of the path,
// - span (min/max).
//
// This class can maintain two states at once: a committed state and a current
// state. The current state can be modified by first describing a path p to be
// modified with PushNode() and MakePathFromNewNodes(). Then the dimension
// values of this path can be modified with views returned by MutableXXX()
// methods.
//
// When a set of paths has been modified, the caller can decide to definitely
// change the committed state to the new state, or to revert to the committed
// state.
//
// Operations are meant to be efficient:
// - all path modifications, i.e. PushNode(), MakePathFromNewNodes(),
//   MutableXXX(), MutableSpan() operations are O(1).
// - Revert() is O(num changed paths).
// - Commit() has two behaviors:
//   - if there are less than max_num_committed_elements_ elements in the
//     committed state, then Commit() is O(num changed paths).
//   - otherwise, Commit() does a compaction of the committed state, in
//     O(num_nodes + num_paths).
//   The amortized cost of Commit(), when taking modifications into account,
//   is O(size of changed paths), because all modifications pay at worst
//   O(1) for its own compaction.
//
// Note that this class does not support the semantics associated with its
// fields names, for instance it does not make sure that cumul_min <= cumul_max.
// The field names are meant for readability for the user.
// However, path sizes are enforced: if a path has n nodes, then it has
// n fields for cumul min/max, n for transit_sums, and max(0, n-1) for transits.
class DimensionValues {
 public:
  DimensionValues(int num_paths, int num_nodes)
      : range_of_path_(num_paths, {.begin = 0, .end = 0}),
        committed_range_of_path_(num_paths, {.begin = 0, .end = 0}),
        span_(num_paths, Interval::AllIntegers()),
        committed_span_(num_paths, Interval::AllIntegers()),
        vehicle_breaks_(num_paths),
        committed_vehicle_breaks_(num_paths),
        changed_paths_(num_paths),
        max_num_committed_elements_(16 * num_nodes) {
    nodes_.reserve(max_num_committed_elements_);
    transit_.reserve(max_num_committed_elements_);
    travel_.reserve(max_num_committed_elements_);
    travel_sum_.reserve(max_num_committed_elements_);
    cumul_.reserve(max_num_committed_elements_);
  }

  struct Interval {
    int64_t min;
    int64_t max;
    // Tests inequality between intervals.
    bool operator!=(const Interval& other) const {
      return min != other.min || max != other.max;
    }
    // Tests equality between intervals.
    bool operator==(const Interval& other) const {
      return min == other.min && max == other.max;
    }
    // Returns true iff the interval is empty.
    bool IsEmpty() const { return min > max; }
    // Increases the min to be at least lower_bound,
    // returns true iff the interval is nonempty.
    bool IncreaseMin(int64_t lower_bound) {
      min = std::max(min, lower_bound);
      return min <= max;
    }
    // Decreases the max to be at most upper_bound,
    // returns true iff the interval is nonempty.
    bool DecreaseMax(int64_t upper_bound) {
      max = std::min(max, upper_bound);
      return min <= max;
    }
    // Intersects this interval with the other, returns true iff the interval
    // is nonempty.
    bool IntersectWith(const Interval& other) {
      min = std::max(min, other.min);
      max = std::min(max, other.max);
      return min <= max;
    }
    // A set addition, with intervals: adds other.min to the min, other.max to
    // the max, with CapAdd().
    void Add(const Interval& other) {
      DCHECK(!IsEmpty());
      DCHECK(!other.IsEmpty());
      min = CapAdd(min, other.min);
      max = CapAdd(max, other.max);
    }
    // A set subtraction, with intervals: subtracts other.max from the min,
    // other.min from the max, with CapSub().
    void Subtract(const Interval& other) {
      DCHECK(!IsEmpty());
      DCHECK(!other.IsEmpty());
      min = CapSub(min, other.max);
      max = CapSub(max, other.min);
    }
    // Returns an interval containing all integers: {kint64min, kint64max}.
    static Interval AllIntegers() {
      return {.min = kint64min, .max = kint64max};
    }
  };

  struct VehicleBreak {
    Interval start;
    Interval end;
    Interval duration;
    Interval is_performed;
    bool operator==(const VehicleBreak& other) const {
      return start == other.start && end == other.end &&
             duration == other.duration && is_performed == other.is_performed;
    }
  };

  // Adds a node to new nodes.
  void PushNode(int node) { nodes_.push_back(node); }

  // Turns new nodes into a new path, allocating dimension values for it.
  void MakePathFromNewNodes(int path) {
    DCHECK_GE(path, 0);
    DCHECK_LT(path, range_of_path_.size());
    DCHECK(!changed_paths_[path]);
    range_of_path_[path] = {.begin = num_current_elements_,
                            .end = nodes_.size()};
    changed_paths_.Set(path);
    // Allocate dimension values. We allocate n cells for all dimension values,
    // even transits, so they can all be indexed by the same range_of_path.
    transit_.resize(nodes_.size(), Interval::AllIntegers());
    travel_.resize(nodes_.size(), 0);
    travel_sum_.resize(nodes_.size(), 0);
    cumul_.resize(nodes_.size(), Interval::AllIntegers());
    num_current_elements_ = nodes_.size();
    span_[path] = Interval::AllIntegers();
  }

  // Resets all path to empty, in both committed and current state.
  void Reset() {
    const int num_paths = range_of_path_.size();
    range_of_path_.assign(num_paths, {.begin = 0, .end = 0});
    committed_range_of_path_.assign(num_paths, {.begin = 0, .end = 0});
    changed_paths_.SparseClearAll();
    num_current_elements_ = 0;
    num_committed_elements_ = 0;
    nodes_.clear();
    transit_.clear();
    travel_.clear();
    travel_sum_.clear();
    cumul_.clear();
    committed_span_.assign(num_paths, Interval::AllIntegers());
  }

  // Clears the changed state, make it point to the committed state.
  void Revert() {
    for (const int path : changed_paths_.PositionsSetAtLeastOnce()) {
      range_of_path_[path] = committed_range_of_path_[path];
    }
    changed_paths_.SparseClearAll();
    num_current_elements_ = num_committed_elements_;
    nodes_.resize(num_current_elements_);
    transit_.resize(num_current_elements_);
    travel_.resize(num_current_elements_);
    travel_sum_.resize(num_current_elements_);
    cumul_.resize(num_current_elements_);
  }

  // Makes the committed state point to the current state.
  // If the state representation is too large, reclaims memory by compacting
  // the committed state.
  void Commit() {
    for (const int path : changed_paths_.PositionsSetAtLeastOnce()) {
      committed_range_of_path_[path] = range_of_path_[path];
      committed_span_[path] = span_[path];
      committed_vehicle_breaks_[path] = vehicle_breaks_[path];
    }
    changed_paths_.SparseClearAll();
    num_committed_elements_ = num_current_elements_;
    // If the committed data would take too much space, compact the data:
    // copy committed data to the end of vectors, erase old data, refresh
    // indexing (range_of_path_).
    if (num_current_elements_ <= max_num_committed_elements_) return;
    temp_nodes_.clear();
    temp_transit_.clear();
    temp_travel_.clear();
    temp_travel_sum_.clear();
    temp_cumul_.clear();
    for (int path = 0; path < range_of_path_.size(); ++path) {
      if (committed_range_of_path_[path].Size() == 0) continue;
      const size_t new_begin = temp_nodes_.size();
      const auto [begin, end] = committed_range_of_path_[path];
      temp_nodes_.insert(temp_nodes_.end(), nodes_.begin() + begin,
                         nodes_.begin() + end);
      temp_transit_.insert(temp_transit_.end(), transit_.begin() + begin,
                           transit_.begin() + end);
      temp_travel_.insert(temp_travel_.end(), travel_.begin() + begin,
                          travel_.begin() + end);
      temp_travel_sum_.insert(temp_travel_sum_.end(),
                              travel_sum_.begin() + begin,
                              travel_sum_.begin() + end);
      temp_cumul_.insert(temp_cumul_.end(), cumul_.begin() + begin,
                         cumul_.begin() + end);
      committed_range_of_path_[path] = {.begin = new_begin,
                                        .end = temp_nodes_.size()};
    }
    std::swap(nodes_, temp_nodes_);
    std::swap(transit_, temp_transit_);
    std::swap(travel_, temp_travel_);
    std::swap(travel_sum_, temp_travel_sum_);
    std::swap(cumul_, temp_cumul_);
    range_of_path_ = committed_range_of_path_;
    num_committed_elements_ = nodes_.size();
    num_current_elements_ = nodes_.size();
  }

  // Returns a const view of the nodes of the path, in the committed state.
  absl::Span<const int> CommittedNodes(int path) const {
    const auto [begin, end] = committed_range_of_path_[path];
    return absl::MakeConstSpan(nodes_.data() + begin, nodes_.data() + end);
  }

  // Returns a const view of the nodes of the path, in the current state.
  absl::Span<const int> Nodes(int path) const {
    const auto [begin, end] = range_of_path_[path];
    return absl::MakeConstSpan(nodes_.data() + begin, nodes_.data() + end);
  }

  // Returns a const view of the transits of the path, in the current state.
  absl::Span<const Interval> Transits(int path) const {
    auto [begin, end] = range_of_path_[path];
    // When the path is not empty, #transits = #nodes - 1.
    // When the path is empty, begin = end, return empty span.
    if (begin < end) --end;
    return absl::MakeConstSpan(transit_.data() + begin, transit_.data() + end);
  }

  // Returns a mutable view of the transits of the path, in the current state.
  absl::Span<Interval> MutableTransits(int path) {
    auto [begin, end] = range_of_path_[path];
    // When the path is not empty, #transits = #nodes - 1.
    // When the path is empty, begin = end, return empty span.
    if (begin < end) --end;
    return absl::MakeSpan(transit_.data() + begin, transit_.data() + end);
  }

  // Returns a const view of the travels of the path, in the current
  // state.
  absl::Span<const int64_t> Travels(int path) const {
    auto [begin, end] = range_of_path_[path];
    if (begin < end) --end;
    return absl::MakeConstSpan(travel_.data() + begin, travel_.data() + end);
  }

  // Returns a mutable view of the travels of the path, in the current
  // state.
  absl::Span<int64_t> MutableTravels(int path) {
    auto [begin, end] = range_of_path_[path];
    if (begin < end) --end;
    return absl::MakeSpan(travel_.data() + begin, travel_.data() + end);
  }

  // Returns a const view of the travel sums of the path, in the current state.
  absl::Span<const int64_t> TravelSums(int path) const {
    const auto [begin, end] = range_of_path_[path];
    return absl::MakeConstSpan(travel_sum_.data() + begin,
                               travel_sum_.data() + end);
  }

  // Returns a mutable view of the travel sums of the path in the current state.
  absl::Span<int64_t> MutableTravelSums(int path) {
    const auto [begin, end] = range_of_path_[path];
    return absl::MakeSpan(travel_sum_.data() + begin, travel_sum_.data() + end);
  }

  // Returns a const view of the cumul mins of the path, in the current state.
  absl::Span<const Interval> Cumuls(int path) const {
    const auto [begin, end] = range_of_path_[path];
    return absl::MakeConstSpan(cumul_.data() + begin, cumul_.data() + end);
  }

  // Returns a mutable view of the cumul mins of the path, in the current state.
  absl::Span<Interval> MutableCumuls(int path) {
    const auto [begin, end] = range_of_path_[path];
    return absl::MakeSpan(cumul_.data() + begin, cumul_.data() + end);
  }

  // Returns the span interval of the path, in the current state.
  Interval Span(int path) const {
    return changed_paths_[path] ? span_[path] : committed_span_[path];
  }
  // Returns a mutable view of the span of the path, in the current state.
  // The path must have been changed since the last commit.
  Interval& MutableSpan(int path) {
    DCHECK(changed_paths_[path]);
    return span_[path];
  }

  // Returns a const view of the vehicle breaks of the path, in the current
  // state.
  absl::Span<const VehicleBreak> VehicleBreaks(int path) const {
    return absl::MakeConstSpan(changed_paths_[path]
                                   ? vehicle_breaks_[path]
                                   : committed_vehicle_breaks_[path]);
  }

  // Returns a mutable vector of the vehicle breaks of the path, in the current
  // state. The path must have been changed since the last commit.
  std::vector<VehicleBreak>& MutableVehicleBreaks(int path) {
    DCHECK(changed_paths_[path]);
    return vehicle_breaks_[path];
  }

  // Returns the number of nodes of the path, in the current state.
  int NumNodes(int path) const { return range_of_path_[path].Size(); }
  // Returns a const view of the set of paths changed, in the current state.
  absl::Span<const int> ChangedPaths() const {
    return absl::MakeConstSpan(changed_paths_.PositionsSetAtLeastOnce());
  }
  // Returns whether the given path was changed, in the current state.
  bool PathHasChanged(int path) const { return changed_paths_[path]; }

 private:
  // These vectors hold the data of both committed and current states.
  // The ranges below determine which indices are associated to each path and
  // each state. It is up to the user to maintain the following invariants:
  // If range_of_path_[p] == {.begin = b, .end = e}, then, in the current
  // state:
  // - nodes_[i] for i in [b, e) are the nodes of the path p.
  // - cumul_[r] + transit_[r] == cumul_[r+1] for r in [b, e-1).
  // - travel_[r] <= transit_[r].min for r in [b, e-1).
  // - travel_sum_[r] == sum_{r' in [0, r')} travel_[r'], for r in [b+1, e)
  // - cumul[b] + span_[p] == cumul[e-1].
  //
  // The same invariants should hold for committed_range_of_path_ and the
  // committed state.
  std::vector<int> nodes_;
  std::vector<Interval> transit_;
  std::vector<int64_t> travel_;
  std::vector<int64_t> travel_sum_;
  std::vector<Interval> cumul_;
  // Temporary vectors used in Commit() during compaction.
  std::vector<int> temp_nodes_;
  std::vector<Interval> temp_transit_;
  std::vector<int64_t> temp_travel_;
  std::vector<int64_t> temp_travel_sum_;
  std::vector<Interval> temp_cumul_;
  // A path has a range of indices in the committed state and another one in the
  // current state.
  struct Range {
    size_t begin = 0;
    size_t end = 0;
    int Size() const { return end - begin; }
  };
  std::vector<Range> range_of_path_;
  std::vector<Range> committed_range_of_path_;
  // Associates span to each path.
  std::vector<Interval> span_;
  std::vector<Interval> committed_span_;
  // Associates vehicle breaks with each path.
  std::vector<std::vector<VehicleBreak>> vehicle_breaks_;
  std::vector<std::vector<VehicleBreak>> committed_vehicle_breaks_;
  // Stores whether each path has been changed since last committed state.
  SparseBitset<int> changed_paths_;
  // Threshold for the size of the committed vector. This is purely heuristic:
  // it should be more than the number of nodes so compactions do not occur at
  // each submit, but ranges should not be too far apart to avoid cache misses.
  const size_t max_num_committed_elements_;
  // This locates the start of new nodes.
  size_t num_current_elements_ = 0;
  size_t num_committed_elements_ = 0;
};

// Propagates vehicle break constraints in dimension_values.
// This returns false if breaks cannot fit the path.
// Otherwise, this returns true, and modifies the start cumul, end cumul and the
// span of the given path.
// This applies light reasoning, and runs in O(#breaks * #interbreak rules).
bool PropagateLightweightVehicleBreaks(
    int path, DimensionValues& dimension_values,
    const std::vector<std::pair<int64_t, int64_t>>& interbreaks);

/// Returns a filter tracking route constraints.
IntVarLocalSearchFilter* MakeRouteConstraintFilter(
    const RoutingModel& routing_model);

/// Returns a filter ensuring that max active vehicles constraints are enforced.
IntVarLocalSearchFilter* MakeMaxActiveVehiclesFilter(
    const RoutingModel& routing_model);

/// Returns a filter ensuring that all nodes in a same activity group have the
/// same activity.
IntVarLocalSearchFilter* MakeActiveNodeGroupFilter(
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
                                             bool may_use_optimizers);

/// Returns a filter handling dimension cumul bounds.
IntVarLocalSearchFilter* MakeCumulBoundsPropagatorFilter(
    const RoutingDimension& dimension);

/// Returns a filter checking global linear constraints and costs.
IntVarLocalSearchFilter* MakeGlobalLPCumulFilter(
    GlobalDimensionCumulOptimizer* lp_optimizer,
    GlobalDimensionCumulOptimizer* mp_optimizer, bool filter_objective_cost);

/// Returns a filter checking the feasibility and cost of the resource
/// assignment.
LocalSearchFilter* MakeResourceAssignmentFilter(
    LocalDimensionCumulOptimizer* optimizer,
    LocalDimensionCumulOptimizer* mp_optimizer,
    bool propagate_own_objective_value, bool filter_objective_cost);

/// Returns a filter checking the current solution using CP propagation.
IntVarLocalSearchFilter* MakeCPFeasibilityFilter(RoutingModel* routing_model);

#if !defined(SWIG)
// A PathState represents a set of paths and changes made on it.
//
// More accurately, let us define P_{num_nodes, starts, ends}-graphs the set of
// directed graphs with nodes [0, num_nodes) whose connected components are
// paths from starts[i] to ends[i] (for the same i) and loops.
// Let us fix num_nodes, starts and ends, so we call these P-graphs.
//
// A P-graph can be described by the sequence of nodes of each of its paths,
// and its set of loops. To describe a change made on a given P-graph G0 that
// yields another P-graph G1, we choose to describe G1 in terms of G0. When
// the difference between G0 and G1 is small, as is almost always the case in a
// local search setting, the description is compact, allowing for incremental
// filters to be efficient.
//
// In order to describe G1 in terms of G0 succintly, we describe each path of
// G1 as a sequence of chains of G0. A chain of G0 is either a nonempty sequence
// of consecutive nodes of a path of G0, or a node that was a loop in G0.
// For instance, a path that was not modified from G0 to G1 has one chain,
// the sequence of all nodes in the path. Typically, local search operators
// modify one or two paths, and the resulting paths can described as sequences
// of two to four chains of G0. Paths that were modified are listed explicitly,
// allowing to iterate only on changed paths.
// The loops of G1 are described more implicitly: the loops of G1 not in G0
// are listed explicitly, but those in both G1 and G0 are not listed.
//
// A PathState object can be in two states: committed or changed.
// At construction, the object is committed, G0.
// To enter a changed state G1, one can pass modifications with ChangePath() and
// ChangeLoops(). For reasons of efficiency, a chain is described as a range of
// node indices in the representation of the committed graph G0. To that effect,
// the nodes of a path of G0 are guaranteed to have consecutive indices.
//
// Filters can then browse the change efficiently using ChangedPaths(),
// Chains(), Nodes() and ChangedLoops().
//
// Then Commit() or Revert() can be called: Commit() sets the changed state G1
// as the new committed state, Revert() erases all changes.
class PathState {
 public:
  // A Chain allows to iterate on all nodes of a chain, and access some data:
  // first node, last node, number of nodes in the chain.
  // Chain is a range, its iterator ChainNodeIterator, its value type int.
  // Chains are returned by PathChainIterator's operator*().
  class Chain;
  // A ChainRange allows to iterate on all chains of a path.
  // ChainRange is a range, its iterator Chain*, its value type Chain.
  class ChainRange;
  // A NodeRange allows to iterate on all nodes of a path.
  // NodeRange is a range, its iterator PathNodeIterator, its value type int.
  class NodeRange;

  struct ChainBounds {
    ChainBounds() = default;
    ChainBounds(int begin_index, int end_index)
        : begin_index(begin_index), end_index(end_index) {}
    int begin_index;
    int end_index;
  };
  int CommittedIndex(int node) const { return committed_index_[node]; }
  ChainBounds CommittedPathRange(int path) const { return chains_[path]; }

  // Path constructor: path_start and path_end must be disjoint,
  // their values in [0, num_nodes).
  PathState(int num_nodes, std::vector<int> path_start,
            std::vector<int> path_end);

  // Instance-constant accessors.

  // Returns the number of nodes in the underlying graph.
  int NumNodes() const { return num_nodes_; }
  // Returns the number of paths (empty paths included).
  int NumPaths() const { return num_paths_; }
  // Returns the start of a path.
  int Start(int path) const { return path_start_end_[path].start; }
  // Returns the end of a path.
  int End(int path) const { return path_start_end_[path].end; }

  // State-dependent accessors.

  // Returns the committed path of a given node, -1 if it is a loop.
  int Path(int node) const { return committed_paths_[node]; }
  // Returns the set of paths that actually changed,
  // i.e. that have more than one chain.
  const std::vector<int>& ChangedPaths() const { return changed_paths_; }
  // Returns the set of loops that were added by the change.
  const std::vector<int>& ChangedLoops() const { return changed_loops_; }
  // Returns the current range of chains of path.
  ChainRange Chains(int path) const;
  // Returns the current range of nodes of path.
  NodeRange Nodes(int path) const;

  // State modifiers.

  // Changes the path to the given sequence of chains of the committed state.
  // Chains are described by semi-open intervals. No optimization is made in
  // case two consecutive chains are actually already consecutive in the
  // committed state: they are not merged into one chain, and Chains(path) will
  // report the two chains.
  void ChangePath(int path, absl::Span<const ChainBounds> chains);
  // Same as above, but the initializer_list interface avoids the need to pass
  // a vector.
  void ChangePath(int path, const std::initializer_list<ChainBounds>& chains) {
    changed_paths_.push_back(path);
    const int path_begin_index = chains_.size();
    chains_.insert(chains_.end(), chains.begin(), chains.end());
    const int path_end_index = chains_.size();
    paths_[path] = {path_begin_index, path_end_index};
    // Always add sentinel, in case this is the last path change.
    chains_.emplace_back(0, 0);
  }

  // Describes the nodes that are newly loops in this change.
  void ChangeLoops(absl::Span<const int> new_loops);

  // Set the current state G1 as committed. See class comment for details.
  void Commit();
  // Erase incremental changes. See class comment for details.
  void Revert();

  // LNS Operators may not fix variables,
  // in which case we mark the candidate invalid.
  void SetInvalid() { is_invalid_ = true; }
  bool IsInvalid() const { return is_invalid_; }

 private:
  // Most structs below are named pairs of ints, for typing purposes.

  // Start and end are stored together to optimize (likely) simultaneous access.
  struct PathStartEnd {
    PathStartEnd(int start, int end) : start(start), end(end) {}
    int start;
    int end;
  };
  // Paths are ranges of chains, which are ranges of committed nodes, see below.
  struct PathBounds {
    int begin_index;
    int end_index;
  };

  // Copies nodes in chains of path at the end of nodes,
  // and sets those nodes' path member to value path.
  void CopyNewPathAtEndOfNodes(int path);
  // Commits paths in O(#{changed paths' nodes}) time,
  // increasing this object's space usage by O(|changed path nodes|).
  void IncrementalCommit();
  // Commits paths in O(num_nodes + num_paths) time,
  // reducing this object's space usage to O(num_nodes + num_paths).
  void FullCommit();

  // Instance-constant data.
  const int num_nodes_;
  const int num_paths_;
  std::vector<PathStartEnd> path_start_end_;

  // Representation of the committed and changed paths.
  // A path is a range of chains, which is a range of nodes.
  // Ranges are represented internally by indices in vectors:
  // ChainBounds are indices in committed_nodes_. PathBounds are indices in
  // chains_. When committed (after construction, Revert() or Commit()):
  // - path ranges are [path, path+1): they have one chain.
  // - chain ranges don't overlap, chains_ has an empty sentinel at the end.
  //   The sentinel allows the Nodes() iterator to maintain its current pointer
  //   to committed nodes on NodeRange::operator++().
  // - committed_nodes_ contains all nodes, both paths and loops.
  //   Actually, old duplicates will likely appear,
  //   the current version of a node is at the index given by
  //   committed_index_[node]. A Commit() can add nodes at the end of
  //   committed_nodes_ in a space/time tradeoff, but if committed_nodes_' size
  //   is above num_nodes_threshold_, Commit() must reclaim useless duplicates'
  //   space by rewriting the path/chain/nodes structure.
  // When changed (after ChangePaths() and ChangeLoops()),
  // the structure is updated accordingly:
  // - path ranges that were changed have nonoverlapping values [begin, end)
  //   where begin is >= num_paths_ + 1, i.e. new chains are stored after
  //   the committed state.
  // - additional chain ranges are stored after the committed chains and its
  //   sentinel to represent the new chains resulting from the changes.
  //   Those chains do not overlap with one another or with committed chains.
  // - committed_nodes_ are not modified, and still represent the committed
  //   paths. committed_index_ is not modified either.
  std::vector<int> committed_nodes_;
  // Maps nodes to their path in the latest committed state.
  std::vector<int> committed_paths_;
  // Maps nodes to their index in the latest committed state.
  std::vector<int> committed_index_;
  const int num_nodes_threshold_;
  std::vector<ChainBounds> chains_;
  std::vector<PathBounds> paths_;

  // Incremental information.
  std::vector<int> changed_paths_;
  std::vector<int> changed_loops_;

  // See IsInvalid() and SetInvalid().
  bool is_invalid_ = false;
};

// A Chain is a range of committed nodes.
class PathState::Chain {
 public:
  class Iterator {
   public:
    Iterator& operator++() {
      ++current_node_;
      return *this;
    }
    int operator*() const { return *current_node_; }
    bool operator!=(Iterator other) const {
      return current_node_ != other.current_node_;
    }

   private:
    // Only a Chain can construct its iterator.
    friend class PathState::Chain;
    explicit Iterator(const int* node) : current_node_(node) {}
    const int* current_node_;
  };

  // Chains hold CommittedNode* values, a Chain may be invalidated
  // if the underlying vector is modified.
  Chain(const int* begin_node, const int* end_node)
      : begin_(begin_node), end_(end_node) {}

  int NumNodes() const { return end_ - begin_; }
  int First() const { return *begin_; }
  int Last() const { return *(end_ - 1); }
  Iterator begin() const { return Iterator(begin_); }
  Iterator end() const { return Iterator(end_); }

  Chain WithoutFirstNode() const { return Chain(begin_ + 1, end_); }

 private:
  const int* const begin_;
  const int* const end_;
};

// A ChainRange is a range of Chains, committed or not.
class PathState::ChainRange {
 public:
  class Iterator {
   public:
    Iterator& operator++() {
      ++current_chain_;
      return *this;
    }
    Chain operator*() const {
      return {first_node_ + current_chain_->begin_index,
              first_node_ + current_chain_->end_index};
    }
    bool operator!=(Iterator other) const {
      return current_chain_ != other.current_chain_;
    }

   private:
    // Only a ChainRange can construct its Iterator.
    friend class ChainRange;
    Iterator(const ChainBounds* chain, const int* const first_node)
        : current_chain_(chain), first_node_(first_node) {}
    const ChainBounds* current_chain_;
    const int* const first_node_;
  };

  // ChainRanges hold ChainBounds* and CommittedNode*,
  // a ChainRange may be invalidated if on of the underlying vector is modified.
  ChainRange(const ChainBounds* const begin_chain,
             const ChainBounds* const end_chain, const int* const first_node)
      : begin_(begin_chain), end_(end_chain), first_node_(first_node) {}

  Iterator begin() const { return {begin_, first_node_}; }
  Iterator end() const { return {end_, first_node_}; }

 private:
  const ChainBounds* const begin_;
  const ChainBounds* const end_;
  const int* const first_node_;
};

// A NodeRange allows to iterate on all nodes of a path,
// by a two-level iteration on ChainBounds* and CommittedNode* of a PathState.
class PathState::NodeRange {
 public:
  class Iterator {
   public:
    Iterator& operator++() {
      ++current_node_;
      if (current_node_ == end_node_) {
        ++current_chain_;
        // Note: dereferencing bounds is valid because there is a sentinel
        // value at the end of PathState::chains_ to that intent.
        const ChainBounds bounds = *current_chain_;
        current_node_ = first_node_ + bounds.begin_index;
        end_node_ = first_node_ + bounds.end_index;
      }
      return *this;
    }
    int operator*() const { return *current_node_; }
    bool operator!=(Iterator other) const {
      return current_chain_ != other.current_chain_;
    }

   private:
    // Only a NodeRange can construct its Iterator.
    friend class NodeRange;
    Iterator(const ChainBounds* current_chain, const int* const first_node)
        : current_node_(first_node + current_chain->begin_index),
          end_node_(first_node + current_chain->end_index),
          current_chain_(current_chain),
          first_node_(first_node) {}
    const int* current_node_;
    const int* end_node_;
    const ChainBounds* current_chain_;
    const int* const first_node_;
  };

  // NodeRanges hold ChainBounds* and int* (first committed node),
  // a NodeRange may be invalidated if on of the underlying vector is modified.
  NodeRange(const ChainBounds* begin_chain, const ChainBounds* end_chain,
            const int* first_node)
      : begin_chain_(begin_chain),
        end_chain_(end_chain),
        first_node_(first_node) {}
  Iterator begin() const { return {begin_chain_, first_node_}; }
  // Note: there is a sentinel value at the end of PathState::chains_,
  // so dereferencing chain_range_.end()->begin_ is always valid.
  Iterator end() const { return {end_chain_, first_node_}; }

 private:
  const ChainBounds* begin_chain_;
  const ChainBounds* end_chain_;
  const int* const first_node_;
};

// Make a filter that takes ownership of a PathState and synchronizes it with
// solver events. The solver represents a graph with array of variables 'nexts'.
// Solver events are embodied by Assignment* deltas, that are translated to node
// changes during Relax(), committed during Synchronize(), and reverted on
// Revert().
LocalSearchFilter* MakePathStateFilter(Solver* solver,
                                       std::unique_ptr<PathState> path_state,
                                       const std::vector<IntVar*>& nexts);

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
