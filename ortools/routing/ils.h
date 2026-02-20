// Copyright 2010-2025 Google LLC
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

#ifndef ORTOOLS_ROUTING_ILS_H_
#define ORTOOLS_ROUTING_ILS_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <random>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/time/time.h"
#include "ortools/constraint_solver/assignment.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/routing/ils.pb.h"
#include "ortools/routing/parameters.pb.h"
#include "ortools/routing/routing.h"
#include "ortools/util/bitset.h"

namespace operations_research::routing {

// Wraps a routing assignment providing extra features.
class Solution {
 public:
  explicit Solution(const Model& model);

  // Initializes the routing solution for the given assignment.
  void Reset(const Assignment* assignment);

  // Initializes next and prev pointers for the route served by the given
  // vehicle, if not already done.
  void InitializeRouteInfoIfNeeded(int vehicle);

  // Returns whether node belongs to a route that has been initialized.
  bool BelongsToInitializedRoute(int64_t node) const;

  // Returns the next node index of the given node.
  int64_t GetNextNodeIndex(int64_t node) const;

  // Returns the previous node index of the given node.
  // This must be called for node belonging to initialized routes.
  int64_t GetInitializedPrevNodeIndex(int64_t node) const;

  // Returns the number of visits performed by the given vehicle.
  // This must be called for a vehicle associated with an initialized route.
  int GetRouteSize(int vehicle) const;

  // Returns whether node can be removed from the solution.
  // This must be called for node belonging to initialized routes.
  bool CanBeRemoved(int64_t node) const;

  // Removes the node with the given node.
  // This must be called for node belonging to initialized routes.
  void RemoveNode(int64_t node);

  // Removes the performed sibling pickup or delivery of node, if any.
  void RemovePerformedPickupDeliverySibling(int64_t node);

  // Randomly returns the next or previous visit of the given performed
  // visit. Returns -1 if there are no other available visits. When the
  // selected adjacent vertex is a vehicle start/end, we always pick the
  // visit in the opposite direction.
  // This must be called for a performed visit belonging to an initialized
  // route.
  int64_t GetRandomAdjacentVisit(
      int64_t visit, std::mt19937& rnd,
      std::bernoulli_distribution& boolean_dist) const;

  // Returns a randomly selected sequence of contiguous visits that includes
  // the seed visit.
  // This must be called for a performed seed visit belonging to an
  // initialized route.
  std::vector<int64_t> GetRandomSequenceOfVisits(
      int64_t seed_visit, std::mt19937& rnd,
      std::bernoulli_distribution& boolean_dist, int size) const;

 private:
  const Model& model_;
  std::vector<int64_t> nexts_;
  std::vector<int64_t> prevs_;
  std::vector<int> route_sizes_;

  // Assignment that the routing solution refers to. It's changed at every
  // Reset call.
  const Assignment* assignment_ = nullptr;
};

// Interface for ILS event subscribers to be notified of ILS events.
class IteratedLocalSearchEventSubscriber {
 public:
  virtual ~IteratedLocalSearchEventSubscriber() = default;

  // Called when the ILS algorithm reaches a local optimum, i.e., after the
  // perturbation and the optional local search phases.
  virtual void OnLocalOptimumReached(const Assignment*) {
    // No-op by default.
  }

  // Called when the reference solution is updated, i.e., when a candidate
  // solution is accepted according to the reference solution acceptance
  // criterion.
  virtual void OnReferenceSolutionUpdated(const Assignment*) {
    // No-op by default.
  }
};

// Manages a set of ILS event subscribers, notifying them of ILS events.
class IteratedLocalSearchEventManager {
 public:
  // Adds a subscriber to the list of subscribers.
  bool AddSubscriber(IteratedLocalSearchEventSubscriber* subscriber);

  // Removes a subscriber from the list of subscribers.
  bool RemoveSubscriber(IteratedLocalSearchEventSubscriber* subscriber);

  // Notifies all subscribers that a local optimum has been reached.
  void OnLocalOptimumReached(const Assignment* assignment);

  // Notifies all subscribers that the reference solution has been updated.
  void OnReferenceSolutionUpdated(const Assignment* assignment);

 private:
  absl::flat_hash_set<IteratedLocalSearchEventSubscriber*> subscribers_;
};

// Ruin interface.
class RuinProcedure : public IteratedLocalSearchEventSubscriber {
 public:
  virtual ~RuinProcedure() = default;

  // Returns next accessors describing the ruined solution.
  virtual std::function<int64_t(int64_t)> Ruin(
      const Assignment* assignment) = 0;
};

// Removes a number of routes that are spatially close together.
class CloseRoutesRemovalRuinProcedure : public RuinProcedure {
 public:
  CloseRoutesRemovalRuinProcedure(Model* model, std::mt19937* rnd,
                                  size_t num_routes,
                                  int num_neighbors_for_route_selection);
  // Returns next accessors where at most num_routes routes have been shortcut,
  // i.e., next(shortcut route begin) = shortcut route end.
  // Next accessors for nodes belonging to shortcut routes are still set to
  // their original value and should not be used.
  std::function<int64_t(int64_t)> Ruin(const Assignment* assignment) override;

 private:
  const Model& model_;
  const Model::NodeNeighborsByCostClass* const neighbors_manager_;
  const size_t num_routes_;
  std::mt19937& rnd_;
  std::uniform_int_distribution<int64_t> node_dist_;
  SparseBitset<int64_t> removed_routes_;
};

// Removes a number of non start/end nodes by performing a random walk on the
// routing solution graph described by the assignment.
// Note that the removal of a pickup and delivery counts as the removal of a
// single entity.
class RandomWalkRemovalRuinProcedure : public RuinProcedure {
 public:
  RandomWalkRemovalRuinProcedure(Model* model, std::mt19937* rnd,
                                 int walk_length,
                                 int num_neighbors_for_route_selection);

  std::function<int64_t(int64_t)> Ruin(const Assignment* assignment) override;

 protected:
  // Returns the seed node and the length of the random walk.
  virtual std::pair<int64_t, int> GetWalkSeedAndLength(
      const Assignment* assignment);

  // Called when a node is removed from the solution.
  virtual void OnNodeRemoved(int64_t) {
    // No-op by default.
  }

  const Model& model_;
  std::mt19937& rnd_;
  std::uniform_int_distribution<int64_t> node_dist_;

 private:
  // Returns the next node towards which the random walk is extended.
  int64_t GetNextNodeToRemove(const Assignment* assignment, int node);

  Solution routing_solution_;
  const Model::NodeNeighborsByCostClass* const neighbors_manager_;
  const int walk_length_;
  std::bernoulli_distribution boolean_dist_;
};

class AdaptiveRandomWalkRemovalRuinProcedure
    : public RandomWalkRemovalRuinProcedure {
 public:
  AdaptiveRandomWalkRemovalRuinProcedure(Model* model,
                                         const Assignment* reference_assignment,
                                         std::mt19937* rnd,
                                         int num_neighbors_for_route_selection,
                                         double strengthening_factor,
                                         double weakening_factor);

  std::function<int64_t(int64_t)> Ruin(const Assignment* assignment) override;
  void OnLocalOptimumReached(const Assignment* assignment) override;
  void OnReferenceSolutionUpdated(const Assignment* assignment) override;

 private:
  std::pair<int64_t, int> GetWalkSeedAndLength(
      const Assignment* assignment) override;
  void OnNodeRemoved(int64_t node) override;

  const Assignment* reference_assignment_;
  double strengthening_factor_;
  double weakening_factor_;
  int64_t weakening_threshold_;
  int64_t strengthening_threshold_;
  std::uniform_int_distribution<int> random_choice_;
  std::vector<int> walk_lengths_;

  std::vector<int64_t> removed_nodes_;
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
      Model* model, std::vector<std::unique_ptr<RuinProcedure>> ruin_procedures,
      RuinCompositionStrategy::Value composition_strategy, std::mt19937* rnd);

  std::function<int64_t(int64_t)> Ruin(const Assignment* assignment) override;

  void OnLocalOptimumReached(const Assignment* assignment) override;

  void OnReferenceSolutionUpdated(const Assignment* assignment) override;

 private:
  // Creates a new assignment from the given next accessor.
  const Assignment* BuildAssignmentFromNextAccessor(
      const std::function<int64_t(int64_t)>& next_accessor);

  const Model& model_;
  std::vector<std::unique_ptr<RuinProcedure>> ruin_procedures_;
  std::unique_ptr<CompositionStrategy> composition_strategy_;

  // Used by BuildAssignmentFromNextAccessor to rebuild a proper assignment
  // from next accessors. Stored at the object level to minimize re-allocations.
  Assignment* ruined_assignment_;
  Assignment* next_assignment_;
};

// Performs a ruin based on the Slack Induction by String Removals (SISR)
// procedure described in "Slack Induction by String Removals for Vehicle
// Routing Problems" by Jan Christiaens and Greet Vanden Berghe, Transportation
// Science 2020.
// Link to paper:
// https://kuleuven.limo.libis.be/discovery/search?query=any,contains,LIRIAS1988666&tab=LIRIAS&search_scope=lirias_profile&vid=32KUL_KUL:Lirias&offset=0
// Note that, in this implementation, the notion of "string" is replaced by
// "sequence".
//  In short, at every ruin application a number of routes are
// disrupted. This number of routes is selected according to a careful
// combination of user-defined parameters and solution and instance properties.
// Every selected route is then disrupted by removing a contiguous sequence of
// visits, possibly bypassing a contiguous subsequence.
// See also SISRRuinStrategy in ils.proto.
class SISRRuinProcedure : public RuinProcedure {
 public:
  SISRRuinProcedure(Model* model, std::mt19937* rnd,
                    int max_removed_sequence_size, int avg_num_removed_visits,
                    double bypass_factor, int num_neighbors);

  std::function<int64_t(int64_t)> Ruin(const Assignment* assignment) override;

 private:
  int RuinRoute(const Assignment& assignment, int64_t seed_visit,
                double global_max_sequence_size);

  // Removes a randomly selected sequence that includes the given seed visit.
  void RuinRouteWithSequenceProcedure(int64_t seed_visit, int sequence_size);

  // Randomly removes a sequence including the seed visit but bypassing and
  // preserving a random subsequence.
  void RuinRouteWithSplitSequenceProcedure(int64_t route, int64_t seed_visit,
                                           int sequence_size);

  const Model& model_;
  std::mt19937& rnd_;
  int max_removed_sequence_size_;
  int avg_num_removed_visits_;
  double bypass_factor_;
  const Model::NodeNeighborsByCostClass* const neighbors_manager_;
  std::uniform_int_distribution<int64_t> node_dist_;
  std::bernoulli_distribution boolean_dist_;
  std::uniform_real_distribution<double> probability_dist_;
  SparseBitset<int64_t> ruined_routes_;
  Solution routing_solution_;
};

// Returns a DecisionBuilder implementing a perturbation step of an Iterated
// Local Search approach.
DecisionBuilder* MakePerturbationDecisionBuilder(
    const RoutingSearchParameters& parameters, Model* model,
    IteratedLocalSearchEventManager* event_manager, std::mt19937* rnd,
    const Assignment* assignment, std::function<bool()> stop_search,
    LocalSearchFilterManager* filter_manager);

// Neighbor acceptance criterion interface.
class NeighborAcceptanceCriterion : public IteratedLocalSearchEventSubscriber {
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

  // Called at the end of an ILS iteration.
  virtual void OnIterationEnd([[maybe_unused]] const Assignment* reference) {}

  // Called when a new best solution found is found.
  virtual void OnBestSolutionFound([[maybe_unused]] Assignment* reference) {}
};

// Returns a neighbor acceptance criterion based on the given parameters.
std::unique_ptr<NeighborAcceptanceCriterion> MakeNeighborAcceptanceCriterion(
    const Model& model, IteratedLocalSearchEventManager* event_manager,
    const AcceptancePolicy& acceptance_policy,
    const NeighborAcceptanceCriterion::SearchState& final_search_state,
    std::mt19937* rnd);

// Returns initial and final simulated annealing temperatures according to the
// given simulated annealing input parameters.
std::pair<double, double> GetSimulatedAnnealingTemperatures(
    const Model& model, const SimulatedAnnealingAcceptanceStrategy& sa_params,
    std::mt19937* rnd);

}  // namespace operations_research::routing

#endif  // ORTOOLS_ROUTING_ILS_H_
