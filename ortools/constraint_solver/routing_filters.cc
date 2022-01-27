// Copyright 2010-2021 Google LLC
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

// Implementation of local search filters for routing models.

#include "ortools/constraint_solver/routing_filters.h"

#include <stddef.h>

#include <algorithm>
#include <cstdint>
#include <deque>
#include <functional>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <numeric>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/flags/flag.h"
#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "ortools/base/int_type.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/map_util.h"
#include "ortools/base/small_map.h"
#include "ortools/base/small_ordered_set.h"
#include "ortools/base/strong_vector.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "ortools/constraint_solver/routing.h"
#include "ortools/constraint_solver/routing_lp_scheduling.h"
#include "ortools/constraint_solver/routing_parameters.pb.h"
#include "ortools/graph/min_cost_flow.h"
#include "ortools/util/bitset.h"
#include "ortools/util/piecewise_linear_function.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/util/sorted_interval_list.h"

ABSL_FLAG(bool, routing_strong_debug_checks, false,
          "Run stronger checks in debug; these stronger tests might change "
          "the complexity of the code in particular.");

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
              int64_t objective_min, int64_t objective_max) override {
    const int64_t kUnassigned = -1;
    const Assignment::IntContainer& container = delta->IntVarContainer();
    const int delta_size = container.Size();
    int current_active_vehicles = active_vehicles_;
    for (int i = 0; i < delta_size; ++i) {
      const IntVarElement& new_element = container.Element(i);
      IntVar* const var = new_element.Var();
      int64_t index = kUnassigned;
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
  explicit NodeDisjunctionFilter(const RoutingModel& routing_model,
                                 bool filter_cost)
      : IntVarLocalSearchFilter(routing_model.Nexts()),
        routing_model_(routing_model),
        active_per_disjunction_(routing_model.GetNumberOfDisjunctions(), 0),
        inactive_per_disjunction_(routing_model.GetNumberOfDisjunctions(), 0),
        synchronized_objective_value_(std::numeric_limits<int64_t>::min()),
        accepted_objective_value_(std::numeric_limits<int64_t>::min()),
        filter_cost_(filter_cost),
        has_mandatory_disjunctions_(routing_model.HasMandatoryDisjunctions()) {}

  bool Accept(const Assignment* delta, const Assignment* deltadelta,
              int64_t objective_min, int64_t objective_max) override {
    const int64_t kUnassigned = -1;
    const Assignment::IntContainer& container = delta->IntVarContainer();
    const int delta_size = container.Size();
    gtl::small_map<absl::flat_hash_map<RoutingModel::DisjunctionIndex, int>>
        disjunction_active_deltas;
    gtl::small_map<absl::flat_hash_map<RoutingModel::DisjunctionIndex, int>>
        disjunction_inactive_deltas;
    bool lns_detected = false;
    // Update active/inactive count per disjunction for each element of delta.
    for (int i = 0; i < delta_size; ++i) {
      const IntVarElement& new_element = container.Element(i);
      IntVar* const var = new_element.Var();
      int64_t index = kUnassigned;
      if (FindIndex(var, &index)) {
        const bool is_inactive =
            (new_element.Min() <= index && new_element.Max() >= index);
        if (new_element.Min() != new_element.Max()) {
          lns_detected = true;
        }
        for (const RoutingModel::DisjunctionIndex disjunction_index :
             routing_model_.GetDisjunctionIndices(index)) {
          const bool is_var_synced = IsVarSynced(index);
          if (!is_var_synced || (Value(index) == index) != is_inactive) {
            ++gtl::LookupOrInsert(is_inactive ? &disjunction_inactive_deltas
                                              : &disjunction_active_deltas,
                                  disjunction_index, 0);
            if (is_var_synced) {
              --gtl::LookupOrInsert(is_inactive ? &disjunction_active_deltas
                                                : &disjunction_inactive_deltas,
                                    disjunction_index, 0);
            }
          }
        }
      }
    }
    // Check if any disjunction has too many active nodes.
    for (const auto [disjunction_index, active_nodes] :
         disjunction_active_deltas) {
      // Too many active nodes.
      if (active_per_disjunction_[disjunction_index] + active_nodes >
          routing_model_.GetDisjunctionMaxCardinality(disjunction_index)) {
        return false;
      }
    }
    if (lns_detected || (!filter_cost_ && !has_mandatory_disjunctions_)) {
      accepted_objective_value_ = 0;
      return true;
    }
    // Update penalty costs for disjunctions.
    accepted_objective_value_ = synchronized_objective_value_;
    for (const auto [disjunction_index, inactive_nodes] :
         disjunction_inactive_deltas) {
      const int64_t penalty =
          routing_model_.GetDisjunctionPenalty(disjunction_index);
      if (penalty == 0) continue;
      const int current_inactive_nodes =
          inactive_per_disjunction_[disjunction_index];
      const int max_inactive_cardinality =
          routing_model_.GetDisjunctionNodeIndices(disjunction_index).size() -
          routing_model_.GetDisjunctionMaxCardinality(disjunction_index);
      // Too many inactive nodes.
      if (current_inactive_nodes + inactive_nodes > max_inactive_cardinality) {
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
        accepted_objective_value_ = CapSub(accepted_objective_value_, penalty);
      }
    }
    // Only compare to max as a cost lower bound is computed.
    return accepted_objective_value_ <= objective_max;
  }
  std::string DebugString() const override { return "NodeDisjunctionFilter"; }
  int64_t GetSynchronizedObjectiveValue() const override {
    return synchronized_objective_value_;
  }
  int64_t GetAcceptedObjectiveValue() const override {
    return accepted_objective_value_;
  }

 private:
  void OnSynchronize(const Assignment* delta) override {
    synchronized_objective_value_ = 0;
    for (RoutingModel::DisjunctionIndex i(0);
         i < active_per_disjunction_.size(); ++i) {
      active_per_disjunction_[i] = 0;
      inactive_per_disjunction_[i] = 0;
      const std::vector<int64_t>& disjunction_indices =
          routing_model_.GetDisjunctionNodeIndices(i);
      for (const int64_t index : disjunction_indices) {
        if (IsVarSynced(index)) {
          if (Value(index) != index) {
            ++active_per_disjunction_[i];
          } else {
            ++inactive_per_disjunction_[i];
          }
        }
      }
      if (!filter_cost_) continue;
      const int64_t penalty = routing_model_.GetDisjunctionPenalty(i);
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

  absl::StrongVector<RoutingModel::DisjunctionIndex, int>
      active_per_disjunction_;
  absl::StrongVector<RoutingModel::DisjunctionIndex, int>
      inactive_per_disjunction_;
  int64_t synchronized_objective_value_;
  int64_t accepted_objective_value_;
  const bool filter_cost_;
  const bool has_mandatory_disjunctions_;
};
}  // namespace

IntVarLocalSearchFilter* MakeNodeDisjunctionFilter(
    const RoutingModel& routing_model, bool filter_cost) {
  return routing_model.solver()->RevAlloc(
      new NodeDisjunctionFilter(routing_model, filter_cost));
}

const int64_t BasePathFilter::kUnassigned = -1;

BasePathFilter::BasePathFilter(const std::vector<IntVar*>& nexts,
                               int next_domain_size)
    : IntVarLocalSearchFilter(nexts),
      node_path_starts_(next_domain_size, kUnassigned),
      paths_(nexts.size(), -1),
      new_synchronized_unperformed_nodes_(nexts.size()),
      new_nexts_(nexts.size(), kUnassigned),
      touched_paths_(nexts.size()),
      touched_path_chain_start_ends_(nexts.size(), {kUnassigned, kUnassigned}),
      ranks_(next_domain_size, -1),
      status_(BasePathFilter::UNKNOWN) {}

bool BasePathFilter::Accept(const Assignment* delta,
                            const Assignment* deltadelta, int64_t objective_min,
                            int64_t objective_max) {
  if (IsDisabled()) return true;
  for (const int touched : delta_touched_) {
    new_nexts_[touched] = kUnassigned;
  }
  delta_touched_.clear();
  const Assignment::IntContainer& container = delta->IntVarContainer();
  const int delta_size = container.Size();
  delta_touched_.reserve(delta_size);
  // Determining touched paths and their touched chain start and ends (a node is
  // touched if it corresponds to an element of delta or that an element of
  // delta points to it).
  // The start and end of a touched path subchain will have remained on the same
  // path and will correspond to the min and max ranks of touched nodes in the
  // current assignment.
  for (int64_t touched_path : touched_paths_.PositionsSetAtLeastOnce()) {
    touched_path_chain_start_ends_[touched_path] = {kUnassigned, kUnassigned};
  }
  touched_paths_.SparseClearAll();

  const auto update_touched_path_chain_start_end = [this](int64_t index) {
    const int64_t start = node_path_starts_[index];
    if (start == kUnassigned) return;
    touched_paths_.Set(start);

    int64_t& chain_start = touched_path_chain_start_ends_[start].first;
    if (chain_start == kUnassigned || ranks_[index] < ranks_[chain_start]) {
      chain_start = index;
    }

    int64_t& chain_end = touched_path_chain_start_ends_[start].second;
    if (chain_end == kUnassigned || ranks_[index] > ranks_[chain_end]) {
      chain_end = index;
    }
  };

  for (int i = 0; i < delta_size; ++i) {
    const IntVarElement& new_element = container.Element(i);
    IntVar* const var = new_element.Var();
    int64_t index = kUnassigned;
    if (FindIndex(var, &index)) {
      if (!new_element.Bound()) {
        // LNS detected
        return true;
      }
      new_nexts_[index] = new_element.Value();
      delta_touched_.push_back(index);
      update_touched_path_chain_start_end(index);
      update_touched_path_chain_start_end(new_nexts_[index]);
    }
  }
  // Checking feasibility of touched paths.
  if (!InitializeAcceptPath()) return false;
  for (const int64_t touched_start : touched_paths_.PositionsSetAtLeastOnce()) {
    const std::pair<int64_t, int64_t> start_end =
        touched_path_chain_start_ends_[touched_start];
    if (!AcceptPath(touched_start, start_end.first, start_end.second)) {
      return false;
    }
  }
  // NOTE: FinalizeAcceptPath() is only called if InitializeAcceptPath() is true
  // and all paths are accepted.
  return FinalizeAcceptPath(objective_min, objective_max);
}

void BasePathFilter::ComputePathStarts(std::vector<int64_t>* path_starts,
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
  std::vector<int64_t> path_starts;
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
  for (int64_t index = 0; index < Size(); index++) {
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
  for (const int64_t start : starts_) {
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
  DCHECK(!absl::GetFlag(FLAGS_routing_strong_debug_checks) ||
         !HavePathsChanged());
  const Assignment::IntContainer& container = delta->IntVarContainer();
  touched_paths_.SparseClearAll();
  for (int i = 0; i < container.Size(); ++i) {
    const IntVarElement& new_element = container.Element(i);
    int64_t index = kUnassigned;
    if (FindIndex(new_element.Var(), &index)) {
      const int64_t start = node_path_starts_[index];
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
  for (const int64_t touched_start : touched_paths_.PositionsSetAtLeastOnce()) {
    int64_t node = touched_start;
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
  int64_t node = start;
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
  int64_t GetSynchronizedObjectiveValue() const override {
    return current_vehicle_cost_;
  }
  int64_t GetAcceptedObjectiveValue() const override {
    return delta_vehicle_cost_;
  }

 private:
  void OnSynchronizePathFromStart(int64_t start) override;
  void OnAfterSynchronizePaths() override;
  bool InitializeAcceptPath() override;
  bool AcceptPath(int64_t path_start, int64_t chain_start,
                  int64_t chain_end) override;
  bool FinalizeAcceptPath(int64_t objective_min,
                          int64_t objective_max) override;

  int64_t current_vehicle_cost_;
  int64_t delta_vehicle_cost_;
  std::vector<int> current_route_lengths_;
  std::vector<int64_t> start_to_end_;
  std::vector<int> start_to_vehicle_;
  std::vector<int64_t> vehicle_to_start_;
  const std::vector<int64_t>& linear_cost_factor_of_vehicle_;
  const std::vector<int64_t>& quadratic_cost_factor_of_vehicle_;
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
    const int64_t start = routing_model.Start(v);
    start_to_vehicle_[start] = v;
    start_to_end_[start] = routing_model.End(v);
    vehicle_to_start_[v] = start;
  }
}

void VehicleAmortizedCostFilter::OnSynchronizePathFromStart(int64_t start) {
  const int64_t end = start_to_end_[start];
  CHECK_GE(end, 0);
  const int route_length = Rank(end) - 1;
  CHECK_GE(route_length, 0);
  current_route_lengths_[start] = route_length;
}

void VehicleAmortizedCostFilter::OnAfterSynchronizePaths() {
  current_vehicle_cost_ = 0;
  for (int vehicle = 0; vehicle < vehicle_to_start_.size(); vehicle++) {
    const int64_t start = vehicle_to_start_[vehicle];
    DCHECK_EQ(vehicle, start_to_vehicle_[start]);

    const int route_length = current_route_lengths_[start];
    DCHECK_GE(route_length, 0);

    if (route_length == 0) {
      // The path is empty.
      continue;
    }

    const int64_t linear_cost_factor = linear_cost_factor_of_vehicle_[vehicle];
    const int64_t route_length_cost =
        CapProd(quadratic_cost_factor_of_vehicle_[vehicle],
                route_length * route_length);

    current_vehicle_cost_ = CapAdd(
        current_vehicle_cost_, CapSub(linear_cost_factor, route_length_cost));
  }
}

bool VehicleAmortizedCostFilter::InitializeAcceptPath() {
  delta_vehicle_cost_ = current_vehicle_cost_;
  return true;
}

bool VehicleAmortizedCostFilter::AcceptPath(int64_t path_start,
                                            int64_t chain_start,
                                            int64_t chain_end) {
  // Number of nodes previously between chain_start and chain_end
  const int previous_chain_nodes = Rank(chain_end) - 1 - Rank(chain_start);
  CHECK_GE(previous_chain_nodes, 0);
  int new_chain_nodes = 0;
  int64_t node = GetNext(chain_start);
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
  const int64_t quadratic_cost_factor =
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

bool VehicleAmortizedCostFilter::FinalizeAcceptPath(int64_t objective_min,
                                                    int64_t objective_max) {
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
  void OnSynchronizePathFromStart(int64_t start) override;
  bool AcceptPath(int64_t path_start, int64_t chain_start,
                  int64_t chain_end) override;

  bool HardIncompatibilitiesRespected(int vehicle, int64_t chain_start,
                                      int64_t chain_end);

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
    const int64_t start = model.Start(vehicle);
    start_to_vehicle_[start] = vehicle;
    if (has_hard_type_incompatibilities) {
      hard_incompatibility_type_counts_per_vehicle_[vehicle].resize(
          num_visit_types, 0);
    }
  }
}

void TypeRegulationsFilter::OnSynchronizePathFromStart(int64_t start) {
  if (!routing_model_.HasHardTypeIncompatibilities()) return;

  const int vehicle = start_to_vehicle_[start];
  CHECK_GE(vehicle, 0);
  std::vector<int>& type_counts =
      hard_incompatibility_type_counts_per_vehicle_[vehicle];
  std::fill(type_counts.begin(), type_counts.end(), 0);
  const int num_types = type_counts.size();

  int64_t node = start;
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
                                                           int64_t chain_start,
                                                           int64_t chain_end) {
  if (!routing_model_.HasHardTypeIncompatibilities()) return true;

  const std::vector<int>& previous_type_counts =
      hard_incompatibility_type_counts_per_vehicle_[vehicle];

  absl::flat_hash_map</*type*/ int, /*new_count*/ int> new_type_counts;
  absl::flat_hash_set<int> types_to_check;

  // Go through the new nodes on the path and increment their type counts.
  int64_t node = GetNext(chain_start);
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

bool TypeRegulationsFilter::AcceptPath(int64_t path_start, int64_t chain_start,
                                       int64_t chain_end) {
  const int vehicle = start_to_vehicle_[path_start];
  CHECK_GE(vehicle, 0);
  const auto next_accessor = [this](int64_t node) { return GetNext(node); };
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
  void OnSynchronizePathFromStart(int64_t start) override;
  bool AcceptPath(int64_t path_start, int64_t chain_start,
                  int64_t chain_end) override;

  const std::vector<IntVar*> cumuls_;
  std::vector<int64_t> start_to_vehicle_;
  std::vector<int64_t> start_to_end_;
  std::vector<const RoutingModel::TransitCallback2*> evaluators_;
  const std::vector<int64_t> vehicle_capacities_;
  std::vector<int64_t> current_path_cumul_mins_;
  std::vector<int64_t> current_max_of_path_end_cumul_mins_;
  std::vector<int64_t> old_nexts_;
  std::vector<int> old_vehicles_;
  std::vector<int64_t> current_transits_;
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
void ChainCumulFilter::OnSynchronizePathFromStart(int64_t start) {
  const int vehicle = start_to_vehicle_[start];
  std::vector<int64_t> path_nodes;
  int64_t node = start;
  int64_t cumul = cumuls_[node]->Min();
  while (node < Size()) {
    path_nodes.push_back(node);
    current_path_cumul_mins_[node] = cumul;
    const int64_t next = Value(node);
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
  int64_t max_cumuls = cumul;
  for (int i = path_nodes.size() - 1; i >= 0; --i) {
    const int64_t node = path_nodes[i];
    max_cumuls = std::max(max_cumuls, current_path_cumul_mins_[node]);
    current_max_of_path_end_cumul_mins_[node] = max_cumuls;
  }
}

// The complexity of the method is O(size of chain (chain_start...chain_end).
bool ChainCumulFilter::AcceptPath(int64_t path_start, int64_t chain_start,
                                  int64_t chain_end) {
  const int vehicle = start_to_vehicle_[path_start];
  const int64_t capacity = vehicle_capacities_[vehicle];
  int64_t node = chain_start;
  int64_t cumul = current_path_cumul_mins_[node];
  while (node != chain_end) {
    const int64_t next = GetNext(node);
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
  const int64_t end = start_to_end_[path_start];
  const int64_t end_cumul_delta =
      CapSub(current_path_cumul_mins_[end], current_path_cumul_mins_[node]);
  const int64_t after_chain_cumul_delta =
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
  int64_t GetSynchronizedObjectiveValue() const override {
    return propagate_own_objective_value_ ? synchronized_objective_value_ : 0;
  }
  int64_t GetAcceptedObjectiveValue() const override {
    return propagate_own_objective_value_ ? accepted_objective_value_ : 0;
  }

 private:
  // This structure stores the "best" path cumul value for a solution, the path
  // supporting this value, and the corresponding path cumul values for all
  // paths.
  struct SupportedPathCumul {
    SupportedPathCumul() : cumul_value(0), cumul_value_support(0) {}
    int64_t cumul_value;
    int cumul_value_support;
    std::vector<int64_t> path_values;
  };

  struct SoftBound {
    SoftBound() : bound(-1), coefficient(0) {}
    int64_t bound;
    int64_t coefficient;
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
    void PushTransit(int path, int node, int next, int64_t transit) {
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
    int64_t Transit(int path, int position) const {
      return transits_[path][position];
    }

   private:
    // paths_[r][i] is the ith node on path r.
    std::vector<std::vector<int64_t>> paths_;
    // transits_[r][i] is the transit value between nodes path_[i] and
    // path_[i+1] on path r.
    std::vector<std::vector<int64_t>> transits_;
  };

  bool InitializeAcceptPath() override {
    cumul_cost_delta_ = total_current_cumul_cost_value_;
    node_with_precedence_to_delta_min_max_cumuls_.clear();
    // Cleaning up for the new delta.
    delta_max_end_cumul_ = std::numeric_limits<int64_t>::min();
    delta_paths_.clear();
    delta_path_transits_.Clear();
    lns_detected_ = false;
    delta_nodes_with_precedences_and_changed_cumul_.ClearAll();
    return true;
  }
  bool AcceptPath(int64_t path_start, int64_t chain_start,
                  int64_t chain_end) override;
  bool FinalizeAcceptPath(int64_t objective_min,
                          int64_t objective_max) override;
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

  int64_t GetCumulSoftCost(int64_t node, int64_t cumul_value) const;

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
    if (vehicle_span_upper_bounds_[vehicle] <
        std::numeric_limits<int64_t>::max()) {
      ++num_linear_constraints;
    }
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

  int64_t GetCumulPiecewiseLinearCost(int64_t node, int64_t cumul_value) const;

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

  int64_t GetCumulSoftLowerBoundCost(int64_t node, int64_t cumul_value) const;

  int64_t GetPathCumulSoftLowerBoundCost(const PathTransits& path_transits,
                                         int path) const;

  void InitializeSupportedPathCumul(SupportedPathCumul* supported_cumul,
                                    int64_t default_value);

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
      const std::vector<int64_t>& min_path_cumuls) const;

  // Computes the maximum cumul value of nodes along the path using
  // [current|delta]_path_transits_, and stores the min/max cumul
  // related to each node in the corresponding vector
  // [current|delta]_[min|max]_node_cumuls_.
  // The boolean is_delta indicates if the computations should take place on the
  // "delta" or "current" members. When true, the nodes for which the min/max
  // cumul has changed from the current value are marked in
  // delta_nodes_with_precedences_and_changed_cumul_.
  void StoreMinMaxCumulOfNodesOnPath(
      int path, const std::vector<int64_t>& min_path_cumuls, bool is_delta);

  // Compute the max start cumul value for a given path and a given minimal end
  // cumul value.
  // NOTE: Since this function is used to compute a lower bound on the span of
  // the routes, we don't "jump" over the forbidden intervals with this min end
  // cumul value. We do however concurrently compute the max possible start
  // given the max end cumul, for which we can "jump" over forbidden intervals,
  // and return the minimum of the two.
  int64_t ComputePathMaxStartFromEndCumul(const PathTransits& path_transits,
                                          int path, int64_t path_start,
                                          int64_t min_end_cumul) const;

  const RoutingModel& routing_model_;
  const RoutingDimension& dimension_;
  const std::vector<IntVar*> cumuls_;
  const std::vector<IntVar*> slacks_;
  std::vector<int64_t> start_to_vehicle_;
  std::vector<const RoutingModel::TransitCallback2*> evaluators_;
  std::vector<int64_t> vehicle_span_upper_bounds_;
  bool has_vehicle_span_upper_bounds_;
  int64_t total_current_cumul_cost_value_;
  int64_t synchronized_objective_value_;
  int64_t accepted_objective_value_;
  // Map between paths and path soft cumul bound costs. The paths are indexed
  // by the index of the start node of the path.
  absl::flat_hash_map<int64_t, int64_t> current_cumul_cost_values_;
  int64_t cumul_cost_delta_;
  // Cumul cost values for paths in delta, indexed by vehicle.
  std::vector<int64_t> delta_path_cumul_cost_values_;
  const int64_t global_span_cost_coefficient_;
  std::vector<SoftBound> cumul_soft_bounds_;
  std::vector<SoftBound> cumul_soft_lower_bounds_;
  std::vector<const PiecewiseLinearFunction*> cumul_piecewise_linear_costs_;
  std::vector<int64_t> vehicle_span_cost_coefficients_;
  bool has_nonzero_vehicle_span_cost_coefficients_;
  const std::vector<int64_t> vehicle_capacities_;
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
  std::vector<std::pair<int64_t, int64_t>> current_min_max_node_cumuls_;
  // Data reflecting information on paths and cumul variables for the "delta"
  // solution (aka neighbor solution) being examined.
  PathTransits delta_path_transits_;
  int64_t delta_max_end_cumul_;
  SparseBitset<int64_t> delta_nodes_with_precedences_and_changed_cumul_;
  absl::flat_hash_map<int64_t, std::pair<int64_t, int64_t>>
      node_with_precedence_to_delta_min_max_cumuls_;
  // Note: small_ordered_set only support non-hash sets.
  gtl::small_ordered_set<std::set<int>> delta_paths_;
  const std::string name_;

  LocalDimensionCumulOptimizer* optimizer_;
  LocalDimensionCumulOptimizer* mp_optimizer_;
  const bool filter_objective_cost_;
  // This boolean indicates if the LP optimizer can be used if necessary to
  // optimize the dimension cumuls, and is only used for testing purposes.
  const bool can_use_lp_;
  const bool propagate_own_objective_value_;

  // Used to do span lower bounding in presence of vehicle breaks.
  DisjunctivePropagator disjunctive_propagator_;
  DisjunctivePropagator::Tasks tasks_;
  TravelBounds travel_bounds_;
  std::vector<int64_t> current_path_;

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
      delta_path_cumul_cost_values_(routing_model.vehicles(),
                                    std::numeric_limits<int64_t>::min()),
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
  for (const int64_t upper_bound : vehicle_span_upper_bounds_) {
    if (upper_bound != std::numeric_limits<int64_t>::max()) {
      has_vehicle_span_upper_bounds_ = true;
      break;
    }
  }
  for (const int64_t coefficient : vehicle_span_cost_coefficients_) {
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
    if (cumul_var->Min() > 0 ||
        cumul_var->Max() < std::numeric_limits<int64_t>::max()) {
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

#ifndef NDEBUG
  for (int vehicle = 0; vehicle < routing_model.vehicles(); vehicle++) {
    if (FilterWithDimensionCumulOptimizerForVehicle(vehicle)) {
      DCHECK_NE(optimizer_, nullptr);
      DCHECK_NE(mp_optimizer_, nullptr);
    }
  }
#endif  // NDEBUG
}

int64_t PathCumulFilter::GetCumulSoftCost(int64_t node,
                                          int64_t cumul_value) const {
  if (node < cumul_soft_bounds_.size()) {
    const int64_t bound = cumul_soft_bounds_[node].bound;
    const int64_t coefficient = cumul_soft_bounds_[node].coefficient;
    if (coefficient > 0 && bound < cumul_value) {
      return CapProd(CapSub(cumul_value, bound), coefficient);
    }
  }
  return 0;
}

int64_t PathCumulFilter::GetCumulPiecewiseLinearCost(
    int64_t node, int64_t cumul_value) const {
  if (node < cumul_piecewise_linear_costs_.size()) {
    const PiecewiseLinearFunction* cost = cumul_piecewise_linear_costs_[node];
    if (cost != nullptr) {
      return cost->Value(cumul_value);
    }
  }
  return 0;
}

int64_t PathCumulFilter::GetCumulSoftLowerBoundCost(int64_t node,
                                                    int64_t cumul_value) const {
  if (node < cumul_soft_lower_bounds_.size()) {
    const int64_t bound = cumul_soft_lower_bounds_[node].bound;
    const int64_t coefficient = cumul_soft_lower_bounds_[node].coefficient;
    if (coefficient > 0 && bound > cumul_value) {
      return CapProd(CapSub(bound, cumul_value), coefficient);
    }
  }
  return 0;
}

int64_t PathCumulFilter::GetPathCumulSoftLowerBoundCost(
    const PathTransits& path_transits, int path) const {
  int64_t node = path_transits.Node(path, path_transits.PathSize(path) - 1);
  int64_t cumul = cumuls_[node]->Max();
  int64_t current_cumul_cost_value = GetCumulSoftLowerBoundCost(node, cumul);
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
    InitializeSupportedPathCumul(&current_min_start_,
                                 std::numeric_limits<int64_t>::max());
    InitializeSupportedPathCumul(&current_max_end_,
                                 std::numeric_limits<int64_t>::min());
    current_path_transits_.Clear();
    current_path_transits_.AddPaths(NumPaths());
    // For each path, compute the minimum end cumul and store the max of these.
    for (int r = 0; r < NumPaths(); ++r) {
      int64_t node = Start(r);
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
      int64_t cumul = cumuls_[node]->Min();
      std::vector<int64_t> min_path_cumuls;
      min_path_cumuls.reserve(number_of_route_arcs + 1);
      min_path_cumuls.push_back(cumul);

      int64_t current_cumul_cost_value = GetCumulSoftCost(node, cumul);
      current_cumul_cost_value = CapAdd(
          current_cumul_cost_value, GetCumulPiecewiseLinearCost(node, cumul));

      int64_t total_transit = 0;
      while (node < Size()) {
        const int64_t next = Value(node);
        const int64_t transit = (*evaluators_[vehicle])(node, next);
        total_transit = CapAdd(total_transit, transit);
        const int64_t transit_slack = CapAdd(transit, slacks_[node]->Min());
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
          !routing_model_.IsVehicleUsedWhenEmpty(vehicle)) {
        // This is an empty route (single start->end arc) which we don't take
        // into account for costs.
        current_cumul_cost_values_[Start(r)] = 0;
        current_path_transits_.ClearPath(r);
        continue;
      }
      if (FilterSlackCost() || FilterSoftSpanCost() ||
          FilterSoftSpanQuadraticCost()) {
        const int64_t start = ComputePathMaxStartFromEndCumul(
            current_path_transits_, r, Start(r), cumul);
        const int64_t span_lower_bound = CapSub(cumul, start);
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
            const int64_t violation =
                CapSub(span_lower_bound, bound_cost.bound);
            current_cumul_cost_value = CapAdd(
                current_cumul_cost_value, CapProd(bound_cost.cost, violation));
          }
        }
        if (FilterSoftSpanQuadraticCost()) {
          const SimpleBoundCosts::BoundCost bound_cost =
              dimension_.GetQuadraticCostSoftSpanUpperBoundForVehicle(vehicle);
          if (bound_cost.bound < span_lower_bound) {
            const int64_t violation =
                CapSub(span_lower_bound, bound_cost.bound);
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
        int64_t lp_cumul_cost_value = 0;
        LocalDimensionCumulOptimizer* const optimizer =
            FilterBreakCost(vehicle) ? mp_optimizer_ : optimizer_;
        DCHECK(optimizer != nullptr);
        const DimensionSchedulingStatus status =
            optimizer->ComputeRouteCumulCostWithoutFixedTransits(
                vehicle, [this](int64_t node) { return Value(node); },
                &lp_cumul_cost_value);
        switch (status) {
          case DimensionSchedulingStatus::INFEASIBLE:
            lp_cumul_cost_value = 0;
            break;
          case DimensionSchedulingStatus::RELAXED_OPTIMAL_ONLY:
            DCHECK(mp_optimizer_ != nullptr);
            if (mp_optimizer_->ComputeRouteCumulCostWithoutFixedTransits(
                    vehicle, [this](int64_t node) { return Value(node); },
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
      for (int64_t node : GetNewSynchronizedUnperformedNodes()) {
        current_min_max_node_cumuls_[node] = {-1, -1};
      }
    }
    // Use the max of the path end cumul mins to compute the corresponding
    // maximum start cumul of each path; store the minimum of these.
    for (int r = 0; r < NumPaths(); ++r) {
      const int64_t start = ComputePathMaxStartFromEndCumul(
          current_path_transits_, r, Start(r), current_max_end_.cumul_value);
      current_min_start_.path_values[r] = start;
      if (current_min_start_.cumul_value > start) {
        current_min_start_.cumul_value = start;
        current_min_start_.cumul_value_support = r;
      }
    }
  }
  // Initialize this before considering any deltas (neighbor).
  delta_max_end_cumul_ = std::numeric_limits<int64_t>::min();
  lns_detected_ = false;

  DCHECK(global_span_cost_coefficient_ == 0 ||
         current_min_start_.cumul_value <= current_max_end_.cumul_value);
  synchronized_objective_value_ =
      CapAdd(total_current_cumul_cost_value_,
             CapProd(global_span_cost_coefficient_,
                     CapSub(current_max_end_.cumul_value,
                            current_min_start_.cumul_value)));
}

bool PathCumulFilter::AcceptPath(int64_t path_start, int64_t chain_start,
                                 int64_t chain_end) {
  int64_t node = path_start;
  int64_t cumul = cumuls_[node]->Min();
  int64_t cumul_cost_delta = 0;
  int64_t total_transit = 0;
  const int path = delta_path_transits_.AddPaths(1);
  const int vehicle = start_to_vehicle_[path_start];
  const int64_t capacity = vehicle_capacities_[vehicle];
  const bool filter_vehicle_costs =
      !routing_model_.IsEnd(GetNext(node)) ||
      routing_model_.IsVehicleUsedWhenEmpty(vehicle);
  if (filter_vehicle_costs) {
    cumul_cost_delta = CapAdd(GetCumulSoftCost(node, cumul),
                              GetCumulPiecewiseLinearCost(node, cumul));
  }
  // Evaluating route length to reserve memory to store transit information.
  int number_of_route_arcs = 0;
  while (node < Size()) {
    const int64_t next = GetNext(node);
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
  std::vector<int64_t> min_path_cumuls;
  min_path_cumuls.reserve(number_of_route_arcs + 1);
  min_path_cumuls.push_back(cumul);
  // Check that the path is feasible with regards to cumul bounds, scanning
  // the paths from start to end (caching path node sequences and transits
  // for further span cost filtering).
  node = path_start;
  while (node < Size()) {
    const int64_t next = GetNext(node);
    const int64_t transit = (*evaluators_[vehicle])(node, next);
    total_transit = CapAdd(total_transit, transit);
    const int64_t transit_slack = CapAdd(transit, slacks_[node]->Min());
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
  const int64_t min_end = cumul;

  if (!PickupToDeliveryLimitsRespected(delta_path_transits_, path,
                                       min_path_cumuls)) {
    return false;
  }
  if (FilterSlackCost() || FilterBreakCost(vehicle) ||
      FilterSoftSpanCost(vehicle) || FilterSoftSpanQuadraticCost(vehicle)) {
    int64_t slack_max = std::numeric_limits<int64_t>::max();
    if (vehicle_span_upper_bounds_[vehicle] <
        std::numeric_limits<int64_t>::max()) {
      const int64_t span_max = vehicle_span_upper_bounds_[vehicle];
      slack_max = std::min(slack_max, CapSub(span_max, total_transit));
    }
    const int64_t max_start_from_min_end = ComputePathMaxStartFromEndCumul(
        delta_path_transits_, path, path_start, min_end);
    const int64_t span_lb = CapSub(min_end, max_start_from_min_end);
    int64_t min_total_slack = CapSub(span_lb, total_transit);
    if (min_total_slack > slack_max) return false;

    if (dimension_.HasBreakConstraints()) {
      for (const auto [limit, min_break_duration] :
           dimension_.GetBreakDistanceDurationOfVehicle(vehicle)) {
        // Minimal number of breaks depends on total transit:
        // 0 breaks for 0 <= total transit <= limit,
        // 1 break for limit + 1 <= total transit <= 2 * limit,
        // i breaks for i * limit + 1 <= total transit <= (i+1) * limit, ...
        if (limit == 0 || total_transit == 0) continue;
        const int num_breaks_lb = (total_transit - 1) / limit;
        const int64_t slack_lb = CapProd(num_breaks_lb, min_break_duration);
        if (slack_lb > slack_max) return false;
        min_total_slack = std::max(min_total_slack, slack_lb);
      }
      // Compute a lower bound of the amount of break that must be made inside
      // the route. We compute a mandatory interval (might be empty)
      // [max_start, min_end[ during which the route will have to happen,
      // then the duration of break that must happen during this interval.
      int64_t min_total_break = 0;
      int64_t max_path_end = cumuls_[routing_model_.End(vehicle)]->Max();
      const int64_t max_start = ComputePathMaxStartFromEndCumul(
          delta_path_transits_, path, path_start, max_path_end);
      for (const IntervalVar* br :
           dimension_.GetBreakIntervalsOfVehicle(vehicle)) {
        if (!br->MustBePerformed()) continue;
        if (max_start < br->EndMin() && br->StartMax() < min_end) {
          min_total_break = CapAdd(min_total_break, br->DurationMin());
        }
      }
      if (min_total_break > slack_max) return false;
      min_total_slack = std::max(min_total_slack, min_total_break);
    }
    if (filter_vehicle_costs) {
      cumul_cost_delta = CapAdd(
          cumul_cost_delta,
          CapProd(vehicle_span_cost_coefficients_[vehicle], min_total_slack));
      const int64_t span_lower_bound = CapAdd(total_transit, min_total_slack);
      if (FilterSoftSpanCost()) {
        const SimpleBoundCosts::BoundCost bound_cost =
            dimension_.GetSoftSpanUpperBoundForVehicle(vehicle);
        if (bound_cost.bound < span_lower_bound) {
          const int64_t violation = CapSub(span_lower_bound, bound_cost.bound);
          cumul_cost_delta =
              CapAdd(cumul_cost_delta, CapProd(bound_cost.cost, violation));
        }
      }
      if (FilterSoftSpanQuadraticCost()) {
        const SimpleBoundCosts::BoundCost bound_cost =
            dimension_.GetQuadraticCostSoftSpanUpperBoundForVehicle(vehicle);
        if (bound_cost.bound < span_lower_bound) {
          const int64_t violation = CapSub(span_lower_bound, bound_cost.bound);
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

bool PathCumulFilter::FinalizeAcceptPath(int64_t objective_min,
                                         int64_t objective_max) {
  if ((!FilterSpanCost() && !FilterCumulSoftBounds() && !FilterSlackCost() &&
       !FilterCumulSoftLowerBounds() && !FilterCumulPiecewiseLinearCosts() &&
       !FilterPrecedences() && !FilterSoftSpanCost() &&
       !FilterSoftSpanQuadraticCost()) ||
      lns_detected_) {
    return true;
  }
  if (FilterPrecedences()) {
    for (int64_t node : delta_nodes_with_precedences_and_changed_cumul_
                            .PositionsSetAtLeastOnce()) {
      const std::pair<int64_t, int64_t> node_min_max_cumul_in_delta =
          gtl::FindWithDefault(node_with_precedence_to_delta_min_max_cumuls_,
                               node, {-1, -1});
      // NOTE: This node was seen in delta, so its delta min/max cumul should be
      // stored in the map.
      DCHECK(node_min_max_cumul_in_delta.first >= 0 &&
             node_min_max_cumul_in_delta.second >= 0);
      for (const RoutingDimension::NodePrecedence& precedence :
           node_index_to_precedences_[node]) {
        const bool node_is_first = (precedence.first_node == node);
        const int64_t other_node =
            node_is_first ? precedence.second_node : precedence.first_node;
        if (GetNext(other_node) == kUnassigned ||
            GetNext(other_node) == other_node) {
          // The other node is unperformed, so the precedence constraint is
          // inactive.
          continue;
        }
        // max_cumul[second_node] should be greater or equal than
        // min_cumul[first_node] + offset.
        const std::pair<int64_t, int64_t>& other_min_max_cumul_in_delta =
            gtl::FindWithDefault(node_with_precedence_to_delta_min_max_cumuls_,
                                 other_node,
                                 current_min_max_node_cumuls_[other_node]);

        const int64_t first_min_cumul =
            node_is_first ? node_min_max_cumul_in_delta.first
                          : other_min_max_cumul_in_delta.first;
        const int64_t second_max_cumul =
            node_is_first ? other_min_max_cumul_in_delta.second
                          : node_min_max_cumul_in_delta.second;

        if (second_max_cumul < first_min_cumul + precedence.offset) {
          return false;
        }
      }
    }
  }
  int64_t new_max_end = delta_max_end_cumul_;
  int64_t new_min_start = std::numeric_limits<int64_t>::max();
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
    std::vector<int64_t> path_delta_cost_values(num_touched_paths, 0);
    std::vector<bool> requires_mp(num_touched_paths, false);
    for (int i = 0; i < num_touched_paths; ++i) {
      const int64_t start = GetTouchedPathStarts()[i];
      const int vehicle = start_to_vehicle_[start];
      if (!FilterWithDimensionCumulOptimizerForVehicle(vehicle)) {
        continue;
      }
      int64_t path_delta_cost_with_lp = 0;
      const DimensionSchedulingStatus status =
          optimizer_->ComputeRouteCumulCostWithoutFixedTransits(
              vehicle, [this](int64_t node) { return GetNext(node); },
              &path_delta_cost_with_lp);
      if (status == DimensionSchedulingStatus::INFEASIBLE) {
        return false;
      }
      DCHECK(gtl::ContainsKey(delta_paths_, GetPath(start)));
      const int64_t path_cost_diff_with_lp = CapSub(
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
      DCHECK_NE(mp_optimizer_, nullptr);
      requires_mp[i] =
          FilterBreakCost(vehicle) ||
          (status == DimensionSchedulingStatus::RELAXED_OPTIMAL_ONLY);
    }

    DCHECK_LE(accepted_objective_value_, objective_max);

    for (int i = 0; i < num_touched_paths; ++i) {
      if (!requires_mp[i]) {
        continue;
      }
      const int64_t start = GetTouchedPathStarts()[i];
      const int vehicle = start_to_vehicle_[start];
      int64_t path_delta_cost_with_mp = 0;
      if (mp_optimizer_->ComputeRouteCumulCostWithoutFixedTransits(
              vehicle, [this](int64_t node) { return GetNext(node); },
              &path_delta_cost_with_mp) ==
          DimensionSchedulingStatus::INFEASIBLE) {
        return false;
      }
      DCHECK(gtl::ContainsKey(delta_paths_, GetPath(start)));
      const int64_t path_cost_diff_with_mp =
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
    SupportedPathCumul* supported_cumul, int64_t default_value) {
  supported_cumul->cumul_value = default_value;
  supported_cumul->cumul_value_support = -1;
  supported_cumul->path_values.resize(NumPaths(), default_value);
}

bool PathCumulFilter::PickupToDeliveryLimitsRespected(
    const PathTransits& path_transits, int path,
    const std::vector<int64_t>& min_path_cumuls) const {
  if (!dimension_.HasPickupToDeliveryLimits()) {
    return true;
  }
  const int num_pairs = routing_model_.GetPickupAndDeliveryPairs().size();
  DCHECK_GT(num_pairs, 0);
  std::vector<std::pair<int, int64_t>> visited_delivery_and_min_cumul_per_pair(
      num_pairs, {-1, -1});

  const int path_size = path_transits.PathSize(path);
  CHECK_EQ(min_path_cumuls.size(), path_size);

  int64_t max_cumul = min_path_cumuls.back();
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
      const int64_t cumul_diff_limit =
          dimension_.GetPickupToDeliveryLimitForPair(
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
      std::pair<int, int64_t>& delivery_index_and_cumul =
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
    int path, const std::vector<int64_t>& min_path_cumuls, bool is_delta) {
  const PathTransits& path_transits =
      is_delta ? delta_path_transits_ : current_path_transits_;

  const int path_size = path_transits.PathSize(path);
  DCHECK_EQ(min_path_cumuls.size(), path_size);

  int64_t max_cumul = cumuls_[path_transits.Node(path, path_size - 1)]->Max();
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

    std::pair<int64_t, int64_t>& min_max_cumuls =
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

int64_t PathCumulFilter::ComputePathMaxStartFromEndCumul(
    const PathTransits& path_transits, int path, int64_t path_start,
    int64_t min_end_cumul) const {
  int64_t cumul_from_min_end = min_end_cumul;
  int64_t cumul_from_max_end =
      cumuls_[routing_model_.End(start_to_vehicle_[path_start])]->Max();
  for (int i = path_transits.PathSize(path) - 2; i >= 0; --i) {
    const int64_t transit = path_transits.Transit(path, i);
    const int64_t node = path_transits.Node(path, i);
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
  for (const int64_t coefficient : dimension.vehicle_span_cost_coefficients()) {
    if (coefficient != 0) return true;
  }
  for (int i = 0; i < dimension.cumuls().size(); ++i) {
    if (dimension.HasCumulVarSoftUpperBound(i)) return true;
    if (dimension.HasCumulVarSoftLowerBound(i)) return true;
    if (dimension.HasCumulVarPiecewiseLinearCost(i)) return true;
  }
  return false;
}

bool DimensionHasPathCumulConstraint(const RoutingDimension& dimension) {
  if (dimension.HasBreakConstraints()) return true;
  if (dimension.HasPickupToDeliveryLimits()) return true;
  for (const int64_t upper_bound : dimension.vehicle_span_upper_bounds()) {
    if (upper_bound != std::numeric_limits<int64_t>::max()) return true;
  }
  for (const IntVar* const slack : dimension.slacks()) {
    if (slack->Min() > 0) return true;
  }
  const std::vector<IntVar*>& cumuls = dimension.cumuls();
  for (int i = 0; i < cumuls.size(); ++i) {
    IntVar* const cumul_var = cumuls[i];
    if (cumul_var->Min() > 0 &&
        cumul_var->Max() < std::numeric_limits<int64_t>::max() &&
        !dimension.model()->IsEnd(i)) {
      return true;
    }
    if (dimension.forbidden_intervals()[i].NumIntervals() > 0) return true;
  }
  return false;
}

}  // namespace

void AppendLightWeightDimensionFilters(
    const PathState* path_state,
    const std::vector<RoutingDimension*>& dimensions,
    std::vector<LocalSearchFilterManager::FilterEvent>* filters) {
  // For every dimension that fits, add a UnaryDimensionChecker.
  for (const RoutingDimension* dimension : dimensions) {
    // Skip dimension if not unary.
    if (dimension->GetUnaryTransitEvaluator(0) == nullptr) continue;

    using Intervals = std::vector<UnaryDimensionChecker::Interval>;
    // Fill path capacities and classes.
    const int num_vehicles = dimension->model()->vehicles();
    Intervals path_capacity(num_vehicles);
    std::vector<int> path_class(num_vehicles);
    for (int v = 0; v < num_vehicles; ++v) {
      const auto& vehicle_capacities = dimension->vehicle_capacities();
      path_capacity[v] = {0, vehicle_capacities[v]};
      path_class[v] = dimension->vehicle_to_class(v);
    }
    // For each class, retrieve the demands of each node.
    // Dimension store evaluators with a double indirection for compacity:
    // vehicle -> vehicle_class -> evaluator_index.
    // We replicate this in UnaryDimensionChecker,
    // except we expand evaluator_index to an array of values for all nodes.
    const int num_vehicle_classes =
        1 + *std::max_element(path_class.begin(), path_class.end());
    std::vector<Intervals> demands(num_vehicle_classes);
    const int num_cumuls = dimension->cumuls().size();
    const int num_slacks = dimension->slacks().size();
    for (int vehicle = 0; vehicle < num_vehicles; ++vehicle) {
      const int vehicle_class = path_class[vehicle];
      if (!demands[vehicle_class].empty()) continue;
      const auto& evaluator = dimension->GetUnaryTransitEvaluator(vehicle);
      Intervals class_demands(num_cumuls);
      for (int node = 0; node < num_cumuls; ++node) {
        if (node < num_slacks) {
          const int64_t demand_min = evaluator(node);
          const int64_t slack_max = dimension->SlackVar(node)->Max();
          class_demands[node] = {demand_min, CapAdd(demand_min, slack_max)};
        } else {
          class_demands[node] = {0, 0};
        }
      }
      demands[vehicle_class] = std::move(class_demands);
    }
    // Fill node capacities.
    Intervals node_capacity(num_cumuls);
    for (int node = 0; node < num_cumuls; ++node) {
      const IntVar* cumul = dimension->CumulVar(node);
      node_capacity[node] = {cumul->Min(), cumul->Max()};
    }
    // Make the dimension checker and pass ownership to the filter.
    auto checker = absl::make_unique<UnaryDimensionChecker>(
        path_state, std::move(path_capacity), std::move(path_class),
        std::move(demands), std::move(node_capacity));
    const auto kAccept = LocalSearchFilterManager::FilterEventType::kAccept;
    LocalSearchFilter* filter = MakeUnaryDimensionFilter(
        dimension->model()->solver(), std::move(checker), dimension->name());
    filters->push_back({filter, kAccept});
  }
}

void AppendDimensionCumulFilters(
    const std::vector<RoutingDimension*>& dimensions,
    const RoutingSearchParameters& parameters, bool filter_objective_cost,
    bool filter_light_weight_unary_dimensions,
    std::vector<LocalSearchFilterManager::FilterEvent>* filters) {
  const auto kAccept = LocalSearchFilterManager::FilterEventType::kAccept;
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
  std::vector<bool> use_resource_assignment_filter(num_dimensions);
  std::vector<int> filtering_difficulty(num_dimensions);
  for (int d = 0; d < num_dimensions; d++) {
    const RoutingDimension& dimension = *dimensions[d];
    const bool has_cumul_cost = DimensionHasCumulCost(dimension);
    use_path_cumul_filter[d] =
        has_cumul_cost || DimensionHasPathCumulConstraint(dimension);

    const int num_dimension_resource_groups =
        dimension.model()->GetDimensionResourceGroupIndices(&dimension).size();
    const bool can_use_cumul_bounds_propagator_filter =
        !dimension.HasBreakConstraints() &&
        num_dimension_resource_groups == 0 &&
        (!filter_objective_cost || !has_cumul_cost);
    const bool has_precedences = !dimension.GetNodePrecedences().empty();
    use_global_lp_filter[d] =
        (has_precedences && !can_use_cumul_bounds_propagator_filter) ||
        (filter_objective_cost &&
         dimension.global_span_cost_coefficient() > 0) ||
        num_dimension_resource_groups > 1;

    use_cumul_bounds_propagator_filter[d] =
        has_precedences && !use_global_lp_filter[d];

    use_resource_assignment_filter[d] = num_dimension_resource_groups > 0;

    filtering_difficulty[d] =
        8 * use_global_lp_filter[d] + 4 * use_resource_assignment_filter[d] +
        2 * use_cumul_bounds_propagator_filter[d] + use_path_cumul_filter[d];
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
    const bool filter_resource_assignment = use_resource_assignment_filter[d];
    if (use_path_cumul_filter[d]) {
      filters->push_back(
          {MakePathCumulFilter(dimension, parameters,
                               /*propagate_own_objective_value*/
                               !use_global_lp && !filter_resource_assignment,
                               filter_objective_cost),
           kAccept});
    } else if (filter_light_weight_unary_dimensions ||
               dimension.GetUnaryTransitEvaluator(0) == nullptr) {
      filters->push_back(
          {model.solver()->RevAlloc(new ChainCumulFilter(model, dimension)),
           kAccept});
    }

    if (use_cumul_bounds_propagator_filter[d]) {
      DCHECK(!use_global_lp);
      DCHECK(!filter_resource_assignment);
      filters->push_back({MakeCumulBoundsPropagatorFilter(dimension), kAccept});
    }

    if (filter_resource_assignment) {
      filters->push_back({MakeResourceAssignmentFilter(
          model.GetMutableLocalCumulOptimizer(dimension),
          model.GetMutableLocalCumulMPOptimizer(dimension),
          /*propagate_own_objective_value*/ !use_global_lp,
          filter_objective_cost)});
    }

    if (use_global_lp) {
      DCHECK(model.GetMutableGlobalCumulOptimizer(dimension) != nullptr);
      filters->push_back({MakeGlobalLPCumulFilter(
                              model.GetMutableGlobalCumulOptimizer(dimension),
                              model.GetMutableGlobalCumulMPOptimizer(dimension),
                              filter_objective_cost),
                          kAccept});
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
  bool AcceptPath(int64_t path_start, int64_t chain_start,
                  int64_t chain_end) override;
  std::string DebugString() const override { return "PickupDeliveryFilter"; }

 private:
  bool AcceptPathDefault(int64_t path_start);
  template <bool lifo>
  bool AcceptPathOrdered(int64_t path_start);

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

bool PickupDeliveryFilter::AcceptPath(int64_t path_start, int64_t chain_start,
                                      int64_t chain_end) {
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

bool PickupDeliveryFilter::AcceptPathDefault(int64_t path_start) {
  visited_.ClearAll();
  int64_t node = path_start;
  int64_t path_length = 1;
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
    const int64_t next = GetNext(node);
    if (next == kUnassigned) {
      // LNS detected, return true since path was ok up to now.
      return true;
    }
    node = next;
    ++path_length;
  }
  for (const int64_t node : visited_.PositionsSetAtLeastOnce()) {
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
bool PickupDeliveryFilter::AcceptPathOrdered(int64_t path_start) {
  visited_deque_.clear();
  int64_t node = path_start;
  int64_t path_length = 1;
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
    const int64_t next = GetNext(node);
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
  bool AcceptPath(int64_t path_start, int64_t chain_start,
                  int64_t chain_end) override;
  std::string DebugString() const override { return "VehicleVariableFilter"; }

 private:
  bool DisableFiltering() const override;
  bool IsVehicleVariableConstrained(int index) const;

  std::vector<int64_t> start_to_vehicle_;
  std::vector<IntVar*> vehicle_vars_;
  const int64_t unconstrained_vehicle_var_domain_size_;
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

bool VehicleVarFilter::AcceptPath(int64_t path_start, int64_t chain_start,
                                  int64_t chain_end) {
  const int64_t vehicle = start_to_vehicle_[path_start];
  int64_t node = chain_start;
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
              int64_t objective_min, int64_t objective_max) override;
  std::string DebugString() const override {
    return "CumulBoundsPropagatorFilter(" + propagator_.dimension().name() +
           ")";
  }

 private:
  CumulBoundsPropagator propagator_;
  const int64_t cumul_offset_;
  SparseBitset<int64_t> delta_touched_;
  std::vector<int64_t> delta_nexts_;
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
                                         int64_t objective_min,
                                         int64_t objective_max) {
  delta_touched_.ClearAll();
  for (const IntVarElement& delta_element :
       delta->IntVarContainer().elements()) {
    int64_t index = -1;
    if (FindIndex(delta_element.Var(), &index)) {
      if (!delta_element.Bound()) {
        // LNS detected
        return true;
      }
      delta_touched_.Set(index);
      delta_nexts_[index] = delta_element.Value();
    }
  }
  const auto& next_accessor = [this](int64_t index) {
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
                GlobalDimensionCumulOptimizer* mp_optimizer,
                bool filter_objective_cost);
  bool Accept(const Assignment* delta, const Assignment* deltadelta,
              int64_t objective_min, int64_t objective_max) override;
  int64_t GetAcceptedObjectiveValue() const override;
  void OnSynchronize(const Assignment* delta) override;
  int64_t GetSynchronizedObjectiveValue() const override;
  std::string DebugString() const override {
    return "LPCumulFilter(" + optimizer_.dimension()->name() + ")";
  }

 private:
  GlobalDimensionCumulOptimizer& optimizer_;
  GlobalDimensionCumulOptimizer& mp_optimizer_;
  const bool filter_objective_cost_;
  int64_t synchronized_cost_without_transit_;
  int64_t delta_cost_without_transit_;
  SparseBitset<int64_t> delta_touched_;
  std::vector<int64_t> delta_nexts_;
};

LPCumulFilter::LPCumulFilter(const std::vector<IntVar*>& nexts,
                             GlobalDimensionCumulOptimizer* optimizer,
                             GlobalDimensionCumulOptimizer* mp_optimizer,
                             bool filter_objective_cost)
    : IntVarLocalSearchFilter(nexts),
      optimizer_(*optimizer),
      mp_optimizer_(*mp_optimizer),
      filter_objective_cost_(filter_objective_cost),
      synchronized_cost_without_transit_(-1),
      delta_cost_without_transit_(-1),
      delta_touched_(Size()),
      delta_nexts_(Size()) {}

bool LPCumulFilter::Accept(const Assignment* delta,
                           const Assignment* deltadelta, int64_t objective_min,
                           int64_t objective_max) {
  delta_touched_.ClearAll();
  for (const IntVarElement& delta_element :
       delta->IntVarContainer().elements()) {
    int64_t index = -1;
    if (FindIndex(delta_element.Var(), &index)) {
      if (!delta_element.Bound()) {
        // LNS detected
        return true;
      }
      delta_touched_.Set(index);
      delta_nexts_[index] = delta_element.Value();
    }
  }
  const auto& next_accessor = [this](int64_t index) {
    return delta_touched_[index] ? delta_nexts_[index] : Value(index);
  };

  if (!filter_objective_cost_) {
    // No need to compute the cost of the LP, only verify its feasibility.
    delta_cost_without_transit_ = 0;
    const DimensionSchedulingStatus status =
        optimizer_.ComputeCumuls(next_accessor, nullptr, nullptr, nullptr);
    if (status == DimensionSchedulingStatus::OPTIMAL) return true;
    if (status == DimensionSchedulingStatus::RELAXED_OPTIMAL_ONLY &&
        mp_optimizer_.ComputeCumuls(next_accessor, nullptr, nullptr, nullptr) ==
            DimensionSchedulingStatus::OPTIMAL) {
      return true;
    }
    return false;
  }

  const DimensionSchedulingStatus status =
      optimizer_.ComputeCumulCostWithoutFixedTransits(
          next_accessor, &delta_cost_without_transit_);
  if (status == DimensionSchedulingStatus::INFEASIBLE) {
    delta_cost_without_transit_ = std::numeric_limits<int64_t>::max();
    return false;
  }
  if (delta_cost_without_transit_ > objective_max) return false;

  if (status == DimensionSchedulingStatus::RELAXED_OPTIMAL_ONLY &&
      mp_optimizer_.ComputeCumulCostWithoutFixedTransits(
          next_accessor, &delta_cost_without_transit_) !=
          DimensionSchedulingStatus::OPTIMAL) {
    delta_cost_without_transit_ = std::numeric_limits<int64_t>::max();
    return false;
  }
  return delta_cost_without_transit_ <= objective_max;
}

int64_t LPCumulFilter::GetAcceptedObjectiveValue() const {
  return delta_cost_without_transit_;
}

void LPCumulFilter::OnSynchronize(const Assignment* delta) {
  // TODO(user): Try to optimize this so the LP is not called when the last
  // computed delta cost corresponds to the solution being synchronized.
  const RoutingModel& model = *optimizer_.dimension()->model();
  const auto& next_accessor = [this, &model](int64_t index) {
    return IsVarSynced(index)     ? Value(index)
           : model.IsStart(index) ? model.End(model.VehicleIndex(index))
                                  : index;
  };

  const DimensionSchedulingStatus status =
      optimizer_.ComputeCumulCostWithoutFixedTransits(
          next_accessor, &synchronized_cost_without_transit_);
  if (status == DimensionSchedulingStatus::INFEASIBLE) {
    // TODO(user): This should only happen if the LP solver times out.
    // DCHECK the fail wasn't due to an infeasible model.
    synchronized_cost_without_transit_ = 0;
  }
  if (status == DimensionSchedulingStatus::RELAXED_OPTIMAL_ONLY &&
      mp_optimizer_.ComputeCumulCostWithoutFixedTransits(
          next_accessor, &synchronized_cost_without_transit_) !=
          DimensionSchedulingStatus::OPTIMAL) {
    // TODO(user): This should only happen if the MP solver times out.
    // DCHECK the fail wasn't due to an infeasible model.
    synchronized_cost_without_transit_ = 0;
  }
}

int64_t LPCumulFilter::GetSynchronizedObjectiveValue() const {
  return synchronized_cost_without_transit_;
}

}  // namespace

IntVarLocalSearchFilter* MakeGlobalLPCumulFilter(
    GlobalDimensionCumulOptimizer* optimizer,
    GlobalDimensionCumulOptimizer* mp_optimizer, bool filter_objective_cost) {
  const RoutingModel& model = *optimizer->dimension()->model();
  return model.solver()->RevAlloc(new LPCumulFilter(
      model.Nexts(), optimizer, mp_optimizer, filter_objective_cost));
}

namespace {

using ResourceGroup = RoutingModel::ResourceGroup;

class ResourceGroupAssignmentFilter : public BasePathFilter {
 public:
  ResourceGroupAssignmentFilter(const std::vector<IntVar*>& nexts,
                                const ResourceGroup* resource_group,
                                LocalDimensionCumulOptimizer* optimizer,
                                LocalDimensionCumulOptimizer* mp_optimizer,
                                bool filter_objective_cost);
  bool InitializeAcceptPath() override;
  bool AcceptPath(int64_t path_start, int64_t chain_start,
                  int64_t chain_end) override;
  bool FinalizeAcceptPath(int64_t objective_min,
                          int64_t objective_max) override;
  void OnSynchronizePathFromStart(int64_t start) override;
  void OnAfterSynchronizePaths() override;

  int64_t GetAcceptedObjectiveValue() const override {
    return delta_cost_without_transit_;
  }
  int64_t GetSynchronizedObjectiveValue() const override {
    return synchronized_cost_without_transit_;
  }
  std::string DebugString() const override {
    return "ResourceGroupAssignmentFilter(" +
           resource_assignment_optimizer_.dimension()->name() + ")";
  }

 private:
  ResourceAssignmentOptimizer resource_assignment_optimizer_;
  const RoutingModel& model_;
  const ResourceGroup& resource_group_;
  const bool filter_objective_cost_;
  bool synch_timed_out_;
  int64_t synchronized_cost_without_transit_;
  int64_t delta_cost_without_transit_;
  std::vector<std::vector<int64_t>> vehicle_to_resource_assignment_costs_;
  std::vector<std::vector<int64_t>> delta_vehicle_to_resource_assignment_costs_;
};

ResourceGroupAssignmentFilter::ResourceGroupAssignmentFilter(
    const std::vector<IntVar*>& nexts, const ResourceGroup* resource_group,
    LocalDimensionCumulOptimizer* optimizer,
    LocalDimensionCumulOptimizer* mp_optimizer, bool filter_objective_cost)
    : BasePathFilter(nexts, optimizer->dimension()->cumuls().size()),
      resource_assignment_optimizer_(resource_group, optimizer, mp_optimizer),
      model_(*optimizer->dimension()->model()),
      resource_group_(*resource_group),
      filter_objective_cost_(filter_objective_cost),
      synch_timed_out_(false),
      synchronized_cost_without_transit_(-1),
      delta_cost_without_transit_(-1) {
  vehicle_to_resource_assignment_costs_.resize(model_.vehicles());
  delta_vehicle_to_resource_assignment_costs_.resize(model_.vehicles());
}

bool ResourceGroupAssignmentFilter::InitializeAcceptPath() {
  delta_vehicle_to_resource_assignment_costs_.assign(model_.vehicles(), {});
  // TODO(user): Keep track of num_used_vehicles internally and compute its
  // new value here by only going through the touched_paths_.
  int num_used_vehicles = 0;
  const int num_resources = resource_group_.Size();
  for (int v = 0; v < model_.vehicles(); v++) {
    if (GetNext(model_.Start(v)) != model_.End(v) ||
        model_.IsVehicleUsedWhenEmpty(v)) {
      if (++num_used_vehicles > num_resources) {
        return false;
      }
    }
  }
  return true;
}

bool ResourceGroupAssignmentFilter::AcceptPath(int64_t path_start,
                                               int64_t chain_start,
                                               int64_t chain_end) {
  const int vehicle = model_.VehicleIndex(path_start);
  return resource_assignment_optimizer_.ComputeAssignmentCostsForVehicle(
      vehicle, [this](int64_t index) { return GetNext(index); },
      filter_objective_cost_,
      &delta_vehicle_to_resource_assignment_costs_[vehicle], nullptr, nullptr);
}

bool ResourceGroupAssignmentFilter::FinalizeAcceptPath(int64_t objective_min,
                                                       int64_t objective_max) {
  delta_cost_without_transit_ =
      resource_assignment_optimizer_.ComputeBestAssignmentCost(
          delta_vehicle_to_resource_assignment_costs_,
          vehicle_to_resource_assignment_costs_,
          [this](int v) { return PathStartTouched(model_.Start(v)); }, nullptr);
  return delta_cost_without_transit_ >= 0 &&
         delta_cost_without_transit_ <= objective_max;
}

void ResourceGroupAssignmentFilter::OnSynchronizePathFromStart(int64_t start) {
  if (synch_timed_out_) return;
  // NOTE(user): Even if filter_objective_cost_ is false, we still need to
  // call ComputeAssignmentCostsForVehicle() for every vehicle to keep track
  // of whether or not a given vehicle-to-resource assignment is possible by
  // storing 0 or -1 in vehicle_to_resource_assignment_costs_.
  const auto& next_accessor = [this](int64_t index) {
    return IsVarSynced(index)      ? Value(index)
           : model_.IsStart(index) ? model_.End(model_.VehicleIndex(index))
                                   : index;
  };
  const int v = model_.VehicleIndex(start);
  if (!resource_assignment_optimizer_.ComputeAssignmentCostsForVehicle(
          v, next_accessor, filter_objective_cost_,
          &vehicle_to_resource_assignment_costs_[v], nullptr, nullptr)) {
    // A timeout was reached.
    synch_timed_out_ = true;
  }
}

void ResourceGroupAssignmentFilter::OnAfterSynchronizePaths() {
  synchronized_cost_without_transit_ =
      (synch_timed_out_ || !filter_objective_cost_)
          ? 0
          : resource_assignment_optimizer_.ComputeBestAssignmentCost(
                vehicle_to_resource_assignment_costs_,
                vehicle_to_resource_assignment_costs_, [](int) { return true; },
                nullptr);
  DCHECK_GE(synchronized_cost_without_transit_, 0);
}

// ResourceAssignmentFilter
class ResourceAssignmentFilter : public LocalSearchFilter {
 public:
  ResourceAssignmentFilter(const std::vector<IntVar*>& nexts,
                           LocalDimensionCumulOptimizer* optimizer,
                           LocalDimensionCumulOptimizer* mp_optimizer,
                           bool propagate_own_objective_value,
                           bool filter_objective_cost);
  bool Accept(const Assignment* delta, const Assignment* deltadelta,
              int64_t objective_min, int64_t objective_max) override;
  void Synchronize(const Assignment* assignment,
                   const Assignment* delta) override;

  int64_t GetAcceptedObjectiveValue() const override {
    return propagate_own_objective_value_ ? delta_cost_ : 0;
  }
  int64_t GetSynchronizedObjectiveValue() const override {
    return propagate_own_objective_value_ ? synchronized_cost_ : 0;
  }
  std::string DebugString() const override {
    return "ResourceAssignmentFilter(" + dimension_name_ + ")";
  }

 private:
  std::vector<IntVarLocalSearchFilter*> resource_group_assignment_filters_;
  int64_t synchronized_cost_;
  int64_t delta_cost_;
  const bool propagate_own_objective_value_;
  const std::string dimension_name_;
};

ResourceAssignmentFilter::ResourceAssignmentFilter(
    const std::vector<IntVar*>& nexts, LocalDimensionCumulOptimizer* optimizer,
    LocalDimensionCumulOptimizer* mp_optimizer,
    bool propagate_own_objective_value, bool filter_objective_cost)
    : propagate_own_objective_value_(propagate_own_objective_value),
      dimension_name_(optimizer->dimension()->name()) {
  const RoutingModel& model = *optimizer->dimension()->model();
  for (const auto& resource_group : model.GetResourceGroups()) {
    resource_group_assignment_filters_.push_back(
        model.solver()->RevAlloc(new ResourceGroupAssignmentFilter(
            nexts, resource_group.get(), optimizer, mp_optimizer,
            filter_objective_cost)));
  }
}

bool ResourceAssignmentFilter::Accept(const Assignment* delta,
                                      const Assignment* deltadelta,
                                      int64_t objective_min,
                                      int64_t objective_max) {
  delta_cost_ = 0;
  for (LocalSearchFilter* group_filter : resource_group_assignment_filters_) {
    if (!group_filter->Accept(delta, deltadelta, objective_min,
                              objective_max)) {
      return false;
    }
    delta_cost_ =
        std::max(delta_cost_, group_filter->GetAcceptedObjectiveValue());
    DCHECK_LE(delta_cost_, objective_max)
        << "ResourceGroupAssignmentFilter should return false when the "
           "objective_max is exceeded.";
  }
  return true;
}

void ResourceAssignmentFilter::Synchronize(const Assignment* assignment,
                                           const Assignment* delta) {
  synchronized_cost_ = 0;
  for (LocalSearchFilter* group_filter : resource_group_assignment_filters_) {
    group_filter->Synchronize(assignment, delta);
    synchronized_cost_ = std::max(
        synchronized_cost_, group_filter->GetSynchronizedObjectiveValue());
  }
}

}  // namespace

LocalSearchFilter* MakeResourceAssignmentFilter(
    LocalDimensionCumulOptimizer* optimizer,
    LocalDimensionCumulOptimizer* mp_optimizer,
    bool propagate_own_objective_value, bool filter_objective_cost) {
  const RoutingModel& model = *optimizer->dimension()->model();
  return model.solver()->RevAlloc(new ResourceAssignmentFilter(
      model.Nexts(), optimizer, mp_optimizer, propagate_own_objective_value,
      filter_objective_cost));
}

namespace {

// This filter accepts deltas for which the assignment satisfies the
// constraints of the Solver. This is verified by keeping an internal copy of
// the assignment with all Next vars and their updated values, and calling
// RestoreAssignment() on the assignment+delta.
// TODO(user): Also call the solution finalizer on variables, with the
// exception of Next Vars (woud fail on large instances).
// WARNING: In the case of mandatory nodes, when all vehicles are currently
// being used in the solution but uninserted nodes still remain, this filter
// will reject the solution, even if the node could be inserted on one of these
// routes, because all Next vars of vehicle starts are already instantiated.
// TODO(user): Avoid such false negatives.

class CPFeasibilityFilter : public IntVarLocalSearchFilter {
 public:
  explicit CPFeasibilityFilter(RoutingModel* routing_model);
  ~CPFeasibilityFilter() override {}
  std::string DebugString() const override { return "CPFeasibilityFilter"; }
  bool Accept(const Assignment* delta, const Assignment* deltadelta,
              int64_t objective_min, int64_t objective_max) override;
  void OnSynchronize(const Assignment* delta) override;

 private:
  void AddDeltaToAssignment(const Assignment* delta, Assignment* assignment);

  static const int64_t kUnassigned;
  const RoutingModel* const model_;
  Solver* const solver_;
  Assignment* const assignment_;
  Assignment* const temp_assignment_;
  DecisionBuilder* const restore_;
  SearchLimit* const limit_;
};

const int64_t CPFeasibilityFilter::kUnassigned = -1;

CPFeasibilityFilter::CPFeasibilityFilter(RoutingModel* routing_model)
    : IntVarLocalSearchFilter(routing_model->Nexts()),
      model_(routing_model),
      solver_(routing_model->solver()),
      assignment_(solver_->MakeAssignment()),
      temp_assignment_(solver_->MakeAssignment()),
      restore_(solver_->MakeRestoreAssignment(temp_assignment_)),
      limit_(solver_->MakeCustomLimit(
          [routing_model]() { return routing_model->CheckLimit(); })) {
  assignment_->Add(routing_model->Nexts());
}

bool CPFeasibilityFilter::Accept(const Assignment* delta,
                                 const Assignment* deltadelta,
                                 int64_t objective_min, int64_t objective_max) {
  temp_assignment_->Copy(assignment_);
  AddDeltaToAssignment(delta, temp_assignment_);

  return solver_->Solve(restore_, limit_);
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
    int64_t index = kUnassigned;
    CHECK(FindIndex(var, &index));
    DCHECK_EQ(var, Var(index));
    const int64_t value = delta_element.Value();

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

}  // namespace

IntVarLocalSearchFilter* MakeCPFeasibilityFilter(RoutingModel* routing_model) {
  return routing_model->solver()->RevAlloc(
      new CPFeasibilityFilter(routing_model));
}

// TODO(user): Implement same-vehicle filter. Could be merged with node
// precedence filter.

}  // namespace operations_research
