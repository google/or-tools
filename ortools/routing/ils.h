// Copyright 2010-2024 Google LLC
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

#ifndef OR_TOOLS_ROUTING_ILS_H_
#define OR_TOOLS_ROUTING_ILS_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <random>
#include <utility>
#include <vector>

#include "absl/time/time.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/routing/ils.pb.h"
#include "ortools/routing/parameters.pb.h"
#include "ortools/routing/routing.h"
#include "ortools/util/bitset.h"

namespace operations_research::routing {

// Wraps a routing assignment providing extra features.
class RoutingSolution {
 public:
  explicit RoutingSolution(const RoutingModel& model);

  // Initializes the routing solution for the given assignment.
  void Reset(const Assignment* assignment);

  // Initializes next and prev pointers for the route served by the given
  // vehicle, if not already done.
  void InitializeRouteInfoIfNeeded(int vehicle);

  // Returns whether node_index belongs to a route that has been initialized.
  bool BelongsToInitializedRoute(int64_t node_index) const;

  // Returns the next node index of the given node_index.
  int64_t GetNextNodeIndex(int64_t node_index) const;

  // Returns the previous node index of the given node_index.
  // This must be called for node_index belonging to initialized routes.
  int64_t GetInitializedPrevNodeIndex(int64_t node_index) const;

  // Returns the number of visits performed by the given vehicle.
  // This must be called for a vehicle associated with an initialized route.
  int GetRouteSize(int vehicle) const;

  // Returns whether node_index can be removed from the solution.
  // This must be called for node_index belonging to initialized routes.
  bool CanBeRemoved(int64_t node_index) const;

  // Removes the node with the given node_index.
  // This must be called for node_index belonging to initialized routes.
  void RemoveNode(int64_t node_index);

  // Removes the performed sibling pickup or delivery of customer, if any.
  void RemovePerformedPickupDeliverySibling(int64_t customer);

  // Randomly returns the next or previous visit of the given performed
  // visit. Returns -1 if there are no other available visits. When the
  // selected adjacent vertex is a vehicle start/end, we always pick the
  // visit in the opposite direction.
  // This must be called for a performed visit belonging to an initialized
  // route.
  int64_t GetRandomAdjacentVisit(
      int64_t visit, std::mt19937& rnd,
      std::bernoulli_distribution& boolean_dist) const;

 private:
  const RoutingModel& model_;
  std::vector<int64_t> nexts_;
  std::vector<int64_t> prevs_;
  std::vector<int> route_sizes_;

  // Assignment that the routing solution refers to. It's changed at every
  // Reset call.
  const Assignment* assignment_ = nullptr;
};

// Ruin interface.
class RuinProcedure {
 public:
  virtual ~RuinProcedure() = default;

  // Returns next accessors describing the ruined solution.
  virtual std::function<int64_t(int64_t)> Ruin(
      const Assignment* assignment) = 0;
};

// Removes a number of routes that are spatially close together.
class CloseRoutesRemovalRuinProcedure : public RuinProcedure {
 public:
  CloseRoutesRemovalRuinProcedure(RoutingModel* model, std::mt19937* rnd,
                                  size_t num_routes,
                                  int num_neighbors_for_route_selection);
  // Returns next accessors where at most num_routes routes have been shortcut,
  // i.e., next(shortcut route begin) = shortcut route end.
  // Next accessors for customers belonging to shortcut routes are still set to
  // their original value and should not be used.
  std::function<int64_t(int64_t)> Ruin(const Assignment* assignment) override;

 private:
  const RoutingModel& model_;
  const RoutingModel::NodeNeighborsByCostClass* const neighbors_manager_;
  const size_t num_routes_;
  std::mt19937& rnd_;
  std::uniform_int_distribution<int64_t> customer_dist_;
  SparseBitset<int64_t> removed_routes_;
};

// Removes a number of non start/end nodes by performing a random walk on the
// routing solution graph described by the assignment.
// Note that the removal of a pickup and delivery counts as the removal of a
// single entity.
class RandomWalkRemovalRuinProcedure : public RuinProcedure {
 public:
  RandomWalkRemovalRuinProcedure(RoutingModel* model, std::mt19937* rnd,
                                 int walk_length,
                                 int num_neighbors_for_route_selection);

  std::function<int64_t(int64_t)> Ruin(const Assignment* assignment) override;

 private:
  // Returns the next node towards which the random walk is extended.
  int64_t GetNextNodeToRemove(const Assignment* assignment, int node);

  const RoutingModel& model_;
  RoutingSolution routing_solution_;
  const RoutingModel::NodeNeighborsByCostClass* const neighbors_manager_;
  std::mt19937& rnd_;
  const int walk_length_;
  std::uniform_int_distribution<int64_t> customer_dist_;
  std::bernoulli_distribution boolean_dist_;
};

// Applies one or more ruin procedures according to the selected composition
// strategy.
class CompositeRuinProcedure : public RuinProcedure {
 public:
  // Composition strategy interface.
  class CompositionStrategy {
   public:
    explicit CompositionStrategy(std::vector<RuinProcedure*> ruin_procedures);
    virtual ~CompositionStrategy() = default;

    // Returns the selected ruin procedures.
    virtual const std::vector<RuinProcedure*>& Select() = 0;

   protected:
    // Contains ptrs to the available ruins.
    std::vector<RuinProcedure*> ruins_;
  };

  CompositeRuinProcedure(
      RoutingModel* model,
      std::vector<std::unique_ptr<RuinProcedure>> ruin_procedures,
      RuinCompositionStrategy::Value composition_strategy, std::mt19937* rnd);

  std::function<int64_t(int64_t)> Ruin(const Assignment* assignment) override;

 private:
  // Creates a new assignment from the given next accessor.
  const Assignment* BuildAssignmentFromNextAccessor(
      const std::function<int64_t(int64_t)>& next_accessor);

  const RoutingModel& model_;
  std::vector<std::unique_ptr<RuinProcedure>> ruin_procedures_;
  std::unique_ptr<CompositionStrategy> composition_strategy_;

  // Used by BuildAssignmentFromNextAccessor to rebuild a proper assignment
  // from next accessors. Stored at the object level to minimize re-allocations.
  Assignment* ruined_assignment_;
  Assignment* next_assignment_;
};

// Returns a DecisionBuilder implementing a perturbation step of an Iterated
// Local Search approach.
DecisionBuilder* MakePerturbationDecisionBuilder(
    const RoutingSearchParameters& parameters, RoutingModel* model,
    std::mt19937* rnd, const Assignment* assignment,
    std::function<bool()> stop_search,
    LocalSearchFilterManager* filter_manager);

// Neighbor acceptance criterion interface.
class NeighborAcceptanceCriterion {
 public:
  // Representation of the search process state.
  struct SearchState {
    // Search duration.
    absl::Duration duration;
    // Explored solutions.
    int64_t solutions;
  };

  virtual ~NeighborAcceptanceCriterion() = default;
  // Returns whether `candidate` should replace `reference` given the provided
  // search state.
  virtual bool Accept(const SearchState& search_state,
                      const Assignment* candidate,
                      const Assignment* reference) = 0;
};

// Returns a neighbor acceptance criterion based on the given parameters.
std::unique_ptr<NeighborAcceptanceCriterion> MakeNeighborAcceptanceCriterion(
    const RoutingModel& model, const RoutingSearchParameters& parameters,
    std::mt19937* rnd);

// Returns initial and final simulated annealing temperatures according to the
// given simulated annealing input parameters.
std::pair<double, double> GetSimulatedAnnealingTemperatures(
    const RoutingModel& model, const SimulatedAnnealingParameters& sa_params,
    std::mt19937* rnd);

}  // namespace operations_research::routing

#endif  // OR_TOOLS_ROUTING_ILS_H_
