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


// The vehicle routing library lets one model and solve generic vehicle routing
// problems ranging from the Traveling Salesman Problem to more complex
// problems such as the Capacitated Vehicle Routing Problem with Time Windows.
//
// The objective of a vehicle routing problem is to build routes covering a set
// of nodes minimizing the overall cost of the routes (usually proportional to
// the sum of the lengths of each segment of the routes) while respecting some
// problem-specific constraints (such as the length of a route). A route is
// equivalent to a path connecting nodes, starting/ending at specific
// starting/ending nodes.
//
// The term "vehicle routing" is historical and the category of problems solved
// is not limited to the routing of vehicles: any problem involving finding
// routes visiting a given number of nodes optimally falls under this category
// of problems, such as finding the optimal sequence in a playlist.
// The literature around vehicle routing problems is extremely dense but one
// can find some basic introductions in the following links:
// - http://en.wikipedia.org/wiki/Travelling_salesman_problem
// - http://www.tsp.gatech.edu/history/index.html
// - http://en.wikipedia.org/wiki/Vehicle_routing_problem
//
// The vehicle routing library is a vertical layer above the constraint
// programming library (ortools/constraint_programming:cp).
// One has access to all underlying constrained variables of the vehicle
// routing model which can therefore be enriched by adding any constraint
// available in the constraint programming library.
//
// There are two sets of variables available:
// - path variables:
//   * "next(i)" variables representing the immediate successor of the node
//     corresponding to i; use IndexToNode() to get the node corresponding to
//     a "next" variable value; note that node indices are strongly typed
//     integers (cf. ortools/base/int_type.h);
//   * "vehicle(i)" variables representing the vehicle route to which the
//     node corresponding to i belongs;
//   * "active(i)" boolean variables, true if the node corresponding to i is
//     visited and false if not; this can be false when nodes are either
//     optional or part of a disjunction;
//   * The following relationships hold for all i:
//      active(i) == 0 <=> next(i) == i <=> vehicle(i) == -1,
//      next(i) == j => vehicle(j) == vehicle(i).
// - dimension variables, used when one is accumulating quantities along routes,
//   such as weight or volume carried, distance or time:
//   * "cumul(i,d)" variables representing the quantity of dimension d when
//     arriving at the node corresponding to i;
//   * "transit(i,d)" variables representing the quantity of dimension d added
//     after visiting the node corresponding to i.
//   * The following relationship holds for all (i,d):
//       next(i) == j => cumul(j,d) == cumul(i,d) + transit(i,d).
// Solving the vehicle routing problems is mainly done using approximate methods
// (namely local search,
// cf. http://en.wikipedia.org/wiki/Local_search_(optimization) ), potentially
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
//   * Meta-heuristics: used to guide the search out of local minima found by
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
//   number of routes (here, 1):
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
// - Inspect the solution cost and route (only one route here):
//
//    LOG(INFO) << "Cost " << solution->ObjectiveValue();
//    const int route_number = 0;
//    for (int64 node = routing.Start(route_number);
//         !routing.IsEnd(node);
//         node = solution->Value(routing.NextVar(node))) {
//      LOG(INFO) << routing.IndexToNode(node);
//    }
//
//
// Keywords: Vehicle Routing, Traveling Salesman Problem, TSP, VRP, CVRPTW, PDP.

#ifndef OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_H_
#define OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_H_

#include <stddef.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "ortools/base/callback.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"
#include "ortools/base/int_type_indexed_vector.h"
#include "ortools/base/hash.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "ortools/constraint_solver/routing_parameters.pb.h"
#include "ortools/constraint_solver/routing_types.h"
#include "ortools/graph/graph.h"
#include "ortools/util/range_query_function.h"
#include "ortools/util/sorted_interval_list.h"
#include "ortools/base/adjustable_priority_queue-inl.h"
#include "ortools/base/adjustable_priority_queue.h"


namespace operations_research {

class IntVarFilteredDecisionBuilder;
class LocalSearchOperator;
class RoutingDimension;
#ifndef SWIG
class SweepArranger;
#endif
struct SweepNode;

class RoutingModel {
 public:
  // Status of the search.
  enum Status {
    // Problem not solved yet (before calling RoutingModel::Solve()).
    ROUTING_NOT_SOLVED,
    // Problem solved successfully after calling RoutingModel::Solve().
    ROUTING_SUCCESS,
    // No solution found to the problem after calling RoutingModel::Solve().
    ROUTING_FAIL,
    // Time limit reached before finding a solution with RoutingModel::Solve().
    ROUTING_FAIL_TIMEOUT,
    // Model, model parameters or flags are not valid.
    ROUTING_INVALID
  };

  typedef RoutingNodeIndex NodeIndex;
  typedef RoutingCostClassIndex CostClassIndex;
  typedef RoutingDimensionIndex DimensionIndex;
  typedef RoutingDisjunctionIndex DisjunctionIndex;
  typedef RoutingVehicleClassIndex VehicleClassIndex;
  typedef RoutingNodeEvaluator2 NodeEvaluator2;
  typedef RoutingTransitEvaluator2 TransitEvaluator2;

// TODO(user): Remove all SWIG guards by adding the @ignore in .i.
#if !defined(SWIG)
  typedef RoutingNodePair NodePair;
  typedef RoutingNodePairs NodePairs;
#endif  // SWIG

#if !defined(SWIG)
  // What follows is relevant for models with time/state dependent transits.
  // Such transits, say from node A to node B, are functions f: int64->int64
  // of the cumuls of a dimension. The user is free to implement the abstract
  // RangeIntToIntFunction interface, but it is expected that the implementation
  // of each method is quite fast. For performance-related reasons,
  // StateDependentTransit keeps an additional pointer to a
  // RangeMinMaxIndexFunction, with similar functionality to
  // RangeIntToIntFunction, for g(x) = f(x)+x, where f is the transit from A to
  // B. In most situations the best solutions are problem-specific, but in case
  // of doubt the user may use the MakeStateDependentTransit function from the
  // routing library, which works out-of-the-box, with very good running time,
  // but memory inefficient in some situations.
  struct StateDependentTransit {
    RangeIntToIntFunction* transit;                   // f(x)
    RangeMinMaxIndexFunction* transit_plus_identity;  // g(x) = f(x) + x
  };
  typedef ResultCallback2<StateDependentTransit, NodeIndex, NodeIndex>
      VariableNodeEvaluator2;
  typedef std::function<StateDependentTransit(int64, int64)>
      VariableIndexEvaluator2;
#endif  // SWIG

#if !defined(SWIG)
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
    struct DimensionCost {
      int64 transit_evaluator_class;
      int64 cost_coefficient;
      const RoutingDimension* dimension;
      bool operator<(const DimensionCost& cost) const {
        if (transit_evaluator_class != cost.transit_evaluator_class) {
          return transit_evaluator_class < cost.transit_evaluator_class;
        }
        return cost_coefficient < cost.cost_coefficient;
      }
    };
    std::vector<DimensionCost>
        dimension_transit_evaluator_class_and_cost_coefficient;

    explicit CostClass(NodeEvaluator2* arc_cost_evaluator)
        : arc_cost_evaluator(arc_cost_evaluator) {
      CHECK(arc_cost_evaluator != nullptr);
    }

    // Comparator for STL containers and algorithms.
    static bool LessThan(const CostClass& a, const CostClass& b) {
      if (a.arc_cost_evaluator != b.arc_cost_evaluator) {
        return a.arc_cost_evaluator < b.arc_cost_evaluator;
      }
      return a.dimension_transit_evaluator_class_and_cost_coefficient <
             b.dimension_transit_evaluator_class_and_cost_coefficient;
    }
  };

  struct VehicleClass {
    // The cost class of the vehicle.
    CostClassIndex cost_class_index;
    // Contrarily to CostClass, here we need strict equivalence.
    int64 fixed_cost;
    // Vehicle start and end nodes. Currently if two vehicles have different
    // start/end nodes which are "physically" located at the same place, these
    // two vehicles will be considered as non-equivalent.
    // TODO(user): Find equivalent start/end nodes wrt dimensions and
    // callbacks.
    RoutingModel::NodeIndex start;
    RoutingModel::NodeIndex end;
    // Bounds of cumul variables at start and end vehicle nodes.
    // dimension_{start,end}_cumuls_{min,max}[d] is the bound for dimension d.
    ITIVector<DimensionIndex, int64> dimension_start_cumuls_min;
    ITIVector<DimensionIndex, int64> dimension_start_cumuls_max;
    ITIVector<DimensionIndex, int64> dimension_end_cumuls_min;
    ITIVector<DimensionIndex, int64> dimension_end_cumuls_max;
    ITIVector<DimensionIndex, int64> dimension_capacities;
    // dimension_evaluators[d]->Run(from, to) is the transit value of arc
    // from->to for a dimension d.
    ITIVector<DimensionIndex, int64> dimension_evaluator_classes;
    // Fingerprint of unvisitable non-start/end nodes.
    uint64 unvisitable_nodes_fprint;

    // Comparator for STL containers and algorithms.
    static bool LessThan(const VehicleClass& a, const VehicleClass& b);
  };
#endif  // defined(SWIG)

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

  // In the following constructors, the versions which do not take
  // RoutingModelParameters are equivalent to passing DefaultModelParameters.
  // A depot is the start and end node of the route of a vehicle.
  // Constructor taking a single depot node which is used as the start and
  // end node for all vehicles.
  RoutingModel(int nodes, int vehicles, NodeIndex depot);
  RoutingModel(int nodes, int vehicles, NodeIndex depot,
               const RoutingModelParameters& parameters);
  // Constructor taking a vector of (start node, end node) pairs for each
  // vehicle route. Used to model multiple depots.
  RoutingModel(int nodes, int vehicles,
               const std::vector<std::pair<NodeIndex, NodeIndex> >& start_end);
  RoutingModel(int nodes, int vehicles,
               const std::vector<std::pair<NodeIndex, NodeIndex> >& start_end,
               const RoutingModelParameters& parameters);
  // Constructor taking vectors of start nodes and end nodes for each
  // vehicle route. Used to model multiple depots.
  // TODO(user): added to simplify SWIG wrapping. Remove when swigging
  // std::vector<std::pair<int, int> > is ok.
  RoutingModel(int nodes, int vehicles, const std::vector<NodeIndex>& starts,
               const std::vector<NodeIndex>& ends);
  RoutingModel(int nodes, int vehicles, const std::vector<NodeIndex>& starts,
               const std::vector<NodeIndex>& ends,
               const RoutingModelParameters& parameters);
  ~RoutingModel();

  static RoutingModelParameters DefaultModelParameters();
  static RoutingSearchParameters DefaultSearchParameters();

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
                                       std::vector<int64> vehicle_capacities,
                                       bool fix_start_cumul_to_zero,
                                       const std::string& name);
  bool AddDimensionWithVehicleTransitAndCapacity(
      const std::vector<NodeEvaluator2*>& evaluators, int64 slack_max,
      std::vector<int64> vehicle_capacities, bool fix_start_cumul_to_zero,
      const std::string& name);
  // Creates a dimension where the transit variable is constrained to be
  // equal to 'value'; 'capacity' is the upper bound of the cumul variables.
  // 'name' is the name used to reference the dimension; this name is used to
  // get cumul and transit variables from the routing model.
  // Returns false if a dimension with the same name has already been created
  // (and doesn't create the new dimension).
  bool AddConstantDimensionWithSlack(int64 value, int64 capacity,
                                     int64 slack_max,
                                     bool fix_start_cumul_to_zero,
                                     const std::string& name);
  bool AddConstantDimension(int64 value, int64 capacity,
                            bool fix_start_cumul_to_zero, const std::string& name) {
    return AddConstantDimensionWithSlack(value, capacity, 0,
                                         fix_start_cumul_to_zero, name);
  }
  // Creates a dimension where the transit variable is constrained to be
  // equal to 'values[i]' for node i; 'capacity' is the upper bound of
  // the cumul variables. 'name' is the name used to reference the dimension;
  // this name is used to get cumul and transit variables from the routing
  // model.
  // Returns false if a dimension with the same name has already been created
  // (and doesn't create the new dimension).
  bool AddVectorDimension(std::vector<int64> values, int64 capacity,
                          bool fix_start_cumul_to_zero, const std::string& name);
  // Creates a dimension where the transit variable is constrained to be
  // equal to 'values[i][next(i)]' for node i; 'capacity' is the upper bound of
  // the cumul variables. 'name' is the name used to reference the dimension;
  // this name is used to get cumul and transit variables from the routing
  // model.
  // Returns false if a dimension with the same name has already been created
  // (and doesn't create the new dimension).
  bool AddMatrixDimension(std::vector<std::vector<int64> > values,
                          int64 capacity, bool fix_start_cumul_to_zero,
                          const std::string& name);
#if !defined(SWIG)
  // Creates a dimension with transits depending on the cumuls of another
  // dimension. 'pure_transits' are the per-vehicle fixed transits as above.
  // 'dependent_transits' is a vector containing for each vehicle a function
  // taking two nodes and returning a function taking a dimension cumul value
  // and returning a transit value. 'base_dimension' indicates the dimension
  // from which the cumul variable is taken. If 'base_dimension' is nullptr,
  // then the newly created dimension is self-based.
  bool AddDimensionDependentDimensionWithVehicleCapacity(
      const std::vector<NodeEvaluator2*>& pure_transits,
      const std::vector<VariableNodeEvaluator2*>& dependent_transits,
      const RoutingDimension* base_dimension, int64 slack_max,
      std::vector<int64> vehicle_capacities, bool fix_start_cumul_to_zero,
      const std::string& name) {
    return AddDimensionDependentDimensionWithVehicleCapacityInternal(
        pure_transits, dependent_transits, base_dimension, slack_max,
        std::move(vehicle_capacities), fix_start_cumul_to_zero, name);
  }
  // As above, but pure_transits are taken to be zero evaluators.
  bool AddDimensionDependentDimensionWithVehicleCapacity(
      const std::vector<VariableNodeEvaluator2*>& transits,
      const RoutingDimension* base_dimension, int64 slack_max,
      std::vector<int64> vehicle_capacities, bool fix_start_cumul_to_zero,
      const std::string& name);
  // Homogeneous versions of the functions above.
  bool AddDimensionDependentDimensionWithVehicleCapacity(
      VariableNodeEvaluator2* transits, const RoutingDimension* base_dimension,
      int64 slack_max, int64 vehicle_capacity, bool fix_start_cumul_to_zero,
      const std::string& name);
  bool AddDimensionDependentDimensionWithVehicleCapacity(
      NodeEvaluator2* pure_transits, VariableNodeEvaluator2* dependent_transits,
      const RoutingDimension* base_dimension, int64 slack_max,
      int64 vehicle_capacity, bool fix_start_cumul_to_zero, const std::string& name);
  // Creates a cached StateDependentTransit from an std::function.
  static RoutingModel::StateDependentTransit MakeStateDependentTransit(
      const std::function<int64(int64)>& f, int64 domain_start,
      int64 domain_end);
#endif  // SWIG
  // Outputs the names of all dimensions added to the routing engine.
  // TODO(user): rename.
  std::vector<::std::string> GetAllDimensionNames() const;
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
  // "penalty" must be positive to make the disjunction optional; a negative
  // penalty will force one node of the disjunction to be performed, and
  // therefore p == 0.
  // Note: passing a vector with a single node will model an optional node
  // with a penalty cost if it is not visited.
  void AddDisjunction(const std::vector<NodeIndex>& nodes, int64 penalty);
  // Same as above except that instead of allowing at most one of the nodes to
  // be active, the maximum number of active nodes is max_cardinality.
  void AddDisjunction(const std::vector<NodeIndex>& nodes, int64 penalty,
                      int64 max_cardinality);
  // Returns the indices of the disjunctions to which a node belongs.
  const std::vector<DisjunctionIndex>& GetDisjunctionIndicesFromNode(
      NodeIndex node) const {
    return GetDisjunctionIndicesFromVariableIndex(NodeToIndex(node));
  }
  const std::vector<DisjunctionIndex>& GetDisjunctionIndicesFromVariableIndex(
      int64 index) const {
    return node_to_disjunctions_[index];
  }
  // Calls f for each variable index of nodes in the same disjunctions as the
  // node corresponding to the variable index 'index'; only disjunctions of
  // cardinality 'cardinality' are considered.
  template <typename F>
  void ForEachNodeInDisjunctionWithMaxCardinalityFromIndex(
      int64 index, int64 max_cardinality, F f) const {
    for (const DisjunctionIndex disjunction :
         GetDisjunctionIndicesFromVariableIndex(index)) {
      if (disjunctions_[disjunction].value.max_cardinality == max_cardinality) {
        for (const int node : disjunctions_[disjunction].nodes) {
          f(node);
        }
      }
    }
  }
#if !defined(SWIGPYTHON)
  // Returns the variable indices of the nodes in the disjunction of index
  // 'index'.
  const std::vector<int>& GetDisjunctionIndices(DisjunctionIndex index) const {
    return disjunctions_[index].nodes;
  }
#endif  // !defined(SWIGPYTHON)
  // Returns the penalty of the node disjunction of index 'index'.
  int64 GetDisjunctionPenalty(DisjunctionIndex index) const {
    return disjunctions_[index].value.penalty;
  }
  // Returns the maximum number of possible active nodes of the node disjunction
  // of index 'index'.
  int64 GetDisjunctionMaxCardinality(DisjunctionIndex index) const {
    return disjunctions_[index].value.max_cardinality;
  }
  // Returns the number of node disjunctions in the model.
  int GetNumberOfDisjunctions() const { return disjunctions_.size(); }
  // Returns the list of all perfect binary disjunctions, as pairs of variable
  // indices: a disjunction is "perfect" when its variables do not appear in
  // any other disjunction. Each pair is sorted (lowest variable index first),
  // and the output vector is also sorted (lowest pairs first).
  std::vector<std::pair<int, int> > GetPerfectBinaryDisjunctions() const;
  // SPECIAL: Makes the solver ignore all the disjunctions whose active
  // variables are all trivially zero (i.e. Max() == 0), by setting their
  // max_cardinality to 0.
  // This can be useful when using the BaseBinaryDisjunctionNeighborhood
  // operators, in the context of arc-based routing.
  void IgnoreDisjunctionsAlreadyForcedToZero();

  // Adds a soft contraint to force a set of nodes to be on the same vehicle.
  // If all nodes are not on the same vehicle, each extra vehicle used adds
  // 'cost' to the cost function.
  void AddSoftSameVehicleConstraint(const std::vector<NodeIndex>& nodes,
                                    int64 cost);

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
        {{NodeToIndex(node1)}, {NodeToIndex(node2)}});
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
  int64 UnperformedPenalty(int64 var_index) const;
  // Same as above except that it returns default_value instead of 0 when
  // penalty is not well defined (default value is passed as first argument to
  // simplify the usage of the method in a callback).
  int64 UnperformedPenaltyOrValue(int64 default_value, int64 var_index) const;
  // Returns the variable index of the first starting or ending node of all
  // routes. If all routes start  and end at the same node (single depot), this
  // is the node returned.
  int64 GetDepot() const;

  // Sets the cost function of the model such that the cost of a segment of a
  // route between node 'from' and 'to' is evaluator(from, to), whatever the
  // route or vehicle performing the route.
  // Takes ownership of the callback 'evaluator'.
  void SetArcCostEvaluatorOfAllVehicles(NodeEvaluator2* evaluator);
  // Sets the cost function for a given vehicle route.
  // Takes ownership of the callback 'evaluator'.
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
// Gets/sets the evaluator used when the first solution heuristic is set to
// ROUTING_EVALUATOR_STRATEGY (variant of ROUTING_PATH_CHEAPEST_ARC using
// 'evaluator' to sort node segments).
#ifndef SWIG
  const Solver::IndexEvaluator2& first_solution_evaluator() const {
    return first_solution_evaluator_;
  }
#endif
  // Takes ownership of evaluator.
  void SetFirstSolutionEvaluator(Solver::IndexEvaluator2 evaluator) {
    first_solution_evaluator_ = std::move(evaluator);
  }
  // Adds a local search operator to the set of operators used to solve the
  // vehicle routing problem.
  void AddLocalSearchOperator(LocalSearchOperator* ls_operator);
  // Adds a search monitor to the search used to solve the routing model.
  void AddSearchMonitor(SearchMonitor* const monitor);
  // Adds a callback called each time a solution is found during the search.
  // This is a shortcut to creating a monitor to call the callback on
  // AtSolution() and adding it with AddSearchMonitor.
  void AddAtSolutionCallback(std::function<void()> callback);
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
  // This is equivalent to calling
  // CloseModelWithParameters(DefaultSearchParameters()).
  void CloseModel();
  // Same as above taking search parameters (as of 10/2015 some the parameters
  // have to be set when closing the model).
  void CloseModelWithParameters(
      const RoutingSearchParameters& search_parameters);
  // Solves the current routing model; closes the current model.
  // This is equivalent to calling
  // SolveWithParameters(DefaultSearchParameters())
  // or
  // SolveFromAssignmentWithParameters(assignment, DefaultSearchParameters()).
  const Assignment* Solve(const Assignment* assignment = nullptr);
  // Solves the current routing model with the given parameters.
  const Assignment* SolveWithParameters(
      const RoutingSearchParameters& search_parameters);
  const Assignment* SolveFromAssignmentWithParameters(
      const Assignment* assignment,
      const RoutingSearchParameters& search_parameters);
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
  bool ApplyLocksToAllVehicles(
      const std::vector<std::vector<NodeIndex> >& locks, bool close_routes);
  // Returns an assignment used to fix some of the variables of the problem.
  // In practice, this assignment locks partial routes of the problem. This
  // can be used in the context of locking the parts of the routes which have
  // already been driven in online routing problems.
  const Assignment* const PreAssignment() const { return preassignment_; }
  Assignment* MutablePreAssignment() { return preassignment_; }
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
  Assignment* ReadAssignmentFromRoutes(
      const std::vector<std::vector<NodeIndex> >& routes,
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
  void AssignmentToRoutes(
      const Assignment& assignment,
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
  // performed; if one of these checks fails no attempts to repair it are made
  // (instead, the method returns nullptr).
  Assignment* CompactAssignment(const Assignment& assignment) const;
  // Same as CompactAssignment() but also checks the validity of the final
  // compact solution; if it is not valid, no attempts to repair it are made
  // (instead, the method returns nullptr).
  Assignment* CompactAndCheckAssignment(const Assignment& assignment) const;
  // Adds an extra variable to the vehicle routing assignment.
  void AddToAssignment(IntVar* const var);
  void AddIntervalToAssignment(IntervalVar* const interval);
#ifndef SWIG
  // TODO(user): Revisit if coordinates are added to the RoutingModel class.
  void SetSweepArranger(SweepArranger* sweep_arranger) {
    sweep_arranger_.reset(sweep_arranger);
  }
  // Returns the sweep arranger to be used by routing heuristics.
  SweepArranger* sweep_arranger() const { return sweep_arranger_.get(); }
#endif
  // Adds a custom local search filter to the list of filters used to speed up
  // local search by pruning unfeasible variable assignments.
  // Calling this method after the routing model has been closed (CloseModel()
  // or Solve() has been called) has no effect.
  // The routing model does not take ownership of the filter.
  void AddLocalSearchFilter(LocalSearchFilter* filter) {
    CHECK(filter != nullptr);
    if (closed_) {
      LOG(WARNING) << "Model is closed, filter addition will be ignored.";
    }
    extra_filters_.push_back(filter);
  }

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
#if !defined(SWIGPYTHON)
  // Returns all next variables of the model, such that Nexts(i) is the next
  // variable of the node corresponding to i.
  const std::vector<IntVar*>& Nexts() const { return nexts_; }
  // Returns all vehicle variables of the model,  such that VehicleVars(i) is
  // the vehicle variable of the node corresponding to i.
  const std::vector<IntVar*>& VehicleVars() const { return vehicle_vars_; }
#endif  // !defined(SWIGPYTHON)
  // Returns the next variable of the node corresponding to index. Note that
  // NextVar(index) == index is equivalent to ActiveVar(index) == 0.
  IntVar* NextVar(int64 index) const { return nexts_[index]; }
  // Returns the active variable of the node corresponding to index.
  IntVar* ActiveVar(int64 index) const { return active_[index]; }
  // Returns the vehicle variable of the node corresponding to index. Note that
  // VehicleVar(index) == -1 is equivalent to ActiveVar(index) == 0.
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
  VehicleClassIndex GetVehicleClassIndexOfVehicle(int64 vehicle) const {
    DCHECK(closed_);
    return vehicle_class_index_of_vehicle_[vehicle];
  }
  // Returns the number of different vehicle classes in the model.
  int GetVehicleClassesCount() const { return vehicle_classes_.size(); }
  // Returns variable indices of nodes constrained to be on the same route.
  const std::vector<int>& GetSameVehicleIndicesOfIndex(int node) const {
    DCHECK(closed_);
    return same_vehicle_groups_[same_vehicle_group_[node]];
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
  // Returns the node index from an index value resulting from a next variable.
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

  // Returns statistics on first solution search, number of decisions sent to
  // filters, number of decisions rejected by filters.
  int64 GetNumberOfDecisionsInFirstSolution(
      const RoutingSearchParameters& search_parameters) const;
  int64 GetNumberOfRejectsInFirstSolution(
      const RoutingSearchParameters& search_parameters) const;

  // Internal only: initializes the builders used to build a solver model from
  // CpModels.
  static void InitializeBuilders(Solver* solver);

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

  // The next few members are in the public section only for testing purposes.
  // TODO(user): Find a way to test and restrict the access at the same time.
  //
  // MakeGuidedSlackFinalizer creates a DecisionBuilder for the slacks of a
  // dimension using a callback to choose which values to start with.
  // The finalizer works only when all next variables in the model have
  // been fixed. It has the following two characteristics:
  // 1. It follows the routes defined by the nexts variables when choosing a
  //    variable to make a decision on.
  // 2. When it comes to choose a value for the slack of node i, the decision
  //    builder first calls the callback with argument i, and supposingly the
  //    returned value is x it creates decisions slack[i] = x, slack[i] = x + 1,
  //    slack[i] = x - 1, slack[i] = x + 2, etc.
  DecisionBuilder* MakeGuidedSlackFinalizer(
      const RoutingDimension* dimension,
      std::function<int64(int64)> initializer);
#ifndef SWIG
  // TODO(user): MakeGreedyDescentLSOperator is too general for routing.h.
  // Perhaps move it to constraint_solver.h.
  // MakeGreedyDescentLSOperator creates a local search operator that tries to
  // improve the initial assignment by moving a logarithmically decreasing step
  // away in each possible dimension.
  static std::unique_ptr<LocalSearchOperator> MakeGreedyDescentLSOperator(
      std::vector<IntVar*> variables);
#endif  // __SWIG__
  // MakeSelfDependentDimensionFinalizer is a finalizer for the slacks of a
  // self-dependent dimension. It makes an extensive use of the caches of the
  // state dependent transits.
  // In detail, MakeSelfDependentDimensionFinalizer returns a composition of a
  // local search decision builder with a greedy descent operator for the cumul
  // of the start of each route and a guided slack finalizer. Provided there are
  // no time windows and the maximum slacks are large enough, once the cumul of
  // the start of route is fixed, the guided finalizer can find optimal values
  // of the slacks for the rest of the route in time proportional to the length
  // of the route. Therefore the composed finalizer generally works in time
  // O(log(t)*n*m), where t is the latest possible departute time, n is the
  // number of nodes in the network and m is the number of vehicles.
  DecisionBuilder* MakeSelfDependentDimensionFinalizer(
      const RoutingDimension* dimension);

 private:
  // Local search move operator usable in routing.
  enum RoutingLocalSearchOperator {
    RELOCATE = 0,
    RELOCATE_PAIR,
    RELOCATE_NEIGHBORS,
    EXCHANGE,
    CROSS,
    CROSS_EXCHANGE,
    TWO_OPT,
    OR_OPT,
    LIN_KERNIGHAN,
    TSP_OPT,
    MAKE_ACTIVE,
    RELOCATE_AND_MAKE_ACTIVE,
    MAKE_ACTIVE_AND_RELOCATE,
    MAKE_INACTIVE,
    MAKE_CHAIN_INACTIVE,
    SWAP_ACTIVE,
    EXTENDED_SWAP_ACTIVE,
    NODE_PAIR_SWAP,
    PATH_LNS,
    FULL_PATH_LNS,
    TSP_LNS,
    INACTIVE_LNS,
    LOCAL_SEARCH_OPERATOR_COUNTER
  };

  // Structure storing a value for a set of nodes. Is used to store data for
  // node disjunctions (nodes, max_cardinality and penalty when unperformed).
  template <typename T>
  struct ValuedNodes {
    std::vector<int> nodes;
    T value;
  };
  struct DisjunctionValues {
    int64 penalty;
    int64 max_cardinality;
  };
  typedef ValuedNodes<DisjunctionValues> Disjunction;

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
  void SetStartEnd(
      const std::vector<std::pair<NodeIndex, NodeIndex> >& start_end);
  void AddDisjunctionInternal(const std::vector<NodeIndex>& nodes,
                              int64 penalty, int64 max_cardinality);
  void AddNoCycleConstraintInternal();
  bool AddDimensionWithCapacityInternal(
      const std::vector<NodeEvaluator2*>& evaluators, int64 slack_max,
      std::vector<int64> vehicle_capacities, bool fix_start_cumul_to_zero,
      const std::string& dimension_name);
  bool AddDimensionDependentDimensionWithVehicleCapacityInternal(
      const std::vector<NodeEvaluator2*>& pure_transits,
      const std::vector<VariableNodeEvaluator2*>& dependent_transits,
      const RoutingDimension* base_dimension, int64 slack_max,
      std::vector<int64> vehicle_capacities, bool fix_start_cumul_to_zero,
      const std::string& name);
  bool InitializeDimensionInternal(
      const std::vector<NodeEvaluator2*>& evaluators,
      const std::vector<VariableNodeEvaluator2*>& state_dependent_evaluators,
      int64 slack_max, bool fix_start_cumul_to_zero,
      RoutingDimension* dimension);
  DimensionIndex GetDimensionIndex(const std::string& dimension_name) const;
  uint64 GetFingerprintOfEvaluator(NodeEvaluator2* evaluator,
                                   bool fingerprint_arc_cost_evaluators) const;
  void ComputeCostClasses(const RoutingSearchParameters& parameters);
  void ComputeVehicleClasses();
  int64 GetArcCostForClassInternal(int64 from_index, int64 to_index,
                                   CostClassIndex cost_class_index);
  void AppendHomogeneousArcCosts(const RoutingSearchParameters& parameters,
                                 int node_index,
                                 std::vector<IntVar*>* cost_elements);
  void AppendArcCosts(const RoutingSearchParameters& parameters, int node_index,
                      std::vector<IntVar*>* cost_elements);
  Assignment* DoRestoreAssignment();
  static const CostClassIndex kCostClassIndexOfZeroCost;
  int64 SafeGetCostClassInt64OfVehicle(int64 vehicle) const {
    DCHECK_LT(0, vehicles_);
    return (vehicle >= 0 ? GetCostClassIndexOfVehicle(vehicle)
                         : kCostClassIndexOfZeroCost)
        .value();
  }
  int64 GetDimensionTransitCostSum(int64 i, int64 j,
                                   const CostClass& cost_class) const;
  // Returns nullptr if no penalty cost, otherwise returns penalty variable.
  IntVar* CreateDisjunction(DisjunctionIndex disjunction);
  // Returns the cost variable related to the soft same vehicle constraint of
  // index 'index'.
  IntVar* CreateSameVehicleCost(int index);
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
  VariableNodeEvaluator2* NewCachedStateDependentCallback(
      VariableNodeEvaluator2* callback);
  void QuietCloseModel();
  void QuietCloseModelWithParameters(
      const RoutingSearchParameters& parameters) {
    if (!closed_) {
      CloseModelWithParameters(parameters);
    }
  }
  // See CompactAssignment. Checks the final solution if
  // check_compact_assignement is true.
  Assignment* CompactAssignmentInternal(const Assignment& assignment,
                                        bool check_compact_assignment) const;
  // Check that current search parameters are valid. If not, the status of the
  // solver is set to ROUTING_INVALID.
  bool ValidateSearchParameters(
      const RoutingSearchParameters& search_parameters);
  // Sets up search objects, such as decision builders and monitors.
  void SetupSearch(const RoutingSearchParameters& search_parameters);
  // Set of auxiliary methods used to setup the search.
  // TODO(user): Document each auxiliary method.
  Assignment* GetOrCreateAssignment();
  SearchLimit* GetOrCreateLimit();
  SearchLimit* GetOrCreateLocalSearchLimit();
  SearchLimit* GetOrCreateLargeNeighborhoodSearchLimit();
  LocalSearchOperator* CreateInsertionOperator();
  LocalSearchOperator* CreateMakeInactiveOperator();
  void CreateNeighborhoodOperators();
  LocalSearchOperator* GetNeighborhoodOperators(
      const RoutingSearchParameters& search_parameters) const;
  const std::vector<LocalSearchFilter*>& GetOrCreateLocalSearchFilters();
  const std::vector<LocalSearchFilter*>& GetOrCreateFeasibilityFilters();
  DecisionBuilder* CreateSolutionFinalizer();
  void CreateFirstSolutionDecisionBuilders(
      const RoutingSearchParameters& search_parameters);
  DecisionBuilder* GetFirstSolutionDecisionBuilder(
      const RoutingSearchParameters& search_parameters) const;
  IntVarFilteredDecisionBuilder* GetFilteredFirstSolutionDecisionBuilderOrNull(
      const RoutingSearchParameters& parameters) const;
  LocalSearchPhaseParameters* CreateLocalSearchParameters(
      const RoutingSearchParameters& search_parameters);
  DecisionBuilder* CreateLocalSearchDecisionBuilder(
      const RoutingSearchParameters& search_parameters);
  void SetupDecisionBuilders(const RoutingSearchParameters& search_parameters);
  void SetupMetaheuristics(const RoutingSearchParameters& search_parameters);
  void SetupAssignmentCollector();
  void SetupTrace(const RoutingSearchParameters& search_parameters);
  void SetupSearchMonitors(const RoutingSearchParameters& search_parameters);
  bool UsesLightPropagation(
      const RoutingSearchParameters& search_parameters) const;

  int64 GetArcCostForCostClassInternal(int64 i, int64 j, int64 cost_class);
  int GetVehicleStartClass(int64 start) const;

  void InitSameVehicleGroups(int number_of_groups) {
    same_vehicle_group_.assign(Size(), 0);
    same_vehicle_groups_.assign(number_of_groups, {});
  }
  void SetSameVehicleGroup(int index, int group) {
    same_vehicle_group_[index] = group;
    same_vehicle_groups_[group].push_back(index);
  }

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
  RevSwitch is_bound_to_end_ct_added_;
  // Dimensions
  std::unordered_map<std::string, DimensionIndex> dimension_name_to_index_;
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
  bool cache_callbacks_;
  std::vector<CostCacheElement> cost_cache_;  // Index by source index.
  std::vector<VehicleClassIndex> vehicle_class_index_of_vehicle_;
#ifndef SWIG
  ITIVector<VehicleClassIndex, VehicleClass> vehicle_classes_;
#endif  // SWIG
  std::function<int(int64)> vehicle_start_class_callback_;
  // Cached callbacks
  std::unordered_map<const NodeEvaluator2*, NodeEvaluator2*>
      cached_node_callbacks_;
  std::unordered_map<const VariableNodeEvaluator2*, VariableNodeEvaluator2*>
      cached_state_dependent_callbacks_;
  // Disjunctions
  ITIVector<DisjunctionIndex, Disjunction> disjunctions_;
  std::vector<std::vector<DisjunctionIndex> > node_to_disjunctions_;
  // Same vehicle costs
  std::vector<ValuedNodes<int64> > same_vehicle_costs_;
  // Pickup and delivery
  NodePairs pickup_delivery_pairs_;
  // Same vehicle group to which a node belongs.
  std::vector<int> same_vehicle_group_;
  // Same vehicle node groups.
  std::vector<std::vector<int> > same_vehicle_groups_;
  // Index management
  std::vector<NodeIndex> index_to_node_;
  ITIVector<NodeIndex, int> node_to_index_;
  std::vector<int> index_to_vehicle_;
  std::vector<int64> starts_;
  std::vector<int64> ends_;
  int start_end_count_;
  // Model status
  bool closed_;
  Status status_;

  // Search data
  std::vector<DecisionBuilder*> first_solution_decision_builders_;
  std::vector<IntVarFilteredDecisionBuilder*>
      first_solution_filtered_decision_builders_;
  Solver::IndexEvaluator2 first_solution_evaluator_;
  std::vector<LocalSearchOperator*> local_search_operators_;
  std::vector<SearchMonitor*> monitors_;
  SolutionCollector* collect_assignments_;
  DecisionBuilder* solve_db_;
  DecisionBuilder* improve_db_;
  DecisionBuilder* restore_assignment_;
  Assignment* assignment_;
  Assignment* preassignment_;
  std::vector<IntVar*> extra_vars_;
  std::vector<IntervalVar*> extra_intervals_;
  std::vector<LocalSearchOperator*> extra_operators_;
  std::vector<LocalSearchFilter*> filters_;
  std::vector<LocalSearchFilter*> feasibility_filters_;
  std::vector<LocalSearchFilter*> extra_filters_;
  std::vector<IntVar*> variables_maximized_by_finalizer_;
  std::vector<IntVar*> variables_minimized_by_finalizer_;
#ifndef SWIG
  std::unique_ptr<SweepArranger> sweep_arranger_;
#endif

  SearchLimit* limit_;
  SearchLimit* ls_limit_;
  SearchLimit* lns_limit_;

  // Callbacks to be deleted
  std::unordered_set<const NodeEvaluator2*> owned_node_callbacks_;
  std::unordered_set<const VariableNodeEvaluator2*>
      owned_state_dependent_callbacks_;

  friend class RoutingDimension;
  friend class RoutingModelInspector;

  DISALLOW_COPY_AND_ASSIGN(RoutingModel);
};

// Routing model visitor.
class RoutingModelVisitor : public BaseObject {
 public:
  // Constraint types.
  static const char kLightElement[];
  static const char kLightElement2[];
};

// Dimensions represent quantities accumulated at nodes along the routes. They
// represent quantities such as weights or volumes carried along the route, or
// distance or times.
//
// Quantities at a node are represented by "cumul" variables and the increase
// or decrease of quantities between nodes are represented by "transit"
// variables. These variables are linked as follows:
//
// if j == next(i),
// cumuls(j) = cumuls(i) + transits(i) + slacks(i) + state_dependent_transits(i)
//
// where slack is a positive slack variable (can represent waiting times for
// a time dimension), and state_dependent_transits is a non-purely functional
// version of transits_. Favour transits over state_dependent_transits when
// possible, because purely functional callbacks allow more optimisations and
// make the model faster and easier to solve.
class RoutingDimension {
 public:
  ~RoutingDimension();
  // Returns the model on which the dimension was created.
  RoutingModel* model() const { return model_; }
  // Returns the transition value for a given pair of nodes (as var index);
  // this value is the one taken by the corresponding transit variable when
  // the 'next' variable for 'from_index' is bound to 'to_index'.
  int64 GetTransitValue(int64 from_index, int64 to_index, int64 vehicle) const;
  // Same as above but taking a vehicle class of the dimension instead of a
  // vehicle (the class of a vehicle can be obtained with vehicle_to_class()).
  int64 GetTransitValueFromClass(int64 from_index, int64 to_index,
                                 int64 vehicle_class) const {
    return class_evaluators_[vehicle_class](from_index, to_index);
  }
  // Get the cumul, transit and slack variables for the given node (given as
  // int64 var index).
  IntVar* CumulVar(int64 index) const { return cumuls_[index]; }
  IntVar* TransitVar(int64 index) const { return transits_[index]; }
  IntVar* FixedTransitVar(int64 index) const { return fixed_transits_[index]; }
  IntVar* SlackVar(int64 index) const { return slacks_[index]; }
#if !defined(SWIGPYTHON)
  // Like CumulVar(), TransitVar(), SlackVar() but return the whole variable
  // vectors instead (indexed by int64 var index).
  const std::vector<IntVar*>& cumuls() const { return cumuls_; }
  const std::vector<IntVar*>& transits() const { return transits_; }
  const std::vector<IntVar*>& slacks() const { return slacks_; }
#if !defined(SWIGCSHARP) && !defined(SWIGJAVA)
  // Returns forbidden intervals for each node.
  const std::vector<SortedDisjointIntervalList>& forbidden_intervals() const {
    return forbidden_intervals_;
  }
  // Returns the capacities for all vehicles.
  const std::vector<int64>& vehicle_capacities() const {
    return vehicle_capacities_;
  }
  // Returns the callback evaluating the transit value between two node indices
  // for a given vehicle.
  const RoutingModel::TransitEvaluator2& transit_evaluator(int vehicle) const {
    return class_evaluators_[vehicle_to_class_[vehicle]];
  }
  int vehicle_to_class(int vehicle) const { return vehicle_to_class_[vehicle]; }
#endif  // !defined(SWIGCSHARP) && !defined(SWIGJAVA)
#endif  // !defined(SWIGPYTHON)
  // Sets an upper bound on the dimension span on a given vehicle. This is the
  // preferred way to limit the "length" of the route of a vehicle according to
  // a dimension.
  void SetSpanUpperBoundForVehicle(int64 upper_bound, int vehicle);
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

#ifndef SWIG
  // Sets a piecewise linear cost on the cumul variable of a given node.
  // If f is a piecewsie linear function, the resulting cost at node n will be
  // f(CumulVar(n)).
  // As of 3/2017, only non-decreasing positive cost functions are supported.
  void SetCumulVarPiecewiseLinearCost(RoutingModel::NodeIndex node,
                                      const PiecewiseLinearFunction& cost);
  // Same as SetCumulVarPiecewiseLinearCost applied to vehicle start node,
  void SetStartCumulVarPiecewiseLinearCost(int vehicle,
                                           const PiecewiseLinearFunction& cost);
  // Same as SetCumulVarPiecewiseLinearCost applied to vehicle end node,
  void SetEndCumulVarPiecewiseLinearCost(int vehicle,
                                         const PiecewiseLinearFunction& cost);
  // Same as above but using a variable index.
  void SetCumulVarPiecewiseLinearCostFromIndex(
      int64 index, const PiecewiseLinearFunction& cost);
  // Returns true if a piecewise linear cost has been set for a given node.
  bool HasCumulVarPiecewiseLinearCost(RoutingModel::NodeIndex node) const;
  // Returns true if a piecewise linear cost has been set for a given vehicle
  // start node.
  bool HasStartCumulVarPiecewiseLinearCost(int vehicle) const;
  // Returns true if a piecewise linear cost has been set for a given vehicle
  // end node.
  bool HasEndCumulVarPiecewiseLinearCost(int vehicle) const;
  // Returns true if a piecewise linear cost has been set for a given variable
  // index.
  bool HasCumulVarPiecewiseLinearCostFromIndex(int64 index) const;
  // Returns the piecewise linear cost of a cumul variable for a given node.
  const PiecewiseLinearFunction* GetCumulVarPiecewiseLinearCost(
      RoutingModel::NodeIndex node) const;
  // Returns the piecewise linear cost of a cumul variable for a given vehicle
  // start node.
  const PiecewiseLinearFunction* GetStartCumulVarPiecewiseLinearCost(
      int vehicle) const;
  // Returns the piecewise linear cost of a cumul variable for a given vehicle
  // end node.
  const PiecewiseLinearFunction* GetEndCumulVarPiecewiseLinearCost(
      int vehicle) const;
  // Returns the piecewise linear cost of a cumul variable for a given variable
  // index.
  const PiecewiseLinearFunction* GetCumulVarPiecewiseLinearCostFromIndex(
      int64 index) const;
#endif

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
  int64 GetCumulVarSoftUpperBoundCoefficient(
      RoutingModel::NodeIndex node) const;
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

  // Sets a soft lower bound to the cumul variable of a given node. If the
  // value of the cumul variable is less than the bound, a cost proportional
  // to the difference between this value and the bound is added to the cost
  // function of the model:
  // cumulVar > lower_bound -> cost = 0
  // cumulVar <= lower_bound -> cost = coefficient * (lower_bound - cumulVar).
  // This is also handy to model earliness costs when the dimension represents
  // time.
  // Note: Using soft lower and upper bounds or span costs together is, as of
  // 6/2014, not well supported in the sense that an optimal schedule is not
  // guaranteed.
  void SetCumulVarSoftLowerBound(RoutingModel::NodeIndex node,
                                 int64 lower_bound, int64 coefficient);
  // Same as SetCumulVarSoftLowerBound applied to vehicle start node,
  void SetStartCumulVarSoftLowerBound(int vehicle, int64 lower_bound,
                                      int64 coefficient);
  // Same as SetCumulVarSoftLowerBound applied to vehicle end node,
  void SetEndCumulVarSoftLowerBound(int vehicle, int64 lower_bound,
                                    int64 coefficient);
  // Same as SetCumulVarSoftLowerBound but using a variable index.
  void SetCumulVarSoftLowerBoundFromIndex(int64 index, int64 lower_bound,
                                          int64 coefficient);
  // Returns true if a soft lower bound has been set for a given node.
  bool HasCumulVarSoftLowerBound(RoutingModel::NodeIndex node) const;
  // Returns true if a soft lower bound has been set for a given vehicle start
  // node.
  bool HasStartCumulVarSoftLowerBound(int vehicle) const;
  // Returns true if a soft lower bound has been set for a given vehicle end
  // node.
  bool HasEndCumulVarSoftLowerBound(int vehicle) const;
  // Returns true if a soft lower bound has been set for a given variable index.
  bool HasCumulVarSoftLowerBoundFromIndex(int64 index) const;
  // Returns the soft lower bound of a cumul variable for a given node. The
  // "hard" lower bound of the variable is returned if no soft lower bound has
  // been set.
  int64 GetCumulVarSoftLowerBound(RoutingModel::NodeIndex node) const;
  // Returns the soft lower bound of a cumul variable for a given vehicle start
  // node. The "hard" lower bound of the variable is returned if no soft lower
  // bound has been set.
  int64 GetStartCumulVarSoftLowerBound(int vehicle) const;
  // Returns the soft lower bound of a cumul variable for a given vehicle end
  // node. The "hard" lower bound of the variable is returned if no soft lower
  // bound has been set.
  int64 GetEndCumulVarSoftLowerBound(int vehicle) const;
  // Returns the soft lower bound of a cumul variable for a given variable
  // index. The "hard" lower bound of the variable is returned if no soft lower
  // bound has been set.
  int64 GetCumulVarSoftLowerBoundFromIndex(int64 index) const;
  // Returns the cost coefficient of the soft lower bound of a cumul variable
  // for a given node. If no soft lower bound has been set, 0 is returned.
  int64 GetCumulVarSoftLowerBoundCoefficient(
      RoutingModel::NodeIndex node) const;
  // Returns the cost coefficient of the soft lower bound of a cumul variable
  // for a given vehicle start node. If no soft lower bound has been set, 0 is
  // returned.
  int64 GetStartCumulVarSoftLowerBoundCoefficient(int vehicle) const;
  // Returns the cost coefficient of the soft lower bound of a cumul variable
  // for a given vehicle end node. If no soft lower bound has been set, 0 is
  // returned.
  int64 GetEndCumulVarSoftLowerBoundCoefficient(int vehicle) const;
  // Returns the cost coefficient of the soft lower bound of a cumul variable
  // for a variable index. If no soft lower bound has been set, 0 is returned.
  int64 GetCumulVarSoftLowerBoundCoefficientFromIndex(int64 index) const;
  // Sets the breaks for a given vehicle. Breaks are represented by
  // IntervalVars. They may interrupt transits between nodes and increase
  // the value of corresponding slack variables. However an interval cannot
  // overlap the cumul variable of a node (the interval must either be before
  // or after the node).
  void SetBreakIntervalsOfVehicle(std::vector<IntervalVar*> breaks,
                                  int vehicle);
  // Returns the parent in the dependency tree if any or nullptr otherwise.
  const RoutingDimension* base_dimension() const { return base_dimension_; }
  // It makes sense to use the function only for self-dependent dimension.
  // For such dimensions the value of the slack of a node determines the
  // transition cost of the next transit. Provided that
  //   1. cumul[node] is fixed,
  //   2. next[node] and next[next[node]] (if exists) are fixed,
  // the value of slack[node] for which cumul[next[node]] + transit[next[node]]
  // is minimized can be found in O(1) using this function.
  int64 ShortestTransitionSlack(int64 node) const;

  // Returns the name of the dimension.
  const std::string& name() const { return name_; }

  // Accessors.
#ifndef SWIG
  const ReverseArcListGraph<int, int>& GetPrecedenceGraph() const {
    return precedence_graph_;
  }
#endif

  int64 GetSpanUpperBoundForVehicle(int vehicle) const {
    return vehicle_span_upper_bounds_[vehicle];
  }
#ifndef SWIG
  const std::vector<int64>& vehicle_span_upper_bounds() const {
    return vehicle_span_upper_bounds_;
  }
#endif  // SWIG
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

  struct PiecewiseLinearCost {
    PiecewiseLinearCost() : var(nullptr), cost(nullptr) {}
    IntVar* var;
    PiecewiseLinearFunction* cost;
  };

  class SelfBased {};
  RoutingDimension(RoutingModel* model, std::vector<int64> vehicle_capacities,
                   const std::string& name, const RoutingDimension* base_dimension);
  RoutingDimension(RoutingModel* model, std::vector<int64> vehicle_capacities,
                   const std::string& name, SelfBased);
  void Initialize(
      const std::vector<RoutingModel::NodeEvaluator2*>& transit_evaluators,
      const std::vector<RoutingModel::VariableNodeEvaluator2*>&
          state_dependent_node_evaluators,
      int64 slack_max);
  void InitializeCumuls();
  void InitializeTransits(
      const std::vector<RoutingModel::NodeEvaluator2*>& transit_evaluators,
      const std::vector<RoutingModel::VariableNodeEvaluator2*>&
          state_dependent_transit_evaluators,
      int64 slack_max);
  // Sets up the cost variables related to cumul soft upper bounds.
  void SetupCumulVarSoftUpperBoundCosts(
      std::vector<IntVar*>* cost_elements) const;
  // Sets up the cost variables related to cumul soft lower bounds.
  void SetupCumulVarSoftLowerBoundCosts(
      std::vector<IntVar*>* cost_elements) const;
  void SetupCumulVarPiecewiseLinearCosts(
      std::vector<IntVar*>* cost_elements) const;
  // Sets up the cost variables related to the global span and per-vehicle span
  // costs (only for the "slack" part of the latter).
  void SetupGlobalSpanCost(std::vector<IntVar*>* cost_elements) const;
  void SetupSlackAndDependentTransitCosts(
      std::vector<IntVar*>* cost_elements) const;
  // Finalize the model of the dimension.
  void CloseModel(bool use_light_propagation);

  std::vector<IntVar*> cumuls_;
  std::vector<SortedDisjointIntervalList> forbidden_intervals_;
  std::vector<IntVar*> capacity_vars_;
  const std::vector<int64> vehicle_capacities_;
  std::vector<IntVar*> transits_;
  std::vector<IntVar*> fixed_transits_;
  // "transit_evaluators_" does the indexing by vehicle, while
  // "class_evaluators_" does the de-duplicated ownership.
  std::vector<RoutingModel::TransitEvaluator2> class_evaluators_;
  std::vector<int64> vehicle_to_class_;
#ifndef SWIG
  ReverseArcListGraph<int, int> precedence_graph_;
#endif

  // The transits of a dimension may depend on its cumuls or the cumuls of
  // another dimension. There can be no cycles, except for self loops, a typical
  // example for this is a time dimension.
  const RoutingDimension* const base_dimension_;

  // "state_dependent_transit_evaluators_" does the indexing by vehicle, while
  // "state_dependent_class_evaluators_" does the de-duplicated ownership.
  std::vector<RoutingModel::VariableIndexEvaluator2>
      state_dependent_class_evaluators_;
  std::vector<int64> state_dependent_vehicle_to_class_;

  std::vector<IntVar*> slacks_;
  std::vector<IntVar*> dependent_transits_;
  std::vector<int64> vehicle_span_upper_bounds_;
  int64 global_span_cost_coefficient_;
  std::vector<int64> vehicle_span_cost_coefficients_;
  std::vector<SoftBound> cumul_var_soft_upper_bound_;
  std::vector<SoftBound> cumul_var_soft_lower_bound_;
  std::vector<PiecewiseLinearCost> cumul_var_piecewise_linear_cost_;
  RoutingModel* const model_;
  const std::string name_;

  friend class RoutingModel;
  friend class RoutingModelInspector;

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
  IntVarFilteredDecisionBuilder(Solver* solver,
                                const std::vector<IntVar*>& vars,
                                const std::vector<LocalSearchFilter*>& filters);
  ~IntVarFilteredDecisionBuilder() override {}
  Decision* Next(Solver* solver) override;
  // Virtual method to redefine to build a solution.
  virtual bool BuildSolution() = 0;
  // Returns statistics on search, number of decisions sent to filters, number
  // of decisions rejected by filters.
  int64 number_of_decisions() const { return number_of_decisions_; }
  int64 number_of_rejects() const { return number_of_rejects_; }

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
  // Stats on search
  int64 number_of_decisions_;
  int64 number_of_rejects_;
};

// Filter-based decision builder dedicated to routing.
class RoutingFilteredDecisionBuilder : public IntVarFilteredDecisionBuilder {
 public:
  RoutingFilteredDecisionBuilder(
      RoutingModel* model, const std::vector<LocalSearchFilter*>& filters);
  ~RoutingFilteredDecisionBuilder() override {}
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
      RoutingModel* model,
      ResultCallback3<int64, int64, int64, int64>* evaluator,
      ResultCallback1<int64, int64>* penalty_evaluator,
      const std::vector<LocalSearchFilter*>& filters);
  ~CheapestInsertionFilteredDecisionBuilder() override {}

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
  void AppendEvaluatedPositionsAfter(
      int64 node_to_insert, int64 start, int64 next_after_start, int64 vehicle,
      std::vector<ValuedPosition>* valued_positions);
  // Returns the cost of unperforming node 'node_to_insert'. Returns kint64max
  // if penalty callback is null or if the node cannot be unperformed.
  int64 GetUnperformedValue(int64 node_to_insert) const;

  std::unique_ptr<ResultCallback3<int64, int64, int64, int64> > evaluator_;
  std::unique_ptr<ResultCallback1<int64, int64> > penalty_evaluator_;
};

// Filter-based decision builder which builds a solution by inserting
// nodes at their cheapest position on any route; potentially several routes can
// be built in parallel. The cost of a position is computed from an arc-based
// cost callback. The node selected for insertion is the one which minimizes
// insertion cost. If a non null penalty evaluator is passed, making nodes
// unperformed is also taken into account with the corresponding penalty cost.
class GlobalCheapestInsertionFilteredDecisionBuilder
    : public CheapestInsertionFilteredDecisionBuilder {
 public:
  // Takes ownership of evaluators.
  GlobalCheapestInsertionFilteredDecisionBuilder(
      RoutingModel* model,
      ResultCallback3<int64, int64, int64, int64>* evaluator,
      ResultCallback1<int64, int64>* penalty_evaluator,
      const std::vector<LocalSearchFilter*>& filters);
  ~GlobalCheapestInsertionFilteredDecisionBuilder() override {}
  bool BuildSolution() override;

 private:
  class PairEntry;
  class NodeEntry;
  typedef std::unordered_set<PairEntry*> PairEntries;
  typedef std::unordered_set<NodeEntry*> NodeEntries;

  // Inserts all non-inserted pickup and delivery pairs. Maintains a priority
  // queue of possible pair insertions, which is incrementally updated when a
  // pair insertion is committed. Incrementality is obtained by updating pair
  // insertion positions on the four newly modified route arcs: after the pickup
  // insertion position, after the pickup position, after the delivery insertion
  // position and after the delivery position.
  void InsertPairs();
  // Inserts all non-inserted individual nodes. Maintains a priority queue of
  // possible insertions, which is incrementally updated when an insertion is
  // committed. Incrementality is obtained by updating insertion positions on
  // the two newly modified route arcs: after the node insertion position and
  // after the node position.
  void InsertNodes();
  // Initializes the priority queue and the pair entries with the current state
  // of the solution.
  void InitializePairPositions(
      AdjustablePriorityQueue<PairEntry>* priority_queue,
      std::vector<PairEntries>* pickup_to_entries,
      std::vector<PairEntries>* delivery_to_entries);
  // Updates all pair entries inserting a node after node "insert_after" and
  // updates the priority queue accordingly.
  void UpdatePairPositions(int vehicle, int64 insert_after,
                           AdjustablePriorityQueue<PairEntry>* priority_queue,
                           std::vector<PairEntries>* pickup_to_entries,
                           std::vector<PairEntries>* delivery_to_entries) {
    UpdatePickupPositions(vehicle, insert_after, priority_queue,
                          pickup_to_entries, delivery_to_entries);
    UpdateDeliveryPositions(vehicle, insert_after, priority_queue,
                            pickup_to_entries, delivery_to_entries);
  }
  // Updates all pair entries inserting their pickup node after node
  // "insert_after" and updates the priority queue accordingly.
  void UpdatePickupPositions(int vehicle, int64 pickup_insert_after,
                             AdjustablePriorityQueue<PairEntry>* priority_queue,
                             std::vector<PairEntries>* pickup_to_entries,
                             std::vector<PairEntries>* delivery_to_entries);
  // Updates all pair entries inserting their delivery node after node
  // "insert_after" and updates the priority queue accordingly.
  void UpdateDeliveryPositions(
      int vehicle, int64 delivery_insert_after,
      AdjustablePriorityQueue<PairEntry>* priority_queue,
      std::vector<PairEntries>* pickup_to_entries,
      std::vector<PairEntries>* delivery_to_entries);
  // Deletes an entry, removing it from the priority queue and the appropriate
  // pickup and delivery entry sets.
  void DeletePairEntry(PairEntry* entry,
                       AdjustablePriorityQueue<PairEntry>* priority_queue,
                       std::vector<PairEntries>* pickup_to_entries,
                       std::vector<PairEntries>* delivery_to_entries);
  // Initializes the priority queue and the node entries with the current state
  // of the solution.
  void InitializePositions(AdjustablePriorityQueue<NodeEntry>* priority_queue,
                           std::vector<NodeEntries>* position_to_node_entries);
  // Updates all node entries inserting a node after node "insert_after" and
  // updates the priority queue accordingly.
  void UpdatePositions(int vehicle, int64 insert_after,
                       AdjustablePriorityQueue<NodeEntry>* priority_queue,
                       std::vector<NodeEntries>* node_entries);
  // Deletes an entry, removing it from the priority queue and the appropriate
  // node entry sets.
  void DeleteNodeEntry(NodeEntry* entry,
                       AdjustablePriorityQueue<NodeEntry>* priority_queue,
                       std::vector<NodeEntries>* node_entries);
};

// Filter-base decision builder which builds a solution by inserting
// nodes at their cheapest position. The cost of a position is computed
// an arc-based cost callback. Node selected for insertion are considered in
// the order they were created in the model.
class LocalCheapestInsertionFilteredDecisionBuilder
    : public CheapestInsertionFilteredDecisionBuilder {
 public:
  // Takes ownership of evaluator.
  LocalCheapestInsertionFilteredDecisionBuilder(
      RoutingModel* model,
      ResultCallback3<int64, int64, int64, int64>* evaluator,
      const std::vector<LocalSearchFilter*>& filters);
  ~LocalCheapestInsertionFilteredDecisionBuilder() override {}
  bool BuildSolution() override;

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
  ~CheapestAdditionFilteredDecisionBuilder() override {}
  bool BuildSolution() override;

 private:
  class PartialRoutesAndLargeVehicleIndicesFirst {
   public:
    explicit PartialRoutesAndLargeVehicleIndicesFirst(
        const CheapestAdditionFilteredDecisionBuilder& builder)
        : builder_(builder) {}
    bool operator()(int vehicle1, int vehicle2) const;

   private:
    const CheapestAdditionFilteredDecisionBuilder& builder_;
  };
  // Returns a sorted vector of nodes which can come next in the route after
  // node 'from'.
  // 'from' is a variable index corresponding to a node, 'sorted_nexts' is a
  // vector of variable indices corresponding to nodes which can follow 'from'.
  virtual void SortPossibleNexts(int64 from,
                                 std::vector<int64>* sorted_nexts) = 0;
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
  ~EvaluatorCheapestAdditionFilteredDecisionBuilder() override {}

 private:
  // Next nodes are sorted according to the current evaluator.
  void SortPossibleNexts(int64 from, std::vector<int64>* sorted_nexts) override;

  std::unique_ptr<ResultCallback2<int64, int64, int64> > evaluator_;
};

// A CheapestAdditionFilteredDecisionBuilder where the notion of 'cheapest arc'
// comes from an arc comparator.
class ComparatorCheapestAdditionFilteredDecisionBuilder
    : public CheapestAdditionFilteredDecisionBuilder {
 public:
  // Takes ownership of evaluator.
  ComparatorCheapestAdditionFilteredDecisionBuilder(
      RoutingModel* model, Solver::VariableValueComparator comparator,
      const std::vector<LocalSearchFilter*>& filters);
  ~ComparatorCheapestAdditionFilteredDecisionBuilder() override {}

 private:
  // Next nodes are sorted according to the current comparator.
  void SortPossibleNexts(int64 from, std::vector<int64>* sorted_nexts) override;

  Solver::VariableValueComparator comparator_;
};

// Filter-based decision builder which builds a solution by using
// Clarke & Wright's Savings heuristic. For each pair of nodes, the savings
// value is the difference between the cost of two routes visiting one node each
// and one route visiting both nodes. Routes are built sequentially, each route
// being initialized from the pair with the best avalaible savings value then
// extended by selecting the nodes with best savings on both ends of the partial
// route.
// Cost is based on the arc cost function of the routing model and cost classes
// are taken into account.
class SavingsFilteredDecisionBuilder : public RoutingFilteredDecisionBuilder {
 public:
  // If savings_neighbors_ratio > 0 then for each node only this ratio of its
  // neighbors leading to the smallest arc costs are considered.
  // Furthermore, if add_reverse_arcs is true, the neighborhood relationships
  // are always considered symmetrically.
  SavingsFilteredDecisionBuilder(
      RoutingModel* model, double savings_neighbors_ratio,
      bool add_reverse_arcs, const std::vector<LocalSearchFilter*>& filters);
  ~SavingsFilteredDecisionBuilder() override {}
  bool BuildSolution() override;

 private:
  typedef std::pair</*saving*/ int64, /*saving index*/ int64> Saving;

  // Computes saving values for all node pairs and vehicle types (see
  // ComputeVehicleTypes()).
  // The saving index attached to each saving value is an index used to
  // store and recover the node pair to which the value is linked (cf. the
  // index conversion methods below).
  std::vector<Saving> ComputeSavings();
  // Builds a saving from a saving value, a cost class and two nodes.
  Saving BuildSaving(int64 saving, int vehicle_type, int before_node,
                     int after_node) const {
    return std::make_pair(saving, vehicle_type * size_squared_ +
                                      before_node * Size() + after_node);
  }
  // Returns the cost class from a saving.
  int64 GetVehicleTypeFromSaving(const Saving& saving) const {
    return saving.second / size_squared_;
  }
  // Returns the "before node" from a saving.
  int64 GetBeforeNodeFromSaving(const Saving& saving) const {
    return (saving.second % size_squared_) / Size();
  }
  // Returns the "after node" from a saving.
  int64 GetAfterNodeFromSaving(const Saving& saving) const {
    return (saving.second % size_squared_) % Size();
  }
  // Returns the saving value from a saving.
  int64 GetSavingValue(const Saving& saving) const { return saving.first; }

  // Computes the vehicle type of every vehicle and stores it in
  // type_index_of_vehicle_. A "vehicle type" consists of the set of vehicles
  // having the same cost class and start/end nodes, therefore the same savings
  // value for each arc.
  // The vehicles corresponding to each vehicle type index are stored in
  // vehicles_per_vehicle_type_.
  void ComputeVehicleTypes();

  const double savings_neighbors_ratio_;
  const bool add_reverse_arcs_;
  int64 size_squared_;
  std::vector<int> type_index_of_vehicle_;
  // clang-format off
  std::vector<std::vector<int> > vehicles_per_vehicle_type_;
  // clang-format on
};

// Christofides addition heuristic. Initially created to solve TSPs, extended to
// support any model by extending routes as much as possible following the path
// found by the heuristic, before starting a new route.

class ChristofidesFilteredDecisionBuilder
    : public RoutingFilteredDecisionBuilder {
 public:
  ChristofidesFilteredDecisionBuilder(
      RoutingModel* model, const std::vector<LocalSearchFilter*>& filters);
  ~ChristofidesFilteredDecisionBuilder() override {}
  bool BuildSolution() override;
};

// Routing filters

class RoutingLocalSearchFilter : public IntVarLocalSearchFilter {
 public:
  RoutingLocalSearchFilter(const std::vector<IntVar*>& nexts,
                           std::function<void(int64)> objective_callback);
  ~RoutingLocalSearchFilter() override {}
  virtual void InjectObjectiveValue(int64 objective_value);

 protected:
  bool CanPropagateObjectiveValue() const {
    return objective_callback_ != nullptr;
  }
  void PropagateObjectiveValue(int64 objective_value);

  int64 injected_objective_value_;

 private:
  std::function<void(int64)> objective_callback_;
};

// Generic path-based filter class.

class BasePathFilter : public RoutingLocalSearchFilter {
 public:
  BasePathFilter(const std::vector<IntVar*>& nexts, int next_domain_size,
                 std::function<void(int64)> objective_callback);
  ~BasePathFilter() override {}
  bool Accept(const Assignment* delta, const Assignment* deltadelta) override;
  void OnSynchronize(const Assignment* delta) override;

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

#if !defined(SWIG)
RoutingLocalSearchFilter* MakeNodeDisjunctionFilter(
    const RoutingModel& routing_model,
    std::function<void(int64)> objective_callback);
RoutingLocalSearchFilter* MakePathCumulFilter(
    const RoutingModel& routing_model, const RoutingDimension& dimension,
    std::function<void(int64)> objective_callback);
RoutingLocalSearchFilter* MakeNodePrecedenceFilter(
    const RoutingModel& routing_model, const RoutingModel::NodePairs& pairs);
RoutingLocalSearchFilter* MakeVehicleVarFilter(
    const RoutingModel& routing_model);
#endif
}  // namespace operations_research
#endif  // OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_H_
