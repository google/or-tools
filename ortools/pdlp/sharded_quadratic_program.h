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

#ifndef PDLP_SHARDED_QUADRATIC_PROGRAM_H_
#define PDLP_SHARDED_QUADRATIC_PROGRAM_H_

#include <cstdint>
#include <memory>

#include "Eigen/Core"
#include "Eigen/SparseCore"
#include "ortools/base/threadpool.h"
#include "ortools/pdlp/quadratic_program.h"
#include "ortools/pdlp/sharder.h"

namespace operations_research::pdlp {

// This class stores:
//  - A `QuadraticProgram` (QP)
//  - A transposed version of the QP's constraint matrix
//  - A thread pool
//  - Various `Sharder` objects for doing sharded matrix and vector
//    computations.
class ShardedQuadraticProgram {
 public:
  // Requires `num_shards` >= `num_threads` >= 1.
  // Note that the `qp` is intentionally passed by value.
  ShardedQuadraticProgram(QuadraticProgram qp, int num_threads, int num_shards);

  // Movable but not copyable.
  ShardedQuadraticProgram(const ShardedQuadraticProgram&) = delete;
  ShardedQuadraticProgram& operator=(const ShardedQuadraticProgram&) = delete;
  ShardedQuadraticProgram(ShardedQuadraticProgram&&) = default;
  ShardedQuadraticProgram& operator=(ShardedQuadraticProgram&&) = default;

  const QuadraticProgram& Qp() const { return qp_; }

  // Returns a reference to the transpose of the QP's constraint matrix.
  const Eigen::SparseMatrix<double, Eigen::ColMajor, int64_t>&
  TransposedConstraintMatrix() const {
    return transposed_constraint_matrix_;
  }

  // Returns a `Sharder` intended for the columns of the QP's constraint matrix.
  const Sharder& ConstraintMatrixSharder() const {
    return constraint_matrix_sharder_;
  }
  // Returns a `Sharder` intended for the rows of the QP's constraint matrix.
  const Sharder& TransposedConstraintMatrixSharder() const {
    return transposed_constraint_matrix_sharder_;
  }
  // Returns a `Sharder` intended for primal vectors.
  const Sharder& PrimalSharder() const { return primal_sharder_; }
  // Returns a `Sharder` intended for dual vectors.
  const Sharder& DualSharder() const { return dual_sharder_; }

  int64_t PrimalSize() const { return qp_.variable_lower_bounds.size(); }
  int64_t DualSize() const { return qp_.constraint_lower_bounds.size(); }

  // Rescales the QP (including objective, variable bounds, constraint bounds,
  // constraint matrix, and transposed constraint matrix) based on
  // `col_scaling_vec` and `row_scaling_vec`. That is, rescale the problem so
  // that each variable is rescaled as variable[i] <- variable[i] /
  // `col_scaling_vec[i]`, and the j-th constraint is multiplied by
  // `row_scaling_vec[j]`. `col_scaling_vec` and `row_scaling_vec` must be
  // positive.
  void RescaleQuadraticProgram(const Eigen::VectorXd& col_scaling_vec,
                               const Eigen::VectorXd& row_scaling_vec);

  void SwapVariableBounds(Eigen::VectorXd& variable_lower_bounds,
                          Eigen::VectorXd& variable_upper_bounds) {
    qp_.variable_lower_bounds.swap(variable_lower_bounds);
    qp_.variable_upper_bounds.swap(variable_upper_bounds);
  }

  void SwapConstraintBounds(Eigen::VectorXd& constraint_lower_bounds,
                            Eigen::VectorXd& constraint_upper_bounds) {
    qp_.constraint_lower_bounds.swap(constraint_lower_bounds);
    qp_.constraint_upper_bounds.swap(constraint_upper_bounds);
  }

  // Swaps `objective` with the `objective_vector` in the quadratic program.
  // Swapping `objective_matrix` is not yet supported because it hasn't been
  // needed.
  void SwapObjectiveVector(Eigen::VectorXd& objective) {
    qp_.objective_vector.swap(objective);
  }

 private:
  QuadraticProgram qp_;
  Eigen::SparseMatrix<double, Eigen::ColMajor, int64_t>
      transposed_constraint_matrix_;
  std::unique_ptr<ThreadPool> thread_pool_;
  Sharder constraint_matrix_sharder_;
  Sharder transposed_constraint_matrix_sharder_;
  Sharder primal_sharder_;
  Sharder dual_sharder_;
};

}  // namespace operations_research::pdlp

#endif  // PDLP_SHARDED_QUADRATIC_PROGRAM_H_
