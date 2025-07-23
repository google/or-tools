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

#include "ortools/constraint_solver/routing_parameters.h"

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/extension_set.h"
#include "google/protobuf/message.h"
#include "ortools/base/logging.h"
#include "ortools/base/proto_enum_utils.h"
#include "ortools/base/protoutil.h"
#include "ortools/base/types.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/routing_enums.pb.h"
#include "ortools/constraint_solver/routing_heuristic_parameters.pb.h"
#include "ortools/constraint_solver/routing_ils.pb.h"
#include "ortools/constraint_solver/routing_parameters.pb.h"
#include "ortools/constraint_solver/solver_parameters.pb.h"
#include "ortools/port/proto_utils.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/optional_boolean.pb.h"
#include "ortools/util/testing_utils.h"

namespace operations_research {

RoutingModelParameters DefaultRoutingModelParameters() {
  RoutingModelParameters parameters;
  ConstraintSolverParameters* const solver_parameters =
      parameters.mutable_solver_parameters();
  *solver_parameters = Solver::DefaultSolverParameters();
  solver_parameters->set_compress_trail(
      ConstraintSolverParameters::COMPRESS_WITH_ZLIB);
  solver_parameters->set_skip_locally_optimal_paths(true);
  parameters.set_reduce_vehicle_cost_model(true);
  return parameters;
}

namespace {
IteratedLocalSearchParameters CreateDefaultIteratedLocalSearchParameters() {
  IteratedLocalSearchParameters ils;
  ils.set_perturbation_strategy(PerturbationStrategy::RUIN_AND_RECREATE);
  RuinRecreateParameters* rr = ils.mutable_ruin_recreate_parameters();
  // NOTE: As of 07/2024, we no longer add any default ruin strategies to the
  // default RuinRecreateParameters, because since ruin_strategies is a repeated
  // field, it will only be appended to when merging with a proto containing
  // this field.
  // A ruin strategy can be added as follows.
  // rr->add_ruin_strategies()
  //     ->mutable_spatially_close_routes()
  //     ->set_num_ruined_routes(2);
  rr->set_ruin_composition_strategy(RuinCompositionStrategy::UNSET);
  rr->mutable_recreate_strategy()->set_heuristic(
      FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION);
  rr->set_route_selection_neighbors_ratio(1.0);
  rr->set_route_selection_min_neighbors(10);
  rr->set_route_selection_max_neighbors(100);
  ils.set_improve_perturbed_solution(true);
  ils.set_acceptance_strategy(AcceptanceStrategy::GREEDY_DESCENT);
  SimulatedAnnealingParameters* sa =
      ils.mutable_simulated_annealing_parameters();
  sa->set_cooling_schedule_strategy(CoolingScheduleStrategy::EXPONENTIAL);
  sa->set_initial_temperature(100.0);
  sa->set_final_temperature(0.01);
  sa->set_automatic_temperatures(false);
  return ils;
}

RoutingSearchParameters CreateDefaultRoutingSearchParameters() {
  RoutingSearchParameters p;
  p.set_first_solution_strategy(FirstSolutionStrategy::AUTOMATIC);
  p.set_use_unfiltered_first_solution_strategy(false);
  p.mutable_savings_parameters()->set_neighbors_ratio(1);
  p.mutable_savings_parameters()->set_max_memory_usage_bytes(6e9);
  p.mutable_savings_parameters()->set_add_reverse_arcs(false);
  p.mutable_savings_parameters()->set_arc_coefficient(1);
  p.set_cheapest_insertion_farthest_seeds_ratio(0);
  p.set_cheapest_insertion_first_solution_neighbors_ratio(1);
  p.set_cheapest_insertion_first_solution_min_neighbors(1);
  p.set_cheapest_insertion_ls_operator_neighbors_ratio(1);
  p.set_cheapest_insertion_ls_operator_min_neighbors(1);
  p.set_cheapest_insertion_first_solution_use_neighbors_ratio_for_initialization(  // NOLINT
      false);
  p.set_cheapest_insertion_add_unperformed_entries(false);
  p.mutable_local_cheapest_insertion_parameters()->set_pickup_delivery_strategy(
      LocalCheapestInsertionParameters::BEST_PICKUP_THEN_BEST_DELIVERY);
  p.mutable_local_cheapest_cost_insertion_parameters()
      ->set_pickup_delivery_strategy(
          LocalCheapestInsertionParameters::BEST_PICKUP_DELIVERY_PAIR);
  RoutingSearchParameters::LocalSearchNeighborhoodOperators* o =
      p.mutable_local_search_operators();
  o->set_use_relocate(BOOL_TRUE);
  o->set_use_relocate_pair(BOOL_TRUE);
  o->set_use_light_relocate_pair(BOOL_TRUE);
  o->set_use_relocate_subtrip(BOOL_TRUE);
  o->set_use_relocate_neighbors(BOOL_FALSE);
  o->set_use_exchange(BOOL_TRUE);
  o->set_use_exchange_pair(BOOL_TRUE);
  o->set_use_exchange_subtrip(BOOL_TRUE);
  o->set_use_cross(BOOL_TRUE);
  o->set_use_cross_exchange(BOOL_FALSE);
  o->set_use_relocate_expensive_chain(BOOL_TRUE);
  o->set_use_two_opt(BOOL_TRUE);
  o->set_use_or_opt(BOOL_TRUE);
  o->set_use_lin_kernighan(BOOL_TRUE);
  o->set_use_tsp_opt(BOOL_FALSE);
  o->set_use_make_active(BOOL_TRUE);
  o->set_use_relocate_and_make_active(BOOL_FALSE);  // costly if true by default
  o->set_use_exchange_and_make_active(BOOL_FALSE);  // very costly
  o->set_use_exchange_path_start_ends_and_make_active(BOOL_FALSE);
  o->set_use_make_inactive(BOOL_TRUE);
  o->set_use_make_chain_inactive(BOOL_TRUE);
  o->set_use_swap_active(BOOL_TRUE);
  o->set_use_swap_active_chain(BOOL_TRUE);
  o->set_use_extended_swap_active(BOOL_FALSE);
  o->set_use_shortest_path_swap_active(BOOL_TRUE);
  o->set_use_shortest_path_two_opt(BOOL_TRUE);
  o->set_use_node_pair_swap_active(BOOL_FALSE);
  o->set_use_path_lns(BOOL_FALSE);
  o->set_use_full_path_lns(BOOL_FALSE);
  o->set_use_tsp_lns(BOOL_FALSE);
  o->set_use_inactive_lns(BOOL_FALSE);
  o->set_use_global_cheapest_insertion_path_lns(BOOL_TRUE);
  o->set_use_local_cheapest_insertion_path_lns(BOOL_TRUE);
  o->set_use_relocate_path_global_cheapest_insertion_insert_unperformed(
      BOOL_TRUE);
  o->set_use_global_cheapest_insertion_expensive_chain_lns(BOOL_FALSE);
  o->set_use_local_cheapest_insertion_expensive_chain_lns(BOOL_FALSE);
  o->set_use_global_cheapest_insertion_close_nodes_lns(BOOL_FALSE);
  o->set_use_local_cheapest_insertion_close_nodes_lns(BOOL_FALSE);
  o->set_use_global_cheapest_insertion_visit_types_lns(BOOL_TRUE);
  o->set_use_local_cheapest_insertion_visit_types_lns(BOOL_TRUE);
  p.set_ls_operator_neighbors_ratio(1);
  p.set_ls_operator_min_neighbors(1);
  p.set_use_multi_armed_bandit_concatenate_operators(false);
  p.set_multi_armed_bandit_compound_operator_memory_coefficient(0.04);
  p.set_multi_armed_bandit_compound_operator_exploration_coefficient(1e12);
  p.set_max_swap_active_chain_size(10);
  p.set_relocate_expensive_chain_num_arcs_to_consider(4);
  p.set_heuristic_expensive_chain_lns_num_arcs_to_consider(4);
  p.set_heuristic_close_nodes_lns_num_nodes(5);
  p.set_local_search_metaheuristic(LocalSearchMetaheuristic::AUTOMATIC);
  p.set_num_max_local_optima_before_metaheuristic_switch(200);
  p.set_guided_local_search_lambda_coefficient(0.1);
  p.set_guided_local_search_reset_penalties_on_new_best_solution(false);
  p.set_use_depth_first_search(false);
  p.set_use_cp(BOOL_TRUE);
  p.set_use_cp_sat(BOOL_FALSE);
  p.set_use_generalized_cp_sat(BOOL_FALSE);
  p.mutable_sat_parameters()->set_linearization_level(2);
  p.mutable_sat_parameters()->set_num_workers(1);
  p.set_report_intermediate_cp_sat_solutions(false);
  p.set_fallback_to_cp_sat_size_threshold(20);
  p.set_continuous_scheduling_solver(RoutingSearchParameters::SCHEDULING_GLOP);
  p.set_mixed_integer_scheduling_solver(
      RoutingSearchParameters::SCHEDULING_CP_SAT);
  p.set_disable_scheduling_beware_this_may_degrade_performance(false);
  p.set_optimization_step(0.0);
  p.set_number_of_solutions_to_collect(1);
  // No global time_limit by default.
  p.set_solution_limit(kint64max);
  p.mutable_lns_time_limit()->set_nanos(100000000);  // 0.1s.
  p.set_secondary_ls_time_limit_ratio(0);
  p.set_use_full_propagation(false);
  p.set_log_search(false);
  p.set_log_cost_scaling_factor(1.0);
  p.set_log_cost_offset(0.0);
  p.set_use_iterated_local_search(false);
  *p.mutable_iterated_local_search_parameters() =
      CreateDefaultIteratedLocalSearchParameters();

  const std::string error = FindErrorInRoutingSearchParameters(p);
  LOG_IF(DFATAL, !error.empty())
      << "The default search parameters aren't valid: " << error;
  return p;
}

RoutingSearchParameters CreateDefaultSecondaryRoutingSearchParameters() {
  RoutingSearchParameters p = CreateDefaultRoutingSearchParameters();
  p.set_local_search_metaheuristic(LocalSearchMetaheuristic::GREEDY_DESCENT);
  p.set_use_iterated_local_search(false);
  *p.mutable_iterated_local_search_parameters() =
      CreateDefaultIteratedLocalSearchParameters();
  RoutingSearchParameters::LocalSearchNeighborhoodOperators* o =
      p.mutable_local_search_operators();
  o->set_use_relocate(BOOL_TRUE);
  o->set_use_relocate_pair(BOOL_FALSE);
  o->set_use_light_relocate_pair(BOOL_TRUE);
  o->set_use_relocate_subtrip(BOOL_TRUE);
  o->set_use_relocate_neighbors(BOOL_FALSE);
  o->set_use_exchange(BOOL_TRUE);
  o->set_use_exchange_pair(BOOL_TRUE);
  o->set_use_exchange_subtrip(BOOL_TRUE);
  o->set_use_cross(BOOL_TRUE);
  o->set_use_cross_exchange(BOOL_FALSE);
  o->set_use_relocate_expensive_chain(BOOL_FALSE);
  o->set_use_two_opt(BOOL_TRUE);
  o->set_use_or_opt(BOOL_TRUE);
  o->set_use_lin_kernighan(BOOL_TRUE);
  o->set_use_tsp_opt(BOOL_FALSE);
  o->set_use_make_active(BOOL_FALSE);
  o->set_use_relocate_and_make_active(BOOL_FALSE);
  o->set_use_exchange_and_make_active(BOOL_FALSE);
  o->set_use_exchange_path_start_ends_and_make_active(BOOL_FALSE);
  o->set_use_make_inactive(BOOL_FALSE);
  o->set_use_make_chain_inactive(BOOL_FALSE);
  o->set_use_swap_active(BOOL_FALSE);
  o->set_use_swap_active_chain(BOOL_FALSE);
  o->set_use_extended_swap_active(BOOL_FALSE);
  o->set_use_shortest_path_swap_active(BOOL_FALSE);
  o->set_use_shortest_path_two_opt(BOOL_FALSE);
  o->set_use_node_pair_swap_active(BOOL_FALSE);
  o->set_use_path_lns(BOOL_FALSE);
  o->set_use_full_path_lns(BOOL_FALSE);
  o->set_use_tsp_lns(BOOL_FALSE);
  o->set_use_inactive_lns(BOOL_FALSE);
  o->set_use_global_cheapest_insertion_path_lns(BOOL_FALSE);
  o->set_use_local_cheapest_insertion_path_lns(BOOL_FALSE);
  o->set_use_relocate_path_global_cheapest_insertion_insert_unperformed(
      BOOL_FALSE);
  const std::string error = FindErrorInRoutingSearchParameters(p);
  LOG_IF(DFATAL, !error.empty())
      << "The default secondary search parameters aren't valid: " << error;
  return p;
}
}  // namespace

// static
RoutingSearchParameters DefaultRoutingSearchParameters() {
  static const auto* default_parameters =
      new RoutingSearchParameters(CreateDefaultRoutingSearchParameters());
  return *default_parameters;
}

RoutingSearchParameters DefaultSecondaryRoutingSearchParameters() {
  static const auto* default_parameters = new RoutingSearchParameters(
      CreateDefaultSecondaryRoutingSearchParameters());
  return *default_parameters;
}

namespace {
bool IsValidNonNegativeDuration(const google::protobuf::Duration& d) {
  const auto status_or_duration = util_time::DecodeGoogleApiProto(d);
  return status_or_duration.ok() &&
         status_or_duration.value() >= absl::ZeroDuration();
}

// Searches for errors in LocalCheapestInsertionParameters and appends them to
// the given `errors` vector.
void FindErrorsInLocalCheapestInsertionParameters(
    absl::string_view prefix,
    const LocalCheapestInsertionParameters& parameters,
    std::vector<std::string>& errors) {
  using absl::StrCat;

  absl::flat_hash_map<
      LocalCheapestInsertionParameters::InsertionSortingProperty, int>
      sorting_properties_map;
  for (const LocalCheapestInsertionParameters::InsertionSortingProperty
           property :
       REPEATED_ENUM_ADAPTER(parameters, insertion_sorting_properties)) {
    if (property ==
        LocalCheapestInsertionParameters::SORTING_PROPERTY_UNSPECIFIED) {
      errors.emplace_back(StrCat(
          prefix, " - Invalid insertion sorting property: ",
          LocalCheapestInsertionParameters::InsertionSortingProperty_Name(
              LocalCheapestInsertionParameters::SORTING_PROPERTY_UNSPECIFIED)));
    }
    const int occurrences = sorting_properties_map[property]++;
    if (occurrences == 2) {
      errors.emplace_back(StrCat(
          prefix, " - Duplicate insertion sorting property: ",
          LocalCheapestInsertionParameters::InsertionSortingProperty_Name(
              property)));
    }
    if (property == LocalCheapestInsertionParameters::SORTING_PROPERTY_RANDOM &&
        parameters.insertion_sorting_properties().size() > 1) {
      errors.emplace_back(
          StrCat(prefix,
                 " - SORTING_PROPERTY_RANDOM cannot be used in conjunction "
                 "with other properties."));
    }
  }
}

void FindErrorsInRecreateParameters(
    const FirstSolutionStrategy::Value heuristic,
    const RecreateParameters& parameters, std::vector<std::string>& errors) {
  switch (parameters.parameters_case()) {
    case RecreateParameters::kLocalCheapestInsertion: {
      const std::string prefix =
          heuristic == FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION
              ? "Local cheapest insertion (recreate heuristic)"
              : "Local cheapest cost insertion (recreate heuristic)";
      FindErrorsInLocalCheapestInsertionParameters(
          prefix, parameters.local_cheapest_insertion(), errors);
      break;
    }
    default:
      LOG(DFATAL) << "Unsupported unset recreate parameters.";
      break;
  }
}

std::string GetRecreateParametersName(const RecreateParameters& parameters) {
  switch (parameters.parameters_case()) {
    case RecreateParameters::kLocalCheapestInsertion:
      return "local_cheapest_insertion";
    case RecreateParameters::PARAMETERS_NOT_SET:
      return "PARAMETERS_NOT_SET";
  }
}

// Searches for errors in ILS parameters and appends them to the given `errors`
// vector.
void FindErrorsInIteratedLocalSearchParameters(
    const RoutingSearchParameters& search_parameters,
    std::vector<std::string>& errors) {
  using absl::StrCat;
  if (!search_parameters.use_iterated_local_search()) {
    return;
  }

  if (!search_parameters.has_iterated_local_search_parameters()) {
    errors.emplace_back(
        "use_iterated_local_search is true but "
        "iterated_local_search_parameters are missing.");
    return;
  }

  const IteratedLocalSearchParameters& ils =
      search_parameters.iterated_local_search_parameters();

  if (ils.perturbation_strategy() == PerturbationStrategy::UNSET) {
    errors.emplace_back(
        StrCat("Invalid value for "
               "iterated_local_search_parameters.perturbation_strategy: ",
               ils.perturbation_strategy()));
  }

  if (ils.perturbation_strategy() == PerturbationStrategy::RUIN_AND_RECREATE) {
    if (!ils.has_ruin_recreate_parameters()) {
      errors.emplace_back(StrCat(
          "iterated_local_search_parameters.perturbation_strategy is ",
          PerturbationStrategy::RUIN_AND_RECREATE,
          " but iterated_local_search_parameters.ruin_recreate_parameters are "
          "missing."));
      return;
    }

    const RuinRecreateParameters& rr = ils.ruin_recreate_parameters();

    if (rr.ruin_strategies().empty()) {
      errors.emplace_back(
          StrCat("iterated_local_search_parameters.ruin_recreate_parameters."
                 "ruin_strategies is empty"));
    }

    if (rr.ruin_strategies().size() > 1 &&
        rr.ruin_composition_strategy() == RuinCompositionStrategy::UNSET) {
      errors.emplace_back(StrCat(
          "iterated_local_search_parameters.ruin_recreate_parameters."
          "ruin_composition_strategy cannot be unset when more than one ruin "
          "strategy is defined"));
    }

    for (const auto& ruin : rr.ruin_strategies()) {
      if (ruin.strategy_case() == RuinStrategy::kSpatiallyCloseRoutes &&
          ruin.spatially_close_routes().num_ruined_routes() == 0) {
        errors.emplace_back(StrCat(
            "iterated_local_search_parameters.ruin_recreate_parameters."
            "ruin_strategy is set to SpatiallyCloseRoutesRuinStrategy"
            " but spatially_close_routes.num_ruined_routes is 0 (should be "
            "strictly positive)"));
      } else if (ruin.strategy_case() == RuinStrategy::kRandomWalk &&
                 ruin.random_walk().num_removed_visits() == 0) {
        errors.emplace_back(
            StrCat("iterated_local_search_parameters.ruin_recreate_parameters."
                   "ruin_strategy is set to RandomWalkRuinStrategy"
                   " but random_walk.num_removed_visits is 0 (should be "
                   "strictly positive)"));
      } else if (ruin.strategy_case() == RuinStrategy::kSisr) {
        if (ruin.sisr().avg_num_removed_visits() == 0) {
          errors.emplace_back(
              "iterated_local_search_parameters.ruin_recreate_parameters."
              "ruin is set to SISRRuinStrategy"
              " but sisr.avg_num_removed_visits is 0 (should be strictly "
              "positive)");
        }
        if (ruin.sisr().max_removed_sequence_size() == 0) {
          errors.emplace_back(
              "iterated_local_search_parameters.ruin_recreate_parameters.ruin "
              "is set to SISRRuinStrategy but "
              "sisr.max_removed_sequence_size is 0 (should be strictly "
              "positive)");
        }
        if (ruin.sisr().bypass_factor() < 0 ||
            ruin.sisr().bypass_factor() > 1) {
          errors.emplace_back(StrCat(
              "iterated_local_search_parameters.ruin_recreate_parameters."
              "ruin is set to SISRRuinStrategy"
              " but sisr.bypass_factor is not in [0, 1]"));
        }
      }
    }

    if (const double ratio = rr.route_selection_neighbors_ratio();
        std::isnan(ratio) || ratio <= 0 || ratio > 1) {
      errors.emplace_back(
          StrCat("Invalid "
                 "iterated_local_search_parameters.ruin_recreate_parameters."
                 "route_selection_neighbors_ratio: ",
                 ratio));
    }
    if (rr.route_selection_min_neighbors() == 0) {
      errors.emplace_back(
          StrCat("iterated_local_search_parameters.ruin_recreate_parameters."
                 "route_selection_min_neighbors must be positive"));
    }
    if (rr.route_selection_min_neighbors() >
        rr.route_selection_max_neighbors()) {
      errors.emplace_back(
          StrCat("iterated_local_search_parameters.ruin_recreate_parameters."
                 "route_selection_min_neighbors cannot be greater than "
                 "iterated_local_search_parameters.ruin_recreate_parameters."
                 "route_selection_max_neighbors"));
    }

    const FirstSolutionStrategy::Value recreate_heuristic =
        rr.recreate_strategy().heuristic();
    if (recreate_heuristic == FirstSolutionStrategy::UNSET) {
      errors.emplace_back(
          StrCat("Invalid value for "
                 "iterated_local_search_parameters.ruin_recreate_parameters."
                 "recreate_strategy.heuristic: ",
                 FirstSolutionStrategy::Value_Name(recreate_heuristic)));
    }

    if (rr.recreate_strategy().has_parameters()) {
      const RecreateParameters& recreate_params =
          rr.recreate_strategy().parameters();
      if (recreate_params.parameters_case() ==
          RecreateParameters::PARAMETERS_NOT_SET) {
        errors.emplace_back(
            StrCat("Invalid value for "
                   "iterated_local_search_parameters.ruin_recreate_parameters."
                   "recreate_strategy.parameters: ",
                   GetRecreateParametersName(recreate_params)));
      } else {
        const absl::flat_hash_map<FirstSolutionStrategy::Value,
                                  RecreateParameters::ParametersCase>
            strategy_to_parameters_case_map = {
                {FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION,
                 RecreateParameters::kLocalCheapestInsertion},
                {FirstSolutionStrategy::LOCAL_CHEAPEST_COST_INSERTION,
                 RecreateParameters::kLocalCheapestInsertion}};

        const RecreateParameters& recreate_params =
            rr.recreate_strategy().parameters();

        if (const auto params =
                strategy_to_parameters_case_map.find(recreate_heuristic);
            params == strategy_to_parameters_case_map.end() ||
            recreate_params.parameters_case() != params->second) {
          errors.emplace_back(
              StrCat("recreate_strategy.heuristic is set to ",
                     FirstSolutionStrategy::Value_Name(recreate_heuristic),
                     " but recreate_strategy.parameters define ",
                     GetRecreateParametersName(recreate_params)));
        } else {
          FindErrorsInRecreateParameters(recreate_heuristic, recreate_params,
                                         errors);
        }
      }
    }
  }

  if (ils.acceptance_strategy() == AcceptanceStrategy::UNSET) {
    errors.emplace_back(
        StrCat("Invalid value for "
               "iterated_local_search_parameters.acceptance_strategy: ",
               ils.acceptance_strategy()));
  }

  if (ils.acceptance_strategy() == AcceptanceStrategy::SIMULATED_ANNEALING) {
    if (!ils.has_simulated_annealing_parameters()) {
      errors.emplace_back(
          StrCat("iterated_local_search_parameters.acceptance_strategy is ",
                 AcceptanceStrategy::SIMULATED_ANNEALING,
                 " but "
                 "iterated_local_search_parameters.simulated_annealing_"
                 "parameters are missing."));
      return;
    }

    const SimulatedAnnealingParameters& sa_params =
        ils.simulated_annealing_parameters();

    if (sa_params.cooling_schedule_strategy() ==
        CoolingScheduleStrategy::UNSET) {
      errors.emplace_back(
          StrCat("Invalid value for "
                 "iterated_local_search_parameters.simulated_annealing_"
                 "parameters.cooling_schedule_strategy: ",
                 sa_params.cooling_schedule_strategy()));
    }

    if (!sa_params.automatic_temperatures()) {
      if (sa_params.initial_temperature() < sa_params.final_temperature()) {
        errors.emplace_back(
            "iterated_local_search_parameters.simulated_annealing_parameters."
            "initial_temperature cannot be lower than "
            "iterated_local_search_parameters.simulated_annealing_parameters."
            "final_temperature.");
      }

      if (sa_params.initial_temperature() < 1e-9) {
        errors.emplace_back(
            "iterated_local_search_parameters.simulated_annealing_parameters."
            "initial_temperature cannot be lower than 1e-9.");
      }

      if (sa_params.final_temperature() < 1e-9) {
        errors.emplace_back(
            "iterated_local_search_parameters.simulated_annealing_parameters."
            "final_temperature cannot be lower than 1e-9.");
      }
    }
  }
}

}  // namespace

std::string FindErrorInRoutingSearchParameters(
    const RoutingSearchParameters& search_parameters) {
  const std::vector<std::string> errors =
      FindErrorsInRoutingSearchParameters(search_parameters);
  return (errors.empty()) ? "" : errors[0];
}

std::vector<std::string> FindErrorsInRoutingSearchParameters(
    const RoutingSearchParameters& search_parameters) {
  using absl::StrCat;
  std::vector<std::string> errors;

  // Check that all local search operators are set to either BOOL_TRUE or
  // BOOL_FALSE (and not BOOL_UNSPECIFIED). Do that only in non-portable mode,
  // since it needs proto reflection etc.
#if !defined(__ANDROID__) && !defined(__wasm__)
  {
    using Reflection = google::protobuf::Reflection;
    using Descriptor = google::protobuf::Descriptor;
    using FieldDescriptor = google::protobuf::FieldDescriptor;
    const RoutingSearchParameters::LocalSearchNeighborhoodOperators& operators =
        search_parameters.local_search_operators();
    const Reflection* ls_reflection = operators.GetReflection();
    const Descriptor* ls_descriptor = operators.GetDescriptor();
    for (int /*this is NOT the field's tag number*/ field_index = 0;
         field_index < ls_descriptor->field_count(); ++field_index) {
      const FieldDescriptor* field = ls_descriptor->field(field_index);
      if (field->type() != FieldDescriptor::TYPE_ENUM ||
          field->enum_type() != OptionalBoolean_descriptor()) {
        DLOG(FATAL)
            << "In RoutingSearchParameters::LocalSearchNeighborhoodOperators,"
            << " field '" << field->name() << "' is not an OptionalBoolean.";
      } else {
        const int value = ls_reflection->GetEnum(operators, field)->number();
        if (!OptionalBoolean_IsValid(value) || value == 0) {
          errors.emplace_back(absl::StrFormat(
              "local_search_neighborhood_operator.%s should be set to "
              "BOOL_TRUE or BOOL_FALSE instead of %s (value: %d)",
              field->name(),
              OptionalBoolean_Name(static_cast<OptionalBoolean>(value)),
              value));
        }
      }
    }
  }
#endif  // !__ANDROID__ && !__wasm__
  if (const double ratio =
          search_parameters.savings_parameters().neighbors_ratio();
      std::isnan(ratio) || ratio <= 0 || ratio > 1) {
    errors.emplace_back(
        StrCat("Invalid savings_parameters.neighbors_ratio: ", ratio));
  }
  if (const double max_memory =
          search_parameters.savings_parameters().max_memory_usage_bytes();
      std::isnan(max_memory) || max_memory <= 0 || max_memory > 1e10) {
    errors.emplace_back(StrCat(
        "Invalid savings_parameters.max_memory_usage_bytes: ", max_memory));
  }
  if (const double coefficient =
          search_parameters.savings_parameters().arc_coefficient();
      std::isnan(coefficient) || coefficient <= 0 || std::isinf(coefficient)) {
    errors.emplace_back(
        StrCat("Invalid savings_parameters.arc_coefficient: ", coefficient));
  }
  if (const double ratio =
          search_parameters.cheapest_insertion_farthest_seeds_ratio();
      std::isnan(ratio) || ratio < 0 || ratio > 1) {
    errors.emplace_back(
        StrCat("Invalid cheapest_insertion_farthest_seeds_ratio: ", ratio));
  }
  if (const double ratio =
          search_parameters.cheapest_insertion_first_solution_neighbors_ratio();
      std::isnan(ratio) || ratio <= 0 || ratio > 1) {
    errors.emplace_back(StrCat(
        "Invalid cheapest_insertion_first_solution_neighbors_ratio: ", ratio));
  }
  if (const int32_t min_neighbors =
          search_parameters.cheapest_insertion_first_solution_min_neighbors();
      min_neighbors < 1) {
    errors.emplace_back(
        StrCat("Invalid cheapest_insertion_first_solution_min_neighbors: ",
               min_neighbors, ". Must be greater or equal to 1."));
  }
  if (const double ratio =
          search_parameters.cheapest_insertion_ls_operator_neighbors_ratio();
      std::isnan(ratio) || ratio <= 0 || ratio > 1) {
    errors.emplace_back(StrCat(
        "Invalid cheapest_insertion_ls_operator_neighbors_ratio: ", ratio));
  }
  if (const int32_t min_neighbors =
          search_parameters.cheapest_insertion_ls_operator_min_neighbors();
      min_neighbors < 1) {
    errors.emplace_back(StrCat(
        "Invalid cheapest_insertion_ls_operator_min_neighbors: ", min_neighbors,
        ". Must be greater or equal to 1."));
  }

  FindErrorsInLocalCheapestInsertionParameters(
      "Local cheapest insertion (first solution heuristic)",
      search_parameters.local_cheapest_insertion_parameters(), errors);
  FindErrorsInLocalCheapestInsertionParameters(
      "Local cheapest cost insertion (first solution heuristic)",
      search_parameters.local_cheapest_cost_insertion_parameters(), errors);

  if (const double ratio = search_parameters.ls_operator_neighbors_ratio();
      std::isnan(ratio) || ratio <= 0 || ratio > 1) {
    errors.emplace_back(StrCat("Invalid ls_operator_neighbors_ratio: ", ratio));
  }
  if (const int32_t min_neighbors =
          search_parameters.ls_operator_min_neighbors();
      min_neighbors < 1) {
    errors.emplace_back(
        StrCat("Invalid ls_operator_min_neighbors: ", min_neighbors,
               ". Must be greater or equal to 1."));
  }
  if (const int32_t num_arcs =
          search_parameters.relocate_expensive_chain_num_arcs_to_consider();
      num_arcs < 2 || num_arcs > 1e6) {
    errors.emplace_back(StrCat(
        "Invalid relocate_expensive_chain_num_arcs_to_consider: ", num_arcs,
        ". Must be between 2 and 10^6 (included)."));
  }
  if (const int32_t num_arcs =
          search_parameters
              .heuristic_expensive_chain_lns_num_arcs_to_consider();
      num_arcs < 2 || num_arcs > 1e6) {
    errors.emplace_back(
        StrCat("Invalid heuristic_expensive_chain_lns_num_arcs_to_consider: ",
               num_arcs, ". Must be between 2 and 10^6 (included)."));
  }
  if (const int32_t num_nodes =
          search_parameters.heuristic_close_nodes_lns_num_nodes();
      num_nodes < 0 || num_nodes > 1e4) {
    errors.emplace_back(
        StrCat("Invalid heuristic_close_nodes_lns_num_nodes: ", num_nodes,
               ". Must be between 0 and 10000 (included)."));
  }
  if (const double gls_coefficient =
          search_parameters.guided_local_search_lambda_coefficient();
      std::isnan(gls_coefficient) || gls_coefficient < 0 ||
      std::isinf(gls_coefficient)) {
    errors.emplace_back(StrCat(
        "Invalid guided_local_search_lambda_coefficient: ", gls_coefficient));
  }
  if (const double step = search_parameters.optimization_step();
      std::isnan(step) || step < 0.0) {
    errors.emplace_back(StrCat("Invalid optimization_step: ", step));
  }
  if (const int32_t num = search_parameters.number_of_solutions_to_collect();
      num < 1) {
    errors.emplace_back(
        StrCat("Invalid number_of_solutions_to_collect: ", num));
  }
  if (const int64_t lim = search_parameters.solution_limit(); lim < 1)
    errors.emplace_back(StrCat("Invalid solution_limit: ", lim));
  if (!IsValidNonNegativeDuration(search_parameters.time_limit())) {
    errors.emplace_back(
        "Invalid time_limit: " +
        ProtobufShortDebugString(search_parameters.time_limit()));
  }
  if (!IsValidNonNegativeDuration(search_parameters.lns_time_limit())) {
    errors.emplace_back(
        "Invalid lns_time_limit: " +
        ProtobufShortDebugString(search_parameters.lns_time_limit()));
  }
  if (const double ratio = search_parameters.secondary_ls_time_limit_ratio();
      std::isnan(ratio) || ratio < 0 || ratio >= 1) {
    errors.emplace_back(
        StrCat("Invalid secondary_ls_time_limit_ratio: ", ratio));
  }
  if (!FirstSolutionStrategy::Value_IsValid(
          search_parameters.first_solution_strategy())) {
    errors.emplace_back(StrCat("Invalid first_solution_strategy: ",
                               search_parameters.first_solution_strategy()));
  }
  const LocalSearchMetaheuristic::Value local_search_metaheuristic =
      search_parameters.local_search_metaheuristic();
  if (local_search_metaheuristic != LocalSearchMetaheuristic::UNSET &&
      local_search_metaheuristic != LocalSearchMetaheuristic::AUTOMATIC &&
      !search_parameters.local_search_metaheuristics().empty()) {
    errors.emplace_back(
        StrCat("local_search_metaheuristics cannot be set if "
               "local_search_metaheuristic is different from "
               "UNSET or AUTOMATIC: ",
               local_search_metaheuristic));
  }
  if (!LocalSearchMetaheuristic::Value_IsValid(local_search_metaheuristic)) {
    errors.emplace_back(
        StrCat("Invalid metaheuristic: ", local_search_metaheuristic));
  }
  for (const int metaheuristic :
       search_parameters.local_search_metaheuristics()) {
    if (!LocalSearchMetaheuristic::Value_IsValid(metaheuristic) ||
        metaheuristic == LocalSearchMetaheuristic::UNSET) {
      errors.emplace_back(StrCat("Invalid metaheuristic: ", metaheuristic));
    }
  }
  if (!search_parameters.local_search_metaheuristics().empty() &&
      search_parameters.num_max_local_optima_before_metaheuristic_switch() <
          1) {
    errors.emplace_back(StrCat(
        "Invalid num_max_local_optima_before_metaheuristic_switch: ",
        search_parameters.num_max_local_optima_before_metaheuristic_switch()));
  }

  const double scaling_factor = search_parameters.log_cost_scaling_factor();
  if (scaling_factor == 0 || std::isnan(scaling_factor) ||
      std::isinf(scaling_factor)) {
    errors.emplace_back(
        StrCat("Invalid value for log_cost_scaling_factor: ", scaling_factor));
  }
  const double offset = search_parameters.log_cost_offset();
  if (std::isnan(offset) || std::isinf(offset)) {
    errors.emplace_back(StrCat("Invalid value for log_cost_offset: ", offset));
  }
  const RoutingSearchParameters::SchedulingSolver continuous_scheduling_solver =
      search_parameters.continuous_scheduling_solver();
  if (continuous_scheduling_solver ==
          RoutingSearchParameters::SCHEDULING_UNSET ||
      continuous_scheduling_solver ==
          RoutingSearchParameters::SCHEDULING_CP_SAT) {
    errors.emplace_back(
        StrCat("Invalid value for continuous_scheduling_solver: ",
               RoutingSearchParameters::SchedulingSolver_Name(
                   continuous_scheduling_solver)));
  }

  if (const RoutingSearchParameters::SchedulingSolver
          mixed_integer_scheduling_solver =
              search_parameters.mixed_integer_scheduling_solver();
      mixed_integer_scheduling_solver ==
      RoutingSearchParameters::SCHEDULING_UNSET) {
    errors.emplace_back(
        StrCat("Invalid value for mixed_integer_scheduling_solver: ",
               RoutingSearchParameters::SchedulingSolver_Name(
                   mixed_integer_scheduling_solver)));
  }

  if (search_parameters.has_improvement_limit_parameters()) {
    const double improvement_rate_coefficient =
        search_parameters.improvement_limit_parameters()
            .improvement_rate_coefficient();
    if (std::isnan(improvement_rate_coefficient) ||
        improvement_rate_coefficient <= 0) {
      errors.emplace_back(
          StrCat("Invalid value for "
                 "improvement_limit_parameters.improvement_rate_coefficient: ",
                 improvement_rate_coefficient));
    }

    const int32_t improvement_rate_solutions_distance =
        search_parameters.improvement_limit_parameters()
            .improvement_rate_solutions_distance();
    if (improvement_rate_solutions_distance <= 0) {
      errors.emplace_back(StrCat(
          "Invalid value for "
          "improvement_limit_parameters.improvement_rate_solutions_distance: ",
          improvement_rate_solutions_distance));
    }
  }

  if (const double memory_coefficient =
          search_parameters
              .multi_armed_bandit_compound_operator_memory_coefficient();
      std::isnan(memory_coefficient) || memory_coefficient < 0 ||
      memory_coefficient > 1) {
    errors.emplace_back(
        StrCat("Invalid value for "
               "multi_armed_bandit_compound_operator_memory_coefficient: ",
               memory_coefficient));
  }
  if (const double exploration_coefficient =
          search_parameters
              .multi_armed_bandit_compound_operator_exploration_coefficient();
      std::isnan(exploration_coefficient) || exploration_coefficient < 0) {
    errors.emplace_back(
        StrCat("Invalid value for "
               "multi_armed_bandit_compound_operator_exploration_coefficient: ",
               exploration_coefficient));
  }

  if (const sat::SatParameters& sat_parameters =
          search_parameters.sat_parameters();
      sat_parameters.enumerate_all_solutions() &&
      (sat_parameters.num_workers() > 1 ||
       sat_parameters.interleave_search())) {
    errors.emplace_back(
        "sat_parameters.enumerate_all_solutions cannot be true in parallel"
        " search");
  }

  if (search_parameters.max_swap_active_chain_size() < 1 &&
      search_parameters.local_search_operators().use_swap_active_chain() ==
          OptionalBoolean::BOOL_TRUE) {
    errors.emplace_back(
        "max_swap_active_chain_size must be greater than 1 if "
        "local_search_operators.use_swap_active_chain is BOOL_TRUE");
  }

  FindErrorsInIteratedLocalSearchParameters(search_parameters, errors);

  return errors;
}

}  // namespace operations_research
