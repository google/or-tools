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

#include "ortools/math_opt/io/proto_converter.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "ortools/base/integral_types.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/linear_solver/model_validator.h"
#include "ortools/math_opt/core/math_opt_proto_utils.h"
#include "ortools/math_opt/core/sparse_vector_view.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/validators/model_validator.h"
#include "ortools/base/status_macros.h"

namespace operations_research {
namespace math_opt {
namespace {

absl::Status IsSupported(const MPModelProto& model) {
  std::string validity_string = FindErrorInMPModelProto(model);
  if (validity_string.length() > 0) {
    return absl::InvalidArgumentError(validity_string);
  }
  if (model.general_constraint_size() > 0) {
    return absl::InvalidArgumentError("General constraints are not supported");
  }
  if (model.quadratic_objective().coefficient_size() > 0) {
    return absl::InvalidArgumentError("Quadratic objectives not supported");
  }
  if (model.solution_hint().var_index_size() > 0) {
    return absl::InvalidArgumentError("Solution Hint not supported");
  }
  return absl::OkStatus();
}

absl::Status IsSupported(const math_opt::ModelProto& model) {
  return ValidateModel(model);
}

bool AnyVarNamed(const MPModelProto& model) {
  for (const MPVariableProto& var : model.variable()) {
    if (var.name().length() > 0) {
      return true;
    }
  }
  return false;
}

bool AnyConstraintNamed(const MPModelProto& model) {
  for (const MPConstraintProto& constraint : model.constraint()) {
    if (constraint.name().length() > 0) {
      return true;
    }
  }
  return false;
}

}  // namespace

absl::StatusOr<::operations_research::math_opt::ModelProto>
MPModelProtoToMathOptModel(const ::operations_research::MPModelProto& model) {
  RETURN_IF_ERROR(IsSupported(model));

  ModelProto output;
  output.set_name(model.name());

  math_opt::VariablesProto* const vars = output.mutable_variables();
  int objective_non_zeros = 0;
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
      ++objective_non_zeros;
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
  if (objective_non_zeros > 0) {
    objective->mutable_linear_coefficients()->mutable_ids()->Reserve(
        objective_non_zeros);
    objective->mutable_linear_coefficients()->mutable_values()->Reserve(
        objective_non_zeros);
    for (int j = 0; j < num_vars; ++j) {
      const double value = model.variable(j).objective_coefficient();
      if (value == 0.0) continue;
      objective->mutable_linear_coefficients()->add_ids(j);
      objective->mutable_linear_coefficients()->add_values(value);
    }
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
      matrix->add_column_ids(i);
      matrix->add_row_ids(term.first);
      matrix->add_coefficients(term.second);
    }
    terms_in_order.clear();
  }
  return output;
}

absl::StatusOr<::operations_research::MPModelProto> MathOptModelToMPModelProto(
    const ::operations_research::math_opt::ModelProto& model) {
  RETURN_IF_ERROR(IsSupported(model));

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

  return output;
}

}  // namespace math_opt
}  // namespace operations_research
