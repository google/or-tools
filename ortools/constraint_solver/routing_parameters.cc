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

#include "ortools/constraint_solver/routing_parameters.h"

#include "absl/strings/str_cat.h"
#include "absl/time/time.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/message.h"
#include "google/protobuf/text_format.h"
#include "ortools/base/logging.h"
#include "ortools/base/protoutil.h"
#include "ortools/base/statusor.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/routing_enums.pb.h"
#include "ortools/constraint_solver/solver_parameters.pb.h"
#include "ortools/util/optional_boolean.pb.h"

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

// static
RoutingSearchParameters DefaultRoutingSearchParameters() {
  static const char* const kSearchParameters =
      "first_solution_strategy: AUTOMATIC "
      "use_unfiltered_first_solution_strategy: false "
      "savings_neighbors_ratio: 1 "
      "savings_max_memory_usage_bytes: 6e9 "
      "savings_add_reverse_arcs: false "
      "savings_arc_coefficient: 1 "
      "savings_parallel_routes: false "
      "cheapest_insertion_farthest_seeds_ratio: 0 "
      "cheapest_insertion_first_solution_neighbors_ratio: 1 "
      "cheapest_insertion_ls_operator_neighbors_ratio: 1 "
      "local_search_operators {"
      "  use_relocate: BOOL_TRUE"
      "  use_relocate_pair: BOOL_TRUE"
      "  use_light_relocate_pair: BOOL_TRUE"
      "  use_relocate_subtrip: BOOL_TRUE"
      "  use_relocate_neighbors: BOOL_FALSE"
      "  use_exchange: BOOL_TRUE"
      "  use_exchange_pair: BOOL_TRUE"
      "  use_exchange_subtrip: BOOL_TRUE"
      "  use_cross: BOOL_TRUE"
      "  use_cross_exchange: BOOL_FALSE"
      "  use_relocate_expensive_chain: BOOL_TRUE"
      "  use_two_opt: BOOL_TRUE"
      "  use_or_opt: BOOL_TRUE"
      "  use_lin_kernighan: BOOL_TRUE"
      "  use_tsp_opt: BOOL_FALSE"
      "  use_make_active: BOOL_TRUE"
      "  use_relocate_and_make_active: BOOL_FALSE"  // costly if true by default
      "  use_make_inactive: BOOL_TRUE"
      "  use_make_chain_inactive: BOOL_FALSE"
      "  use_swap_active: BOOL_TRUE"
      "  use_extended_swap_active: BOOL_FALSE"
      "  use_node_pair_swap_active: BOOL_TRUE"
      "  use_path_lns: BOOL_FALSE"
      "  use_full_path_lns: BOOL_FALSE"
      "  use_tsp_lns: BOOL_FALSE"
      "  use_inactive_lns: BOOL_FALSE"
      "  use_global_cheapest_insertion_path_lns: BOOL_TRUE"
      "  use_local_cheapest_insertion_path_lns: BOOL_TRUE"
      "  use_global_cheapest_insertion_expensive_chain_lns: BOOL_FALSE"
      "  use_local_cheapest_insertion_expensive_chain_lns: BOOL_FALSE"
      "  use_global_cheapest_insertion_close_nodes_lns: BOOL_FALSE"
      "  use_local_cheapest_insertion_close_nodes_lns: BOOL_FALSE"
      "}"
      "relocate_expensive_chain_num_arcs_to_consider: 4 "
      "heuristic_expensive_chain_lns_num_arcs_to_consider: 4 "
      "heuristic_close_nodes_lns_num_nodes: 5 "
      "local_search_metaheuristic: AUTOMATIC "
      "guided_local_search_lambda_coefficient: 0.1 "
      "use_depth_first_search: false "
      "use_cp: BOOL_TRUE "
      "use_cp_sat: BOOL_FALSE "
      "continuous_scheduling_solver: GLOP "
      "mixed_integer_scheduling_solver: CP_SAT "
      "optimization_step: 0.0 "
      "number_of_solutions_to_collect: 1 "
      // No "time_limit" by default.
      "solution_limit: 0x7fffffffffffffff "             // kint64max
      "lns_time_limit: { seconds:0 nanos:100000000 } "  // 0.1s
      "use_full_propagation: false "
      "log_search: false "
      "log_cost_scaling_factor: 1.0 "
      "log_cost_offset: 0.0";
  RoutingSearchParameters parameters;
  if (!google::protobuf::TextFormat::ParseFromString(kSearchParameters,
                                                     &parameters)) {
    LOG(DFATAL) << "Unsupported default search parameters: "
                << kSearchParameters;
  }
  const std::string error = FindErrorInRoutingSearchParameters(parameters);
  LOG_IF(DFATAL, !error.empty())
      << "The default search parameters aren't valid: " << error;
  return parameters;
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
  using absl::StrCat;
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
        return "The file 'routing_search_parameters.proto' itself is invalid!";
      }
      const int value = ls_reflection->GetEnum(operators, field)->number();
      if (!OptionalBoolean_IsValid(value) || value == 0) {
        return StrCat("local_search_neighborhood_operator.", field->name(),
                      " should be set to BOOL_TRUE or BOOL_FALSE instead of ",
                      OptionalBoolean_Name(static_cast<OptionalBoolean>(value)),
                      " (value: ", value, ")");
      }
    }
  }
  {
    const double ratio = search_parameters.savings_neighbors_ratio();
    if (std::isnan(ratio) || ratio <= 0 || ratio > 1) {
      return StrCat("Invalid savings_neighbors_ratio:", ratio);
    }
  }
  {
    const double max_memory =
        search_parameters.savings_max_memory_usage_bytes();
    if (std::isnan(max_memory) || max_memory <= 0 || max_memory > 1e10) {
      return StrCat("Invalid savings_max_memory_usage_bytes: ", max_memory);
    }
  }
  {
    const double coefficient = search_parameters.savings_arc_coefficient();
    if (std::isnan(coefficient) || coefficient <= 0 ||
        std::isinf(coefficient)) {
      return StrCat("Invalid savings_arc_coefficient:", coefficient);
    }
  }
  {
    const double ratio =
        search_parameters.cheapest_insertion_farthest_seeds_ratio();
    if (std::isnan(ratio) || ratio < 0 || ratio > 1) {
      return StrCat("Invalid cheapest_insertion_farthest_seeds_ratio:", ratio);
    }
  }
  {
    const double ratio =
        search_parameters.cheapest_insertion_first_solution_neighbors_ratio();
    if (std::isnan(ratio) || ratio <= 0 || ratio > 1) {
      return StrCat(
          "Invalid cheapest_insertion_first_solution_neighbors_ratio: ", ratio);
    }
  }
  {
    const double ratio =
        search_parameters.cheapest_insertion_ls_operator_neighbors_ratio();
    if (std::isnan(ratio) || ratio <= 0 || ratio > 1) {
      return StrCat("Invalid cheapest_insertion_ls_operator_neighbors_ratio: ",
                    ratio);
    }
  }
  {
    const int32 num_arcs =
        search_parameters.relocate_expensive_chain_num_arcs_to_consider();
    if (num_arcs < 2 || num_arcs > 1e6) {
      return StrCat("Invalid relocate_expensive_chain_num_arcs_to_consider: ",
                    num_arcs, ". Must be between 2 and 10^6 (included).");
    }
  }
  {
    const int32 num_arcs =
        search_parameters.heuristic_expensive_chain_lns_num_arcs_to_consider();
    if (num_arcs < 2 || num_arcs > 1e6) {
      return StrCat(
          "Invalid heuristic_expensive_chain_lns_num_arcs_to_consider: ",
          num_arcs, ". Must be between 2 and 10^6 (included).");
    }
  }
  {
    const int32 num_nodes =
        search_parameters.heuristic_close_nodes_lns_num_nodes();
    if (num_nodes < 0 || num_nodes > 1e4) {
      return StrCat("Invalid heuristic_close_nodes_lns_num_nodes: ", num_nodes,
                    ". Must be between 0 and 10000 (included).");
    }
  }
  {
    const double gls_coefficient =
        search_parameters.guided_local_search_lambda_coefficient();
    if (std::isnan(gls_coefficient) || gls_coefficient < 0 ||
        std::isinf(gls_coefficient)) {
      return StrCat("Invalid guided_local_search_lambda_coefficient: ",
                    gls_coefficient);
    }
  }
  {
    const double step = search_parameters.optimization_step();
    if (std::isnan(step) || step < 0.0) {
      return StrCat("Invalid optimization_step: ", step);
    }
  }
  {
    const int32 num = search_parameters.number_of_solutions_to_collect();
    if (num < 1) return StrCat("Invalid number_of_solutions_to_collect:", num);
  }
  {
    const int64 lim = search_parameters.solution_limit();
    if (lim < 1) return StrCat("Invalid solution_limit:", lim);
  }
  if (!IsValidNonNegativeDuration(search_parameters.time_limit())) {
    return "Invalid time_limit: " +
           search_parameters.time_limit().ShortDebugString();
  }
  if (!IsValidNonNegativeDuration(search_parameters.lns_time_limit())) {
    return "Invalid lns_time_limit: " +
           search_parameters.lns_time_limit().ShortDebugString();
  }
  if (!FirstSolutionStrategy::Value_IsValid(
          search_parameters.first_solution_strategy())) {
    return StrCat("Invalid first_solution_strategy: ",
                  search_parameters.first_solution_strategy());
  }
  if (!LocalSearchMetaheuristic::Value_IsValid(
          search_parameters.local_search_metaheuristic())) {
    return StrCat("Invalid metaheuristic: ",
                  search_parameters.local_search_metaheuristic());
  }

  const double scaling_factor = search_parameters.log_cost_scaling_factor();
  if (scaling_factor == 0 || std::isnan(scaling_factor) ||
      std::isinf(scaling_factor)) {
    return StrCat("Invalid value for log_cost_scaling_factor: ",
                  scaling_factor);
  }
  const double offset = search_parameters.log_cost_offset();
  if (std::isnan(offset) || std::isinf(offset)) {
    return StrCat("Invalid value for log_cost_offset: ", offset);
  }
  const RoutingSearchParameters::SchedulingSolver continuous_scheduling_solver =
      search_parameters.continuous_scheduling_solver();
  if (continuous_scheduling_solver == RoutingSearchParameters::UNSET ||
      continuous_scheduling_solver == RoutingSearchParameters::CP_SAT) {
    return StrCat("Invalid value for continuous_scheduling_solver: ",
                  RoutingSearchParameters::SchedulingSolver_Name(
                      continuous_scheduling_solver));
  }
  const RoutingSearchParameters::SchedulingSolver
      mixed_integer_scheduling_solver =
          search_parameters.mixed_integer_scheduling_solver();
  if (mixed_integer_scheduling_solver == RoutingSearchParameters::UNSET) {
    return StrCat("Invalid value for mixed_integer_scheduling_solver: ",
                  RoutingSearchParameters::SchedulingSolver_Name(
                      mixed_integer_scheduling_solver));
  }

  return "";  // = Valid (No error).
}

}  // namespace operations_research
