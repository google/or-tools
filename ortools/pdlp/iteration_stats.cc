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

#include "ortools/pdlp/iteration_stats.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <random>
#include <utility>
#include <vector>

#include "Eigen/Core"
#include "Eigen/SparseCore"
#include "absl/log/check.h"
#include "absl/random/distributions.h"
#include "ortools/base/mathutil.h"
#include "ortools/pdlp/quadratic_program.h"
#include "ortools/pdlp/sharded_quadratic_program.h"
#include "ortools/pdlp/sharder.h"
#include "ortools/pdlp/solve_log.pb.h"
#include "ortools/pdlp/solvers.pb.h"

namespace operations_research::pdlp {
namespace {

using ::Eigen::VectorXd;

// `ResidualNorms` contains measures of the infeasibility of a primal or dual
// solution. `objective_correction` is the (additive) adjustment to the
// objective function from the reduced costs. `objective_full_correction` is the
// (additive) adjustment to the objective function if all dual residuals were
// set to zero, while `l_inf_residual`, `l_2_residual`, and
// `l_inf_componentwise_residual` are the L_infinity, L_2, and L_infinity
// (componentwise) norms of the residuals (portions of the primal gradient not
// included in the reduced costs).
struct ResidualNorms {
  double objective_correction;
  double objective_full_correction;
  double l_inf_residual;
  double l_2_residual;
  double l_inf_componentwise_residual;
};

// Computes norms of the primal residual infeasibilities (b - A x) of the
// unscaled problem. Note the primal residuals of the unscaled problem are equal
// to those of the scaled problem divided by `row_scaling_vec`. Does not perform
// any corrections (so the returned `.objective_correction == 0` and
// `.objective_full_correction == 0`). `sharded_qp` is assumed to be the scaled
// problem. If `use_homogeneous_constraint_bounds` is set to true the residuals
// are computed with all finite bounds mapped to zero.
// NOTE: `componentwise_residual_offset` only affects the value of
// `l_inf_componentwise_residual` in the returned `ResidualNorms`.
ResidualNorms PrimalResidualNorms(
    const ShardedQuadraticProgram& sharded_qp, const VectorXd& row_scaling_vec,
    const VectorXd& scaled_primal_solution,
    const double componentwise_residual_offset,
    bool use_homogeneous_constraint_bounds = false) {
  const QuadraticProgram& qp = sharded_qp.Qp();
  CHECK_EQ(row_scaling_vec.size(), sharded_qp.DualSize());
  CHECK_EQ(scaled_primal_solution.size(), sharded_qp.PrimalSize());

  VectorXd primal_product = TransposedMatrixVectorProduct(
      sharded_qp.TransposedConstraintMatrix(), scaled_primal_solution,
      sharded_qp.TransposedConstraintMatrixSharder());
  VectorXd local_l_inf_residual(sharded_qp.DualSharder().NumShards());
  VectorXd local_sumsq_residual(sharded_qp.DualSharder().NumShards());
  VectorXd local_l_inf_componentwise_residual(
      sharded_qp.DualSharder().NumShards());
  sharded_qp.DualSharder().ParallelForEachShard(
      [&](const Sharder::Shard& shard) {
        const auto lower_bound_shard = shard(qp.constraint_lower_bounds);
        const auto upper_bound_shard = shard(qp.constraint_upper_bounds);
        const auto row_scaling_shard = shard(row_scaling_vec);
        const auto primal_product_shard = shard(primal_product);
        double l_inf_residual = 0.0;
        double sumsq_residual = 0.0;
        double l_inf_componentwise_residual = 0.0;
        for (int64_t i = 0; i < primal_product_shard.size(); ++i) {
          const double upper_bound = (use_homogeneous_constraint_bounds &&
                                      std::isfinite(upper_bound_shard[i]))
                                         ? 0.0
                                         : upper_bound_shard[i];
          const double lower_bound = (use_homogeneous_constraint_bounds &&
                                      std::isfinite(lower_bound_shard[i]))
                                         ? 0.0
                                         : lower_bound_shard[i];
          double scaled_residual = 0.0;
          double residual_bound = 0.0;
          if (primal_product_shard[i] > upper_bound) {
            scaled_residual = primal_product_shard[i] - upper_bound;
            residual_bound = upper_bound;
          } else if (primal_product_shard[i] < lower_bound) {
            scaled_residual = lower_bound - primal_product_shard[i];
            residual_bound = lower_bound;
          }
          const double residual = scaled_residual / row_scaling_shard[i];
          l_inf_residual = std::max(l_inf_residual, residual);
          sumsq_residual += residual * residual;
          // Special case: ignore `residual` if == 0, to avoid NaN if offset and
          // bound are both zero.
          if (residual > 0.0) {
            l_inf_componentwise_residual = std::max(
                l_inf_componentwise_residual,
                residual / (componentwise_residual_offset +
                            std::abs(residual_bound / row_scaling_shard[i])));
          }
        }
        local_l_inf_residual[shard.Index()] = l_inf_residual;
        local_sumsq_residual[shard.Index()] = sumsq_residual;
        local_l_inf_componentwise_residual[shard.Index()] =
            l_inf_componentwise_residual;
      });
  return ResidualNorms{
      .objective_correction = 0.0,
      .objective_full_correction = 0.0,
      .l_inf_residual = local_l_inf_residual.lpNorm<Eigen::Infinity>(),
      .l_2_residual = std::sqrt(local_sumsq_residual.sum()),
      .l_inf_componentwise_residual =
          local_l_inf_componentwise_residual.lpNorm<Eigen::Infinity>(),
  };
}

bool TreatVariableBoundAsFinite(const PrimalDualHybridGradientParams& params,
                                double primal_value, double bound) {
  if (params.handle_some_primal_gradients_on_finite_bounds_as_residuals()) {
    // Note that this test is always false if `bound` is infinite.
    return std::abs(primal_value - bound) <= std::abs(primal_value);
  } else {
    return std::isfinite(bound);
  }
}

struct VariableBounds {
  double lower_bound;
  double upper_bound;
};

VariableBounds EffectiveVariableBounds(
    const PrimalDualHybridGradientParams& params, double primal_value,
    double lower_bound, double upper_bound) {
  return {.lower_bound =
              TreatVariableBoundAsFinite(params, primal_value, lower_bound)
                  ? lower_bound
                  : -std::numeric_limits<double>::infinity(),
          .upper_bound =
              TreatVariableBoundAsFinite(params, primal_value, upper_bound)
                  ? upper_bound
                  : std::numeric_limits<double>::infinity()};
}

double VariableBoundForDualObjective(double primal_gradient,
                                     const VariableBounds& bounds) {
  const double primary_bound =
      primal_gradient >= 0.0 ? bounds.lower_bound : bounds.upper_bound;
  const double secondary_bound =
      primal_gradient >= 0.0 ? bounds.upper_bound : bounds.lower_bound;
  if (std::isfinite(primary_bound)) {
    return primary_bound;
  } else if (std::isfinite(secondary_bound)) {
    return secondary_bound;
  } else {
    return 0.0;
  }
}

// Computes norms of the dual residuals and reduced costs of the unscaled
// problem. Note the primal gradient of the unscaled problem is equal to
// `scaled_primal_gradient` divided by `col_scaling_vec`. `sharded_qp` is
// assumed to be the scaled problem. See
// https://developers.google.com/optimization/lp/pdlp_math and the documentation
// for `PrimalDualHybridGradientParams::
// handle_some_primal_gradients_on_finite_bounds_as_residuals` for details and
// notation.
// NOTE: `componentwise_residual_offset` only affects the value of
// `l_inf_componentwise_residual` in the returned `ResidualNorms`.
ResidualNorms DualResidualNorms(const PrimalDualHybridGradientParams& params,
                                const ShardedQuadraticProgram& sharded_qp,
                                const VectorXd& col_scaling_vec,
                                const VectorXd& scaled_primal_solution,
                                const VectorXd& scaled_primal_gradient,
                                const double componentwise_residual_offset) {
  const QuadraticProgram& qp = sharded_qp.Qp();
  CHECK_EQ(col_scaling_vec.size(), sharded_qp.PrimalSize());
  CHECK_EQ(scaled_primal_gradient.size(), sharded_qp.PrimalSize());
  VectorXd local_dual_correction(sharded_qp.PrimalSharder().NumShards());
  VectorXd local_dual_full_correction(sharded_qp.PrimalSharder().NumShards());
  VectorXd local_l_inf_residual(sharded_qp.PrimalSharder().NumShards());
  VectorXd local_sumsq_residual(sharded_qp.PrimalSharder().NumShards());
  VectorXd local_l_inf_componentwise_residual(
      sharded_qp.PrimalSharder().NumShards());
  sharded_qp.PrimalSharder().ParallelForEachShard(
      [&](const Sharder::Shard& shard) {
        const auto lower_bound_shard = shard(qp.variable_lower_bounds);
        const auto upper_bound_shard = shard(qp.variable_upper_bounds);
        const auto primal_gradient_shard = shard(scaled_primal_gradient);
        const auto col_scaling_shard = shard(col_scaling_vec);
        const auto primal_solution_shard = shard(scaled_primal_solution);
        const auto objective_shard = shard(qp.objective_vector);
        double dual_correction = 0.0;
        double dual_full_correction = 0.0;
        double l_inf_residual = 0.0;
        double sumsq_residual = 0.0;
        double l_inf_componentwise_residual = 0.0;
        for (int64_t i = 0; i < primal_gradient_shard.size(); ++i) {
          // The corrections use the scaled values because
          // unscaled_lower_bound = lower_bound * scale and
          // unscaled_primal_gradient = primal_gradient / scale, so the scales
          // cancel out.
          if (primal_gradient_shard[i] == 0.0) continue;
          const double upper_bound = upper_bound_shard[i];
          const double lower_bound = lower_bound_shard[i];
          const double bound_for_rc =
              primal_gradient_shard[i] > 0.0 ? lower_bound : upper_bound;
          dual_full_correction += bound_for_rc * primal_gradient_shard[i];
          VariableBounds effective_bounds = EffectiveVariableBounds(
              params, primal_solution_shard[i], lower_bound, upper_bound);
          // The dual correction (using the appropriate bound) is applied even
          // if the gradient is handled as a residual, so that the dual
          // objective is convex.
          dual_correction += VariableBoundForDualObjective(
                                 primal_gradient_shard[i], effective_bounds) *
                             primal_gradient_shard[i];
          const double effective_bound_for_residual =
              primal_gradient_shard[i] > 0.0 ? effective_bounds.lower_bound
                                             : effective_bounds.upper_bound;
          if (std::isinf(effective_bound_for_residual)) {
            const double scaled_residual = std::abs(primal_gradient_shard[i]);
            const double residual = scaled_residual / col_scaling_shard[i];
            l_inf_residual = std::max(l_inf_residual, residual);
            sumsq_residual += residual * residual;
            // Special case: ignore `residual` if == 0, to avoid NaN if offset
            // and objective are both zero.
            if (residual > 0.0) {
              l_inf_componentwise_residual = std::max(
                  l_inf_componentwise_residual,
                  residual /
                      (componentwise_residual_offset +
                       std::abs(objective_shard[i] / col_scaling_shard[i])));
            }
          }
        }
        local_dual_correction[shard.Index()] = dual_correction;
        local_dual_full_correction[shard.Index()] = dual_full_correction;
        local_l_inf_residual[shard.Index()] = l_inf_residual;
        local_sumsq_residual[shard.Index()] = sumsq_residual;
        local_l_inf_componentwise_residual[shard.Index()] =
            l_inf_componentwise_residual;
      });
  return ResidualNorms{
      .objective_correction = local_dual_correction.sum(),
      .objective_full_correction = local_dual_full_correction.sum(),
      .l_inf_residual = local_l_inf_residual.lpNorm<Eigen::Infinity>(),
      .l_2_residual = std::sqrt(local_sumsq_residual.sum()),
      .l_inf_componentwise_residual =
          local_l_inf_componentwise_residual.lpNorm<Eigen::Infinity>(),
  };
}

// Returns Qx.
VectorXd ObjectiveProduct(const ShardedQuadraticProgram& sharded_qp,
                          const VectorXd& primal_solution) {
  CHECK_EQ(primal_solution.size(), sharded_qp.PrimalSize());
  VectorXd result(primal_solution.size());
  if (IsLinearProgram(sharded_qp.Qp())) {
    SetZero(sharded_qp.PrimalSharder(), result);
  } else {
    sharded_qp.PrimalSharder().ParallelForEachShard(
        [&](const Sharder::Shard& shard) {
          shard(result) =
              shard(*sharded_qp.Qp().objective_matrix) * shard(primal_solution);
        });
  }
  return result;
}

// Returns 1/2 x^T Q x (the quadratic term in the objective).
double QuadraticObjective(const ShardedQuadraticProgram& sharded_qp,
                          const VectorXd& primal_solution,
                          const VectorXd& objective_product) {
  CHECK_EQ(primal_solution.size(), sharded_qp.PrimalSize());
  CHECK_EQ(objective_product.size(), sharded_qp.PrimalSize());
  return 0.5 *
         Dot(objective_product, primal_solution, sharded_qp.PrimalSharder());
}

// Returns `objective_product` + c − A^T y when `use_zero_primal_objective` is
// false, and returns − A^T y when `use_zero_primal_objective` is true.
// `objective_product` is passed by value, and modified in place.
VectorXd PrimalGradientFromObjectiveProduct(
    const ShardedQuadraticProgram& sharded_qp, const VectorXd& dual_solution,
    VectorXd objective_product, bool use_zero_primal_objective = false) {
  const QuadraticProgram& qp = sharded_qp.Qp();
  CHECK_EQ(dual_solution.size(), sharded_qp.DualSize());
  CHECK_EQ(objective_product.size(), sharded_qp.PrimalSize());

  // Note that this modifies `objective_product`, replacing its entries with
  // the primal gradient.
  sharded_qp.ConstraintMatrixSharder().ParallelForEachShard(
      [&](const Sharder::Shard& shard) {
        if (use_zero_primal_objective) {
          shard(objective_product) =
              -shard(qp.constraint_matrix).transpose() * dual_solution;
        } else {
          shard(objective_product) +=
              shard(qp.objective_vector) -
              shard(qp.constraint_matrix).transpose() * dual_solution;
        }
      });
  return objective_product;
}

// Returns the value of y term in the objective of the dual problem, that is,
// (l^c)^T[y]_+ − (u^c)^T[y]_− in the dual objective from
// https://developers.google.com/optimization/lp/pdlp_math.
double DualObjectiveBoundsTerm(const ShardedQuadraticProgram& sharded_qp,
                               const VectorXd& dual_solution) {
  const QuadraticProgram& qp = sharded_qp.Qp();
  return sharded_qp.DualSharder().ParallelSumOverShards(
      [&](const Sharder::Shard& shard) {
        // This assumes that the dual variables are feasible, that is, that
        // the term corresponding to the "y" variables in the dual objective
        // in https://developers.google.com/optimization/lp/pdlp_math is finite.
        const auto lower_bound_shard = shard(qp.constraint_lower_bounds);
        const auto upper_bound_shard = shard(qp.constraint_upper_bounds);
        const auto dual_shard = shard(dual_solution);
        // Can't use `.dot(.cwiseMin(...))` because that gives 0 * inf = NaN.
        double sum = 0.0;
        for (int64_t i = 0; i < dual_shard.size(); ++i) {
          if (dual_shard[i] > 0.0) {
            sum += lower_bound_shard[i] * dual_shard[i];
          } else if (dual_shard[i] < 0.0) {
            sum += upper_bound_shard[i] * dual_shard[i];
          }
        }
        return sum;
      });
}

// Computes the projection of `vector` onto a pseudo-random vector determined
// by `seed_generator`. `seed_generator` is used as the source of a random seed
// for each shard's portion of the vector.
double RandomProjection(const VectorXd& vector, const Sharder& sharder,
                        std::mt19937& seed_generator) {
  std::vector<std::mt19937> shard_seeds;
  shard_seeds.reserve(sharder.NumShards());
  for (int shard = 0; shard < sharder.NumShards(); ++shard) {
    shard_seeds.emplace_back((seed_generator)());
  }
  // Computes `vector` * gaussian_random_vector and ||gaussian_random_vector||^2
  // to normalize by afterwards.
  VectorXd dot_product(sharder.NumShards());
  VectorXd gaussian_norm_squared(sharder.NumShards());
  sharder.ParallelForEachShard([&](const Sharder::Shard& shard) {
    const auto vector_shard = shard(vector);
    double shard_dot_product = 0.0;
    double shard_norm_squared = 0.0;
    std::mt19937 random{shard_seeds[shard.Index()]};
    for (int64_t i = 0; i < vector_shard.size(); ++i) {
      const double projection_element = absl::Gaussian(random, 0.0, 1.0);
      shard_dot_product += projection_element * vector_shard[i];
      shard_norm_squared += MathUtil::Square(projection_element);
    }
    dot_product[shard.Index()] = shard_dot_product;
    gaussian_norm_squared[shard.Index()] = shard_norm_squared;
  });
  return dot_product.sum() / std::sqrt(gaussian_norm_squared.sum());
}
}  // namespace

ConvergenceInformation ComputeConvergenceInformation(
    const PrimalDualHybridGradientParams& params,
    const ShardedQuadraticProgram& scaled_sharded_qp,
    const Eigen::VectorXd& col_scaling_vec,
    const Eigen::VectorXd& row_scaling_vec,
    const Eigen::VectorXd& scaled_primal_solution,
    const Eigen::VectorXd& scaled_dual_solution,
    const double componentwise_primal_residual_offset,
    const double componentwise_dual_residual_offset, PointType candidate_type) {
  const QuadraticProgram& qp = scaled_sharded_qp.Qp();
  CHECK_EQ(col_scaling_vec.size(), scaled_sharded_qp.PrimalSize());
  CHECK_EQ(row_scaling_vec.size(), scaled_sharded_qp.DualSize());
  CHECK_EQ(scaled_primal_solution.size(), scaled_sharded_qp.PrimalSize());
  CHECK_EQ(scaled_dual_solution.size(), scaled_sharded_qp.DualSize());

  // See https://developers.google.com/optimization/lp/pdlp_math#rescaling for
  // notes describing the connection between the scaled and unscaled problem.

  ConvergenceInformation result;
  ResidualNorms primal_residuals = PrimalResidualNorms(
      scaled_sharded_qp, row_scaling_vec, scaled_primal_solution,
      componentwise_primal_residual_offset);
  result.set_l_inf_primal_residual(primal_residuals.l_inf_residual);
  result.set_l2_primal_residual(primal_residuals.l_2_residual);
  result.set_l_inf_componentwise_primal_residual(
      primal_residuals.l_inf_componentwise_residual);

  result.set_l_inf_primal_variable(
      ScaledLInfNorm(scaled_primal_solution, col_scaling_vec,
                     scaled_sharded_qp.PrimalSharder()));
  result.set_l2_primal_variable(ScaledNorm(scaled_primal_solution,
                                           col_scaling_vec,
                                           scaled_sharded_qp.PrimalSharder()));
  result.set_l_inf_dual_variable(ScaledLInfNorm(
      scaled_dual_solution, row_scaling_vec, scaled_sharded_qp.DualSharder()));
  result.set_l2_dual_variable(ScaledNorm(scaled_dual_solution, row_scaling_vec,
                                         scaled_sharded_qp.DualSharder()));

  VectorXd scaled_objective_product =
      ObjectiveProduct(scaled_sharded_qp, scaled_primal_solution);
  const double quadratic_objective = QuadraticObjective(
      scaled_sharded_qp, scaled_primal_solution, scaled_objective_product);
  VectorXd scaled_primal_gradient = PrimalGradientFromObjectiveProduct(
      scaled_sharded_qp, scaled_dual_solution,
      std::move(scaled_objective_product));
  result.set_primal_objective(qp.ApplyObjectiveScalingAndOffset(
      quadratic_objective + Dot(qp.objective_vector, scaled_primal_solution,
                                scaled_sharded_qp.PrimalSharder())));

  // This is the dual objective from
  // https://developers.google.com/optimization/lp/pdlp_math minus the last term
  // (involving r). All scaling terms cancel out.
  const double dual_objective_piece =
      -quadratic_objective +
      DualObjectiveBoundsTerm(scaled_sharded_qp, scaled_dual_solution);

  ResidualNorms dual_residuals = DualResidualNorms(
      params, scaled_sharded_qp, col_scaling_vec, scaled_primal_solution,
      scaled_primal_gradient, componentwise_dual_residual_offset);
  result.set_dual_objective(qp.ApplyObjectiveScalingAndOffset(
      dual_objective_piece + dual_residuals.objective_correction));
  result.set_corrected_dual_objective(qp.ApplyObjectiveScalingAndOffset(
      dual_objective_piece + dual_residuals.objective_full_correction));
  result.set_l_inf_dual_residual(dual_residuals.l_inf_residual);
  result.set_l2_dual_residual(dual_residuals.l_2_residual);
  result.set_l_inf_componentwise_dual_residual(
      dual_residuals.l_inf_componentwise_residual);

  result.set_candidate_type(candidate_type);
  return result;
}

namespace {

double PrimalRayMaxSignViolation(const ShardedQuadraticProgram& sharded_qp,
                                 const VectorXd& col_scaling_vec,
                                 const VectorXd& scaled_primal_ray) {
  VectorXd primal_ray_local_max_sign_violation(
      sharded_qp.PrimalSharder().NumShards());
  sharded_qp.PrimalSharder().ParallelForEachShard(
      [&](const Sharder::Shard& shard) {
        const auto lower_bound_shard =
            shard(sharded_qp.Qp().variable_lower_bounds);
        const auto upper_bound_shard =
            shard(sharded_qp.Qp().variable_upper_bounds);
        const auto ray_shard = shard(scaled_primal_ray);
        const auto scale_shard = shard(col_scaling_vec);
        double local_max = 0.0;
        for (int64_t i = 0; i < ray_shard.size(); ++i) {
          if (std::isfinite(lower_bound_shard[i])) {
            local_max = std::max(local_max, -ray_shard[i] * scale_shard[i]);
          }
          if (std::isfinite(upper_bound_shard[i])) {
            local_max = std::max(local_max, ray_shard[i] * scale_shard[i]);
          }
        }
        primal_ray_local_max_sign_violation[shard.Index()] = local_max;
      });
  return primal_ray_local_max_sign_violation.lpNorm<Eigen::Infinity>();
}

}  // namespace

InfeasibilityInformation ComputeInfeasibilityInformation(
    const PrimalDualHybridGradientParams& params,
    const ShardedQuadraticProgram& scaled_sharded_qp,
    const Eigen::VectorXd& col_scaling_vec,
    const Eigen::VectorXd& row_scaling_vec,
    const Eigen::VectorXd& scaled_primal_ray,
    const Eigen::VectorXd& scaled_dual_ray,
    const Eigen::VectorXd& primal_solution_for_residual_tests,
    PointType candidate_type) {
  const QuadraticProgram& qp = scaled_sharded_qp.Qp();
  CHECK_EQ(col_scaling_vec.size(), scaled_sharded_qp.PrimalSize());
  CHECK_EQ(row_scaling_vec.size(), scaled_sharded_qp.DualSize());
  CHECK_EQ(scaled_primal_ray.size(), scaled_sharded_qp.PrimalSize());
  CHECK_EQ(scaled_dual_ray.size(), scaled_sharded_qp.DualSize());

  double l_inf_primal = ScaledLInfNorm(scaled_primal_ray, col_scaling_vec,
                                       scaled_sharded_qp.PrimalSharder());
  double l_inf_dual = ScaledLInfNorm(scaled_dual_ray, row_scaling_vec,
                                     scaled_sharded_qp.DualSharder());
  InfeasibilityInformation result;
  // Compute primal infeasibility information.
  VectorXd scaled_primal_gradient = PrimalGradientFromObjectiveProduct(
      scaled_sharded_qp, scaled_dual_ray,
      ZeroVector(scaled_sharded_qp.PrimalSharder()),
      /*use_zero_primal_objective=*/true);
  // We don't use `dual_residuals.l_inf_componentwise_residual`, so don't need
  // to set `componentwise_residual_offset` to a meaningful value.
  ResidualNorms dual_residuals = DualResidualNorms(
      params, scaled_sharded_qp, col_scaling_vec,
      primal_solution_for_residual_tests, scaled_primal_gradient,
      /*componentwise_residual_offset=*/0.0);

  double dual_ray_objective =
      DualObjectiveBoundsTerm(scaled_sharded_qp, scaled_dual_ray) +
      dual_residuals.objective_correction;
  if (l_inf_dual > 0) {
    result.set_dual_ray_objective(dual_ray_objective / l_inf_dual);
    result.set_max_dual_ray_infeasibility(dual_residuals.l_inf_residual /
                                          l_inf_dual);
  } else {
    result.set_dual_ray_objective(0.0);
    result.set_max_dual_ray_infeasibility(0.0);
  }

  // Compute dual infeasibility information. We don't use
  // `primal_residuals.l_inf_componentwise_residual`, so don't need to set
  // `componentwise_residual_offset` to a meaningful value.
  ResidualNorms primal_residuals =
      PrimalResidualNorms(scaled_sharded_qp, row_scaling_vec, scaled_primal_ray,
                          /*componentwise_residual_offset=*/0.0,
                          /*use_homogeneous_constraint_bounds=*/true);

  // The primal ray should have been projected onto the feasibility bounds, so
  // that it has the correct signs.
  DCHECK_EQ(PrimalRayMaxSignViolation(scaled_sharded_qp, col_scaling_vec,
                                      scaled_primal_ray),
            0.0);

  if (l_inf_primal > 0.0) {
    VectorXd scaled_objective_product =
        ObjectiveProduct(scaled_sharded_qp, scaled_primal_ray);
    result.set_primal_ray_quadratic_norm(
        LInfNorm(scaled_objective_product, scaled_sharded_qp.PrimalSharder()) /
        l_inf_primal);
    result.set_max_primal_ray_infeasibility(primal_residuals.l_inf_residual /
                                            l_inf_primal);
    result.set_primal_ray_linear_objective(
        Dot(scaled_primal_ray, qp.objective_vector,
            scaled_sharded_qp.PrimalSharder()) /
        l_inf_primal);
  } else {
    result.set_primal_ray_quadratic_norm(0.0);
    result.set_max_primal_ray_infeasibility(0.0);
    result.set_primal_ray_linear_objective(0.0);
  }

  result.set_candidate_type(candidate_type);
  return result;
}

ConvergenceInformation ComputeScaledConvergenceInformation(
    const PrimalDualHybridGradientParams& params,
    const ShardedQuadraticProgram& sharded_qp, const VectorXd& primal_solution,
    const VectorXd& dual_solution,
    const double componentwise_primal_residual_offset,
    const double componentwise_dual_residual_offset, PointType candidate_type) {
  return ComputeConvergenceInformation(
      params, sharded_qp, OnesVector(sharded_qp.PrimalSharder()),
      OnesVector(sharded_qp.DualSharder()), primal_solution, dual_solution,
      componentwise_primal_residual_offset, componentwise_dual_residual_offset,
      candidate_type);
}

VectorXd ReducedCosts(const PrimalDualHybridGradientParams& params,
                      const ShardedQuadraticProgram& sharded_qp,
                      const VectorXd& primal_solution,
                      const VectorXd& dual_solution,
                      bool use_zero_primal_objective) {
  VectorXd objective_product;
  if (use_zero_primal_objective) {
    objective_product = ZeroVector(sharded_qp.PrimalSharder());
  } else {
    objective_product = ObjectiveProduct(sharded_qp, primal_solution);
  }
  return PrimalGradientFromObjectiveProduct(sharded_qp, dual_solution,
                                            std::move(objective_product),
                                            use_zero_primal_objective);
}

std::optional<ConvergenceInformation> GetConvergenceInformation(
    const IterationStats& stats, PointType candidate_type) {
  for (const auto& convergence_information : stats.convergence_information()) {
    if (convergence_information.candidate_type() == candidate_type) {
      return convergence_information;
    }
  }
  return std::nullopt;
}

std::optional<InfeasibilityInformation> GetInfeasibilityInformation(
    const IterationStats& stats, PointType candidate_type) {
  for (const auto& infeasibility_information :
       stats.infeasibility_information()) {
    if (infeasibility_information.candidate_type() == candidate_type) {
      return infeasibility_information;
    }
  }
  return std::nullopt;
}

std::optional<PointMetadata> GetPointMetadata(const IterationStats& stats,
                                              const PointType point_type) {
  for (const auto& metadata : stats.point_metadata()) {
    if (metadata.point_type() == point_type) {
      return metadata;
    }
  }
  return std::nullopt;
}

void SetRandomProjections(const ShardedQuadraticProgram& sharded_qp,
                          const Eigen::VectorXd& primal_solution,
                          const Eigen::VectorXd& dual_solution,
                          const std::vector<int>& random_projection_seeds,
                          PointMetadata& metadata) {
  for (const int random_projection_seed : random_projection_seeds) {
    std::mt19937 seed_generator(random_projection_seed);
    metadata.mutable_random_primal_projections()->Add(RandomProjection(
        primal_solution, sharded_qp.PrimalSharder(), seed_generator));
    metadata.mutable_random_dual_projections()->Add(RandomProjection(
        dual_solution, sharded_qp.DualSharder(), seed_generator));
  }
}

}  // namespace operations_research::pdlp
