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

#ifndef PDLP_ITERATION_STATS_H_
#define PDLP_ITERATION_STATS_H_

#include <optional>
#include <vector>

#include "Eigen/Core"
#include "ortools/pdlp/sharded_quadratic_program.h"
#include "ortools/pdlp/solve_log.pb.h"
#include "ortools/pdlp/solvers.pb.h"

namespace operations_research::pdlp {

// Returns convergence statistics about a primal/dual solution pair. The stats
// are with respect to `sharded_qp` (which is typically scaled).
// This function is equivalent to `ComputeConvergenceInformation` given scaling
// vectors uniformly equal to one.
ConvergenceInformation ComputeScaledConvergenceInformation(
    const PrimalDualHybridGradientParams& params,
    const ShardedQuadraticProgram& sharded_qp,
    const Eigen::VectorXd& primal_solution,
    const Eigen::VectorXd& dual_solution,
    double componentwise_primal_residual_offset,
    double componentwise_dual_residual_offset, PointType candidate_type);

// Returns convergence statistics about a primal/dual solution pair. It is
// assumed that `scaled_sharded_qp` has been transformed from the original qp by
// `ShardedQuadraticProgram::RescaleQuadraticProgram(col_scaling_vec,
// row_scaling_vec)`. `scaled_primal_solution` and `scaled_dual_solution` are
// solutions for the scaled problem. The stats are computed with respect to the
// implicit original problem. `componentwise_primal_residual_offset` and
// `componentwise_dual_residual_offset` are the offsets (i.e., eps_ratio) used
// for computing the l_inf_componentwise residual norms.
// NOTE: This function assumes that `scaled_primal_solution` satisfies the
// variable bounds and `scaled_dual_solution` satisfies the dual variable
// bounds; see
// https://developers.google.com/optimization/lp/pdlp_math#dual_variable_bounds.
ConvergenceInformation ComputeConvergenceInformation(
    const PrimalDualHybridGradientParams& params,
    const ShardedQuadraticProgram& sharded_qp,
    const Eigen::VectorXd& col_scaling_vec,
    const Eigen::VectorXd& row_scaling_vec,
    const Eigen::VectorXd& scaled_primal_solution,
    const Eigen::VectorXd& scaled_dual_solution,
    double componentwise_primal_residual_offset,
    double componentwise_dual_residual_offset, PointType candidate_type);

// Returns infeasibility statistics about a primal/dual infeasibility
// certificate estimate. It is assumed that `scaled_sharded_qp` has been
// transformed from the original qp by
// `ShardedQuadraticProgram::RescaleQuadraticProgram(col_scaling_vec,
// row_scaling_vec)`. `scaled_primal_ray` and `scaled_dual_ray` are
// potential certificates for the scaled problem. The stats are computed with
// respect to the implicit original problem.
InfeasibilityInformation ComputeInfeasibilityInformation(
    const PrimalDualHybridGradientParams& params,
    const ShardedQuadraticProgram& scaled_sharded_qp,
    const Eigen::VectorXd& col_scaling_vec,
    const Eigen::VectorXd& row_scaling_vec,
    const Eigen::VectorXd& scaled_primal_ray,
    const Eigen::VectorXd& scaled_dual_ray, PointType candidate_type);

// Computes the reduced costs vector, objective_matrix * `primal_solution` +
// objective_vector - constraint_matrix * `dual_solution` - dual_residuals, when
// `use_zero_primal_objective` is false, and -constraint_matrix *
// `dual_solution` - dual_residuals when `use_zero_primal_objective` is true.
// The elements of the vector are corrected component-wise to zero to ensure
// that the dual objective takes a finite value. See
// https://developers.google.com/optimization/lp/pdlp_math#reduced_costs_dual_residuals_and_the_corrected_dual_objective.
Eigen::VectorXd ReducedCosts(const PrimalDualHybridGradientParams& params,
                             const ShardedQuadraticProgram& scaled_sharded_qp,
                             const Eigen::VectorXd& primal_solution,
                             const Eigen::VectorXd& dual_solution,
                             bool use_zero_primal_objective = false);

// Finds and returns the `ConvergenceInformation` with the specified
// `candidate_type`, or std::nullopt if no such candidate exists.
std::optional<ConvergenceInformation> GetConvergenceInformation(
    const IterationStats& stats, PointType candidate_type);

// Finds and returns the `InfeasibilityInformation` with the specified
// `candidate_type`, or std::nullopt if no such candidate exists.
std::optional<InfeasibilityInformation> GetInfeasibilityInformation(
    const IterationStats& stats, PointType candidate_type);

// Finds and returns the `PointMetadata` with the specified
// `point_type`, or std::nullopt if no such point exists.
std::optional<PointMetadata> GetPointMetadata(const IterationStats& stats,
                                              PointType point_type);

// For each entry in `random_projection_seeds`, computes a random projection of
// the primal/dual solution pair onto pseudo-random vectors generated from that
// seed and adds the results to
// `random_primal_projections`/`random_dual_projections` in `metadata`.
void SetRandomProjections(const ShardedQuadraticProgram& sharded_qp,
                          const Eigen::VectorXd& primal_solution,
                          const Eigen::VectorXd& dual_solution,
                          const std::vector<int>& random_projection_seeds,
                          PointMetadata& metadata);

}  // namespace operations_research::pdlp

#endif  // PDLP_ITERATION_STATS_H_
