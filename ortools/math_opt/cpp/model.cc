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

#include "ortools/math_opt/cpp/model.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "ortools/base/check.h"
#include "ortools/base/status_macros.h"
#include "ortools/base/strong_int.h"
#include "ortools/math_opt/constraints/indicator/indicator_constraint.h"
#include "ortools/math_opt/constraints/quadratic/quadratic_constraint.h"
#include "ortools/math_opt/constraints/sos/sos1_constraint.h"
#include "ortools/math_opt/constraints/sos/sos2_constraint.h"
#include "ortools/math_opt/constraints/util/model_util.h"
#include "ortools/math_opt/cpp/linear_constraint.h"
#include "ortools/math_opt/cpp/update_tracker.h"
#include "ortools/math_opt/cpp/variable_and_expressions.h"
#include "ortools/math_opt/storage/model_storage.h"
#include "ortools/math_opt/storage/model_storage_types.h"
#include "ortools/math_opt/storage/sparse_coefficient_map.h"
#include "ortools/math_opt/storage/sparse_matrix.h"

namespace operations_research {
namespace math_opt {

constexpr double kInf = std::numeric_limits<double>::infinity();

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

std::unique_ptr<Model> Model::Clone(
    const std::optional<absl::string_view> new_name) const {
  return std::make_unique<Model>(storage_->Clone(new_name));
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

std::vector<LinearConstraint> Model::ColumnNonzeros(
    const Variable variable) const {
  CheckModel(variable.storage());
  std::vector<LinearConstraint> result;
  for (const LinearConstraintId constraint :
       storage()->linear_constraints_with_variable(variable.typed_id())) {
    result.push_back(LinearConstraint(storage(), constraint));
  }
  return result;
}

std::vector<Variable> Model::RowNonzeros(
    const LinearConstraint constraint) const {
  CheckModel(constraint.storage());
  std::vector<Variable> result;
  for (const VariableId variable :
       storage()->variables_in_linear_constraint(constraint.typed_id())) {
    result.push_back(Variable(storage(), variable));
  }
  return result;
}

std::vector<LinearConstraint> Model::LinearConstraints() const {
  std::vector<LinearConstraint> result;
  result.reserve(storage()->num_linear_constraints());
  for (const LinearConstraintId lin_con_id : storage()->LinearConstraints()) {
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
  CHECK_EQ(storage()->num_quadratic_objective_terms(), 0)
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
  for (const auto& [v1, v2, coef] : storage()->quadratic_objective_terms()) {
    result +=
        QuadraticTerm(Variable(storage(), v1), Variable(storage(), v2), coef);
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

std::ostream& operator<<(std::ostream& ostr, const Model& model) {
  ostr << "Model";
  if (!model.name().empty()) ostr << " " << model.name();
  ostr << ":\n";

  ostr << " Objective:\n"
       << (model.is_maximize() ? "  maximize " : "  minimize ")
       << model.ObjectiveAsQuadraticExpression() << "\n";

  ostr << " Linear constraints:\n";
  for (const LinearConstraint constraint : model.SortedLinearConstraints()) {
    ostr << "  " << constraint << ": " << constraint.AsBoundedLinearExpression()
         << "\n";
  }

  if (model.num_quadratic_constraints() > 0) {
    ostr << " Quadratic constraints:\n";
    for (const QuadraticConstraint constraint :
         model.SortedQuadraticConstraints()) {
      ostr << "  " << constraint << ": "
           << constraint.AsBoundedQuadraticExpression() << "\n";
    }
  }

  if (model.num_sos1_constraints() > 0) {
    ostr << " SOS1 constraints:\n";
    for (const Sos1Constraint constraint : model.SortedSos1Constraints()) {
      ostr << "  " << constraint << ": " << constraint.ToString() << "\n";
    }
  }

  if (model.num_sos2_constraints() > 0) {
    ostr << " SOS2 constraints:\n";
    for (const Sos2Constraint constraint : model.SortedSos2Constraints()) {
      ostr << "  " << constraint << ": " << constraint.ToString() << "\n";
    }
  }

  if (model.num_indicator_constraints() > 0) {
    ostr << " Indicator constraints:\n";
    for (const IndicatorConstraint constraint :
         model.SortedIndicatorConstraints()) {
      ostr << "  " << constraint << ": " << constraint.ToString() << "\n";
    }
  }

  ostr << " Variables:\n";
  for (const Variable v : model.SortedVariables()) {
    ostr << "  " << v;
    if (v.is_integer()) {
      if (v.lower_bound() == 0 && v.upper_bound() == 1) {
        ostr << " (binary)\n";
        continue;
      }
      ostr << " (integer)";
    }
    ostr << " in ";
    if (v.lower_bound() == -kInf) {
      ostr << "(-∞";
    } else {
      ostr << "[" << RoundTripDoubleFormat(v.lower_bound());
    }
    ostr << ", ";
    if (v.upper_bound() == kInf) {
      ostr << "+∞)";
    } else {
      ostr << RoundTripDoubleFormat(v.upper_bound()) << "]";
    }
    ostr << "\n";
  }
  return ostr;
}

// ------------------------- Quadratic constraints -----------------------------

QuadraticConstraint Model::AddQuadraticConstraint(
    const BoundedQuadraticExpression& bounded_expr,
    const absl::string_view name) {
  CheckOptionalModel(bounded_expr.expression.storage());
  SparseCoefficientMap linear_terms;
  for (const auto [var, coeff] : bounded_expr.expression.linear_terms()) {
    linear_terms.set(var.typed_id(), coeff);
  }
  SparseSymmetricMatrix quadratic_terms;
  for (const auto& [var_ids, coeff] :
       bounded_expr.expression.raw_quadratic_terms()) {
    quadratic_terms.set(var_ids.first, var_ids.second, coeff);
  }
  const QuadraticConstraintId id =
      storage()->AddAtomicConstraint(QuadraticConstraintData{
          .lower_bound = bounded_expr.lower_bound_minus_offset(),
          .upper_bound = bounded_expr.upper_bound_minus_offset(),
          .linear_terms = std::move(linear_terms),
          .quadratic_terms = std::move(quadratic_terms),
          .name = std::string(name),
      });
  return QuadraticConstraint(storage(), id);
}

// --------------------------- SOS1 constraints --------------------------------

namespace {

template <typename SosData>
SosData MakeSosData(const std::vector<LinearExpression>& expressions,
                    std::vector<double> weights, const absl::string_view name) {
  std::vector<typename SosData::LinearExpression> storage_expressions;
  storage_expressions.reserve(expressions.size());
  for (const LinearExpression& expr : expressions) {
    typename SosData::LinearExpression& storage_expr =
        storage_expressions.emplace_back();
    storage_expr.offset = expr.offset();
    for (const auto [var, coeff] : expr.raw_terms()) {
      storage_expr.terms[var] = coeff;
    }
  }
  return SosData(std::move(storage_expressions), std::move(weights),
                 std::string(name));
}

}  // namespace

Sos1Constraint Model::AddSos1Constraint(
    const std::vector<LinearExpression>& expressions,
    std::vector<double> weights, const absl::string_view name) {
  for (const LinearExpression& expr : expressions) {
    CheckOptionalModel(expr.storage());
  }
  const Sos1ConstraintId id = storage()->AddAtomicConstraint(
      MakeSosData<Sos1ConstraintData>(expressions, std::move(weights), name));
  return Sos1Constraint(storage(), id);
}

// --------------------------- SOS2 constraints --------------------------------

Sos2Constraint Model::AddSos2Constraint(
    const std::vector<LinearExpression>& expressions,
    std::vector<double> weights, const absl::string_view name) {
  for (const LinearExpression& expr : expressions) {
    CheckOptionalModel(expr.storage());
  }
  const Sos2ConstraintId id = storage()->AddAtomicConstraint(
      MakeSosData<Sos2ConstraintData>(expressions, std::move(weights), name));
  return Sos2Constraint(storage(), id);
}

// --------------------------- Indicator constraints ---------------------------

IndicatorConstraint Model::AddIndicatorConstraint(
    const Variable indicator_variable,
    const BoundedLinearExpression& implied_constraint,
    const bool activate_on_zero, const absl::string_view name) {
  CheckModel(indicator_variable.storage());
  CheckOptionalModel(implied_constraint.expression.storage());
  // We ignore the offset while unpacking here; instead, we account for it below
  // by using the `{lower,upper}_bound_minus_offset` member functions.
  auto [expr, _] = FromLinearExpression(implied_constraint.expression);
  const IndicatorConstraintId id =
      storage()->AddAtomicConstraint(IndicatorConstraintData{
          .lower_bound = implied_constraint.lower_bound_minus_offset(),
          .upper_bound = implied_constraint.upper_bound_minus_offset(),
          .linear_terms = std::move(expr),
          .indicator = indicator_variable.typed_id(),
          .activate_on_zero = activate_on_zero,
          .name = std::string(name),
      });
  return IndicatorConstraint(storage(), id);
}

}  // namespace math_opt
}  // namespace operations_research
