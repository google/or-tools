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

#ifndef PDLP_TRUST_REGION_H_
#define PDLP_TRUST_REGION_H_

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

#include "Eigen/Core"
#include "absl/algorithm/container.h"
#include "absl/log/check.h"
#include "ortools/base/mathutil.h"
#include "ortools/pdlp/sharded_quadratic_program.h"
#include "ortools/pdlp/sharder.h"

namespace operations_research::pdlp {

struct TrustRegionResult {
  // The step_size of the solution.
  double solution_step_size;
  // The value objective_vector^T * (solution - center_point)
  // when using the linear-time solver for LPs and QPs with objective matrix not
  // treated in the prox term. When using the approximate solver for QPs, this
  // field contains the value
  //   0.5 * (solution - center_point)^T * objective_matrix * (
  //     solution - center_point)
  //   + objective_vector^T * (solution - center_point)
  // instead.
  double objective_value;
  // The solution.
  Eigen::VectorXd solution;
};

// Solves the following trust-region problem with bound constraints:
// min_x `objective_vector`^T * (x - `center_point`)
// s.t. `variable_lower_bounds` <= x <= `variable_upper_bounds`
//      || x - `center_point` ||_W <= `target_radius`
// where ||y||_W = sqrt(sum_i `norm_weights`[i] * y[i]^2)
// using an exact linear-time method.
// `sharder` should have the same size as the number of variables in the
// problem.
// Assumes that there is always a feasible solution, that is, that
// `variable_lower_bounds` <= `center_point` <= `variable_upper_bounds`, and
// that `norm_weights` > 0, for 0 <= i < `sharder.NumElements()`.
TrustRegionResult SolveTrustRegion(const Eigen::VectorXd& objective_vector,
                                   const Eigen::VectorXd& variable_lower_bounds,
                                   const Eigen::VectorXd& variable_upper_bounds,
                                   const Eigen::VectorXd& center_point,
                                   const Eigen::VectorXd& norm_weights,
                                   double target_radius,
                                   const Sharder& sharder);

// Solves the following trust-region problem with bound constraints:
// min_x (1/2) * (x - `center_point`)^T * Q * (x - `center_point`)
//        + `objective_vector`^T * (x - `center_point`)
// s.t. `variable_lower_bounds` <= x <= `variable_upper_bounds`
//      || x - `center_point` ||_W <= `target_radius`
// where ||y||_W = sqrt(sum_i `norm_weights`[i] * y[i]^2).
// It replaces the ball constraint || x - `center_point` ||_W <= `target_radius`
// with the equivalent constraint 0.5 * || x - `center_point` ||_W^2 <= 0.5 *
// `target_radius`^2 and does a binary search for a Lagrange multiplier for the
// latter constraint that is at most (`solve_tolerance` * max(1, lambda*)) away
// from the optimum Lagrange multiplier lambda*.
// `sharder` should have the same size as the number of variables in the
// problem.
// Assumes that there is always a feasible solution, that is, that
// `variable_lower_bounds` <= `center_point` <= `variable_upper_bounds`, and
// that `norm_weights` > 0, for 0 <= i < `sharder.NumElements()`.
TrustRegionResult SolveDiagonalTrustRegion(
    const Eigen::VectorXd& objective_vector,
    const Eigen::VectorXd& objective_matrix_diagonal,
    const Eigen::VectorXd& variable_lower_bounds,
    const Eigen::VectorXd& variable_upper_bounds,
    const Eigen::VectorXd& center_point, const Eigen::VectorXd& norm_weights,
    double target_radius, const Sharder& sharder, double solve_tolerance);

// Like `SolveDiagonalTrustRegion`, but extracts the problem data from a
// `ShardedQuadraticProgram` and implicitly concatenates the primal and dual
// parts before solving the trust-region subproblem.
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
// or "trust-region" constraint on the domains of x and y.
//
// The bounds are derived from the solution of the following problem:
// min_{x,y}
//    ∇_x L(`primal_solution`, `dual_solution`)^T (x - `primal_solution`)
//  - ∇_y L(`primal_solution`, `dual_solution`)^T (y - `dual_solution`)
// subject to
//    ||(x - `primal_solution`, y - `dual_solution`)||_PD <= `radius`,
// where x and y are constrained to their respective bounds and ||(x,y)||_PD is
// defined by `primal_dual_norm`.
// When `use_diagonal_qp_trust_region_solver` is true, the solver instead solves
// the following problem:
// min_{x,y}
//    ∇_x L(`primal_solution`, `dual_solution`)^T (x - `primal_solution`)
//  - ∇_y L(`primal_solution`, `dual_solution`)^T (y - `dual_solution`)
//  + (1 / 2) * (x - `primal_solution`)^T * `objective_matrix`
//    * (x - `primal_solution`),
// subject to
//    ||(x - `primal_solution`, y - `dual_solution`)||_PD <= `radius`.
// `use_diagonal_qp_trust_region_solver` == true assumes that `primal_dual_norm`
// is the Euclidean norm and the objective matrix is diagonal.
// See `SolveDiagonalTrustRegion()` above for the meaning of
// `diagonal_qp_trust_region_solver_tolerance`.
//
// In the context of primal_dual_norm, the primal norm ||.||_P is defined as
// (||x||_P)^2 = (1 / 2) * `primal_weight` * ||x||_2^2, and the dual norm
// ||.||_D is defined as
// (||y||_D)^2 = (1 / 2) * (1 / `primal_weight`) * ||y||_2^2.
//
// Given an optimal solution (x, y) to the above problem, the lower bound is
// computed as L(`primal_solution`, `dual_solution`) +
// ∇_x L(`primal_solution`, `dual_solution`)^T (x - `primal_solution`)
// and the upper bound is computed as L(`primal_solution`, `dual_solution`) +
// ∇_y L(`primal_solution`, `dual_solution`)^T (y - `dual_solution`).
//
// The bounds are "localized" because they are guaranteed to bound OPT only if
// the ||.||_PD ball contains an optimal solution.
// `primal_product` and `dual_product` optionally specify the values of
// `constraint_matrix` * `primal_solution` and `constraint_matrix.transpose()` *
// `dual_solution`, respectively. If set to nullptr, they will be computed.
LocalizedLagrangianBounds ComputeLocalizedLagrangianBounds(
    const ShardedQuadraticProgram& sharded_qp,
    const Eigen::VectorXd& primal_solution,
    const Eigen::VectorXd& dual_solution, PrimalDualNorm primal_dual_norm,
    double primal_weight, double radius, const Eigen::VectorXd* primal_product,
    const Eigen::VectorXd* dual_product,
    bool use_diagonal_qp_trust_region_solver,
    double diagonal_qp_trust_region_solver_tolerance);

namespace internal {

// These functions templated on `TrustRegionProblem` compute values useful to
// the trust region solve. The templated `TrustRegionProblem` type should
// provide methods:
//   `double Objective(int64_t index) const;`
//   `double LowerBound(int64_t index) const;`
//   `double UpperBound(int64_t index) const;`
//   `double CenterPoint(int64_t index) const;`
//   `double NormWeight(int64_t index) const;`
// See trust_region.cc for more details and several implementations.

// The distance (in the indexed element) from the center point to the bound, in
// the direction that reduces the objective.
template <typename TrustRegionProblem>
double DistanceAtCriticalStepSize(const TrustRegionProblem& problem,
                                  const int64_t index) {
  if (problem.Objective(index) == 0.0) {
    return 0.0;
  }
  if (problem.Objective(index) > 0.0) {
    return problem.LowerBound(index) - problem.CenterPoint(index);
  } else {
    return problem.UpperBound(index) - problem.CenterPoint(index);
  }
}

// The critical step size is the step size at which the indexed element hits its
// bound (or infinity if that doesn't happen).
template <typename TrustRegionProblem>
double CriticalStepSize(const TrustRegionProblem& problem,
                        const int64_t index) {
  if (problem.Objective(index) == 0.0) {
    return std::numeric_limits<double>::infinity();
  }
  return -problem.NormWeight(index) *
         DistanceAtCriticalStepSize(problem, index) / problem.Objective(index);
}

// The value of the indexed element at the given step_size, projected onto the
// bounds.
template <typename TrustRegionProblem>
double ProjectedValue(const TrustRegionProblem& problem, const int64_t index,
                      const double step_size) {
  const double full_step =
      problem.CenterPoint(index) -
      step_size * problem.Objective(index) / problem.NormWeight(index);
  return std::min(std::max(full_step, problem.LowerBound(index)),
                  problem.UpperBound(index));
}

// An easy way of computing medians that's slightly off when the length of the
// array is even. `array` is intentionally passed by value.
// `value_function` maps an element of `array` to its (double) value. Returns
// the value of the median element.
template <typename ArrayType, typename ValueFunction>
double EasyMedian(ArrayType array, ValueFunction value_function) {
  CHECK_GT(array.size(), 0);
  auto middle = array.begin() + (array.size() / 2);
  absl::c_nth_element(array, middle,
                      [&](typename ArrayType::value_type lhs,
                          typename ArrayType::value_type rhs) {
                        return value_function(lhs) < value_function(rhs);
                      });
  return value_function(*middle);
}

// Lists the undecided components (from [`start_index`, `end_index`) as those
// with finite critical step sizes. The components with infinite critical step
// sizes will never hit their bounds, so returns their contribution to square of
// the radius.
template <typename TrustRegionProblem>
double ComputeInitialUndecidedComponents(
    const TrustRegionProblem& problem, int64_t start_index, int64_t end_index,
    std::vector<int64_t>& undecided_components) {
  // TODO(user): Evaluate dropping this `reserve()`, since it wastes space
  // if many components are decided.
  undecided_components.clear();
  undecided_components.reserve(end_index - start_index);
  double radius_coefficient = 0.0;
  for (int64_t index = start_index; index < end_index; ++index) {
    if (std::isfinite(internal::CriticalStepSize(problem, index))) {
      undecided_components.push_back(index);
    } else {
      // Simplified from norm_weight * (objective / norm_weight)^2.
      radius_coefficient += MathUtil::Square(problem.Objective(index)) /
                            problem.NormWeight(index);
    }
  }
  return radius_coefficient;
}

template <typename TrustRegionProblem>
double RadiusSquaredOfUndecidedComponents(
    const TrustRegionProblem& problem, const double step_size,
    const std::vector<int64_t>& undecided_components) {
  return absl::c_accumulate(
      undecided_components, 0.0, [&](double sum, int64_t index) {
        const double distance_at_projected_value =
            internal::ProjectedValue(problem, index, step_size) -
            problem.CenterPoint(index);
        return sum + problem.NormWeight(index) *
                         MathUtil::Square(distance_at_projected_value);
      });
}

// Points whose critical step sizes are greater than or equal to
// `step_size_threshold` are eliminated from the undecided components (we know
// they'll be determined by center_point - step_size * objective /
// norm_weights). Returns the coefficient of step_size^2 that accounts of the
// contribution of the removed variables to the radius squared.
template <typename TrustRegionProblem>
double RemoveCriticalStepsAboveThreshold(
    const TrustRegionProblem& problem, const double step_size_threshold,
    std::vector<int64_t>& undecided_components) {
  double variable_radius_coefficient = 0.0;
  for (const int64_t index : undecided_components) {
    if (internal::CriticalStepSize(problem, index) >= step_size_threshold) {
      // Simplified from norm_weight * (objective / norm_weight)^2.
      variable_radius_coefficient +=
          MathUtil::Square(problem.Objective(index)) /
          problem.NormWeight(index);
    }
  }
  auto result =
      std::remove_if(undecided_components.begin(), undecided_components.end(),
                     [&](const int64_t index) {
                       return internal::CriticalStepSize(problem, index) >=
                              step_size_threshold;
                     });
  undecided_components.erase(result, undecided_components.end());
  return variable_radius_coefficient;
}

// Points whose critical step sizes are smaller than or equal to
// `step_size_threshold` are eliminated from the undecided components (we know
// they'll always be at their bounds). Returns the weighted distance squared
// from the center point for the removed components.
template <typename TrustRegionProblem>
double RemoveCriticalStepsBelowThreshold(
    const TrustRegionProblem& problem, const double step_size_threshold,
    std::vector<int64_t>& undecided_components) {
  double radius_sq = 0.0;
  for (const int64_t index : undecided_components) {
    if (internal::CriticalStepSize(problem, index) <= step_size_threshold) {
      radius_sq += problem.NormWeight(index) *
                   MathUtil::Square(
                       internal::DistanceAtCriticalStepSize(problem, index));
    }
  }
  auto result =
      std::remove_if(undecided_components.begin(), undecided_components.end(),
                     [&](const int64_t index) {
                       return internal::CriticalStepSize(problem, index) <=
                              step_size_threshold;
                     });
  undecided_components.erase(result, undecided_components.end());
  return radius_sq;
}

// `PrimalTrustRegionProblem` defines the primal trust region problem given a
// `QuadraticProgram`, `primal_solution`, and `primal_gradient`. It captures
// const references to the constructor arguments, which should outlive the class
// instance.
// The corresponding trust region problem is
// min_x `primal_gradient`^T * (x - `primal_solution`)
// s.t. `qp.variable_lower_bounds` <= x <= `qp.variable_upper_bounds`
//      || x - `primal_solution` ||_2 <= target_radius
class PrimalTrustRegionProblem {
 public:
  PrimalTrustRegionProblem(const QuadraticProgram* qp,
                           const Eigen::VectorXd* primal_solution,
                           const Eigen::VectorXd* primal_gradient,
                           const double norm_weight = 1.0)
      : qp_(*qp),
        primal_solution_(*primal_solution),
        primal_gradient_(*primal_gradient),
        norm_weight_(norm_weight) {}
  double Objective(int64_t index) const { return primal_gradient_[index]; }
  double LowerBound(int64_t index) const {
    return qp_.variable_lower_bounds[index];
  }
  double UpperBound(int64_t index) const {
    return qp_.variable_upper_bounds[index];
  }
  double CenterPoint(int64_t index) const { return primal_solution_[index]; }
  double NormWeight(int64_t index) const { return norm_weight_; }

 private:
  const QuadraticProgram& qp_;
  const Eigen::VectorXd& primal_solution_;
  const Eigen::VectorXd& primal_gradient_;
  const double norm_weight_;
};

// `DualTrustRegionProblem` defines the dual trust region problem given a
// `QuadraticProgram`, `dual_solution`, and `dual_gradient`. It captures const
// references to the constructor arguments, which should outlive the class
// instance.
// The corresponding trust region problem is
// max_y `dual_gradient`^T * (y - `dual_solution`)
// s.t. `qp.implicit_dual_lower_bounds` <= y <= `qp.implicit_dual_upper_bounds`
//      || y - `dual_solution` ||_2 <= target_radius
// where the implicit dual bounds are those given in
// https://developers.google.com/optimization/lp/pdlp_math#dual_variable_bounds
class DualTrustRegionProblem {
 public:
  DualTrustRegionProblem(const QuadraticProgram* qp,
                         const Eigen::VectorXd* dual_solution,
                         const Eigen::VectorXd* dual_gradient,
                         const double norm_weight = 1.0)
      : qp_(*qp),
        dual_solution_(*dual_solution),
        dual_gradient_(*dual_gradient),
        norm_weight_(norm_weight) {}
  double Objective(int64_t index) const {
    // The objective is negated because the trust region problem objective is
    // minimize, but for the dual problem we want to maximize the gradient.
    return -dual_gradient_[index];
  }
  double LowerBound(int64_t index) const {
    return std::isfinite(qp_.constraint_upper_bounds[index])
               ? -std::numeric_limits<double>::infinity()
               : 0.0;
  }
  double UpperBound(int64_t index) const {
    return std::isfinite(qp_.constraint_lower_bounds[index])
               ? std::numeric_limits<double>::infinity()
               : 0.0;
  }
  double CenterPoint(int64_t index) const { return dual_solution_[index]; }
  double NormWeight(int64_t index) const { return norm_weight_; }

 private:
  const QuadraticProgram& qp_;
  const Eigen::VectorXd& dual_solution_;
  const Eigen::VectorXd& dual_gradient_;
  const double norm_weight_;
};

}  // namespace internal

}  // namespace operations_research::pdlp

#endif  // PDLP_TRUST_REGION_H_
