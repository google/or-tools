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
//    routing.SetArcCostEvaluatorOfAllVehicles(
//        NewPermanentCallback(MyDistance));
//
// - Find a solution using Solve(), returns a solution if any (owned by
//   routing):
//
//    const Assignment* solution = routing.Solve();
//    CHECK(solution != nullptr);
//
// - Inspect the solution cost and route (only one route here:
//
//    LOG(INFO) << "Cost " << solution->ObjectiveValue();
//    const int route_number = 0;
//    for (int64 node = routing.Start(route_number);
//         !routing.IsEnd(node);
//         node = solution->Value(routing.NextVar(node))) {
//      LOG(INFO) << routing.IndexToNode(node);
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
#include "base/unique_ptr.h"
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/int_type_indexed_vector.h"
#include "base/int_type.h"
#include "base/hash.h"
#include "constraint_solver/constraint_solver.h"
#include "constraint_solver/constraint_solveri.h"


namespace operations_research {

class LocalSearchOperator;
class RoutingCache;
class RoutingDimension;
#ifndef SWIG
class SweepArranger;
#endif
struct SweepNode;

// The type must be defined outside the class RoutingModel, SWIG does not parse
// it correctly if it's inside.
DEFINE_INT_TYPE(_RoutingModel_NodeIndex, int);
DEFINE_INT_TYPE(_RoutingModel_CostClassIndex, int);
DEFINE_INT_TYPE(_RoutingModel_DimensionIndex, int);
DEFINE_INT_TYPE(_RoutingModel_DisjunctionIndex, int);

// This class stores solver parameters.
struct RoutingParameters {
  RoutingParameters() {
    use_light_propagation = false;
    cache_callbacks = false;
    max_cache_size = 1000;
  }

  // Use constraints with light propagation in routing model.
  bool use_light_propagation;
  // Cache callback calls.
  bool cache_callbacks;
  // Maximum cache size when callback caching is on.
  int64 max_cache_size;
};

// This class stores search parameters.
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
  std::string first_solution;
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
    // Iteratively build a solution by inserting the cheapest node at its
    // cheapest position; the cost of insertion is based on the global cost
    // function of the routing model. As of 2/2012, only works on models with
    // optional nodes (with finite penalty costs).
    ROUTING_BEST_INSERTION,
    // Iteratively build a solution by inserting the cheapest node at its
    // cheapest position; the cost of insertion is based on the the arc cost
    // function. Is faster than BEST_INSERTION.
    ROUTING_GLOBAL_CHEAPEST_INSERTION,
    // Iteratively build a solution by inserting each node at its cheapest
    // position; the cost of insertion is based on the the arc cost function.
    // Differs from ROUTING_GLOBAL_CHEAPEST_INSERTION by the node selected for
    // insertion; here nodes are considered in their order of creation. Is
    // faster than ROUTING_GLOBAL_CHEAPEST_INSERTION.
    ROUTING_LOCAL_CHEAPEST_INSERTION,
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
  typedef _RoutingModel_CostClassIndex CostClassIndex;
  typedef _RoutingModel_DimensionIndex DimensionIndex;
  typedef _RoutingModel_DisjunctionIndex DisjunctionIndex;
  typedef ResultCallback1<int64, int64> VehicleEvaluator;
  typedef ResultCallback2<int64, NodeIndex, NodeIndex> NodeEvaluator2;
  typedef std::pair<int, int> NodePair;
  typedef std::vector<NodePair> NodePairs;

  struct CostClass {
    // arc_cost_evaluator->Run(from, to) is the transit cost of arc
    // from->to. This may never be nullptr.
    NodeEvaluator2* arc_cost_evaluator;

    // SUBTLE:
    // The vehicle's fixed cost is skipped on purpose here, because we
    // can afford to do so:
    // - We don't really care about creating "strict" equivalence classes;
    //   all we care about is to:
    //   1) compress the space of cost callbacks so that
    //      we can cache them more efficiently.
    //   2) have a smaller IntVar domain thanks to using a "cost class var"
    //      instead of the vehicle var, so that we reduce the search space.
    //   Both of these are an incentive for *fewer* cost classes. Ignoring
    //   the fixed costs can only be good in that regard.
    // - The fixed costs are only needed when evaluating the cost of the
    //   first arc of the route, in which case we know the vehicle, since we
    //   have the route's start node.

    // Only dimensions that have non-zero cost evaluator and a non-zero cost
    // coefficient (in this cost class) are listed here. Since we only need
    // their transit evaluator (the raw version that takes var index, not Node
    // Index) and their span cost coefficient, we just store those.
    // This is sorted by the natural operator < (and *not* by DimensionIndex).
    std::vector<std::pair<Solver::IndexEvaluator2*, int64> >
        dimension_transit_evaluator_and_cost_coefficient;

    explicit CostClass(NodeEvaluator2* arc_cost_evaluator)
        : arc_cost_evaluator(arc_cost_evaluator) {
      CHECK(arc_cost_evaluator != nullptr);
    }

    // Comparator for STL containers and algorithms.
    static bool LessThan(const CostClass& a, const CostClass b) {
      if (a.arc_cost_evaluator != b.arc_cost_evaluator) {
        return a.arc_cost_evaluator < b.arc_cost_evaluator;
      }
      return a.dimension_transit_evaluator_and_cost_coefficient <
             b.dimension_transit_evaluator_and_cost_coefficient;
    }
  };

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

  // global parameters.
  static void SetGlobalParameters(const RoutingParameters& parameters);

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
                    bool fix_start_cumul_to_zero, const std::string& name);
  bool AddDimensionWithVehicleTransits(
      const std::vector<NodeEvaluator2*>& evaluators, int64 slack_max,
      int64 capacity, bool fix_start_cumul_to_zero, const std::string& name);
  // Takes ownership of both 'evaluator' and 'vehicle_capacity' callbacks.
  bool AddDimensionWithVehicleCapacity(NodeEvaluator2* evaluator,
                                       int64 slack_max,
                                       VehicleEvaluator* vehicle_capacity,
                                       bool fix_start_cumul_to_zero,
                                       const std::string& name);
  bool AddDimensionWithVehicleTransitAndCapacity(
      const std::vector<NodeEvaluator2*>& evaluators, int64 slack_max,
      VehicleEvaluator* vehicle_capacity, bool fix_start_cumul_to_zero,
      const std::string& name);
  // Creates a dimension where the transit variable is constrained to be
  // equal to 'value'; 'capacity' is the upper bound of the cumul variables.
  // 'name' is the name used to reference the dimension; this name is used to
  // get cumul and transit variables from the routing model.
  // Returns false if a dimension with the same name has already been created
  // (and doesn't create the new dimension).
  bool AddConstantDimension(int64 value, int64 capacity,
                            bool fix_start_cumul_to_zero, const std::string& name);
  // Creates a dimension where the transit variable is constrained to be
  // equal to 'values[i]' for node i; 'capacity' is the upper bound of
  // the cumul variables. 'name' is the name used to reference the dimension;
  // this name is used to get cumul and transit variables from the routing
  // model.
  // Returns false if a dimension with the same name has already been created
  // (and doesn't create the new dimension).
  bool AddVectorDimension(const int64* values, int64 capacity,
                          bool fix_start_cumul_to_zero, const std::string& name);
  // Creates a dimension where the transit variable is constrained to be
  // equal to 'values[i][next(i)]' for node i; 'capacity' is the upper bound of
  // the cumul variables. 'name' is the name used to reference the dimension;
  // this name is used to get cumul and transit variables from the routing
  // model.
  // Returns false if a dimension with the same name has already been created
  // (and doesn't create the new dimension).
  bool AddMatrixDimension(const int64* const* values, int64 capacity,
                          bool fix_start_cumul_to_zero, const std::string& name);
  // Outputs the names of all dimensions added to the routing engine.
  // TODO(user): rename.
  void GetAllDimensions(std::vector<std::string>* dimension_names) const;
  // Returns true if a dimension exists for a given dimension name.
  bool HasDimension(const std::string& dimension_name) const;
  // Returns a dimension from its name. Dies if the dimension does not exist.
  const RoutingDimension& GetDimensionOrDie(const std::string& dimension_name) const;
  // Returns a dimension from its name. Returns nullptr if the dimension does
  // not exist.
  RoutingDimension* GetMutableDimension(const std::string& dimension_name) const;
  // Set the given dimension as "primary constrained". As of August 2013, this
  // is only used by ArcIsMoreConstrainedThanArc().
  // "dimension" must be the name of an existing dimension, or be empty, in
  // which case there will not be a primary dimension after this call.
  void SetPrimaryConstrainedDimension(const std::string& dimension_name) {
    DCHECK(dimension_name.empty() || HasDimension(dimension_name));
    primary_constrained_dimension_ = dimension_name;
  }
  // Get the primary constrained dimension, or an empty std::string if it is unset.
  const std::string& GetPrimaryConstrainedDimension() const {
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
#ifndef SWIG
  // Returns the variable indices of the nodes in the disjunction of index
  // 'index'.
  const std::vector<int>& GetDisjunctionIndices(DisjunctionIndex index) const {
    return disjunctions_[index].nodes;
  }
#endif
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
#ifndef SWIG
  // Returns pickup and delivery pairs currently in the model.
  const NodePairs& GetPickupAndDeliveryPairs() const {
    return pickup_delivery_pairs_;
  }
#endif
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
  void SetArcCostEvaluatorOfAllVehicles(NodeEvaluator2* evaluator);
  // Sets the cost function for a given vehicle route.
  void SetArcCostEvaluatorOfVehicle(NodeEvaluator2* evaluator, int vehicle);
  // Sets the fixed cost of all vehicle routes. It is equivalent to calling
  // SetFixedCostOfVehicle on all vehicle routes.
  void SetFixedCostOfAllVehicles(int64 cost);
  // Sets the fixed cost of one vehicle route.
  void SetFixedCostOfVehicle(int64 cost, int vehicle);
  // Returns the route fixed cost taken into account if the route of the
  // vehicle is not empty, aka there's at least one node on the route other than
  // the first and last nodes.
  int64 GetFixedCostOfVehicle(int vehicle) const;

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
  // Adds a variable to minimize in the solution finalizer. The solution
  // finalizer is called each time a solution is found during the search and
  // allows to instantiate secondary variables (such as dimension cumul
  // variables).
  void AddVariableMinimizedByFinalizer(IntVar* var);
  // Adds a variable to maximize in the solution finalizer (see above for
  // information on the solution finalizer).
  void AddVariableMaximizedByFinalizer(IntVar* var);
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
  bool WriteAssignment(const std::string& file_name) const;
  // Reads an assignment from a file and returns the current solution.
  // Returns nullptr if the file cannot be opened or if the assignment is not
  // valid.
  Assignment* ReadAssignment(const std::string& file_name);
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
  // Returns the global cost variable which is being minimized.
  IntVar* CostVar() const { return cost_; }
  // Returns the cost of the transit arc between two nodes for a given vehicle.
  // Input are variable indices of node. This returns 0 if vehicle < 0.
  int64 GetArcCostForVehicle(int64 from_index, int64 to_index, int64 vehicle);
  // Whether costs are homogeneous across all vehicles.
  bool CostsAreHomogeneousAcrossVehicles() const {
    return costs_are_homogeneous_across_vehicles_;
  }
  // Returns the cost of the segment between two nodes supposing all vehicle
  // costs are the same (returns the cost for the first vehicle otherwise).
  int64 GetHomogeneousCost(int64 from_index, int64 to_index) {
    return GetArcCostForVehicle(from_index, to_index, /*vehicle=*/0);
  }
  // Returns the cost of the arc in the context of the first solution strategy.
  // This is typically a simplification of the actual cost; see the .cc.
  int64 GetArcCostForFirstSolution(int64 from_index, int64 to_index);
  // Returns the cost of the segment between two nodes for a given cost
  // class. Input are variable indices of nodes and the cost class.
  // Unlike GetArcCostForVehicle(), if cost_class is kNoCost, then the
  // returned cost won't necessarily be zero: only some of the components
  // of the cost that depend on the cost class will be omited. See the code
  // for details.
  int64 GetArcCostForClass(int64 from_index, int64 to_index,
                           int64 /*CostClassIndex*/ cost_class_index);
  // Get the cost class index of the given vehicle.
  CostClassIndex GetCostClassIndexOfVehicle(int64 vehicle) const {
    DCHECK(closed_);
    return cost_class_index_of_vehicle_[vehicle];
  }
  // Returns the number of different cost classes in the model.
  int GetCostClassesCount() const { return cost_classes_.size(); }
  // Ditto, minus the 'always zero', built-in cost class.
  int GetNonZeroCostClassesCount() const {
    return std::max(0, GetCostClassesCount() - 1);
  }
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
  std::string DebugOutputAssignment(const Assignment& solution_assignment,
                               const std::string& dimension_to_print) const;

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
  static bool ParseRoutingStrategy(const std::string& strategy_str,
                                   RoutingStrategy* strategy);
  static const char* RoutingMetaheuristicName(
      RoutingMetaheuristic metaheuristic);
  static bool ParseRoutingMetaheuristic(const std::string& metaheuristic_str,
                                        RoutingMetaheuristic* metaheuristic);

  // DEPRECATED METHODS.
  // Don't use these methods; instead use their proposed replacement (in
  // comments).
  // TODO(user): only keep those for the open-source export.
  void SetCost(NodeEvaluator2* e);  // SetArcCostEvaluatorOfAllVehicles()
  // SetArcCostEvaluatorOfVehicle()
  void SetVehicleCost(int v, NodeEvaluator2* e);
  int64 GetRouteFixedCost() const;           // GetFixedCostOfVehicle()
  void SetRouteFixedCost(int64 c);           // SetFixedCostOfAllVehicles()
  int64 GetVehicleFixedCost(int v) const;    // GetFixedCostOfVehicle()
  void SetVehicleFixedCost(int v, int64 c);  // SetFixedCostOfVehicle()
  bool homogeneous_costs() const;   // CostsAreHomogeneousAcrossVehicles()
  int GetVehicleCostCount() const;  // GetNonZeroCostClassesCount()
  int64 GetCost(int64 i, int64 j, int64 v);  // GetArcCostForVehicle()
  int64 GetVehicleClassCost(int64 i, int64 j, int64 c);  // GetArcCostForClass()
  // All the methods below are replaced by public methods of the
  // RoutingDimension class. See that class.
  void SetDimensionTransitCost(const std::string& d, int64 c);
  int64 GetDimensionTransitCost(const std::string& d) const;
  void SetDimensionSpanCost(const std::string& d, int64 c);
  int64 GetDimensionSpanCost(const std::string& d) const;
  int64 GetTransitValue(const std::string& d, int64 from, int64 to,
                        int64 vehicle) const;
#ifndef SWIG
  const std::vector<IntVar*>& CumulVars(const std::string& dimension_name) const;
#endif
  // All the methods below are replaced by public methods with the same name
  // on the RoutingDimension class. See those.
  IntVar* CumulVar(int64 index, const std::string& dimension_name) const;
  IntVar* TransitVar(int64 index, const std::string& dimension_name) const;
  IntVar* SlackVar(int64 index, const std::string& dimension_name) const;
  void SetCumulVarSoftUpperBound(NodeIndex, const std::string&, int64, int64);
  bool HasCumulVarSoftUpperBound(NodeIndex, const std::string&) const;
  int64 GetCumulVarSoftUpperBound(NodeIndex, const std::string&) const;
  int64 GetCumulVarSoftUpperBoundCoefficient(NodeIndex, const std::string&) const;
  void SetStartCumulVarSoftUpperBound(int, const std::string&, int64, int64);
  bool HasStartCumulVarSoftUpperBound(int, const std::string&) const;
  int64 GetStartCumulVarSoftUpperBound(int, const std::string&) const;
  int64 GetStartCumulVarSoftUpperBoundCoefficient(int, const std::string&) const;
  void SetEndCumulVarSoftUpperBound(int, const std::string&, int64, int64);
  bool HasEndCumulVarSoftUpperBound(int, const std::string&) const;
  int64 GetEndCumulVarSoftUpperBound(int, const std::string&) const;
  int64 GetEndCumulVarSoftUpperBoundCoefficient(int, const std::string&) const;

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
  // node 'index' and on the cost class 'cost_class'.
  struct CostCacheElement {
    // This is usually an int64, but using an int here decreases the RAM usage,
    // and should be fine since in practice we never have more than 1<<31 vars.
    // Note(user): on 2013-11, microbenchmarks on the arc costs callbacks
    // also showed a 2% speed-up thanks to using int rather than int64.
    int index;
    CostClassIndex cost_class_index;
    int64 cost;
  };

  // Internal methods.
  void Initialize();
  void SetStartEnd(const std::vector<std::pair<NodeIndex, NodeIndex> >& start_end);
  void AddDisjunctionInternal(const std::vector<NodeIndex>& nodes, int64 penalty);
  void AddNoCycleConstraintInternal();
  bool AddDimensionWithCapacityInternal(
      const std::vector<NodeEvaluator2*>& evaluators, int64 slack_max,
      int64 capacity, VehicleEvaluator* vehicle_capacity,
      bool fix_start_cumul_to_zero, const std::string& dimension_name);
  DimensionIndex GetDimensionIndex(const std::string& dimension_name) const;
  void ComputeCostClasses();
  int64 GetArcCostForClassInternal(int64 from_index, int64 to_index,
                                   CostClassIndex cost_class_index);
  void AppendHomogeneousArcCosts(int node_index,
                                 std::vector<IntVar*>* cost_elements);
  void AppendArcCosts(int node_index, std::vector<IntVar*>* cost_elements);
  Assignment* DoRestoreAssignment();
  static const CostClassIndex kCostClassIndexOfZeroCost;
  int64 SafeGetCostClassInt64OfVehicle(int64 vehicle) const {
    DCHECK_LT(0, vehicles_);
    return (vehicle >= 0 ? GetCostClassIndexOfVehicle(vehicle)
                         : kCostClassIndexOfZeroCost).value();
  }
  int64 GetDimensionTransitCostSum(int64 i, int64 j,
                                   const CostClass& cost_class) const;
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
  const std::vector<LocalSearchFilter*>& GetOrCreateFeasibilityFilters();
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

  int64 GetArcCostForCostClassInternal(int64 i, int64 j, int64 cost_class);

  // Model
  std::unique_ptr<Solver> solver_;
  int nodes_;
  int vehicles_;
  Constraint* no_cycle_constraint_;
  // Decision variables: indexed by int64 var index.
  std::vector<IntVar*> nexts_;
  std::vector<IntVar*> vehicle_vars_;
  std::vector<IntVar*> active_;
  // is_bound_to_end_[i] will be true iff the path starting at var #i is fully
  // bound and reaches the end of a route, i.e. either:
  // - IsEnd(i) is true
  // - or nexts_[i] is bound and is_bound_to_end_[nexts_[i].Value()] is true.
  std::vector<IntVar*> is_bound_to_end_;
  // Dimensions
  hash_map<std::string, DimensionIndex> dimension_name_to_index_;
  ITIVector<DimensionIndex, RoutingDimension*> dimensions_;
  std::string primary_constrained_dimension_;
  // Costs
  IntVar* cost_;
  std::vector<NodeEvaluator2*> transit_cost_of_vehicle_;
  std::vector<int64> fixed_cost_of_vehicle_;
  std::vector<CostClassIndex> cost_class_index_of_vehicle_;
#ifndef SWIG
  ITIVector<CostClassIndex, CostClass> cost_classes_;
#endif  // SWIG
  bool costs_are_homogeneous_across_vehicles_;
  std::vector<CostCacheElement> cost_cache_;  // Index by source index.
  std::vector<RoutingCache*> routing_caches_;
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
  std::unique_ptr<Solver::IndexEvaluator2> first_solution_evaluator_;
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
  std::vector<LocalSearchFilter*> feasibility_filters_;
  std::vector<IntVar*> variables_maximized_by_finalizer_;
  std::vector<IntVar*> variables_minimized_by_finalizer_;
#ifndef SWIG
  std::unique_ptr<SweepArranger> sweep_arranger_;
#endif

  int64 time_limit_ms_;
  int64 lns_time_limit_ms_;
  SearchLimit* limit_;
  SearchLimit* ls_limit_;
  SearchLimit* lns_limit_;

  // Callbacks to be deleted
  hash_set<const NodeEvaluator2*> owned_node_callbacks_;
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
  // Returns the transition value for a given pair of nodes (as var index);
  // this value is the one taken by the corresponding transit variable when
  // the 'next' variable for 'from_index' is bound to 'to_index'.
  int64 GetTransitValue(int64 from_index, int64 to_index, int64 vehicle) const;
  // Get the cumul, transit and slack variables for the given node (given as
  // int64 var index).
  IntVar* CumulVar(int64 index) const { return cumuls_[index]; }
  IntVar* TransitVar(int64 index) const { return transits_[index]; }
  IntVar* SlackVar(int64 index) const { return slacks_[index]; }
#ifndef SWIG
  // Like CumulVar(), TransitVar(), SlackVar() but return the whole variable
  // vectors instead (indexed by int64 var index).
  const std::vector<IntVar*>& cumuls() const { return cumuls_; }
  const std::vector<IntVar*>& transits() const { return transits_; }
  const std::vector<IntVar*>& slacks() const { return slacks_; }
  // Returns the callback evaluating the capacity for vehicle indices.
  RoutingModel::VehicleEvaluator* capacity_evaluator() const {
    return capacity_evaluator_.get();
  }
  // Returns the callback evaluating the transit value between two node indices
  // for a given vehicle.
  Solver::IndexEvaluator2* transit_evaluator(int vehicle) const {
    return transit_evaluators_[vehicle];
  }
#endif
  // Sets a cost proportional to the dimension span on a given vehicle,
  // or on all vehicles at once. "coefficient" must be nonnegative.
  // This is handy to model costs proportional to idle time when the dimension
  // represents time.
  // The cost for a vehicle is
  //   span_cost = coefficient * (dimension end value - dimension start value).
  void SetSpanCostCoefficientForVehicle(int64 coefficient, int vehicle);
  void SetSpanCostCoefficientForAllVehicles(int64 coefficient);
  // Sets a cost proportional to the *global* dimension span, that is the
  // difference between the largest value of route end cumul variables and
  // the smallest value of route start cumul variables.
  // In other words:
  // global_span_cost =
  //   coefficient * (Max(dimension end value) - Min(dimension start value)).
  void SetGlobalSpanCostCoefficient(int64 coefficient);

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
  // Same as SetCumulVarSoftUpperBound applied to vehicle start node,
  void SetStartCumulVarSoftUpperBound(int vehicle, int64 upper_bound,
                                      int64 coefficient);
  // Same as SetCumulVarSoftUpperBound applied to vehicle end node,
  void SetEndCumulVarSoftUpperBound(int vehicle, int64 upper_bound,
                                    int64 coefficient);
  // Same as SetCumulVarSoftUpperBound but using a variable index.
  void SetCumulVarSoftUpperBoundFromIndex(int64 index, int64 upper_bound,
                                          int64 coefficient);
  // Returns true if a soft upper bound has been set for a given node.
  bool HasCumulVarSoftUpperBound(RoutingModel::NodeIndex node) const;
  // Returns true if a soft upper bound has been set for a given vehicle start
  // node.
  bool HasStartCumulVarSoftUpperBound(int vehicle) const;
  // Returns true if a soft upper bound has been set for a given vehicle end
  // node.
  bool HasEndCumulVarSoftUpperBound(int vehicle) const;
  // Returns true if a soft upper bound has been set for a given variable index.
  bool HasCumulVarSoftUpperBoundFromIndex(int64 index) const;
  // Returns the soft upper bound of a cumul variable for a given node. The
  // "hard" upper bound of the variable is returned if no soft upper bound has
  // been set.
  int64 GetCumulVarSoftUpperBound(RoutingModel::NodeIndex node) const;
  // Returns the soft upper bound of a cumul variable for a given vehicle start
  // node. The "hard" upper bound of the variable is returned if no soft upper
  // bound has been set.
  int64 GetStartCumulVarSoftUpperBound(int vehicle) const;
  // Returns the soft upper bound of a cumul variable for a given vehicle end
  // node. The "hard" upper bound of the variable is returned if no soft upper
  // bound has been set.
  int64 GetEndCumulVarSoftUpperBound(int vehicle) const;
  // Returns the soft upper bound of a cumul variable for a given variable
  // index. The "hard" upper bound of the variable is returned if no soft upper
  // bound has been set.
  int64 GetCumulVarSoftUpperBoundFromIndex(int64 index) const;
  // Returns the cost coefficient of the soft upper bound of a cumul variable
  // for a given node. If no soft upper bound has been set, 0 is returned.
  int64 GetCumulVarSoftUpperBoundCoefficient(RoutingModel::NodeIndex node)
      const;
  // Returns the cost coefficient of the soft upper bound of a cumul variable
  // for a given vehicle start node. If no soft upper bound has been set, 0 is
  // returned.
  int64 GetStartCumulVarSoftUpperBoundCoefficient(int vehicle) const;
  // Returns the cost coefficient of the soft upper bound of a cumul variable
  // for a given vehicle end node. If no soft upper bound has been set, 0 is
  // returned.
  int64 GetEndCumulVarSoftUpperBoundCoefficient(int vehicle) const;
  // Returns the cost coefficient of the soft upper bound of a cumul variable
  // for a variable index. If no soft upper bound has been set, 0 is returned.
  int64 GetCumulVarSoftUpperBoundCoefficientFromIndex(int64 index) const;
  // Returns the name of the dimension.
  const std::string& name() const { return name_; }

  // Accessors.
  int64 GetSpanCostCoefficientForVehicle(int vehicle) const {
    return vehicle_span_cost_coefficients_[vehicle];
  }
#ifndef SWIG
  const std::vector<int64>& vehicle_span_cost_coefficients() const {
    return vehicle_span_cost_coefficients_;
  }
#endif  // SWIG
  int64 global_span_cost_coefficient() const {
    return global_span_cost_coefficient_;
  }

 private:
  struct SoftBound {
    SoftBound() : var(nullptr), bound(0), coefficient(0) {}
    IntVar* var;
    int64 bound;
    int64 coefficient;
  };

  RoutingDimension(RoutingModel* model, const std::string& name);
  void Initialize(
      RoutingModel::VehicleEvaluator* vehicle_capacity, int64 capacity,
      const std::vector<RoutingModel::NodeEvaluator2*>& transit_evaluators,
      int64 slack_max);
  void InitializeCumuls(RoutingModel::VehicleEvaluator* vehicle_capacity,
                        int64 capacity);
  void InitializeTransits(
      const std::vector<RoutingModel::NodeEvaluator2*>& transit_evaluators,
      int64 slack_max);
  // Sets up the cost variables related to cumul soft upper bounds.
  void SetupCumulVarSoftUpperBoundCosts(std::vector<IntVar*>* cost_elements) const;
  // Sets up the cost variables related to the global span and per-vehicle span
  // costs (only for the "slack" part of the latter).
  void SetupGlobalSpanCost(std::vector<IntVar*>* cost_elements) const;
  void SetupSlackCosts(std::vector<IntVar*>* cost_elements) const;

  std::vector<IntVar*> cumuls_;
  std::unique_ptr<RoutingModel::VehicleEvaluator> capacity_evaluator_;
  std::vector<IntVar*> transits_;
  // "transit_evaluators_" does the indexing by vehicle, while
  // "class_evaluators_" does the de-duplicated ownership.
  std::vector<Solver::IndexEvaluator2*> transit_evaluators_;
  std::vector<std::unique_ptr<Solver::IndexEvaluator2> > class_evaluators_;
  std::vector<IntVar*> slacks_;
  int64 global_span_cost_coefficient_;
  std::vector<int64> vehicle_span_cost_coefficients_;
  std::vector<SoftBound> cumul_var_soft_upper_bound_;
  RoutingModel* const model_;
  const std::string name_;

  friend class RoutingModel;

  DISALLOW_COPY_AND_ASSIGN(RoutingDimension);
};


#ifndef SWIG
// Class to arrange nodes by by their distance and their angles from the
// depot. Used in the Sweep first solution heuristic.
class SweepArranger {
 public:
  explicit SweepArranger(
      const ITIVector<RoutingModel::NodeIndex, std::pair<int64, int64> >& points);
  virtual ~SweepArranger() {}
  void ArrangeNodes(std::vector<RoutingModel::NodeIndex>* nodes);
  void SetSectors(int sectors) { sectors_ = sectors; }

 private:
  ITIVector<RoutingModel::NodeIndex, int> coordinates_;
  int sectors_;

  DISALLOW_COPY_AND_ASSIGN(SweepArranger);
};
#endif

// Routing Search

// Decision builders building a solution using local search filters to evaluate
// its feasibility. This is very fast but can eventually fail when the solution
// is restored if filters did not detect all infeasiblities.
// More details:
// Using local search filters to build a solution. The approach is pretty
// straight-forward: have a general assignment storing the current solution,
// build delta assigment representing possible extensions to the current
// solution and validate them with filters.
// The tricky bit comes from using the assignment and filter APIs in a way
// which avoids the lazy creation of internal hash_maps between variables
// and indices.

// Generic filter-based decision builder applied to IntVars.
// TODO(user): Eventually move this to the core CP solver library
// when the code is mature enough.
class IntVarFilteredDecisionBuilder : public DecisionBuilder {
 public:
  IntVarFilteredDecisionBuilder(Solver* solver, const std::vector<IntVar*>& vars,
                                const std::vector<LocalSearchFilter*>& filters);
  virtual ~IntVarFilteredDecisionBuilder() {}
  virtual Decision* Next(Solver* solver);
  // Virtual method to redefine to build a solution.
  virtual bool BuildSolution() = 0;

 protected:
  // Commits the modifications to the current solution if these modifications
  // are "filter-feasible", returns false otherwise; in any case discards
  // all modifications.
  bool Commit();
  // Modifies the current solution by setting the variable of index 'index' to
  // value 'value'.
  void SetValue(int64 index, int64 value) {
    if (!is_in_delta_[index]) {
      delta_->FastAdd(vars_[index])->SetValue(value);
      delta_indices_.push_back(index);
      is_in_delta_[index] = true;
    } else {
      delta_->SetValue(vars_[index], value);
    }
  }
  // Returns the value of the variable of index 'index' in the last committed
  // solution.
  int64 Value(int64 index) const {
    return assignment_->IntVarContainer().Element(index).Value();
  }
  // Returns true if the variable of index 'index' is in the current solution.
  bool Contains(int64 index) const {
    return assignment_->IntVarContainer().Element(index).Var() != nullptr;
  }
  // Returns the number of variables the decision builder is trying to
  // instantiate.
  int Size() const { return vars_.size(); }
  // Returns the variable of index 'index'.
  IntVar* Var(int64 index) const { return vars_[index]; }

 private:
  // Sets all variables currently bound to their value in the current solution.
  void SetValuesFromDomains();
  // Synchronizes filters with an assignment (the current solution).
  void SynchronizeFilters();
  // Checks if filters accept a given modification to the current solution
  // (represented by delta).
  bool FilterAccept();

  const std::vector<IntVar*> vars_;
  Assignment* const assignment_;
  Assignment* const delta_;
  std::vector<int> delta_indices_;
  std::vector<bool> is_in_delta_;
  const Assignment* const empty_;
  std::vector<LocalSearchFilter*> filters_;
};

// Filter-based decision builder dedicated to routing.
class RoutingFilteredDecisionBuilder : public IntVarFilteredDecisionBuilder {
 public:
  RoutingFilteredDecisionBuilder(RoutingModel* model,
                                 const std::vector<LocalSearchFilter*>& filters);
  virtual ~RoutingFilteredDecisionBuilder() {}
  RoutingModel* model() const { return model_; }
  // Initializes the current solution with empty or partial vehicle routes.
  bool InitializeRoutes();
  // Returns the end of the start chain of vehicle,
  int GetStartChainEnd(int vehicle) const { return start_chain_ends_[vehicle]; }
  // Make nodes in the same disjunction as 'node' unperformed. 'node' is a
  // variable index corresponding to a node.
  void MakeDisjunctionNodesUnperformed(int64 node);
  // Make all unassigned nodes unperformed.
  void MakeUnassignedNodesUnperformed();

 private:
  RoutingModel* const model_;
  std::vector<int64> start_chain_ends_;
};

class CheapestInsertionFilteredDecisionBuilder
    : public RoutingFilteredDecisionBuilder {
 public:
  // Takes ownership of evaluator.
  CheapestInsertionFilteredDecisionBuilder(
      RoutingModel* model, ResultCallback2<int64, int64, int64>* evaluator,
      const std::vector<LocalSearchFilter*>& filters);
  virtual ~CheapestInsertionFilteredDecisionBuilder() {}

 protected:
  typedef std::pair<int64, int64> ValuedPosition;
  // Inserts 'node' just after 'predecessor', and just before 'successor',
  // resulting in the following subsequence: predecessor -> node -> successor.
  // If 'node' is part of a disjunction, other nodes of the disjunction are made
  // unperformed.
  void InsertBetween(int64 node, int64 predecessor, int64 successor);
  // Helper method to the ComputeEvaluatorSortedPositions* methods. Finds all
  // possible insertion positions of node 'node_to_insert' in the partial route
  // starting at node 'start' and adds them to 'valued_position', a list of
  // unsorted pairs of (cost, position to insert the node).
  void AppendEvaluatedPositionsAfter(int64 node_to_insert, int64 start,
                                     int64 next_after_start,
                                     std::vector<ValuedPosition>* valued_positions);

  std::unique_ptr<ResultCallback2<int64, int64, int64> > evaluator_;
};

// Filtered-base decision builder which builds a solution by inserting
// nodes at their cheapest position. The cost of a position is computed
// an arc-based cost callback. The node selected for insertion is the one
// which minimizes insertion cost.
class GlobalCheapestInsertionFilteredDecisionBuilder
    : public CheapestInsertionFilteredDecisionBuilder {
 public:
  // Takes ownership of evaluator.
  GlobalCheapestInsertionFilteredDecisionBuilder(
      RoutingModel* model, ResultCallback2<int64, int64, int64>* evaluator,
      const std::vector<LocalSearchFilter*>& filters);
  virtual ~GlobalCheapestInsertionFilteredDecisionBuilder() {}
  virtual bool BuildSolution();

 private:
  typedef std::pair<int64, int64> InsertionPosition;
  // Computes the possible insertion positions for all non-inserted nodes and
  // sorts them according to the current cost evaluator.
  // Each InsertionPosition of the output represents an already performed node,
  // followed by a non-inserted node that should be set as the "Next" of the
  // former.
  void ComputeEvaluatorSortedPositions(
      std::vector<InsertionPosition>* sorted_positions);
  // Same as above but limited to pickup and delivery pairs. Each pair of
  // InsertionPosition applies respectively to a pickup and its delivery.
  void ComputeEvaluatorSortedPositionPairs(
      std::vector<std::pair<InsertionPosition, InsertionPosition> >* sorted_positions);
};

// Filtered-base decision builder which builds a solution by inserting
// nodes at their cheapest position. The cost of a position is computed
// an arc-based cost callback. Node selected for insertion are considered in
// the order they were created in the model.
class LocalCheapestInsertionFilteredDecisionBuilder
    : public CheapestInsertionFilteredDecisionBuilder {
 public:
  // Takes ownership of evaluator.
  LocalCheapestInsertionFilteredDecisionBuilder(
      RoutingModel* model, ResultCallback2<int64, int64, int64>* evaluator,
      const std::vector<LocalSearchFilter*>& filters);
  virtual ~LocalCheapestInsertionFilteredDecisionBuilder() {}
  virtual bool BuildSolution();

 private:
  // Computes the possible insertion positions of 'node' and sorts them
  // according to the current cost evaluator.
  // 'node' is a variable index corresponding to a node, 'sorted_positions' is a
  // vector of variable indices corresponding to nodes after which 'node' can be
  // inserted.
  void ComputeEvaluatorSortedPositions(int64 node,
                                       std::vector<int64>* sorted_positions);
  // Like ComputeEvaluatorSortedPositions, subject to the additional
  // restrictions that the node may only be inserted after node 'start' on the
  // route. For convenience, this method also needs the node that is right after
  // 'start' on the route.
  void ComputeEvaluatorSortedPositionsOnRouteAfter(
      int64 node, int64 start, int64 next_after_start,
      std::vector<int64>* sorted_positions);
};

// Filtered-base decision builder based on the addition heuristic, extending
// a path from its start node with the cheapest arc.
class CheapestAdditionFilteredDecisionBuilder
    : public RoutingFilteredDecisionBuilder {
 public:
  CheapestAdditionFilteredDecisionBuilder(
      RoutingModel* model, const std::vector<LocalSearchFilter*>& filters);
  virtual ~CheapestAdditionFilteredDecisionBuilder() {}
  virtual bool BuildSolution();

 private:
  class PartialRoutesAndLargeVehicleIndicesFirst {
   public:
    PartialRoutesAndLargeVehicleIndicesFirst(
        const CheapestAdditionFilteredDecisionBuilder& builder)
        : builder_(builder) {}
    bool operator()(int vehicle1, int vehicle2);

   private:
    const CheapestAdditionFilteredDecisionBuilder& builder_;
  };
  // Returns a sorted vector of nodes which can come next in the route after
  // node 'from'.
  // 'from' is a variable index corresponding to a node, 'sorted_nexts' is a
  // vector of variable indices corresponding to nodes which can follow 'from'.
  virtual void SortPossibleNexts(int64 from, std::vector<int64>* sorted_nexts) = 0;
};

// A CheapestAdditionFilteredDecisionBuilder where the notion of 'cheapest arc'
// comes from an arc evaluator.
class EvaluatorCheapestAdditionFilteredDecisionBuilder
    : public CheapestAdditionFilteredDecisionBuilder {
 public:
  // Takes ownership of evaluator.
  EvaluatorCheapestAdditionFilteredDecisionBuilder(
      RoutingModel* model,
      ResultCallback2<int64, /*from*/ int64, /*to*/ int64>* evaluator,
      const std::vector<LocalSearchFilter*>& filters);
  virtual ~EvaluatorCheapestAdditionFilteredDecisionBuilder() {}

 private:
  // Next nodes are sorted according to the current evaluator.
  virtual void SortPossibleNexts(int64 from, std::vector<int64>* sorted_nexts);

  std::unique_ptr<ResultCallback2<int64, int64, int64> > evaluator_;
};

// A CheapestAdditionFilteredDecisionBuilder where the notion of 'cheapest arc'
// comes from an arc comparator.
class ComparatorCheapestAdditionFilteredDecisionBuilder
    : public CheapestAdditionFilteredDecisionBuilder {
 public:
  // Takes ownership of evaluator.
  ComparatorCheapestAdditionFilteredDecisionBuilder(
      RoutingModel* model, ResultCallback3<bool, /*from*/ int64, /*to1*/ int64,
                                           /*to2*/ int64>* comparator,
      const std::vector<LocalSearchFilter*>& filters);
  virtual ~ComparatorCheapestAdditionFilteredDecisionBuilder() {}

 private:
  // Next nodes are sorted according to the current comparator.
  virtual void SortPossibleNexts(int64 from, std::vector<int64>* sorted_nexts);

  std::unique_ptr<ResultCallback3<bool, int64, int64, int64> > comparator_;
};

// Routing filters

class RoutingLocalSearchFilter : public IntVarLocalSearchFilter {
 public:
  RoutingLocalSearchFilter(const std::vector<IntVar*> nexts,
                           Callback1<int64>* objective_callback);
  virtual ~RoutingLocalSearchFilter() {}
  virtual void InjectObjectiveValue(int64 objective_value);

 protected:
  bool CanPropagateObjectiveValue() const {
    return objective_callback_.get() != nullptr;
  }
  void PropagateObjectiveValue(int64 objective_value);

  int64 injected_objective_value_;

 private:
  std::unique_ptr<Callback1<int64> > objective_callback_;
};

RoutingLocalSearchFilter* MakeNodeDisjunctionFilter(
    const RoutingModel& routing_model, Callback1<int64>* objective_callback);
RoutingLocalSearchFilter* MakePathCumulFilter(
    const RoutingModel& routing_model, const RoutingDimension& dimension,
    Callback1<int64>* objective_callback);
RoutingLocalSearchFilter* MakeNodePrecedenceFilter(
    const RoutingModel& routing_model, const RoutingModel::NodePairs& pairs);

}  // namespace operations_research

#endif  // OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_H_
