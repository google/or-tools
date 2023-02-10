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

#ifndef PDLP_QUADRATIC_PROGRAM_H_
#define PDLP_QUADRATIC_PROGRAM_H_

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "Eigen/Core"
#include "Eigen/SparseCore"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "ortools/linear_solver/linear_solver.pb.h"

namespace operations_research::pdlp {

// Represents the quadratic program (QP):
//   min_x (`objective_vector`^T x + (1/2) x^T `objective_matrix` x) s.t.
//     `constraint_lower_bounds` <= `constraint_matrix` x
//                               <= `constraint_upper_bounds`
//       `variable_lower_bounds` <= x <= `variable_upper_bounds`
//
// `constraint_lower_bounds` and `variable_lower_bounds` may include negative
// infinities. `constraint_upper_bounds` and `variable_upper_bounds` may
// contain positive infinities. Other than that all entries of all fields must
// be finite. The `objective_matrix` must be diagonal and non-negative.
//
// For convenience, the struct also stores `scaling_factor` and
// `objective_offset`. These factors can be used to transform objective values
// based on the problem definition above into objective values that are
// meaningful for the user. See `ApplyObjectiveScalingAndOffset`.
//
// This struct is also intended for use with linear programs (LPs), which are
// QPs with a zero `objective_matrix`.
//
// The dual is documented at
// https://developers.google.com/optimization/lp/pdlp_math.
struct QuadraticProgram {
  QuadraticProgram(int64_t num_variables, int64_t num_constraints) {
    ResizeAndInitialize(num_variables, num_constraints);
  }
  QuadraticProgram() : QuadraticProgram(0, 0) {}

  // `QuadraticProgram` may be copied or moved. `Eigen::SparseMatrix` doesn't
  // have move operations so we use custom implementations based on swap.
  QuadraticProgram(const QuadraticProgram& other) = default;
  QuadraticProgram(QuadraticProgram&& other) noexcept
      : objective_vector(std::move(other.objective_vector)),
        objective_matrix(std::move(other.objective_matrix)),
        constraint_lower_bounds(std::move(other.constraint_lower_bounds)),
        constraint_upper_bounds(std::move(other.constraint_upper_bounds)),
        variable_lower_bounds(std::move(other.variable_lower_bounds)),
        variable_upper_bounds(std::move(other.variable_upper_bounds)),
        problem_name(std::move(other.problem_name)),
        variable_names(std::move(other.variable_names)),
        constraint_names(std::move(other.constraint_names)),
        objective_offset(other.objective_offset),
        objective_scaling_factor(other.objective_scaling_factor) {
    constraint_matrix.swap(other.constraint_matrix);
  }
  QuadraticProgram& operator=(const QuadraticProgram& other) = default;
  QuadraticProgram& operator=(QuadraticProgram&& other) {
    objective_vector = std::move(other.objective_vector);
    objective_matrix = std::move(other.objective_matrix);
    constraint_matrix.swap(other.constraint_matrix);
    constraint_lower_bounds = std::move(other.constraint_lower_bounds);
    constraint_upper_bounds = std::move(other.constraint_upper_bounds);
    variable_lower_bounds = std::move(other.variable_lower_bounds);
    variable_upper_bounds = std::move(other.variable_upper_bounds);
    problem_name = std::move(other.problem_name);
    variable_names = std::move(other.variable_names);
    constraint_names = std::move(other.constraint_names);
    objective_offset = other.objective_offset;
    objective_scaling_factor = other.objective_scaling_factor;
    return *this;
  }

  // Initializes the quadratic program with `num_variables` variables and
  // `num_constraints` constraints. Lower and upper bounds are set to negative
  // and positive infinity, repectively. `objective_matrix` is cleared. All
  // other matrices and vectors are set to zero. Resets the optional names
  // (`program_name`, `variable_names`, and `constraint_names`).
  // `objective_offset` is set to 0 and `objective_scaling_factor` is set to 1.
  void ResizeAndInitialize(int64_t num_variables, int64_t num_constraints) {
    constexpr double kInfinity = std::numeric_limits<double>::infinity();
    objective_vector = Eigen::VectorXd::Zero(num_variables);
    objective_matrix.reset();
    constraint_matrix.resize(num_constraints, num_variables);
    constraint_lower_bounds =
        Eigen::VectorXd::Constant(num_constraints, -kInfinity);
    constraint_upper_bounds =
        Eigen::VectorXd::Constant(num_constraints, kInfinity);
    variable_lower_bounds =
        Eigen::VectorXd::Constant(num_variables, -kInfinity);
    variable_upper_bounds = Eigen::VectorXd::Constant(num_variables, kInfinity);
    problem_name.reset();
    variable_names.reset();
    constraint_names.reset();
    objective_offset = 0.0;
    objective_scaling_factor = 1.0;
  }

  // Returns `objective_scaling_factor * (objective + objective_offset)`.
  // `objective_scaling_factor` is useful for modeling maximization problems.
  // For example, max c^T x = -1 * min (-c)^T x. `objective_offset` can be a
  // by-product of presolve transformations that eliminate variables.
  double ApplyObjectiveScalingAndOffset(double objective) const {
    return objective_scaling_factor * (objective + objective_offset);
  }

  Eigen::VectorXd objective_vector;
  // If this field isn't set, the `objective matrix` is interpreted to be zero,
  // i.e., this is a linear programming problem.
  std::optional<Eigen::DiagonalMatrix<double, Eigen::Dynamic>> objective_matrix;
  Eigen::SparseMatrix<double, Eigen::ColMajor, int64_t> constraint_matrix;
  Eigen::VectorXd constraint_lower_bounds, constraint_upper_bounds;
  Eigen::VectorXd variable_lower_bounds, variable_upper_bounds;

  std::optional<std::string> problem_name;
  std::optional<std::vector<std::string>> variable_names;
  std::optional<std::vector<std::string>> constraint_names;

  // These fields are provided for convenience; they don't change the
  // mathematical definition of the problem, but they change the objective
  // values reported to the user.
  double objective_offset;
  double objective_scaling_factor;
};

// Returns `InvalidArgumentError` if vector or matrix dimensions are
// inconsistent. Returns `OkStatus` otherwise.
absl::Status ValidateQuadraticProgramDimensions(const QuadraticProgram& qp);

inline bool IsLinearProgram(const QuadraticProgram& qp) {
  return !qp.objective_matrix.has_value();
}

// Checks if the lower and upper bounds of the problem are consistent, i.e. for
// each variable and constraint bound we have `lower_bound <= upper_bound`. If
// the input is consistent the method returns true, otherwise it returns false.
// See also `HasValidBounds(const ShardedQuadraticProgram&)`.
bool HasValidBounds(const QuadraticProgram& qp);

// Converts an `MPModelProto` into a `QuadraticProgram`.
// Returns an error if general constraints are present.
// If `relax_integer_variables` is true integer variables are relaxed to
// continuous; otherwise integer variables are an error.
// If `include_names` is true (the default is false), the problem, constraint,
// and variable names are included in the `QuadraticProgram`; otherwise they are
// left empty.
// Maximization problems are converted to minimization by negating the
// objective and setting `objective_scaling_factor` to -1, which preserves the
// reported objective values.
absl::StatusOr<QuadraticProgram> QpFromMpModelProto(
    const MPModelProto& proto, bool relax_integer_variables,
    bool include_names = false);

// Returns `InvalidArgumentError` if `qp` is too large to convert to
// `MPModelProto` and `OkStatus` otherwise.
absl::Status CanFitInMpModelProto(const QuadraticProgram& qp);

// Converts a `QuadraticProgram` into an `MPModelProto`. To preserve objective
// values in the conversion, `objective_vector`, `objective_matrix`, and
// `objective_offset` are scaled by `objective_scaling_factor`, and if
// `objective_scaling_factor` is negative, then the proto is a maximization
// problem (otherwise it's a minimization problem). Returns
// `InvalidArgumentError` if `objective_scaling_factor` is zero or if
// `CanFitInMpModelProto()` fails.
absl::StatusOr<MPModelProto> QpToMpModelProto(const QuadraticProgram& qp);

// Like `matrix.setFromTriplets(triplets)`, except that `setFromTriplets`
// results in having three copies of the nonzeros in memory at the same time,
// because it first fills one matrix from triplets, and then transposes it into
// another. This avoids having the third copy in memory by sorting the triplets,
// reserving space in the matrix, and then inserting in sorted order.
// Compresses the matrix (`SparseMatrix.makeCompressed()`) after loading it.
// NOTE: This intentionally passes `triplets` by value, because it modifies
// them. To avoid the copy, pass a move reference.
void SetEigenMatrixFromTriplets(
    std::vector<Eigen::Triplet<double, int64_t>> triplets,
    Eigen::SparseMatrix<double, Eigen::ColMajor, int64_t>& matrix);

// Utility functions for internal use only.
namespace internal {
// Like `CanFitInMpModelProto()` but has an extra argument for the largest
// number of variables, constraints, or objective non-zeros that should be
// counted as convertible. `CanFitInMpModelProto()` passes 2^31 - 1 for this
// argument and unit tests pass small values.
absl::Status TestableCanFitInMpModelProto(const QuadraticProgram& qp,
                                          int64_t largest_ok_size);

// Modifies `triplets` in place, combining consecutive entries with the same row
// and column, summing their values. This is most effective if `triplets` are
// sorted by row and column, so that multiple entries for the same entry will be
// consecutive.
void CombineRepeatedTripletsInPlace(
    std::vector<Eigen::Triplet<double, int64_t>>& triplets);
}  // namespace internal
}  // namespace operations_research::pdlp

#endif  // PDLP_QUADRATIC_PROGRAM_H_
