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

#ifndef PDLP_TERMINATION_H_
#define PDLP_TERMINATION_H_

#include <optional>

#include "ortools/pdlp/solve_log.pb.h"
#include "ortools/pdlp/solvers.pb.h"

namespace operations_research::pdlp {

struct TerminationReasonAndPointType {
  TerminationReason reason;
  PointType type;
};

// Information about the quadratic program's primal and dual bounds that are
// needed to evaluate relative convergence criteria.
struct QuadraticProgramBoundNorms {
  double l2_norm_primal_linear_objective;
  double l2_norm_constraint_bounds;
  double l_inf_norm_primal_linear_objective;
  double l_inf_norm_constraint_bounds;
};

// Checks if any termination criteria are satisfied by the solution state
// described by the IterationStats instance stats (see definitions of
// termination criteria in solvers.proto). bound_norms provides the instance-
// dependent data required for the relative convergence criteria. Returns a
// termination reason and a point type if so (if multiple are satisfied, the
// optimality and infeasibility conditions are checked first). If
// force_numerical_termination is true, returns NUMERICAL_ERROR if no other
// criteria are satisfied. The return value is empty in any other case. If the
// output is not empty, the PointType indicates which entry prompted
// termination. If no entry prompted termination, e.g. NUMERICAL_ERROR or
// ITERATION_LIMIT is returned, then the PointType is set to POINT_TYPE_NONE.
// NOTE: This function assumes that the solution used to compute the stats
// satisfies the primal and dual variable bounds; see
// https://developers.google.com/optimization/lp/pdlp_math#dual_variable_bounds.
std::optional<TerminationReasonAndPointType> CheckTerminationCriteria(
    const TerminationCriteria& criteria, const IterationStats& stats,
    const QuadraticProgramBoundNorms& bound_norms,
    bool force_numerical_termination = false);

// Extracts the norms needed for the termination criteria from the full problem
// statistics.
QuadraticProgramBoundNorms BoundNormsFromProblemStats(
    const QuadraticProgramStats& stats);

// Metrics for tracking progress when relative convergence criteria are used.
// These depend on the ConvergenceInformation, the problem data, and the
// convergence tolerances.
struct RelativeConvergenceInformation {
  // Relative versions of the residuals, defined as
  //   relative_residual = residual / (eps_ratio + norm),
  // where
  //   eps_ratio = eps_optimal_absolute / eps_optimal_relative
  //   residual = one of the residuals (l{2,_inf}_{primal,dual}_residual)
  //   norm = the relative norm (l{2,_inf} norm of
  //          {constraint_bounds,primal_linear_objective} respectively).
  // If eps_optimal_relative = 0.0, these will all be 0.0.
  //
  // If eps_optimal_relative > 0.0, the absolute and relative termination
  // criteria translate to relative_residual <= eps_optimal_relative.
  double relative_l_inf_primal_residual = 0;
  double relative_l2_primal_residual = 0;
  double relative_l_inf_dual_residual = 0;
  double relative_l2_dual_residual = 0;

  // Relative optimality gap:
  //   (primal_objective - dual_objective) /
  //   (eps_ratio + |primal_objective| + |dual_objective|)
  double relative_optimality_gap = 0;
};

RelativeConvergenceInformation ComputeRelativeResiduals(
    double eps_optimal_absolute, double eps_optimal_relative,
    const QuadraticProgramBoundNorms& norms,
    const ConvergenceInformation& stats);

}  // namespace operations_research::pdlp

#endif  // PDLP_TERMINATION_H_
