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

// These are internal helper functions and classes that implicitly or explicitly
// operate on a `ShardedQuadraticProgram`. Utilities that are purely linear
// algebra operations (e.g., norms) should be defined in sharder.h instead.

#ifndef PDLP_SHARDED_OPTIMIZATION_UTILS_H_
#define PDLP_SHARDED_OPTIMIZATION_UTILS_H_

#include <limits>
#include <optional>
#include <random>

#include "Eigen/Core"
#include "ortools/pdlp/sharded_quadratic_program.h"
#include "ortools/pdlp/sharder.h"
#include "ortools/pdlp/solve_log.pb.h"

namespace operations_research::pdlp {

// This computes weighted averages of vectors.
// It satisfies the following: if all the averaged vectors have component i
// equal to x then the average has component i exactly equal to x, without any
// floating-point roundoff. In practice the above is probably still true with
// "equal to x" replaced with "at least x" or "at most x". However unrealistic
// counter examples probably exist involving a new item with weight 10^15 times
// greater than the total weight so far.
class ShardedWeightedAverage {
 public:
  // Initializes the weighted average by creating a vector sized according to
  // the number of elements in `sharder`. Retains the pointer to `sharder`, so
  // `sharder` must outlive this object.
  explicit ShardedWeightedAverage(const Sharder* sharder);

  ShardedWeightedAverage(ShardedWeightedAverage&&) = default;
  ShardedWeightedAverage& operator=(ShardedWeightedAverage&&) = default;

  // Adds `datapoint` to the average weighted by `weight`. CHECK-fails if
  // `weight` is negative.
  void Add(const Eigen::VectorXd& datapoint, double weight);

  // Clears the sum to zero, i.e., just constructed.
  void Clear();

  // Returns true if there is at least one term in the average with a positive
  // weight.
  bool HasNonzeroWeight() const { return sum_weights_ > 0.0; }

  // Returns the sum of the weights of the datapoints added so far.
  double Weight() const { return sum_weights_; }

  // Computes the weighted average of the datapoints added so far, i.e.,
  // sum_i weight[i] * datapoint[i] / sum_i weight[i]. The results are set to
  // zero if `HasNonzeroWeight()` is false.
  Eigen::VectorXd ComputeAverage() const;

  int NumTerms() const { return num_terms_; }

 private:
  Eigen::VectorXd average_;
  double sum_weights_ = 0.0;
  int num_terms_ = 0;
  const Sharder* sharder_;
};

// Returns a `QuadraticProgramStats` for a `ShardedQuadraticProgram`.
QuadraticProgramStats ComputeStats(const ShardedQuadraticProgram& qp,
                                   double infinite_constraint_bound_threshold =
                                       std::numeric_limits<double>::infinity());

// `LInfRuizRescaling` and `L2NormRescaling` rescale the (scaled) constraint
// matrix of the LP by updating the scaling vectors in-place. More specifically,
// the (scaled) constraint matrix always has the format: `diag(row_scaling_vec)
// * sharded_qp.Qp().constraint_matrix * diag(col_scaling_vec)`, and
// `row_scaling_vec` and `col_scaling_vec` are updated in-place. On input,
// `row_scaling_vec` and `col_scaling_vec` provide the initial scaling.

// With each iteration of `LInfRuizRescaling` scaling, `row_scaling_vec`
// (`col_scaling_vec`) is divided by the sqrt of the row (col) LInf norm of the
// current (scaled) constraint matrix. The (scaled) constraint matrix approaches
// having all row and column LInf norms equal to 1 as the number of iterations
// goes to infinity. This convergence is fast (linear). More details of Ruiz
// rescaling algorithm can be found at:
// http://www.numerical.rl.ac.uk/reports/drRAL2001034.pdf.
void LInfRuizRescaling(const ShardedQuadraticProgram& sharded_qp,
                       int num_iterations, Eigen::VectorXd& row_scaling_vec,
                       Eigen::VectorXd& col_scaling_vec);

// `L2NormRescaling` divides `row_scaling_vec` (`col_scaling_vec`) by the sqrt
// of the row (col) L2 norm of the current (scaled) constraint matrix. Unlike
// `LInfRescaling`, this function does only one iteration because the scaling
// procedure does not converge in general. This is not Ruiz rescaling for the L2
// norm.
void L2NormRescaling(const ShardedQuadraticProgram& sharded_qp,
                     Eigen::VectorXd& row_scaling_vec,
                     Eigen::VectorXd& col_scaling_vec);

struct RescalingOptions {
  int l_inf_ruiz_iterations;
  bool l2_norm_rescaling;
};

struct ScalingVectors {
  Eigen::VectorXd row_scaling_vec;
  Eigen::VectorXd col_scaling_vec;
};

// Applies the rescaling specified by `rescaling_options` to `sharded_qp` (in
// place). Returns the scaling vectors that were applied.
ScalingVectors ApplyRescaling(const RescalingOptions& rescaling_options,
                              ShardedQuadraticProgram& sharded_qp);

struct LagrangianPart {
  double value = 0.0;
  Eigen::VectorXd gradient;
};

// Computes the value of the primal part of the Lagrangian function defined at
// https://developers.google.com/optimization/lp/pdlp_math, i.e.,
// c^T x + (1/2) x^T Qx - y^T Ax and its gradient with respect to the primal
// variables x, i.e., c + Qx - A^T y. `dual_product` is A^T y. Note: The
// objective constant is omitted. The result is undefined and invalid if any
// primal bounds are violated.
LagrangianPart ComputePrimalGradient(const ShardedQuadraticProgram& sharded_qp,
                                     const Eigen::VectorXd& primal_solution,
                                     const Eigen::VectorXd& dual_product);

// Returns a subderivative of the concave dual penalty function that appears in
// the Lagrangian:
//   -p(`dual`; -`constraint_upper_bound`, -`constraint_lower_bound`) =
//     { `constraint_upper_bound * dual` when `dual < 0`, 0 when `dual == 0`,
//       and `constraint_lower_bound * dual` when `dual > 0`}
// (as defined at https://developers.google.com/optimization/lp/pdlp_math).
// The subderivative is not necessarily unique when dual == 0. In this case, if
// only one of the bounds is finite, we return that one. If both are finite, we
// return `primal_product` projected onto the bounds, which causes the dual
// Lagrangian gradient to be zero when the constraint is not violated. If both
// are infinite, we return zero. The value returned is valid only when the
// function is finite-valued.
double DualSubgradientCoefficient(double constraint_lower_bound,
                                  double constraint_upper_bound, double dual,
                                  double primal_product);

// Computes the value of the dual part of the Lagrangian function defined at
// https://developers.google.com/optimization/lp/pdlp_math, i.e., -h^*(y) and
// the gradient of the Lagrangian with respect to the dual variables y, i.e.,
// -Ax - \grad_y h^*(y). Note the asymmetry with `ComputePrimalGradient`: the
// term -y^T Ax is not part of the value. Because h^*(y) is piece-wise linear, a
// subgradient is returned at a point of non-smoothness. `primal_product` is Ax.
// The result is undefined and invalid if any duals violate their bounds.
LagrangianPart ComputeDualGradient(const ShardedQuadraticProgram& sharded_qp,
                                   const Eigen::VectorXd& dual_solution,
                                   const Eigen::VectorXd& primal_product);

struct SingularValueAndIterations {
  double singular_value;
  int num_iterations;
  double estimated_relative_error;
};

// Estimates the maximum singular value of A by applying the method of power
// iteration to A^T A. If `primal_solution` or `dual_solution` is provided,
// restricts to the "active" part of A, that is, the columns (rows) for
// variables that are not at their bounds in the solution. The estimate will
// have `desired_relative_error` with probability at least 1 -
// `failure_probability`. The number of iterations will be approximately
// log(`primal_size` / `failure_probability`^2)/(2 * `desired_relative_error`).
// Uses a mersenne-twister portable random number generator to generate the
// starting point for the power method, in order to have deterministic results.
SingularValueAndIterations EstimateMaximumSingularValueOfConstraintMatrix(
    const ShardedQuadraticProgram& sharded_qp,
    const std::optional<Eigen::VectorXd>& primal_solution,
    const std::optional<Eigen::VectorXd>& dual_solution,
    double desired_relative_error, double failure_probability,
    std::mt19937& mt_generator);

// Checks if the lower and upper bounds of the problem are consistent, i.e. for
// each variable and constraint bound we have lower_bound <= upper_bound. If
// the input is consistent the method returns true, otherwise it returns false.
// See also `HasValidBounds(const QuadraticProgram&)`.
bool HasValidBounds(const ShardedQuadraticProgram& sharded_qp);

// Projects `primal` onto the variable bounds constraints.
void ProjectToPrimalVariableBounds(const ShardedQuadraticProgram& sharded_qp,
                                   Eigen::VectorXd& primal);

// Projects `dual` onto the dual variable bounds; see
// https://developers.google.com/optimization/lp/pdlp_math#dual_variable_bounds.
void ProjectToDualVariableBounds(const ShardedQuadraticProgram& sharded_qp,
                                 Eigen::VectorXd& dual);

}  // namespace operations_research::pdlp

#endif  // PDLP_SHARDED_OPTIMIZATION_UTILS_H_
