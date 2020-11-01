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

#ifndef OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_FLAGS_H_
#define OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_FLAGS_H_

#include <vector>

#include "ortools/base/commandlineflags.h"
#include "ortools/base/integral_types.h"
#include "ortools/constraint_solver/routing_parameters.pb.h"

/// Neighborhood activation/deactivation
ABSL_DECLARE_FLAG(bool, routing_no_lns);
ABSL_DECLARE_FLAG(bool, routing_no_fullpathlns);
ABSL_DECLARE_FLAG(bool, routing_no_relocate);
ABSL_DECLARE_FLAG(bool, routing_no_relocate_neighbors);
ABSL_DECLARE_FLAG(bool, routing_no_relocate_subtrip);
ABSL_DECLARE_FLAG(bool, routing_no_exchange);
ABSL_DECLARE_FLAG(bool, routing_no_exchange_subtrip);
ABSL_DECLARE_FLAG(bool, routing_no_cross);
ABSL_DECLARE_FLAG(bool, routing_no_2opt);
ABSL_DECLARE_FLAG(bool, routing_no_oropt);
ABSL_DECLARE_FLAG(bool, routing_no_make_active);
ABSL_DECLARE_FLAG(bool, routing_no_lkh);
ABSL_DECLARE_FLAG(bool, routing_no_relocate_expensive_chain);
ABSL_DECLARE_FLAG(bool, routing_no_tsp);
ABSL_DECLARE_FLAG(bool, routing_no_tsplns);
ABSL_DECLARE_FLAG(bool, routing_use_chain_make_inactive);
ABSL_DECLARE_FLAG(bool, routing_use_extended_swap_active);

/// Meta-heuristics
ABSL_DECLARE_FLAG(bool, routing_guided_local_search);
ABSL_DECLARE_FLAG(double, routing_guided_local_search_lambda_coefficient);
ABSL_DECLARE_FLAG(bool, routing_simulated_annealing);
ABSL_DECLARE_FLAG(bool, routing_tabu_search);
ABSL_DECLARE_FLAG(bool, routing_generic_tabu_search);

/// Search limits
ABSL_DECLARE_FLAG(int64, routing_solution_limit);
ABSL_DECLARE_FLAG(int64, routing_time_limit);
ABSL_DECLARE_FLAG(int64, routing_lns_time_limit);

/// Search control
ABSL_DECLARE_FLAG(std::string, routing_first_solution);
ABSL_DECLARE_FLAG(bool, routing_use_filtered_first_solutions);
ABSL_DECLARE_FLAG(double, savings_neighbors_ratio);
ABSL_DECLARE_FLAG(bool, savings_add_reverse_arcs);
ABSL_DECLARE_FLAG(double, savings_arc_coefficient);
ABSL_DECLARE_FLAG(double, cheapest_insertion_farthest_seeds_ratio);
ABSL_DECLARE_FLAG(double, cheapest_insertion_first_solution_neighbors_ratio);
ABSL_DECLARE_FLAG(bool, routing_dfs);
ABSL_DECLARE_FLAG(double, routing_optimization_step);
ABSL_DECLARE_FLAG(int, routing_number_of_solutions_to_collect);
ABSL_DECLARE_FLAG(int, routing_relocate_expensive_chain_num_arcs_to_consider);

/// Propagation control
ABSL_DECLARE_FLAG(bool, routing_use_light_propagation);

/// Cache settings.
ABSL_DECLARE_FLAG(bool, routing_cache_callbacks);
ABSL_DECLARE_FLAG(int64, routing_max_cache_size);

/// Misc
ABSL_DECLARE_FLAG(bool, routing_trace);
ABSL_DECLARE_FLAG(bool, routing_profile);

/// --- Routing model flags ---
ABSL_DECLARE_FLAG(bool, routing_use_homogeneous_costs);
ABSL_DECLARE_FLAG(bool, routing_gzip_compress_trail);

namespace operations_research {

/// Builds routing search parameters from flags.
RoutingModelParameters BuildModelParametersFromFlags();

/// Builds routing search parameters from flags.
// TODO(user): Make this return a StatusOr, verifying that the flags
/// describe a valid set of routing search parameters.
RoutingSearchParameters BuildSearchParametersFromFlags();

}  // namespace operations_research

#endif  // OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_FLAGS_H_
