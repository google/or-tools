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

#include "ortools/math_opt/solvers/pdlp_bridge.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "Eigen/Core"
#include "Eigen/SparseCore"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/core/inverted_bounds.h"
#include "ortools/math_opt/core/math_opt_proto_utils.h"
#include "ortools/math_opt/core/sparse_vector_view.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/solution.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/pdlp/quadratic_program.h"

namespace operations_research {
namespace math_opt {
namespace {

constexpr SupportedProblemStructures kPdlpSupportedStructures = {
    .quadratic_objectives = SupportType::kSupported};

absl::StatusOr<SparseDoubleVectorProto> ExtractSolution(
    const Eigen::VectorXd& values, absl::Span<const int64_t> pdlp_index_to_id,
    const SparseVectorFilterProto& filter, const double scale) {
  if (values.size() != pdlp_index_to_id.size()) {
    return absl::InternalError(
        absl::StrCat("Expected solution vector with ", pdlp_index_to_id.size(),
                     " elements, found: ", values.size()));
  }
  SparseVectorFilterPredicate predicate(filter);
  SparseDoubleVectorProto result;
  for (int i = 0; i < pdlp_index_to_id.size(); ++i) {
    const double value = scale * values[i];
    const int64_t id = pdlp_index_to_id[i];
    if (predicate.AcceptsAndUpdate(id, value)) {
      result.add_ids(id);
      result.add_values(value);
    }
  }
  return result;
}

// We are implicitly assuming that all missing IDs have correspoding value 0.
Eigen::VectorXd EncodeSolution(
    const SparseDoubleVectorProto& values,
    const absl::flat_hash_map<int64_t, int64_t>& id_to_pdlp_index,
    const double scale) {
  Eigen::VectorXd pdlp_vector(Eigen::VectorXd::Zero(id_to_pdlp_index.size()));
  const int num_values = values.values_size();
  for (int k = 0; k < num_values; ++k) {
    const int64_t index = id_to_pdlp_index.at(values.ids(k));
    pdlp_vector[index] = values.values(k) / scale;
  }
  return pdlp_vector;
}

}  // namespace

absl::StatusOr<PdlpBridge> PdlpBridge::FromProto(
    const ModelProto& model_proto) {
  RETURN_IF_ERROR(
      ModelIsSupported(model_proto, kPdlpSupportedStructures, "PDLP"));
  PdlpBridge result;
  pdlp::QuadraticProgram& pdlp_lp = result.pdlp_lp_;
  const VariablesProto& variables = model_proto.variables();
  const LinearConstraintsProto& linear_constraints =
      model_proto.linear_constraints();
  pdlp_lp.ResizeAndInitialize(variables.ids_size(),
                              linear_constraints.ids_size());
  if (!model_proto.name().empty()) {
    pdlp_lp.problem_name = model_proto.name();
  }
  if (variables.names_size() > 0) {
    pdlp_lp.variable_names = {variables.names().begin(),
                              variables.names().end()};
  }
  if (linear_constraints.names_size() > 0) {
    pdlp_lp.constraint_names = {linear_constraints.names().begin(),
                                linear_constraints.names().end()};
  }
  for (int i = 0; i < variables.ids_size(); ++i) {
    result.var_id_to_pdlp_index_[variables.ids(i)] = i;
    result.pdlp_index_to_var_id_.push_back(variables.ids(i));
    pdlp_lp.variable_lower_bounds[i] = variables.lower_bounds(i);
    pdlp_lp.variable_upper_bounds[i] = variables.upper_bounds(i);
  }
  for (int i = 0; i < linear_constraints.ids_size(); ++i) {
    result.lin_con_id_to_pdlp_index_[linear_constraints.ids(i)] = i;
    result.pdlp_index_to_lin_con_id_.push_back(linear_constraints.ids(i));
    pdlp_lp.constraint_lower_bounds[i] = linear_constraints.lower_bounds(i);
    pdlp_lp.constraint_upper_bounds[i] = linear_constraints.upper_bounds(i);
  }
  const bool is_maximize = model_proto.objective().maximize();
  const double obj_scale = is_maximize ? -1.0 : 1.0;
  pdlp_lp.objective_offset = obj_scale * model_proto.objective().offset();
  for (const auto [var_id, coef] :
       MakeView(model_proto.objective().linear_coefficients())) {
    pdlp_lp.objective_vector[result.var_id_to_pdlp_index_.at(var_id)] =
        obj_scale * coef;
  }
  const SparseDoubleMatrixProto& quadratic_objective =
      model_proto.objective().quadratic_coefficients();
  const int obj_nnz = quadratic_objective.row_ids().size();
  if (obj_nnz > 0) {
    pdlp_lp.objective_matrix.emplace();
    pdlp_lp.objective_matrix->setZero(variables.ids_size());
  }
  for (int i = 0; i < obj_nnz; ++i) {
    const int64_t row_index =
        result.var_id_to_pdlp_index_.at(quadratic_objective.row_ids(i));
    const int64_t column_index =
        result.var_id_to_pdlp_index_.at(quadratic_objective.column_ids(i));
    const double value = obj_scale * quadratic_objective.coefficients(i);
    if (row_index != column_index) {
      return absl::InvalidArgumentError(
          "PDLP cannot solve problems with non-diagonal objective matrices");
    }
    // MathOpt represents quadratic objectives in "terms" form, i.e. as a sum
    // of double * Variable * Variable terms. They are stored in upper
    // triangular form with row_index <= column_index. In contrast, PDLP
    // represents quadratic objectives in "matrix" form as 1/2 x'Qx, where Q is
    // diagonal. To get to the right format, we simply double each diagonal
    // entry.
    pdlp_lp.objective_matrix->diagonal()[row_index] = 2 * value;
  }
  pdlp_lp.objective_scaling_factor = obj_scale;
  // Note: MathOpt stores the constraint data in row major order, but PDLP
  // wants the data in column major order. There is probably a more efficient
  // method to do this transformation.
  std::vector<Eigen::Triplet<double, int64_t>> mat_triplets;
  const int nnz = model_proto.linear_constraint_matrix().row_ids_size();
  mat_triplets.reserve(nnz);
  const SparseDoubleMatrixProto& proto_mat =
      model_proto.linear_constraint_matrix();
  for (int i = 0; i < nnz; ++i) {
    const int64_t row_index =
        result.lin_con_id_to_pdlp_index_.at(proto_mat.row_ids(i));
    const int64_t column_index =
        result.var_id_to_pdlp_index_.at(proto_mat.column_ids(i));
    const double value = proto_mat.coefficients(i);
    mat_triplets.emplace_back(row_index, column_index, value);
  }
  pdlp_lp.constraint_matrix.setFromTriplets(mat_triplets.begin(),
                                            mat_triplets.end());
  return result;
}

InvertedBounds PdlpBridge::ListInvertedBounds() const {
  InvertedBounds inverted_bounds;
  for (int64_t var_index = 0; var_index < pdlp_index_to_var_id_.size();
       ++var_index) {
    if (pdlp_lp_.variable_lower_bounds[var_index] >
        pdlp_lp_.variable_upper_bounds[var_index]) {
      inverted_bounds.variables.push_back(pdlp_index_to_var_id_[var_index]);
    }
  }
  for (int64_t lin_con_index = 0;
       lin_con_index < pdlp_index_to_lin_con_id_.size(); ++lin_con_index) {
    if (pdlp_lp_.constraint_lower_bounds[lin_con_index] >
        pdlp_lp_.constraint_upper_bounds[lin_con_index]) {
      inverted_bounds.linear_constraints.push_back(
          pdlp_index_to_lin_con_id_[lin_con_index]);
    }
  }
  return inverted_bounds;
}

absl::StatusOr<SparseDoubleVectorProto> PdlpBridge::PrimalVariablesToProto(
    const Eigen::VectorXd& primal_values,
    const SparseVectorFilterProto& variable_filter) const {
  return ExtractSolution(primal_values, pdlp_index_to_var_id_, variable_filter,
                         /*scale=*/1.0);
}
absl::StatusOr<SparseDoubleVectorProto> PdlpBridge::DualVariablesToProto(
    const Eigen::VectorXd& dual_values,
    const SparseVectorFilterProto& linear_constraint_filter) const {
  return ExtractSolution(dual_values, pdlp_index_to_lin_con_id_,
                         linear_constraint_filter,
                         /*scale=*/pdlp_lp_.objective_scaling_factor);
}
absl::StatusOr<SparseDoubleVectorProto> PdlpBridge::ReducedCostsToProto(
    const Eigen::VectorXd& reduced_costs,
    const SparseVectorFilterProto& variable_filter) const {
  return ExtractSolution(reduced_costs, pdlp_index_to_var_id_, variable_filter,
                         /*scale=*/pdlp_lp_.objective_scaling_factor);
}

pdlp::PrimalAndDualSolution PdlpBridge::SolutionHintToWarmStart(
    const SolutionHintProto& solution_hint) const {
  // We are implicitly assuming that all missing IDs have correspoding value 0.
  pdlp::PrimalAndDualSolution result;
  result.primal_solution = EncodeSolution(solution_hint.variable_values(),
                                          var_id_to_pdlp_index_, /*scale=*/1.0);
  result.dual_solution =
      EncodeSolution(solution_hint.dual_values(), lin_con_id_to_pdlp_index_,
                     /*scale=*/pdlp_lp_.objective_scaling_factor);
  return result;
}

}  // namespace math_opt
}  // namespace operations_research
