// Copyright 2010-2024 Google LLC
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

#include "ortools/pdlp/sharded_quadratic_program.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <utility>

#include "Eigen/Core"
#include "Eigen/SparseCore"
#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "ortools/base/logging.h"
#include "ortools/pdlp/quadratic_program.h"
#include "ortools/pdlp/scheduler.h"
#include "ortools/pdlp/sharder.h"
#include "ortools/pdlp/solvers.pb.h"
#include "ortools/util/logging.h"

namespace operations_research::pdlp {

namespace {

// Logs a warning if `matrix` has more than `density_limit` non-zeros in
// a single column.
void WarnIfMatrixUnbalanced(
    const Eigen::SparseMatrix<double, Eigen::ColMajor, int64_t>& matrix,
    absl::string_view matrix_name, int64_t density_limit,
    operations_research::SolverLogger* logger) {
  if (matrix.cols() == 0) return;
  int64_t worst_column = 0;
  for (int64_t col = 0; col < matrix.cols(); ++col) {
    if (matrix.col(col).nonZeros() > matrix.col(worst_column).nonZeros()) {
      worst_column = col;
    }
  }
  if (matrix.col(worst_column).nonZeros() > density_limit) {
    // TODO(user): fix this automatically in presolve instead of asking the
    // user to do it.
    if (logger) {
      SOLVER_LOG(
          logger, "WARNING: The ", matrix_name, " has ",
          matrix.col(worst_column).nonZeros(), " non-zeros in its ",
          worst_column,
          "th column. For best parallelization all rows and columns should "
          "have at most order ",
          density_limit,
          " non-zeros. Consider rewriting the QP to split the corresponding "
          "variable or constraint.");
    } else {
      LOG(WARNING)
          << "The " << matrix_name << " has "
          << matrix.col(worst_column).nonZeros() << " non-zeros in its "
          << worst_column
          << "th column. For best parallelization all rows and columns should "
             "have at most order "
          << density_limit
          << " non-zeros. Consider rewriting the QP to split the corresponding "
             "variable or constraint.";
    }
  }
}

}  // namespace

ShardedQuadraticProgram::ShardedQuadraticProgram(
    QuadraticProgram qp, const int num_threads, const int num_shards,
    SchedulerType scheduler_type, operations_research::SolverLogger* logger)
    : qp_(std::move(qp)),
      transposed_constraint_matrix_(qp_.constraint_matrix.transpose()),
      scheduler_(num_threads == 1 ? nullptr
                                  : MakeScheduler(scheduler_type, num_threads)),
      constraint_matrix_sharder_(qp_.constraint_matrix, num_shards,
                                 scheduler_.get()),
      transposed_constraint_matrix_sharder_(transposed_constraint_matrix_,
                                            num_shards, scheduler_.get()),
      primal_sharder_(qp_.variable_lower_bounds.size(), num_shards,
                      scheduler_.get()),
      dual_sharder_(qp_.constraint_lower_bounds.size(), num_shards,
                    scheduler_.get()) {
  CHECK_GE(num_threads, 1);
  CHECK_GE(num_shards, num_threads);
  if (num_threads > 1) {
    const int64_t work_per_iteration = qp_.constraint_matrix.nonZeros() +
                                       qp_.variable_lower_bounds.size() +
                                       qp_.constraint_lower_bounds.size();
    const int64_t column_density_limit = work_per_iteration / num_threads;
    WarnIfMatrixUnbalanced(qp_.constraint_matrix, "constraint matrix",
                           column_density_limit, logger);
    WarnIfMatrixUnbalanced(transposed_constraint_matrix_,
                           "transposed constraint matrix", column_density_limit,
                           logger);
  }
}

namespace {

// Multiply each entry of `matrix` by the corresponding element of
// `row_scaling_vec` and `col_scaling_vec`, i.e.,
// `matrix[i,j] *= row_scaling_vec[i] * col_scaling_vec[j]`.
void ScaleMatrix(
    const Eigen::VectorXd& col_scaling_vec,
    const Eigen::VectorXd& row_scaling_vec, const Sharder& sharder,
    Eigen::SparseMatrix<double, Eigen::ColMajor, int64_t>& matrix) {
  CHECK_EQ(matrix.cols(), col_scaling_vec.size());
  CHECK_EQ(matrix.rows(), row_scaling_vec.size());
  sharder.ParallelForEachShard([&](const Sharder::Shard& shard) {
    auto matrix_shard = shard(matrix);
    auto col_scaling_vec_shard = shard(col_scaling_vec);
    for (int64_t col_num = 0; col_num < shard(matrix).outerSize(); ++col_num) {
      for (decltype(matrix_shard)::InnerIterator it(matrix_shard, col_num); it;
           ++it) {
        it.valueRef() *=
            row_scaling_vec[it.row()] * col_scaling_vec_shard[it.col()];
      }
    }
  });
}

void ReplaceLargeValuesWithInfinity(const double threshold,
                                    const Sharder& sharder,
                                    Eigen::VectorXd& vector) {
  constexpr double kInfinity = std::numeric_limits<double>::infinity();
  sharder.ParallelForEachShard([&](const Sharder::Shard& shard) {
    auto vector_shard = shard(vector);
    for (int64_t i = 0; i < vector_shard.size(); ++i) {
      if (vector_shard[i] <= -threshold) vector_shard[i] = -kInfinity;
      if (vector_shard[i] >= threshold) vector_shard[i] = kInfinity;
    }
  });
}

}  // namespace

void ShardedQuadraticProgram::RescaleQuadraticProgram(
    const Eigen::VectorXd& col_scaling_vec,
    const Eigen::VectorXd& row_scaling_vec) {
  CHECK_EQ(PrimalSize(), col_scaling_vec.size());
  CHECK_EQ(DualSize(), row_scaling_vec.size());
  primal_sharder_.ParallelForEachShard([&](const Sharder::Shard& shard) {
    CHECK((shard(col_scaling_vec).array() > 0.0).all());
    shard(qp_.objective_vector) =
        shard(qp_.objective_vector).cwiseProduct(shard(col_scaling_vec));
    shard(qp_.variable_lower_bounds) =
        shard(qp_.variable_lower_bounds).cwiseQuotient(shard(col_scaling_vec));
    shard(qp_.variable_upper_bounds) =
        shard(qp_.variable_upper_bounds).cwiseQuotient(shard(col_scaling_vec));
    if (!IsLinearProgram(qp_)) {
      shard(qp_.objective_matrix->diagonal()) =
          shard(qp_.objective_matrix->diagonal())
              .cwiseProduct(
                  shard(col_scaling_vec).cwiseProduct(shard(col_scaling_vec)));
    }
  });
  dual_sharder_.ParallelForEachShard([&](const Sharder::Shard& shard) {
    CHECK((shard(row_scaling_vec).array() > 0.0).all());
    shard(qp_.constraint_lower_bounds) =
        shard(qp_.constraint_lower_bounds).cwiseProduct(shard(row_scaling_vec));
    shard(qp_.constraint_upper_bounds) =
        shard(qp_.constraint_upper_bounds).cwiseProduct(shard(row_scaling_vec));
  });

  ScaleMatrix(col_scaling_vec, row_scaling_vec, constraint_matrix_sharder_,
              qp_.constraint_matrix);
  ScaleMatrix(row_scaling_vec, col_scaling_vec,
              transposed_constraint_matrix_sharder_,
              transposed_constraint_matrix_);
}

void ShardedQuadraticProgram::ReplaceLargeConstraintBoundsWithInfinity(
    const double threshold) {
  ReplaceLargeValuesWithInfinity(threshold, DualSharder(),
                                 qp_.constraint_lower_bounds);
  ReplaceLargeValuesWithInfinity(threshold, DualSharder(),
                                 qp_.constraint_upper_bounds);
}

void ShardedQuadraticProgram::SetConstraintBounds(
    int64_t constraint_index, std::optional<double> lower_bound,
    std::optional<double> upper_bound) {
  CHECK_LT(constraint_index, DualSize());
  CHECK_GE(constraint_index, 0);
  if (lower_bound.has_value()) {
    qp_.constraint_lower_bounds[constraint_index] = *lower_bound;
  }
  if (upper_bound.has_value()) {
    qp_.constraint_upper_bounds[constraint_index] = *upper_bound;
  }
}

}  // namespace operations_research::pdlp
