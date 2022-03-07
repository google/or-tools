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

#include "ortools/pdlp/sharded_optimization_utils.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <numeric>
#include <optional>
#include <random>
#include <utility>
#include <vector>

#include "Eigen/Core"
#include "Eigen/SparseCore"
#include "absl/algorithm/container.h"
#include "absl/random/distributions.h"
#include "ortools/base/logging.h"
#include "ortools/base/mathutil.h"
#include "ortools/pdlp/quadratic_program.h"
#include "ortools/pdlp/sharded_quadratic_program.h"
#include "ortools/pdlp/sharder.h"

namespace operations_research::pdlp {

constexpr double kInfinity = std::numeric_limits<double>::infinity();
using ::Eigen::ColMajor;
using ::Eigen::SparseMatrix;
using ::Eigen::VectorXd;
using ::Eigen::VectorXi;

ShardedWeightedAverage::ShardedWeightedAverage(const Sharder* sharder)
    : sharder_(sharder) {
  average_ = VectorXd::Zero(sharder->NumElements());
}

// We considered the five averaging algorithms M_* listed on the first page of
// https://www.jstor.org/stable/2286154 and the Kahan summation algorithm
// (https://en.wikipedia.org/wiki/Kahan_summation_algorithm). Of these only M_14
// satisfies our desired property that a constant sequence is averaged without
// roundoff while requiring only a single vector be stored. We therefore use
// M_14 (actually a natural weighted generalization, see below).
void ShardedWeightedAverage::Add(const VectorXd& datapoint, double weight) {
  CHECK_GE(weight, 0.0);
  CHECK_EQ(datapoint.size(), average_.size());
  const double weight_ratio = weight / (sum_weights_ + weight);
  sharder_->ParallelForEachShard([&](const Sharder::Shard& shard) {
    shard(average_) += weight_ratio * (shard(datapoint) - shard(average_));
  });
  sum_weights_ += weight;
  ++num_terms_;
}

void ShardedWeightedAverage::Clear() {
  // TODO(user): There may be a performance gain from using the sharder to
  // zero-out the vectors.
  average_.setZero();
  sum_weights_ = 0.0;
  num_terms_ = 0;
}

VectorXd ShardedWeightedAverage::ComputeAverage() const {
  VectorXd result;
  // TODO(user): consider returning a reference to avoid this copy.
  AssignVector(average_, *sharder_, result);
  return result;
}

namespace {

double CombineBounds(const double v1, const double v2,
                     const double infinite_bound_threshold) {
  double max = 0.0;
  if (std::abs(v1) < infinite_bound_threshold) {
    max = std::abs(v1);
  }
  if (std::abs(v2) < infinite_bound_threshold) {
    max = std::max(max, std::abs(v2));
  }
  return max;
}

struct VectorInfo {
  int64_t num_finite = 0;
  int64_t num_nonzero = 0;
  double largest = 0.0;
  double smallest = 0.0;
  double average = 0.0;
  double l2_norm = 0.0;
};

struct InfNormInfo {
  double max_col_norm;
  double min_col_norm;
  double max_row_norm;
  double min_row_norm;
};

// The functions below are used to generate default values for
// QuadraticProgramStats when the underlying program is empty or has no
// constraints.

double MaxOrZero(const VectorXd& vec) {
  if (vec.size() == 0) {
    return 0.0;
  } else if (std::isinf(vec.maxCoeff())) {
    return 0.0;
  } else {
    return vec.maxCoeff();
  }
}

double MinOrZero(const VectorXd& vec) {
  if (vec.size() == 0) {
    return 0.0;
  } else if (std::isinf(vec.minCoeff())) {
    return 0.0;
  } else {
    return vec.minCoeff();
  }
}

VectorInfo ComputeVectorInfo(const Eigen::Ref<const VectorXd>& vec,
                             const Sharder& sharder) {
  VectorXd local_max(sharder.NumShards());
  VectorXd local_min(sharder.NumShards());
  VectorXd local_sum(sharder.NumShards());
  VectorXd local_sum_squared(sharder.NumShards());
  std::vector<int64_t> local_num_nonzero(sharder.NumShards());
  sharder.ParallelForEachShard([&](const Sharder::Shard& shard) {
    const VectorXd shard_abs = shard(vec).cwiseAbs();
    local_max[shard.Index()] = shard_abs.maxCoeff();
    local_min[shard.Index()] = shard_abs.minCoeff();
    local_sum[shard.Index()] = shard_abs.sum();
    local_sum_squared[shard.Index()] = shard_abs.squaredNorm();
    for (double element : shard_abs) {
      if (element != 0) {
        local_num_nonzero[shard.Index()] += 1;
      }
    }
  });
  const int64_t num_elements = vec.size();
  return VectorInfo{
      .num_finite = num_elements,
      .num_nonzero = std::accumulate(local_num_nonzero.begin(),
                                     local_num_nonzero.end(), int64_t{0}),
      .largest = MaxOrZero(local_max),
      .smallest = MinOrZero(local_min),
      .average = (num_elements > 0) ? local_sum.sum() / num_elements : NAN,
      .l2_norm = (num_elements > 0) ? std::sqrt(local_sum_squared.sum()) : 0.0};
}

VectorInfo VariableBoundGapInfo(const VectorXd& lower_bounds,
                                const VectorXd& upper_bounds,
                                const Sharder& sharder) {
  VectorXd local_max(sharder.NumShards());
  VectorXd local_min(sharder.NumShards());
  VectorXd local_sum(sharder.NumShards());
  std::vector<int64_t> local_num_finite(sharder.NumShards());
  std::vector<int64_t> local_num_nonzero(sharder.NumShards());
  sharder.ParallelForEachShard([&](const Sharder::Shard& shard) {
    const auto gap_shard = shard(upper_bounds) - shard(lower_bounds);
    double max = -kInfinity;
    double min = kInfinity;
    double sum = 0.0;
    int64_t num_finite = 0;
    int64_t num_nonzero = 0;
    for (int64_t i = 0; i < gap_shard.size(); ++i) {
      if (std::isfinite(gap_shard[i])) {
        max = std::max(max, gap_shard[i]);
        min = std::min(min, gap_shard[i]);
        sum += gap_shard[i];
        num_finite += 1;
        if (gap_shard[i] != 0) {
          num_nonzero += 1;
        }
      }
    }
    local_max[shard.Index()] = max;
    local_min[shard.Index()] = min;
    local_sum[shard.Index()] = sum;
    local_num_finite[shard.Index()] = num_finite;
    local_num_nonzero[shard.Index()] = num_nonzero;
  });
  // If an empty model was given, local_sum could be an empty vector,
  // in which case calling .sum() directly would crash.
  const int64_t num_finite = std::accumulate(
      local_num_finite.begin(), local_num_finite.end(), int64_t{0});
  return VectorInfo{
      .num_finite = num_finite,
      .num_nonzero = std::accumulate(local_num_nonzero.begin(),
                                     local_num_nonzero.end(), int64_t{0}),
      .largest = MaxOrZero(local_max),
      .smallest = MinOrZero(local_min),
      .average = (num_finite > 0) ? local_sum.sum() / num_finite : NAN};
}

VectorInfo MatrixAbsElementInfo(
    const SparseMatrix<double, ColMajor, int64_t>& matrix,
    const Sharder& sharder) {
  VectorXd local_max(sharder.NumShards());
  VectorXd local_min(sharder.NumShards());
  VectorXd local_sum(sharder.NumShards());
  sharder.ParallelForEachShard([&](const Sharder::Shard& shard) {
    const auto matrix_shard = shard(matrix);
    double max = -kInfinity;
    double min = kInfinity;
    double sum = 0.0;
    for (int64_t col_idx = 0; col_idx < matrix_shard.outerSize(); ++col_idx) {
      for (decltype(matrix_shard)::InnerIterator it(matrix_shard, col_idx); it;
           ++it) {
        max = std::max(max, std::abs(it.value()));
        min = std::min(min, std::abs(it.value()));
        sum += std::abs(it.value());
      }
    }
    local_max[shard.Index()] = max;
    local_min[shard.Index()] = min;
    local_sum[shard.Index()] = sum;
  });
  const int64_t num_nonzeros = matrix.nonZeros();
  return VectorInfo{
      .num_finite = num_nonzeros,
      .largest = MaxOrZero(local_max),
      .smallest = MinOrZero(local_min),
      .average = (num_nonzeros > 0) ? local_sum.sum() / num_nonzeros : NAN};
}

VectorInfo CombinedBoundsInfo(const VectorXd& rhs_upper_bounds,
                              const VectorXd& rhs_lower_bounds,
                              const Sharder& sharder,
                              const double infinite_bound_threshold =
                                  std::numeric_limits<double>::infinity()) {
  VectorXd local_max(sharder.NumShards());
  VectorXd local_min(sharder.NumShards());
  VectorXd local_sum(sharder.NumShards());
  VectorXd local_sum_squared(sharder.NumShards());
  sharder.ParallelForEachShard([&](const Sharder::Shard& shard) {
    const auto lb_shard = shard(rhs_lower_bounds);
    const auto ub_shard = shard(rhs_upper_bounds);
    double max = -kInfinity;
    double min = kInfinity;
    double sum = 0.0;
    double sum_squared = 0.0;
    for (int64_t i = 0; i < lb_shard.size(); ++i) {
      const double combined =
          CombineBounds(ub_shard[i], lb_shard[i], infinite_bound_threshold);
      max = std::max(max, combined);
      min = std::min(min, combined);
      sum += combined;
      sum_squared += combined * combined;
    }
    local_max[shard.Index()] = max;
    local_min[shard.Index()] = min;
    local_sum[shard.Index()] = sum;
    local_sum_squared[shard.Index()] = sum_squared;
  });
  const int num_constraints = rhs_lower_bounds.size();
  return VectorInfo{
      .num_finite = num_constraints,
      .largest = MaxOrZero(local_max),
      .smallest = MinOrZero(local_min),
      .average =
          (num_constraints > 0) ? local_sum.sum() / num_constraints : NAN,
      .l2_norm =
          (num_constraints > 0) ? std::sqrt(local_sum_squared.sum()) : 0.0};
}

InfNormInfo ConstraintMatrixRowColInfo(
    const SparseMatrix<double, ColMajor, int64_t>& constraint_matrix,
    const SparseMatrix<double, ColMajor, int64_t>& constraint_matrix_transpose,
    const Sharder& matrix_sharder, const Sharder& matrix_transpose_sharder) {
  VectorXd col_norms = ScaledColLInfNorm(
      constraint_matrix,
      /*row_scaling_vec=*/VectorXd::Ones(constraint_matrix.rows()),
      /*col_scaling_vec=*/VectorXd::Ones(constraint_matrix.cols()),
      matrix_sharder);
  VectorXd row_norms =
      ScaledColLInfNorm(constraint_matrix_transpose,
                        VectorXd::Ones(constraint_matrix_transpose.rows()),
                        VectorXd::Ones(constraint_matrix_transpose.cols()),
                        matrix_transpose_sharder);
  return InfNormInfo{.max_col_norm = MaxOrZero(col_norms),
                     .min_col_norm = MinOrZero(col_norms),
                     .max_row_norm = MaxOrZero(row_norms),
                     .min_row_norm = MinOrZero(row_norms)};
}
}  // namespace

QuadraticProgramStats ComputeStats(
    const ShardedQuadraticProgram& qp,
    const double infinite_constraint_bound_threshold) {
  // Caution: if the constraint matrix is empty, elementwise operations
  // (like .coeffs().maxCoeff() or .minCoeff()) will fail.
  InfNormInfo cons_matrix_norm_info = ConstraintMatrixRowColInfo(
      qp.Qp().constraint_matrix, qp.TransposedConstraintMatrix(),
      qp.ConstraintMatrixSharder(), qp.TransposedConstraintMatrixSharder());
  VectorInfo cons_matrix_info = MatrixAbsElementInfo(
      qp.Qp().constraint_matrix, qp.ConstraintMatrixSharder());
  VectorInfo combined_bounds_info = CombinedBoundsInfo(
      qp.Qp().constraint_lower_bounds, qp.Qp().constraint_upper_bounds,
      qp.DualSharder(), infinite_constraint_bound_threshold);
  VectorInfo obj_vec_info =
      ComputeVectorInfo(qp.Qp().objective_vector, qp.PrimalSharder());
  VectorInfo gaps_info =
      VariableBoundGapInfo(qp.Qp().variable_lower_bounds,
                           qp.Qp().variable_upper_bounds, qp.PrimalSharder());
  QuadraticProgramStats program_stats;
  program_stats.set_num_variables(qp.PrimalSize());
  program_stats.set_num_constraints(qp.DualSize());
  program_stats.set_constraint_matrix_col_min_l_inf_norm(
      cons_matrix_norm_info.min_col_norm);
  program_stats.set_constraint_matrix_row_min_l_inf_norm(
      cons_matrix_norm_info.min_row_norm);
  program_stats.set_constraint_matrix_num_nonzeros(cons_matrix_info.num_finite);
  program_stats.set_constraint_matrix_abs_max(cons_matrix_info.largest);
  program_stats.set_constraint_matrix_abs_min(cons_matrix_info.smallest);
  program_stats.set_constraint_matrix_abs_avg(cons_matrix_info.average);
  program_stats.set_combined_bounds_max(combined_bounds_info.largest);
  program_stats.set_combined_bounds_min(combined_bounds_info.smallest);
  program_stats.set_combined_bounds_avg(combined_bounds_info.average);
  program_stats.set_combined_bounds_l2_norm(combined_bounds_info.l2_norm);
  program_stats.set_variable_bound_gaps_num_finite(gaps_info.num_finite);
  program_stats.set_variable_bound_gaps_max(gaps_info.largest);
  program_stats.set_variable_bound_gaps_min(gaps_info.smallest);
  program_stats.set_variable_bound_gaps_avg(gaps_info.average);
  program_stats.set_objective_vector_abs_max(obj_vec_info.largest);
  program_stats.set_objective_vector_abs_min(obj_vec_info.smallest);
  program_stats.set_objective_vector_abs_avg(obj_vec_info.average);
  program_stats.set_objective_vector_l2_norm(obj_vec_info.l2_norm);
  if (IsLinearProgram(qp.Qp())) {
    program_stats.set_objective_matrix_num_nonzeros(0);
    program_stats.set_objective_matrix_abs_max(0);
    program_stats.set_objective_matrix_abs_min(0);
    program_stats.set_objective_matrix_abs_avg(NAN);
  } else {
    VectorInfo obj_matrix_info = ComputeVectorInfo(
        qp.Qp().objective_matrix->diagonal(), qp.PrimalSharder());
    program_stats.set_objective_matrix_num_nonzeros(
        obj_matrix_info.num_nonzero);
    program_stats.set_objective_matrix_abs_max(obj_matrix_info.largest);
    program_stats.set_objective_matrix_abs_min(obj_matrix_info.smallest);
    program_stats.set_objective_matrix_abs_avg(obj_matrix_info.average);
  }
  return program_stats;
}

namespace {

enum class ScalingNorm { kL2, kLInf };

// Divides the vector (componentwise) by the square root of the divisor,
// updating the vector in-place. If a component of the divisor is equal to zero,
// leaves the component of the vector unchanged. The Sharder should have the
// same size as the vector. For best performance the Sharder should have been
// created with the Sharder(int64_t, int, ThreadPool*) constructor.
void DivideBySquareRootOfDivisor(const VectorXd& divisor,
                                 const Sharder& sharder, VectorXd& vector) {
  sharder.ParallelForEachShard([&](const Sharder::Shard& shard) {
    auto vec_shard = shard(vector);
    auto divisor_shard = shard(divisor);
    for (int64_t index = 0; index < vec_shard.size(); ++index) {
      if (divisor_shard[index] != 0) {
        vec_shard[index] /= std::sqrt(divisor_shard[index]);
      }
    }
  });
}

void ApplyScalingIterationsForNorm(const ShardedQuadraticProgram& sharded_qp,
                                   const int num_iterations,
                                   const ScalingNorm norm,
                                   VectorXd& row_scaling_vec,
                                   VectorXd& col_scaling_vec) {
  const QuadraticProgram& qp = sharded_qp.Qp();
  const int64_t num_col = qp.constraint_matrix.cols();
  const int64_t num_row = qp.constraint_matrix.rows();
  CHECK_EQ(num_col, col_scaling_vec.size());
  CHECK_EQ(num_row, row_scaling_vec.size());
  for (int i = 0; i < num_iterations; ++i) {
    VectorXd col_norm;
    VectorXd row_norm;
    switch (norm) {
      case ScalingNorm::kL2: {
        col_norm = ScaledColL2Norm(qp.constraint_matrix, row_scaling_vec,
                                   col_scaling_vec,
                                   sharded_qp.ConstraintMatrixSharder());
        row_norm = ScaledColL2Norm(
            sharded_qp.TransposedConstraintMatrix(), col_scaling_vec,
            row_scaling_vec, sharded_qp.TransposedConstraintMatrixSharder());
        break;
      }
      case ScalingNorm::kLInf: {
        col_norm = ScaledColLInfNorm(qp.constraint_matrix, row_scaling_vec,
                                     col_scaling_vec,
                                     sharded_qp.ConstraintMatrixSharder());
        row_norm = ScaledColLInfNorm(
            sharded_qp.TransposedConstraintMatrix(), col_scaling_vec,
            row_scaling_vec, sharded_qp.TransposedConstraintMatrixSharder());
        break;
      }
    }
    DivideBySquareRootOfDivisor(col_norm, sharded_qp.PrimalSharder(),
                                col_scaling_vec);
    DivideBySquareRootOfDivisor(row_norm, sharded_qp.DualSharder(),
                                row_scaling_vec);
  }
}

}  // namespace

void LInfRuizRescaling(const ShardedQuadraticProgram& sharded_qp,
                       const int num_iterations, VectorXd& row_scaling_vec,
                       VectorXd& col_scaling_vec) {
  ApplyScalingIterationsForNorm(sharded_qp, num_iterations, ScalingNorm::kLInf,
                                row_scaling_vec, col_scaling_vec);
}

void L2NormRescaling(const ShardedQuadraticProgram& sharded_qp,
                     VectorXd& row_scaling_vec, VectorXd& col_scaling_vec) {
  ApplyScalingIterationsForNorm(sharded_qp, /*num_iterations=*/1,
                                ScalingNorm::kL2, row_scaling_vec,
                                col_scaling_vec);
}

ScalingVectors ApplyRescaling(const RescalingOptions& rescaling_options,
                              ShardedQuadraticProgram& sharded_qp) {
  ScalingVectors scaling{
      .row_scaling_vec = VectorXd::Ones(sharded_qp.DualSize()),
      .col_scaling_vec = VectorXd::Ones(sharded_qp.PrimalSize())};
  bool do_rescale = false;
  if (rescaling_options.l_inf_ruiz_iterations > 0) {
    do_rescale = true;
    LInfRuizRescaling(sharded_qp, rescaling_options.l_inf_ruiz_iterations,
                      scaling.row_scaling_vec, scaling.col_scaling_vec);
  }
  if (rescaling_options.l2_norm_rescaling) {
    do_rescale = true;
    L2NormRescaling(sharded_qp, scaling.row_scaling_vec,
                    scaling.col_scaling_vec);
  }
  if (do_rescale) {
    sharded_qp.RescaleQuadraticProgram(scaling.col_scaling_vec,
                                       scaling.row_scaling_vec);
  }
  return scaling;
}

LagrangianPart ComputePrimalGradient(const ShardedQuadraticProgram& sharded_qp,
                                     const VectorXd& primal_solution,
                                     const VectorXd& dual_product) {
  LagrangianPart result{.gradient = VectorXd(sharded_qp.PrimalSize())};
  const QuadraticProgram& qp = sharded_qp.Qp();
  VectorXd value_parts(sharded_qp.PrimalSharder().NumShards());
  sharded_qp.PrimalSharder().ParallelForEachShard(
      [&](const Sharder::Shard& shard) {
        if (IsLinearProgram(qp)) {
          shard(result.gradient) =
              shard(qp.objective_vector) - shard(dual_product);
          value_parts[shard.Index()] =
              shard(primal_solution).dot(shard(result.gradient));
        } else {
          // Note: using auto instead of VectorXd for the type of
          // objective_product causes eigen to defer the matrix product until it
          // is used (twice).
          const VectorXd objective_product =
              shard(*qp.objective_matrix) * shard(primal_solution);
          shard(result.gradient) = shard(qp.objective_vector) +
                                   objective_product - shard(dual_product);
          value_parts[shard.Index()] =
              shard(primal_solution)
                  .dot(shard(result.gradient) - 0.5 * objective_product);
        }
      });
  result.value = value_parts.sum();
  return result;
}

double DualSubgradientCoefficient(const double constraint_lower_bound,
                                  const double constraint_upper_bound,
                                  const double dual,
                                  const double primal_product) {
  if (dual < 0.0) {
    return constraint_upper_bound;
  } else if (dual > 0.0) {
    return constraint_lower_bound;
  } else if (std::isfinite(constraint_lower_bound) &&
             std::isfinite(constraint_upper_bound)) {
    if (primal_product < constraint_lower_bound) {
      return constraint_lower_bound;
    } else if (primal_product > constraint_upper_bound) {
      return constraint_upper_bound;
    } else {
      return primal_product;
    }
  } else if (std::isfinite(constraint_lower_bound)) {
    return constraint_lower_bound;
  } else if (std::isfinite(constraint_upper_bound)) {
    return constraint_upper_bound;
  } else {
    return 0.0;
  }
}

LagrangianPart ComputeDualGradient(const ShardedQuadraticProgram& sharded_qp,
                                   const Eigen::VectorXd& dual_solution,
                                   const Eigen::VectorXd& primal_product) {
  LagrangianPart result{.gradient = VectorXd(sharded_qp.DualSize())};
  const QuadraticProgram& qp = sharded_qp.Qp();
  VectorXd value_parts(sharded_qp.DualSharder().NumShards());
  sharded_qp.DualSharder().ParallelForEachShard(
      [&](const Sharder::Shard& shard) {
        auto constraint_lower_bounds = shard(qp.constraint_lower_bounds);
        auto constraint_upper_bounds = shard(qp.constraint_upper_bounds);
        auto dual_solution_shard = shard(dual_solution);
        auto dual_gradient_shard = shard(result.gradient);
        auto primal_product_shard = shard(primal_product);
        double value_sum = 0.0;
        for (int64_t i = 0; i < dual_gradient_shard.size(); ++i) {
          dual_gradient_shard[i] = DualSubgradientCoefficient(
              constraint_lower_bounds[i], constraint_upper_bounds[i],
              dual_solution_shard[i], primal_product_shard[i]);
          value_sum += dual_gradient_shard[i] * dual_solution_shard[i];
        }
        value_parts[shard.Index()] = value_sum;
        dual_gradient_shard -= primal_product_shard;
      });
  result.value = value_parts.sum();
  return result;
}

namespace {

using ::Eigen::ColMajor;
using ::Eigen::SparseMatrix;

// Scales a vector (in-place) to have norm 1, unless it has norm 0 (in which
// case it is left unscaled). Returns the norm of the input vector.
double NormalizeVector(const Sharder& sharder, VectorXd& vector) {
  const double norm = Norm(vector, sharder);
  if (norm != 0.0) {
    sharder.ParallelForEachShard(
        [&](const Sharder::Shard& shard) { shard(vector) /= norm; });
  }
  return norm;
}

// Estimates the probability that the power method, after k iterations, has
// relative error > epsilon.  This is based on Theorem 4.1(a) (on page 13) from
// "Estimating the Largest Eigenvalue by the Power and Lanczos Algorithms with a
// Random Start"
// https://pdfs.semanticscholar.org/2b2e/a941e55e5fa2ee9d8f4ff393c14482051143.pdf
double PowerMethodFailureProbability(int64_t dimension, double epsilon, int k) {
  if (k < 2 || epsilon <= 0.0) {
    // The theorem requires epsilon > 0 and k >= 2.
    return 1.0;
  }
  return std::min(0.824, 0.354 / (epsilon * (k - 1))) * std::sqrt(dimension) *
         std::pow(1.0 - epsilon, k - 0.5);
}

SingularValueAndIterations EstimateMaximumSingularValue(
    const SparseMatrix<double, ColMajor, int64_t>& matrix,
    const SparseMatrix<double, ColMajor, int64_t>& matrix_transpose,
    const std::optional<VectorXd>& active_set_indicator,
    const std::optional<VectorXd>& transpose_active_set_indicator,
    const Sharder& matrix_sharder, const Sharder& matrix_transpose_sharder,
    const Sharder& primal_vector_sharder, const Sharder& dual_vector_sharder,
    const double desired_relative_error, const double failure_probability,
    std::mt19937& mt_generator) {
  // Easy case: matrix is diagonal.
  if (IsDiagonal(matrix, matrix_sharder)) {
    VectorXd local_max(matrix_sharder.NumShards());
    matrix_sharder.ParallelForEachShard([&](const Sharder::Shard& shard) {
      const auto matrix_shard = shard(matrix);
      local_max[shard.Index()] =
          (matrix_shard *
           VectorXd::Ones(matrix_sharder.ShardSize(shard.Index())))
              .lpNorm<Eigen::Infinity>();
    });
    return {.singular_value = local_max.lpNorm<Eigen::Infinity>(),
            .num_iterations = 0,
            .estimated_relative_error = 0.0};
  }
  const int64_t dimension = matrix.cols();
  VectorXd eigenvector(dimension);
  // Even though it will be slower, we initialize eigenvector sequentially so
  // that the result doesn't depend on the number of threads.
  for (double& entry : eigenvector) {
    entry = absl::Gaussian<double>(mt_generator);
  }
  if (active_set_indicator.has_value()) {
    CoefficientWiseProductInPlace(*active_set_indicator, primal_vector_sharder,
                                  eigenvector);
  }
  NormalizeVector(primal_vector_sharder, eigenvector);
  double eigenvalue_estimate = 0.0;

  int num_iterations = 0;
  // The maximum singular value of A is the square root of the maximum
  // eigenvalue of A^T A.  epsilon is the relative error needed for the maximum
  // eigenvalue of A^T A that gives desired_relative_error for the maximum
  // singular value of A.
  const double epsilon = 1.0 - MathUtil::Square(1.0 - desired_relative_error);
  while (PowerMethodFailureProbability(dimension, epsilon, num_iterations) >
         failure_probability) {
    VectorXd dual_eigenvector = TransposedMatrixVectorProduct(
        matrix_transpose, eigenvector, matrix_transpose_sharder);
    if (transpose_active_set_indicator.has_value()) {
      CoefficientWiseProductInPlace(*transpose_active_set_indicator,
                                    dual_vector_sharder, dual_eigenvector);
    }
    VectorXd next_eigenvector =
        TransposedMatrixVectorProduct(matrix, dual_eigenvector, matrix_sharder);
    if (active_set_indicator.has_value()) {
      CoefficientWiseProductInPlace(*active_set_indicator,
                                    primal_vector_sharder, next_eigenvector);
    }
    eigenvalue_estimate =
        Dot(eigenvector, next_eigenvector, primal_vector_sharder);
    eigenvector = std::move(next_eigenvector);
    ++num_iterations;
    const double primal_norm =
        NormalizeVector(primal_vector_sharder, eigenvector);

    VLOG(1) << "Iteration " << num_iterations << " singular value estimate "
            << std::sqrt(eigenvalue_estimate) << " primal norm " << primal_norm;
  }
  return SingularValueAndIterations{
      .singular_value = std::sqrt(eigenvalue_estimate),
      .num_iterations = num_iterations,
      .estimated_relative_error = desired_relative_error};
}

// Given a primal solution, compute a {0, 1}-valued vector that is nonzero in
// all the coordinates that are not saturating the primal variable bounds.
VectorXd ComputePrimalActiveSetIndicator(
    const ShardedQuadraticProgram& sharded_qp,
    const VectorXd& primal_solution) {
  VectorXd indicator(sharded_qp.PrimalSize());
  sharded_qp.PrimalSharder().ParallelForEachShard(
      [&](const Sharder::Shard& shard) {
        const auto lower_bound_shard =
            shard(sharded_qp.Qp().variable_lower_bounds);
        const auto upper_bound_shard =
            shard(sharded_qp.Qp().variable_upper_bounds);
        const auto primal_solution_shard = shard(primal_solution);
        auto indicator_shard = shard(indicator);
        const int64_t shard_size =
            sharded_qp.PrimalSharder().ShardSize(shard.Index());
        for (int64_t i = 0; i < shard_size; ++i) {
          if ((primal_solution_shard[i] == lower_bound_shard[i]) ||
              (primal_solution_shard[i] == upper_bound_shard[i])) {
            indicator_shard[i] = 0.0;
          } else {
            indicator_shard[i] = 1.0;
          }
        }
      });
  return indicator;
}

// Like ComputePrimalActiveSetIndicator(sharded_qp, primal_solution), but this
// time using the implicit bounds on the dual variable.
VectorXd ComputeDualActiveSetIndicator(
    const ShardedQuadraticProgram& sharded_qp, const VectorXd& dual_solution) {
  VectorXd indicator(sharded_qp.DualSize());
  sharded_qp.DualSharder().ParallelForEachShard(
      [&](const Sharder::Shard& shard) {
        const auto lower_bound_shard =
            shard(sharded_qp.Qp().constraint_lower_bounds);
        const auto upper_bound_shard =
            shard(sharded_qp.Qp().constraint_upper_bounds);
        const auto dual_solution_shard = shard(dual_solution);
        auto indicator_shard = shard(indicator);
        const int64_t shard_size =
            sharded_qp.DualSharder().ShardSize(shard.Index());
        for (int64_t i = 0; i < shard_size; ++i) {
          if (dual_solution_shard[i] == 0.0 &&
              (std::isinf(lower_bound_shard[i]) ||
               std::isinf(upper_bound_shard[i]))) {
            indicator_shard[i] = 0.0;
          } else {
            indicator_shard[i] = 1.0;
          }
        }
      });
  return indicator;
}

}  // namespace

SingularValueAndIterations EstimateMaximumSingularValueOfConstraintMatrix(
    const ShardedQuadraticProgram& sharded_qp,
    const std::optional<VectorXd>& primal_solution,
    const std::optional<VectorXd>& dual_solution,
    const double desired_relative_error, const double failure_probability,
    std::mt19937& mt_generator) {
  std::optional<VectorXd> primal_active_set_indicator;
  std::optional<VectorXd> dual_active_set_indicator;
  if (primal_solution.has_value()) {
    primal_active_set_indicator =
        ComputePrimalActiveSetIndicator(sharded_qp, *primal_solution);
  }
  if (dual_solution.has_value()) {
    dual_active_set_indicator =
        ComputeDualActiveSetIndicator(sharded_qp, *dual_solution);
  }
  return EstimateMaximumSingularValue(
      sharded_qp.Qp().constraint_matrix,
      sharded_qp.TransposedConstraintMatrix(), primal_active_set_indicator,
      dual_active_set_indicator, sharded_qp.ConstraintMatrixSharder(),
      sharded_qp.TransposedConstraintMatrixSharder(),
      sharded_qp.PrimalSharder(), sharded_qp.DualSharder(),
      desired_relative_error, failure_probability, mt_generator);
}

bool HasValidBounds(const ShardedQuadraticProgram& sharded_qp) {
  const QuadraticProgram& qp = sharded_qp.Qp();
  const bool constraint_bounds_valid =
      sharded_qp.DualSharder().ParallelTrueForAllShards(
          [&](const Sharder::Shard& shard) {
            return (shard(qp.constraint_lower_bounds).array() <=
                    shard(qp.constraint_upper_bounds).array())
                .all();
          });
  const bool variable_bounds_valid =
      sharded_qp.PrimalSharder().ParallelTrueForAllShards(
          [&](const Sharder::Shard& shard) {
            return (shard(qp.variable_lower_bounds).array() <=
                    shard(qp.variable_upper_bounds).array())
                .all();
          });

  return constraint_bounds_valid && variable_bounds_valid;
}

void ProjectToPrimalVariableBounds(const ShardedQuadraticProgram& sharded_qp,
                                   VectorXd& primal) {
  sharded_qp.PrimalSharder().ParallelForEachShard(
      [&](const Sharder::Shard& shard) {
        const QuadraticProgram& qp = sharded_qp.Qp();
        shard(primal) = shard(primal)
                            .cwiseMin(shard(qp.variable_upper_bounds))
                            .cwiseMax(shard(qp.variable_lower_bounds));
      });
}

void ProjectToDualVariableBounds(const ShardedQuadraticProgram& sharded_qp,
                                 VectorXd& dual) {
  const QuadraticProgram& qp = sharded_qp.Qp();
  sharded_qp.DualSharder().ParallelForEachShard(
      [&](const Sharder::Shard& shard) {
        const auto lower_bound_shard = shard(qp.constraint_lower_bounds);
        const auto upper_bound_shard = shard(qp.constraint_upper_bounds);
        auto dual_shard = shard(dual);

        for (int64_t i = 0; i < dual_shard.size(); ++i) {
          if (!std::isfinite(upper_bound_shard[i])) {
            dual_shard[i] = std::max(dual_shard[i], 0.0);
          }
          if (!std::isfinite(lower_bound_shard[i])) {
            dual_shard[i] = std::min(dual_shard[i], 0.0);
          }
        }
      });
}

}  // namespace operations_research::pdlp
