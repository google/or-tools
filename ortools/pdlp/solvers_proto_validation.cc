// Copyright 2010-2021 Google LLC
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

#include "ortools/pdlp/solvers_proto_validation.h"

#include "absl/status/status.h"
#include "ortools/base/status_macros.h"
#include "ortools/pdlp/solvers.pb.h"

namespace operations_research::pdlp {

using ::absl::InvalidArgumentError;
using ::absl::OkStatus;

absl::Status ValidateTerminationCriteria(const TerminationCriteria& criteria) {
  if (criteria.optimality_norm() != OPTIMALITY_NORM_L_INF &&
      criteria.optimality_norm() != OPTIMALITY_NORM_L2) {
    return InvalidArgumentError("invalid value for optimality_norm");
  }
  if (criteria.eps_optimal_absolute() < 0) {
    return InvalidArgumentError("eps_optimal_absolute must be non-negative");
  }
  if (criteria.eps_optimal_relative() < 0) {
    return InvalidArgumentError("eps_optimal_relative must be non-negative");
  }
  if (criteria.eps_primal_infeasible() < 0) {
    return InvalidArgumentError("eps_primal_infeasible must be non-negative");
  }
  if (criteria.eps_dual_infeasible() < 0) {
    return InvalidArgumentError("eps_dual_infeasible must be non-negative");
  }
  if (criteria.time_sec_limit() < 0) {
    return InvalidArgumentError("time_sec_limit must be non-negative");
  }
  if (criteria.iteration_limit() < 0) {
    return InvalidArgumentError("iteration_limit must be non-negative");
  }
  if (criteria.kkt_matrix_pass_limit() < 0) {
    return InvalidArgumentError("kkt_matrix_pass_limit must be non-negative");
  }
  return OkStatus();
}

absl::Status ValidateAdaptiveLinesearchParams(
    const AdaptiveLinesearchParams& params) {
  if (params.step_size_reduction_exponent() <= 0) {
    return InvalidArgumentError(
        "step_size_reduction_exponent must be positive");
  }
  if (params.step_size_growth_exponent() <= 0) {
    return InvalidArgumentError("step_size_growth_exponent must be positive");
  }
  return OkStatus();
}

absl::Status ValidateMalitskyPockParams(const MalitskyPockParams& params) {
  if (params.step_size_downscaling_factor() <= 0 ||
      params.step_size_downscaling_factor() >= 1) {
    return InvalidArgumentError(
        "step_size_downscaling_factor must be between 0 and 1 exclusive");
  }
  if (params.linesearch_contraction_factor() <= 0 ||
      params.linesearch_contraction_factor() >= 1) {
    return InvalidArgumentError(
        "linesearch_contraction_factor must be between 0 and 1 exclusive");
  }
  if (params.step_size_interpolation() < 0) {
    return InvalidArgumentError("step_size_interpolation must be non-negative");
  }
  return OkStatus();
}

absl::Status ValidatePrimalDualHybridGradientParams(
    const PrimalDualHybridGradientParams& params) {
  RETURN_IF_ERROR(ValidateTerminationCriteria(params.termination_criteria()))
      << "termination_criteria invalid";
  if (params.num_threads() <= 0) {
    return InvalidArgumentError("num_threads must be positive");
  }
  if (params.verbosity_level() < 0) {
    return InvalidArgumentError("verbosity_level must be non-negative");
  }
  if (params.major_iteration_frequency() <= 0) {
    return InvalidArgumentError("major_iteration_frequency must be positive");
  }
  if (params.termination_check_frequency() <= 0) {
    return InvalidArgumentError("termination_check_frequency must be positive");
  }
  if (params.restart_strategy() !=
          PrimalDualHybridGradientParams::NO_RESTARTS &&
      params.restart_strategy() !=
          PrimalDualHybridGradientParams::EVERY_MAJOR_ITERATION &&
      params.restart_strategy() !=
          PrimalDualHybridGradientParams::ADAPTIVE_HEURISTIC &&
      params.restart_strategy() !=
          PrimalDualHybridGradientParams::ADAPTIVE_DISTANCE_BASED) {
    return InvalidArgumentError("invalid restart_strategy");
  }
  if (params.primal_weight_update_smoothing() < 0 ||
      params.primal_weight_update_smoothing() > 1) {
    return InvalidArgumentError(
        "primal_weight_update_smoothing must be between 0 and 1 inclusive");
  }
  if (params.has_initial_primal_weight() &&
      params.initial_primal_weight() <= 0) {
    return InvalidArgumentError(
        "initial_primal_weight must be positive if specified");
  }
  if (params.l_inf_ruiz_iterations() < 0) {
    return InvalidArgumentError("l_inf_ruiz_iterations must be non-negative");
  }
  if (params.sufficient_reduction_for_restart() <= 0 ||
      params.sufficient_reduction_for_restart() >= 1) {
    return InvalidArgumentError(
        "sufficient_reduction_for_restart must be between 0 and 1 exclusive");
  }
  if (params.necessary_reduction_for_restart() <
          params.sufficient_reduction_for_restart() ||
      params.necessary_reduction_for_restart() >= 1) {
    return InvalidArgumentError(
        "necessary_reduction_for_restart must be in the interval "
        "[sufficient_reduction_for_restart, 1)");
  }
  if (params.linesearch_rule() !=
          PrimalDualHybridGradientParams::ADAPTIVE_LINESEARCH_RULE &&
      params.linesearch_rule() !=
          PrimalDualHybridGradientParams::MALITSKY_POCK_LINESEARCH_RULE &&
      params.linesearch_rule() !=
          PrimalDualHybridGradientParams::CONSTANT_STEP_SIZE_RULE) {
    return InvalidArgumentError("invalid linesearch_rule");
  }
  RETURN_IF_ERROR(
      ValidateAdaptiveLinesearchParams(params.adaptive_linesearch_parameters()))
      << "adaptive_linesearch_parameters invalid";
  RETURN_IF_ERROR(ValidateMalitskyPockParams(params.malitsky_pock_parameters()))
      << "malitsky_pock_parameters invalid";
  if (params.initial_step_size_scaling() <= 0.0) {
    return InvalidArgumentError("initial_step_size_scaling must be positive");
  }

  if (params.infinite_constraint_bound_threshold() < 0.0) {
    return InvalidArgumentError(
        "infinite_constraint_bound_threshold must be non-negative");
  }
  if (params.diagonal_qp_trust_region_solver_tolerance() < 0.0) {
    return InvalidArgumentError(
        "diagonal_qp_trust_region_solver_tolerance must be non-negative");
  }
  return OkStatus();
}

}  // namespace operations_research::pdlp
