// Copyright 2010-2017 Google
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

#include "ortools/base/map_util.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/routing_enums.pb.h"

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
DEFINE_bool(routing_no_exchange, false,
            "Routing: forbids use of Exchange neighborhood.");
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
DEFINE_bool(routing_objective_tabu_search, false,
            "Routing: use tabu search based on objective value.");

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
DEFINE_double(savings_neighbors_ratio, 0,
              "Ratio of neighbors to consider for each node when "
              "constructing the savings.");
DEFINE_bool(savings_add_reverse_arcs, false,
            "Add savings related to reverse arcs when finding the nearest "
            "neighbors of the nodes.");
DEFINE_bool(routing_dfs, false, "Routing: use a complete depth-first search.");
DEFINE_int64(routing_optimization_step, 1, "Optimization step.");

// Propagation control
DEFINE_bool(routing_use_light_propagation, true,
            "Use constraints with light propagation in routing model.");

// Cache settings.
DEFINE_bool(routing_cache_callbacks, false, "Cache callback calls.");
DEFINE_int64(routing_max_cache_size, 1000,
             "Maximum cache size when callback caching is on.");

// Misc
DEFINE_bool(routing_fingerprint_arc_cost_evaluators, true,
            "Compare arc-cost evaluators using the fingerprint of their "
            "corresponding matrix instead of evaluator addresses.");
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
          {"LocalCheapestInsertion",
           FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION},
          {"GlobalCheapestArc", FirstSolutionStrategy::GLOBAL_CHEAPEST_ARC},
          {"LocalCheapestArc", FirstSolutionStrategy::LOCAL_CHEAPEST_ARC},
          {"DefaultStrategy", FirstSolutionStrategy::FIRST_UNBOUND_MIN_VALUE},
          {"", FirstSolutionStrategy::FIRST_UNBOUND_MIN_VALUE}};
  FirstSolutionStrategy::Value strategy;
  if (FindCopy(first_solution_string_to_parameters,
               FLAGS_routing_first_solution, &strategy)) {
    parameters->set_first_solution_strategy(strategy);
  }
  parameters->set_use_filtered_first_solution_strategy(
      FLAGS_routing_use_filtered_first_solutions);
  parameters->set_savings_neighbors_ratio(FLAGS_savings_neighbors_ratio);
  parameters->set_savings_add_reverse_arcs(FLAGS_savings_add_reverse_arcs);
}

void SetLocalSearchMetaheuristicFromFlags(RoutingSearchParameters* parameters) {
  CHECK(parameters != nullptr);
  if (FLAGS_routing_tabu_search) {
    parameters->set_local_search_metaheuristic(
        LocalSearchMetaheuristic::TABU_SEARCH);
  } else if (FLAGS_routing_objective_tabu_search) {
    parameters->set_local_search_metaheuristic(
        LocalSearchMetaheuristic::OBJECTIVE_TABU_SEARCH);
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

void AddLocalSearchNeighborhoodOperatorsFromFlags(
    RoutingSearchParameters* parameters) {
  CHECK(parameters != nullptr);
  RoutingSearchParameters::LocalSearchNeighborhoodOperators* const
      local_search_operators = parameters->mutable_local_search_operators();
  local_search_operators->set_use_relocate_pair(true);
  local_search_operators->set_use_relocate(!FLAGS_routing_no_relocate);
  local_search_operators->set_use_relocate_neighbors(
      !FLAGS_routing_no_relocate_neighbors);
  local_search_operators->set_use_exchange(!FLAGS_routing_no_exchange);
  local_search_operators->set_use_cross(!FLAGS_routing_no_cross);
  local_search_operators->set_use_two_opt(!FLAGS_routing_no_2opt);
  local_search_operators->set_use_or_opt(!FLAGS_routing_no_oropt);
  local_search_operators->set_use_lin_kernighan(!FLAGS_routing_no_lkh);
  local_search_operators->set_use_tsp_opt(!FLAGS_routing_no_tsp);
  local_search_operators->set_use_make_active(!FLAGS_routing_no_make_active);
  local_search_operators->set_use_make_inactive(
      !FLAGS_routing_use_chain_make_inactive && !FLAGS_routing_no_make_active);
  local_search_operators->set_use_make_chain_inactive(
      FLAGS_routing_use_chain_make_inactive && !FLAGS_routing_no_make_active);
  local_search_operators->set_use_swap_active(
      !FLAGS_routing_use_extended_swap_active && !FLAGS_routing_no_make_active);
  local_search_operators->set_use_extended_swap_active(
      FLAGS_routing_use_extended_swap_active && !FLAGS_routing_no_make_active);
  local_search_operators->set_use_path_lns(!FLAGS_routing_no_lns);
  local_search_operators->set_use_inactive_lns(!FLAGS_routing_no_lns);
  local_search_operators->set_use_full_path_lns(!FLAGS_routing_no_fullpathlns);
  local_search_operators->set_use_tsp_lns(!FLAGS_routing_no_tsplns);
}

void SetSearchLimitsFromFlags(RoutingSearchParameters* parameters) {
  CHECK(parameters != nullptr);
  parameters->set_use_depth_first_search(FLAGS_routing_dfs);
  parameters->set_optimization_step(FLAGS_routing_optimization_step);
  parameters->set_solution_limit(FLAGS_routing_solution_limit);
  parameters->set_time_limit_ms(FLAGS_routing_time_limit);
  parameters->set_lns_time_limit_ms(FLAGS_routing_lns_time_limit);
}

void SetMiscellaneousParametersFromFlags(RoutingSearchParameters* parameters) {
  CHECK(parameters != nullptr);
  parameters->set_use_light_propagation(FLAGS_routing_use_light_propagation);
  parameters->set_fingerprint_arc_cost_evaluators(
      FLAGS_routing_fingerprint_arc_cost_evaluators);
  parameters->set_log_search(FLAGS_routing_trace);
}

RoutingSearchParameters BuildSearchParametersFromFlags() {
  RoutingSearchParameters parameters;
  SetFirstSolutionStrategyFromFlags(&parameters);
  SetLocalSearchMetaheuristicFromFlags(&parameters);
  AddLocalSearchNeighborhoodOperatorsFromFlags(&parameters);
  SetSearchLimitsFromFlags(&parameters);
  SetMiscellaneousParametersFromFlags(&parameters);
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
