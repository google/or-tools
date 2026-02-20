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

#include "ortools/math_opt/io/proto_converter.h"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/base/status_macros.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/linear_solver/model_validator.h"
#include "ortools/math_opt/core/math_opt_proto_utils.h"
#include "ortools/math_opt/core/sparse_vector_view.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_parameters.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/validators/model_validator.h"

namespace operations_research {
namespace math_opt {
namespace {

absl::Status IsSupported(const MPModelProto& model) {
  std::string validity_string = FindErrorInMPModelProto(model);
  if (!validity_string.empty()) {
    return absl::InvalidArgumentError(validity_string);
  }
  for (const MPGeneralConstraintProto& general_constraint :
       model.general_constraint()) {
    if (!(general_constraint.has_quadratic_constraint() ||
          general_constraint.has_sos_constraint() ||
          general_constraint.has_indicator_constraint())) {
      return absl::InvalidArgumentError("Unsupported general constraint");
    }
  }
  return absl::OkStatus();
}

bool AnyVarNamed(const MPModelProto& model) {
  for (const MPVariableProto& var : model.variable()) {
    if (!var.name().empty()) {
      return true;
    }
  }
  return false;
}

bool AnyConstraintNamed(const MPModelProto& model) {
  for (const MPConstraintProto& constraint : model.constraint()) {
    if (!constraint.name().empty()) {
      return true;
    }
  }
  return false;
}

void LinearTermsFromMPModelToMathOpt(
    const absl::Span<const int32_t> in_ids,
    const absl::Span<const double> in_coeffs,
    google::protobuf::RepeatedField<int64_t>& out_ids,
    google::protobuf::RepeatedField<double>& out_coeffs) {
  CHECK_EQ(in_ids.size(), in_coeffs.size());
  const int num_terms = static_cast<int>(in_ids.size());
  std::vector<std::pair<int, double>> linear_terms_in_order;
  for (int i = 0; i < num_terms; ++i) {
    linear_terms_in_order.push_back({in_ids[i], in_coeffs[i]});
  }
  absl::c_sort(linear_terms_in_order);
  out_ids.Resize(num_terms, -1);
  out_coeffs.Resize(num_terms, std::numeric_limits<double>::quiet_NaN());
  for (int i = 0; i < num_terms; ++i) {
    out_ids.Set(i, linear_terms_in_order[i].first);
    out_coeffs.Set(i, linear_terms_in_order[i].second);
  }
}

// Copies quadratic terms from MPModelProto format to MathOpt format. In
// particular, MathOpt requires three things not enforced by MPModelProto:
//    1. No duplicate entries,
//    2. No lower triangular entries, and
//    3. Lexicographic sortedness of (row_id, column_id) keys.
SparseDoubleMatrixProto QuadraticTermsFromMPModelToMathOpt(
    const absl::Span<const int32_t> in_row_var_indices,
    const absl::Span<const int32_t> in_col_var_indices,
    const absl::Span<const double> in_coefficients) {
  CHECK_EQ(in_row_var_indices.size(), in_col_var_indices.size());
  CHECK_EQ(in_row_var_indices.size(), in_coefficients.size());

  SparseDoubleMatrixProto out_expression;
  std::vector<std::pair<std::pair<int32_t, int32_t>, double>> qp_terms_in_order;
  for (int k = 0; k < in_row_var_indices.size(); ++k) {
    int32_t first_index = in_row_var_indices[k];
    int32_t second_index = in_col_var_indices[k];
    if (first_index > second_index) {
      std::swap(first_index, second_index);
    }
    qp_terms_in_order.push_back(
        {{first_index, second_index}, in_coefficients[k]});
  }
  absl::c_sort(qp_terms_in_order);
  std::pair<int32_t, int32_t> previous = {-1, -1};
  for (const auto& [indices, coeff] : qp_terms_in_order) {
    if (indices == previous) {
      *out_expression.mutable_coefficients()->rbegin() += coeff;
    } else {
      out_expression.add_row_ids(indices.first);
      out_expression.add_column_ids(indices.second);
      out_expression.add_coefficients(coeff);
      previous = indices;
    }
  }
  return out_expression;
}

QuadraticConstraintProto QuadraticConstraintFromMPModelToMathOpt(
    const MPQuadraticConstraint& in_constraint, const absl::string_view name) {
  QuadraticConstraintProto out_constraint;
  out_constraint.set_lower_bound(in_constraint.lower_bound());
  out_constraint.set_upper_bound(in_constraint.upper_bound());
  out_constraint.set_name(name);
  LinearTermsFromMPModelToMathOpt(
      in_constraint.var_index(), in_constraint.coefficient(),
      *out_constraint.mutable_linear_terms()->mutable_ids(),
      *out_constraint.mutable_linear_terms()->mutable_values());
  *out_constraint.mutable_quadratic_terms() =
      QuadraticTermsFromMPModelToMathOpt(in_constraint.qvar1_index(),
                                         in_constraint.qvar2_index(),
                                         in_constraint.qcoefficient());
  return out_constraint;
}

SosConstraintProto SosConstraintFromMPModelToMathOpt(
    const MPSosConstraint& in_constraint, const absl::string_view name) {
  SosConstraintProto out_constraint;
  out_constraint.set_name(name);
  for (const int j : in_constraint.var_index()) {
    LinearExpressionProto& expr = *out_constraint.add_expressions();
    expr.add_ids(j);
    expr.add_coefficients(1.0);
  }
  for (const double weight : in_constraint.weight()) {
    out_constraint.add_weights(weight);
  }
  return out_constraint;
}

// NOTE: We ignore the `is_lazy` field in the MPIndicatorConstraint.
absl::StatusOr<IndicatorConstraintProto>
IndicatorConstraintFromMPModelToMathOpt(
    const MPIndicatorConstraint& in_constraint, const absl::string_view name) {
  IndicatorConstraintProto out_constraint;
  out_constraint.set_name(name);
  out_constraint.set_indicator_id(in_constraint.var_index());
  out_constraint.set_activate_on_zero(in_constraint.has_var_value() &&
                                      in_constraint.var_value() == 0);
  out_constraint.set_lower_bound(in_constraint.constraint().lower_bound());
  out_constraint.set_upper_bound(in_constraint.constraint().upper_bound());
  LinearTermsFromMPModelToMathOpt(
      in_constraint.constraint().var_index(),
      in_constraint.constraint().coefficient(),
      *out_constraint.mutable_expression()->mutable_ids(),
      *out_constraint.mutable_expression()->mutable_values());
  return out_constraint;
}

absl::StatusOr<MPGeneralConstraintProto> SosConstraintFromMathOptToMPModel(
    const SosConstraintProto& in_constraint,
    const MPSosConstraint::Type sos_type,
    const absl::flat_hash_map<int64_t, int>& variable_id_to_mp_position) {
  MPGeneralConstraintProto out_general_constraint;
  out_general_constraint.set_name(in_constraint.name());
  MPSosConstraint& out_constraint =
      *out_general_constraint.mutable_sos_constraint();
  out_constraint.set_type(sos_type);
  for (const double weight : in_constraint.weights()) {
    out_constraint.add_weight(weight);
  }
  for (const LinearExpressionProto& expression : in_constraint.expressions()) {
    if (expression.ids_size() != 1 || expression.coefficients(0) != 1.0 ||
        expression.offset() != 0.0) {
      return absl::InvalidArgumentError(
          "MPModelProto does not support SOS constraints with "
          "expressions that are not equivalent to a single variable");
    }
    out_constraint.add_var_index(
        variable_id_to_mp_position.at(expression.ids(0)));
  }
  return out_general_constraint;
}

}  // namespace

absl::StatusOr<::operations_research::math_opt::ModelProto>
MPModelProtoToMathOptModel(const ::operations_research::MPModelProto& model) {
  RETURN_IF_ERROR(IsSupported(model));

  ModelProto output;
  output.set_name(model.name());

  math_opt::VariablesProto* const vars = output.mutable_variables();
  int linear_objective_non_zeros = 0;
  const int num_vars = model.variable_size();
  const bool vars_have_name = AnyVarNamed(model);
  vars->mutable_lower_bounds()->Reserve(num_vars);
  vars->mutable_upper_bounds()->Reserve(num_vars);
  vars->mutable_integers()->Reserve(num_vars);
  if (vars_have_name) {
    vars->mutable_names()->Reserve(num_vars);
  }
  for (int i = 0; i < model.variable_size(); ++i) {
    const MPVariableProto& var = model.variable(i);
    if (var.objective_coefficient() != 0.0) {
      ++linear_objective_non_zeros;
    }
    vars->add_ids(i);
    vars->add_lower_bounds(var.lower_bound());
    vars->add_upper_bounds(var.upper_bound());
    vars->add_integers(var.is_integer());
    if (vars_have_name) {
      vars->add_names(var.name());
    }
  }

  math_opt::ObjectiveProto* const objective = output.mutable_objective();
  if (linear_objective_non_zeros > 0) {
    objective->mutable_linear_coefficients()->mutable_ids()->Reserve(
        linear_objective_non_zeros);
    objective->mutable_linear_coefficients()->mutable_values()->Reserve(
        linear_objective_non_zeros);
    for (int j = 0; j < num_vars; ++j) {
      const double value = model.variable(j).objective_coefficient();
      if (value == 0.0) continue;
      objective->mutable_linear_coefficients()->add_ids(j);
      objective->mutable_linear_coefficients()->add_values(value);
    }
  }
  const MPQuadraticObjective& origin_qp_terms = model.quadratic_objective();
  const int num_qp_terms = origin_qp_terms.coefficient().size();
  if (num_qp_terms > 0) {
    *objective->mutable_quadratic_coefficients() =
        QuadraticTermsFromMPModelToMathOpt(origin_qp_terms.qvar1_index(),
                                           origin_qp_terms.qvar2_index(),
                                           origin_qp_terms.coefficient());
  }
  objective->set_maximize(model.maximize());
  objective->set_offset(model.objective_offset());

  math_opt::LinearConstraintsProto* const constraints =
      output.mutable_linear_constraints();
  const int num_constraints = model.constraint_size();
  const bool constraints_have_name = AnyConstraintNamed(model);
  int num_non_zeros = 0;
  constraints->mutable_lower_bounds()->Reserve(num_constraints);
  constraints->mutable_upper_bounds()->Reserve(num_constraints);
  if (constraints_have_name) {
    constraints->mutable_names()->Reserve(num_constraints);
  }
  for (int i = 0; i < num_constraints; ++i) {
    const MPConstraintProto& constraint = model.constraint(i);
    constraints->add_ids(i);
    constraints->add_lower_bounds(constraint.lower_bound());
    constraints->add_upper_bounds(constraint.upper_bound());
    if (constraints_have_name) {
      constraints->add_names(constraint.name());
    }
    num_non_zeros += constraint.var_index_size();
  }

  SparseDoubleMatrixProto* const matrix =
      output.mutable_linear_constraint_matrix();
  matrix->mutable_column_ids()->Reserve(num_non_zeros);
  matrix->mutable_row_ids()->Reserve(num_non_zeros);
  matrix->mutable_coefficients()->Reserve(num_non_zeros);
  // This allocation is reused across loop iterations, use caution!
  std::vector<std::pair<int, double>> terms_in_order;
  for (int i = 0; i < num_constraints; ++i) {
    const MPConstraintProto& constraint = model.constraint(i);
    const int constraint_non_zeros = constraint.var_index_size();
    for (int k = 0; k < constraint_non_zeros; ++k) {
      const double coefficient = constraint.coefficient(k);
      if (coefficient == 0.0) {
        continue;
      }
      terms_in_order.emplace_back(constraint.var_index(k), coefficient);
    }
    std::sort(terms_in_order.begin(), terms_in_order.end());
    for (const auto& term : terms_in_order) {
      matrix->add_row_ids(i);
      matrix->add_column_ids(term.first);
      matrix->add_coefficients(term.second);
    }
    terms_in_order.clear();
  }

  for (const MPGeneralConstraintProto& general_constraint :
       model.general_constraint()) {
    absl::string_view in_name = general_constraint.name();
    switch (general_constraint.general_constraint_case()) {
      case MPGeneralConstraintProto::kQuadraticConstraint: {
        (*output.mutable_quadratic_constraints())
            [output.quadratic_constraints_size()] =
                QuadraticConstraintFromMPModelToMathOpt(
                    general_constraint.quadratic_constraint(), in_name);
        break;
      }
      case MPGeneralConstraintProto::kSosConstraint: {
        const MPSosConstraint& in_constraint =
            general_constraint.sos_constraint();
        switch (in_constraint.type()) {
          case operations_research::MPSosConstraint::SOS1_DEFAULT: {
            (*output
                  .mutable_sos1_constraints())[output.sos1_constraints_size()] =
                SosConstraintFromMPModelToMathOpt(in_constraint, in_name);
            break;
          }
          case operations_research::MPSosConstraint::SOS2: {
            (*output
                  .mutable_sos2_constraints())[output.sos2_constraints_size()] =
                SosConstraintFromMPModelToMathOpt(in_constraint, in_name);
            break;
          }
        }
        break;
      }
      case MPGeneralConstraintProto::kIndicatorConstraint: {
        // Note that the open-source version of ASSIGN_OR_RETURN does not
        // support inlining the new_indicator_constraint expression.
        auto& new_indicator_constraint =
            (*output.mutable_indicator_constraints())
                [output.indicator_constraints_size()];
        ASSIGN_OR_RETURN(
            new_indicator_constraint,
            IndicatorConstraintFromMPModelToMathOpt(
                general_constraint.indicator_constraint(), in_name));
        break;
      }
      default: {
        return absl::InternalError(
            "Reached unrecognized general constraint in MPModelProto");
      }
    }
  }
  return output;
}

absl::StatusOr<std::optional<SolutionHintProto>>
MPModelProtoSolutionHintToMathOptHint(const MPModelProto& model) {
  std::string validity_string = FindErrorInMPModelProto(model);
  if (!validity_string.empty()) {
    return absl::InvalidArgumentError(validity_string);
  }

  if (model.solution_hint().var_index_size() == 0) {
    return std::nullopt;
  }

  SolutionHintProto hint;
  auto& variable_values = *hint.mutable_variable_values();
  LinearTermsFromMPModelToMathOpt(
      model.solution_hint().var_index(), model.solution_hint().var_value(),
      *variable_values.mutable_ids(), *variable_values.mutable_values());

  return hint;
}

absl::StatusOr<::operations_research::MPModelProto> MathOptModelToMPModelProto(
    const ::operations_research::math_opt::ModelProto& model) {
  RETURN_IF_ERROR(ValidateModel(model).status());
  if (!model.second_order_cone_constraints().empty()) {
    return absl::InvalidArgumentError(
        "translating models with second-order cone constraints is not "
        "supported");
  }

  const bool vars_have_name = model.variables().names_size() > 0;
  const bool constraints_have_name =
      model.linear_constraints().names_size() > 0;
  absl::flat_hash_map<int64_t, int> variable_id_to_mp_position;
  absl::flat_hash_map<int64_t, MPConstraintProto*>
      constraint_id_to_mp_constraint;

  MPModelProto output;
  output.set_name(model.name());

  const int num_vars = NumVariables(model.variables());
  output.mutable_variable()->Reserve(num_vars);
  for (int j = 0; j < num_vars; ++j) {
    MPVariableProto* const variable = output.add_variable();
    variable_id_to_mp_position.emplace(model.variables().ids(j), j);
    variable->set_lower_bound(model.variables().lower_bounds(j));
    variable->set_upper_bound(model.variables().upper_bounds(j));
    variable->set_is_integer(model.variables().integers(j));
    if (vars_have_name) {
      variable->set_name(model.variables().names(j));
    }
  }

  const int num_constraints = NumConstraints(model.linear_constraints());
  output.mutable_constraint()->Reserve(num_constraints);
  for (int i = 0; i < num_constraints; ++i) {
    MPConstraintProto* const constraint = output.add_constraint();
    constraint_id_to_mp_constraint.emplace(model.linear_constraints().ids(i),
                                           constraint);
    constraint->set_lower_bound(model.linear_constraints().lower_bounds(i));
    constraint->set_upper_bound(model.linear_constraints().upper_bounds(i));
    if (constraints_have_name) {
      constraint->set_name(model.linear_constraints().names(i));
    }
  }

  output.set_maximize(model.objective().maximize());
  output.set_objective_offset(model.objective().offset());
  for (const auto& [var, coef] :
       MakeView(model.objective().linear_coefficients())) {
    const int var_position = variable_id_to_mp_position[var];
    MPVariableProto* const variable = output.mutable_variable(var_position);
    variable->set_objective_coefficient(coef);
  }
  const SparseDoubleMatrixProto& origin_qp_terms =
      model.objective().quadratic_coefficients();
  if (!origin_qp_terms.coefficients().empty()) {
    MPQuadraticObjective& destination_qp_terms =
        *output.mutable_quadratic_objective();
    for (int k = 0; k < origin_qp_terms.coefficients().size(); ++k) {
      destination_qp_terms.add_qvar1_index(
          variable_id_to_mp_position[origin_qp_terms.row_ids(k)]);
      destination_qp_terms.add_qvar2_index(
          variable_id_to_mp_position[origin_qp_terms.column_ids(k)]);
      destination_qp_terms.add_coefficient(origin_qp_terms.coefficients(k));
    }
  }

  // TODO(user): use the constraint iterator from scip_solver.cc here.
  const int constraint_non_zeros =
      model.linear_constraint_matrix().coefficients_size();
  for (int k = 0; k < constraint_non_zeros; ++k) {
    const int64_t constraint_id = model.linear_constraint_matrix().row_ids(k);
    MPConstraintProto* const constraint =
        constraint_id_to_mp_constraint[constraint_id];
    const int64_t variable_id = model.linear_constraint_matrix().column_ids(k);
    const int variable_position = variable_id_to_mp_position[variable_id];
    constraint->add_var_index(variable_position);
    const double value = model.linear_constraint_matrix().coefficients(k);
    constraint->add_coefficient(value);
  }

  for (const auto& [id, in_constraint] : model.quadratic_constraints()) {
    MPGeneralConstraintProto& out_general_constraint =
        *output.add_general_constraint();
    out_general_constraint.set_name(in_constraint.name());
    MPQuadraticConstraint& out_constraint =
        *out_general_constraint.mutable_quadratic_constraint();
    out_constraint.set_lower_bound(in_constraint.lower_bound());
    out_constraint.set_upper_bound(in_constraint.upper_bound());
    for (const auto [index, coeff] : MakeView(in_constraint.linear_terms())) {
      out_constraint.add_var_index(variable_id_to_mp_position[index]);
      out_constraint.add_coefficient(coeff);
    }
    for (int k = 0; k < in_constraint.quadratic_terms().row_ids_size(); ++k) {
      out_constraint.add_qvar1_index(
          variable_id_to_mp_position[in_constraint.quadratic_terms().row_ids(
              k)]);
      out_constraint.add_qvar2_index(
          variable_id_to_mp_position[in_constraint.quadratic_terms().column_ids(
              k)]);
      out_constraint.add_qcoefficient(
          in_constraint.quadratic_terms().coefficients(k));
    }
  }
  for (const auto& [id, in_constraint] : model.sos1_constraints()) {
    ASSIGN_OR_RETURN(*output.add_general_constraint(),
                     SosConstraintFromMathOptToMPModel(
                         in_constraint, MPSosConstraint::SOS1_DEFAULT,
                         variable_id_to_mp_position));
  }

  for (const auto& [id, in_constraint] : model.sos2_constraints()) {
    ASSIGN_OR_RETURN(
        *output.add_general_constraint(),
        SosConstraintFromMathOptToMPModel(in_constraint, MPSosConstraint::SOS2,
                                          variable_id_to_mp_position));
  }

  for (const auto& [id, in_constraint] : model.indicator_constraints()) {
    if (!in_constraint.has_indicator_id()) {
      continue;
    }
    MPGeneralConstraintProto& out_general_constraint =
        *output.add_general_constraint();
    out_general_constraint.set_name(in_constraint.name());
    MPIndicatorConstraint& out_constraint =
        *out_general_constraint.mutable_indicator_constraint();
    out_constraint.set_var_index(
        variable_id_to_mp_position[in_constraint.indicator_id()]);
    out_constraint.set_var_value(in_constraint.activate_on_zero() ? 0 : 1);
    out_constraint.mutable_constraint()->set_lower_bound(
        in_constraint.lower_bound());
    out_constraint.mutable_constraint()->set_upper_bound(
        in_constraint.upper_bound());
    for (const auto [index, coeff] : MakeView(in_constraint.expression())) {
      out_constraint.mutable_constraint()->add_var_index(
          variable_id_to_mp_position[index]);
      out_constraint.mutable_constraint()->add_coefficient(coeff);
    }
  }

  return output;
}

}  // namespace math_opt
}  // namespace operations_research
