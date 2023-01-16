// Copyright 2010-2022 Google LLC
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
///     int64_t MyDistance(int64_t from, int64_t to) {
///       return from + to;
///     }
///
/// - Create a routing model for a given problem size (int number of nodes) and
///   number of routes (here, 1):
///
///     RoutingIndexManager manager(...number of nodes..., 1);
///     RoutingModel routing(manager);
///
/// - Set the cost function by registering an std::function<int64_t(int64_t,
/// int64_t)> in the model and passing its index as the vehicle cost.
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
///    for (int64_t node = routing.Start(route_number);
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

#include <algorithm>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/inlined_vector.h"
#include "absl/time/time.h"
#include "ortools/base/int_type.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"
#include "ortools/base/strong_vector.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "ortools/constraint_solver/routing_enums.pb.h"
#include "ortools/constraint_solver/routing_index_manager.h"
#include "ortools/constraint_solver/routing_parameters.pb.h"
#include "ortools/constraint_solver/routing_types.h"
#include "ortools/graph/graph.h"
#include "ortools/sat/theta_tree.h"
#include "ortools/util/piecewise_linear_function.h"
#include "ortools/util/range_query_function.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {

class GlobalDimensionCumulOptimizer;
class LocalDimensionCumulOptimizer;
class LocalSearchPhaseParameters;
#ifndef SWIG
class IndexNeighborFinder;
class IntVarFilteredDecisionBuilder;
#endif
class RoutingDimension;
#ifndef SWIG
using util::ReverseArcListGraph;
class SweepArranger;
#endif

class PathsMetadata {
 public:
  explicit PathsMetadata(const RoutingIndexManager& manager) {
    const int num_indices = manager.num_indices();
    const int num_paths = manager.num_vehicles();
    path_of_node_.resize(num_indices, -1);
    is_start_.resize(num_indices, false);
    is_end_.resize(num_indices, false);
    start_of_path_.resize(num_paths);
    end_of_path_.resize(num_paths);
    for (int v = 0; v < num_paths; ++v) {
      const int64_t start = manager.GetStartIndex(v);
      start_of_path_[v] = start;
      path_of_node_[start] = v;
      is_start_[start] = true;
      const int64_t end = manager.GetEndIndex(v);
      end_of_path_[v] = end;
      path_of_node_[end] = v;
      is_end_[end] = true;
    }
  }

  bool IsStart(int64_t node) const { return is_start_[node]; }
  bool IsEnd(int64_t node) const { return is_end_[node]; }
  int GetPath(int64_t start_or_end_node) const {
    return path_of_node_[start_or_end_node];
  }
  const std::vector<int64_t>& Starts() const { return start_of_path_; }
  const std::vector<int64_t>& Ends() const { return end_of_path_; }

 private:
  std::vector<bool> is_start_;
  std::vector<bool> is_end_;
  std::vector<int64_t> start_of_path_;
  std::vector<int64_t> end_of_path_;
  std::vector<int64_t> path_of_node_;
};

class RoutingModel {
 public:
  /// Status of the search.
  enum Status {
    /// Problem not solved yet (before calling RoutingModel::Solve()).
    ROUTING_NOT_SOLVED,
    /// Problem solved successfully after calling RoutingModel::Solve().
    ROUTING_SUCCESS,
    /// Problem solved successfully after calling RoutingModel::Solve(), except
    /// that a local optimum has not been reached. Leaving more time would allow
    /// improving the solution.
    ROUTING_PARTIAL_SUCCESS_LOCAL_OPTIMUM_NOT_REACHED,
    /// No solution found to the problem after calling RoutingModel::Solve().
    ROUTING_FAIL,
    /// Time limit reached before finding a solution with RoutingModel::Solve().
    ROUTING_FAIL_TIMEOUT,
    /// Model, model parameters or flags are not valid.
    ROUTING_INVALID,
    /// Problem proven to be infeasible.
    ROUTING_INFEASIBLE
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
  /// Such transits, say from node A to node B, are functions f:
  /// int64_t->int64_t of the cumuls of a dimension. The user is free to
  /// implement the abstract RangeIntToIntFunction interface, but it is expected
  /// that the implementation of each method is quite fast. For
  /// performance-related reasons, StateDependentTransit keeps an additional
  /// pointer to a RangeMinMaxIndexFunction, with similar functionality to
  /// RangeIntToIntFunction, for g(x) = f(x)+x, where f is the transit from A to
  /// B. In most situations the best solutions are problem-specific, but in case
  /// of doubt the user may use the MakeStateDependentTransit function from the
  /// routing library, which works out-of-the-box, with very good running time,
  /// but memory inefficient in some situations.
  struct StateDependentTransit {
    RangeIntToIntFunction* transit;                   /// f(x)
    RangeMinMaxIndexFunction* transit_plus_identity;  /// g(x) = f(x) + x
  };
  typedef std::function<StateDependentTransit(int64_t, int64_t)>
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
      int64_t transit_evaluator_class;
      int64_t cost_coefficient;
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
    int64_t fixed_cost;
    /// Whether or not the vehicle is used when empty.
    bool used_when_empty;
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
    absl::StrongVector<DimensionIndex, int64_t> dimension_start_cumuls_min;
    absl::StrongVector<DimensionIndex, int64_t> dimension_start_cumuls_max;
    absl::StrongVector<DimensionIndex, int64_t> dimension_end_cumuls_min;
    absl::StrongVector<DimensionIndex, int64_t> dimension_end_cumuls_max;
    absl::StrongVector<DimensionIndex, int64_t> dimension_capacities;
    /// dimension_evaluators[d]->Run(from, to) is the transit value of arc
    /// from->to for a dimension d.
    absl::StrongVector<DimensionIndex, int64_t> dimension_evaluator_classes;
    /// Fingerprint of unvisitable non-start/end nodes.
    uint64_t unvisitable_nodes_fprint;
    /// Sorted set of resource groups for which the vehicle requires a resource.
    std::vector<int> required_resource_group_indices;

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
      int64_t fixed_cost;

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

  /// A ResourceGroup defines a set of available Resources with attributes on
  /// one or multiple dimensions.
  /// For every ResourceGroup in the model, each (used) vehicle in the solution
  /// which requires a resource (see NotifyVehicleRequiresResource()) from this
  /// group must be assigned to exactly 1 resource, and each resource can in
  /// turn be assigned to at most 1 vehicle requiring it. This
  /// vehicle-to-resource assignment will apply the corresponding Attributes to
  /// the dimensions affected by the resource group. NOTE: As of 2021/07, each
  /// ResourceGroup can only affect a single RoutingDimension at a time, i.e.
  /// all Resources in a group must apply attributes to the same single
  /// dimension.
  class ResourceGroup {
   public:
    /// Attributes for a dimension.
    class Attributes {
     public:
      Attributes();
      Attributes(Domain start_domain, Domain end_domain);

      const Domain& start_domain() const { return start_domain_; }
      const Domain& end_domain() const { return end_domain_; }

     private:
      /// The following domains constrain the dimension start/end cumul of the
      /// the vehicle assigned to this resource:
      /// start_domain_.Min() <= cumul[Start(v)] <= start_domain_.Max()
      Domain start_domain_;
      /// end_domain_.Min() <= cumul[End(v)] <= end_domain_.Max()
      Domain end_domain_;
    };

    /// A Resource sets attributes (costs/constraints) for a set of dimensions.
    class Resource {
     public:
      const ResourceGroup::Attributes& GetDimensionAttributes(
          const RoutingDimension* dimension) const;

     private:
      explicit Resource(const RoutingModel* model) : model_(model) {}

      void SetDimensionAttributes(ResourceGroup::Attributes attributes,
                                  const RoutingDimension* dimension);
      const ResourceGroup::Attributes& GetDefaultAttributes() const;

      const RoutingModel* const model_;
      absl::flat_hash_map<DimensionIndex, ResourceGroup::Attributes>
          dimension_attributes_;

      friend class ResourceGroup;
    };

    explicit ResourceGroup(const RoutingModel* model)
        : model_(model), vehicle_requires_resource_(model->vehicles(), false) {}

    /// Adds a Resource with the given attributes for the corresponding
    /// dimension. Returns the index of the added resource in resources_.
    int AddResource(Attributes attributes, const RoutingDimension* dimension);

    /// Notifies that the given vehicle index requires a resource from this
    /// group if the vehicle is used (i.e. if its route is non-empty or
    /// vehicle_used_when_empty_[vehicle] is true).
    void NotifyVehicleRequiresAResource(int vehicle);

    const std::vector<int>& GetVehiclesRequiringAResource() const {
      return vehicles_requiring_resource_;
    }

    bool VehicleRequiresAResource(int vehicle) const {
      return vehicle_requires_resource_[vehicle];
    }

    const std::vector<Resource>& GetResources() const { return resources_; }
    const Resource& GetResource(int resource_index) const {
      DCHECK_LT(resource_index, resources_.size());
      return resources_[resource_index];
    }
    const absl::flat_hash_set<DimensionIndex>& GetAffectedDimensionIndices()
        const {
      return affected_dimension_indices_;
    }
    int Size() const { return resources_.size(); }

   private:
    const RoutingModel* const model_;
    std::vector<Resource> resources_;
    std::vector<bool> vehicle_requires_resource_;
    std::vector<int> vehicles_requiring_resource_;
    /// All indices of dimensions affected by this resource group.
    absl::flat_hash_set<DimensionIndex> affected_dimension_indices_;
  };

  /// Constant used to express a hard constraint instead of a soft penalty.
  static const int64_t kNoPenalty;

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
  int RegisterUnaryTransitVector(std::vector<int64_t> values);
  int RegisterUnaryTransitCallback(TransitCallback1 callback);
  int RegisterPositiveUnaryTransitCallback(TransitCallback1 callback);

  int RegisterTransitMatrix(
      std::vector<std::vector<int64_t> /*needed_for_swig*/> values);
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
  /// if j == next(i), cumul(j) = cumul(i) + transit(i, j) + slack(i)
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
  bool AddDimension(int evaluator_index, int64_t slack_max, int64_t capacity,
                    bool fix_start_cumul_to_zero, const std::string& name);
  bool AddDimensionWithVehicleTransits(
      const std::vector<int>& evaluator_indices, int64_t slack_max,
      int64_t capacity, bool fix_start_cumul_to_zero, const std::string& name);
  bool AddDimensionWithVehicleCapacity(int evaluator_index, int64_t slack_max,
                                       std::vector<int64_t> vehicle_capacities,
                                       bool fix_start_cumul_to_zero,
                                       const std::string& name);
  bool AddDimensionWithVehicleTransitAndCapacity(
      const std::vector<int>& evaluator_indices, int64_t slack_max,
      std::vector<int64_t> vehicle_capacities, bool fix_start_cumul_to_zero,
      const std::string& name);
  /// Creates a dimension where the transit variable is constrained to be
  /// equal to 'value'; 'capacity' is the upper bound of the cumul variables.
  /// 'name' is the name used to reference the dimension; this name is used to
  /// get cumul and transit variables from the routing model.
  /// Returns a pair consisting of an index to the registered unary transit
  /// callback and a bool denoting whether the dimension has been created.
  /// It is false if a dimension with the same name has already been created
  /// (and doesn't create the new dimension but still register a new callback).
  std::pair<int, bool> AddConstantDimensionWithSlack(
      int64_t value, int64_t capacity, int64_t slack_max,
      bool fix_start_cumul_to_zero, const std::string& name);
  std::pair<int, bool> AddConstantDimension(int64_t value, int64_t capacity,
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
  /// Returns a pair consisting of an index to the registered unary transit
  /// callback and a bool denoting whether the dimension has been created.
  /// It is false if a dimension with the same name has already been created
  /// (and doesn't create the new dimension but still register a new callback).
  std::pair<int, bool> AddVectorDimension(std::vector<int64_t> values,
                                          int64_t capacity,
                                          bool fix_start_cumul_to_zero,
                                          const std::string& name);
  /// Creates a dimension where the transit variable is constrained to be
  /// equal to 'values[i][next(i)]' for node i; 'capacity' is the upper bound of
  /// the cumul variables. 'name' is the name used to reference the dimension;
  /// this name is used to get cumul and transit variables from the routing
  /// model.
  /// Returns a pair consisting of an index to the registered transit callback
  /// and a bool denoting whether the dimension has been created.
  /// It is false if a dimension with the same name has already been created
  /// (and doesn't create the new dimension but still register a new callback).
  std::pair<int, bool> AddMatrixDimension(
      std::vector<std::vector<int64_t> /*needed_for_swig*/> values,
      int64_t capacity, bool fix_start_cumul_to_zero, const std::string& name);
  /// Creates a dimension with transits depending on the cumuls of another
  /// dimension. 'pure_transits' are the per-vehicle fixed transits as above.
  /// 'dependent_transits' is a vector containing for each vehicle an index to a
  /// registered state dependent transit callback. 'base_dimension' indicates
  /// the dimension from which the cumul variable is taken. If 'base_dimension'
  /// is nullptr, then the newly created dimension is self-based.
  bool AddDimensionDependentDimensionWithVehicleCapacity(
      const std::vector<int>& pure_transits,
      const std::vector<int>& dependent_transits,
      const RoutingDimension* base_dimension, int64_t slack_max,
      std::vector<int64_t> vehicle_capacities, bool fix_start_cumul_to_zero,
      const std::string& name) {
    return AddDimensionDependentDimensionWithVehicleCapacityInternal(
        pure_transits, dependent_transits, base_dimension, slack_max,
        std::move(vehicle_capacities), fix_start_cumul_to_zero, name);
  }

  /// As above, but pure_transits are taken to be zero evaluators.
  bool AddDimensionDependentDimensionWithVehicleCapacity(
      const std::vector<int>& transits, const RoutingDimension* base_dimension,
      int64_t slack_max, std::vector<int64_t> vehicle_capacities,
      bool fix_start_cumul_to_zero, const std::string& name);
  /// Homogeneous versions of the functions above.
  bool AddDimensionDependentDimensionWithVehicleCapacity(
      int transit, const RoutingDimension* base_dimension, int64_t slack_max,
      int64_t vehicle_capacity, bool fix_start_cumul_to_zero,
      const std::string& name);
  bool AddDimensionDependentDimensionWithVehicleCapacity(
      int pure_transit, int dependent_transit,
      const RoutingDimension* base_dimension, int64_t slack_max,
      int64_t vehicle_capacity, bool fix_start_cumul_to_zero,
      const std::string& name);

  /// Creates a cached StateDependentTransit from an std::function.
  static RoutingModel::StateDependentTransit MakeStateDependentTransit(
      const std::function<int64_t(int64_t)>& f, int64_t domain_start,
      int64_t domain_end);

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

  /// Returns the dimensions which have [global|local]_dimension_optimizers_.
  std::vector<const RoutingDimension*> GetDimensionsWithGlobalCumulOptimizers()
      const;
  std::vector<const RoutingDimension*> GetDimensionsWithLocalCumulOptimizers()
      const;

  /// Returns whether the given dimension has global/local cumul optimizers.
  bool HasGlobalCumulOptimizer(const RoutingDimension& dimension) const {
    return GetGlobalCumulOptimizerIndex(dimension) >= 0;
  }
  bool HasLocalCumulOptimizer(const RoutingDimension& dimension) const {
    return GetLocalCumulOptimizerIndex(dimension) >= 0;
  }
  /// Returns the global/local dimension cumul optimizer for a given dimension,
  /// or nullptr if there is none.
  GlobalDimensionCumulOptimizer* GetMutableGlobalCumulLPOptimizer(
      const RoutingDimension& dimension) const;
  GlobalDimensionCumulOptimizer* GetMutableGlobalCumulMPOptimizer(
      const RoutingDimension& dimension) const;
  LocalDimensionCumulOptimizer* GetMutableLocalCumulLPOptimizer(
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

  /// Adds a resource group to the routing model. Returns its index in
  /// resource_groups_.
  int AddResourceGroup();
  // clang-format off
  const std::vector<std::unique_ptr<ResourceGroup> >& GetResourceGroups()
      const {
    return resource_groups_;
  }
  // clang-format on
  ResourceGroup* GetResourceGroup(int rg_index) const {
    DCHECK_LT(rg_index, resource_groups_.size());
    return resource_groups_[rg_index].get();
  }

  /// Returns the indices of resource groups for this dimension. This method can
  /// only be called after the model has been closed.
  const std::vector<int>& GetDimensionResourceGroupIndices(
      const RoutingDimension* dimension) const;

  /// Returns the index of the resource group attached to the dimension.
  /// DCHECKS that there's exactly one resource group for this dimension.
  int GetDimensionResourceGroupIndex(const RoutingDimension* dimension) const {
    DCHECK_EQ(GetDimensionResourceGroupIndices(dimension).size(), 1);
    return GetDimensionResourceGroupIndices(dimension)[0];
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
  DisjunctionIndex AddDisjunction(const std::vector<int64_t>& indices,
                                  int64_t penalty = kNoPenalty,
                                  int64_t max_cardinality = 1);
  /// Returns the indices of the disjunctions to which an index belongs.
  const std::vector<DisjunctionIndex>& GetDisjunctionIndices(
      int64_t index) const {
    return index_to_disjunctions_[index];
  }
  /// Calls f for each variable index of indices in the same disjunctions as the
  /// node corresponding to the variable index 'index'; only disjunctions of
  /// cardinality 'cardinality' are considered.
  template <typename F>
  void ForEachNodeInDisjunctionWithMaxCardinalityFromIndex(
      int64_t index, int64_t max_cardinality, F f) const {
    for (const DisjunctionIndex disjunction : GetDisjunctionIndices(index)) {
      if (disjunctions_[disjunction].value.max_cardinality == max_cardinality) {
        for (const int64_t d_index : disjunctions_[disjunction].indices) {
          f(d_index);
        }
      }
    }
  }
#if !defined(SWIGPYTHON)
  /// Returns the variable indices of the nodes in the disjunction of index
  /// 'index'.
  const std::vector<int64_t>& GetDisjunctionNodeIndices(
      DisjunctionIndex index) const {
    return disjunctions_[index].indices;
  }
#endif  // !defined(SWIGPYTHON)
  /// Returns the penalty of the node disjunction of index 'index'.
  int64_t GetDisjunctionPenalty(DisjunctionIndex index) const {
    return disjunctions_[index].value.penalty;
  }
  /// Returns the maximum number of possible active nodes of the node
  /// disjunction of index 'index'.
  int64_t GetDisjunctionMaxCardinality(DisjunctionIndex index) const {
    return disjunctions_[index].value.max_cardinality;
  }
  /// Returns the number of node disjunctions in the model.
  int GetNumberOfDisjunctions() const { return disjunctions_.size(); }
  /// Returns true if the model contains mandatory disjunctions (ones with
  /// kNoPenalty as penalty).
  bool HasMandatoryDisjunctions() const;
  /// Returns true if the model contains at least one disjunction which is
  /// constrained by its max_cardinality.
  bool HasMaxCardinalityConstrainedDisjunctions() const;
  /// Returns the list of all perfect binary disjunctions, as pairs of variable
  /// indices: a disjunction is "perfect" when its variables do not appear in
  /// any other disjunction. Each pair is sorted (lowest variable index first),
  /// and the output vector is also sorted (lowest pairs first).
  std::vector<std::pair<int64_t, int64_t>> GetPerfectBinaryDisjunctions() const;
  /// SPECIAL: Makes the solver ignore all the disjunctions whose active
  /// variables are all trivially zero (i.e. Max() == 0), by setting their
  /// max_cardinality to 0.
  /// This can be useful when using the BaseBinaryDisjunctionNeighborhood
  /// operators, in the context of arc-based routing.
  void IgnoreDisjunctionsAlreadyForcedToZero();

  /// Adds a soft constraint to force a set of variable indices to be on the
  /// same vehicle. If all nodes are not on the same vehicle, each extra vehicle
  /// used adds 'cost' to the cost function.
  void AddSoftSameVehicleConstraint(const std::vector<int64_t>& indices,
                                    int64_t cost);

  /// Sets the vehicles which can visit a given node. If the node is in a
  /// disjunction, this will not prevent it from being unperformed.
  /// Specifying an empty vector of vehicles has no effect (all vehicles
  /// will be allowed to visit the node).
  void SetAllowedVehiclesForIndex(const std::vector<int>& vehicles,
                                  int64_t index);

  /// Returns true if a vehicle is allowed to visit a given node.
  bool IsVehicleAllowedForIndex(int vehicle, int64_t index) {
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
  ///     int64_t index1 = manager.NodeToIndex(node1);
  ///     int64_t index2 = manager.NodeToIndex(node2);
  ///     solver->AddConstraint(solver->MakeEquality(
  ///         routing.VehicleVar(index1),
  ///         routing.VehicleVar(index2)));
  ///     routing.AddPickupAndDelivery(index1, index2);
  ///
  // TODO(user): Remove this when model introspection detects linked nodes.
  void AddPickupAndDelivery(int64_t pickup, int64_t delivery);
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
  GetPickupIndexPairs(int64_t node_index) const;
  /// Same as above for deliveries.
  const std::vector<std::pair<int, int> >&
      GetDeliveryIndexPairs(int64_t node_index) const;
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
  /// Returns implicit pickup and delivery pairs currently in the model.
  /// Pairs are implicit if they are not linked by a pickup and delivery
  /// constraint but that for a given unary dimension, the first element of the
  /// pair has a positive demand d, and the second element has a demand of -d.
  const IndexPairs& GetImplicitUniquePickupAndDeliveryPairs() const {
    DCHECK(closed_);
    return implicit_pickup_delivery_pairs_without_alternatives_;
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
  void SetVisitType(int64_t index, int type, VisitTypePolicy type_policy);
  int GetVisitType(int64_t index) const;
  const std::vector<int>& GetSingleNodesOfType(int type) const;
  const std::vector<int>& GetPairIndicesOfType(int type) const;
  VisitTypePolicy GetVisitTypePolicy(int64_t index) const;
  /// This function should be called once all node visit types have been set and
  /// prior to adding any incompatibilities/requirements.
  // TODO(user): Reconsider the logic and potentially remove the need to
  /// "close" types.
  void CloseVisitTypes();
  int GetNumberOfVisitTypes() const { return num_visit_types_; }
#ifndef SWIG
  const std::vector<std::vector<int>>& GetTopologicallySortedVisitTypes()
      const {
    DCHECK(closed_);
    return topologically_sorted_visit_types_;
  }
#endif  // SWIG
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
  /// node is only part of a single Disjunction, and that disjunction has a
  /// penalty. For forced active nodes returns max int64_t. In all other cases,
  /// this returns 0.
  int64_t UnperformedPenalty(int64_t var_index) const;
  /// Same as above except that it returns default_value instead of 0 when
  /// penalty is not well defined (default value is passed as first argument to
  /// simplify the usage of the method in a callback).
  int64_t UnperformedPenaltyOrValue(int64_t default_value,
                                    int64_t var_index) const;
  /// Returns the variable index of the first starting or ending node of all
  /// routes. If all routes start  and end at the same node (single depot), this
  /// is the node returned.
  int64_t GetDepot() const;

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
  void SetFixedCostOfAllVehicles(int64_t cost);
  /// Sets the fixed cost of one vehicle route.
  void SetFixedCostOfVehicle(int64_t cost, int vehicle);
  /// Returns the route fixed cost taken into account if the route of the
  /// vehicle is not empty, aka there's at least one node on the route other
  /// than the first and last nodes.
  int64_t GetFixedCostOfVehicle(int vehicle) const;

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
  void SetAmortizedCostFactorsOfAllVehicles(int64_t linear_cost_factor,
                                            int64_t quadratic_cost_factor);
  /// Sets the linear and quadratic cost factor of the given vehicle.
  void SetAmortizedCostFactorsOfVehicle(int64_t linear_cost_factor,
                                        int64_t quadratic_cost_factor,
                                        int vehicle);

  const std::vector<int64_t>& GetAmortizedLinearCostFactorOfVehicles() const {
    return linear_cost_factor_of_vehicle_;
  }
  const std::vector<int64_t>& GetAmortizedQuadraticCostFactorOfVehicles()
      const {
    return quadratic_cost_factor_of_vehicle_;
  }

  void SetVehicleUsedWhenEmpty(bool is_used, int vehicle) {
    DCHECK_LT(vehicle, vehicles_);
    vehicle_used_when_empty_[vehicle] = is_used;
  }

  bool IsVehicleUsedWhenEmpty(int vehicle) const {
    DCHECK_LT(vehicle, vehicles_);
    return vehicle_used_when_empty_[vehicle];
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
  void AddWeightedVariableMinimizedByFinalizer(IntVar* var, int64_t cost);
  /// Adds a variable to maximize in the solution finalizer, with a weighted
  /// priority: the higher the more priority it has.
  void AddWeightedVariableMaximizedByFinalizer(IntVar* var, int64_t cost);
  /// Add a variable to set the closest possible to the target value in the
  /// solution finalizer.
  void AddVariableTargetToFinalizer(IntVar* var, int64_t target);
  /// Same as above with a weighted priority: the higher the cost, the more
  /// priority it has to be set close to the target value.
  void AddWeightedVariableTargetToFinalizer(IntVar* var, int64_t target,
                                            int64_t cost);
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
  /// Same as above, except that if assignment is not null, it will be used as
  /// the initial solution.
  const Assignment* SolveFromAssignmentWithParameters(
      const Assignment* assignment,
      const RoutingSearchParameters& search_parameters,
      std::vector<const Assignment*>* solutions = nullptr);
  /// Same as above but will try all assignments in order as first solutions
  /// until one succeeds.
  const Assignment* SolveFromAssignmentsWithParameters(
      const std::vector<const Assignment*>& assignments,
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
  int64_t ComputeLowerBound();
  /// Returns the current status of the routing model.
  Status status() const { return status_; }
  /// Returns the value of the internal enable_deep_serialization_ parameter.
  bool enable_deep_serialization() const { return enable_deep_serialization_; }
  /// Applies a lock chain to the next search. 'locks' represents an ordered
  /// vector of nodes representing a partial route which will be fixed during
  /// the next search; it will constrain next variables such that:
  /// next[locks[i]] == locks[i+1].
  ///
  /// Returns the next variable at the end of the locked chain; this variable is
  /// not locked. An assignment containing the locks can be obtained by calling
  /// PreAssignment().
  IntVar* ApplyLocks(const std::vector<int64_t>& locks);
  /// Applies lock chains to all vehicles to the next search, such that locks[p]
  /// is the lock chain for route p. Returns false if the locks do not contain
  /// valid routes; expects that the routes do not contain the depots,
  /// i.e. there are empty vectors in place of empty routes.
  /// If close_routes is set to true, adds the end nodes to the route of each
  /// vehicle and deactivates other nodes.
  /// An assignment containing the locks can be obtained by calling
  /// PreAssignment().
  bool ApplyLocksToAllVehicles(const std::vector<std::vector<int64_t>>& locks,
                               bool close_routes);
  /// Returns an assignment used to fix some of the variables of the problem.
  /// In practice, this assignment locks partial routes of the problem. This
  /// can be used in the context of locking the parts of the routes which have
  /// already been driven in online routing problems.
  const Assignment* PreAssignment() const { return preassignment_; }
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
      const std::vector<std::vector<int64_t>>& routes,
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
  bool RoutesToAssignment(const std::vector<std::vector<int64_t>>& routes,
                          bool ignore_inactive_indices, bool close_routes,
                          Assignment* const assignment) const;
  /// Converts the solution in the given assignment to routes for all vehicles.
  /// Expects that assignment contains a valid solution (i.e. routes for all
  /// vehicles end with an end index for that vehicle).
  void AssignmentToRoutes(
      const Assignment& assignment,
      std::vector<std::vector<int64_t>>* const routes) const;
  /// Converts the solution in the given assignment to routes for all vehicles.
  /// If the returned vector is route_indices, route_indices[i][j] is the index
  /// for jth location visited on route i. Note that contrary to
  /// AssignmentToRoutes, the vectors do include start and end locations.
#ifndef SWIG
  std::vector<std::vector<int64_t>> GetRoutesFromAssignment(
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
      const Assignment* original_assignment, absl::Duration duration_limit,
      bool* time_limit_was_reached = nullptr);
  /// Contains the information needed by the solver to optimize a dimension's
  /// cumuls with travel-start dependent transit values.
  struct RouteDimensionTravelInfo {
    /// Contains the information for a single transition on the route.
    struct TransitionInfo {
      /// The following struct defines a piecewise linear formulation, with
      /// int64_t values for the "anchor" x and y values, and potential double
      /// values for the slope of each linear function.
      // TODO(user): Adjust the inlined vector sizes based on experiments.
      struct PiecewiseLinearFormulation {
        /// The set of *increasing* anchor cumul values for the interpolation.
        absl::InlinedVector<int64_t, 8> x_anchors;
        /// The y values used for the interpolation:
        /// For any x anchor value, let i be an index such that
        /// x_anchors[i] ≤ x < x_anchors[i+1], then the y value for x is
        /// y_anchors[i] * (1-λ) + y_anchors[i+1] * λ, with
        /// λ = (x - x_anchors[i]) / (x_anchors[i+1] - x_anchors[i]).
        absl::InlinedVector<int64_t, 8> y_anchors;

        std::string DebugString(std::string line_prefix = "") const;
      };

      /// Models the (real) travel value Tᵣ, for this transition based on the
      /// departure value of the travel.
      PiecewiseLinearFormulation travel_start_dependent_travel;

      /// travel_compression_cost models the cost of the difference between the
      /// (real) travel value Tᵣ given by travel_start_dependent_travel and the
      /// compressed travel value considered in the scheduling.
      PiecewiseLinearFormulation travel_compression_cost;

      /// The parts of the transit which occur pre/post travel between the
      /// nodes. The total transit between the two nodes i and j is
      /// = pre_travel_transit_value + travel(i, j) + post_travel_transit_value.
      int64_t pre_travel_transit_value;
      int64_t post_travel_transit_value;

      /// The hard lower bound of the compressed travel value that will be
      /// enforced by the scheduling module.
      int64_t compressed_travel_value_lower_bound;

      /// The hard upper bound of the (real) travel value Tᵣ (see
      /// above). This value should be chosen so as to prevent
      /// the overall cost of the model
      /// (dimension costs + travel_compression_cost) to overflow.
      int64_t travel_value_upper_bound;

      std::string DebugString(std::string line_prefix = "") const;
    };

    /// For each node #i on the route, transition_info[i] contains the relevant
    /// information for the travel between nodes #i and #(i + 1) on the route.
    std::vector<TransitionInfo> transition_info;
    /// The cost per unit of travel for this vehicle.
    int64_t travel_cost_coefficient;

    std::string DebugString(std::string line_prefix = "") const;
  };

#ifndef SWIG
  // TODO(user): Revisit if coordinates are added to the RoutingModel class.
  void SetSweepArranger(SweepArranger* sweep_arranger);
  /// Returns the sweep arranger to be used by routing heuristics.
  SweepArranger* sweep_arranger() const;
#endif
  class NodeNeighborsByCostClass {
   public:
    NodeNeighborsByCostClass() = default;

    /// Computes num_neighbors neighbors of all nodes for every cost class in
    /// routing_model.
    void ComputeNeighbors(const RoutingModel& routing_model, int num_neighbors);
    /// Returns the neighbors of the given node for the given cost_class.
    const std::vector<int>& GetNeighborsOfNodeForCostClass(
        int cost_class, int node_index) const {
      return all_nodes_.empty() ? node_index_to_neighbors_by_cost_class_
                                      [node_index][cost_class]
                                          ->PositionsSetAtLeastOnce()
                                : all_nodes_;
    }

   private:
    std::vector<std::vector<std::unique_ptr<SparseBitset<int>>>>
        node_index_to_neighbors_by_cost_class_;
    std::vector<int> all_nodes_;
  };

  /// Returns num_neighbors neighbors of all nodes for every cost class. The
  /// result is cached and is computed once.
  const NodeNeighborsByCostClass* GetOrCreateNodeNeighborsByCostClass(
      int num_neighbors);
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
    extra_filters_.push_back({filter, LocalSearchFilterManager::kRelax});
    extra_filters_.push_back({filter, LocalSearchFilterManager::kAccept});
  }

  /// Model inspection.
  /// Returns the variable index of the starting node of a vehicle route.
  int64_t Start(int vehicle) const { return paths_metadata_.Starts()[vehicle]; }
  /// Returns the variable index of the ending node of a vehicle route.
  int64_t End(int vehicle) const { return paths_metadata_.Ends()[vehicle]; }
  /// Returns true if 'index' represents the first node of a route.
  bool IsStart(int64_t index) const { return paths_metadata_.IsStart(index); }
  /// Returns true if 'index' represents the last node of a route.
  bool IsEnd(int64_t index) const { return paths_metadata_.IsEnd(index); }
  /// Returns the vehicle of the given start/end index, and -1 if the given
  /// index is not a vehicle start/end.
  int VehicleIndex(int64_t index) const {
    return paths_metadata_.GetPath(index);
  }
  /// Assignment inspection
  /// Returns the variable index of the node directly after the node
  /// corresponding to 'index' in 'assignment'.
  int64_t Next(const Assignment& assignment, int64_t index) const;
  /// Returns true if the route of 'vehicle' is non empty in 'assignment'.
  bool IsVehicleUsed(const Assignment& assignment, int vehicle) const;

#if !defined(SWIGPYTHON)
  /// Returns all next variables of the model, such that Nexts(i) is the next
  /// variable of the node corresponding to i.
  const std::vector<IntVar*>& Nexts() const { return nexts_; }
  /// Returns all vehicle variables of the model,  such that VehicleVars(i) is
  /// the vehicle variable of the node corresponding to i.
  const std::vector<IntVar*>& VehicleVars() const { return vehicle_vars_; }
  /// Returns vehicle resource variables for a given resource group, such that
  /// ResourceVars(r_g)[v] is the resource variable for vehicle 'v' in resource
  /// group 'r_g'.
  const std::vector<IntVar*>& ResourceVars(int resource_group) const {
    return resource_vars_[resource_group];
  }
#endif  /// !defined(SWIGPYTHON)
  /// Returns the next variable of the node corresponding to index. Note that
  /// NextVar(index) == index is equivalent to ActiveVar(index) == 0.
  IntVar* NextVar(int64_t index) const { return nexts_[index]; }
  /// Returns the active variable of the node corresponding to index.
  IntVar* ActiveVar(int64_t index) const { return active_[index]; }
  /// Returns the active variable of the vehicle. It will be equal to 1 iff the
  /// route of the vehicle is not empty, 0 otherwise.
  IntVar* ActiveVehicleVar(int vehicle) const {
    return vehicle_active_[vehicle];
  }
  /// Returns the variable specifying whether or not the given vehicle route is
  /// considered for costs and constraints. It will be equal to 1 iff the route
  /// of the vehicle is not empty OR vehicle_used_when_empty_[vehicle] is true.
  IntVar* VehicleRouteConsideredVar(int vehicle) const {
    return vehicle_route_considered_[vehicle];
  }
  /// Returns the vehicle variable of the node corresponding to index. Note that
  /// VehicleVar(index) == -1 is equivalent to ActiveVar(index) == 0.
  IntVar* VehicleVar(int64_t index) const { return vehicle_vars_[index]; }
  /// Returns the resource variable for the given vehicle index in the given
  /// resource group. If a vehicle doesn't require a resource from the
  /// corresponding resource group, then ResourceVar(v, r_g) == -1.
  IntVar* ResourceVar(int vehicle, int resource_group) const {
    DCHECK_LT(resource_group, resource_vars_.size());
    DCHECK_LT(vehicle, resource_vars_[resource_group].size());
    return resource_vars_[resource_group][vehicle];
  }
  /// Returns the global cost variable which is being minimized.
  IntVar* CostVar() const { return cost_; }

  /// Returns the cost of the transit arc between two nodes for a given vehicle.
  /// Input are variable indices of node. This returns 0 if vehicle < 0.
  int64_t GetArcCostForVehicle(int64_t from_index, int64_t to_index,
                               int64_t vehicle) const;
  /// Whether costs are homogeneous across all vehicles.
  bool CostsAreHomogeneousAcrossVehicles() const {
    return costs_are_homogeneous_across_vehicles_;
  }
  /// Returns the cost of the segment between two nodes supposing all vehicle
  /// costs are the same (returns the cost for the first vehicle otherwise).
  int64_t GetHomogeneousCost(int64_t from_index, int64_t to_index) const {
    return GetArcCostForVehicle(from_index, to_index, /*vehicle=*/0);
  }
  /// Returns the cost of the arc in the context of the first solution strategy.
  /// This is typically a simplification of the actual cost; see the .cc.
  int64_t GetArcCostForFirstSolution(int64_t from_index,
                                     int64_t to_index) const;
  /// Returns the cost of the segment between two nodes for a given cost
  /// class. Input are variable indices of nodes and the cost class.
  /// Unlike GetArcCostForVehicle(), if cost_class is kNoCost, then the
  /// returned cost won't necessarily be zero: only some of the components
  /// of the cost that depend on the cost class will be omited. See the code
  /// for details.
  int64_t GetArcCostForClass(int64_t from_index, int64_t to_index,
                             int64_t /*CostClassIndex*/ cost_class_index) const;
  /// Get the cost class index of the given vehicle.
  CostClassIndex GetCostClassIndexOfVehicle(int64_t vehicle) const {
    DCHECK(closed_);
    DCHECK_GE(vehicle, 0);
    DCHECK_LT(vehicle, cost_class_index_of_vehicle_.size());
    DCHECK_GE(cost_class_index_of_vehicle_[vehicle], 0);
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
  VehicleClassIndex GetVehicleClassIndexOfVehicle(int64_t vehicle) const {
    DCHECK(closed_);
    return vehicle_class_index_of_vehicle_[vehicle];
  }
  /// Returns a vehicle of the given vehicle class, and -1 if there are no
  /// vehicles for this class.
  int GetVehicleOfClass(VehicleClassIndex vehicle_class) const {
    DCHECK(closed_);
    const RoutingModel::VehicleTypeContainer& vehicle_type_container =
        GetVehicleTypeContainer();
    if (vehicle_class.value() >= GetVehicleClassesCount() ||
        vehicle_type_container.vehicles_per_vehicle_class[vehicle_class.value()]
            .empty()) {
      return -1;
    }
    return vehicle_type_container
        .vehicles_per_vehicle_class[vehicle_class.value()]
        .front();
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
  bool ArcIsMoreConstrainedThanArc(int64_t from, int64_t to1, int64_t to2);
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
  std::vector<std::vector<std::pair<int64_t, int64_t>>> GetCumulBounds(
      const Assignment& solution_assignment, const RoutingDimension& dimension);
#endif
  /// Returns the underlying constraint solver. Can be used to add extra
  /// constraints and/or modify search algorithms.
  Solver* solver() const { return solver_.get(); }

  /// Returns true if the search limit has been crossed with the given time
  /// offset.
  bool CheckLimit(absl::Duration offset = absl::ZeroDuration()) {
    DCHECK(limit_ != nullptr);
    return limit_->CheckWithOffset(offset);
  }

  /// Returns the time left in the search limit.
  absl::Duration RemainingTime() const {
    DCHECK(limit_ != nullptr);
    return limit_->AbsoluteSolverDeadline() - solver_->Now();
  }

  /// Returns the time buffer to safely return a solution.
  absl::Duration TimeBuffer() const { return time_buffer_; }

  /// Sizes and indices
  /// Returns the number of nodes in the model.
  int nodes() const { return nodes_; }
  /// Returns the number of vehicle routes in the model.
  int vehicles() const { return vehicles_; }
  /// Returns the number of next variables in the model.
  int64_t Size() const { return nodes_ + vehicles_ - start_end_count_; }

  /// Returns statistics on first solution search, number of decisions sent to
  /// filters, number of decisions rejected by filters.
  int64_t GetNumberOfDecisionsInFirstSolution(
      const RoutingSearchParameters& search_parameters) const;
  int64_t GetNumberOfRejectsInFirstSolution(
      const RoutingSearchParameters& search_parameters) const;
  /// Returns the automatic first solution strategy selected.
  operations_research::FirstSolutionStrategy::Value
  GetAutomaticFirstSolutionStrategy() const {
    return automatic_first_solution_strategy_;
  }

  /// Returns true if a vehicle/node matching problem is detected.
  bool IsMatchingModel() const;

  /// Returns true if routes are interdependent. This means that any
  /// modification to a route might impact another.
  bool AreRoutesInterdependent(const RoutingSearchParameters& parameters) const;

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
      std::function<int64_t(int64_t)> initializer);
#ifndef SWIG
  // TODO(user): MakeGreedyDescentLSOperator is too general for routing.h.
  /// Perhaps move it to constraint_solver.h.
  /// MakeGreedyDescentLSOperator creates a local search operator that tries to
  /// improve the initial assignment by moving a logarithmically decreasing step
  /// away in each possible dimension.
  static std::unique_ptr<LocalSearchOperator> MakeGreedyDescentLSOperator(
      std::vector<IntVar*> variables);
  // Read access to currently registered search monitors.
  const std::vector<SearchMonitor*>& GetSearchMonitors() const {
    return monitors_;
  }
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
    RELOCATE_PATH_GLOBAL_CHEAPEST_INSERTION_INSERT_UNPERFORMED,
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
    SHORTEST_PATH_SWAP_ACTIVE,
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
    std::vector<int64_t> indices;
    T value;
  };
  struct DisjunctionValues {
    int64_t penalty;
    int64_t max_cardinality;
  };
  typedef ValuedNodes<DisjunctionValues> Disjunction;

  /// Storage of a cost cache element corresponding to a cost arc ending at
  /// node 'index' and on the cost class 'cost_class'.
  struct CostCacheElement {
    /// This is usually an int64_t, but using an int here decreases the RAM
    /// usage, and should be fine since in practice we never have more than
    /// 1<<31 vars. Note(user): on 2013-11, microbenchmarks on the arc costs
    /// callbacks also showed a 2% speed-up thanks to using int rather than
    /// int64_t.
    int index;
    CostClassIndex cost_class_index;
    int64_t cost;
  };

  /// Internal struct used to store the lp/mp versions of the local and global
  /// cumul optimizers for a given dimension.
  template <class DimensionCumulOptimizer>
  struct DimensionCumulOptimizers {
    std::unique_ptr<DimensionCumulOptimizer> lp_optimizer;
    std::unique_ptr<DimensionCumulOptimizer> mp_optimizer;
  };

  /// Internal methods.
  void Initialize();
  void AddNoCycleConstraintInternal();
  bool AddDimensionWithCapacityInternal(
      const std::vector<int>& evaluator_indices, int64_t slack_max,
      std::vector<int64_t> vehicle_capacities, bool fix_start_cumul_to_zero,
      const std::string& name);
  bool AddDimensionDependentDimensionWithVehicleCapacityInternal(
      const std::vector<int>& pure_transits,
      const std::vector<int>& dependent_transits,
      const RoutingDimension* base_dimension, int64_t slack_max,
      std::vector<int64_t> vehicle_capacities, bool fix_start_cumul_to_zero,
      const std::string& name);
  bool InitializeDimensionInternal(
      const std::vector<int>& evaluator_indices,
      const std::vector<int>& state_dependent_evaluator_indices,
      int64_t slack_max, bool fix_start_cumul_to_zero,
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
  // Called by FinalizeVisitTypes() to setup topologically_sorted_visit_types_.
  void TopologicallySortVisitTypes();
  int64_t GetArcCostForClassInternal(int64_t from_index, int64_t to_index,
                                     CostClassIndex cost_class_index) const;
  void AppendHomogeneousArcCosts(const RoutingSearchParameters& parameters,
                                 int node_index,
                                 std::vector<IntVar*>* cost_elements);
  void AppendArcCosts(const RoutingSearchParameters& parameters, int node_index,
                      std::vector<IntVar*>* cost_elements);
  Assignment* DoRestoreAssignment();
  static const CostClassIndex kCostClassIndexOfZeroCost;
  int64_t SafeGetCostClassInt64OfVehicle(int64_t vehicle) const {
    DCHECK_LT(0, vehicles_);
    return (vehicle >= 0 ? GetCostClassIndexOfVehicle(vehicle)
                         : kCostClassIndexOfZeroCost)
        .value();
  }
  int64_t GetDimensionTransitCostSum(int64_t i, int64_t j,
                                     const CostClass& cost_class) const;
  /// Returns nullptr if no penalty cost, otherwise returns penalty variable.
  IntVar* CreateDisjunction(DisjunctionIndex disjunction);
  /// Sets up pickup and delivery sets.
  void AddPickupAndDeliverySetsInternal(const std::vector<int64_t>& pickups,
                                        const std::vector<int64_t>& deliveries);
  /// Returns the cost variable related to the soft same vehicle constraint of
  /// index 'vehicle_index'.
  IntVar* CreateSameVehicleCost(int vehicle_index);
  /// Returns the first active variable index in 'indices' starting from index
  /// + 1.
  int FindNextActive(int index, const std::vector<int64_t>& indices) const;

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
                   const std::string& description, int64_t solution_cost,
                   int64_t start_time_ms);
  /// See CompactAssignment. Checks the final solution if
  /// check_compact_assignment is true.
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
  template <class T>
  LocalSearchOperator* CreateCPOperator(const T& operator_factory) {
    return operator_factory(solver_.get(), nexts_,
                            CostsAreHomogeneousAcrossVehicles()
                                ? std::vector<IntVar*>()
                                : vehicle_vars_,
                            vehicle_start_class_callback_);
  }
  template <class T>
  LocalSearchOperator* CreateCPOperator() {
    return CreateCPOperator(MakeLocalSearchOperator<T>);
  }
  template <class T, class Arg>
  LocalSearchOperator* CreateOperator(const Arg& arg) {
    return solver_->RevAlloc(new T(nexts_,
                                   CostsAreHomogeneousAcrossVehicles()
                                       ? std::vector<IntVar*>()
                                       : vehicle_vars_,
                                   vehicle_start_class_callback_, arg));
  }
  template <class T, class Arg1, class MoveableArg2>
  LocalSearchOperator* CreateOperator(const Arg1& arg1, MoveableArg2 arg2) {
    return solver_->RevAlloc(
        new T(nexts_,
              CostsAreHomogeneousAcrossVehicles() ? std::vector<IntVar*>()
                                                  : vehicle_vars_,
              vehicle_start_class_callback_, arg1, std::move(arg2)));
  }
  template <class T>
  LocalSearchOperator* CreatePairOperator() {
    return CreateOperator<T>(pickup_delivery_pairs_);
  }
  void CreateNeighborhoodOperators(const RoutingSearchParameters& parameters);
  LocalSearchOperator* ConcatenateOperators(
      const RoutingSearchParameters& search_parameters,
      const std::vector<LocalSearchOperator*>& operators) const;
  LocalSearchOperator* GetNeighborhoodOperators(
      const RoutingSearchParameters& search_parameters) const;

  struct FilterOptions {
    bool filter_objective;
    bool filter_with_cp_solver;

    bool operator==(const FilterOptions& other) const {
      return other.filter_objective == filter_objective &&
             other.filter_with_cp_solver == filter_with_cp_solver;
    }
    template <typename H>
    friend H AbslHashValue(H h, const FilterOptions& options) {
      return H::combine(std::move(h), options.filter_objective,
                        options.filter_with_cp_solver);
    }
  };
  std::vector<LocalSearchFilterManager::FilterEvent> CreateLocalSearchFilters(
      const RoutingSearchParameters& parameters, const FilterOptions& options);
  LocalSearchFilterManager* GetOrCreateLocalSearchFilterManager(
      const RoutingSearchParameters& parameters, const FilterOptions& options);
  DecisionBuilder* CreateSolutionFinalizer(
      const RoutingSearchParameters& parameters, SearchLimit* lns_limit);
  DecisionBuilder* CreateFinalizerForMinimizedAndMaximizedVariables();
  void CreateFirstSolutionDecisionBuilders(
      const RoutingSearchParameters& search_parameters);
  DecisionBuilder* GetFirstSolutionDecisionBuilder(
      const RoutingSearchParameters& search_parameters) const;
  IntVarFilteredDecisionBuilder* GetFilteredFirstSolutionDecisionBuilderOrNull(
      const RoutingSearchParameters& parameters) const;
#ifndef SWIG
  template <typename Heuristic, typename... Args>
  IntVarFilteredDecisionBuilder* CreateIntVarFilteredDecisionBuilder(
      const Args&... args);
#endif
  LocalSearchPhaseParameters* CreateLocalSearchParameters(
      const RoutingSearchParameters& search_parameters);
  DecisionBuilder* CreateLocalSearchDecisionBuilder(
      const RoutingSearchParameters& search_parameters);
  void SetupDecisionBuilders(const RoutingSearchParameters& search_parameters);
  void SetupMetaheuristics(const RoutingSearchParameters& search_parameters);
  void SetupAssignmentCollector(
      const RoutingSearchParameters& search_parameters);
  void SetupTrace(const RoutingSearchParameters& search_parameters);
  void SetupImprovementLimit(const RoutingSearchParameters& search_parameters);
  void SetupSearchMonitors(const RoutingSearchParameters& search_parameters);
  bool UsesLightPropagation(
      const RoutingSearchParameters& search_parameters) const;
  GetTabuVarsCallback tabu_var_callback_;

  // Detects implicit pickup delivery pairs. These pairs are
  // non-pickup/delivery pairs for which there exists a unary dimension such
  // that the demand d of the implicit pickup is positive and the demand of the
  // implicit delivery is equal to -d.
  void DetectImplicitPickupAndDeliveries();

  int GetVehicleStartClass(int64_t start) const;

  void InitSameVehicleGroups(int number_of_groups) {
    same_vehicle_group_.assign(Size(), 0);
    same_vehicle_groups_.assign(number_of_groups, {});
  }
  void SetSameVehicleGroup(int index, int group) {
    same_vehicle_group_[index] = group;
    same_vehicle_groups_[group].push_back(index);
  }

  /// Returns the internal global/local optimizer index for the given dimension
  /// if any, and -1 otherwise.
  int GetGlobalCumulOptimizerIndex(const RoutingDimension& dimension) const;
  int GetLocalCumulOptimizerIndex(const RoutingDimension& dimension) const;

  /// Model
  std::unique_ptr<Solver> solver_;
  int nodes_;
  int vehicles_;
  int max_active_vehicles_;
  Constraint* no_cycle_constraint_ = nullptr;
  /// Decision variables: indexed by int64_t var index.
  std::vector<IntVar*> nexts_;
  std::vector<IntVar*> vehicle_vars_;
  std::vector<IntVar*> active_;
  /// Resource variables, indexed first by resource group index and then by
  /// vehicle index. A resource variable can have a negative value of -1, iff
  /// the corresponding vehicle doesn't require a resource from this resource
  /// group, OR if the vehicle is unused (i.e. no visits on its route and
  /// vehicle_used_when_empty_[v] is false).
  // clang-format off
  std::vector<std::vector<IntVar*> > resource_vars_;
  // clang-format on
  // The following vectors are indexed by vehicle index.
  std::vector<IntVar*> vehicle_active_;
  std::vector<IntVar*> vehicle_route_considered_;
  /// is_bound_to_end_[i] will be true iff the path starting at var #i is fully
  /// bound and reaches the end of a route, i.e. either:
  /// - IsEnd(i) is true
  /// - or nexts_[i] is bound and is_bound_to_end_[nexts_[i].Value()] is true.
  std::vector<IntVar*> is_bound_to_end_;
  mutable RevSwitch is_bound_to_end_ct_added_;
  /// Dimensions
  absl::flat_hash_map<std::string, DimensionIndex> dimension_name_to_index_;
  absl::StrongVector<DimensionIndex, RoutingDimension*> dimensions_;
  /// Resource Groups.
  /// If resource_groups_ is not empty, then for each group of resources, each
  /// (used) vehicle must be assigned to exactly 1 resource, and each resource
  /// can in turn be assigned to at most 1 vehicle.
  // clang-format off
  std::vector<std::unique_ptr<ResourceGroup> > resource_groups_;
  /// Stores the set of resource groups related to each dimension.
  absl::StrongVector<DimensionIndex, std::vector<int> >
      dimension_resource_group_indices_;

  /// TODO(user): Define a new Dimension[Global|Local]OptimizerIndex type
  /// and use it to define ITIVectors and for the dimension to optimizer index
  /// mappings below.
  std::vector<DimensionCumulOptimizers<GlobalDimensionCumulOptimizer> >
      global_dimension_optimizers_;
  absl::StrongVector<DimensionIndex, int> global_optimizer_index_;
  std::vector<DimensionCumulOptimizers<LocalDimensionCumulOptimizer> >
      local_dimension_optimizers_;
  absl::StrongVector<DimensionIndex, int> local_optimizer_index_;
  // clang-format on
  std::string primary_constrained_dimension_;
  /// Costs
  IntVar* cost_ = nullptr;
  std::vector<int> vehicle_to_transit_cost_;
  std::vector<int64_t> fixed_cost_of_vehicle_;
  std::vector<CostClassIndex> cost_class_index_of_vehicle_;
  bool has_vehicle_with_zero_cost_class_;
  std::vector<int64_t> linear_cost_factor_of_vehicle_;
  std::vector<int64_t> quadratic_cost_factor_of_vehicle_;
  bool vehicle_amortized_cost_factors_set_;
  /// vehicle_used_when_empty_[vehicle] determines if "vehicle" should be
  /// taken into account for costs (arc costs, span costs, etc.) and constraints
  /// (eg. resources) even when the route of the vehicle is empty (i.e. goes
  /// straight from its start to its end).
  ///
  /// NOTE1: A vehicle's fixed cost is added iff the vehicle serves nodes on its
  /// route, regardless of this variable's value.
  ///
  /// NOTE2: The default value for this boolean is 'false' for all vehicles,
  /// i.e. by default empty routes will not contribute to the cost nor be
  /// considered for constraints.
  std::vector<bool> vehicle_used_when_empty_;
#ifndef SWIG
  absl::StrongVector<CostClassIndex, CostClass> cost_classes_;
#endif  // SWIG
  bool costs_are_homogeneous_across_vehicles_;
  bool cache_callbacks_;
  mutable std::vector<CostCacheElement> cost_cache_;  /// Index by source index.
  std::vector<VehicleClassIndex> vehicle_class_index_of_vehicle_;
#ifndef SWIG
  absl::StrongVector<VehicleClassIndex, VehicleClass> vehicle_classes_;
#endif  // SWIG
  VehicleTypeContainer vehicle_type_container_;
  std::function<int(int64_t)> vehicle_start_class_callback_;
  /// Disjunctions
  absl::StrongVector<DisjunctionIndex, Disjunction> disjunctions_;
  // clang-format off
  std::vector<std::vector<DisjunctionIndex> > index_to_disjunctions_;
  /// Same vehicle costs
  std::vector<ValuedNodes<int64_t> > same_vehicle_costs_;
  /// Allowed vehicles
#ifndef SWIG
  std::vector<absl::flat_hash_set<int>> allowed_vehicles_;
#endif  // SWIG
  /// Pickup and delivery
  IndexPairs pickup_delivery_pairs_;
  IndexPairs implicit_pickup_delivery_pairs_without_alternatives_;
  std::vector<std::pair<DisjunctionIndex, DisjunctionIndex> >
      pickup_delivery_disjunctions_;
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

  // Visit types sorted topologically based on required-->dependent requirement
  // arcs between the types (if the requirement/dependency graph is acyclic).
  // Visit types of the same topological level are sorted in each sub-vector
  // by decreasing requirement "tightness", computed as the pair of the two
  // following criteria:
  //
  // 1) How highly *dependent* this type is, determined by
  //    (total number of required alternative sets for that type)
  //        / (average number of types in the required alternative sets)
  // 2) How highly *required* this type t is, computed as
  //    SUM_{S required set containing t} ( 1 / |S| ),
  //    i.e. the sum of reverse number of elements of all required sets
  //    containing the type t.
  //
  // The higher these two numbers, the tighter the type is wrt requirements.
  std::vector<std::vector<int> > topologically_sorted_visit_types_;
  // clang-format on
  int num_visit_types_;
  // Two indices are equivalent if they correspond to the same node (as given
  // to the constructors taking a RoutingIndexManager).
  std::vector<int> index_to_equivalence_class_;
  const PathsMetadata paths_metadata_;
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
  bool local_optimum_reached_ = false;
  // Best lower bound found during the search.
  int64_t objective_lower_bound_ = kint64min;
  SolutionCollector* collect_assignments_ = nullptr;
  SolutionCollector* collect_one_assignment_ = nullptr;
  SolutionCollector* optimized_dimensions_assignment_collector_ = nullptr;
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
  absl::flat_hash_map<FilterOptions, LocalSearchFilterManager*>
      local_search_filter_managers_;
  std::vector<LocalSearchFilterManager::FilterEvent> extra_filters_;
  absl::flat_hash_map<int, std::unique_ptr<NodeNeighborsByCostClass>>
      node_neighbors_by_cost_class_per_size_;
#ifndef SWIG
  struct VarTarget {
    VarTarget(IntVar* v, int64_t t) : var(v), target(t) {}

    IntVar* var;
    int64_t target;
  };
  std::vector<std::pair<VarTarget, int64_t>>
      weighted_finalizer_variable_targets_;
  std::vector<VarTarget> finalizer_variable_targets_;
  absl::flat_hash_map<IntVar*, int> weighted_finalizer_variable_index_;
  absl::flat_hash_set<IntVar*> finalizer_variable_target_set_;
  std::unique_ptr<SweepArranger> sweep_arranger_;
#endif

  RegularLimit* limit_ = nullptr;
  RegularLimit* ls_limit_ = nullptr;
  RegularLimit* lns_limit_ = nullptr;
  RegularLimit* first_solution_lns_limit_ = nullptr;
  absl::Duration time_buffer_;

  typedef std::pair<int64_t, int64_t> CacheKey;
  typedef absl::flat_hash_map<CacheKey, int64_t> TransitCallbackCache;
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
  friend class ResourceGroup::Resource;

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
    std::vector<int64_t> start_min;
    std::vector<int64_t> start_max;
    std::vector<int64_t> duration_min;
    std::vector<int64_t> duration_max;
    std::vector<int64_t> end_min;
    std::vector<int64_t> end_max;
    std::vector<bool> is_preemptible;
    std::vector<const SortedDisjointIntervalList*> forbidden_intervals;
    std::vector<std::pair<int64_t, int64_t>> distance_duration;
    int64_t span_min = 0;
    int64_t span_max = kint64max;

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
  sat::ThetaLambdaTree<int64_t> theta_lambda_tree_;
  /// Mappings between events and tasks.
  std::vector<int> tasks_by_start_min_;
  std::vector<int> tasks_by_end_max_;
  std::vector<int> event_of_task_;
  std::vector<int> nonchain_tasks_by_start_max_;
  /// Maps chain elements to the sum of chain task durations before them.
  std::vector<int64_t> total_duration_before_;
};

struct TravelBounds {
  std::vector<int64_t> min_travels;
  std::vector<int64_t> max_travels;
  std::vector<int64_t> pre_travels;
  std::vector<int64_t> post_travels;
};

void AppendTasksFromPath(const std::vector<int64_t>& path,
                         const TravelBounds& travel_bounds,
                         const RoutingDimension& dimension,
                         DisjunctivePropagator::Tasks* tasks);
void AppendTasksFromIntervals(const std::vector<IntervalVar*>& intervals,
                              DisjunctivePropagator::Tasks* tasks);
void FillPathEvaluation(const std::vector<int64_t>& path,
                        const RoutingModel::TransitCallback2& evaluator,
                        std::vector<int64_t>* values);
void FillTravelBoundsOfVehicle(int vehicle, const std::vector<int64_t>& path,
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

  const RoutingModel* model_;
  const RoutingDimension* const dimension_;
  std::vector<Demon*> vehicle_demons_;
  std::vector<int64_t> path_;

  /// Sets path_ to be the longest sequence such that
  /// _ path_[0] is the start of the vehicle
  /// _ Next(path_[i-1]) is Bound() and has value path_[i],
  /// followed by the end of the vehicle if the last node was not an end.
  void FillPartialPathOfVehicle(int vehicle);
  void FillPathTravels(const std::vector<int64_t>& path);

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
    TaskTranslator(IntVar* start, int64_t before_start, int64_t after_start)
        : start_(start),
          before_start_(before_start),
          after_start_(after_start) {}
    explicit TaskTranslator(IntervalVar* interval) : interval_(interval) {}
    TaskTranslator() = default;

    void SetStartMin(int64_t value) {
      if (start_ != nullptr) {
        start_->SetMin(CapAdd(before_start_, value));
      } else if (interval_ != nullptr) {
        interval_->SetStartMin(value);
      }
    }
    void SetStartMax(int64_t value) {
      if (start_ != nullptr) {
        start_->SetMax(CapAdd(before_start_, value));
      } else if (interval_ != nullptr) {
        interval_->SetStartMax(value);
      }
    }
    void SetDurationMin(int64_t value) {
      if (interval_ != nullptr) {
        interval_->SetDurationMin(value);
      }
    }
    void SetEndMin(int64_t value) {
      if (start_ != nullptr) {
        start_->SetMin(CapSub(value, after_start_));
      } else if (interval_ != nullptr) {
        interval_->SetEndMin(value);
      }
    }
    void SetEndMax(int64_t value) {
      if (start_ != nullptr) {
        start_->SetMax(CapSub(value, after_start_));
      } else if (interval_ != nullptr) {
        interval_->SetEndMax(value);
      }
    }

   private:
    IntVar* start_ = nullptr;
    int64_t before_start_;
    int64_t after_start_;
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
  virtual ~TypeRegulationsChecker() = default;

  bool CheckVehicle(int vehicle,
                    const std::function<int64_t(int64_t)>& next_accessor);

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
                       const std::function<int64_t(int64_t)>& next_accessor);
  virtual void OnInitializeCheck() {}
  virtual bool HasRegulationsToCheck() const = 0;
  virtual bool CheckTypeRegulations(int type, VisitTypePolicy policy,
                                    int pos) = 0;
  virtual bool FinalizeCheck() const { return true; }

  const RoutingModel& model_;

 private:
  std::vector<TypePolicyOccurrence> occurrences_of_type_;
  std::vector<int64_t> current_route_visits_;
};

/// Checker for type incompatibilities.
class TypeIncompatibilityChecker : public TypeRegulationsChecker {
 public:
  TypeIncompatibilityChecker(const RoutingModel& model,
                             bool check_hard_incompatibilities);
  ~TypeIncompatibilityChecker() override = default;

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
  ~TypeRequirementChecker() override = default;

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
struct BoundCost {
  int64_t bound;
  int64_t cost;
  BoundCost() : bound(0), cost(0) {}
  BoundCost(int64_t bound, int64_t cost) : bound(bound), cost(cost) {}
};

class SimpleBoundCosts {
 public:
  SimpleBoundCosts(int num_bounds, BoundCost default_bound_cost)
      : bound_costs_(num_bounds, default_bound_cost) {}
#ifndef SWIG
  BoundCost& bound_cost(int element) { return bound_costs_[element]; }
#endif
  BoundCost bound_cost(int element) const { return bound_costs_[element]; }
  int Size() { return bound_costs_.size(); }
  SimpleBoundCosts(const SimpleBoundCosts&) = delete;
  SimpleBoundCosts operator=(const SimpleBoundCosts&) = delete;

 private:
  std::vector<BoundCost> bound_costs_;
};

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
  int64_t GetTransitValue(int64_t from_index, int64_t to_index,
                          int64_t vehicle) const;
  /// Same as above but taking a vehicle class of the dimension instead of a
  /// vehicle (the class of a vehicle can be obtained with vehicle_to_class()).
  int64_t GetTransitValueFromClass(int64_t from_index, int64_t to_index,
                                   int64_t vehicle_class) const {
    return model_->TransitCallback(class_evaluators_[vehicle_class])(from_index,
                                                                     to_index);
  }
  /// Get the cumul, transit and slack variables for the given node (given as
  /// int64_t var index).
  IntVar* CumulVar(int64_t index) const { return cumuls_[index]; }
  IntVar* TransitVar(int64_t index) const { return transits_[index]; }
  IntVar* FixedTransitVar(int64_t index) const {
    return fixed_transits_[index];
  }
  IntVar* SlackVar(int64_t index) const { return slacks_[index]; }

#if !defined(SWIGPYTHON)
  /// Like CumulVar(), TransitVar(), SlackVar() but return the whole variable
  /// vectors instead (indexed by int64_t var index).
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
  SortedDisjointIntervalList GetAllowedIntervalsInRange(
      int64_t index, int64_t min_value, int64_t max_value) const;
  /// Returns the smallest value outside the forbidden intervals of node 'index'
  /// that is greater than or equal to a given 'min_value'.
  int64_t GetFirstPossibleGreaterOrEqualValueForNode(int64_t index,
                                                     int64_t min_value) const {
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
  int64_t GetLastPossibleLessOrEqualValueForNode(int64_t index,
                                                 int64_t max_value) const {
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
  const std::vector<int64_t>& vehicle_capacities() const {
    return vehicle_capacities_;
  }
  /// Returns the callback evaluating the transit value between two node indices
  /// for a given vehicle.
  const RoutingModel::TransitCallback2& transit_evaluator(int vehicle) const {
    return model_->TransitCallback(
        class_evaluators_[vehicle_to_class_[vehicle]]);
  }

  /// Returns the callback evaluating the transit value between two node indices
  /// for a given vehicle class.
  const RoutingModel::TransitCallback2& class_transit_evaluator(
      RoutingVehicleClassIndex vehicle_class) const {
    const int vehicle = model_->GetVehicleOfClass(vehicle_class);
    DCHECK_NE(vehicle, -1);
    return transit_evaluator(vehicle);
  }

  /// Returns the unary callback evaluating the transit value between two node
  /// indices for a given vehicle. If the corresponding callback is not unary,
  /// returns a null callback.
  const RoutingModel::TransitCallback1& GetUnaryTransitEvaluator(
      int vehicle) const {
    return model_->UnaryTransitCallbackOrNull(
        class_evaluators_[vehicle_to_class_[vehicle]]);
  }
  const RoutingModel::TransitCallback2& GetBinaryTransitEvaluator(
      int vehicle) const {
    return model_->TransitCallback(
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
  void SetSpanUpperBoundForVehicle(int64_t upper_bound, int vehicle);
  /// Sets a cost proportional to the dimension span on a given vehicle,
  /// or on all vehicles at once. "coefficient" must be nonnegative.
  /// This is handy to model costs proportional to idle time when the dimension
  /// represents time.
  /// The cost for a vehicle is
  ///   span_cost = coefficient * (dimension end value - dimension start value).
  void SetSpanCostCoefficientForVehicle(int64_t coefficient, int vehicle);
  void SetSpanCostCoefficientForAllVehicles(int64_t coefficient);
  /// Sets a cost proportional to the *global* dimension span, that is the
  /// difference between the largest value of route end cumul variables and
  /// the smallest value of route start cumul variables.
  /// In other words:
  /// global_span_cost =
  ///   coefficient * (Max(dimension end value) - Min(dimension start value)).
  void SetGlobalSpanCostCoefficient(int64_t coefficient);

#ifndef SWIG
  /// Sets a piecewise linear cost on the cumul variable of a given variable
  /// index. If f is a piecewise linear function, the resulting cost at 'index'
  /// will be f(CumulVar(index)). As of 3/2017, only non-decreasing positive
  /// cost functions are supported.
  void SetCumulVarPiecewiseLinearCost(int64_t index,
                                      const PiecewiseLinearFunction& cost);
  /// Returns true if a piecewise linear cost has been set for a given variable
  /// index.
  bool HasCumulVarPiecewiseLinearCost(int64_t index) const;
  /// Returns the piecewise linear cost of a cumul variable for a given variable
  /// index. The returned pointer has the same validity as this class.
  const PiecewiseLinearFunction* GetCumulVarPiecewiseLinearCost(
      int64_t index) const;
#endif

  /// Sets a soft upper bound to the cumul variable of a given variable index.
  /// If the value of the cumul variable is greater than the bound, a cost
  /// proportional to the difference between this value and the bound is added
  /// to the cost function of the model:
  ///   cumulVar <= upper_bound -> cost = 0
  ///    cumulVar > upper_bound -> cost = coefficient * (cumulVar - upper_bound)
  /// This is also handy to model tardiness costs when the dimension represents
  /// time.
  void SetCumulVarSoftUpperBound(int64_t index, int64_t upper_bound,
                                 int64_t coefficient);
  /// Returns true if a soft upper bound has been set for a given variable
  /// index.
  bool HasCumulVarSoftUpperBound(int64_t index) const;
  /// Returns the soft upper bound of a cumul variable for a given variable
  /// index. The "hard" upper bound of the variable is returned if no soft upper
  /// bound has been set.
  int64_t GetCumulVarSoftUpperBound(int64_t index) const;
  /// Returns the cost coefficient of the soft upper bound of a cumul variable
  /// for a given variable index. If no soft upper bound has been set, 0 is
  /// returned.
  int64_t GetCumulVarSoftUpperBoundCoefficient(int64_t index) const;

  /// Sets a soft lower bound to the cumul variable of a given variable index.
  /// If the value of the cumul variable is less than the bound, a cost
  /// proportional to the difference between this value and the bound is added
  /// to the cost function of the model:
  ///   cumulVar > lower_bound -> cost = 0
  ///   cumulVar <= lower_bound -> cost = coefficient * (lower_bound -
  ///               cumulVar).
  /// This is also handy to model earliness costs when the dimension represents
  /// time.
  void SetCumulVarSoftLowerBound(int64_t index, int64_t lower_bound,
                                 int64_t coefficient);
  /// Returns true if a soft lower bound has been set for a given variable
  /// index.
  bool HasCumulVarSoftLowerBound(int64_t index) const;
  /// Returns the soft lower bound of a cumul variable for a given variable
  /// index. The "hard" lower bound of the variable is returned if no soft lower
  /// bound has been set.
  int64_t GetCumulVarSoftLowerBound(int64_t index) const;
  /// Returns the cost coefficient of the soft lower bound of a cumul variable
  /// for a given variable index. If no soft lower bound has been set, 0 is
  /// returned.
  int64_t GetCumulVarSoftLowerBoundCoefficient(int64_t index) const;
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
                                  std::vector<int64_t> node_visit_transits);

  /// With breaks supposed to be consecutive, this forces the distance between
  /// breaks of size at least minimum_break_duration to be at most distance.
  /// This supposes that the time until route start and after route end are
  /// infinite breaks.
  void SetBreakDistanceDurationOfVehicle(int64_t distance, int64_t duration,
                                         int vehicle);
  /// Sets up vehicle_break_intervals_, vehicle_break_distance_duration_,
  /// pre_travel_evaluators and post_travel_evaluators.
  void InitializeBreaks();
  /// Returns true if any break interval or break distance was defined.
  bool HasBreakConstraints() const;
#if !defined(SWIGPYTHON)
  /// Deprecated, sets pre_travel(i, j) = node_visit_transit[i]
  /// and post_travel(i, j) = delays(i, j).
  void SetBreakIntervalsOfVehicle(
      std::vector<IntervalVar*> breaks, int vehicle,
      std::vector<int64_t> node_visit_transits,
      std::function<int64_t(int64_t, int64_t)> delays);

  /// Returns the break intervals set by SetBreakIntervalsOfVehicle().
  const std::vector<IntervalVar*>& GetBreakIntervalsOfVehicle(
      int vehicle) const;
  /// Returns the pairs (distance, duration) specified by break distance
  /// constraints.
  // clang-format off
  const std::vector<std::pair<int64_t, int64_t> >&
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
  int64_t ShortestTransitionSlack(int64_t node) const;

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
  typedef std::function<int64_t(int, int)> PickupToDeliveryLimitFunction;

  void SetPickupToDeliveryLimitFunctionForPair(
      PickupToDeliveryLimitFunction limit_function, int pair_index);

  bool HasPickupToDeliveryLimits() const;
#ifndef SWIG
  int64_t GetPickupToDeliveryLimitForPair(int pair_index, int pickup,
                                          int delivery) const;

  struct NodePrecedence {
    int64_t first_node;
    int64_t second_node;
    int64_t offset;
  };

  void AddNodePrecedence(NodePrecedence precedence) {
    node_precedences_.push_back(precedence);
  }
  const std::vector<NodePrecedence>& GetNodePrecedences() const {
    return node_precedences_;
  }
#endif  // SWIG

  void AddNodePrecedence(int64_t first_node, int64_t second_node,
                         int64_t offset) {
    AddNodePrecedence({first_node, second_node, offset});
  }

  int64_t GetSpanUpperBoundForVehicle(int vehicle) const {
    return vehicle_span_upper_bounds_[vehicle];
  }
#ifndef SWIG
  const std::vector<int64_t>& vehicle_span_upper_bounds() const {
    return vehicle_span_upper_bounds_;
  }
#endif  // SWIG
  int64_t GetSpanCostCoefficientForVehicle(int vehicle) const {
    return vehicle_span_cost_coefficients_[vehicle];
  }
#ifndef SWIG
  int64_t GetSpanCostCoefficientForVehicleClass(
      RoutingVehicleClassIndex vehicle_class) const {
    const int vehicle = model_->GetVehicleOfClass(vehicle_class);
    DCHECK_NE(vehicle, -1);
    return GetSpanCostCoefficientForVehicle(vehicle);
  }
#endif  // SWIG
#ifndef SWIG
  const std::vector<int64_t>& vehicle_span_cost_coefficients() const {
    return vehicle_span_cost_coefficients_;
  }
#endif  // SWIG
  int64_t global_span_cost_coefficient() const {
    return global_span_cost_coefficient_;
  }

  int64_t GetGlobalOptimizerOffset() const {
    DCHECK_GE(global_optimizer_offset_, 0);
    return global_optimizer_offset_;
  }
  int64_t GetLocalOptimizerOffsetForVehicle(int vehicle) const {
    if (vehicle >= local_optimizer_offset_for_vehicle_.size()) {
      return 0;
    }
    DCHECK_GE(local_optimizer_offset_for_vehicle_[vehicle], 0);
    return local_optimizer_offset_for_vehicle_[vehicle];
  }

  /// If the span of vehicle on this dimension is larger than bound,
  /// the cost will be increased by cost * (span - bound).
  void SetSoftSpanUpperBoundForVehicle(BoundCost bound_cost, int vehicle) {
    if (!HasSoftSpanUpperBounds()) {
      vehicle_soft_span_upper_bound_ = std::make_unique<SimpleBoundCosts>(
          model_->vehicles(), BoundCost{kint64max, 0});
    }
    vehicle_soft_span_upper_bound_->bound_cost(vehicle) = bound_cost;
  }
  bool HasSoftSpanUpperBounds() const {
    return vehicle_soft_span_upper_bound_ != nullptr;
  }
  BoundCost GetSoftSpanUpperBoundForVehicle(int vehicle) const {
    DCHECK(HasSoftSpanUpperBounds());
    return vehicle_soft_span_upper_bound_->bound_cost(vehicle);
  }
  /// If the span of vehicle on this dimension is larger than bound,
  /// the cost will be increased by cost * (span - bound)^2.
  void SetQuadraticCostSoftSpanUpperBoundForVehicle(BoundCost bound_cost,
                                                    int vehicle) {
    if (!HasQuadraticCostSoftSpanUpperBounds()) {
      vehicle_quadratic_cost_soft_span_upper_bound_ =
          std::make_unique<SimpleBoundCosts>(model_->vehicles(),
                                             BoundCost{kint64max, 0});
    }
    vehicle_quadratic_cost_soft_span_upper_bound_->bound_cost(vehicle) =
        bound_cost;
  }
  bool HasQuadraticCostSoftSpanUpperBounds() const {
    return vehicle_quadratic_cost_soft_span_upper_bound_ != nullptr;
  }
  BoundCost GetQuadraticCostSoftSpanUpperBoundForVehicle(int vehicle) const {
    DCHECK(HasQuadraticCostSoftSpanUpperBounds());
    return vehicle_quadratic_cost_soft_span_upper_bound_->bound_cost(vehicle);
  }

 private:
  struct SoftBound {
    IntVar* var;
    int64_t bound;
    int64_t coefficient;
  };

  struct PiecewiseLinearCost {
    PiecewiseLinearCost() : var(nullptr), cost(nullptr) {}
    IntVar* var;
    std::unique_ptr<PiecewiseLinearFunction> cost;
  };

  class SelfBased {};
  RoutingDimension(RoutingModel* model, std::vector<int64_t> vehicle_capacities,
                   const std::string& name,
                   const RoutingDimension* base_dimension);
  RoutingDimension(RoutingModel* model, std::vector<int64_t> vehicle_capacities,
                   const std::string& name, SelfBased);
  void Initialize(const std::vector<int>& transit_evaluators,
                  const std::vector<int>& state_dependent_transit_evaluators,
                  int64_t slack_max);
  void InitializeCumuls();
  void InitializeTransits(
      const std::vector<int>& transit_evaluators,
      const std::vector<int>& state_dependent_transit_evaluators,
      int64_t slack_max);
  void InitializeTransitVariables(int64_t slack_max);
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

  void SetOffsetForGlobalOptimizer(int64_t offset) {
    global_optimizer_offset_ = std::max(Zero(), offset);
  }
  /// Moves elements of "offsets" into vehicle_offsets_for_local_optimizer_.
  void SetVehicleOffsetsForLocalOptimizer(std::vector<int64_t> offsets) {
    /// Make sure all offsets are positive.
    std::transform(offsets.begin(), offsets.end(), offsets.begin(),
                   [](int64_t offset) { return std::max(Zero(), offset); });
    local_optimizer_offset_for_vehicle_ = std::move(offsets);
  }

  std::vector<IntVar*> cumuls_;
  std::vector<SortedDisjointIntervalList> forbidden_intervals_;
  std::vector<IntVar*> capacity_vars_;
  const std::vector<int64_t> vehicle_capacities_;
  std::vector<IntVar*> transits_;
  std::vector<IntVar*> fixed_transits_;
  /// Values in class_evaluators_ correspond to the evaluators in
  /// RoutingModel::transit_evaluators_ for each vehicle class.
  std::vector<int> class_evaluators_;
  std::vector<int64_t> vehicle_to_class_;
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
  std::vector<int64_t> state_dependent_vehicle_to_class_;

  // For each pickup/delivery pair_index for which limits have been set,
  // pickup_to_delivery_limits_per_pair_index_[pair_index] contains the
  // PickupToDeliveryLimitFunction for the pickup and deliveries in this pair.
  std::vector<PickupToDeliveryLimitFunction>
      pickup_to_delivery_limits_per_pair_index_;

  // Used if some vehicle has breaks in this dimension, typically time.
  bool break_constraints_are_initialized_ = false;
  // clang-format off
  std::vector<std::vector<IntervalVar*> > vehicle_break_intervals_;
  std::vector<std::vector<std::pair<int64_t, int64_t> > >
      vehicle_break_distance_duration_;
  // clang-format on
  // For each vehicle, stores the part of travel that is made directly
  // after (before) the departure (arrival) node of the travel.
  // These parts of the travel are non-interruptible, in particular by a break.
  std::vector<int> vehicle_pre_travel_evaluators_;
  std::vector<int> vehicle_post_travel_evaluators_;

  std::vector<IntVar*> slacks_;
  std::vector<IntVar*> dependent_transits_;
  std::vector<int64_t> vehicle_span_upper_bounds_;
  int64_t global_span_cost_coefficient_;
  std::vector<int64_t> vehicle_span_cost_coefficients_;
  std::vector<SoftBound> cumul_var_soft_upper_bound_;
  std::vector<SoftBound> cumul_var_soft_lower_bound_;
  std::vector<PiecewiseLinearCost> cumul_var_piecewise_linear_cost_;
  RoutingModel* const model_;
  const std::string name_;
  int64_t global_optimizer_offset_;
  std::vector<int64_t> local_optimizer_offset_for_vehicle_;
  /// nullptr if not defined.
  std::unique_ptr<SimpleBoundCosts> vehicle_soft_span_upper_bound_;
  std::unique_ptr<SimpleBoundCosts>
      vehicle_quadratic_cost_soft_span_upper_bound_;
  friend class RoutingModel;
  friend class RoutingModelInspector;

  DISALLOW_COPY_AND_ASSIGN(RoutingDimension);
};

/// A decision builder which tries to assign values to variables as close as
/// possible to target values first.
DecisionBuilder* MakeSetValuesFromTargets(Solver* solver,
                                          std::vector<IntVar*> variables,
                                          std::vector<int64_t> targets);

/// Attempts to solve the model using the cp-sat solver. As of 5/2019, will
/// solve the TSP corresponding to the model if it has a single vehicle.
/// Therefore the resulting solution might not actually be feasible. Will return
/// false if a solution could not be found.
bool SolveModelWithSat(const RoutingModel& model,
                       const RoutingSearchParameters& search_parameters,
                       const Assignment* initial_solution,
                       Assignment* solution);

#if !defined(SWIG)
IntVarLocalSearchFilter* MakeVehicleBreaksFilter(
    const RoutingModel& routing_model, const RoutingDimension& dimension);

// A decision builder that monitors solutions, and tries to fix dimension
// variables whose route did not change in the candidate solution.
// Dimension variables are Cumul, Slack and break variables of all dimensions.
// The user must make sure that those variables will be always be fixed at
// solution, typically by composing another DecisionBuilder after this one.
// If this DecisionBuilder returns a non-nullptr value at some node of the
// search tree, it will always return nullptr in the subtree of that node.
// Moreover, the decision will be a simultaneous assignment of the dimension
// variables of unchanged routes on the left branch, and an empty decision on
// the right branch.
DecisionBuilder* MakeRestoreDimensionValuesForUnchangedRoutes(
    RoutingModel* model);
#endif

}  // namespace operations_research
#endif  // OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_H_
