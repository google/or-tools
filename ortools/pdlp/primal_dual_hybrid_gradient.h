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

#ifndef PDLP_PRIMAL_DUAL_HYBRID_GRADIENT_H_
#define PDLP_PRIMAL_DUAL_HYBRID_GRADIENT_H_

#include <atomic>
#include <functional>
#include <optional>

#include "Eigen/Core"
#include "ortools/lp_data/lp_data.h"
#include "ortools/pdlp/quadratic_program.h"
#include "ortools/pdlp/solve_log.pb.h"
#include "ortools/pdlp/solvers.pb.h"
#include "ortools/pdlp/termination.h"

namespace operations_research::pdlp {

struct PrimalAndDualSolution {
  Eigen::VectorXd primal_solution;
  Eigen::VectorXd dual_solution;
};

// The following table defines the interpretation of the result vectors
// depending on the value of `solve_log.termination_reason`: (the
// TERMINATION_REASON_ prefix is omitted for brevity):
//
// * OPTIMAL: the vectors satisfy the termination criteria for optimality.
// * PRIMAL_INFEASIBLE: dual_solution and reduced_costs provide an approximate
//   certificate of primal infeasibility; see
//   https://developers.google.com/optimization/lp/pdlp_math#infeasibility_identification
//   for more information.
// * DUAL_INFEASIBLE: `primal_solution` provides an approximate certificate of
//   dual infeasibility; see
//   https://developers.google.com/optimization/lp/pdlp_math#infeasibility_identification
//   for more information.
// * PRIMAL_OR_DUAL_INFEASIBLE: the problem was shown to be primal and/or dual
//   infeasible but no certificate of infeasibility is available. The
//   `primal_solution` and `dual_solution` have no meaning. This status is only
//   used when presolve is enabled.
// * TIME_LIMIT, ITERATION_LIMIT, KKT_MATRIX_PASS_LIMIT, NUMERICAL_ERROR,
//   INTERRUPTED_BY_USER: the vectors contain an iterate at the time that the
//   respective event occurred. Their values may or may not be meaningful. In
//   some cases solution quality information is available; see documentation for
//   `solve_log.solution_type`.
// * INVALID_PROBLEM, INVALID_PARAMETER, OTHER: the solution vectors are
//   meaningless and may not have lengths consistent with the input problem.
struct SolverResult {
  Eigen::VectorXd primal_solution;
  // See https://developers.google.com/optimization/lp/pdlp_math for the
  // interpretation of `dual_solution` and `reduced_costs`.
  Eigen::VectorXd dual_solution;
  Eigen::VectorXd reduced_costs;
  SolveLog solve_log;
};

struct IterationCallbackInfo {
  TerminationCriteria termination_criteria;
  IterationStats iteration_stats;
  QuadraticProgramBoundNorms bound_norms;
};

// Solves the given QP using PDLP (Primal-Dual hybrid gradient enhanced for LP).
//
// All operations that are repeated each iteration are executed in parallel
// using `params.num_threads()` threads.
//
// The algorithm generally follows the description in
// https://arxiv.org/pdf/2106.04756.pdf, with further enhancements for QP.
// Notation here and in the implementation follows Chambolle and Pock, "On the
// ergodic convergence rates of a first-order primal-dual algorithm"
// (http://www.optimization-online.org/DB_FILE/2014/09/4532.pdf).
// That paper doesn't explicitly use the terminology "primal-dual hybrid
// gradient" but their Theorem 1 is analyzing PDHG. See
// https://developers.google.com/optimization/lp/pdlp_math#saddle-point_formulation
// for the saddle-point formulation of the QP we use that is compatible with
// Chambolle and Pock.
//
// We use 0.5 ||.||^2 for both the primal and dual distance functions.
//
// We parameterize the primal and dual step sizes (tau and sigma in Chambolle
// and Pock) as:
//     primal_stepsize = step_size / primal_weight
//     dual_stepsize = step_size * primal_weight
// where step_size and primal_weight are parameters.
// `params.linesearch_rule` determines the update rule for step_size.
// `params.initial_primal_weight` specifies how primal_weight is initialized
// and `params.primal_weight_update_smoothing` controls how primal_weight is
// updated.
//
// If `interrupt_solve` is not nullptr, then the solver will periodically check
// if `interrupt_solve->load()` is true, in which case the solve will terminate
// with `TERMINATION_REASON_INTERRUPTED_BY_USER`.
//
// If `iteration_stats_callback` is not nullptr, then at each termination step
// (when iteration stats are logged), `iteration_stats_callback` will also
// be called with those iteration stats.
//
// Callers MUST check `solve_log.termination_reason` before using the vectors in
// the `SolverResult`. See the comment on `SolverResult` for interpreting the
// termination reason.
//
// All objective values reported by the algorithm are transformed by using
// `QuadraticProgram::ApplyObjectiveScalingAndOffset`.
//
// NOTE: `qp` is intentionally passed by value, because
// `PrimalDualHybridGradient` modifies its copy.
SolverResult PrimalDualHybridGradient(
    QuadraticProgram qp, const PrimalDualHybridGradientParams& params,
    const std::atomic<bool>* interrupt_solve = nullptr,
    std::function<void(const IterationCallbackInfo&)> iteration_stats_callback =
        nullptr);

// Like above but optionally starts with the given `initial_solution`. If no
// `initial_solution` is given the zero vector is used. In either case
// `initial_solution` is projected onto the primal and dual variable bounds
// before use. Convergence should be faster if `initial_solution` is close to an
// optimal solution. NOTE: `initial_solution` is intentionally passed by value.
SolverResult PrimalDualHybridGradient(
    QuadraticProgram qp, const PrimalDualHybridGradientParams& params,
    std::optional<PrimalAndDualSolution> initial_solution,
    const std::atomic<bool>* interrupt_solve = nullptr,
    std::function<void(const IterationCallbackInfo&)> iteration_stats_callback =
        nullptr);

namespace internal {

// Computes variable and constraint statuses. This determines if primal
// variables are at their bounds based on exact comparisons and therefore may
// not work with unscaled solutions. The primal and dual solution in the
// returned `ProblemSolution` are NOT set.
glop::ProblemSolution ComputeStatuses(const QuadraticProgram& qp,
                                      const PrimalAndDualSolution& solution);

}  // namespace internal

}  // namespace operations_research::pdlp

#endif  // PDLP_PRIMAL_DUAL_HYBRID_GRADIENT_H_
