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

#ifndef OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_INSERTION_LNS_H_
#define OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_INSERTION_LNS_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "ortools/constraint_solver/routing.h"
#include "ortools/constraint_solver/routing_search.h"
#include "ortools/util/bitset.h"

namespace operations_research {

/// Class of operators using a RoutingFilteredHeuristic to insert unperformed
/// nodes after changes have been made to the current solution.
// TODO(user): Put these methods in an object with helper methods instead
// of adding a layer to the class hierarchy.
class FilteredHeuristicLocalSearchOperator : public IntVarLocalSearchOperator {
 public:
  explicit FilteredHeuristicLocalSearchOperator(
      std::unique_ptr<RoutingFilteredHeuristic> heuristic,
      bool keep_inverse_values = false);
  ~FilteredHeuristicLocalSearchOperator() override {}

 protected:
  virtual bool IncrementPosition() = 0;
  /// Virtual method to return the next_accessor to be passed to the heuristic
  /// to build a new solution. This method should also correctly set the
  /// nodes being removed (if any) in removed_nodes_.
  virtual std::function<int64_t(int64_t)> SetupNextAccessorForNeighbor() = 0;

  std::string HeuristicName() const {
    std::string heuristic_name = heuristic_->DebugString();
    const int erase_pos = heuristic_name.find("FilteredHeuristic");
    if (erase_pos != std::string::npos) {
      const int expected_name_size = heuristic_name.size() - 17;
      heuristic_name.erase(erase_pos);
      // NOTE: Verify that the "FilteredHeuristic" string was at the end of the
      // heuristic name.
      DCHECK_EQ(heuristic_name.size(), expected_name_size);
    }
    return heuristic_name;
  }

  // TODO(user): Remove the dependency from RoutingModel by storing an
  // IntVarFilteredHeuristic here instead and storing information on path
  // start/ends like PathOperator does (instead of relying on the model).
  RoutingModel* const model_;
  /// Keeps track of removed nodes when making a neighbor.
  SparseBitset<> removed_nodes_;

 private:
  bool MakeOneNeighbor() override;
  bool MakeChangesAndInsertNodes();

  int64_t VehicleVarIndex(int64_t node) const { return model_->Size() + node; }

  const std::unique_ptr<RoutingFilteredHeuristic> heuristic_;
  const bool consider_vehicle_vars_;
};

/// LNS-like operator based on a filtered first solution heuristic to rebuild
/// the solution, after the destruction phase consisting of removing one route.
class FilteredHeuristicPathLNSOperator
    : public FilteredHeuristicLocalSearchOperator {
 public:
  explicit FilteredHeuristicPathLNSOperator(
      std::unique_ptr<RoutingFilteredHeuristic> heuristic);
  ~FilteredHeuristicPathLNSOperator() override {}

  std::string DebugString() const override {
    return absl::StrCat("HeuristicPathLNS(", HeuristicName(), ")");
  }

 private:
  void OnStart() override;

  bool IncrementPosition() override;
  bool CurrentRouteIsEmpty() const;
  void IncrementCurrentRouteToNextNonEmpty();

  std::function<int64_t(int64_t)> SetupNextAccessorForNeighbor() override;

  int current_route_;
  int last_route_;
  bool just_started_;
};

/// Heuristic-based local search operator which relocates an entire route to
/// an empty vehicle of different vehicle class and then tries to insert
/// unperformed nodes using the heuristic.
class RelocatePathAndHeuristicInsertUnperformedOperator
    : public FilteredHeuristicLocalSearchOperator {
 public:
  explicit RelocatePathAndHeuristicInsertUnperformedOperator(
      std::unique_ptr<RoutingFilteredHeuristic> heuristic);
  ~RelocatePathAndHeuristicInsertUnperformedOperator() override {}

  std::string DebugString() const override {
    return absl::StrCat("RelocatePathAndHeuristicInsertUnperformed(",
                        HeuristicName(), ")");
  }

 private:
  void OnStart() override;

  bool IncrementPosition() override;
  bool IncrementRoutes();

  std::function<int64_t(int64_t)> SetupNextAccessorForNeighbor() override;

  int route_to_relocate_index_;
  int last_route_to_relocate_index_;
  int empty_route_index_;
  int last_empty_route_index_;
  std::vector<int> routes_to_relocate_;
  std::vector<int> empty_routes_;
  std::vector<int64_t> last_node_on_route_;
  bool has_unperformed_nodes_;
  bool just_started_;
};

/// Similar to the heuristic path LNS above, but instead of removing one route
/// entirely, the destruction phase consists of removing all nodes on an
/// "expensive" chain from a route.
class FilteredHeuristicExpensiveChainLNSOperator
    : public FilteredHeuristicLocalSearchOperator {
 public:
  FilteredHeuristicExpensiveChainLNSOperator(
      std::unique_ptr<RoutingFilteredHeuristic> heuristic,
      int num_arcs_to_consider,
      std::function<int64_t(int64_t, int64_t, int64_t)>
          arc_cost_for_route_start);
  ~FilteredHeuristicExpensiveChainLNSOperator() override {}

  std::string DebugString() const override {
    return absl::StrCat("HeuristicExpensiveChainLNS(", HeuristicName(), ")");
  }

 private:
  void OnStart() override;

  bool IncrementPosition() override;
  bool IncrementRoute();
  bool IncrementCurrentArcIndices();
  bool FindMostExpensiveChainsOnRemainingRoutes();

  std::function<int64_t(int64_t)> SetupNextAccessorForNeighbor() override;

  int current_route_;
  int last_route_;

  const int num_arcs_to_consider_;
  std::vector<std::pair<int64_t, int>> most_expensive_arc_starts_and_ranks_;
  /// Indices in most_expensive_arc_starts_and_ranks_ corresponding to the first
  /// and second arcs currently being considered for removal.
  std::pair</*first_arc_index*/ int, /*second_arc_index*/ int>
      current_expensive_arc_indices_;
  std::function<int64_t(/*before_node*/ int64_t, /*after_node*/ int64_t,
                        /*path_start*/ int64_t)>
      arc_cost_for_route_start_;

  bool just_started_;
};

/// Filtered heuristic LNS operator, where the destruction phase consists of
/// removing a node and the 'num_close_nodes' nodes closest to it, along with
/// each of their corresponding sibling pickup/deliveries that are performed.
class FilteredHeuristicCloseNodesLNSOperator
    : public FilteredHeuristicLocalSearchOperator {
 public:
  FilteredHeuristicCloseNodesLNSOperator(
      std::unique_ptr<RoutingFilteredHeuristic> heuristic, int num_close_nodes);
  ~FilteredHeuristicCloseNodesLNSOperator() override {}

  std::string DebugString() const override {
    return absl::StrCat("HeuristicCloseNodesLNS(", HeuristicName(), ")");
  }

 private:
  void Initialize();

  void OnStart() override;

  bool IncrementPosition() override;

  std::function<int64_t(int64_t)> SetupNextAccessorForNeighbor() override;

  void RemoveNode(int64_t node);
  void RemoveNodeAndActiveSibling(int64_t node);

  bool IsActive(int64_t node) const {
    DCHECK_LT(node, model_->Size());
    return Value(node) != node && !removed_nodes_[node];
  }

  int64_t Prev(int64_t node) const {
    DCHECK_EQ(Value(InverseValue(node)), node);
    DCHECK_LT(node, new_prevs_.size());
    return changed_prevs_[node] ? new_prevs_[node] : InverseValue(node);
  }
  int64_t Next(int64_t node) const {
    DCHECK(!model_->IsEnd(node));
    return changed_nexts_[node] ? new_nexts_[node] : Value(node);
  }

  std::vector<int64_t> GetActiveSiblings(int64_t node) const;

  const std::vector<PickupDeliveryPair>& pickup_delivery_pairs_;

  int current_node_;
  int last_node_;
  bool just_started_;
  bool initialized_;

  std::vector<std::vector<int64_t>> close_nodes_;
  const int num_close_nodes_;
  /// Keep track of changes when making a neighbor.
  std::vector<int64_t> new_nexts_;
  SparseBitset<> changed_nexts_;
  std::vector<int64_t> new_prevs_;
  SparseBitset<> changed_prevs_;
};

}  // namespace operations_research

#endif  // OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_INSERTION_LNS_H_
