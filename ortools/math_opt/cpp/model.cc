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

#include "ortools/math_opt/cpp/model.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "ortools/base/logging.h"
#include "ortools/base/status_macros.h"
#include "ortools/base/strong_int.h"
#include "ortools/math_opt/core/model_storage.h"

namespace operations_research {
namespace math_opt {

absl::StatusOr<std::unique_ptr<Model>> Model::FromModelProto(
    const ModelProto& model_proto) {
  ASSIGN_OR_RETURN(std::unique_ptr<ModelStorage> storage,
                   ModelStorage::FromModelProto(model_proto));
  return std::make_unique<Model>(std::move(storage));
}

Model::Model(const absl::string_view name)
    : storage_(std::make_shared<ModelStorage>(name)) {}

Model::Model(std::unique_ptr<ModelStorage> storage)
    : storage_(std::move(storage)) {}

std::unique_ptr<Model> Model::Clone() const {
  return std::make_unique<Model>(storage_->Clone());
}

LinearConstraint Model::AddLinearConstraint(
    const BoundedLinearExpression& bounded_expr, absl::string_view name) {
  CheckOptionalModel(bounded_expr.expression.storage());

  const LinearConstraintId constraint = storage()->AddLinearConstraint(
      bounded_expr.lower_bound_minus_offset(),
      bounded_expr.upper_bound_minus_offset(), name);
  for (auto [variable, coef] : bounded_expr.expression.raw_terms()) {
    storage()->set_linear_constraint_coefficient(constraint, variable, coef);
  }
  return LinearConstraint(storage(), constraint);
}

std::vector<Variable> Model::Variables() const {
  std::vector<Variable> result;
  result.reserve(storage()->num_variables());
  for (const VariableId var_id : storage()->variables()) {
    result.push_back(Variable(storage(), var_id));
  }
  return result;
}

std::vector<Variable> Model::SortedVariables() const {
  std::vector<Variable> result = Variables();
  std::sort(result.begin(), result.end(),
            [](const Variable& l, const Variable& r) {
              return l.typed_id() < r.typed_id();
            });
  return result;
}

std::vector<LinearConstraint> Model::ColumnNonzeros(const Variable variable) {
  CheckModel(variable.storage());
  std::vector<LinearConstraint> result;
  for (const LinearConstraintId constraint :
       storage()->linear_constraints_with_variable(variable.typed_id())) {
    result.push_back(LinearConstraint(storage(), constraint));
  }
  return result;
}

std::vector<Variable> Model::RowNonzeros(const LinearConstraint constraint) {
  CheckModel(constraint.storage());
  std::vector<Variable> result;
  for (const VariableId variable :
       storage()->variables_in_linear_constraint(constraint.typed_id())) {
    result.push_back(Variable(storage(), variable));
  }
  return result;
}

BoundedLinearExpression Model::AsBoundedLinearExpression(
    const LinearConstraint constraint) {
  CheckModel(constraint.storage());
  LinearExpression terms;
  for (const VariableId var :
       storage()->variables_in_linear_constraint(constraint.typed_id())) {
    terms +=
        Variable(storage(), var) *
        storage()->linear_constraint_coefficient(constraint.typed_id(), var);
  }
  return storage()->linear_constraint_lower_bound(constraint.typed_id()) <=
         std::move(terms) <=
         storage()->linear_constraint_upper_bound(constraint.typed_id());
}

std::vector<LinearConstraint> Model::LinearConstraints() const {
  std::vector<LinearConstraint> result;
  result.reserve(storage()->num_linear_constraints());
  for (const LinearConstraintId lin_con_id : storage()->linear_constraints()) {
    result.push_back(LinearConstraint(storage(), lin_con_id));
  }
  return result;
}

std::vector<LinearConstraint> Model::SortedLinearConstraints() const {
  std::vector<LinearConstraint> result = LinearConstraints();
  std::sort(result.begin(), result.end(),
            [](const LinearConstraint& l, const LinearConstraint& r) {
              return l.typed_id() < r.typed_id();
            });
  return result;
}

void Model::SetObjective(const LinearExpression& objective,
                         const bool is_maximize) {
  CheckOptionalModel(objective.storage());
  storage()->clear_objective();
  storage()->set_is_maximize(is_maximize);
  storage()->set_objective_offset(objective.offset());
  for (auto [var, coef] : objective.raw_terms()) {
    storage()->set_linear_objective_coefficient(var, coef);
  }
}

void Model::SetObjective(const QuadraticExpression& objective,
                         const bool is_maximize) {
  CheckOptionalModel(objective.storage());
  storage()->clear_objective();
  storage()->set_is_maximize(is_maximize);
  storage()->set_objective_offset(objective.offset());
  for (auto [var, coef] : objective.raw_linear_terms()) {
    storage()->set_linear_objective_coefficient(var, coef);
  }
  for (auto [vars, coef] : objective.raw_quadratic_terms()) {
    storage()->set_quadratic_objective_coefficient(vars.first, vars.second,
                                                   coef);
  }
}

void Model::AddToObjective(const LinearExpression& objective_terms) {
  CheckOptionalModel(objective_terms.storage());
  storage()->set_objective_offset(objective_terms.offset() +
                                  storage()->objective_offset());
  for (auto [var, coef] : objective_terms.raw_terms()) {
    storage()->set_linear_objective_coefficient(
        var, coef + storage()->linear_objective_coefficient(var));
  }
}

void Model::AddToObjective(const QuadraticExpression& objective_terms) {
  CheckOptionalModel(objective_terms.storage());
  storage()->set_objective_offset(objective_terms.offset() +
                                  storage()->objective_offset());
  for (auto [var, coef] : objective_terms.raw_linear_terms()) {
    storage()->set_linear_objective_coefficient(
        var, coef + storage()->linear_objective_coefficient(var));
  }
  for (auto [vars, coef] : objective_terms.raw_quadratic_terms()) {
    storage()->set_quadratic_objective_coefficient(
        vars.first, vars.second,
        coef + storage()->quadratic_objective_coefficient(vars.first,
                                                          vars.second));
  }
}

LinearExpression Model::ObjectiveAsLinearExpression() const {
  CHECK(storage()->quadratic_objective().empty())
      << "The objective function contains quadratic terms and cannot be "
         "represented as a LinearExpression";
  LinearExpression result = storage()->objective_offset();
  for (const auto& [v, coef] : storage()->linear_objective()) {
    result += Variable(storage(), v) * coef;
  }
  return result;
}

QuadraticExpression Model::ObjectiveAsQuadraticExpression() const {
  QuadraticExpression result = storage()->objective_offset();
  for (const auto& [v, coef] : storage()->linear_objective()) {
    result += Variable(storage(), v) * coef;
  }
  for (const auto& [vars, coef] : storage()->quadratic_objective()) {
    result += QuadraticTerm(Variable(storage(), vars.first),
                            Variable(storage(), vars.second), coef);
  }
  return result;
}

ModelProto Model::ExportModel() const { return storage()->ExportModel(); }

std::unique_ptr<UpdateTracker> Model::NewUpdateTracker() {
  return std::make_unique<UpdateTracker>(storage_);
}

absl::Status Model::ApplyUpdateProto(const ModelUpdateProto& update_proto) {
  return storage()->ApplyUpdateProto(update_proto);
}

}  // namespace math_opt
}  // namespace operations_research
