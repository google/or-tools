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

#include "ortools/constraint_solver/routing_flags.h"

#include <map>
#include <vector>

#include "absl/status/status.h"
#include "absl/time/time.h"
#include "ortools/base/map_util.h"
#include "ortools/base/protoutil.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/routing_enums.pb.h"
#include "ortools/constraint_solver/routing_parameters.h"
#include "ortools/util/optional_boolean.pb.h"

// --- Routing search flags ---

// Neighborhood activation/deactivation
DEFINE_bool(routing_no_lns, false,
            "Routing: forbids use of Large Neighborhood Search.");
DEFINE_bool(routing_no_fullpathlns, true,
            "Routing: forbids use of Full-path Large Neighborhood Search.");
DEFINE_bool(routing_no_relocate, false,
            "Routing: forbids use of Relocate neighborhood.");
DEFINE_bool(routing_no_relocate_neighbors, true,
            "Routing: forbids use of RelocateNeighbors neighborhood.");
DEFINE_bool(routing_no_relocate_subtrip, false,
            "Routing: forbids use of RelocateSubtrips neighborhood.");
DEFINE_bool(routing_no_exchange, false,
            "Routing: forbids use of Exchange neighborhood.");
DEFINE_bool(routing_no_exchange_subtrip, false,
            "Routing: forbids use of ExchangeSubtrips neighborhood.");
DEFINE_bool(routing_no_cross, false,
            "Routing: forbids use of Cross neighborhood.");
DEFINE_bool(routing_no_2opt, false,
            "Routing: forbids use of 2Opt neighborhood.");
DEFINE_bool(routing_no_oropt, false,
            "Routing: forbids use of OrOpt neighborhood.");
DEFINE_bool(routing_no_make_active, false,
            "Routing: forbids use of MakeActive/SwapActive/MakeInactive "
            "neighborhoods.");
DEFINE_bool(routing_no_lkh, false, "Routing: forbids use of LKH neighborhood.");
DEFINE_bool(routing_no_relocate_expensive_chain, false,
            "Routing: forbids use of RelocateExpensiveChain operator.");
DEFINE_bool(routing_no_tsp, true,
            "Routing: forbids use of TSPOpt neighborhood.");
DEFINE_bool(routing_no_tsplns, true,
            "Routing: forbids use of TSPLNS neighborhood.");
DEFINE_bool(routing_use_chain_make_inactive, false,
            "Routing: use chain version of MakeInactive neighborhood.");
DEFINE_bool(routing_use_extended_swap_active, false,
            "Routing: use extended version of SwapActive neighborhood.");

// Meta-heuristics
DEFINE_bool(routing_guided_local_search, false, "Routing: use GLS.");
DEFINE_double(routing_guided_local_search_lambda_coefficient, 0.1,
              "Lambda coefficient in GLS.");
DEFINE_bool(routing_simulated_annealing, false,
            "Routing: use simulated annealing.");
DEFINE_bool(routing_tabu_search, false, "Routing: use tabu search.");
DEFINE_bool(routing_generic_tabu_search, false,
            "Routing: use tabu search based on a list of values.");

// Search limits
DEFINE_int64(routing_solution_limit, kint64max,
             "Routing: number of solutions limit.");
DEFINE_int64(routing_time_limit, kint64max, "Routing: time limit in ms.");
DEFINE_int64(routing_lns_time_limit, 100,
             "Routing: time limit in ms for LNS sub-decisionbuilder.");

// Search control
DEFINE_string(routing_first_solution, "",
              "Routing first solution heuristic. See SetupParametersFromFlags "
              "in the code to get a full list.");
DEFINE_bool(routing_use_filtered_first_solutions, true,
            "Use filtered version of first solution heuristics if available.");
DEFINE_double(savings_neighbors_ratio, 1,
              "Ratio of neighbors to consider for each node when "
              "constructing the savings.");
DEFINE_bool(savings_add_reverse_arcs, false,
            "Add savings related to reverse arcs when finding the nearest "
            "neighbors of the nodes.");
DEFINE_double(savings_arc_coefficient, 1.0,
              "Coefficient of the cost of the arc for which the saving value "
              "is being computed.");
DEFINE_double(cheapest_insertion_farthest_seeds_ratio, 0,
              "Ratio of available vehicles in the model on which farthest "
              "nodes of the model are inserted as seeds.");
DEFINE_double(cheapest_insertion_first_solution_neighbors_ratio, 1.0,
              "Ratio of nodes considered as neighbors in the "
              "GlobalCheapestInsertion first solution heuristic.");
DEFINE_bool(routing_dfs, false, "Routing: use a complete depth-first search.");
DEFINE_double(routing_optimization_step, 0.0, "Optimization step.");
DEFINE_int32(routing_number_of_solutions_to_collect, 1,
             "Number of solutions to collect.");
DEFINE_int32(routing_relocate_expensive_chain_num_arcs_to_consider, 4,
             "Number of arcs to consider in the RelocateExpensiveChain "
             "neighborhood operator.");

// Propagation control
DEFINE_bool(routing_use_light_propagation, true,
            "Use constraints with light propagation in routing model.");

// Cache settings.
DEFINE_bool(routing_cache_callbacks, false, "Cache callback calls.");
DEFINE_int64(routing_max_cache_size, 1000,
             "Maximum cache size when callback caching is on.");

// Misc
DEFINE_bool(routing_trace, false, "Routing: trace search.");
DEFINE_bool(routing_profile, false, "Routing: profile search.");

// --- Routing model flags ---
DEFINE_bool(routing_use_homogeneous_costs, true,
            "Routing: use homogeneous cost model when possible.");
DEFINE_bool(routing_gzip_compress_trail, false,
            "Use gzip to compress the trail, zippy otherwise.");

namespace operations_research {

void SetFirstSolutionStrategyFromFlags(RoutingSearchParameters* parameters) {
  CHECK(parameters != nullptr);
  const std::map<std::string, FirstSolutionStrategy::Value>
      first_solution_string_to_parameters = {
          {"PathCheapestArc", FirstSolutionStrategy::PATH_CHEAPEST_ARC},
          {"PathMostConstrainedArc",
           FirstSolutionStrategy::PATH_MOST_CONSTRAINED_ARC},
          {"EvaluatorStrategy", FirstSolutionStrategy::EVALUATOR_STRATEGY},
          {"Savings", FirstSolutionStrategy::SAVINGS},
          {"Sweep", FirstSolutionStrategy::SWEEP},
          {"Christofides", FirstSolutionStrategy::CHRISTOFIDES},
          {"AllUnperformed", FirstSolutionStrategy::ALL_UNPERFORMED},
          {"BestInsertion", FirstSolutionStrategy::BEST_INSERTION},
          {"GlobalCheapestInsertion",
           FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION},
          {"SequentialGlobalCheapestInsertion",
           FirstSolutionStrategy::SEQUENTIAL_CHEAPEST_INSERTION},
          {"LocalCheapestInsertion",
           FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION},
          {"GlobalCheapestArc", FirstSolutionStrategy::GLOBAL_CHEAPEST_ARC},
          {"LocalCheapestArc", FirstSolutionStrategy::LOCAL_CHEAPEST_ARC},
          {"DefaultStrategy", FirstSolutionStrategy::FIRST_UNBOUND_MIN_VALUE},
          {"", FirstSolutionStrategy::FIRST_UNBOUND_MIN_VALUE}};
  FirstSolutionStrategy::Value strategy;
  if (gtl::FindCopy(first_solution_string_to_parameters,
                    FLAGS_routing_first_solution, &strategy)) {
    parameters->set_first_solution_strategy(strategy);
  }
  parameters->set_use_unfiltered_first_solution_strategy(
      !FLAGS_routing_use_filtered_first_solutions);
  parameters->set_savings_neighbors_ratio(FLAGS_savings_neighbors_ratio);
  parameters->set_savings_max_memory_usage_bytes(6e9);
  parameters->set_savings_add_reverse_arcs(FLAGS_savings_add_reverse_arcs);
  parameters->set_savings_arc_coefficient(FLAGS_savings_arc_coefficient);
  parameters->set_cheapest_insertion_farthest_seeds_ratio(
      FLAGS_cheapest_insertion_farthest_seeds_ratio);
  parameters->set_cheapest_insertion_first_solution_neighbors_ratio(
      FLAGS_cheapest_insertion_first_solution_neighbors_ratio);
}

void SetLocalSearchMetaheuristicFromFlags(RoutingSearchParameters* parameters) {
  CHECK(parameters != nullptr);
  if (FLAGS_routing_tabu_search) {
    parameters->set_local_search_metaheuristic(
        LocalSearchMetaheuristic::TABU_SEARCH);
  } else if (FLAGS_routing_generic_tabu_search) {
    parameters->set_local_search_metaheuristic(
        LocalSearchMetaheuristic::GENERIC_TABU_SEARCH);
  } else if (FLAGS_routing_simulated_annealing) {
    parameters->set_local_search_metaheuristic(
        LocalSearchMetaheuristic::SIMULATED_ANNEALING);
  } else if (FLAGS_routing_guided_local_search) {
    parameters->set_local_search_metaheuristic(
        LocalSearchMetaheuristic::GUIDED_LOCAL_SEARCH);
  }
  parameters->set_guided_local_search_lambda_coefficient(
      FLAGS_routing_guided_local_search_lambda_coefficient);
}

namespace {
OptionalBoolean ToOptionalBoolean(bool x) { return x ? BOOL_TRUE : BOOL_FALSE; }
}  // namespace

void AddLocalSearchNeighborhoodOperatorsFromFlags(
    RoutingSearchParameters* parameters) {
  CHECK(parameters != nullptr);
  parameters->set_cheapest_insertion_ls_operator_neighbors_ratio(1.0);
  RoutingSearchParameters::LocalSearchNeighborhoodOperators* const
      local_search_operators = parameters->mutable_local_search_operators();

  // TODO(user): Remove these overrides: they should be set by the caller, via
  // a baseline RoutingSearchParameters obtained from DefaultSearchParameters().
  local_search_operators->set_use_relocate_pair(BOOL_TRUE);
  local_search_operators->set_use_light_relocate_pair(BOOL_TRUE);
  local_search_operators->set_use_exchange_pair(BOOL_TRUE);
  local_search_operators->set_use_relocate_and_make_active(BOOL_FALSE);
  local_search_operators->set_use_node_pair_swap_active(BOOL_FALSE);
  local_search_operators->set_use_cross_exchange(BOOL_FALSE);
  local_search_operators->set_use_global_cheapest_insertion_path_lns(BOOL_TRUE);
  local_search_operators->set_use_local_cheapest_insertion_path_lns(BOOL_TRUE);
  local_search_operators->set_use_global_cheapest_insertion_expensive_chain_lns(
      BOOL_FALSE);
  local_search_operators->set_use_local_cheapest_insertion_expensive_chain_lns(
      BOOL_FALSE);
  local_search_operators->set_use_global_cheapest_insertion_close_nodes_lns(
      BOOL_FALSE);
  local_search_operators->set_use_local_cheapest_insertion_close_nodes_lns(
      BOOL_FALSE);

  local_search_operators->set_use_relocate(
      ToOptionalBoolean(!FLAGS_routing_no_relocate));
  local_search_operators->set_use_relocate_neighbors(
      ToOptionalBoolean(!FLAGS_routing_no_relocate_neighbors));
  local_search_operators->set_use_relocate_subtrip(
      ToOptionalBoolean(!FLAGS_routing_no_relocate_subtrip));
  local_search_operators->set_use_exchange_subtrip(
      ToOptionalBoolean(!FLAGS_routing_no_exchange_subtrip));
  local_search_operators->set_use_exchange(
      ToOptionalBoolean(!FLAGS_routing_no_exchange));
  local_search_operators->set_use_cross(
      ToOptionalBoolean(!FLAGS_routing_no_cross));
  local_search_operators->set_use_two_opt(
      ToOptionalBoolean(!FLAGS_routing_no_2opt));
  local_search_operators->set_use_or_opt(
      ToOptionalBoolean(!FLAGS_routing_no_oropt));
  local_search_operators->set_use_lin_kernighan(
      ToOptionalBoolean(!FLAGS_routing_no_lkh));
  local_search_operators->set_use_relocate_expensive_chain(
      ToOptionalBoolean(!FLAGS_routing_no_relocate_expensive_chain));
  local_search_operators->set_use_tsp_opt(
      ToOptionalBoolean(!FLAGS_routing_no_tsp));
  local_search_operators->set_use_make_active(
      ToOptionalBoolean(!FLAGS_routing_no_make_active));
  local_search_operators->set_use_make_inactive(ToOptionalBoolean(
      !FLAGS_routing_use_chain_make_inactive && !FLAGS_routing_no_make_active));
  local_search_operators->set_use_make_chain_inactive(ToOptionalBoolean(
      FLAGS_routing_use_chain_make_inactive && !FLAGS_routing_no_make_active));
  local_search_operators->set_use_swap_active(
      ToOptionalBoolean(!FLAGS_routing_use_extended_swap_active &&
                        !FLAGS_routing_no_make_active));
  local_search_operators->set_use_extended_swap_active(ToOptionalBoolean(
      FLAGS_routing_use_extended_swap_active && !FLAGS_routing_no_make_active));
  local_search_operators->set_use_path_lns(
      ToOptionalBoolean(!FLAGS_routing_no_lns));
  local_search_operators->set_use_inactive_lns(
      ToOptionalBoolean(!FLAGS_routing_no_lns));
  local_search_operators->set_use_full_path_lns(
      ToOptionalBoolean(!FLAGS_routing_no_fullpathlns));
  local_search_operators->set_use_tsp_lns(
      ToOptionalBoolean(!FLAGS_routing_no_tsplns));
}

void SetSearchLimitsFromFlags(RoutingSearchParameters* parameters) {
  CHECK(parameters != nullptr);
  parameters->set_use_depth_first_search(FLAGS_routing_dfs);
  parameters->set_use_cp(BOOL_TRUE);
  parameters->set_use_cp_sat(BOOL_FALSE);
  parameters->set_optimization_step(FLAGS_routing_optimization_step);
  parameters->set_number_of_solutions_to_collect(
      FLAGS_routing_number_of_solutions_to_collect);
  parameters->set_solution_limit(FLAGS_routing_solution_limit);
  if (FLAGS_routing_time_limit != kint64max) {
    CHECK_OK(util_time::EncodeGoogleApiProto(
        absl::Milliseconds(FLAGS_routing_time_limit),
        parameters->mutable_time_limit()));
  }
  if (FLAGS_routing_lns_time_limit != kint64max) {
    CHECK_OK(util_time::EncodeGoogleApiProto(
        absl::Milliseconds(FLAGS_routing_lns_time_limit),
        parameters->mutable_lns_time_limit()));
  }
}

void SetMiscellaneousParametersFromFlags(RoutingSearchParameters* parameters) {
  CHECK(parameters != nullptr);
  parameters->set_use_full_propagation(!FLAGS_routing_use_light_propagation);
  parameters->set_log_search(FLAGS_routing_trace);
  parameters->set_log_cost_scaling_factor(1.0);
  parameters->set_relocate_expensive_chain_num_arcs_to_consider(
      FLAGS_routing_relocate_expensive_chain_num_arcs_to_consider);
  parameters->set_heuristic_expensive_chain_lns_num_arcs_to_consider(4);
  parameters->set_heuristic_close_nodes_lns_num_nodes(5);
  parameters->set_continuous_scheduling_solver(RoutingSearchParameters::GLOP);
  parameters->set_mixed_integer_scheduling_solver(
      RoutingSearchParameters::CP_SAT);
}

RoutingSearchParameters BuildSearchParametersFromFlags() {
  RoutingSearchParameters parameters;
  SetFirstSolutionStrategyFromFlags(&parameters);
  SetLocalSearchMetaheuristicFromFlags(&parameters);
  AddLocalSearchNeighborhoodOperatorsFromFlags(&parameters);
  SetSearchLimitsFromFlags(&parameters);
  SetMiscellaneousParametersFromFlags(&parameters);
  const std::string error = FindErrorInRoutingSearchParameters(parameters);
  LOG_IF(DFATAL, !error.empty())
      << "Error in the routing search parameters built from flags: " << error;
  return parameters;
}

RoutingModelParameters BuildModelParametersFromFlags() {
  RoutingModelParameters parameters;
  ConstraintSolverParameters* const solver_parameters =
      parameters.mutable_solver_parameters();
  *solver_parameters = Solver::DefaultSolverParameters();
  parameters.set_reduce_vehicle_cost_model(FLAGS_routing_use_homogeneous_costs);
  if (FLAGS_routing_cache_callbacks) {
    parameters.set_max_callback_cache_size(FLAGS_routing_max_cache_size);
  }
  solver_parameters->set_profile_local_search(FLAGS_routing_profile);
  return parameters;
}

}  // namespace operations_research
