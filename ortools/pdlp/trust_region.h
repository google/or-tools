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

#ifndef PDLP_TRUST_REGION_H_
#define PDLP_TRUST_REGION_H_

#include "Eigen/Core"
#include "ortools/pdlp/sharded_quadratic_program.h"
#include "ortools/pdlp/sharder.h"

namespace operations_research::pdlp {

struct TrustRegionResult {
  // The step_size of the solution.
  double solution_step_size;
  // The value objective_vector' * (solution - center_point)
  // when using the linear-time solver for LPs and QPs with objective matrix not
  // treated in the prox term. When using the approximate solver for QPs, this
  // field contains the value
  //   0.5 * (solution - center_point)' * objective_matrix * (
  //     solution - center_point)
  //   + objective_vector' * (solution - center_point)
  // instead.
  double objective_value;
  // The solution.
  Eigen::VectorXd solution;
};

// Solves the following trust-region problem with bound constraints:
// min_x objective_vector' * (x - center_point)
// s.t. variable_lower_bounds <= x <= variable_upper_bounds
//      || x - center_point ||_W <= target_radius
// where ||y||_W = sqrt(sum_i norm_weights[i] * y[i]^2)
// using an exact linear-time method.
// The sharder should have the same size as the number of variables in the
// problem.
// Assumes that there is always a feasible solution, that is, that
// variable_lower_bounds <= center_point <= variable_upper_bounds, and that
// norm_weights > 0, for 0 <= i < sharder.NumElements().
TrustRegionResult SolveTrustRegion(const Eigen::VectorXd& objective_vector,
                                   const Eigen::VectorXd& variable_lower_bounds,
                                   const Eigen::VectorXd& variable_upper_bounds,
                                   const Eigen::VectorXd& center_point,
                                   const Eigen::VectorXd& norm_weights,
                                   double target_radius,
                                   const Sharder& sharder);

// Solves the following trust-region problem with bound constraints:
// min_x (1/2) * (x - center_point)' * Q * (x - center_point)
//        + objective_vector' * (x - center_point)
// s.t. variable_lower_bounds <= x <= variable_upper_bounds
//      || x - center_point ||_W <= target_radius
// where ||y||_W = sqrt(sum_i norm_weights[i] * y[i]^2).
// It replaces the ball constraint || x - center_point ||_W <= target_radius
// with the equivalent constraint 0.5 * || x - center_point ||_W^2 <= 0.5 *
// target_radius^2 and does a binary search for a Lagrange multiplier for the
// latter constraint that is at most `solve_tolerance * max(1, lambda*)` away
// from the optimum Lagrange multiplier lambda*.
// The sharder should have the same size as the number of variables in the
// problem.
// Assumes that there is always a feasible solution, that is, that
// variable_lower_bounds <= center_point <= variable_upper_bounds, and that
// norm_weights > 0, for 0 <= i < sharder.NumElements().
TrustRegionResult SolveDiagonalTrustRegion(
    const Eigen::VectorXd& objective_vector,
    const Eigen::VectorXd& objective_matrix_diagonal,
    const Eigen::VectorXd& variable_lower_bounds,
    const Eigen::VectorXd& variable_upper_bounds,
    const Eigen::VectorXd& center_point, const Eigen::VectorXd& norm_weights,
    double target_radius, const Sharder& sharder, double solve_tolerance);

// Like SolveDiagonalTrustRegion, but extracts the problem data from a
// ShardedQuadraticProgram and implicitly concatenates the primal and dual parts
// before solving the trust-region subproblem.
TrustRegionResult SolveDiagonalQpTrustRegion(
    const ShardedQuadraticProgram& sharded_qp,
    const Eigen::VectorXd& primal_solution,
    const Eigen::VectorXd& dual_solution,
    const Eigen::VectorXd& primal_gradient,
    const Eigen::VectorXd& dual_gradient, const double primal_weight,
    double target_radius, double solve_tolerance);

struct LocalizedLagrangianBounds {
  // The value of the Lagrangian function L(x, y) at the given solution.
  double lagrangian_value;
  // A lower bound on the Lagrangian value, valid for the given radius.
  double lower_bound;
  // An upper bound on the Lagrangian value, valid for the given radius.
  double upper_bound;
  // The radius used when computing the bounds.
  double radius;
};

inline double BoundGap(const LocalizedLagrangianBounds& bounds) {
  return bounds.upper_bound - bounds.lower_bound;
}

// Defines a norm on a vector partitioned as (x, y) where x is the primal and y
// is the dual. The enum values define a joint norm as a function of ||x||_P and
// ||y||_D, whose definition depends on the context.
enum class PrimalDualNorm {
  // The joint norm ||(x,y)||_PD = max{||x||_P, ||y||_D}.
  kMaxNorm,
  // The joint norm (||(x,y)||_PD)^2 = (||x||_P)^2 + (||y||_D)^2.
  kEuclideanNorm
};

// Recall the saddle-point formulation OPT = min_x max_y L(x, y) defined at
// https://developers.google.com/optimization/lp/pdlp_math#saddle-point_formulation.
// This function computes lower and upper bounds on OPT with an additional ball
// or "trust- region" constraint on the domains of x and y.
//
// The bounds are derived from the solution of the following problem:
// min_{x,y} ∇_x L(primal_solution, dual_solution)^T (x - primal_solution) -
// ∇_y L(primal_solution, dual_solution)^T (y - dual_solution)
// subject to ||(x - primal_solution, y - dual_solution)||_PD <= radius,
// where x and y are constrained to their respective bounds and ||(x,y)||_PD is
// defined by primal_dual_norm.
// When use_diagonal_qp_trust_region_solver is true, the solver instead solves
// the following problem:
// min_{x,y} ∇_x L(primal_solution, dual_solution)^T (x - primal_solution) -
// ∇_y L(primal_solution, dual_solution)^T (y - dual_solution) +
// (1 / 2) * (x - primal_solution)^T * objective_matrix * (x - primal_solution),
// subject to ||(x - primal_solution, y - dual_solution)||_PD <= radius.
// use_diagonal_qp_trust_region_solver == true assumes that primal_dual_norm
// is the Euclidean norm and the objective matrix is diagonal.
// See SolveDiagonalTrustRegion() above for the meaning of
// diagonal_qp_trust_region_solver_tolerance.
//
// In the context of primal_dual_norm, the primal norm ||.||_P is defined as
// (||x||_P)^2 = (1 / 2) * primal_weight * ||x||_2^2, and the dual norm ||.||_D
// is defined as
// (||y||_D)^2 = (1 / 2) * (1 / primal_weight) * ||y||_2^2.
//
// Given an optimal solution (x, y) to the above problem, the lower bound is
// computed as L(primal_solution, dual_solution) +
// ∇_x L(primal_solution, dual_solution)^T (x - primal_solution)
// and the upper bound is computed as L(primal_solution, dual_solution) +
// ∇_y L(primal_solution, dual_solution)^T (y - dual_solution).
//
// The bounds are "localized" because they are guaranteed to bound OPT only if
// the ||.||_PD ball contains an optimal solution.
// The primal_product and dual_product arguments optionally specify the values
// of constraint_matrix * primal_solution and constraint_matrix.transpose() *
// dual_solution, respectively. If set to nullptr, they will be computed.
LocalizedLagrangianBounds ComputeLocalizedLagrangianBounds(
    const ShardedQuadraticProgram& sharded_qp,
    const Eigen::VectorXd& primal_solution,
    const Eigen::VectorXd& dual_solution, PrimalDualNorm primal_dual_norm,
    double primal_weight, double radius, const Eigen::VectorXd* primal_product,
    const Eigen::VectorXd* dual_product,
    bool use_diagonal_qp_trust_region_solver,
    double diagonal_qp_trust_region_solver_tolerance);

}  // namespace operations_research::pdlp

#endif  // PDLP_TRUST_REGION_H_
