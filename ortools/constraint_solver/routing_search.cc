// Copyright 2010-2018 Google LLC
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

// Implementation of all classes related to routing and search.
// This includes decision builders, local search neighborhood operators
// and local search filters.
// TODO(user): Move all existing routing search code here.

#include <algorithm>
#include <cstdlib>
#include <map>
#include <numeric>
#include <set>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/iterator_adaptors.h"
#include "ortools/base/map_util.h"
#include "ortools/base/small_map.h"
#include "ortools/base/small_ordered_set.h"
#include "ortools/base/stl_util.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "ortools/constraint_solver/routing.h"
#include "ortools/constraint_solver/routing_lp_scheduling.h"
#include "ortools/graph/christofides.h"
#include "ortools/util/bitset.h"
#include "ortools/util/saturated_arithmetic.h"

DEFINE_bool(routing_strong_debug_checks, false,
            "Run stronger checks in debug; these stronger tests might change "
            "the complexity of the code in particular.");
DEFINE_bool(routing_shift_insertion_cost_by_penalty, true,
            "Shift insertion costs by the penalty of the inserted node(s).");

namespace operations_research {

namespace {

// Max active vehicles filter.

class MaxActiveVehiclesFilter : public IntVarLocalSearchFilter {
 public:
  explicit MaxActiveVehiclesFilter(const RoutingModel& routing_model)
      : IntVarLocalSearchFilter(routing_model.Nexts()),
        routing_model_(routing_model),
        is_active_(routing_model.vehicles(), false),
        active_vehicles_(0) {}
  bool Accept(const Assignment* delta, const Assignment* deltadelta,
              int64 objective_min, int64 objective_max) override {
    const int64 kUnassigned = -1;
    const Assignment::IntContainer& container = delta->IntVarContainer();
    const int delta_size = container.Size();
    int current_active_vehicles = active_vehicles_;
    for (int i = 0; i < delta_size; ++i) {
      const IntVarElement& new_element = container.Element(i);
      IntVar* const var = new_element.Var();
      int64 index = kUnassigned;
      if (FindIndex(var, &index) && routing_model_.IsStart(index)) {
        if (new_element.Min() != new_element.Max()) {
          // LNS detected.
          return true;
        }
        const int vehicle = routing_model_.VehicleIndex(index);
        const bool is_active =
            (new_element.Min() != routing_model_.End(vehicle));
        if (is_active && !is_active_[vehicle]) {
          ++current_active_vehicles;
        } else if (!is_active && is_active_[vehicle]) {
          --current_active_vehicles;
        }
      }
    }
    return current_active_vehicles <=
           routing_model_.GetMaximumNumberOfActiveVehicles();
  }

 private:
  void OnSynchronize(const Assignment* delta) override {
    active_vehicles_ = 0;
    for (int i = 0; i < routing_model_.vehicles(); ++i) {
      const int index = routing_model_.Start(i);
      if (IsVarSynced(index) && Value(index) != routing_model_.End(i)) {
        is_active_[i] = true;
        ++active_vehicles_;
      } else {
        is_active_[i] = false;
      }
    }
  }

  const RoutingModel& routing_model_;
  std::vector<bool> is_active_;
  int active_vehicles_;
};
}  // namespace

IntVarLocalSearchFilter* MakeMaxActiveVehiclesFilter(
    const RoutingModel& routing_model) {
  return routing_model.solver()->RevAlloc(
      new MaxActiveVehiclesFilter(routing_model));
}

namespace {

// Node disjunction filter class.

class NodeDisjunctionFilter : public IntVarLocalSearchFilter {
 public:
  explicit NodeDisjunctionFilter(const RoutingModel& routing_model)
      : IntVarLocalSearchFilter(routing_model.Nexts()),
        routing_model_(routing_model),
        active_per_disjunction_(routing_model.GetNumberOfDisjunctions(), 0),
        inactive_per_disjunction_(routing_model.GetNumberOfDisjunctions(), 0),
        synchronized_objective_value_(kint64min),
        accepted_objective_value_(kint64min) {}

  bool Accept(const Assignment* delta, const Assignment* deltadelta,
              int64 objective_min, int64 objective_max) override {
    const int64 kUnassigned = -1;
    const Assignment::IntContainer& container = delta->IntVarContainer();
    const int delta_size = container.Size();
    gtl::small_map<std::map<RoutingModel::DisjunctionIndex, int>>
        disjunction_active_deltas;
    gtl::small_map<std::map<RoutingModel::DisjunctionIndex, int>>
        disjunction_inactive_deltas;
    bool lns_detected = false;
    // Update active/inactive count per disjunction for each element of delta.
    for (int i = 0; i < delta_size; ++i) {
      const IntVarElement& new_element = container.Element(i);
      IntVar* const var = new_element.Var();
      int64 index = kUnassigned;
      if (FindIndex(var, &index)) {
        const bool is_inactive =
            (new_element.Min() <= index && new_element.Max() >= index);
        if (new_element.Min() != new_element.Max()) {
          lns_detected = true;
        }
        for (const RoutingModel::DisjunctionIndex disjunction_index :
             routing_model_.GetDisjunctionIndices(index)) {
          const bool active_state_changed =
              !IsVarSynced(index) || (Value(index) == index) != is_inactive;
          if (active_state_changed) {
            if (!is_inactive) {
              ++gtl::LookupOrInsert(&disjunction_active_deltas,
                                    disjunction_index, 0);
              if (IsVarSynced(index)) {
                --gtl::LookupOrInsert(&disjunction_inactive_deltas,
                                      disjunction_index, 0);
              }
            } else {
              ++gtl::LookupOrInsert(&disjunction_inactive_deltas,
                                    disjunction_index, 0);
              if (IsVarSynced(index)) {
                --gtl::LookupOrInsert(&disjunction_active_deltas,
                                      disjunction_index, 0);
              }
            }
          }
        }
      }
    }
    // Check if any disjunction has too many active nodes.
    for (const std::pair<RoutingModel::DisjunctionIndex, int>
             disjunction_active_delta : disjunction_active_deltas) {
      const int current_active_nodes =
          active_per_disjunction_[disjunction_active_delta.first];
      const int active_nodes =
          current_active_nodes + disjunction_active_delta.second;
      const int max_cardinality = routing_model_.GetDisjunctionMaxCardinality(
          disjunction_active_delta.first);
      // Too many active nodes.
      if (active_nodes > max_cardinality) {
        return false;
      }
    }
    // Update penalty costs for disjunctions.
    accepted_objective_value_ = synchronized_objective_value_;
    for (const std::pair<RoutingModel::DisjunctionIndex, int>
             disjunction_inactive_delta : disjunction_inactive_deltas) {
      const int64 penalty = routing_model_.GetDisjunctionPenalty(
          disjunction_inactive_delta.first);
      if (penalty != 0 && !lns_detected) {
        const RoutingModel::DisjunctionIndex disjunction_index =
            disjunction_inactive_delta.first;
        const int current_inactive_nodes =
            inactive_per_disjunction_[disjunction_index];
        const int inactive_nodes =
            current_inactive_nodes + disjunction_inactive_delta.second;
        const int max_inactive_cardinality =
            routing_model_.GetDisjunctionIndices(disjunction_index).size() -
            routing_model_.GetDisjunctionMaxCardinality(disjunction_index);
        // Too many inactive nodes.
        if (inactive_nodes > max_inactive_cardinality) {
          if (penalty < 0) {
            // Nodes are mandatory, i.e. exactly max_cardinality nodes must be
            // performed, so the move is not acceptable.
            return false;
          } else if (current_inactive_nodes <= max_inactive_cardinality) {
            // Add penalty if there were not too many inactive nodes before the
            // move.
            accepted_objective_value_ =
                CapAdd(accepted_objective_value_, penalty);
          }
        } else if (current_inactive_nodes > max_inactive_cardinality) {
          // Remove penalty if there were too many inactive nodes before the
          // move and there are not too many after the move.
          accepted_objective_value_ =
              CapSub(accepted_objective_value_, penalty);
        }
      }
    }
    if (lns_detected) {
      accepted_objective_value_ = 0;
      return true;
    } else {
      // Only compare to max as a cost lower bound is computed.
      return accepted_objective_value_ <= objective_max;
    }
  }
  std::string DebugString() const override { return "NodeDisjunctionFilter"; }
  int64 GetSynchronizedObjectiveValue() const override {
    return synchronized_objective_value_;
  }
  int64 GetAcceptedObjectiveValue() const override {
    return accepted_objective_value_;
  }

 private:
  void OnSynchronize(const Assignment* delta) override {
    synchronized_objective_value_ = 0;
    for (RoutingModel::DisjunctionIndex i(0);
         i < active_per_disjunction_.size(); ++i) {
      active_per_disjunction_[i] = 0;
      inactive_per_disjunction_[i] = 0;
      const std::vector<int64>& disjunction_indices =
          routing_model_.GetDisjunctionIndices(i);
      for (const int64 index : disjunction_indices) {
        const bool index_synced = IsVarSynced(index);
        if (index_synced) {
          if (Value(index) != index) {
            ++active_per_disjunction_[i];
          } else {
            ++inactive_per_disjunction_[i];
          }
        }
      }
      const int64 penalty = routing_model_.GetDisjunctionPenalty(i);
      const int max_cardinality =
          routing_model_.GetDisjunctionMaxCardinality(i);
      if (inactive_per_disjunction_[i] >
              disjunction_indices.size() - max_cardinality &&
          penalty > 0) {
        synchronized_objective_value_ =
            CapAdd(synchronized_objective_value_, penalty);
      }
    }
  }

  const RoutingModel& routing_model_;

  gtl::ITIVector<RoutingModel::DisjunctionIndex, int> active_per_disjunction_;
  gtl::ITIVector<RoutingModel::DisjunctionIndex, int> inactive_per_disjunction_;
  int64 synchronized_objective_value_;
  int64 accepted_objective_value_;
};
}  // namespace

IntVarLocalSearchFilter* MakeNodeDisjunctionFilter(
    const RoutingModel& routing_model) {
  return routing_model.solver()->RevAlloc(
      new NodeDisjunctionFilter(routing_model));
}

const int64 BasePathFilter::kUnassigned = -1;

BasePathFilter::BasePathFilter(const std::vector<IntVar*>& nexts,
                               int next_domain_size)
    : IntVarLocalSearchFilter(nexts),
      node_path_starts_(next_domain_size, kUnassigned),
      paths_(nexts.size(), -1),
      new_synchronized_unperformed_nodes_(nexts.size()),
      new_nexts_(nexts.size(), kUnassigned),
      touched_paths_(nexts.size()),
      touched_path_nodes_(next_domain_size),
      ranks_(next_domain_size, -1),
      status_(BasePathFilter::UNKNOWN) {}

bool BasePathFilter::Accept(const Assignment* delta,
                            const Assignment* deltadelta, int64 objective_min,
                            int64 objective_max) {
  if (IsDisabled()) return true;
  for (const int touched : delta_touched_) {
    new_nexts_[touched] = kUnassigned;
  }
  delta_touched_.clear();
  const Assignment::IntContainer& container = delta->IntVarContainer();
  const int delta_size = container.Size();
  delta_touched_.reserve(delta_size);
  // Determining touched paths and touched nodes (a node is touched if it
  // corresponds to an element of delta or that an element of delta points to
  // it.
  touched_paths_.SparseClearAll();
  touched_path_nodes_.SparseClearAll();
  for (int i = 0; i < delta_size; ++i) {
    const IntVarElement& new_element = container.Element(i);
    IntVar* const var = new_element.Var();
    int64 index = kUnassigned;
    if (FindIndex(var, &index)) {
      if (!new_element.Bound()) {
        // LNS detected
        return true;
      }
      new_nexts_[index] = new_element.Value();
      delta_touched_.push_back(index);
      const int64 start = node_path_starts_[index];
      touched_path_nodes_.Set(index);
      touched_path_nodes_.Set(new_nexts_[index]);
      if (start != kUnassigned) {
        touched_paths_.Set(start);
      }
    }
  }
  // Checking feasibility of touched paths.
  InitializeAcceptPath();
  bool accept = true;
  // Finding touched subchains from ranks of touched nodes in paths; the first
  // and last node of a subchain will have remained on the same path and will
  // correspond to the min and max ranks of touched nodes in the current
  // assignment.
  for (const int64 touched_start : touched_paths_.PositionsSetAtLeastOnce()) {
    int min_rank = kint32max;
    int64 start = kUnassigned;
    int max_rank = kint32min;
    int64 end = kUnassigned;
    // Linear search on touched nodes is ok since there shouldn't be many of
    // them.
    // TODO(user): Remove the linear loop.
    for (const int64 touched_path_node :
         touched_path_nodes_.PositionsSetAtLeastOnce()) {
      if (node_path_starts_[touched_path_node] == touched_start) {
        const int rank = ranks_[touched_path_node];
        if (rank < min_rank) {
          min_rank = rank;
          start = touched_path_node;
        }
        if (rank > max_rank) {
          max_rank = rank;
          end = touched_path_node;
        }
      }
    }
    if (!AcceptPath(touched_start, start, end)) {
      accept = false;
      break;
    }
  }
  // NOTE: FinalizeAcceptPath() is only called if all paths are accepted.
  return accept && FinalizeAcceptPath(delta, objective_min, objective_max);
}

void BasePathFilter::ComputePathStarts(std::vector<int64>* path_starts,
                                       std::vector<int>* index_to_path) {
  path_starts->clear();
  const int nexts_size = Size();
  index_to_path->assign(nexts_size, kUnassigned);
  Bitset64<> has_prevs(nexts_size);
  for (int i = 0; i < nexts_size; ++i) {
    if (!IsVarSynced(i)) {
      has_prevs.Set(i);
    } else {
      const int next = Value(i);
      if (next < nexts_size) {
        has_prevs.Set(next);
      }
    }
  }
  for (int i = 0; i < nexts_size; ++i) {
    if (!has_prevs[i]) {
      (*index_to_path)[i] = path_starts->size();
      path_starts->push_back(i);
    }
  }
}

bool BasePathFilter::HavePathsChanged() {
  std::vector<int64> path_starts;
  std::vector<int> index_to_path(Size(), kUnassigned);
  ComputePathStarts(&path_starts, &index_to_path);
  if (path_starts.size() != starts_.size()) {
    return true;
  }
  for (int i = 0; i < path_starts.size(); ++i) {
    if (path_starts[i] != starts_[i]) {
      return true;
    }
  }
  for (int i = 0; i < Size(); ++i) {
    if (index_to_path[i] != paths_[i]) {
      return true;
    }
  }
  return false;
}

void BasePathFilter::SynchronizeFullAssignment() {
  // Subclasses of BasePathFilter might not propagate injected objective values
  // so making sure it is done here (can be done again by the subclass if
  // needed).
  ComputePathStarts(&starts_, &paths_);
  for (int64 index = 0; index < Size(); index++) {
    if (IsVarSynced(index) && Value(index) == index &&
        node_path_starts_[index] != kUnassigned) {
      // index was performed before and is now unperformed.
      new_synchronized_unperformed_nodes_.Set(index);
    }
  }
  // Marking unactive nodes (which are not on a path).
  node_path_starts_.assign(node_path_starts_.size(), kUnassigned);
  // Marking nodes on a path and storing next values.
  const int nexts_size = Size();
  for (const int64 start : starts_) {
    int node = start;
    node_path_starts_[node] = start;
    DCHECK(IsVarSynced(node));
    int next = Value(node);
    while (next < nexts_size) {
      node = next;
      node_path_starts_[node] = start;
      DCHECK(IsVarSynced(node));
      next = Value(node);
    }
    node_path_starts_[next] = start;
  }
  OnBeforeSynchronizePaths();
  UpdateAllRanks();
  OnAfterSynchronizePaths();
}

void BasePathFilter::OnSynchronize(const Assignment* delta) {
  if (status_ == BasePathFilter::UNKNOWN) {
    status_ =
        DisableFiltering() ? BasePathFilter::DISABLED : BasePathFilter::ENABLED;
  }
  if (IsDisabled()) return;
  new_synchronized_unperformed_nodes_.ClearAll();
  if (delta == nullptr || delta->Empty() || starts_.empty()) {
    SynchronizeFullAssignment();
    return;
  }
  // Subclasses of BasePathFilter might not propagate injected objective values
  // so making sure it is done here (can be done again by the subclass if
  // needed).
  // This code supposes that path starts didn't change.
  DCHECK(!FLAGS_routing_strong_debug_checks || !HavePathsChanged());
  const Assignment::IntContainer& container = delta->IntVarContainer();
  touched_paths_.SparseClearAll();
  for (int i = 0; i < container.Size(); ++i) {
    const IntVarElement& new_element = container.Element(i);
    int64 index = kUnassigned;
    if (FindIndex(new_element.Var(), &index)) {
      const int64 start = node_path_starts_[index];
      if (start != kUnassigned) {
        touched_paths_.Set(start);
        if (Value(index) == index) {
          // New unperformed node (its previous start isn't unassigned).
          DCHECK_LT(index, new_nexts_.size());
          new_synchronized_unperformed_nodes_.Set(index);
          node_path_starts_[index] = kUnassigned;
        }
      }
    }
  }
  OnBeforeSynchronizePaths();
  for (const int64 touched_start : touched_paths_.PositionsSetAtLeastOnce()) {
    int64 node = touched_start;
    while (node < Size()) {
      node_path_starts_[node] = touched_start;
      node = Value(node);
    }
    node_path_starts_[node] = touched_start;
    UpdatePathRanksFromStart(touched_start);
    OnSynchronizePathFromStart(touched_start);
  }
  OnAfterSynchronizePaths();
}

void BasePathFilter::UpdateAllRanks() {
  for (int i = 0; i < ranks_.size(); ++i) {
    ranks_[i] = kUnassigned;
  }
  for (int r = 0; r < NumPaths(); ++r) {
    UpdatePathRanksFromStart(Start(r));
    OnSynchronizePathFromStart(Start(r));
  }
}

void BasePathFilter::UpdatePathRanksFromStart(int start) {
  int rank = 0;
  int64 node = start;
  while (node < Size()) {
    ranks_[node] = rank;
    rank++;
    node = Value(node);
  }
  ranks_[node] = rank;
}

namespace {

class VehicleAmortizedCostFilter : public BasePathFilter {
 public:
  explicit VehicleAmortizedCostFilter(const RoutingModel& routing_model);
  ~VehicleAmortizedCostFilter() override {}
  std::string DebugString() const override {
    return "VehicleAmortizedCostFilter";
  }
  int64 GetSynchronizedObjectiveValue() const override {
    return current_vehicle_cost_;
  }
  int64 GetAcceptedObjectiveValue() const override {
    return delta_vehicle_cost_;
  }

 private:
  void OnSynchronizePathFromStart(int64 start) override;
  void OnAfterSynchronizePaths() override;
  void InitializeAcceptPath() override;
  bool AcceptPath(int64 path_start, int64 chain_start,
                  int64 chain_end) override;
  bool FinalizeAcceptPath(const Assignment* delta, int64 objective_min,
                          int64 objective_max) override;

  int64 current_vehicle_cost_;
  int64 delta_vehicle_cost_;
  std::vector<int> current_route_lengths_;
  std::vector<int64> start_to_end_;
  std::vector<int> start_to_vehicle_;
  std::vector<int64> vehicle_to_start_;
  const std::vector<int64>& linear_cost_factor_of_vehicle_;
  const std::vector<int64>& quadratic_cost_factor_of_vehicle_;
};

VehicleAmortizedCostFilter::VehicleAmortizedCostFilter(
    const RoutingModel& routing_model)
    : BasePathFilter(routing_model.Nexts(),
                     routing_model.Size() + routing_model.vehicles()),
      current_vehicle_cost_(0),
      delta_vehicle_cost_(0),
      current_route_lengths_(Size(), -1),
      linear_cost_factor_of_vehicle_(
          routing_model.GetAmortizedLinearCostFactorOfVehicles()),
      quadratic_cost_factor_of_vehicle_(
          routing_model.GetAmortizedQuadraticCostFactorOfVehicles()) {
  start_to_end_.resize(Size(), -1);
  start_to_vehicle_.resize(Size(), -1);
  vehicle_to_start_.resize(routing_model.vehicles());
  for (int v = 0; v < routing_model.vehicles(); v++) {
    const int64 start = routing_model.Start(v);
    start_to_vehicle_[start] = v;
    start_to_end_[start] = routing_model.End(v);
    vehicle_to_start_[v] = start;
  }
}

void VehicleAmortizedCostFilter::OnSynchronizePathFromStart(int64 start) {
  const int64 end = start_to_end_[start];
  CHECK_GE(end, 0);
  const int route_length = Rank(end) - 1;
  CHECK_GE(route_length, 0);
  current_route_lengths_[start] = route_length;
}

void VehicleAmortizedCostFilter::OnAfterSynchronizePaths() {
  current_vehicle_cost_ = 0;
  for (int vehicle = 0; vehicle < vehicle_to_start_.size(); vehicle++) {
    const int64 start = vehicle_to_start_[vehicle];
    DCHECK_EQ(vehicle, start_to_vehicle_[start]);

    const int route_length = current_route_lengths_[start];
    DCHECK_GE(route_length, 0);

    if (route_length == 0) {
      // The path is empty.
      continue;
    }

    const int64 linear_cost_factor = linear_cost_factor_of_vehicle_[vehicle];
    const int64 route_length_cost =
        CapProd(quadratic_cost_factor_of_vehicle_[vehicle],
                route_length * route_length);

    current_vehicle_cost_ = CapAdd(
        current_vehicle_cost_, CapSub(linear_cost_factor, route_length_cost));
  }
}

void VehicleAmortizedCostFilter::InitializeAcceptPath() {
  delta_vehicle_cost_ = current_vehicle_cost_;
}

bool VehicleAmortizedCostFilter::AcceptPath(int64 path_start, int64 chain_start,
                                            int64 chain_end) {
  // Number of nodes previously between chain_start and chain_end
  const int previous_chain_nodes = Rank(chain_end) - 1 - Rank(chain_start);
  CHECK_GE(previous_chain_nodes, 0);
  int new_chain_nodes = 0;
  int64 node = GetNext(chain_start);
  while (node != chain_end) {
    new_chain_nodes++;
    node = GetNext(node);
  }

  const int previous_route_length = current_route_lengths_[path_start];
  CHECK_GE(previous_route_length, 0);
  const int new_route_length =
      previous_route_length - previous_chain_nodes + new_chain_nodes;

  const int vehicle = start_to_vehicle_[path_start];
  CHECK_GE(vehicle, 0);
  DCHECK_EQ(path_start, vehicle_to_start_[vehicle]);

  // Update the cost related to used vehicles.
  // TODO(user): Handle possible overflows.
  if (previous_route_length == 0) {
    // The route was empty before, it is no longer the case (changed path).
    CHECK_GT(new_route_length, 0);
    delta_vehicle_cost_ =
        CapAdd(delta_vehicle_cost_, linear_cost_factor_of_vehicle_[vehicle]);
  } else if (new_route_length == 0) {
    // The route is now empty.
    delta_vehicle_cost_ =
        CapSub(delta_vehicle_cost_, linear_cost_factor_of_vehicle_[vehicle]);
  }

  // Update the cost related to the sum of the squares of the route lengths.
  const int64 quadratic_cost_factor =
      quadratic_cost_factor_of_vehicle_[vehicle];
  delta_vehicle_cost_ =
      CapAdd(delta_vehicle_cost_,
             CapProd(quadratic_cost_factor,
                     previous_route_length * previous_route_length));
  delta_vehicle_cost_ = CapSub(
      delta_vehicle_cost_,
      CapProd(quadratic_cost_factor, new_route_length * new_route_length));

  return true;
}

bool VehicleAmortizedCostFilter::FinalizeAcceptPath(const Assignment* delta,
                                                    int64 objective_min,
                                                    int64 objective_max) {
  return delta_vehicle_cost_ <= objective_max;
}

}  // namespace

IntVarLocalSearchFilter* MakeVehicleAmortizedCostFilter(
    const RoutingModel& routing_model) {
  return routing_model.solver()->RevAlloc(
      new VehicleAmortizedCostFilter(routing_model));
}

namespace {

class TypeRegulationsFilter : public BasePathFilter {
 public:
  explicit TypeRegulationsFilter(const RoutingModel& model);
  ~TypeRegulationsFilter() override {}
  std::string DebugString() const override { return "TypeRegulationsFilter"; }

 private:
  void OnSynchronizePathFromStart(int64 start) override;
  bool AcceptPath(int64 path_start, int64 chain_start,
                  int64 chain_end) override;

  bool HardIncompatibilitiesRespected(int vehicle, int64 chain_start,
                                      int64 chain_end);

  const RoutingModel& routing_model_;
  std::vector<int> start_to_vehicle_;
  // The following vector is used to keep track of the type counts for hard
  // incompatibilities.
  std::vector<std::vector<int>> hard_incompatibility_type_counts_per_vehicle_;
  // Used to verify the temporal incompatibilities and requirements.
  TypeIncompatibilityChecker temporal_incompatibility_checker_;
  TypeRequirementChecker requirement_checker_;
};

TypeRegulationsFilter::TypeRegulationsFilter(const RoutingModel& model)
    : BasePathFilter(model.Nexts(), model.Size() + model.vehicles()),
      routing_model_(model),
      start_to_vehicle_(model.Size(), -1),
      temporal_incompatibility_checker_(model,
                                        /*check_hard_incompatibilities*/ false),
      requirement_checker_(model) {
  const int num_vehicles = model.vehicles();
  const bool has_hard_type_incompatibilities =
      model.HasHardTypeIncompatibilities();
  if (has_hard_type_incompatibilities) {
    hard_incompatibility_type_counts_per_vehicle_.resize(num_vehicles);
  }
  const int num_visit_types = model.GetNumberOfVisitTypes();
  for (int vehicle = 0; vehicle < num_vehicles; vehicle++) {
    const int64 start = model.Start(vehicle);
    start_to_vehicle_[start] = vehicle;
    if (has_hard_type_incompatibilities) {
      hard_incompatibility_type_counts_per_vehicle_[vehicle].resize(
          num_visit_types, 0);
    }
  }
}

void TypeRegulationsFilter::OnSynchronizePathFromStart(int64 start) {
  if (!routing_model_.HasHardTypeIncompatibilities()) return;

  const int vehicle = start_to_vehicle_[start];
  CHECK_GE(vehicle, 0);
  std::vector<int>& type_counts =
      hard_incompatibility_type_counts_per_vehicle_[vehicle];
  std::fill(type_counts.begin(), type_counts.end(), 0);
  const int num_types = type_counts.size();

  int64 node = start;
  while (node < Size()) {
    DCHECK(IsVarSynced(node));
    const int type = routing_model_.GetVisitType(node);
    if (type >= 0 && routing_model_.GetVisitTypePolicy(node) !=
                         RoutingModel::ADDED_TYPE_REMOVED_FROM_VEHICLE) {
      CHECK_LT(type, num_types);
      type_counts[type]++;
    }
    node = Value(node);
  }
}

bool TypeRegulationsFilter::HardIncompatibilitiesRespected(int vehicle,
                                                           int64 chain_start,
                                                           int64 chain_end) {
  if (!routing_model_.HasHardTypeIncompatibilities()) return true;

  const std::vector<int>& previous_type_counts =
      hard_incompatibility_type_counts_per_vehicle_[vehicle];

  absl::flat_hash_map</*type*/ int, /*new_count*/ int> new_type_counts;
  absl::flat_hash_set<int> types_to_check;

  // Go through the new nodes on the path and increment their type counts.
  int64 node = GetNext(chain_start);
  while (node != chain_end) {
    const int type = routing_model_.GetVisitType(node);
    if (type >= 0 && routing_model_.GetVisitTypePolicy(node) !=
                         RoutingModel::ADDED_TYPE_REMOVED_FROM_VEHICLE) {
      DCHECK_LT(type, previous_type_counts.size());
      int& type_count = gtl::LookupOrInsert(&new_type_counts, type,
                                            previous_type_counts[type]);
      if (type_count++ == 0) {
        // New type on the route, mark to check its incompatibilities.
        types_to_check.insert(type);
      }
    }
    node = GetNext(node);
  }

  // Update new_type_counts by decrementing the occurrence of the types of the
  // nodes no longer on the route.
  node = Value(chain_start);
  while (node != chain_end) {
    const int type = routing_model_.GetVisitType(node);
    if (type >= 0 && routing_model_.GetVisitTypePolicy(node) !=
                         RoutingModel::ADDED_TYPE_REMOVED_FROM_VEHICLE) {
      DCHECK_LT(type, previous_type_counts.size());
      int& type_count = gtl::LookupOrInsert(&new_type_counts, type,
                                            previous_type_counts[type]);
      CHECK_GE(type_count, 1);
      type_count--;
    }
    node = Value(node);
  }

  // Check the incompatibilities for types in types_to_check.
  for (int type : types_to_check) {
    for (int incompatible_type :
         routing_model_.GetHardTypeIncompatibilitiesOfType(type)) {
      if (gtl::FindWithDefault(new_type_counts, incompatible_type,
                               previous_type_counts[incompatible_type]) > 0) {
        return false;
      }
    }
  }
  return true;
}

bool TypeRegulationsFilter::AcceptPath(int64 path_start, int64 chain_start,
                                       int64 chain_end) {
  const int vehicle = start_to_vehicle_[path_start];
  CHECK_GE(vehicle, 0);
  const auto next_accessor = [this](int64 node) { return GetNext(node); };
  return HardIncompatibilitiesRespected(vehicle, chain_start, chain_end) &&
         temporal_incompatibility_checker_.CheckVehicle(vehicle,
                                                        next_accessor) &&
         requirement_checker_.CheckVehicle(vehicle, next_accessor);
}

}  // namespace

IntVarLocalSearchFilter* MakeTypeRegulationsFilter(
    const RoutingModel& routing_model) {
  return routing_model.solver()->RevAlloc(
      new TypeRegulationsFilter(routing_model));
}

namespace {

// ChainCumul filter. Version of dimension path filter which is O(delta) rather
// than O(length of touched paths). Currently only supports dimensions without
// costs (global and local span cost, soft bounds) and with unconstrained
// cumul variables except overall capacity and cumul variables of path ends.

class ChainCumulFilter : public BasePathFilter {
 public:
  ChainCumulFilter(const RoutingModel& routing_model,
                   const RoutingDimension& dimension);
  ~ChainCumulFilter() override {}
  std::string DebugString() const override {
    return "ChainCumulFilter(" + name_ + ")";
  }

 private:
  void OnSynchronizePathFromStart(int64 start) override;
  bool AcceptPath(int64 path_start, int64 chain_start,
                  int64 chain_end) override;

  const std::vector<IntVar*> cumuls_;
  std::vector<int64> start_to_vehicle_;
  std::vector<int64> start_to_end_;
  std::vector<const RoutingModel::TransitCallback2*> evaluators_;
  const std::vector<int64> vehicle_capacities_;
  std::vector<int64> current_path_cumul_mins_;
  std::vector<int64> current_max_of_path_end_cumul_mins_;
  std::vector<int64> old_nexts_;
  std::vector<int> old_vehicles_;
  std::vector<int64> current_transits_;
  const std::string name_;
};

ChainCumulFilter::ChainCumulFilter(const RoutingModel& routing_model,
                                   const RoutingDimension& dimension)
    : BasePathFilter(routing_model.Nexts(), dimension.cumuls().size()),
      cumuls_(dimension.cumuls()),
      evaluators_(routing_model.vehicles(), nullptr),
      vehicle_capacities_(dimension.vehicle_capacities()),
      current_path_cumul_mins_(dimension.cumuls().size(), 0),
      current_max_of_path_end_cumul_mins_(dimension.cumuls().size(), 0),
      old_nexts_(routing_model.Size(), kUnassigned),
      old_vehicles_(routing_model.Size(), kUnassigned),
      current_transits_(routing_model.Size(), 0),
      name_(dimension.name()) {
  start_to_vehicle_.resize(Size(), -1);
  start_to_end_.resize(Size(), -1);
  for (int i = 0; i < routing_model.vehicles(); ++i) {
    start_to_vehicle_[routing_model.Start(i)] = i;
    start_to_end_[routing_model.Start(i)] = routing_model.End(i);
    evaluators_[i] = &dimension.transit_evaluator(i);
  }
}

// On synchronization, maintain "propagated" cumul mins and max level of cumul
// from each node to the end of the path; to be used by AcceptPath to
// incrementally check feasibility.
void ChainCumulFilter::OnSynchronizePathFromStart(int64 start) {
  const int vehicle = start_to_vehicle_[start];
  std::vector<int64> path_nodes;
  int64 node = start;
  int64 cumul = cumuls_[node]->Min();
  while (node < Size()) {
    path_nodes.push_back(node);
    current_path_cumul_mins_[node] = cumul;
    const int64 next = Value(node);
    if (next != old_nexts_[node] || vehicle != old_vehicles_[node]) {
      old_nexts_[node] = next;
      old_vehicles_[node] = vehicle;
      current_transits_[node] = (*evaluators_[vehicle])(node, next);
    }
    cumul = CapAdd(cumul, current_transits_[node]);
    cumul = std::max(cumuls_[next]->Min(), cumul);
    node = next;
  }
  path_nodes.push_back(node);
  current_path_cumul_mins_[node] = cumul;
  int64 max_cumuls = cumul;
  for (int i = path_nodes.size() - 1; i >= 0; --i) {
    const int64 node = path_nodes[i];
    max_cumuls = std::max(max_cumuls, current_path_cumul_mins_[node]);
    current_max_of_path_end_cumul_mins_[node] = max_cumuls;
  }
}

// The complexity of the method is O(size of chain (chain_start...chain_end).
bool ChainCumulFilter::AcceptPath(int64 path_start, int64 chain_start,
                                  int64 chain_end) {
  const int vehicle = start_to_vehicle_[path_start];
  const int64 capacity = vehicle_capacities_[vehicle];
  int64 node = chain_start;
  int64 cumul = current_path_cumul_mins_[node];
  while (node != chain_end) {
    const int64 next = GetNext(node);
    if (IsVarSynced(node) && next == Value(node) &&
        vehicle == old_vehicles_[node]) {
      cumul = CapAdd(cumul, current_transits_[node]);
    } else {
      cumul = CapAdd(cumul, (*evaluators_[vehicle])(node, next));
    }
    cumul = std::max(cumuls_[next]->Min(), cumul);
    if (cumul > capacity) return false;
    node = next;
  }
  const int64 end = start_to_end_[path_start];
  const int64 end_cumul_delta =
      CapSub(current_path_cumul_mins_[end], current_path_cumul_mins_[node]);
  const int64 after_chain_cumul_delta =
      CapSub(current_max_of_path_end_cumul_mins_[node],
             current_path_cumul_mins_[node]);
  return CapAdd(cumul, after_chain_cumul_delta) <= capacity &&
         CapAdd(cumul, end_cumul_delta) <= cumuls_[end]->Max();
}

// PathCumul filter.

class PathCumulFilter : public BasePathFilter {
 public:
  PathCumulFilter(const RoutingModel& routing_model,
                  const RoutingDimension& dimension,
                  const RoutingSearchParameters& parameters,
                  bool propagate_own_objective_value,
                  bool filter_objective_cost, bool can_use_lp);
  ~PathCumulFilter() override {}
  std::string DebugString() const override {
    return "PathCumulFilter(" + name_ + ")";
  }
  int64 GetSynchronizedObjectiveValue() const override {
    return propagate_own_objective_value_ ? synchronized_objective_value_ : 0;
  }
  int64 GetAcceptedObjectiveValue() const override {
    return propagate_own_objective_value_ ? accepted_objective_value_ : 0;
  }

 private:
  // This structure stores the "best" path cumul value for a solution, the path
  // supporting this value, and the corresponding path cumul values for all
  // paths.
  struct SupportedPathCumul {
    SupportedPathCumul() : cumul_value(0), cumul_value_support(0) {}
    int64 cumul_value;
    int cumul_value_support;
    std::vector<int64> path_values;
  };

  struct SoftBound {
    SoftBound() : bound(-1), coefficient(0) {}
    int64 bound;
    int64 coefficient;
  };

  // This class caches transit values between nodes of paths. Transit and path
  // nodes are to be added in the order in which they appear on a path.
  class PathTransits {
   public:
    void Clear() {
      paths_.clear();
      transits_.clear();
    }
    void ClearPath(int path) {
      paths_[path].clear();
      transits_[path].clear();
    }
    int AddPaths(int num_paths) {
      const int first_path = paths_.size();
      paths_.resize(first_path + num_paths);
      transits_.resize(first_path + num_paths);
      return first_path;
    }
    void ReserveTransits(int path, int number_of_route_arcs) {
      transits_[path].reserve(number_of_route_arcs);
      paths_[path].reserve(number_of_route_arcs + 1);
    }
    // Stores the transit between node and next on path. For a given non-empty
    // path, node must correspond to next in the previous call to PushTransit.
    void PushTransit(int path, int node, int next, int64 transit) {
      transits_[path].push_back(transit);
      if (paths_[path].empty()) {
        paths_[path].push_back(node);
      }
      DCHECK_EQ(paths_[path].back(), node);
      paths_[path].push_back(next);
    }
    int NumPaths() const { return paths_.size(); }
    int PathSize(int path) const { return paths_[path].size(); }
    int Node(int path, int position) const { return paths_[path][position]; }
    int64 Transit(int path, int position) const {
      return transits_[path][position];
    }

   private:
    // paths_[r][i] is the ith node on path r.
    std::vector<std::vector<int64>> paths_;
    // transits_[r][i] is the transit value between nodes path_[i] and
    // path_[i+1] on path r.
    std::vector<std::vector<int64>> transits_;
  };

  void InitializeAcceptPath() override {
    cumul_cost_delta_ = total_current_cumul_cost_value_;
    node_with_precedence_to_delta_min_max_cumuls_.clear();
    // Cleaning up for the new delta.
    delta_max_end_cumul_ = kint64min;
    delta_paths_.clear();
    delta_path_transits_.Clear();
    lns_detected_ = false;
    delta_nodes_with_precedences_and_changed_cumul_.ClearAll();
  }
  bool AcceptPath(int64 path_start, int64 chain_start,
                  int64 chain_end) override;
  bool FinalizeAcceptPath(const Assignment* delta, int64 objective_min,
                          int64 objective_max) override;
  void OnBeforeSynchronizePaths() override;

  bool FilterSpanCost() const { return global_span_cost_coefficient_ != 0; }

  bool FilterSlackCost() const {
    return has_nonzero_vehicle_span_cost_coefficients_ ||
           has_vehicle_span_upper_bounds_;
  }

  bool FilterBreakCost(int vehicle) const {
    return dimension_.HasBreakConstraints() &&
           !dimension_.GetBreakIntervalsOfVehicle(vehicle).empty();
  }

  bool FilterCumulSoftBounds() const { return !cumul_soft_bounds_.empty(); }

  int64 GetCumulSoftCost(int64 node, int64 cumul_value) const;

  bool FilterCumulPiecewiseLinearCosts() const {
    return !cumul_piecewise_linear_costs_.empty();
  }

  bool FilterWithDimensionCumulOptimizerForVehicle(int vehicle) const {
    if (!can_use_lp_ || FilterCumulPiecewiseLinearCosts()) {
      return false;
    }

    int num_linear_constraints = 0;
    if (dimension_.GetSpanCostCoefficientForVehicle(vehicle) > 0)
      ++num_linear_constraints;
    if (FilterSoftSpanCost(vehicle)) ++num_linear_constraints;
    if (FilterCumulSoftLowerBounds()) ++num_linear_constraints;
    if (FilterCumulSoftBounds()) ++num_linear_constraints;
    if (vehicle_span_upper_bounds_[vehicle] < kint64max)
      ++num_linear_constraints;
    const bool has_breaks = FilterBreakCost(vehicle);
    if (has_breaks) ++num_linear_constraints;

    // The DimensionCumulOptimizer is used to compute a more precise value of
    // the cost related to the cumul values (soft bounds and span costs).
    // It is also used to garantee feasibility with complex mixes of constraints
    // and in particular in the presence of break requests along other
    // constraints.
    // Therefore, without breaks, we only use the optimizer when the costs are
    // actually used to filter the solutions, i.e. when filter_objective_cost_
    // is true.
    return num_linear_constraints >= 2 &&
           (has_breaks || filter_objective_cost_);
  }

  bool FilterDimensionForbiddenIntervals() const {
    for (const SortedDisjointIntervalList& intervals :
         dimension_.forbidden_intervals()) {
      // TODO(user): Change the following test to check intervals within
      // the domain of the corresponding variables.
      if (intervals.NumIntervals() > 0) {
        return true;
      }
    }
    return false;
  }

  int64 GetCumulPiecewiseLinearCost(int64 node, int64 cumul_value) const;

  bool FilterCumulSoftLowerBounds() const {
    return !cumul_soft_lower_bounds_.empty();
  }

  bool FilterPrecedences() const { return !node_index_to_precedences_.empty(); }

  bool FilterSoftSpanCost() const {
    return dimension_.HasSoftSpanUpperBounds();
  }
  bool FilterSoftSpanCost(int vehicle) const {
    return dimension_.HasSoftSpanUpperBounds() &&
           dimension_.GetSoftSpanUpperBoundForVehicle(vehicle).cost > 0;
  }
  bool FilterSoftSpanQuadraticCost() const {
    return dimension_.HasQuadraticCostSoftSpanUpperBounds();
  }
  bool FilterSoftSpanQuadraticCost(int vehicle) const {
    return dimension_.HasQuadraticCostSoftSpanUpperBounds() &&
           dimension_.GetQuadraticCostSoftSpanUpperBoundForVehicle(vehicle)
                   .cost > 0;
  }

  int64 GetCumulSoftLowerBoundCost(int64 node, int64 cumul_value) const;

  int64 GetPathCumulSoftLowerBoundCost(const PathTransits& path_transits,
                                       int path) const;

  void InitializeSupportedPathCumul(SupportedPathCumul* supported_cumul,
                                    int64 default_value);

  // Given the vector of minimum cumuls on the path, determines if the pickup to
  // delivery limits for this dimension (if there are any) can be respected by
  // this path.
  // Returns true if for every pickup/delivery nodes visited on this path,
  // min_cumul_value(delivery) - max_cumul_value(pickup) is less than the limit
  // set for this pickup to delivery.
  // TODO(user): Verify if we should filter the pickup/delivery limits using
  // the LP, for a perfect filtering.
  bool PickupToDeliveryLimitsRespected(
      const PathTransits& path_transits, int path,
      const std::vector<int64>& min_path_cumuls) const;

  // Computes the maximum cumul value of nodes along the path using
  // [current|delta]_path_transits_, and stores the min/max cumul
  // related to each node in the corresponding vector
  // [current|delta]_[min|max]_node_cumuls_.
  // The boolean is_delta indicates if the computations should take place on the
  // "delta" or "current" members. When true, the nodes for which the min/max
  // cumul has changed from the current value are marked in
  // delta_nodes_with_precedences_and_changed_cumul_.
  void StoreMinMaxCumulOfNodesOnPath(int path,
                                     const std::vector<int64>& min_path_cumuls,
                                     bool is_delta);

  // Compute the max start cumul value for a given path and a given minimal end
  // cumul value.
  // NOTE: Since this function is used to compute a lower bound on the span of
  // the routes, we don't "jump" over the forbidden intervals with this min end
  // cumul value. We do however concurrently compute the max possible start
  // given the max end cumul, for which we can "jump" over forbidden intervals,
  // and return the minimum of the two.
  int64 ComputePathMaxStartFromEndCumul(const PathTransits& path_transits,
                                        int path, int64 path_start,
                                        int64 min_end_cumul) const;

  const RoutingModel& routing_model_;
  const RoutingDimension& dimension_;
  const std::vector<IntVar*> cumuls_;
  const std::vector<IntVar*> slacks_;
  std::vector<int64> start_to_vehicle_;
  std::vector<const RoutingModel::TransitCallback2*> evaluators_;
  std::vector<int64> vehicle_span_upper_bounds_;
  bool has_vehicle_span_upper_bounds_;
  int64 total_current_cumul_cost_value_;
  int64 synchronized_objective_value_;
  int64 accepted_objective_value_;
  // Map between paths and path soft cumul bound costs. The paths are indexed
  // by the index of the start node of the path.
  absl::flat_hash_map<int64, int64> current_cumul_cost_values_;
  int64 cumul_cost_delta_;
  // Cumul cost values for paths in delta, indexed by vehicle.
  std::vector<int64> delta_path_cumul_cost_values_;
  const int64 global_span_cost_coefficient_;
  std::vector<SoftBound> cumul_soft_bounds_;
  std::vector<SoftBound> cumul_soft_lower_bounds_;
  std::vector<const PiecewiseLinearFunction*> cumul_piecewise_linear_costs_;
  std::vector<int64> vehicle_span_cost_coefficients_;
  bool has_nonzero_vehicle_span_cost_coefficients_;
  const std::vector<int64> vehicle_capacities_;
  // node_index_to_precedences_[node_index] contains all NodePrecedence elements
  // with node_index as either "first_node" or "second_node".
  // This vector is empty if there are no precedences on the dimension_.
  std::vector<std::vector<RoutingDimension::NodePrecedence>>
      node_index_to_precedences_;
  // Data reflecting information on paths and cumul variables for the solution
  // to which the filter was synchronized.
  SupportedPathCumul current_min_start_;
  SupportedPathCumul current_max_end_;
  PathTransits current_path_transits_;
  // Current min/max cumul values, indexed by node.
  std::vector<std::pair<int64, int64>> current_min_max_node_cumuls_;
  // Data reflecting information on paths and cumul variables for the "delta"
  // solution (aka neighbor solution) being examined.
  PathTransits delta_path_transits_;
  int64 delta_max_end_cumul_;
  SparseBitset<int64> delta_nodes_with_precedences_and_changed_cumul_;
  absl::flat_hash_map<int64, std::pair<int64, int64>>
      node_with_precedence_to_delta_min_max_cumuls_;
  // Note: small_ordered_set only support non-hash sets.
  gtl::small_ordered_set<std::set<int>> delta_paths_;
  const std::string name_;

  LocalDimensionCumulOptimizer* optimizer_;
  std::unique_ptr<LocalDimensionCumulOptimizer> internal_optimizer_;
  LocalDimensionCumulOptimizer* mp_optimizer_;
  std::unique_ptr<LocalDimensionCumulOptimizer> internal_mp_optimizer_;
  const bool filter_objective_cost_;
  // This boolean indicates if the LP optimizer can be used if necessary to
  // optimize the dimension cumuls, and is only used for testing purposes.
  const bool can_use_lp_;
  const bool propagate_own_objective_value_;

  // Used to do span lower bounding in presence of vehicle breaks.
  DisjunctivePropagator disjunctive_propagator_;
  DisjunctivePropagator::Tasks tasks_;
  TravelBounds travel_bounds_;
  std::vector<int64> current_path_;

  bool lns_detected_;
};

PathCumulFilter::PathCumulFilter(const RoutingModel& routing_model,
                                 const RoutingDimension& dimension,
                                 const RoutingSearchParameters& parameters,
                                 bool propagate_own_objective_value,
                                 bool filter_objective_cost, bool can_use_lp)
    : BasePathFilter(routing_model.Nexts(), dimension.cumuls().size()),
      routing_model_(routing_model),
      dimension_(dimension),
      cumuls_(dimension.cumuls()),
      slacks_(dimension.slacks()),
      evaluators_(routing_model.vehicles(), nullptr),
      vehicle_span_upper_bounds_(dimension.vehicle_span_upper_bounds()),
      has_vehicle_span_upper_bounds_(false),
      total_current_cumul_cost_value_(0),
      synchronized_objective_value_(0),
      accepted_objective_value_(0),
      current_cumul_cost_values_(),
      cumul_cost_delta_(0),
      delta_path_cumul_cost_values_(routing_model.vehicles(), kint64min),
      global_span_cost_coefficient_(dimension.global_span_cost_coefficient()),
      vehicle_span_cost_coefficients_(
          dimension.vehicle_span_cost_coefficients()),
      has_nonzero_vehicle_span_cost_coefficients_(false),
      vehicle_capacities_(dimension.vehicle_capacities()),
      delta_max_end_cumul_(0),
      delta_nodes_with_precedences_and_changed_cumul_(routing_model.Size()),
      name_(dimension.name()),
      optimizer_(routing_model.GetMutableLocalCumulOptimizer(dimension)),
      mp_optimizer_(routing_model.GetMutableLocalCumulMPOptimizer(dimension)),
      filter_objective_cost_(filter_objective_cost),
      can_use_lp_(can_use_lp),
      propagate_own_objective_value_(propagate_own_objective_value),
      lns_detected_(false) {
  for (const int64 upper_bound : vehicle_span_upper_bounds_) {
    if (upper_bound != kint64max) {
      has_vehicle_span_upper_bounds_ = true;
      break;
    }
  }
  for (const int64 coefficient : vehicle_span_cost_coefficients_) {
    if (coefficient != 0) {
      has_nonzero_vehicle_span_cost_coefficients_ = true;
      break;
    }
  }
  cumul_soft_bounds_.resize(cumuls_.size());
  cumul_soft_lower_bounds_.resize(cumuls_.size());
  cumul_piecewise_linear_costs_.resize(cumuls_.size());
  bool has_cumul_soft_bounds = false;
  bool has_cumul_soft_lower_bounds = false;
  bool has_cumul_piecewise_linear_costs = false;
  bool has_cumul_hard_bounds = false;
  for (const IntVar* const slack : slacks_) {
    if (slack->Min() > 0) {
      has_cumul_hard_bounds = true;
      break;
    }
  }
  for (int i = 0; i < cumuls_.size(); ++i) {
    if (dimension.HasCumulVarSoftUpperBound(i)) {
      has_cumul_soft_bounds = true;
      cumul_soft_bounds_[i].bound = dimension.GetCumulVarSoftUpperBound(i);
      cumul_soft_bounds_[i].coefficient =
          dimension.GetCumulVarSoftUpperBoundCoefficient(i);
    }
    if (dimension.HasCumulVarSoftLowerBound(i)) {
      has_cumul_soft_lower_bounds = true;
      cumul_soft_lower_bounds_[i].bound =
          dimension.GetCumulVarSoftLowerBound(i);
      cumul_soft_lower_bounds_[i].coefficient =
          dimension.GetCumulVarSoftLowerBoundCoefficient(i);
    }
    if (dimension.HasCumulVarPiecewiseLinearCost(i)) {
      has_cumul_piecewise_linear_costs = true;
      cumul_piecewise_linear_costs_[i] =
          dimension.GetCumulVarPiecewiseLinearCost(i);
    }
    IntVar* const cumul_var = cumuls_[i];
    if (cumul_var->Min() > 0 || cumul_var->Max() < kint64max) {
      has_cumul_hard_bounds = true;
    }
  }
  if (!has_cumul_soft_bounds) {
    cumul_soft_bounds_.clear();
  }
  if (!has_cumul_soft_lower_bounds) {
    cumul_soft_lower_bounds_.clear();
  }
  if (!has_cumul_piecewise_linear_costs) {
    cumul_piecewise_linear_costs_.clear();
  }
  if (!has_cumul_hard_bounds) {
    // Slacks don't need to be constrained if the cumuls don't have hard bounds;
    // therefore we can ignore the vehicle span cost coefficient (note that the
    // transit part is already handled by the arc cost filters).
    // This doesn't concern the global span filter though.
    vehicle_span_cost_coefficients_.assign(routing_model.vehicles(), 0);
    has_nonzero_vehicle_span_cost_coefficients_ = false;
  }
  start_to_vehicle_.resize(Size(), -1);
  for (int i = 0; i < routing_model.vehicles(); ++i) {
    start_to_vehicle_[routing_model.Start(i)] = i;
    evaluators_[i] = &dimension.transit_evaluator(i);
  }

  const std::vector<RoutingDimension::NodePrecedence>& node_precedences =
      dimension.GetNodePrecedences();
  if (!node_precedences.empty()) {
    current_min_max_node_cumuls_.resize(cumuls_.size(), {-1, -1});
    node_index_to_precedences_.resize(cumuls_.size());
    for (const auto& node_precedence : node_precedences) {
      node_index_to_precedences_[node_precedence.first_node].push_back(
          node_precedence);
      node_index_to_precedences_[node_precedence.second_node].push_back(
          node_precedence);
    }
  }
  // NOTE(user): The model's local optimizer for this dimension could be
  // null because the finalizer is using a global optimizer, so we create a
  // separate optimizer for the PathCumulFilter if we need it.
  if (can_use_lp_ && optimizer_ == nullptr) {
    DCHECK_EQ(mp_optimizer_, nullptr);
    for (int vehicle = 0; vehicle < routing_model.vehicles(); vehicle++) {
      if (!FilterWithDimensionCumulOptimizerForVehicle(vehicle)) {
        continue;
      }
      if (optimizer_ == nullptr) {
        // NOTE: The optimizer_ might have already been set in the for loop,
        // since we continue scanning vehicles in case one of them needs the
        // the mp_optimizer_ for break constraints.
        internal_optimizer_ = absl::make_unique<LocalDimensionCumulOptimizer>(
            &dimension, parameters.continuous_scheduling_solver());
        optimizer_ = internal_optimizer_.get();
      }
      if (FilterBreakCost(vehicle) || FilterDimensionForbiddenIntervals()) {
        internal_mp_optimizer_ =
            absl::make_unique<LocalDimensionCumulOptimizer>(
                &dimension, parameters.mixed_integer_scheduling_solver());
        mp_optimizer_ = internal_mp_optimizer_.get();
        break;
      }
    }
  }
}

int64 PathCumulFilter::GetCumulSoftCost(int64 node, int64 cumul_value) const {
  if (node < cumul_soft_bounds_.size()) {
    const int64 bound = cumul_soft_bounds_[node].bound;
    const int64 coefficient = cumul_soft_bounds_[node].coefficient;
    if (coefficient > 0 && bound < cumul_value) {
      return CapProd(CapSub(cumul_value, bound), coefficient);
    }
  }
  return 0;
}

int64 PathCumulFilter::GetCumulPiecewiseLinearCost(int64 node,
                                                   int64 cumul_value) const {
  if (node < cumul_piecewise_linear_costs_.size()) {
    const PiecewiseLinearFunction* cost = cumul_piecewise_linear_costs_[node];
    if (cost != nullptr) {
      return cost->Value(cumul_value);
    }
  }
  return 0;
}

int64 PathCumulFilter::GetCumulSoftLowerBoundCost(int64 node,
                                                  int64 cumul_value) const {
  if (node < cumul_soft_lower_bounds_.size()) {
    const int64 bound = cumul_soft_lower_bounds_[node].bound;
    const int64 coefficient = cumul_soft_lower_bounds_[node].coefficient;
    if (coefficient > 0 && bound > cumul_value) {
      return CapProd(CapSub(bound, cumul_value), coefficient);
    }
  }
  return 0;
}

int64 PathCumulFilter::GetPathCumulSoftLowerBoundCost(
    const PathTransits& path_transits, int path) const {
  int64 node = path_transits.Node(path, path_transits.PathSize(path) - 1);
  int64 cumul = cumuls_[node]->Max();
  int64 current_cumul_cost_value = GetCumulSoftLowerBoundCost(node, cumul);
  for (int i = path_transits.PathSize(path) - 2; i >= 0; --i) {
    node = path_transits.Node(path, i);
    cumul = CapSub(cumul, path_transits.Transit(path, i));
    cumul = std::min(cumuls_[node]->Max(), cumul);
    current_cumul_cost_value = CapAdd(current_cumul_cost_value,
                                      GetCumulSoftLowerBoundCost(node, cumul));
  }
  return current_cumul_cost_value;
}

void PathCumulFilter::OnBeforeSynchronizePaths() {
  total_current_cumul_cost_value_ = 0;
  cumul_cost_delta_ = 0;
  current_cumul_cost_values_.clear();
  if (NumPaths() > 0 &&
      (FilterSpanCost() || FilterCumulSoftBounds() || FilterSlackCost() ||
       FilterCumulSoftLowerBounds() || FilterCumulPiecewiseLinearCosts() ||
       FilterPrecedences() || FilterSoftSpanCost() ||
       FilterSoftSpanQuadraticCost())) {
    InitializeSupportedPathCumul(&current_min_start_, kint64max);
    InitializeSupportedPathCumul(&current_max_end_, kint64min);
    current_path_transits_.Clear();
    current_path_transits_.AddPaths(NumPaths());
    // For each path, compute the minimum end cumul and store the max of these.
    for (int r = 0; r < NumPaths(); ++r) {
      int64 node = Start(r);
      const int vehicle = start_to_vehicle_[Start(r)];
      // First pass: evaluating route length to reserve memory to store route
      // information.
      int number_of_route_arcs = 0;
      while (node < Size()) {
        ++number_of_route_arcs;
        node = Value(node);
      }
      current_path_transits_.ReserveTransits(r, number_of_route_arcs);
      // Second pass: update cumul, transit and cost values.
      node = Start(r);
      int64 cumul = cumuls_[node]->Min();
      std::vector<int64> min_path_cumuls;
      min_path_cumuls.reserve(number_of_route_arcs + 1);
      min_path_cumuls.push_back(cumul);

      int64 current_cumul_cost_value = GetCumulSoftCost(node, cumul);
      current_cumul_cost_value = CapAdd(
          current_cumul_cost_value, GetCumulPiecewiseLinearCost(node, cumul));

      int64 total_transit = 0;
      while (node < Size()) {
        const int64 next = Value(node);
        const int64 transit = (*evaluators_[vehicle])(node, next);
        total_transit = CapAdd(total_transit, transit);
        const int64 transit_slack = CapAdd(transit, slacks_[node]->Min());
        current_path_transits_.PushTransit(r, node, next, transit_slack);
        cumul = CapAdd(cumul, transit_slack);
        cumul =
            dimension_.GetFirstPossibleGreaterOrEqualValueForNode(next, cumul);
        cumul = std::max(cumuls_[next]->Min(), cumul);
        min_path_cumuls.push_back(cumul);
        node = next;
        current_cumul_cost_value =
            CapAdd(current_cumul_cost_value, GetCumulSoftCost(node, cumul));
        current_cumul_cost_value = CapAdd(
            current_cumul_cost_value, GetCumulPiecewiseLinearCost(node, cumul));
      }
      if (FilterPrecedences()) {
        StoreMinMaxCumulOfNodesOnPath(/*path=*/r, min_path_cumuls,
                                      /*is_delta=*/false);
      }
      if (number_of_route_arcs == 1 &&
          !routing_model_.AreEmptyRouteCostsConsideredForVehicle(vehicle)) {
        // This is an empty route (single start->end arc) which we don't take
        // into account for costs.
        current_cumul_cost_values_[Start(r)] = 0;
        current_path_transits_.ClearPath(r);
        continue;
      }
      if (FilterSlackCost() || FilterSoftSpanCost() ||
          FilterSoftSpanQuadraticCost()) {
        const int64 start = ComputePathMaxStartFromEndCumul(
            current_path_transits_, r, Start(r), cumul);
        const int64 span_lower_bound = CapSub(cumul, start);
        if (FilterSlackCost()) {
          current_cumul_cost_value =
              CapAdd(current_cumul_cost_value,
                     CapProd(vehicle_span_cost_coefficients_[vehicle],
                             CapSub(span_lower_bound, total_transit)));
        }
        if (FilterSoftSpanCost()) {
          const SimpleBoundCosts::BoundCost bound_cost =
              dimension_.GetSoftSpanUpperBoundForVehicle(vehicle);
          if (bound_cost.bound < span_lower_bound) {
            const int64 violation = CapSub(span_lower_bound, bound_cost.bound);
            current_cumul_cost_value = CapAdd(
                current_cumul_cost_value, CapProd(bound_cost.cost, violation));
          }
        }
        if (FilterSoftSpanQuadraticCost()) {
          const SimpleBoundCosts::BoundCost bound_cost =
              dimension_.GetQuadraticCostSoftSpanUpperBoundForVehicle(vehicle);
          if (bound_cost.bound < span_lower_bound) {
            const int64 violation = CapSub(span_lower_bound, bound_cost.bound);
            current_cumul_cost_value =
                CapAdd(current_cumul_cost_value,
                       CapProd(bound_cost.cost, CapProd(violation, violation)));
          }
        }
      }
      if (FilterCumulSoftLowerBounds()) {
        current_cumul_cost_value =
            CapAdd(current_cumul_cost_value,
                   GetPathCumulSoftLowerBoundCost(current_path_transits_, r));
      }
      if (FilterWithDimensionCumulOptimizerForVehicle(vehicle)) {
        // TODO(user): Return a status from the optimizer to detect failures
        // The only admissible failures here are because of LP timeout.
        int64 lp_cumul_cost_value = 0;
        LocalDimensionCumulOptimizer* const optimizer =
            FilterBreakCost(vehicle) ? mp_optimizer_ : optimizer_;
        DCHECK_NE(optimizer, nullptr);
        const DimensionSchedulingStatus status =
            optimizer->ComputeRouteCumulCostWithoutFixedTransits(
                vehicle, [this](int64 node) { return Value(node); },
                &lp_cumul_cost_value);
        switch (status) {
          case DimensionSchedulingStatus::INFEASIBLE:
            lp_cumul_cost_value = 0;
            break;
          case DimensionSchedulingStatus::RELAXED_OPTIMAL_ONLY:
            DCHECK_NE(mp_optimizer_, nullptr);
            if (mp_optimizer_->ComputeRouteCumulCostWithoutFixedTransits(
                    vehicle, [this](int64 node) { return Value(node); },
                    &lp_cumul_cost_value) ==
                DimensionSchedulingStatus::INFEASIBLE) {
              lp_cumul_cost_value = 0;
            }
            break;
          default:
            DCHECK(status == DimensionSchedulingStatus::OPTIMAL);
        }
        current_cumul_cost_value =
            std::max(current_cumul_cost_value, lp_cumul_cost_value);
      }
      current_cumul_cost_values_[Start(r)] = current_cumul_cost_value;
      current_max_end_.path_values[r] = cumul;
      if (current_max_end_.cumul_value < cumul) {
        current_max_end_.cumul_value = cumul;
        current_max_end_.cumul_value_support = r;
      }
      total_current_cumul_cost_value_ =
          CapAdd(total_current_cumul_cost_value_, current_cumul_cost_value);
    }
    if (FilterPrecedences()) {
      // Update the min/max node cumuls of new unperformed nodes.
      for (int64 node : GetNewSynchronizedUnperformedNodes()) {
        current_min_max_node_cumuls_[node] = {-1, -1};
      }
    }
    // Use the max of the path end cumul mins to compute the corresponding
    // maximum start cumul of each path; store the minimum of these.
    for (int r = 0; r < NumPaths(); ++r) {
      const int64 start = ComputePathMaxStartFromEndCumul(
          current_path_transits_, r, Start(r), current_max_end_.cumul_value);
      current_min_start_.path_values[r] = start;
      if (current_min_start_.cumul_value > start) {
        current_min_start_.cumul_value = start;
        current_min_start_.cumul_value_support = r;
      }
    }
  }
  // Initialize this before considering any deltas (neighbor).
  delta_max_end_cumul_ = kint64min;
  lns_detected_ = false;

  DCHECK_GE(current_max_end_.cumul_value, current_min_start_.cumul_value);
  synchronized_objective_value_ =
      CapAdd(total_current_cumul_cost_value_,
             CapProd(global_span_cost_coefficient_,
                     CapSub(current_max_end_.cumul_value,
                            current_min_start_.cumul_value)));
}

bool PathCumulFilter::AcceptPath(int64 path_start, int64 chain_start,
                                 int64 chain_end) {
  int64 node = path_start;
  int64 cumul = cumuls_[node]->Min();
  int64 cumul_cost_delta = 0;
  int64 total_transit = 0;
  const int path = delta_path_transits_.AddPaths(1);
  const int vehicle = start_to_vehicle_[path_start];
  const int64 capacity = vehicle_capacities_[vehicle];
  const bool filter_vehicle_costs =
      !routing_model_.IsEnd(GetNext(node)) ||
      routing_model_.AreEmptyRouteCostsConsideredForVehicle(vehicle);
  if (filter_vehicle_costs) {
    cumul_cost_delta = CapAdd(GetCumulSoftCost(node, cumul),
                              GetCumulPiecewiseLinearCost(node, cumul));
  }
  // Evaluating route length to reserve memory to store transit information.
  int number_of_route_arcs = 0;
  while (node < Size()) {
    const int64 next = GetNext(node);
    // TODO(user): This shouldn't be needed anymore as such deltas should
    // have been filtered already.
    if (next == kUnassigned) {
      // LNS detected, return true since other paths were ok up to now.
      lns_detected_ = true;
      return true;
    }
    ++number_of_route_arcs;
    node = next;
  }
  delta_path_transits_.ReserveTransits(path, number_of_route_arcs);
  std::vector<int64> min_path_cumuls;
  min_path_cumuls.reserve(number_of_route_arcs + 1);
  min_path_cumuls.push_back(cumul);
  // Check that the path is feasible with regards to cumul bounds, scanning
  // the paths from start to end (caching path node sequences and transits
  // for further span cost filtering).
  node = path_start;
  while (node < Size()) {
    const int64 next = GetNext(node);
    const int64 transit = (*evaluators_[vehicle])(node, next);
    total_transit = CapAdd(total_transit, transit);
    const int64 transit_slack = CapAdd(transit, slacks_[node]->Min());
    delta_path_transits_.PushTransit(path, node, next, transit_slack);
    cumul = CapAdd(cumul, transit_slack);
    cumul = dimension_.GetFirstPossibleGreaterOrEqualValueForNode(next, cumul);
    if (cumul > std::min(capacity, cumuls_[next]->Max())) {
      return false;
    }
    cumul = std::max(cumuls_[next]->Min(), cumul);
    min_path_cumuls.push_back(cumul);
    node = next;
    if (filter_vehicle_costs) {
      cumul_cost_delta =
          CapAdd(cumul_cost_delta, GetCumulSoftCost(node, cumul));
      cumul_cost_delta =
          CapAdd(cumul_cost_delta, GetCumulPiecewiseLinearCost(node, cumul));
    }
  }
  const int64 min_end = cumul;

  if (!PickupToDeliveryLimitsRespected(delta_path_transits_, path,
                                       min_path_cumuls)) {
    return false;
  }
  if (FilterSlackCost() || FilterBreakCost(vehicle) ||
      FilterSoftSpanCost(vehicle) || FilterSoftSpanQuadraticCost(vehicle)) {
    const int64 max_start_from_min_end = ComputePathMaxStartFromEndCumul(
        delta_path_transits_, path, path_start, min_end);
    int64 min_total_slack =
        CapSub(CapSub(min_end, max_start_from_min_end), total_transit);
    if (FilterBreakCost(vehicle)) {
      // Copy path of vehicle.
      {
        current_path_.clear();
        int64 node = path_start;
        while (node < Size()) {
          current_path_.push_back(node);
          node = GetNext(node);
        }
        current_path_.push_back(node);
      }
      FillTravelBoundsOfVehicle(vehicle, current_path_, dimension_,
                                &travel_bounds_);
      tasks_.Clear();
      AppendTasksFromPath(current_path_, travel_bounds_, dimension_, &tasks_);
      tasks_.num_chain_tasks = tasks_.start_min.size();
      AppendTasksFromIntervals(dimension_.GetBreakIntervalsOfVehicle(vehicle),
                               &tasks_);
      tasks_.distance_duration =
          dimension_.GetBreakDistanceDurationOfVehicle(vehicle);
      if (!disjunctive_propagator_.Precedences(&tasks_) ||
          !disjunctive_propagator_.ChainSpanMin(&tasks_)) {
        return false;
      }
      min_total_slack =
          std::max(min_total_slack, CapSub(tasks_.span_min, total_transit));
    }
    if (filter_vehicle_costs) {
      cumul_cost_delta = CapAdd(
          cumul_cost_delta,
          CapProd(vehicle_span_cost_coefficients_[vehicle], min_total_slack));
      const int64 span_lower_bound = CapAdd(total_transit, min_total_slack);
      if (FilterSoftSpanCost()) {
        const SimpleBoundCosts::BoundCost bound_cost =
            dimension_.GetSoftSpanUpperBoundForVehicle(vehicle);
        if (bound_cost.bound < span_lower_bound) {
          const int64 violation = CapSub(span_lower_bound, bound_cost.bound);
          cumul_cost_delta =
              CapAdd(cumul_cost_delta, CapProd(bound_cost.cost, violation));
        }
      }
      if (FilterSoftSpanQuadraticCost()) {
        const SimpleBoundCosts::BoundCost bound_cost =
            dimension_.GetQuadraticCostSoftSpanUpperBoundForVehicle(vehicle);
        if (bound_cost.bound < span_lower_bound) {
          const int64 violation = CapSub(span_lower_bound, bound_cost.bound);
          cumul_cost_delta =
              CapAdd(cumul_cost_delta,
                     CapProd(bound_cost.cost, CapProd(violation, violation)));
        }
      }
    }
    if (CapAdd(total_transit, min_total_slack) >
        vehicle_span_upper_bounds_[vehicle]) {
      return false;
    }
  }
  if (FilterCumulSoftLowerBounds() && filter_vehicle_costs) {
    cumul_cost_delta =
        CapAdd(cumul_cost_delta,
               GetPathCumulSoftLowerBoundCost(delta_path_transits_, path));
  }
  if (FilterPrecedences()) {
    StoreMinMaxCumulOfNodesOnPath(path, min_path_cumuls, /*is_delta=*/true);
  }
  if (!filter_vehicle_costs) {
    // If this route's costs should't be taken into account, reset the
    // cumul_cost_delta and delta_path_transits_ for this path.
    cumul_cost_delta = 0;
    delta_path_transits_.ClearPath(path);
  }
  if (FilterSpanCost() || FilterCumulSoftBounds() || FilterSlackCost() ||
      FilterCumulSoftLowerBounds() || FilterCumulPiecewiseLinearCosts() ||
      FilterSoftSpanCost(vehicle) || FilterSoftSpanQuadraticCost(vehicle)) {
    delta_paths_.insert(GetPath(path_start));
    delta_path_cumul_cost_values_[vehicle] = cumul_cost_delta;
    cumul_cost_delta =
        CapSub(cumul_cost_delta, current_cumul_cost_values_[path_start]);
    if (filter_vehicle_costs) {
      delta_max_end_cumul_ = std::max(delta_max_end_cumul_, min_end);
    }
  }
  cumul_cost_delta_ = CapAdd(cumul_cost_delta_, cumul_cost_delta);
  return true;
}

bool PathCumulFilter::FinalizeAcceptPath(const Assignment* delta,
                                         int64 objective_min,
                                         int64 objective_max) {
  if ((!FilterSpanCost() && !FilterCumulSoftBounds() && !FilterSlackCost() &&
       !FilterCumulSoftLowerBounds() && !FilterCumulPiecewiseLinearCosts() &&
       !FilterPrecedences() && !FilterSoftSpanCost() &&
       !FilterSoftSpanQuadraticCost()) ||
      lns_detected_) {
    return true;
  }
  if (FilterPrecedences()) {
    for (int64 node : delta_nodes_with_precedences_and_changed_cumul_
                          .PositionsSetAtLeastOnce()) {
      const std::pair<int64, int64> node_min_max_cumul_in_delta =
          gtl::FindWithDefault(node_with_precedence_to_delta_min_max_cumuls_,
                               node, {-1, -1});
      // NOTE: This node was seen in delta, so its delta min/max cumul should be
      // stored in the map.
      DCHECK(node_min_max_cumul_in_delta.first >= 0 &&
             node_min_max_cumul_in_delta.second >= 0);
      for (const RoutingDimension::NodePrecedence& precedence :
           node_index_to_precedences_[node]) {
        const bool node_is_first = (precedence.first_node == node);
        const int64 other_node =
            node_is_first ? precedence.second_node : precedence.first_node;
        if (GetNext(other_node) == kUnassigned ||
            GetNext(other_node) == other_node) {
          // The other node is unperformed, so the precedence constraint is
          // inactive.
          continue;
        }
        // max_cumul[second_node] should be greater or equal than
        // min_cumul[first_node] + offset.
        const std::pair<int64, int64>& other_min_max_cumul_in_delta =
            gtl::FindWithDefault(node_with_precedence_to_delta_min_max_cumuls_,
                                 other_node,
                                 current_min_max_node_cumuls_[other_node]);

        const int64 first_min_cumul = node_is_first
                                          ? node_min_max_cumul_in_delta.first
                                          : other_min_max_cumul_in_delta.first;
        const int64 second_max_cumul = node_is_first
                                           ? other_min_max_cumul_in_delta.second
                                           : node_min_max_cumul_in_delta.second;

        if (second_max_cumul < first_min_cumul + precedence.offset) {
          return false;
        }
      }
    }
  }
  int64 new_max_end = delta_max_end_cumul_;
  int64 new_min_start = kint64max;
  if (FilterSpanCost()) {
    if (new_max_end < current_max_end_.cumul_value) {
      // Delta max end is lower than the current solution one.
      // If the path supporting the current max end has been modified, we need
      // to check all paths to find the largest max end.
      if (!gtl::ContainsKey(delta_paths_,
                            current_max_end_.cumul_value_support)) {
        new_max_end = current_max_end_.cumul_value;
      } else {
        for (int i = 0; i < current_max_end_.path_values.size(); ++i) {
          if (current_max_end_.path_values[i] > new_max_end &&
              !gtl::ContainsKey(delta_paths_, i)) {
            new_max_end = current_max_end_.path_values[i];
          }
        }
      }
    }
    // Now that the max end cumul has been found, compute the corresponding
    // min start cumul, first from the delta, then if the max end cumul has
    // changed, from the unchanged paths as well.
    for (int r = 0; r < delta_path_transits_.NumPaths(); ++r) {
      new_min_start =
          std::min(ComputePathMaxStartFromEndCumul(delta_path_transits_, r,
                                                   Start(r), new_max_end),
                   new_min_start);
    }
    if (new_max_end != current_max_end_.cumul_value) {
      for (int r = 0; r < NumPaths(); ++r) {
        if (gtl::ContainsKey(delta_paths_, r)) {
          continue;
        }
        new_min_start = std::min(new_min_start, ComputePathMaxStartFromEndCumul(
                                                    current_path_transits_, r,
                                                    Start(r), new_max_end));
      }
    } else if (new_min_start > current_min_start_.cumul_value) {
      // Delta min start is greater than the current solution one.
      // If the path supporting the current min start has been modified, we need
      // to check all paths to find the smallest min start.
      if (!gtl::ContainsKey(delta_paths_,
                            current_min_start_.cumul_value_support)) {
        new_min_start = current_min_start_.cumul_value;
      } else {
        for (int i = 0; i < current_min_start_.path_values.size(); ++i) {
          if (current_min_start_.path_values[i] < new_min_start &&
              !gtl::ContainsKey(delta_paths_, i)) {
            new_min_start = current_min_start_.path_values[i];
          }
        }
      }
    }
  }

  // Filtering on objective value, calling LPs and MIPs if needed..
  accepted_objective_value_ =
      CapAdd(cumul_cost_delta_, CapProd(global_span_cost_coefficient_,
                                        CapSub(new_max_end, new_min_start)));

  if (can_use_lp_ && optimizer_ != nullptr &&
      accepted_objective_value_ <= objective_max) {
    const size_t num_touched_paths = GetTouchedPathStarts().size();
    std::vector<int64> path_delta_cost_values(num_touched_paths, 0);
    std::vector<bool> requires_mp(num_touched_paths, false);
    for (int i = 0; i < num_touched_paths; ++i) {
      const int64 start = GetTouchedPathStarts()[i];
      const int vehicle = start_to_vehicle_[start];
      if (!FilterWithDimensionCumulOptimizerForVehicle(vehicle)) {
        continue;
      }
      int64 path_delta_cost_with_lp = 0;
      const DimensionSchedulingStatus status =
          optimizer_->ComputeRouteCumulCostWithoutFixedTransits(
              vehicle, [this](int64 node) { return GetNext(node); },
              &path_delta_cost_with_lp);
      if (status == DimensionSchedulingStatus::INFEASIBLE) {
        return false;
      }
      DCHECK(gtl::ContainsKey(delta_paths_, GetPath(start)));
      const int64 path_cost_diff_with_lp = CapSub(
          path_delta_cost_with_lp, delta_path_cumul_cost_values_[vehicle]);
      if (path_cost_diff_with_lp > 0) {
        path_delta_cost_values[i] = path_delta_cost_with_lp;
        accepted_objective_value_ =
            CapAdd(accepted_objective_value_, path_cost_diff_with_lp);
        if (accepted_objective_value_ > objective_max) {
          return false;
        }
      } else {
        path_delta_cost_values[i] = delta_path_cumul_cost_values_[vehicle];
      }
      if (mp_optimizer_ != nullptr) {
        requires_mp[i] =
            FilterBreakCost(vehicle) ||
            (status == DimensionSchedulingStatus::RELAXED_OPTIMAL_ONLY);
      }
    }
    if (mp_optimizer_ == nullptr) {
      return accepted_objective_value_ <= objective_max;
    }
    for (int i = 0; i < num_touched_paths; ++i) {
      if (!requires_mp[i]) {
        continue;
      }
      const int64 start = GetTouchedPathStarts()[i];
      const int vehicle = start_to_vehicle_[start];
      int64 path_delta_cost_with_mp = 0;
      if (mp_optimizer_->ComputeRouteCumulCostWithoutFixedTransits(
              vehicle, [this](int64 node) { return GetNext(node); },
              &path_delta_cost_with_mp) ==
          DimensionSchedulingStatus::INFEASIBLE) {
        return false;
      }
      DCHECK(gtl::ContainsKey(delta_paths_, GetPath(start)));
      const int64 path_cost_diff_with_mp =
          CapSub(path_delta_cost_with_mp, path_delta_cost_values[i]);
      if (path_cost_diff_with_mp > 0) {
        accepted_objective_value_ =
            CapAdd(accepted_objective_value_, path_cost_diff_with_mp);
        if (accepted_objective_value_ > objective_max) {
          return false;
        }
      }
    }
  }

  return accepted_objective_value_ <= objective_max;
}

void PathCumulFilter::InitializeSupportedPathCumul(
    SupportedPathCumul* supported_cumul, int64 default_value) {
  supported_cumul->cumul_value = default_value;
  supported_cumul->cumul_value_support = -1;
  supported_cumul->path_values.resize(NumPaths(), default_value);
}

bool PathCumulFilter::PickupToDeliveryLimitsRespected(
    const PathTransits& path_transits, int path,
    const std::vector<int64>& min_path_cumuls) const {
  if (!dimension_.HasPickupToDeliveryLimits()) {
    return true;
  }
  const int num_pairs = routing_model_.GetPickupAndDeliveryPairs().size();
  DCHECK_GT(num_pairs, 0);
  std::vector<std::pair<int, int64>> visited_delivery_and_min_cumul_per_pair(
      num_pairs, {-1, -1});

  const int path_size = path_transits.PathSize(path);
  CHECK_EQ(min_path_cumuls.size(), path_size);

  int64 max_cumul = min_path_cumuls.back();
  for (int i = path_transits.PathSize(path) - 2; i >= 0; i--) {
    const int node_index = path_transits.Node(path, i);
    max_cumul = CapSub(max_cumul, path_transits.Transit(path, i));
    max_cumul = std::min(cumuls_[node_index]->Max(), max_cumul);

    const std::vector<std::pair<int, int>>& pickup_index_pairs =
        routing_model_.GetPickupIndexPairs(node_index);
    const std::vector<std::pair<int, int>>& delivery_index_pairs =
        routing_model_.GetDeliveryIndexPairs(node_index);
    if (!pickup_index_pairs.empty()) {
      // The node is a pickup. Check that it is not a delivery and that it
      // appears in a single pickup/delivery pair (as required when limits are
      // set on dimension cumuls for pickup and deliveries).
      DCHECK(delivery_index_pairs.empty());
      DCHECK_EQ(pickup_index_pairs.size(), 1);
      const int pair_index = pickup_index_pairs[0].first;
      // Get the delivery visited for this pair.
      const int delivery_index =
          visited_delivery_and_min_cumul_per_pair[pair_index].first;
      if (delivery_index < 0) {
        // No delivery visited after this pickup for this pickup/delivery pair.
        continue;
      }
      const int64 cumul_diff_limit = dimension_.GetPickupToDeliveryLimitForPair(
          pair_index, pickup_index_pairs[0].second, delivery_index);
      if (CapSub(visited_delivery_and_min_cumul_per_pair[pair_index].second,
                 max_cumul) > cumul_diff_limit) {
        return false;
      }
    }
    if (!delivery_index_pairs.empty()) {
      // The node is a delivery. Check that it's not a pickup and it belongs to
      // a single pair.
      DCHECK(pickup_index_pairs.empty());
      DCHECK_EQ(delivery_index_pairs.size(), 1);
      const int pair_index = delivery_index_pairs[0].first;
      std::pair<int, int64>& delivery_index_and_cumul =
          visited_delivery_and_min_cumul_per_pair[pair_index];
      int& delivery_index = delivery_index_and_cumul.first;
      DCHECK_EQ(delivery_index, -1);
      delivery_index = delivery_index_pairs[0].second;
      delivery_index_and_cumul.second = min_path_cumuls[i];
    }
  }
  return true;
}

void PathCumulFilter::StoreMinMaxCumulOfNodesOnPath(
    int path, const std::vector<int64>& min_path_cumuls, bool is_delta) {
  const PathTransits& path_transits =
      is_delta ? delta_path_transits_ : current_path_transits_;

  const int path_size = path_transits.PathSize(path);
  DCHECK_EQ(min_path_cumuls.size(), path_size);

  int64 max_cumul = cumuls_[path_transits.Node(path, path_size - 1)]->Max();
  for (int i = path_size - 1; i >= 0; i--) {
    const int node_index = path_transits.Node(path, i);

    if (i < path_size - 1) {
      max_cumul = CapSub(max_cumul, path_transits.Transit(path, i));
      max_cumul = std::min(cumuls_[node_index]->Max(), max_cumul);
    }

    if (is_delta && node_index_to_precedences_[node_index].empty()) {
      // No need to update the delta cumul map for nodes without precedences.
      continue;
    }

    std::pair<int64, int64>& min_max_cumuls =
        is_delta ? node_with_precedence_to_delta_min_max_cumuls_[node_index]
                 : current_min_max_node_cumuls_[node_index];
    min_max_cumuls.first = min_path_cumuls[i];
    min_max_cumuls.second = max_cumul;

    if (is_delta && !routing_model_.IsEnd(node_index) &&
        (min_max_cumuls.first !=
             current_min_max_node_cumuls_[node_index].first ||
         max_cumul != current_min_max_node_cumuls_[node_index].second)) {
      delta_nodes_with_precedences_and_changed_cumul_.Set(node_index);
    }
  }
}

int64 PathCumulFilter::ComputePathMaxStartFromEndCumul(
    const PathTransits& path_transits, int path, int64 path_start,
    int64 min_end_cumul) const {
  int64 cumul_from_min_end = min_end_cumul;
  int64 cumul_from_max_end =
      cumuls_[routing_model_.End(start_to_vehicle_[path_start])]->Max();
  for (int i = path_transits.PathSize(path) - 2; i >= 0; --i) {
    const int64 transit = path_transits.Transit(path, i);
    const int64 node = path_transits.Node(path, i);
    cumul_from_min_end =
        std::min(cumuls_[node]->Max(), CapSub(cumul_from_min_end, transit));
    cumul_from_max_end = dimension_.GetLastPossibleLessOrEqualValueForNode(
        node, CapSub(cumul_from_max_end, transit));
  }
  return std::min(cumul_from_min_end, cumul_from_max_end);
}

}  // namespace

IntVarLocalSearchFilter* MakePathCumulFilter(
    const RoutingDimension& dimension,
    const RoutingSearchParameters& parameters,
    bool propagate_own_objective_value, bool filter_objective_cost,
    bool can_use_lp) {
  RoutingModel& model = *dimension.model();
  return model.solver()->RevAlloc(new PathCumulFilter(
      model, dimension, parameters, propagate_own_objective_value,
      filter_objective_cost, can_use_lp));
}

namespace {

bool DimensionHasCumulCost(const RoutingDimension& dimension) {
  if (dimension.global_span_cost_coefficient() != 0) return true;
  if (dimension.HasSoftSpanUpperBounds()) return true;
  if (dimension.HasQuadraticCostSoftSpanUpperBounds()) return true;
  for (const int64 coefficient : dimension.vehicle_span_cost_coefficients()) {
    if (coefficient != 0) return true;
  }
  for (int i = 0; i < dimension.cumuls().size(); ++i) {
    if (dimension.HasCumulVarSoftUpperBound(i)) return true;
    if (dimension.HasCumulVarSoftLowerBound(i)) return true;
    if (dimension.HasCumulVarPiecewiseLinearCost(i)) return true;
  }
  return false;
}

bool DimensionHasCumulConstraint(const RoutingDimension& dimension) {
  if (dimension.HasBreakConstraints()) return true;
  if (dimension.HasPickupToDeliveryLimits()) return true;
  if (!dimension.GetNodePrecedences().empty()) return true;
  for (const int64 upper_bound : dimension.vehicle_span_upper_bounds()) {
    if (upper_bound != kint64max) return true;
  }
  for (const IntVar* const slack : dimension.slacks()) {
    if (slack->Min() > 0) return true;
  }
  const std::vector<IntVar*>& cumuls = dimension.cumuls();
  for (int i = 0; i < cumuls.size(); ++i) {
    IntVar* const cumul_var = cumuls[i];
    if (cumul_var->Min() > 0 && cumul_var->Max() < kint64max &&
        !dimension.model()->IsEnd(i)) {
      return true;
    }
    if (dimension.forbidden_intervals()[i].NumIntervals() > 0) return true;
  }
  return false;
}

}  // namespace

void AppendDimensionCumulFilters(
    const std::vector<RoutingDimension*>& dimensions,
    const RoutingSearchParameters& parameters, bool filter_objective_cost,
    std::vector<LocalSearchFilter*>* filters) {
  // NOTE: We first sort the dimensions by increasing complexity of filtering:
  // - Dimensions without any cumul-related costs or constraints will have a
  //   ChainCumulFilter.
  // - Dimensions with cumul costs or constraints, but no global span cost
  //   and/or precedences will have a PathCumulFilter.
  // - Dimensions with a global span cost coefficient and/or precedences will
  //   have a global LP filter.
  const int num_dimensions = dimensions.size();

  std::vector<bool> use_path_cumul_filter(num_dimensions);
  std::vector<bool> use_cumul_bounds_propagator_filter(num_dimensions);
  std::vector<bool> use_global_lp_filter(num_dimensions);
  std::vector<int> filtering_difficulty(num_dimensions);
  for (int d = 0; d < num_dimensions; d++) {
    const RoutingDimension& dimension = *dimensions[d];
    const bool has_cumul_cost = DimensionHasCumulCost(dimension);
    use_path_cumul_filter[d] =
        has_cumul_cost || DimensionHasCumulConstraint(dimension);

    const bool can_use_cumul_bounds_propagator_filter =
        !dimension.HasBreakConstraints() &&
        (!filter_objective_cost || !has_cumul_cost);
    const bool has_precedences = !dimension.GetNodePrecedences().empty();
    use_global_lp_filter[d] =
        (has_precedences && !can_use_cumul_bounds_propagator_filter) ||
        (filter_objective_cost && dimension.global_span_cost_coefficient() > 0);

    use_cumul_bounds_propagator_filter[d] =
        has_precedences && !use_global_lp_filter[d];

    filtering_difficulty[d] = 4 * use_global_lp_filter[d] +
                              2 * use_cumul_bounds_propagator_filter[d] +
                              use_path_cumul_filter[d];
  }

  std::vector<int> sorted_dimension_indices(num_dimensions);
  std::iota(sorted_dimension_indices.begin(), sorted_dimension_indices.end(),
            0);
  std::sort(sorted_dimension_indices.begin(), sorted_dimension_indices.end(),
            [&filtering_difficulty](int d1, int d2) {
              return filtering_difficulty[d1] < filtering_difficulty[d2];
            });

  for (const int d : sorted_dimension_indices) {
    const RoutingDimension& dimension = *dimensions[d];
    const RoutingModel& model = *dimension.model();
    // NOTE: We always add the [Chain|Path]CumulFilter to filter each route's
    // feasibility separately to try and cut bad decisions earlier in the
    // search, but we don't propagate the computed cost if the LPCumulFilter is
    // already doing it.
    const bool use_global_lp = use_global_lp_filter[d];
    if (use_path_cumul_filter[d]) {
      filters->push_back(MakePathCumulFilter(
          dimension, parameters, !use_global_lp, filter_objective_cost));
    } else {
      filters->push_back(
          model.solver()->RevAlloc(new ChainCumulFilter(model, dimension)));
    }

    if (use_global_lp) {
      DCHECK(model.GetMutableGlobalCumulOptimizer(dimension) != nullptr);
      filters->push_back(MakeGlobalLPCumulFilter(
          model.GetMutableGlobalCumulOptimizer(dimension),
          filter_objective_cost));
    } else if (use_cumul_bounds_propagator_filter[d]) {
      filters->push_back(MakeCumulBoundsPropagatorFilter(dimension));
    }
  }
}

namespace {

// Filter for pickup/delivery precedences.
class PickupDeliveryFilter : public BasePathFilter {
 public:
  PickupDeliveryFilter(const std::vector<IntVar*>& nexts, int next_domain_size,
                       const RoutingModel::IndexPairs& pairs,
                       const std::vector<RoutingModel::PickupAndDeliveryPolicy>&
                           vehicle_policies);
  ~PickupDeliveryFilter() override {}
  bool AcceptPath(int64 path_start, int64 chain_start,
                  int64 chain_end) override;
  std::string DebugString() const override { return "PickupDeliveryFilter"; }

 private:
  bool AcceptPathDefault(int64 path_start);
  template <bool lifo>
  bool AcceptPathOrdered(int64 path_start);

  std::vector<int> pair_firsts_;
  std::vector<int> pair_seconds_;
  const RoutingModel::IndexPairs pairs_;
  SparseBitset<> visited_;
  std::deque<int> visited_deque_;
  const std::vector<RoutingModel::PickupAndDeliveryPolicy> vehicle_policies_;
};

PickupDeliveryFilter::PickupDeliveryFilter(
    const std::vector<IntVar*>& nexts, int next_domain_size,
    const RoutingModel::IndexPairs& pairs,
    const std::vector<RoutingModel::PickupAndDeliveryPolicy>& vehicle_policies)
    : BasePathFilter(nexts, next_domain_size),
      pair_firsts_(next_domain_size, kUnassigned),
      pair_seconds_(next_domain_size, kUnassigned),
      pairs_(pairs),
      visited_(Size()),
      vehicle_policies_(vehicle_policies) {
  for (int i = 0; i < pairs.size(); ++i) {
    const auto& index_pair = pairs[i];
    for (int first : index_pair.first) {
      pair_firsts_[first] = i;
    }
    for (int second : index_pair.second) {
      pair_seconds_[second] = i;
    }
  }
}

bool PickupDeliveryFilter::AcceptPath(int64 path_start, int64 chain_start,
                                      int64 chain_end) {
  switch (vehicle_policies_[GetPath(path_start)]) {
    case RoutingModel::PICKUP_AND_DELIVERY_NO_ORDER:
      return AcceptPathDefault(path_start);
    case RoutingModel::PICKUP_AND_DELIVERY_LIFO:
      return AcceptPathOrdered<true>(path_start);
    case RoutingModel::PICKUP_AND_DELIVERY_FIFO:
      return AcceptPathOrdered<false>(path_start);
    default:
      return true;
  }
}

bool PickupDeliveryFilter::AcceptPathDefault(int64 path_start) {
  visited_.ClearAll();
  int64 node = path_start;
  int64 path_length = 1;
  while (node < Size()) {
    // Detect sub-cycles (path is longer than longest possible path).
    if (path_length > Size()) {
      return false;
    }
    if (pair_firsts_[node] != kUnassigned) {
      // Checking on pair firsts is not actually necessary (inconsistencies
      // will get caught when checking pair seconds); doing it anyway to
      // cut checks early.
      for (int second : pairs_[pair_firsts_[node]].second) {
        if (visited_[second]) {
          return false;
        }
      }
    }
    if (pair_seconds_[node] != kUnassigned) {
      bool found_first = false;
      bool some_synced = false;
      for (int first : pairs_[pair_seconds_[node]].first) {
        if (visited_[first]) {
          found_first = true;
          break;
        }
        if (IsVarSynced(first)) {
          some_synced = true;
        }
      }
      if (!found_first && some_synced) {
        return false;
      }
    }
    visited_.Set(node);
    const int64 next = GetNext(node);
    if (next == kUnassigned) {
      // LNS detected, return true since path was ok up to now.
      return true;
    }
    node = next;
    ++path_length;
  }
  for (const int64 node : visited_.PositionsSetAtLeastOnce()) {
    if (pair_firsts_[node] != kUnassigned) {
      bool found_second = false;
      bool some_synced = false;
      for (int second : pairs_[pair_firsts_[node]].second) {
        if (visited_[second]) {
          found_second = true;
          break;
        }
        if (IsVarSynced(second)) {
          some_synced = true;
        }
      }
      if (!found_second && some_synced) {
        return false;
      }
    }
  }
  return true;
}

template <bool lifo>
bool PickupDeliveryFilter::AcceptPathOrdered(int64 path_start) {
  visited_deque_.clear();
  int64 node = path_start;
  int64 path_length = 1;
  while (node < Size()) {
    // Detect sub-cycles (path is longer than longest possible path).
    if (path_length > Size()) {
      return false;
    }
    if (pair_firsts_[node] != kUnassigned) {
      if (lifo) {
        visited_deque_.push_back(node);
      } else {
        visited_deque_.push_front(node);
      }
    }
    if (pair_seconds_[node] != kUnassigned) {
      bool found_first = false;
      bool some_synced = false;
      for (int first : pairs_[pair_seconds_[node]].first) {
        if (!visited_deque_.empty() && visited_deque_.back() == first) {
          found_first = true;
          break;
        }
        if (IsVarSynced(first)) {
          some_synced = true;
        }
      }
      if (!found_first && some_synced) {
        return false;
      } else if (!visited_deque_.empty()) {
        visited_deque_.pop_back();
      }
    }
    const int64 next = GetNext(node);
    if (next == kUnassigned) {
      // LNS detected, return true since path was ok up to now.
      return true;
    }
    node = next;
    ++path_length;
  }
  while (!visited_deque_.empty()) {
    for (int second : pairs_[pair_firsts_[visited_deque_.back()]].second) {
      if (IsVarSynced(second)) {
        return false;
      }
    }
    visited_deque_.pop_back();
  }
  return true;
}

}  // namespace

IntVarLocalSearchFilter* MakePickupDeliveryFilter(
    const RoutingModel& routing_model, const RoutingModel::IndexPairs& pairs,
    const std::vector<RoutingModel::PickupAndDeliveryPolicy>&
        vehicle_policies) {
  return routing_model.solver()->RevAlloc(new PickupDeliveryFilter(
      routing_model.Nexts(), routing_model.Size() + routing_model.vehicles(),
      pairs, vehicle_policies));
}

namespace {

// Vehicle variable filter
class VehicleVarFilter : public BasePathFilter {
 public:
  explicit VehicleVarFilter(const RoutingModel& routing_model);
  ~VehicleVarFilter() override {}
  bool AcceptPath(int64 path_start, int64 chain_start,
                  int64 chain_end) override;
  std::string DebugString() const override { return "VehicleVariableFilter"; }

 private:
  bool DisableFiltering() const override;
  bool IsVehicleVariableConstrained(int index) const;

  std::vector<int64> start_to_vehicle_;
  std::vector<IntVar*> vehicle_vars_;
  const int64 unconstrained_vehicle_var_domain_size_;
};

VehicleVarFilter::VehicleVarFilter(const RoutingModel& routing_model)
    : BasePathFilter(routing_model.Nexts(),
                     routing_model.Size() + routing_model.vehicles()),
      vehicle_vars_(routing_model.VehicleVars()),
      unconstrained_vehicle_var_domain_size_(routing_model.vehicles()) {
  start_to_vehicle_.resize(Size(), -1);
  for (int i = 0; i < routing_model.vehicles(); ++i) {
    start_to_vehicle_[routing_model.Start(i)] = i;
  }
}

bool VehicleVarFilter::AcceptPath(int64 path_start, int64 chain_start,
                                  int64 chain_end) {
  const int64 vehicle = start_to_vehicle_[path_start];
  int64 node = chain_start;
  while (node != chain_end) {
    if (!vehicle_vars_[node]->Contains(vehicle)) {
      return false;
    }
    node = GetNext(node);
  }
  return vehicle_vars_[node]->Contains(vehicle);
}

bool VehicleVarFilter::DisableFiltering() const {
  for (int i = 0; i < vehicle_vars_.size(); ++i) {
    if (IsVehicleVariableConstrained(i)) return false;
  }
  return true;
}

bool VehicleVarFilter::IsVehicleVariableConstrained(int index) const {
  const IntVar* const vehicle_var = vehicle_vars_[index];
  // If vehicle variable contains -1 (optional node), then we need to
  // add it to the "unconstrained" domain. Impact we don't filter mandatory
  // nodes made inactive here, but it is covered by other filters.
  const int adjusted_unconstrained_vehicle_var_domain_size =
      vehicle_var->Min() >= 0 ? unconstrained_vehicle_var_domain_size_
                              : unconstrained_vehicle_var_domain_size_ + 1;
  return vehicle_var->Size() != adjusted_unconstrained_vehicle_var_domain_size;
}

}  // namespace

IntVarLocalSearchFilter* MakeVehicleVarFilter(
    const RoutingModel& routing_model) {
  return routing_model.solver()->RevAlloc(new VehicleVarFilter(routing_model));
}

namespace {

class CumulBoundsPropagatorFilter : public IntVarLocalSearchFilter {
 public:
  explicit CumulBoundsPropagatorFilter(const RoutingDimension& dimension);
  bool Accept(const Assignment* delta, const Assignment* deltadelta,
              int64 objective_min, int64 objective_max) override;
  std::string DebugString() const override {
    return "CumulBoundsPropagatorFilter(" + propagator_.dimension().name() +
           ")";
  }

 private:
  CumulBoundsPropagator propagator_;
  const int64 cumul_offset_;
  SparseBitset<int64> delta_touched_;
  std::vector<int64> delta_nexts_;
};

CumulBoundsPropagatorFilter::CumulBoundsPropagatorFilter(
    const RoutingDimension& dimension)
    : IntVarLocalSearchFilter(dimension.model()->Nexts()),
      propagator_(&dimension),
      cumul_offset_(dimension.GetGlobalOptimizerOffset()),
      delta_touched_(Size()),
      delta_nexts_(Size()) {}

bool CumulBoundsPropagatorFilter::Accept(const Assignment* delta,
                                         const Assignment* deltadelta,
                                         int64 objective_min,
                                         int64 objective_max) {
  delta_touched_.ClearAll();
  for (const IntVarElement& delta_element :
       delta->IntVarContainer().elements()) {
    int64 index = -1;
    if (FindIndex(delta_element.Var(), &index)) {
      if (!delta_element.Bound()) {
        // LNS detected
        return true;
      }
      delta_touched_.Set(index);
      delta_nexts_[index] = delta_element.Value();
    }
  }
  const auto& next_accessor = [this](int64 index) {
    return delta_touched_[index] ? delta_nexts_[index] : Value(index);
  };

  return propagator_.PropagateCumulBounds(next_accessor, cumul_offset_);
}

}  // namespace

IntVarLocalSearchFilter* MakeCumulBoundsPropagatorFilter(
    const RoutingDimension& dimension) {
  return dimension.model()->solver()->RevAlloc(
      new CumulBoundsPropagatorFilter(dimension));
}

namespace {

class LPCumulFilter : public IntVarLocalSearchFilter {
 public:
  LPCumulFilter(const std::vector<IntVar*>& nexts,
                GlobalDimensionCumulOptimizer* optimizer,
                bool filter_objective_cost);
  bool Accept(const Assignment* delta, const Assignment* deltadelta,
              int64 objective_min, int64 objective_max) override;
  int64 GetAcceptedObjectiveValue() const override;
  void OnSynchronize(const Assignment* delta) override;
  int64 GetSynchronizedObjectiveValue() const override;
  std::string DebugString() const override {
    return "LPCumulFilter(" + optimizer_.dimension()->name() + ")";
  }

 private:
  GlobalDimensionCumulOptimizer& optimizer_;
  const bool filter_objective_cost_;
  int64 synchronized_cost_without_transit_;
  int64 delta_cost_without_transit_;
  SparseBitset<int64> delta_touched_;
  std::vector<int64> delta_nexts_;
};

LPCumulFilter::LPCumulFilter(const std::vector<IntVar*>& nexts,
                             GlobalDimensionCumulOptimizer* optimizer,
                             bool filter_objective_cost)
    : IntVarLocalSearchFilter(nexts),
      optimizer_(*optimizer),
      filter_objective_cost_(filter_objective_cost),
      synchronized_cost_without_transit_(-1),
      delta_cost_without_transit_(-1),
      delta_touched_(Size()),
      delta_nexts_(Size()) {}

bool LPCumulFilter::Accept(const Assignment* delta,
                           const Assignment* deltadelta, int64 objective_min,
                           int64 objective_max) {
  delta_touched_.ClearAll();
  for (const IntVarElement& delta_element :
       delta->IntVarContainer().elements()) {
    int64 index = -1;
    if (FindIndex(delta_element.Var(), &index)) {
      if (!delta_element.Bound()) {
        // LNS detected
        return true;
      }
      delta_touched_.Set(index);
      delta_nexts_[index] = delta_element.Value();
    }
  }
  const auto& next_accessor = [this](int64 index) {
    return delta_touched_[index] ? delta_nexts_[index] : Value(index);
  };

  if (!filter_objective_cost_) {
    // No need to compute the cost of the LP, only verify its feasibility.
    delta_cost_without_transit_ = 0;
    return optimizer_.IsFeasible(next_accessor);
  }

  if (!optimizer_.ComputeCumulCostWithoutFixedTransits(
          next_accessor, &delta_cost_without_transit_)) {
    // Infeasible.
    delta_cost_without_transit_ = kint64max;
    return false;
  }
  return delta_cost_without_transit_ <= objective_max;
}

int64 LPCumulFilter::GetAcceptedObjectiveValue() const {
  return delta_cost_without_transit_;
}

void LPCumulFilter::OnSynchronize(const Assignment* delta) {
  // TODO(user): Try to optimize this so the LP is not called when the last
  // computed delta cost corresponds to the solution being synchronized.
  if (!optimizer_.ComputeCumulCostWithoutFixedTransits(
          [this](int64 index) { return Value(index); },
          &synchronized_cost_without_transit_)) {
    // TODO(user): This should only happen if the LP solver times out.
    // DCHECK the fail wasn't due to an infeasible model.
    synchronized_cost_without_transit_ = 0;
  }
}

int64 LPCumulFilter::GetSynchronizedObjectiveValue() const {
  return synchronized_cost_without_transit_;
}

}  // namespace

IntVarLocalSearchFilter* MakeGlobalLPCumulFilter(
    GlobalDimensionCumulOptimizer* optimizer, bool filter_objective_cost) {
  const RoutingModel& model = *optimizer->dimension()->model();
  return model.solver()->RevAlloc(
      new LPCumulFilter(model.Nexts(), optimizer, filter_objective_cost));
}

const int64 CPFeasibilityFilter::kUnassigned = -1;

CPFeasibilityFilter::CPFeasibilityFilter(const RoutingModel* routing_model)
    : IntVarLocalSearchFilter(routing_model->Nexts()),
      model_(routing_model),
      solver_(routing_model->solver()),
      assignment_(solver_->MakeAssignment()),
      temp_assignment_(solver_->MakeAssignment()),
      restore_(solver_->MakeRestoreAssignment(temp_assignment_)) {
  assignment_->Add(routing_model->Nexts());
}

bool CPFeasibilityFilter::Accept(const Assignment* delta,
                                 const Assignment* deltadelta,
                                 int64 objective_min, int64 objective_max) {
  temp_assignment_->Copy(assignment_);
  AddDeltaToAssignment(delta, temp_assignment_);
  return solver_->Solve(restore_);
}

void CPFeasibilityFilter::OnSynchronize(const Assignment* delta) {
  AddDeltaToAssignment(delta, assignment_);
}

void CPFeasibilityFilter::AddDeltaToAssignment(const Assignment* delta,
                                               Assignment* assignment) {
  if (delta == nullptr) {
    return;
  }
  Assignment::IntContainer* const container =
      assignment->MutableIntVarContainer();
  const Assignment::IntContainer& delta_container = delta->IntVarContainer();
  const int delta_size = delta_container.Size();

  for (int i = 0; i < delta_size; i++) {
    const IntVarElement& delta_element = delta_container.Element(i);
    IntVar* const var = delta_element.Var();
    int64 index = kUnassigned;
    CHECK(FindIndex(var, &index));
    DCHECK_EQ(var, Var(index));
    const int64 value = delta_element.Value();

    container->AddAtPosition(var, index)->SetValue(value);
    if (model_->IsStart(index)) {
      if (model_->IsEnd(value)) {
        // Do not restore unused routes.
        container->MutableElement(index)->Deactivate();
      } else {
        // Re-activate the route's start in case it was deactivated before.
        container->MutableElement(index)->Activate();
      }
    }
  }
}

IntVarLocalSearchFilter* MakeCPFeasibilityFilter(
    const RoutingModel* routing_model) {
  return routing_model->solver()->RevAlloc(
      new CPFeasibilityFilter(routing_model));
}

// TODO(user): Implement same-vehicle filter. Could be merged with node
// precedence filter.

// --- VehicleTypeCurator ---

int VehicleTypeCurator::GetCompatibleVehicleOfType(
    int type, std::function<bool(int)> vehicle_is_compatible) {
  std::set<VehicleTypeCurator::VehicleClassEntry>& sorted_classes =
      sorted_vehicle_classes_per_type_[type];
  auto vehicle_class_it = sorted_classes.begin();

  while (vehicle_class_it != sorted_classes.end()) {
    const int vehicle_class = vehicle_class_it->vehicle_class;
    std::vector<int>& vehicles = vehicles_per_vehicle_class_[vehicle_class];
    DCHECK(!vehicles.empty());

    for (auto vehicle_it = vehicles.begin(); vehicle_it != vehicles.end();
         vehicle_it++) {
      const int vehicle = *vehicle_it;
      if (vehicle_is_compatible(vehicle)) {
        vehicles.erase(vehicle_it);
        if (vehicles.empty()) {
          sorted_classes.erase(vehicle_class_it);
        }
        return vehicle;
      }
    }
    // If no compatible vehicle was found in this class, move on to the next
    // vehicle class.
    vehicle_class_it++;
  }
  // No compatible vehicle of the given type was found.
  return -1;
}

// --- First solution decision builder ---

// IntVarFilteredDecisionBuilder

IntVarFilteredDecisionBuilder::IntVarFilteredDecisionBuilder(
    std::unique_ptr<IntVarFilteredHeuristic> heuristic)
    : heuristic_(std::move(heuristic)) {}

Decision* IntVarFilteredDecisionBuilder::Next(Solver* solver) {
  Assignment* const assignment = heuristic_->BuildSolution();
  if (assignment != nullptr) {
    VLOG(2) << "Number of decisions: " << heuristic_->number_of_decisions();
    VLOG(2) << "Number of rejected decisions: "
            << heuristic_->number_of_rejects();
    assignment->Restore();
  } else {
    solver->Fail();
  }
  return nullptr;
}

int64 IntVarFilteredDecisionBuilder::number_of_decisions() const {
  return heuristic_->number_of_decisions();
}

int64 IntVarFilteredDecisionBuilder::number_of_rejects() const {
  return heuristic_->number_of_rejects();
}

std::string IntVarFilteredDecisionBuilder::DebugString() const {
  return absl::StrCat("IntVarFilteredDecisionBuilder(",
                      heuristic_->DebugString(), ")");
}

// --- First solution heuristics ---

// IntVarFilteredHeuristic

IntVarFilteredHeuristic::IntVarFilteredHeuristic(
    Solver* solver, const std::vector<IntVar*>& vars,
    LocalSearchFilterManager* filter_manager)
    : assignment_(solver->MakeAssignment()),
      solver_(solver),
      vars_(vars),
      delta_(solver->MakeAssignment()),
      is_in_delta_(vars_.size(), false),
      empty_(solver->MakeAssignment()),
      filter_manager_(filter_manager),
      number_of_decisions_(0),
      number_of_rejects_(0) {
  assignment_->MutableIntVarContainer()->Resize(vars_.size());
  delta_indices_.reserve(vars_.size());
}

void IntVarFilteredHeuristic::ResetSolution() {
  number_of_decisions_ = 0;
  number_of_rejects_ = 0;
  // Wiping assignment when starting a new search.
  assignment_->MutableIntVarContainer()->Clear();
  assignment_->MutableIntVarContainer()->Resize(vars_.size());
  delta_->MutableIntVarContainer()->Clear();
  SynchronizeFilters();
}

Assignment* const IntVarFilteredHeuristic::BuildSolution() {
  ResetSolution();
  if (!InitializeSolution()) {
    return nullptr;
  }
  SynchronizeFilters();
  if (BuildSolutionInternal()) {
    return assignment_;
  }
  return nullptr;
}

const Assignment* RoutingFilteredHeuristic::BuildSolutionFromRoutes(
    const std::function<int64(int64)>& next_accessor) {
  ResetSolution();
  ResetVehicleIndices();
  // NOTE: We don't need to clear or pre-set the two following vectors as the
  // for loop below will set all elements.
  start_chain_ends_.resize(model()->vehicles());
  end_chain_starts_.resize(model()->vehicles());

  for (int v = 0; v < model_->vehicles(); v++) {
    int64 node = model_->Start(v);
    while (!model_->IsEnd(node)) {
      const int64 next = next_accessor(node);
      DCHECK_NE(next, node);
      SetValue(node, next);
      SetVehicleIndex(node, v);
      node = next;
    }
    // All vehicles have full routes from start to end here.
    start_chain_ends_[v] = model()->End(v);
    end_chain_starts_[v] = model()->Start(v);
  }
  if (!Commit()) {
    return nullptr;
  }
  SynchronizeFilters();
  if (BuildSolutionInternal()) {
    return assignment_;
  }
  return nullptr;
}

bool IntVarFilteredHeuristic::Commit() {
  ++number_of_decisions_;
  const bool accept = FilterAccept();
  if (accept) {
    const Assignment::IntContainer& delta_container = delta_->IntVarContainer();
    const int delta_size = delta_container.Size();
    Assignment::IntContainer* const container =
        assignment_->MutableIntVarContainer();
    for (int i = 0; i < delta_size; ++i) {
      const IntVarElement& delta_element = delta_container.Element(i);
      IntVar* const var = delta_element.Var();
      DCHECK_EQ(var, vars_[delta_indices_[i]]);
      container->AddAtPosition(var, delta_indices_[i])
          ->SetValue(delta_element.Value());
    }
    SynchronizeFilters();
  } else {
    ++number_of_rejects_;
  }
  // Reset is_in_delta to all false.
  for (const int delta_index : delta_indices_) {
    is_in_delta_[delta_index] = false;
  }
  delta_->Clear();
  delta_indices_.clear();
  return accept;
}

void IntVarFilteredHeuristic::SynchronizeFilters() {
  if (filter_manager_) filter_manager_->Synchronize(assignment_, delta_);
}

bool IntVarFilteredHeuristic::FilterAccept() {
  if (!filter_manager_) return true;
  LocalSearchMonitor* const monitor = solver_->GetLocalSearchMonitor();
  return filter_manager_->Accept(monitor, delta_, empty_, kint64min, kint64max);
}

// RoutingFilteredHeuristic

RoutingFilteredHeuristic::RoutingFilteredHeuristic(
    RoutingModel* model, LocalSearchFilterManager* filter_manager)
    : IntVarFilteredHeuristic(model->solver(), model->Nexts(), filter_manager),
      model_(model) {}

bool RoutingFilteredHeuristic::InitializeSolution() {
  // Find the chains of nodes (when nodes have their "Next" value bound in the
  // current solution, it forms a link in a chain). Eventually, starts[end]
  // will contain the index of the first node of the chain ending at node 'end'
  // and ends[start] will be the last node of the chain starting at node
  // 'start'. Values of starts[node] and ends[node] for other nodes is used
  // for intermediary computations and do not necessarily reflect actual chain
  // starts and ends.

  // Start by adding partial start chains to current assignment.
  start_chain_ends_.clear();
  start_chain_ends_.resize(model()->vehicles(), -1);
  end_chain_starts_.clear();
  end_chain_starts_.resize(model()->vehicles(), -1);

  ResetVehicleIndices();
  for (int vehicle = 0; vehicle < model()->vehicles(); ++vehicle) {
    int64 node = model()->Start(vehicle);
    while (!model()->IsEnd(node) && Var(node)->Bound()) {
      const int64 next = Var(node)->Min();
      SetValue(node, next);
      SetVehicleIndex(node, vehicle);
      node = next;
    }
    start_chain_ends_[vehicle] = node;
  }

  std::vector<int64> starts(Size() + model()->vehicles(), -1);
  std::vector<int64> ends(Size() + model()->vehicles(), -1);
  for (int node = 0; node < Size() + model()->vehicles(); ++node) {
    // Each node starts as a singleton chain.
    starts[node] = node;
    ends[node] = node;
  }
  std::vector<bool> touched(Size(), false);
  for (int node = 0; node < Size(); ++node) {
    int current = node;
    while (!model()->IsEnd(current) && !touched[current]) {
      touched[current] = true;
      IntVar* const next_var = Var(current);
      if (next_var->Bound()) {
        current = next_var->Value();
      }
    }
    // Merge the sub-chain starting from 'node' and ending at 'current' with
    // the existing sub-chain starting at 'current'.
    starts[ends[current]] = starts[node];
    ends[starts[node]] = ends[current];
  }

  // Set each route to be the concatenation of the chain at its starts and the
  // chain at its end, without nodes in between.
  for (int vehicle = 0; vehicle < model()->vehicles(); ++vehicle) {
    end_chain_starts_[vehicle] = starts[model()->End(vehicle)];
    int64 node = start_chain_ends_[vehicle];
    if (!model()->IsEnd(node)) {
      int64 next = starts[model()->End(vehicle)];
      SetValue(node, next);
      SetVehicleIndex(node, vehicle);
      node = next;
      while (!model()->IsEnd(node)) {
        next = Var(node)->Min();
        SetValue(node, next);
        SetVehicleIndex(node, vehicle);
        node = next;
      }
    }
  }

  if (!Commit()) {
    ResetVehicleIndices();
    return false;
  }
  return true;
}

void RoutingFilteredHeuristic::MakeDisjunctionNodesUnperformed(int64 node) {
  model()->ForEachNodeInDisjunctionWithMaxCardinalityFromIndex(
      node, 1, [this, node](int alternate) {
        if (node != alternate && !Contains(alternate)) {
          SetValue(alternate, alternate);
        }
      });
}

void RoutingFilteredHeuristic::MakeUnassignedNodesUnperformed() {
  for (int index = 0; index < Size(); ++index) {
    if (!Contains(index)) {
      SetValue(index, index);
    }
  }
}

// CheapestInsertionFilteredHeuristic

CheapestInsertionFilteredHeuristic::CheapestInsertionFilteredHeuristic(
    RoutingModel* model, std::function<int64(int64, int64, int64)> evaluator,
    std::function<int64(int64)> penalty_evaluator,
    LocalSearchFilterManager* filter_manager)
    : RoutingFilteredHeuristic(model, filter_manager),
      evaluator_(std::move(evaluator)),
      penalty_evaluator_(std::move(penalty_evaluator)) {}

std::vector<std::vector<CheapestInsertionFilteredHeuristic::StartEndValue>>
CheapestInsertionFilteredHeuristic::ComputeStartEndDistanceForVehicles(
    const std::vector<int>& vehicles) {
  std::vector<std::vector<StartEndValue>> start_end_distances_per_node(
      model()->Size());

  for (int node = 0; node < model()->Size(); node++) {
    if (Contains(node)) continue;
    std::vector<StartEndValue>& start_end_distances =
        start_end_distances_per_node[node];

    for (const int vehicle : vehicles) {
      const int64 start = model()->Start(vehicle);
      const int64 end = model()->End(vehicle);

      // We compute the distance of node to the start/end nodes of the route.
      const int64 distance =
          CapAdd(model()->GetArcCostForVehicle(start, node, vehicle),
                 model()->GetArcCostForVehicle(node, end, vehicle));
      start_end_distances.push_back({distance, vehicle});
    }
    // Sort the distances for the node to all start/ends of available vehicles
    // in decreasing order.
    std::sort(start_end_distances.begin(), start_end_distances.end(),
              [](const StartEndValue& first, const StartEndValue& second) {
                return second < first;
              });
  }
  return start_end_distances_per_node;
}

template <class Queue>
void CheapestInsertionFilteredHeuristic::InitializePriorityQueue(
    std::vector<std::vector<StartEndValue>>* start_end_distances_per_node,
    Queue* priority_queue) {
  const int num_nodes = model()->Size();
  DCHECK_EQ(start_end_distances_per_node->size(), num_nodes);

  for (int node = 0; node < num_nodes; node++) {
    if (Contains(node)) continue;
    std::vector<StartEndValue>& start_end_distances =
        (*start_end_distances_per_node)[node];
    if (start_end_distances.empty()) {
      continue;
    }
    // Put the best StartEndValue for this node in the priority queue.
    const StartEndValue& start_end_value = start_end_distances.back();
    priority_queue->push(std::make_pair(start_end_value, node));
    start_end_distances.pop_back();
  }
}

void CheapestInsertionFilteredHeuristic::InsertBetween(int64 node,
                                                       int64 predecessor,
                                                       int64 successor) {
  SetValue(predecessor, node);
  SetValue(node, successor);
  MakeDisjunctionNodesUnperformed(node);
}

void CheapestInsertionFilteredHeuristic::AppendEvaluatedPositionsAfter(
    int64 node_to_insert, int64 start, int64 next_after_start, int64 vehicle,
    std::vector<ValuedPosition>* valued_positions) {
  CHECK(valued_positions != nullptr);
  int64 insert_after = start;
  while (!model()->IsEnd(insert_after)) {
    const int64 insert_before =
        (insert_after == start) ? next_after_start : Value(insert_after);
    valued_positions->push_back(std::make_pair(
        GetInsertionCostForNodeAtPosition(node_to_insert, insert_after,
                                          insert_before, vehicle),
        insert_after));
    insert_after = insert_before;
  }
}

int64 CheapestInsertionFilteredHeuristic::GetInsertionCostForNodeAtPosition(
    int64 node_to_insert, int64 insert_after, int64 insert_before,
    int vehicle) const {
  return CapSub(CapAdd(evaluator_(insert_after, node_to_insert, vehicle),
                       evaluator_(node_to_insert, insert_before, vehicle)),
                evaluator_(insert_after, insert_before, vehicle));
}

int64 CheapestInsertionFilteredHeuristic::GetUnperformedValue(
    int64 node_to_insert) const {
  if (penalty_evaluator_ != nullptr) {
    return penalty_evaluator_(node_to_insert);
  }
  return kint64max;
}

namespace {
template <class T>
void SortAndExtractPairSeconds(std::vector<std::pair<int64, T>>* pairs,
                               std::vector<T>* sorted_seconds) {
  CHECK(pairs != nullptr);
  CHECK(sorted_seconds != nullptr);
  std::sort(pairs->begin(), pairs->end());
  sorted_seconds->reserve(pairs->size());
  for (const std::pair<int64, T>& p : *pairs) {
    sorted_seconds->push_back(p.second);
  }
}
}  // namespace

// Priority queue entries used by global cheapest insertion heuristic.

// Entry in priority queue containing the insertion positions of a node pair.
class GlobalCheapestInsertionFilteredHeuristic::PairEntry {
 public:
  PairEntry(int pickup_to_insert, int pickup_insert_after,
            int delivery_to_insert, int delivery_insert_after, int vehicle)
      : heap_index_(-1),
        value_(kint64max),
        pickup_to_insert_(pickup_to_insert),
        pickup_insert_after_(pickup_insert_after),
        delivery_to_insert_(delivery_to_insert),
        delivery_insert_after_(delivery_insert_after),
        vehicle_(vehicle) {}
  // Note: for compatibility reasons, comparator follows tie-breaking rules used
  // in the first version of GlobalCheapestInsertion.
  bool operator<(const PairEntry& other) const {
    // We first compare by value, then we favor insertions (vehicle != -1).
    // The rest of the tie-breaking is done with std::tie.
    if (value_ != other.value_) {
      return value_ > other.value_;
    }
    if ((vehicle_ == -1) ^ (other.vehicle_ == -1)) {
      return vehicle_ == -1;
    }
    return std::tie(pickup_insert_after_, pickup_to_insert_,
                    delivery_insert_after_, delivery_to_insert_, vehicle_) >
           std::tie(other.pickup_insert_after_, other.pickup_to_insert_,
                    other.delivery_insert_after_, other.delivery_to_insert_,
                    other.vehicle_);
  }
  void SetHeapIndex(int h) { heap_index_ = h; }
  int GetHeapIndex() const { return heap_index_; }
  int64 value() const { return value_; }
  void set_value(int64 value) { value_ = value; }
  int pickup_to_insert() const { return pickup_to_insert_; }
  int pickup_insert_after() const { return pickup_insert_after_; }
  int delivery_to_insert() const { return delivery_to_insert_; }
  int delivery_insert_after() const { return delivery_insert_after_; }
  int vehicle() const { return vehicle_; }

 private:
  int heap_index_;
  int64 value_;
  const int pickup_to_insert_;
  const int pickup_insert_after_;
  const int delivery_to_insert_;
  const int delivery_insert_after_;
  const int vehicle_;
};

// Entry in priority queue containing the insertion position of a node.
class GlobalCheapestInsertionFilteredHeuristic::NodeEntry {
 public:
  NodeEntry(int node_to_insert, int insert_after, int vehicle)
      : heap_index_(-1),
        value_(kint64max),
        node_to_insert_(node_to_insert),
        insert_after_(insert_after),
        vehicle_(vehicle) {}
  bool operator<(const NodeEntry& other) const {
    // See PairEntry::operator<(), above. This one is similar.
    if (value_ != other.value_) {
      return value_ > other.value_;
    }
    if ((vehicle_ == -1) ^ (other.vehicle_ == -1)) {
      return vehicle_ == -1;
    }
    return std::tie(insert_after_, node_to_insert_, vehicle_) >
           std::tie(other.insert_after_, other.node_to_insert_, other.vehicle_);
  }
  void SetHeapIndex(int h) { heap_index_ = h; }
  int GetHeapIndex() const { return heap_index_; }
  int64 value() const { return value_; }
  void set_value(int64 value) { value_ = value; }
  int node_to_insert() const { return node_to_insert_; }
  int insert_after() const { return insert_after_; }
  int vehicle() const { return vehicle_; }

 private:
  int heap_index_;
  int64 value_;
  const int node_to_insert_;
  const int insert_after_;
  const int vehicle_;
};

// GlobalCheapestInsertionFilteredHeuristic

GlobalCheapestInsertionFilteredHeuristic::
    GlobalCheapestInsertionFilteredHeuristic(
        RoutingModel* model,
        std::function<int64(int64, int64, int64)> evaluator,
        std::function<int64(int64)> penalty_evaluator,
        LocalSearchFilterManager* filter_manager,
        GlobalCheapestInsertionParameters parameters)
    : CheapestInsertionFilteredHeuristic(model, std::move(evaluator),
                                         std::move(penalty_evaluator),
                                         filter_manager),
      gci_params_(parameters),
      node_index_to_vehicle_(model->Size(), -1) {
  CHECK_GT(gci_params_.neighbors_ratio, 0);
  CHECK_LE(gci_params_.neighbors_ratio, 1);

  const int64 size = model->Size();
  const int64 num_neighbors = std::max(1.0, gci_params_.neighbors_ratio * size);

  if (num_neighbors >= size - 1) {
    // All nodes are neighbors, so we set the neighbors_ratio to 1 to avoid
    // unnecessary computations in the code.
    gci_params_.neighbors_ratio = 1;
  }

  if (gci_params_.neighbors_ratio == 1) {
    gci_params_.use_neighbors_ratio_for_initialization = false;
    for (int64 node = 0; node < size; node++) {
      if (!model->GetPickupIndexPairs(node).empty()) {
        pickup_nodes_.push_back(node);
      } else if (!model->GetDeliveryIndexPairs(node).empty()) {
        delivery_nodes_.push_back(node);
      } else {
        single_nodes_.push_back(node);
      }
    }
  }
}

void GlobalCheapestInsertionFilteredHeuristic::ComputeNeighborhoods() {
  if (gci_params_.neighbors_ratio == 1) {
    // Neighborhood computations not needed.
    return;
  }
  if (!node_index_to_single_neighbors_by_cost_class_.empty()) {
    // Neigborhoods already computed.
    DCHECK(!node_index_to_pickup_neighbors_by_cost_class_.empty());
    DCHECK(!node_index_to_delivery_neighbors_by_cost_class_.empty());
    return;
  }

  // TODO(user): Refactor the neighborhood computations in RoutingModel.
  const RoutingModel& routing_model = *model();
  const int64 size = routing_model.Size();
  const int64 num_neighbors = std::max(1.0, gci_params_.neighbors_ratio * size);
  // If num_neighbors was greater or equal size - 1, gci_params_.neighbors_ratio
  // should have been set to 1.
  DCHECK_LT(num_neighbors, size - 1);

  node_index_to_single_neighbors_by_cost_class_.resize(size);
  node_index_to_pickup_neighbors_by_cost_class_.resize(size);
  node_index_to_delivery_neighbors_by_cost_class_.resize(size);
  const int num_cost_classes = routing_model.GetCostClassesCount();
  for (int64 node_index = 0; node_index < size; node_index++) {
    node_index_to_single_neighbors_by_cost_class_[node_index].resize(
        num_cost_classes);
    node_index_to_pickup_neighbors_by_cost_class_[node_index].resize(
        num_cost_classes);
    node_index_to_delivery_neighbors_by_cost_class_[node_index].resize(
        num_cost_classes);
    for (int cc = 0; cc < num_cost_classes; cc++) {
      node_index_to_single_neighbors_by_cost_class_[node_index][cc] =
          absl::make_unique<SparseBitset<int64>>(size);
      node_index_to_pickup_neighbors_by_cost_class_[node_index][cc] =
          absl::make_unique<SparseBitset<int64>>(size);
      node_index_to_delivery_neighbors_by_cost_class_[node_index][cc] =
          absl::make_unique<SparseBitset<int64>>(size);
    }
  }

  for (int64 node_index = 0; node_index < size; ++node_index) {
    DCHECK(!routing_model.IsEnd(node_index));
    const bool node_is_pickup =
        !routing_model.GetPickupIndexPairs(node_index).empty();
    const bool node_is_delivery =
        !routing_model.GetDeliveryIndexPairs(node_index).empty();

    // TODO(user): Use the model's IndexNeighborFinder when available.
    for (int cost_class = 0; cost_class < num_cost_classes; cost_class++) {
      if (!routing_model.HasVehicleWithCostClassIndex(
              RoutingCostClassIndex(cost_class))) {
        // No vehicle with this cost class, avoid unnecessary computations.
        continue;
      }
      std::vector<std::pair</*cost*/ int64, /*node*/ int64>> costed_after_nodes;
      costed_after_nodes.reserve(size);
      for (int after_node = 0; after_node < size; ++after_node) {
        if (after_node != node_index) {
          costed_after_nodes.push_back(
              std::make_pair(routing_model.GetArcCostForClass(
                                 node_index, after_node, cost_class),
                             after_node));
        }
      }
      std::nth_element(costed_after_nodes.begin(),
                       costed_after_nodes.begin() + num_neighbors - 1,
                       costed_after_nodes.end());
      costed_after_nodes.resize(num_neighbors);

      for (const auto& costed_neighbor : costed_after_nodes) {
        const int64 neighbor = costed_neighbor.second;
        AddNeighborForCostClass(
            cost_class, node_index, neighbor,
            !routing_model.GetPickupIndexPairs(neighbor).empty(),
            !routing_model.GetDeliveryIndexPairs(neighbor).empty());

        // Add reverse neighborhood.
        DCHECK(!routing_model.IsEnd(neighbor));
        AddNeighborForCostClass(cost_class, neighbor, node_index,
                                node_is_pickup, node_is_delivery);
      }
    }
  }
}

void GlobalCheapestInsertionFilteredHeuristic::AddNeighborForCostClass(
    int cost_class, int64 node_index, int64 neighbor_index,
    bool neighbor_is_pickup, bool neighbor_is_delivery) {
  if (neighbor_is_pickup) {
    node_index_to_pickup_neighbors_by_cost_class_[node_index][cost_class]->Set(
        neighbor_index);
  }
  if (neighbor_is_delivery) {
    node_index_to_delivery_neighbors_by_cost_class_[node_index][cost_class]
        ->Set(neighbor_index);
  }
  if (!neighbor_is_pickup && !neighbor_is_delivery) {
    node_index_to_single_neighbors_by_cost_class_[node_index][cost_class]->Set(
        neighbor_index);
  }
}

bool GlobalCheapestInsertionFilteredHeuristic::IsNeighborForCostClass(
    int cost_class, int64 node_index, int64 neighbor_index) const {
  if (gci_params_.neighbors_ratio == 1) {
    return true;
  }
  const SparseBitset<int64>* neighbors;
  if (!model()->GetPickupIndexPairs(neighbor_index).empty()) {
    neighbors =
        node_index_to_pickup_neighbors_by_cost_class_[node_index][cost_class]
            .get();
  } else if (!model()->GetDeliveryIndexPairs(neighbor_index).empty()) {
    neighbors =
        node_index_to_delivery_neighbors_by_cost_class_[node_index][cost_class]
            .get();
  } else {
    neighbors =
        node_index_to_single_neighbors_by_cost_class_[node_index][cost_class]
            .get();
  }
  return (*neighbors)[neighbor_index];
}

bool GlobalCheapestInsertionFilteredHeuristic::CheckVehicleIndices() const {
  std::vector<bool> node_is_visited(model()->Size(), -1);
  for (int v = 0; v < model()->vehicles(); v++) {
    for (int node = model()->Start(v); !model()->IsEnd(node);
         node = Value(node)) {
      if (node_index_to_vehicle_[node] != v) {
        return false;
      }
      node_is_visited[node] = true;
    }
  }

  for (int node = 0; node < model()->Size(); node++) {
    if (!node_is_visited[node] && node_index_to_vehicle_[node] != -1) {
      return false;
    }
  }

  return true;
}

bool GlobalCheapestInsertionFilteredHeuristic::BuildSolutionInternal() {
  ComputeNeighborhoods();
  // Insert partially inserted pairs.
  absl::flat_hash_map<int, std::vector<int>> vehicle_to_pair_nodes;
  for (const RoutingModel::IndexPair& index_pair :
       model()->GetPickupAndDeliveryPairs()) {
    int pickup_vehicle = -1;
    for (int64 pickup : index_pair.first) {
      if (Contains(pickup)) {
        pickup_vehicle = node_index_to_vehicle_[pickup];
        break;
      }
    }
    int delivery_vehicle = -1;
    for (int64 delivery : index_pair.second) {
      if (Contains(delivery)) {
        delivery_vehicle = node_index_to_vehicle_[delivery];
        break;
      }
    }
    if (pickup_vehicle >= 0 && delivery_vehicle < 0) {
      std::vector<int>& pair_nodes = vehicle_to_pair_nodes[pickup_vehicle];
      for (int64 delivery : index_pair.second) {
        pair_nodes.push_back(delivery);
      }
    }
    if (pickup_vehicle < 0 && delivery_vehicle >= 0) {
      std::vector<int>& pair_nodes = vehicle_to_pair_nodes[delivery_vehicle];
      for (int64 pickup : index_pair.first) {
        pair_nodes.push_back(pickup);
      }
    }
  }
  for (const auto& vehicle_and_nodes : vehicle_to_pair_nodes) {
    InsertNodesOnRoutes(vehicle_and_nodes.second, {vehicle_and_nodes.first});
  }

  InsertNodesByRequirementTopologicalOrder();

  // TODO(user): Adapt the pair insertions to also support seed and
  // sequential insertion.
  InsertPairs();
  std::vector<int> nodes;
  for (int node = 0; node < model()->Size(); ++node) {
    if (!Contains(node)) {
      nodes.push_back(node);
    }
  }
  InsertFarthestNodesAsSeeds();
  if (gci_params_.is_sequential) {
    SequentialInsertNodes(nodes);
  } else {
    InsertNodesOnRoutes(nodes, {});
  }
  MakeUnassignedNodesUnperformed();
  DCHECK(CheckVehicleIndices());
  return Commit();
}

void GlobalCheapestInsertionFilteredHeuristic::
    InsertNodesByRequirementTopologicalOrder() {
  for (int type : model()->GetTopologicallySortedVisitTypes()) {
    InsertNodesOnRoutes(model()->GetSingleNodesOfType(type), {});
  }
}

void GlobalCheapestInsertionFilteredHeuristic::InsertPairs() {
  AdjustablePriorityQueue<PairEntry> priority_queue;
  std::vector<PairEntries> pickup_to_entries;
  std::vector<PairEntries> delivery_to_entries;
  InitializePairPositions(&priority_queue, &pickup_to_entries,
                          &delivery_to_entries);
  while (!priority_queue.IsEmpty()) {
    if (StopSearch()) {
      for (PairEntry* const entry : *priority_queue.Raw()) {
        delete entry;
      }
      return;
    }
    PairEntry* const entry = priority_queue.Top();
    if (Contains(entry->pickup_to_insert()) ||
        Contains(entry->delivery_to_insert())) {
      DeletePairEntry(entry, &priority_queue, &pickup_to_entries,
                      &delivery_to_entries);
      continue;
    }

    if (entry->vehicle() == -1) {
      // Pair is unperformed.
      SetValue(entry->pickup_to_insert(), entry->pickup_to_insert());
      SetValue(entry->delivery_to_insert(), entry->delivery_to_insert());
      if (!Commit()) {
        DeletePairEntry(entry, &priority_queue, &pickup_to_entries,
                        &delivery_to_entries);
      }
      continue;
    }

    // Pair is performed.
    const int64 pickup_insert_before = Value(entry->pickup_insert_after());
    InsertBetween(entry->pickup_to_insert(), entry->pickup_insert_after(),
                  pickup_insert_before);
    const int64 delivery_insert_before =
        (entry->pickup_to_insert() == entry->delivery_insert_after())
            ? pickup_insert_before
            : Value(entry->delivery_insert_after());
    InsertBetween(entry->delivery_to_insert(), entry->delivery_insert_after(),
                  delivery_insert_before);
    if (Commit()) {
      const int64 pickup_after = entry->pickup_insert_after();
      const int64 pickup = entry->pickup_to_insert();
      const int64 delivery_after = entry->delivery_insert_after();
      const int64 delivery = entry->delivery_to_insert();
      const int vehicle = entry->vehicle();
      UpdatePairPositions(vehicle, pickup_after, &priority_queue,
                          &pickup_to_entries, &delivery_to_entries);
      UpdatePairPositions(vehicle, pickup, &priority_queue, &pickup_to_entries,
                          &delivery_to_entries);
      UpdatePairPositions(vehicle, delivery, &priority_queue,
                          &pickup_to_entries, &delivery_to_entries);
      if (pickup != delivery_after) {
        UpdatePairPositions(vehicle, delivery_after, &priority_queue,
                            &pickup_to_entries, &delivery_to_entries);
      }
      SetVehicleIndex(pickup, vehicle);
      SetVehicleIndex(delivery, vehicle);
    } else {
      DeletePairEntry(entry, &priority_queue, &pickup_to_entries,
                      &delivery_to_entries);
    }
  }
}

void GlobalCheapestInsertionFilteredHeuristic::InsertNodesOnRoutes(
    const std::vector<int>& nodes, const absl::flat_hash_set<int>& vehicles) {
  AdjustablePriorityQueue<NodeEntry> priority_queue;
  std::vector<NodeEntries> position_to_node_entries;
  InitializePositions(nodes, &priority_queue, &position_to_node_entries,
                      vehicles);
  const bool all_routes =
      vehicles.empty() || vehicles.size() == model()->vehicles();

  while (!priority_queue.IsEmpty()) {
    NodeEntry* const node_entry = priority_queue.Top();
    if (StopSearch()) {
      for (NodeEntry* const entry : *priority_queue.Raw()) {
        delete entry;
      }
      return;
    }
    const int64 node_to_insert = node_entry->node_to_insert();
    if (Contains(node_to_insert)) {
      DeleteNodeEntry(node_entry, &priority_queue, &position_to_node_entries);
      continue;
    }

    if (node_entry->vehicle() == -1) {
      // Node is unperformed.
      if (all_routes) {
        // Make node unperformed.
        SetValue(node_to_insert, node_to_insert);
        if (!Commit()) {
          DeleteNodeEntry(node_entry, &priority_queue,
                          &position_to_node_entries);
        }
      } else {
        DCHECK_EQ(node_entry->value(), 0);
        // In this case, all routes are not being considered simultaneously,
        // so we do not make nodes unperformed (they might be better performed
        // on some other route later).
        // Furthermore, since in this case the node penalty is necessarily
        // taken into account in the NodeEntry, the values "cost - penalty"
        // for all nodes are now positive for all remaining entries in the
        // priority queue, so we can empty the priority queue.
        DeleteNodeEntry(node_entry, &priority_queue, &position_to_node_entries);
        while (!priority_queue.IsEmpty()) {
          NodeEntry* const to_delete = priority_queue.Top();
          DeleteNodeEntry(to_delete, &priority_queue,
                          &position_to_node_entries);
        }
      }
      continue;
    }

    // Make node performed.
    const int64 insert_after = node_entry->insert_after();
    InsertBetween(node_to_insert, insert_after, Value(insert_after));
    if (Commit()) {
      const int vehicle = node_entry->vehicle();
      UpdatePositions(nodes, vehicle, node_to_insert, &priority_queue,
                      &position_to_node_entries);
      UpdatePositions(nodes, vehicle, insert_after, &priority_queue,
                      &position_to_node_entries);
      SetVehicleIndex(node_to_insert, vehicle);
    } else {
      DeleteNodeEntry(node_entry, &priority_queue, &position_to_node_entries);
    }
  }
}

void GlobalCheapestInsertionFilteredHeuristic::SequentialInsertNodes(
    const std::vector<int>& nodes) {
  std::vector<bool> is_vehicle_used;
  absl::flat_hash_set<int> used_vehicles;
  std::vector<int> unused_vehicles;

  DetectUsedVehicles(&is_vehicle_used, &unused_vehicles, &used_vehicles);
  if (!used_vehicles.empty()) {
    InsertNodesOnRoutes(nodes, used_vehicles);
  }

  std::vector<std::vector<StartEndValue>> start_end_distances_per_node =
      ComputeStartEndDistanceForVehicles(unused_vehicles);
  std::priority_queue<Seed, std::vector<Seed>, std::greater<Seed>>
      first_node_queue;
  InitializePriorityQueue(&start_end_distances_per_node, &first_node_queue);

  int vehicle = InsertSeedNode(&start_end_distances_per_node, &first_node_queue,
                               &is_vehicle_used);

  while (vehicle >= 0) {
    InsertNodesOnRoutes(nodes, {vehicle});
    vehicle = InsertSeedNode(&start_end_distances_per_node, &first_node_queue,
                             &is_vehicle_used);
  }
}

void GlobalCheapestInsertionFilteredHeuristic::DetectUsedVehicles(
    std::vector<bool>* is_vehicle_used, std::vector<int>* unused_vehicles,
    absl::flat_hash_set<int>* used_vehicles) {
  is_vehicle_used->clear();
  is_vehicle_used->resize(model()->vehicles());

  used_vehicles->clear();
  used_vehicles->reserve(model()->vehicles());

  unused_vehicles->clear();
  unused_vehicles->reserve(model()->vehicles());

  for (int vehicle = 0; vehicle < model()->vehicles(); vehicle++) {
    if (Value(model()->Start(vehicle)) != model()->End(vehicle)) {
      (*is_vehicle_used)[vehicle] = true;
      used_vehicles->insert(vehicle);
    } else {
      (*is_vehicle_used)[vehicle] = false;
      unused_vehicles->push_back(vehicle);
    }
  }
}

void GlobalCheapestInsertionFilteredHeuristic::InsertFarthestNodesAsSeeds() {
  if (gci_params_.farthest_seeds_ratio <= 0) return;
  // Insert at least 1 farthest Seed if the parameter is positive.
  const int num_seeds = static_cast<int>(
      std::ceil(gci_params_.farthest_seeds_ratio * model()->vehicles()));

  std::vector<bool> is_vehicle_used;
  absl::flat_hash_set<int> used_vehicles;
  std::vector<int> unused_vehicles;
  DetectUsedVehicles(&is_vehicle_used, &unused_vehicles, &used_vehicles);
  std::vector<std::vector<StartEndValue>> start_end_distances_per_node =
      ComputeStartEndDistanceForVehicles(unused_vehicles);

  // Priority queue where the Seeds with a larger distance are given higher
  // priority.
  std::priority_queue<Seed> farthest_node_queue;
  InitializePriorityQueue(&start_end_distances_per_node, &farthest_node_queue);

  int inserted_seeds = 0;
  while (!farthest_node_queue.empty() && inserted_seeds < num_seeds) {
    InsertSeedNode(&start_end_distances_per_node, &farthest_node_queue,
                   &is_vehicle_used);
    inserted_seeds++;
  }
}

template <class Queue>
int GlobalCheapestInsertionFilteredHeuristic::InsertSeedNode(
    std::vector<std::vector<StartEndValue>>* start_end_distances_per_node,
    Queue* priority_queue, std::vector<bool>* is_vehicle_used) {
  while (!priority_queue->empty()) {
    if (StopSearch()) break;
    const Seed& seed = priority_queue->top();

    const int seed_node = seed.second;
    const int seed_vehicle = seed.first.vehicle;

    std::vector<StartEndValue>& other_start_end_values =
        (*start_end_distances_per_node)[seed_node];

    if (Contains(seed_node)) {
      // The node is already inserted, it is therefore no longer considered as
      // a potential seed.
      priority_queue->pop();
      other_start_end_values.clear();
      continue;
    }
    if (!(*is_vehicle_used)[seed_vehicle]) {
      // Try to insert this seed_node on this vehicle's route.
      const int64 start = model()->Start(seed_vehicle);
      const int64 end = model()->End(seed_vehicle);
      DCHECK_EQ(Value(start), end);
      InsertBetween(seed_node, start, end);
      if (Commit()) {
        priority_queue->pop();
        (*is_vehicle_used)[seed_vehicle] = true;
        other_start_end_values.clear();
        SetVehicleIndex(seed_node, seed_vehicle);
        return seed_vehicle;
      }
    }
    // Either the vehicle is already used, or the Commit() wasn't successful.
    // In both cases, we remove this Seed from the priority queue, and insert
    // the next StartEndValue from start_end_distances_per_node[seed_node]
    // in the priority queue.
    priority_queue->pop();
    if (!other_start_end_values.empty()) {
      const StartEndValue& next_seed_value = other_start_end_values.back();
      priority_queue->push(std::make_pair(next_seed_value, seed_node));
      other_start_end_values.pop_back();
    }
  }
  // No seed node was inserted.
  return -1;
}

void GlobalCheapestInsertionFilteredHeuristic::InitializePairPositions(
    AdjustablePriorityQueue<
        GlobalCheapestInsertionFilteredHeuristic::PairEntry>* priority_queue,
    std::vector<GlobalCheapestInsertionFilteredHeuristic::PairEntries>*
        pickup_to_entries,
    std::vector<GlobalCheapestInsertionFilteredHeuristic::PairEntries>*
        delivery_to_entries) {
  priority_queue->Clear();
  pickup_to_entries->clear();
  pickup_to_entries->resize(model()->Size());
  delivery_to_entries->clear();
  delivery_to_entries->resize(model()->Size());
  for (const RoutingModel::IndexPair& index_pair :
       model()->GetPickupAndDeliveryPairs()) {
    for (int64 pickup : index_pair.first) {
      for (int64 delivery : index_pair.second) {
        if (Contains(pickup) || Contains(delivery)) {
          continue;
        }
        int64 penalty =
            FLAGS_routing_shift_insertion_cost_by_penalty ? kint64max : 0;
        // Add insertion entry making pair unperformed. When the pair is part
        // of a disjunction we do not try to make any of its pairs unperformed
        // as it requires having an entry with all pairs being unperformed.
        // TODO(user): Adapt the code to make pair disjunctions unperformed.
        if (index_pair.first.size() == 1 && index_pair.second.size() == 1) {
          const int64 pickup_penalty = GetUnperformedValue(pickup);
          const int64 delivery_penalty = GetUnperformedValue(delivery);
          if (pickup_penalty != kint64max && delivery_penalty != kint64max) {
            PairEntry* const entry =
                new PairEntry(pickup, -1, delivery, -1, -1);
            if (FLAGS_routing_shift_insertion_cost_by_penalty) {
              entry->set_value(0);
              penalty = CapAdd(pickup_penalty, delivery_penalty);
            } else {
              entry->set_value(CapAdd(pickup_penalty, delivery_penalty));
              penalty = 0;
            }
            priority_queue->Add(entry);
          }
        }
        // Add all other insertion entries with pair performed.
        InitializeInsertionEntriesPerformingPair(
            pickup, delivery, penalty, priority_queue, pickup_to_entries,
            delivery_to_entries);
      }
    }
  }
}

void GlobalCheapestInsertionFilteredHeuristic::
    InitializeInsertionEntriesPerformingPair(
        int64 pickup, int64 delivery, int64 penalty,
        AdjustablePriorityQueue<
            GlobalCheapestInsertionFilteredHeuristic::PairEntry>*
            priority_queue,
        std::vector<GlobalCheapestInsertionFilteredHeuristic::PairEntries>*
            pickup_to_entries,
        std::vector<GlobalCheapestInsertionFilteredHeuristic::PairEntries>*
            delivery_to_entries) {
  if (!gci_params_.use_neighbors_ratio_for_initialization) {
    std::vector<std::pair<std::pair<int64, int>, std::pair<int64, int64>>>
        valued_positions;
    for (int vehicle = 0; vehicle < model()->vehicles(); ++vehicle) {
      std::vector<ValuedPosition> valued_pickup_positions;
      const int64 start = model()->Start(vehicle);
      AppendEvaluatedPositionsAfter(pickup, start, Value(start), vehicle,
                                    &valued_pickup_positions);
      for (const ValuedPosition& valued_pickup_position :
           valued_pickup_positions) {
        const int64 pickup_position = valued_pickup_position.second;
        CHECK(!model()->IsEnd(pickup_position));
        std::vector<ValuedPosition> valued_delivery_positions;
        AppendEvaluatedPositionsAfter(delivery, pickup, Value(pickup_position),
                                      vehicle, &valued_delivery_positions);
        for (const ValuedPosition& valued_delivery_position :
             valued_delivery_positions) {
          valued_positions.push_back(std::make_pair(
              std::make_pair(CapAdd(valued_pickup_position.first,
                                    valued_delivery_position.first),
                             vehicle),
              std::make_pair(pickup_position,
                             valued_delivery_position.second)));
        }
      }
    }
    for (const std::pair<std::pair<int64, int>, std::pair<int64, int64>>&
             valued_position : valued_positions) {
      PairEntry* const entry = new PairEntry(
          pickup, valued_position.second.first, delivery,
          valued_position.second.second, valued_position.first.second);
      entry->set_value(CapSub(valued_position.first.first, penalty));
      pickup_to_entries->at(valued_position.second.first).insert(entry);
      DCHECK_NE(valued_position.second.first, valued_position.second.second);
      delivery_to_entries->at(valued_position.second.second).insert(entry);
      priority_queue->Add(entry);
    }
    return;
  }

  // We're only considering the closest neighbors as insertion positions for
  // the pickup/delivery pair.
  for (int cost_class = 0; cost_class < model()->GetCostClassesCount();
       cost_class++) {
    absl::flat_hash_set<std::pair<int64, int64>> existing_insertion_positions;
    // Explore the neighborhood of the pickup.
    for (const std::vector<int64>* const neighbors :
         GetNeighborsOfNodeForCostClass(cost_class, pickup)) {
      for (const int64 pickup_insert_after : *neighbors) {
        if (!Contains(pickup_insert_after)) {
          continue;
        }
        const int vehicle = node_index_to_vehicle_[pickup_insert_after];
        if (model()->GetCostClassIndexOfVehicle(vehicle).value() !=
            cost_class) {
          continue;
        }

        const int64 pickup_value = GetInsertionCostForNodeAtPosition(
            pickup, pickup_insert_after, Value(pickup_insert_after), vehicle);
        int64 delivery_insert_after = pickup;
        while (!model()->IsEnd(delivery_insert_after)) {
          const std::pair<int64, int64> insertion_position = {
              pickup_insert_after, delivery_insert_after};
          DCHECK(!gtl::ContainsKey(existing_insertion_positions,
                                   insertion_position));
          existing_insertion_positions.insert(insertion_position);
          PairEntry* const entry =
              new PairEntry(pickup, pickup_insert_after, delivery,
                            delivery_insert_after, vehicle);
          pickup_to_entries->at(pickup_insert_after).insert(entry);
          delivery_to_entries->at(delivery_insert_after).insert(entry);

          const int64 delivery_insert_before =
              (delivery_insert_after == pickup) ? Value(pickup_insert_after)
                                                : Value(delivery_insert_after);
          const int64 delivery_value = GetInsertionCostForNodeAtPosition(
              delivery, delivery_insert_after, delivery_insert_before, vehicle);
          entry->set_value(
              CapSub(CapAdd(pickup_value, delivery_value), penalty));
          priority_queue->Add(entry);
          delivery_insert_after = delivery_insert_before;
        }
      }
    }

    // Explore the neighborhood of the delivery.
    for (const std::vector<int64>* const neighbors :
         GetNeighborsOfNodeForCostClass(cost_class, delivery)) {
      for (const int64 delivery_insert_after : *neighbors) {
        if (!Contains(delivery_insert_after)) {
          continue;
        }
        const int vehicle = node_index_to_vehicle_[delivery_insert_after];
        if (model()->GetCostClassIndexOfVehicle(vehicle).value() !=
            cost_class) {
          continue;
        }

        const int64 delivery_value = GetInsertionCostForNodeAtPosition(
            delivery, delivery_insert_after, Value(delivery_insert_after),
            vehicle);
        int64 pickup_insert_after = model()->Start(vehicle);
        while (pickup_insert_after != delivery_insert_after) {
          const int64 pickup_insert_before = Value(pickup_insert_after);
          if (gtl::ContainsKey(
                  existing_insertion_positions,
                  std::make_pair(pickup_insert_after, delivery_insert_after))) {
            pickup_insert_after = pickup_insert_before;
            continue;
          }
          PairEntry* const entry =
              new PairEntry(pickup, pickup_insert_after, delivery,
                            delivery_insert_after, vehicle);
          pickup_to_entries->at(pickup_insert_after).insert(entry);
          delivery_to_entries->at(delivery_insert_after).insert(entry);
          const int64 pickup_value = GetInsertionCostForNodeAtPosition(
              pickup, pickup_insert_after, pickup_insert_before, vehicle);
          entry->set_value(
              CapSub(CapAdd(pickup_value, delivery_value), penalty));
          priority_queue->Add(entry);
          pickup_insert_after = pickup_insert_before;
        }
      }
    }
  }
}

void GlobalCheapestInsertionFilteredHeuristic::UpdatePickupPositions(
    int vehicle, int64 pickup_insert_after,
    AdjustablePriorityQueue<
        GlobalCheapestInsertionFilteredHeuristic::PairEntry>* priority_queue,
    std::vector<GlobalCheapestInsertionFilteredHeuristic::PairEntries>*
        pickup_to_entries,
    std::vector<GlobalCheapestInsertionFilteredHeuristic::PairEntries>*
        delivery_to_entries) {
  // First, remove entries which have already been inserted and keep track of
  // the entries which are being kept and must be updated.
  using Pair = std::pair<int64, int64>;
  using Insertion = std::pair<Pair, /*delivery_insert_after*/ int64>;
  absl::flat_hash_set<Insertion> existing_insertions;
  std::vector<PairEntry*> to_remove;
  for (PairEntry* const pair_entry :
       pickup_to_entries->at(pickup_insert_after)) {
    DCHECK(priority_queue->Contains(pair_entry));
    DCHECK_EQ(pair_entry->pickup_insert_after(), pickup_insert_after);
    if (Contains(pair_entry->pickup_to_insert()) ||
        Contains(pair_entry->delivery_to_insert())) {
      to_remove.push_back(pair_entry);
    } else {
      existing_insertions.insert(
          {{pair_entry->pickup_to_insert(), pair_entry->delivery_to_insert()},
           pair_entry->delivery_insert_after()});
    }
  }
  for (PairEntry* const pair_entry : to_remove) {
    DeletePairEntry(pair_entry, priority_queue, pickup_to_entries,
                    delivery_to_entries);
  }
  // Create new entries for which the pickup is to be inserted after
  // pickup_insert_after.
  const int cost_class = model()->GetCostClassIndexOfVehicle(vehicle).value();
  const int64 pickup_insert_before = Value(pickup_insert_after);
  for (int64 pickup :
       GetPickupNeighborsOfNodeForCostClass(cost_class, pickup_insert_after)) {
    if (Contains(pickup)) {
      continue;
    }
    for (const auto& pickup_index_pair : model()->GetPickupIndexPairs(pickup)) {
      const RoutingModel::IndexPair& index_pair =
          model()->GetPickupAndDeliveryPairs()[pickup_index_pair.first];
      for (int64 delivery : index_pair.second) {
        if (Contains(delivery)) {
          continue;
        }
        int64 delivery_insert_after = pickup;
        while (!model()->IsEnd(delivery_insert_after)) {
          const std::pair<Pair, int64> insertion = {{pickup, delivery},
                                                    delivery_insert_after};
          if (!gtl::ContainsKey(existing_insertions, insertion)) {
            PairEntry* const entry =
                new PairEntry(pickup, pickup_insert_after, delivery,
                              delivery_insert_after, vehicle);
            pickup_to_entries->at(pickup_insert_after).insert(entry);
            delivery_to_entries->at(delivery_insert_after).insert(entry);
          }
          if (delivery_insert_after == pickup) {
            delivery_insert_after = pickup_insert_before;
          } else {
            delivery_insert_after = Value(delivery_insert_after);
          }
        }
      }
    }
  }
  // Compute new value of entries and either update the priority queue
  // accordingly if the entry already existed or add it to the queue if it's
  // new.
  for (PairEntry* const pair_entry :
       pickup_to_entries->at(pickup_insert_after)) {
    const int64 pickup = pair_entry->pickup_to_insert();
    const int64 delivery = pair_entry->delivery_to_insert();
    DCHECK_EQ(pickup_insert_after, pair_entry->pickup_insert_after());
    const int64 pickup_value = GetInsertionCostForNodeAtPosition(
        pickup, pickup_insert_after, pickup_insert_before, vehicle);
    const int64 delivery_insert_after = pair_entry->delivery_insert_after();
    const int64 delivery_insert_before = (delivery_insert_after == pickup)
                                             ? pickup_insert_before
                                             : Value(delivery_insert_after);
    const int64 delivery_value = GetInsertionCostForNodeAtPosition(
        delivery, delivery_insert_after, delivery_insert_before, vehicle);
    const int64 penalty =
        FLAGS_routing_shift_insertion_cost_by_penalty
            ? CapAdd(GetUnperformedValue(pickup), GetUnperformedValue(delivery))
            : 0;
    pair_entry->set_value(
        CapSub(CapAdd(pickup_value, delivery_value), penalty));
    if (priority_queue->Contains(pair_entry)) {
      priority_queue->NoteChangedPriority(pair_entry);
    } else {
      priority_queue->Add(pair_entry);
    }
  }
}

void GlobalCheapestInsertionFilteredHeuristic::UpdateDeliveryPositions(
    int vehicle, int64 delivery_insert_after,
    AdjustablePriorityQueue<
        GlobalCheapestInsertionFilteredHeuristic::PairEntry>* priority_queue,
    std::vector<GlobalCheapestInsertionFilteredHeuristic::PairEntries>*
        pickup_to_entries,
    std::vector<GlobalCheapestInsertionFilteredHeuristic::PairEntries>*
        delivery_to_entries) {
  // First, remove entries which have already been inserted and keep track of
  // the entries which are being kept and must be updated.
  using Pair = std::pair<int64, int64>;
  using Insertion = std::pair<Pair, /*pickup_insert_after*/ int64>;
  absl::flat_hash_set<Insertion> existing_insertions;
  std::vector<PairEntry*> to_remove;
  for (PairEntry* const pair_entry :
       delivery_to_entries->at(delivery_insert_after)) {
    DCHECK(priority_queue->Contains(pair_entry));
    DCHECK_EQ(pair_entry->delivery_insert_after(), delivery_insert_after);
    if (Contains(pair_entry->pickup_to_insert()) ||
        Contains(pair_entry->delivery_to_insert())) {
      to_remove.push_back(pair_entry);
    } else {
      existing_insertions.insert(
          {{pair_entry->pickup_to_insert(), pair_entry->delivery_to_insert()},
           pair_entry->pickup_insert_after()});
    }
  }
  for (PairEntry* const pair_entry : to_remove) {
    DeletePairEntry(pair_entry, priority_queue, pickup_to_entries,
                    delivery_to_entries);
  }
  // Create new entries for which the delivery is to be inserted after
  // delivery_insert_after.
  const int cost_class = model()->GetCostClassIndexOfVehicle(vehicle).value();
  const int64 delivery_insert_before = Value(delivery_insert_after);
  for (int64 delivery : GetDeliveryNeighborsOfNodeForCostClass(
           cost_class, delivery_insert_after)) {
    if (Contains(delivery)) {
      continue;
    }
    for (const auto& delivery_index_pair :
         model()->GetDeliveryIndexPairs(delivery)) {
      const RoutingModel::IndexPair& index_pair =
          model()->GetPickupAndDeliveryPairs()[delivery_index_pair.first];
      for (int64 pickup : index_pair.first) {
        if (Contains(pickup)) {
          continue;
        }
        int64 pickup_insert_after = model()->Start(vehicle);
        while (pickup_insert_after != delivery_insert_after) {
          std::pair<Pair, int64> insertion = {{pickup, delivery},
                                              pickup_insert_after};
          if (!gtl::ContainsKey(existing_insertions, insertion)) {
            PairEntry* const entry =
                new PairEntry(pickup, pickup_insert_after, delivery,
                              delivery_insert_after, vehicle);
            pickup_to_entries->at(pickup_insert_after).insert(entry);
            delivery_to_entries->at(delivery_insert_after).insert(entry);
          }
          pickup_insert_after = Value(pickup_insert_after);
        }
      }
    }
  }
  // Compute new value of entries and either update the priority queue
  // accordingly if the entry already existed or add it to the queue if it's
  // new.
  for (PairEntry* const pair_entry :
       delivery_to_entries->at(delivery_insert_after)) {
    const int64 pickup = pair_entry->pickup_to_insert();
    const int64 delivery = pair_entry->delivery_to_insert();
    DCHECK_EQ(delivery_insert_after, pair_entry->delivery_insert_after());
    const int64 pickup_insert_after = pair_entry->pickup_insert_after();
    const int64 pickup_value = GetInsertionCostForNodeAtPosition(
        pickup, pickup_insert_after, Value(pickup_insert_after), vehicle);
    const int64 delivery_value = GetInsertionCostForNodeAtPosition(
        delivery, delivery_insert_after, delivery_insert_before, vehicle);
    const int64 penalty =
        FLAGS_routing_shift_insertion_cost_by_penalty
            ? CapAdd(GetUnperformedValue(pickup), GetUnperformedValue(delivery))
            : 0;
    pair_entry->set_value(
        CapSub(CapAdd(pickup_value, delivery_value), penalty));
    if (priority_queue->Contains(pair_entry)) {
      priority_queue->NoteChangedPriority(pair_entry);
    } else {
      priority_queue->Add(pair_entry);
    }
  }
}

void GlobalCheapestInsertionFilteredHeuristic::DeletePairEntry(
    GlobalCheapestInsertionFilteredHeuristic::PairEntry* entry,
    AdjustablePriorityQueue<
        GlobalCheapestInsertionFilteredHeuristic::PairEntry>* priority_queue,
    std::vector<PairEntries>* pickup_to_entries,
    std::vector<PairEntries>* delivery_to_entries) {
  priority_queue->Remove(entry);
  if (entry->pickup_insert_after() != -1) {
    pickup_to_entries->at(entry->pickup_insert_after()).erase(entry);
  }
  if (entry->delivery_insert_after() != -1) {
    delivery_to_entries->at(entry->delivery_insert_after()).erase(entry);
  }
  delete entry;
}

void GlobalCheapestInsertionFilteredHeuristic::InitializePositions(
    const std::vector<int>& nodes,
    AdjustablePriorityQueue<
        GlobalCheapestInsertionFilteredHeuristic::NodeEntry>* priority_queue,
    std::vector<GlobalCheapestInsertionFilteredHeuristic::NodeEntries>*
        position_to_node_entries,
    const absl::flat_hash_set<int>& vehicles) {
  priority_queue->Clear();
  position_to_node_entries->clear();
  position_to_node_entries->resize(model()->Size());

  const int num_vehicles =
      vehicles.empty() ? model()->vehicles() : vehicles.size();

  for (int node : nodes) {
    if (Contains(node)) {
      continue;
    }
    const int64 node_penalty = GetUnperformedValue(node);
    int64 penalty =
        FLAGS_routing_shift_insertion_cost_by_penalty ? kint64max : 0;
    // Add insertion entry making node unperformed.
    if (node_penalty != kint64max) {
      NodeEntry* const node_entry = new NodeEntry(node, -1, -1);
      if (FLAGS_routing_shift_insertion_cost_by_penalty ||
          num_vehicles < model()->vehicles()) {
        // In the case where we're not considering all routes simultaneously,
        // always shift insertion costs by penalty.
        node_entry->set_value(0);
        penalty = node_penalty;
      } else {
        node_entry->set_value(node_penalty);
        penalty = 0;
      }
      priority_queue->Add(node_entry);
    }
    // Add all insertion entries making node performed.
    InitializeInsertionEntriesPerformingNode(
        node, penalty, vehicles, priority_queue, position_to_node_entries);
  }
}

void GlobalCheapestInsertionFilteredHeuristic::
    InitializeInsertionEntriesPerformingNode(
        int64 node, int64 penalty, const absl::flat_hash_set<int>& vehicles,
        AdjustablePriorityQueue<
            GlobalCheapestInsertionFilteredHeuristic::NodeEntry>*
            priority_queue,
        std::vector<GlobalCheapestInsertionFilteredHeuristic::NodeEntries>*
            position_to_node_entries) {
  const int num_vehicles =
      vehicles.empty() ? model()->vehicles() : vehicles.size();
  if (!gci_params_.use_neighbors_ratio_for_initialization) {
    auto vehicles_it = vehicles.begin();
    for (int v = 0; v < num_vehicles; v++) {
      const int vehicle = vehicles.empty() ? v : *vehicles_it++;

      std::vector<ValuedPosition> valued_positions;
      const int64 start = model()->Start(vehicle);
      AppendEvaluatedPositionsAfter(node, start, Value(start), vehicle,
                                    &valued_positions);
      for (const std::pair<int64, int64>& valued_position : valued_positions) {
        NodeEntry* const node_entry =
            new NodeEntry(node, valued_position.second, vehicle);
        node_entry->set_value(CapSub(valued_position.first, penalty));
        position_to_node_entries->at(valued_position.second).insert(node_entry);
        priority_queue->Add(node_entry);
      }
    }
    return;
  }

  // We're only considering the closest neighbors as insertion positions for
  // the node.
  absl::flat_hash_set<int> vehicles_to_consider;
  const bool all_vehicles = (num_vehicles == model()->vehicles());
  const auto insert_on_vehicle_for_cost_class = [this, &vehicles, all_vehicles](
                                                    int v, int cost_class) {
    return (model()->GetCostClassIndexOfVehicle(v).value() == cost_class) &&
           (all_vehicles || vehicles.contains(v));
  };
  for (int cost_class = 0; cost_class < model()->GetCostClassesCount();
       cost_class++) {
    for (const std::vector<int64>* const neighbors :
         GetNeighborsOfNodeForCostClass(cost_class, node)) {
      for (const int64 insert_after : *neighbors) {
        if (!Contains(insert_after)) {
          continue;
        }
        const int vehicle = node_index_to_vehicle_[insert_after];
        if (!insert_on_vehicle_for_cost_class(vehicle, cost_class)) {
          continue;
        }
        NodeEntry* const node_entry =
            new NodeEntry(node, insert_after, vehicle);
        node_entry->set_value(
            CapSub(GetInsertionCostForNodeAtPosition(
                       node, insert_after, Value(insert_after), vehicle),
                   penalty));
        position_to_node_entries->at(insert_after).insert(node_entry);
        priority_queue->Add(node_entry);
      }
    }
  }
}

void GlobalCheapestInsertionFilteredHeuristic::UpdatePositions(
    const std::vector<int>& nodes, int vehicle, int64 insert_after,
    AdjustablePriorityQueue<
        GlobalCheapestInsertionFilteredHeuristic::NodeEntry>* priority_queue,
    std::vector<GlobalCheapestInsertionFilteredHeuristic::NodeEntries>*
        node_entries) {
  // Either create new entries if we are inserting after a newly inserted node
  // or remove entries which have already been inserted.
  bool update = true;
  if (node_entries->at(insert_after).empty()) {
    update = false;
    const int cost_class = model()->GetCostClassIndexOfVehicle(vehicle).value();
    for (int node_to_insert : nodes) {
      if (!Contains(node_to_insert) &&
          IsNeighborForCostClass(cost_class, insert_after, node_to_insert)) {
        NodeEntry* const node_entry =
            new NodeEntry(node_to_insert, insert_after, vehicle);
        node_entries->at(insert_after).insert(node_entry);
      }
    }
  } else {
    std::vector<NodeEntry*> to_remove;
    for (NodeEntry* const node_entry : node_entries->at(insert_after)) {
      if (priority_queue->Contains(node_entry)) {
        DCHECK_EQ(node_entry->insert_after(), insert_after);
        if (Contains(node_entry->node_to_insert())) {
          to_remove.push_back(node_entry);
        }
      }
    }
    for (NodeEntry* const node_entry : to_remove) {
      DeleteNodeEntry(node_entry, priority_queue, node_entries);
    }
  }
  // Compute new value of entries and either update the priority queue
  // accordingly if the entry already existed or add it to the queue if it's
  // new.
  DCHECK_GE(model()->Size(), node_entries->at(insert_after).size());
  const int64 insert_before = Value(insert_after);
  for (NodeEntry* const node_entry : node_entries->at(insert_after)) {
    DCHECK_EQ(node_entry->insert_after(), insert_after);
    const int64 value = GetInsertionCostForNodeAtPosition(
        node_entry->node_to_insert(), insert_after, insert_before, vehicle);
    const int64 penalty =
        FLAGS_routing_shift_insertion_cost_by_penalty
            ? GetUnperformedValue(node_entry->node_to_insert())
            : 0;
    node_entry->set_value(CapSub(value, penalty));
    if (update) {
      priority_queue->NoteChangedPriority(node_entry);
    } else {
      priority_queue->Add(node_entry);
    }
  }
}

void GlobalCheapestInsertionFilteredHeuristic::DeleteNodeEntry(
    GlobalCheapestInsertionFilteredHeuristic::NodeEntry* entry,
    AdjustablePriorityQueue<
        GlobalCheapestInsertionFilteredHeuristic::NodeEntry>* priority_queue,
    std::vector<NodeEntries>* node_entries) {
  priority_queue->Remove(entry);
  if (entry->insert_after() != -1) {
    node_entries->at(entry->insert_after()).erase(entry);
  }
  delete entry;
}

// LocalCheapestInsertionFilteredHeuristic
// TODO(user): Add support for penalty costs.
LocalCheapestInsertionFilteredHeuristic::
    LocalCheapestInsertionFilteredHeuristic(
        RoutingModel* model,
        std::function<int64(int64, int64, int64)> evaluator,
        LocalSearchFilterManager* filter_manager)
    : CheapestInsertionFilteredHeuristic(model, std::move(evaluator), nullptr,
                                         filter_manager) {
  std::vector<int> all_vehicles(model->vehicles());
  std::iota(std::begin(all_vehicles), std::end(all_vehicles), 0);

  start_end_distances_per_node_ =
      ComputeStartEndDistanceForVehicles(all_vehicles);
}

bool LocalCheapestInsertionFilteredHeuristic::BuildSolutionInternal() {
  // Marking if we've tried inserting a node.
  std::vector<bool> visited(model()->Size(), false);
  // Possible positions where the current node can inserted.
  std::vector<int64> insertion_positions;
  // Possible positions where its associated delivery node can inserted (if the
  // current node has one).
  std::vector<int64> delivery_insertion_positions;
  // Iterating on pickup and delivery pairs
  const RoutingModel::IndexPairs& index_pairs =
      model()->GetPickupAndDeliveryPairs();
  for (const auto& index_pair : index_pairs) {
    for (int64 pickup : index_pair.first) {
      if (Contains(pickup)) {
        continue;
      }
      for (int64 delivery : index_pair.second) {
        // If either is already in the solution, let it be inserted in the
        // standard node insertion loop.
        if (Contains(delivery)) {
          continue;
        }
        if (StopSearch()) return false;
        visited[pickup] = true;
        visited[delivery] = true;
        ComputeEvaluatorSortedPositions(pickup, &insertion_positions);
        for (const int64 pickup_insertion : insertion_positions) {
          const int pickup_insertion_next = Value(pickup_insertion);
          ComputeEvaluatorSortedPositionsOnRouteAfter(
              delivery, pickup, pickup_insertion_next,
              &delivery_insertion_positions);
          bool found = false;
          for (const int64 delivery_insertion : delivery_insertion_positions) {
            InsertBetween(pickup, pickup_insertion, pickup_insertion_next);
            const int64 delivery_insertion_next =
                (delivery_insertion == pickup_insertion) ? pickup
                : (delivery_insertion == pickup)         ? pickup_insertion_next
                                                 : Value(delivery_insertion);
            InsertBetween(delivery, delivery_insertion,
                          delivery_insertion_next);
            if (Commit()) {
              found = true;
              break;
            }
          }
          if (found) {
            break;
          }
        }
      }
    }
  }

  std::priority_queue<Seed> node_queue;
  InitializePriorityQueue(&start_end_distances_per_node_, &node_queue);

  while (!node_queue.empty()) {
    const int node = node_queue.top().second;
    node_queue.pop();
    if (Contains(node) || visited[node]) continue;
    ComputeEvaluatorSortedPositions(node, &insertion_positions);
    for (const int64 insertion : insertion_positions) {
      if (StopSearch()) return false;
      InsertBetween(node, insertion, Value(insertion));
      if (Commit()) {
        break;
      }
    }
  }
  MakeUnassignedNodesUnperformed();
  return Commit();
}

void LocalCheapestInsertionFilteredHeuristic::ComputeEvaluatorSortedPositions(
    int64 node, std::vector<int64>* sorted_positions) {
  CHECK(sorted_positions != nullptr);
  CHECK(!Contains(node));
  sorted_positions->clear();
  const int size = model()->Size();
  if (node < size) {
    std::vector<std::pair<int64, int64>> valued_positions;
    for (int vehicle = 0; vehicle < model()->vehicles(); ++vehicle) {
      const int64 start = model()->Start(vehicle);
      AppendEvaluatedPositionsAfter(node, start, Value(start), vehicle,
                                    &valued_positions);
    }
    SortAndExtractPairSeconds(&valued_positions, sorted_positions);
  }
}

void LocalCheapestInsertionFilteredHeuristic::
    ComputeEvaluatorSortedPositionsOnRouteAfter(
        int64 node, int64 start, int64 next_after_start,
        std::vector<int64>* sorted_positions) {
  CHECK(sorted_positions != nullptr);
  CHECK(!Contains(node));
  sorted_positions->clear();
  const int size = model()->Size();
  if (node < size) {
    // TODO(user): Take vehicle into account.
    std::vector<std::pair<int64, int64>> valued_positions;
    AppendEvaluatedPositionsAfter(node, start, next_after_start, 0,
                                  &valued_positions);
    SortAndExtractPairSeconds(&valued_positions, sorted_positions);
  }
}

// CheapestAdditionFilteredHeuristic

CheapestAdditionFilteredHeuristic::CheapestAdditionFilteredHeuristic(
    RoutingModel* model, LocalSearchFilterManager* filter_manager)
    : RoutingFilteredHeuristic(model, filter_manager) {}

bool CheapestAdditionFilteredHeuristic::BuildSolutionInternal() {
  const int kUnassigned = -1;
  const RoutingModel::IndexPairs& pairs = model()->GetPickupAndDeliveryPairs();
  std::vector<std::vector<int64>> deliveries(Size());
  std::vector<std::vector<int64>> pickups(Size());
  for (const RoutingModel::IndexPair& pair : pairs) {
    for (int first : pair.first) {
      for (int second : pair.second) {
        deliveries[first].push_back(second);
        pickups[second].push_back(first);
      }
    }
  }
  // To mimic the behavior of PathSelector (cf. search.cc), iterating on
  // routes with partial route at their start first then on routes with largest
  // index.
  std::vector<int> sorted_vehicles(model()->vehicles(), 0);
  for (int vehicle = 0; vehicle < model()->vehicles(); ++vehicle) {
    sorted_vehicles[vehicle] = vehicle;
  }
  std::sort(sorted_vehicles.begin(), sorted_vehicles.end(),
            PartialRoutesAndLargeVehicleIndicesFirst(*this));
  // Neighbors of the node currently being extended.
  for (const int vehicle : sorted_vehicles) {
    int64 last_node = GetStartChainEnd(vehicle);
    bool extend_route = true;
    // Extend the route of the current vehicle while it's possible. We can
    // iterate more than once if pickup and delivery pairs have been inserted
    // in the last iteration (see comment below); the new iteration will try to
    // extend the route after the last delivery on the route.
    while (extend_route) {
      extend_route = false;
      bool found = true;
      int64 index = last_node;
      int64 end = GetEndChainStart(vehicle);
      // Extend the route until either the end node of the vehicle is reached
      // or no node or node pair can be added. Deliveries in pickup and
      // delivery pairs are added at the same time as pickups, at the end of the
      // route, in reverse order of the pickups. Deliveries are never added
      // alone.
      while (found && !model()->IsEnd(index)) {
        found = false;
        std::vector<int64> neighbors;
        if (index < model()->Nexts().size()) {
          std::unique_ptr<IntVarIterator> it(
              model()->Nexts()[index]->MakeDomainIterator(false));
          auto next_values = InitAndGetValues(it.get());
          neighbors = GetPossibleNextsFromIterator(index, next_values.begin(),
                                                   next_values.end());
        }
        for (int i = 0; !found && i < neighbors.size(); ++i) {
          int64 next = -1;
          switch (i) {
            case 0:
              next = FindTopSuccessor(index, neighbors);
              break;
            case 1:
              SortSuccessors(index, &neighbors);
              ABSL_FALLTHROUGH_INTENDED;
            default:
              next = neighbors[i];
          }
          if (model()->IsEnd(next) && next != end) {
            continue;
          }
          // Only add a delivery if one of its pickups has been added already.
          if (!model()->IsEnd(next) && !pickups[next].empty()) {
            bool contains_pickups = false;
            for (int64 pickup : pickups[next]) {
              if (Contains(pickup)) {
                contains_pickups = true;
                break;
              }
            }
            if (!contains_pickups) {
              continue;
            }
          }
          std::vector<int64> next_deliveries;
          if (next < deliveries.size()) {
            next_deliveries = GetPossibleNextsFromIterator(
                next, deliveries[next].begin(), deliveries[next].end());
          }
          if (next_deliveries.empty()) next_deliveries = {kUnassigned};
          for (int j = 0; !found && j < next_deliveries.size(); ++j) {
            if (StopSearch()) return false;
            int delivery = -1;
            switch (j) {
              case 0:
                delivery = FindTopSuccessor(next, next_deliveries);
                break;
              case 1:
                SortSuccessors(next, &next_deliveries);
                ABSL_FALLTHROUGH_INTENDED;
              default:
                delivery = next_deliveries[j];
            }
            // Insert "next" after "index", and before "end" if it is not the
            // end already.
            SetValue(index, next);
            if (!model()->IsEnd(next)) {
              SetValue(next, end);
              MakeDisjunctionNodesUnperformed(next);
              if (delivery != kUnassigned) {
                SetValue(next, delivery);
                SetValue(delivery, end);
                MakeDisjunctionNodesUnperformed(delivery);
              }
            }
            if (Commit()) {
              index = next;
              found = true;
              if (delivery != kUnassigned) {
                if (model()->IsEnd(end) && last_node != delivery) {
                  last_node = delivery;
                  extend_route = true;
                }
                end = delivery;
              }
              break;
            }
          }
        }
      }
    }
  }
  MakeUnassignedNodesUnperformed();
  return Commit();
}

bool CheapestAdditionFilteredHeuristic::
    PartialRoutesAndLargeVehicleIndicesFirst::operator()(int vehicle1,
                                                         int vehicle2) const {
  const bool has_partial_route1 = (builder_.model()->Start(vehicle1) !=
                                   builder_.GetStartChainEnd(vehicle1));
  const bool has_partial_route2 = (builder_.model()->Start(vehicle2) !=
                                   builder_.GetStartChainEnd(vehicle2));
  if (has_partial_route1 == has_partial_route2) {
    return vehicle2 < vehicle1;
  } else {
    return has_partial_route2 < has_partial_route1;
  }
}

// EvaluatorCheapestAdditionFilteredHeuristic

EvaluatorCheapestAdditionFilteredHeuristic::
    EvaluatorCheapestAdditionFilteredHeuristic(
        RoutingModel* model, std::function<int64(int64, int64)> evaluator,
        LocalSearchFilterManager* filter_manager)
    : CheapestAdditionFilteredHeuristic(model, filter_manager),
      evaluator_(std::move(evaluator)) {}

int64 EvaluatorCheapestAdditionFilteredHeuristic::FindTopSuccessor(
    int64 node, const std::vector<int64>& successors) {
  int64 best_evaluation = kint64max;
  int64 best_successor = -1;
  for (int64 successor : successors) {
    const int64 evaluation =
        (successor >= 0) ? evaluator_(node, successor) : kint64max;
    if (evaluation < best_evaluation ||
        (evaluation == best_evaluation && successor > best_successor)) {
      best_evaluation = evaluation;
      best_successor = successor;
    }
  }
  return best_successor;
}

void EvaluatorCheapestAdditionFilteredHeuristic::SortSuccessors(
    int64 node, std::vector<int64>* successors) {
  std::vector<std::pair<int64, int64>> values;
  values.reserve(successors->size());
  for (int64 successor : *successors) {
    // Tie-breaking on largest node index to mimic the behavior of
    // CheapestValueSelector (search.cc).
    values.push_back({evaluator_(node, successor), -successor});
  }
  std::sort(values.begin(), values.end());
  successors->clear();
  for (auto value : values) {
    successors->push_back(-value.second);
  }
}

// ComparatorCheapestAdditionFilteredHeuristic

ComparatorCheapestAdditionFilteredHeuristic::
    ComparatorCheapestAdditionFilteredHeuristic(
        RoutingModel* model, Solver::VariableValueComparator comparator,
        LocalSearchFilterManager* filter_manager)
    : CheapestAdditionFilteredHeuristic(model, filter_manager),
      comparator_(std::move(comparator)) {}

int64 ComparatorCheapestAdditionFilteredHeuristic::FindTopSuccessor(
    int64 node, const std::vector<int64>& successors) {
  return *std::min_element(successors.begin(), successors.end(),
                           [this, node](int successor1, int successor2) {
                             return comparator_(node, successor1, successor2);
                           });
}

void ComparatorCheapestAdditionFilteredHeuristic::SortSuccessors(
    int64 node, std::vector<int64>* successors) {
  std::sort(successors->begin(), successors->end(),
            [this, node](int successor1, int successor2) {
              return comparator_(node, successor1, successor2);
            });
}

// Class storing and allowing access to the savings according to the number of
// vehicle types.
// The savings are stored and sorted in sorted_savings_per_vehicle_type_.
// Furthermore, when there is more than one vehicle type, the savings for a same
// before-->after arc are sorted in costs_and_savings_per_arc_[arc] by
// increasing cost(s-->before-->after-->e), where s and e are the start and end
// of the route, in order to make sure the arc is served by the route with the
// closest depot (start/end) possible.
// When there is only one vehicle "type" (i.e. all vehicles have the same
// start/end and cost class), each arc has a single saving value associated to
// it, so we ignore this last step to avoid unnecessary computations, and only
// work with sorted_savings_per_vehicle_type_[0].
// In case of multiple vehicle types, the best savings for each arc, i.e. the
// savings corresponding to the closest vehicle type, are inserted and sorted in
// sorted_savings_.
//
// This class also handles skipped Savings:
// The vectors skipped_savings_starting/ending_at_ contain all the Savings that
// weren't added to the model, which we want to consider for later:
// 1) When a Saving before-->after with both nodes uncontained cannot be used to
//    start a new route (no more available vehicles or could not commit on any
//    of those available).
// 2) When only one of the nodes of the Saving is contained but on a different
//    vehicle type.
// In these cases, the Update() method is called with update_best_saving = true,
// which in turn calls SkipSavingForArc() (within
// UpdateNextAndSkippedSavingsForArcWithType()) to mark the Saving for this arc
// (with the correct type in the second case) as "skipped", by storing it in
// skipped_savings_starting_at_[before] and skipped_savings_ending_at_[after].
//
// UpdateNextAndSkippedSavingsForArcWithType() also updates the next_savings_
// vector, which stores the savings to go through once we've iterated through
// all sorted_savings_.
// In the first case above, where neither nodes are contained, we skip the
// current Saving (current_saving_), and add the next best Saving for this arc
// to next_savings_ (in case this skipped Saving is never considered).
// In the second case with a specific type, we search for the Saving with the
// correct type for this arc, and add it to both next_savings_ and the skipped
// Savings.
//
// The skipped Savings are then re-considered when one of their ends gets
// inserted:
// When another Saving other_node-->before (or after-->other_node) gets
// inserted, all skipped Savings in skipped_savings_starting_at_[before] (or
// skipped_savings_ending_at_[after]) are once again considered by calling
// ReinjectSkippedSavingsStartingAt() (or ReinjectSkippedSavingsEndingAt()).
// Then, when calling GetSaving(), we iterate through the reinjected Savings in
// order of insertion in the vectors while there are reinjected savings.
template <typename Saving>
class SavingsFilteredHeuristic::SavingsContainer {
 public:
  explicit SavingsContainer(const SavingsFilteredHeuristic* savings_db,
                            int vehicle_types)
      : savings_db_(savings_db),
        vehicle_types_(vehicle_types),
        index_in_sorted_savings_(0),
        single_vehicle_type_(vehicle_types == 1),
        using_incoming_reinjected_saving_(false),
        sorted_(false),
        to_update_(true) {}

  void InitializeContainer(int64 size, int64 saving_neighbors) {
    sorted_savings_per_vehicle_type_.clear();
    sorted_savings_per_vehicle_type_.resize(vehicle_types_);
    for (std::vector<Saving>& savings : sorted_savings_per_vehicle_type_) {
      savings.reserve(size * saving_neighbors);
    }

    sorted_savings_.clear();
    costs_and_savings_per_arc_.clear();
    arc_indices_per_before_node_.clear();

    if (!single_vehicle_type_) {
      costs_and_savings_per_arc_.reserve(size * saving_neighbors);
      arc_indices_per_before_node_.resize(size);
      for (int before_node = 0; before_node < size; before_node++) {
        arc_indices_per_before_node_[before_node].reserve(saving_neighbors);
      }
    }
    skipped_savings_starting_at_.clear();
    skipped_savings_starting_at_.resize(size);
    skipped_savings_ending_at_.clear();
    skipped_savings_ending_at_.resize(size);
    incoming_reinjected_savings_ = nullptr;
    outgoing_reinjected_savings_ = nullptr;
    incoming_new_reinjected_savings_ = nullptr;
    outgoing_new_reinjected_savings_ = nullptr;
  }

  void AddNewSaving(const Saving& saving, int64 total_cost, int64 before_node,
                    int64 after_node, int vehicle_type) {
    CHECK(!sorted_savings_per_vehicle_type_.empty())
        << "Container not initialized!";
    sorted_savings_per_vehicle_type_[vehicle_type].push_back(saving);
    UpdateArcIndicesCostsAndSavings(before_node, after_node,
                                    {total_cost, saving});
  }

  void Sort() {
    CHECK(!sorted_) << "Container already sorted!";

    for (std::vector<Saving>& savings : sorted_savings_per_vehicle_type_) {
      std::sort(savings.begin(), savings.end());
    }

    if (single_vehicle_type_) {
      const auto& savings = sorted_savings_per_vehicle_type_[0];
      sorted_savings_.resize(savings.size());
      std::transform(savings.begin(), savings.end(), sorted_savings_.begin(),
                     [](const Saving& saving) {
                       return SavingAndArc({saving, /*arc_index*/ -1});
                     });
    } else {
      // For each arc, sort the savings by decreasing total cost
      // start-->a-->b-->end.
      // The best saving for each arc is therefore the last of its vector.
      sorted_savings_.reserve(vehicle_types_ *
                              costs_and_savings_per_arc_.size());

      for (int arc_index = 0; arc_index < costs_and_savings_per_arc_.size();
           arc_index++) {
        std::vector<std::pair<int64, Saving>>& costs_and_savings =
            costs_and_savings_per_arc_[arc_index];
        DCHECK(!costs_and_savings.empty());

        std::sort(
            costs_and_savings.begin(), costs_and_savings.end(),
            [](const std::pair<int64, Saving>& cs1,
               const std::pair<int64, Saving>& cs2) { return cs1 > cs2; });

        // Insert all Savings for this arc with the lowest cost into
        // sorted_savings_.
        // TODO(user): Also do this when reiterating on next_savings_.
        const int64 cost = costs_and_savings.back().first;
        while (!costs_and_savings.empty() &&
               costs_and_savings.back().first == cost) {
          sorted_savings_.push_back(
              {costs_and_savings.back().second, arc_index});
          costs_and_savings.pop_back();
        }
      }
      std::sort(sorted_savings_.begin(), sorted_savings_.end());
      next_saving_type_and_index_for_arc_.clear();
      next_saving_type_and_index_for_arc_.resize(
          costs_and_savings_per_arc_.size(), {-1, -1});
    }
    sorted_ = true;
    index_in_sorted_savings_ = 0;
    to_update_ = false;
  }

  bool HasSaving() {
    return index_in_sorted_savings_ < sorted_savings_.size() ||
           HasReinjectedSavings();
  }

  Saving GetSaving() {
    CHECK(sorted_) << "Calling GetSaving() before Sort() !";
    CHECK(!to_update_)
        << "Update() should be called between two calls to GetSaving() !";

    to_update_ = true;

    if (HasReinjectedSavings()) {
      if (incoming_reinjected_savings_ != nullptr &&
          outgoing_reinjected_savings_ != nullptr) {
        // Get the best Saving among the two.
        SavingAndArc& incoming_saving = incoming_reinjected_savings_->front();
        SavingAndArc& outgoing_saving = outgoing_reinjected_savings_->front();
        if (incoming_saving < outgoing_saving) {
          current_saving_ = incoming_saving;
          using_incoming_reinjected_saving_ = true;
        } else {
          current_saving_ = outgoing_saving;
          using_incoming_reinjected_saving_ = false;
        }
      } else {
        if (incoming_reinjected_savings_ != nullptr) {
          current_saving_ = incoming_reinjected_savings_->front();
          using_incoming_reinjected_saving_ = true;
        }
        if (outgoing_reinjected_savings_ != nullptr) {
          current_saving_ = outgoing_reinjected_savings_->front();
          using_incoming_reinjected_saving_ = false;
        }
      }
    } else {
      current_saving_ = sorted_savings_[index_in_sorted_savings_];
    }
    return current_saving_.saving;
  }

  void Update(bool update_best_saving, int type = -1) {
    CHECK(to_update_) << "Container already up to date!";
    if (update_best_saving) {
      const int64 arc_index = current_saving_.arc_index;
      UpdateNextAndSkippedSavingsForArcWithType(arc_index, type);
    }
    if (!HasReinjectedSavings()) {
      index_in_sorted_savings_++;

      if (index_in_sorted_savings_ == sorted_savings_.size()) {
        sorted_savings_.swap(next_savings_);
        gtl::STLClearObject(&next_savings_);
        index_in_sorted_savings_ = 0;

        std::sort(sorted_savings_.begin(), sorted_savings_.end());
        next_saving_type_and_index_for_arc_.clear();
        next_saving_type_and_index_for_arc_.resize(
            costs_and_savings_per_arc_.size(), {-1, -1});
      }
    }
    UpdateReinjectedSavings();
    to_update_ = false;
  }

  void UpdateWithType(int type) {
    CHECK(!single_vehicle_type_);
    Update(/*update_best_saving*/ true, type);
  }

  const std::vector<Saving>& GetSortedSavingsForVehicleType(int type) {
    CHECK(sorted_) << "Savings not sorted yet!";
    CHECK_LT(type, vehicle_types_);
    return sorted_savings_per_vehicle_type_[type];
  }

  void ReinjectSkippedSavingsStartingAt(int64 node) {
    CHECK(outgoing_new_reinjected_savings_ == nullptr);
    outgoing_new_reinjected_savings_ = &(skipped_savings_starting_at_[node]);
  }

  void ReinjectSkippedSavingsEndingAt(int64 node) {
    CHECK(incoming_new_reinjected_savings_ == nullptr);
    incoming_new_reinjected_savings_ = &(skipped_savings_ending_at_[node]);
  }

 private:
  struct SavingAndArc {
    Saving saving;
    int64 arc_index;

    bool operator<(const SavingAndArc& other) const {
      return std::tie(saving, arc_index) <
             std::tie(other.saving, other.arc_index);
    }
  };

  // Skips the Saving for the arc before_node-->after_node, by adding it to the
  // skipped_savings_ vector of the nodes, if they're uncontained.
  void SkipSavingForArc(const SavingAndArc& saving_and_arc) {
    const Saving& saving = saving_and_arc.saving;
    const int64 before_node = savings_db_->GetBeforeNodeFromSaving(saving);
    const int64 after_node = savings_db_->GetAfterNodeFromSaving(saving);
    if (!savings_db_->Contains(before_node)) {
      skipped_savings_starting_at_[before_node].push_back(saving_and_arc);
    }
    if (!savings_db_->Contains(after_node)) {
      skipped_savings_ending_at_[after_node].push_back(saving_and_arc);
    }
  }

  // Called within Update() when update_best_saving is true, this method updates
  // the next_savings_ and skipped savings vectors for a given arc_index and
  // vehicle type.
  // When a Saving with the right type has already been added to next_savings_
  // for this arc, no action is needed on next_savings_.
  // Otherwise, if such a Saving exists, GetNextSavingForArcWithType() will find
  // and assign it to next_saving, which is then used to update next_savings_.
  // Finally, the right Saving is skipped for this arc: if looking for a
  // specific type (i.e. type != -1), next_saving (which has the correct type)
  // is skipped, otherwise the current_saving_ is.
  void UpdateNextAndSkippedSavingsForArcWithType(int64 arc_index, int type) {
    if (single_vehicle_type_) {
      // No next Saving, skip the current Saving.
      CHECK_EQ(type, -1);
      SkipSavingForArc(current_saving_);
      return;
    }
    CHECK_GE(arc_index, 0);
    auto& type_and_index = next_saving_type_and_index_for_arc_[arc_index];
    const int previous_index = type_and_index.second;
    const int previous_type = type_and_index.first;
    bool next_saving_added = false;
    Saving next_saving;

    if (previous_index >= 0) {
      // Next Saving already added for this arc.
      DCHECK_GE(previous_type, 0);
      if (type == -1 || previous_type == type) {
        // Not looking for a specific type, or correct type already in
        // next_savings_.
        next_saving_added = true;
        next_saving = next_savings_[previous_index].saving;
      }
    }

    if (!next_saving_added &&
        GetNextSavingForArcWithType(arc_index, type, &next_saving)) {
      type_and_index.first = savings_db_->GetVehicleTypeFromSaving(next_saving);
      if (previous_index >= 0) {
        // Update the previous saving.
        next_savings_[previous_index] = {next_saving, arc_index};
      } else {
        // Insert the new next Saving for this arc.
        type_and_index.second = next_savings_.size();
        next_savings_.push_back({next_saving, arc_index});
      }
      next_saving_added = true;
    }

    // Skip the Saving based on the vehicle type.
    if (type == -1) {
      // Skip the current Saving.
      SkipSavingForArc(current_saving_);
    } else {
      // Skip the Saving with the correct type, already added to next_savings_
      // if it was found.
      if (next_saving_added) {
        SkipSavingForArc({next_saving, arc_index});
      }
    }
  }

  void UpdateReinjectedSavings() {
    UpdateGivenReinjectedSavings(incoming_new_reinjected_savings_,
                                 &incoming_reinjected_savings_,
                                 using_incoming_reinjected_saving_);
    UpdateGivenReinjectedSavings(outgoing_new_reinjected_savings_,
                                 &outgoing_reinjected_savings_,
                                 !using_incoming_reinjected_saving_);
    incoming_new_reinjected_savings_ = nullptr;
    outgoing_new_reinjected_savings_ = nullptr;
  }

  void UpdateGivenReinjectedSavings(
      std::deque<SavingAndArc>* new_reinjected_savings,
      std::deque<SavingAndArc>** reinjected_savings,
      bool using_reinjected_savings) {
    if (new_reinjected_savings == nullptr) {
      // No new reinjected savings, update the previous ones if needed.
      if (*reinjected_savings != nullptr && using_reinjected_savings) {
        CHECK(!(*reinjected_savings)->empty());
        (*reinjected_savings)->pop_front();
        if ((*reinjected_savings)->empty()) {
          *reinjected_savings = nullptr;
        }
      }
      return;
    }

    // New savings reinjected.
    // Forget about the previous reinjected savings and add the new ones if
    // there are any.
    if (*reinjected_savings != nullptr) {
      (*reinjected_savings)->clear();
    }
    *reinjected_savings = nullptr;
    if (!new_reinjected_savings->empty()) {
      *reinjected_savings = new_reinjected_savings;
    }
  }

  bool HasReinjectedSavings() {
    return outgoing_reinjected_savings_ != nullptr ||
           incoming_reinjected_savings_ != nullptr;
  }

  void UpdateArcIndicesCostsAndSavings(
      int64 before_node, int64 after_node,
      const std::pair<int64, Saving>& cost_and_saving) {
    if (single_vehicle_type_) {
      return;
    }
    absl::flat_hash_map<int, int>& arc_indices =
        arc_indices_per_before_node_[before_node];
    const auto& arc_inserted = arc_indices.insert(
        std::make_pair(after_node, costs_and_savings_per_arc_.size()));
    const int index = arc_inserted.first->second;
    if (arc_inserted.second) {
      costs_and_savings_per_arc_.push_back({cost_and_saving});
    } else {
      DCHECK_LT(index, costs_and_savings_per_arc_.size());
      costs_and_savings_per_arc_[index].push_back(cost_and_saving);
    }
  }

  bool GetNextSavingForArcWithType(int64 arc_index, int type,
                                   Saving* next_saving) {
    std::vector<std::pair<int64, Saving>>& costs_and_savings =
        costs_and_savings_per_arc_[arc_index];

    bool found_saving = false;
    while (!costs_and_savings.empty() && !found_saving) {
      const Saving& saving = costs_and_savings.back().second;
      if (type == -1 || savings_db_->GetVehicleTypeFromSaving(saving) == type) {
        *next_saving = saving;
        found_saving = true;
      }
      costs_and_savings.pop_back();
    }
    return found_saving;
  }

  const SavingsFilteredHeuristic* const savings_db_;
  const int vehicle_types_;
  int64 index_in_sorted_savings_;
  std::vector<std::vector<Saving>> sorted_savings_per_vehicle_type_;
  std::vector<SavingAndArc> sorted_savings_;
  std::vector<SavingAndArc> next_savings_;
  std::vector<std::pair</*type*/ int, /*index*/ int>>
      next_saving_type_and_index_for_arc_;
  SavingAndArc current_saving_;
  const bool single_vehicle_type_;
  std::vector<std::vector<std::pair</*cost*/ int64, Saving>>>
      costs_and_savings_per_arc_;
  std::vector<absl::flat_hash_map</*after_node*/ int, /*arc_index*/ int>>
      arc_indices_per_before_node_;
  std::vector<std::deque<SavingAndArc>> skipped_savings_starting_at_;
  std::vector<std::deque<SavingAndArc>> skipped_savings_ending_at_;
  std::deque<SavingAndArc>* outgoing_reinjected_savings_;
  std::deque<SavingAndArc>* incoming_reinjected_savings_;
  bool using_incoming_reinjected_saving_;
  std::deque<SavingAndArc>* outgoing_new_reinjected_savings_;
  std::deque<SavingAndArc>* incoming_new_reinjected_savings_;
  bool sorted_;
  bool to_update_;
};

// SavingsFilteredHeuristic

SavingsFilteredHeuristic::SavingsFilteredHeuristic(
    RoutingModel* model, const RoutingIndexManager* manager,
    SavingsParameters parameters, LocalSearchFilterManager* filter_manager)
    : RoutingFilteredHeuristic(model, filter_manager),
      vehicle_type_curator_(nullptr),
      manager_(manager),
      savings_params_(parameters) {
  DCHECK_GT(savings_params_.neighbors_ratio, 0);
  DCHECK_LE(savings_params_.neighbors_ratio, 1);
  DCHECK_GT(savings_params_.max_memory_usage_bytes, 0);
  DCHECK_GT(savings_params_.arc_coefficient, 0);
  const int size = model->Size();
  size_squared_ = size * size;
}

SavingsFilteredHeuristic::~SavingsFilteredHeuristic() {}

bool SavingsFilteredHeuristic::BuildSolutionInternal() {
  if (vehicle_type_curator_ == nullptr) {
    vehicle_type_curator_ = absl::make_unique<VehicleTypeCurator>(
        model()->GetVehicleTypeContainer());
  }
  vehicle_type_curator_->Reset();
  ComputeSavings();
  BuildRoutesFromSavings();
  // Free all the space used to store the Savings in the container.
  savings_container_.reset();
  MakeUnassignedNodesUnperformed();
  return Commit();
}

int SavingsFilteredHeuristic::StartNewRouteWithBestVehicleOfType(
    int type, int64 before_node, int64 after_node) {
  auto vehicle_is_compatible = [this, before_node, after_node](int vehicle) {
    if (!model()->VehicleVar(before_node)->Contains(vehicle) ||
        !model()->VehicleVar(after_node)->Contains(vehicle)) {
      return false;
    }
    // Try to commit the arc on this vehicle.
    const int64 start = model()->Start(vehicle);
    const int64 end = model()->End(vehicle);
    SetValue(start, before_node);
    SetValue(before_node, after_node);
    SetValue(after_node, end);
    return Commit();
  };

  const int vehicle = vehicle_type_curator_->GetCompatibleVehicleOfType(
      type, vehicle_is_compatible);
  return vehicle;
}

void SavingsFilteredHeuristic::AddSymmetricArcsToAdjacencyLists(
    std::vector<std::vector<int64>>* adjacency_lists) {
  for (int64 node = 0; node < adjacency_lists->size(); node++) {
    for (int64 neighbor : (*adjacency_lists)[node]) {
      if (model()->IsStart(neighbor) || model()->IsEnd(neighbor)) {
        continue;
      }
      (*adjacency_lists)[neighbor].push_back(node);
    }
  }
  std::transform(adjacency_lists->begin(), adjacency_lists->end(),
                 adjacency_lists->begin(), [](std::vector<int64> vec) {
                   std::sort(vec.begin(), vec.end());
                   vec.erase(std::unique(vec.begin(), vec.end()), vec.end());
                   return vec;
                 });
}

// Computes the savings related to each pair of non-start and non-end nodes.
// The savings value for an arc a-->b for a vehicle starting at node s and
// ending at node e is:
// saving = cost(s-->a-->e) + cost(s-->b-->e) - cost(s-->a-->b-->e), i.e.
// saving = cost(a-->e) + cost(s-->b) - cost(a-->b)
// The saving value also considers a coefficient for the cost of the arc
// a-->b, which results in:
// saving = cost(a-->e) + cost(s-->b) - [arc_coefficient_ * cost(a-->b)]
// The higher this saving value, the better the arc.
// Here, the value stored for the savings is -saving, which are therefore
// considered in decreasing order.
void SavingsFilteredHeuristic::ComputeSavings() {
  const int num_vehicle_types = vehicle_type_curator_->NumTypes();
  const int size = model()->Size();

  std::vector<int64> uncontained_non_start_end_nodes;
  uncontained_non_start_end_nodes.reserve(size);
  for (int node = 0; node < size; node++) {
    if (!model()->IsStart(node) && !model()->IsEnd(node) && !Contains(node)) {
      uncontained_non_start_end_nodes.push_back(node);
    }
  }

  const int64 saving_neighbors =
      std::min(MaxNumNeighborsPerNode(num_vehicle_types),
               static_cast<int64>(uncontained_non_start_end_nodes.size()));

  savings_container_ =
      absl::make_unique<SavingsContainer<Saving>>(this, num_vehicle_types);
  savings_container_->InitializeContainer(size, saving_neighbors);

  std::vector<std::vector<int64>> adjacency_lists(size);

  for (int type = 0; type < num_vehicle_types; ++type) {
    const int vehicle = vehicle_type_curator_->GetVehicleOfType(type);
    if (vehicle < 0) {
      continue;
    }

    const int64 cost_class =
        model()->GetCostClassIndexOfVehicle(vehicle).value();
    const int64 start = model()->Start(vehicle);
    const int64 end = model()->End(vehicle);
    const int64 fixed_cost = model()->GetFixedCostOfVehicle(vehicle);

    // Compute the neighbors for each non-start/end node not already inserted in
    // the model.
    for (int before_node : uncontained_non_start_end_nodes) {
      std::vector<std::pair</*cost*/ int64, /*node*/ int64>> costed_after_nodes;
      costed_after_nodes.reserve(uncontained_non_start_end_nodes.size());
      for (int after_node : uncontained_non_start_end_nodes) {
        if (after_node != before_node) {
          costed_after_nodes.push_back(std::make_pair(
              model()->GetArcCostForClass(before_node, after_node, cost_class),
              after_node));
        }
      }
      if (saving_neighbors < costed_after_nodes.size()) {
        std::nth_element(costed_after_nodes.begin(),
                         costed_after_nodes.begin() + saving_neighbors,
                         costed_after_nodes.end());
        costed_after_nodes.resize(saving_neighbors);
      }
      adjacency_lists[before_node].resize(costed_after_nodes.size());
      std::transform(costed_after_nodes.begin(), costed_after_nodes.end(),
                     adjacency_lists[before_node].begin(),
                     [](std::pair<int64, int64> cost_and_node) {
                       return cost_and_node.second;
                     });
    }
    if (savings_params_.add_reverse_arcs) {
      AddSymmetricArcsToAdjacencyLists(&adjacency_lists);
    }

    // Build the savings for this vehicle type given the adjacency_lists.
    for (int before_node : uncontained_non_start_end_nodes) {
      const int64 before_to_end_cost =
          model()->GetArcCostForClass(before_node, end, cost_class);
      const int64 start_to_before_cost =
          CapSub(model()->GetArcCostForClass(start, before_node, cost_class),
                 fixed_cost);
      for (int64 after_node : adjacency_lists[before_node]) {
        if (model()->IsStart(after_node) || model()->IsEnd(after_node) ||
            before_node == after_node || Contains(after_node)) {
          continue;
        }
        const int64 arc_cost =
            model()->GetArcCostForClass(before_node, after_node, cost_class);
        const int64 start_to_after_cost =
            CapSub(model()->GetArcCostForClass(start, after_node, cost_class),
                   fixed_cost);
        const int64 after_to_end_cost =
            model()->GetArcCostForClass(after_node, end, cost_class);

        const double weighted_arc_cost_fp =
            savings_params_.arc_coefficient * arc_cost;
        const int64 weighted_arc_cost =
            weighted_arc_cost_fp < kint64max
                ? static_cast<int64>(weighted_arc_cost_fp)
                : kint64max;
        const int64 saving_value = CapSub(
            CapAdd(before_to_end_cost, start_to_after_cost), weighted_arc_cost);

        const Saving saving =
            BuildSaving(-saving_value, type, before_node, after_node);

        const int64 total_cost =
            CapAdd(CapAdd(start_to_before_cost, arc_cost), after_to_end_cost);

        savings_container_->AddNewSaving(saving, total_cost, before_node,
                                         after_node, type);
      }
    }
  }
  savings_container_->Sort();
}

int64 SavingsFilteredHeuristic::MaxNumNeighborsPerNode(
    int num_vehicle_types) const {
  const int64 size = model()->Size();

  const int64 num_neighbors_with_ratio =
      std::max(1.0, size * savings_params_.neighbors_ratio);

  // A single Saving takes 2*8 bytes of memory.
  // max_memory_usage_in_savings_unit = num_savings * multiplicative_factor,
  // Where multiplicative_factor is the memory taken (in Savings unit) for each
  // computed Saving.
  const double max_memory_usage_in_savings_unit =
      savings_params_.max_memory_usage_bytes / 16;

  // In the SavingsContainer, for each Saving, the Savings are stored:
  // - Once in "sorted_savings_per_vehicle_type", and (at most) once in
  //   "sorted_savings_" --> factor 2
  // - If num_vehicle_types > 1, they're also stored by arc_index in
  //   "costs_and_savings_per_arc", along with their int64 cost --> factor 1.5
  //
  // On top of that,
  // - In the sequential version, the Saving* are also stored by in-coming and
  //   outgoing node (in in/out_savings_ptr), adding another 2*8 bytes per
  //   Saving --> factor 1.
  // - In the parallel version, skipped Savings are also stored in
  //   skipped_savings_starting/ending_at_, resulting in a maximum added factor
  //   of 2 for each Saving.
  // These extra factors are given by ExtraSavingsMemoryMultiplicativeFactor().
  double multiplicative_factor = 2.0 + ExtraSavingsMemoryMultiplicativeFactor();
  if (num_vehicle_types > 1) {
    multiplicative_factor += 1.5;
  }
  const double num_savings =
      max_memory_usage_in_savings_unit / multiplicative_factor;
  const int64 num_neighbors_with_memory_restriction =
      std::max(1.0, num_savings / (num_vehicle_types * size));

  return std::min(num_neighbors_with_ratio,
                  num_neighbors_with_memory_restriction);
}

// SequentialSavingsFilteredHeuristic

void SequentialSavingsFilteredHeuristic::BuildRoutesFromSavings() {
  const int vehicle_types = vehicle_type_curator_->NumTypes();
  DCHECK_GT(vehicle_types, 0);
  const int size = model()->Size();
  // Store savings for each incoming and outgoing node and by vehicle type. This
  // is necessary to quickly extend partial chains without scanning all savings.
  std::vector<std::vector<const Saving*>> in_savings_ptr(size * vehicle_types);
  std::vector<std::vector<const Saving*>> out_savings_ptr(size * vehicle_types);
  for (int type = 0; type < vehicle_types; type++) {
    const int vehicle_type_offset = type * size;
    const std::vector<Saving>& sorted_savings_for_type =
        savings_container_->GetSortedSavingsForVehicleType(type);
    for (const Saving& saving : sorted_savings_for_type) {
      DCHECK_EQ(GetVehicleTypeFromSaving(saving), type);
      const int before_node = GetBeforeNodeFromSaving(saving);
      in_savings_ptr[vehicle_type_offset + before_node].push_back(&saving);
      const int after_node = GetAfterNodeFromSaving(saving);
      out_savings_ptr[vehicle_type_offset + after_node].push_back(&saving);
    }
  }

  // Build routes from savings.
  while (savings_container_->HasSaving()) {
    // First find the best saving to start a new route.
    const Saving saving = savings_container_->GetSaving();
    int before_node = GetBeforeNodeFromSaving(saving);
    int after_node = GetAfterNodeFromSaving(saving);
    const bool nodes_not_contained =
        !Contains(before_node) && !Contains(after_node);

    bool committed = false;

    if (nodes_not_contained) {
      // Find the right vehicle to start the route with this Saving.
      const int type = GetVehicleTypeFromSaving(saving);
      const int vehicle =
          StartNewRouteWithBestVehicleOfType(type, before_node, after_node);

      if (vehicle >= 0) {
        committed = true;
        const int64 start = model()->Start(vehicle);
        const int64 end = model()->End(vehicle);
        // Then extend the route from both ends of the partial route.
        int in_index = 0;
        int out_index = 0;
        const int saving_offset = type * size;

        while (in_index < in_savings_ptr[saving_offset + after_node].size() ||
               out_index <
                   out_savings_ptr[saving_offset + before_node].size()) {
          if (StopSearch()) return;
          // First determine how to extend the route.
          int before_before_node = -1;
          int after_after_node = -1;
          if (in_index < in_savings_ptr[saving_offset + after_node].size()) {
            const Saving& in_saving =
                *(in_savings_ptr[saving_offset + after_node][in_index]);
            if (out_index <
                out_savings_ptr[saving_offset + before_node].size()) {
              const Saving& out_saving =
                  *(out_savings_ptr[saving_offset + before_node][out_index]);
              if (GetSavingValue(in_saving) < GetSavingValue(out_saving)) {
                // Should extend after after_node
                after_after_node = GetAfterNodeFromSaving(in_saving);
              } else {
                // Should extend before before_node
                before_before_node = GetBeforeNodeFromSaving(out_saving);
              }
            } else {
              // Should extend after after_node
              after_after_node = GetAfterNodeFromSaving(in_saving);
            }
          } else {
            // Should extend before before_node
            before_before_node = GetBeforeNodeFromSaving(
                *(out_savings_ptr[saving_offset + before_node][out_index]));
          }
          // Extend the route
          if (after_after_node != -1) {
            DCHECK_EQ(before_before_node, -1);
            // Extending after after_node
            if (!Contains(after_after_node)) {
              SetValue(after_node, after_after_node);
              SetValue(after_after_node, end);
              if (Commit()) {
                in_index = 0;
                after_node = after_after_node;
              } else {
                ++in_index;
              }
            } else {
              ++in_index;
            }
          } else {
            // Extending before before_node
            CHECK_GE(before_before_node, 0);
            if (!Contains(before_before_node)) {
              SetValue(start, before_before_node);
              SetValue(before_before_node, before_node);
              if (Commit()) {
                out_index = 0;
                before_node = before_before_node;
              } else {
                ++out_index;
              }
            } else {
              ++out_index;
            }
          }
        }
      }
    }
    savings_container_->Update(nodes_not_contained && !committed);
  }
}

// ParallelSavingsFilteredHeuristic

void ParallelSavingsFilteredHeuristic::BuildRoutesFromSavings() {
  // Initialize the vehicles of the first/last non start/end nodes served by
  // each route.
  const int64 size = model()->Size();
  const int vehicles = model()->vehicles();

  first_node_on_route_.resize(vehicles, -1);
  last_node_on_route_.resize(vehicles, -1);
  vehicle_of_first_or_last_node_.resize(size, -1);

  for (int vehicle = 0; vehicle < vehicles; vehicle++) {
    const int64 start = model()->Start(vehicle);
    const int64 end = model()->End(vehicle);
    if (!Contains(start)) {
      continue;
    }
    int64 node = Value(start);
    if (node != end) {
      vehicle_of_first_or_last_node_[node] = vehicle;
      first_node_on_route_[vehicle] = node;

      int64 next = Value(node);
      while (next != end) {
        node = next;
        next = Value(node);
      }
      vehicle_of_first_or_last_node_[node] = vehicle;
      last_node_on_route_[vehicle] = node;
    }
  }

  while (savings_container_->HasSaving()) {
    if (StopSearch()) return;
    const Saving saving = savings_container_->GetSaving();
    const int64 before_node = GetBeforeNodeFromSaving(saving);
    const int64 after_node = GetAfterNodeFromSaving(saving);
    const int type = GetVehicleTypeFromSaving(saving);

    if (!Contains(before_node) && !Contains(after_node)) {
      // Neither nodes are contained, start a new route.
      bool committed = false;

      const int vehicle =
          StartNewRouteWithBestVehicleOfType(type, before_node, after_node);

      if (vehicle >= 0) {
        committed = true;
        // Store before_node and after_node as first and last nodes of the route
        vehicle_of_first_or_last_node_[before_node] = vehicle;
        vehicle_of_first_or_last_node_[after_node] = vehicle;
        first_node_on_route_[vehicle] = before_node;
        last_node_on_route_[vehicle] = after_node;
        savings_container_->ReinjectSkippedSavingsStartingAt(after_node);
        savings_container_->ReinjectSkippedSavingsEndingAt(before_node);
      }
      savings_container_->Update(!committed);
      continue;
    }

    if (Contains(before_node) && Contains(after_node)) {
      // Merge the two routes if before_node is last and after_node first of its
      // route, the two nodes aren't already on the same route, and the vehicle
      // types are compatible.
      const int v1 = vehicle_of_first_or_last_node_[before_node];
      const int64 last_node = v1 == -1 ? -1 : last_node_on_route_[v1];

      const int v2 = vehicle_of_first_or_last_node_[after_node];
      const int64 first_node = v2 == -1 ? -1 : first_node_on_route_[v2];

      if (before_node == last_node && after_node == first_node && v1 != v2 &&
          vehicle_type_curator_->Type(v1) == vehicle_type_curator_->Type(v2)) {
        CHECK_EQ(Value(before_node), model()->End(v1));
        CHECK_EQ(Value(model()->Start(v2)), after_node);

        // We try merging the two routes.
        // TODO(user): Try to use skipped savings to start new routes when
        // a vehicle becomes available after a merge (not trivial because it can
        // result in an infinite loop).
        MergeRoutes(v1, v2, before_node, after_node);
      }
    }

    if (Contains(before_node) && !Contains(after_node)) {
      const int vehicle = vehicle_of_first_or_last_node_[before_node];
      const int64 last_node = vehicle == -1 ? -1 : last_node_on_route_[vehicle];

      if (before_node == last_node) {
        const int64 end = model()->End(vehicle);
        CHECK_EQ(Value(before_node), end);

        const int route_type = vehicle_type_curator_->Type(vehicle);
        if (type != route_type) {
          // The saving doesn't correspond to the type of the vehicle serving
          // before_node. We update the container with the correct type.
          savings_container_->UpdateWithType(route_type);
          continue;
        }

        // Try adding after_node on route of before_node.
        SetValue(before_node, after_node);
        SetValue(after_node, end);
        if (Commit()) {
          if (first_node_on_route_[vehicle] != before_node) {
            // before_node is no longer the start or end of its route
            DCHECK_NE(Value(model()->Start(vehicle)), before_node);
            vehicle_of_first_or_last_node_[before_node] = -1;
          }
          vehicle_of_first_or_last_node_[after_node] = vehicle;
          last_node_on_route_[vehicle] = after_node;
          savings_container_->ReinjectSkippedSavingsStartingAt(after_node);
        }
      }
    }

    if (!Contains(before_node) && Contains(after_node)) {
      const int vehicle = vehicle_of_first_or_last_node_[after_node];
      const int64 first_node =
          vehicle == -1 ? -1 : first_node_on_route_[vehicle];

      if (after_node == first_node) {
        const int64 start = model()->Start(vehicle);
        CHECK_EQ(Value(start), after_node);

        const int route_type = vehicle_type_curator_->Type(vehicle);
        if (type != route_type) {
          // The saving doesn't correspond to the type of the vehicle serving
          // after_node. We update the container with the correct type.
          savings_container_->UpdateWithType(route_type);
          continue;
        }

        // Try adding before_node on route of after_node.
        SetValue(before_node, after_node);
        SetValue(start, before_node);
        if (Commit()) {
          if (last_node_on_route_[vehicle] != after_node) {
            // after_node is no longer the start or end of its route
            DCHECK_NE(Value(after_node), model()->End(vehicle));
            vehicle_of_first_or_last_node_[after_node] = -1;
          }
          vehicle_of_first_or_last_node_[before_node] = vehicle;
          first_node_on_route_[vehicle] = before_node;
          savings_container_->ReinjectSkippedSavingsEndingAt(before_node);
        }
      }
    }
    savings_container_->Update(/*update_best_saving*/ false);
  }
}

void ParallelSavingsFilteredHeuristic::MergeRoutes(int first_vehicle,
                                                   int second_vehicle,
                                                   int64 before_node,
                                                   int64 after_node) {
  if (StopSearch()) return;
  const int64 new_first_node = first_node_on_route_[first_vehicle];
  DCHECK_EQ(vehicle_of_first_or_last_node_[new_first_node], first_vehicle);
  CHECK_EQ(Value(model()->Start(first_vehicle)), new_first_node);
  const int64 new_last_node = last_node_on_route_[second_vehicle];
  DCHECK_EQ(vehicle_of_first_or_last_node_[new_last_node], second_vehicle);
  CHECK_EQ(Value(new_last_node), model()->End(second_vehicle));

  // Select the vehicle with lower fixed cost to merge the routes.
  int used_vehicle = first_vehicle;
  int unused_vehicle = second_vehicle;
  if (model()->GetFixedCostOfVehicle(first_vehicle) >
      model()->GetFixedCostOfVehicle(second_vehicle)) {
    used_vehicle = second_vehicle;
    unused_vehicle = first_vehicle;
  }

  SetValue(before_node, after_node);
  SetValue(model()->Start(unused_vehicle), model()->End(unused_vehicle));
  if (used_vehicle == first_vehicle) {
    SetValue(new_last_node, model()->End(used_vehicle));
  } else {
    SetValue(model()->Start(used_vehicle), new_first_node);
  }
  bool committed = Commit();
  if (!committed &&
      model()->GetVehicleClassIndexOfVehicle(first_vehicle).value() !=
          model()->GetVehicleClassIndexOfVehicle(second_vehicle).value()) {
    // Try committing on other vehicle instead.
    std::swap(used_vehicle, unused_vehicle);
    SetValue(before_node, after_node);
    SetValue(model()->Start(unused_vehicle), model()->End(unused_vehicle));
    if (used_vehicle == first_vehicle) {
      SetValue(new_last_node, model()->End(used_vehicle));
    } else {
      SetValue(model()->Start(used_vehicle), new_first_node);
    }
    committed = Commit();
  }
  if (committed) {
    // Make unused_vehicle available
    vehicle_type_curator_->ReinjectVehicleOfClass(
        unused_vehicle,
        model()->GetVehicleClassIndexOfVehicle(unused_vehicle).value(),
        model()->GetFixedCostOfVehicle(unused_vehicle));

    // Update the first and last nodes on vehicles.
    first_node_on_route_[unused_vehicle] = -1;
    last_node_on_route_[unused_vehicle] = -1;
    vehicle_of_first_or_last_node_[before_node] = -1;
    vehicle_of_first_or_last_node_[after_node] = -1;
    first_node_on_route_[used_vehicle] = new_first_node;
    last_node_on_route_[used_vehicle] = new_last_node;
    vehicle_of_first_or_last_node_[new_last_node] = used_vehicle;
    vehicle_of_first_or_last_node_[new_first_node] = used_vehicle;
  }
}

// ChristofidesFilteredHeuristic
ChristofidesFilteredHeuristic::ChristofidesFilteredHeuristic(
    RoutingModel* model, LocalSearchFilterManager* filter_manager,
    bool use_minimum_matching)
    : RoutingFilteredHeuristic(model, filter_manager),
      use_minimum_matching_(use_minimum_matching) {}

// TODO(user): Support pickup & delivery.
bool ChristofidesFilteredHeuristic::BuildSolutionInternal() {
  const int size = model()->Size() - model()->vehicles() + 1;
  // Node indices for Christofides solver.
  // 0: start/end node
  // >0: non start/end nodes
  // TODO(user): Add robustness to fixed arcs by collapsing them into meta-
  // nodes.
  std::vector<int> indices(1, 0);
  for (int i = 1; i < size; ++i) {
    if (!model()->IsStart(i) && !model()->IsEnd(i)) {
      indices.push_back(i);
    }
  }
  const int num_cost_classes = model()->GetCostClassesCount();
  std::vector<std::vector<int>> path_per_cost_class(num_cost_classes);
  std::vector<bool> class_covered(num_cost_classes, false);
  for (int vehicle = 0; vehicle < model()->vehicles(); ++vehicle) {
    const int64 cost_class =
        model()->GetCostClassIndexOfVehicle(vehicle).value();
    if (!class_covered[cost_class]) {
      class_covered[cost_class] = true;
      const int64 start = model()->Start(vehicle);
      const int64 end = model()->End(vehicle);
      auto cost = [this, &indices, start, end, cost_class](int from, int to) {
        DCHECK_LT(from, indices.size());
        DCHECK_LT(to, indices.size());
        const int from_index = (from == 0) ? start : indices[from];
        const int to_index = (to == 0) ? end : indices[to];
        const int64 cost =
            model()->GetArcCostForClass(from_index, to_index, cost_class);
        // To avoid overflow issues, capping costs at kint64max/2, the maximum
        // value supported by MinCostPerfectMatching.
        // TODO(user): Investigate if ChristofidesPathSolver should not
        // return a status to bail out fast in case of problem.
        return std::min(cost, kint64max / 2);
      };
      using Cost = decltype(cost);
      ChristofidesPathSolver<int64, int64, int, Cost> christofides_solver(
          indices.size(), cost);
      if (use_minimum_matching_) {
        christofides_solver.SetMatchingAlgorithm(
            ChristofidesPathSolver<int64, int64, int, Cost>::MatchingAlgorithm::
                MINIMUM_WEIGHT_MATCHING);
      }
      path_per_cost_class[cost_class] =
          christofides_solver.TravelingSalesmanPath();
    }
  }
  // TODO(user): Investigate if sorting paths per cost improves solutions.
  for (int vehicle = 0; vehicle < model()->vehicles(); ++vehicle) {
    const int64 cost_class =
        model()->GetCostClassIndexOfVehicle(vehicle).value();
    const std::vector<int>& path = path_per_cost_class[cost_class];
    DCHECK_EQ(0, path[0]);
    DCHECK_EQ(0, path.back());
    // Extend route from start.
    int prev = GetStartChainEnd(vehicle);
    const int end = model()->End(vehicle);
    for (int i = 1; i < path.size() - 1 && prev != end; ++i) {
      if (StopSearch()) return false;
      int next = indices[path[i]];
      if (!Contains(next)) {
        SetValue(prev, next);
        SetValue(next, end);
        if (Commit()) {
          prev = next;
        }
      }
    }
  }
  MakeUnassignedNodesUnperformed();
  return Commit();
}

namespace {
// The description is in routing.h:MakeGuidedSlackFinalizer
class GuidedSlackFinalizer : public DecisionBuilder {
 public:
  GuidedSlackFinalizer(const RoutingDimension* dimension, RoutingModel* model,
                       std::function<int64(int64)> initializer);
  Decision* Next(Solver* solver) override;

 private:
  int64 SelectValue(int64 index);
  int64 ChooseVariable();

  const RoutingDimension* const dimension_;
  RoutingModel* const model_;
  const std::function<int64(int64)> initializer_;
  RevArray<bool> is_initialized_;
  std::vector<int64> initial_values_;
  Rev<int64> current_index_;
  Rev<int64> current_route_;
  RevArray<int64> last_delta_used_;

  DISALLOW_COPY_AND_ASSIGN(GuidedSlackFinalizer);
};

GuidedSlackFinalizer::GuidedSlackFinalizer(
    const RoutingDimension* dimension, RoutingModel* model,
    std::function<int64(int64)> initializer)
    : dimension_(ABSL_DIE_IF_NULL(dimension)),
      model_(ABSL_DIE_IF_NULL(model)),
      initializer_(std::move(initializer)),
      is_initialized_(dimension->slacks().size(), false),
      initial_values_(dimension->slacks().size(), kint64min),
      current_index_(model_->Start(0)),
      current_route_(0),
      last_delta_used_(dimension->slacks().size(), 0) {}

Decision* GuidedSlackFinalizer::Next(Solver* solver) {
  CHECK_EQ(solver, model_->solver());
  const int node_idx = ChooseVariable();
  CHECK(node_idx == -1 ||
        (node_idx >= 0 && node_idx < dimension_->slacks().size()));
  if (node_idx != -1) {
    if (!is_initialized_[node_idx]) {
      initial_values_[node_idx] = initializer_(node_idx);
      is_initialized_.SetValue(solver, node_idx, true);
    }
    const int64 value = SelectValue(node_idx);
    IntVar* const slack_variable = dimension_->SlackVar(node_idx);
    return solver->MakeAssignVariableValue(slack_variable, value);
  }
  return nullptr;
}

int64 GuidedSlackFinalizer::SelectValue(int64 index) {
  const IntVar* const slack_variable = dimension_->SlackVar(index);
  const int64 center = initial_values_[index];
  const int64 max_delta =
      std::max(center - slack_variable->Min(), slack_variable->Max() - center) +
      1;
  int64 delta = last_delta_used_[index];

  // The sequence of deltas is 0, 1, -1, 2, -2 ...
  // Only the values inside the domain of variable are returned.
  while (std::abs(delta) < max_delta &&
         !slack_variable->Contains(center + delta)) {
    if (delta > 0) {
      delta = -delta;
    } else {
      delta = -delta + 1;
    }
  }
  last_delta_used_.SetValue(model_->solver(), index, delta);
  return center + delta;
}

int64 GuidedSlackFinalizer::ChooseVariable() {
  int64 int_current_node = current_index_.Value();
  int64 int_current_route = current_route_.Value();

  while (int_current_route < model_->vehicles()) {
    while (!model_->IsEnd(int_current_node) &&
           dimension_->SlackVar(int_current_node)->Bound()) {
      int_current_node = model_->NextVar(int_current_node)->Value();
    }
    if (!model_->IsEnd(int_current_node)) {
      break;
    }
    int_current_route += 1;
    if (int_current_route < model_->vehicles()) {
      int_current_node = model_->Start(int_current_route);
    }
  }

  CHECK(int_current_route == model_->vehicles() ||
        !dimension_->SlackVar(int_current_node)->Bound());
  current_index_.SetValue(model_->solver(), int_current_node);
  current_route_.SetValue(model_->solver(), int_current_route);
  if (int_current_route < model_->vehicles()) {
    return int_current_node;
  } else {
    return -1;
  }
}
}  // namespace

DecisionBuilder* RoutingModel::MakeGuidedSlackFinalizer(
    const RoutingDimension* dimension,
    std::function<int64(int64)> initializer) {
  return solver_->RevAlloc(
      new GuidedSlackFinalizer(dimension, this, std::move(initializer)));
}

int64 RoutingDimension::ShortestTransitionSlack(int64 node) const {
  CHECK_EQ(base_dimension_, this);
  CHECK(!model_->IsEnd(node));
  // Recall that the model is cumul[i+1] = cumul[i] + transit[i] + slack[i]. Our
  // aim is to find a value for slack[i] such that cumul[i+1] + transit[i+1] is
  // minimized.
  const int64 next = model_->NextVar(node)->Value();
  if (model_->IsEnd(next)) {
    return SlackVar(node)->Min();
  }
  const int64 next_next = model_->NextVar(next)->Value();
  const int64 serving_vehicle = model_->VehicleVar(node)->Value();
  CHECK_EQ(serving_vehicle, model_->VehicleVar(next)->Value());
  const RoutingModel::StateDependentTransit transit_from_next =
      model_->StateDependentTransitCallback(
          state_dependent_class_evaluators_
              [state_dependent_vehicle_to_class_[serving_vehicle]])(next,
                                                                    next_next);
  // We have that transit[i+1] is a function of cumul[i+1].
  const int64 next_cumul_min = CumulVar(next)->Min();
  const int64 next_cumul_max = CumulVar(next)->Max();
  const int64 optimal_next_cumul =
      transit_from_next.transit_plus_identity->RangeMinArgument(
          next_cumul_min, next_cumul_max + 1);
  // A few checks to make sure we're on the same page.
  DCHECK_LE(next_cumul_min, optimal_next_cumul);
  DCHECK_LE(optimal_next_cumul, next_cumul_max);
  // optimal_next_cumul = cumul + transit + optimal_slack, so
  // optimal_slack = optimal_next_cumul - cumul - transit.
  // In the current implementation TransitVar(i) = transit[i] + slack[i], so we
  // have to find the transit from the evaluators.
  const int64 current_cumul = CumulVar(node)->Value();
  const int64 current_state_independent_transit = model_->TransitCallback(
      class_evaluators_[vehicle_to_class_[serving_vehicle]])(node, next);
  const int64 current_state_dependent_transit =
      model_
          ->StateDependentTransitCallback(
              state_dependent_class_evaluators_
                  [state_dependent_vehicle_to_class_[serving_vehicle]])(node,
                                                                        next)
          .transit->Query(current_cumul);
  const int64 optimal_slack = optimal_next_cumul - current_cumul -
                              current_state_independent_transit -
                              current_state_dependent_transit;
  CHECK_LE(SlackVar(node)->Min(), optimal_slack);
  CHECK_LE(optimal_slack, SlackVar(node)->Max());
  return optimal_slack;
}

namespace {
class GreedyDescentLSOperator : public LocalSearchOperator {
 public:
  explicit GreedyDescentLSOperator(std::vector<IntVar*> variables);

  bool MakeNextNeighbor(Assignment* delta, Assignment* deltadelta) override;
  void Start(const Assignment* assignment) override;

 private:
  int64 FindMaxDistanceToDomain(const Assignment* assignment);

  const std::vector<IntVar*> variables_;
  const Assignment* center_;
  int64 current_step_;
  // The deltas are returned in this order:
  // (current_step_, 0, ... 0), (-current_step_, 0, ... 0),
  // (0, current_step_, ... 0), (0, -current_step_, ... 0),
  // ...
  // (0, ... 0, current_step_), (0, ... 0, -current_step_).
  // current_direction_ keeps track what was the last returned delta.
  int64 current_direction_;

  DISALLOW_COPY_AND_ASSIGN(GreedyDescentLSOperator);
};

GreedyDescentLSOperator::GreedyDescentLSOperator(std::vector<IntVar*> variables)
    : variables_(std::move(variables)),
      center_(nullptr),
      current_step_(0),
      current_direction_(0) {}

bool GreedyDescentLSOperator::MakeNextNeighbor(Assignment* delta,
                                               Assignment* /*deltadelta*/) {
  static const int64 sings[] = {1, -1};
  for (; 1 <= current_step_; current_step_ /= 2) {
    for (; current_direction_ < 2 * variables_.size();) {
      const int64 variable_idx = current_direction_ / 2;
      IntVar* const variable = variables_[variable_idx];
      const int64 sign_index = current_direction_ % 2;
      const int64 sign = sings[sign_index];
      const int64 offset = sign * current_step_;
      const int64 new_value = center_->Value(variable) + offset;
      ++current_direction_;
      if (variable->Contains(new_value)) {
        delta->Add(variable);
        delta->SetValue(variable, new_value);
        return true;
      }
    }
    current_direction_ = 0;
  }
  return false;
}

void GreedyDescentLSOperator::Start(const Assignment* assignment) {
  CHECK(assignment != nullptr);
  current_step_ = FindMaxDistanceToDomain(assignment);
  center_ = assignment;
}

int64 GreedyDescentLSOperator::FindMaxDistanceToDomain(
    const Assignment* assignment) {
  int64 result = kint64min;
  for (const IntVar* const var : variables_) {
    result = std::max(result, std::abs(var->Max() - assignment->Value(var)));
    result = std::max(result, std::abs(var->Min() - assignment->Value(var)));
  }
  return result;
}
}  // namespace

std::unique_ptr<LocalSearchOperator> RoutingModel::MakeGreedyDescentLSOperator(
    std::vector<IntVar*> variables) {
  return std::unique_ptr<LocalSearchOperator>(
      new GreedyDescentLSOperator(std::move(variables)));
}

DecisionBuilder* RoutingModel::MakeSelfDependentDimensionFinalizer(
    const RoutingDimension* dimension) {
  CHECK(dimension != nullptr);
  CHECK(dimension->base_dimension() == dimension);
  std::function<int64(int64)> slack_guide = [dimension](int64 index) {
    return dimension->ShortestTransitionSlack(index);
  };
  DecisionBuilder* const guided_finalizer =
      MakeGuidedSlackFinalizer(dimension, slack_guide);
  DecisionBuilder* const slacks_finalizer =
      solver_->MakeSolveOnce(guided_finalizer);
  std::vector<IntVar*> start_cumuls(vehicles_, nullptr);
  for (int64 vehicle_idx = 0; vehicle_idx < vehicles_; ++vehicle_idx) {
    start_cumuls[vehicle_idx] = dimension->CumulVar(starts_[vehicle_idx]);
  }
  LocalSearchOperator* const hill_climber =
      solver_->RevAlloc(new GreedyDescentLSOperator(start_cumuls));
  LocalSearchPhaseParameters* const parameters =
      solver_->MakeLocalSearchPhaseParameters(CostVar(), hill_climber,
                                              slacks_finalizer);
  Assignment* const first_solution = solver_->MakeAssignment();
  first_solution->Add(start_cumuls);
  for (IntVar* const cumul : start_cumuls) {
    first_solution->SetValue(cumul, cumul->Min());
  }
  DecisionBuilder* const finalizer =
      solver_->MakeLocalSearchPhase(first_solution, parameters);
  return finalizer;
}
}  // namespace operations_research
