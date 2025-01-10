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

#include "ortools/pdlp/sharded_optimization_utils.h"

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
  average_ = ZeroVector(*sharder_);
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
  // This `if` protects against NaN if `sum_weights_` also == 0.0.
  if (weight > 0.0) {
    const double weight_ratio = weight / (sum_weights_ + weight);
    sharder_->ParallelForEachShard([&](const Sharder::Shard& shard) {
      shard(average_) += weight_ratio * (shard(datapoint) - shard(average_));
    });
    sum_weights_ += weight;
  }
  ++num_terms_;
}

void ShardedWeightedAverage::Clear() {
  SetZero(*sharder_, average_);
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

double CombineBounds(const double v1, const double v2) {
  double max = 0.0;
  if (std::abs(v1) < kInfinity) {
    max = std::abs(v1);
  }
  if (std::abs(v2) < kInfinity) {
    max = std::max(max, std::abs(v2));
  }
  return max;
}

struct VectorInfo {
  int64_t num_finite_nonzero = 0;
  int64_t num_infinite = 0;
  int64_t num_zero = 0;
  // The largest absolute value of the finite non-zero values.
  double largest = 0.0;
  // The smallest absolute value of the finite non-zero values.
  double smallest = 0.0;
  // The average absolute value of the finite values.
  double average = 0.0;
  // The L2 norm of the finite values.
  double l2_norm = 0.0;
};

struct InfNormInfo {
  VectorInfo row_norms;
  VectorInfo col_norms;
};

// `VectorInfoAccumulator` accumulates values for a `VectorInfo`.
// NOTE: In `VectorInfo`, the max and min of an empty set is 0.0 by convention.
// In `VectorInfoAccumulator`, it is -`kInfinity` and `kInfinity` to simplify
// adding additional values.
class VectorInfoAccumulator {
 public:
  VectorInfoAccumulator() {}
  // Move-only even though move and copy are the same cost, to help catch
  // unintentional moves/copies (which are probably performance bugs).
  VectorInfoAccumulator(const VectorInfoAccumulator&) = delete;
  VectorInfoAccumulator& operator=(const VectorInfoAccumulator&) = delete;
  VectorInfoAccumulator(VectorInfoAccumulator&&) = default;
  VectorInfoAccumulator& operator=(VectorInfoAccumulator&&) = default;
  void Add(double value);
  void Add(const VectorInfoAccumulator& other);
  explicit operator VectorInfo() const;

 private:
  int64_t num_infinite_ = 0;
  int64_t num_zero_ = 0;
  int64_t num_finite_nonzero_ = 0;
  double max_ = -kInfinity;
  double min_ = kInfinity;
  double sum_ = 0.0;
  double sum_squared_ = 0.0;
};

void VectorInfoAccumulator::Add(const double value) {
  if (std::isinf(value)) {
    ++num_infinite_;
  } else if (value == 0) {
    ++num_zero_;
  } else {
    ++num_finite_nonzero_;
    const double abs_value = std::abs(value);
    max_ = std::max(max_, abs_value);
    min_ = std::min(min_, abs_value);
    sum_ += abs_value;
    sum_squared_ += abs_value * abs_value;
  }
}

void VectorInfoAccumulator::Add(const VectorInfoAccumulator& other) {
  num_infinite_ += other.num_infinite_;
  num_zero_ += other.num_zero_;
  num_finite_nonzero_ += other.num_finite_nonzero_;
  max_ = std::max(max_, other.max_);
  min_ = std::min(min_, other.min_);
  sum_ += other.sum_;
  sum_squared_ += other.sum_squared_;
}

VectorInfoAccumulator::operator VectorInfo() const {
  return VectorInfo{
      .num_finite_nonzero = num_finite_nonzero_,
      .num_infinite = num_infinite_,
      .num_zero = num_zero_,
      .largest = num_finite_nonzero_ > 0 ? max_ : 0.0,
      .smallest = num_finite_nonzero_ > 0 ? min_ : 0.0,
      .average = num_finite_nonzero_ + num_zero_ > 0
                     ? sum_ / (num_finite_nonzero_ + num_zero_)
                     : std::numeric_limits<double>::quiet_NaN(),
      .l2_norm = std::sqrt(sum_squared_),
  };
}

VectorInfo CombineAccumulators(
    const std::vector<VectorInfoAccumulator>& accumulators) {
  VectorInfoAccumulator result;
  for (const VectorInfoAccumulator& accumulator : accumulators) {
    result.Add(accumulator);
  }
  return VectorInfo(result);
}

// TODO(b/223148482): Switch `vec` to `const Eigen::Ref<const VectorXd>` if/when
// `Sharder` supports `Eigen::Ref`, to avoid a copy when called on
// `qp.Qp().objective_matrix->diagonal()`.
VectorInfo ComputeVectorInfo(const VectorXd& vec, const Sharder& sharder) {
  std::vector<VectorInfoAccumulator> local_accumulator(sharder.NumShards());
  sharder.ParallelForEachShard([&](const Sharder::Shard& shard) {
    VectorInfoAccumulator shard_accumulator;
    for (double element : shard(vec)) {
      shard_accumulator.Add(element);
    }
    local_accumulator[shard.Index()] = std::move(shard_accumulator);
  });
  return CombineAccumulators(local_accumulator);
}

VectorInfo VariableBoundGapInfo(const VectorXd& lower_bounds,
                                const VectorXd& upper_bounds,
                                const Sharder& sharder) {
  std::vector<VectorInfoAccumulator> local_accumulator(sharder.NumShards());
  sharder.ParallelForEachShard([&](const Sharder::Shard& shard) {
    VectorInfoAccumulator shard_accumulator;
    for (double element : shard(upper_bounds) - shard(lower_bounds)) {
      shard_accumulator.Add(element);
    }
    local_accumulator[shard.Index()] = std::move(shard_accumulator);
  });
  return CombineAccumulators(local_accumulator);
}

VectorInfo MatrixAbsElementInfo(
    const SparseMatrix<double, ColMajor, int64_t>& matrix,
    const Sharder& sharder) {
  std::vector<VectorInfoAccumulator> local_accumulator(sharder.NumShards());
  sharder.ParallelForEachShard([&](const Sharder::Shard& shard) {
    VectorInfoAccumulator shard_accumulator;
    const auto matrix_shard = shard(matrix);
    for (int64_t col_idx = 0; col_idx < matrix_shard.outerSize(); ++col_idx) {
      for (decltype(matrix_shard)::InnerIterator it(matrix_shard, col_idx); it;
           ++it) {
        shard_accumulator.Add(it.value());
      }
    }
    local_accumulator[shard.Index()] = std::move(shard_accumulator);
  });
  return CombineAccumulators(local_accumulator);
}

VectorInfo CombinedBoundsInfo(const VectorXd& rhs_upper_bounds,
                              const VectorXd& rhs_lower_bounds,
                              const Sharder& sharder) {
  std::vector<VectorInfoAccumulator> local_accumulator(sharder.NumShards());
  sharder.ParallelForEachShard([&](const Sharder::Shard& shard) {
    VectorInfoAccumulator shard_accumulator;
    const auto lb_shard = shard(rhs_lower_bounds);
    const auto ub_shard = shard(rhs_upper_bounds);
    for (int64_t i = 0; i < lb_shard.size(); ++i) {
      shard_accumulator.Add(CombineBounds(ub_shard[i], lb_shard[i]));
    }
    local_accumulator[shard.Index()] = std::move(shard_accumulator);
  });
  return CombineAccumulators(local_accumulator);
}

InfNormInfo ConstraintMatrixRowColInfo(
    const SparseMatrix<double, ColMajor, int64_t>& constraint_matrix,
    const SparseMatrix<double, ColMajor, int64_t>& constraint_matrix_transpose,
    const Sharder& matrix_sharder, const Sharder& matrix_transpose_sharder,
    const Sharder& primal_sharder, const Sharder& dual_sharder) {
  VectorXd row_norms = ScaledColLInfNorm(
      constraint_matrix_transpose,
      /*col_scaling_vec=*/OnesVector(primal_sharder),
      /*row_scaling_vec=*/OnesVector(dual_sharder), matrix_transpose_sharder);
  VectorXd col_norms = ScaledColLInfNorm(
      constraint_matrix,
      /*row_scaling_vec=*/OnesVector(dual_sharder),
      /*col_scaling_vec=*/OnesVector(primal_sharder), matrix_sharder);
  return InfNormInfo{.row_norms = ComputeVectorInfo(row_norms, dual_sharder),
                     .col_norms = ComputeVectorInfo(col_norms, primal_sharder)};
}

}  // namespace

QuadraticProgramStats ComputeStats(const ShardedQuadraticProgram& qp) {
  // Caution: if the constraint matrix is empty, elementwise operations
  // (like `.coeffs().maxCoeff()` or `.minCoeff()`) will fail.
  InfNormInfo cons_matrix_norm_info = ConstraintMatrixRowColInfo(
      qp.Qp().constraint_matrix, qp.TransposedConstraintMatrix(),
      qp.ConstraintMatrixSharder(), qp.TransposedConstraintMatrixSharder(),
      qp.PrimalSharder(), qp.DualSharder());
  VectorInfo cons_matrix_info = MatrixAbsElementInfo(
      qp.Qp().constraint_matrix, qp.ConstraintMatrixSharder());
  VectorInfo combined_bounds_info =
      CombinedBoundsInfo(qp.Qp().constraint_lower_bounds,
                         qp.Qp().constraint_upper_bounds, qp.DualSharder());
  VectorInfo combined_variable_bounds_info =
      CombinedBoundsInfo(qp.Qp().variable_lower_bounds,
                         qp.Qp().variable_upper_bounds, qp.PrimalSharder());
  VectorInfo obj_vec_info =
      ComputeVectorInfo(qp.Qp().objective_vector, qp.PrimalSharder());
  VectorInfo gaps_info =
      VariableBoundGapInfo(qp.Qp().variable_lower_bounds,
                           qp.Qp().variable_upper_bounds, qp.PrimalSharder());
  QuadraticProgramStats program_stats;
  program_stats.set_num_variables(qp.PrimalSize());
  program_stats.set_num_constraints(qp.DualSize());
  program_stats.set_constraint_matrix_col_min_l_inf_norm(
      cons_matrix_norm_info.col_norms.smallest);
  program_stats.set_constraint_matrix_row_min_l_inf_norm(
      cons_matrix_norm_info.row_norms.smallest);
  program_stats.set_constraint_matrix_num_nonzeros(
      cons_matrix_info.num_finite_nonzero);
  program_stats.set_constraint_matrix_abs_max(cons_matrix_info.largest);
  program_stats.set_constraint_matrix_abs_min(cons_matrix_info.smallest);
  program_stats.set_constraint_matrix_abs_avg(cons_matrix_info.average);
  program_stats.set_constraint_matrix_l2_norm(cons_matrix_info.l2_norm);
  program_stats.set_combined_bounds_max(combined_bounds_info.largest);
  program_stats.set_combined_bounds_min(combined_bounds_info.smallest);
  program_stats.set_combined_bounds_avg(combined_bounds_info.average);
  program_stats.set_combined_bounds_l2_norm(combined_bounds_info.l2_norm);
  program_stats.set_combined_variable_bounds_max(
      combined_variable_bounds_info.largest);
  program_stats.set_combined_variable_bounds_min(
      combined_variable_bounds_info.smallest);
  program_stats.set_combined_variable_bounds_avg(
      combined_variable_bounds_info.average);
  program_stats.set_combined_variable_bounds_l2_norm(
      combined_variable_bounds_info.l2_norm);
  program_stats.set_variable_bound_gaps_num_finite(
      gaps_info.num_finite_nonzero + gaps_info.num_zero);
  program_stats.set_variable_bound_gaps_max(gaps_info.largest);
  program_stats.set_variable_bound_gaps_min(gaps_info.smallest);
  program_stats.set_variable_bound_gaps_avg(gaps_info.average);
  program_stats.set_variable_bound_gaps_l2_norm(gaps_info.l2_norm);
  program_stats.set_objective_vector_abs_max(obj_vec_info.largest);
  program_stats.set_objective_vector_abs_min(obj_vec_info.smallest);
  program_stats.set_objective_vector_abs_avg(obj_vec_info.average);
  program_stats.set_objective_vector_l2_norm(obj_vec_info.l2_norm);
  if (IsLinearProgram(qp.Qp())) {
    program_stats.set_objective_matrix_num_nonzeros(0);
    program_stats.set_objective_matrix_abs_max(0);
    program_stats.set_objective_matrix_abs_min(0);
    program_stats.set_objective_matrix_abs_avg(
        std::numeric_limits<double>::quiet_NaN());
    program_stats.set_objective_matrix_l2_norm(0);
  } else {
    VectorInfo obj_matrix_info = ComputeVectorInfo(
        qp.Qp().objective_matrix->diagonal(), qp.PrimalSharder());
    program_stats.set_objective_matrix_num_nonzeros(
        obj_matrix_info.num_finite_nonzero);
    program_stats.set_objective_matrix_abs_max(obj_matrix_info.largest);
    program_stats.set_objective_matrix_abs_min(obj_matrix_info.smallest);
    program_stats.set_objective_matrix_abs_avg(obj_matrix_info.average);
    program_stats.set_objective_matrix_l2_norm(obj_matrix_info.l2_norm);
  }
  return program_stats;
}

namespace {

enum class ScalingNorm { kL2, kLInf };

// Divides `vector` (componentwise) by the square root of `divisor`, updating
// `vector` in-place. If a component of `divisor` is equal to zero, leaves the
// component of `vector` unchanged. `sharder` should have the same size as
// `vector`. For best performance `sharder` should have been created with the
// `Sharder(int64_t, int, ThreadPool*)` constructor.
void DivideBySquareRootOfDivisor(const VectorXd& divisor,
                                 const Sharder& sharder, VectorXd& vector) {
  sharder.ParallelForEachShard([&](const Sharder::Shard& shard) {
    auto vec_shard = shard(vector);
    const auto divisor_shard = shard(divisor);
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
      .row_scaling_vec = OnesVector(sharded_qp.DualSharder()),
      .col_scaling_vec = OnesVector(sharded_qp.PrimalSharder())};
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
          // Note: using `auto` instead of `VectorXd` for the type of
          // `objective_product` causes eigen to defer the matrix product until
          // it is used (twice).
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
                                   const VectorXd& dual_solution,
                                   const VectorXd& primal_product) {
  LagrangianPart result{.gradient = VectorXd(sharded_qp.DualSize())};
  const QuadraticProgram& qp = sharded_qp.Qp();
  VectorXd value_parts(sharded_qp.DualSharder().NumShards());
  sharded_qp.DualSharder().ParallelForEachShard(
      [&](const Sharder::Shard& shard) {
        const auto constraint_lower_bounds = shard(qp.constraint_lower_bounds);
        const auto constraint_upper_bounds = shard(qp.constraint_upper_bounds);
        const auto dual_solution_shard = shard(dual_solution);
        auto dual_gradient_shard = shard(result.gradient);
        const auto primal_product_shard = shard(primal_product);
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

// Scales `vector` (in-place) to have norm 1, unless it has norm 0 (in which
// case it is left unscaled). Returns the original norm of `vector`.
double NormalizeVector(const Sharder& sharder, VectorXd& vector) {
  const double norm = Norm(vector, sharder);
  if (norm != 0.0) {
    sharder.ParallelForEachShard(
        [&](const Sharder::Shard& shard) { shard(vector) /= norm; });
  }
  return norm;
}

// Estimates the probability that the power method, after k iterations, has
// relative error > `epsilon`. This is based on Theorem 4.1(a) (on page 13) from
// "Estimating the Largest Eigenvalue by the Power and Lanczos Algorithms with a
// Random Start"
// https://pdfs.semanticscholar.org/2b2e/a941e55e5fa2ee9d8f4ff393c14482051143.pdf
double PowerMethodFailureProbability(int64_t dimension, double epsilon, int k) {
  if (k < 2 || epsilon <= 0.0) {
    // The theorem requires `epsilon > 0` and `k >= 2`.
    return 1.0;
  }
  return std::min(0.824, 0.354 / std::sqrt(epsilon * (k - 1))) *
         std::sqrt(dimension) * std::pow(1.0 - epsilon, k - 0.5);
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
  const int64_t dimension = matrix.cols();
  VectorXd eigenvector(dimension);
  // Even though it will be slower, we initialize `eigenvector` sequentially so
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
  // eigenvalue of A^T A. `epsilon` is the relative error needed for the maximum
  // eigenvalue of A^T A that gives `desired_relative_error` for the maximum
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

// Given `primal_solution`, compute a {0, 1}-valued vector that is nonzero in
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

// Like `ComputePrimalActiveSetIndicator(sharded_qp, primal_solution)`, but this
// time using the implicit bounds on the dual variables.
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
                        shard(qp.constraint_upper_bounds).array() &&
                    shard(qp.constraint_lower_bounds).array() < kInfinity &&
                    shard(qp.constraint_upper_bounds).array() > -kInfinity)
                .all();
          });
  const bool variable_bounds_valid =
      sharded_qp.PrimalSharder().ParallelTrueForAllShards(
          [&](const Sharder::Shard& shard) {
            return (shard(qp.variable_lower_bounds).array() <=
                        shard(qp.variable_upper_bounds).array() &&
                    shard(qp.variable_lower_bounds).array() < kInfinity &&
                    shard(qp.variable_upper_bounds).array() > -kInfinity)
                .all();
          });

  return constraint_bounds_valid && variable_bounds_valid;
}

void ProjectToPrimalVariableBounds(const ShardedQuadraticProgram& sharded_qp,
                                   VectorXd& primal,
                                   const bool use_feasibility_bounds) {
  const auto make_finite_values_zero = [](const double x) {
    return std::isfinite(x) ? 0.0 : x;
  };
  sharded_qp.PrimalSharder().ParallelForEachShard(
      [&](const Sharder::Shard& shard) {
        const QuadraticProgram& qp = sharded_qp.Qp();
        if (use_feasibility_bounds) {
          shard(primal) =
              shard(primal)
                  .cwiseMin(shard(qp.variable_upper_bounds)
                                .unaryExpr(make_finite_values_zero))
                  .cwiseMax(shard(qp.variable_lower_bounds)
                                .unaryExpr(make_finite_values_zero));
        } else {
          shard(primal) = shard(primal)
                              .cwiseMin(shard(qp.variable_upper_bounds))
                              .cwiseMax(shard(qp.variable_lower_bounds));
        }
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

        // TODO(user): Investigate whether it is more efficient to
        // use .cwiseMax() + .cwiseMin() with unaryExpr(s) that map
        // upper_bound_shard and lower_bound_shard to appropriate values.
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
