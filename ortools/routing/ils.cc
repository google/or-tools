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

#include "ortools/routing/ils.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <random>
#include <utility>
#include <vector>

#include "absl/functional/bind_front.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "google/protobuf/repeated_ptr_field.h"
#include "ortools/base/protoutil.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/routing/ils.pb.h"
#include "ortools/routing/parameters.pb.h"
#include "ortools/routing/routing.h"
#include "ortools/routing/search.h"
#include "ortools/routing/types.h"

namespace operations_research::routing {
namespace {

// Returns global cheapest insertion parameters based on the given recreate
// strategy if available. Returns default parameters otherwise.
GlobalCheapestInsertionParameters
GetGlobalCheapestInsertionParametersForRecreateStrategy(
    const RecreateStrategy& recreate_strategy,
    const GlobalCheapestInsertionParameters& default_parameters) {
  return recreate_strategy.has_parameters() &&
                 recreate_strategy.parameters().has_global_cheapest_insertion()
             ? recreate_strategy.parameters().global_cheapest_insertion()
             : default_parameters;
}

// Returns local cheapest insertion parameters based on the given recreate
// strategy if available. Returns default parameters otherwise.
LocalCheapestInsertionParameters
GetLocalCheapestInsertionParametersForRecreateStrategy(
    const RecreateStrategy& recreate_strategy,
    const LocalCheapestInsertionParameters& default_parameters) {
  return recreate_strategy.has_parameters() &&
                 recreate_strategy.parameters().has_local_cheapest_insertion()
             ? recreate_strategy.parameters().local_cheapest_insertion()
             : default_parameters;
}

SavingsParameters GetSavingsParametersForRecreateStrategy(
    const RecreateStrategy& recreate_strategy,
    const SavingsParameters& default_parameters) {
  return recreate_strategy.has_parameters() &&
                 recreate_strategy.parameters().has_savings()
             ? recreate_strategy.parameters().savings()
             : default_parameters;
}

// Returns a ruin procedure based on the given ruin strategy.
std::unique_ptr<RuinProcedure> MakeRuinProcedure(
    RoutingModel* model, std::mt19937* rnd, RuinStrategy ruin,
    int num_neighbors_for_route_selection) {
  switch (ruin.strategy_case()) {
    case RuinStrategy::kSpatiallyCloseRoutes:
      return std::make_unique<CloseRoutesRemovalRuinProcedure>(
          model, rnd, ruin.spatially_close_routes().num_ruined_routes(),
          num_neighbors_for_route_selection);
      break;
    case RuinStrategy::kRandomWalk:
      return std::make_unique<RandomWalkRemovalRuinProcedure>(
          model, rnd, ruin.random_walk().num_removed_visits(),
          num_neighbors_for_route_selection);
    case RuinStrategy::kSisr:
      return std::make_unique<SISRRuinProcedure>(
          model, rnd, ruin.sisr().max_removed_sequence_size(),
          ruin.sisr().avg_num_removed_visits(), ruin.sisr().bypass_factor(),
          num_neighbors_for_route_selection);
    default:
      LOG(DFATAL) << "Unsupported ruin procedure.";
      return nullptr;
  }
}

// Returns the ruin procedures associated with the given ruin strategies.
std::vector<std::unique_ptr<RuinProcedure>> MakeRuinProcedures(
    RoutingModel* model, std::mt19937* rnd,
    const google::protobuf::RepeatedPtrField<
        ::operations_research::routing::RuinStrategy>& ruin_strategies,
    int num_neighbors_for_route_selection) {
  std::vector<std::unique_ptr<RuinProcedure>> ruin_procedures;
  for (const RuinStrategy& ruin : ruin_strategies) {
    ruin_procedures.push_back(
        MakeRuinProcedure(model, rnd, ruin, num_neighbors_for_route_selection));
  }
  return ruin_procedures;
}

class SequentialCompositionStrategy
    : public CompositeRuinProcedure::CompositionStrategy {
 public:
  explicit SequentialCompositionStrategy(
      std::vector<RuinProcedure*> ruin_procedures)
      : CompositeRuinProcedure::CompositionStrategy(
            std::move(ruin_procedures)) {}
  const std::vector<RuinProcedure*>& Select() override { return ruins_; }
};

class SequentialRandomizedCompositionStrategy
    : public CompositeRuinProcedure::CompositionStrategy {
 public:
  SequentialRandomizedCompositionStrategy(
      std::vector<RuinProcedure*> ruin_procedures, std::mt19937* rnd)
      : CompositeRuinProcedure::CompositionStrategy(std::move(ruin_procedures)),
        rnd_(*rnd) {}
  const std::vector<RuinProcedure*>& Select() override {
    std::shuffle(ruins_.begin(), ruins_.end(), rnd_);
    return ruins_;
  }

 private:
  std::mt19937& rnd_;
};

class SingleRandomCompositionStrategy
    : public CompositeRuinProcedure::CompositionStrategy {
 public:
  SingleRandomCompositionStrategy(std::vector<RuinProcedure*> ruin_procedures,
                                  std::mt19937* rnd)
      : CompositeRuinProcedure::CompositionStrategy(std::move(ruin_procedures)),
        rnd_(*rnd) {
    single_ruin_.resize(1);
  }
  const std::vector<RuinProcedure*>& Select() override {
    single_ruin_[0] = ruins_[rnd_() % ruins_.size()];
    return single_ruin_;
  }

 private:
  std::mt19937& rnd_;

  // Stores the single ruin that will be returned.
  std::vector<RuinProcedure*> single_ruin_;
};

// Returns a composition strategy based on the input parameters.
std::unique_ptr<CompositeRuinProcedure::CompositionStrategy>
MakeRuinCompositionStrategy(
    absl::Span<const std::unique_ptr<RuinProcedure>> ruins,
    RuinCompositionStrategy::Value composition_strategy, std::mt19937* rnd) {
  std::vector<RuinProcedure*> ruin_ptrs;
  ruin_ptrs.reserve(ruins.size());
  for (const auto& ruin : ruins) {
    ruin_ptrs.push_back(ruin.get());
  }

  switch (composition_strategy) {
    case RuinCompositionStrategy::RUN_ALL_SEQUENTIALLY:
      return std::make_unique<SequentialCompositionStrategy>(
          std::move(ruin_ptrs));
    case RuinCompositionStrategy::RUN_ALL_RANDOMLY:
      return std::make_unique<SequentialRandomizedCompositionStrategy>(
          std::move(ruin_ptrs), rnd);
    case RuinCompositionStrategy::RUN_ONE_RANDOMLY:
      return std::make_unique<SingleRandomCompositionStrategy>(
          std::move(ruin_ptrs), rnd);
    default:
      LOG(DFATAL) << "Unsupported composition strategy.";
      return nullptr;
  }
}

// Returns a ruin procedure based on the given ruin and recreate parameters.
std::unique_ptr<RuinProcedure> MakeRuinProcedure(
    const RuinRecreateParameters& parameters, RoutingModel* model,
    std::mt19937* rnd) {
  const int num_non_start_end_nodes = model->Size() - model->vehicles();
  const uint32_t preferred_num_neighbors =
      parameters.route_selection_neighbors_ratio() * num_non_start_end_nodes;

  // TODO(user): rename parameters.route_selection_max_neighbors to something
  // more general that can be used by multiple ruin procedures.
  const uint32_t num_neighbors_for_route_selection =
      std::min(parameters.route_selection_max_neighbors(),
               std::max(parameters.route_selection_min_neighbors(),
                        preferred_num_neighbors));

  if (parameters.ruin_strategies().size() == 1) {
    return MakeRuinProcedure(model, rnd, *parameters.ruin_strategies().begin(),
                             num_neighbors_for_route_selection);
  }

  return std::make_unique<CompositeRuinProcedure>(
      model,
      MakeRuinProcedures(model, rnd, parameters.ruin_strategies(),
                         num_neighbors_for_route_selection),
      parameters.ruin_composition_strategy(), rnd);
}

// Returns a recreate procedure based on the given parameters.
std::unique_ptr<RoutingFilteredHeuristic> MakeRecreateProcedure(
    const RoutingSearchParameters& parameters, RoutingModel* model,
    std::function<bool()> stop_search,
    LocalSearchFilterManager* filter_manager) {
  const RecreateStrategy& recreate_strategy =
      parameters.iterated_local_search_parameters()
          .ruin_recreate_parameters()
          .recreate_strategy();
  switch (recreate_strategy.heuristic()) {
    case FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION:
      return std::make_unique<LocalCheapestInsertionFilteredHeuristic>(
          model, std::move(stop_search),
          absl::bind_front(&RoutingModel::GetArcCostForVehicle, model),
          GetLocalCheapestInsertionParametersForRecreateStrategy(
              recreate_strategy,
              parameters.local_cheapest_insertion_parameters()),
          filter_manager, model->GetBinCapacities());
    case FirstSolutionStrategy::LOCAL_CHEAPEST_COST_INSERTION:
      return std::make_unique<LocalCheapestInsertionFilteredHeuristic>(
          model, std::move(stop_search),
          /*evaluator=*/nullptr,
          GetLocalCheapestInsertionParametersForRecreateStrategy(
              recreate_strategy,
              parameters.local_cheapest_cost_insertion_parameters()),
          filter_manager, model->GetBinCapacities());
    case FirstSolutionStrategy::SEQUENTIAL_CHEAPEST_INSERTION:
      return std::make_unique<GlobalCheapestInsertionFilteredHeuristic>(
          model, std::move(stop_search),
          absl::bind_front(&RoutingModel::GetArcCostForVehicle, model),
          [model](int64_t i) { return model->UnperformedPenaltyOrValue(0, i); },
          filter_manager,
          GetGlobalCheapestInsertionParametersForRecreateStrategy(
              recreate_strategy,
              parameters.global_cheapest_insertion_first_solution_parameters()),
          /*is_sequential=*/true);
    case FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION:
      return std::make_unique<GlobalCheapestInsertionFilteredHeuristic>(
          model, std::move(stop_search),
          absl::bind_front(&RoutingModel::GetArcCostForVehicle, model),
          [model](int64_t i) { return model->UnperformedPenaltyOrValue(0, i); },
          filter_manager,
          GetGlobalCheapestInsertionParametersForRecreateStrategy(
              recreate_strategy,
              parameters.global_cheapest_insertion_first_solution_parameters()),
          /*is_sequential=*/false);
    case FirstSolutionStrategy::SAVINGS:
      return std::make_unique<SequentialSavingsFilteredHeuristic>(
          model, std::move(stop_search),
          GetSavingsParametersForRecreateStrategy(
              recreate_strategy, parameters.savings_parameters()),
          filter_manager);
    case FirstSolutionStrategy::PARALLEL_SAVINGS:
      return std::make_unique<ParallelSavingsFilteredHeuristic>(
          model, std::move(stop_search),
          GetSavingsParametersForRecreateStrategy(
              recreate_strategy, parameters.savings_parameters()),
          filter_manager);
    default:
      LOG(DFATAL) << "Unsupported recreate procedure.";
      return nullptr;
  }

  return nullptr;
}

// Greedy criterion in which the reference assignment is only replaced by an
// improving candidate assignment.
class GreedyDescentAcceptanceCriterion : public NeighborAcceptanceCriterion {
 public:
  bool Accept([[maybe_unused]] const SearchState& search_state,
              const Assignment* candidate,
              const Assignment* reference) override {
    return candidate->ObjectiveValue() < reference->ObjectiveValue();
  }
};

// Simulated annealing cooling schedule interface.
class CoolingSchedule {
 public:
  CoolingSchedule(NeighborAcceptanceCriterion::SearchState final_search_state,
                  double initial_temperature, double final_temperature)
      : final_search_state_(std::move(final_search_state)),
        initial_temperature_(initial_temperature),
        final_temperature_(final_temperature) {
    DCHECK_GE(initial_temperature_, final_temperature_);
  }
  virtual ~CoolingSchedule() = default;

  // Returns the temperature according to given search state.
  virtual double GetTemperature(
      const NeighborAcceptanceCriterion::SearchState& search_state) const = 0;

 protected:
  // Returns the progress of the given search state with respect to the final
  // search state.
  double GetProgress(
      const NeighborAcceptanceCriterion::SearchState& search_state) const {
    const double duration_progress =
        absl::FDivDuration(search_state.duration, final_search_state_.duration);
    const double solutions_progress =
        static_cast<double>(search_state.solutions) /
        final_search_state_.solutions;
    const double progress = std::max(duration_progress, solutions_progress);
    // We take the min with 1 as at the end of the search we may go a bit above
    // 1 with duration_progress depending on when we check the time limit.
    return std::min(1.0, progress);
  }

  const NeighborAcceptanceCriterion::SearchState final_search_state_;
  const double initial_temperature_;
  const double final_temperature_;
};

// A cooling schedule that lowers the temperature in an exponential way.
class ExponentialCoolingSchedule : public CoolingSchedule {
 public:
  ExponentialCoolingSchedule(
      NeighborAcceptanceCriterion::SearchState final_search_state,
      double initial_temperature, double final_temperature)
      : CoolingSchedule(std::move(final_search_state), initial_temperature,
                        final_temperature),
        temperature_ratio_(final_temperature / initial_temperature) {}

  double GetTemperature(const NeighborAcceptanceCriterion::SearchState&
                            search_state) const override {
    const double progress = GetProgress(search_state);

    return initial_temperature_ * std::pow(temperature_ratio_, progress);
  }

 private:
  const double temperature_ratio_;
};

// A cooling schedule that lowers the temperature in a linear way.
class LinearCoolingSchedule : public CoolingSchedule {
 public:
  LinearCoolingSchedule(
      NeighborAcceptanceCriterion::SearchState final_search_state,
      double initial_temperature, double final_temperature)
      : CoolingSchedule(std::move(final_search_state), initial_temperature,
                        final_temperature) {}

  double GetTemperature(const NeighborAcceptanceCriterion::SearchState&
                            search_state) const override {
    const double progress = GetProgress(search_state);
    return initial_temperature_ -
           progress * (initial_temperature_ - final_temperature_);
  }
};

// Returns a cooling schedule based on the given input parameters.
std::unique_ptr<CoolingSchedule> MakeCoolingSchedule(
    const RoutingModel& model, const RoutingSearchParameters& parameters,
    std::mt19937* rnd) {
  const absl::Duration final_duration =
      !parameters.has_time_limit()
          ? absl::InfiniteDuration()
          : util_time::DecodeGoogleApiProto(parameters.time_limit()).value();

  const SimulatedAnnealingParameters& sa_params =
      parameters.iterated_local_search_parameters()
          .simulated_annealing_parameters();

  NeighborAcceptanceCriterion::SearchState final_search_state{
      final_duration, parameters.solution_limit()};

  const auto [initial_temperature, final_temperature] =
      GetSimulatedAnnealingTemperatures(model, sa_params, rnd);

  switch (sa_params.cooling_schedule_strategy()) {
    case CoolingScheduleStrategy::EXPONENTIAL:
      return std::make_unique<ExponentialCoolingSchedule>(
          NeighborAcceptanceCriterion::SearchState{final_duration,
                                                   parameters.solution_limit()},
          initial_temperature, final_temperature);
    case CoolingScheduleStrategy::LINEAR:
      return std::make_unique<LinearCoolingSchedule>(
          std::move(final_search_state), initial_temperature,
          final_temperature);
    default:
      LOG(DFATAL) << "Unsupported cooling schedule strategy.";
      return nullptr;
  }
}

// Simulated annealing acceptance criterion in which the reference assignment is
// replaced with a probability given by the quality of the candidate solution,
// the current search state and the chosen cooling schedule.
class SimulatedAnnealingAcceptanceCriterion
    : public NeighborAcceptanceCriterion {
 public:
  explicit SimulatedAnnealingAcceptanceCriterion(
      std::unique_ptr<CoolingSchedule> cooling_schedule, std::mt19937* rnd)
      : cooling_schedule_(std::move(cooling_schedule)),
        rnd_(*rnd),
        probability_distribution_(0.0, 1.0) {}

  bool Accept(const SearchState& search_state, const Assignment* candidate,
              const Assignment* reference) override {
    double temperature = cooling_schedule_->GetTemperature(search_state);
    return candidate->ObjectiveValue() +
               temperature * std::log(probability_distribution_(rnd_)) <
           reference->ObjectiveValue();
  }

 private:
  std::unique_ptr<CoolingSchedule> cooling_schedule_;
  std::mt19937 rnd_;
  std::uniform_real_distribution<double> probability_distribution_;
};

// Returns whether the given assignment has at least one performed node.
bool HasPerformedNodes(const RoutingModel& model,
                       const Assignment& assignment) {
  for (int v = 0; v < model.vehicles(); ++v) {
    if (model.Next(assignment, model.Start(v)) != model.End(v)) {
      return true;
    }
  }
  return false;
}

// Returns the number of used vehicles.
int CountUsedVehicles(const RoutingModel& model, const Assignment& assignment) {
  int count = 0;
  for (int vehicle = 0; vehicle < model.vehicles(); ++vehicle) {
    count += model.Next(assignment, model.Start(vehicle)) != model.End(vehicle);
  }
  return count;
}

// Returns the average route size of non empty routes.
double ComputeAverageNonEmptyRouteSize(const RoutingModel& model,
                                       const Assignment& assignment) {
  const int num_used_vehicles = CountUsedVehicles(model, assignment);
  if (num_used_vehicles == 0) return 0;

  const double num_visits = model.Size() - model.vehicles();
  return num_visits / num_used_vehicles;
}

// Returns a random performed visit for the given assignment. The procedure
// requires a distribution including all visits. Returns -1 if there are no
// performed visits.
int64_t PickRandomPerformedVisit(
    const RoutingModel& model, const Assignment& assignment, std::mt19937& rnd,
    std::uniform_int_distribution<int64_t>& customer_dist) {
  DCHECK_EQ(customer_dist.min(), 0);
  DCHECK_EQ(customer_dist.max(), model.Size() - model.vehicles());

  if (!HasPerformedNodes(model, assignment)) {
    return -1;
  }

  int64_t customer;
  do {
    customer = customer_dist(rnd);
  } while (model.IsStart(customer) ||
           assignment.Value(model.VehicleVar(customer)) == -1);
  DCHECK(!model.IsEnd(customer));
  return customer;
}
}  // namespace

RoutingSolution::RoutingSolution(const RoutingModel& model) : model_(model) {
  const int all_nodes = model.Size() + model.vehicles();

  nexts_.resize(all_nodes, -1);
  prevs_.resize(all_nodes, -1);
  route_sizes_.resize(model.vehicles(), 0);
}

void RoutingSolution::Reset(const Assignment* assignment) {
  assignment_ = assignment;

  // TODO(user): consider resetting only previously set values.
  nexts_.assign(nexts_.size(), -1);
  // TODO(user): consider removing the resets below, and only rely on
  // nexts_.
  prevs_.assign(prevs_.size(), -1);
  route_sizes_.assign(model_.vehicles(), -1);
}

void RoutingSolution::InitializeRouteInfoIfNeeded(int vehicle) {
  const int64_t start = model_.Start(vehicle);

  if (BelongsToInitializedRoute(start)) {
    return;
  }

  const int64_t end = model_.End(vehicle);

  int64_t prev = end;
  int64_t curr = start;

  // Setup the start and inner nodes.
  route_sizes_[vehicle] = -1;
  while (curr != end) {
    const int64_t next = assignment_->Value(model_.NextVar(curr));

    nexts_[curr] = next;
    prevs_[curr] = prev;
    ++route_sizes_[vehicle];

    prev = curr;
    curr = next;
  }

  // Setup the end node.
  nexts_[end] = start;
  prevs_[end] = prev;
}

bool RoutingSolution::BelongsToInitializedRoute(int64_t node_index) const {
  DCHECK_EQ(nexts_[node_index] != -1, prevs_[node_index] != -1);
  return nexts_[node_index] != -1;
}

int64_t RoutingSolution::GetNextNodeIndex(int64_t node_index) const {
  return BelongsToInitializedRoute(node_index)
             ? nexts_[node_index]
             : assignment_->Value(model_.NextVar(node_index));
}

int64_t RoutingSolution::GetInitializedPrevNodeIndex(int64_t node_index) const {
  DCHECK(BelongsToInitializedRoute(node_index));
  return prevs_[node_index];
}

int RoutingSolution::GetRouteSize(int vehicle) const {
  DCHECK(BelongsToInitializedRoute(model_.Start(vehicle)));
  return route_sizes_[vehicle];
}

bool RoutingSolution::CanBeRemoved(int64_t node_index) const {
  return !model_.IsStart(node_index) && !model_.IsEnd(node_index) &&
         GetNextNodeIndex(node_index) != node_index;
}

void RoutingSolution::RemoveNode(int64_t node_index) {
  DCHECK(BelongsToInitializedRoute(node_index));

  DCHECK_NE(nexts_[node_index], node_index);
  DCHECK_NE(prevs_[node_index], node_index);

  const int64_t next = nexts_[node_index];
  const int64_t prev = prevs_[node_index];

  const int vehicle = assignment_->Value(model_.VehicleVar(node_index));
  --route_sizes_[vehicle];
  DCHECK_GE(route_sizes_[vehicle], 0);

  nexts_[prev] = next;
  prevs_[next] = prev;

  nexts_[node_index] = node_index;
  prevs_[node_index] = node_index;
}

void RoutingSolution::RemovePerformedPickupDeliverySibling(int64_t customer) {
  DCHECK(!model_.IsStart(customer));
  DCHECK(!model_.IsEnd(customer));
  if (const std::optional<int64_t> sibling_node =
          model_.GetFirstMatchingPickupDeliverySibling(
              customer, [this](int64_t node) { return CanBeRemoved(node); });
      sibling_node.has_value()) {
    const int sibling_vehicle =
        assignment_->Value(model_.VehicleVar(sibling_node.value()));
    DCHECK_NE(sibling_vehicle, -1);

    InitializeRouteInfoIfNeeded(sibling_vehicle);
    RemoveNode(sibling_node.value());
  }
}

int64_t RoutingSolution::GetRandomAdjacentVisit(
    int64_t visit, std::mt19937& rnd,
    std::bernoulli_distribution& boolean_dist) const {
  DCHECK(BelongsToInitializedRoute(visit));
  DCHECK(!model_.IsStart(visit));
  DCHECK(!model_.IsEnd(visit));
  // The visit is performed.
  DCHECK(CanBeRemoved(visit));

  const int vehicle = assignment_->Value(model_.VehicleVar(visit));
  if (GetRouteSize(vehicle) <= 1) {
    return -1;
  }

  const bool move_forward = boolean_dist(rnd);
  int64_t next_node = move_forward ? GetNextNodeIndex(visit)
                                   : GetInitializedPrevNodeIndex(visit);
  if (model_.IsStart(next_node) || model_.IsEnd(next_node)) {
    next_node = move_forward ? GetInitializedPrevNodeIndex(visit)
                             : GetNextNodeIndex(visit);
  }
  DCHECK(!model_.IsStart(next_node));
  DCHECK(!model_.IsEnd(next_node));
  return next_node;
}

std::vector<int64_t> RoutingSolution::GetRandomSequenceOfVisits(
    int64_t seed_visit, std::mt19937& rnd,
    std::bernoulli_distribution& boolean_dist, int size) const {
  DCHECK(BelongsToInitializedRoute(seed_visit));
  DCHECK(!model_.IsStart(seed_visit));
  DCHECK(!model_.IsEnd(seed_visit));
  // The seed visit is actually performed.
  DCHECK(CanBeRemoved(seed_visit));

  // The seed visit is always included.
  --size;

  // Sequence's excluded boundaries.
  int64_t left = GetInitializedPrevNodeIndex(seed_visit);
  int64_t right = GetNextNodeIndex(seed_visit);

  while (size-- > 0) {
    if (model_.IsStart(left) && model_.IsEnd(right)) {
      // We can no longer extend the sequence either way.
      break;
    }

    // When left is at the start (resp. right is at the end), we can
    // only extend right (resp. left), and if both ends are free to
    // move we decide the direction at random.
    if (model_.IsStart(left)) {
      right = GetNextNodeIndex(right);
    } else if (model_.IsEnd(right)) {
      left = GetInitializedPrevNodeIndex(left);
    } else {
      const bool move_forward = boolean_dist(rnd);
      if (move_forward) {
        right = GetNextNodeIndex(right);
      } else {
        left = GetInitializedPrevNodeIndex(left);
      }
    }
  }

  // TODO(user): consider taking the container in input to avoid multiple
  // memory allocations.
  std::vector<int64_t> sequence;
  int64_t curr = GetNextNodeIndex(left);
  while (curr != right) {
    sequence.push_back(curr);
    curr = GetNextNodeIndex(curr);
  }
  return sequence;
}

CompositeRuinProcedure::CompositionStrategy::CompositionStrategy(
    std::vector<RuinProcedure*> ruin_procedures)
    : ruins_(std::move(ruin_procedures)) {}

CompositeRuinProcedure::CompositeRuinProcedure(
    RoutingModel* model,
    std::vector<std::unique_ptr<RuinProcedure>> ruin_procedures,
    RuinCompositionStrategy::Value composition_strategy, std::mt19937* rnd)
    : model_(*model),
      ruin_procedures_(std::move(ruin_procedures)),
      composition_strategy_(MakeRuinCompositionStrategy(
          ruin_procedures_, composition_strategy, rnd)),
      ruined_assignment_(model_.solver()->MakeAssignment()),
      next_assignment_(model_.solver()->MakeAssignment()) {}

std::function<int64_t(int64_t)> CompositeRuinProcedure::Ruin(
    const Assignment* assignment) {
  const std::vector<RuinProcedure*>& ruins = composition_strategy_->Select();

  auto next_accessors = ruins[0]->Ruin(assignment);
  for (int i = 1; i < ruins.size(); ++i) {
    const Assignment* next_assignment =
        BuildAssignmentFromNextAccessor(next_accessors);
    ruined_assignment_->Copy(next_assignment);
    next_accessors = ruins[i]->Ruin(ruined_assignment_);
  }

  return next_accessors;
}

const Assignment* CompositeRuinProcedure::BuildAssignmentFromNextAccessor(
    const std::function<int64_t(int64_t)>& next_accessors) {
  next_assignment_->Clear();

  // Setup next variables for nodes and vehicle variables for unperformed nodes.
  for (int node = 0; node < model_.Size(); node++) {
    const int64_t next = next_accessors(node);
    next_assignment_->Add(model_.NextVar(node))->SetValue(next);
    if (next == node) {
      // Node is unperformed, we set its vehicle var accordingly.
      next_assignment_->Add(model_.VehicleVar(node))->SetValue(-1);
    }
  }

  // Setup vehicle variables for performed nodes.
  for (int vehicle = 0; vehicle < model_.vehicles(); ++vehicle) {
    int64_t node = model_.Start(vehicle);
    while (!model_.IsEnd(node)) {
      next_assignment_->Add(model_.VehicleVar(node))->SetValue(vehicle);
      node = next_accessors(node);
    }
    // Also set the vehicle var for the vehicle end.
    next_assignment_->Add(model_.VehicleVar(node))->SetValue(vehicle);
  }

  return next_assignment_;
}

CloseRoutesRemovalRuinProcedure::CloseRoutesRemovalRuinProcedure(
    RoutingModel* model, std::mt19937* rnd, size_t num_routes,
    int num_neighbors_for_route_selection)
    : model_(*model),
      neighbors_manager_(model->GetOrCreateNodeNeighborsByCostClass(
          {num_neighbors_for_route_selection,
           /*add_vehicle_starts_to_neighbors=*/false,
           /*add_vehicle_ends_to_neighbors=*/false,
           /*only_sort_neighbors_for_partial_neighborhoods=*/false})),
      num_routes_(num_routes),
      rnd_(*rnd),
      customer_dist_(0, model->Size() - model->vehicles()),
      removed_routes_(model->vehicles()) {}

std::function<int64_t(int64_t)> CloseRoutesRemovalRuinProcedure::Ruin(
    const Assignment* assignment) {
  if (num_routes_ == 0) {
    return [this, assignment](int64_t node) {
      return assignment->Value(model_.NextVar(node));
    };
  }

  const int64_t seed_node =
      PickRandomPerformedVisit(model_, *assignment, rnd_, customer_dist_);
  if (seed_node == -1) {
    return [this, assignment](int64_t node) {
      return assignment->Value(model_.NextVar(node));
    };
  }

  removed_routes_.ResetAllToFalse();

  const int seed_route = assignment->Value(model_.VehicleVar(seed_node));
  DCHECK_GE(seed_route, 0);

  removed_routes_.Set(seed_route);

  const RoutingCostClassIndex cost_class_index =
      model_.GetCostClassIndexOfVehicle(seed_route);

  const std::vector<int>& neighbors =
      neighbors_manager_->GetOutgoingNeighborsOfNodeForCostClass(
          cost_class_index.value(), seed_node);

  for (int neighbor : neighbors) {
    if (removed_routes_.NumberOfSetCallsWithDifferentArguments() ==
        num_routes_) {
      break;
    }
    const int64_t route = assignment->Value(model_.VehicleVar(neighbor));
    if (route < 0 || removed_routes_[route]) {
      continue;
    }
    removed_routes_.Set(route);
  }

  return [this, assignment](int64_t node) {
    // Shortcut removed routes to remove associated customers.
    if (model_.IsStart(node)) {
      const int route = assignment->Value(model_.VehicleVar(node));
      if (removed_routes_[route]) {
        return model_.End(route);
      }
    }
    return assignment->Value(model_.NextVar(node));
  };
}

RandomWalkRemovalRuinProcedure::RandomWalkRemovalRuinProcedure(
    RoutingModel* model, std::mt19937* rnd, int walk_length,
    int num_neighbors_for_route_selection)
    : model_(*model),
      routing_solution_(*model),
      neighbors_manager_(model->GetOrCreateNodeNeighborsByCostClass(
          {num_neighbors_for_route_selection,
           /*add_vehicle_starts_to_neighbors=*/false,
           /*add_vehicle_ends_to_neighbors=*/false,
           /*only_sort_neighbors_for_partial_neighborhoods=*/false})),
      rnd_(*rnd),
      walk_length_(walk_length),
      customer_dist_(0, model->Size() - model->vehicles()) {}

std::function<int64_t(int64_t)> RandomWalkRemovalRuinProcedure::Ruin(
    const Assignment* assignment) {
  if (walk_length_ == 0) {
    return [this, assignment](int64_t node) {
      return assignment->Value(model_.NextVar(node));
    };
  }

  int64_t curr_node =
      PickRandomPerformedVisit(model_, *assignment, rnd_, customer_dist_);
  if (curr_node == -1) {
    return [this, assignment](int64_t node) {
      return assignment->Value(model_.NextVar(node));
    };
  }

  routing_solution_.Reset(assignment);

  int walk_length = walk_length_;

  while (walk_length-- > 0) {
    // Remove the active siblings node of curr before selecting next, so that
    // we do not accidentally end up with next being one of these sibling
    // nodes.
    routing_solution_.RemovePerformedPickupDeliverySibling(curr_node);

    const int64_t next_node = GetNextNodeToRemove(assignment, curr_node);

    routing_solution_.RemoveNode(curr_node);

    if (next_node == -1) {
      // We were not able to find a vertex where to move next.
      // We thus prematurely abort the ruin.
      break;
    }

    curr_node = next_node;
  }

  return
      [this](int64_t node) { return routing_solution_.GetNextNodeIndex(node); };
}

int64_t RandomWalkRemovalRuinProcedure::GetNextNodeToRemove(
    const Assignment* assignment, int node) {
  const int curr_vehicle = assignment->Value(model_.VehicleVar(node));
  routing_solution_.InitializeRouteInfoIfNeeded(curr_vehicle);

  if (boolean_dist_(rnd_)) {
    const int64_t next_node =
        routing_solution_.GetRandomAdjacentVisit(node, rnd_, boolean_dist_);
    if (next_node != -1) {
      return next_node;
    }
  }

  // Pick the next node by jumping to a neighboring (non empty) route,
  // otherwise.
  const RoutingCostClassIndex cost_class_index =
      model_.GetCostClassIndexOfVehicle(curr_vehicle);

  const std::vector<int>& neighbors =
      neighbors_manager_->GetOutgoingNeighborsOfNodeForCostClass(
          cost_class_index.value(), node);

  int64_t same_route_closest_neighbor = -1;

  for (int neighbor : neighbors) {
    const int neighbor_vehicle = assignment->Value(model_.VehicleVar(neighbor));

    if (!routing_solution_.CanBeRemoved(neighbor)) {
      continue;
    }

    if (neighbor_vehicle == curr_vehicle) {
      if (same_route_closest_neighbor == -1) {
        same_route_closest_neighbor = neighbor;
      }
      continue;
    }

    return neighbor;
  }

  // If we are not able to find a customer in another route, we are ok
  // with taking a customer from the current one.
  // Note that it can be -1 if no removable neighbor was found for the input
  // node.
  return same_route_closest_neighbor;
}

SISRRuinProcedure::SISRRuinProcedure(RoutingModel* model, std::mt19937* rnd,
                                     int max_removed_sequence_size,
                                     int avg_num_removed_visits,
                                     double bypass_factor, int num_neighbors)
    : model_(*model),
      rnd_(*rnd),
      max_removed_sequence_size_(max_removed_sequence_size),
      avg_num_removed_visits_(avg_num_removed_visits),
      bypass_factor_(bypass_factor),
      neighbors_manager_(model->GetOrCreateNodeNeighborsByCostClass(
          {num_neighbors,
           /*add_vehicle_starts_to_neighbors=*/false,
           /*add_vehicle_ends_to_neighbors=*/false,
           /*only_sort_neighbors_for_partial_neighborhoods=*/false})),
      customer_dist_(0, model->Size() - model->vehicles()),
      probability_dist_(0.0, 1.0),
      ruined_routes_(model->vehicles()),
      routing_solution_(*model) {}

std::function<int64_t(int64_t)> SISRRuinProcedure::Ruin(
    const Assignment* assignment) {
  const int64_t seed_node =
      PickRandomPerformedVisit(model_, *assignment, rnd_, customer_dist_);
  if (seed_node == -1) {
    return [this, assignment](int64_t node) {
      return assignment->Value(model_.NextVar(node));
    };
  }

  routing_solution_.Reset(assignment);
  ruined_routes_.ResetAllToFalse();

  const double max_sequence_size =
      std::min<double>(max_removed_sequence_size_,
                       ComputeAverageNonEmptyRouteSize(model_, *assignment));

  const double max_num_removed_sequences =
      (4 * avg_num_removed_visits_) / (1 + max_sequence_size) - 1;
  DCHECK_GE(max_num_removed_sequences, 1);

  const int num_sequences_to_remove =
      std::floor(std::uniform_real_distribution<double>(
          1.0, max_num_removed_sequences + 1)(rnd_));

  // We start by disrupting the route where the seed visit is served.
  const int seed_route = RuinRoute(*assignment, seed_node, max_sequence_size);
  DCHECK_NE(seed_route, -1);

  const RoutingCostClassIndex cost_class_index =
      model_.GetCostClassIndexOfVehicle(seed_route);

  for (const int neighbor :
       neighbors_manager_->GetOutgoingNeighborsOfNodeForCostClass(
           cost_class_index.value(), seed_node)) {
    if (ruined_routes_.NumberOfSetCallsWithDifferentArguments() ==
        num_sequences_to_remove) {
      break;
    }

    if (!routing_solution_.CanBeRemoved(neighbor)) {
      continue;
    }

    RuinRoute(*assignment, neighbor, max_sequence_size);
  }

  return
      [this](int64_t node) { return routing_solution_.GetNextNodeIndex(node); };
}

int SISRRuinProcedure::RuinRoute(const Assignment& assignment,
                                 int64_t seed_visit,
                                 double global_max_sequence_size) {
  const int route = assignment.Value(model_.VehicleVar(seed_visit));
  DCHECK_GE(route, 0);
  if (ruined_routes_[route]) return -1;

  routing_solution_.InitializeRouteInfoIfNeeded(route);
  ruined_routes_.Set(route);

  const double max_sequence_size = std::min<double>(
      routing_solution_.GetRouteSize(route), global_max_sequence_size);

  int sequence_size = std::floor(
      std::uniform_real_distribution<double>(1.0, max_sequence_size + 1)(rnd_));

  if (sequence_size == 1 || sequence_size == max_sequence_size ||
      boolean_dist_(rnd_)) {
    RuinRouteWithSequenceProcedure(seed_visit, sequence_size);
  } else {
    RuinRouteWithSplitSequenceProcedure(route, seed_visit, sequence_size);
  }

  return route;
}

void SISRRuinProcedure::RuinRouteWithSequenceProcedure(int64_t seed_visit,
                                                       int sequence_size) {
  const std::vector<int64_t> sequence =
      routing_solution_.GetRandomSequenceOfVisits(seed_visit, rnd_,
                                                  boolean_dist_, sequence_size);

  // Remove the selected visits.
  for (const int64_t visit : sequence) {
    routing_solution_.RemoveNode(visit);
  }

  // Remove any still performed pickup or delivery siblings.
  for (const int64_t visit : sequence) {
    routing_solution_.RemovePerformedPickupDeliverySibling(visit);
  }
}

void SISRRuinProcedure::RuinRouteWithSplitSequenceProcedure(int64_t route,
                                                            int64_t seed_visit,
                                                            int sequence_size) {
  const int max_num_bypassed_visits =
      routing_solution_.GetRouteSize(route) - sequence_size;
  int num_bypassed_visits = 1;
  while (num_bypassed_visits < max_num_bypassed_visits &&
         probability_dist_(rnd_) >= bypass_factor_ * probability_dist_(rnd_)) {
    ++num_bypassed_visits;
  }

  const std::vector<int64_t> sequence =
      routing_solution_.GetRandomSequenceOfVisits(
          seed_visit, rnd_, boolean_dist_, sequence_size + num_bypassed_visits);

  const int start_bypassed_visits = rnd_() % (sequence_size + 1);
  const int end_bypassed_visits = start_bypassed_visits + num_bypassed_visits;

  // Remove the selected visits.
  for (int i = 0; i < start_bypassed_visits; ++i) {
    routing_solution_.RemoveNode(sequence[i]);
  }
  for (int i = end_bypassed_visits; i < sequence.size(); ++i) {
    routing_solution_.RemoveNode(sequence[i]);
  }

  // Remove any still performed pickup or delivery siblings.
  for (int i = 0; i < start_bypassed_visits; ++i) {
    routing_solution_.RemovePerformedPickupDeliverySibling(sequence[i]);
  }
  for (int i = end_bypassed_visits; i < sequence.size(); ++i) {
    routing_solution_.RemovePerformedPickupDeliverySibling(sequence[i]);
  }
}

class RuinAndRecreateDecisionBuilder : public DecisionBuilder {
 public:
  RuinAndRecreateDecisionBuilder(
      const Assignment* assignment, std::unique_ptr<RuinProcedure> ruin,
      std::unique_ptr<RoutingFilteredHeuristic> recreate)
      : assignment_(*assignment),
        ruin_(std::move(ruin)),
        recreate_(std::move(recreate)) {}

  Decision* Next(Solver* const solver) override {
    Assignment* const new_assignment = Recreate(ruin_->Ruin(&assignment_));
    if (new_assignment) {
      new_assignment->Restore();
    } else {
      solver->Fail();
    }
    return nullptr;
  }

 private:
  Assignment* Recreate(const std::function<int64_t(int64_t)>& next_accessor) {
    return recreate_->BuildSolutionFromRoutes(next_accessor);
  }

  const Assignment& assignment_;
  std::unique_ptr<RuinProcedure> ruin_;
  std::unique_ptr<RoutingFilteredHeuristic> recreate_;
};

DecisionBuilder* MakeRuinAndRecreateDecisionBuilder(
    const RoutingSearchParameters& parameters, RoutingModel* model,
    std::mt19937* rnd, const Assignment* assignment,
    std::function<bool()> stop_search,
    LocalSearchFilterManager* filter_manager) {
  std::unique_ptr<RuinProcedure> ruin = MakeRuinProcedure(
      parameters.iterated_local_search_parameters().ruin_recreate_parameters(),
      model, rnd);

  std::unique_ptr<RoutingFilteredHeuristic> recreate = MakeRecreateProcedure(
      parameters, model, std::move(stop_search), filter_manager);

  return model->solver()->RevAlloc(new RuinAndRecreateDecisionBuilder(
      assignment, std::move(ruin), std::move(recreate)));
}

DecisionBuilder* MakePerturbationDecisionBuilder(
    const RoutingSearchParameters& parameters, RoutingModel* model,
    std::mt19937* rnd, const Assignment* assignment,
    std::function<bool()> stop_search,
    LocalSearchFilterManager* filter_manager) {
  switch (
      parameters.iterated_local_search_parameters().perturbation_strategy()) {
    case PerturbationStrategy::RUIN_AND_RECREATE:
      return MakeRuinAndRecreateDecisionBuilder(
          parameters, model, rnd, assignment, std::move(stop_search),
          filter_manager);
    default:
      LOG(DFATAL) << "Unsupported perturbation strategy.";
      return nullptr;
  }
}

std::unique_ptr<NeighborAcceptanceCriterion> MakeNeighborAcceptanceCriterion(
    const RoutingModel& model, const RoutingSearchParameters& parameters,
    std::mt19937* rnd) {
  CHECK(parameters.has_iterated_local_search_parameters());
  switch (parameters.iterated_local_search_parameters().acceptance_strategy()) {
    case AcceptanceStrategy::GREEDY_DESCENT:
      return std::make_unique<GreedyDescentAcceptanceCriterion>();
    case AcceptanceStrategy::SIMULATED_ANNEALING:
      return std::make_unique<SimulatedAnnealingAcceptanceCriterion>(
          MakeCoolingSchedule(model, parameters, rnd), rnd);
    default:
      LOG(DFATAL) << "Unsupported acceptance strategy.";
      return nullptr;
  }
}

std::pair<double, double> GetSimulatedAnnealingTemperatures(
    const RoutingModel& model, const SimulatedAnnealingParameters& sa_params,
    std::mt19937* rnd) {
  if (!sa_params.automatic_temperatures()) {
    return {sa_params.initial_temperature(), sa_params.final_temperature()};
  }

  // In the unlikely case there are no vehicles (i.e., we will end up with an
  // "all unperformed" solution), we simply return 0.0 as initial and final
  // temperatures.
  if (model.vehicles() == 0) {
    return {0.0, 0.0};
  }

  std::vector<int> num_vehicles_of_class(model.GetCostClassesCount(), 0);
  for (int vehicle = 0; vehicle < model.vehicles(); ++vehicle) {
    const RoutingCostClassIndex cost_class =
        model.GetCostClassIndexOfVehicle(vehicle);
    ++num_vehicles_of_class[cost_class.value()];
  }

  std::uniform_int_distribution<int64_t> node_dist(0, model.nodes() - 1);

  const int sample_size = model.nodes();
  DCHECK_GT(sample_size, 0);

  std::vector<double> mean_arc_cost_for_class(model.GetCostClassesCount(), 0.0);
  for (int cost_class = 0; cost_class < model.GetCostClassesCount();
       ++cost_class) {
    if (num_vehicles_of_class[cost_class] == 0) {
      continue;
    }

    for (int i = 0; i < sample_size; ++i) {
      mean_arc_cost_for_class[cost_class] += model.GetArcCostForClass(
          node_dist(*rnd), node_dist(*rnd), cost_class);
    }
    mean_arc_cost_for_class[cost_class] /= sample_size;
  }

  double reference_temperature = 0;
  DCHECK_GT(model.vehicles(), 0);
  for (int cost_class = 0; cost_class < model.GetCostClassesCount();
       ++cost_class) {
    reference_temperature += mean_arc_cost_for_class[cost_class] *
                             num_vehicles_of_class[cost_class] /
                             model.vehicles();
  }

  return {reference_temperature * 0.1, reference_temperature * 0.001};
}

}  // namespace operations_research::routing
