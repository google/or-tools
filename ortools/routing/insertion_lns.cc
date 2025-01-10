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

#include "ortools/routing/insertion_lns.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "ortools/base/int_type.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "ortools/routing/routing.h"
#include "ortools/routing/search.h"
#include "ortools/routing/utils.h"
#include "ortools/util/bitset.h"

namespace operations_research::routing {

// FilteredHeuristicLocalSearchOperator

FilteredHeuristicLocalSearchOperator::FilteredHeuristicLocalSearchOperator(
    std::unique_ptr<RoutingFilteredHeuristic> heuristic,
    bool keep_inverse_values)
    : IntVarLocalSearchOperator(heuristic->model()->Nexts(),
                                keep_inverse_values),
      model_(heuristic->model()),
      removed_nodes_(model_->Size()),
      heuristic_(std::move(heuristic)),
      consider_vehicle_vars_(!model_->CostsAreHomogeneousAcrossVehicles()) {
  if (consider_vehicle_vars_) {
    AddVars(model_->VehicleVars());
  }
}

bool FilteredHeuristicLocalSearchOperator::MakeOneNeighbor() {
  while (IncrementPosition()) {
    if (model_->CheckLimit()) {
      // NOTE: Even though the limit is checked in the BuildSolutionFromRoutes()
      // method of the heuristics, we still check it here to avoid calling
      // IncrementPosition() and building a solution for every possible position
      // if the time limit is reached.
      return false;
    }
    // NOTE: No need to call RevertChanges() here as MakeChangeAndInsertNodes()
    // will always return true if any change was made.
    if (MakeChangesAndInsertNodes()) {
      return true;
    }
  }
  return false;
}

bool FilteredHeuristicLocalSearchOperator::MakeChangesAndInsertNodes() {
  removed_nodes_.SparseClearAll();

  const std::function<int64_t(int64_t)> next_accessor =
      SetupNextAccessorForNeighbor();
  if (next_accessor == nullptr) {
    return false;
  }
  model_->solver()->set_context(DebugString());
  const Assignment* const result_assignment =
      heuristic_->BuildSolutionFromRoutes(next_accessor);
  model_->solver()->set_context("");

  if (result_assignment == nullptr) {
    return false;
  }

  bool has_change = false;
  const std::vector<IntVarElement>& elements =
      result_assignment->IntVarContainer().elements();
  for (int vehicle = 0; vehicle < model_->vehicles(); vehicle++) {
    int64_t node_index = model_->Start(vehicle);
    while (!model_->IsEnd(node_index)) {
      // NOTE: When building the solution in the heuristic, Next vars are added
      // to the assignment at the position corresponding to their index.
      const IntVarElement& node_element = elements[node_index];
      DCHECK_EQ(node_element.Var(), model_->NextVar(node_index));

      const int64_t new_node_value = node_element.Value();
      DCHECK_NE(new_node_value, node_index);

      const int64_t vehicle_var_index = VehicleVarIndex(node_index);
      if (OldValue(node_index) != new_node_value ||
          (consider_vehicle_vars_ && OldValue(vehicle_var_index) != vehicle)) {
        has_change = true;
        SetValue(node_index, new_node_value);
        if (consider_vehicle_vars_) {
          SetValue(vehicle_var_index, vehicle);
        }
      }
      node_index = new_node_value;
    }
  }
  // Check for newly unperformed nodes among the ones removed for insertion by
  // the heuristic.
  for (int64_t node : removed_nodes_.PositionsSetAtLeastOnce()) {
    const IntVarElement& node_element = elements[node];
    DCHECK_EQ(node_element.Var(), model_->NextVar(node));
    if (node_element.Value() == node) {
      DCHECK_NE(OldValue(node), node);
      has_change = true;
      SetValue(node, node);
      if (consider_vehicle_vars_) {
        const int64_t vehicle_var_index = VehicleVarIndex(node);
        DCHECK_NE(OldValue(vehicle_var_index), -1);
        SetValue(vehicle_var_index, -1);
      }
    }
  }
  return has_change;
}

// FilteredHeuristicPathLNSOperator

FilteredHeuristicPathLNSOperator::FilteredHeuristicPathLNSOperator(
    std::unique_ptr<RoutingFilteredHeuristic> heuristic)
    : FilteredHeuristicLocalSearchOperator(std::move(heuristic)),
      current_route_(0),
      last_route_(0),
      just_started_(false) {}

void FilteredHeuristicPathLNSOperator::OnStart() {
  // NOTE: We set last_route_ to current_route_ here to make sure all routes
  // are scanned in IncrementCurrentRouteToNextNonEmpty().
  last_route_ = current_route_;
  if (CurrentRouteIsEmpty()) {
    IncrementCurrentRouteToNextNonEmpty();
  }
  just_started_ = true;
}

bool FilteredHeuristicPathLNSOperator::IncrementPosition() {
  if (just_started_) {
    just_started_ = false;
    return !CurrentRouteIsEmpty();
  }
  IncrementCurrentRouteToNextNonEmpty();
  return current_route_ != last_route_;
}

bool FilteredHeuristicPathLNSOperator::CurrentRouteIsEmpty() const {
  return model_->IsEnd(OldValue(model_->Start(current_route_)));
}

void FilteredHeuristicPathLNSOperator::IncrementCurrentRouteToNextNonEmpty() {
  const int num_routes = model_->vehicles();
  do {
    ++current_route_ %= num_routes;
    if (current_route_ == last_route_) {
      // All routes have been scanned.
      return;
    }
  } while (CurrentRouteIsEmpty());
}

std::function<int64_t(int64_t)>
FilteredHeuristicPathLNSOperator::SetupNextAccessorForNeighbor() {
  const int64_t start_node = model_->Start(current_route_);
  const int64_t end_node = model_->End(current_route_);

  int64_t node = Value(start_node);
  while (node != end_node) {
    removed_nodes_.Set(node);
    node = Value(node);
  }

  return [this, start_node, end_node](int64_t node) {
    if (node == start_node) return end_node;
    return Value(node);
  };
}

// RelocatePathAndHeuristicInsertUnperformedOperator

RelocatePathAndHeuristicInsertUnperformedOperator::
    RelocatePathAndHeuristicInsertUnperformedOperator(
        std::unique_ptr<RoutingFilteredHeuristic> heuristic)
    : FilteredHeuristicLocalSearchOperator(std::move(heuristic)),
      route_to_relocate_index_(0),
      empty_route_index_(0),
      just_started_(false) {}

void RelocatePathAndHeuristicInsertUnperformedOperator::OnStart() {
  has_unperformed_nodes_ = false;
  last_node_on_route_.resize(model_->vehicles());
  routes_to_relocate_.clear();
  empty_routes_.clear();
  std::vector<bool> empty_vehicle_of_vehicle_class_added(
      model_->GetVehicleClassesCount(), false);
  for (int64_t node = 0; node < model_->Size(); node++) {
    const int64_t next = OldValue(node);
    if (next == node) {
      has_unperformed_nodes_ = true;
      continue;
    }
    if (model_->IsEnd(next)) {
      last_node_on_route_[model_->VehicleIndex(next)] = node;
    }
  }

  for (int vehicle = 0; vehicle < model_->vehicles(); vehicle++) {
    const int64_t next = OldValue(model_->Start(vehicle));
    if (!model_->IsEnd(next)) {
      routes_to_relocate_.push_back(vehicle);
      continue;
    }
    const int vehicle_class =
        model_->GetVehicleClassIndexOfVehicle(vehicle).value();
    if (!empty_vehicle_of_vehicle_class_added[vehicle_class]) {
      empty_routes_.push_back(vehicle);
      empty_vehicle_of_vehicle_class_added[vehicle_class] = true;
    }
  }

  if (empty_route_index_ >= empty_routes_.size()) {
    empty_route_index_ = 0;
  }
  if (route_to_relocate_index_ >= routes_to_relocate_.size()) {
    route_to_relocate_index_ = 0;
  }
  last_empty_route_index_ = empty_route_index_;
  last_route_to_relocate_index_ = route_to_relocate_index_;

  just_started_ = true;
}

bool RelocatePathAndHeuristicInsertUnperformedOperator::IncrementPosition() {
  if (!has_unperformed_nodes_ || empty_routes_.empty() ||
      routes_to_relocate_.empty()) {
    return false;
  }
  if (just_started_) {
    just_started_ = false;
    return true;
  }
  return IncrementRoutes();
}

bool RelocatePathAndHeuristicInsertUnperformedOperator::IncrementRoutes() {
  ++empty_route_index_ %= empty_routes_.size();
  if (empty_route_index_ != last_empty_route_index_) {
    return true;
  }
  ++route_to_relocate_index_ %= routes_to_relocate_.size();
  return route_to_relocate_index_ != last_route_to_relocate_index_;
}

std::function<int64_t(int64_t)>
RelocatePathAndHeuristicInsertUnperformedOperator::
    SetupNextAccessorForNeighbor() {
  const int empty_route = empty_routes_[empty_route_index_];
  const int relocated_route = routes_to_relocate_[route_to_relocate_index_];
  if (model_->GetVehicleClassIndexOfVehicle(empty_route) ==
      model_->GetVehicleClassIndexOfVehicle(relocated_route)) {
    // Don't try to relocate the route to an empty vehicle of the same class.
    return nullptr;
  }

  const int64_t empty_start_node = model_->Start(empty_route);
  const int64_t empty_end_node = model_->End(empty_route);

  const int64_t relocated_route_start = model_->Start(relocated_route);
  const int64_t first_relocated_node = OldValue(relocated_route_start);
  const int64_t last_relocated_node = last_node_on_route_[relocated_route];
  const int64_t relocated_route_end = model_->End(relocated_route);

  return [this, empty_start_node, empty_end_node, first_relocated_node,
          last_relocated_node, relocated_route_start,
          relocated_route_end](int64_t node) {
    if (node == relocated_route_start) return relocated_route_end;
    if (node == empty_start_node) return first_relocated_node;
    if (node == last_relocated_node) return empty_end_node;
    return Value(node);
  };
}

// FilteredHeuristicCloseNodesLNSOperator

FilteredHeuristicCloseNodesLNSOperator::FilteredHeuristicCloseNodesLNSOperator(
    std::unique_ptr<RoutingFilteredHeuristic> heuristic, int num_close_nodes)
    : FilteredHeuristicLocalSearchOperator(std::move(heuristic),
                                           /*keep_inverse_values*/ true),
      pickup_delivery_pairs_(model_->GetPickupAndDeliveryPairs()),
      current_node_(0),
      last_node_(0),
      just_started_(false),
      initialized_(false),
      close_nodes_(model_->Size()),
      num_close_nodes_(num_close_nodes),
      new_nexts_(model_->Size()),
      changed_nexts_(model_->Size()),
      new_prevs_(model_->Size()),
      changed_prevs_(model_->Size()) {}

void FilteredHeuristicCloseNodesLNSOperator::Initialize() {
  if (initialized_) return;
  initialized_ = true;
  const int64_t size = model_->Size();
  const int64_t max_num_neighbors =
      std::max<int64_t>(0, size - 1 - model_->vehicles());
  const int64_t num_closest_neighbors =
      std::min<int64_t>(num_close_nodes_, max_num_neighbors);
  DCHECK_GE(num_closest_neighbors, 0);

  if (num_closest_neighbors == 0) return;

  const int64_t num_cost_classes = model_->GetCostClassesCount();

  for (int64_t node = 0; node < size; node++) {
    if (model_->IsStart(node) || model_->IsEnd(node)) continue;

    std::vector<std::pair</*cost*/ double, /*node*/ int64_t>>
        costed_after_nodes;
    costed_after_nodes.reserve(size);
    for (int64_t after_node = 0; after_node < size; after_node++) {
      if (model_->IsStart(after_node) || model_->IsEnd(after_node) ||
          after_node == node) {
        continue;
      }
      double total_cost = 0.0;
      // NOTE: We don't consider the 'always-zero' cost class when searching for
      // closest neighbors.
      for (int cost_class = 1; cost_class < num_cost_classes; cost_class++) {
        total_cost += model_->GetArcCostForClass(node, after_node, cost_class);
      }
      costed_after_nodes.emplace_back(total_cost, after_node);
    }

    std::nth_element(costed_after_nodes.begin(),
                     costed_after_nodes.begin() + num_closest_neighbors - 1,
                     costed_after_nodes.end());
    std::vector<int64_t>& neighbors = close_nodes_[node];
    neighbors.reserve(num_closest_neighbors);
    for (int index = 0; index < num_closest_neighbors; index++) {
      neighbors.push_back(costed_after_nodes[index].second);
    }
  }
}

void FilteredHeuristicCloseNodesLNSOperator::OnStart() {
  Initialize();
  last_node_ = current_node_;
  just_started_ = true;
}

bool FilteredHeuristicCloseNodesLNSOperator::IncrementPosition() {
  DCHECK(initialized_);
  if (just_started_) {
    just_started_ = false;
    return true;
  }
  ++current_node_ %= model_->Size();
  return current_node_ != last_node_;
}

void FilteredHeuristicCloseNodesLNSOperator::RemoveNode(int64_t node) {
  DCHECK(!model_->IsEnd(node) && !model_->IsStart(node));
  DCHECK_NE(Value(node), node);
  DCHECK(IsActive(node));

  removed_nodes_.Set(node);
  const int64_t prev = Prev(node);
  const int64_t next = Next(node);
  changed_nexts_.Set(prev);
  new_nexts_[prev] = next;
  if (next < model_->Size()) {
    changed_prevs_.Set(next);
    new_prevs_[next] = prev;
  }
}

void FilteredHeuristicCloseNodesLNSOperator::RemoveNodeAndActiveSibling(
    int64_t node) {
  if (!IsActive(node)) return;
  RemoveNode(node);

  if (const std::optional<int64_t> sibling_node =
          model_->GetFirstMatchingPickupDeliverySibling(
              node,
              [this](int64_t node) {
                return IsActive(node) && !model_->IsStart(node) &&
                       !model_->IsEnd(node);
              });
      sibling_node.has_value()) {
    RemoveNode(sibling_node.value());
  }
}

std::function<int64_t(int64_t)>
FilteredHeuristicCloseNodesLNSOperator::SetupNextAccessorForNeighbor() {
  DCHECK(initialized_);
  if (model_->IsStart(current_node_)) {
    return nullptr;
  }
  DCHECK(!model_->IsEnd(current_node_));

  changed_nexts_.SparseClearAll();
  changed_prevs_.SparseClearAll();

  RemoveNodeAndActiveSibling(current_node_);

  for (int64_t neighbor : close_nodes_[current_node_]) {
    RemoveNodeAndActiveSibling(neighbor);
  }

  return [this](int64_t node) { return Next(node); };
}

// FilteredHeuristicExpensiveChainLNSOperator

FilteredHeuristicExpensiveChainLNSOperator::
    FilteredHeuristicExpensiveChainLNSOperator(
        std::unique_ptr<RoutingFilteredHeuristic> heuristic,
        int num_arcs_to_consider,
        std::function<int64_t(int64_t, int64_t, int64_t)>
            arc_cost_for_route_start)
    : FilteredHeuristicLocalSearchOperator(std::move(heuristic)),
      current_route_(0),
      last_route_(0),
      num_arcs_to_consider_(num_arcs_to_consider),
      current_expensive_arc_indices_({-1, -1}),
      arc_cost_for_route_start_(std::move(arc_cost_for_route_start)),
      just_started_(false) {
  DCHECK_GE(num_arcs_to_consider_, 2);
}

void FilteredHeuristicExpensiveChainLNSOperator::OnStart() {
  last_route_ = current_route_;
  just_started_ = true;
}

bool FilteredHeuristicExpensiveChainLNSOperator::IncrementPosition() {
  if (just_started_) {
    just_started_ = false;
    return FindMostExpensiveChainsOnRemainingRoutes();
  }

  if (IncrementCurrentArcIndices()) return true;

  return IncrementRoute() && FindMostExpensiveChainsOnRemainingRoutes();
}

std::function<int64_t(int64_t)>
FilteredHeuristicExpensiveChainLNSOperator::SetupNextAccessorForNeighbor() {
  const int first_arc_index = current_expensive_arc_indices_.first;
  const int second_arc_index = current_expensive_arc_indices_.second;
  DCHECK_LE(0, first_arc_index);
  DCHECK_LT(first_arc_index, second_arc_index);
  DCHECK_LT(second_arc_index, most_expensive_arc_starts_and_ranks_.size());

  const std::pair<int, int>& first_start_and_rank =
      most_expensive_arc_starts_and_ranks_[first_arc_index];
  const std::pair<int, int>& second_start_and_rank =
      most_expensive_arc_starts_and_ranks_[second_arc_index];
  int64_t before_chain, after_chain;
  if (first_start_and_rank.second < second_start_and_rank.second) {
    before_chain = first_start_and_rank.first;
    after_chain = OldValue(second_start_and_rank.first);
  } else {
    before_chain = second_start_and_rank.first;
    after_chain = OldValue(first_start_and_rank.first);
  }

  int node = Value(before_chain);
  while (node != after_chain) {
    removed_nodes_.Set(node);
    node = Value(node);
  }

  return [this, before_chain, after_chain](int64_t node) {
    if (node == before_chain) return after_chain;
    return OldValue(node);
  };
}

bool FilteredHeuristicExpensiveChainLNSOperator::IncrementRoute() {
  ++current_route_ %= model_->vehicles();
  return current_route_ != last_route_;
}

bool FilteredHeuristicExpensiveChainLNSOperator::IncrementCurrentArcIndices() {
  int& second_index = current_expensive_arc_indices_.second;
  if (++second_index < most_expensive_arc_starts_and_ranks_.size()) {
    return true;
  }
  int& first_index = current_expensive_arc_indices_.first;
  if (first_index + 2 < most_expensive_arc_starts_and_ranks_.size()) {
    first_index++;
    second_index = first_index + 1;
    return true;
  }
  return false;
}

bool FilteredHeuristicExpensiveChainLNSOperator::
    FindMostExpensiveChainsOnRemainingRoutes() {
  do {
    if (FindMostExpensiveArcsOnRoute(
            num_arcs_to_consider_, model_->Start(current_route_),
            [this](int64_t i) { return OldValue(i); },
            [this](int64_t node) { return model_->IsEnd(node); },
            arc_cost_for_route_start_, &most_expensive_arc_starts_and_ranks_,
            &current_expensive_arc_indices_)) {
      return true;
    }
  } while (IncrementRoute());

  return false;
}

}  // namespace operations_research::routing
