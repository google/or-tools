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

#ifndef OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_FILTERS_H_
#define OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_FILTERS_H_

#include <utility>
#include <vector>

#include "ortools/base/integral_types.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "ortools/constraint_solver/routing.h"
#include "ortools/constraint_solver/routing_lp_scheduling.h"
#include "ortools/constraint_solver/routing_parameters.pb.h"
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
    const RoutingModel& routing_model, const RoutingModel::IndexPairs& pairs,
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
  BasePathFilter(const std::vector<IntVar*>& nexts, int next_domain_size);
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
  int NumPaths() const { return starts_.size(); }
  int64_t Start(int i) const { return starts_[i]; }
  int GetPath(int64_t node) const { return paths_[node]; }
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
  virtual void OnSynchronizePathFromStart(int64_t start) {}
  virtual bool InitializeAcceptPath() { return true; }
  virtual bool AcceptPath(int64_t path_start, int64_t chain_start,
                          int64_t chain_end) = 0;
  virtual bool FinalizeAcceptPath(int64_t objective_min,
                                  int64_t objective_max) {
    return true;
  }
  /// Detects path starts, used to track which node belongs to which path.
  void ComputePathStarts(std::vector<int64_t>* path_starts,
                         std::vector<int>* index_to_path);
  bool HavePathsChanged();
  void SynchronizeFullAssignment();
  void UpdateAllRanks();
  void UpdatePathRanksFromStart(int start);

  std::vector<int64_t> node_path_starts_;
  std::vector<int64_t> starts_;
  std::vector<int> paths_;
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
