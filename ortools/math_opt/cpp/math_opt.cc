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

#include "ortools/math_opt/cpp/math_opt.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "ortools/base/logging.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "ortools/base/int_type.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/cpp/key_types.h"
#include "ortools/math_opt/indexed_model.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/solver.h"
#include "ortools/base/status_macros.h"

namespace operations_research {
namespace math_opt {

absl::StatusOr<Result> MathOpt::Solve(
    const SolveParametersProto& solver_parameters,
    const ModelSolveParameters& model_parameters,
    const CallbackRegistration& callback_registration, Callback callback) {
  CheckModel(model_parameters.model());
  CheckModel(callback_registration.model());
  if (callback == nullptr) {
    CHECK(callback_registration.events.empty())
        << "No callback was provided to run, but callback events were "
           "registered.";
  }

  bool attempted_incremental_solve = false;
  if (solver_ != nullptr) {
    const ModelUpdateProto model_update = model_->ExportModelUpdate();
    ASSIGN_OR_RETURN(const bool did_update, solver_->Update(model_update));
    if (did_update) {
      attempted_incremental_solve = true;
    } else {
      solver_ = nullptr;
    }
  }
  if (solver_ == nullptr) {
    ASSIGN_OR_RETURN(solver_, Solver::New(solver_type_, model_->ExportModel(),
                                          solver_initializer_));
  }
  model_->Checkpoint();

  Solver::Callback cb = nullptr;
  if (callback != nullptr) {
    cb = [&](const CallbackDataProto& callback_data_proto) {
      const CallbackData data(model_.get(), callback_data_proto);
      const CallbackResult result = callback(data);
      CheckModel(result.model());
      return result.Proto();
    };
  }
  ASSIGN_OR_RETURN(const SolveResultProto solve_result,
                   solver_->Solve(solver_parameters, model_parameters.Proto(),
                                  callback_registration.Proto(), cb));
  Result result(model_.get(), solve_result);
  result.attempted_incremental_solve = attempted_incremental_solve;
  return result;
}

LinearConstraint MathOpt::AddLinearConstraint(
    const BoundedLinearExpression& bounded_expr, absl::string_view name) {
  CheckModel(bounded_expr.expression.model());

  const LinearConstraintId constraint = model_->AddLinearConstraint(
      bounded_expr.lower_bound_minus_offset(),
      bounded_expr.upper_bound_minus_offset(), name);
  for (auto [variable, coef] : bounded_expr.expression.raw_terms()) {
    model_->set_linear_constraint_coefficient(constraint, variable, coef);
  }
  return LinearConstraint(model_.get(), constraint);
}

std::vector<Variable> MathOpt::Variables() {
  std::vector<Variable> result;
  result.reserve(model_->num_variables());
  for (const VariableId var_id : model_->variables()) {
    result.push_back(Variable(model_.get(), var_id));
  }
  return result;
}

std::vector<Variable> MathOpt::SortedVariables() {
  std::vector<Variable> result = Variables();
  std::sort(result.begin(), result.end(),
            [](const Variable& l, const Variable& r) {
              return l.typed_id() < r.typed_id();
            });
  return result;
}

std::vector<LinearConstraint> MathOpt::LinearConstraints() {
  std::vector<LinearConstraint> result;
  result.reserve(model_->num_linear_constraints());
  for (const LinearConstraintId lin_con_id : model_->linear_constraints()) {
    result.push_back(LinearConstraint(model_.get(), lin_con_id));
  }
  return result;
}

std::vector<LinearConstraint> MathOpt::SortedLinearConstraints() {
  std::vector<LinearConstraint> result = LinearConstraints();
  std::sort(result.begin(), result.end(),
            [](const LinearConstraint& l, const LinearConstraint& r) {
              return l.typed_id() < r.typed_id();
            });
  return result;
}

ModelProto MathOpt::ExportModel() const { return model_->ExportModel(); }

void MathOpt::CheckModel(IndexedModel* model) {
  if (model != nullptr) {
    CHECK_EQ(model, model_.get()) << internal::kObjectsFromOtherIndexedModel;
  }
}

}  // namespace math_opt
}  // namespace operations_research
