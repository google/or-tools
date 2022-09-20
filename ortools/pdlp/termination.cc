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

#include "ortools/pdlp/termination.h"

#include <atomic>
#include <cmath>
#include <limits>
#include <optional>

#include "ortools/base/logging.h"
#include "ortools/pdlp/solve_log.pb.h"
#include "ortools/pdlp/solvers.pb.h"

namespace operations_research::pdlp {

namespace {

bool OptimalityCriteriaMet(
    const OptimalityNorm optimality_norm,
    const TerminationCriteria::DetailedOptimalityCriteria& optimality_criteria,
    const ConvergenceInformation& stats,
    const QuadraticProgramBoundNorms& bound_norms) {
  const double abs_obj =
      std::abs(stats.primal_objective()) + std::abs(stats.dual_objective());
  const double gap =
      std::abs(stats.primal_objective() - stats.dual_objective());

  double primal_err;
  double primal_err_baseline;
  double dual_err;
  double dual_err_baseline;

  switch (optimality_norm) {
    case OPTIMALITY_NORM_L_INF:
      primal_err = stats.l_inf_primal_residual();
      primal_err_baseline = bound_norms.l_inf_norm_constraint_bounds;
      dual_err = stats.l_inf_dual_residual();
      dual_err_baseline = bound_norms.l_inf_norm_primal_linear_objective;
      break;
    case OPTIMALITY_NORM_L2:
      primal_err = stats.l2_primal_residual();
      primal_err_baseline = bound_norms.l2_norm_constraint_bounds;
      dual_err = stats.l2_dual_residual();
      dual_err_baseline = bound_norms.l2_norm_primal_linear_objective;
      break;
    default:
      LOG(FATAL) << "Invalid optimality_norm value "
                 << OptimalityNorm_Name(optimality_norm);
  }

  return dual_err <=
             optimality_criteria.eps_optimal_dual_residual_absolute() +
                 optimality_criteria.eps_optimal_dual_residual_relative() *
                     dual_err_baseline &&
         primal_err <=
             optimality_criteria.eps_optimal_primal_residual_absolute() +
                 optimality_criteria.eps_optimal_primal_residual_relative() *
                     primal_err_baseline &&
         std::isfinite(abs_obj) &&
         gap <= optimality_criteria.eps_optimal_objective_gap_absolute() +
                    optimality_criteria.eps_optimal_objective_gap_relative() *
                        abs_obj;
}

// Checks if the criteria for primal infeasibility are approximately
// satisfied; see https://developers.google.com/optimization/lp/pdlp_math for
// more details.
bool PrimalInfeasibilityCriteriaMet(double eps_primal_infeasible,
                                    const InfeasibilityInformation& stats) {
  if (stats.dual_ray_objective() <= 0.0) return false;
  return stats.max_dual_ray_infeasibility() / stats.dual_ray_objective() <=
         eps_primal_infeasible;
}

// Checks if the criteria for dual infeasibility are approximately satisfied;
// see https://developers.google.com/optimization/lp/pdlp_math for more details.
bool DualInfeasibilityCriteriaMet(double eps_dual_infeasible,
                                  const InfeasibilityInformation& stats) {
  if (stats.primal_ray_linear_objective() >= 0.0) return false;
  return (stats.max_primal_ray_infeasibility() /
              -stats.primal_ray_linear_objective() <=
          eps_dual_infeasible) &&
         (stats.primal_ray_quadratic_norm() /
              -stats.primal_ray_linear_objective() <=
          eps_dual_infeasible);
}

}  // namespace
TerminationCriteria::DetailedOptimalityCriteria EffectiveOptimalityCriteria(
    const TerminationCriteria& termination_criteria) {
  if (termination_criteria.has_detailed_optimality_criteria()) {
    return termination_criteria.detailed_optimality_criteria();
  }
  TerminationCriteria::SimpleOptimalityCriteria simple_criteria;
  if (termination_criteria.has_simple_optimality_criteria()) {
    simple_criteria = termination_criteria.simple_optimality_criteria();
  } else {
    simple_criteria.set_eps_optimal_absolute(
        termination_criteria.eps_optimal_absolute());
    simple_criteria.set_eps_optimal_relative(
        termination_criteria.eps_optimal_relative());
  }
  return EffectiveOptimalityCriteria(simple_criteria);
}

TerminationCriteria::DetailedOptimalityCriteria EffectiveOptimalityCriteria(
    const TerminationCriteria::SimpleOptimalityCriteria& simple_criteria) {
  TerminationCriteria::DetailedOptimalityCriteria result;
  result.set_eps_optimal_primal_residual_absolute(
      simple_criteria.eps_optimal_absolute());
  result.set_eps_optimal_primal_residual_relative(
      simple_criteria.eps_optimal_relative());
  result.set_eps_optimal_dual_residual_absolute(
      simple_criteria.eps_optimal_absolute());
  result.set_eps_optimal_dual_residual_relative(
      simple_criteria.eps_optimal_relative());
  result.set_eps_optimal_objective_gap_absolute(
      simple_criteria.eps_optimal_absolute());
  result.set_eps_optimal_objective_gap_relative(
      simple_criteria.eps_optimal_relative());
  return result;
}

std::optional<TerminationReasonAndPointType> CheckSimpleTerminationCriteria(
    const TerminationCriteria& criteria, const IterationStats& stats,
    const std::atomic<bool>* interrupt_solve) {
  if (stats.iteration_number() >= criteria.iteration_limit()) {
    return TerminationReasonAndPointType{
        .reason = TERMINATION_REASON_ITERATION_LIMIT, .type = POINT_TYPE_NONE};
  }
  if (stats.cumulative_kkt_matrix_passes() >=
      criteria.kkt_matrix_pass_limit()) {
    return TerminationReasonAndPointType{
        .reason = TERMINATION_REASON_KKT_MATRIX_PASS_LIMIT,
        .type = POINT_TYPE_NONE};
  }
  if (stats.cumulative_time_sec() >= criteria.time_sec_limit()) {
    return TerminationReasonAndPointType{
        .reason = TERMINATION_REASON_TIME_LIMIT, .type = POINT_TYPE_NONE};
  }
  if (interrupt_solve != nullptr && interrupt_solve->load() == true) {
    return TerminationReasonAndPointType{
        .reason = TERMINATION_REASON_INTERRUPTED_BY_USER,
        .type = POINT_TYPE_NONE};
  }
  return std::nullopt;
}

std::optional<TerminationReasonAndPointType> CheckTerminationCriteria(
    const TerminationCriteria& criteria, const IterationStats& stats,
    const QuadraticProgramBoundNorms& bound_norms,
    const std::atomic<bool>* interrupt_solve,
    const bool force_numerical_termination) {
  TerminationCriteria::DetailedOptimalityCriteria optimality_criteria =
      EffectiveOptimalityCriteria(criteria);
  for (const auto& convergence_stats : stats.convergence_information()) {
    if (OptimalityCriteriaMet(criteria.optimality_norm(), optimality_criteria,
                              convergence_stats, bound_norms)) {
      return TerminationReasonAndPointType{
          .reason = TERMINATION_REASON_OPTIMAL,
          .type = convergence_stats.candidate_type()};
    }
  }
  for (const auto& infeasibility_stats : stats.infeasibility_information()) {
    if (PrimalInfeasibilityCriteriaMet(criteria.eps_primal_infeasible(),
                                       infeasibility_stats)) {
      return TerminationReasonAndPointType{
          .reason = TERMINATION_REASON_PRIMAL_INFEASIBLE,
          .type = infeasibility_stats.candidate_type()};
    }
    if (DualInfeasibilityCriteriaMet(criteria.eps_dual_infeasible(),
                                     infeasibility_stats)) {
      return TerminationReasonAndPointType{
          .reason = TERMINATION_REASON_DUAL_INFEASIBLE,
          .type = infeasibility_stats.candidate_type()};
    }
  }
  if (const auto termination =
          CheckSimpleTerminationCriteria(criteria, stats, interrupt_solve);
      termination.has_value()) {
    return termination;
  }
  if (force_numerical_termination) {
    return TerminationReasonAndPointType{
        .reason = TERMINATION_REASON_NUMERICAL_ERROR, .type = POINT_TYPE_NONE};
  }
  return std::nullopt;
}

QuadraticProgramBoundNorms BoundNormsFromProblemStats(
    const QuadraticProgramStats& stats) {
  return {
      .l2_norm_primal_linear_objective = stats.objective_vector_l2_norm(),
      .l2_norm_constraint_bounds = stats.combined_bounds_l2_norm(),
      .l_inf_norm_primal_linear_objective = stats.objective_vector_abs_max(),
      .l_inf_norm_constraint_bounds = stats.combined_bounds_max()};
}

RelativeConvergenceInformation ComputeRelativeResiduals(
    const TerminationCriteria::DetailedOptimalityCriteria& optimality_criteria,
    const QuadraticProgramBoundNorms& norms,
    const ConvergenceInformation& stats) {
  auto eps_ratio = [](const double absolute, const double relative) {
    return relative == 0.0 ? std::numeric_limits<double>::infinity()
                           : absolute / relative;
  };
  const double eps_ratio_primal =
      eps_ratio(optimality_criteria.eps_optimal_primal_residual_absolute(),
                optimality_criteria.eps_optimal_primal_residual_relative());
  const double eps_ratio_dual =
      eps_ratio(optimality_criteria.eps_optimal_dual_residual_absolute(),
                optimality_criteria.eps_optimal_dual_residual_relative());
  const double eps_ratio_gap =
      eps_ratio(optimality_criteria.eps_optimal_objective_gap_absolute(),
                optimality_criteria.eps_optimal_objective_gap_relative());
  RelativeConvergenceInformation info;
  info.relative_l_inf_primal_residual =
      stats.l_inf_primal_residual() /
      (eps_ratio_primal + norms.l_inf_norm_constraint_bounds);
  info.relative_l2_primal_residual =
      stats.l2_primal_residual() /
      (eps_ratio_primal + norms.l2_norm_constraint_bounds);
  info.relative_l_inf_dual_residual =
      stats.l_inf_dual_residual() /
      (eps_ratio_dual + norms.l_inf_norm_primal_linear_objective);
  info.relative_l2_dual_residual =
      stats.l2_dual_residual() /
      (eps_ratio_dual + norms.l2_norm_primal_linear_objective);
  const double abs_obj =
      std::abs(stats.primal_objective()) + std::abs(stats.dual_objective());
  const double gap = stats.primal_objective() - stats.dual_objective();
  info.relative_optimality_gap = gap / (eps_ratio_gap + abs_obj);

  return info;
}

}  // namespace operations_research::pdlp
