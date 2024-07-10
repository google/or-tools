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
#include "absl/time/time.h"
#include "google/protobuf/repeated_ptr_field.h"
#include "ortools/base/logging.h"
#include "ortools/base/protoutil.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/routing/ils.pb.h"
#include "ortools/routing/parameters.pb.h"
#include "ortools/routing/routing.h"
#include "ortools/routing/search.h"
#include "ortools/routing/types.h"

namespace operations_research {
namespace {

// Returns global cheapest insertion parameters based on the given search
// parameters.
// TODO(user): consider having an ILS specific set of parameters.
GlobalCheapestInsertionFilteredHeuristic::GlobalCheapestInsertionParameters
MakeGlobalCheapestInsertionParameters(
    const RoutingSearchParameters& search_parameters, bool is_sequential) {
  GlobalCheapestInsertionFilteredHeuristic::GlobalCheapestInsertionParameters
      gci_parameters;
  gci_parameters.is_sequential = is_sequential;
  gci_parameters.farthest_seeds_ratio =
      search_parameters.cheapest_insertion_farthest_seeds_ratio();
  gci_parameters.neighbors_ratio =
      search_parameters.cheapest_insertion_first_solution_neighbors_ratio();
  gci_parameters.min_neighbors =
      search_parameters.cheapest_insertion_first_solution_min_neighbors();
  gci_parameters.use_neighbors_ratio_for_initialization =
      search_parameters
          .cheapest_insertion_first_solution_use_neighbors_ratio_for_initialization();  // NOLINT
  gci_parameters.add_unperformed_entries =
      search_parameters.cheapest_insertion_add_unperformed_entries();
  return gci_parameters;
}

// Returns savings parameters based on the given search parameters.
// TODO(user): consider having an ILS specific set of parameters.
SavingsFilteredHeuristic::SavingsParameters MakeSavingsParameters(
    const RoutingSearchParameters& search_parameters) {
  SavingsFilteredHeuristic::SavingsParameters savings_parameters;
  savings_parameters.neighbors_ratio =
      search_parameters.savings_neighbors_ratio();
  savings_parameters.max_memory_usage_bytes =
      search_parameters.savings_max_memory_usage_bytes();
  savings_parameters.add_reverse_arcs =
      search_parameters.savings_add_reverse_arcs();
  savings_parameters.arc_coefficient =
      search_parameters.savings_arc_coefficient();
  return savings_parameters;
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
    default:
      LOG(DFATAL) << "Unsupported ruin procedure.";
      return nullptr;
  }
}

// Returns the ruin procedures associated with the given ruin strategies.
std::vector<std::unique_ptr<RuinProcedure>> MakeRuinProcedures(
    RoutingModel* model, std::mt19937* rnd,
    const google::protobuf::RepeatedPtrField<
        ::operations_research::RuinStrategy>& ruin_strategies,
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
    const std::vector<std::unique_ptr<RuinProcedure>>& ruins,
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
  switch (parameters.iterated_local_search_parameters()
              .ruin_recreate_parameters()
              .recreate_strategy()) {
    case FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION:
      return std::make_unique<LocalCheapestInsertionFilteredHeuristic>(
          model, std::move(stop_search),
          absl::bind_front(&RoutingModel::GetArcCostForVehicle, model),
          parameters.local_cheapest_cost_insertion_pickup_delivery_strategy(),
          filter_manager, model->GetBinCapacities());
    case FirstSolutionStrategy::LOCAL_CHEAPEST_COST_INSERTION:
      return std::make_unique<LocalCheapestInsertionFilteredHeuristic>(
          model, std::move(stop_search),
          /*evaluator=*/nullptr,
          parameters.local_cheapest_cost_insertion_pickup_delivery_strategy(),
          filter_manager, model->GetBinCapacities());
    case FirstSolutionStrategy::SEQUENTIAL_CHEAPEST_INSERTION: {
      GlobalCheapestInsertionFilteredHeuristic::
          GlobalCheapestInsertionParameters gci_parameters =
              MakeGlobalCheapestInsertionParameters(parameters,
                                                    /*is_sequential=*/true);
      return std::make_unique<GlobalCheapestInsertionFilteredHeuristic>(
          model, std::move(stop_search),
          absl::bind_front(&RoutingModel::GetArcCostForVehicle, model),
          [model](int64_t i) { return model->UnperformedPenaltyOrValue(0, i); },
          filter_manager, gci_parameters);
    }
    case FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION: {
      GlobalCheapestInsertionFilteredHeuristic::
          GlobalCheapestInsertionParameters gci_parameters =
              MakeGlobalCheapestInsertionParameters(parameters,
                                                    /*is_sequential=*/false);
      return std::make_unique<GlobalCheapestInsertionFilteredHeuristic>(
          model, std::move(stop_search),
          absl::bind_front(&RoutingModel::GetArcCostForVehicle, model),
          [model](int64_t i) { return model->UnperformedPenaltyOrValue(0, i); },
          filter_manager, gci_parameters);
    }
    case FirstSolutionStrategy::SAVINGS: {
      return std::make_unique<SequentialSavingsFilteredHeuristic>(
          model, std::move(stop_search), MakeSavingsParameters(parameters),
          filter_manager);
    }
    case FirstSolutionStrategy::PARALLEL_SAVINGS: {
      return std::make_unique<ParallelSavingsFilteredHeuristic>(
          model, std::move(stop_search), MakeSavingsParameters(parameters),
          filter_manager);
    }
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

}  // namespace

RoutingSolution::RoutingSolution(const RoutingModel& model) : model_(model) {
  const int all_nodes = model.Size() + model.vehicles();

  nexts_.resize(all_nodes, -1);
  prevs_.resize(all_nodes, -1);
}

void RoutingSolution::Reset(const Assignment* assignment) {
  assignment_ = assignment;

  // TODO(user): consider resetting only previously set values.
  nexts_.assign(nexts_.size(), -1);
  prevs_.assign(prevs_.size(), -1);
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
  while (curr != end) {
    const int64_t next = assignment_->Value(model_.NextVar(curr));

    nexts_[curr] = next;
    prevs_[curr] = prev;

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

bool RoutingSolution::IsSingleCustomerRoute(int vehicle) const {
  const int64_t start = model_.Start(vehicle);
  DCHECK(BelongsToInitializedRoute(start));

  return nexts_[nexts_[start]] == model_.End(vehicle);
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

  nexts_[prev] = next;
  prevs_[next] = prev;

  nexts_[node_index] = node_index;
  prevs_[node_index] = node_index;
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
           /*only_sort_neighbors_for_partial_neighborhoods=*/false})),
      num_routes_(num_routes),
      rnd_(*rnd),
      customer_dist_(0, model->Size() - 1),
      removed_routes_(model->vehicles()) {}

std::function<int64_t(int64_t)> CloseRoutesRemovalRuinProcedure::Ruin(
    const Assignment* assignment) {
  removed_routes_.SparseClearAll();

  if (num_routes_ > 0 && HasPerformedNodes(model_, *assignment)) {
    int64_t seed_node;
    int seed_route = -1;
    do {
      seed_node = customer_dist_(rnd_);
      seed_route = assignment->Value(model_.VehicleVar(seed_node));
    } while (model_.IsStart(seed_node) || seed_route == -1);
    DCHECK(!model_.IsEnd(seed_node));

    removed_routes_.Set(seed_route);

    const RoutingCostClassIndex cost_class_index =
        model_.GetCostClassIndexOfVehicle(seed_route);

    const std::vector<int>& neighbors =
        neighbors_manager_->GetNeighborsOfNodeForCostClass(
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
           /*only_sort_neighbors_for_partial_neighborhoods=*/false})),
      rnd_(*rnd),
      walk_length_(walk_length),
      customer_dist_(0, model->Size() - 1) {}

std::function<int64_t(int64_t)> RandomWalkRemovalRuinProcedure::Ruin(
    const Assignment* assignment) {
  if (!HasPerformedNodes(model_, *assignment) || walk_length_ == 0) {
    return [&model = model_, assignment](int64_t node) {
      return assignment->Value(model.NextVar(node));
    };
  }

  routing_solution_.Reset(assignment);

  int64_t seed_node;
  do {
    seed_node = customer_dist_(rnd_);
  } while (model_.IsStart(seed_node) ||
           assignment->Value(model_.VehicleVar(seed_node)) == -1);
  DCHECK(!model_.IsEnd(seed_node));

  int64_t curr_node = seed_node;

  int walk_length = walk_length_;

  while (walk_length-- > 0) {
    // Remove the active siblings node of curr before selecting next, so that
    // we do not accidentally end up with next being one of these sibling
    // nodes.
    RemovePickupDeliverySiblings(assignment, curr_node);

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

void RandomWalkRemovalRuinProcedure::RemovePickupDeliverySiblings(
    const Assignment* assignment, int node) {
  if (const std::optional<int64_t> sibling_node =
          model_.GetFirstMatchingPickupDeliverySibling(
              node,
              [&routing_solution = routing_solution_](int64_t node) {
                return routing_solution.CanBeRemoved(node);
              });
      sibling_node.has_value()) {
    const int sibling_vehicle =
        assignment->Value(model_.VehicleVar(sibling_node.value()));
    DCHECK_NE(sibling_vehicle, -1);

    routing_solution_.InitializeRouteInfoIfNeeded(sibling_vehicle);
    routing_solution_.RemoveNode(sibling_node.value());
  }
}

int64_t RandomWalkRemovalRuinProcedure::GetNextNodeToRemove(
    const Assignment* assignment, int node) {
  const int curr_vehicle = assignment->Value(model_.VehicleVar(node));
  routing_solution_.InitializeRouteInfoIfNeeded(curr_vehicle);

  if (!routing_solution_.IsSingleCustomerRoute(curr_vehicle) &&
      boolean_dist_(rnd_)) {
    const bool move_forward = boolean_dist_(rnd_);
    int64_t next_node =
        move_forward ? routing_solution_.GetNextNodeIndex(node)
                     : routing_solution_.GetInitializedPrevNodeIndex(node);
    if (model_.IsStart(next_node) || model_.IsEnd(next_node)) {
      // The Next or Prev of the node is a vehicle start/end, so we go in the
      // opposite direction from before (and since the route isn't a
      // single-customer route we shouldn't have a start/end again).
      next_node = move_forward
                      ? routing_solution_.GetInitializedPrevNodeIndex(node)
                      : routing_solution_.GetNextNodeIndex(node);
    }
    DCHECK(!model_.IsStart(next_node));
    DCHECK(!model_.IsEnd(next_node));
    return next_node;
  }

  // Pick the next node by jumping to a neighboring (non empty) route,
  // otherwise.
  const RoutingCostClassIndex cost_class_index =
      model_.GetCostClassIndexOfVehicle(curr_vehicle);

  const std::vector<int>& neighbors =
      neighbors_manager_->GetNeighborsOfNodeForCostClass(
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

}  // namespace operations_research
