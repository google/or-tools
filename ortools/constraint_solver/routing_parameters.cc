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

#include "ortools/constraint_solver/routing_parameters.h"

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/time/time.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/message.h"
#include "google/protobuf/text_format.h"
#include "ortools/base/logging.h"
#include "ortools/base/protoutil.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/routing_enums.pb.h"
#include "ortools/constraint_solver/solver_parameters.pb.h"
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
RoutingSearchParameters CreateDefaultRoutingSearchParameters() {
  static constexpr char kSearchParameters[] = R"pb(
    first_solution_strategy: AUTOMATIC
    use_unfiltered_first_solution_strategy: false
    savings_neighbors_ratio: 1
    savings_max_memory_usage_bytes: 6e9
    savings_add_reverse_arcs: false
    savings_arc_coefficient: 1
    savings_parallel_routes: false
    cheapest_insertion_farthest_seeds_ratio: 0
    cheapest_insertion_first_solution_neighbors_ratio: 1
    cheapest_insertion_first_solution_min_neighbors: 1
    cheapest_insertion_ls_operator_neighbors_ratio: 1
    cheapest_insertion_ls_operator_min_neighbors: 1
    cheapest_insertion_first_solution_use_neighbors_ratio_for_initialization:
        false
    cheapest_insertion_add_unperformed_entries: false
    local_cheapest_insertion_evaluate_pickup_delivery_costs_independently: true
    local_search_operators {
      use_relocate: BOOL_TRUE
      use_relocate_pair: BOOL_TRUE
      use_light_relocate_pair: BOOL_TRUE
      use_relocate_subtrip: BOOL_TRUE
      use_relocate_neighbors: BOOL_FALSE
      use_exchange: BOOL_TRUE
      use_exchange_pair: BOOL_TRUE
      use_exchange_subtrip: BOOL_TRUE
      use_cross: BOOL_TRUE
      use_cross_exchange: BOOL_FALSE
      use_relocate_expensive_chain: BOOL_TRUE
      use_two_opt: BOOL_TRUE
      use_or_opt: BOOL_TRUE
      use_lin_kernighan: BOOL_TRUE
      use_tsp_opt: BOOL_FALSE
      use_make_active: BOOL_TRUE
      use_relocate_and_make_active: BOOL_FALSE  # costly if true by default
      use_make_inactive: BOOL_TRUE
      use_make_chain_inactive: BOOL_TRUE
      use_swap_active: BOOL_TRUE
      use_extended_swap_active: BOOL_FALSE
      use_node_pair_swap_active: BOOL_TRUE
      use_path_lns: BOOL_FALSE
      use_full_path_lns: BOOL_FALSE
      use_tsp_lns: BOOL_FALSE
      use_inactive_lns: BOOL_FALSE
      use_global_cheapest_insertion_path_lns: BOOL_TRUE
      use_local_cheapest_insertion_path_lns: BOOL_TRUE
      use_relocate_path_global_cheapest_insertion_insert_unperformed: BOOL_TRUE
      use_global_cheapest_insertion_expensive_chain_lns: BOOL_FALSE
      use_local_cheapest_insertion_expensive_chain_lns: BOOL_FALSE
      use_global_cheapest_insertion_close_nodes_lns: BOOL_FALSE
      use_local_cheapest_insertion_close_nodes_lns: BOOL_FALSE
    }
    use_multi_armed_bandit_concatenate_operators: false
    multi_armed_bandit_compound_operator_memory_coefficient: 0.04
    multi_armed_bandit_compound_operator_exploration_coefficient: 1e12
    relocate_expensive_chain_num_arcs_to_consider: 4
    heuristic_expensive_chain_lns_num_arcs_to_consider: 4
    heuristic_close_nodes_lns_num_nodes: 5
    local_search_metaheuristic: AUTOMATIC
    guided_local_search_lambda_coefficient: 0.1
    use_depth_first_search: false
    use_cp: BOOL_TRUE
    use_cp_sat: BOOL_FALSE
    use_generalized_cp_sat: BOOL_FALSE
    sat_parameters { linearization_level: 2 num_search_workers: 1 }
    continuous_scheduling_solver: SCHEDULING_GLOP
    mixed_integer_scheduling_solver: SCHEDULING_CP_SAT
    disable_scheduling_beware_this_may_degrade_performance: false
    optimization_step: 0.0
    number_of_solutions_to_collect: 1
    # No global time_limit by default.
    solution_limit: 0x7fffffffffffffff               # kint64max
    lns_time_limit: { seconds: 0 nanos: 100000000 }  # 0.1s
    use_full_propagation: false
    log_search: false
    log_cost_scaling_factor: 1.0
    log_cost_offset: 0.0
  )pb";
  RoutingSearchParameters parameters;
  using TextFormat = google::protobuf::TextFormat;
  if (!TextFormat::ParseFromString(kSearchParameters, &parameters)) {
    LOG(DFATAL) << "Unsupported default search parameters: "
                << kSearchParameters;
  }
  const std::string error = FindErrorInRoutingSearchParameters(parameters);
  LOG_IF(DFATAL, !error.empty())
      << "The default search parameters aren't valid: " << error;
  return parameters;
}
}  // namespace

// static
RoutingSearchParameters DefaultRoutingSearchParameters() {
  static const auto* default_parameters =
      new RoutingSearchParameters(CreateDefaultRoutingSearchParameters());
  return *default_parameters;
}

namespace {
bool IsValidNonNegativeDuration(const google::protobuf::Duration& d) {
  const auto status_or_duration = util_time::DecodeGoogleApiProto(d);
  return status_or_duration.ok() &&
         status_or_duration.value() >= absl::ZeroDuration();
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
  // BOOL_FALSE (and not BOOL_UNSPECIFIED).
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
  if (const double ratio = search_parameters.savings_neighbors_ratio();
      std::isnan(ratio) || ratio <= 0 || ratio > 1) {
    errors.emplace_back(StrCat("Invalid savings_neighbors_ratio: ", ratio));
  }
  if (const double max_memory =
          search_parameters.savings_max_memory_usage_bytes();
      std::isnan(max_memory) || max_memory <= 0 || max_memory > 1e10) {
    errors.emplace_back(
        StrCat("Invalid savings_max_memory_usage_bytes: ", max_memory));
  }
  if (const double coefficient = search_parameters.savings_arc_coefficient();
      std::isnan(coefficient) || coefficient <= 0 || std::isinf(coefficient)) {
    errors.emplace_back(
        StrCat("Invalid savings_arc_coefficient: ", coefficient));
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
    errors.emplace_back("Invalid time_limit: " +
                        search_parameters.time_limit().ShortDebugString());
  }
  if (!IsValidNonNegativeDuration(search_parameters.lns_time_limit())) {
    errors.emplace_back("Invalid lns_time_limit: " +
                        search_parameters.lns_time_limit().ShortDebugString());
  }
  if (!FirstSolutionStrategy::Value_IsValid(
          search_parameters.first_solution_strategy())) {
    errors.emplace_back(StrCat("Invalid first_solution_strategy: ",
                               search_parameters.first_solution_strategy()));
  }
  if (!LocalSearchMetaheuristic::Value_IsValid(
          search_parameters.local_search_metaheuristic())) {
    errors.emplace_back(StrCat("Invalid metaheuristic: ",
                               search_parameters.local_search_metaheuristic()));
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
      (sat_parameters.num_search_workers() > 1 ||
       sat_parameters.interleave_search())) {
    errors.emplace_back(
        "sat_parameters.enumerate_all_solutions cannot be true in parallel"
        " search");
  }

  return errors;
}

}  // namespace operations_research
