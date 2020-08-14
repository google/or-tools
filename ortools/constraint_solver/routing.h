// Copyright 2010-2018 Google LLC
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

/// The vehicle routing library lets one model and solve generic vehicle routing
/// problems ranging from the Traveling Salesman Problem to more complex
/// problems such as the Capacitated Vehicle Routing Problem with Time Windows.
///
/// The objective of a vehicle routing problem is to build routes covering a set
/// of nodes minimizing the overall cost of the routes (usually proportional to
/// the sum of the lengths of each segment of the routes) while respecting some
/// problem-specific constraints (such as the length of a route). A route is
/// equivalent to a path connecting nodes, starting/ending at specific
/// starting/ending nodes.
///
/// The term "vehicle routing" is historical and the category of problems solved
/// is not limited to the routing of vehicles: any problem involving finding
/// routes visiting a given number of nodes optimally falls under this category
/// of problems, such as finding the optimal sequence in a playlist.
/// The literature around vehicle routing problems is extremely dense but one
/// can find some basic introductions in the following links:
/// - http://en.wikipedia.org/wiki/Travelling_salesman_problem
/// - http://www.tsp.gatech.edu/history/index.html
/// - http://en.wikipedia.org/wiki/Vehicle_routing_problem
///
/// The vehicle routing library is a vertical layer above the constraint
/// programming library (ortools/constraint_programming:cp).
/// One has access to all underlying constrained variables of the vehicle
/// routing model which can therefore be enriched by adding any constraint
/// available in the constraint programming library.
///
/// There are two sets of variables available:
/// - path variables:
///   * "next(i)" variables representing the immediate successor of the node
///     corresponding to i; use IndexToNode() to get the node corresponding to
///     a "next" variable value; note that node indices are strongly typed
///     integers (cf. ortools/base/int_type.h);
///   * "vehicle(i)" variables representing the vehicle route to which the
///     node corresponding to i belongs;
///   * "active(i)" boolean variables, true if the node corresponding to i is
///     visited and false if not; this can be false when nodes are either
///     optional or part of a disjunction;
///   * The following relationships hold for all i:
///      active(i) == 0 <=> next(i) == i <=> vehicle(i) == -1,
///      next(i) == j => vehicle(j) == vehicle(i).
/// - dimension variables, used when one is accumulating quantities along
///   routes, such as weight or volume carried, distance or time:
///   * "cumul(i,d)" variables representing the quantity of dimension d when
///     arriving at the node corresponding to i;
///   * "transit(i,d)" variables representing the quantity of dimension d added
///     after visiting the node corresponding to i.
///   * The following relationship holds for all (i,d):
///       next(i) == j => cumul(j,d) == cumul(i,d) + transit(i,d).
/// Solving the vehicle routing problems is mainly done using approximate
/// methods (namely local search,
/// cf. http://en.wikipedia.org/wiki/Local_search_(optimization) ), potentially
/// combined with exact techniques based on dynamic programming and exhaustive
/// tree search.
// TODO(user): Add a section on costs (vehicle arc costs, span costs,
//                disjunctions costs).
//
/// Advanced tips: Flags are available to tune the search used to solve routing
/// problems. Here is a quick overview of the ones one might want to modify:
/// - Limiting the search for solutions:
///   * routing_solution_limit (default: kint64max): stop the search after
///     finding 'routing_solution_limit' improving solutions;
///   * routing_time_limit (default: kint64max): stop the search after
///     'routing_time_limit' milliseconds;
/// - Customizing search:
///   * routing_first_solution (default: select the first node with an unbound
///     successor and connect it to the first available node): selects the
///     heuristic to build a first solution which will then be improved by local
///     search; possible values are GlobalCheapestArc (iteratively connect two
///     nodes which produce the cheapest route segment), LocalCheapestArc
///     (select the first node with an unbound successor and connect it to the
///     node which produces the cheapest route segment), PathCheapestArc
///     (starting from a route "start" node, connect it to the node which
///     produces the cheapest route segment, then extend the route by iterating
///     on the last node added to the route).
///   * Local search neighborhoods:
///     - routing_no_lns (default: false): forbids the use of Large Neighborhood
///       Search (LNS); LNS can find good solutions but is usually very slow.
///       Refer to the description of PATHLNS in the LocalSearchOperators enum
///       in constraint_solver.h for more information.
///     - routing_no_tsp (default: true): forbids the use of exact methods to
///       solve "sub"-traveling salesman problems (TSPs) of the current model
///       (such as sub-parts of a route, or one route in a multiple route
///       problem). Uses dynamic programming to solve such TSPs with a maximum
///       size (in number of nodes) up to cp_local_search_tsp_opt_size (flag
///       with a default value of 13 nodes). It is not activated by default
///       because it can slow down the search.
///   * Meta-heuristics: used to guide the search out of local minima found by
///     local search. Note that, in general, a search with metaheuristics
///     activated never stops, therefore one must specify a search limit.
///     Several types of metaheuristics are provided:
///     - routing_guided_local_search (default: false): activates guided local
///       search (cf. http://en.wikipedia.org/wiki/Guided_Local_Search);
///       this is generally the most efficient metaheuristic for vehicle
///       routing;
///     - routing_simulated_annealing (default: false): activates simulated
///       annealing (cf. http://en.wikipedia.org/wiki/Simulated_annealing);
///     - routing_tabu_search (default: false): activates tabu search (cf.
///       http://en.wikipedia.org/wiki/Tabu_search).
///
/// Code sample:
/// Here is a simple example solving a traveling salesman problem given a cost
/// function callback (returns the cost of a route segment):
///
/// - Define a custom distance/cost function from an index to another; in this
///   example just returns the sum of the indices:
///
///     int64 MyDistance(int64 from, int64 to) {
///       return from + to;
///     }
///
/// - Create a routing model for a given problem size (int number of nodes) and
///   number of routes (here, 1):
///
///     RoutingIndexManager manager(...number of nodes..., 1);
///     RoutingModel routing(manager);
///
/// - Set the cost function by registering an std::function<int64(int64, int64)>
/// in the model and passing its index as the vehicle cost.
///
///    const int cost = routing.RegisterTransitCallback(MyDistance);
///    routing.SetArcCostEvaluatorOfAllVehicles(cost);
///
/// - Find a solution using Solve(), returns a solution if any (owned by
///   routing):
///
///    const Assignment* solution = routing.Solve();
///    CHECK(solution != nullptr);
///
/// - Inspect the solution cost and route (only one route here):
///
///    LOG(INFO) << "Cost " << solution->ObjectiveValue();
///    const int route_number = 0;
///    for (int64 node = routing.Start(route_number);
///         !routing.IsEnd(node);
///         node = solution->Value(routing.NextVar(node))) {
///      LOG(INFO) << manager.IndexToNode(node);
///    }
///
///
/// Keywords: Vehicle Routing, Traveling Salesman Problem, TSP, VRP, CVRPTW,
/// PDP.

#ifndef OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_H_
#define OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_H_

#include <cstddef>
#include <functional>
#include <memory>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/hash/hash.h"
#include "absl/time/time.h"
#include "ortools/base/adjustable_priority_queue-inl.h"
#include "ortools/base/adjustable_priority_queue.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/hash.h"
#include "ortools/base/int_type_indexed_vector.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "ortools/constraint_solver/routing_enums.pb.h"
#include "ortools/constraint_solver/routing_index_manager.h"
#include "ortools/constraint_solver/routing_parameters.pb.h"
#include "ortools/constraint_solver/routing_types.h"
#include "ortools/glop/lp_solver.h"
#include "ortools/glop/parameters.pb.h"
#include "ortools/graph/graph.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/sat/theta_tree.h"
#include "ortools/util/range_query_function.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {

class GlobalDimensionCumulOptimizer;
class LocalDimensionCumulOptimizer;
class LocalSearchOperator;
#ifndef SWIG
class IntVarFilteredDecisionBuilder;
class IntVarFilteredHeuristic;
class IndexNeighborFinder;
#endif
class RoutingDimension;
#ifndef SWIG
using util::ReverseArcListGraph;
class SweepArranger;
#endif
struct SweepIndex;

class RoutingModel {
 public:
  /// Status of the search.
  enum Status {
    /// Problem not solved yet (before calling RoutingModel::Solve()).
    ROUTING_NOT_SOLVED,
    /// Problem solved successfully after calling RoutingModel::Solve().
    ROUTING_SUCCESS,
    /// No solution found to the problem after calling RoutingModel::Solve().
    ROUTING_FAIL,
    /// Time limit reached before finding a solution with RoutingModel::Solve().
    ROUTING_FAIL_TIMEOUT,
    /// Model, model parameters or flags are not valid.
    ROUTING_INVALID
  };

  /// Types of precedence policy applied to pickup and delivery pairs.
  enum PickupAndDeliveryPolicy {
    /// Any precedence is accepted.
    PICKUP_AND_DELIVERY_NO_ORDER,
    /// Deliveries must be performed in reverse order of pickups.
    PICKUP_AND_DELIVERY_LIFO,
    /// Deliveries must be performed in the same order as pickups.
    PICKUP_AND_DELIVERY_FIFO
  };
  typedef RoutingCostClassIndex CostClassIndex;
  typedef RoutingDimensionIndex DimensionIndex;
  typedef RoutingDisjunctionIndex DisjunctionIndex;
  typedef RoutingVehicleClassIndex VehicleClassIndex;
  typedef RoutingTransitCallback1 TransitCallback1;
  typedef RoutingTransitCallback2 TransitCallback2;

// TODO(user): Remove all SWIG guards by adding the @ignore in .i.
#if !defined(SWIG)
  typedef RoutingIndexPair IndexPair;
  typedef RoutingIndexPairs IndexPairs;
#endif  // SWIG

#if !defined(SWIG)
  /// What follows is relevant for models with time/state dependent transits.
  /// Such transits, say from node A to node B, are functions f: int64->int64
  /// of the cumuls of a dimension. The user is free to implement the abstract
  /// RangeIntToIntFunction interface, but it is expected that the
  /// implementation of each method is quite fast. For performance-related
  /// reasons, StateDependentTransit keeps an additional pointer to a
  /// RangeMinMaxIndexFunction, with similar functionality to
  /// RangeIntToIntFunction, for g(x) = f(x)+x, where f is the transit from A to
  /// B. In most situations the best solutions are problem-specific, but in case
  /// of doubt the user may use the MakeStateDependentTransit function from the
  /// routing library, which works out-of-the-box, with very good running time,
  /// but memory inefficient in some situations.
  struct StateDependentTransit {
    RangeIntToIntFunction* transit;                   /// f(x)
    RangeMinMaxIndexFunction* transit_plus_identity;  /// g(x) = f(x) + x
  };
  typedef std::function<StateDependentTransit(int64, int64)>
      VariableIndexEvaluator2;
#endif  // SWIG

#if !defined(SWIG)
  struct CostClass {
    /// Index of the arc cost evaluator, registered in the RoutingModel class.
    int evaluator_index = 0;

    /// SUBTLE:
    /// The vehicle's fixed cost is skipped on purpose here, because we
    /// can afford to do so:
    /// - We don't really care about creating "strict" equivalence classes;
    ///   all we care about is to:
    ///   1) compress the space of cost callbacks so that
    ///      we can cache them more efficiently.
    ///   2) have a smaller IntVar domain thanks to using a "cost class var"
    ///      instead of the vehicle var, so that we reduce the search space.
    ///   Both of these are an incentive for *fewer* cost classes. Ignoring
    ///   the fixed costs can only be good in that regard.
    /// - The fixed costs are only needed when evaluating the cost of the
    ///   first arc of the route, in which case we know the vehicle, since we
    ///   have the route's start node.

    /// Only dimensions that have non-zero cost evaluator and a non-zero cost
    /// coefficient (in this cost class) are listed here. Since we only need
    /// their transit evaluator (the raw version that takes var index, not Node
    /// Index) and their span cost coefficient, we just store those.
    /// This is sorted by the natural operator < (and *not* by DimensionIndex).
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

    explicit CostClass(int evaluator_index)
        : evaluator_index(evaluator_index) {}

    /// Comparator for STL containers and algorithms.
    static bool LessThan(const CostClass& a, const CostClass& b) {
      if (a.evaluator_index != b.evaluator_index) {
        return a.evaluator_index < b.evaluator_index;
      }
      return a.dimension_transit_evaluator_class_and_cost_coefficient <
             b.dimension_transit_evaluator_class_and_cost_coefficient;
    }
  };

  struct VehicleClass {
    /// The cost class of the vehicle.
    CostClassIndex cost_class_index;
    /// Contrarily to CostClass, here we need strict equivalence.
    int64 fixed_cost;
    /// Vehicle start and end equivalence classes. Currently if two vehicles
    /// have different start/end nodes which are "physically" located at the
    /// same place, these two vehicles will be considered as non-equivalent
    /// unless the two indices are in the same class.
    // TODO(user): Find equivalent start/end nodes wrt dimensions and
    // callbacks.
    int start_equivalence_class;
    int end_equivalence_class;
    /// Bounds of cumul variables at start and end vehicle nodes.
    /// dimension_{start,end}_cumuls_{min,max}[d] is the bound for dimension d.
    gtl::ITIVector<DimensionIndex, int64> dimension_start_cumuls_min;
    gtl::ITIVector<DimensionIndex, int64> dimension_start_cumuls_max;
    gtl::ITIVector<DimensionIndex, int64> dimension_end_cumuls_min;
    gtl::ITIVector<DimensionIndex, int64> dimension_end_cumuls_max;
    gtl::ITIVector<DimensionIndex, int64> dimension_capacities;
    /// dimension_evaluators[d]->Run(from, to) is the transit value of arc
    /// from->to for a dimension d.
    gtl::ITIVector<DimensionIndex, int64> dimension_evaluator_classes;
    /// Fingerprint of unvisitable non-start/end nodes.
    uint64 unvisitable_nodes_fprint;

    /// Comparator for STL containers and algorithms.
    static bool LessThan(const VehicleClass& a, const VehicleClass& b);
  };
#endif  // defined(SWIG)

  /// Struct used to sort and store vehicles by their type. Two vehicles have
  /// the same "vehicle type" iff they have the same cost class and start/end
  /// nodes.
  struct VehicleTypeContainer {
    struct VehicleClassEntry {
      int vehicle_class;
      int64 fixed_cost;

      bool operator<(const VehicleClassEntry& other) const {
        return std::tie(fixed_cost, vehicle_class) <
               std::tie(other.fixed_cost, other.vehicle_class);
      }
    };

    int NumTypes() const { return sorted_vehicle_classes_per_type.size(); }

    int Type(int vehicle) const {
      DCHECK_LT(vehicle, type_index_of_vehicle.size());
      return type_index_of_vehicle[vehicle];
    }

    std::vector<int> type_index_of_vehicle;
    // clang-format off
    std::vector<std::set<VehicleClassEntry> > sorted_vehicle_classes_per_type;
    std::vector<std::deque<int> > vehicles_per_vehicle_class;
    // clang-format on
  };

  /// Constant used to express a hard constraint instead of a soft penalty.
  static const int64 kNoPenalty;

  /// Constant used to express the "no disjunction" index, returned when a node
  /// does not appear in any disjunction.
  static const DisjunctionIndex kNoDisjunction;

  /// Constant used to express the "no dimension" index, returned when a
  /// dimension name does not correspond to an actual dimension.
  static const DimensionIndex kNoDimension;

  /// Constructor taking an index manager. The version which does not take
  /// RoutingModelParameters is equivalent to passing
  /// DefaultRoutingModelParameters().
  explicit RoutingModel(const RoutingIndexManager& index_manager);
  RoutingModel(const RoutingIndexManager& index_manager,
               const RoutingModelParameters& parameters);
  ~RoutingModel();

  /// Registers 'callback' and returns its index.
  int RegisterUnaryTransitCallback(TransitCallback1 callback);
  int RegisterPositiveUnaryTransitCallback(TransitCallback1 callback);
  int RegisterTransitCallback(TransitCallback2 callback);
  int RegisterPositiveTransitCallback(TransitCallback2 callback);
  int RegisterStateDependentTransitCallback(VariableIndexEvaluator2 callback);
  const TransitCallback2& TransitCallback(int callback_index) const {
    CHECK_LT(callback_index, transit_evaluators_.size());
    return transit_evaluators_[callback_index];
  }
  const TransitCallback1& UnaryTransitCallbackOrNull(int callback_index) const {
    CHECK_LT(callback_index, unary_transit_evaluators_.size());
    return unary_transit_evaluators_[callback_index];
  }
  const VariableIndexEvaluator2& StateDependentTransitCallback(
      int callback_index) const {
    CHECK_LT(callback_index, state_dependent_transit_evaluators_.size());
    return state_dependent_transit_evaluators_[callback_index];
  }

  /// Model creation

  /// Methods to add dimensions to routes; dimensions represent quantities
  /// accumulated at nodes along the routes. They represent quantities such as
  /// weights or volumes carried along the route, or distance or times.
  /// Quantities at a node are represented by "cumul" variables and the increase
  /// or decrease of quantities between nodes are represented by "transit"
  /// variables. These variables are linked as follows:
  /// if j == next(i), cumul(j) = cumul(i) + transit(i) + slack(i)
  /// where slack is a positive slack variable (can represent waiting times for
  /// a time dimension).
  /// Setting the value of fix_start_cumul_to_zero to true will force the
  /// "cumul" variable of the start node of all vehicles to be equal to 0.

  /// Creates a dimension where the transit variable is constrained to be
  /// equal to evaluator(i, next(i)); 'slack_max' is the upper bound of the
  /// slack variable and 'capacity' is the upper bound of the cumul variables.
  /// 'name' is the name used to reference the dimension; this name is used to
  /// get cumul and transit variables from the routing model.
  /// Returns false if a dimension with the same name has already been created
  /// (and doesn't create the new dimension).
  /// Takes ownership of the callback 'evaluator'.
  bool AddDimension(int evaluator_index, int64 slack_max, int64 capacity,
                    bool fix_start_cumul_to_zero, const std::string& name);
  bool AddDimensionWithVehicleTransits(
      const std::vector<int>& evaluator_indices, int64 slack_max,
      int64 capacity, bool fix_start_cumul_to_zero, const std::string& name);
  bool AddDimensionWithVehicleCapacity(int evaluator_index, int64 slack_max,
                                       std::vector<int64> vehicle_capacities,
                                       bool fix_start_cumul_to_zero,
                                       const std::string& name);
  bool AddDimensionWithVehicleTransitAndCapacity(
      const std::vector<int>& evaluator_indices, int64 slack_max,
      std::vector<int64> vehicle_capacities, bool fix_start_cumul_to_zero,
      const std::string& name);
  /// Creates a dimension where the transit variable is constrained to be
  /// equal to 'value'; 'capacity' is the upper bound of the cumul variables.
  /// 'name' is the name used to reference the dimension; this name is used to
  /// get cumul and transit variables from the routing model.
  /// Returns false if a dimension with the same name has already been created
  /// (and doesn't create the new dimension).
  bool AddConstantDimensionWithSlack(int64 value, int64 capacity,
                                     int64 slack_max,
                                     bool fix_start_cumul_to_zero,
                                     const std::string& name);
  bool AddConstantDimension(int64 value, int64 capacity,
                            bool fix_start_cumul_to_zero,
                            const std::string& name) {
    return AddConstantDimensionWithSlack(value, capacity, 0,
                                         fix_start_cumul_to_zero, name);
  }
  /// Creates a dimension where the transit variable is constrained to be
  /// equal to 'values[i]' for node i; 'capacity' is the upper bound of
  /// the cumul variables. 'name' is the name used to reference the dimension;
  /// this name is used to get cumul and transit variables from the routing
  /// model.
  /// Returns false if a dimension with the same name has already been created
  /// (and doesn't create the new dimension).
  bool AddVectorDimension(std::vector<int64> values, int64 capacity,
                          bool fix_start_cumul_to_zero,
                          const std::string& name);
  /// Creates a dimension where the transit variable is constrained to be
  /// equal to 'values[i][next(i)]' for node i; 'capacity' is the upper bound of
  /// the cumul variables. 'name' is the name used to reference the dimension;
  /// this name is used to get cumul and transit variables from the routing
  /// model.
  /// Returns false if a dimension with the same name has already been created
  /// (and doesn't create the new dimension).
  bool AddMatrixDimension(
      std::vector<std::vector<int64> /*needed_for_swig*/> values,
      int64 capacity, bool fix_start_cumul_to_zero, const std::string& name);
  /// Creates a dimension with transits depending on the cumuls of another
  /// dimension. 'pure_transits' are the per-vehicle fixed transits as above.
  /// 'dependent_transits' is a vector containing for each vehicle an index to a
  /// registered state dependent transit callback. 'base_dimension' indicates
  /// the dimension from which the cumul variable is taken. If 'base_dimension'
  /// is nullptr, then the newly created dimension is self-based.
  bool AddDimensionDependentDimensionWithVehicleCapacity(
      const std::vector<int>& pure_transits,
      const std::vector<int>& dependent_transits,
      const RoutingDimension* base_dimension, int64 slack_max,
      std::vector<int64> vehicle_capacities, bool fix_start_cumul_to_zero,
      const std::string& name) {
    return AddDimensionDependentDimensionWithVehicleCapacityInternal(
        pure_transits, dependent_transits, base_dimension, slack_max,
        std::move(vehicle_capacities), fix_start_cumul_to_zero, name);
  }

  /// As above, but pure_transits are taken to be zero evaluators.
  bool AddDimensionDependentDimensionWithVehicleCapacity(
      const std::vector<int>& transits, const RoutingDimension* base_dimension,
      int64 slack_max, std::vector<int64> vehicle_capacities,
      bool fix_start_cumul_to_zero, const std::string& name);
  /// Homogeneous versions of the functions above.
  bool AddDimensionDependentDimensionWithVehicleCapacity(
      int transit, const RoutingDimension* base_dimension, int64 slack_max,
      int64 vehicle_capacity, bool fix_start_cumul_to_zero,
      const std::string& name);
  bool AddDimensionDependentDimensionWithVehicleCapacity(
      int pure_transit, int dependent_transit,
      const RoutingDimension* base_dimension, int64 slack_max,
      int64 vehicle_capacity, bool fix_start_cumul_to_zero,
      const std::string& name);

  /// Creates a cached StateDependentTransit from an std::function.
  static RoutingModel::StateDependentTransit MakeStateDependentTransit(
      const std::function<int64(int64)>& f, int64 domain_start,
      int64 domain_end);

  /// For every vehicle of the routing model:
  /// - if total_slacks[vehicle] is not nullptr, constrains it to be the sum of
  ///   slacks on that vehicle, that is,
  ///   dimension->CumulVar(end) - dimension->CumulVar(start) -
  ///   sum_{node in path of vehicle} dimension->FixedTransitVar(node).
  /// - if spans[vehicle] is not nullptr, constrains it to be
  ///   dimension->CumulVar(end) - dimension->CumulVar(start)
  /// This does stronger propagation than a decomposition, and takes breaks into
  /// account.
  Constraint* MakePathSpansAndTotalSlacks(const RoutingDimension* dimension,
                                          std::vector<IntVar*> spans,
                                          std::vector<IntVar*> total_slacks);

  /// Outputs the names of all dimensions added to the routing engine.
  // TODO(user): rename.
  std::vector<std::string> GetAllDimensionNames() const;
  /// Returns all dimensions of the model.
  const std::vector<RoutingDimension*>& GetDimensions() const {
    return dimensions_.get();
  }
  /// Returns dimensions with soft or vehicle span costs.
  std::vector<RoutingDimension*> GetDimensionsWithSoftOrSpanCosts() const;
  // clang-format off
  /// Returns [global|local]_dimension_optimizers_, which are empty if the model
  /// has not been closed.
  const std::vector<std::unique_ptr<GlobalDimensionCumulOptimizer> >&
  GetGlobalDimensionCumulOptimizers() const {
    return global_dimension_optimizers_;
  }
  const std::vector<std::unique_ptr<LocalDimensionCumulOptimizer> >&
  GetLocalDimensionCumulOptimizers() const {
    return local_dimension_optimizers_;
  }
  const std::vector<std::unique_ptr<LocalDimensionCumulOptimizer> >&
  GetLocalDimensionCumulMPOptimizers() const {
    return local_dimension_mp_optimizers_;
  }
  // clang-format on

  /// Returns the global/local dimension cumul optimizer for a given dimension,
  /// or nullptr if there is none.
  GlobalDimensionCumulOptimizer* GetMutableGlobalCumulOptimizer(
      const RoutingDimension& dimension) const;
  LocalDimensionCumulOptimizer* GetMutableLocalCumulOptimizer(
      const RoutingDimension& dimension) const;
  LocalDimensionCumulOptimizer* GetMutableLocalCumulMPOptimizer(
      const RoutingDimension& dimension) const;

  /// Returns true if a dimension exists for a given dimension name.
  bool HasDimension(const std::string& dimension_name) const;
  /// Returns a dimension from its name. Dies if the dimension does not exist.
  const RoutingDimension& GetDimensionOrDie(
      const std::string& dimension_name) const;
  /// Returns a dimension from its name. Returns nullptr if the dimension does
  /// not exist.
  RoutingDimension* GetMutableDimension(
      const std::string& dimension_name) const;
  /// Set the given dimension as "primary constrained". As of August 2013, this
  /// is only used by ArcIsMoreConstrainedThanArc().
  /// "dimension" must be the name of an existing dimension, or be empty, in
  /// which case there will not be a primary dimension after this call.
  void SetPrimaryConstrainedDimension(const std::string& dimension_name) {
    DCHECK(dimension_name.empty() || HasDimension(dimension_name));
    primary_constrained_dimension_ = dimension_name;
  }
  /// Get the primary constrained dimension, or an empty string if it is unset.
  const std::string& GetPrimaryConstrainedDimension() const {
    return primary_constrained_dimension_;
  }
  /// Adds a disjunction constraint on the indices: exactly 'max_cardinality' of
  /// the indices are active. Start and end indices of any vehicle cannot be
  /// part of a disjunction.
  ///
  /// If a penalty is given, at most 'max_cardinality' of the indices can be
  /// active, and if less are active, 'penalty' is payed per inactive index.
  /// This is equivalent to adding the constraint:
  ///     p + Sum(i)active[i] == max_cardinality
  /// where p is an integer variable, and the following cost to the cost
  /// function:
  ///     p * penalty.
  /// 'penalty' must be positive to make the disjunction optional; a negative
  /// penalty will force 'max_cardinality' indices of the disjunction to be
  /// performed, and therefore p == 0.
  /// Note: passing a vector with a single index will model an optional index
  /// with a penalty cost if it is not visited.
  DisjunctionIndex AddDisjunction(const std::vector<int64>& indices,
                                  int64 penalty = kNoPenalty,
                                  int64 max_cardinality = 1);
  /// Returns the indices of the disjunctions to which an index belongs.
  const std::vector<DisjunctionIndex>& GetDisjunctionIndices(
      int64 index) const {
    return index_to_disjunctions_[index];
  }
  /// Calls f for each variable index of indices in the same disjunctions as the
  /// node corresponding to the variable index 'index'; only disjunctions of
  /// cardinality 'cardinality' are considered.
  template <typename F>
  void ForEachNodeInDisjunctionWithMaxCardinalityFromIndex(
      int64 index, int64 max_cardinality, F f) const {
    for (const DisjunctionIndex disjunction : GetDisjunctionIndices(index)) {
      if (disjunctions_[disjunction].value.max_cardinality == max_cardinality) {
        for (const int64 d_index : disjunctions_[disjunction].indices) {
          f(d_index);
        }
      }
    }
  }
#if !defined(SWIGPYTHON)
  /// Returns the variable indices of the nodes in the disjunction of index
  /// 'index'.
  const std::vector<int64>& GetDisjunctionIndices(
      DisjunctionIndex index) const {
    return disjunctions_[index].indices;
  }
#endif  // !defined(SWIGPYTHON)
  /// Returns the penalty of the node disjunction of index 'index'.
  int64 GetDisjunctionPenalty(DisjunctionIndex index) const {
    return disjunctions_[index].value.penalty;
  }
  /// Returns the maximum number of possible active nodes of the node
  /// disjunction of index 'index'.
  int64 GetDisjunctionMaxCardinality(DisjunctionIndex index) const {
    return disjunctions_[index].value.max_cardinality;
  }
  /// Returns the number of node disjunctions in the model.
  int GetNumberOfDisjunctions() const { return disjunctions_.size(); }
  /// Returns the list of all perfect binary disjunctions, as pairs of variable
  /// indices: a disjunction is "perfect" when its variables do not appear in
  /// any other disjunction. Each pair is sorted (lowest variable index first),
  /// and the output vector is also sorted (lowest pairs first).
  std::vector<std::pair<int64, int64>> GetPerfectBinaryDisjunctions() const;
  /// SPECIAL: Makes the solver ignore all the disjunctions whose active
  /// variables are all trivially zero (i.e. Max() == 0), by setting their
  /// max_cardinality to 0.
  /// This can be useful when using the BaseBinaryDisjunctionNeighborhood
  /// operators, in the context of arc-based routing.
  void IgnoreDisjunctionsAlreadyForcedToZero();

  /// Adds a soft contraint to force a set of variable indices to be on the same
  /// vehicle. If all nodes are not on the same vehicle, each extra vehicle used
  /// adds 'cost' to the cost function.
  void AddSoftSameVehicleConstraint(const std::vector<int64>& indices,
                                    int64 cost);

  /// Sets the vehicles which can visit a given node. If the node is in a
  /// disjunction, this will not prevent it from being unperformed.
  /// Specifying an empty vector of vehicles has no effect (all vehicles
  /// will be allowed to visit the node).
  void SetAllowedVehiclesForIndex(const std::vector<int>& vehicles,
                                  int64 index);

  /// Returns true if a vehicle is allowed to visit a given node.
  bool IsVehicleAllowedForIndex(int vehicle, int64 index) {
    return allowed_vehicles_[index].empty() ||
           allowed_vehicles_[index].find(vehicle) !=
               allowed_vehicles_[index].end();
  }

  /// Notifies that index1 and index2 form a pair of nodes which should belong
  /// to the same route. This methods helps the search find better solutions,
  /// especially in the local search phase.
  /// It should be called each time you have an equality constraint linking
  /// the vehicle variables of two node (including for instance pickup and
  /// delivery problems):
  ///     Solver* const solver = routing.solver();
  ///     int64 index1 = manager.NodeToIndex(node1);
  ///     int64 index2 = manager.NodeToIndex(node2);
  ///     solver->AddConstraint(solver->MakeEquality(
  ///         routing.VehicleVar(index1),
  ///         routing.VehicleVar(index2)));
  ///     routing.AddPickupAndDelivery(index1, index2);
  ///
  // TODO(user): Remove this when model introspection detects linked nodes.
  void AddPickupAndDelivery(int64 pickup, int64 delivery);
  /// Same as AddPickupAndDelivery but notifying that the performed node from
  /// the disjunction of index 'pickup_disjunction' is on the same route as the
  /// performed node from the disjunction of index 'delivery_disjunction'.
  void AddPickupAndDeliverySets(DisjunctionIndex pickup_disjunction,
                                DisjunctionIndex delivery_disjunction);
  // clang-format off
  /// Returns pairs for which the node is a pickup; the first element of each
  /// pair is the index in the pickup and delivery pairs list in which the
  /// pickup appears, the second element is its index in the pickups list.
  const std::vector<std::pair<int, int> >&
  GetPickupIndexPairs(int64 node_index) const;
  /// Same as above for deliveries.
  const std::vector<std::pair<int, int> >&
      GetDeliveryIndexPairs(int64 node_index) const;
  // clang-format on

  /// Sets the Pickup and delivery policy of all vehicles. It is equivalent to
  /// calling SetPickupAndDeliveryPolicyOfVehicle on all vehicles.
  void SetPickupAndDeliveryPolicyOfAllVehicles(PickupAndDeliveryPolicy policy);
  void SetPickupAndDeliveryPolicyOfVehicle(PickupAndDeliveryPolicy policy,
                                           int vehicle);
  PickupAndDeliveryPolicy GetPickupAndDeliveryPolicyOfVehicle(
      int vehicle) const;
  /// Returns the number of non-start/end nodes which do not appear in a
  /// pickup/delivery pair.

  int GetNumOfSingletonNodes() const;

#ifndef SWIG
  /// Returns pickup and delivery pairs currently in the model.
  const IndexPairs& GetPickupAndDeliveryPairs() const {
    return pickup_delivery_pairs_;
  }
  const std::vector<std::pair<DisjunctionIndex, DisjunctionIndex>>&
  GetPickupAndDeliveryDisjunctions() const {
    return pickup_delivery_disjunctions_;
  }
#endif  // SWIG
  /// Set the node visit types and incompatibilities/requirements between the
  /// types (see below).
  ///
  /// NOTE: Before adding any incompatibilities and/or requirements on types:
  ///       1) All corresponding node types must have been set.
  ///       2) CloseVisitTypes() must be called so all containers are resized
  ///          accordingly.
  ///
  /// The following enum is used to describe how a node with a given type 'T'
  /// impacts the number of types 'T' on the route when visited, and thus
  /// determines how temporal incompatibilities and requirements take effect.
  enum VisitTypePolicy {
    /// When visited, the number of types 'T' on the vehicle increases by one.
    TYPE_ADDED_TO_VEHICLE,
    /// When visited, one instance of type 'T' previously added to the route
    /// (TYPE_ADDED_TO_VEHICLE), if any, is removed from the vehicle.
    /// If the type was not previously added to the route or all added instances
    /// have already been removed, this visit has no effect on the types.
    ADDED_TYPE_REMOVED_FROM_VEHICLE,
    /// With the following policy, the visit enforces that type 'T' is
    /// considered on the route from its start until this node is visited.
    TYPE_ON_VEHICLE_UP_TO_VISIT,
    /// The visit doesn't have an impact on the number of types 'T' on the
    /// route, as it's (virtually) added and removed directly.
    /// This policy can be used for visits which are part of an incompatibility
    /// or requirement set without affecting the type count on the route.
    TYPE_SIMULTANEOUSLY_ADDED_AND_REMOVED
  };
  // TODO(user): Support multiple visit types per node?
  void SetVisitType(int64 index, int type, VisitTypePolicy type_policy);
  int GetVisitType(int64 index) const;
  const std::vector<int>& GetSingleNodesOfType(int type) const;
  const std::vector<int>& GetPairIndicesOfType(int type) const;
  VisitTypePolicy GetVisitTypePolicy(int64 index) const;
  /// This function should be called once all node visit types have been set and
  /// prior to adding any incompatibilities/requirements.
  // TODO(user): Reconsider the logic and potentially remove the need to
  /// "close" types.
  void CloseVisitTypes();
  int GetNumberOfVisitTypes() const { return num_visit_types_; }
  const std::vector<int>& GetTopologicallySortedVisitTypes() const {
    DCHECK(closed_);
    return topologically_sorted_visit_types_;
  }
  /// Incompatibilities:
  /// Two nodes with "hard" incompatible types cannot share the same route at
  /// all, while with a "temporal" incompatibility they can't be on the same
  /// route at the same time.
  void AddHardTypeIncompatibility(int type1, int type2);
  void AddTemporalTypeIncompatibility(int type1, int type2);
  /// Returns visit types incompatible with a given type.
  const absl::flat_hash_set<int>& GetHardTypeIncompatibilitiesOfType(
      int type) const;
  const absl::flat_hash_set<int>& GetTemporalTypeIncompatibilitiesOfType(
      int type) const;
  /// Returns true iff any hard (resp. temporal) type incompatibilities have
  /// been added to the model.
  bool HasHardTypeIncompatibilities() const {
    return has_hard_type_incompatibilities_;
  }
  bool HasTemporalTypeIncompatibilities() const {
    return has_temporal_type_incompatibilities_;
  }
  /// Requirements:
  /// NOTE: As of 2019-04, cycles in the requirement graph are not supported,
  /// and lead to the dependent nodes being skipped if possible (otherwise
  /// the model is considered infeasible).
  /// The following functions specify that "dependent_type" requires at least
  /// one of the types in "required_type_alternatives".
  ///
  /// For same-vehicle requirements, a node of dependent type type_D requires at
  /// least one node of type type_R among the required alternatives on the same
  /// route.
  void AddSameVehicleRequiredTypeAlternatives(
      int dependent_type, absl::flat_hash_set<int> required_type_alternatives);
  /// If type_D depends on type_R when adding type_D, any node_D of type_D and
  /// VisitTypePolicy TYPE_ADDED_TO_VEHICLE or
  /// TYPE_SIMULTANEOUSLY_ADDED_AND_REMOVED requires at least one type_R on its
  /// vehicle at the time node_D is visited.
  void AddRequiredTypeAlternativesWhenAddingType(
      int dependent_type, absl::flat_hash_set<int> required_type_alternatives);
  /// The following requirements apply when visiting dependent nodes that remove
  /// their type from the route, i.e. type_R must be on the vehicle when type_D
  /// of VisitTypePolicy ADDED_TYPE_REMOVED_FROM_VEHICLE,
  /// TYPE_ON_VEHICLE_UP_TO_VISIT or TYPE_SIMULTANEOUSLY_ADDED_AND_REMOVED is
  /// visited.
  void AddRequiredTypeAlternativesWhenRemovingType(
      int dependent_type, absl::flat_hash_set<int> required_type_alternatives);
  // clang-format off
  /// Returns the set of same-vehicle requirement alternatives for the given
  /// type.
  const std::vector<absl::flat_hash_set<int> >&
      GetSameVehicleRequiredTypeAlternativesOfType(int type) const;
  /// Returns the set of requirement alternatives when adding the given type.
  const std::vector<absl::flat_hash_set<int> >&
      GetRequiredTypeAlternativesWhenAddingType(int type) const;
  /// Returns the set of requirement alternatives when removing the given type.
  const std::vector<absl::flat_hash_set<int> >&
      GetRequiredTypeAlternativesWhenRemovingType(int type) const;
  // clang-format on
  /// Returns true iff any same-route (resp. temporal) type requirements have
  /// been added to the model.
  bool HasSameVehicleTypeRequirements() const {
    return has_same_vehicle_type_requirements_;
  }
  bool HasTemporalTypeRequirements() const {
    return has_temporal_type_requirements_;
  }

  /// Returns true iff the model has any incompatibilities or requirements set
  /// on node types.
  bool HasTypeRegulations() const {
    return HasTemporalTypeIncompatibilities() ||
           HasHardTypeIncompatibilities() || HasSameVehicleTypeRequirements() ||
           HasTemporalTypeRequirements();
  }

  /// Get the "unperformed" penalty of a node. This is only well defined if the
  /// node is only part of a single Disjunction involving only itself, and that
  /// disjunction has a penalty. In all other cases, including forced active
  /// nodes, this returns 0.
  int64 UnperformedPenalty(int64 var_index) const;
  /// Same as above except that it returns default_value instead of 0 when
  /// penalty is not well defined (default value is passed as first argument to
  /// simplify the usage of the method in a callback).
  int64 UnperformedPenaltyOrValue(int64 default_value, int64 var_index) const;
  /// Returns the variable index of the first starting or ending node of all
  /// routes. If all routes start  and end at the same node (single depot), this
  /// is the node returned.
  int64 GetDepot() const;

  /// Constrains the maximum number of active vehicles, aka the number of
  /// vehicles which do not have an empty route. For instance, this can be used
  /// to limit the number of routes in the case where there are fewer drivers
  /// than vehicles and that the fleet of vehicle is heterogeneous.
  void SetMaximumNumberOfActiveVehicles(int max_active_vehicles) {
    max_active_vehicles_ = max_active_vehicles;
  }
  /// Returns the maximum number of active vehicles.
  int GetMaximumNumberOfActiveVehicles() const { return max_active_vehicles_; }
  /// Sets the cost function of the model such that the cost of a segment of a
  /// route between node 'from' and 'to' is evaluator(from, to), whatever the
  /// route or vehicle performing the route.
  void SetArcCostEvaluatorOfAllVehicles(int evaluator_index);
  /// Sets the cost function for a given vehicle route.
  void SetArcCostEvaluatorOfVehicle(int evaluator_index, int vehicle);
  /// Sets the fixed cost of all vehicle routes. It is equivalent to calling
  /// SetFixedCostOfVehicle on all vehicle routes.
  void SetFixedCostOfAllVehicles(int64 cost);
  /// Sets the fixed cost of one vehicle route.
  void SetFixedCostOfVehicle(int64 cost, int vehicle);
  /// Returns the route fixed cost taken into account if the route of the
  /// vehicle is not empty, aka there's at least one node on the route other
  /// than the first and last nodes.
  int64 GetFixedCostOfVehicle(int vehicle) const;

  /// The following methods set the linear and quadratic cost factors of
  /// vehicles (must be positive values). The default value of these parameters
  /// is zero for all vehicles.
  ///
  /// When set, the cost_ of the model will contain terms aiming at reducing the
  /// number of vehicles used in the model, by adding the following to the
  /// objective for every vehicle v:
  /// INDICATOR(v used in the model) *
  ///   [linear_cost_factor_of_vehicle_[v]
  ///    - quadratic_cost_factor_of_vehicle_[v]*(square of length of route v)]
  /// i.e. for every used vehicle, we add the linear factor as fixed cost, and
  /// subtract the square of the route length multiplied by the quadratic
  /// factor. This second term aims at making the routes as dense as possible.
  ///
  /// Sets the linear and quadratic cost factor of all vehicles.
  void SetAmortizedCostFactorsOfAllVehicles(int64 linear_cost_factor,
                                            int64 quadratic_cost_factor);
  /// Sets the linear and quadratic cost factor of the given vehicle.
  void SetAmortizedCostFactorsOfVehicle(int64 linear_cost_factor,
                                        int64 quadratic_cost_factor,
                                        int vehicle);

  const std::vector<int64>& GetAmortizedLinearCostFactorOfVehicles() const {
    return linear_cost_factor_of_vehicle_;
  }
  const std::vector<int64>& GetAmortizedQuadraticCostFactorOfVehicles() const {
    return quadratic_cost_factor_of_vehicle_;
  }

  void ConsiderEmptyRouteCostsForVehicle(bool consider_costs, int vehicle) {
    DCHECK_LT(vehicle, vehicles_);
    consider_empty_route_costs_[vehicle] = consider_costs;
  }

  bool AreEmptyRouteCostsConsideredForVehicle(int vehicle) const {
    DCHECK_LT(vehicle, vehicles_);
    return consider_empty_route_costs_[vehicle];
  }

/// Gets/sets the evaluator used during the search. Only relevant when
/// RoutingSearchParameters.first_solution_strategy = EVALUATOR_STRATEGY.
#ifndef SWIG
  const Solver::IndexEvaluator2& first_solution_evaluator() const {
    return first_solution_evaluator_;
  }
#endif
  /// Takes ownership of evaluator.
  void SetFirstSolutionEvaluator(Solver::IndexEvaluator2 evaluator) {
    first_solution_evaluator_ = std::move(evaluator);
  }
  /// Adds a local search operator to the set of operators used to solve the
  /// vehicle routing problem.
  void AddLocalSearchOperator(LocalSearchOperator* ls_operator);
  /// Adds a search monitor to the search used to solve the routing model.
  void AddSearchMonitor(SearchMonitor* const monitor);
  /// Adds a callback called each time a solution is found during the search.
  /// This is a shortcut to creating a monitor to call the callback on
  /// AtSolution() and adding it with AddSearchMonitor.
  void AddAtSolutionCallback(std::function<void()> callback);
  /// Adds a variable to minimize in the solution finalizer. The solution
  /// finalizer is called each time a solution is found during the search and
  /// allows to instantiate secondary variables (such as dimension cumul
  /// variables).
  void AddVariableMinimizedByFinalizer(IntVar* var);
  /// Adds a variable to maximize in the solution finalizer (see above for
  /// information on the solution finalizer).
  void AddVariableMaximizedByFinalizer(IntVar* var);
  /// Adds a variable to minimize in the solution finalizer, with a weighted
  /// priority: the higher the more priority it has.
  void AddWeightedVariableMinimizedByFinalizer(IntVar* var, int64 cost);
  /// Add a variable to set the closest possible to the target value in the
  /// solution finalizer.
  void AddVariableTargetToFinalizer(IntVar* var, int64 target);
  /// Closes the current routing model; after this method is called, no
  /// modification to the model can be done, but RoutesToAssignment becomes
  /// available. Note that CloseModel() is automatically called by Solve() and
  /// other methods that produce solution.
  /// This is equivalent to calling
  /// CloseModelWithParameters(DefaultRoutingSearchParameters()).
  void CloseModel();
  /// Same as above taking search parameters (as of 10/2015 some the parameters
  /// have to be set when closing the model).
  void CloseModelWithParameters(
      const RoutingSearchParameters& search_parameters);
  /// Solves the current routing model; closes the current model.
  /// This is equivalent to calling
  /// SolveWithParameters(DefaultRoutingSearchParameters())
  /// or
  /// SolveFromAssignmentWithParameters(assignment,
  ///                                   DefaultRoutingSearchParameters()).
  const Assignment* Solve(const Assignment* assignment = nullptr);
  /// Solves the current routing model with the given parameters. If 'solutions'
  /// is specified, it will contain the k best solutions found during the search
  /// (from worst to best, including the one returned by this method), where k
  /// corresponds to the 'number_of_solutions_to_collect' in
  /// 'search_parameters'. Note that the Assignment returned by the method and
  /// the ones in solutions are owned by the underlying solver and should not be
  /// deleted.
  const Assignment* SolveWithParameters(
      const RoutingSearchParameters& search_parameters,
      std::vector<const Assignment*>* solutions = nullptr);
  const Assignment* SolveFromAssignmentWithParameters(
      const Assignment* assignment,
      const RoutingSearchParameters& search_parameters,
      std::vector<const Assignment*>* solutions = nullptr);
  /// Given a "source_model" and its "source_assignment", resets
  /// "target_assignment" with the IntVar variables (nexts_, and vehicle_vars_
  /// if costs aren't homogeneous across vehicles) of "this" model, with the
  /// values set according to those in "other_assignment".
  /// The objective_element of target_assignment is set to this->cost_.
  void SetAssignmentFromOtherModelAssignment(
      Assignment* target_assignment, const RoutingModel* source_model,
      const Assignment* source_assignment);
  /// Computes a lower bound to the routing problem solving a linear assignment
  /// problem. The routing model must be closed before calling this method.
  /// Note that problems with node disjunction constraints (including optional
  /// nodes) and non-homogenous costs are not supported (the method returns 0 in
  /// these cases).
  // TODO(user): Add support for non-homogeneous costs and disjunctions.
  int64 ComputeLowerBound();
  /// Returns the current status of the routing model.
  Status status() const { return status_; }
  /// Applies a lock chain to the next search. 'locks' represents an ordered
  /// vector of nodes representing a partial route which will be fixed during
  /// the next search; it will constrain next variables such that:
  /// next[locks[i]] == locks[i+1].
  ///
  /// Returns the next variable at the end of the locked chain; this variable is
  /// not locked. An assignment containing the locks can be obtained by calling
  /// PreAssignment().
  IntVar* ApplyLocks(const std::vector<int64>& locks);
  /// Applies lock chains to all vehicles to the next search, such that locks[p]
  /// is the lock chain for route p. Returns false if the locks do not contain
  /// valid routes; expects that the routes do not contain the depots,
  /// i.e. there are empty vectors in place of empty routes.
  /// If close_routes is set to true, adds the end nodes to the route of each
  /// vehicle and deactivates other nodes.
  /// An assignment containing the locks can be obtained by calling
  /// PreAssignment().
  bool ApplyLocksToAllVehicles(const std::vector<std::vector<int64>>& locks,
                               bool close_routes);
  /// Returns an assignment used to fix some of the variables of the problem.
  /// In practice, this assignment locks partial routes of the problem. This
  /// can be used in the context of locking the parts of the routes which have
  /// already been driven in online routing problems.
  const Assignment* const PreAssignment() const { return preassignment_; }
  Assignment* MutablePreAssignment() { return preassignment_; }
  /// Writes the current solution to a file containing an AssignmentProto.
  /// Returns false if the file cannot be opened or if there is no current
  /// solution.
  bool WriteAssignment(const std::string& file_name) const;
  /// Reads an assignment from a file and returns the current solution.
  /// Returns nullptr if the file cannot be opened or if the assignment is not
  /// valid.
  Assignment* ReadAssignment(const std::string& file_name);
  /// Restores an assignment as a solution in the routing model and returns the
  /// new solution. Returns nullptr if the assignment is not valid.
  Assignment* RestoreAssignment(const Assignment& solution);
  /// Restores the routes as the current solution. Returns nullptr if the
  /// solution cannot be restored (routes do not contain a valid solution). Note
  /// that calling this method will run the solver to assign values to the
  /// dimension variables; this may take considerable amount of time, especially
  /// when using dimensions with slack.
  Assignment* ReadAssignmentFromRoutes(
      const std::vector<std::vector<int64>>& routes,
      bool ignore_inactive_indices);
  /// Fills an assignment from a specification of the routes of the
  /// vehicles. The routes are specified as lists of variable indices that
  /// appear on the routes of the vehicles. The indices of the outer vector in
  /// 'routes' correspond to vehicles IDs, the inner vector contains the
  /// variable indices on the routes for the given vehicle. The inner vectors
  /// must not contain the start and end indices, as these are determined by the
  /// routing model.  Sets the value of NextVars in the assignment, adding the
  /// variables to the assignment if necessary. The method does not touch other
  /// variables in the assignment. The method can only be called after the model
  /// is closed.  With ignore_inactive_indices set to false, this method will
  /// fail (return nullptr) in case some of the route contain indices that are
  /// deactivated in the model; when set to true, these indices will be
  /// skipped.  Returns true if routes were successfully
  /// loaded. However, such assignment still might not be a valid
  /// solution to the routing problem due to more complex constraints;
  /// it is advisible to call solver()->CheckSolution() afterwards.
  bool RoutesToAssignment(const std::vector<std::vector<int64>>& routes,
                          bool ignore_inactive_indices, bool close_routes,
                          Assignment* const assignment) const;
  /// Converts the solution in the given assignment to routes for all vehicles.
  /// Expects that assignment contains a valid solution (i.e. routes for all
  /// vehicles end with an end index for that vehicle).
  void AssignmentToRoutes(const Assignment& assignment,
                          std::vector<std::vector<int64>>* const routes) const;
  /// Converts the solution in the given assignment to routes for all vehicles.
  /// If the returned vector is route_indices, route_indices[i][j] is the index
  /// for jth location visited on route i. Note that contrary to
  /// AssignmentToRoutes, the vectors do include start and end locations.
#ifndef SWIG
  std::vector<std::vector<int64>> GetRoutesFromAssignment(
      const Assignment& assignment);
#endif
  /// Returns a compacted version of the given assignment, in which all vehicles
  /// with id lower or equal to some N have non-empty routes, and all vehicles
  /// with id greater than N have empty routes. Does not take ownership of the
  /// returned object.
  /// If found, the cost of the compact assignment is the same as in the
  /// original assignment and it preserves the values of 'active' variables.
  /// Returns nullptr if a compact assignment was not found.
  /// This method only works in homogenous mode, and it only swaps equivalent
  /// vehicles (vehicles with the same start and end nodes). When creating the
  /// compact assignment, the empty plan is replaced by the route assigned to
  /// the compatible vehicle with the highest id. Note that with more complex
  /// constraints on vehicle variables, this method might fail even if a compact
  /// solution exists.
  /// This method changes the vehicle and dimension variables as necessary.
  /// While compacting the solution, only basic checks on vehicle variables are
  /// performed; if one of these checks fails no attempts to repair it are made
  /// (instead, the method returns nullptr).
  Assignment* CompactAssignment(const Assignment& assignment) const;
  /// Same as CompactAssignment() but also checks the validity of the final
  /// compact solution; if it is not valid, no attempts to repair it are made
  /// (instead, the method returns nullptr).
  Assignment* CompactAndCheckAssignment(const Assignment& assignment) const;
  /// Adds an extra variable to the vehicle routing assignment.
  void AddToAssignment(IntVar* const var);
  void AddIntervalToAssignment(IntervalVar* const interval);
  /// For every dimension in the model with an optimizer in
  /// local/global_dimension_optimizers_, this method tries to pack the cumul
  /// values of the dimension, such that:
  /// - The cumul costs (span costs, soft lower and upper bound costs, etc) are
  ///   minimized.
  /// - The cumuls of the ends of the routes are minimized for this given
  ///   minimal cumul cost.
  /// - Given these minimal end cumuls, the route start cumuls are maximized.
  /// Returns the assignment resulting from allocating these packed cumuls with
  /// the solver, and nullptr if these cumuls could not be set by the solver.
  const Assignment* PackCumulsOfOptimizerDimensionsFromAssignment(
      const Assignment* original_assignment, absl::Duration duration_limit);
#ifndef SWIG
  // TODO(user): Revisit if coordinates are added to the RoutingModel class.
  void SetSweepArranger(SweepArranger* sweep_arranger) {
    sweep_arranger_.reset(sweep_arranger);
  }
  /// Returns the sweep arranger to be used by routing heuristics.
  SweepArranger* sweep_arranger() const { return sweep_arranger_.get(); }
#endif
  /// Adds a custom local search filter to the list of filters used to speed up
  /// local search by pruning unfeasible variable assignments.
  /// Calling this method after the routing model has been closed (CloseModel()
  /// or Solve() has been called) has no effect.
  /// The routing model does not take ownership of the filter.
  void AddLocalSearchFilter(LocalSearchFilter* filter) {
    CHECK(filter != nullptr);
    if (closed_) {
      LOG(WARNING) << "Model is closed, filter addition will be ignored.";
    }
    extra_filters_.push_back(filter);
  }

  /// Model inspection.
  /// Returns the variable index of the starting node of a vehicle route.
  int64 Start(int vehicle) const { return starts_[vehicle]; }
  /// Returns the variable index of the ending node of a vehicle route.
  int64 End(int vehicle) const { return ends_[vehicle]; }
  /// Returns true if 'index' represents the first node of a route.
  bool IsStart(int64 index) const;
  /// Returns true if 'index' represents the last node of a route.
  bool IsEnd(int64 index) const { return index >= Size(); }
  /// Returns the vehicle of the given start/end index, and -1 if the given
  /// index is not a vehicle start/end.
  int VehicleIndex(int index) const { return index_to_vehicle_[index]; }
  /// Assignment inspection
  /// Returns the variable index of the node directly after the node
  /// corresponding to 'index' in 'assignment'.
  int64 Next(const Assignment& assignment, int64 index) const;
  /// Returns true if the route of 'vehicle' is non empty in 'assignment'.
  bool IsVehicleUsed(const Assignment& assignment, int vehicle) const;

#if !defined(SWIGPYTHON)
  /// Returns all next variables of the model, such that Nexts(i) is the next
  /// variable of the node corresponding to i.
  const std::vector<IntVar*>& Nexts() const { return nexts_; }
  /// Returns all vehicle variables of the model,  such that VehicleVars(i) is
  /// the vehicle variable of the node corresponding to i.
  const std::vector<IntVar*>& VehicleVars() const { return vehicle_vars_; }
#endif  /// !defined(SWIGPYTHON)
  /// Returns the next variable of the node corresponding to index. Note that
  /// NextVar(index) == index is equivalent to ActiveVar(index) == 0.
  IntVar* NextVar(int64 index) const { return nexts_[index]; }
  /// Returns the active variable of the node corresponding to index.
  IntVar* ActiveVar(int64 index) const { return active_[index]; }
  /// Returns the active variable of the vehicle. It will be equal to 1 iff the
  /// route of the vehicle is not empty, 0 otherwise.
  IntVar* ActiveVehicleVar(int vehicle) const {
    return vehicle_active_[vehicle];
  }
  /// Returns the variable specifying whether or not costs are considered for
  /// vehicle.
  IntVar* VehicleCostsConsideredVar(int vehicle) const {
    return vehicle_costs_considered_[vehicle];
  }
  /// Returns the vehicle variable of the node corresponding to index. Note that
  /// VehicleVar(index) == -1 is equivalent to ActiveVar(index) == 0.
  IntVar* VehicleVar(int64 index) const { return vehicle_vars_[index]; }
  /// Returns the global cost variable which is being minimized.
  IntVar* CostVar() const { return cost_; }

  /// Returns the cost of the transit arc between two nodes for a given vehicle.
  /// Input are variable indices of node. This returns 0 if vehicle < 0.
  int64 GetArcCostForVehicle(int64 from_index, int64 to_index,
                             int64 vehicle) const;
  /// Whether costs are homogeneous across all vehicles.
  bool CostsAreHomogeneousAcrossVehicles() const {
    return costs_are_homogeneous_across_vehicles_;
  }
  /// Returns the cost of the segment between two nodes supposing all vehicle
  /// costs are the same (returns the cost for the first vehicle otherwise).
  int64 GetHomogeneousCost(int64 from_index, int64 to_index) const {
    return GetArcCostForVehicle(from_index, to_index, /*vehicle=*/0);
  }
  /// Returns the cost of the arc in the context of the first solution strategy.
  /// This is typically a simplification of the actual cost; see the .cc.
  int64 GetArcCostForFirstSolution(int64 from_index, int64 to_index) const;
  /// Returns the cost of the segment between two nodes for a given cost
  /// class. Input are variable indices of nodes and the cost class.
  /// Unlike GetArcCostForVehicle(), if cost_class is kNoCost, then the
  /// returned cost won't necessarily be zero: only some of the components
  /// of the cost that depend on the cost class will be omited. See the code
  /// for details.
  int64 GetArcCostForClass(int64 from_index, int64 to_index,
                           int64 /*CostClassIndex*/ cost_class_index) const;
  /// Get the cost class index of the given vehicle.
  CostClassIndex GetCostClassIndexOfVehicle(int64 vehicle) const {
    DCHECK(closed_);
    return cost_class_index_of_vehicle_[vehicle];
  }
  /// Returns true iff the model contains a vehicle with the given
  /// cost_class_index.
  bool HasVehicleWithCostClassIndex(CostClassIndex cost_class_index) const {
    DCHECK(closed_);
    if (cost_class_index == kCostClassIndexOfZeroCost) {
      return has_vehicle_with_zero_cost_class_;
    }
    return cost_class_index < cost_classes_.size();
  }
  /// Returns the number of different cost classes in the model.
  int GetCostClassesCount() const { return cost_classes_.size(); }
  /// Ditto, minus the 'always zero', built-in cost class.
  int GetNonZeroCostClassesCount() const {
    return std::max(0, GetCostClassesCount() - 1);
  }
  VehicleClassIndex GetVehicleClassIndexOfVehicle(int64 vehicle) const {
    DCHECK(closed_);
    return vehicle_class_index_of_vehicle_[vehicle];
  }
  /// Returns the number of different vehicle classes in the model.
  int GetVehicleClassesCount() const { return vehicle_classes_.size(); }
  /// Returns variable indices of nodes constrained to be on the same route.
  const std::vector<int>& GetSameVehicleIndicesOfIndex(int node) const {
    DCHECK(closed_);
    return same_vehicle_groups_[same_vehicle_group_[node]];
  }

  const VehicleTypeContainer& GetVehicleTypeContainer() const {
    DCHECK(closed_);
    return vehicle_type_container_;
  }

  /// Returns whether the arc from->to1 is more constrained than from->to2,
  /// taking into account, in order:
  /// - whether the destination node isn't an end node
  /// - whether the destination node is mandatory
  /// - whether the destination node is bound to the same vehicle as the source
  /// - the "primary constrained" dimension (see SetPrimaryConstrainedDimension)
  /// It then breaks ties using, in order:
  /// - the arc cost (taking unperformed penalties into account)
  /// - the size of the vehicle vars of "to1" and "to2" (lowest size wins)
  /// - the value: the lowest value of the indices to1 and to2 wins.
  /// See the .cc for details.
  /// The more constrained arc is typically preferable when building a
  /// first solution. This method is intended to be used as a callback for the
  /// BestValueByComparisonSelector value selector.
  /// Args:
  ///   from: the variable index of the source node
  ///   to1: the variable index of the first candidate destination node.
  ///   to2: the variable index of the second candidate destination node.
  bool ArcIsMoreConstrainedThanArc(int64 from, int64 to1, int64 to2);
  /// Print some debugging information about an assignment, including the
  /// feasible intervals of the CumulVar for dimension "dimension_to_print"
  /// at each step of the routes.
  /// If "dimension_to_print" is omitted, all dimensions will be printed.
  std::string DebugOutputAssignment(
      const Assignment& solution_assignment,
      const std::string& dimension_to_print) const;
  /// Returns a vector cumul_bounds, for which cumul_bounds[i][j] is a pair
  /// containing the minimum and maximum of the CumulVar of the jth node on
  /// route i.
  /// - cumul_bounds[i][j].first is the minimum.
  /// - cumul_bounds[i][j].second is the maximum.
#ifndef SWIG
  std::vector<std::vector<std::pair<int64, int64>>> GetCumulBounds(
      const Assignment& solution_assignment, const RoutingDimension& dimension);
#endif
  /// Returns the underlying constraint solver. Can be used to add extra
  /// constraints and/or modify search algoithms.
  Solver* solver() const { return solver_.get(); }

  /// Returns true if the search limit has been crossed.
  bool CheckLimit() {
    DCHECK(limit_ != nullptr);
    return limit_->Check();
  }

  /// Returns the time left in the search limit.
  absl::Duration RemainingTime() const {
    DCHECK(limit_ != nullptr);
    return limit_->AbsoluteSolverDeadline() - solver_->Now();
  }

  /// Sizes and indices
  /// Returns the number of nodes in the model.
  int nodes() const { return nodes_; }
  /// Returns the number of vehicle routes in the model.
  int vehicles() const { return vehicles_; }
  /// Returns the number of next variables in the model.
  int64 Size() const { return nodes_ + vehicles_ - start_end_count_; }

  /// Returns statistics on first solution search, number of decisions sent to
  /// filters, number of decisions rejected by filters.
  int64 GetNumberOfDecisionsInFirstSolution(
      const RoutingSearchParameters& search_parameters) const;
  int64 GetNumberOfRejectsInFirstSolution(
      const RoutingSearchParameters& search_parameters) const;
  /// Returns the automatic first solution strategy selected.
  operations_research::FirstSolutionStrategy::Value
  GetAutomaticFirstSolutionStrategy() const {
    return automatic_first_solution_strategy_;
  }

  /// Returns true if a vehicle/node matching problem is detected.
  bool IsMatchingModel() const;

#ifndef SWIG
  /// Sets the callback returning the variable to use for the Tabu Search
  /// metaheuristic.
  using GetTabuVarsCallback =
      std::function<std::vector<operations_research::IntVar*>(RoutingModel*)>;

  void SetTabuVarsCallback(GetTabuVarsCallback tabu_var_callback);
#endif  // SWIG

  /// The next few members are in the public section only for testing purposes.
  // TODO(user): Find a way to test and restrict the access at the same time.
  ///
  /// MakeGuidedSlackFinalizer creates a DecisionBuilder for the slacks of a
  /// dimension using a callback to choose which values to start with.
  /// The finalizer works only when all next variables in the model have
  /// been fixed. It has the following two characteristics:
  /// 1. It follows the routes defined by the nexts variables when choosing a
  ///    variable to make a decision on.
  /// 2. When it comes to choose a value for the slack of node i, the decision
  ///    builder first calls the callback with argument i, and supposingly the
  ///    returned value is x it creates decisions slack[i] = x, slack[i] = x +
  ///    1, slack[i] = x - 1, slack[i] = x + 2, etc.
  DecisionBuilder* MakeGuidedSlackFinalizer(
      const RoutingDimension* dimension,
      std::function<int64(int64)> initializer);
#ifndef SWIG
  // TODO(user): MakeGreedyDescentLSOperator is too general for routing.h.
  /// Perhaps move it to constraint_solver.h.
  /// MakeGreedyDescentLSOperator creates a local search operator that tries to
  /// improve the initial assignment by moving a logarithmically decreasing step
  /// away in each possible dimension.
  static std::unique_ptr<LocalSearchOperator> MakeGreedyDescentLSOperator(
      std::vector<IntVar*> variables);
#endif  /// __SWIG__
  /// MakeSelfDependentDimensionFinalizer is a finalizer for the slacks of a
  /// self-dependent dimension. It makes an extensive use of the caches of the
  /// state dependent transits.
  /// In detail, MakeSelfDependentDimensionFinalizer returns a composition of a
  /// local search decision builder with a greedy descent operator for the cumul
  /// of the start of each route and a guided slack finalizer. Provided there
  /// are no time windows and the maximum slacks are large enough, once the
  /// cumul of the start of route is fixed, the guided finalizer can find
  /// optimal values of the slacks for the rest of the route in time
  /// proportional to the length of the route. Therefore the composed finalizer
  /// generally works in time O(log(t)*n*m), where t is the latest possible
  /// departute time, n is the number of nodes in the network and m is the
  /// number of vehicles.
  DecisionBuilder* MakeSelfDependentDimensionFinalizer(
      const RoutingDimension* dimension);

 private:
  /// Local search move operator usable in routing.
  enum RoutingLocalSearchOperator {
    RELOCATE = 0,
    RELOCATE_PAIR,
    LIGHT_RELOCATE_PAIR,
    RELOCATE_NEIGHBORS,
    EXCHANGE,
    EXCHANGE_PAIR,
    CROSS,
    CROSS_EXCHANGE,
    TWO_OPT,
    OR_OPT,
    GLOBAL_CHEAPEST_INSERTION_CLOSE_NODES_LNS,
    LOCAL_CHEAPEST_INSERTION_CLOSE_NODES_LNS,
    GLOBAL_CHEAPEST_INSERTION_PATH_LNS,
    LOCAL_CHEAPEST_INSERTION_PATH_LNS,
    GLOBAL_CHEAPEST_INSERTION_EXPENSIVE_CHAIN_LNS,
    LOCAL_CHEAPEST_INSERTION_EXPENSIVE_CHAIN_LNS,
    RELOCATE_EXPENSIVE_CHAIN,
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
    EXCHANGE_RELOCATE_PAIR,
    RELOCATE_SUBTRIP,
    EXCHANGE_SUBTRIP,
    LOCAL_SEARCH_OPERATOR_COUNTER
  };

  /// Structure storing a value for a set of variable indices. Is used to store
  /// data for index disjunctions (variable indices, max_cardinality and penalty
  /// when unperformed).
  template <typename T>
  struct ValuedNodes {
    std::vector<int64> indices;
    T value;
  };
  struct DisjunctionValues {
    int64 penalty;
    int64 max_cardinality;
  };
  typedef ValuedNodes<DisjunctionValues> Disjunction;

  /// Storage of a cost cache element corresponding to a cost arc ending at
  /// node 'index' and on the cost class 'cost_class'.
  struct CostCacheElement {
    /// This is usually an int64, but using an int here decreases the RAM usage,
    /// and should be fine since in practice we never have more than 1<<31 vars.
    /// Note(user): on 2013-11, microbenchmarks on the arc costs callbacks
    /// also showed a 2% speed-up thanks to using int rather than int64.
    int index;
    CostClassIndex cost_class_index;
    int64 cost;
  };

  /// Internal methods.
  void Initialize();
  void AddNoCycleConstraintInternal();
  bool AddDimensionWithCapacityInternal(
      const std::vector<int>& evaluator_indices, int64 slack_max,
      std::vector<int64> vehicle_capacities, bool fix_start_cumul_to_zero,
      const std::string& name);
  bool AddDimensionDependentDimensionWithVehicleCapacityInternal(
      const std::vector<int>& pure_transits,
      const std::vector<int>& dependent_transits,
      const RoutingDimension* base_dimension, int64 slack_max,
      std::vector<int64> vehicle_capacities, bool fix_start_cumul_to_zero,
      const std::string& name);
  bool InitializeDimensionInternal(
      const std::vector<int>& evaluator_indices,
      const std::vector<int>& state_dependent_evaluator_indices,
      int64 slack_max, bool fix_start_cumul_to_zero,
      RoutingDimension* dimension);
  DimensionIndex GetDimensionIndex(const std::string& dimension_name) const;

  /// Creates global and local cumul optimizers for the dimensions needing them,
  /// and stores them in the corresponding [local|global]_dimension_optimizers_
  /// vectors.
  /// This function also computes and stores the "offsets" for these dimensions,
  /// used in the local/global optimizers to simplify LP computations.
  ///
  /// Note on the offsets computation:
  /// The global/local cumul offsets are used by the respective optimizers to
  /// have smaller numbers, and therefore better numerical behavior in the LP.
  /// These offsets are used as a minimum value for the cumuls over the route
  /// (or globally), i.e. a value we consider all cumuls to be greater or equal
  /// to. When transits are all positive, the cumuls of every node on a route is
  /// necessarily greater than the cumul of its start. Therefore, the local
  /// offset for a vehicle can be set to the minimum of its start node's cumul,
  /// and for the global optimizers, to the min start cumul over all vehicles.
  /// However, to be able to distinguish between infeasible nodes (i.e. nodes
  /// for which the cumul upper bound is less than the min cumul of the
  /// vehicle's start), we set the offset to "min_start_cumul" - 1. By doing so,
  /// all infeasible nodes described above will have bounds of [0, 0]. Example:
  /// Start cumul bounds: [11, 20] --> offset = 11 - 1 = 10.
  /// Two nodes with cumul bounds. Node1: [5, 10],  Node2: [7, 20]
  /// After applying the offset to the above windows, they become:
  /// Vehicle: [1, 10].     Node1: [0, 0] (infeasible).     Node2: [0, 10].
  ///
  /// On the other hand, when transits on a route can be negative, no assumption
  /// can be made on the cumuls of nodes wrt the start cumuls, and the offset is
  /// therefore set to 0.
  void StoreDimensionCumulOptimizers(const RoutingSearchParameters& parameters);

  void ComputeCostClasses(const RoutingSearchParameters& parameters);
  void ComputeVehicleClasses();
  /// The following method initializes the vehicle_type_container_:
  /// - Computes the vehicle types of vehicles and stores it in
  ///   type_index_of_vehicle.
  /// - The vehicle classes corresponding to each vehicle type index are stored
  ///   and sorted by fixed cost in sorted_vehicle_classes_per_type.
  /// - The vehicles for each vehicle class are stored in
  ///   vehicles_per_vehicle_class.
  void ComputeVehicleTypes();
  /// This method scans the visit types and sets up the following members:
  /// - single_nodes_of_type_[type] contains indices of nodes of visit type
  ///   "type" which are not part of any pickup/delivery pair.
  /// - pair_indices_of_type_[type] is the set of "pair_index" such that
  ///   pickup_delivery_pairs_[pair_index] has at least one pickup or delivery
  ///   with visit type "type".
  /// - topologically_sorted_visit_types_ contains the visit types in
  ///   topological order based on required-->dependent arcs from the
  ///   visit type requirements.
  void FinalizeVisitTypes();
  int64 GetArcCostForClassInternal(int64 from_index, int64 to_index,
                                   CostClassIndex cost_class_index) const;
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
  /// Returns nullptr if no penalty cost, otherwise returns penalty variable.
  IntVar* CreateDisjunction(DisjunctionIndex disjunction);
  /// Sets up pickup and delivery sets.
  void AddPickupAndDeliverySetsInternal(const std::vector<int64>& pickups,
                                        const std::vector<int64>& deliveries);
  /// Returns the cost variable related to the soft same vehicle constraint of
  /// index 'vehicle_index'.
  IntVar* CreateSameVehicleCost(int vehicle_index);
  /// Returns the first active variable index in 'indices' starting from index
  /// + 1.
  int FindNextActive(int index, const std::vector<int64>& indices) const;

  /// Checks that all nodes on the route starting at start_index (using the
  /// solution stored in assignment) can be visited by the given vehicle.
  bool RouteCanBeUsedByVehicle(const Assignment& assignment, int start_index,
                               int vehicle) const;
  /// Replaces the route of unused_vehicle with the route of active_vehicle in
  /// compact_assignment. Expects that unused_vehicle is a vehicle with an empty
  /// route and that the route of active_vehicle is non-empty. Also expects that
  /// 'assignment' contains the original assignment, from which
  /// compact_assignment was created.
  /// Returns true if the vehicles were successfully swapped; otherwise, returns
  /// false.
  bool ReplaceUnusedVehicle(int unused_vehicle, int active_vehicle,
                            Assignment* compact_assignment) const;

  void QuietCloseModel();
  void QuietCloseModelWithParameters(
      const RoutingSearchParameters& parameters) {
    if (!closed_) {
      CloseModelWithParameters(parameters);
    }
  }

  /// Solve matching problem with min-cost flow and store result in assignment.
  bool SolveMatchingModel(Assignment* assignment,
                          const RoutingSearchParameters& parameters);
#ifndef SWIG
  /// Append an assignment to a vector of assignments if it is feasible.
  bool AppendAssignmentIfFeasible(
      const Assignment& assignment,
      std::vector<std::unique_ptr<Assignment>>* assignments);
#endif
  /// Log a solution.
  void LogSolution(const RoutingSearchParameters& parameters,
                   const std::string& description, int64 solution_cost,
                   int64 start_time_ms);
  /// See CompactAssignment. Checks the final solution if
  /// check_compact_assignement is true.
  Assignment* CompactAssignmentInternal(const Assignment& assignment,
                                        bool check_compact_assignment) const;
  /// Checks that the current search parameters are valid for the current
  /// model's specific settings. This assumes that FindErrorInSearchParameters()
  /// from
  /// ./routing_flags.h caught no error.
  std::string FindErrorInSearchParametersForModel(
      const RoutingSearchParameters& search_parameters) const;
  /// Sets up search objects, such as decision builders and monitors.
  void SetupSearch(const RoutingSearchParameters& search_parameters);
  /// Set of auxiliary methods used to setup the search.
  // TODO(user): Document each auxiliary method.
  Assignment* GetOrCreateAssignment();
  Assignment* GetOrCreateTmpAssignment();
  RegularLimit* GetOrCreateLimit();
  RegularLimit* GetOrCreateLocalSearchLimit();
  RegularLimit* GetOrCreateLargeNeighborhoodSearchLimit();
  RegularLimit* GetOrCreateFirstSolutionLargeNeighborhoodSearchLimit();
  LocalSearchOperator* CreateInsertionOperator();
  LocalSearchOperator* CreateMakeInactiveOperator();
  void CreateNeighborhoodOperators(const RoutingSearchParameters& parameters);
  LocalSearchOperator* GetNeighborhoodOperators(
      const RoutingSearchParameters& search_parameters) const;
  std::vector<LocalSearchFilter*> GetOrCreateLocalSearchFilters(
      const RoutingSearchParameters& parameters);
  LocalSearchFilterManager* GetOrCreateLocalSearchFilterManager(
      const RoutingSearchParameters& parameters);
  std::vector<LocalSearchFilter*> GetOrCreateFeasibilityFilters(
      const RoutingSearchParameters& parameters);
  LocalSearchFilterManager* GetOrCreateFeasibilityFilterManager(
      const RoutingSearchParameters& parameters);
  LocalSearchFilterManager* GetOrCreateStrongFeasibilityFilterManager(
      const RoutingSearchParameters& parameters);
  DecisionBuilder* CreateSolutionFinalizer(SearchLimit* lns_limit);
  DecisionBuilder* CreateFinalizerForMinimizedAndMaximizedVariables();
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
  void SetupAssignmentCollector(
      const RoutingSearchParameters& search_parameters);
  void SetupTrace(const RoutingSearchParameters& search_parameters);
  void SetupSearchMonitors(const RoutingSearchParameters& search_parameters);
  bool UsesLightPropagation(
      const RoutingSearchParameters& search_parameters) const;
  GetTabuVarsCallback tabu_var_callback_;

  int GetVehicleStartClass(int64 start) const;

  void InitSameVehicleGroups(int number_of_groups) {
    same_vehicle_group_.assign(Size(), 0);
    same_vehicle_groups_.assign(number_of_groups, {});
  }
  void SetSameVehicleGroup(int index, int group) {
    same_vehicle_group_[index] = group;
    same_vehicle_groups_[group].push_back(index);
  }

  /// Model
  std::unique_ptr<Solver> solver_;
  int nodes_;
  int vehicles_;
  int max_active_vehicles_;
  Constraint* no_cycle_constraint_ = nullptr;
  /// Decision variables: indexed by int64 var index.
  std::vector<IntVar*> nexts_;
  std::vector<IntVar*> vehicle_vars_;
  std::vector<IntVar*> active_;
  // The following vectors are indexed by vehicle index.
  std::vector<IntVar*> vehicle_active_;
  std::vector<IntVar*> vehicle_costs_considered_;
  /// is_bound_to_end_[i] will be true iff the path starting at var #i is fully
  /// bound and reaches the end of a route, i.e. either:
  /// - IsEnd(i) is true
  /// - or nexts_[i] is bound and is_bound_to_end_[nexts_[i].Value()] is true.
  std::vector<IntVar*> is_bound_to_end_;
  mutable RevSwitch is_bound_to_end_ct_added_;
  /// Dimensions
  absl::flat_hash_map<std::string, DimensionIndex> dimension_name_to_index_;
  gtl::ITIVector<DimensionIndex, RoutingDimension*> dimensions_;
  // clang-format off
  /// TODO(user): Define a new Dimension[Global|Local]OptimizerIndex type
  /// and use it to define ITIVectors and for the dimension to optimizer index
  /// mappings below.
  std::vector<std::unique_ptr<GlobalDimensionCumulOptimizer> >
      global_dimension_optimizers_;
  gtl::ITIVector<DimensionIndex, int> global_optimizer_index_;
  std::vector<std::unique_ptr<LocalDimensionCumulOptimizer> >
      local_dimension_optimizers_;
  std::vector<std::unique_ptr<LocalDimensionCumulOptimizer> >
      local_dimension_mp_optimizers_;
  // clang-format off
  gtl::ITIVector<DimensionIndex, int> local_optimizer_index_;
  std::string primary_constrained_dimension_;
  /// Costs
  IntVar* cost_ = nullptr;
  std::vector<int> vehicle_to_transit_cost_;
  std::vector<int64> fixed_cost_of_vehicle_;
  std::vector<CostClassIndex> cost_class_index_of_vehicle_;
  bool has_vehicle_with_zero_cost_class_;
  std::vector<int64> linear_cost_factor_of_vehicle_;
  std::vector<int64> quadratic_cost_factor_of_vehicle_;
  bool vehicle_amortized_cost_factors_set_;
  /// consider_empty_route_costs_[vehicle] determines if "vehicle" should be
  /// taken into account for costs (arc costs, span costs, etc.) even when the
  /// route of the vehicle is empty (i.e. goes straight from its start to its
  /// end).
  ///
  /// NOTE1: A vehicle's fixed cost is added iff the vehicle serves nodes on its
  /// route, regardless of this variable's value.
  ///
  /// NOTE2: The default value for this boolean is 'false' for all vehicles,
  /// i.e. by default empty routes will not contribute to the cost.
  std::vector<bool> consider_empty_route_costs_;
#ifndef SWIG
  gtl::ITIVector<CostClassIndex, CostClass> cost_classes_;
#endif  // SWIG
  bool costs_are_homogeneous_across_vehicles_;
  bool cache_callbacks_;
  mutable std::vector<CostCacheElement> cost_cache_;  /// Index by source index.
  std::vector<VehicleClassIndex> vehicle_class_index_of_vehicle_;
#ifndef SWIG
  gtl::ITIVector<VehicleClassIndex, VehicleClass> vehicle_classes_;
#endif  // SWIG
  VehicleTypeContainer vehicle_type_container_;
  std::function<int(int64)> vehicle_start_class_callback_;
  /// Disjunctions
  gtl::ITIVector<DisjunctionIndex, Disjunction> disjunctions_;
  std::vector<std::vector<DisjunctionIndex> > index_to_disjunctions_;
  /// Same vehicle costs
  std::vector<ValuedNodes<int64> > same_vehicle_costs_;
  /// Allowed vehicles
#ifndef SWIG
  std::vector<absl::flat_hash_set<int>> allowed_vehicles_;
#endif  // SWIG
  /// Pickup and delivery
  IndexPairs pickup_delivery_pairs_;
  std::vector<std::pair<DisjunctionIndex, DisjunctionIndex> >
      pickup_delivery_disjunctions_;
  // clang-format off
  // If node_index is a pickup, index_to_pickup_index_pairs_[node_index] is the
  // vector of pairs {pair_index, pickup_index} such that
  // (pickup_delivery_pairs_[pair_index].first)[pickup_index] == node_index
  std::vector<std::vector<std::pair<int, int> > > index_to_pickup_index_pairs_;
  // Same as above for deliveries.
  std::vector<std::vector<std::pair<int, int> > >
      index_to_delivery_index_pairs_;
  // clang-format on
  std::vector<PickupAndDeliveryPolicy> vehicle_pickup_delivery_policy_;
  // Same vehicle group to which a node belongs.
  std::vector<int> same_vehicle_group_;
  // Same vehicle node groups.
  std::vector<std::vector<int>> same_vehicle_groups_;
  // Node visit types
  // Variable index to visit type index.
  std::vector<int> index_to_visit_type_;
  // Variable index to VisitTypePolicy.
  std::vector<VisitTypePolicy> index_to_type_policy_;
  // clang-format off
  std::vector<std::vector<int> > single_nodes_of_type_;
  std::vector<std::vector<int> > pair_indices_of_type_;

  std::vector<absl::flat_hash_set<int> >
      hard_incompatible_types_per_type_index_;
  bool has_hard_type_incompatibilities_;
  std::vector<absl::flat_hash_set<int> >
      temporal_incompatible_types_per_type_index_;
  bool has_temporal_type_incompatibilities_;

  std::vector<std::vector<absl::flat_hash_set<int> > >
      same_vehicle_required_type_alternatives_per_type_index_;
  bool has_same_vehicle_type_requirements_;
  std::vector<std::vector<absl::flat_hash_set<int> > >
      required_type_alternatives_when_adding_type_index_;
  std::vector<std::vector<absl::flat_hash_set<int> > >
      required_type_alternatives_when_removing_type_index_;
  bool has_temporal_type_requirements_;
  absl::flat_hash_map</*type*/int, absl::flat_hash_set<VisitTypePolicy> >
      trivially_infeasible_visit_types_to_policies_;
  // clang-format on

  // Visit types sorted topologically based on required-->dependent requirement
  // arcs between the types (if the requirement/dependency graph is acyclic).
  std::vector<int> topologically_sorted_visit_types_;
  int num_visit_types_;
  // Two indices are equivalent if they correspond to the same node (as given
  // to the constructors taking a RoutingIndexManager).
  std::vector<int> index_to_equivalence_class_;
  std::vector<int> index_to_vehicle_;
  std::vector<int64> starts_;
  std::vector<int64> ends_;
  // TODO(user): b/62478706 Once the port is done, this shouldn't be needed
  //                  anymore.
  RoutingIndexManager manager_;
  int start_end_count_;
  // Model status
  bool closed_ = false;
  Status status_ = ROUTING_NOT_SOLVED;
  bool enable_deep_serialization_ = true;

  // Search data
  std::vector<DecisionBuilder*> first_solution_decision_builders_;
  std::vector<IntVarFilteredDecisionBuilder*>
      first_solution_filtered_decision_builders_;
  Solver::IndexEvaluator2 first_solution_evaluator_;
  FirstSolutionStrategy::Value automatic_first_solution_strategy_ =
      FirstSolutionStrategy::UNSET;
  std::vector<LocalSearchOperator*> local_search_operators_;
  std::vector<SearchMonitor*> monitors_;
  SolutionCollector* collect_assignments_ = nullptr;
  SolutionCollector* collect_one_assignment_ = nullptr;
  SolutionCollector* packed_dimensions_assignment_collector_ = nullptr;
  DecisionBuilder* solve_db_ = nullptr;
  DecisionBuilder* improve_db_ = nullptr;
  DecisionBuilder* restore_assignment_ = nullptr;
  DecisionBuilder* restore_tmp_assignment_ = nullptr;
  Assignment* assignment_ = nullptr;
  Assignment* preassignment_ = nullptr;
  Assignment* tmp_assignment_ = nullptr;
  std::vector<IntVar*> extra_vars_;
  std::vector<IntervalVar*> extra_intervals_;
  std::vector<LocalSearchOperator*> extra_operators_;
  LocalSearchFilterManager* local_search_filter_manager_ = nullptr;
  LocalSearchFilterManager* feasibility_filter_manager_ = nullptr;
  LocalSearchFilterManager* strong_feasibility_filter_manager_ = nullptr;
  std::vector<LocalSearchFilter*> extra_filters_;
#ifndef SWIG
  std::vector<std::pair<IntVar*, int64>> finalizer_variable_cost_pairs_;
  std::vector<std::pair<IntVar*, int64>> finalizer_variable_target_pairs_;
  absl::flat_hash_map<IntVar*, int> finalizer_variable_cost_index_;
  absl::flat_hash_set<IntVar*> finalizer_variable_target_set_;
  std::unique_ptr<SweepArranger> sweep_arranger_;
#endif

  RegularLimit* limit_ = nullptr;
  RegularLimit* ls_limit_ = nullptr;
  RegularLimit* lns_limit_ = nullptr;
  RegularLimit* first_solution_lns_limit_ = nullptr;

  typedef std::pair<int64, int64> CacheKey;
  typedef absl::flat_hash_map<CacheKey, int64> TransitCallbackCache;
  typedef absl::flat_hash_map<CacheKey, StateDependentTransit>
      StateDependentTransitCallbackCache;

  std::vector<TransitCallback1> unary_transit_evaluators_;
  std::vector<TransitCallback2> transit_evaluators_;
  // The following vector stores a boolean per transit_evaluator_, indicating
  // whether the transits are all positive.
  // is_transit_evaluator_positive_ will be set to true only when registering a
  // callback via RegisterPositiveTransitCallback(), and to false otherwise.
  // The actual positivity of the transit values will only be checked in debug
  // mode, when calling RegisterPositiveTransitCallback().
  // Therefore, RegisterPositiveTransitCallback() should only be called when the
  // transits are known to be positive, as the positivity of a callback will
  // allow some improvements in the solver, but will entail in errors if the
  // transits are falsely assumed positive.
  std::vector<bool> is_transit_evaluator_positive_;
  std::vector<VariableIndexEvaluator2> state_dependent_transit_evaluators_;
  std::vector<std::unique_ptr<StateDependentTransitCallbackCache>>
      state_dependent_transit_evaluators_cache_;

  friend class RoutingDimension;
  friend class RoutingModelInspector;

  DISALLOW_COPY_AND_ASSIGN(RoutingModel);
};

/// Routing model visitor.
class RoutingModelVisitor : public BaseObject {
 public:
  /// Constraint types.
  static const char kLightElement[];
  static const char kLightElement2[];
  static const char kRemoveValues[];
};

#if !defined(SWIG)
/// This class acts like a CP propagator: it takes a set of tasks given by
/// their start/duration/end features, and reduces the range of possible values.
class DisjunctivePropagator {
 public:
  /// A structure to hold tasks described by their features.
  /// The first num_chain_tasks are considered linked by a chain of precedences,
  /// i.e. if i < j < num_chain_tasks, then end(i) <= start(j).
  /// This occurs frequently in routing, and can be leveraged by
  /// some variants of classic propagators.
  struct Tasks {
    int num_chain_tasks = 0;
    std::vector<int64> start_min;
    std::vector<int64> start_max;
    std::vector<int64> duration_min;
    std::vector<int64> duration_max;
    std::vector<int64> end_min;
    std::vector<int64> end_max;
    std::vector<bool> is_preemptible;
    std::vector<const SortedDisjointIntervalList*> forbidden_intervals;
    std::vector<std::pair<int64, int64>> distance_duration;
    int64 span_min = 0;
    int64 span_max = kint64max;

    void Clear() {
      start_min.clear();
      start_max.clear();
      duration_min.clear();
      duration_max.clear();
      end_min.clear();
      end_max.clear();
      is_preemptible.clear();
      forbidden_intervals.clear();
      distance_duration.clear();
      span_min = 0;
      span_max = kint64max;
      num_chain_tasks = 0;
    }
  };

  /// Computes new bounds for all tasks, returns false if infeasible.
  /// This does not compute a fixed point, so recalling it may filter more.
  bool Propagate(Tasks* tasks);

  /// Propagates the deductions from the chain of precedences, if there is one.
  bool Precedences(Tasks* tasks);
  /// Transforms the problem with a time symmetry centered in 0. Returns true
  /// for convenience.
  bool MirrorTasks(Tasks* tasks);
  /// Does edge-finding deductions on all tasks.
  bool EdgeFinding(Tasks* tasks);
  /// Does detectable precedences deductions on tasks in the chain precedence,
  /// taking the time windows of nonchain tasks into account.
  bool DetectablePrecedencesWithChain(Tasks* tasks);
  /// Tasks might have holes in their domain, this enforces such holes.
  bool ForbiddenIntervals(Tasks* tasks);
  /// Propagates distance_duration constraints, if any.
  bool DistanceDuration(Tasks* tasks);
  /// Propagates a lower bound of the chain span,
  /// end[num_chain_tasks] - start[0], to span_min.
  bool ChainSpanMin(Tasks* tasks);
  /// Computes a lower bound of the span of the chain, taking into account only
  /// the first nonchain task.
  /// For more accurate results, this should be called after Precedences(),
  /// otherwise the lower bound might be lower than feasible.
  bool ChainSpanMinDynamic(Tasks* tasks);

 private:
  /// The main algorithm uses Vilim's theta tree data structure.
  /// See Petr Vilim's PhD thesis "Global Constraints in Scheduling".
  sat::ThetaLambdaTree<int64> theta_lambda_tree_;
  /// Mappings between events and tasks.
  std::vector<int> tasks_by_start_min_;
  std::vector<int> tasks_by_end_max_;
  std::vector<int> event_of_task_;
  std::vector<int> nonchain_tasks_by_start_max_;
  /// Maps chain elements to the sum of chain task durations before them.
  std::vector<int64> total_duration_before_;
};

struct TravelBounds {
  std::vector<int64> min_travels;
  std::vector<int64> max_travels;
  std::vector<int64> pre_travels;
  std::vector<int64> post_travels;
};

void AppendTasksFromPath(const std::vector<int64>& path,
                         const TravelBounds& travel_bounds,
                         const RoutingDimension& dimension,
                         DisjunctivePropagator::Tasks* tasks);
void AppendTasksFromIntervals(const std::vector<IntervalVar*>& intervals,
                              DisjunctivePropagator::Tasks* tasks);
void FillPathEvaluation(const std::vector<int64>& path,
                        const RoutingModel::TransitCallback2& evaluator,
                        std::vector<int64>* values);
void FillTravelBoundsOfVehicle(int vehicle, const std::vector<int64>& path,
                               const RoutingDimension& dimension,
                               TravelBounds* travel_bounds);
#endif  // !defined(SWIG)

/// GlobalVehicleBreaksConstraint ensures breaks constraints are enforced on
/// all vehicles in the dimension passed to its constructor.
/// It is intended to be used for dimensions representing time.
/// A break constraint ensures break intervals fit on the route of a vehicle.
/// For a given vehicle, it forces break intervals to be disjoint from visit
/// intervals, where visit intervals start at CumulVar(node) and last for
/// node_visit_transit[node]. Moreover, it ensures that there is enough time
/// between two consecutive nodes of a route to do transit and vehicle breaks,
/// i.e. if Next(nodeA) = nodeB, CumulVar(nodeA) = tA and CumulVar(nodeB) = tB,
/// then SlackVar(nodeA) >= sum_{breaks \subseteq [tA, tB)} duration(break).
class GlobalVehicleBreaksConstraint : public Constraint {
 public:
  explicit GlobalVehicleBreaksConstraint(const RoutingDimension* dimension);
  std::string DebugString() const override {
    return "GlobalVehicleBreaksConstraint";
  }

  void Post() override;
  void InitialPropagate() override;

 private:
  void PropagateNode(int node);
  void PropagateVehicle(int vehicle);
  void PropagateMaxBreakDistance(int vehicle);

  const RoutingModel* model_;
  const RoutingDimension* const dimension_;
  std::vector<Demon*> vehicle_demons_;
  std::vector<int64> path_;

  /// Sets path_ to be the longest sequence such that
  /// _ path_[0] is the start of the vehicle
  /// _ Next(path_[i-1]) is Bound() and has value path_[i],
  /// followed by the end of the vehicle if the last node was not an end.
  void FillPartialPathOfVehicle(int vehicle);
  void FillPathTravels(const std::vector<int64>& path);

  /// This translates pruning information to solver variables.
  /// If constructed with an IntervalVar*, it follows the usual semantics of
  /// IntervalVars. If constructed with an IntVar*, before_start and
  /// after_start, operations are translated to simulate an interval that starts
  /// at start - before_start and ends and start + after_start. If constructed
  /// with nothing, the TaskTranslator will do nothing. This class should have
  /// been an interface + subclasses, but that would force pointers in the
  /// user's task vector, which means dynamic allocation. With this union-like
  /// structure, a vector's reserved size will adjust to usage and eventually no
  /// more dynamic allocation will be made.
  class TaskTranslator {
   public:
    TaskTranslator(IntVar* start, int64 before_start, int64 after_start)
        : start_(start),
          before_start_(before_start),
          after_start_(after_start) {}
    explicit TaskTranslator(IntervalVar* interval) : interval_(interval) {}
    TaskTranslator() {}

    void SetStartMin(int64 value) {
      if (start_ != nullptr) {
        start_->SetMin(CapAdd(before_start_, value));
      } else if (interval_ != nullptr) {
        interval_->SetStartMin(value);
      }
    }
    void SetStartMax(int64 value) {
      if (start_ != nullptr) {
        start_->SetMax(CapAdd(before_start_, value));
      } else if (interval_ != nullptr) {
        interval_->SetStartMax(value);
      }
    }
    void SetDurationMin(int64 value) {
      if (interval_ != nullptr) {
        interval_->SetDurationMin(value);
      }
    }
    void SetEndMin(int64 value) {
      if (start_ != nullptr) {
        start_->SetMin(CapSub(value, after_start_));
      } else if (interval_ != nullptr) {
        interval_->SetEndMin(value);
      }
    }
    void SetEndMax(int64 value) {
      if (start_ != nullptr) {
        start_->SetMax(CapSub(value, after_start_));
      } else if (interval_ != nullptr) {
        interval_->SetEndMax(value);
      }
    }

   private:
    IntVar* start_ = nullptr;
    int64 before_start_;
    int64 after_start_;
    IntervalVar* interval_ = nullptr;
  };

  /// Route and interval variables are normalized to the following values.
  std::vector<TaskTranslator> task_translators_;

  /// This is used to restrict bounds of tasks.
  DisjunctivePropagator disjunctive_propagator_;
  DisjunctivePropagator::Tasks tasks_;

  /// Used to help filling tasks_ at each propagation.
  TravelBounds travel_bounds_;
};

class TypeRegulationsChecker {
 public:
  explicit TypeRegulationsChecker(const RoutingModel& model);
  virtual ~TypeRegulationsChecker() {}

  bool CheckVehicle(int vehicle,
                    const std::function<int64(int64)>& next_accessor);

 protected:
#ifndef SWIG
  using VisitTypePolicy = RoutingModel::VisitTypePolicy;
#endif  // SWIG

  struct TypePolicyOccurrence {
    /// Number of TYPE_ADDED_TO_VEHICLE and
    /// TYPE_SIMULTANEOUSLY_ADDED_AND_REMOVED node type policies seen on the
    /// route.
    int num_type_added_to_vehicle = 0;
    /// Number of ADDED_TYPE_REMOVED_FROM_VEHICLE (effectively removing a type
    /// from the route) and TYPE_SIMULTANEOUSLY_ADDED_AND_REMOVED node type
    /// policies seen on the route.
    /// This number is always <= num_type_added_to_vehicle, as a type is only
    /// actually removed if it was on the route before.
    int num_type_removed_from_vehicle = 0;
    /// Position of the last node of policy TYPE_ON_VEHICLE_UP_TO_VISIT visited
    /// on the route.
    /// If positive, the type is considered on the vehicle from the start of the
    /// route until this position.
    int position_of_last_type_on_vehicle_up_to_visit = -1;
  };

  /// Returns true iff any occurrence of the given type was seen on the route,
  /// i.e. iff the added count for this type is positive, or if a node of this
  /// type and policy TYPE_ON_VEHICLE_UP_TO_VISIT is visited on the route (see
  /// TypePolicyOccurrence.last_type_on_vehicle_up_to_visit).
  bool TypeOccursOnRoute(int type) const;
  /// Returns true iff there's at least one instance of the given type on the
  /// route when scanning the route at the given position 'pos'.
  /// This is the case iff we have at least one added but non-removed instance
  /// of the type, or if
  /// occurrences_of_type_[type].last_type_on_vehicle_up_to_visit is greater
  /// than 'pos'.
  bool TypeCurrentlyOnRoute(int type, int pos) const;

  void InitializeCheck(int vehicle,
                       const std::function<int64(int64)>& next_accessor);
  virtual void OnInitializeCheck() {}
  virtual bool HasRegulationsToCheck() const = 0;
  virtual bool CheckTypeRegulations(int type, VisitTypePolicy policy,
                                    int pos) = 0;
  virtual bool FinalizeCheck() const { return true; }

  const RoutingModel& model_;

 private:
  std::vector<TypePolicyOccurrence> occurrences_of_type_;
  std::vector<int64> current_route_visits_;
};

/// Checker for type incompatibilities.
class TypeIncompatibilityChecker : public TypeRegulationsChecker {
 public:
  TypeIncompatibilityChecker(const RoutingModel& model,
                             bool check_hard_incompatibilities);
  ~TypeIncompatibilityChecker() override {}

 private:
  bool HasRegulationsToCheck() const override;
  bool CheckTypeRegulations(int type, VisitTypePolicy policy, int pos) override;
  /// NOTE(user): As temporal incompatibilities are always verified with
  /// this checker, we only store 1 boolean indicating whether or not hard
  /// incompatibilities are also verified.
  bool check_hard_incompatibilities_;
};

/// Checker for type requirements.
class TypeRequirementChecker : public TypeRegulationsChecker {
 public:
  explicit TypeRequirementChecker(const RoutingModel& model)
      : TypeRegulationsChecker(model) {}
  ~TypeRequirementChecker() override {}

 private:
  bool HasRegulationsToCheck() const override;
  void OnInitializeCheck() override {
    types_with_same_vehicle_requirements_on_route_.clear();
  }
  // clang-format off
  /// Verifies that for each set in required_type_alternatives, at least one of
  /// the required types is on the route at position 'pos'.
  bool CheckRequiredTypesCurrentlyOnRoute(
      const std::vector<absl::flat_hash_set<int> >& required_type_alternatives,
      int pos);
  // clang-format on
  bool CheckTypeRegulations(int type, VisitTypePolicy policy, int pos) override;
  bool FinalizeCheck() const override;

  absl::flat_hash_set<int> types_with_same_vehicle_requirements_on_route_;
};

/// The following constraint ensures that incompatibilities and requirements
/// between types are respected.
///
/// It verifies both "hard" and "temporal" incompatibilities.
/// Two nodes with hard incompatible types cannot be served by the same vehicle
/// at all, while with a temporal incompatibility they can't be on the same
/// route at the same time.
/// The VisitTypePolicy of a node determines how visiting it impacts the type
/// count on the route.
///
/// For example, for
/// - three temporally incompatible types T1 T2 and T3
/// - 2 pairs of nodes a1/r1 and a2/r2 of type T1 and T2 respectively, with
///     - a1 and a2 of VisitTypePolicy TYPE_ADDED_TO_VEHICLE
///     - r1 and r2 of policy ADDED_TYPE_REMOVED_FROM_VEHICLE
/// - 3 nodes A, UV and AR of type T3, respectively with type policies
///   TYPE_ADDED_TO_VEHICLE, TYPE_ON_VEHICLE_UP_TO_VISIT and
///   TYPE_SIMULTANEOUSLY_ADDED_AND_REMOVED
/// the configurations
/// UV --> a1 --> r1 --> a2 --> r2,   a1 --> r1 --> a2 --> r2 --> A and
/// a1 --> r1 --> AR --> a2 --> r2 are acceptable, whereas the configurations
/// a1 --> a2 --> r1 --> ..., or A --> a1 --> r1 --> ..., or
/// a1 --> r1 --> UV --> ... are not feasible.
///
/// It also verifies same-vehicle and temporal type requirements.
/// A node of type T_d with a same-vehicle requirement for type T_r needs to be
/// served by the same vehicle as a node of type T_r.
/// Temporal requirements, on the other hand, can take effect either when the
/// dependent type is being added to the route or when it's removed from it,
/// which is determined by the dependent node's VisitTypePolicy.
/// In the above example:
/// - If T3 is required on the same vehicle as T1, A, AR or UV must be on the
///   same vehicle as a1.
/// - If T2 is required when adding T1, a2 must be visited *before* a1, and if
///   r2 is also visited on the route, it must be *after* a1, i.e. T2 must be on
///   the vehicle when a1 is visited:
///   ... --> a2 --> ... --> a1 --> ... --> r2 --> ...
/// - If T3 is required when removing T1, T3 needs to be on the vehicle when
///   r1 is visited:
///   ... --> A --> ... --> r1 --> ...   OR   ... --> r1 --> ... --> UV --> ...
class TypeRegulationsConstraint : public Constraint {
 public:
  explicit TypeRegulationsConstraint(const RoutingModel& model);

  void Post() override;
  void InitialPropagate() override;

 private:
  void PropagateNodeRegulations(int node);
  void CheckRegulationsOnVehicle(int vehicle);

  const RoutingModel& model_;
  TypeIncompatibilityChecker incompatibility_checker_;
  TypeRequirementChecker requirement_checker_;
  std::vector<Demon*> vehicle_demons_;
};
#if !defined SWIG
/// A structure meant to store soft bounds and associated violation constants.
/// It is 'Simple' because it has one BoundCost per element,
/// in contrast to 'Multiple'. Design notes:
/// - it is meant to store model information to be shared through pointers,
///   so it disallows copy and assign to avoid accidental duplication.
/// - it keeps soft bounds as an array of structs to help cache,
///   because code that uses such bounds typically use both bound and cost.
/// - soft bounds are named pairs, prevents some mistakes.
/// - using operator[] to access elements is not interesting,
///   because the structure will be accessed through pointers, moreover having
///   to type bound_cost reminds the user of the order if they do a copy
///   assignment of the element.
class SimpleBoundCosts {
 public:
  struct BoundCost {
    int64 bound;
    int64 cost;
  };
  SimpleBoundCosts(int num_bounds, BoundCost default_bound_cost)
      : bound_costs_(num_bounds, default_bound_cost) {}
  BoundCost& bound_cost(int element) { return bound_costs_[element]; }
  BoundCost bound_cost(int element) const { return bound_costs_[element]; }
  int Size() { return bound_costs_.size(); }
  SimpleBoundCosts(const SimpleBoundCosts&) = delete;
  SimpleBoundCosts operator=(const SimpleBoundCosts&) = delete;

 private:
  std::vector<BoundCost> bound_costs_;
};
#endif  // !defined SWIG

/// Dimensions represent quantities accumulated at nodes along the routes. They
/// represent quantities such as weights or volumes carried along the route, or
/// distance or times.
///
/// Quantities at a node are represented by "cumul" variables and the increase
/// or decrease of quantities between nodes are represented by "transit"
/// variables. These variables are linked as follows:
///
/// if j == next(i),
/// cumuls(j) = cumuls(i) + transits(i) + slacks(i) +
///             state_dependent_transits(i)
///
/// where slack is a positive slack variable (can represent waiting times for
/// a time dimension), and state_dependent_transits is a non-purely functional
/// version of transits_. Favour transits over state_dependent_transits when
/// possible, because purely functional callbacks allow more optimisations and
/// make the model faster and easier to solve.
// TODO(user): Break constraints need to know the service time of nodes
/// for a given vehicle, it is passed as an external vector, it would be better
/// to have this information here.
class RoutingDimension {
 public:
  ~RoutingDimension();
  /// Returns the model on which the dimension was created.
  RoutingModel* model() const { return model_; }
  /// Returns the transition value for a given pair of nodes (as var index);
  /// this value is the one taken by the corresponding transit variable when
  /// the 'next' variable for 'from_index' is bound to 'to_index'.
  int64 GetTransitValue(int64 from_index, int64 to_index, int64 vehicle) const;
  /// Same as above but taking a vehicle class of the dimension instead of a
  /// vehicle (the class of a vehicle can be obtained with vehicle_to_class()).
  int64 GetTransitValueFromClass(int64 from_index, int64 to_index,
                                 int64 vehicle_class) const {
    return model_->TransitCallback(class_evaluators_[vehicle_class])(from_index,
                                                                     to_index);
  }
  /// Get the cumul, transit and slack variables for the given node (given as
  /// int64 var index).
  IntVar* CumulVar(int64 index) const { return cumuls_[index]; }
  IntVar* TransitVar(int64 index) const { return transits_[index]; }
  IntVar* FixedTransitVar(int64 index) const { return fixed_transits_[index]; }
  IntVar* SlackVar(int64 index) const { return slacks_[index]; }

#if !defined(SWIGPYTHON)
  /// Like CumulVar(), TransitVar(), SlackVar() but return the whole variable
  /// vectors instead (indexed by int64 var index).
  const std::vector<IntVar*>& cumuls() const { return cumuls_; }
  const std::vector<IntVar*>& fixed_transits() const { return fixed_transits_; }
  const std::vector<IntVar*>& transits() const { return transits_; }
  const std::vector<IntVar*>& slacks() const { return slacks_; }
#if !defined(SWIGCSHARP) && !defined(SWIGJAVA)
  /// Returns forbidden intervals for each node.
  const std::vector<SortedDisjointIntervalList>& forbidden_intervals() const {
    return forbidden_intervals_;
  }
  /// Returns allowed intervals for a given node in a given interval.
  SortedDisjointIntervalList GetAllowedIntervalsInRange(int64 index,
                                                        int64 min_value,
                                                        int64 max_value) const;
  /// Returns the smallest value outside the forbidden intervals of node 'index'
  /// that is greater than or equal to a given 'min_value'.
  int64 GetFirstPossibleGreaterOrEqualValueForNode(int64 index,
                                                   int64 min_value) const {
    DCHECK_LT(index, forbidden_intervals_.size());
    const SortedDisjointIntervalList& forbidden_intervals =
        forbidden_intervals_[index];
    const auto first_forbidden_interval_it =
        forbidden_intervals.FirstIntervalGreaterOrEqual(min_value);
    if (first_forbidden_interval_it != forbidden_intervals.end() &&
        min_value >= first_forbidden_interval_it->start) {
      /// min_value is in a forbidden interval.
      return CapAdd(first_forbidden_interval_it->end, 1);
    }
    /// min_value is not forbidden.
    return min_value;
  }
  /// Returns the largest value outside the forbidden intervals of node 'index'
  /// that is less than or equal to a given 'max_value'.
  /// NOTE: If this method is called with a max_value lower than the node's
  /// cumul min, it will return -1.
  int64 GetLastPossibleLessOrEqualValueForNode(int64 index,
                                               int64 max_value) const {
    DCHECK_LT(index, forbidden_intervals_.size());
    const SortedDisjointIntervalList& forbidden_intervals =
        forbidden_intervals_[index];
    const auto last_forbidden_interval_it =
        forbidden_intervals.LastIntervalLessOrEqual(max_value);
    if (last_forbidden_interval_it != forbidden_intervals.end() &&
        max_value <= last_forbidden_interval_it->end) {
      /// max_value is in a forbidden interval.
      return CapSub(last_forbidden_interval_it->start, 1);
    }
    /// max_value is not forbidden.
    return max_value;
  }
  /// Returns the capacities for all vehicles.
  const std::vector<int64>& vehicle_capacities() const {
    return vehicle_capacities_;
  }
  /// Returns the callback evaluating the transit value between two node indices
  /// for a given vehicle.
  const RoutingModel::TransitCallback2& transit_evaluator(int vehicle) const {
    return model_->TransitCallback(
        class_evaluators_[vehicle_to_class_[vehicle]]);
  }
  /// Returns the unary callback evaluating the transit value between two node
  /// indices for a given vehicle. If the corresponding callback is not unary,
  /// returns a null callback.
  const RoutingModel::TransitCallback1& GetUnaryTransitEvaluator(
      int vehicle) const {
    return model_->UnaryTransitCallbackOrNull(
        class_evaluators_[vehicle_to_class_[vehicle]]);
  }
  /// Returns true iff the transit evaluator of 'vehicle' is positive for all
  /// arcs.
  bool AreVehicleTransitsPositive(int vehicle) const {
    return model()->is_transit_evaluator_positive_
        [class_evaluators_[vehicle_to_class_[vehicle]]];
  }
  int vehicle_to_class(int vehicle) const { return vehicle_to_class_[vehicle]; }
#endif  /// !defined(SWIGCSHARP) && !defined(SWIGJAVA)
#endif  /// !defined(SWIGPYTHON)
  /// Sets an upper bound on the dimension span on a given vehicle. This is the
  /// preferred way to limit the "length" of the route of a vehicle according to
  /// a dimension.
  void SetSpanUpperBoundForVehicle(int64 upper_bound, int vehicle);
  /// Sets a cost proportional to the dimension span on a given vehicle,
  /// or on all vehicles at once. "coefficient" must be nonnegative.
  /// This is handy to model costs proportional to idle time when the dimension
  /// represents time.
  /// The cost for a vehicle is
  ///   span_cost = coefficient * (dimension end value - dimension start value).
  void SetSpanCostCoefficientForVehicle(int64 coefficient, int vehicle);
  void SetSpanCostCoefficientForAllVehicles(int64 coefficient);
  /// Sets a cost proportional to the *global* dimension span, that is the
  /// difference between the largest value of route end cumul variables and
  /// the smallest value of route start cumul variables.
  /// In other words:
  /// global_span_cost =
  ///   coefficient * (Max(dimension end value) - Min(dimension start value)).
  void SetGlobalSpanCostCoefficient(int64 coefficient);

#ifndef SWIG
  /// Sets a piecewise linear cost on the cumul variable of a given variable
  /// index. If f is a piecewise linear function, the resulting cost at 'index'
  /// will be f(CumulVar(index)). As of 3/2017, only non-decreasing positive
  /// cost functions are supported.
  void SetCumulVarPiecewiseLinearCost(int64 index,
                                      const PiecewiseLinearFunction& cost);
  /// Returns true if a piecewise linear cost has been set for a given variable
  /// index.
  bool HasCumulVarPiecewiseLinearCost(int64 index) const;
  /// Returns the piecewise linear cost of a cumul variable for a given variable
  /// index. The returned pointer has the same validity as this class.
  const PiecewiseLinearFunction* GetCumulVarPiecewiseLinearCost(
      int64 index) const;
#endif

  /// Sets a soft upper bound to the cumul variable of a given variable index.
  /// If the value of the cumul variable is greater than the bound, a cost
  /// proportional to the difference between this value and the bound is added
  /// to the cost function of the model:
  ///   cumulVar <= upper_bound -> cost = 0
  ///    cumulVar > upper_bound -> cost = coefficient * (cumulVar - upper_bound)
  /// This is also handy to model tardiness costs when the dimension represents
  /// time.
  void SetCumulVarSoftUpperBound(int64 index, int64 upper_bound,
                                 int64 coefficient);
  /// Returns true if a soft upper bound has been set for a given variable
  /// index.
  bool HasCumulVarSoftUpperBound(int64 index) const;
  /// Returns the soft upper bound of a cumul variable for a given variable
  /// index. The "hard" upper bound of the variable is returned if no soft upper
  /// bound has been set.
  int64 GetCumulVarSoftUpperBound(int64 index) const;
  /// Returns the cost coefficient of the soft upper bound of a cumul variable
  /// for a given variable index. If no soft upper bound has been set, 0 is
  /// returned.
  int64 GetCumulVarSoftUpperBoundCoefficient(int64 index) const;

  /// Sets a soft lower bound to the cumul variable of a given variable index.
  /// If the value of the cumul variable is less than the bound, a cost
  /// proportional to the difference between this value and the bound is added
  /// to the cost function of the model:
  ///   cumulVar > lower_bound -> cost = 0
  ///   cumulVar <= lower_bound -> cost = coefficient * (lower_bound -
  ///               cumulVar).
  /// This is also handy to model earliness costs when the dimension represents
  /// time.
  /// Note: Using soft lower and upper bounds or span costs together is, as of
  /// 6/2014, not well supported in the sense that an optimal schedule is not
  /// guaranteed.
  void SetCumulVarSoftLowerBound(int64 index, int64 lower_bound,
                                 int64 coefficient);
  /// Returns true if a soft lower bound has been set for a given variable
  /// index.
  bool HasCumulVarSoftLowerBound(int64 index) const;
  /// Returns the soft lower bound of a cumul variable for a given variable
  /// index. The "hard" lower bound of the variable is returned if no soft lower
  /// bound has been set.
  int64 GetCumulVarSoftLowerBound(int64 index) const;
  /// Returns the cost coefficient of the soft lower bound of a cumul variable
  /// for a given variable index. If no soft lower bound has been set, 0 is
  /// returned.
  int64 GetCumulVarSoftLowerBoundCoefficient(int64 index) const;
  /// Sets the breaks for a given vehicle. Breaks are represented by
  /// IntervalVars. They may interrupt transits between nodes and increase
  /// the value of corresponding slack variables.
  /// A break may take place before the start of a vehicle, after the end of
  /// a vehicle, or during a travel i -> j.
  ///
  /// In that case, the interval [break.Start(), break.End()) must be a subset
  /// of [CumulVar(i) + pre_travel(i, j), CumulVar(j) - post_travel(i, j)). In
  /// other words, a break may not overlap any node n's visit, given by
  /// [CumulVar(n) - post_travel(_, n), CumulVar(n) + pre_travel(n, _)).
  /// This formula considers post_travel(_, start) and pre_travel(end, _) to be
  /// 0; pre_travel will never be called on any (_, start) and post_travel will
  /// never we called on any (end, _). If pre_travel_evaluator or
  /// post_travel_evaluator is -1, it will be taken as a function that always
  /// returns 0.
  // TODO(user): Remove if !defined when routing.i is repaired.
#if !defined(SWIGPYTHON)
  void SetBreakIntervalsOfVehicle(std::vector<IntervalVar*> breaks, int vehicle,
                                  int pre_travel_evaluator,
                                  int post_travel_evaluator);
#endif  // !defined(SWIGPYTHON)

  /// Deprecated, sets pre_travel(i, j) = node_visit_transit[i].
  void SetBreakIntervalsOfVehicle(std::vector<IntervalVar*> breaks, int vehicle,
                                  std::vector<int64> node_visit_transits);

  /// With breaks supposed to be consecutive, this forces the distance between
  /// breaks of size at least minimum_break_duration to be at most distance.
  /// This supposes that the time until route start and after route end are
  /// infinite breaks.
  void SetBreakDistanceDurationOfVehicle(int64 distance, int64 duration,
                                         int vehicle);
  /// Sets up vehicle_break_intervals_, vehicle_break_distance_duration_,
  /// pre_travel_evaluators and post_travel_evaluators.
  void InitializeBreaks();
  /// Returns true if any break interval or break distance was defined.
  bool HasBreakConstraints() const;
#if !defined(SWIGPYTHON)
  /// Deprecated, sets pre_travel(i, j) = node_visit_transit[i]
  /// and post_travel(i, j) = group_delays(i, j).
  void SetBreakIntervalsOfVehicle(
      std::vector<IntervalVar*> breaks, int vehicle,
      std::vector<int64> node_visit_transits,
      std::function<int64(int64, int64)> group_delays);

  /// Returns the break intervals set by SetBreakIntervalsOfVehicle().
  const std::vector<IntervalVar*>& GetBreakIntervalsOfVehicle(
      int vehicle) const;
  /// Returns the pairs (distance, duration) specified by break distance
  /// constraints.
  // clang-format off
  const std::vector<std::pair<int64, int64> >&
      GetBreakDistanceDurationOfVehicle(int vehicle) const;
  // clang-format on
#endif  /// !defined(SWIGPYTHON)
  int GetPreTravelEvaluatorOfVehicle(int vehicle) const;
  int GetPostTravelEvaluatorOfVehicle(int vehicle) const;

  /// Returns the parent in the dependency tree if any or nullptr otherwise.
  const RoutingDimension* base_dimension() const { return base_dimension_; }
  /// It makes sense to use the function only for self-dependent dimension.
  /// For such dimensions the value of the slack of a node determines the
  /// transition cost of the next transit. Provided that
  ///   1. cumul[node] is fixed,
  ///   2. next[node] and next[next[node]] (if exists) are fixed,
  /// the value of slack[node] for which cumul[next[node]] + transit[next[node]]
  /// is minimized can be found in O(1) using this function.
  int64 ShortestTransitionSlack(int64 node) const;

  /// Returns the name of the dimension.
  const std::string& name() const { return name_; }

  /// Accessors.
#ifndef SWIG
  const ReverseArcListGraph<int, int>& GetPathPrecedenceGraph() const {
    return path_precedence_graph_;
  }
#endif  // SWIG

  /// Limits, in terms of maximum difference between the cumul variables,
  /// between the pickup and delivery alternatives belonging to a single
  /// pickup/delivery pair in the RoutingModel. The indices passed to the
  /// function respectively correspond to the position of the pickup in the
  /// vector of pickup alternatives, and delivery position in the delivery
  /// alternatives for this pickup/delivery pair. These limits should only be
  /// set when each node index appears in at most one pickup/delivery pair, i.e.
  /// each pickup (delivery) index is in a single pickup/delivery pair.first
  /// (pair.second).
  typedef std::function<int64(int, int)> PickupToDeliveryLimitFunction;

  void SetPickupToDeliveryLimitFunctionForPair(
      PickupToDeliveryLimitFunction limit_function, int pair_index);

  bool HasPickupToDeliveryLimits() const;
#ifndef SWIG
  int64 GetPickupToDeliveryLimitForPair(int pair_index, int pickup,
                                        int delivery) const;

  struct NodePrecedence {
    int64 first_node;
    int64 second_node;
    int64 offset;
  };

  void AddNodePrecedence(NodePrecedence precedence) {
    node_precedences_.push_back(precedence);
  }
  const std::vector<NodePrecedence>& GetNodePrecedences() const {
    return node_precedences_;
  }
#endif  // SWIG

  void AddNodePrecedence(int64 first_node, int64 second_node, int64 offset) {
    AddNodePrecedence({first_node, second_node, offset});
  }

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

  int64 GetGlobalOptimizerOffset() const {
    DCHECK_GE(global_optimizer_offset_, 0);
    return global_optimizer_offset_;
  }
  int64 GetLocalOptimizerOffsetForVehicle(int vehicle) const {
    if (vehicle >= local_optimizer_offset_for_vehicle_.size()) {
      return 0;
    }
    DCHECK_GE(local_optimizer_offset_for_vehicle_[vehicle], 0);
    return local_optimizer_offset_for_vehicle_[vehicle];
  }
#if !defined SWIG
  /// If the span of vehicle on this dimension is larger than bound,
  /// the cost will be increased by cost * (span - bound).
  void SetSoftSpanUpperBoundForVehicle(SimpleBoundCosts::BoundCost bound_cost,
                                       int vehicle) {
    if (!HasSoftSpanUpperBounds()) {
      vehicle_soft_span_upper_bound_ = absl::make_unique<SimpleBoundCosts>(
          model_->vehicles(), SimpleBoundCosts::BoundCost{kint64max, 0});
    }
    vehicle_soft_span_upper_bound_->bound_cost(vehicle) = bound_cost;
  }
  bool HasSoftSpanUpperBounds() const {
    return vehicle_soft_span_upper_bound_ != nullptr;
  }
  SimpleBoundCosts::BoundCost GetSoftSpanUpperBoundForVehicle(
      int vehicle) const {
    DCHECK(HasSoftSpanUpperBounds());
    return vehicle_soft_span_upper_bound_->bound_cost(vehicle);
  }
  /// If the span of vehicle on this dimension is larger than bound,
  /// the cost will be increased by cost * (span - bound)^2.
  void SetQuadraticCostSoftSpanUpperBoundForVehicle(
      SimpleBoundCosts::BoundCost bound_cost, int vehicle) {
    if (!HasQuadraticCostSoftSpanUpperBounds()) {
      vehicle_quadratic_cost_soft_span_upper_bound_ =
          absl::make_unique<SimpleBoundCosts>(
              model_->vehicles(), SimpleBoundCosts::BoundCost{kint64max, 0});
    }
    vehicle_quadratic_cost_soft_span_upper_bound_->bound_cost(vehicle) =
        bound_cost;
  }
  bool HasQuadraticCostSoftSpanUpperBounds() const {
    return vehicle_quadratic_cost_soft_span_upper_bound_ != nullptr;
  }
  SimpleBoundCosts::BoundCost GetQuadraticCostSoftSpanUpperBoundForVehicle(
      int vehicle) const {
    DCHECK(HasQuadraticCostSoftSpanUpperBounds());
    return vehicle_quadratic_cost_soft_span_upper_bound_->bound_cost(vehicle);
  }
#endif  /// !defined SWIG

 private:
  struct SoftBound {
    IntVar* var;
    int64 bound;
    int64 coefficient;
  };

  struct PiecewiseLinearCost {
    PiecewiseLinearCost() : var(nullptr), cost(nullptr) {}
    IntVar* var;
    std::unique_ptr<PiecewiseLinearFunction> cost;
  };

  class SelfBased {};
  RoutingDimension(RoutingModel* model, std::vector<int64> vehicle_capacities,
                   const std::string& name,
                   const RoutingDimension* base_dimension);
  RoutingDimension(RoutingModel* model, std::vector<int64> vehicle_capacities,
                   const std::string& name, SelfBased);
  void Initialize(const std::vector<int>& transit_evaluators,
                  const std::vector<int>& state_dependent_transit_evaluators,
                  int64 slack_max);
  void InitializeCumuls();
  void InitializeTransits(
      const std::vector<int>& transit_evaluators,
      const std::vector<int>& state_dependent_transit_evaluators,
      int64 slack_max);
  void InitializeTransitVariables(int64 slack_max);
  /// Sets up the cost variables related to cumul soft upper bounds.
  void SetupCumulVarSoftUpperBoundCosts(
      std::vector<IntVar*>* cost_elements) const;
  /// Sets up the cost variables related to cumul soft lower bounds.
  void SetupCumulVarSoftLowerBoundCosts(
      std::vector<IntVar*>* cost_elements) const;
  void SetupCumulVarPiecewiseLinearCosts(
      std::vector<IntVar*>* cost_elements) const;
  /// Sets up the cost variables related to the global span and per-vehicle span
  /// costs (only for the "slack" part of the latter).
  void SetupGlobalSpanCost(std::vector<IntVar*>* cost_elements) const;
  void SetupSlackAndDependentTransitCosts() const;
  /// Finalize the model of the dimension.
  void CloseModel(bool use_light_propagation);

  void SetOffsetForGlobalOptimizer(int64 offset) {
    global_optimizer_offset_ = std::max(Zero(), offset);
  }
  /// Moves elements of "offsets" into vehicle_offsets_for_local_optimizer_.
  void SetVehicleOffsetsForLocalOptimizer(std::vector<int64> offsets) {
    /// Make sure all offsets are positive.
    std::transform(offsets.begin(), offsets.end(), offsets.begin(),
                   [](int64 offset) { return std::max(Zero(), offset); });
    local_optimizer_offset_for_vehicle_ = std::move(offsets);
  }

  std::vector<IntVar*> cumuls_;
  std::vector<SortedDisjointIntervalList> forbidden_intervals_;
  std::vector<IntVar*> capacity_vars_;
  const std::vector<int64> vehicle_capacities_;
  std::vector<IntVar*> transits_;
  std::vector<IntVar*> fixed_transits_;
  /// Values in class_evaluators_ correspond to the evaluators in
  /// RoutingModel::transit_evaluators_ for each vehicle class.
  std::vector<int> class_evaluators_;
  std::vector<int64> vehicle_to_class_;
#ifndef SWIG
  ReverseArcListGraph<int, int> path_precedence_graph_;
#endif
  // For every {first_node, second_node, offset} element in node_precedences_,
  // if both first_node and second_node are performed, then
  // cumuls_[second_node] must be greater than (or equal to)
  // cumuls_[first_node] + offset.
  std::vector<NodePrecedence> node_precedences_;

  // The transits of a dimension may depend on its cumuls or the cumuls of
  // another dimension. There can be no cycles, except for self loops, a
  // typical example for this is a time dimension.
  const RoutingDimension* const base_dimension_;

  // Values in state_dependent_class_evaluators_ correspond to the evaluators
  // in RoutingModel::state_dependent_transit_evaluators_ for each vehicle
  // class.
  std::vector<int> state_dependent_class_evaluators_;
  std::vector<int64> state_dependent_vehicle_to_class_;

  // For each pickup/delivery pair_index for which limits have been set,
  // pickup_to_delivery_limits_per_pair_index_[pair_index] contains the
  // PickupToDeliveryLimitFunction for the pickup and deliveries in this pair.
  std::vector<PickupToDeliveryLimitFunction>
      pickup_to_delivery_limits_per_pair_index_;

  // Used if some vehicle has breaks in this dimension, typically time.
  bool break_constraints_are_initialized_ = false;
  // clang-format off
  std::vector<std::vector<IntervalVar*> > vehicle_break_intervals_;
  std::vector<std::vector<std::pair<int64, int64> > >
      vehicle_break_distance_duration_;
  // clang-format on
  // For each vehicle, stores the part of travel that is made directly
  // after (before) the departure (arrival) node of the travel.
  // These parts of the travel are non-interruptible, in particular by a break.
  std::vector<int> vehicle_pre_travel_evaluators_;
  std::vector<int> vehicle_post_travel_evaluators_;

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
  int64 global_optimizer_offset_;
  std::vector<int64> local_optimizer_offset_for_vehicle_;
  /// nullptr if not defined.
  std::unique_ptr<SimpleBoundCosts> vehicle_soft_span_upper_bound_;
  std::unique_ptr<SimpleBoundCosts>
      vehicle_quadratic_cost_soft_span_upper_bound_;
  friend class RoutingModel;
  friend class RoutingModelInspector;

  DISALLOW_COPY_AND_ASSIGN(RoutingDimension);
};

#ifndef SWIG
/// Class to arrange indices by by their distance and their angles from the
/// depot. Used in the Sweep first solution heuristic.
class SweepArranger {
 public:
  explicit SweepArranger(const std::vector<std::pair<int64, int64>>& points);
  virtual ~SweepArranger() {}
  void ArrangeIndices(std::vector<int64>* indices);
  void SetSectors(int sectors) { sectors_ = sectors; }

 private:
  std::vector<int> coordinates_;
  int sectors_;

  DISALLOW_COPY_AND_ASSIGN(SweepArranger);
};
#endif

/// A decision builder which tries to assign values to variables as close as
/// possible to target values first.
DecisionBuilder* MakeSetValuesFromTargets(Solver* solver,
                                          std::vector<IntVar*> variables,
                                          std::vector<int64> targets);

#ifndef SWIG
// Helper class that stores vehicles by their type. Two vehicles have the same
// "vehicle type" iff they have the same cost class and start/end nodes.
class VehicleTypeCurator {
 public:
  explicit VehicleTypeCurator(
      const RoutingModel::VehicleTypeContainer& vehicle_type_container)
      : vehicle_type_container_(&vehicle_type_container) {}

  int NumTypes() const { return vehicle_type_container_->NumTypes(); }

  int Type(int vehicle) const { return vehicle_type_container_->Type(vehicle); }

  void Reset() {
    sorted_vehicle_classes_per_type_ =
        vehicle_type_container_->sorted_vehicle_classes_per_type;
    const std::vector<std::deque<int>>& vehicles_per_class =
        vehicle_type_container_->vehicles_per_vehicle_class;
    vehicles_per_vehicle_class_.resize(vehicles_per_class.size());
    for (int i = 0; i < vehicles_per_vehicle_class_.size(); i++) {
      vehicles_per_vehicle_class_[i].resize(vehicles_per_class[i].size());
      std::copy(vehicles_per_class[i].begin(), vehicles_per_class[i].end(),
                vehicles_per_vehicle_class_[i].begin());
    }
  }

  int GetVehicleOfType(int type) const {
    DCHECK_LT(type, NumTypes());
    const std::set<VehicleClassEntry>& vehicle_classes =
        sorted_vehicle_classes_per_type_[type];
    if (vehicle_classes.empty()) {
      return -1;
    }
    const int vehicle_class = (vehicle_classes.begin())->vehicle_class;
    DCHECK(!vehicles_per_vehicle_class_[vehicle_class].empty());
    return vehicles_per_vehicle_class_[vehicle_class][0];
  }

  void ReinjectVehicleOfClass(int vehicle, int vehicle_class,
                              int64 fixed_cost) {
    std::vector<int>& vehicles = vehicles_per_vehicle_class_[vehicle_class];
    if (vehicles.empty()) {
      // Add the vehicle class entry to the set (it was removed when
      // vehicles_per_vehicle_class_[vehicle_class] got empty).
      std::set<VehicleClassEntry>& vehicle_classes =
          sorted_vehicle_classes_per_type_[Type(vehicle)];
      const auto& insertion =
          vehicle_classes.insert({vehicle_class, fixed_cost});
      DCHECK(insertion.second);
    }
    vehicles.push_back(vehicle);
  }

  // Searches for the best compatible vehicle of the given type, i.e. the first
  // vehicle v of type 'type' for which vehicle_is_compatible(v) returns true.
  // If a compatible vehicle is found, its index is removed from
  // vehicles_per_vehicle_class_ and returned.
  // Returns -1 otherwise.
  int GetCompatibleVehicleOfType(
      int type, std::function<bool(int)> vehicle_is_compatible);

 private:
  using VehicleClassEntry =
      RoutingModel::VehicleTypeContainer::VehicleClassEntry;
  const RoutingModel::VehicleTypeContainer* const vehicle_type_container_;
  // clang-format off
  std::vector<std::set<VehicleClassEntry> > sorted_vehicle_classes_per_type_;
  std::vector<std::vector<int> > vehicles_per_vehicle_class_;
  // clang-format on
};

/// Decision builder building a solution using heuristics with local search
/// filters to evaluate its feasibility. This is very fast but can eventually
/// fail when the solution is restored if filters did not detect all
/// infeasiblities.
/// More details:
/// Using local search filters to build a solution. The approach is pretty
/// straight-forward: have a general assignment storing the current solution,
/// build delta assigment representing possible extensions to the current
/// solution and validate them with filters.
/// The tricky bit comes from using the assignment and filter APIs in a way
/// which avoids the lazy creation of internal hash_maps between variables
/// and indices.

/// Generic filter-based decision builder using an IntVarFilteredHeuristic.
// TODO(user): Eventually move this to the core CP solver library
/// when the code is mature enough.
class IntVarFilteredDecisionBuilder : public DecisionBuilder {
 public:
  explicit IntVarFilteredDecisionBuilder(
      std::unique_ptr<IntVarFilteredHeuristic> heuristic);

  ~IntVarFilteredDecisionBuilder() override {}

  Decision* Next(Solver* solver) override;

  std::string DebugString() const override;

  /// Returns statistics from its underlying heuristic.
  int64 number_of_decisions() const;
  int64 number_of_rejects() const;

 private:
  const std::unique_ptr<IntVarFilteredHeuristic> heuristic_;
};

/// Generic filter-based heuristic applied to IntVars.
class IntVarFilteredHeuristic {
 public:
  IntVarFilteredHeuristic(Solver* solver, const std::vector<IntVar*>& vars,
                          LocalSearchFilterManager* filter_manager);

  virtual ~IntVarFilteredHeuristic() {}

  /// Builds a solution. Returns the resulting assignment if a solution was
  /// found, and nullptr otherwise.
  Assignment* const BuildSolution();

  /// Returns statistics on search, number of decisions sent to filters, number
  /// of decisions rejected by filters.
  int64 number_of_decisions() const { return number_of_decisions_; }
  int64 number_of_rejects() const { return number_of_rejects_; }

  virtual std::string DebugString() const { return "IntVarFilteredHeuristic"; }

 protected:
  /// Resets the data members for a new solution.
  void ResetSolution();
  /// Virtual method to initialize the solution.
  virtual bool InitializeSolution() { return true; }
  /// Virtual method to redefine how to build a solution.
  virtual bool BuildSolutionInternal() = 0;
  /// Commits the modifications to the current solution if these modifications
  /// are "filter-feasible", returns false otherwise; in any case discards
  /// all modifications.
  bool Commit();
  /// Returns true if the search must be stopped.
  virtual bool StopSearch() { return false; }
  /// Modifies the current solution by setting the variable of index 'index' to
  /// value 'value'.
  void SetValue(int64 index, int64 value) {
    if (!is_in_delta_[index]) {
      delta_->FastAdd(vars_[index])->SetValue(value);
      delta_indices_.push_back(index);
      is_in_delta_[index] = true;
    } else {
      delta_->SetValue(vars_[index], value);
    }
  }
  /// Returns the value of the variable of index 'index' in the last committed
  /// solution.
  int64 Value(int64 index) const {
    return assignment_->IntVarContainer().Element(index).Value();
  }
  /// Returns true if the variable of index 'index' is in the current solution.
  bool Contains(int64 index) const {
    return assignment_->IntVarContainer().Element(index).Var() != nullptr;
  }
  /// Returns the number of variables the decision builder is trying to
  /// instantiate.
  int Size() const { return vars_.size(); }
  /// Returns the variable of index 'index'.
  IntVar* Var(int64 index) const { return vars_[index]; }
  /// Synchronizes filters with an assignment (the current solution).
  void SynchronizeFilters();

  Assignment* const assignment_;

 private:
  /// Checks if filters accept a given modification to the current solution
  /// (represented by delta).
  bool FilterAccept();

  Solver* solver_;
  const std::vector<IntVar*> vars_;
  Assignment* const delta_;
  std::vector<int> delta_indices_;
  std::vector<bool> is_in_delta_;
  Assignment* const empty_;
  LocalSearchFilterManager* filter_manager_;
  /// Stats on search
  int64 number_of_decisions_;
  int64 number_of_rejects_;
};

/// Filter-based heuristic dedicated to routing.
class RoutingFilteredHeuristic : public IntVarFilteredHeuristic {
 public:
  RoutingFilteredHeuristic(RoutingModel* model,
                           LocalSearchFilterManager* filter_manager);
  ~RoutingFilteredHeuristic() override {}
  /// Builds a solution starting from the routes formed by the next accessor.
  const Assignment* BuildSolutionFromRoutes(
      const std::function<int64(int64)>& next_accessor);
  RoutingModel* model() const { return model_; }
  /// Returns the end of the start chain of vehicle,
  int GetStartChainEnd(int vehicle) const { return start_chain_ends_[vehicle]; }
  /// Returns the start of the end chain of vehicle,
  int GetEndChainStart(int vehicle) const { return end_chain_starts_[vehicle]; }
  /// Make nodes in the same disjunction as 'node' unperformed. 'node' is a
  /// variable index corresponding to a node.
  void MakeDisjunctionNodesUnperformed(int64 node);
  /// Make all unassigned nodes unperformed.
  void MakeUnassignedNodesUnperformed();

 protected:
  bool StopSearch() override { return model_->CheckLimit(); }
  virtual void SetVehicleIndex(int64 node, int vehicle) {}
  virtual void ResetVehicleIndices() {}

 private:
  /// Initializes the current solution with empty or partial vehicle routes.
  bool InitializeSolution() override;

  RoutingModel* const model_;
  std::vector<int64> start_chain_ends_;
  std::vector<int64> end_chain_starts_;
};

class CheapestInsertionFilteredHeuristic : public RoutingFilteredHeuristic {
 public:
  /// Takes ownership of evaluator.
  CheapestInsertionFilteredHeuristic(
      RoutingModel* model, std::function<int64(int64, int64, int64)> evaluator,
      std::function<int64(int64)> penalty_evaluator,
      LocalSearchFilterManager* filter_manager);
  ~CheapestInsertionFilteredHeuristic() override {}

 protected:
  typedef std::pair<int64, int64> ValuedPosition;
  struct StartEndValue {
    int64 distance;
    int vehicle;

    bool operator<(const StartEndValue& other) const {
      return std::tie(distance, vehicle) <
             std::tie(other.distance, other.vehicle);
    }
  };
  typedef std::pair<StartEndValue, /*seed_node*/ int> Seed;

  /// Computes and returns the distance of each uninserted node to every vehicle
  /// in "vehicles" as a std::vector<std::vector<StartEndValue>>,
  /// start_end_distances_per_node.
  /// For each node, start_end_distances_per_node[node] is sorted in decreasing
  /// order.
  // clang-format off
  std::vector<std::vector<StartEndValue> >
      ComputeStartEndDistanceForVehicles(const std::vector<int>& vehicles);

  /// Initializes the priority_queue by inserting the best entry corresponding
  /// to each node, i.e. the last element of start_end_distances_per_node[node],
  /// which is supposed to be sorted in decreasing order.
  /// Queue is a priority queue containing Seeds.
  template <class Queue>
  void InitializePriorityQueue(
      std::vector<std::vector<StartEndValue> >* start_end_distances_per_node,
      Queue* priority_queue);
  // clang-format on

  /// Inserts 'node' just after 'predecessor', and just before 'successor',
  /// resulting in the following subsequence: predecessor -> node -> successor.
  /// If 'node' is part of a disjunction, other nodes of the disjunction are
  /// made unperformed.
  void InsertBetween(int64 node, int64 predecessor, int64 successor);
  /// Helper method to the ComputeEvaluatorSortedPositions* methods. Finds all
  /// possible insertion positions of node 'node_to_insert' in the partial route
  /// starting at node 'start' and adds them to 'valued_position', a list of
  /// unsorted pairs of (cost, position to insert the node).
  void AppendEvaluatedPositionsAfter(
      int64 node_to_insert, int64 start, int64 next_after_start, int64 vehicle,
      std::vector<ValuedPosition>* valued_positions);
  /// Returns the cost of inserting 'node_to_insert' between 'insert_after' and
  /// 'insert_before' on the 'vehicle', i.e.
  /// Cost(insert_after-->node) + Cost(node-->insert_before)
  /// - Cost (insert_after-->insert_before).
  int64 GetInsertionCostForNodeAtPosition(int64 node_to_insert,
                                          int64 insert_after,
                                          int64 insert_before,
                                          int vehicle) const;
  /// Returns the cost of unperforming node 'node_to_insert'. Returns kint64max
  /// if penalty callback is null or if the node cannot be unperformed.
  int64 GetUnperformedValue(int64 node_to_insert) const;

  std::function<int64(int64, int64, int64)> evaluator_;
  std::function<int64(int64)> penalty_evaluator_;
};

/// Filter-based decision builder which builds a solution by inserting
/// nodes at their cheapest position on any route; potentially several routes
/// can be built in parallel. The cost of a position is computed from an
/// arc-based cost callback. The node selected for insertion is the one which
/// minimizes insertion cost. If a non null penalty evaluator is passed, making
/// nodes unperformed is also taken into account with the corresponding penalty
/// cost.
class GlobalCheapestInsertionFilteredHeuristic
    : public CheapestInsertionFilteredHeuristic {
 public:
  struct GlobalCheapestInsertionParameters {
    /// Whether the routes are constructed sequentially or in parallel.
    bool is_sequential;
    /// The ratio of routes on which to insert farthest nodes as seeds before
    /// starting the cheapest insertion.
    double farthest_seeds_ratio;
    /// If neighbors_ratio < 1 then for each node only this ratio of its
    /// neighbors leading to the smallest arc costs are considered.
    double neighbors_ratio;
    /// If true, only closest neighbors (see neighbors_ratio) are considered
    /// as insertion positions during initialization. Otherwise, all possible
    /// insertion positions are considered.
    bool use_neighbors_ratio_for_initialization;
  };

  /// Takes ownership of evaluators.
  GlobalCheapestInsertionFilteredHeuristic(
      RoutingModel* model, std::function<int64(int64, int64, int64)> evaluator,
      std::function<int64(int64)> penalty_evaluator,
      LocalSearchFilterManager* filter_manager,
      GlobalCheapestInsertionParameters parameters);
  ~GlobalCheapestInsertionFilteredHeuristic() override {}
  bool BuildSolutionInternal() override;
  std::string DebugString() const override {
    return "GlobalCheapestInsertionFilteredHeuristic";
  }

 private:
  class PairEntry;
  class NodeEntry;
  typedef absl::flat_hash_set<PairEntry*> PairEntries;
  typedef absl::flat_hash_set<NodeEntry*> NodeEntries;

  /// Inserts non-inserted single nodes which have a visit type in the
  /// type requirement graph, i.e. required for or requiring another type for
  /// insertions.
  /// These nodes are inserted iff the requirement graph is acyclic, in which
  /// case nodes are inserted based on the topological order of their type,
  /// given by the routing model's GetTopologicallySortedVisitTypes() method.
  /// TODO(user): Adapt this method to also insert pairs.
  void InsertNodesByRequirementTopologicalOrder();

  /// Inserts all non-inserted pickup and delivery pairs. Maintains a priority
  /// queue of possible pair insertions, which is incrementally updated when a
  /// pair insertion is committed. Incrementality is obtained by updating pair
  /// insertion positions on the four newly modified route arcs: after the
  /// pickup insertion position, after the pickup position, after the delivery
  /// insertion position and after the delivery position.
  void InsertPairs();

  /// Inserts non-inserted individual nodes on the given routes (or all routes
  /// if "vehicles" is an empty vector), by constructing routes in parallel.
  /// Maintains a priority queue of possible insertions, which is incrementally
  /// updated when an insertion is committed.
  /// Incrementality is obtained by updating insertion positions on the two
  /// newly modified route arcs: after the node insertion position and after the
  /// node position.
  void InsertNodesOnRoutes(const std::vector<int>& nodes,
                           const absl::flat_hash_set<int>& vehicles);

  /// Inserts non-inserted individual nodes on routes by constructing routes
  /// sequentially.
  /// For each new route, the vehicle to use and the first node to insert on it
  /// are given by calling InsertSeedNode(). The route is then completed with
  /// other nodes by calling InsertNodesOnRoutes({vehicle}).
  void SequentialInsertNodes(const std::vector<int>& nodes);

  /// Goes through all vehicles in the model to check if they are already used
  /// (i.e. Value(start) != end) or not.
  /// Updates the three passed vectors accordingly.
  void DetectUsedVehicles(std::vector<bool>* is_vehicle_used,
                          std::vector<int>* unused_vehicles,
                          absl::flat_hash_set<int>* used_vehicles);

  /// Inserts the (farthest_seeds_ratio_ * model()->vehicles()) nodes farthest
  /// from the start/ends of the available vehicle routes as seeds on their
  /// closest route.
  void InsertFarthestNodesAsSeeds();

  /// Inserts a "seed node" based on the given priority_queue of Seeds.
  /// A "seed" is the node used in order to start a new route.
  /// If the Seed at the top of the priority queue cannot be inserted,
  /// (node already inserted in the model, corresponding vehicle already used,
  /// or unsuccessful Commit()), start_end_distances_per_node is updated and
  /// used to insert a new entry for that node if necessary (next best vehicle).
  /// If a seed node is successfully inserted, updates is_vehicle_used and
  /// returns the vehice of the corresponding route. Returns -1 otherwise.
  template <class Queue>
  int InsertSeedNode(
      std::vector<std::vector<StartEndValue>>* start_end_distances_per_node,
      Queue* priority_queue, std::vector<bool>* is_vehicle_used);
  // clang-format on

  /// Initializes the priority queue and the pair entries with the current state
  /// of the solution.
  void InitializePairPositions(
      AdjustablePriorityQueue<PairEntry>* priority_queue,
      std::vector<PairEntries>* pickup_to_entries,
      std::vector<PairEntries>* delivery_to_entries);
  /// Adds insertion entries performing the 'pickup' and 'delivery', and updates
  /// 'priority_queue', pickup_to_entries and delivery_to_entries accordingly.
  /// Based on gci_params_.use_neighbors_ratio_for_initialization, either all
  /// contained nodes are considered as insertion positions, or only the
  /// closest neighbors of 'pickup' and/or 'delivery'.
  void InitializeInsertionEntriesPerformingPair(
      int64 pickup, int64 delivery, int64 penalty,
      AdjustablePriorityQueue<PairEntry>* priority_queue,
      std::vector<PairEntries>* pickup_to_entries,
      std::vector<PairEntries>* delivery_to_entries);
  /// Updates all pair entries inserting a node after node "insert_after" and
  /// updates the priority queue accordingly.
  void UpdatePairPositions(int vehicle, int64 insert_after,
                           AdjustablePriorityQueue<PairEntry>* priority_queue,
                           std::vector<PairEntries>* pickup_to_entries,
                           std::vector<PairEntries>* delivery_to_entries) {
    UpdatePickupPositions(vehicle, insert_after, priority_queue,
                          pickup_to_entries, delivery_to_entries);
    UpdateDeliveryPositions(vehicle, insert_after, priority_queue,
                            pickup_to_entries, delivery_to_entries);
  }
  /// Updates all pair entries inserting their pickup node after node
  /// "insert_after" and updates the priority queue accordingly.
  void UpdatePickupPositions(int vehicle, int64 pickup_insert_after,
                             AdjustablePriorityQueue<PairEntry>* priority_queue,
                             std::vector<PairEntries>* pickup_to_entries,
                             std::vector<PairEntries>* delivery_to_entries);
  /// Updates all pair entries inserting their delivery node after node
  /// "insert_after" and updates the priority queue accordingly.
  void UpdateDeliveryPositions(
      int vehicle, int64 delivery_insert_after,
      AdjustablePriorityQueue<PairEntry>* priority_queue,
      std::vector<PairEntries>* pickup_to_entries,
      std::vector<PairEntries>* delivery_to_entries);
  /// Deletes an entry, removing it from the priority queue and the appropriate
  /// pickup and delivery entry sets.
  void DeletePairEntry(PairEntry* entry,
                       AdjustablePriorityQueue<PairEntry>* priority_queue,
                       std::vector<PairEntries>* pickup_to_entries,
                       std::vector<PairEntries>* delivery_to_entries);
  /// Initializes the priority queue and the node entries with the current state
  /// of the solution on the given vehicle routes.
  void InitializePositions(const std::vector<int>& nodes,
                           AdjustablePriorityQueue<NodeEntry>* priority_queue,
                           std::vector<NodeEntries>* position_to_node_entries,
                           const absl::flat_hash_set<int>& vehicles);
  /// Adds insertion entries performing 'node', and updates 'priority_queue' and
  /// position_to_node_entries accordingly.
  /// Based on gci_params_.use_neighbors_ratio_for_initialization, either all
  /// contained nodes are considered as insertion positions, or only the
  /// closest neighbors of 'node'.
  void InitializeInsertionEntriesPerformingNode(
      int64 node, int64 penalty, const absl::flat_hash_set<int>& vehicles,
      AdjustablePriorityQueue<NodeEntry>* priority_queue,
      std::vector<NodeEntries>* position_to_node_entries);
  /// Updates all node entries inserting a node after node "insert_after" and
  /// updates the priority queue accordingly.
  void UpdatePositions(const std::vector<int>& nodes, int vehicle,
                       int64 insert_after,
                       AdjustablePriorityQueue<NodeEntry>* priority_queue,
                       std::vector<NodeEntries>* node_entries);
  /// Deletes an entry, removing it from the priority queue and the appropriate
  /// node entry sets.
  void DeleteNodeEntry(NodeEntry* entry,
                       AdjustablePriorityQueue<NodeEntry>* priority_queue,
                       std::vector<NodeEntries>* node_entries);

  /// Computes the neighborhood of all nodes for every cost class, if needed and
  /// not done already.
  void ComputeNeighborhoods();

  /// Marks neighbor_index as visited in
  /// node_index_to_[pickup|delivery|single]_neighbors_by_cost_class_
  /// [node_index][cost_class] according to whether the neighbor is a pickup,
  /// a delivery, or neither.
  void AddNeighborForCostClass(int cost_class, int64 node_index,
                               int64 neighbor_index, bool neighbor_is_pickup,
                               bool neighbor_is_delivery);

  /// Returns true iff neighbor_index is in node_index's neighbors list
  /// corresponding to neighbor_is_pickup and neighbor_is_delivery.
  bool IsNeighborForCostClass(int cost_class, int64 node_index,
                              int64 neighbor_index) const;

  /// Returns a reference to the set of pickup neighbors of node_index.
  const std::vector<int64>& GetPickupNeighborsOfNodeForCostClass(
      int cost_class, int64 node_index) const {
    if (gci_params_.neighbors_ratio == 1) {
      return pickup_nodes_;
    }
    return node_index_to_pickup_neighbors_by_cost_class_[node_index][cost_class]
        ->PositionsSetAtLeastOnce();
  }

  /// Same as above for delivery neighbors.
  const std::vector<int64>& GetDeliveryNeighborsOfNodeForCostClass(
      int cost_class, int64 node_index) const {
    if (gci_params_.neighbors_ratio == 1) {
      return delivery_nodes_;
    }
    return node_index_to_delivery_neighbors_by_cost_class_
        [node_index][cost_class]
            ->PositionsSetAtLeastOnce();
  }

  /// Same as above for non pickup/delivery neighbors.
  const std::vector<int64>& GetSingleNeighborsOfNodeForCostClass(
      int cost_class, int64 node_index) const {
    if (gci_params_.neighbors_ratio == 1) {
      return single_nodes_;
    }
    return node_index_to_single_neighbors_by_cost_class_[node_index][cost_class]
        ->PositionsSetAtLeastOnce();
  }

  /// Returns an iterator to the concatenation of all neighbors.
  std::vector<const std::vector<int64>*> GetNeighborsOfNodeForCostClass(
      int cost_class, int64 node_index) const {
    return {&GetSingleNeighborsOfNodeForCostClass(cost_class, node_index),
            &GetPickupNeighborsOfNodeForCostClass(cost_class, node_index),
            &GetDeliveryNeighborsOfNodeForCostClass(cost_class, node_index)};
  }

  void ResetVehicleIndices() override {
    node_index_to_vehicle_.assign(node_index_to_vehicle_.size(), -1);
  }

  void SetVehicleIndex(int64 node, int vehicle) override {
    DCHECK_LT(node, node_index_to_vehicle_.size());
    node_index_to_vehicle_[node] = vehicle;
  }

  /// Function that verifies node_index_to_vehicle_ is correctly filled for all
  /// nodes given the current routes.
  bool CheckVehicleIndices() const;

  GlobalCheapestInsertionParameters gci_params_;
  /// Stores the vehicle index of each node in the current assignment.
  std::vector<int> node_index_to_vehicle_;

  // clang-format off
  std::vector<std::vector<std::unique_ptr<SparseBitset<int64> > > >
      node_index_to_single_neighbors_by_cost_class_;
  std::vector<std::vector<std::unique_ptr<SparseBitset<int64> > > >
      node_index_to_pickup_neighbors_by_cost_class_;
  std::vector<std::vector<std::unique_ptr<SparseBitset<int64> > > >
      node_index_to_delivery_neighbors_by_cost_class_;
  // clang-format on

  /// When neighbors_ratio is 1, we don't compute the neighborhood members
  /// above, and use the following vectors in the code to avoid unnecessary
  /// computations and decrease the time and space complexities.
  std::vector<int64> single_nodes_;
  std::vector<int64> pickup_nodes_;
  std::vector<int64> delivery_nodes_;
};

/// Filter-base decision builder which builds a solution by inserting
/// nodes at their cheapest position. The cost of a position is computed
/// an arc-based cost callback. Node selected for insertion are considered in
/// decreasing order of distance to the start/ends of the routes, i.e. farthest
/// nodes are inserted first.
class LocalCheapestInsertionFilteredHeuristic
    : public CheapestInsertionFilteredHeuristic {
 public:
  /// Takes ownership of evaluator.
  LocalCheapestInsertionFilteredHeuristic(
      RoutingModel* model, std::function<int64(int64, int64, int64)> evaluator,
      LocalSearchFilterManager* filter_manager);
  ~LocalCheapestInsertionFilteredHeuristic() override {}
  bool BuildSolutionInternal() override;
  std::string DebugString() const override {
    return "LocalCheapestInsertionFilteredHeuristic";
  }

 private:
  /// Computes the possible insertion positions of 'node' and sorts them
  /// according to the current cost evaluator.
  /// 'node' is a variable index corresponding to a node, 'sorted_positions' is
  /// a vector of variable indices corresponding to nodes after which 'node' can
  /// be inserted.
  void ComputeEvaluatorSortedPositions(int64 node,
                                       std::vector<int64>* sorted_positions);
  /// Like ComputeEvaluatorSortedPositions, subject to the additional
  /// restrictions that the node may only be inserted after node 'start' on the
  /// route. For convenience, this method also needs the node that is right
  /// after 'start' on the route.
  void ComputeEvaluatorSortedPositionsOnRouteAfter(
      int64 node, int64 start, int64 next_after_start,
      std::vector<int64>* sorted_positions);

  std::vector<std::vector<StartEndValue>> start_end_distances_per_node_;
};

/// Filtered-base decision builder based on the addition heuristic, extending
/// a path from its start node with the cheapest arc.
class CheapestAdditionFilteredHeuristic : public RoutingFilteredHeuristic {
 public:
  CheapestAdditionFilteredHeuristic(RoutingModel* model,
                                    LocalSearchFilterManager* filter_manager);
  ~CheapestAdditionFilteredHeuristic() override {}
  bool BuildSolutionInternal() override;

 private:
  class PartialRoutesAndLargeVehicleIndicesFirst {
   public:
    explicit PartialRoutesAndLargeVehicleIndicesFirst(
        const CheapestAdditionFilteredHeuristic& builder)
        : builder_(builder) {}
    bool operator()(int vehicle1, int vehicle2) const;

   private:
    const CheapestAdditionFilteredHeuristic& builder_;
  };
  /// Returns a vector of possible next indices of node from an iterator.
  template <typename Iterator>
  std::vector<int64> GetPossibleNextsFromIterator(int64 node, Iterator start,
                                                  Iterator end) const {
    const int size = model()->Size();
    std::vector<int64> nexts;
    for (Iterator it = start; it != end; ++it) {
      const int64 next = *it;
      if (next != node && (next >= size || !Contains(next))) {
        nexts.push_back(next);
      }
    }
    return nexts;
  }
  /// Sorts a vector of successors of node.
  virtual void SortSuccessors(int64 node, std::vector<int64>* successors) = 0;
  virtual int64 FindTopSuccessor(int64 node,
                                 const std::vector<int64>& successors) = 0;
};

/// A CheapestAdditionFilteredHeuristic where the notion of 'cheapest arc'
/// comes from an arc evaluator.
class EvaluatorCheapestAdditionFilteredHeuristic
    : public CheapestAdditionFilteredHeuristic {
 public:
  /// Takes ownership of evaluator.
  EvaluatorCheapestAdditionFilteredHeuristic(
      RoutingModel* model, std::function<int64(int64, int64)> evaluator,
      LocalSearchFilterManager* filter_manager);
  ~EvaluatorCheapestAdditionFilteredHeuristic() override {}
  std::string DebugString() const override {
    return "EvaluatorCheapestAdditionFilteredHeuristic";
  }

 private:
  /// Next nodes are sorted according to the current evaluator.
  void SortSuccessors(int64 node, std::vector<int64>* successors) override;
  int64 FindTopSuccessor(int64 node,
                         const std::vector<int64>& successors) override;

  std::function<int64(int64, int64)> evaluator_;
};

/// A CheapestAdditionFilteredHeuristic where the notion of 'cheapest arc'
/// comes from an arc comparator.
class ComparatorCheapestAdditionFilteredHeuristic
    : public CheapestAdditionFilteredHeuristic {
 public:
  /// Takes ownership of evaluator.
  ComparatorCheapestAdditionFilteredHeuristic(
      RoutingModel* model, Solver::VariableValueComparator comparator,
      LocalSearchFilterManager* filter_manager);
  ~ComparatorCheapestAdditionFilteredHeuristic() override {}
  std::string DebugString() const override {
    return "ComparatorCheapestAdditionFilteredHeuristic";
  }

 private:
  /// Next nodes are sorted according to the current comparator.
  void SortSuccessors(int64 node, std::vector<int64>* successors) override;
  int64 FindTopSuccessor(int64 node,
                         const std::vector<int64>& successors) override;

  Solver::VariableValueComparator comparator_;
};

/// Filter-based decision builder which builds a solution by using
/// Clarke & Wright's Savings heuristic. For each pair of nodes, the savings
/// value is the difference between the cost of two routes visiting one node
/// each and one route visiting both nodes. Routes are built sequentially, each
/// route being initialized from the pair with the best avalaible savings value
/// then extended by selecting the nodes with best savings on both ends of the
/// partial route. Cost is based on the arc cost function of the routing model
/// and cost classes are taken into account.
class SavingsFilteredHeuristic : public RoutingFilteredHeuristic {
 public:
  struct SavingsParameters {
    /// If neighbors_ratio < 1 then for each node only this ratio of its
    /// neighbors leading to the smallest arc costs are considered.
    double neighbors_ratio = 1.0;
    /// The number of neighbors considered for each node is also adapted so that
    /// the stored Savings don't use up more than max_memory_usage_bytes bytes.
    double max_memory_usage_bytes = 6e9;
    /// If add_reverse_arcs is true, the neighborhood relationships are
    /// considered symmetrically.
    bool add_reverse_arcs = false;
    /// arc_coefficient is a strictly positive parameter indicating the
    /// coefficient of the arc being considered in the Saving formula.
    double arc_coefficient = 1.0;
  };

  SavingsFilteredHeuristic(RoutingModel* model,
                           const RoutingIndexManager* manager,
                           SavingsParameters parameters,
                           LocalSearchFilterManager* filter_manager);
  ~SavingsFilteredHeuristic() override;
  bool BuildSolutionInternal() override;

 protected:
  typedef std::pair</*saving*/ int64, /*saving index*/ int64> Saving;

  template <typename S>
  class SavingsContainer;

  virtual double ExtraSavingsMemoryMultiplicativeFactor() const = 0;

  virtual void BuildRoutesFromSavings() = 0;

  /// Returns the cost class from a saving.
  int64 GetVehicleTypeFromSaving(const Saving& saving) const {
    return saving.second / size_squared_;
  }
  /// Returns the "before node" from a saving.
  int64 GetBeforeNodeFromSaving(const Saving& saving) const {
    return (saving.second % size_squared_) / Size();
  }
  /// Returns the "after node" from a saving.
  int64 GetAfterNodeFromSaving(const Saving& saving) const {
    return (saving.second % size_squared_) % Size();
  }
  /// Returns the saving value from a saving.
  int64 GetSavingValue(const Saving& saving) const { return saving.first; }

  /// Finds the best available vehicle of type "type" to start a new route to
  /// serve the arc before_node-->after_node.
  /// Since there are different vehicle classes for each vehicle type, each
  /// vehicle class having its own capacity constraints, we go through all
  /// vehicle types (in each case only studying the first available vehicle) to
  /// make sure this Saving is inserted if possible.
  /// If possible, the arc is committed to the best vehicle, and the vehicle
  /// index is returned. If this arc can't be served by any vehicle of this
  /// type, the function returns -1.
  int StartNewRouteWithBestVehicleOfType(int type, int64 before_node,
                                         int64 after_node);

  // clang-format off
  std::unique_ptr<SavingsContainer<Saving> > savings_container_;
  // clang-format on
  std::unique_ptr<VehicleTypeCurator> vehicle_type_curator_;

 private:
  /// Used when add_reverse_arcs_ is true.
  /// Given the vector of adjacency lists of a graph, adds symmetric arcs not
  /// already in the graph to the adjacencies (i.e. if n1-->n2 is present and
  /// not n2-->n1, then n1 is added to adjacency_matrix[n2].
  // clang-format off
  void AddSymmetricArcsToAdjacencyLists(
      std::vector<std::vector<int64> >* adjacency_lists);
  // clang-format on

  /// Computes saving values for node pairs (see MaxNumNeighborsPerNode()) and
  /// all vehicle types (see ComputeVehicleTypes()).
  /// The saving index attached to each saving value is an index used to
  /// store and recover the node pair to which the value is linked (cf. the
  /// index conversion methods below).
  /// The computed savings are stored and sorted using the savings_container_.
  void ComputeSavings();
  /// Builds a saving from a saving value, a vehicle type and two nodes.
  Saving BuildSaving(int64 saving, int vehicle_type, int before_node,
                     int after_node) const {
    return std::make_pair(saving, vehicle_type * size_squared_ +
                                      before_node * Size() + after_node);
  }

  /// Computes and returns the maximum number of (closest) neighbors to consider
  /// for each node when computing Savings, based on the neighbors ratio and max
  /// memory usage specified by the savings_params_.
  int64 MaxNumNeighborsPerNode(int num_vehicle_types) const;

  const RoutingIndexManager* const manager_;
  const SavingsParameters savings_params_;
  int64 size_squared_;

  friend class SavingsFilteredHeuristicTestPeer;
};

class SequentialSavingsFilteredHeuristic : public SavingsFilteredHeuristic {
 public:
  SequentialSavingsFilteredHeuristic(RoutingModel* model,
                                     const RoutingIndexManager* manager,
                                     SavingsParameters parameters,
                                     LocalSearchFilterManager* filter_manager)
      : SavingsFilteredHeuristic(model, manager, parameters, filter_manager) {}
  ~SequentialSavingsFilteredHeuristic() override{};
  std::string DebugString() const override {
    return "SequentialSavingsFilteredHeuristic";
  }

 private:
  /// Builds routes sequentially.
  /// Once a Saving is used to start a new route, we extend this route as much
  /// as possible from both ends by gradually inserting the best Saving at
  /// either end of the route.
  void BuildRoutesFromSavings() override;
  double ExtraSavingsMemoryMultiplicativeFactor() const override { return 1.0; }
};

class ParallelSavingsFilteredHeuristic : public SavingsFilteredHeuristic {
 public:
  ParallelSavingsFilteredHeuristic(RoutingModel* model,
                                   const RoutingIndexManager* manager,
                                   SavingsParameters parameters,
                                   LocalSearchFilterManager* filter_manager)
      : SavingsFilteredHeuristic(model, manager, parameters, filter_manager) {}
  ~ParallelSavingsFilteredHeuristic() override{};
  std::string DebugString() const override {
    return "ParallelSavingsFilteredHeuristic";
  }

 private:
  /// Goes through the ordered computed Savings to build routes in parallel.
  /// Given a Saving for a before-->after arc :
  /// -- If both before and after are uncontained, we start a new route.
  /// -- If only before is served and is the last node on its route, we try
  ///    adding after at the end of the route.
  /// -- If only after is served and is first on its route, we try adding before
  ///    as first node on this route.
  /// -- If both nodes are contained and are respectively the last and first
  ///    nodes on their (different) routes, we merge the routes of the two nodes
  ///    into one if possible.
  void BuildRoutesFromSavings() override;

  double ExtraSavingsMemoryMultiplicativeFactor() const override { return 2.0; }

  /// Merges the routes of first_vehicle and second_vehicle onto the vehicle
  /// with lower fixed cost. The routes respectively end at before_node and
  /// start at after_node, and are merged into one by adding the arc
  /// before_node-->after_node.
  void MergeRoutes(int first_vehicle, int second_vehicle, int64 before_node,
                   int64 after_node);

  /// First and last non start/end nodes served by each vehicle.
  std::vector<int64> first_node_on_route_;
  std::vector<int64> last_node_on_route_;
  /// For each first/last node served by a vehicle (besides start/end nodes of
  /// vehicle), this vector contains the index of the vehicle serving them.
  /// For other (intermediary) nodes, contains -1.
  std::vector<int> vehicle_of_first_or_last_node_;
};

/// Christofides addition heuristic. Initially created to solve TSPs, extended
/// to support any model by extending routes as much as possible following the
/// path found by the heuristic, before starting a new route.

class ChristofidesFilteredHeuristic : public RoutingFilteredHeuristic {
 public:
  ChristofidesFilteredHeuristic(RoutingModel* model,
                                LocalSearchFilterManager* filter_manager,
                                bool use_minimum_matching);
  ~ChristofidesFilteredHeuristic() override {}
  bool BuildSolutionInternal() override;
  std::string DebugString() const override {
    return "ChristofidesFilteredHeuristic";
  }

 private:
  const bool use_minimum_matching_;
};
#endif  // SWIG

/// Attempts to solve the model using the cp-sat solver. As of 5/2019, will
/// solve the TSP corresponding to the model if it has a single vehicle.
/// Therefore the resulting solution might not actually be feasible. Will return
/// false if a solution could not be found.
bool SolveModelWithSat(const RoutingModel& model,
                       const RoutingSearchParameters& search_parameters,
                       const Assignment* initial_solution,
                       Assignment* solution);

/// Generic path-based filter class.

class BasePathFilter : public IntVarLocalSearchFilter {
 public:
  BasePathFilter(const std::vector<IntVar*>& nexts, int next_domain_size);
  ~BasePathFilter() override {}
  bool Accept(const Assignment* delta, const Assignment* deltadelta,
              int64 objective_min, int64 objective_max) override;
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
  int Rank(int64 node) const { return ranks_[node]; }
  bool IsDisabled() const { return status_ == DISABLED; }
  const std::vector<int64>& GetTouchedPathStarts() const {
    return touched_paths_.PositionsSetAtLeastOnce();
  }
  const std::vector<int64>& GetNewSynchronizedUnperformedNodes() const {
    return new_synchronized_unperformed_nodes_.PositionsSetAtLeastOnce();
  }

 private:
  enum Status { UNKNOWN, ENABLED, DISABLED };

  virtual bool DisableFiltering() const { return false; }
  virtual void OnBeforeSynchronizePaths() {}
  virtual void OnAfterSynchronizePaths() {}
  virtual void OnSynchronizePathFromStart(int64 start) {}
  virtual void InitializeAcceptPath() {}
  virtual bool AcceptPath(int64 path_start, int64 chain_start,
                          int64 chain_end) = 0;
  virtual bool FinalizeAcceptPath(const Assignment* delta, int64 objective_min,
                                  int64 objective_max) {
    return true;
  }
  /// Detects path starts, used to track which node belongs to which path.
  void ComputePathStarts(std::vector<int64>* path_starts,
                         std::vector<int>* index_to_path);
  bool HavePathsChanged();
  void SynchronizeFullAssignment();
  void UpdateAllRanks();
  void UpdatePathRanksFromStart(int start);

  std::vector<int64> node_path_starts_;
  std::vector<int64> starts_;
  std::vector<int> paths_;
  SparseBitset<int64> new_synchronized_unperformed_nodes_;
  std::vector<int64> new_nexts_;
  std::vector<int> delta_touched_;
  SparseBitset<> touched_paths_;
  SparseBitset<> touched_path_nodes_;
  std::vector<int> ranks_;

  Status status_;
};

/// This filter accepts deltas for which the assignment satisfies the
/// constraints of the Solver. This is verified by keeping an internal copy of
/// the assignment with all Next vars and their updated values, and calling
/// RestoreAssignment() on the assignment+delta.
// TODO(user): Also call the solution finalizer on variables, with the
/// exception of Next Vars (woud fail on large instances).
/// WARNING: In the case of mandatory nodes, when all vehicles are currently
/// being used in the solution but uninserted nodes still remain, this filter
/// will reject the solution, even if the node could be inserted on one of these
/// routes, because all Next vars of vehicle starts are already instantiated.
// TODO(user): Avoid such false negatives.
class CPFeasibilityFilter : public IntVarLocalSearchFilter {
 public:
  explicit CPFeasibilityFilter(const RoutingModel* routing_model);
  ~CPFeasibilityFilter() override {}
  std::string DebugString() const override { return "CPFeasibilityFilter"; }
  bool Accept(const Assignment* delta, const Assignment* deltadelta,
              int64 objective_min, int64 objective_max) override;
  void OnSynchronize(const Assignment* delta) override;

 private:
  void AddDeltaToAssignment(const Assignment* delta, Assignment* assignment);

  static const int64 kUnassigned;
  const RoutingModel* const model_;
  Solver* const solver_;
  Assignment* const assignment_;
  Assignment* const temp_assignment_;
  DecisionBuilder* const restore_;
};

#if !defined(SWIG)
IntVarLocalSearchFilter* MakeMaxActiveVehiclesFilter(
    const RoutingModel& routing_model);
IntVarLocalSearchFilter* MakeNodeDisjunctionFilter(
    const RoutingModel& routing_model);
IntVarLocalSearchFilter* MakeVehicleAmortizedCostFilter(
    const RoutingModel& routing_model);
IntVarLocalSearchFilter* MakeTypeRegulationsFilter(
    const RoutingModel& routing_model);
void AppendDimensionCumulFilters(
    const std::vector<RoutingDimension*>& dimensions,
    const RoutingSearchParameters& parameters, bool filter_objective_cost,
    std::vector<LocalSearchFilter*>* filters);
IntVarLocalSearchFilter* MakePathCumulFilter(
    const RoutingDimension& dimension,
    const RoutingSearchParameters& parameters,
    bool propagate_own_objective_value, bool filter_objective_cost,
    bool can_use_lp = true);
IntVarLocalSearchFilter* MakeCumulBoundsPropagatorFilter(
    const RoutingDimension& dimension);
IntVarLocalSearchFilter* MakeGlobalLPCumulFilter(
    GlobalDimensionCumulOptimizer* optimizer, bool filter_objective_cost);
IntVarLocalSearchFilter* MakePickupDeliveryFilter(
    const RoutingModel& routing_model, const RoutingModel::IndexPairs& pairs,
    const std::vector<RoutingModel::PickupAndDeliveryPolicy>& vehicle_policies);
IntVarLocalSearchFilter* MakeVehicleVarFilter(
    const RoutingModel& routing_model);
IntVarLocalSearchFilter* MakeVehicleBreaksFilter(
    const RoutingModel& routing_model, const RoutingDimension& dimension);
IntVarLocalSearchFilter* MakeCPFeasibilityFilter(
    const RoutingModel* routing_model);
#endif

}  // namespace operations_research
#endif  // OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_H_
