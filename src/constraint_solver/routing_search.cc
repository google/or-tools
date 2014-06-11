// Copyright 2010-2013 Google
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

#include "constraint_solver/routing.h"

#include <map>
#include <set>
#include "base/small_map.h"
#include "base/small_ordered_set.h"
#include "util/bitset.h"

namespace operations_research {

// --- Routing-specific local search filters ---

// RoutingLocalSearchFilter

RoutingLocalSearchFilter::RoutingLocalSearchFilter(
    const std::vector<IntVar*> nexts, Callback1<int64>* objective_callback)
    : IntVarLocalSearchFilter(nexts),
      injected_objective_value_(0),
      objective_callback_(objective_callback) {}

void RoutingLocalSearchFilter::InjectObjectiveValue(int64 objective_value) {
  injected_objective_value_ = objective_value;
}

void RoutingLocalSearchFilter::PropagateObjectiveValue(int64 objective_value) {
  if (objective_callback_.get() != nullptr) {
    objective_callback_->Run(objective_value);
  }
}

namespace {

// Node disjunction filter class.

class NodeDisjunctionFilter : public RoutingLocalSearchFilter {
 public:
  NodeDisjunctionFilter(const RoutingModel& routing_model,
                        Callback1<int64>* objective_callback)
      : RoutingLocalSearchFilter(routing_model.Nexts(), objective_callback),
        routing_model_(routing_model),
        active_per_disjunction_(routing_model.GetNumberOfDisjunctions(), 0),
        penalty_value_(0) {}

  virtual bool Accept(const Assignment* delta, const Assignment* deltadelta) {
    const int64 kUnassigned = -1;
    const Assignment::IntContainer& container = delta->IntVarContainer();
    const int delta_size = container.Size();
    small_map<std::map<RoutingModel::DisjunctionIndex, int>>
        disjunction_active_deltas;
    bool lns_detected = false;
    for (int i = 0; i < delta_size; ++i) {
      const IntVarElement& new_element = container.Element(i);
      IntVar* const var = new_element.Var();
      int64 index = kUnassigned;
      if (FindIndex(var, &index) && IsVarSynced(index)) {
        RoutingModel::DisjunctionIndex disjunction_index(kUnassigned);
        if (routing_model_.GetDisjunctionIndexFromVariableIndex(
                index, &disjunction_index)) {
          const bool was_inactive = (Value(index) == index);
          const bool is_inactive =
              (new_element.Min() <= index && new_element.Max() >= index);
          if (new_element.Min() != new_element.Max()) {
            lns_detected = true;
          }
          if (was_inactive && !is_inactive) {
            ++LookupOrInsert(&disjunction_active_deltas, disjunction_index, 0);
          } else if (!was_inactive && is_inactive) {
            --LookupOrInsert(&disjunction_active_deltas, disjunction_index, 0);
          }
        }
      }
    }
    int64 new_objective_value = injected_objective_value_ + penalty_value_;
    for (const std::pair<RoutingModel::DisjunctionIndex, int>
             disjunction_active_delta : disjunction_active_deltas) {
      const int active_nodes =
          active_per_disjunction_[disjunction_active_delta.first] +
          disjunction_active_delta.second;
      if (active_nodes > 1) {
        PropagateObjectiveValue(0);
        return false;
      }
      if (!lns_detected) {
        const int64 penalty = routing_model_.GetDisjunctionPenalty(
            disjunction_active_delta.first);
        if (disjunction_active_delta.second < 0) {
          if (penalty < 0) {
            PropagateObjectiveValue(0);
            return false;
          } else {
            new_objective_value += penalty;
          }
        } else if (disjunction_active_delta.second > 0) {
          new_objective_value -= penalty;
        }
      }
    }
    PropagateObjectiveValue(new_objective_value);
    if (lns_detected) {
      return true;
    } else {
      IntVar* const cost_var = routing_model_.CostVar();
      // Only compare to max as a cost lower bound is computed.
      return new_objective_value <= cost_var->Max();
    }
  }
  virtual std::string DebugString() const { return "NodeDisjunctionFilter"; }

 private:
  virtual void OnSynchronize(const Assignment* delta) {
    penalty_value_ = 0;
    for (RoutingModel::DisjunctionIndex i(0);
         i < active_per_disjunction_.size(); ++i) {
      active_per_disjunction_[i] = 0;
      const std::vector<int>& disjunction_nodes =
          routing_model_.GetDisjunctionIndices(i);
      bool all_nodes_synced = true;
      for (const int64 node : disjunction_nodes) {
        const bool node_synced = IsVarSynced(node);
        if (!node_synced) all_nodes_synced = false;
        if (node_synced && Value(node) != node) {
          ++active_per_disjunction_[i];
        }
      }
      const int64 penalty = routing_model_.GetDisjunctionPenalty(i);
      if (active_per_disjunction_[i] == 0 && penalty > 0 && all_nodes_synced) {
        penalty_value_ += penalty;
      }
    }
    PropagateObjectiveValue(injected_objective_value_ + penalty_value_);
  }

  const RoutingModel& routing_model_;
  ITIVector<RoutingModel::DisjunctionIndex, int> active_per_disjunction_;
  int64 penalty_value_;
};
}  // namespace

RoutingLocalSearchFilter* MakeNodeDisjunctionFilter(
    const RoutingModel& routing_model, Callback1<int64>* objective_callback) {
  return routing_model.solver()->RevAlloc(
      new NodeDisjunctionFilter(routing_model, objective_callback));
}

namespace {

// Generic path-based filter class.

class BasePathFilter : public RoutingLocalSearchFilter {
 public:
  BasePathFilter(const std::vector<IntVar*>& nexts, int next_domain_size,
                 Callback1<int64>* objective_callback);
  virtual ~BasePathFilter() {}
  virtual bool Accept(const Assignment* delta, const Assignment* deltadelta);
  virtual void OnSynchronize(const Assignment* delta);

 protected:
  static const int64 kUnassigned;

  int64 GetNext(int64 node) const {
    return (new_nexts_[node] == kUnassigned)
               ? (IsVarSynced(node) ? Value(node) : kUnassigned)
               : new_nexts_[node];
  }
  int NumPaths() const { return starts_.size(); }
  int64 Start(int i) const { return starts_[i]; }
  int GetPath(int64 node) const { return paths_[node]; }

 private:
  virtual void OnBeforeSynchronizePaths() {}
  virtual void OnAfterSynchronizePaths() {}
  virtual void OnSynchronizePathFromStart(int64 start) {}
  virtual void InitializeAcceptPath() {}
  virtual bool AcceptPath(int64 path_start, int64 chain_start,
                          int64 chain_end) = 0;
  virtual bool FinalizeAcceptPath() { return true; }
  // Detects path starts, used to track which node belongs to which path.
  void ComputePathStarts(std::vector<int64>* path_starts,
                         std::vector<int>* index_to_path);
  bool HavePathsChanged();
  void SynchronizeFullAssignment();
  void UpdateAllRanks();
  void UpdatePathRanksFromStart(int start);

  std::vector<int64> node_path_starts_;
  std::vector<int64> starts_;
  std::vector<int> paths_;
  std::vector<int64> new_nexts_;
  std::vector<int> delta_touched_;
  SparseBitset<> touched_paths_;
  SparseBitset<> touched_path_nodes_;
  std::vector<int> ranks_;
};

const int64 BasePathFilter::kUnassigned = -1;

BasePathFilter::BasePathFilter(const std::vector<IntVar*>& nexts,
                               int next_domain_size,
                               Callback1<int64>* objective_callback)
    : RoutingLocalSearchFilter(nexts, objective_callback),
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
  // correspond the the min and max ranks of touched nodes in the current
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
  DCHECK(!HavePathsChanged());
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

// ChainCumul filter. Version of dimension path filter which is O(delta) rather
// than O(length of touched paths). Currently only supports dimensions without
// costs (global and local span cost, soft bounds) and with unconstrained
// cumul variables except overall capacity and cumul variables of path ends.

class ChainCumulFilter : public BasePathFilter {
 public:
  ChainCumulFilter(const RoutingModel& routing_model,
                   const RoutingDimension& dimension,
                   Callback1<int64>* objective_callback);
  virtual ~ChainCumulFilter() {}
  virtual std::string DebugString() const {
    return "ChainCumulFilter(" + name_ + ")";
  }

 private:
  virtual void OnSynchronizePathFromStart(int64 start);
  virtual bool AcceptPath(int64 path_start, int64 chain_start, int64 chain_end);

  const std::vector<IntVar*> cumuls_;
  std::vector<int64> start_to_vehicle_;
  std::vector<int64> start_to_end_;
  std::vector<Solver::IndexEvaluator2*> evaluators_;
  RoutingModel::VehicleEvaluator* const capacity_evaluator_;
  std::vector<int64> current_path_cumul_mins_;
  std::vector<int64> current_max_of_path_end_cumul_mins_;
  std::vector<int64> old_nexts_;
  std::vector<int> old_vehicles_;
  std::vector<int64> current_transits_;
  const std::string name_;
};

ChainCumulFilter::ChainCumulFilter(const RoutingModel& routing_model,
                                   const RoutingDimension& dimension,
                                   Callback1<int64>* objective_callback)
    : BasePathFilter(routing_model.Nexts(), dimension.cumuls().size(),
                     objective_callback),
      cumuls_(dimension.cumuls()),
      evaluators_(routing_model.vehicles(), nullptr),
      capacity_evaluator_(dimension.capacity_evaluator()),
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
    evaluators_[i] = dimension.transit_evaluator(i);
  }
}

// On synchronization, maintain "propagated" cumul mins and max level of cumul
// from each node to the end of the path; to be used by AcceptPath to
// incrementally check feasibility.
void ChainCumulFilter::OnSynchronizePathFromStart(int64 start) {
  const int vehicle = start_to_vehicle_[start];
  Solver::IndexEvaluator2* const evaluator = evaluators_[vehicle];
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
      current_transits_[node] = evaluator->Run(node, next);
    }
    cumul += current_transits_[node];
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
  const int64 capacity = capacity_evaluator_ == nullptr
                             ? kint64max
                             : capacity_evaluator_->Run(vehicle);
  Solver::IndexEvaluator2* const evaluator = evaluators_[vehicle];
  int64 node = chain_start;
  int64 cumul = current_path_cumul_mins_[node];
  while (node != chain_end) {
    const int64 next = GetNext(node);
    if (IsVarSynced(node) && next == Value(node) &&
        vehicle == old_vehicles_[node]) {
      cumul += current_transits_[node];
    } else {
      cumul += evaluator->Run(node, next);
    }
    cumul = std::max(cumuls_[next]->Min(), cumul);
    if (cumul > capacity) return false;
    node = next;
  }
  const int64 end = start_to_end_[path_start];
  const int64 end_cumul_delta =
      current_path_cumul_mins_[end] - current_path_cumul_mins_[node];
  const int64 after_chain_cumul_delta =
      current_max_of_path_end_cumul_mins_[node] -
      current_path_cumul_mins_[node];
  return cumul + after_chain_cumul_delta <= capacity &&
         cumul + end_cumul_delta <= cumuls_[end]->Max();
}

// PathCumul filter.

class PathCumulFilter : public BasePathFilter {
 public:
  PathCumulFilter(const RoutingModel& routing_model,
                  const RoutingDimension& dimension,
                  Callback1<int64>* objective_callback);
  virtual ~PathCumulFilter() {}
  virtual std::string DebugString() const {
    return "PathCumulFilter(" + name_ + ")";
  }

 private:
  // This structure stores the "best" path cumul value for a solution, the path
  // supporting this value, and the corresponding path cumul values for all
  // paths.
  struct SupportedPathCumul {
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

  virtual void InitializeAcceptPath() {
    cumul_cost_delta_ = total_current_cumul_cost_value_;
  }
  virtual bool AcceptPath(int64 path_start, int64 chain_start, int64 chain_end);
  virtual bool FinalizeAcceptPath();
  virtual void OnBeforeSynchronizePaths();

  bool FilterSpanCost() const { return global_span_cost_coefficient_ != 0; }

  bool FilterSlackCost() const {
    return has_nonzero_vehicle_span_cost_coefficients_
        || has_vehicle_span_upper_bounds_;
  }

  bool FilterCumulSoftBounds() const { return !cumul_soft_bounds_.empty(); }

  int64 GetCumulSoftCost(int64 node, int64 cumul_value) const;

  void InitializeSupportedPathCumul(SupportedPathCumul* supported_cumul,
                                    int64 default_value);

  // Compute the max start cumul value for a given path given an end cumul
  // value.
  int64 ComputePathMaxStartFromEndCumul(const PathTransits& path_transits,
                                        int path, int end_cumul) const;

  const std::vector<IntVar*> cumuls_;
  const std::vector<IntVar*> slacks_;
  std::vector<int64> start_to_vehicle_;
  std::vector<Solver::IndexEvaluator2*> evaluators_;
  std::vector<int64> vehicle_span_upper_bounds_;
  bool has_vehicle_span_upper_bounds_;
  int64 total_current_cumul_cost_value_;
  // Map between paths and path soft cumul bound costs. The paths are indexed
  // by the index of the start node of the path.
  hash_map<int64, int64> current_cumul_cost_values_;
  int64 cumul_cost_delta_;
  const int64 global_span_cost_coefficient_;
  std::vector<SoftBound> cumul_soft_bounds_;
  std::vector<int64> vehicle_span_cost_coefficients_;
  bool has_nonzero_vehicle_span_cost_coefficients_;
  IntVar* const cost_var_;
  RoutingModel::VehicleEvaluator* const capacity_evaluator_;
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
                                 Callback1<int64>* objective_callback)
    : BasePathFilter(routing_model.Nexts(), dimension.cumuls().size(),
                     objective_callback),
      cumuls_(dimension.cumuls()),
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
      capacity_evaluator_(dimension.capacity_evaluator()),
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
  bool has_cumul_soft_bounds = false;
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
    IntVar* const cumul_var = cumuls_[i];
    if (cumul_var->Min() > 0 && cumul_var->Max() < kint64max) {
      has_cumul_hard_bounds = true;
    }
  }
  if (!has_cumul_soft_bounds) {
    cumul_soft_bounds_.clear();
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
    evaluators_[i] = dimension.transit_evaluator(i);
  }
}

int64 PathCumulFilter::GetCumulSoftCost(int64 node, int64 cumul_value) const {
  if (node < cumul_soft_bounds_.size()) {
    const int64 bound = cumul_soft_bounds_[node].bound;
    const int64 coefficient = cumul_soft_bounds_[node].coefficient;
    if (coefficient > 0 && bound < cumul_value) {
      return (cumul_value - bound) * coefficient;
    }
  }
  return 0;
}

void PathCumulFilter::OnBeforeSynchronizePaths() {
  total_current_cumul_cost_value_ = 0;
  cumul_cost_delta_ = 0;
  current_cumul_cost_values_.clear();
  if (FilterSpanCost() || FilterCumulSoftBounds() || FilterSlackCost()) {
    InitializeSupportedPathCumul(&current_min_start_, kint64max);
    InitializeSupportedPathCumul(&current_max_end_, kint64min);
    current_path_transits_.Clear();
    current_path_transits_.AddPaths(NumPaths());
    // For each path, compute the minimum end cumul and store the max of these.
    for (int r = 0; r < NumPaths(); ++r) {
      int64 node = Start(r);
      const int vehicle = start_to_vehicle_[Start(r)];
      Solver::IndexEvaluator2* const evaluator = evaluators_[vehicle];
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
      int64 total_transit = 0;
      while (node < Size()) {
        const int64 next = Value(node);
        const int64 transit = evaluator->Run(node, next);
        total_transit += transit;
        const int64 transit_slack = transit + slacks_[node]->Min();
        current_path_transits_.PushTransit(r, node, next, transit_slack);
        cumul += transit_slack;
        cumul = std::max(cumuls_[next]->Min(), cumul);
        node = next;
        current_cumul_cost_value += GetCumulSoftCost(node, cumul);
      }
      if (FilterSlackCost()) {
        const int64 start =
            ComputePathMaxStartFromEndCumul(current_path_transits_, r, cumul);
        current_cumul_cost_value += vehicle_span_cost_coefficients_[vehicle] *
                                    (cumul - start - total_transit);
      }
      current_cumul_cost_values_[Start(r)] = current_cumul_cost_value;
      current_max_end_.path_values[r] = cumul;
      if (current_max_end_.cumul_value < cumul) {
        current_max_end_.cumul_value = cumul;
        current_max_end_.cumul_value_support = r;
      }
      total_current_cumul_cost_value_ += current_cumul_cost_value;
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
    const int64 new_objective_value =
        injected_objective_value_ + total_current_cumul_cost_value_ +
        global_span_cost_coefficient_ *
            (current_max_end_.cumul_value - current_min_start_.cumul_value);
    PropagateObjectiveValue(new_objective_value);
  }
}

bool PathCumulFilter::AcceptPath(int64 path_start, int64 chain_start,
                                 int64 chain_end) {
  int64 node = path_start;
  int64 cumul = cumuls_[node]->Min();
  cumul_cost_delta_ += GetCumulSoftCost(node, cumul);
  int64 total_transit = 0;
  const int path = delta_path_transits_.AddPaths(1);
  const int vehicle = start_to_vehicle_[path_start];
  const int64 capacity = capacity_evaluator_ == nullptr
                             ? kint64max
                             : capacity_evaluator_->Run(vehicle);
  Solver::IndexEvaluator2* const evaluator = evaluators_[vehicle];
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
    const int64 transit = evaluator->Run(node, next);
    total_transit += transit;
    const int64 transit_slack = transit + slacks_[node]->Min();
    delta_path_transits_.PushTransit(path, node, next, transit_slack);
    cumul += transit_slack;
    if (cumul > std::min(capacity, cumuls_[next]->Max())) {
      return false;
    }
    cumul = std::max(cumuls_[next]->Min(), cumul);
    node = next;
    cumul_cost_delta_ += GetCumulSoftCost(node, cumul);
  }
  if (FilterSlackCost()) {
    const int64 start =
        ComputePathMaxStartFromEndCumul(delta_path_transits_, path, cumul);
    const int64 path_cumul_range = cumul - start;
    if (path_cumul_range > vehicle_span_upper_bounds_[vehicle]) {
      return false;
    }
    cumul_cost_delta_ += vehicle_span_cost_coefficients_[vehicle] *
                         (path_cumul_range - total_transit);
  }
  if (FilterSpanCost() || FilterCumulSoftBounds() || FilterSlackCost()) {
    delta_paths_.insert(GetPath(path_start));
    delta_max_end_cumul_ = std::max(delta_max_end_cumul_, cumul);
    cumul_cost_delta_ -= current_cumul_cost_values_[path_start];
  }
  return true;
}

bool PathCumulFilter::FinalizeAcceptPath() {
  if ((!FilterSpanCost() && !FilterCumulSoftBounds() && !FilterSlackCost()) ||
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
        new_min_start =
            std::min(new_min_start, ComputePathMaxStartFromEndCumul(
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
      injected_objective_value_ + cumul_cost_delta_ +
      global_span_cost_coefficient_ * (new_max_end - new_min_start);
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
    cumul -= path_transits.Transit(path, i);
    cumul = std::min(cumuls_[path_transits.Node(path, i)]->Max(), cumul);
  }
  return cumul;
}

}  // namespace

RoutingLocalSearchFilter* MakePathCumulFilter(
    const RoutingModel& routing_model, const RoutingDimension& dimension,
    Callback1<int64>* objective_callback) {
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
    IntVar* const cumul_var = cumuls[i];
    if (cumul_var->Min() > 0 && cumul_var->Max() < kint64max) {
      if (!routing_model.IsEnd(i)) {
        return routing_model.solver()->RevAlloc(
            new PathCumulFilter(routing_model, dimension, objective_callback));
      }
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
  virtual ~NodePrecedenceFilter() {}
  virtual bool AcceptPath(int64 path_start, int64 chain_start, int64 chain_end);
  virtual std::string DebugString() const { return "NodePrecedenceFilter"; }

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
  for (const std::pair<int64, int64> node_pair : pairs) {
    pair_firsts_[node_pair.first] = node_pair.second;
    pair_seconds_[node_pair.second] = node_pair.first;
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
  ~VehicleVarFilter() {}
  virtual bool Accept(const Assignment* delta,
                      const Assignment* deltadelta);
  virtual bool AcceptPath(int64 path_start, int64 chain_start, int64 chain_end);
  virtual std::string DebugString() const { return "VehicleVariableFilter"; }

 private:
  std::vector<int64> start_to_vehicle_;
  std::vector<IntVar*> vehicle_vars_;
  const int64 unconstrained_vehicle_var_domain_size_;
};

VehicleVarFilter::VehicleVarFilter(const RoutingModel& routing_model)
    : BasePathFilter(routing_model.Nexts(),
                     routing_model.Size() + routing_model.vehicles(),
                     nullptr),
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
          vehicle_var->Min() >= 0
          ? unconstrained_vehicle_var_domain_size_
          : unconstrained_vehicle_var_domain_size_ + 1;
      if (vehicle_var->Size()
          != adjusted_unconstrained_vehicle_var_domain_size) {
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
  std::vector<int> alternates;
  model()->GetDisjunctionIndicesFromIndex(node, &alternates);
  for (const int alternate : alternates) {
    if (node != alternate) {
      SetValue(alternate, alternate);
    }
  }
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
        const std::vector<LocalSearchFilter*>& filters)
    : RoutingFilteredDecisionBuilder(model, filters), evaluator_(evaluator) {
  evaluator_->CheckIsRepeatable();
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
    valued_positions->push_back(
        std::make_pair(evaluator_->Run(insert_after, node_to_insert, vehicle) +
                      evaluator_->Run(node_to_insert, insert_before, vehicle) -
                      evaluator_->Run(insert_after, insert_before, vehicle),
                  insert_after));
    insert_after = insert_before;
  }
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
        const std::vector<LocalSearchFilter*>& filters)
    : CheapestInsertionFilteredDecisionBuilder(model, evaluator, filters) {}

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

void GlobalCheapestInsertionFilteredDecisionBuilder::InsertNodes() {
  AdjustablePriorityQueue<NodeEntry> priority_queue;
  std::vector<NodeEntries> position_to_node_entries;
  InitializePositions(&priority_queue, &position_to_node_entries);
  while (!priority_queue.IsEmpty()) {
    NodeEntry* const node_entry = priority_queue.Top();
    if (Contains(node_entry->node_to_insert())) {
      DeleteNodeEntry(node_entry, &priority_queue, &position_to_node_entries);
    } else {
      InsertBetween(node_entry->node_to_insert(), node_entry->insert_after(),
                    Value(node_entry->insert_after()));
      if (Commit()) {
        const int vehicle = node_entry->vehicle();
        UpdatePositions(
            vehicle, node_entry->node_to_insert(), &priority_queue,
            &position_to_node_entries);
        UpdatePositions(vehicle, node_entry->insert_after(), &priority_queue,
                        &position_to_node_entries);
      } else {
        DeleteNodeEntry(node_entry, &priority_queue, &position_to_node_entries);
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
  for (const RoutingModel::NodePair node_pair :
       model()->GetPickupAndDeliveryPairs()) {
    const int64 pickup = node_pair.first;
    const int64 delivery = node_pair.second;
    if (Contains(pickup) || Contains(delivery)) {
      continue;
    }
    std::vector<std::pair<std::pair<int64, int>, std::pair<int64, int64>>> valued_positions;
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
              std::make_pair(
                  valued_pickup_position.first + valued_delivery_position.first,
                  vehicle),
              std::make_pair(pickup_position, valued_delivery_position.second)));
        }
      }
    }
    for (const std::pair<std::pair<int64, int>, std::pair<int64, int64>>& valued_position :
         valued_positions) {
      PairEntry* const entry = new PairEntry(
          pickup, valued_position.second.first, delivery,
          valued_position.second.second, valued_position.first.second);
      entry->set_value(valued_position.first.first);
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
  hash_set<std::pair<RoutingModel::NodePair, /*delivery_insert_after*/ int64>>
      existing_insertions;
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
          std::make_pair(std::make_pair(pair_entry->pickup_to_insert(),
                              pair_entry->delivery_to_insert()),
                    pair_entry->delivery_insert_after()));
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
    const int64 pickup = node_pair.first;
    const int64 delivery = node_pair.second;
    if (!Contains(pickup) && !Contains(delivery)) {
      int64 delivery_insert_after = pickup;
      while (!model()->IsEnd(delivery_insert_after)) {
        const std::pair<RoutingModel::NodePair, int64> insertion =
            std::make_pair(std::make_pair(pickup, delivery), delivery_insert_after);
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
        evaluator_->Run(pickup_insert_after, pair_entry->pickup_to_insert(),
                        vehicle) +
        evaluator_->Run(pair_entry->pickup_to_insert(), pickup_insert_before,
                        vehicle) -
        old_pickup_value;
    const int64 delivery_insert_after = pair_entry->delivery_insert_after();
    const int64 delivery_insert_before =
        (delivery_insert_after == pair_entry->pickup_to_insert())
            ? pickup_insert_before
            : Value(delivery_insert_after);
    const int64 delivery_value =
        evaluator_->Run(delivery_insert_after, pair_entry->delivery_to_insert(),
                        vehicle) +
        evaluator_->Run(pair_entry->delivery_to_insert(),
                        delivery_insert_before, vehicle) -
        evaluator_->Run(delivery_insert_after, delivery_insert_before, vehicle);
    pair_entry->set_value(pickup_value + delivery_value);
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
  hash_set<std::pair<RoutingModel::NodePair, /*pickup_insert_after*/ int64>>
      existing_insertions;
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
          std::make_pair(std::make_pair(pair_entry->pickup_to_insert(),
                              pair_entry->delivery_to_insert()),
                    pair_entry->pickup_insert_after()));
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
    const int64 pickup = node_pair.first;
    const int64 delivery = node_pair.second;
    if (!Contains(pickup) && !Contains(delivery)) {
      int64 pickup_insert_after = model()->Start(vehicle);
      while (pickup_insert_after != delivery_insert_after) {
        std::pair<RoutingModel::NodePair, int64> insertion =
            std::make_pair(std::make_pair(pickup, delivery), pickup_insert_after);
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
    const int64 pickup_value =
        evaluator_->Run(pair_entry->pickup_insert_after(),
                        pair_entry->pickup_to_insert(), vehicle) +
        evaluator_->Run(pair_entry->pickup_to_insert(),
                        Value(pair_entry->pickup_insert_after()), vehicle) -
        evaluator_->Run(pair_entry->pickup_insert_after(),
                        Value(pair_entry->pickup_insert_after()), vehicle);
    const int64 delivery_value =
        evaluator_->Run(delivery_insert_after, pair_entry->delivery_to_insert(),
                        vehicle) +
        evaluator_->Run(pair_entry->delivery_to_insert(),
                        delivery_insert_before, vehicle) -
        old_delivery_value;
    pair_entry->set_value(pickup_value + delivery_value);
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
  pickup_to_entries->at(entry->pickup_insert_after()).erase(entry);
  delivery_to_entries->at(entry->delivery_insert_after()).erase(entry);
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
    for (int vehicle = 0; vehicle < model()->vehicles(); ++vehicle) {
      std::vector<ValuedPosition> valued_positions;
      const int64 start = model()->Start(vehicle);
      AppendEvaluatedPositionsAfter(node, start, Value(start), vehicle,
                                    &valued_positions);
      for (const std::pair<int64, int64>& valued_position : valued_positions) {
        NodeEntry* const node_entry =
            new NodeEntry(node, valued_position.second, vehicle);
        node_entry->set_value(valued_position.first);
        position_to_node_entries->at(valued_position.second)
            .insert(node_entry);
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
        evaluator_->Run(insert_after, node_entry->node_to_insert(), vehicle) +
        evaluator_->Run(node_entry->node_to_insert(), insert_before, vehicle) -
        old_value;
    node_entry->set_value(value);
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
  node_entries->at(entry->insert_after()).erase(entry);
  priority_queue->Remove(entry);
  delete entry;
}

// LocalCheapestInsertionFilteredDecisionBuilder

LocalCheapestInsertionFilteredDecisionBuilder::
    LocalCheapestInsertionFilteredDecisionBuilder(
        RoutingModel* model,
        ResultCallback3<int64, int64, int64, int64>* evaluator,
        const std::vector<LocalSearchFilter*>& filters)
    : CheapestInsertionFilteredDecisionBuilder(model, evaluator, filters) {}

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
  for (const std::pair<int64, int64> node_pair : node_pairs) {
    const int64 pickup = node_pair.first;
    const int64 delivery = node_pair.second;
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

void
LocalCheapestInsertionFilteredDecisionBuilder::ComputeEvaluatorSortedPositions(
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
  for (const RoutingModel::NodePair pair : pairs) {
    deliveries[pair.first] = pair.second;
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
    int64 index = GetStartChainEnd(vehicle);
    int64 end = model()->End(vehicle);
    bool found = true;
    // Extend the route of the current vehicle while it's possible.
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
            end = delivery;
          }
          break;
        }
      }
    }
  }
  MakeUnassignedNodesUnperformed();
  return Commit();
}

bool CheapestAdditionFilteredDecisionBuilder::
    PartialRoutesAndLargeVehicleIndicesFirst::
    operator()(int vehicle1, int vehicle2) const {
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
        RoutingModel* model,
        ResultCallback3<bool, int64, int64, int64>* comparator,
        const std::vector<LocalSearchFilter*>& filters)
    : CheapestAdditionFilteredDecisionBuilder(model, filters),
      comparator_(comparator) {
  comparator_->CheckIsRepeatable();
}

namespace {
class ArcComparator {
 public:
  ArcComparator(int from,
                ResultCallback3<bool, int64, int64, int64>* comparator)
      : from_(from), comparator_(comparator) {}
  bool operator()(int next1, int next2) const {
    return comparator_->Run(from_, next1, next2);
  }

 private:
  const int from_;
  ResultCallback3<bool, int64, int64, int64>* const comparator_;
};
}  // namespace

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
    ArcComparator comparator(from, comparator_.get());
    std::sort(sorted_nexts->begin(), sorted_nexts->end(), comparator);
  }
}

// SavingsFilteredDecisionBuilder

SavingsFilteredDecisionBuilder::SavingsFilteredDecisionBuilder(
    RoutingModel* model, int64 saving_neighbors,
    const std::vector<LocalSearchFilter*>& filters)
    : RoutingFilteredDecisionBuilder(model, filters),
      saving_neighbors_(saving_neighbors),
      size_squared_(0) {}

bool SavingsFilteredDecisionBuilder::BuildSolution() {
  if (!InitializeRoutes()) {
    return false;
  }
  const int size = model()->Size();
  size_squared_ = size * size;
  std::vector<Saving> savings = ComputeSavings();
  // Store savings for each incoming and outgoing node and by cost class. This
  // is necessary to quickly extend partial chains without scanning all savings.
  const int cost_classes = model()->GetCostClassesCount();
  std::vector<std::vector<int>> in_savings(size * cost_classes);
  std::vector<std::vector<int>> out_savings(size * cost_classes);
  for (int i = 0; i < savings.size(); ++i) {
    const Saving& saving = savings[i];
    const int cost_class_offset = GetCostClassFromSaving(saving) * size;
    const int before_node = GetBeforeNodeFromSaving(saving);
    in_savings[cost_class_offset + before_node].push_back(i);
    const int after_node = GetAfterNodeFromSaving(saving);
    out_savings[cost_class_offset + after_node].push_back(i);
  }
  // Build routes from savings.
  std::vector<bool> closed(model()->vehicles(), false);
  for (const Saving& saving : savings) {
    // First find the best saving to start a new route.
    const int cost_class = GetCostClassFromSaving(saving);
    int vehicle = -1;
    for (int v = 0; v < model()->vehicles(); ++v) {
      if (!closed[v] &&
          model()->GetCostClassIndexOfVehicle(v).value() == cost_class) {
        vehicle = v;
        break;
      }
    }
    if (vehicle == -1) continue;
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
        closed[vehicle] = true;
        int in_index = 0;
        int out_index = 0;
        const int saving_offset = cost_class * size;
        while (in_index < in_savings[saving_offset + after_node].size() &&
               out_index < out_savings[saving_offset + before_node].size()) {
          const Saving& in_saving =
              savings[in_savings[saving_offset + after_node][in_index]];
          const Saving& out_saving =
              savings[out_savings[saving_offset + before_node][out_index]];
          if (GetSavingValue(in_saving) < GetSavingValue(out_saving)) {
            // Extending after after_node
            const int after_after_node = GetAfterNodeFromSaving(in_saving);
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
            const int before_before_node = GetBeforeNodeFromSaving(out_saving);
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

std::vector<SavingsFilteredDecisionBuilder::Saving>
SavingsFilteredDecisionBuilder::ComputeSavings() const {
  const int size = model()->Size();
  const int64 saving_neighbors =
      saving_neighbors_ <= 0 ? size : saving_neighbors_;
  const int num_cost_classes = model()->GetCostClassesCount();
  std::vector<Saving> savings;
  savings.reserve(num_cost_classes * size * saving_neighbors);
  std::vector<bool> class_covered(num_cost_classes, false);
  for (int vehicle = 0; vehicle < model()->vehicles(); ++vehicle) {
    const int64 cost_class =
        model()->GetCostClassIndexOfVehicle(vehicle).value();
    if (!class_covered[cost_class]) {
      class_covered[cost_class] = true;
      const int64 start = model()->Start(vehicle);
      const int64 end = model()->End(vehicle);
      for (int before_node = 0; before_node < size; ++before_node) {
        if (!Contains(before_node) && !model()->IsEnd(before_node) &&
            !model()->IsStart(before_node)) {
          const int64 in_saving =
              model()->GetArcCostForClass(before_node, end, cost_class);
          std::vector<std::pair</*cost*/ int64, /*node*/ int64>> costed_after_nodes;
          costed_after_nodes.reserve(size);
          for (int after_node = 0; after_node < size; ++after_node) {
            if (after_node != before_node && !Contains(after_node) &&
                !model()->IsEnd(after_node) && !model()->IsStart(after_node)) {
              costed_after_nodes.push_back(
                  std::make_pair(model()->GetArcCostForClass(before_node, after_node,
                                                        cost_class),
                            after_node));
            }
          }
          if (saving_neighbors < size) {
            nth_element(costed_after_nodes.begin(),
                        costed_after_nodes.begin() + saving_neighbors,
                        costed_after_nodes.end());
            costed_after_nodes.resize(saving_neighbors);
          }
          for (const auto& after_node : costed_after_nodes) {
            const int64 saving = in_saving +
                                 model()->GetArcCostForClass(
                                     start, after_node.second, cost_class) -
                                 after_node.first;
            savings.push_back(BuildSaving(-saving, cost_class, before_node,
                                          after_node.second));
          }
        }
      }
    }
  }
  std::sort(savings.begin(), savings.end());
  return savings;
}
}  // namespace operations_research
