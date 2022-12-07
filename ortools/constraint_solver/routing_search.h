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

#ifndef OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_SEARCH_H_
#define OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_SEARCH_H_

#include <sys/types.h>

#include <algorithm>
#include <deque>
#include <functional>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "ortools/base/adjustable_priority_queue.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"
#include "ortools/base/mathutil.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "ortools/constraint_solver/routing.h"
#include "ortools/constraint_solver/routing_index_manager.h"
#include "ortools/util/bitset.h"

namespace operations_research {

class IntVarFilteredHeuristic;
#ifndef SWIG
/// Helper class that manages vehicles. This class is based on the
/// RoutingModel::VehicleTypeContainer that sorts and stores vehicles based on
/// their "type".
class VehicleTypeCurator {
 public:
  explicit VehicleTypeCurator(
      const RoutingModel::VehicleTypeContainer& vehicle_type_container)
      : vehicle_type_container_(&vehicle_type_container) {}

  int NumTypes() const { return vehicle_type_container_->NumTypes(); }

  int Type(int vehicle) const { return vehicle_type_container_->Type(vehicle); }

  /// Resets the vehicles stored, storing only vehicles from the
  /// vehicle_type_container_ for which store_vehicle() returns true.
  void Reset(const std::function<bool(int)>& store_vehicle);

  /// Goes through all the currently stored vehicles and removes vehicles for
  /// which remove_vehicle() returns true.
  void Update(const std::function<bool(int)>& remove_vehicle);

  int GetLowestFixedCostVehicleOfType(int type) const {
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
                              int64_t fixed_cost) {
    std::vector<int>& vehicles = vehicles_per_vehicle_class_[vehicle_class];
    if (vehicles.empty()) {
      /// Add the vehicle class entry to the set (it was removed when
      /// vehicles_per_vehicle_class_[vehicle_class] got empty).
      std::set<VehicleClassEntry>& vehicle_classes =
          sorted_vehicle_classes_per_type_[Type(vehicle)];
      const auto& insertion =
          vehicle_classes.insert({vehicle_class, fixed_cost});
      DCHECK(insertion.second);
    }
    vehicles.push_back(vehicle);
  }

  /// Searches a compatible vehicle of the given type; returns false if none was
  /// found.
  bool HasCompatibleVehicleOfType(
      int type, const std::function<bool(int)>& vehicle_is_compatible) const;
  /// Searches for the best compatible vehicle of the given type, i.e. the first
  /// vehicle v of type 'type' for which vehicle_is_compatible(v) returns true.
  /// If a compatible *vehicle* is found, its index is removed from
  /// vehicles_per_vehicle_class_ and the function returns {vehicle, -1}.
  /// If for some *vehicle* 'stop_and_return_vehicle' returns true before a
  /// compatible vehicle is found, the function simply returns {-1, vehicle}.
  /// Returns {-1, -1} if no compatible vehicle is found and the stopping
  /// condition is never met.
  std::pair<int, int> GetCompatibleVehicleOfType(
      int type, const std::function<bool(int)>& vehicle_is_compatible,
      const std::function<bool(int)>& stop_and_return_vehicle);

 private:
  using VehicleClassEntry =
      RoutingModel::VehicleTypeContainer::VehicleClassEntry;
  const RoutingModel::VehicleTypeContainer* const vehicle_type_container_;
  // clang-format off
  std::vector<std::set<VehicleClassEntry> > sorted_vehicle_classes_per_type_;
  std::vector<std::vector<int> > vehicles_per_vehicle_class_;
  // clang-format on
};

/// Returns the best value for the automatic first solution strategy, based on
/// the given model parameters.
operations_research::FirstSolutionStrategy::Value
AutomaticFirstSolutionStrategy(bool has_pickup_deliveries,
                               bool has_node_precedences,
                               bool has_single_vehicle_node);

/// Computes and returns the first node in the end chain of each vehicle in the
/// model, based on the current bound NextVar values.
std::vector<int64_t> ComputeVehicleEndChainStarts(const RoutingModel& model);

/// Decision builder building a solution using heuristics with local search
/// filters to evaluate its feasibility. This is very fast but can eventually
/// fail when the solution is restored if filters did not detect all
/// infeasiblities.
/// More details:
/// Using local search filters to build a solution. The approach is pretty
/// straight-forward: have a general assignment storing the current solution,
/// build delta assignment representing possible extensions to the current
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
  int64_t number_of_decisions() const;
  int64_t number_of_rejects() const;

 private:
  const std::unique_ptr<IntVarFilteredHeuristic> heuristic_;
};

/// Generic filter-based heuristic applied to IntVars.
class IntVarFilteredHeuristic {
 public:
  IntVarFilteredHeuristic(Solver* solver, const std::vector<IntVar*>& vars,
                          const std::vector<IntVar*>& secondary_vars,
                          LocalSearchFilterManager* filter_manager);

  virtual ~IntVarFilteredHeuristic() = default;

  /// Builds a solution. Returns the resulting assignment if a solution was
  /// found, and nullptr otherwise.
  Assignment* const BuildSolution();

  /// Returns statistics on search, number of decisions sent to filters, number
  /// of decisions rejected by filters.
  int64_t number_of_decisions() const { return number_of_decisions_; }
  int64_t number_of_rejects() const { return number_of_rejects_; }

  virtual std::string DebugString() const { return "IntVarFilteredHeuristic"; }

 protected:
  /// Resets the data members for a new solution.
  void ResetSolution();
  /// Initialize the heuristic; called before starting to build a new solution.
  virtual void Initialize() {}
  /// Virtual method to initialize the solution.
  virtual bool InitializeSolution() { return true; }
  /// Virtual method to redefine how to build a solution.
  virtual bool BuildSolutionInternal() = 0;
  /// Evaluates the modifications to the current solution. If these
  /// modifications are "filter-feasible" returns their corresponding cost
  /// computed by filters.
  /// If 'commit' is true, the modifications are committed to the current
  /// solution.
  /// In any case all modifications to the internal delta are cleared before
  /// returning.
  std::optional<int64_t> Evaluate(bool commit);
  /// Returns true if the search must be stopped.
  virtual bool StopSearch() { return false; }
  /// Modifies the current solution by setting the variable of index 'index' to
  /// value 'value'.
  void SetValue(int64_t index, int64_t value) {
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
  int64_t Value(int64_t index) const {
    return assignment_->IntVarContainer().Element(index).Value();
  }
  /// Returns true if the variable of index 'index' is in the current solution.
  bool Contains(int64_t index) const {
    return assignment_->IntVarContainer().Element(index).Var() != nullptr;
  }
  /// Returns the number of variables the decision builder is trying to
  /// instantiate.
  int Size() const { return vars_.size(); }
  /// Returns the variable of index 'index'.
  IntVar* Var(int64_t index) const { return vars_[index]; }
  /// Returns the index of a secondary var.
  int64_t SecondaryVarIndex(int64_t index) const {
    DCHECK(HasSecondaryVars());
    return index + base_vars_size_;
  }
  /// Returns true if there are secondary variables.
  bool HasSecondaryVars() const { return base_vars_size_ != vars_.size(); }
  /// Returns true if 'index' is a secondary variable index.
  bool IsSecondaryVar(int64_t index) const { return index >= base_vars_size_; }
  /// Synchronizes filters with an assignment (the current solution).
  void SynchronizeFilters();

  Assignment* const assignment_;

 private:
  /// Checks if filters accept a given modification to the current solution
  /// (represented by delta).
  bool FilterAccept();

  Solver* solver_;
  std::vector<IntVar*> vars_;
  const int base_vars_size_;
  Assignment* const delta_;
  std::vector<int> delta_indices_;
  std::vector<bool> is_in_delta_;
  Assignment* const empty_;
  LocalSearchFilterManager* filter_manager_;
  int64_t objective_upper_bound_;
  /// Stats on search
  int64_t number_of_decisions_;
  int64_t number_of_rejects_;
};

/// Filter-based heuristic dedicated to routing.
class RoutingFilteredHeuristic : public IntVarFilteredHeuristic {
 public:
  RoutingFilteredHeuristic(RoutingModel* model,
                           std::function<bool()> stop_search,
                           LocalSearchFilterManager* filter_manager,
                           bool omit_secondary_vars = true);
  ~RoutingFilteredHeuristic() override = default;
  /// Builds a solution starting from the routes formed by the next accessor.
  const Assignment* BuildSolutionFromRoutes(
      const std::function<int64_t(int64_t)>& next_accessor);
  RoutingModel* model() const { return model_; }
  /// Returns the end of the start chain of vehicle,
  int GetStartChainEnd(int vehicle) const { return start_chain_ends_[vehicle]; }
  /// Returns the start of the end chain of vehicle,
  int GetEndChainStart(int vehicle) const { return end_chain_starts_[vehicle]; }
  /// Make nodes in the same disjunction as 'node' unperformed. 'node' is a
  /// variable index corresponding to a node.
  void MakeDisjunctionNodesUnperformed(int64_t node);
  /// Make all unassigned nodes unperformed, always returns true.
  bool MakeUnassignedNodesUnperformed();
  /// Make all partially performed pickup and delivery pairs unperformed. A
  /// pair is partially unperformed if one element of the pair has one of its
  /// alternatives performed in the solution and the other has no alternatives
  /// in the solution or none performed.
  void MakePartiallyPerformedPairsUnperformed();

 protected:
  bool StopSearch() override { return stop_search_(); }
  virtual void SetVehicleIndex(int64_t /*node*/, int /*vehicle*/) {}
  virtual void ResetVehicleIndices() {}
  bool VehicleIsEmpty(int vehicle) const {
    return Value(model()->Start(vehicle)) == model()->End(vehicle);
  }

 private:
  /// Initializes the current solution with empty or partial vehicle routes.
  bool InitializeSolution() override;

  RoutingModel* const model_;
  std::function<bool()> stop_search_;
  std::vector<int64_t> start_chain_ends_;
  std::vector<int64_t> end_chain_starts_;
};

class CheapestInsertionFilteredHeuristic : public RoutingFilteredHeuristic {
 public:
  /// Takes ownership of evaluator.
  CheapestInsertionFilteredHeuristic(
      RoutingModel* model, std::function<bool()> stop_search,
      std::function<int64_t(int64_t, int64_t, int64_t)> evaluator,
      std::function<int64_t(int64_t)> penalty_evaluator,
      LocalSearchFilterManager* filter_manager);
  ~CheapestInsertionFilteredHeuristic() override = default;

 protected:
  struct NodeInsertion {
    int64_t insert_after;
    int vehicle;
    int64_t value;

    bool operator<(const NodeInsertion& other) const {
      return std::tie(value, insert_after, vehicle) <
             std::tie(other.value, other.insert_after, other.vehicle);
    }
  };
  struct StartEndValue {
    int64_t num_allowed_vehicles;
    int64_t distance;
    int vehicle;

    bool operator<(const StartEndValue& other) const {
      return std::tie(num_allowed_vehicles, distance, vehicle) <
             std::tie(other.num_allowed_vehicles, other.distance,
                      other.vehicle);
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

  /// Inserts 'node' just after 'predecessor', and just before 'successor' on
  /// the route of 'vehicle', resulting in the following subsequence:
  /// predecessor -> node -> successor.
  /// If 'node' is part of a disjunction, other nodes of the disjunction are
  /// made unperformed.
  void InsertBetween(int64_t node, int64_t predecessor, int64_t successor,
                     int vehicle = -1);
  /// Helper method to the ComputeEvaluatorSortedPositions* methods. Finds all
  /// possible insertion positions of node 'node_to_insert' in the partial route
  /// starting at node 'start' and adds them to 'node_insertions' (no sorting is
  /// done). If ignore_cost is true, insertion costs may not be computed.
  void AppendInsertionPositionsAfter(
      int64_t node_to_insert, int64_t start, int64_t next_after_start,
      int vehicle, bool ignore_cost,
      std::vector<NodeInsertion>* node_insertions);
  /// Returns the cost of inserting 'node_to_insert' between 'insert_after' and
  /// 'insert_before' on the 'vehicle', i.e.
  /// Cost(insert_after-->node) + Cost(node-->insert_before)
  /// - Cost (insert_after-->insert_before).
  // TODO(user): Replace 'insert_before' and 'insert_after' by 'predecessor'
  // and 'successor' in the code.
  int64_t GetInsertionCostForNodeAtPosition(int64_t node_to_insert,
                                            int64_t insert_after,
                                            int64_t insert_before,
                                            int vehicle) const;
  /// Returns the cost of unperforming node 'node_to_insert'. Returns kint64max
  /// if penalty callback is null or if the node cannot be unperformed.
  int64_t GetUnperformedValue(int64_t node_to_insert) const;

  std::function<int64_t(int64_t, int64_t, int64_t)> evaluator_;
  std::function<int64_t(int64_t)> penalty_evaluator_;
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
    /// neighbors leading to the smallest arc costs are considered for
    /// insertions, with a minimum of 'min_neighbors':
    /// num_closest_neighbors = max(min_neighbors, neighbors_ratio*N),
    /// where N is the number of non-start/end nodes in the model.
    double neighbors_ratio;
    int64_t min_neighbors;
    /// If true, only closest neighbors (see neighbors_ratio and min_neighbors)
    /// are considered as insertion positions during initialization. Otherwise,
    /// all possible insertion positions are considered.
    bool use_neighbors_ratio_for_initialization;
    /// If true, entries are created for making the nodes/pairs unperformed, and
    /// when the cost of making a node unperformed is lower than all insertions,
    /// the node/pair will be made unperformed. If false, only entries making
    /// a node/pair performed are considered.
    bool add_unperformed_entries;
  };

  /// Takes ownership of evaluators.
  GlobalCheapestInsertionFilteredHeuristic(
      RoutingModel* model, std::function<bool()> stop_search,
      std::function<int64_t(int64_t, int64_t, int64_t)> evaluator,
      std::function<int64_t(int64_t)> penalty_evaluator,
      LocalSearchFilterManager* filter_manager,
      GlobalCheapestInsertionParameters parameters);
  ~GlobalCheapestInsertionFilteredHeuristic() override = default;
  bool BuildSolutionInternal() override;
  std::string DebugString() const override {
    return "GlobalCheapestInsertionFilteredHeuristic";
  }

 private:
  /// Priority queue entries used by global cheapest insertion heuristic.
  class NodeEntryQueue;

  /// Entry in priority queue containing the insertion positions of a node pair.
  class PairEntry {
   public:
    PairEntry(int pickup_to_insert, int pickup_insert_after,
              int delivery_to_insert, int delivery_insert_after, int vehicle,
              int64_t bucket)
        : value_(std::numeric_limits<int64_t>::max()),
          heap_index_(-1),
          pickup_to_insert_(pickup_to_insert),
          pickup_insert_after_(pickup_insert_after),
          delivery_to_insert_(delivery_to_insert),
          delivery_insert_after_(delivery_insert_after),
          vehicle_(vehicle),
          bucket_(bucket) {}
    // Note: for compatibility reasons, comparator follows tie-breaking rules
    // used in the first version of GlobalCheapestInsertion.
    bool operator<(const PairEntry& other) const {
      // We give higher priority to insertions from lower buckets.
      if (bucket_ != other.bucket_) {
        return bucket_ > other.bucket_;
      }
      // We then compare by value, then we favor insertions (vehicle != -1).
      // The rest of the tie-breaking is done with std::tie.
      if (value_ != other.value_) {
        return value_ > other.value_;
      }
      if ((vehicle_ == -1) ^ (other.vehicle_ == -1)) {
        return vehicle_ == -1;
      }
      return std::tie(pickup_insert_after_, pickup_to_insert_,
                      delivery_insert_after_, delivery_to_insert_, vehicle_) >
             std::tie(other.pickup_insert_after_, other.pickup_to_insert_,
                      other.delivery_insert_after_, other.delivery_to_insert_,
                      other.vehicle_);
    }
    void SetHeapIndex(int h) { heap_index_ = h; }
    int GetHeapIndex() const { return heap_index_; }
    void set_value(int64_t value) { value_ = value; }
    int pickup_to_insert() const { return pickup_to_insert_; }
    int pickup_insert_after() const { return pickup_insert_after_; }
    void set_pickup_insert_after(int pickup_insert_after) {
      pickup_insert_after_ = pickup_insert_after;
    }
    int delivery_to_insert() const { return delivery_to_insert_; }
    int delivery_insert_after() const { return delivery_insert_after_; }
    int vehicle() const { return vehicle_; }
    void set_vehicle(int vehicle) { vehicle_ = vehicle; }

   private:
    int64_t value_;
    int heap_index_;
    int pickup_to_insert_;
    int pickup_insert_after_;
    int delivery_to_insert_;
    int delivery_insert_after_;
    int vehicle_;
    int64_t bucket_;
  };

  typedef absl::flat_hash_set<PairEntry*> PairEntries;

  /// Priority queue entry allocator.
  template <typename T>
  class EntryAllocator {
   public:
    EntryAllocator() {}
    void Clear() {
      entries_.clear();
      free_entries_.clear();
    }
    template <typename... Args>
    T* NewEntry(const Args&... args) {
      if (!free_entries_.empty()) {
        auto* entry = free_entries_.back();
        free_entries_.pop_back();
        *entry = T(args...);
        return entry;
      } else {
        entries_.emplace_back(args...);
        return &entries_.back();
      }
    }
    void FreeEntry(T* entry) { free_entries_.push_back(entry); }

   private:
    /// std::deque references to elements are stable when extended.
    std::deque<T> entries_;
    std::vector<T*> free_entries_;
  };

  /// Inserts non-inserted single nodes or pickup/delivery pairs which have a
  /// visit type in the type requirement graph, i.e. required for or requiring
  /// another type for insertions.
  /// These nodes are inserted iff the requirement graph is acyclic, in which
  /// case nodes are inserted based on the topological order of their type,
  /// given by the routing model's GetTopologicallySortedVisitTypes() method.
  bool InsertPairsAndNodesByRequirementTopologicalOrder();

  /// Inserts non-inserted pickup and delivery pairs. Maintains a priority
  /// queue of possible pair insertions, which is incrementally updated when a
  /// pair insertion is committed. Incrementality is obtained by updating pair
  /// insertion positions on the four newly modified route arcs: after the
  /// pickup insertion position, after the pickup position, after the delivery
  /// insertion position and after the delivery position.
  bool InsertPairs(
      const std::map<int64_t, std::vector<int>>& pair_indices_by_bucket);

  /// Returns true iff the empty_vehicle_type_curator_ should be used to insert
  /// nodes/pairs on the given vehicle, i.e. iff the route of the given vehicle
  /// is empty and 'all_vehicles' is true.
  bool UseEmptyVehicleTypeCuratorForVehicle(int vehicle,
                                            bool all_vehicles = true) {
    return vehicle >= 0 && VehicleIsEmpty(vehicle) && all_vehicles;
  }

  /// Tries to insert the pickup/delivery pair on a vehicle of the same type and
  /// same fixed cost as the pair_entry.vehicle() using the
  /// empty_vehicle_type_curator_, and updates the pair_entry accordingly if the
  /// insertion was not possible.
  /// NOTE: Assumes (DCHECKS) that
  /// UseEmptyVehicleTypeCuratorForVehicle(pair_entry.vehicle()) is true.
  bool InsertPairEntryUsingEmptyVehicleTypeCurator(
      const absl::flat_hash_set<int>& pair_indices, PairEntry* const pair_entry,
      AdjustablePriorityQueue<PairEntry>* priority_queue,
      std::vector<PairEntries>* pickup_to_entries,
      std::vector<PairEntries>* delivery_to_entries);

  /// Inserts non-inserted individual nodes on the given routes (or all routes
  /// if "vehicles" is an empty vector), by constructing routes in parallel.
  /// Maintains a priority queue of possible insertions, which is incrementally
  /// updated when an insertion is committed.
  /// Incrementality is obtained by updating insertion positions on the two
  /// newly modified route arcs: after the node insertion position and after the
  /// node position.
  bool InsertNodesOnRoutes(
      const std::map<int64_t, std::vector<int>>& nodes_by_bucket,
      const absl::flat_hash_set<int>& vehicles);

  /// Tries to insert the node_entry.node_to_insert on a vehicle of the same
  /// type and same fixed cost as the node_entry.vehicle() using the
  /// empty_vehicle_type_curator_, and updates the node_entry accordingly if the
  /// insertion was not possible.
  /// NOTE: Assumes (DCHECKS) that
  /// UseEmptyVehicleTypeCuratorForVehicle(node_entry.vehicle(), all_vehicles)
  /// is true.
  bool InsertNodeEntryUsingEmptyVehicleTypeCurator(
      const std::vector<bool>& nodes, bool all_vehicles, NodeEntryQueue* queue);

  /// Inserts non-inserted individual nodes on routes by constructing routes
  /// sequentially.
  /// For each new route, the vehicle to use and the first node to insert on it
  /// are given by calling InsertSeedNode(). The route is then completed with
  /// other nodes by calling InsertNodesOnRoutes({vehicle}).
  bool SequentialInsertNodes(
      const std::map<int64_t, std::vector<int>>& nodes_by_bucket);

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

  /// Initializes the priority queue and the pair entries for the given pair
  /// indices with the current state of the solution.
  bool InitializePairPositions(
      const absl::flat_hash_set<int>& pair_indices,
      AdjustablePriorityQueue<PairEntry>* priority_queue,
      std::vector<PairEntries>* pickup_to_entries,
      std::vector<PairEntries>* delivery_to_entries);
  /// Adds insertion entries performing the 'pickup' and 'delivery', and updates
  /// 'priority_queue', pickup_to_entries and delivery_to_entries accordingly.
  /// Based on gci_params_.use_neighbors_ratio_for_initialization, either all
  /// contained nodes are considered as insertion positions, or only the
  /// closest neighbors of 'pickup' and/or 'delivery'.
  void InitializeInsertionEntriesPerformingPair(
      int64_t pickup, int64_t delivery,
      AdjustablePriorityQueue<PairEntry>* priority_queue,
      std::vector<PairEntries>* pickup_to_entries,
      std::vector<PairEntries>* delivery_to_entries);
  /// Performs all the necessary updates after a pickup/delivery pair was
  /// successfully inserted on the 'vehicle', respectively after
  /// 'pickup_position' and 'delivery_position'.
  bool UpdateAfterPairInsertion(
      const absl::flat_hash_set<int>& pair_indices, int vehicle, int64_t pickup,
      int64_t pickup_position, int64_t delivery, int64_t delivery_position,
      AdjustablePriorityQueue<PairEntry>* priority_queue,
      std::vector<PairEntries>* pickup_to_entries,
      std::vector<PairEntries>* delivery_to_entries);
  /// Updates all existing pair entries inserting a node after nodes of the
  /// chain starting at 'insert_after_start' and ending before
  /// 'insert_after_end', and updates the priority queue accordingly.
  bool UpdateExistingPairEntriesOnChain(
      int64_t insert_after_start, int64_t insert_after_end,
      AdjustablePriorityQueue<PairEntry>* priority_queue,
      std::vector<PairEntries>* pickup_to_entries,
      std::vector<PairEntries>* delivery_to_entries);
  /// Adds pair entries inserting either a pickup or a delivery after
  /// "insert_after". When inserting pickups after "insert_after", will skip
  /// entries inserting their delivery after
  /// "skip_entries_inserting_delivery_after". This can be used to avoid the
  /// insertion of redundant entries.
  bool AddPairEntriesAfter(const absl::flat_hash_set<int>& pair_indices,
                           int vehicle, int64_t insert_after,
                           int64_t skip_entries_inserting_delivery_after,
                           AdjustablePriorityQueue<PairEntry>* priority_queue,
                           std::vector<PairEntries>* pickup_to_entries,
                           std::vector<PairEntries>* delivery_to_entries) {
    return AddPairEntriesWithDeliveryAfter(pair_indices, vehicle, insert_after,
                                           priority_queue, pickup_to_entries,
                                           delivery_to_entries) &&
           AddPairEntriesWithPickupAfter(pair_indices, vehicle, insert_after,
                                         skip_entries_inserting_delivery_after,
                                         priority_queue, pickup_to_entries,
                                         delivery_to_entries);
  }
  /// Adds pair entries inserting a pickup after node "insert_after" and a
  /// delivery in a position after the pickup, and updates the priority queue
  /// accordingly.
  /// Will skip entries inserting their delivery after
  /// "skip_entries_inserting_delivery_after". This can be used to avoid the
  /// insertion of redundant entries.
  bool AddPairEntriesWithPickupAfter(
      const absl::flat_hash_set<int>& pair_indices, int vehicle,
      int64_t insert_after, int64_t skip_entries_inserting_delivery_after,
      AdjustablePriorityQueue<PairEntry>* priority_queue,
      std::vector<PairEntries>* pickup_to_entries,
      std::vector<PairEntries>* delivery_to_entries);
  /// Adds pair entries inserting a delivery after node "insert_after" and a
  /// pickup in a position before "insert_after", and updates the priority queue
  /// accordingly.
  bool AddPairEntriesWithDeliveryAfter(
      const absl::flat_hash_set<int>& pair_indices, int vehicle,
      int64_t insert_after, AdjustablePriorityQueue<PairEntry>* priority_queue,
      std::vector<PairEntries>* pickup_to_entries,
      std::vector<PairEntries>* delivery_to_entries);
  /// Deletes an entry, removing it from the priority queue and the appropriate
  /// pickup and delivery entry sets.
  void DeletePairEntry(PairEntry* entry,
                       AdjustablePriorityQueue<PairEntry>* priority_queue,
                       std::vector<PairEntries>* pickup_to_entries,
                       std::vector<PairEntries>* delivery_to_entries);
  /// Creates a PairEntry corresponding to the insertion of 'pickup' and
  /// 'delivery' respectively after 'pickup_insert_after' and
  /// 'delivery_insert_after', and adds it to the 'priority_queue',
  /// 'pickup_entries' and 'delivery_entries'.
  void AddPairEntry(int64_t pickup, int64_t pickup_insert_after,
                    int64_t delivery, int64_t delivery_insert_after,
                    int vehicle,
                    AdjustablePriorityQueue<PairEntry>* priority_queue,
                    std::vector<PairEntries>* pickup_entries,
                    std::vector<PairEntries>* delivery_entries) const;
  /// Updates the pair entry's value and rearranges the priority queue
  /// accordingly.
  void UpdatePairEntry(
      PairEntry* const pair_entry,
      AdjustablePriorityQueue<PairEntry>* priority_queue) const;
  /// Computes and returns the insertion value of inserting 'pickup' and
  /// 'delivery' respectively after 'pickup_insert_after' and
  /// 'delivery_insert_after' on 'vehicle'.
  int64_t GetInsertionValueForPairAtPositions(int64_t pickup,
                                              int64_t pickup_insert_after,
                                              int64_t delivery,
                                              int64_t delivery_insert_after,
                                              int vehicle) const;

  /// Initializes the priority queue and the node entries with the current state
  /// of the solution on the given vehicle routes.
  bool InitializePositions(const std::vector<bool>& nodes,
                           const absl::flat_hash_set<int>& vehicles,
                           NodeEntryQueue* queue);
  /// Adds insertion entries performing 'node', and updates 'queue' and
  /// position_to_node_entries accordingly.
  /// Based on gci_params_.use_neighbors_ratio_for_initialization, either all
  /// contained nodes are considered as insertion positions, or only the
  /// closest neighbors of 'node'.
  void InitializeInsertionEntriesPerformingNode(
      int64_t node, const absl::flat_hash_set<int>& vehicles,
      NodeEntryQueue* queue);
  /// Performs all the necessary updates after 'node' was successfully inserted
  /// on the 'vehicle' after 'insert_after'.
  bool UpdateAfterNodeInsertion(const std::vector<bool>& nodes, int vehicle,
                                int64_t node, int64_t insert_after,
                                bool all_vehicles, NodeEntryQueue* queue);
  /// Updates all existing node entries inserting a node after nodes of the
  /// chain starting at 'insert_after_start' and ending before
  /// 'insert_after_end', and updates the priority queue accordingly.
  bool UpdateExistingNodeEntriesOnChain(const std::vector<bool>& nodes,
                                        int vehicle, int64_t insert_after_start,
                                        int64_t insert_after_end,
                                        bool all_vehicles,
                                        NodeEntryQueue* queue);
  /// Adds node entries inserting a node after "insert_after" and updates the
  /// priority queue accordingly.
  bool AddNodeEntriesAfter(const std::vector<bool>& nodes, int vehicle,
                           int64_t insert_after, bool all_vehicles,
                           NodeEntryQueue* queue);

  /// Creates a NodeEntry corresponding to the insertion of 'node' after
  /// 'insert_after' on 'vehicle' and adds it to the 'queue' and
  /// 'node_entries'.
  void AddNodeEntry(int64_t node, int64_t insert_after, int vehicle,
                    bool all_vehicles, NodeEntryQueue* queue) const;

  int64_t NumNonStartEndNodes() const {
    return model()->Size() - model()->vehicles();
  }

  int64_t NumNeighbors() const {
    return std::max(gci_params_.min_neighbors,
                    MathUtil::FastInt64Round(gci_params_.neighbors_ratio *
                                             NumNonStartEndNodes()));
  }

  void ResetVehicleIndices() override {
    node_index_to_vehicle_.assign(node_index_to_vehicle_.size(), -1);
  }

  void SetVehicleIndex(int64_t node, int vehicle) override {
    DCHECK_LT(node, node_index_to_vehicle_.size());
    node_index_to_vehicle_[node] = vehicle;
  }

  /// Function that verifies node_index_to_vehicle_ is correctly filled for all
  /// nodes given the current routes.
  bool CheckVehicleIndices() const;

  /// Returns the bucket of a node.
  int64_t GetBucketOfNode(int node) const {
    return model()->VehicleVar(node)->Size();
  }

  /// Returns the bucket of a pair of pickup and delivery alternates.
  int64_t GetBucketOfPair(const RoutingModel::IndexPair& index_pair) const {
    int64_t max_pickup_bucket = 0;
    for (int64_t pickup : index_pair.first) {
      max_pickup_bucket = std::max(max_pickup_bucket, GetBucketOfNode(pickup));
    }
    int64_t max_delivery_bucket = 0;
    for (int64_t delivery : index_pair.second) {
      max_delivery_bucket =
          std::max(max_delivery_bucket, GetBucketOfNode(delivery));
    }
    return std::min(max_pickup_bucket, max_delivery_bucket);
  }

  /// Checks if the search should be stopped (time limit reached), and cleans up
  /// the priority queue if it's the case.
  template <typename T>
  bool StopSearchAndCleanup(AdjustablePriorityQueue<T>* priority_queue) {
    if (!StopSearch()) return false;
    if constexpr (std::is_same_v<T, PairEntry>) {
      pair_entry_allocator_.Clear();
    }
    priority_queue->Clear();
    return true;
  }

  GlobalCheapestInsertionParameters gci_params_;
  /// Stores the vehicle index of each node in the current assignment.
  std::vector<int> node_index_to_vehicle_;

  const RoutingModel::NodeNeighborsByCostClass*
      node_index_to_neighbors_by_cost_class_;

  std::unique_ptr<VehicleTypeCurator> empty_vehicle_type_curator_;

  mutable EntryAllocator<PairEntry> pair_entry_allocator_;
};

// Generates insertion positions respecting structural constraints.
class InsertionGenerator {
 public:
  InsertionGenerator() = default;

  struct PickupDeliveryInsertion {
    int64_t insert_pickup_after;
    int64_t insert_delivery_after;
    int64_t value;
    int vehicle;

    bool operator<(const PickupDeliveryInsertion& other) const {
      return std::tie(value, insert_pickup_after, insert_delivery_after,
                      vehicle) <
             std::tie(other.value, other.insert_pickup_after,
                      other.insert_delivery_after, other.vehicle);
    }
  };
  /// Generates insertions for a pickup and delivery pair in a multitour path:
  /// - a series of pickups may only start if all the deliveries of previous
  ///   pickups have been performed.
  /// - given a maximal pickup*delivery* subsequence, either the pickups or the
  ///   deliveries are symmetric, meaning their order does not matter.
  /// Under these specifications, this method generates all
  /// unique insertions of the given pair that keep the multitour property.
  /// Note that there are several ways one can extend pure pickup and delivery
  /// problems to mixed paired and unpaired visits.
  /// Two extreme views:
  /// - multitour heuristics should consider that only consecutive pickups or
  ///   consecutive deliveries have a symmetry. Heuristics should only break
  ///   those symmetries, in the mixed case inserting a pair in a path of length
  ///   p has O(p²) possibilities.
  /// - multitour heuristics should not consider unpaired visits. Insertions
  ///   are made on the subpath of paired nodes, all extensions to the original
  ///   path that conserve order are equivalent.
  void AppendPickupDeliveryMultitourInsertions(
      int pickup, const std::vector<int>& path,
      const std::vector<bool>& node_is_pickup,
      const std::vector<bool>& node_is_delivery,
      std::vector<PickupDeliveryInsertion>& insertions);

 private:
  // Information[i] describes the insertion between path[i] and path[i+1].
  std::vector<int> next_decrease_;  // next position after a delivery.
  std::vector<int> next_increase_;  // next position after a pickup.
  std::vector<int> prev_decrease_;  // previous position after delivery.
  std::vector<int> prev_increase_;  // previous position after pickup.
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
      RoutingModel* model, std::function<bool()> stop_search,
      std::function<int64_t(int64_t, int64_t, int64_t)> evaluator,
      RoutingSearchParameters::PairInsertionStrategy pair_insertion_strategy,
      LocalSearchFilterManager* filter_manager);
  ~LocalCheapestInsertionFilteredHeuristic() override = default;
  bool BuildSolutionInternal() override;
  std::string DebugString() const override {
    return "LocalCheapestInsertionFilteredHeuristic";
  }

 protected:
  void Initialize() override;

 private:
  /// Computes the possible insertion positions of 'node' and sorts them
  /// according to the current cost evaluator.
  /// 'node' is a variable index corresponding to a node.
  std::vector<NodeInsertion> ComputeEvaluatorSortedPositions(int64_t node);
  /// Like ComputeEvaluatorSortedPositions, subject to the additional
  /// restrictions that the node may only be inserted after node 'start' on the
  /// route. For convenience, this method also needs the node that is right
  /// after 'start' on the route.
  std::vector<NodeInsertion> ComputeEvaluatorSortedPositionsOnRouteAfter(
      int64_t node, int64_t start, int64_t next_after_start, int vehicle);

  /// Computes the possible simultaneous insertion positions of the pair
  /// 'pickup' and 'delivery'. Sorts them according to the current cost
  /// evaluator. If a timeout is detected returns std::nullopt.
  std::optional<std::vector<InsertionGenerator::PickupDeliveryInsertion>>
  ComputeEvaluatorSortedPairPositions(int64_t pickup, int64_t delivery);

  // Tries to insert any alternative of the given pair,
  // ordered by cost of pickup insertion, then by cost of delivery insertion.
  void InsertBestPickupThenDelivery(const RoutingModel::IndexPair& index_pair);
  // Tries to insert any alternative of the given pair,
  // ordered by the sum of pickup and delivery insertion.
  void InsertBestPair(const RoutingModel::IndexPair& index_pair);
  // Tries to insert any alternative of the given pair,
  // at a position that preserves the multitour property,
  // ordered by the sum of pickup and delivery insertion.
  void InsertBestPairMultitour(const RoutingModel::IndexPair& index_pair,
                               const std::vector<bool>& node_is_pickup,
                               const std::vector<bool>& node_is_delivery);
  // Tries to insert a pair at the given location. Returns true iff inserted.
  bool InsertPair(int64_t pickup, int64_t insert_pickup_after, int64_t delivery,
                  int64_t insert_delivery_after, int vehicle);
  // Sets all nodes of pair alternatives as visited.
  void SetIndexPairVisited(const RoutingModel::IndexPair& index_pair);

  bool update_start_end_distances_per_node_;
  std::vector<std::vector<StartEndValue>> start_end_distances_per_node_;
  const RoutingSearchParameters::PairInsertionStrategy pair_insertion_strategy_;
  InsertionGenerator insertion_generator_;

  // Marks whether a node has already been tried for insertion.
  std::vector<bool> visited_;
};

/// Filtered-base decision builder based on the addition heuristic, extending
/// a path from its start node with the cheapest arc.
class CheapestAdditionFilteredHeuristic : public RoutingFilteredHeuristic {
 public:
  CheapestAdditionFilteredHeuristic(RoutingModel* model,
                                    std::function<bool()> stop_search,
                                    LocalSearchFilterManager* filter_manager);
  ~CheapestAdditionFilteredHeuristic() override = default;
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
  std::vector<int64_t> GetPossibleNextsFromIterator(int64_t node,
                                                    Iterator start,
                                                    Iterator end) const {
    const int size = model()->Size();
    std::vector<int64_t> nexts;
    for (Iterator it = start; it != end; ++it) {
      const int64_t next = *it;
      if (next != node && (next >= size || !Contains(next))) {
        nexts.push_back(next);
      }
    }
    return nexts;
  }
  /// Sorts a vector of successors of node.
  virtual void SortSuccessors(int64_t node,
                              std::vector<int64_t>* successors) = 0;
  virtual int64_t FindTopSuccessor(int64_t node,
                                   const std::vector<int64_t>& successors) = 0;
};

/// A CheapestAdditionFilteredHeuristic where the notion of 'cheapest arc'
/// comes from an arc evaluator.
class EvaluatorCheapestAdditionFilteredHeuristic
    : public CheapestAdditionFilteredHeuristic {
 public:
  /// Takes ownership of evaluator.
  EvaluatorCheapestAdditionFilteredHeuristic(
      RoutingModel* model, std::function<bool()> stop_search,
      std::function<int64_t(int64_t, int64_t)> evaluator,
      LocalSearchFilterManager* filter_manager);
  ~EvaluatorCheapestAdditionFilteredHeuristic() override = default;
  std::string DebugString() const override {
    return "EvaluatorCheapestAdditionFilteredHeuristic";
  }

 private:
  /// Next nodes are sorted according to the current evaluator.
  void SortSuccessors(int64_t node, std::vector<int64_t>* successors) override;
  int64_t FindTopSuccessor(int64_t node,
                           const std::vector<int64_t>& successors) override;

  std::function<int64_t(int64_t, int64_t)> evaluator_;
};

/// A CheapestAdditionFilteredHeuristic where the notion of 'cheapest arc'
/// comes from an arc comparator.
class ComparatorCheapestAdditionFilteredHeuristic
    : public CheapestAdditionFilteredHeuristic {
 public:
  /// Takes ownership of evaluator.
  ComparatorCheapestAdditionFilteredHeuristic(
      RoutingModel* model, std::function<bool()> stop_search,
      Solver::VariableValueComparator comparator,
      LocalSearchFilterManager* filter_manager);
  ~ComparatorCheapestAdditionFilteredHeuristic() override = default;
  std::string DebugString() const override {
    return "ComparatorCheapestAdditionFilteredHeuristic";
  }

 private:
  /// Next nodes are sorted according to the current comparator.
  void SortSuccessors(int64_t node, std::vector<int64_t>* successors) override;
  int64_t FindTopSuccessor(int64_t node,
                           const std::vector<int64_t>& successors) override;

  Solver::VariableValueComparator comparator_;
};

/// Filter-based decision builder which builds a solution by using
/// Clarke & Wright's Savings heuristic. For each pair of nodes, the savings
/// value is the difference between the cost of two routes visiting one node
/// each and one route visiting both nodes. Routes are built sequentially, each
/// route being initialized from the pair with the best available savings value
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
                           std::function<bool()> stop_search,
                           SavingsParameters parameters,
                           LocalSearchFilterManager* filter_manager);
  ~SavingsFilteredHeuristic() override;
  bool BuildSolutionInternal() override;

 protected:
  typedef std::pair</*saving*/ int64_t, /*saving index*/ int64_t> Saving;

  template <typename S>
  class SavingsContainer;

  virtual double ExtraSavingsMemoryMultiplicativeFactor() const = 0;

  virtual void BuildRoutesFromSavings() = 0;

  /// Returns the cost class from a saving.
  int64_t GetVehicleTypeFromSaving(const Saving& saving) const {
    return saving.second / size_squared_;
  }
  /// Returns the "before node" from a saving.
  int64_t GetBeforeNodeFromSaving(const Saving& saving) const {
    return (saving.second % size_squared_) / Size();
  }
  /// Returns the "after node" from a saving.
  int64_t GetAfterNodeFromSaving(const Saving& saving) const {
    return (saving.second % size_squared_) % Size();
  }
  /// Returns the saving value from a saving.
  int64_t GetSavingValue(const Saving& saving) const { return saving.first; }

  /// Finds the best available vehicle of type "type" to start a new route to
  /// serve the arc before_node-->after_node.
  /// Since there are different vehicle classes for each vehicle type, each
  /// vehicle class having its own capacity constraints, we go through all
  /// vehicle types (in each case only studying the first available vehicle) to
  /// make sure this Saving is inserted if possible.
  /// If possible, the arc is committed to the best vehicle, and the vehicle
  /// index is returned. If this arc can't be served by any vehicle of this
  /// type, the function returns -1.
  int StartNewRouteWithBestVehicleOfType(int type, int64_t before_node,
                                         int64_t after_node);

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
      std::vector<std::vector<int64_t> >* adjacency_lists);
  // clang-format on

  /// Computes saving values for node pairs (see MaxNumNeighborsPerNode()) and
  /// all vehicle types (see ComputeVehicleTypes()).
  /// The saving index attached to each saving value is an index used to
  /// store and recover the node pair to which the value is linked (cf. the
  /// index conversion methods below).
  /// The computed savings are stored and sorted using the savings_container_.
  /// Returns false if the computation could not be done within the model's
  /// time limit.
  bool ComputeSavings();
  /// Builds a saving from a saving value, a vehicle type and two nodes.
  Saving BuildSaving(int64_t saving, int vehicle_type, int before_node,
                     int after_node) const {
    return std::make_pair(saving, vehicle_type * size_squared_ +
                                      before_node * Size() + after_node);
  }

  /// Computes and returns the maximum number of (closest) neighbors to consider
  /// for each node when computing Savings, based on the neighbors ratio and max
  /// memory usage specified by the savings_params_.
  int64_t MaxNumNeighborsPerNode(int num_vehicle_types) const;

  const SavingsParameters savings_params_;
  int64_t size_squared_;

  friend class SavingsFilteredHeuristicTestPeer;
};

class SequentialSavingsFilteredHeuristic : public SavingsFilteredHeuristic {
 public:
  SequentialSavingsFilteredHeuristic(RoutingModel* model,
                                     std::function<bool()> stop_search,
                                     SavingsParameters parameters,
                                     LocalSearchFilterManager* filter_manager)
      : SavingsFilteredHeuristic(model, std::move(stop_search), parameters,
                                 filter_manager) {}
  ~SequentialSavingsFilteredHeuristic() override = default;
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
                                   std::function<bool()> stop_search,
                                   SavingsParameters parameters,
                                   LocalSearchFilterManager* filter_manager)
      : SavingsFilteredHeuristic(model, std::move(stop_search), parameters,
                                 filter_manager) {}
  ~ParallelSavingsFilteredHeuristic() override = default;
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
  void MergeRoutes(int first_vehicle, int second_vehicle, int64_t before_node,
                   int64_t after_node);

  /// First and last non start/end nodes served by each vehicle.
  std::vector<int64_t> first_node_on_route_;
  std::vector<int64_t> last_node_on_route_;
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
                                std::function<bool()> stop_search,
                                LocalSearchFilterManager* filter_manager,
                                bool use_minimum_matching);
  ~ChristofidesFilteredHeuristic() override = default;
  bool BuildSolutionInternal() override;
  std::string DebugString() const override {
    return "ChristofidesFilteredHeuristic";
  }

 private:
  const bool use_minimum_matching_;
};

/// Class to arrange indices by their distance and their angle from the depot.
/// Used in the Sweep first solution heuristic.
class SweepArranger {
 public:
  explicit SweepArranger(
      const std::vector<std::pair<int64_t, int64_t>>& points);
  virtual ~SweepArranger() {}
  void ArrangeIndices(std::vector<int64_t>* indices);
  void SetSectors(int sectors) { sectors_ = sectors; }

 private:
  std::vector<int> coordinates_;
  int sectors_;

  DISALLOW_COPY_AND_ASSIGN(SweepArranger);
};
#endif  // SWIG

// Returns a DecisionBuilder building a first solution based on the Sweep
// heuristic. Mostly suitable when cost is proportional to distance.
DecisionBuilder* MakeSweepDecisionBuilder(RoutingModel* model,
                                          bool check_assignment);

// Returns a DecisionBuilder making all nodes unperformed.
DecisionBuilder* MakeAllUnperformed(RoutingModel* model);

}  // namespace operations_research

#endif  // OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_SEARCH_H_
