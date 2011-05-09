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

// Generic routing problem (TSP, VRP) modeller and solver.
// Costs are specified by filling the cost matrix with SetCost().
// Dimension and dimension constraints such as rank, capacity, time or distance
// dimensions are specified using one of the AddDimensionXXX() methods;
// corresponding variables can be retrieved using CumulVar().
// The depot (starting and ending node for paths) is set with SetDepot; by
// default it is set to node 0.
// Solve() solves the routing problem and returns the corresponding assignment.
// Optionally Solve can take an assignment that will be used to build a first
// solution. The model is "closed" after the first call to Solve() and cannot
// be modified afterwards; however Solve can be called more than once.

#ifndef CONSTRAINT_SOLVER_ROUTING_H_
#define CONSTRAINT_SOLVER_ROUTING_H_

#include <stddef.h>
#include <string>
#include <utility>
#include <vector>

#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/scoped_ptr.h"
#include "constraint_solver/constraint_solver.h"

namespace operations_research {

class LocalSearchOperator;
class RoutingCache;

class RoutingModel {
 public:
  enum RoutingStrategy {
    ROUTING_DEFAULT_STRATEGY,  // choose first unbound, assign min value
    ROUTING_GLOBAL_CHEAPEST_ARC,
    ROUTING_LOCAL_CHEAPEST_ARC,
    ROUTING_PATH_CHEAPEST_ARC,
    ROUTING_EVALUATOR_STRATEGY
  };

  enum RoutingMetaheuristic {
    ROUTING_GREEDY_DESCENT,  // default
    ROUTING_GUIDED_LOCAL_SEARCH,
    ROUTING_SIMULATED_ANNEALING,
    ROUTING_TABU_SEARCH
  };

  // Supposes a single depot. A depot is the start and end node of the route of
  // a vehicle.
  RoutingModel(int nodes, int vehicles);
  // Constructor taking a vector of (start node, end node) pairs for each
  // vehicle route. Used to model multiple depots.
  RoutingModel(int nodes,
               int vehicles,
               const vector<pair<int, int> >& start_end);
  // Constructor taking vectors of start nodes and end nodes for each
  // vehicle route. Used to model multiple depots.
  // TODO(user): added to simplify SWIG wrapping. Remove when swigging
  // vector<pair<int, int> > is ok.
  RoutingModel(int nodes,
               int vehicles,
               const vector<int>& starts,
               const vector<int>& ends);
  ~RoutingModel();

  // Model creation

  // DEPRECATED
  // addition of NoCycle constraint; is automatically added in CloseModel().
  // TODO(user): make private
  void AddNoCycleConstraint();

  void AddDimension(Solver::IndexEvaluator2* evaluator,
                    int64 slack_max,
                    int64 capacity,
                    const string& name);
  void AddConstantDimension(int64 value, int64 capacity, const string& name);
  void AddVectorDimension(const int64* values,
                          int64 capacity,
                          const string& name);
  void AddMatrixDimension(const int64* const* values,
                          int64 capacity,
                          const string& name);
  void AddAllActive();
  // Adds a disjunction constraint on the nodes: exactly one of the nodes is
  // active.
  void AddDisjunction(const vector<int64>& nodes);
  // Adds a penalized disjunction constraint on the nodes: at most one of the
  // nodes is active; if none are active a penalty cost is applied (this cost
  // is added to the global cost function).
  // This is equivalent to adding the constraint:
  // p + Sum(i)active[i] == 1, where p is a boolean variable
  // and the following cost to the cost function:
  // p * penalty.
  // "penalty" must be positive.
  void AddDisjunction(const vector<int64>& nodes, int64 penalty);
  void AddLocalSearchOperator(LocalSearchOperator* ls_operator);
  void SetCost(Solver::IndexEvaluator2* evaluator);
  void SetVehicleCost(int vehicle, Solver::IndexEvaluator2* evaluator);
  void SetDepot(int depot);
  // Route fixed cost taken into account if the route is not empty, aka there's
  // at least one node on the route other than the first and last nodes.
  // Cost is applied to all vehicle routes; SetRouteFixedCost is equivalent to
  // calling SetVehicleFixedCost on all vehicles.
  // Deprecated by GetVehicleFixedCost and SetVehicleFixedCost.
  int64 GetRouteFixedCost() const;
  void SetRouteFixedCost(int64 cost);
  // Vehicle fixed cost taken into account if the route of the vehicle is not
  // empty, aka there's at least one node on the route other than the first and
  // last nodes.
  int64 GetVehicleFixedCost(int vehicle) const;
  void SetVehicleFixedCost(int vehicle, int64 cost);

  // Search
  RoutingStrategy first_solution_strategy() const {
    return first_solution_strategy_;
  }
  void set_first_solution_strategy(RoutingStrategy strategy) {
    first_solution_strategy_ = strategy;
  }
  // Gets/sets the evaluator used when the first solution heuristic is set to
  // ROUTING_EVALUATOR_STRATEGY (variant of ROUTING_PATH_CHEAPEST_ARC using
  // 'evaluator' to sort arcs).
#ifndef SWIG
  Solver::IndexEvaluator2* first_solution_evaluator() const {
    return first_solution_evaluator_.get();
  }
#endif
  // Takes ownership of evaluator.
  void SetFirstSolutionEvaluator(Solver::IndexEvaluator2* evaluator) {
    first_solution_evaluator_.reset(evaluator);
  }
  // If a first solution flag has been set (to a value different from Default),
  // returns the corresponding strategy, otherwise returns the strategy which
  // was set.
  RoutingStrategy GetSelectedFirstSolutionStrategy() const;
  RoutingMetaheuristic metaheuristic() const { return metaheuristic_; }
  void set_metaheuristic(RoutingMetaheuristic metaheuristic) {
    metaheuristic_ = metaheuristic;
  }
  // If a metaheuristic flag has been set, returns the corresponding
  // metaheuristic, otherwise returns the metaheuristic which was set.
  RoutingMetaheuristic GetSelectedMetaheuristic() const;
  void AddSearchMonitor(SearchMonitor* const monitor);
  const Assignment* Solve(const Assignment* assignment = NULL);
  // Applies lock chain to the next search. Returns next var at the end of the
  // locked chain; this variable is not locked. Assignment containing locks
  // can be obtained calling PreAssignment().
  IntVar* ApplyLocks(const vector<int>& locks);

  // Inspection
  int Start(int vehicle) const { return starts_[vehicle]; }
  int End(int vehicle) const { return ends_[vehicle]; }
  bool IsStart(int64 index) const;
  bool IsEnd(int64 index) const { return index >= Size(); }

  int64 GetCost(int64 i, int64 j, int64 vehicle);
  int64 GetHomogeneousCost(int64 i, int64 j) {
    return GetCost(i, j , 0);
  }
  int64 GetFilterCost(int64 i, int64 j, int64 vehicle);
  int64 GetHomogeneousFilterCost(int64 i, int64 j) {
    return GetFilterCost(i, j , 0);
  }
  int64 GetFirstSolutionCost(int64 i, int64 j);
  bool homogeneous_costs() const { return homogeneous_costs_; }

  // Variables
  IntVar** Nexts() const { return nexts_.get(); }
  IntVar** VehicleVars() const { return vehicle_vars_.get(); }
  IntVar* NextVar(int64 node) const { return nexts_[node]; }
  IntVar* ActiveVar(int64 node) const { return active_[node]; }
  IntVar* VehicleVar(int64 node) const { return vehicle_vars_[node]; }
  // Returns the variable created by the AddDimension with the same name
  IntVar* CumulVar(int64 node, const string& name) const;
  IntVar* TransitVar(int64 node, const string& name) const;
  // Add extra variables to assignments
  void AddToAssignment(IntVar* var);

  // Sizes and indices
  int nodes() const { return nodes_; }
  int vehicles() const { return vehicles_; }
  int Size() const { return nodes_ + vehicles_ - start_end_count_; }
  // Returns the node index from an index value resulting fron a next variable.
  int64 IndexToNode(int64 index) const;

  Solver* solver() const { return solver_.get(); }
  IntVar* CostVar() const { return cost_; }

  int64 TimeLimit() const { return time_limit_ms_; }
  void UpdateTimeLimit(int64 limit_ms);
  void UpdateLNSTimeLimit(int64 limit_ms);

  const Assignment* const PreAssignment() const { return preassignment_; }

  // Utilities for swig
  void SetCommandLineOption(const string& name, const string& value);
 private:
  typedef hash_map<string, IntVar**> VarMap;
  struct Disjunction {
    vector<int64> nodes;
    int64 penalty;
  };

  struct CostCacheElement {
    int64 node;
    int64 vehicle;
    int64 cost;
  };

  void Initialize();
  void SetStartEnd(const vector<pair<int, int> >& start_end);
  void AddDisjunctionInternal(const vector<int64>& nodes, int64 penalty);
  void AddNoCycleConstraintInternal();
  void SetVehicleCostInternal(int vehicle, Solver::IndexEvaluator2* evaluator);
  // Returns NULL if no penalty cost, otherwise returns penalty variable.
  IntVar* CreateDisjunction(int disjunction);
  // Returns the first active node in nodes starting from index + 1.
  int FindNextActive(int index, const vector<int>& nodes) const;

  Solver::IndexEvaluator2* NewCachedCallback(Solver::IndexEvaluator2* callback);
  Solver::IndexEvaluator3* BuildCostCallback();
  void CheckDepot();
  void CloseModel();
  void SetUpSearch();

  IntVar** GetOrMakeCumuls(int64 capacity, const string& name);
  IntVar** GetOrMakeTransits(Solver::IndexEvaluator2* evaluator,
                             int64 slack_max,
                             int64 capacity,
                             const string& name);

  int64 GetArcCost(int64 i, int64 j, int64 vehicle);
  int64 GetPenaltyCost(int64 i) const;
  int64 WrappedEvaluator(Solver::IndexEvaluator2* evaluator,
                         int64 from,
                         int64 to);

  // Model
  scoped_ptr<Solver> solver_;
  Constraint* no_cycle_constraint_;
  scoped_array<IntVar*> nexts_;
  scoped_array<IntVar*> vehicle_vars_;
  scoped_array<IntVar*> active_;
  vector<Solver::IndexEvaluator2*> costs_;
  bool homogeneous_costs_;
  vector<CostCacheElement> cost_cache_;
  vector<RoutingCache*> routing_caches_;
  vector<Disjunction> disjunctions_;
  hash_map<int64, int> node_to_disjunction_;
  IntVar* cost_;
  vector<int64> fixed_costs_;
  int nodes_;
  int vehicles_;
  vector<int> index_to_node_;
  vector<int> node_to_index_;
  vector<int> index_to_vehicle_;
  vector<int> starts_;
  vector<int> ends_;
  int start_end_count_;
  bool is_depot_set_;
  VarMap cumuls_;
  VarMap transits_;
  hash_map<string, Solver::IndexEvaluator2*> transit_evaluators_;
  bool closed_;

  // Search data
  RoutingStrategy first_solution_strategy_;
  scoped_ptr<Solver::IndexEvaluator2> first_solution_evaluator_;
  RoutingMetaheuristic metaheuristic_;
  vector<SearchMonitor*> monitors_;
  SolutionCollector* collect_assignments_;
  DecisionBuilder* solve_db_;
  DecisionBuilder* improve_db_;
  DecisionBuilder* restore_assignment_;
  Assignment* assignment_;
  Assignment* preassignment_;
  vector<IntVar*> extra_vars_;
  vector<LocalSearchOperator*> extra_operators_;

  int64 time_limit_ms_;
  int64 lns_time_limit_ms_;
  SearchLimit* limit_;
  SearchLimit* ls_limit_;
  SearchLimit* lns_limit_;

  // Callbacks to be deleted
  hash_set<Solver::IndexEvaluator2*> owned_callbacks_;
};

}  // namespace operations_research

#endif  // CONSTRAINT_SOLVER_ROUTING_H_
