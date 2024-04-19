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

#include "ortools/constraint_solver/routing_ils.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <random>
#include <utility>
#include <vector>

#include "absl/functional/bind_front.h"
#include "absl/log/check.h"
#include "absl/time/time.h"
#include "ortools/base/logging.h"
#include "ortools/base/protoutil.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/routing.h"
#include "ortools/constraint_solver/routing_search.h"
#include "ortools/constraint_solver/routing_types.h"
#include "ortools/routing/ils.pb.h"
#include "ortools/routing/parameters.pb.h"

namespace operations_research {
namespace {

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

// Returns a ruin procedure based to the given parameters.
std::unique_ptr<RuinProcedure> MakeRuinProcedure(
    const RuinRecreateParameters& parameters, RoutingModel* model,
    std::mt19937* rnd) {
  const int num_non_start_end_nodes = model->Size() - model->vehicles();
  const uint32_t preferred_num_neighbors =
      parameters.route_selection_neighbors_ratio() * num_non_start_end_nodes;

  switch (parameters.ruin_strategy()) {
    case RuinStrategy::SPATIALLY_CLOSE_ROUTES_REMOVAL:
      return std::make_unique<CloseRoutesRemovalRuinProcedure>(
          model, rnd, parameters.num_ruined_routes(),
          std::min(parameters.route_selection_max_neighbors(),
                   std::max(parameters.route_selection_min_neighbors(),
                            preferred_num_neighbors)));
      break;
    default:
      LOG(ERROR) << "Unsupported ruin procedure.";
      return nullptr;
  }
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
      LOG(ERROR) << "Unsupported recreate procedure.";
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
      LOG(ERROR) << "Unsupported cooling schedule strategy.";
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

}  // namespace

CloseRoutesRemovalRuinProcedure::CloseRoutesRemovalRuinProcedure(
    RoutingModel* model, std::mt19937* rnd, size_t num_routes,
    int num_neighbors_for_route_selection)
    : model_(*model),
      neighbors_manager_(model->GetOrCreateNodeNeighborsByCostClass(
          num_neighbors_for_route_selection,
          /*add_vehicle_starts_to_neighbors=*/false)),
      num_routes_(num_routes),
      rnd_(*rnd),
      customer_dist_(0, model->Size() - 1),
      removed_routes_(model->vehicles()) {}

std::function<int64_t(int64_t)> CloseRoutesRemovalRuinProcedure::Ruin(
    const Assignment* assignment) {
  removed_routes_.SparseClearAll();

  if (num_routes_ > 0 && HasPerformedNodes(assignment)) {
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

bool CloseRoutesRemovalRuinProcedure::HasPerformedNodes(
    const Assignment* assignment) {
  for (int v = 0; v < model_.vehicles(); ++v) {
    if (model_.Next(*assignment, model_.Start(v)) != model_.End(v)) {
      return true;
    }
  }
  return false;
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
      LOG(ERROR) << "Unsupported perturbation strategy.";
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
      LOG(ERROR) << "Unsupported acceptance strategy.";
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
