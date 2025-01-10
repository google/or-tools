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

#include "ortools/pdlp/solvers_proto_validation.h"

#include <cmath>
#include <limits>
#include <string>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "ortools/base/status_macros.h"
#include "ortools/pdlp/solvers.pb.h"

namespace operations_research::pdlp {

using ::absl::InvalidArgumentError;
using ::absl::OkStatus;

const double kTinyDouble = 1.0e-50;
const double kHugeDouble = 1.0e50;

absl::Status CheckNonNegative(const double value,
                              const absl::string_view name) {
  if (std::isnan(value)) {
    return InvalidArgumentError(absl::StrCat(name, " is NAN"));
  }
  if (value < 0) {
    return InvalidArgumentError(absl::StrCat(name, " must be non-negative"));
  }
  return absl::OkStatus();
}

absl::Status ValidateTerminationCriteria(const TerminationCriteria& criteria) {
  if (criteria.optimality_norm() != OPTIMALITY_NORM_L_INF &&
      criteria.optimality_norm() != OPTIMALITY_NORM_L2 &&
      criteria.optimality_norm() != OPTIMALITY_NORM_L_INF_COMPONENTWISE) {
    return InvalidArgumentError("invalid value for optimality_norm");
  }
  if (criteria.has_detailed_optimality_criteria() ||
      criteria.has_simple_optimality_criteria()) {
    if (criteria.has_eps_optimal_absolute()) {
      return InvalidArgumentError(
          "eps_optimal_absolute should not be set if "
          "detailed_optimality_criteria or simple_optimality_criteria is used");
    }
    if (criteria.has_eps_optimal_relative()) {
      return InvalidArgumentError(
          "eps_optimal_relative should not be set if "
          "detailed_optimality_criteria or simple_optimality_criteria is used");
    }
  }
  if (criteria.has_detailed_optimality_criteria()) {
    RETURN_IF_ERROR(CheckNonNegative(
        criteria.detailed_optimality_criteria()
            .eps_optimal_primal_residual_absolute(),
        "detailed_optimality_criteria.eps_optimal_primal_residual_absolute"));
    RETURN_IF_ERROR(CheckNonNegative(
        criteria.detailed_optimality_criteria()
            .eps_optimal_primal_residual_relative(),
        "detailed_optimality_criteria.eps_optimal_primal_residual_relative"));
    RETURN_IF_ERROR(CheckNonNegative(
        criteria.detailed_optimality_criteria()
            .eps_optimal_dual_residual_absolute(),
        "detailed_optimality_criteria.eps_optimal_dual_residual_absolute"));
    RETURN_IF_ERROR(CheckNonNegative(
        criteria.detailed_optimality_criteria()
            .eps_optimal_dual_residual_relative(),
        "detailed_optimality_criteria.eps_optimal_dual_residual_relative"));
    RETURN_IF_ERROR(CheckNonNegative(
        criteria.detailed_optimality_criteria()
            .eps_optimal_objective_gap_absolute(),
        "detailed_optimality_criteria.eps_optimal_objective_gap_absolute"));
    RETURN_IF_ERROR(CheckNonNegative(
        criteria.detailed_optimality_criteria()
            .eps_optimal_objective_gap_relative(),
        "detailed_optimality_criteria.eps_optimal_objective_gap_relative"));
  } else if (criteria.has_simple_optimality_criteria()) {
    RETURN_IF_ERROR(CheckNonNegative(
        criteria.simple_optimality_criteria().eps_optimal_absolute(),
        "simple_optimality_criteria.eps_optimal_absolute"));
    RETURN_IF_ERROR(CheckNonNegative(
        criteria.simple_optimality_criteria().eps_optimal_relative(),
        "simple_optimality_criteria.eps_optimal_relative"));
  } else {
    RETURN_IF_ERROR(CheckNonNegative(criteria.eps_optimal_absolute(),
                                     "eps_optimal_absolute"));
    RETURN_IF_ERROR(CheckNonNegative(criteria.eps_optimal_relative(),
                                     "eps_optimal_relative"));
  }
  RETURN_IF_ERROR(CheckNonNegative(criteria.eps_primal_infeasible(),
                                   "eps_primal_infeasible"));
  RETURN_IF_ERROR(
      CheckNonNegative(criteria.eps_dual_infeasible(), "eps_dual_infeasible"));
  RETURN_IF_ERROR(
      CheckNonNegative(criteria.eps_dual_infeasible(), "eps_dual_infeasible"));
  RETURN_IF_ERROR(
      CheckNonNegative(criteria.time_sec_limit(), "time_sec_limit"));

  if (criteria.iteration_limit() < 0) {
    return InvalidArgumentError("iteration_limit must be non-negative");
  }
  RETURN_IF_ERROR(CheckNonNegative(criteria.kkt_matrix_pass_limit(),
                                   "kkt_matrix_pass_limit"));

  return OkStatus();
}

absl::Status ValidateAdaptiveLinesearchParams(
    const AdaptiveLinesearchParams& params) {
  if (std::isnan(params.step_size_reduction_exponent())) {
    return InvalidArgumentError("step_size_reduction_exponent is NAN");
  }
  if (params.step_size_reduction_exponent() < 0.1 ||
      params.step_size_reduction_exponent() > 1.0) {
    return InvalidArgumentError(
        "step_size_reduction_exponent must be between 0.1 and 1.0 inclusive");
  }
  if (std::isnan(params.step_size_growth_exponent())) {
    return InvalidArgumentError("step_size_growth_exponent is NAN");
  }
  if (params.step_size_growth_exponent() < 0.1 ||
      params.step_size_growth_exponent() > 1.0) {
    return InvalidArgumentError(
        "step_size_growth_exponent must be between 0.1 and 1.0 inclusive");
  }
  return OkStatus();
}

absl::Status ValidateMalitskyPockParams(const MalitskyPockParams& params) {
  if (std::isnan(params.step_size_downscaling_factor())) {
    return InvalidArgumentError("step_size_downscaling_factor is NAN");
  }
  if (params.step_size_downscaling_factor() <= kTinyDouble ||
      params.step_size_downscaling_factor() >= 1) {
    return InvalidArgumentError(
        absl::StrCat("step_size_downscaling_factor must be between ",
                     kTinyDouble, " and 1 exclusive"));
  }
  if (std::isnan(params.linesearch_contraction_factor())) {
    return InvalidArgumentError("linesearch_contraction_factor is NAN");
  }
  if (params.linesearch_contraction_factor() <= 0 ||
      params.linesearch_contraction_factor() >= 1) {
    return InvalidArgumentError(
        "linesearch_contraction_factor must be between 0 and 1 exclusive");
  }
  if (std::isnan(params.step_size_interpolation())) {
    return InvalidArgumentError("step_size_interpolation is NAN");
  }
  if (params.step_size_interpolation() < 0 ||
      params.step_size_interpolation() >= kHugeDouble) {
    return InvalidArgumentError(absl::StrCat(
        "step_size_interpolation must be non-negative and less than ",
        kHugeDouble));
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
  if (params.log_interval_seconds() < 0.0) {
    return InvalidArgumentError("log_interval_seconds must be non-negative");
  }
  if (std::isnan(params.log_interval_seconds())) {
    return InvalidArgumentError("log_interval_seconds is NAN");
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
  if (std::isnan(params.primal_weight_update_smoothing())) {
    return InvalidArgumentError("primal_weight_update_smoothing is NAN");
  }
  if (params.primal_weight_update_smoothing() < 0 ||
      params.primal_weight_update_smoothing() > 1) {
    return InvalidArgumentError(
        "primal_weight_update_smoothing must be between 0 and 1 inclusive");
  }
  if (std::isnan(params.initial_primal_weight())) {
    return InvalidArgumentError("initial_primal_weight is NAN");
  }
  if (params.has_initial_primal_weight() &&
      (params.initial_primal_weight() <= kTinyDouble ||
       params.initial_primal_weight() >= kHugeDouble)) {
    return InvalidArgumentError(
        absl::StrCat("initial_primal_weight must be between ", kTinyDouble,
                     " and ", kHugeDouble, " if specified"));
  }
  if (params.l_inf_ruiz_iterations() < 0) {
    return InvalidArgumentError("l_inf_ruiz_iterations must be non-negative");
  }
  if (params.l_inf_ruiz_iterations() > 100) {
    return InvalidArgumentError("l_inf_ruiz_iterations must be at most 100");
  }
  if (std::isnan(params.sufficient_reduction_for_restart())) {
    return InvalidArgumentError("sufficient_reduction_for_restart is NAN");
  }
  if (params.sufficient_reduction_for_restart() <= 0 ||
      params.sufficient_reduction_for_restart() >= 1) {
    return InvalidArgumentError(
        "sufficient_reduction_for_restart must be between 0 and 1 exclusive");
  }
  if (std::isnan(params.necessary_reduction_for_restart())) {
    return InvalidArgumentError("necessary_reduction_for_restart is NAN");
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
  if (std::isnan(params.initial_step_size_scaling())) {
    return InvalidArgumentError("initial_step_size_scaling is NAN");
  }
  if (params.initial_step_size_scaling() <= kTinyDouble ||
      params.initial_step_size_scaling() >= kHugeDouble) {
    return InvalidArgumentError(
        absl::StrCat("initial_step_size_scaling must be between ", kTinyDouble,
                     " and ", kHugeDouble));
  }

  if (std::isnan(params.infinite_constraint_bound_threshold())) {
    return InvalidArgumentError("infinite_constraint_bound_threshold is NAN");
  }
  if (params.infinite_constraint_bound_threshold() <= 0.0) {
    return InvalidArgumentError(
        "infinite_constraint_bound_threshold must be positive");
  }
  if (std::isnan(params.diagonal_qp_trust_region_solver_tolerance())) {
    return InvalidArgumentError(
        "diagonal_qp_trust_region_solver_tolerance is NAN");
  }
  if (params.diagonal_qp_trust_region_solver_tolerance() <
      10 * std::numeric_limits<double>::epsilon()) {
    return InvalidArgumentError(absl::StrCat(
        "diagonal_qp_trust_region_solver_tolerance must be at least ",
        10 * std::numeric_limits<double>::epsilon()));
  }
  if (params.use_feasibility_polishing() &&
      params.handle_some_primal_gradients_on_finite_bounds_as_residuals()) {
    return InvalidArgumentError(
        "use_feasibility_polishing requires "
        "!handle_some_primal_gradients_on_finite_bounds_as_residuals");
  }
  if (params.use_feasibility_polishing() &&
      params.presolve_options().use_glop()) {
    return InvalidArgumentError(
        "use_feasibility_polishing and glop presolve can not be used "
        "together.");
  }
  return OkStatus();
}

}  // namespace operations_research::pdlp
