// Copyright 2010-2011 Google
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

#include "constraint_solver/routing.h"

#include <algorithm>
#include "base/hash.h"
#include "base/callback.h"
#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/timer.h"
#include "base/concise_iterator.h"
#include "base/map-util.h"
#include "base/stl_util-inl.h"
#include "base/hash.h"
#include "constraint_solver/constraint_solveri.h"

// Neighborhood deactivation
DEFINE_bool(routing_no_lns, false,
            "Routing: forbids use of Large Neighborhood Search.");
DEFINE_bool(routing_no_relocate, false,
            "Routing: forbids use of Relocate neighborhood.");
DEFINE_bool(routing_no_exchange, false,
            "Routing: forbids use of Exchange neighborhood.");
DEFINE_bool(routing_no_cross, false,
            "Routing: forbids use of Cross neighborhood.");
DEFINE_bool(routing_no_2opt, false,
            "Routing: forbids use of 2Opt neighborhood.");
DEFINE_bool(routing_no_oropt, false,
            "Routing: forbids use of OrOpt neighborhood.");
DEFINE_bool(routing_no_make_active, false,
            "Routing: forbids use of MakeActive/SwapActive/MakeInactive "
            "neighborhoods.");
DEFINE_bool(routing_no_lkh, false,
            "Routing: forbids use of LKH neighborhood.");
DEFINE_bool(routing_no_tsp, true,
            "Routing: forbids use of TSPOpt neighborhood.");
DEFINE_bool(routing_no_tsplns, true,
            "Routing: forbids use of TSPLNS neighborhood.");
DEFINE_bool(routing_use_extended_swap_active, false,
            "Routing: use extended version of SwapActive neighborhood.");

// Search limits
DEFINE_int64(routing_solution_limit, kint64max,
             "Routing: number of solutions limit.");
DEFINE_int64(routing_time_limit, kint64max,
             "Routing: time limit in ms.");
DEFINE_int64(routing_lns_time_limit, 100,
             "Routing: time limit in ms for LNS sub-decisionbuilder.");

// Meta-heuritics
DEFINE_bool(routing_guided_local_search, false, "Routing: use GLS.");
DEFINE_double(routing_guided_local_search_lamda_coefficient, 0.1,
              "Lamda coefficient in GLS.");
DEFINE_bool(routing_simulated_annealing, false,
            "Routing: use simulated annealing.");
DEFINE_bool(routing_tabu_search, false, "Routing: use tabu search.");

// Search control
DEFINE_bool(routing_dfs, false,
            "Routing: use a complete deoth-first search.");
DEFINE_string(routing_first_solution, "",
              "Routing: first solution heuristic;possible values are "
              "Default, GlobalCheapestArc, LocalCheapestArc, PathCheapestArc.");
DEFINE_bool(routing_use_first_solution_dive, false,
            "Dive (left-branch) for first solution.");
DEFINE_int64(routing_optimization_step, 1, "Optimization step.");

// Filtering control

DEFINE_bool(routing_use_objective_filter, true,
            "Use objective filter to speed up local search.");
DEFINE_bool(routing_use_path_cumul_filter, true,
            "Use PathCumul constraint filter to speed up local search.");

// Misc
DEFINE_bool(routing_cache_callbacks, false, "Cache callback calls.");
DEFINE_int64(routing_max_cache_size, 1000,
             "Maximum cache size when callback caching is on.");
DEFINE_bool(routing_trace, false, "Routing: trace search.");
DEFINE_bool(routing_use_homogeneous_costs, true,
            "Routing: use homogeneous cost model when possible.");

namespace operations_research {

// Cached callbacks

class RoutingCache {
 public:
  RoutingCache(Solver::IndexEvaluator2* callback, int size) :
      cache_(size), callback_(callback) {
    callback->CheckIsRepeatable();
  }
  int64 Run(int64 i, int64 j) {
    // This method does lazy caching of results of callbacks: first
    // checks if it has been run with these parameters before, and
    // returns previous result if so, or runs underlaying callback and
    // stores its result.
    int64 cached_value = 0;
    if (!FindCopy(cache_[i], j, &cached_value)) {
      cached_value = callback_->Run(i, j);
      cache_[i][j] = cached_value;
    }
    return cached_value;
  }
 private:
  std::vector<hash_map<int, int64> > cache_;
  scoped_ptr<Solver::IndexEvaluator2> callback_;
};

namespace {

// PathCumul filter
// TODO(user): Move this to local_search.cc.

class PathCumulFilter : public IntVarLocalSearchFilter {
 public:
  // Does not take ownership of evaluator.
  PathCumulFilter(const IntVar* const* nexts,
                  int nexts_size,
                  const IntVar* const* cumuls,
                  int cumuls_size,
                  Solver::IndexEvaluator2* evaluator,
                  const string& name);
  virtual ~PathCumulFilter() {}
  virtual bool Accept(const Assignment* delta,
                      const Assignment* deltadelta);
  virtual void OnSynchronize();
 private:
  scoped_array<IntVar*> cumuls_;
  const int cumuls_size_;
  std::vector<int64> saved_nexts_;
  std::vector<int64> node_path_starts_;
  Solver::IndexEvaluator2* const evaluator_;
  const string name_;
  static const int64 kUnassigned;
};

const int64 PathCumulFilter::kUnassigned = -1;

PathCumulFilter::PathCumulFilter(const IntVar* const* nexts,
                                 int nexts_size,
                                 const IntVar* const* cumuls,
                                 int cumuls_size,
                                 Solver::IndexEvaluator2* evaluator,
                                 const string& name)
    : IntVarLocalSearchFilter(nexts, nexts_size),
      cumuls_size_(cumuls_size),
      saved_nexts_(nexts_size),
      node_path_starts_(cumuls_size),
      evaluator_(evaluator),
      name_(name) {
  cumuls_.reset(new IntVar*[cumuls_size_]);
  memcpy(cumuls_.get(), cumuls, cumuls_size_ * sizeof(*cumuls));
}

// Complexity: O(Sum(Length(paths modified)) + #paths modified ^ 2).
// (#paths modified is usually very small).
bool PathCumulFilter::Accept(const Assignment* delta,
                             const Assignment* deltadelta) {
  const Assignment::IntContainer& container = delta->IntVarContainer();
  const int delta_size = container.Size();
  // Determining touched paths.
  std::vector<int64> touched_paths;
  for (int i = 0; i < delta_size; ++i) {
    const IntVarElement& new_element = container.Element(i);
    const IntVar* const var = new_element.Var();
    int64 index = kUnassigned;
    if (FindIndex(var, &index)) {
      const int64 start = node_path_starts_[index];
      if (start != kUnassigned
          && find(touched_paths.begin(), touched_paths.end(), start)
          == touched_paths.end()) {
        touched_paths.push_back(start);
      }
    }
  }
  // Checking feasibility of touched paths.
  const int touched_paths_size = touched_paths.size();
  for (int i = 0; i < touched_paths_size; ++i) {
    int64 node = touched_paths[i];
    int64 cumul = cumuls_[node]->Min();
    while (node < Size()) {
      const IntVar* const next_var = Var(node);
      int64 next = saved_nexts_[node];
      if (container.Contains(next_var)) {
        const IntVarElement& element = container.Element(next_var);
        if (element.Bound()) {
          next = element.Value();
        } else {
          // LNS detected, return true since path was ok up to now.
          return true;
        }
      }
      cumul += evaluator_->Run(node, next);
      if (cumul > cumuls_[next]->Max()) {
        return false;
      }
      cumul = std::max(cumuls_[next]->Min(), cumul);
      node = next;
    }
  }
  return true;
}

void PathCumulFilter::OnSynchronize() {
  const int nexts_size = Size();
  // Detecting path starts, used to track which node belongs to which path.
  std::vector<int64> path_starts;
  Bitmap has_prevs(nexts_size, false);
  for (int i = 0; i < nexts_size; ++i) {
    const int next = Value(i);
    if (next < nexts_size) {
      has_prevs.Set(next, true);
    }
  }
  for (int i = 0; i < nexts_size; ++i) {
    if (!has_prevs.Get(i)) {
      path_starts.push_back(i);
    }
  }
  // Marking unactive nodes (which are not on a path).
  node_path_starts_.assign(cumuls_size_, kUnassigned);
  // Marking nodes on a path and storing next values.
  for (int i = 0; i < path_starts.size(); ++i) {
    const int64 start = path_starts[i];
    int node = start;
    node_path_starts_[node] = start;
    int next = Value(node);
    saved_nexts_[node] = next;
    while (next < nexts_size) {
      node = next;
      node_path_starts_[node] = start;
      next = Value(node);
      saved_nexts_[node] = next;
    }
    saved_nexts_[node] = next;
    node_path_starts_[next] = start;
  }
}

// Evaluators

int64 CostFunction(int64** eval, int64 i, int64 j) {
  return eval[i][j];
}

class MatrixEvaluator : public BaseObject {
 public:
  MatrixEvaluator(const int64* const* values,
                  int nodes,
                  RoutingModel* model)
      : values_(new int64*[nodes]), nodes_(nodes), model_(model) {
    CHECK(values) << "null pointer";
    for (int i = 0; i < nodes_; ++i) {
      values_[i] = new int64[nodes_];
      memcpy(values_[i], values[i], nodes_ * sizeof(*values[i]));
    }
  }
  virtual ~MatrixEvaluator() {
    for (int i = 0; i < nodes_; ++i) {
      delete values_[i];
    }
    delete [] values_;
  }
  int64 Value(int64 i, int64 j) const {
    return values_[model_->IndexToNode(i)][model_->IndexToNode(j)];
  }
 private:
  int64** const values_;
  const int nodes_;
  RoutingModel* const model_;
};

class VectorEvaluator : public BaseObject {
 public:
  VectorEvaluator(const int64* values, int64 nodes, RoutingModel* model)
      : values_(new int64[nodes]), nodes_(nodes), model_(model) {
    CHECK(values) << "null pointer";
    memcpy(values_.get(), values, nodes * sizeof(*values));
  }
  virtual ~VectorEvaluator() {}
  int64 Value(int64 i, int64 j) const;
 private:
  scoped_array<int64> values_;
  const int64 nodes_;
  RoutingModel* const model_;
};

int64 VectorEvaluator::Value(int64 i, int64 j) const {
  const int64 index = model_->IndexToNode(i);
  return values_[index];
}

class ConstantEvaluator : public BaseObject {
 public:
  explicit ConstantEvaluator(int64 value) : value_(value) {}
  virtual ~ConstantEvaluator() {}
  int64 Value(int64 i, int64 j) const { return value_; }
 private:
  const int64 value_;
};

// Left-branch dive branch selector

Solver::DecisionModification LeftDive(Solver* const s) {
  return Solver::KEEP_LEFT;
}

}  // namespace

// ----- Routing model -----

static const int kUnassigned = -1;
static const int64 kNoPenalty = -1;

RoutingModel::RoutingModel(int nodes, int vehicles)
    : solver_(NULL),
      no_cycle_constraint_(NULL),
      costs_(vehicles),
      homogeneous_costs_(FLAGS_routing_use_homogeneous_costs),
      cost_(0),
      fixed_costs_(vehicles),
      nodes_(nodes),
      vehicles_(vehicles),
      starts_(vehicles),
      ends_(vehicles),
      start_end_count_(1),
      is_depot_set_(false),
      closed_(false),
      first_solution_strategy_(ROUTING_DEFAULT_STRATEGY),
      first_solution_evaluator_(NULL),
      metaheuristic_(ROUTING_GREEDY_DESCENT),
      collect_assignments_(NULL),
      solve_db_(NULL),
      improve_db_(NULL),
      restore_assignment_(NULL),
      assignment_(NULL),
      preassignment_(NULL),
      time_limit_ms_(FLAGS_routing_time_limit),
      lns_time_limit_ms_(FLAGS_routing_lns_time_limit),
      limit_(NULL),
      ls_limit_(NULL),
      lns_limit_(NULL) {
  SolverParameters parameters;
  solver_.reset(new Solver("Routing", parameters));
  Initialize();
}

RoutingModel::RoutingModel(int nodes,
                           int vehicles,
                           const std::vector<pair<int, int> >& start_end)
    : solver_(NULL),
      no_cycle_constraint_(NULL),
      costs_(vehicles),
      homogeneous_costs_(FLAGS_routing_use_homogeneous_costs),
      fixed_costs_(vehicles),
      nodes_(nodes),
      vehicles_(vehicles),
      starts_(vehicles),
      ends_(vehicles),
      is_depot_set_(false),
      closed_(false),
      first_solution_strategy_(ROUTING_DEFAULT_STRATEGY),
      first_solution_evaluator_(NULL),
      metaheuristic_(ROUTING_GREEDY_DESCENT),
      collect_assignments_(NULL),
      solve_db_(NULL),
      improve_db_(NULL),
      restore_assignment_(NULL),
      assignment_(NULL),
      preassignment_(NULL),
      time_limit_ms_(FLAGS_routing_time_limit),
      lns_time_limit_ms_(FLAGS_routing_lns_time_limit),
      limit_(NULL),
      ls_limit_(NULL),
      lns_limit_(NULL) {
  SolverParameters parameters;
  solver_.reset(new Solver("Routing", parameters));
  CHECK_EQ(vehicles, start_end.size());
  hash_set<int> depot_set;
  for (int i = 0; i < start_end.size(); ++i) {
    depot_set.insert(start_end[i].first);
    depot_set.insert(start_end[i].second);
  }
  start_end_count_ = depot_set.size();
  Initialize();
  SetStartEnd(start_end);
}

RoutingModel::RoutingModel(int nodes,
                           int vehicles,
                           const std::vector<int>& starts,
                           const std::vector<int>& ends)
    : solver_(NULL),
      no_cycle_constraint_(NULL),
      costs_(vehicles),
      homogeneous_costs_(FLAGS_routing_use_homogeneous_costs),
      fixed_costs_(vehicles),
      nodes_(nodes),
      vehicles_(vehicles),
      starts_(vehicles),
      ends_(vehicles),
      is_depot_set_(false),
      closed_(false),
      first_solution_strategy_(ROUTING_DEFAULT_STRATEGY),
      first_solution_evaluator_(NULL),
      metaheuristic_(ROUTING_GREEDY_DESCENT),
      collect_assignments_(NULL),
      solve_db_(NULL),
      improve_db_(NULL),
      restore_assignment_(NULL),
      assignment_(NULL),
      preassignment_(NULL),
      time_limit_ms_(FLAGS_routing_time_limit),
      lns_time_limit_ms_(FLAGS_routing_lns_time_limit),
      limit_(NULL),
      ls_limit_(NULL),
      lns_limit_(NULL) {
  SolverParameters parameters;
  solver_.reset(new Solver("Routing", parameters));
  CHECK_EQ(vehicles, starts.size());
  CHECK_EQ(vehicles, ends.size());
  hash_set<int> depot_set;
  std::vector<pair<int, int> > start_end(starts.size());
  for (int i = 0; i < starts.size(); ++i) {
    depot_set.insert(starts[i]);
    depot_set.insert(ends[i]);
    start_end[i] = std::make_pair(starts[i], ends[i]);
  }
  start_end_count_ = depot_set.size();
  Initialize();
  SetStartEnd(start_end);
}

void RoutingModel::Initialize() {
  const int size = Size();
  // Next variables
  nexts_.reset(solver_->MakeIntVarArray(size,
                                        0,
                                        size + vehicles_ - 1,
                                        "Nexts"));
  solver_->AddConstraint(solver_->MakeAllDifferent(nexts_.get(), size, false));
  // Vehicle variables
  vehicle_vars_.reset(solver_->MakeIntVarArray(size + vehicles_,
                                               0,
                                               vehicles_ - 1,
                                               "Vehicles"));
  // Active variables
  active_.reset(solver_->MakeBoolVarArray(size, "Active"));
  // Cost cache
  cost_cache_.clear();
  cost_cache_.resize(size);
  for (int i = 0; i < size; ++i) {
    CostCacheElement& cache = cost_cache_[i];
    cache.node = kUnassigned;
    cache.vehicle = kUnassigned;
    cache.cost = 0;
  }
  preassignment_ = solver_->RevAlloc(new Assignment(solver_.get()));
}

RoutingModel::~RoutingModel() {
  STLDeleteElements(&routing_caches_);
  STLDeleteElements(&owned_callbacks_);
}

void RoutingModel::AddNoCycleConstraint() {
  AddNoCycleConstraintInternal();
}

void RoutingModel::AddNoCycleConstraintInternal() {
  CheckDepot();
  if (no_cycle_constraint_ == NULL) {
    no_cycle_constraint_ = solver_->MakeNoCycle(nexts_.get(),
                                                active_.get(),
                                                Size());
    solver_->AddConstraint(no_cycle_constraint_);
  }
}

void RoutingModel::AddDimension(Solver::IndexEvaluator2* evaluator,
                                int64 slack_max,
                                int64 capacity,
                                const string& name) {
  CheckDepot();
  IntVar** cumuls = GetOrMakeCumuls(capacity, name);
  const int size = Size();
  IntVar** transits = GetOrMakeTransits(NewCachedCallback(evaluator),
                                        slack_max, capacity, name);
  solver_->AddConstraint(solver_->MakePathCumul(nexts_.get(),
                                                active_.get(),
                                                cumuls,
                                                transits,
                                                size,
                                                size + vehicles_));
  // Start cumuls == 0
  for (int i = 0; i < vehicles_; ++i) {
    solver_->AddConstraint(solver_->MakeEquality(cumuls[Start(i)], Zero()));
  }
}

void RoutingModel::AddConstantDimension(int64 value,
                                        int64 capacity,
                                        const string& name) {
  ConstantEvaluator* evaluator =
      solver_->RevAlloc(new ConstantEvaluator(value));
  AddDimension(NewPermanentCallback(evaluator, &ConstantEvaluator::Value),
               0, capacity, name);
}

void RoutingModel::AddVectorDimension(const int64* values,
                                      int64 capacity,
                                      const string& name) {
  VectorEvaluator* evaluator =
      solver_->RevAlloc(new VectorEvaluator(values, nodes_, this));
  AddDimension(NewPermanentCallback(evaluator, &VectorEvaluator::Value),
               0, capacity, name);
}

void RoutingModel::AddMatrixDimension(const int64* const* values,
                                      int64 capacity,
                                      const string& name) {
  MatrixEvaluator* evaluator =
      solver_->RevAlloc(new MatrixEvaluator(values, nodes_, this));
  AddDimension(NewPermanentCallback(evaluator, &MatrixEvaluator::Value),
               0, capacity, name);
}

void RoutingModel::AddAllActive() {
  for (int i = 0; i < Size(); ++i) {
    if (active_[i]->Max() != 0) {
      active_[i]->SetValue(1);
    }
  }
}

void RoutingModel::SetCost(Solver::IndexEvaluator2* evaluator) {
  evaluator->CheckIsRepeatable();
  Solver::IndexEvaluator2* cached_evaluator = NewCachedCallback(evaluator);
  homogeneous_costs_ = FLAGS_routing_use_homogeneous_costs;
  for (int i = 0; i < vehicles_; ++i) {
    SetVehicleCostInternal(i, cached_evaluator);
  }
}

int64 RoutingModel::GetRouteFixedCost() const {
  return GetVehicleFixedCost(0);
}

void RoutingModel::SetVehicleCost(int vehicle,
                                  Solver::IndexEvaluator2* evaluator) {
  evaluator->CheckIsRepeatable();
  homogeneous_costs_ = false;
  SetVehicleCostInternal(vehicle, NewCachedCallback(evaluator));
}

void RoutingModel::SetVehicleCostInternal(int vehicle,
                                          Solver::IndexEvaluator2* evaluator) {
  CHECK_LT(vehicle, vehicles_);
  costs_[vehicle] = evaluator;
}

void RoutingModel::SetRouteFixedCost(int64 cost) {
  for (int i = 0; i < vehicles_; ++i) {
    SetVehicleFixedCost(i, cost);
  }
}

int64 RoutingModel::GetVehicleFixedCost(int vehicle) const {
  CHECK_LT(vehicle, vehicles_);
  return fixed_costs_[vehicle];
}

void RoutingModel::SetVehicleFixedCost(int vehicle, int64 cost) {
  CHECK_LT(vehicle, vehicles_);
  fixed_costs_[vehicle] = cost;
}

void RoutingModel::AddDisjunction(const std::vector<int64>& nodes) {
  AddDisjunctionInternal(nodes, kNoPenalty);
}

void RoutingModel::AddDisjunction(const std::vector<int64>& nodes, int64 penalty) {
  CHECK_GE(penalty, 0) << "Penalty must be positive";
  AddDisjunctionInternal(nodes, penalty);
}

void RoutingModel::AddDisjunctionInternal(const std::vector<int64>& nodes,
                                          int64 penalty) {
  const int size = disjunctions_.size();
  disjunctions_.resize(size + 1);
  std::vector<int64>& disjunction_nodes = disjunctions_[size].nodes;
  disjunction_nodes.resize(nodes.size());
  for (int i = 0; i < nodes.size(); ++i) {
    CHECK_NE(kUnassigned, node_to_index_[nodes[i]]);
    disjunction_nodes[i] = node_to_index_[nodes[i]];
  }
  disjunctions_[size].penalty = penalty;
  for (int i = 0; i < nodes.size(); ++i) {
    // TODO(user): support multiple disjunction per node
    node_to_disjunction_[node_to_index_[nodes[i]]] = size;
  }
}

IntVar* RoutingModel::CreateDisjunction(int disjunction) {
  const std::vector<int64>& nodes = disjunctions_[disjunction].nodes;
  const int nodes_size = nodes.size();
  std::vector<IntVar*> disjunction_vars(nodes_size + 1);
  for (int i = 0; i < nodes_size; ++i) {
    const int64 node = nodes[i];
    CHECK_LT(node, Size());
    disjunction_vars[i] = ActiveVar(node);
  }
  IntVar* no_active_var = solver_->MakeBoolVar();
  disjunction_vars[nodes_size] = no_active_var;
  solver_->AddConstraint(solver_->MakeSumEquality(disjunction_vars, 1));
  const int64 penalty = disjunctions_[disjunction].penalty;
  if (penalty < 0) {
    no_active_var->SetMax(0);
    return NULL;
  } else {
    return solver_->MakeProd(no_active_var, penalty)->Var();
  }
}

void RoutingModel::AddLocalSearchOperator(LocalSearchOperator* ls_operator) {
  extra_operators_.push_back(ls_operator);
}

void RoutingModel::SetDepot(int depot) {
  std::vector<pair<int, int> > start_end(vehicles_, std::make_pair(depot, depot));
  SetStartEnd(start_end);
}

void RoutingModel::SetStartEnd(const std::vector<pair<int, int> >& start_end) {
  if (is_depot_set_) {
    LOG(WARNING) << "A depot has already been specified, ignoring new ones";
    return;
  }
  CHECK_EQ(start_end.size(), vehicles_);
  const int size = Size();
  hash_set<int> starts;
  hash_set<int> ends;
  for (int i = 0; i < vehicles_; ++i) {
    int start = start_end[i].first;
    int end = start_end[i].second;
    CHECK_GE(start, 0);
    CHECK_GE(end, 0);
    CHECK_LE(start, nodes_);
    CHECK_LE(end, nodes_);
    starts.insert(start);
    ends.insert(end);
  }
  index_to_node_.resize(size + vehicles_);
  node_to_index_.resize(nodes_, kUnassigned);
  int index = 0;
  for (int i = 0; i < nodes_; ++i) {
    if (starts.count(i) != 0 || ends.count(i) == 0) {
      index_to_node_[index] = i;
      node_to_index_[i] = index;
      ++index;
    }
  }
  hash_set<int> node_set;
  index_to_vehicle_.resize(size + vehicles_, kUnassigned);
  for (int i = 0; i < vehicles_; ++i) {
    int start = start_end[i].first;
    if (node_set.count(start) == 0) {
      node_set.insert(start);
      const int start_index = node_to_index_[start];
      starts_[i] = start_index;
      CHECK_NE(kUnassigned, start_index);
      index_to_vehicle_[start_index] = i;
    } else {
      starts_[i] = index;
      index_to_node_[index] = start;
      index_to_vehicle_[index] = i;
      ++index;
    }
  }
  for (int i = 0; i < vehicles_; ++i) {
    int end = start_end[i].second;
    index_to_node_[index] = end;
    ends_[i] = index;
    CHECK_LE(size, index);
    index_to_vehicle_[index] = i;
    ++index;
  }
  for (int i = 0; i < size; ++i) {
    for (int j = 0; j < vehicles_; ++j) {
      // "start" node: nexts_[i] != start
      solver_->AddConstraint(solver_->MakeNonEquality(nexts_[i], starts_[j]));
    }
    // Extra constraint to state a node can't point to itself
    solver_->AddConstraint(solver_->MakeIsDifferentCstCt(nexts_[i],
                                                         i,
                                                         active_[i]));
  }
  is_depot_set_ = true;

  // Logging model information.
  VLOG(1) << "Number of nodes: " << nodes_;
  VLOG(1) << "Number of vehicles: " << vehicles_;
  for (int index = 0; index < index_to_node_.size(); ++index) {
    VLOG(1) << "Variable index " << index
            << " -> Node index " << index_to_node_[index];
  }
  for (int node = 0; node < node_to_index_.size(); ++node) {
    VLOG(2) << "Node index " << node
            << " -> Variable index " << node_to_index_[node];
  }
}

void RoutingModel::CloseModel() {
  if (closed_) {
    LOG(WARNING) << "Model already closed";
    return;
  }
  closed_ = true;

  CheckDepot();

  AddNoCycleConstraintInternal();

  const int size = Size();

  // Vehicle variable constraints
  for (int i = 0; i < vehicles_; ++i) {
    solver_->AddConstraint(solver_->MakeEquality(
        vehicle_vars_[starts_[i]], solver_->MakeIntConst(i)));
    solver_->AddConstraint(solver_->MakeEquality(
        vehicle_vars_[ends_[i]], solver_->MakeIntConst(i)));
  }
  std::vector<IntVar*> zero_transit(size, solver_->MakeIntConst(Zero()));
  solver_->AddConstraint(solver_->MakePathCumul(nexts_.get(),
                                                active_.get(),
                                                vehicle_vars_.get(),
                                                zero_transit.data(),
                                                size,
                                                size + vehicles_));

  // Set all active unless there are disjunctions
  if (disjunctions_.size() == 0) {
    AddAllActive();
  }

  // Associate first and "logical" last nodes
  for (int i = 0; i < vehicles_; ++i) {
    for (int j = 0; j < vehicles_; ++j) {
      if (i != j) {
        nexts_[starts_[i]]->RemoveValue(ends_[j]);
      }
    }
  }

  std::vector<IntVar*> cost_elements;
  // Arc costs: the cost of an arc (i, nexts_[i], vehicle_vars_[i]) is
  // costs_(nexts_[i], vehicle_vars_[i]); the total cost is the sum of arc
  // costs.
  for (int i = 0; i < size; ++i) {
    IntExpr* expr = NULL;
    if (homogeneous_costs_) {
      expr = solver_->MakeElement(
          NewPermanentCallback(this,
                               &RoutingModel::GetHomogeneousCost,
                               static_cast<int64>(i)),
          nexts_[i]);
    } else {
      expr = solver_->MakeElement(
          NewPermanentCallback(this,
                               &RoutingModel::GetCost,
                               static_cast<int64>(i)),
          nexts_[i],
          vehicle_vars_[i]);
    }
    IntVar* var = solver_->MakeProd(expr, active_[i])->Var();
    cost_elements.push_back(var);
  }
  // Penalty costs
  for (int i = 0; i < disjunctions_.size(); ++i) {
    IntVar* penalty_var = CreateDisjunction(i);
    if (penalty_var != NULL) {
      cost_elements.push_back(penalty_var);
    }
  }
  cost_ = solver_->MakeSum(cost_elements)->Var();
  cost_->set_name("Cost");

  SetUpSearch();
}

// Flags override strategy selection
RoutingModel::RoutingStrategy
RoutingModel::GetSelectedFirstSolutionStrategy() const {
  if (FLAGS_routing_first_solution.compare("GlobalCheapestArc") == 0) {
    return ROUTING_GLOBAL_CHEAPEST_ARC;
  } else if (FLAGS_routing_first_solution.compare("LocalCheapestArc") == 0) {
    return ROUTING_LOCAL_CHEAPEST_ARC;
  } else if (FLAGS_routing_first_solution.compare("PathCheapestArc") == 0) {
    return ROUTING_PATH_CHEAPEST_ARC;
  }
  return first_solution_strategy_;
}

RoutingModel::RoutingMetaheuristic
RoutingModel::GetSelectedMetaheuristic() const {
  if (FLAGS_routing_tabu_search) {
    return ROUTING_TABU_SEARCH;
  } else if (FLAGS_routing_simulated_annealing) {
    return ROUTING_SIMULATED_ANNEALING;
  } else if (FLAGS_routing_guided_local_search) {
    return ROUTING_GUIDED_LOCAL_SEARCH;
  }
  return metaheuristic_;
}

void RoutingModel::AddSearchMonitor(SearchMonitor* const monitor) {
  monitors_.push_back(monitor);
}

const Assignment* RoutingModel::Solve(const Assignment* assignment) {
  if (!closed_) {
    CloseModel();
  }
  if (NULL == assignment) {
    solver_->Solve(solve_db_, monitors_);
  } else {
    assignment_->Copy(assignment);
    solver_->Solve(improve_db_, monitors_);
  }

  if (collect_assignments_->solution_count() == 1) {
    return collect_assignments_->solution(0);
  } else {
    return NULL;
  }
}

int RoutingModel::FindNextActive(int index, const std::vector<int>& nodes) const {
  ++index;
  CHECK_LE(0, index);
  const int size = nodes.size();
  while (index < size && ActiveVar(nodes[index])->Max() == 0) {
    ++index;
  }
  return index;
}

IntVar* RoutingModel::ApplyLocks(const std::vector<int>& locks) {
  preassignment_->Clear();
  IntVar* next_var = NULL;
  int lock_index = FindNextActive(-1, locks);
  const int size = locks.size();
  if (lock_index < size) {
    next_var = NextVar(locks[lock_index]);
    preassignment_->Add(next_var);
    for (lock_index = FindNextActive(lock_index, locks);
         lock_index < size;
         lock_index = FindNextActive(lock_index, locks)) {
      preassignment_->SetValue(next_var, locks[lock_index]);
      next_var = NextVar(locks[lock_index]);
      preassignment_->Add(next_var);
    }
  }
  return next_var;
}

void RoutingModel::UpdateTimeLimit(int64 limit_ms) {
  time_limit_ms_ = limit_ms;
  if (limit_ != NULL) {
    solver_->UpdateLimits(time_limit_ms_,
                          kint64max,
                          kint64max,
                          FLAGS_routing_solution_limit,
                          limit_);
  }
  if (ls_limit_ != NULL) {
    solver_->UpdateLimits(time_limit_ms_,
                          kint64max,
                          kint64max,
                          1,
                          ls_limit_);
  }
}

void RoutingModel::UpdateLNSTimeLimit(int64 limit_ms) {
  lns_time_limit_ms_ = limit_ms;
  if (lns_limit_ != NULL) {
    solver_->UpdateLimits(lns_time_limit_ms_,
                          kint64max,
                          kint64max,
                          kint64max,
                          lns_limit_);
  }
}

void RoutingModel::SetCommandLineOption(const string& name,
                                        const string& value) {
  google::SetCommandLineOption(name.c_str(), value.c_str());
}


int64 RoutingModel::IndexToNode(int64 index) const {
  DCHECK_LT(index, index_to_node_.size());
  return index_to_node_[index];
}

int64 RoutingModel::NodeToIndex(int64 node) const {
  DCHECK_LT(node, node_to_index_.size());
  DCHECK_NE(node_to_index_[node], kUnassigned)
      << "RoutingModel::NodeToIndex should not be used for Start or End nodes";
  return node_to_index_[node];
}

int64 RoutingModel::GetArcCost(int64 i, int64 j, int64 vehicle) {
  CostCacheElement& cache = cost_cache_[i];
  if (cache.node == j && cache.vehicle == vehicle) {
    return cache.cost;
  }
  const int64 node_i = IndexToNode(i);
  const int64 node_j = IndexToNode(j);
  int64 cost = 0;
  if (!IsStart(i)) {
    cost = costs_[vehicle]->Run(node_i, node_j);
  } else if (!IsEnd(j)) {
    // Apply route fixed cost on first non-first/last node, in other words on
    // the arc from the first node to its next node if it's not the last node.
    cost = costs_[vehicle]->Run(node_i, node_j)
      + fixed_costs_[index_to_vehicle_[i]];
  } else {
    cost = 0;
  }
  cache.node = j;
  cache.vehicle = vehicle;
  cache.cost = cost;
  return cost;
}

int64 RoutingModel::GetPenaltyCost(int64 i) const {
  int index = kUnassigned;
  if (FindCopy(node_to_disjunction_, i, &index)) {
    const Disjunction& disjunction = disjunctions_[index];
    int64 penalty = disjunction.penalty;
    if (penalty > 0 && disjunction.nodes.size() == 1) {
      return penalty;
    } else {
      return 0;
    }
  } else {
    return 0;
  }
}

bool RoutingModel::IsStart(int64 index) const {
  return !IsEnd(index) && index_to_vehicle_[index] != kUnassigned;
}

int64 RoutingModel::GetCost(int64 i, int64 j, int64 vehicle) {
  if (i != j) {
    return GetArcCost(i, j, vehicle);
  } else {
    return 0;
  }
}

int64 RoutingModel::GetFilterCost(int64 i, int64 j, int64 vehicle) {
  if (i != j) {
    return GetArcCost(i, j, vehicle);
  } else {
    return GetPenaltyCost(i);
  }
}

// Return high cost if connecting to end node; used in cost-based first solution
int64 RoutingModel::GetFirstSolutionCost(int64 i, int64 j) {
  if (j < nodes_) {
    // TODO(user): Take vehicle into account.
    return GetCost(i, j, 0);
  } else {
    return kint64max;
  }
}

void RoutingModel::CheckDepot() {
  if (!is_depot_set_) {
    LOG(WARNING) << "A depot must be specified, setting one at node 0";
    SetDepot(0);
  }
}

#define CP_ROUTING_PUSH_BACK_OPERATOR(operator_type)                    \
  if (homogeneous_costs_) {                                             \
    operators.push_back(solver_->MakeOperator(nexts_.get(),             \
                                              size,                     \
                                              operator_type));          \
  } else {                                                              \
    operators.push_back(solver_->MakeOperator(nexts_.get(),             \
                                              vehicle_vars_.get(),      \
                                              size,                     \
                                              operator_type));          \
  }

#define CP_ROUTING_PUSH_BACK_CALLBACK_OPERATOR(operator_type)           \
  if (homogeneous_costs_) {                                             \
    operators.push_back(solver_->MakeOperator(nexts_.get(),             \
                                              size,                     \
                                              BuildCostCallback(),      \
                                              operator_type));          \
  } else {                                                              \
    operators.push_back(solver_->MakeOperator(nexts_.get(),             \
                                              vehicle_vars_.get(),      \
                                              size,                     \
                                              BuildCostCallback(),      \
                                              operator_type));          \
  }

void RoutingModel::SetUpSearch() {
  const int size = Size();
  assignment_ = solver_->RevAlloc(new Assignment(solver_.get()));
  assignment_->Add(nexts_.get(), size);
  if (!homogeneous_costs_) {
    assignment_->Add(vehicle_vars_.get(), size + vehicles_);
  }
  assignment_->AddObjective(cost_);

  Assignment* full_assignment =
      solver_->RevAlloc(new Assignment(solver_.get()));
  for (ConstIter<VarMap> it(cumuls_); !it.at_end(); ++it) {
    full_assignment->Add(it->second, size + vehicles_);
  }
  for (int i = 0; i < extra_vars_.size(); ++i) {
    full_assignment->Add(extra_vars_[i]);
  }
  full_assignment->Add(nexts_.get(), size);
  full_assignment->Add(active_.get(), size);
  full_assignment->Add(vehicle_vars_.get(), size + vehicles_);
  full_assignment->AddObjective(cost_);

  collect_assignments_ =
      solver_->MakeBestValueSolutionCollector(full_assignment, false);
  monitors_.push_back(collect_assignments_);

  SearchMonitor* optimize;
  switch (GetSelectedMetaheuristic()) {
    case ROUTING_GUIDED_LOCAL_SEARCH:
      LG << "Using Guided Local Search";
      if (homogeneous_costs_) {
        optimize = solver_->MakeGuidedLocalSearch(
            false,
            cost_,
            NewPermanentCallback(this, &RoutingModel::GetHomogeneousCost),
            FLAGS_routing_optimization_step,
            nexts_.get(), size,
            FLAGS_routing_guided_local_search_lamda_coefficient);
      } else {
        optimize = solver_->MakeGuidedLocalSearch(
            false,
            cost_,
            BuildCostCallback(),
            FLAGS_routing_optimization_step,
            nexts_.get(), vehicle_vars_.get(), size,
            FLAGS_routing_guided_local_search_lamda_coefficient);
      }
      break;
    case ROUTING_SIMULATED_ANNEALING:
      LG << "Using Simulated Annealing";
      optimize =
          solver_->MakeSimulatedAnnealing(false, cost_,
                                          FLAGS_routing_optimization_step,
                                          100);
      break;
    case ROUTING_TABU_SEARCH:
      LG << "Using Tabu Search";
      optimize = solver_->MakeTabuSearch(false, cost_,
                                         FLAGS_routing_optimization_step,
                                         nexts_.get(), size,
                                         10, 10, .8);
      break;
    default:
      LG << "Using greedy descent";
      optimize = solver_->MakeMinimize(cost_, FLAGS_routing_optimization_step);
  }
  monitors_.push_back(optimize);

  limit_ = solver_->MakeLimit(time_limit_ms_,
                              kint64max,
                              kint64max,
                              FLAGS_routing_solution_limit,
                              true);
  monitors_.push_back(limit_);

  ls_limit_ = solver_->MakeLimit(time_limit_ms_,
                                 kint64max,
                                 kint64max,
                                 1,
                                 true);

  lns_limit_ = solver_->MakeLimit(lns_time_limit_ms_,
                                  kint64max,
                                  kint64max,
                                  kint64max);

  std::vector<LocalSearchOperator*> operators = extra_operators_;
  if (vehicles_ > 1) {
    if (!FLAGS_routing_no_relocate) {
      CP_ROUTING_PUSH_BACK_OPERATOR(Solver::RELOCATE);
    }
    if (!FLAGS_routing_no_exchange) {
      CP_ROUTING_PUSH_BACK_OPERATOR(Solver::EXCHANGE);
    }
    if (!FLAGS_routing_no_cross) {
      CP_ROUTING_PUSH_BACK_OPERATOR(Solver::CROSS);
    }
  }
  if (!FLAGS_routing_no_lkh
      && !FLAGS_routing_tabu_search
      && !FLAGS_routing_simulated_annealing) {
    CP_ROUTING_PUSH_BACK_CALLBACK_OPERATOR(Solver::LK);
  }
  if (!FLAGS_routing_no_2opt) {
    CP_ROUTING_PUSH_BACK_OPERATOR(Solver::TWOOPT);
  }
  if (!FLAGS_routing_no_oropt) {
    CP_ROUTING_PUSH_BACK_OPERATOR(Solver::OROPT);
  }
  if (!FLAGS_routing_no_make_active && disjunctions_.size() != 0) {
    CP_ROUTING_PUSH_BACK_OPERATOR(Solver::MAKEINACTIVE);
    CP_ROUTING_PUSH_BACK_OPERATOR(Solver::MAKEACTIVE);
    if (!FLAGS_routing_use_extended_swap_active) {
      CP_ROUTING_PUSH_BACK_OPERATOR(Solver::SWAPACTIVE);
    } else {
      CP_ROUTING_PUSH_BACK_OPERATOR(Solver::EXTENDEDSWAPACTIVE);
    }
  }
  // TODO(user): move the following operators to a second local search loop.
  if (!FLAGS_routing_no_tsp
      && !FLAGS_routing_tabu_search
      && !FLAGS_routing_simulated_annealing) {
    CP_ROUTING_PUSH_BACK_CALLBACK_OPERATOR(Solver::TSPOPT);
  }
  if (!FLAGS_routing_no_tsplns
      && !FLAGS_routing_tabu_search
      && !FLAGS_routing_simulated_annealing) {
    CP_ROUTING_PUSH_BACK_CALLBACK_OPERATOR(Solver::TSPLNS);
  }
  if (!FLAGS_routing_no_lns) {
    CP_ROUTING_PUSH_BACK_OPERATOR(Solver::PATHLNS);
    if (disjunctions_.size() != 0) {
      CP_ROUTING_PUSH_BACK_OPERATOR(Solver::UNACTIVELNS);
    }
  }
  LocalSearchOperator* local_search_operator =
      solver_->ConcatenateOperators(operators);

  DecisionBuilder* finalize_solution =
      solver_->MakePhase(nexts_.get(), size,
                         Solver::CHOOSE_FIRST_UNBOUND,
                         Solver::ASSIGN_MIN_VALUE);

  DecisionBuilder* first_solution = finalize_solution;
  switch (GetSelectedFirstSolutionStrategy()) {
    case ROUTING_GLOBAL_CHEAPEST_ARC:
      LG << "Using ROUTING_GLOBAL_CHEAPEST_ARC";
      first_solution =
          solver_->MakePhase(nexts_.get(), size,
                             NewPermanentCallback(
                                 this,
                                 &RoutingModel::GetFirstSolutionCost),
                             Solver::CHOOSE_STATIC_GLOBAL_BEST);
      break;
    case ROUTING_LOCAL_CHEAPEST_ARC:
      LG << "Using ROUTING_LOCAL_CHEAPEST_ARC";
      first_solution =
          solver_->MakePhase(nexts_.get(), size,
                             Solver::CHOOSE_FIRST_UNBOUND,
                             NewPermanentCallback(
                                 this,
                                 &RoutingModel::GetFirstSolutionCost));
      break;
    case ROUTING_PATH_CHEAPEST_ARC:
      LG << "Using ROUTING_PATH_CHEAPEST_ARC";
      first_solution =
          solver_->MakePhase(nexts_.get(), size,
                             Solver::CHOOSE_PATH,
                             NewPermanentCallback(
                                 this,
                                 &RoutingModel::GetFirstSolutionCost));
      break;
    case ROUTING_EVALUATOR_STRATEGY:
      LG << "Using ROUTING_EVALUATOR_STRATEGY";
      CHECK(first_solution_evaluator_ != NULL);
      first_solution =
          solver_->MakePhase(nexts_.get(), size,
                             Solver::CHOOSE_PATH,
                             NewPermanentCallback(
                                 first_solution_evaluator_.get(),
                                 &Solver::IndexEvaluator2::Run));
      break;
    case ROUTING_DEFAULT_STRATEGY:
      LG << "Using DEFAULT";
      break;
    default:
      LOG(WARNING) << "Unknown argument for routing_first_solution, "
          "using default";
  }
  if (FLAGS_routing_use_first_solution_dive) {
    DecisionBuilder* apply =
        solver_->MakeApplyBranchSelector(NewPermanentCallback(&LeftDive));
    first_solution = solver_->Compose(apply, first_solution);
  }

  std::vector<LocalSearchFilter*> filters;
  if (FLAGS_routing_use_objective_filter) {
    if (homogeneous_costs_) {
      LocalSearchFilter* filter =
          solver_->MakeLocalSearchObjectiveFilter(
              nexts_.get(),
              size,
              NewPermanentCallback(this,
                                   &RoutingModel::GetHomogeneousFilterCost),
              cost_,
              Solver::EQ,
              Solver::SUM);
      filters.push_back(filter);
    } else {
      LocalSearchFilter* filter =
          solver_->MakeLocalSearchObjectiveFilter(
              nexts_.get(),
              vehicle_vars_.get(),
              size,
              NewPermanentCallback(this, &RoutingModel::GetFilterCost),
              cost_,
              Solver::EQ,
              Solver::SUM);
      filters.push_back(filter);
    }
  }
  if (FLAGS_routing_use_path_cumul_filter) {
    for (ConstIter<VarMap> iter(cumuls_); !iter.at_end(); ++iter) {
      const string& name = iter->first;
      filters.push_back(solver_->RevAlloc(
          new PathCumulFilter(nexts_.get(),
                              Size(),
                              iter->second,
                              Size() + vehicles_,
                              transit_evaluators_[name],
                              name)));
    }
  }

  LocalSearchPhaseParameters* parameters =
      solver_->MakeLocalSearchPhaseParameters(
          local_search_operator,
          solver_->MakeSolveOnce(finalize_solution, lns_limit_),
          ls_limit_,
          filters);

  if (FLAGS_routing_dfs) {
    solve_db_ = finalize_solution;
  } else {
    if (homogeneous_costs_) {
      solve_db_ = solver_->MakeLocalSearchPhase(nexts_.get(),
                                                size,
                                                first_solution,
                                                parameters);
    } else {
      const int all_size = size + size + vehicles_;
      scoped_array<IntVar*> all_vars(new IntVar*[all_size]);
      for (int i = 0; i < size; ++i) {
        all_vars[i] = nexts_[i];
      }
      for (int i = size; i < all_size; ++i) {
        all_vars[i] = vehicle_vars_[i - size];
      }
      solve_db_ = solver_->MakeLocalSearchPhase(all_vars.get(),
                                                all_size,
                                                first_solution,
                                                parameters);
    }
  }
  DecisionBuilder* restore_preassignment =
      solver_->MakeRestoreAssignment(preassignment_);
  solve_db_ = solver_->Compose(restore_preassignment, solve_db_);
  improve_db_ =
      solver_->Compose(restore_preassignment,
                       solver_->MakeLocalSearchPhase(assignment_, parameters));
  restore_assignment_ =
      solver_->Compose(solver_->MakeRestoreAssignment(assignment_),
                       finalize_solution);

  if (FLAGS_routing_trace) {
    const int kLogPeriod = 10000;
    SearchMonitor* trace = solver_->MakeSearchLog(kLogPeriod, cost_);
    monitors_.push_back(trace);
  }
}

#undef CP_ROUTING_PUSH_BACK_CALLBACK_OPERATOR
#undef CP_ROUTING_PUSH_BACK_OPERATOR

IntVar* RoutingModel::CumulVar(int64 node, const string& name) const {
  IntVar** named_cumuls = FindPtrOrNull(cumuls_, name);
  if (named_cumuls != NULL) {
    return named_cumuls[node];
  } else {
    return NULL;
  }
}

IntVar* RoutingModel::TransitVar(int64 node, const string& name) const {
  IntVar** named_transits = FindPtrOrNull(transits_, name);
  if (named_transits != NULL) {
    return named_transits[node];
  } else {
    return NULL;
  }
}

void RoutingModel::AddToAssignment(IntVar* var) {
  extra_vars_.push_back(var);
}

Solver::IndexEvaluator2* RoutingModel::NewCachedCallback(
    Solver::IndexEvaluator2* callback) {
  const int size = Size() + vehicles_;
  if (FLAGS_routing_cache_callbacks && size <= FLAGS_routing_max_cache_size) {
    routing_caches_.push_back(new RoutingCache(callback, size));
    Solver::IndexEvaluator2* cached_evaluator =
        NewPermanentCallback(routing_caches_.back(), &RoutingCache::Run);
    owned_callbacks_.insert(cached_evaluator);
    return cached_evaluator;
  } else {
    owned_callbacks_.insert(callback);
    return callback;
  }
}

Solver::IndexEvaluator3* RoutingModel::BuildCostCallback() {
  return NewPermanentCallback(this, &RoutingModel::GetCost);
}

IntVar** RoutingModel::GetOrMakeCumuls(int64 capacity, const string& name) {
  IntVar** named_cumuls = FindPtrOrNull(cumuls_, name);
  if (named_cumuls == NULL) {
    std::vector<IntVar*> cumuls;
    const int size = Size() + vehicles_;
    solver_->MakeIntVarArray(size, 0LL, capacity, name, &cumuls);
    IntVar** cumul_array = solver_->RevAlloc(new IntVar*[size]);
    memcpy(cumul_array, cumuls.data(), cumuls.size() * sizeof(*cumuls.data()));
    cumuls_[name] = cumul_array;
    return cumul_array;
  }
  return named_cumuls;
}

int64 RoutingModel::WrappedEvaluator(Solver::IndexEvaluator2* evaluator,
                                     int64 from,
                                     int64 to) {
  return evaluator->Run(IndexToNode(from), IndexToNode(to));
}

IntVar** RoutingModel::GetOrMakeTransits(Solver::IndexEvaluator2* evaluator,
                                         int64 slack_max,
                                         int64 capacity,
                                         const string& name) {
  evaluator->CheckIsRepeatable();
  IntVar** named_transits = FindPtrOrNull(transits_, name);
  if (named_transits == NULL) {
    std::vector<IntVar*> transits;
    const int size = Size();
    IntVar** transit_array = solver_->RevAlloc(new IntVar*[size]);
    for (int i = 0; i < size; ++i) {
      IntVar* fixed_transit =
          solver_->MakeElement(NewPermanentCallback(
              this,
              &RoutingModel::WrappedEvaluator,
              evaluator,
              static_cast<int64>(i)),
                               nexts_[i])->Var();
      if (slack_max == 0) {
        transit_array[i] = fixed_transit;
      } else {
        IntVar* slack_var = solver_->MakeIntVar(0, slack_max, "slack");
        transit_array[i] = solver_->MakeSum(slack_var, fixed_transit)->Var();
      }
      transit_array[i]->SetMin(0);
      transit_array[i]->SetMax(capacity);
    }
    transits_[name] = transit_array;
    transit_evaluators_[name] =
        NewPermanentCallback(this,
                             &RoutingModel::WrappedEvaluator,
                             evaluator);
    owned_callbacks_.insert(transit_evaluators_[name]);
    owned_callbacks_.insert(evaluator);
    return transit_array;
  }
  return named_transits;
}

}  // namespace operations_research
