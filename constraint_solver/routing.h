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

#ifndef OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_H_
#define OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_H_

#include <stddef.h>
#include "base/hash.h"
#include "base/hash.h"
#include <string>
#include <utility>
#include <vector>

#include "base/callback-types.h"
#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/macros.h"
#include "base/scoped_ptr.h"
#include "base/int-type-indexed-vector.h"
#include "base/int-type.h"
#include "base/hash.h"
#include "constraint_solver/constraint_solver.h"

namespace operations_research {

class LocalSearchOperator;
class RoutingCache;

// The type must be defined outside the class RoutingModel, SWIG does not parse
// it correctly if it's inside.
DEFINE_INT_TYPE(_RoutingModel_NodeIndex, int);

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

  enum Status {
    ROUTING_NOT_SOLVED,
    ROUTING_SUCCESS,
    ROUTING_FAIL,
    ROUTING_FAIL_TIMEOUT
  };

  typedef _RoutingModel_NodeIndex NodeIndex;
  typedef ResultCallback2<int64, NodeIndex, NodeIndex> NodeEvaluator2;

  // Constants with an index of the first node (to be used in for loops for
  // iteration), and a special index to signalize an invalid/unused value.
  static const NodeIndex kFirstNode;
  static const NodeIndex kInvalidNodeIndex;

  // Supposes a single depot. A depot is the start and end node of the route of
  // a vehicle.
  RoutingModel(int nodes, int vehicles);
  // Constructor taking a vector of (start node, end node) pairs for each
  // vehicle route. Used to model multiple depots.
  RoutingModel(int nodes,
               int vehicles,
               const std::vector<std::pair<NodeIndex, NodeIndex> >& start_end);
  // Constructor taking vectors of start nodes and end nodes for each
  // vehicle route. Used to model multiple depots.
  // TODO(user): added to simplify SWIG wrapping. Remove when swigging
  // std::vector<std::pair<int, int> > is ok.
  RoutingModel(int nodes,
               int vehicles,
               const std::vector<NodeIndex>& starts,
               const std::vector<NodeIndex>& ends);
  ~RoutingModel();

  // Model creation

  // DEPRECATED
  // addition of NoCycle constraint; is automatically added in CloseModel().
  // TODO(user): make private
  void AddNoCycleConstraint();

  void AddDimension(NodeEvaluator2* evaluator,
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
  // active. Start and end nodes of any vehicle cannot be part of a disjunction.
  void AddDisjunction(const std::vector<NodeIndex>& nodes);
  // Adds a penalized disjunction constraint on the nodes: at most one of the
  // nodes is active; if none are active a penalty cost is applied (this cost
  // is added to the global cost function).
  // This is equivalent to adding the constraint:
  // p + Sum(i)active[i] == 1, where p is a boolean variable
  // and the following cost to the cost function:
  // p * penalty.
  // "penalty" must be positive.
  void AddDisjunction(const std::vector<NodeIndex>& nodes, int64 penalty);
#if defined(SWIGPYTHON)
  void AddDisjunctionWithPenalty(const std::vector<NodeIndex>& nodes,
                                 int64 penalty) {
    AddDisjunction(nodes, penalty);
  }
#endif  // SWIGPYTHON
  void AddLocalSearchOperator(LocalSearchOperator* ls_operator);
  void SetCost(NodeEvaluator2* evaluator);
  void SetVehicleCost(int vehicle, NodeEvaluator2* evaluator);
  void SetDepot(NodeIndex depot);
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
  // Closes the current routing model; after this method is called, no
  // modification to the model can be done, but RoutesToAssignment becomes
  // available. Note that CloseModel() is automatically called by Solve() and
  // other methods that produce solution.
  void CloseModel();
  // Solves the current routing model; closes the current model.
  const Assignment* Solve(const Assignment* assignment = NULL);
  // Returns current status of the routing model.
  Status status() const { return status_; }
  // Applies lock chain to the next search. Returns next var at the end of the
  // locked chain; this variable is not locked. Assignment containing locks
  // can be obtained by calling PreAssignment().
  IntVar* ApplyLocks(const std::vector<int>& locks);
  // Applies lock chains to all vehicles to the next search. Returns false if
  // the locks do not contain valid routes; expects that the routes do not
  // contain the depots, i.e. there are empty vectors in place of empty routes.
  // If close_routes is set to true, adds the end nodes to the route of each
  // vehicle and deactivates other nodes.
  // Assignment containing the locks can be obtained by calling PreAssignment().
  bool ApplyLocksToAllVehicles(const std::vector<std::vector<NodeIndex> >& locks,
                               bool close_routes);
  // Restores the routes as the current solution. Returns NULL if the solution
  // cannot be restored (routes do not contain a valid solution).
  // Note that calling this method will run the solver to assign values to the
  // dimension variables; this may take considerable amount of time, especially
  // when using dimensions with slack.
  Assignment* ReadAssignmentFromRoutes(const std::vector<std::vector<NodeIndex> >& routes,
                                       bool ignore_inactive_nodes);
  // Fills an assignment from a specification of the routes of the vehicles. The
  // routes are specified as lists of nodes that appear on the routes of the
  // vehicles. The indices of the outer vector in 'routes' correspond to
  // vehicles IDs, the inner vector contain the nodes on the routes for the
  // given vehicle. The inner vectors must not contain the start and end nodes,
  // as these are determined by the routing model.
  // Sets the value of NextVars in the assignment, adding the variables to the
  // assignment if necessary. The method does not touch other variables in the
  // assignment. The method can only be called after the model is closed.
  // With ignore_inactive_nodes set to false, this method will fail (return
  // NULL) in case some of the route contain nodes that are deactivated in the
  // model; when set to true, these nodes will be skipped.
  // Returns true if the route was successfully loaded. However, such assignment
  // still might not be a valid solution to the routing problem due to more
  // complex constraints; it is advisible to call solver()->CheckSolution()
  // afterwards.
  bool RoutesToAssignment(const std::vector<std::vector<NodeIndex> >& routes,
                          bool ignore_inactive_nodes,
                          bool close_routes,
                          Assignment* const assignment) const;
  // Converts the solution in the given assignment to routes for all vehicles.
  // Expects that assignment contains a valid solution (i.e. routes for all
  // vehicles end with an end node for that vehicle).
  void AssignmentToRoutes(const Assignment& assignment,
                          std::vector<std::vector<NodeIndex> >* const routes) const;

  // Returns a compacted version of the given assignment, in which all vehicles
  // with id lower or equal to some N have non-empty routes, and all vehicles
  // with id greater than N have empty routes. Does not take ownership of the
  // returned object.
  // If found, the cost of the compact assignment is the same as in the
  // original assignment and it preserves the values of 'active' variables.
  // Returns NULL if a compact assignment was not found.
  // This method only works in homogenous mode, and it only swaps equivalent
  // vehicles (vehicles with the same start and end nodes). When creating the
  // compact assignment, the empty plan is replaced by the route assigned to the
  // compatible vehicle with the highest id. Note that with more complex
  // constraints on vehicle variables, this method might fail even if a compact
  // solution exists.
  // This method changes the vehicle and dimension variables as necessary.
  // While compacting the solution, only basic checks on vehicle variables are
  // performed; the complete solution is checked at the end and if it is not
  // valid, no attempts to repair it are made (instead, the method returns
  // NULL).
  Assignment* CompactAssignment(const Assignment& assignment) const;

  // Inspection
  int Start(int vehicle) const { return starts_[vehicle]; }
  int End(int vehicle) const { return ends_[vehicle]; }
  bool IsStart(int64 index) const;
  bool IsEnd(int64 index) const { return index >= Size(); }

  int64 GetCost(int64 i, int64 j, int64 vehicle);
  int64 GetHomogeneousCost(int64 i, int64 j) {
    return GetCost(i, j, 0);
  }
  int64 GetFilterCost(int64 i, int64 j, int64 vehicle);
  int64 GetHomogeneousFilterCost(int64 i, int64 j) {
    return GetFilterCost(i, j, 0);
  }
  int64 GetFirstSolutionCost(int64 i, int64 j);
  bool homogeneous_costs() const { return homogeneous_costs_; }

  // Assignment inspection
  int Next(const Assignment& assignment, int index) const;
  bool IsVehicleUsed(const Assignment& assignment, int vehicle) const;

  // Variables
  IntVar** Nexts() const { return nexts_.get(); }
  IntVar** VehicleVars() const { return vehicle_vars_.get(); }
  IntVar* NextVar(int64 index) const { return nexts_[index]; }
  IntVar* ActiveVar(int64 index) const { return active_[index]; }
  IntVar* VehicleVar(int64 index) const { return vehicle_vars_[index]; }
  // Returns the variable created by the AddDimension with the same name
  IntVar* CumulVar(int64 index, const string& name) const;
  IntVar* TransitVar(int64 index, const string& name) const;
  // Add extra variables to assignments
  void AddToAssignment(IntVar* var);

  // Sizes and indices
  int nodes() const { return nodes_; }
  int vehicles() const { return vehicles_; }
  int Size() const { return nodes_ + vehicles_ - start_end_count_; }
  // Returns the node index from an index value resulting fron a next variable.
  NodeIndex IndexToNode(int64 index) const;
  // Returns the variable index from a node value.
  //
  // Should not be used for nodes at the start / end of a route,
  // because of node multiplicity.  These cases return -1, which is
  // considered a failure case.  Clients who need start and end
  // variable indices should use RoutingModel::Start and RoutingModel::End.
  int64 NodeToIndex(NodeIndex node) const;

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
    std::vector<int> nodes;
    int64 penalty;
  };

  struct CostCacheElement {
    NodeIndex node;
    int vehicle;
    int64 cost;
  };

  void Initialize();
  void SetStartEnd(const std::vector<std::pair<NodeIndex, NodeIndex> >& start_end);
  void AddDisjunctionInternal(const std::vector<NodeIndex>& nodes, int64 penalty);
  void AddNoCycleConstraintInternal();
  void SetVehicleCostInternal(int vehicle, NodeEvaluator2* evaluator);
  Assignment* DoRestoreAssignment();
  // Returns NULL if no penalty cost, otherwise returns penalty variable.
  IntVar* CreateDisjunction(int disjunction);
  // Returns the first active node in nodes starting from index + 1.
  int FindNextActive(int index, const std::vector<int>& nodes) const;

  // Checks that all nodes on the route starting at start_index (using the
  // solution stored in assignment) can be visited by the given vehicle.
  bool RouteCanBeUsedByVehicle(const Assignment& assignment,
                               int start_index,
                               int vehicle) const;
  // Replaces the route of unused_vehicle with the route of active_vehicle in
  // compact_assignment. Expects that unused_vehicle is a vehicle with an empty
  // route and that the route of active_vehicle is non-empty. Also expects that
  // 'assignment' contains the original assignment, from which
  // compact_assignment was created.
  // Returns true if the vehicles were successfully swapped; otherwise, returns
  // false.
  bool ReplaceUnusedVehicle(int unused_vehicle,
                            int active_vehicle,
                            Assignment* compact_assignment) const;

  NodeEvaluator2* NewCachedCallback(NodeEvaluator2* callback);
  Solver::IndexEvaluator3* BuildCostCallback();
  void CheckDepot();
  void QuietCloseModel() {
    if (!closed_) {
      CloseModel();
    }
  }
  void SetUpSearch();

  IntVar** GetOrMakeCumuls(int64 capacity, const string& name);
  IntVar** GetOrMakeTransits(NodeEvaluator2* evaluator,
                             int64 slack_max,
                             int64 capacity,
                             const string& name);

  int64 GetArcCost(int64 i, int64 j, int64 vehicle);
  int64 GetPenaltyCost(int64 i) const;
  int64 WrappedEvaluator(NodeEvaluator2* evaluator,
                         int64 from,
                         int64 to);

  // Model
  scoped_ptr<Solver> solver_;
  Constraint* no_cycle_constraint_;
  scoped_array<IntVar*> nexts_;
  scoped_array<IntVar*> vehicle_vars_;
  scoped_array<IntVar*> active_;
  std::vector<NodeEvaluator2*> costs_;
  bool homogeneous_costs_;
  std::vector<CostCacheElement> cost_cache_;
  std::vector<RoutingCache*> routing_caches_;
  std::vector<Disjunction> disjunctions_;
  hash_map<int64, int> node_to_disjunction_;
  IntVar* cost_;
  std::vector<int64> fixed_costs_;
  int nodes_;
  int vehicles_;
  std::vector<NodeIndex> index_to_node_;
  ITIVector<NodeIndex, int> node_to_index_;
  std::vector<int> index_to_vehicle_;
  std::vector<int> starts_;
  std::vector<int> ends_;
  int start_end_count_;
  bool is_depot_set_;
  VarMap cumuls_;
  VarMap transits_;
  hash_map<string, Solver::IndexEvaluator2*> transit_evaluators_;
  bool closed_;
  Status status_;

  // Search data
  RoutingStrategy first_solution_strategy_;
  scoped_ptr<Solver::IndexEvaluator2> first_solution_evaluator_;
  RoutingMetaheuristic metaheuristic_;
  std::vector<SearchMonitor*> monitors_;
  SolutionCollector* collect_assignments_;
  DecisionBuilder* solve_db_;
  DecisionBuilder* improve_db_;
  DecisionBuilder* restore_assignment_;
  Assignment* assignment_;
  Assignment* preassignment_;
  std::vector<IntVar*> extra_vars_;
  std::vector<LocalSearchOperator*> extra_operators_;

  int64 time_limit_ms_;
  int64 lns_time_limit_ms_;
  SearchLimit* limit_;
  SearchLimit* ls_limit_;
  SearchLimit* lns_limit_;

  // Callbacks to be deleted
  hash_set<NodeEvaluator2*> owned_node_callbacks_;
  hash_set<Solver::IndexEvaluator2*> owned_index_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(RoutingModel);
};

}  // namespace operations_research

#endif  // OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_H_
