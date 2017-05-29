// Copyright 2010-2014 Google
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

#include <cstdlib>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include "ortools/base/small_map.h"
#include "ortools/base/small_ordered_set.h"
#include "ortools/constraint_solver/routing.h"
#include "ortools/graph/christofides.h"
#include "ortools/util/bitset.h"
#include "ortools/util/saturated_arithmetic.h"

DEFINE_bool(routing_strong_debug_checks, false,
            "Run stronger checks in debug; these stronger tests might change "
            "the complexity of the code in particular.");
DEFINE_bool(routing_shift_insertion_cost_by_penalty, true,
            "Shift insertion costs by the penalty of the inserted node(s).");

namespace operations_research {

// --- Routing-specific local search filters ---

// RoutingLocalSearchFilter

RoutingLocalSearchFilter::RoutingLocalSearchFilter(
    const std::vector<IntVar*>& nexts,
    Solver::ObjectiveWatcher objective_callback)
    : IntVarLocalSearchFilter(nexts),
      injected_objective_value_(0),
      objective_callback_(std::move(objective_callback)) {}

void RoutingLocalSearchFilter::InjectObjectiveValue(int64 objective_value) {
  injected_objective_value_ = objective_value;
}

void RoutingLocalSearchFilter::PropagateObjectiveValue(int64 objective_value) {
  if (objective_callback_ != nullptr) {
    objective_callback_(objective_value);
  }
}

namespace {

// Node disjunction filter class.

class NodeDisjunctionFilter : public RoutingLocalSearchFilter {
 public:
  NodeDisjunctionFilter(const RoutingModel& routing_model,
                        Solver::ObjectiveWatcher objective_callback)
      : RoutingLocalSearchFilter(routing_model.Nexts(),
                                 std::move(objective_callback)),
        routing_model_(routing_model),
        active_per_disjunction_(routing_model.GetNumberOfDisjunctions(), 0),
        inactive_per_disjunction_(routing_model.GetNumberOfDisjunctions(), 0),
        penalty_value_(0) {}

  bool Accept(const Assignment* delta, const Assignment* deltadelta) override {
    const int64 kUnassigned = -1;
    const Assignment::IntContainer& container = delta->IntVarContainer();
    const int delta_size = container.Size();
    small_map<std::map<RoutingModel::DisjunctionIndex, int>>
        disjunction_active_deltas;
    small_map<std::map<RoutingModel::DisjunctionIndex, int>>
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
             routing_model_.GetDisjunctionIndicesFromVariableIndex(index)) {
          const bool active_state_changed =
              !IsVarSynced(index) || (Value(index) == index) != is_inactive;
          if (active_state_changed) {
            if (!is_inactive) {
              ++LookupOrInsert(&disjunction_active_deltas, disjunction_index,
                               0);
              if (IsVarSynced(index)) {
                --LookupOrInsert(&disjunction_inactive_deltas,
                                 disjunction_index, 0);
              }
            } else {
              ++LookupOrInsert(&disjunction_inactive_deltas, disjunction_index,
                               0);
              if (IsVarSynced(index)) {
                --LookupOrInsert(&disjunction_active_deltas, disjunction_index,
                                 0);
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
        PropagateObjectiveValue(0);
        return false;
      }
    }
    // Update penalty costs for disjunctions.
    int64 new_objective_value =
        CapAdd(injected_objective_value_, penalty_value_);
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
            PropagateObjectiveValue(0);
            return false;
          } else if (current_inactive_nodes <= max_inactive_cardinality) {
            // Add penalty if there were not too many inactive nodes before the
            // move.
            new_objective_value = CapAdd(new_objective_value, penalty);
          }
        } else if (current_inactive_nodes > max_inactive_cardinality) {
          // Remove penalty if there were too many inactive nodes before the
          // move and there are not too many after the move.
          new_objective_value = CapSub(new_objective_value, penalty);
        }
      }
    }

    PropagateObjectiveValue(new_objective_value);
    if (lns_detected) {
      return true;
    } else {
      IntVar* const cost_var = routing_model_.CostVar();
      // Only compare to max as a cost lower bound is computed.
      // TODO(user): Factor out the access to the objective upper bound.
      int64 cost_max = cost_var->Max();
      if (delta->Objective() == cost_var) {
        cost_max = std::min(cost_max, delta->ObjectiveMax());
      }
      return new_objective_value <= cost_max;
    }
  }
  std::string DebugString() const override { return "NodeDisjunctionFilter"; }

 private:
  void OnSynchronize(const Assignment* delta) override {
    penalty_value_ = 0;
    for (RoutingModel::DisjunctionIndex i(0);
         i < active_per_disjunction_.size(); ++i) {
      active_per_disjunction_[i] = 0;
      inactive_per_disjunction_[i] = 0;
      const std::vector<int>& disjunction_nodes =
          routing_model_.GetDisjunctionIndices(i);
      for (const int64 node : disjunction_nodes) {
        const bool node_synced = IsVarSynced(node);
        if (node_synced) {
          if (Value(node) != node) {
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
              disjunction_nodes.size() - max_cardinality &&
          penalty > 0) {
        penalty_value_ = CapAdd(penalty_value_, penalty);
      }
    }
    PropagateObjectiveValue(CapAdd(injected_objective_value_, penalty_value_));
  }

  const RoutingModel& routing_model_;

  ITIVector<RoutingModel::DisjunctionIndex, int> active_per_disjunction_;
  ITIVector<RoutingModel::DisjunctionIndex, int> inactive_per_disjunction_;
  int64 penalty_value_;
};
}  // namespace

RoutingLocalSearchFilter* MakeNodeDisjunctionFilter(
    const RoutingModel& routing_model,
    Solver::ObjectiveWatcher objective_callback) {
  return routing_model.solver()->RevAlloc(
      new NodeDisjunctionFilter(routing_model, std::move(objective_callback)));
}

const int64 BasePathFilter::kUnassigned = -1;

BasePathFilter::BasePathFilter(const std::vector<IntVar*>& nexts,
                               int next_domain_size,
                               Solver::ObjectiveWatcher objective_callback)
    : RoutingLocalSearchFilter(nexts, std::move(objective_callback)),
      node_path_starts_(next_domain_size, kUnassigned),
      paths_(nexts.size(), -1),
      new_nexts_(nexts.size(), kUnassigned),
      touched_paths_(nexts.size()),
      touched_path_nodes_(next_domain_size),
      ranks_(next_domain_size, -1) {}

bool BasePathFilter::Accept(const Assignment* delta,
                            const Assignment* deltadelta) {
  PropagateObjectiveValue(injected_objective_value_);
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
  // Order is important: FinalizeAcceptPath() must always be called.
  return FinalizeAcceptPath() && accept;
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
  PropagateObjectiveValue(injected_objective_value_);
  ComputePathStarts(&starts_, &paths_);
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
  if (delta == nullptr || delta->Empty() || starts_.empty()) {
    SynchronizeFullAssignment();
    return;
  }
  // Subclasses of BasePathFilter might not propagate injected objective values
  // so making sure it is done here (can be done again by the subclass if
  // needed).
  PropagateObjectiveValue(injected_objective_value_);
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

int64 GetNextValueFromForbiddenIntervals(
    int64 value, const SortedDisjointIntervalList& forbidden_intervals) {
  int64 next_value = value;
  const auto first_interval_it =
      forbidden_intervals.FirstIntervalGreaterOrEqual(next_value);
  if (first_interval_it != forbidden_intervals.end() &&
      next_value >= first_interval_it->start) {
    next_value = CapAdd(first_interval_it->end, 1);
  }
  return next_value;
}

// ChainCumul filter. Version of dimension path filter which is O(delta) rather
// than O(length of touched paths). Currently only supports dimensions without
// costs (global and local span cost, soft bounds) and with unconstrained
// cumul variables except overall capacity and cumul variables of path ends.

class ChainCumulFilter : public BasePathFilter {
 public:
  ChainCumulFilter(const RoutingModel& routing_model,
                   const RoutingDimension& dimension,
                   Solver::ObjectiveWatcher objective_callback);
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
  std::vector<const RoutingModel::TransitEvaluator2*> evaluators_;
  const std::vector<int64> vehicle_capacities_;
  std::vector<int64> current_path_cumul_mins_;
  std::vector<int64> current_max_of_path_end_cumul_mins_;
  std::vector<int64> old_nexts_;
  std::vector<int> old_vehicles_;
  std::vector<int64> current_transits_;
  const std::string name_;
};

ChainCumulFilter::ChainCumulFilter(const RoutingModel& routing_model,
                                   const RoutingDimension& dimension,
                                   Solver::ObjectiveWatcher objective_callback)
    : BasePathFilter(routing_model.Nexts(), dimension.cumuls().size(),
                     std::move(objective_callback)),
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
                  Solver::ObjectiveWatcher objective_callback);
  ~PathCumulFilter() override {}
  std::string DebugString() const override {
    return "PathCumulFilter(" + name_ + ")";
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
  }
  bool AcceptPath(int64 path_start, int64 chain_start,
                  int64 chain_end) override;
  bool FinalizeAcceptPath() override;
  void OnBeforeSynchronizePaths() override;

  bool FilterSpanCost() const { return global_span_cost_coefficient_ != 0; }

  bool FilterSlackCost() const {
    return has_nonzero_vehicle_span_cost_coefficients_ ||
           has_vehicle_span_upper_bounds_;
  }

  bool FilterCumulSoftBounds() const { return !cumul_soft_bounds_.empty(); }

  int64 GetCumulSoftCost(int64 node, int64 cumul_value) const;

  bool FilterCumulPiecewiseLinearCosts() const {
    return !cumul_piecewise_linear_costs_.empty();
  }

  int64 GetCumulPiecewiseLinearCost(int64 node, int64 cumul_value) const;

  bool FilterCumulSoftLowerBounds() const {
    return !cumul_soft_lower_bounds_.empty();
  }

  int64 GetCumulSoftLowerBoundCost(int64 node, int64 cumul_value) const;

  int64 GetPathCumulSoftLowerBoundCost(const PathTransits& path_transits,
                                       int path) const;

  void InitializeSupportedPathCumul(SupportedPathCumul* supported_cumul,
                                    int64 default_value);

  // Compute the max start cumul value for a given path given an end cumul
  // value.
  int64 ComputePathMaxStartFromEndCumul(const PathTransits& path_transits,
                                        int path, int end_cumul) const;

  const std::vector<IntVar*> cumuls_;
  const std::vector<SortedDisjointIntervalList>& forbidden_intervals_;
  const std::vector<IntVar*> slacks_;
  std::vector<int64> start_to_vehicle_;
  std::vector<const RoutingModel::TransitEvaluator2*> evaluators_;
  std::vector<int64> vehicle_span_upper_bounds_;
  bool has_vehicle_span_upper_bounds_;
  int64 total_current_cumul_cost_value_;
  // Map between paths and path soft cumul bound costs. The paths are indexed
  // by the index of the start node of the path.
  std::unordered_map<int64, int64> current_cumul_cost_values_;
  int64 cumul_cost_delta_;
  const int64 global_span_cost_coefficient_;
  std::vector<SoftBound> cumul_soft_bounds_;
  std::vector<SoftBound> cumul_soft_lower_bounds_;
  std::vector<const PiecewiseLinearFunction*> cumul_piecewise_linear_costs_;
  std::vector<int64> vehicle_span_cost_coefficients_;
  bool has_nonzero_vehicle_span_cost_coefficients_;
  IntVar* const cost_var_;
  const std::vector<int64> vehicle_capacities_;
  // Data reflecting information on paths and cumul variables for the solution
  // to which the filter was synchronized.
  SupportedPathCumul current_min_start_;
  SupportedPathCumul current_max_end_;
  PathTransits current_path_transits_;
  // Data reflecting information on paths and cumul variables for the "delta"
  // solution (aka neighbor solution) being examined.
  PathTransits delta_path_transits_;
  int64 delta_max_end_cumul_;
  // Note: small_ordered_set only support non-hash sets.
  small_ordered_set<std::set<int>> delta_paths_;
  const std::string name_;

  bool lns_detected_;
};

PathCumulFilter::PathCumulFilter(const RoutingModel& routing_model,
                                 const RoutingDimension& dimension,
                                 Solver::ObjectiveWatcher objective_callback)
    : BasePathFilter(routing_model.Nexts(), dimension.cumuls().size(),
                     std::move(objective_callback)),
      cumuls_(dimension.cumuls()),
      forbidden_intervals_(dimension.forbidden_intervals()),
      slacks_(dimension.slacks()),
      evaluators_(routing_model.vehicles(), nullptr),
      vehicle_span_upper_bounds_(dimension.vehicle_span_upper_bounds()),
      has_vehicle_span_upper_bounds_(false),
      total_current_cumul_cost_value_(0),
      current_cumul_cost_values_(),
      cumul_cost_delta_(0),
      global_span_cost_coefficient_(dimension.global_span_cost_coefficient()),
      vehicle_span_cost_coefficients_(
          dimension.vehicle_span_cost_coefficients()),
      has_nonzero_vehicle_span_cost_coefficients_(false),
      cost_var_(routing_model.CostVar()),
      vehicle_capacities_(dimension.vehicle_capacities()),
      delta_max_end_cumul_(kint64min),
      name_(dimension.name()),
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
    if (dimension.HasCumulVarSoftUpperBoundFromIndex(i)) {
      has_cumul_soft_bounds = true;
      cumul_soft_bounds_[i].bound =
          dimension.GetCumulVarSoftUpperBoundFromIndex(i);
      cumul_soft_bounds_[i].coefficient =
          dimension.GetCumulVarSoftUpperBoundCoefficientFromIndex(i);
    }
    if (dimension.HasCumulVarSoftLowerBoundFromIndex(i)) {
      has_cumul_soft_lower_bounds = true;
      cumul_soft_lower_bounds_[i].bound =
          dimension.GetCumulVarSoftLowerBoundFromIndex(i);
      cumul_soft_lower_bounds_[i].coefficient =
          dimension.GetCumulVarSoftLowerBoundCoefficientFromIndex(i);
    }
    if (dimension.HasCumulVarPiecewiseLinearCostFromIndex(i)) {
      has_cumul_piecewise_linear_costs = true;
      cumul_piecewise_linear_costs_[i] =
          dimension.GetCumulVarPiecewiseLinearCostFromIndex(i);
    }
    IntVar* const cumul_var = cumuls_[i];
    if (cumul_var->Min() > 0 && cumul_var->Max() < kint64max) {
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
  if (FilterSpanCost() || FilterCumulSoftBounds() || FilterSlackCost() ||
      FilterCumulSoftLowerBounds() || FilterCumulPiecewiseLinearCosts()) {
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
        cumul = GetNextValueFromForbiddenIntervals(cumul,
                                                   forbidden_intervals_[next]);
        cumul = std::max(cumuls_[next]->Min(), cumul);
        node = next;
        current_cumul_cost_value =
            CapAdd(current_cumul_cost_value, GetCumulSoftCost(node, cumul));
        current_cumul_cost_value = CapAdd(
            current_cumul_cost_value, GetCumulPiecewiseLinearCost(node, cumul));
      }
      if (FilterSlackCost()) {
        const int64 start =
            ComputePathMaxStartFromEndCumul(current_path_transits_, r, cumul);
        current_cumul_cost_value =
            CapAdd(current_cumul_cost_value,
                   CapProd(vehicle_span_cost_coefficients_[vehicle],
                           CapSub(CapSub(cumul, start), total_transit)));
      }
      if (FilterCumulSoftLowerBounds()) {
        current_cumul_cost_value =
            CapAdd(current_cumul_cost_value,
                   GetPathCumulSoftLowerBoundCost(current_path_transits_, r));
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
    // Use the max of the path end cumul mins to compute the corresponding
    // maximum start cumul of each path; store the minimum of these.
    for (int r = 0; r < NumPaths(); ++r) {
      const int64 start = ComputePathMaxStartFromEndCumul(
          current_path_transits_, r, current_max_end_.cumul_value);
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
  if (CanPropagateObjectiveValue()) {
    const int64 new_objective_value = CapAdd(
        CapAdd(injected_objective_value_, total_current_cumul_cost_value_),
        CapProd(global_span_cost_coefficient_,
                CapSub(current_max_end_.cumul_value,
                       current_min_start_.cumul_value)));
    PropagateObjectiveValue(new_objective_value);
  }
}

bool PathCumulFilter::AcceptPath(int64 path_start, int64 chain_start,
                                 int64 chain_end) {
  int64 node = path_start;
  int64 cumul = cumuls_[node]->Min();
  cumul_cost_delta_ = CapAdd(cumul_cost_delta_, GetCumulSoftCost(node, cumul));
  cumul_cost_delta_ =
      CapAdd(cumul_cost_delta_, GetCumulPiecewiseLinearCost(node, cumul));
  int64 total_transit = 0;
  const int path = delta_path_transits_.AddPaths(1);
  const int vehicle = start_to_vehicle_[path_start];
  const int64 capacity = vehicle_capacities_[vehicle];
  // Evaluating route length to reserve memory to store transit information.
  int number_of_route_arcs = 0;
  while (node < Size()) {
    const int64 next = GetNext(node);
    // TODO(user): This shouldn't be needed anymore as the such deltas should
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
    cumul =
        GetNextValueFromForbiddenIntervals(cumul, forbidden_intervals_[next]);
    if (cumul > std::min(capacity, cumuls_[next]->Max())) {
      return false;
    }
    cumul = std::max(cumuls_[next]->Min(), cumul);
    node = next;
    cumul_cost_delta_ =
        CapAdd(cumul_cost_delta_, GetCumulSoftCost(node, cumul));
    cumul_cost_delta_ =
        CapAdd(cumul_cost_delta_, GetCumulPiecewiseLinearCost(node, cumul));
  }
  if (FilterSlackCost()) {
    const int64 start =
        ComputePathMaxStartFromEndCumul(delta_path_transits_, path, cumul);
    const int64 path_cumul_range = CapSub(cumul, start);
    if (path_cumul_range > vehicle_span_upper_bounds_[vehicle]) {
      return false;
    }
    cumul_cost_delta_ = CapAdd(
        cumul_cost_delta_, CapProd(vehicle_span_cost_coefficients_[vehicle],
                                   CapSub(path_cumul_range, total_transit)));
  }
  if (FilterCumulSoftLowerBounds()) {
    cumul_cost_delta_ =
        CapAdd(cumul_cost_delta_,
               GetPathCumulSoftLowerBoundCost(delta_path_transits_, path));
  }
  if (FilterSpanCost() || FilterCumulSoftBounds() || FilterSlackCost() ||
      FilterCumulSoftLowerBounds() || FilterCumulPiecewiseLinearCosts()) {
    delta_paths_.insert(GetPath(path_start));
    delta_max_end_cumul_ = std::max(delta_max_end_cumul_, cumul);
    cumul_cost_delta_ =
        CapSub(cumul_cost_delta_, current_cumul_cost_values_[path_start]);
  }
  return true;
}

bool PathCumulFilter::FinalizeAcceptPath() {
  if ((!FilterSpanCost() && !FilterCumulSoftBounds() && !FilterSlackCost() &&
       !FilterCumulSoftLowerBounds() && !FilterCumulPiecewiseLinearCosts()) ||
      lns_detected_) {
    // Cleaning up for the next delta.
    delta_max_end_cumul_ = kint64min;
    delta_paths_.clear();
    delta_path_transits_.Clear();
    lns_detected_ = false;
    PropagateObjectiveValue(injected_objective_value_);
    return true;
  }
  int64 new_max_end = delta_max_end_cumul_;
  int64 new_min_start = kint64max;
  if (FilterSpanCost()) {
    if (new_max_end < current_max_end_.cumul_value) {
      // Delta max end is lower than the current solution one.
      // If the path supporting the current max end has been modified, we need
      // to check all paths to find the largest max end.
      if (!ContainsKey(delta_paths_, current_max_end_.cumul_value_support)) {
        new_max_end = current_max_end_.cumul_value;
      } else {
        for (int i = 0; i < current_max_end_.path_values.size(); ++i) {
          if (current_max_end_.path_values[i] > new_max_end &&
              !ContainsKey(delta_paths_, i)) {
            new_max_end = current_max_end_.path_values[i];
          }
        }
      }
    }
    // Now that the max end cumul has been found, compute the corresponding
    // min start cumul, first from the delta, then if the max end cumul has
    // changed, from the unchanged paths as well.
    for (int r = 0; r < delta_path_transits_.NumPaths(); ++r) {
      new_min_start = std::min(
          ComputePathMaxStartFromEndCumul(delta_path_transits_, r, new_max_end),
          new_min_start);
    }
    if (new_max_end != current_max_end_.cumul_value) {
      for (int r = 0; r < NumPaths(); ++r) {
        if (ContainsKey(delta_paths_, r)) {
          continue;
        }
        new_min_start = std::min(new_min_start,
                                 ComputePathMaxStartFromEndCumul(
                                     current_path_transits_, r, new_max_end));
      }
    } else if (new_min_start > current_min_start_.cumul_value) {
      // Delta min start is greater than the current solution one.
      // If the path supporting the current min start has been modified, we need
      // to check all paths to find the smallest min start.
      if (!ContainsKey(delta_paths_, current_min_start_.cumul_value_support)) {
        new_min_start = current_min_start_.cumul_value;
      } else {
        for (int i = 0; i < current_min_start_.path_values.size(); ++i) {
          if (current_min_start_.path_values[i] < new_min_start &&
              !ContainsKey(delta_paths_, i)) {
            new_min_start = current_min_start_.path_values[i];
          }
        }
      }
    }
  }
  // Cleaning up for the next delta.
  delta_max_end_cumul_ = kint64min;
  delta_paths_.clear();
  delta_path_transits_.Clear();
  lns_detected_ = false;
  // Filtering on objective value, including the injected part of it.
  const int64 new_objective_value =
      CapAdd(CapAdd(injected_objective_value_, cumul_cost_delta_),
             CapProd(global_span_cost_coefficient_,
                     CapSub(new_max_end, new_min_start)));
  PropagateObjectiveValue(new_objective_value);
  // Only compare to max as a cost lower bound is computed.
  return new_objective_value <= cost_var_->Max();
}

void PathCumulFilter::InitializeSupportedPathCumul(
    SupportedPathCumul* supported_cumul, int64 default_value) {
  supported_cumul->cumul_value = default_value;
  supported_cumul->cumul_value_support = -1;
  supported_cumul->path_values.resize(NumPaths(), default_value);
}

int64 PathCumulFilter::ComputePathMaxStartFromEndCumul(
    const PathTransits& path_transits, int path, int end_cumul) const {
  int64 cumul = end_cumul;
  for (int i = path_transits.PathSize(path) - 2; i >= 0; --i) {
    cumul = CapSub(cumul, path_transits.Transit(path, i));
    cumul = std::min(cumuls_[path_transits.Node(path, i)]->Max(), cumul);
  }
  return cumul;
}

}  // namespace

RoutingLocalSearchFilter* MakePathCumulFilter(
    const RoutingModel& routing_model, const RoutingDimension& dimension,
    Solver::ObjectiveWatcher objective_callback) {
  for (const int64 upper_bound : dimension.vehicle_span_upper_bounds()) {
    if (upper_bound != kint64max) {
      return routing_model.solver()->RevAlloc(
          new PathCumulFilter(routing_model, dimension, objective_callback));
    }
  }
  for (const int64 coefficient : dimension.vehicle_span_cost_coefficients()) {
    if (coefficient != 0) {
      return routing_model.solver()->RevAlloc(
          new PathCumulFilter(routing_model, dimension, objective_callback));
    }
  }
  for (const IntVar* const slack : dimension.slacks()) {
    if (slack->Min() > 0) {
      return routing_model.solver()->RevAlloc(
          new PathCumulFilter(routing_model, dimension, objective_callback));
    }
  }
  const std::vector<IntVar*>& cumuls = dimension.cumuls();
  for (int i = 0; i < cumuls.size(); ++i) {
    if (dimension.HasCumulVarSoftUpperBoundFromIndex(i)) {
      return routing_model.solver()->RevAlloc(
          new PathCumulFilter(routing_model, dimension, objective_callback));
    }
    if (dimension.HasCumulVarSoftLowerBoundFromIndex(i)) {
      return routing_model.solver()->RevAlloc(
          new PathCumulFilter(routing_model, dimension, objective_callback));
    }
    if (dimension.HasCumulVarPiecewiseLinearCostFromIndex(i)) {
      return routing_model.solver()->RevAlloc(
          new PathCumulFilter(routing_model, dimension, objective_callback));
    }
    IntVar* const cumul_var = cumuls[i];
    if (cumul_var->Min() > 0 && cumul_var->Max() < kint64max) {
      if (!routing_model.IsEnd(i)) {
        return routing_model.solver()->RevAlloc(
            new PathCumulFilter(routing_model, dimension, objective_callback));
      }
    }
    if (dimension.forbidden_intervals()[i].NumIntervals() > 0) {
      return routing_model.solver()->RevAlloc(
          new PathCumulFilter(routing_model, dimension, objective_callback));
    }
  }
  if (dimension.global_span_cost_coefficient() == 0) {
    return routing_model.solver()->RevAlloc(
        new ChainCumulFilter(routing_model, dimension, objective_callback));
  } else {
    return routing_model.solver()->RevAlloc(
        new PathCumulFilter(routing_model, dimension, objective_callback));
  }
}

namespace {

// Node precedence filter, resulting from pickup and delivery pairs.
class NodePrecedenceFilter : public BasePathFilter {
 public:
  NodePrecedenceFilter(const std::vector<IntVar*>& nexts, int next_domain_size,
                       const RoutingModel::NodePairs& pairs);
  ~NodePrecedenceFilter() override {}
  bool AcceptPath(int64 path_start, int64 chain_start,
                  int64 chain_end) override;
  std::string DebugString() const override { return "NodePrecedenceFilter"; }

 private:
  std::vector<int> pair_firsts_;
  std::vector<int> pair_seconds_;
  SparseBitset<> visited_;
};

NodePrecedenceFilter::NodePrecedenceFilter(const std::vector<IntVar*>& nexts,
                                           int next_domain_size,
                                           const RoutingModel::NodePairs& pairs)
    : BasePathFilter(nexts, next_domain_size, nullptr),
      pair_firsts_(next_domain_size, kUnassigned),
      pair_seconds_(next_domain_size, kUnassigned),
      visited_(Size()) {
  for (const auto& node_pair : pairs) {
    pair_firsts_[node_pair.first[0]] = node_pair.second[0];
    pair_seconds_[node_pair.second[0]] = node_pair.first[0];
  }
}

bool NodePrecedenceFilter::AcceptPath(int64 path_start, int64 chain_start,
                                      int64 chain_end) {
  visited_.ClearAll();
  int64 node = path_start;
  int64 path_length = 1;
  while (node < Size()) {
    // Detect sub-cycles (path is longer than longest possible path).
    if (path_length > Size()) {
      return false;
    }
    if (pair_firsts_[node] != kUnassigned && visited_[pair_firsts_[node]]) {
      return false;
    }
    if (pair_seconds_[node] != kUnassigned && !visited_[pair_seconds_[node]]) {
      return false;
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
    if (pair_firsts_[node] != kUnassigned && !visited_[pair_firsts_[node]]) {
      return false;
    }
  }
  return true;
}

}  // namespace

RoutingLocalSearchFilter* MakeNodePrecedenceFilter(
    const RoutingModel& routing_model, const RoutingModel::NodePairs& pairs) {
  return routing_model.solver()->RevAlloc(new NodePrecedenceFilter(
      routing_model.Nexts(), routing_model.Size() + routing_model.vehicles(),
      pairs));
}

namespace {

// Vehicle variable filter
class VehicleVarFilter : public BasePathFilter {
 public:
  explicit VehicleVarFilter(const RoutingModel& routing_model);
  ~VehicleVarFilter() override {}
  bool Accept(const Assignment* delta, const Assignment* deltadelta) override;
  bool AcceptPath(int64 path_start, int64 chain_start,
                  int64 chain_end) override;
  std::string DebugString() const override { return "VehicleVariableFilter"; }

 private:
  std::vector<int64> start_to_vehicle_;
  std::vector<IntVar*> vehicle_vars_;
  const int64 unconstrained_vehicle_var_domain_size_;
};

VehicleVarFilter::VehicleVarFilter(const RoutingModel& routing_model)
    : BasePathFilter(routing_model.Nexts(),
                     routing_model.Size() + routing_model.vehicles(), nullptr),
      vehicle_vars_(routing_model.VehicleVars()),
      unconstrained_vehicle_var_domain_size_(routing_model.vehicles()) {
  start_to_vehicle_.resize(Size(), -1);
  for (int i = 0; i < routing_model.vehicles(); ++i) {
    start_to_vehicle_[routing_model.Start(i)] = i;
  }
}

// Avoid filtering if variable domains are unconstrained.
bool VehicleVarFilter::Accept(const Assignment* delta,
                              const Assignment* deltadelta) {
  const Assignment::IntContainer& container = delta->IntVarContainer();
  const int size = container.Size();
  bool all_unconstrained = true;
  for (int i = 0; i < size; ++i) {
    int64 index = -1;
    if (FindIndex(container.Element(i).Var(), &index)) {
      const IntVar* const vehicle_var = vehicle_vars_[index];
      // If vehicle variable contains -1 (optional node), then we need to
      // add it to the "unconstrained" domain. Impact we don't filter mandatory
      // nodes made inactive here, but it is covered by other filters.
      const int adjusted_unconstrained_vehicle_var_domain_size =
          vehicle_var->Min() >= 0 ? unconstrained_vehicle_var_domain_size_
                                  : unconstrained_vehicle_var_domain_size_ + 1;
      if (vehicle_var->Size() !=
          adjusted_unconstrained_vehicle_var_domain_size) {
        all_unconstrained = false;
        break;
      }
    }
  }
  if (all_unconstrained) return true;
  return BasePathFilter::Accept(delta, deltadelta);
}

bool VehicleVarFilter::AcceptPath(int64 path_start, int64 chain_start,
                                  int64 chain_end) {
  const int64 vehicle = start_to_vehicle_[path_start];
  int64 node = path_start;
  while (node < Size()) {
    if (!vehicle_vars_[node]->Contains(vehicle)) {
      return false;
    }
    node = GetNext(node);
  }
  return true;
}

}  // namespace

RoutingLocalSearchFilter* MakeVehicleVarFilter(
    const RoutingModel& routing_model) {
  return routing_model.solver()->RevAlloc(new VehicleVarFilter(routing_model));
}

// TODO(user): Implement same-vehicle filter. Could be merged with node
// precedence filter.

// --- First solution decision builders ---

// IntVarFilteredDecisionBuilder

IntVarFilteredDecisionBuilder::IntVarFilteredDecisionBuilder(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<LocalSearchFilter*>& filters)
    : vars_(vars),
      assignment_(solver->MakeAssignment()),
      delta_(solver->MakeAssignment()),
      is_in_delta_(vars_.size(), false),
      empty_(solver->MakeAssignment()),
      filters_(filters),
      number_of_decisions_(0),
      number_of_rejects_(0) {
  assignment_->MutableIntVarContainer()->Resize(vars_.size());
  delta_indices_.reserve(vars_.size());
}

Decision* IntVarFilteredDecisionBuilder::Next(Solver* solver) {
  number_of_decisions_ = 0;
  number_of_rejects_ = 0;
  // Wiping assignment when starting a new search.
  assignment_->MutableIntVarContainer()->Clear();
  assignment_->MutableIntVarContainer()->Resize(vars_.size());
  delta_->MutableIntVarContainer()->Clear();
  SynchronizeFilters();
  if (BuildSolution()) {
    VLOG(2) << "Number of decisions: " << number_of_decisions_;
    VLOG(2) << "Number of rejected decisions: " << number_of_rejects_;
    assignment_->Restore();
  } else {
    solver->Fail();
  }
  return nullptr;
}

bool IntVarFilteredDecisionBuilder::Commit() {
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

void IntVarFilteredDecisionBuilder::SetValuesFromDomains() {
  Assignment::IntContainer* const container =
      assignment_->MutableIntVarContainer();
  for (int index = 0; index < vars_.size(); ++index) {
    IntVar* const var = vars_[index];
    if (var->Bound()) {
      container->AddAtPosition(var, index)->SetValue(var->Min());
    }
  }
}

void IntVarFilteredDecisionBuilder::SynchronizeFilters() {
  for (LocalSearchFilter* const filter : filters_) {
    filter->Synchronize(assignment_, delta_);
  }
}

bool IntVarFilteredDecisionBuilder::FilterAccept() {
  // All incremental filters must be called.
  bool ok = true;
  for (LocalSearchFilter* const filter : filters_) {
    if (filter->IsIncremental() || ok) {
      ok = filter->Accept(delta_, empty_) && ok;
    }
  }
  return ok;
}

// RoutingFilteredDecisionBuilder

RoutingFilteredDecisionBuilder::RoutingFilteredDecisionBuilder(
    RoutingModel* model, const std::vector<LocalSearchFilter*>& filters)
    : IntVarFilteredDecisionBuilder(model->solver(), model->Nexts(), filters),
      model_(model) {}

bool RoutingFilteredDecisionBuilder::InitializeRoutes() {
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
  for (int vehicle = 0; vehicle < model()->vehicles(); ++vehicle) {
    int64 node = model()->Start(vehicle);
    while (!model()->IsEnd(node) && Var(node)->Bound()) {
      const int64 next = Var(node)->Min();
      SetValue(node, next);
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
    int64 node = start_chain_ends_[vehicle];
    if (!model()->IsEnd(node)) {
      int64 next = starts[model()->End(vehicle)];
      SetValue(node, next);
      node = next;
      while (!model()->IsEnd(node)) {
        next = Var(node)->Min();
        SetValue(node, next);
        node = next;
      }
    }
  }
  return Commit();
}

void RoutingFilteredDecisionBuilder::MakeDisjunctionNodesUnperformed(
    int64 node) {
  model()->ForEachNodeInDisjunctionWithMaxCardinalityFromIndex(
      node, 1, [this, node](int alternate) {
        if (node != alternate) {
          SetValue(alternate, alternate);
        }
      });
}

void RoutingFilteredDecisionBuilder::MakeUnassignedNodesUnperformed() {
  for (int index = 0; index < Size(); ++index) {
    if (!Contains(index)) {
      SetValue(index, index);
    }
  }
}

// CheapestInsertionFilteredDecisionBuilder

CheapestInsertionFilteredDecisionBuilder::
    CheapestInsertionFilteredDecisionBuilder(
        RoutingModel* model,
        ResultCallback3<int64, int64, int64, int64>* evaluator,
        ResultCallback1<int64, int64>* penalty_evaluator,
        const std::vector<LocalSearchFilter*>& filters)
    : RoutingFilteredDecisionBuilder(model, filters),
      evaluator_(evaluator),
      penalty_evaluator_(penalty_evaluator) {
  evaluator_->CheckIsRepeatable();
  if (penalty_evaluator_ != nullptr) {
    penalty_evaluator_->CheckIsRepeatable();
  }
}

void CheapestInsertionFilteredDecisionBuilder::InsertBetween(int64 node,
                                                             int64 predecessor,
                                                             int64 successor) {
  SetValue(predecessor, node);
  SetValue(node, successor);
  MakeDisjunctionNodesUnperformed(node);
}

void CheapestInsertionFilteredDecisionBuilder::AppendEvaluatedPositionsAfter(
    int64 node_to_insert, int64 start, int64 next_after_start, int64 vehicle,
    std::vector<ValuedPosition>* valued_positions) {
  CHECK(valued_positions != nullptr);
  int64 insert_after = start;
  while (!model()->IsEnd(insert_after)) {
    const int64 insert_before =
        (insert_after == start) ? next_after_start : Value(insert_after);
    valued_positions->push_back(std::make_pair(
        CapAdd(evaluator_->Run(insert_after, node_to_insert, vehicle),
               CapSub(evaluator_->Run(node_to_insert, insert_before, vehicle),
                      evaluator_->Run(insert_after, insert_before, vehicle))),
        insert_after));
    insert_after = insert_before;
  }
}

int64 CheapestInsertionFilteredDecisionBuilder::GetUnperformedValue(
    int64 node_to_insert) const {
  if (penalty_evaluator_ != nullptr) {
    return penalty_evaluator_->Run(node_to_insert);
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
class GlobalCheapestInsertionFilteredDecisionBuilder::PairEntry {
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
  // Note: for compatibilty reasons, comparator follows tie-breaking rules used
  // in the first version of GlobalCheapestInsertion.
  bool operator<(const PairEntry& other) const {
    if (value_ != other.value_) {
      return value_ > other.value_;
    }
    if (pickup_insert_after_ != other.pickup_insert_after_) {
      return pickup_insert_after_ > other.pickup_insert_after_;
    }
    if (pickup_to_insert_ != other.pickup_to_insert_) {
      return pickup_to_insert_ > other.pickup_to_insert_;
    }
    if (delivery_insert_after_ != other.delivery_insert_after_) {
      return delivery_insert_after_ > other.delivery_insert_after_;
    }
    return delivery_to_insert_ > other.delivery_to_insert_;
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
class GlobalCheapestInsertionFilteredDecisionBuilder::NodeEntry {
 public:
  NodeEntry(int node_to_insert, int insert_after, int vehicle)
      : heap_index_(-1),
        value_(kint64max),
        node_to_insert_(node_to_insert),
        insert_after_(insert_after),
        vehicle_(vehicle) {}
  // Note: comparator follows tie-breaking rules used in the first version
  // GlobalCheapestInsertion.
  bool operator<(const NodeEntry& other) const {
    if (value_ != other.value_) {
      return value_ > other.value_;
    }
    if (insert_after_ != other.insert_after_) {
      return insert_after_ > other.insert_after_;
    }
    return node_to_insert_ > other.node_to_insert_;
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

// GlobalCheapestInsertionFilteredDecisionBuilder

GlobalCheapestInsertionFilteredDecisionBuilder::
    GlobalCheapestInsertionFilteredDecisionBuilder(
        RoutingModel* model,
        ResultCallback3<int64, int64, int64, int64>* evaluator,
        ResultCallback1<int64, int64>* penalty_evaluator,
        const std::vector<LocalSearchFilter*>& filters)
    : CheapestInsertionFilteredDecisionBuilder(model, evaluator,
                                               penalty_evaluator, filters) {}

bool GlobalCheapestInsertionFilteredDecisionBuilder::BuildSolution() {
  if (!InitializeRoutes()) {
    return false;
  }
  InsertPairs();
  InsertNodes();
  MakeUnassignedNodesUnperformed();
  return Commit();
}

void GlobalCheapestInsertionFilteredDecisionBuilder::InsertPairs() {
  AdjustablePriorityQueue<PairEntry> priority_queue;
  std::vector<PairEntries> pickup_to_entries;
  std::vector<PairEntries> delivery_to_entries;
  InitializePairPositions(&priority_queue, &pickup_to_entries,
                          &delivery_to_entries);
  while (!priority_queue.IsEmpty()) {
    PairEntry* const entry = priority_queue.Top();
    if (Contains(entry->pickup_to_insert()) ||
        Contains(entry->delivery_to_insert())) {
      DeletePairEntry(entry, &priority_queue, &pickup_to_entries,
                      &delivery_to_entries);
    } else {
      if (entry->vehicle() == -1) {
        // Pair is unperformed.
        SetValue(entry->pickup_to_insert(), entry->pickup_to_insert());
        SetValue(entry->delivery_to_insert(), entry->delivery_to_insert());
        if (!Commit()) {
          DeletePairEntry(entry, &priority_queue, &pickup_to_entries,
                          &delivery_to_entries);
        }
      } else {
        // Pair is performed.
        const int64 pickup_insert_before = Value(entry->pickup_insert_after());
        InsertBetween(entry->pickup_to_insert(), entry->pickup_insert_after(),
                      pickup_insert_before);
        const int64 delivery_insert_before =
            (entry->pickup_to_insert() == entry->delivery_insert_after())
                ? pickup_insert_before
                : Value(entry->delivery_insert_after());
        InsertBetween(entry->delivery_to_insert(),
                      entry->delivery_insert_after(), delivery_insert_before);
        if (Commit()) {
          const int64 pickup_after = entry->pickup_insert_after();
          const int64 pickup = entry->pickup_to_insert();
          const int64 delivery_after = entry->delivery_insert_after();
          const int64 delivery = entry->delivery_to_insert();
          const int vehicle = entry->vehicle();
          UpdatePairPositions(vehicle, pickup_after, &priority_queue,
                              &pickup_to_entries, &delivery_to_entries);
          UpdatePairPositions(vehicle, pickup, &priority_queue,
                              &pickup_to_entries, &delivery_to_entries);
          UpdatePairPositions(vehicle, delivery, &priority_queue,
                              &pickup_to_entries, &delivery_to_entries);
          if (pickup != delivery_after) {
            UpdatePairPositions(vehicle, delivery_after, &priority_queue,
                                &pickup_to_entries, &delivery_to_entries);
          }
        } else {
          DeletePairEntry(entry, &priority_queue, &pickup_to_entries,
                          &delivery_to_entries);
        }
      }
    }
  }
}

void GlobalCheapestInsertionFilteredDecisionBuilder::InsertNodes() {
  AdjustablePriorityQueue<NodeEntry> priority_queue;
  std::vector<NodeEntries> position_to_node_entries;
  InitializePositions(&priority_queue, &position_to_node_entries);
  while (!priority_queue.IsEmpty()) {
    NodeEntry* const node_entry = priority_queue.Top();
    if (Contains(node_entry->node_to_insert())) {
      DeleteNodeEntry(node_entry, &priority_queue, &position_to_node_entries);
    } else {
      if (node_entry->vehicle() == -1) {
        // Pair is unperformed.
        SetValue(node_entry->node_to_insert(), node_entry->node_to_insert());
        if (!Commit()) {
          DeleteNodeEntry(node_entry, &priority_queue,
                          &position_to_node_entries);
        }
      } else {
        InsertBetween(node_entry->node_to_insert(), node_entry->insert_after(),
                      Value(node_entry->insert_after()));
        if (Commit()) {
          const int vehicle = node_entry->vehicle();
          UpdatePositions(vehicle, node_entry->node_to_insert(),
                          &priority_queue, &position_to_node_entries);
          UpdatePositions(vehicle, node_entry->insert_after(), &priority_queue,
                          &position_to_node_entries);
        } else {
          DeleteNodeEntry(node_entry, &priority_queue,
                          &position_to_node_entries);
        }
      }
    }
  }
}

void GlobalCheapestInsertionFilteredDecisionBuilder::InitializePairPositions(
    AdjustablePriorityQueue<
        GlobalCheapestInsertionFilteredDecisionBuilder::PairEntry>*
        priority_queue,
    std::vector<GlobalCheapestInsertionFilteredDecisionBuilder::PairEntries>*
        pickup_to_entries,
    std::vector<GlobalCheapestInsertionFilteredDecisionBuilder::PairEntries>*
        delivery_to_entries) {
  priority_queue->Clear();
  pickup_to_entries->clear();
  pickup_to_entries->resize(model()->Size());
  delivery_to_entries->clear();
  delivery_to_entries->resize(model()->Size());
  for (const RoutingModel::NodePair& node_pair :
       model()->GetPickupAndDeliveryPairs()) {
    const int64 pickup = node_pair.first[0];
    const int64 delivery = node_pair.second[0];
    if (Contains(pickup) || Contains(delivery)) {
      continue;
    }
    // Add insertion entry making pair unperformed.
    const int64 pickup_penalty = GetUnperformedValue(pickup);
    const int64 delivery_penalty = GetUnperformedValue(delivery);
    int64 penalty =
        FLAGS_routing_shift_insertion_cost_by_penalty ? kint64max : 0;
    if (pickup_penalty != kint64max && delivery_penalty != kint64max) {
      PairEntry* const entry = new PairEntry(pickup, -1, delivery, -1, -1);
      if (FLAGS_routing_shift_insertion_cost_by_penalty) {
        entry->set_value(0);
        penalty = CapAdd(pickup_penalty, delivery_penalty);
      } else {
        entry->set_value(CapAdd(pickup_penalty, delivery_penalty));
        penalty = 0;
      }
      priority_queue->Add(entry);
    }
    // Add all other insertion entries with pair performed.
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
      if (valued_position.second.first != valued_position.second.second) {
        delivery_to_entries->at(valued_position.second.second).insert(entry);
      }
      priority_queue->Add(entry);
    }
  }
}

void GlobalCheapestInsertionFilteredDecisionBuilder::UpdatePickupPositions(
    int vehicle, int64 pickup_insert_after,
    AdjustablePriorityQueue<
        GlobalCheapestInsertionFilteredDecisionBuilder::PairEntry>*
        priority_queue,
    std::vector<GlobalCheapestInsertionFilteredDecisionBuilder::PairEntries>*
        pickup_to_entries,
    std::vector<GlobalCheapestInsertionFilteredDecisionBuilder::PairEntries>*
        delivery_to_entries) {
  // First, remove entries which have already been inserted and keep track of
  // the entries which are being kept and must be updated.
  using Pair = std::pair<int64, int64>;
  using Insertion = std::pair<Pair, /*delivery_insert_after*/ int64>;
  std::unordered_set<Insertion, std::hash<Insertion>> existing_insertions;
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
  const int64 pickup_insert_before = Value(pickup_insert_after);
  for (const RoutingModel::NodePair& node_pair :
       model()->GetPickupAndDeliveryPairs()) {
    const int64 pickup = node_pair.first[0];
    const int64 delivery = node_pair.second[0];
    if (!Contains(pickup) && !Contains(delivery)) {
      int64 delivery_insert_after = pickup;
      while (!model()->IsEnd(delivery_insert_after)) {
        const std::pair<Pair, int64> insertion = {{pickup, delivery},
                                                  delivery_insert_after};
        if (!ContainsKey(existing_insertions, insertion)) {
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
  // Compute new value of entries and either update the priority queue
  // accordingly if the entry already existed or add it to the queue if it's
  // new.
  const int64 old_pickup_value =
      evaluator_->Run(pickup_insert_after, pickup_insert_before, vehicle);
  for (PairEntry* const pair_entry :
       pickup_to_entries->at(pickup_insert_after)) {
    DCHECK_EQ(pickup_insert_after, pair_entry->pickup_insert_after());
    const int64 pickup_value =
        CapSub(CapAdd(evaluator_->Run(pickup_insert_after,
                                      pair_entry->pickup_to_insert(), vehicle),
                      evaluator_->Run(pair_entry->pickup_to_insert(),
                                      pickup_insert_before, vehicle)),
               old_pickup_value);
    const int64 delivery_insert_after = pair_entry->delivery_insert_after();
    const int64 delivery_insert_before =
        (delivery_insert_after == pair_entry->pickup_to_insert())
            ? pickup_insert_before
            : Value(delivery_insert_after);
    const int64 delivery_value = CapSub(
        CapAdd(evaluator_->Run(delivery_insert_after,
                               pair_entry->delivery_to_insert(), vehicle),
               evaluator_->Run(pair_entry->delivery_to_insert(),
                               delivery_insert_before, vehicle)),
        evaluator_->Run(delivery_insert_after, delivery_insert_before,
                        vehicle));
    const int64 penalty =
        FLAGS_routing_shift_insertion_cost_by_penalty
            ? CapAdd(GetUnperformedValue(pair_entry->pickup_to_insert()),
                     GetUnperformedValue(pair_entry->delivery_to_insert()))
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

void GlobalCheapestInsertionFilteredDecisionBuilder::UpdateDeliveryPositions(
    int vehicle, int64 delivery_insert_after,
    AdjustablePriorityQueue<
        GlobalCheapestInsertionFilteredDecisionBuilder::PairEntry>*
        priority_queue,
    std::vector<GlobalCheapestInsertionFilteredDecisionBuilder::PairEntries>*
        pickup_to_entries,
    std::vector<GlobalCheapestInsertionFilteredDecisionBuilder::PairEntries>*
        delivery_to_entries) {
  // First, remove entries which have already been inserted and keep track of
  // the entries which are being kept and must be updated.
  using Pair = std::pair<int64, int64>;
  using Insertion = std::pair<Pair, /*pickup_insert_after*/ int64>;
  std::unordered_set<Insertion, std::hash<Insertion>> existing_insertions;
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
  const int64 delivery_insert_before = Value(delivery_insert_after);
  for (const RoutingModel::NodePair& node_pair :
       model()->GetPickupAndDeliveryPairs()) {
    const int64 pickup = node_pair.first[0];
    const int64 delivery = node_pair.second[0];
    if (!Contains(pickup) && !Contains(delivery)) {
      int64 pickup_insert_after = model()->Start(vehicle);
      while (pickup_insert_after != delivery_insert_after) {
        std::pair<Pair, int64> insertion = {{pickup, delivery},
                                            pickup_insert_after};
        if (!ContainsKey(existing_insertions, insertion)) {
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
  // Compute new value of entries and either update the priority queue
  // accordingly if the entry already existed or add it to the queue if it's
  // new.
  const int64 old_delivery_value =
      evaluator_->Run(delivery_insert_after, delivery_insert_before, vehicle);
  for (PairEntry* const pair_entry :
       delivery_to_entries->at(delivery_insert_after)) {
    DCHECK_EQ(delivery_insert_after, pair_entry->delivery_insert_after());
    const int64 pickup_value = CapSub(
        CapAdd(
            evaluator_->Run(pair_entry->pickup_insert_after(),
                            pair_entry->pickup_to_insert(), vehicle),
            evaluator_->Run(pair_entry->pickup_to_insert(),
                            Value(pair_entry->pickup_insert_after()), vehicle)),
        evaluator_->Run(pair_entry->pickup_insert_after(),
                        Value(pair_entry->pickup_insert_after()), vehicle));
    const int64 delivery_value = CapSub(
        CapAdd(evaluator_->Run(delivery_insert_after,
                               pair_entry->delivery_to_insert(), vehicle),
               evaluator_->Run(pair_entry->delivery_to_insert(),
                               delivery_insert_before, vehicle)),
        old_delivery_value);
    const int64 penalty =
        FLAGS_routing_shift_insertion_cost_by_penalty
            ? CapAdd(GetUnperformedValue(pair_entry->pickup_to_insert()),
                     GetUnperformedValue(pair_entry->delivery_to_insert()))
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

void GlobalCheapestInsertionFilteredDecisionBuilder::DeletePairEntry(
    GlobalCheapestInsertionFilteredDecisionBuilder::PairEntry* entry,
    AdjustablePriorityQueue<
        GlobalCheapestInsertionFilteredDecisionBuilder::PairEntry>*
        priority_queue,
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

void GlobalCheapestInsertionFilteredDecisionBuilder::InitializePositions(
    AdjustablePriorityQueue<
        GlobalCheapestInsertionFilteredDecisionBuilder::NodeEntry>*
        priority_queue,
    std::vector<GlobalCheapestInsertionFilteredDecisionBuilder::NodeEntries>*
        position_to_node_entries) {
  priority_queue->Clear();
  position_to_node_entries->clear();
  position_to_node_entries->resize(model()->Size());
  for (int node = 0; node < model()->Size(); ++node) {
    if (Contains(node)) {
      continue;
    }
    const int64 node_penalty = GetUnperformedValue(node);
    int64 penalty =
        FLAGS_routing_shift_insertion_cost_by_penalty ? kint64max : 0;
    // Add insertion entry making node unperformed.
    if (node_penalty != kint64max) {
      NodeEntry* const node_entry = new NodeEntry(node, -1, -1);
      if (FLAGS_routing_shift_insertion_cost_by_penalty) {
        node_entry->set_value(0);
        penalty = node_penalty;
      } else {
        node_entry->set_value(node_penalty);
        penalty = 0;
      }
      priority_queue->Add(node_entry);
    }
    // Add all insertion entries making node performed.
    for (int vehicle = 0; vehicle < model()->vehicles(); ++vehicle) {
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
  }
}

void GlobalCheapestInsertionFilteredDecisionBuilder::UpdatePositions(
    int vehicle, int64 insert_after,
    AdjustablePriorityQueue<
        GlobalCheapestInsertionFilteredDecisionBuilder::NodeEntry>*
        priority_queue,
    std::vector<GlobalCheapestInsertionFilteredDecisionBuilder::NodeEntries>*
        node_entries) {
  // Either create new entries if we are inserting after a newly inserted node
  // or remove entries which have already been inserted.
  bool update = true;
  if (node_entries->at(insert_after).empty()) {
    update = false;
    for (int node_to_insert = 0; node_to_insert < model()->Size();
         ++node_to_insert) {
      if (!Contains(node_to_insert)) {
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
  const int64 old_value = evaluator_->Run(insert_after, insert_before, vehicle);
  for (NodeEntry* const node_entry : node_entries->at(insert_after)) {
    DCHECK_EQ(node_entry->insert_after(), insert_after);
    const int64 value =
        CapSub(CapAdd(evaluator_->Run(insert_after,
                                      node_entry->node_to_insert(), vehicle),
                      evaluator_->Run(node_entry->node_to_insert(),
                                      insert_before, vehicle)),
               old_value);
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

void GlobalCheapestInsertionFilteredDecisionBuilder::DeleteNodeEntry(
    GlobalCheapestInsertionFilteredDecisionBuilder::NodeEntry* entry,
    AdjustablePriorityQueue<
        GlobalCheapestInsertionFilteredDecisionBuilder::NodeEntry>*
        priority_queue,
    std::vector<NodeEntries>* node_entries) {
  priority_queue->Remove(entry);
  if (entry->insert_after() != -1) {
    node_entries->at(entry->insert_after()).erase(entry);
  }
  delete entry;
}

// LocalCheapestInsertionFilteredDecisionBuilder
// TODO(user): Add support for penalty costs.
LocalCheapestInsertionFilteredDecisionBuilder::
    LocalCheapestInsertionFilteredDecisionBuilder(
        RoutingModel* model,
        ResultCallback3<int64, int64, int64, int64>* evaluator,
        const std::vector<LocalSearchFilter*>& filters)
    : CheapestInsertionFilteredDecisionBuilder(model, evaluator, nullptr,
                                               filters) {}

bool LocalCheapestInsertionFilteredDecisionBuilder::BuildSolution() {
  if (!InitializeRoutes()) {
    return false;
  }
  // Marking if we've tried inserting a node.
  std::vector<bool> visited(model()->Size(), false);
  // Possible positions where the current node can inserted.
  std::vector<int64> insertion_positions;
  // Possible positions where its associated delivery node can inserted (if the
  // current node has one).
  std::vector<int64> delivery_insertion_positions;
  // Iterating on pickup and delivery pairs
  const RoutingModel::NodePairs& node_pairs =
      model()->GetPickupAndDeliveryPairs();
  for (const auto& node_pair : node_pairs) {
    const int64 pickup = node_pair.first[0];
    const int64 delivery = node_pair.second[0];
    // If either is already in the solution, let it be inserted in the standard
    // node insertion loop.
    if (Contains(pickup) || Contains(delivery)) {
      continue;
    }
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
            (delivery_insertion == pickup_insertion)
                ? pickup
                : (delivery_insertion == pickup) ? pickup_insertion_next
                                                 : Value(delivery_insertion);
        InsertBetween(delivery, delivery_insertion, delivery_insertion_next);
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
  // Iterating on remaining nodes.
  for (int node = 0; node < model()->Size(); ++node) {
    if (Contains(node) || visited[node]) {
      continue;
    }
    ComputeEvaluatorSortedPositions(node, &insertion_positions);
    for (const int64 insertion : insertion_positions) {
      InsertBetween(node, insertion, Value(insertion));
      if (Commit()) {
        break;
      }
    }
  }
  MakeUnassignedNodesUnperformed();
  return Commit();
}

void LocalCheapestInsertionFilteredDecisionBuilder::
    ComputeEvaluatorSortedPositions(int64 node,
                                    std::vector<int64>* sorted_positions) {
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

void LocalCheapestInsertionFilteredDecisionBuilder::
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

// CheapestAdditionFilteredDecisionBuilder

CheapestAdditionFilteredDecisionBuilder::
    CheapestAdditionFilteredDecisionBuilder(
        RoutingModel* model, const std::vector<LocalSearchFilter*>& filters)
    : RoutingFilteredDecisionBuilder(model, filters) {}

bool CheapestAdditionFilteredDecisionBuilder::BuildSolution() {
  if (!InitializeRoutes()) {
    return false;
  }
  const int kUnassigned = -1;
  const RoutingModel::NodePairs& pairs = model()->GetPickupAndDeliveryPairs();
  std::vector<int> deliveries(Size(), kUnassigned);
  for (const RoutingModel::NodePair& pair : pairs) {
    deliveries[pair.first[0]] = pair.second[0];
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
  std::vector<int64> neighbors;
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
      int64 end = model()->End(vehicle);
      // Extend the route until either the end node of the vehicle is reached
      // or no node or node pair can be added. Deliveries in pickup and
      // delivery pairs are added at the end of the route in reverse order
      // of the pickups.
      while (found && !model()->IsEnd(index)) {
        found = false;
        SortPossibleNexts(index, &neighbors);
        for (const int64 next : neighbors) {
          if (model()->IsEnd(next) && next != end) {
            continue;
          }
          // Insert "next" after "index", and before "end" if it is not the end
          // already.
          SetValue(index, next);
          const int delivery = next < Size() ? deliveries[next] : kUnassigned;
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
  MakeUnassignedNodesUnperformed();
  return Commit();
}

bool CheapestAdditionFilteredDecisionBuilder::
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

// EvaluatorCheapestAdditionFilteredDecisionBuilder

EvaluatorCheapestAdditionFilteredDecisionBuilder::
    EvaluatorCheapestAdditionFilteredDecisionBuilder(
        RoutingModel* model, ResultCallback2<int64, int64, int64>* evaluator,
        const std::vector<LocalSearchFilter*>& filters)
    : CheapestAdditionFilteredDecisionBuilder(model, filters),
      evaluator_(evaluator) {
  evaluator_->CheckIsRepeatable();
}

void EvaluatorCheapestAdditionFilteredDecisionBuilder::SortPossibleNexts(
    int64 from, std::vector<int64>* sorted_nexts) {
  CHECK(sorted_nexts != nullptr);
  const std::vector<IntVar*>& nexts = model()->Nexts();
  sorted_nexts->clear();
  const int size = model()->Size();
  if (from < size) {
    std::vector<std::pair<int64, int64>> valued_neighbors;
    IntVar* const next = nexts[from];
    std::unique_ptr<IntVarIterator> it(next->MakeDomainIterator(false));
    for (const int64 value : InitAndGetValues(it.get())) {
      if (value != from && (value >= size || !Contains(value))) {
        // Tie-breaking on largest node index to mimic the behavior of
        // CheapestValueSelector (search.cc).
        valued_neighbors.push_back(
            std::make_pair(evaluator_->Run(from, value), -value));
      }
    }
    std::sort(valued_neighbors.begin(), valued_neighbors.end());
    sorted_nexts->reserve(valued_neighbors.size());
    for (const std::pair<int64, int64> neighbor : valued_neighbors) {
      sorted_nexts->push_back(-neighbor.second);
    }
  }
}

// ComparatorCheapestAdditionFilteredDecisionBuilder

ComparatorCheapestAdditionFilteredDecisionBuilder::
    ComparatorCheapestAdditionFilteredDecisionBuilder(
        RoutingModel* model, Solver::VariableValueComparator comparator,
        const std::vector<LocalSearchFilter*>& filters)
    : CheapestAdditionFilteredDecisionBuilder(model, filters),
      comparator_(std::move(comparator)) {}

void ComparatorCheapestAdditionFilteredDecisionBuilder::SortPossibleNexts(
    int64 from, std::vector<int64>* sorted_nexts) {
  CHECK(sorted_nexts != nullptr);
  const std::vector<IntVar*>& nexts = model()->Nexts();
  sorted_nexts->clear();
  const int size = model()->Size();
  if (from < size) {
    IntVar* const next = nexts[from];
    std::unique_ptr<IntVarIterator> it(next->MakeDomainIterator(false));
    for (const int64 value : InitAndGetValues(it.get())) {
      if (value != from && (value >= size || !Contains(value))) {
        sorted_nexts->push_back(value);
      }
    }
    std::sort(sorted_nexts->begin(), sorted_nexts->end(),
              [this, from](int next1, int next2) {
                return comparator_(from, next1, next2);
              });
  }
}

// SavingsFilteredDecisionBuilder

SavingsFilteredDecisionBuilder::SavingsFilteredDecisionBuilder(
    RoutingModel* model, double savings_neighbors_ratio, bool add_reverse_arcs,
    const std::vector<LocalSearchFilter*>& filters)
    : RoutingFilteredDecisionBuilder(model, filters),
      savings_neighbors_ratio_(savings_neighbors_ratio > 0
                                   ? std::min(savings_neighbors_ratio, 1.0)
                                   : 1),
      add_reverse_arcs_(add_reverse_arcs),
      size_squared_(0) {}

bool SavingsFilteredDecisionBuilder::BuildSolution() {
  if (!InitializeRoutes()) {
    return false;
  }
  const int size = model()->Size();
  size_squared_ = size * size;
  std::vector<Saving> savings = ComputeSavings();
  const int vehicle_types = vehicles_per_vehicle_type_.size();
  DCHECK_GT(vehicle_types, 0);
  // Store savings for each incoming and outgoing node and by vehicle type. This
  // is necessary to quickly extend partial chains without scanning all savings.
  std::vector<std::vector<int>> in_savings_indices(size * vehicle_types);
  std::vector<std::vector<int>> out_savings_indices(size * vehicle_types);
  for (int i = 0; i < savings.size(); ++i) {
    const Saving& saving = savings[i];
    const int vehicle_type_offset = GetVehicleTypeFromSaving(saving) * size;
    const int before_node = GetBeforeNodeFromSaving(saving);
    in_savings_indices[vehicle_type_offset + before_node].push_back(i);
    const int after_node = GetAfterNodeFromSaving(saving);
    out_savings_indices[vehicle_type_offset + after_node].push_back(i);
  }
  // For each vehicle type, sort vehicles by decreasing vehicle fixed cost.
  // Vehicles with the same fixed cost are sorted by decreasing vehicle index.
  std::vector<int64> fixed_cost_of_vehicle(model()->vehicles());
  for (int vehicle = 0; vehicle < model()->vehicles(); vehicle++) {
    fixed_cost_of_vehicle[vehicle] = model()->GetFixedCostOfVehicle(vehicle);
  }
  for (int type = 0; type < vehicle_types; type++) {
    std::vector<int>& sorted_vehicles = vehicles_per_vehicle_type_[type];
    std::stable_sort(sorted_vehicles.begin(), sorted_vehicles.end(),
                     [&fixed_cost_of_vehicle](int v1, int v2) {
                       return fixed_cost_of_vehicle[v1] <
                              fixed_cost_of_vehicle[v2];
                     });
    std::reverse(sorted_vehicles.begin(), sorted_vehicles.end());
  }

  // Build routes from savings.
  for (const Saving& saving : savings) {
    // First find the best saving to start a new route.
    const int type = GetVehicleTypeFromSaving(saving);
    std::vector<int>& sorted_vehicles = vehicles_per_vehicle_type_[type];
    if (sorted_vehicles.empty()) continue;
    int vehicle = sorted_vehicles.back();

    int before_node = GetBeforeNodeFromSaving(saving);
    int after_node = GetAfterNodeFromSaving(saving);
    if (!Contains(before_node) && !Contains(after_node)) {
      const int64 start = model()->Start(vehicle);
      const int64 end = model()->End(vehicle);
      SetValue(start, before_node);
      SetValue(before_node, after_node);
      SetValue(after_node, end);
      if (Commit()) {
        // Then extend the route from both ends of the partial route.
        sorted_vehicles.pop_back();
        int in_index = 0;
        int out_index = 0;
        const int saving_offset = type * size;

        while (in_index <
                   in_savings_indices[saving_offset + after_node].size() ||
               out_index <
                   out_savings_indices[saving_offset + before_node].size()) {
          // First determine how to extend the route.
          int before_before_node = -1;
          int after_after_node = -1;
          if (in_index <
              in_savings_indices[saving_offset + after_node].size()) {
            const Saving& in_saving =
                savings[in_savings_indices[saving_offset + after_node]
                                          [in_index]];
            if (out_index <
                out_savings_indices[saving_offset + before_node].size()) {
              const Saving& out_saving =
                  savings[out_savings_indices[saving_offset + before_node]
                                             [out_index]];
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
                savings[out_savings_indices[saving_offset + before_node]
                                           [out_index]]);
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
  }
  MakeUnassignedNodesUnperformed();
  return Commit();
}

void SavingsFilteredDecisionBuilder::ComputeVehicleTypes() {
  type_index_of_vehicle_.clear();
  const int nodes = model()->nodes();
  const int nodes_squared = nodes * nodes;
  const int vehicles = model()->vehicles();
  type_index_of_vehicle_.resize(vehicles);

  vehicles_per_vehicle_type_.clear();
  std::unordered_map<int64, int> type_to_type_index;

  for (int v = 0; v < vehicles; v++) {
    const int start = model()->IndexToNode(model()->Start(v)).value();
    const int end = model()->IndexToNode(model()->End(v)).value();
    const int cost_class = model()->GetCostClassIndexOfVehicle(v).value();
    const int64 type = cost_class * nodes_squared + start * nodes + end;

    const auto& vehicle_type_added = type_to_type_index.insert(
        std::make_pair(type, type_to_type_index.size()));

    const int index = vehicle_type_added.first->second;

    if (vehicle_type_added.second) {
      // Type was not indexed yet.
      DCHECK_EQ(vehicles_per_vehicle_type_.size(), index);
      vehicles_per_vehicle_type_.push_back({v});
    } else {
      // Type already indexed.
      DCHECK_LT(index, vehicles_per_vehicle_type_.size());
      vehicles_per_vehicle_type_[index].push_back(v);
    }
    type_index_of_vehicle_[v] = index;
  }
}

// Computes and returns the savings related to each pair of non-start and
// non-end nodes. The savings value for an arc a-->b for a vehicle starting at
// node s and ending at node e is:
// saving = cost(s-->a-->e) + cost(s-->b-->e) - cost(s-->a-->b-->e), i.e.
// saving = cost(a-->e) + cost(s-->b) - cost(a-->b)
// The higher this saving value, the better the arc.
// Here, the value stored for the savings in the output vector is -saving, and
// the vector is therefore sorted in increasing order (the lower -saving,
// the better).
std::vector<SavingsFilteredDecisionBuilder::Saving>
SavingsFilteredDecisionBuilder::ComputeSavings() {
  ComputeVehicleTypes();
  const int size = model()->Size();

  const int64 saving_neighbors = std::max(1.0, size * savings_neighbors_ratio_);

  const int num_vehicle_types = vehicles_per_vehicle_type_.size();
  std::vector<Saving> savings;
  savings.reserve(num_vehicle_types * size * saving_neighbors);

  for (int type = 0; type < num_vehicle_types; ++type) {
    const std::vector<int>& vehicles = vehicles_per_vehicle_type_[type];
    if (vehicles.empty()) {
      continue;
    }
    const int vehicle = vehicles.front();
    const int64 cost_class =
        model()->GetCostClassIndexOfVehicle(vehicle).value();
    const int64 start = model()->Start(vehicle);
    const int64 end = model()->End(vehicle);
    const int64 fixed_cost = model()->GetFixedCostOfVehicle(vehicle);

    // TODO(user): deal with the add_reverse_arcs_ flag more efficiently.
    std::vector<bool> arc_added;
    if (add_reverse_arcs_) {
      arc_added.resize(size * size, false);
    }
    for (int before_node = 0; before_node < size; ++before_node) {
      if (!Contains(before_node) && !model()->IsEnd(before_node) &&
          !model()->IsStart(before_node)) {
        const int64 in_saving =
            model()->GetArcCostForClass(before_node, end, cost_class);
        std::vector<std::pair</*cost*/ int64, /*node*/ int64>>
            costed_after_nodes;
        costed_after_nodes.reserve(size);
        for (int after_node = 0; after_node < size; ++after_node) {
          if (after_node != before_node && !Contains(after_node) &&
              !model()->IsEnd(after_node) && !model()->IsStart(after_node)) {
            costed_after_nodes.push_back(
                std::make_pair(model()->GetArcCostForClass(
                                   before_node, after_node, cost_class),
                               after_node));
          }
        }
        if (saving_neighbors < size) {
          std::nth_element(costed_after_nodes.begin(),
                           costed_after_nodes.begin() + saving_neighbors,
                           costed_after_nodes.end());
          costed_after_nodes.resize(saving_neighbors);
        }
        for (const auto& costed_after_node : costed_after_nodes) {
          const int64 after_node = costed_after_node.second;
          if (add_reverse_arcs_ && arc_added[before_node * size + after_node]) {
            DCHECK(arc_added[after_node * size + before_node]);
            continue;
          }

          const int64 saving =
              CapSub(CapAdd(in_saving, model()->GetArcCostForClass(
                                           start, after_node, cost_class)),
                     CapAdd(costed_after_node.first, fixed_cost));
          savings.push_back(
              BuildSaving(-saving, type, before_node, after_node));

          if (add_reverse_arcs_) {
            // Also add after->before savings.
            arc_added[before_node * size + after_node] = true;
            arc_added[after_node * size + before_node] = true;
            const int64 second_cost = model()->GetArcCostForClass(
                after_node, before_node, cost_class);
            const int64 second_saving = CapSub(
                CapAdd(model()->GetArcCostForClass(after_node, end, cost_class),
                       model()->GetArcCostForClass(start, before_node,
                                                   cost_class)),
                CapAdd(second_cost, fixed_cost));
            savings.push_back(
                BuildSaving(-second_saving, type, after_node, before_node));
          }
        }
      }
    }
  }
  std::sort(savings.begin(), savings.end());
  return savings;
}

// ChristofidesFilteredDecisionBuilder
ChristofidesFilteredDecisionBuilder::ChristofidesFilteredDecisionBuilder(
    RoutingModel* model, const std::vector<LocalSearchFilter*>& filters)
    : RoutingFilteredDecisionBuilder(model, filters) {}

// TODO(user): Support pickup & delivery.
bool ChristofidesFilteredDecisionBuilder::BuildSolution() {
  if (!InitializeRoutes()) {
    return false;
  }
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
        return model()->GetArcCostForClass(from_index, to_index, cost_class);
      };
      using Cost = decltype(cost);
      ChristofidesPathSolver<int64, int64, int, Cost> christofides_solver(
          indices.size(), cost);
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
    : dimension_(CHECK_NOTNULL(dimension)),
      model_(CHECK_NOTNULL(model)),
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
      state_dependent_class_evaluators_
          [state_dependent_vehicle_to_class_[serving_vehicle]](next, next_next);
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
  const int64 current_state_independent_transit =
      class_evaluators_[vehicle_to_class_[serving_vehicle]](node, next);
  const int64 current_state_dependent_transit =
      state_dependent_class_evaluators_
          [state_dependent_vehicle_to_class_[serving_vehicle]](node, next)
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
      solver_->MakeLocalSearchPhaseParameters(hill_climber, slacks_finalizer);
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
