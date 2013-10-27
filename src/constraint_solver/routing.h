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
//     integers (cf. base/int_type.h);
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
// TODO(user): Add a section on costs (vehicle arc costs, span costs,
//                disjunctions costs).
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
//    CHECK(solution != nullptr);
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

#include "base/callback.h"
#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/macros.h"
#include "base/scoped_ptr.h"
#include "base/int_type_indexed_vector.h"
#include "base/int_type.h"
#include "base/hash.h"
#include "constraint_solver/constraint_solver.h"


namespace operations_research {

class LocalSearchOperator;
class RoutingCache;
class RoutingDimension;
#ifndef SWIG
class SweepArranger;
#endif
struct SweepNode;
struct VehicleClass;

// The type must be defined outside the class RoutingModel, SWIG does not parse
// it correctly if it's inside.
DEFINE_INT_TYPE(_RoutingModel_NodeIndex, int);
DEFINE_INT_TYPE(_RoutingModel_DimensionIndex, int);
DEFINE_INT_TYPE(_RoutingModel_DisjunctionIndex, int);

// This class
struct RoutingSearchParameters {
  RoutingSearchParameters() {
    no_lns = false;
    no_fullpathlns = true;
    no_relocate = false;
    no_relocate_neighbors = true;
    no_exchange = false;
    no_cross = false;
    no_2opt = false;
    no_oropt = false;
    no_make_active = false;
    no_lkh = false;
    no_tsp = true;
    no_tsplns = true;
    use_chain_make_inactive = false;
    use_extended_swap_active = false;
    solution_limit = kint64max;
    time_limit = kint64max;
    lns_time_limit = 100;
    guided_local_search = false;
    guided_local_search_lamda_coefficient = 0.1;
    simulated_annealing = false;
    tabu_search = false;
    dfs = false;
    first_solution = "";
    use_first_solution_dive = false;
    optimization_step = 1;
    trace = false;
  }

  // ----- Neighborhood deactivation -----

  // Routing: forbids use of Large Neighborhood Search.
  bool no_lns;
  // Routing: forbids use of Full-path Large Neighborhood Search.
  bool no_fullpathlns;
  // Routing: forbids use of Relocate neighborhood.
  bool no_relocate;
  // Routing: forbids use of RelocateNeighbors neighborhood.
  bool no_relocate_neighbors;
  // Routing: forbids use of Exchange neighborhood.
  bool no_exchange;
  // Routing: forbids use of Cross neighborhood.
  bool no_cross;
  // Routing: forbids use of 2Opt neighborhood.
  bool no_2opt;
  // Routing: forbids use of OrOpt neighborhood.
  bool no_oropt;
  // Routing: forbids use of MakeActive/SwapActive/MakeInactive neighborhoods.
  bool no_make_active;
  // Routing: forbids use of LKH neighborhood.
  bool no_lkh;
  // Routing: forbids use of TSPOpt neighborhood.
  bool no_tsp;
  // Routing: forbids use of TSPLNS neighborhood.
  bool no_tsplns;
  // Routing: use chain version of MakeInactive neighborhood.
  bool use_chain_make_inactive;
  // Routing: use extended version of SwapActive neighborhood.
  bool use_extended_swap_active;

  // ----- Search limits -----

  // Routing: number of solutions limit.
  int64 solution_limit;
  // Routing: time limit in ms.
  int64 time_limit;
  // Routing: time limit in ms for LNS sub-decision builder.
  int64 lns_time_limit;

  // Meta-heuristics

  // Routing: use GLS.
  bool guided_local_search;
  // Lambda coefficient in GLS.
  double guided_local_search_lamda_coefficient;
  // Routing: use simulated annealing.
  bool simulated_annealing;
  // Routing: use tabu search.
  bool tabu_search;

  // ----- Search control ------

  // Routing: use a complete depth-first search.
  bool dfs;
  // Routing: first solution heuristic. See RoutingStrategyName() in
  // the code to get a full list.
  string first_solution;
  // Dive (left-branch) for first solution.
  bool use_first_solution_dive;
  // Optimization step.
  int64 optimization_step;
  // Trace search.
  bool trace;
};

class RoutingModel {
 public:
  // First solution strategies, used as starting point of local search.
  // TODO(user): Remove this when the corresponding enum is available in
  // the routing search parameters protobuf.
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
    // Same as ROUTING_PATH_CHEAPEST_ARC, but arcs are evaluated with a
    // comparison-based selector which will favor the most constrained arc
    // first. See ArcIsMoreConstrainedThanArc() for details.
    ROUTING_PATH_MOST_CONSTRAINED_ARC,
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
    // Savings algorithm (Clarke & Wright).
    // Reference: Clarke, G. & Wright, J.W.:
    // "Scheduling of Vehicles from a Central Depot to a Number of
    // Delivery Points", Operations Research, Vol. 12, 1964, pp. 568-581
    ROUTING_SAVINGS,
    // Sweep algorithm (Wren & Holliday).
    // Reference: Anthony Wren & Alan Holliday: Computer Scheduling of Vehicles
    // from One or More Depots to a Number of Delivery Points
    // Operational Research Quarterly (1970-1977),
    // Vol. 23, No. 3 (Sep., 1972), pp. 333-344
    ROUTING_SWEEP,
    ROUTING_FIRST_SOLUTION_STRATEGY_COUNTER
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
  typedef _RoutingModel_DimensionIndex DimensionIndex;
  typedef _RoutingModel_DisjunctionIndex DisjunctionIndex;
  typedef ResultCallback1<int64, int64> VehicleEvaluator;
  typedef ResultCallback2<int64, NodeIndex, NodeIndex> NodeEvaluator2;
  typedef std::vector<std::pair<int, int> > NodePairs;

  // Constants with an index of the first node (to be used in for loops for
  // iteration), and a special index to signalize an invalid/unused value.
  static const NodeIndex kFirstNode;
  static const NodeIndex kInvalidNodeIndex;

  // Constant used to express the "no disjunction" index, returned when a node
  // does not appear in any disjunction.
  static const DisjunctionIndex kNoDisjunction;

  // Constant used to express the "no dimension" index, returned when a
  // dimension name does not correspond to an actual dimension.
  static const DimensionIndex kNoDimension;

  // Supposes a single depot. A depot is the start and end node of the route of
  // a vehicle.
  RoutingModel(int nodes, int vehicles);
  // Constructor taking a vector of (start node, end node) pairs for each
  // vehicle route. Used to model multiple depots.
  RoutingModel(int nodes, int vehicles,
               const std::vector<std::pair<NodeIndex, NodeIndex> >& start_end);
  // Constructor taking vectors of start nodes and end nodes for each
  // vehicle route. Used to model multiple depots.
  // TODO(user): added to simplify SWIG wrapping. Remove when swigging
  // std::vector<std::pair<int, int> > is ok.
  RoutingModel(int nodes, int vehicles, const std::vector<NodeIndex>& starts,
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
  // Setting the value of fix_start_cumul_to_zero to true will force the "cumul"
  // variable of the start node of all vehicles to be equal to 0.

  // Creates a dimension where the transit variable is constrained to be
  // equal to evaluator(i, next(i)); 'slack_max' is the upper bound of the
  // slack variable and 'capacity' is the upper bound of the cumul variables.
  // 'name' is the name used to reference the dimension; this name is used to
  // get cumul and transit variables from the routing model.
  // Returns false if a dimension with the same name has already been created
  // (and doesn't create the new dimension).
  // Takes ownership of the callback 'evaluator'.
  bool AddDimension(NodeEvaluator2* evaluator, int64 slack_max, int64 capacity,
                    bool fix_start_cumul_to_zero, const string& name);
  // Takes ownership of both 'evaluator' and 'vehicle_capacity' callbacks.
  bool AddDimensionWithVehicleCapacity(NodeEvaluator2* evaluator,
                                       int64 slack_max,
                                       VehicleEvaluator* vehicle_capacity,
                                       bool fix_start_cumul_to_zero,
                                       const string& name);
  // Creates a dimension where the transit variable is constrained to be
  // equal to 'value'; 'capacity' is the upper bound of the cumul variables.
  // 'name' is the name used to reference the dimension; this name is used to
  // get cumul and transit variables from the routing model.
  // Returns false if a dimension with the same name has already been created
  // (and doesn't create the new dimension).
  bool AddConstantDimension(int64 value, int64 capacity,
                            bool fix_start_cumul_to_zero, const string& name);
  // Creates a dimension where the transit variable is constrained to be
  // equal to 'values[i]' for node i; 'capacity' is the upper bound of
  // the cumul variables. 'name' is the name used to reference the dimension;
  // this name is used to get cumul and transit variables from the routing
  // model.
  // Returns false if a dimension with the same name has already been created
  // (and doesn't create the new dimension).
  bool AddVectorDimension(const int64* values, int64 capacity,
                          bool fix_start_cumul_to_zero, const string& name);
  // Creates a dimension where the transit variable is constrained to be
  // equal to 'values[i][next(i)]' for node i; 'capacity' is the upper bound of
  // the cumul variables. 'name' is the name used to reference the dimension;
  // this name is used to get cumul and transit variables from the routing
  // model.
  // Returns false if a dimension with the same name has already been created
  // (and doesn't create the new dimension).
  bool AddMatrixDimension(const int64* const* values, int64 capacity,
                          bool fix_start_cumul_to_zero, const string& name);
  // Outputs the names of all dimensions added to the routing engine.
  void GetAllDimensions(std::vector<string>* dimension_names) const;
  // Returns true if a dimension exists for a given dimension name.
  bool HasDimension(const string& dimension_name) const;
  // Returns a dimension from its name. Dies if the dimension does not exist.
  const RoutingDimension& GetDimensionOrDie(const string& dimension_name) const;
  // Returns a dimension from its name. Returns nullptr if the dimension does
  // not exist.
  RoutingDimension* GetMutableDimension(const string& dimension_name) const;
  // Set the given dimension as "primary constrained". As of August 2013, this
  // is only used by ArcIsMoreConstrainedThanArc().
  // "dimension" must be the name of an existing dimension, or be empty, in
  // which case there will not be a primary dimension after this call.
  void SetPrimaryConstrainedDimension(const string& dimension_name) {
    DCHECK(dimension_name.empty() || HasDimension(dimension_name));
    primary_constrained_dimension_ = dimension_name;
  }
  // Get the primary constrained dimension, or an empty string if it is unset.
  const string& GetPrimaryConstrainedDimension() const {
    return primary_constrained_dimension_;
  }
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
                                   DisjunctionIndex* disjunction_index) const {
    return GetDisjunctionIndexFromVariableIndex(NodeToIndex(node),
                                                disjunction_index);
  }
  bool GetDisjunctionIndexFromVariableIndex(
      int64 index, DisjunctionIndex* disjunction_index) const {
    if (index < node_to_disjunction_.size()) {
      *disjunction_index = node_to_disjunction_[index];
      return *disjunction_index != kNoDisjunction;
    } else {
      return false;
    }
  }
  // Returns the variable indices of the nodes in the same disjunction as the
  // node corresponding to the variable of index 'index'.
  void GetDisjunctionIndicesFromIndex(int64 index, std::vector<int>* indices) const {
    DisjunctionIndex disjunction = kNoDisjunction;
    if (GetDisjunctionIndexFromVariableIndex(index, &disjunction)) {
      *indices = disjunctions_[disjunction].nodes;
    } else {
      indices->clear();
    }
  }
  // Returns the variable indices of the nodes in the disjunction of index
  // 'index'.
  void GetDisjunctionIndices(DisjunctionIndex index,
                             std::vector<int>* indices) const {
    *indices = disjunctions_[index].nodes;
  }
  // Returns the penalty of the node disjunction of index 'index'.
  int64 GetDisjunctionPenalty(DisjunctionIndex index) const {
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
    pickup_delivery_pairs_.push_back(
        std::make_pair(NodeToIndex(node1), NodeToIndex(node2)));
  }
  // Get the "unperformed" penalty of a node. This is only well defined if the
  // node is only part of a single Disjunction involving only itself, and that
  // disjunction has a penalty. In all other cases, including forced active
  // nodes, this returns 0.
  int64 UnperformedPenalty(int64 var_index);
  // Returns the variable index of the first starting or ending node of all
  // routes. If all routes start  and end at the same node (single depot), this
  // is the node returned.
  int64 GetDepot() const;
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
  // Sets a cost proportional to the sum of the transit variables of a given
  // dimension.
  void SetDimensionTransitCost(const string& dimension_name, int64 coefficient);
  // Gets the cost coefficient corresponding to a dimension.
  int64 GetDimensionTransitCost(const string& dimension_name) const;
  // Sets the dimension span cost. This is the cost proportional to the
  // difference between the largest value of route end cumul variables and
  // the smallest value of route start cumul variables.
  // In other words:
  // span_cost =
  //   coefficient * (Max(dimension end value) - Min(dimension start value)).
  // Only positive coefficients are supported.
  void SetDimensionSpanCost(const string& dimension_name, int64 coefficient);
  // Returns the dimension span cost coefficient for the dimension named
  // "dimension".
  int64 GetDimensionSpanCost(const string& dimension_name) const;
  // Sets a soft upper bound to the cumul variable of a given node. If the
  // value of the cumul variable is greater than the bound, a cost proportional
  // to the difference between this value and the bound is added to the cost
  // function of the model:
  // cumulVar <= upper_bound -> cost = 0
  // cumulVar > upper_bound -> cost = coefficient * (cumulVar - upper_bound).
  // This is also handy to model tardiness costs when the dimension represents
  // time.
  void SetCumulVarSoftUpperBound(NodeIndex node, const string& dimension_name,
                                 int64 upper_bound, int64 coefficient);
  // Returns true if a soft upper bound has been set for a given node and a
  // given dimension.
  bool HasCumulVarSoftUpperBound(NodeIndex node,
                                 const string& dimension_name) const;
  // Returns the soft upper bound of a cumul variable for a given node and
  // dimension. The "hard" upper bound of the variable is returned if no soft
  // upper bound has been set.
  int64 GetCumulVarSoftUpperBound(NodeIndex node,
                                  const string& dimension_name) const;
  // Returns the cost coefficient of the soft upper bound of a cumul variable
  // for a given node and dimension. If no soft upper bound has been set, 0 is
  // returned.
  int64 GetCumulVarSoftUpperBoundCoefficient(
      NodeIndex node, const string& dimension_name) const;
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
  // 'evaluator' to std::sort node segments).
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
  const Assignment* Solve(const Assignment* assignment = nullptr);
  // Solves the current routing model with the given parameters.
  // Closes the current model.
  const Assignment* SolveWithParameters(
      const RoutingSearchParameters& parameters,
      const Assignment* assignment);
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
  // Returns nullptr if the file cannot be opened or if the assignment is not
  // valid.
  Assignment* ReadAssignment(const string& file_name);
  // Restores an assignment as a solution in the routing model and returns the
  // new solution. Returns nullptr if the assignment is not valid.
  Assignment* RestoreAssignment(const Assignment& solution);
  // Restores the routes as the current solution. Returns nullptr if
  // the solution cannot be restored (routes do not contain a valid
  // solution).  Note that calling this method will run the solver to
  // assign values to the dimension variables; this may take
  // considerable amount of time, especially when using dimensions
  // with slack.
  Assignment* ReadAssignmentFromRoutes(const std::vector<std::vector<NodeIndex> >& routes,
                                       bool ignore_inactive_nodes);
  // Fills an assignment from a specification of the routes of the
  // vehicles. The routes are specified as lists of nodes that appear
  // on the routes of the vehicles. The indices of the outer vector in
  // 'routes' correspond to vehicles IDs, the inner vector contain the
  // nodes on the routes for the given vehicle. The inner vectors must
  // not contain the start and end nodes, as these are determined by
  // the routing model.  Sets the value of NextVars in the assignment,
  // adding the variables to the assignment if necessary. The method
  // does not touch other variables in the assignment. The method can
  // only be called after the model is closed.  With
  // ignore_inactive_nodes set to false, this method will fail (return
  // nullptr) in case some of the route contain nodes that are
  // deactivated in the model; when set to true, these nodes will be
  // skipped.  Returns true if routes were successfully
  // loaded. However, such assignment still might not be a valid
  // solution to the routing problem due to more complex constraints;
  // it is advisible to call solver()->CheckSolution() afterwards.
  bool RoutesToAssignment(const std::vector<std::vector<NodeIndex> >& routes,
                          bool ignore_inactive_nodes, bool close_routes,
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
  // Returns nullptr if a compact assignment was not found.
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
  // nullptr).
  Assignment* CompactAssignment(const Assignment& assignment) const;
  // Adds an extra variable to the vehicle routing assignment.
  void AddToAssignment(IntVar* const var);
#ifndef SWIG
  // TODO(user): Revisit if coordinates are added to the RoutingModel class.
  void SetSweepArranger(SweepArranger* sweep_arranger) {
    sweep_arranger_.reset(sweep_arranger);
  }
  // Returns the sweep arranger to be used by routing heuristics.
  SweepArranger* sweep_arranger() const { return sweep_arranger_.get(); }
#endif

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
  // Returns all cumul variables of the model for the dimension named "name".
  const std::vector<IntVar*>& CumulVars(const string& dimension_name) const;
#endif
  // Returns the next variable of the node corresponding to index.
  IntVar* NextVar(int64 index) const { return nexts_[index]; }
  // Returns the active variable of the node corresponding to index.
  IntVar* ActiveVar(int64 index) const { return active_[index]; }
  // Returns the vehicle variable of the node corresponding to index.
  IntVar* VehicleVar(int64 index) const { return vehicle_vars_[index]; }
  // Returns the cumul variable for the dimension named 'name'.
  IntVar* CumulVar(int64 index, const string& dimension_name) const;
  // Returns the transit variable for the dimension named 'name'.
  IntVar* TransitVar(int64 index, const string& dimension_name) const;
  // Return the slack variable for the dimension named 'name'.
  IntVar* SlackVar(int64 index, const string& dimension_name) const;
  // Returns the global cost variable which is being minimized.
  IntVar* CostVar() const { return cost_; }
  // Returns the cost of the segment between two nodes for a given vehicle
  // route. Input are variable indices of node.
  int64 GetCost(int64 from_index, int64 to_index, int64 vehicle);
  // Returns the cost of the segment between two nodes supposing all vehicle
  // costs are the same (returns the cost for the first vehicle otherwise).
  int64 GetHomogeneousCost(int64 i, int64 j) { return GetCost(i, j, 0); }
  // Returns the number of different vehicle cost callbacks in the model.
  int GetVehicleCostCount() const { return costs_.size(); }
  // Returns the different types of vehicles in the model.
  void GetVehicleClasses(std::vector<VehicleClass>* vehicle_classes) const;
  // Returns a transition value given a dimension and a pair of nodes; this
  // value is the one taken by the corresponding transit variable when the
  // 'next' variable for 'from_index' is bound to 'to_index'.
  int64 GetTransitValue(const string& dimension_name, int64 from_index,
                        int64 to_index) const;
  // Returns whether the arc from->to1 is more constrained than from->to2,
  // taking into account, in order:
  // - whether the destination node isn't an end node
  // - whether the destination node is mandatory
  // - whether the destination node is bound to the same vehicle as the source
  // - the "primary constrained" dimension (see SetPrimaryConstrainedDimension)
  // It then breaks ties using, in order:
  // - the arc cost (taking unperformed penalties into account)
  // - the size of the vehicle vars of "to1" and "to2" (lowest size wins)
  // - the value: the lowest value of the indices to1 and to2 wins.
  // See the .cc for details.
  // The more constrained arc is typically preferable when building a
  // first solution. This method is intended to be used as a callback for the
  // BestValueByComparisonSelector value selector.
  // Args:
  //   from: the variable index of the source node
  //   to1: the variable index of the first candidate destination node.
  //   to2: the variable index of the second candidate destination node.
  bool ArcIsMoreConstrainedThanArc(int64 from, int64 to1, int64 to2);
  // Print some debugging information about an assignment, including the
  // feasible intervals of the CumulVar for dimension "dimension_to_print"
  // at each step of the routes.
  // If "dimension_to_print" is omitted, all dimensions will be printed.
  string DebugOutputAssignment(const Assignment& solution_assignment,
                               const string& dimension_to_print) const;

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
  // Returns true if the node can be safely converted to variable index. All
  // nodes that are not end of a route are safe.
  bool HasIndex(NodeIndex node) const;

  // Time limits
  // Returns the current time limit used in the search.
  int64 TimeLimit() const { return time_limit_ms_; }
  // Updates the time limit used in the search.
  void UpdateTimeLimit(int64 limit_ms);
  // Updates the time limit used in the Large Neighborhood search tree.
  void UpdateLNSTimeLimit(int64 limit_ms);

  // Conversion between enums and strings; the Parse*() conversions return true
  // on success and the *Name() conversions return nullptr when given unknown
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
  // Local search move operator usable in routing.
  // TODO(user): Remove this when the corresponding enum is available in
  // the routing search parameters protobuf.
  enum RoutingLocalSearchOperator {
    ROUTING_RELOCATE = 0,
    ROUTING_PAIR_RELOCATE,
    ROUTING_RELOCATE_NEIGHBORS,
    ROUTING_EXCHANGE,
    ROUTING_CROSS,
    ROUTING_TWO_OPT,
    ROUTING_OR_OPT,
    ROUTING_LKH,
    ROUTING_TSP_OPT,
    ROUTING_TSP_LNS,
    ROUTING_PATH_LNS,
    ROUTING_FULL_PATH_LNS,
    ROUTING_INACTIVE_LNS,
    ROUTING_MAKE_ACTIVE,
    ROUTING_MAKE_INACTIVE,
    ROUTING_MAKE_CHAIN_INACTIVE,
    ROUTING_SWAP_ACTIVE,
    ROUTING_EXTENDED_SWAP_ACTIVE,
    ROUTING_LOCAL_SEARCH_OPERATOR_COUNTER
  };

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
  bool AddDimensionWithCapacityInternal(NodeEvaluator2* evaluator,
                                        int64 slack_max, int64 capacity,
                                        VehicleEvaluator* vehicle_capacity,
                                        bool fix_start_cumul_to_zero,
                                        const string& dimension_name);
  DimensionIndex GetDimensionIndex(const string& dimension_name) const;
  void SetVehicleCostInternal(int vehicle, NodeEvaluator2* evaluator);
  void ComputeVehicleCostClasses();
  void AppendHomogeneousArcCosts(int node_index,
                                 std::vector<IntVar*>* cost_elements);
  void AppendArcCosts(int node_index, std::vector<IntVar*>* cost_elements);
  Assignment* DoRestoreAssignment();
  // Returns the cost of the segment between two nodes for a given vehicle
  // class. Input are variable indices of nodes and the vehicle class.
  int64 GetVehicleClassCost(int64 from_index, int64 to_index, int64 cost_class);
  int64 GetSafeVehicleCostClass(int64 vehicle) const {
    DCHECK_LT(0, vehicles_);
    return vehicle >= 0 ? GetVehicleCostClass(vehicle) : -1;
  }
  int64 GetVehicleCostClass(int64 vehicle) const {
    DCHECK(closed_);
    return vehicle_cost_classes_[vehicle];
  }
  void SetVehicleCostClass(int64 vehicle, int64 cost_class) {
    vehicle_cost_classes_[vehicle] = cost_class;
  }
  int64 GetDimensionTransitCostSum(int64 i, int64 j) const;
  // Returns nullptr if no penalty cost, otherwise returns penalty variable.
  IntVar* CreateDisjunction(DisjunctionIndex disjunction);
  // Returns the first active node in nodes starting from index + 1.
  int FindNextActive(int index, const std::vector<int>& nodes) const;

  // Checks that all nodes on the route starting at start_index (using the
  // solution stored in assignment) can be visited by the given vehicle.
  bool RouteCanBeUsedByVehicle(const Assignment& assignment, int start_index,
                               int vehicle) const;
  // Replaces the route of unused_vehicle with the route of active_vehicle in
  // compact_assignment. Expects that unused_vehicle is a vehicle with an empty
  // route and that the route of active_vehicle is non-empty. Also expects that
  // 'assignment' contains the original assignment, from which
  // compact_assignment was created.
  // Returns true if the vehicles were successfully swapped; otherwise, returns
  // false.
  bool ReplaceUnusedVehicle(int unused_vehicle, int active_vehicle,
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
  void CreateNeighborhoodOperators();
  LocalSearchOperator* GetNeighborhoodOperators() const;
  const std::vector<LocalSearchFilter*>& GetOrCreateLocalSearchFilters();
  DecisionBuilder* CreateSolutionFinalizer();
  void CreateFirstSolutionDecisionBuilders();
  DecisionBuilder* GetFirstSolutionDecisionBuilder() const;
  LocalSearchPhaseParameters* CreateLocalSearchParameters();
  DecisionBuilder* CreateLocalSearchDecisionBuilder();
  void SetupDecisionBuilders();
  void SetupMetaheuristics();
  void SetupAssignmentCollector();
  void SetupTrace();
  void SetupSearchMonitors();
  void AddVariableMinimizedByFinalizer(IntVar* var);
  void AddVariableMaximizedByFinalizer(IntVar* var);

  int64 GetArcCost(int64 i, int64 j, int64 cost_class);

  // Model
  scoped_ptr<Solver> solver_;
  int nodes_;
  int vehicles_;
  Constraint* no_cycle_constraint_;
  // Decision variables
  std::vector<IntVar*> nexts_;
  std::vector<IntVar*> vehicle_vars_;
  std::vector<IntVar*> active_;
  // Dimensions
  hash_map<string, DimensionIndex> dimension_name_to_index_;
  ITIVector<DimensionIndex, RoutingDimension*> dimensions_;
  string primary_constrained_dimension_;
  // Costs
  std::vector<NodeEvaluator2*> costs_;
  std::vector<NodeEvaluator2*> vehicle_costs_;
  bool homogeneous_costs_;
  std::vector<CostCacheElement> cost_cache_;
  std::vector<RoutingCache*> routing_caches_;
  std::vector<int64> vehicle_cost_classes_;
  std::vector<int64> fixed_costs_;
  std::vector<const RoutingDimension*> dimensions_with_transit_cost_;
  IntVar* cost_;
  // Disjunctions
  ITIVector<DisjunctionIndex, Disjunction> disjunctions_;
  std::vector<DisjunctionIndex> node_to_disjunction_;
  // Pickup and delivery
  NodePairs pickup_delivery_pairs_;
  // Index management
  std::vector<NodeIndex> index_to_node_;
  ITIVector<NodeIndex, int> node_to_index_;
  std::vector<int> index_to_vehicle_;
  std::vector<int64> starts_;
  std::vector<int64> ends_;
  int start_end_count_;
  // Model status
  bool is_depot_set_;
  bool closed_;
  Status status_;

  // Search data
  std::vector<DecisionBuilder*> first_solution_decision_builders_;
  RoutingStrategy first_solution_strategy_;
  scoped_ptr<Solver::IndexEvaluator2> first_solution_evaluator_;
  std::vector<LocalSearchOperator*> local_search_operators_;
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
  std::vector<IntVar*> variables_maximized_by_finalizer_;
  std::vector<IntVar*> variables_minimized_by_finalizer_;
#ifndef SWIG
  scoped_ptr<SweepArranger> sweep_arranger_;
#endif

  int64 time_limit_ms_;
  int64 lns_time_limit_ms_;
  SearchLimit* limit_;
  SearchLimit* ls_limit_;
  SearchLimit* lns_limit_;

  // Callbacks to be deleted
  hash_set<NodeEvaluator2*> owned_node_callbacks_;
  hash_set<Solver::IndexEvaluator2*> owned_index_callbacks_;

  friend class RoutingDimension;

  DISALLOW_COPY_AND_ASSIGN(RoutingModel);
};

// Dimensions represent quantities accumulated at nodes along the routes. They
// represent quantities such as weights or volumes carried along the route, or
// distance or times.
// Quantities at a node are represented by "cumul" variables and the increase
// or decrease of quantities between nodes are represented by "transit"
// variables. These variables are linked as follows:
// if j == next(i), cumuls(j) = cumuls(i) + transits(i) + slacks(i)
// where slack is a positive slack variable (can represent waiting times for
// a time dimension).
class RoutingDimension {
 public:
#ifndef SWIG
  // Returns all cumul variables for the dimension.
  const std::vector<IntVar*>& cumuls() const { return cumuls_; }
  // Returns all transit variables for the dimension.
  const std::vector<IntVar*>& transits() const { return transits_; }
  // Returns all slack variables for the dimension.
  const std::vector<IntVar*>& slacks() const { return slacks_; }
#endif
  // Returns the cost coefficient corresponding to the transit cost.
  int64 transit_cost_coefficient() const { return transit_cost_coefficient_; }
  // Returns the transition value for a given pair of nodes; this value is the
  // one taken by the corresponding transit variable when the 'next' variable
  // for 'from_index' is bound to 'to_index'.
  int64 GetTransitValue(int64 from_index, int64 to_index) const;
  // Returns the cost coefficient corresponding to the span cost.
  int64 span_cost_coefficient() const { return span_cost_coefficient_; }
#ifndef SWIG
  // Returns the callback evaluating the capacity for vehicle indices.
  RoutingModel::VehicleEvaluator* capacity_evaluator() const {
    return capacity_evaluator_.get();
  }
  // Returns the callback evaluating the transit value between to node indices.
  Solver::IndexEvaluator2* transit_evaluator() const {
    return transit_evaluator_.get();
  }
#endif
  // Returns true if a soft upper bound has been set for a given node.
  bool HasCumulVarSoftUpperBound(RoutingModel::NodeIndex node) const;
  // Returns the soft upper bound of a cumul variable for a given node. The
  // "hard" upper bound of the variable is returned if no soft/ upper bound has
  // been set.
  int64 GetCumulVarSoftUpperBound(RoutingModel::NodeIndex node) const;
  // Returns the cost coefficient of the soft upper bound of a cumul variable
  // for a given node. If no soft upper bound has been set, 0 is returned.
  int64 GetCumulVarSoftUpperBoundCoefficient(
      RoutingModel::NodeIndex node) const;
  // Returns the name of the dimension.
  const string& name() const { return name_; }

 private:
  struct SoftBound {
    SoftBound() : var(nullptr), bound(0), coefficient(0) {}
    IntVar* var;
    int64 bound;
    int64 coefficient;
  };

  RoutingDimension(RoutingModel* model, const string& name);
  void Initialize(RoutingModel::VehicleEvaluator* vehicle_capacity,
                  int64 capacity,
                  RoutingModel::NodeEvaluator2* transit_evaluator,
                  int64 slack_max);
  void InitializeCumuls(RoutingModel::VehicleEvaluator* vehicle_capacity,
                        int64 capacity);
  void InitializeTransits(RoutingModel::NodeEvaluator2* transit_evaluator,
                          int64 slack_max);
  // Sets a cost proportional to the sum of the transit variables.
  void set_transit_cost_coefficient(int64 coefficient) {
    transit_cost_coefficient_ = coefficient;
  }
  // Sets a cost proportional to the span of the dimension. The span is the
  // difference between the largest value of route end cumul variables and
  // the smallest value of route start cumul variables.
  // In other words:
  // span_cost =
  //   coefficient * (Max(dimension end value) - Min(dimension start value)).
  // Only positive coefficients are supported.
  void set_span_cost_coefficient(int64 coefficient) {
    span_cost_coefficient_ = coefficient;
  }
  // Sets a soft upper bound to the cumul variable of a given node. If the
  // value of the cumul variable is greater than the bound, a cost proportional
  // to the difference between this value and the bound is added to the cost
  // function of the model:
  // cumulVar <= upper_bound -> cost = 0
  // cumulVar > upper_bound -> cost = coefficient * (cumulVar - upper_bound).
  // This is also handy to model tardiness costs when the dimension represents
  // time.
  void SetCumulVarSoftUpperBound(RoutingModel::NodeIndex node,
                                 int64 upper_bound, int64 coefficient);
  // Sets up the cost variables related to cumul soft upper bounds.
  void SetupCumulVarSoftUpperBoundCosts(std::vector<IntVar*>* cost_elements) const;
  // Sets up the cost variables related to span costs.
  void SetupSpanCosts(std::vector<IntVar*>* cost_elements) const;
  // Set up the cost variables related to slack costs.
  void SetupSlackCosts(std::vector<IntVar*>* cost_elements) const;

  std::vector<IntVar*> cumuls_;
  scoped_ptr<RoutingModel::VehicleEvaluator> capacity_evaluator_;
  std::vector<IntVar*> transits_;
  scoped_ptr<Solver::IndexEvaluator2> transit_evaluator_;
  std::vector<IntVar*> slacks_;
  int64 transit_cost_coefficient_;
  int64 span_cost_coefficient_;
  std::vector<SoftBound> cumul_var_soft_upper_bound_;
  RoutingModel* const model_;
  const string name_;

  friend class RoutingModel;

  DISALLOW_COPY_AND_ASSIGN(RoutingDimension);
};


#ifndef SWIG
// Class to arrange nodes by by their distance and their angles from the
// depot. Used in the Sweep first solution heuristic.
class SweepArranger {
 public:
  explicit SweepArranger(const ITIVector<RoutingModel::NodeIndex,
                                         std::pair<int64, int64> >& points);
  virtual ~SweepArranger() {}
  void ArrangeNodes(std::vector<RoutingModel::NodeIndex>* nodes);
  void SetSectors(int sectors) { sectors_ = sectors; }

 private:
  ITIVector<RoutingModel::NodeIndex, int> coordinates_;
  int sectors_;

  DISALLOW_COPY_AND_ASSIGN(SweepArranger);
};
#endif

// Routing filters, exposed for testing.
LocalSearchFilter* MakeNodeDisjunctionFilter(const RoutingModel& routing_model);
LocalSearchFilter* MakePathCumulFilter(const RoutingModel& routing_model,
                                       const string& dimension_name,
                                       Callback1<int64>* objective_callback);

}  // namespace operations_research

#endif  // OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_H_
