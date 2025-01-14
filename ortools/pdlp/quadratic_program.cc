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

#include "ortools/pdlp/quadratic_program.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "Eigen/Core"
#include "Eigen/SparseCore"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "ortools/base/status_macros.h"
#include "ortools/linear_solver/linear_solver.pb.h"

namespace operations_research::pdlp {

using ::Eigen::VectorXd;

absl::Status ValidateQuadraticProgramDimensions(const QuadraticProgram& qp) {
  const int64_t var_lb_size = qp.variable_lower_bounds.size();
  const int64_t con_lb_size = qp.constraint_lower_bounds.size();

  if (var_lb_size != qp.variable_upper_bounds.size()) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Inconsistent dimensions: variable lower bound vector has size ",
        var_lb_size, " while variable upper bound vector has size ",
        qp.variable_upper_bounds.size()));
  }
  if (var_lb_size != qp.objective_vector.size()) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Inconsistent dimensions: variable lower bound vector has size ",
        var_lb_size, " while objective vector has size ",
        qp.objective_vector.size()));
  }
  if (var_lb_size != qp.constraint_matrix.cols()) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Inconsistent dimensions: variable lower bound vector has size ",
        var_lb_size, " while constraint matrix has ",
        qp.constraint_matrix.cols(), " columns"));
  }
  if (qp.objective_matrix.has_value() &&
      var_lb_size != qp.objective_matrix->rows()) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Inconsistent dimensions: variable lower bound vector has size ",
        var_lb_size, " while objective matrix has ",
        qp.objective_matrix->rows(), " rows"));
  }
  if (con_lb_size != qp.constraint_upper_bounds.size()) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Inconsistent dimensions: constraint lower bound vector has size ",
        con_lb_size, " while constraint upper bound vector has size ",
        qp.constraint_upper_bounds.size()));
  }
  if (con_lb_size != qp.constraint_matrix.rows()) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Inconsistent dimensions: constraint lower bound vector has size ",
        con_lb_size, " while constraint matrix has ",
        qp.constraint_matrix.rows(), " rows "));
  }

  if (qp.variable_names.has_value() &&
      var_lb_size != qp.variable_names->size()) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Inconsistent dimensions: variable lower bound vector has size ",
        var_lb_size, " while variable names has size ",
        qp.variable_names->size()));
  }

  if (qp.constraint_names.has_value() &&
      con_lb_size != qp.constraint_names->size()) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Inconsistent dimensions: constraint lower bound vector has size ",
        con_lb_size, " while constraint names has size ",
        qp.constraint_names->size()));
  }

  return absl::OkStatus();
}

absl::StatusOr<QuadraticProgram> QpFromMpModelProto(
    const MPModelProto& proto, bool relax_integer_variables,
    bool include_names) {
  if (!proto.general_constraint().empty()) {
    return absl::InvalidArgumentError("General constraints are not supported.");
  }
  const int primal_size = proto.variable_size();
  const int dual_size = proto.constraint_size();
  QuadraticProgram qp(primal_size, dual_size);
  if (include_names) {
    qp.problem_name = proto.name();
    qp.variable_names = std::vector<std::string>(primal_size);
    qp.constraint_names = std::vector<std::string>(dual_size);
  }
  for (int i = 0; i < primal_size; ++i) {
    const auto& var = proto.variable(i);
    qp.variable_lower_bounds[i] = var.lower_bound();
    qp.variable_upper_bounds[i] = var.upper_bound();
    qp.objective_vector[i] = var.objective_coefficient();
    if (var.is_integer() && !relax_integer_variables) {
      return absl::InvalidArgumentError(
          "Integer variable encountered with relax_integer_variables == false");
    }
    if (include_names) {
      (*qp.variable_names)[i] = var.name();
    }
  }
  std::vector<int> nonzeros_by_column(primal_size);
  for (int i = 0; i < dual_size; ++i) {
    const auto& con = proto.constraint(i);
    for (int j = 0; j < con.var_index_size(); ++j) {
      if (con.var_index(j) < 0 || con.var_index(j) >= primal_size) {
        return absl::InvalidArgumentError(absl::StrCat(
            "Variable index of ", i, "th constraint's ", j, "th nonzero is ",
            con.var_index(j), " which is not in the allowed range [0, ",
            primal_size, ")"));
      }
      nonzeros_by_column[con.var_index(j)]++;
    }
    qp.constraint_lower_bounds[i] = con.lower_bound();
    qp.constraint_upper_bounds[i] = con.upper_bound();
    if (include_names) {
      (*qp.constraint_names)[i] = con.name();
    }
  }
  // To reduce peak RAM usage we construct the constraint matrix in-place.
  // According to the documentation of `SparseMatrix::insert()` it's efficient
  // to construct a matrix with insert()s as long as reserve() is called first
  // and the non-zeros are inserted in increasing order of inner index.
  // The non-zeros in each input constraint may not be sorted so this is only
  // efficient with column major format.
  static_assert(qp.constraint_matrix.IsRowMajor == 0, "See comment.");
  qp.constraint_matrix.reserve(nonzeros_by_column);
  for (int i = 0; i < dual_size; ++i) {
    const auto& con = proto.constraint(i);
    CHECK_EQ(con.var_index_size(), con.coefficient_size())
        << " in " << i << "th constraint";
    if (con.var_index_size() != con.coefficient_size()) {
      return absl::InvalidArgumentError(
          absl::StrCat(i, "th constraint has ", con.coefficient_size(),
                       " coefficients, expected ", con.var_index_size()));
    }

    for (int j = 0; j < con.var_index_size(); ++j) {
      qp.constraint_matrix.insert(i, con.var_index(j)) = con.coefficient(j);
    }
  }
  if (qp.constraint_matrix.outerSize() > 0) {
    qp.constraint_matrix.makeCompressed();
  }
  // We use triplets-based initialization for the objective matrix because the
  // objective non-zeros may be in arbitrary order in the input.
  std::vector<Eigen::Triplet<double, int64_t>> triplets;
  const auto& quadratic = proto.quadratic_objective();
  if (quadratic.qvar1_index_size() != quadratic.qvar2_index_size() ||
      quadratic.qvar1_index_size() != quadratic.coefficient_size()) {
    return absl::InvalidArgumentError(absl::StrCat(
        "The quadratic objective has ", quadratic.qvar1_index_size(),
        " qvar1_indices, ", quadratic.qvar2_index_size(),
        " qvar2_indices, and ", quadratic.coefficient_size(),
        " coefficients, expected equal numbers."));
  }
  if (quadratic.qvar1_index_size() > 0) {
    qp.objective_matrix.emplace();
    qp.objective_matrix->setZero(primal_size);
  }

  for (int i = 0; i < quadratic.qvar1_index_size(); ++i) {
    const int index1 = quadratic.qvar1_index(i);
    const int index2 = quadratic.qvar2_index(i);
    if (index1 < 0 || index2 < 0 || index1 >= primal_size ||
        index2 >= primal_size) {
      return absl::InvalidArgumentError(absl::StrCat(
          "The quadratic objective's ", i, "th nonzero has indices ", index1,
          " and ", index2, ", which are not both in the expected range [0, ",
          primal_size, ")"));
    }
    if (index1 != index2) {
      return absl::InvalidArgumentError(absl::StrCat(
          "The quadratic objective's ", i,
          "th nonzero has off-diagonal element at (", index1, ", ", index2,
          "). Only diagonal objective matrices are supported."));
    }
    // Note: `QuadraticProgram` has an implicit "1/2" in front of the quadratic
    // term.
    qp.objective_matrix->diagonal()[index1] = 2 * quadratic.coefficient(i);
  }
  qp.objective_offset = proto.objective_offset();
  if (proto.maximize()) {
    qp.objective_offset *= -1;
    qp.objective_vector *= -1;
    if (qp.objective_matrix.has_value()) {
      qp.objective_matrix->diagonal() *= -1;
    }
    qp.objective_scaling_factor = -1;
  }
  return qp;
}

absl::Status CanFitInMpModelProto(const QuadraticProgram& qp) {
  return internal::TestableCanFitInMpModelProto(
      qp, std::numeric_limits<int32_t>::max());
}

namespace internal {
absl::Status TestableCanFitInMpModelProto(const QuadraticProgram& qp,
                                          const int64_t largest_ok_size) {
  const int64_t primal_size = qp.variable_lower_bounds.size();
  const int64_t dual_size = qp.constraint_lower_bounds.size();
  bool primal_too_big = primal_size > largest_ok_size;
  if (primal_too_big) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Too many variables (", primal_size, ") to index with an int32_t."));
  }
  bool dual_too_big = dual_size > largest_ok_size;
  if (dual_too_big) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Too many constraints (", dual_size, ") to index with an int32_t."));
  }
  return absl::OkStatus();
}
}  // namespace internal

absl::StatusOr<MPModelProto> QpToMpModelProto(const QuadraticProgram& qp) {
  RETURN_IF_ERROR(CanFitInMpModelProto(qp));
  if (qp.objective_scaling_factor == 0) {
    return absl::InvalidArgumentError(
        "objective_scaling_factor cannot be zero.");
  }
  const int64_t primal_size = qp.variable_lower_bounds.size();
  const int64_t dual_size = qp.constraint_lower_bounds.size();
  MPModelProto proto;
  if (qp.problem_name.has_value() && !qp.problem_name->empty()) {
    proto.set_name(*qp.problem_name);
  }
  proto.set_objective_offset(qp.objective_scaling_factor * qp.objective_offset);
  if (qp.objective_scaling_factor < 0) {
    proto.set_maximize(true);
  } else {
    proto.set_maximize(false);
  }

  proto.mutable_variable()->Reserve(primal_size);
  for (int64_t i = 0; i < primal_size; ++i) {
    auto* var = proto.add_variable();
    var->set_lower_bound(qp.variable_lower_bounds[i]);
    var->set_upper_bound(qp.variable_upper_bounds[i]);
    var->set_objective_coefficient(qp.objective_scaling_factor *
                                   qp.objective_vector[i]);
    if (qp.variable_names.has_value() && i < qp.variable_names->size()) {
      const std::string& name = (*qp.variable_names)[i];
      if (!name.empty()) {
        var->set_name(name);
      }
    }
  }

  proto.mutable_constraint()->Reserve(dual_size);
  for (int64_t i = 0; i < dual_size; ++i) {
    auto* con = proto.add_constraint();
    con->set_lower_bound(qp.constraint_lower_bounds[i]);
    con->set_upper_bound(qp.constraint_upper_bounds[i]);
    if (qp.constraint_names.has_value() && i < qp.constraint_names->size()) {
      const std::string& name = (*qp.constraint_names)[i];
      if (!name.empty()) {
        con->set_name(name);
      }
    }
  }

  using InnerIterator =
      ::Eigen::SparseMatrix<double, Eigen::ColMajor, int64_t>::InnerIterator;
  for (int64_t col = 0; col < qp.constraint_matrix.cols(); ++col) {
    for (InnerIterator iter(qp.constraint_matrix, col); iter; ++iter) {
      auto* con = proto.mutable_constraint(iter.row());
      // To avoid reallocs during the inserts, we could count the nonzeros
      // and `reserve()` before filling.
      con->add_var_index(iter.col());
      con->add_coefficient(iter.value());
    }
  }

  // Some OR tools decide the objective is quadratic based on
  // `has_quadratic_objective()` rather than on
  // `quadratic_objective_size() == 0`, so don't create the quadratic objective
  // for linear programs.
  if (!IsLinearProgram(qp)) {
    auto* quadratic_objective = proto.mutable_quadratic_objective();
    const auto& diagonal = qp.objective_matrix->diagonal();
    for (int64_t i = 0; i < diagonal.size(); ++i) {
      if (diagonal[i] != 0.0) {
        quadratic_objective->add_qvar1_index(i);
        quadratic_objective->add_qvar2_index(i);
        // Undo the implicit (1/2) term in `QuadraticProgram`'s objective.
        quadratic_objective->add_coefficient(qp.objective_scaling_factor *
                                             diagonal[i] / 2.0);
      }
    }
  }

  return proto;
}

std::string ToString(const QuadraticProgram& qp, int64_t max_size) {
  auto object_name = [](int64_t index,
                        const std::optional<std::vector<std::string>>& names,
                        absl::string_view prefix) {
    if (names.has_value()) {
      CHECK_LT(index, names->size());
      return (*names)[index];
    }
    return absl::StrCat(prefix, index);
  };
  auto variable_name = [&qp, &object_name](int64_t var_index) {
    return object_name(var_index, qp.variable_names, "x");
  };
  auto constraint_name = [&qp, &object_name](int64_t con_index) {
    return object_name(con_index, qp.constraint_names, "c");
  };

  if (auto status = ValidateQuadraticProgramDimensions(qp); !status.ok()) {
    return absl::StrCat("Quadratic program with inconsistent dimensions: ",
                        status.message());
  }

  std::string result;
  if (qp.problem_name.has_value()) {
    absl::StrAppend(&result, *qp.problem_name, ":\n");
  }
  absl::StrAppend(
      &result, (qp.objective_scaling_factor < 0.0 ? "maximize " : "minimize "),
      qp.objective_scaling_factor, " * (", qp.objective_offset);
  for (int64_t i = 0; i < qp.objective_vector.size(); ++i) {
    if (qp.objective_vector[i] != 0.0) {
      absl::StrAppend(&result, " + ", qp.objective_vector[i], " ",
                      variable_name(i));
      if (result.size() >= max_size) break;
    }
  }
  if (qp.objective_matrix.has_value()) {
    result.append(" + 1/2 * (");
    auto obj_diagonal = qp.objective_matrix->diagonal();
    for (int64_t i = 0; i < obj_diagonal.size(); ++i) {
      if (obj_diagonal[i] != 0.0) {
        absl::StrAppend(&result, " + ", obj_diagonal[i], " ", variable_name(i),
                        "^2");
        if (result.size() >= max_size) break;
      }
    }
    // Closes the objective matrix expression.
    result.append(")");
  }
  // Closes the objective scaling factor expression.
  result.append(")\n");

  Eigen::SparseMatrix<double, Eigen::ColMajor, int64_t>
      constraint_matrix_transpose = qp.constraint_matrix.transpose();
  for (int64_t constraint_idx = 0;
       constraint_idx < constraint_matrix_transpose.outerSize();
       ++constraint_idx) {
    absl::StrAppend(&result, constraint_name(constraint_idx), ":");
    if (qp.constraint_lower_bounds[constraint_idx] !=
        -std::numeric_limits<double>::infinity()) {
      absl::StrAppend(&result, " ", qp.constraint_lower_bounds[constraint_idx],
                      " <=");
    }
    for (decltype(constraint_matrix_transpose)::InnerIterator it(
             constraint_matrix_transpose, constraint_idx);
         it; ++it) {
      absl::StrAppend(&result, " + ", it.value(), " ",
                      variable_name(it.index()));
      if (result.size() >= max_size) break;
    }
    if (qp.constraint_upper_bounds[constraint_idx] !=
        std::numeric_limits<double>::infinity()) {
      absl::StrAppend(&result,
                      " <= ", qp.constraint_upper_bounds[constraint_idx]);
    }
    result.append("\n");
  }
  result.append("Bounds\n");
  for (int64_t i = 0; i < qp.variable_lower_bounds.size(); ++i) {
    if (qp.variable_lower_bounds[i] ==
        -std::numeric_limits<double>::infinity()) {
      if (qp.variable_upper_bounds[i] ==
          std::numeric_limits<double>::infinity()) {
        absl::StrAppend(&result, variable_name(i), " free\n");
      } else {
        absl::StrAppend(&result, variable_name(i),
                        " <= ", qp.variable_upper_bounds[i], "\n");
      }
    } else {
      if (qp.variable_upper_bounds[i] ==
          std::numeric_limits<double>::infinity()) {
        absl::StrAppend(&result, variable_name(i),
                        " >= ", qp.variable_lower_bounds[i], "\n");

      } else {
        absl::StrAppend(&result, qp.variable_lower_bounds[i],
                        " <= ", variable_name(i),
                        " <= ", qp.variable_upper_bounds[i], "\n");
      }
    }
    if (result.size() >= max_size) break;
  }
  if (result.size() > max_size) {
    result.resize(max_size - 4);
    result.append("...\n");
  }
  return result;
}

void SetEigenMatrixFromTriplets(
    std::vector<Eigen::Triplet<double, int64_t>> triplets,
    Eigen::SparseMatrix<double, Eigen::ColMajor, int64_t>& matrix) {
  using Triplet = Eigen::Triplet<double, int64_t>;
  std::sort(triplets.begin(), triplets.end(),
            [](const Triplet& lhs, const Triplet& rhs) {
              return std::tie(lhs.col(), lhs.row()) <
                     std::tie(rhs.col(), rhs.row());
            });

  // The triplets are allowed to contain duplicate entries (and intentionally
  // do for the diagonals of the objective matrix). For efficiency of insert and
  // reserve, merge the duplicates first.
  internal::CombineRepeatedTripletsInPlace(triplets);

  std::vector<int64_t> num_column_entries(matrix.cols());
  for (const Triplet& triplet : triplets) {
    ++num_column_entries[triplet.col()];
  }
  // NOTE: `reserve()` takes column counts because matrix is in column major
  // order.
  matrix.reserve(num_column_entries);
  for (const Triplet& triplet : triplets) {
    matrix.insert(triplet.row(), triplet.col()) = triplet.value();
  }
  if (matrix.outerSize() > 0) {
    matrix.makeCompressed();
  }
}

namespace internal {
void CombineRepeatedTripletsInPlace(
    std::vector<Eigen::Triplet<double, int64_t>>& triplets) {
  if (triplets.empty()) return;
  auto output_iter = triplets.begin();
  for (auto p = output_iter + 1; p != triplets.end(); ++p) {
    if (output_iter->row() == p->row() && output_iter->col() == p->col()) {
      *output_iter = {output_iter->row(), output_iter->col(),
                      output_iter->value() + p->value()};
    } else {
      ++output_iter;
      if (output_iter != p) {  // Small optimization - skip no-op copies.
        *output_iter = *p;
      }
    }
  }
  // `*output_iter` is the last output value, so erase everything after that.
  triplets.erase(output_iter + 1, triplets.end());
}
}  // namespace internal
}  // namespace operations_research::pdlp
