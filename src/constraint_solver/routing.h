// Copyright 2010-2012 Google
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

// The vehicle routing library lets one model and solve generic vehicle routing
// problems ranging from the Traveling Salesman Problem to more complex
// problems such as the Capacitated Vehicle Routing Problem with Time Windows.
// The objective of a vehicle routing problem is to build routes covering a set
// of nodes minimizing the overall cost of the routes (usually proportional to
// the sum of the lengths of each segment of the routes) while respecting some
// problem-specific constraints (such as the length of a route). A route is
// equivalent to a path connecting nodes, starting/ending at specific
// starting/ending nodes.
// The term "vehicle routing" is historical and the category of problems solved
// is not limited to the routing of vehicles: any problem involving finding
// routes visiting a given number of nodes optimally falls under this category
// of problems, such as finding the optimal sequence in a playlist.
// The literature around vehicle routing problems is extremelly dense but one
// can find some basic introductions in the following links:
// http://en.wikipedia.org/wiki/Travelling_salesman_problem
// http://www.tsp.gatech.edu/history/index.html
// http://en.wikipedia.org/wiki/Vehicle_routing_problem
//
// The vehicle routing library is a vertical layer above the constraint
// programming library (constraint_programming:cp).
// One has access to all underlying constrained variables of the vehicle
// routing model which can therefore be enriched by adding any constraint
// available in the constraint programming library.
// There are two sets of variables available:
// - path variables:
//   * "next(i)" variables representing the immediate successor of the node
//     corresponding to i; use IndexToNode() to get the node corresponding to
//     a "next" variable value; note that node indices are strongly typed
//     integers (cf. base/int-type.h);
//   * "vehicle(i)" variables representing the vehicle route to which the
//     node corresponding to i belongs;
//   * "active(i)" boolean variables, true if the node corresponding to i is
//     visited and false if not; this can be false when nodes are either
//     optional or part of a disjunction;
// - dimension variables, used when one is accumulating quantities along routes,
//   such as weight or volume carried, distance or time:
//   * "cumul(i,d)" variables representing the quantity of dimension d when
//     arriving at the node corresponding to i;
//   * "transit(i,d)" variables representing the quantity of dimension d added
//     after visiting the node corresponding to i.
// Solving the vehicle routing problems is mainly done using approximate methods
// (namely local search,
// cf. http://en.wikipedia.org/wiki/Local_search_(optimization)), potentially
// combined with exact techniques based on dynamic programming and exhaustive
// tree search.
//
// Advanced tips: Flags are available to tune the search used to solve routing
// problems. Here is a quick overview of the ones one might want to modify:
// - Limiting the search for solutions:
//   * routing_solution_limit (default: kint64max): stop the search after
//     finding 'routing_solution_limit' improving solutions;
//   * routing_time_limit (default: kint64max): stop the search after
//     'routing_time_limit' milliseconds;
// - Customizing search:
//   * routing_first_solution (default: select the first node with an unbound
//     successor and connect it to the first available node): selects the
//     heuristic to build a first solution which will then be improved by local
//     search; possible values are GlobalCheapestArc (iteratively connect two
//     nodes which produce the cheapest route segment), LocalCheapestArc (select
//     the first node with an unbound successor and connect it to the node
//     which produces the cheapest route segment), PathCheapestArc (starting
//     from a route "start" node, connect it to the node which produces the
//     cheapest route segment, then extend the route by iterating on the last
//     node added to the route).
//   * Local search neighborhoods:
//     - routing_no_lns (default: false): forbids the use of Large Neighborhood
//       Search (LNS); LNS can find good solutions but is usually very slow.
//       Refer to the description of PATHLNS in the LocalSearchOperators enum
//       in constraint_solver.h for more information.
//     - routing_no_tsp (default: true): forbids the use of exact methods to
//       solve "sub"-traveling salesman problems (TSPs) of the current model
//       (such as sub-parts of a route, or one route in a multiple route
//       problem). Uses dynamic programming to solve such TSPs with a maximum
//       size (in number of nodes) up to cp_local_search_tsp_opt_size (flag with
//       a default value of 13 nodes). It is not activated by default because it
//       can slow down the search.
//   * Meta-heuritics: used to guide the search out of local minima found by
//     local search. Note that, in general, a search with metaheuristics
//     activated never stops, therefore one must specify a search limit.
//     Several types of metaheuristics are provided:
//     - routing_guided_local_search (default: false): activates guided local
//       search (cf. http://en.wikipedia.org/wiki/Guided_Local_Search);
//       this is generally the most efficient metaheuristic for vehicle
//       routing;
//     - routing_simulated_annealing (default: false): activates simulated
//       annealing (cf. http://en.wikipedia.org/wiki/Simulated_annealing);
//     - routing_tabu_search (default: false): activates tabu search (cf.
//       http://en.wikipedia.org/wiki/Tabu_search).
//
// Code sample:
// Here is a simple example solving a traveling salesman problem given a cost
// function callback (returns the cost of a route segment):
//
// - Define a custom distance/cost function from a node to another; in this
//   example just returns the sum of the node indices (note the conversion from
//   the strongly-typed indices to integers):
//
//     int64 MyDistance(RoutingModel::NodeIndex from,
//                      RoutingModel::NodeIndex to) {
//       return (from + to).value();
//     }
//
// - Create a routing model for a given problem size (int number of nodes) and
//   number of routes (here 1):
//
//     RoutingModel routing(...number of nodes..., 1);
//
// - Set the cost function by passing a permanent callback to the distance
//   accessor here. The callback has the following signature:
//   ResultCallback2<int64, int64, int64>.
//
//    routing.SetCost(NewPermanentCallback(MyDistance));
//
// - Find a solution using Solve(), returns a solution if any (owned by
//   routing):
//
//    const Assignment* solution = routing.Solve();
//    CHECK(solution != NULL);
//
// - Inspect the solution cost and route (only one route here:
//
//    LG << "Cost " << solution->ObjectiveValue();
//    const int route_number = 0;
//    for (int64 node = routing.Start(route_number);
//         !routing.IsEnd(node);
//         node = solution->Value(routing.NextVar(node))) {
//      LG << routing.IndexToNode(node);
//    }
//
// More information on the usage of the routing library can be found here:
// More information on the range of vehicle routing problems the library can
// tackle can be found here:
// Keywords: Vehicle Routing, Traveling Salesman Problem, TSP, VRP, CVRPTW, PDP.

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
  // First solution strategies, used as starting point of local search.
  enum RoutingStrategy {
    // Select the first node with an unbound successor and connect it to the
    // first available node.
    // This is equivalent to the CHOOSE_FIRST_UNBOUND strategy combined with
    // ASSIGN_MIN_VALUE (cf. constraint_solver.h).
    ROUTING_DEFAULT_STRATEGY,
    // Iteratively connect two nodes which produce the cheapest route segment.
    ROUTING_GLOBAL_CHEAPEST_ARC,
    // Select the first node with an unbound successor and connect it to the
    // node which produces the cheapest route segment.
    ROUTING_LOCAL_CHEAPEST_ARC,
    // Starting from a route "start" node, connect it to the node which produces
    // the cheapest route segment, then extend the route by iterating on the
    // last node added to the route.
    ROUTING_PATH_CHEAPEST_ARC,
    // Same as ROUTING_PATH_CHEAPEST_ARC, except that arc costs are evaluated
    // using the function passed to RoutingModel::SetFirstSolutionEvaluator().
    ROUTING_EVALUATOR_STRATEGY,
    // Make all node inactive. Only finds a solution if nodes are optional (are
    // element of a disjunction constraint with a finite penalty cost).
    ROUTING_ALL_UNPERFORMED,
    // Iteratively build a solution by inserting nodes at their cheapest (best)
    // position. As of 2/2012, only works on models with optional nodes
    // (with finite penalty costs).
    ROUTING_BEST_INSERTION,
  };

  // Metaheuristics used to guide the search. Apart greedy descent, they will
  // try to escape local minima.
  enum RoutingMetaheuristic {
    // Accepts improving (cost-reducing) local search neighbors until a local
    // minimum is reached. This is the default heuristic.
    ROUTING_GREEDY_DESCENT,
    // Uses guided local search to escape local minima
    // (cf. http://en.wikipedia.org/wiki/Guided_Local_Search); this is
    // generally the most efficient metaheuristic for vehicle routing.
    ROUTING_GUIDED_LOCAL_SEARCH,
    // Uses simulated annealing to escape local minima
    // (cf. http://en.wikipedia.org/wiki/Simulated_annealing).
    ROUTING_SIMULATED_ANNEALING,
    // Uses tabu search to escape local minima
    // (cf. http://en.wikipedia.org/wiki/Tabu_search).
    ROUTING_TABU_SEARCH,
  };

  // Status of the search.
  enum Status {
    // Problem not solved yet (before calling RoutingModel::Solve()).
    ROUTING_NOT_SOLVED,
    // Problem solved successfully after calling RoutingModel::Solve().
    ROUTING_SUCCESS,
    // No solution found to the problem after calling RoutingModel::Solve().
    ROUTING_FAIL,
    // Time limit reached before finding a solution with RoutingModel::Solve().
    ROUTING_FAIL_TIMEOUT
  };

  typedef _RoutingModel_NodeIndex NodeIndex;
  typedef ResultCallback1<int64, int64> VehicleEvaluator;
  typedef ResultCallback2<int64, NodeIndex, NodeIndex> NodeEvaluator2;
  typedef std::vector<std::pair<int, int> > NodePairs;

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

  // Methods to add dimensions to routes; dimensions represent quantities
  // accumulated at nodes along the routes. They represent quantities such as
  // weights or volumes carried along the route, or distance or times.
  // Quantities at a node are represented by "cumul" variables and the increase
  // or decrease of quantities between nodes are represented by "transit"
  // variables. These variables are linked as follows:
  // if j == next(i), cumul(j) = cumul(i) + transit(i) + slack(i)
  // where slack is a positive slack variable (can represent waiting times for
  // a time dimension).

  // Creates a dimension where the transit variable is constrained to be
  // equal to evaluator(i, next(i)); 'slack_max' is the upper bound of the
  // slack variable and 'capacity' is the upper bound of the cumul variables.
  // 'name' is the name used to reference the dimension; this name is used to
  // get cumul and transit variables from the routing model.
  void AddDimension(NodeEvaluator2* evaluator,
                    int64 slack_max,
                    int64 capacity,
                    const string& name);
  void AddDimensionWithVehicleCapacity(NodeEvaluator2* evaluator,
                                       int64 slack_max,
                                       VehicleEvaluator* vehicle_capacity,
                                       const string& name);
  // Creates a dimension where the transit variable is constrained to be
  // equal to 'value'; 'capacity' is the upper bound of the cumul variables.
  // 'name' is the name used to reference the dimension; this name is used to
  // get cumul and transit variables from the routing model.
  void AddConstantDimension(int64 value, int64 capacity, const string& name);
  // Creates a dimension where the transit variable is constrained to be
  // equal to 'values[i]' for node i; 'capacity' is the upper bound of
  // the cumul variables. 'name' is the name used to reference the dimension;
  // this name is used to get cumul and transit variables from the routing
  // model.
  void AddVectorDimension(const int64* values,
                          int64 capacity,
                          const string& name);
  // Creates a dimension where the transit variable is constrained to be
  // equal to 'values[i][next[i]' for node i; 'capacity' is the upper bound of
  // the cumul variables. 'name' is the name used to reference the dimension;
  // this name is used to get cumul and transit variables from the routing
  // model.
  void AddMatrixDimension(const int64* const* values,
                          int64 capacity,
                          const string& name);
  // Constrains all nodes to be active (to belong to a route).
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
  // "penalty" must be positive to make the disjunctionn optional; a negative
  // penalty will force one node of the disjunction to be performed, and
  // therefore p == 0.
  // Note: passing a vector with a single node will model an optional node
  // with a penalty cost if it is not visited.
  void AddDisjunction(const std::vector<NodeIndex>& nodes, int64 penalty);
  // Returns the index of the disjunction to which a node belongs; if it doesn't
  // belong to a disjunction, the function returns false, true otherwise.
  bool GetDisjunctionIndexFromNode(NodeIndex node,
                                   int* disjunction_index) const {
    return GetDisjunctionIndexFromVariableIndex(NodeToIndex(node),
                                                disjunction_index);
  }
  int GetDisjunctionIndexFromVariableIndex(int64 index,
                                           int* disjunction_index) const {
    return FindCopy(node_to_disjunction_, index, disjunction_index);
  }
  // Returns the variable indices of the nodes in the same disjunction as the
  // node corresponding to the variable of index 'index'.
  void GetDisjunctionIndicesFromIndex(int64 index, std::vector<int>* indices) const;
  // Returns the variable indices of the nodes in the disjunction of index
  // 'index'.
  void GetDisjunctionIndices(int index, std::vector<int>* indices) const {
    *indices = disjunctions_[index].nodes;
  }
  // Returns the penalty of the node disjunction of index 'index'.
  int64 GetDisjunctionPenalty(int index) const {
    return disjunctions_[index].penalty;
  }
  // Returns the number of node disjunctions in the model.
  int GetNumberOfDisjunctions() const { return disjunctions_.size(); }
  // Notifies that node1 and node2 form a pair of nodes which should belong
  // to the same route. This methods helps the search find better solutions,
  // especially in the local search phase.
  // It should be called each time you have an equality constraint linking
  // the vehicle variables of two node (including for instance pickup and
  // delivery problems):
  //     Solver* const solver = routing.solver();
  //     solver->AddConstraint(solver->MakeEquality(
  //         routing.VehicleVar(routing.NodeToIndex(node1)),
  //         routing.VehicleVar(routing.NodeToIndex(node2))));
  //     solver->AddPickupAndDelivery(node1, node2);
  //
  // TODO(user): Remove this when model introspection detects linked nodes.
  void AddPickupAndDelivery(NodeIndex node1, NodeIndex node2) {
    pickup_delivery_pairs_.push_back(std::make_pair(NodeToIndex(node1),
                                                    NodeToIndex(node2)));
  }
  // Makes 'depot' the starting node of all routes.
  void SetDepot(NodeIndex depot);
  // Sets the cost function of the model such that the cost of a segment of a
  // route between node 'from' and 'to' is evaluator(from, to), whatever the
  // route or vehicle performing the route.
  void SetCost(NodeEvaluator2* evaluator);
  // Sets the cost function for a given vehicle route.
  void SetVehicleCost(int vehicle, NodeEvaluator2* evaluator);
  // The fixed cost of a route is taken into account if the route is
  // not empty, aka there's at least one node on the route other than the
  // first and last nodes.
  // Gets the fixed cost of all vehicle routes if they are all the same;
  // otherwise returns the fixed cost of the first vehicle route.
  // Deprecated by GetVehicleFixedCost().
  int64 GetRouteFixedCost() const;
  // Sets the fixed cost of all vehicle routes. It is equivalent to calling
  // SetVehicleFixedCost on all vehicle routes.
  void SetRouteFixedCost(int64 cost);
  // Returns the route fixed cost taken into account if the route of the
  // vehicle is not empty, aka there's at least one node on the route other than
  // the first and last nodes.
  int64 GetVehicleFixedCost(int vehicle) const;
  // Sets the fixed cost of one vehicle route.
  void SetVehicleFixedCost(int vehicle, int64 cost);


  // Search
  // Returns the strategy used to build a first solution.
  RoutingStrategy first_solution_strategy() const {
    return first_solution_strategy_;
  }
  // Sets the strategy used to build a first solution.
  void set_first_solution_strategy(RoutingStrategy strategy) {
    first_solution_strategy_ = strategy;
  }
  // Gets/sets the evaluator used when the first solution heuristic is set to
  // ROUTING_EVALUATOR_STRATEGY (variant of ROUTING_PATH_CHEAPEST_ARC using
  // 'evaluator' to sort node segments).
#ifndef SWIG
  Solver::IndexEvaluator2* first_solution_evaluator() const {
    return first_solution_evaluator_.get();
  }
#endif
  // Takes ownership of evaluator.
  void SetFirstSolutionEvaluator(Solver::IndexEvaluator2* evaluator) {
    first_solution_evaluator_.reset(evaluator);
  }
  // If a first solution flag has been set (to a value different than Default),
  // returns the corresponding strategy, otherwise returns the strategy which
  // was set.
  RoutingStrategy GetSelectedFirstSolutionStrategy() const;
  // Adds a local search operator to the set of operators used to solve the
  // vehicle routing problem.
  void AddLocalSearchOperator(LocalSearchOperator* ls_operator);
  // Returns the metaheuristic used.
  RoutingMetaheuristic metaheuristic() const { return metaheuristic_; }
  // Sets the metaheuristic to be used.
  void set_metaheuristic(RoutingMetaheuristic metaheuristic) {
    metaheuristic_ = metaheuristic;
  }
  // If a metaheuristic flag has been set, returns the corresponding
  // metaheuristic, otherwise returns the metaheuristic which was set.
  RoutingMetaheuristic GetSelectedMetaheuristic() const;
  // Adds a search monitor to the search used to solve the routing model.
  void AddSearchMonitor(SearchMonitor* const monitor);
  // Closes the current routing model; after this method is called, no
  // modification to the model can be done, but RoutesToAssignment becomes
  // available. Note that CloseModel() is automatically called by Solve() and
  // other methods that produce solution.
  void CloseModel();
  // Solves the current routing model; closes the current model.
  const Assignment* Solve(const Assignment* assignment = NULL);
  // Computes a lower bound to the routing problem solving a linear assignment
  // problem. The routing model must be closed before calling this method.
  // Note that problems with node disjunction constraints (including optional
  // nodes) and non-homogenous costs are not supported (the method returns 0 in
  // these cases).
  // TODO(user): Add support for non-homogeneous costs and disjunctions.
  int64 ComputeLowerBound();
  // Returns the current status of the routing model.
  Status status() const { return status_; }
  // Applies a lock chain to the next search. 'locks' represents an ordered
  // vector of nodes representing a partial route which will be fixed during the
  // next search; it will constrain next variables such that:
  // next[locks[i]] == locks[i+1].
  // Returns the next variable at the end of the locked chain; this variable is
  // not locked. An assignment containing the locks can be obtained by calling
  // PreAssignment().
  IntVar* ApplyLocks(const std::vector<int>& locks);
  // Applies lock chains to all vehicles to the next search, such that locks[p]
  // is the lock chain for route p. Returns false if the locks do not contain
  // valid routes; expects that the routes do not contain the depots,
  // i.e. there are empty vectors in place of empty routes.
  // If close_routes is set to true, adds the end nodes to the route of each
  // vehicle and deactivates other nodes.
  // An assignment containing the locks can be obtained by calling
  // PreAssignment().
  bool ApplyLocksToAllVehicles(const std::vector<std::vector<NodeIndex> >& locks,
                               bool close_routes);
  // Returns an assignment used to fix some of the variables of the problem.
  // In practice, this assignment locks partial routes of the problem. This
  // can be used in the context of locking the parts of the routes which have
  // already been driven in online routing problems.
  const Assignment* const PreAssignment() const { return preassignment_; }
  // Writes the current solution to a file containing an AssignmentProto.
  // Returns false if the file cannot be opened or if there is no current
  // solution.
  bool WriteAssignment(const string& file_name) const;
  // Reads an assignment from a file and returns the current solution.
  // Returns NULL if the file cannot be opened or if the assignment is not
  // valid.
  Assignment* ReadAssignment(const string& file_name);
  // Restores an assignment as a solution in the routing model and returns the
  // new solution. Returns NULL if the assignment is not valid.
  Assignment* RestoreAssignment(const Assignment& solution);
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
  // Adds an extra variable to the vehicle routing assignment.
  void AddToAssignment(IntVar* const var);


  // Model inspection.
  // Returns the variable index of the starting node of a vehicle route.
  int64 Start(int vehicle) const { return starts_[vehicle]; }
  // Returns the variable index of the ending node of a vehicle route.
  int64 End(int vehicle) const { return ends_[vehicle]; }
  // Returns true if 'index' represents the first node of a route.
  bool IsStart(int64 index) const;
  // Returns true if 'index' represents the last node of a route.
  bool IsEnd(int64 index) const { return index >= Size(); }
  int64 GetFirstSolutionCost(int64 i, int64 j);
  bool homogeneous_costs() const { return homogeneous_costs_; }
  // Assignment inspection
  // Returns the variable index of the node directly after the node
  // corresponding to 'index' in 'assignment'.
  int64 Next(const Assignment& assignment, int64 index) const;
  // Returns true if the route of 'vehicle' is non empty in 'assignment'.
  bool IsVehicleUsed(const Assignment& assignment, int vehicle) const;
  // Variables
#if !defined(SWIG)
  // Returns all next variables of the model, such that Nexts(i) is the next
  // variable of the node corresponding to i.
  const std::vector<IntVar*>& Nexts() const { return nexts_; }
  // Returns all vehicle variables of the model,  such that VehicleVars(i) is
  // the vehicle variable of the node corresponding to i.
  const std::vector<IntVar*>& VehicleVars() const { return vehicle_vars_; }
#endif
  // Returns the next variable of the node corresponding to index.
  IntVar* NextVar(int64 index) const { return nexts_[index]; }
  // Returns the active variable of the node corresponding to index.
  IntVar* ActiveVar(int64 index) const { return active_[index]; }
  // Returns the vehicle variable of the node corresponding to index.
  IntVar* VehicleVar(int64 index) const { return vehicle_vars_[index]; }
  // Returns the cumul variable for the dimension named 'name'.
  IntVar* CumulVar(int64 index, const string& name) const;
  // Returns the transit variable for the dimension named 'name'.
  IntVar* TransitVar(int64 index, const string& name) const;
  // Return the slack variable for the dimension named 'name'.
  IntVar* SlackVar(int64 index, const string& name) const;
  // Returns the global cost variable which is being minimized.
  IntVar* CostVar() const { return cost_; }
  // Returns the cost of the segment between two nodes for a given vehicle
  // route. Input are variable indices of node.
  int64 GetCost(int64 from_index, int64 to_index, int64 vehicle);
  // Returns the cost of the segment between two nodes supposing all vehicle
  // costs are the same (returns the cost for the first vehicle otherwise).
  int64 GetHomogeneousCost(int64 i, int64 j) {
    return GetCost(i, j, 0);
  }
  // Returns the number of different vehicle cost callbacks in the model.
  int GetVehicleCostCount() const { return cost_callback_vehicles_.size(); }

  // Returns the underlying constraint solver. Can be used to add extra
  // constraints and/or modify search algoithms.
  Solver* solver() const { return solver_.get(); }

  // Sizes and indices
  // Returns the number of nodes in the model.
  int nodes() const { return nodes_; }
  // Returns the number of vehicle routes in the model.
  int vehicles() const { return vehicles_; }
  // Returns the number of next variables in the model.
  int64 Size() const { return nodes_ + vehicles_ - start_end_count_; }
  // Returns the node index from an index value resulting fron a next variable.
  NodeIndex IndexToNode(int64 index) const;
  // Returns the variable index from a node value.
  // Should not be used for nodes at the start / end of a route,
  // because of node multiplicity.  These cases return -1, which is
  // considered a failure case.  Clients who need start and end
  // variable indices should use RoutingModel::Start and RoutingModel::End.
  int64 NodeToIndex(NodeIndex node) const;

  // Time limits
  // Returns the current time limit used in the search.
  int64 TimeLimit() const { return time_limit_ms_; }
  // Updates the time limit used in the search.
  void UpdateTimeLimit(int64 limit_ms);
  // Updates the time limit used in the Large Neighborhood search tree.
  void UpdateLNSTimeLimit(int64 limit_ms);

  // Utilities for swig to set flags in python or java.
  void SetCommandLineOption(const string& name, const string& value);

  // Conversion between enums and strings; the Parse*() conversions return true
  // on success and the *Name() conversions return NULL when given unknown
  // values. See the .cc for the name conversions. The rule of thumb is:
  // RoutingModel::ROUTING_PATH_CHEAPEST_ARC <-> "PathCheapestArc".
  static const char* RoutingStrategyName(RoutingStrategy strategy);
  static bool ParseRoutingStrategy(const string& strategy_str,
                                   RoutingStrategy* strategy);
  static const char* RoutingMetaheuristicName(
      RoutingMetaheuristic metaheuristic);
  static bool ParseRoutingMetaheuristic(const string& metaheuristic_str,
                                        RoutingMetaheuristic* metaheuristic);

 private:
  typedef hash_map<string, std::vector<IntVar*> > VarMap;

  // Structure storing node disjunction information (nodes and penalty when
  // unperformed).
  struct Disjunction {
    std::vector<int> nodes;
    int64 penalty;
  };

  // Storage of a cost cache element corresponding to a cost arc ending at
  // 'node' and using a vehicle of the class 'cost_class'.
  struct CostCacheElement {
    NodeIndex node;
    int cost_class;
    int64 cost;
  };

  // Internal methods.
  void Initialize();
  void SetStartEnd(const std::vector<std::pair<NodeIndex, NodeIndex> >& start_end);
  void AddDisjunctionInternal(const std::vector<NodeIndex>& nodes, int64 penalty);
  void AddNoCycleConstraintInternal();
  void AddDimensionWithCapacityInternal(NodeEvaluator2* evaluator,
                                        int64 slack_max,
                                        int64 capacity,
                                        VehicleEvaluator* vehicle_capacity,
                                        const string& name);
  void SetVehicleCostInternal(int vehicle, NodeEvaluator2* evaluator);
  Assignment* DoRestoreAssignment();
  // Returns the cost of the segment between two nodes for a given vehicle
  // class. Input are variable indices of nodes and the vehicle class.
  int64 GetVehicleClassCost(int64 from_index, int64 to_index, int64 cost_class);
  int64 GetSafeVehicleCostClass(int64 vehicle) const {
    DCHECK_LT(0, vehicles_);
    return vehicle >= 0 ? GetVehicleCostClass(vehicle) : 0;
  }
  int64 GetVehicleCostClass(int64 vehicle) const {
    return vehicle_cost_classes_[vehicle];
  }
  void SetVehicleCostClass(int64 vehicle, int64 cost_class) {
    vehicle_cost_classes_[vehicle] = cost_class;
  }
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
  // Sets up search objects, such as decision builders and monitors.
  void SetupSearch();
  // Set of auxiliary methods used to setup the search.
  // TODO(user): Document each auxiliary method.
  Assignment* GetOrCreateAssignment();
  SearchLimit* GetOrCreateLimit();
  SearchLimit* GetOrCreateLocalSearchLimit();
  SearchLimit* GetOrCreateLargeNeighborhoodSearchLimit();
  LocalSearchOperator* CreateInsertionOperator();
  LocalSearchOperator* CreateNeighborhoodOperators();
  const std::vector<LocalSearchFilter*>& GetOrCreateLocalSearchFilters();
  DecisionBuilder* CreateSolutionFinalizer();
  DecisionBuilder* CreateFirstSolutionDecisionBuilder();
  LocalSearchPhaseParameters* CreateLocalSearchParameters();
  DecisionBuilder* CreateLocalSearchDecisionBuilder();
  void SetupDecisionBuilders();
  void SetupMetaheuristics();
  void SetupAssignmentCollector();
  void SetupTrace();
  void SetupSearchMonitors();

  const std::vector<IntVar*>& GetOrMakeCumuls(
      VehicleEvaluator* vehicle_capacity,
      int64 capacity,
      const string& name);
  const std::vector<IntVar*>& GetOrMakeTransits(NodeEvaluator2* evaluator,
                                           int64 slack_max,
                                           const string& name);

  int64 GetArcCost(int64 i, int64 j, int64 cost_class);
  int64 WrappedEvaluator(NodeEvaluator2* evaluator,
                         int64 from,
                         int64 to);
  int64 WrappedVehicleEvaluator(VehicleEvaluator* evaluator,
                                int64 vehicle);

  // Model
  scoped_ptr<Solver> solver_;
  Constraint* no_cycle_constraint_;
  std::vector<IntVar*> nexts_;
  std::vector<IntVar*> vehicle_vars_;
  std::vector<IntVar*> active_;
  std::vector<NodeEvaluator2*> costs_;
  bool homogeneous_costs_;
  std::vector<CostCacheElement> cost_cache_;
  std::vector<RoutingCache*> routing_caches_;
  hash_map<NodeEvaluator2*, std::vector<int> > cost_callback_vehicles_;
  std::vector<int64> vehicle_cost_classes_;
  std::vector<Disjunction> disjunctions_;
  hash_map<int64, int> node_to_disjunction_;
  NodePairs pickup_delivery_pairs_;
  IntVar* cost_;
  std::vector<int64> fixed_costs_;
  int nodes_;
  int vehicles_;
  std::vector<NodeIndex> index_to_node_;
  ITIVector<NodeIndex, int> node_to_index_;
  std::vector<int> index_to_vehicle_;
  std::vector<int64> starts_;
  std::vector<int64> ends_;
  int start_end_count_;
  bool is_depot_set_;
  VarMap cumuls_;
  hash_map<string, VehicleEvaluator*> capacity_evaluators_;
  VarMap transits_;
  VarMap slacks_;
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
  std::vector<LocalSearchFilter*> filters_;

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

// Routing filters, exposed for testing.
LocalSearchFilter* MakeNodeDisjunctionFilter(
    const RoutingModel& routing_model);

}  // namespace operations_research

#endif  // OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_H_
