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

#include "ortools/math_opt/core/solver.h"

#include <stdint.h>

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/core/model_summary.h"
#include "ortools/math_opt/core/solver_interface.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_parameters.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/parameters.pb.h"
#include "ortools/math_opt/result.pb.h"
#include "ortools/math_opt/validators/callback_validator.h"
#include "ortools/math_opt/validators/model_parameters_validator.h"
#include "ortools/math_opt/validators/model_validator.h"
#include "ortools/math_opt/validators/solution_validator.h"
#include "ortools/math_opt/validators/solver_parameters_validator.h"
#include "ortools/base/status_macros.h"

namespace operations_research {
namespace math_opt {

namespace {

template <typename IdNameContainer>
void UpdateIdNameMap(const absl::Span<const int64_t> deleted_ids,
                     const IdNameContainer& container, IdNameBiMap& bimap) {
  for (const int64_t deleted_id : deleted_ids) {
    bimap.Erase(deleted_id);
  }
  for (int i = 0; i < container.ids_size(); ++i) {
    std::string name;
    if (!container.names().empty()) {
      name = container.names(i);
    }
    bimap.Insert(container.ids(i), std::move(name));
  }
}

ModelSummary MakeSummary(const ModelProto& model) {
  ModelSummary summary;
  UpdateIdNameMap<VariablesProto>({}, model.variables(), summary.variables);
  UpdateIdNameMap<LinearConstraintsProto>({}, model.linear_constraints(),
                                          summary.linear_constraints);
  return summary;
}

void UpdateSummaryFromModelUpdate(const ModelUpdateProto& model_update,
                                  ModelSummary& summary) {
  UpdateIdNameMap<VariablesProto>(model_update.deleted_variable_ids(),
                                  model_update.new_variables(),
                                  summary.variables);
  UpdateIdNameMap<LinearConstraintsProto>(
      model_update.deleted_linear_constraint_ids(),
      model_update.new_linear_constraints(), summary.linear_constraints);
}

// Returns an InternalError with the input status message if the input status is
// not OK.
absl::Status ToInternalError(const absl::Status original) {
  if (original.ok()) {
    return original;
  }

  return absl::InternalError(original.message());
}

}  // namespace

Solver::Solver(std::unique_ptr<SolverInterface> underlying_solver,
               ModelSummary model_summary)
    : underlying_solver_(std::move(underlying_solver)),
      model_summary_(std::move(model_summary)) {
  CHECK(underlying_solver_ != nullptr);
}

absl::StatusOr<std::unique_ptr<Solver>> Solver::New(
    const SolverType solver_type, const ModelProto& model,
    const SolverInitializerProto& initializer) {
  RETURN_IF_ERROR(ValidateModel(model));
  ASSIGN_OR_RETURN(
      auto underlying_solver,
      AllSolversRegistry::Instance()->Create(solver_type, model, initializer));
  auto result = absl::WrapUnique(
      new Solver(std::move(underlying_solver), MakeSummary(model)));
  return result;
}

absl::StatusOr<SolveResultProto> Solver::Solve(
    const SolveParametersProto& parameters,
    const ModelSolveParametersProto& model_parameters,
    const CallbackRegistrationProto& callback_registration,
    const Callback user_cb) {
  // TODO(b/168037341): we should validate the result maths. Since the result
  // can be filtered, this should be included in the solver_interface
  // implementations.

  RETURN_IF_ERROR(ValidateSolverParameters(parameters)) << "invalid parameters";
  RETURN_IF_ERROR(
      ValidateModelSolveParameters(model_parameters, model_summary_))
      << "invalid model_parameters";

  SolverInterface::Callback cb = nullptr;
  if (user_cb != nullptr) {
    RETURN_IF_ERROR(
        ValidateCallbackRegistration(callback_registration, model_summary_));
    cb = [&](const CallbackDataProto& callback_data)
        -> absl::StatusOr<CallbackResultProto> {
      RETURN_IF_ERROR(ValidateCallbackDataProto(
          callback_data, callback_registration, model_summary_));
      auto callback_result = user_cb(callback_data);
      RETURN_IF_ERROR(
          ValidateCallbackResultProto(callback_result, callback_data.event(),
                                      callback_registration, model_summary_));
      return callback_result;
    };
  }

  ASSIGN_OR_RETURN(const SolveResultProto result,
                   underlying_solver_->Solve(parameters, model_parameters,
                                             callback_registration, cb));

  // We consider errors in `result` to be internal errors, but
  // `ValidateResult()` will return an InvalidArgumentError. So here we convert
  // the error.
  RETURN_IF_ERROR(ToInternalError(
      ValidateResult(result, model_parameters, model_summary_)));

  return result;
}

absl::StatusOr<bool> Solver::Update(const ModelUpdateProto& model_update) {
  RETURN_IF_ERROR(ValidateModelUpdateAndSummary(model_update, model_summary_));
  if (!underlying_solver_->CanUpdate(model_update)) {
    return false;
  }
  UpdateSummaryFromModelUpdate(model_update, model_summary_);
  RETURN_IF_ERROR(underlying_solver_->Update(model_update));
  return true;
}

}  // namespace math_opt
}  // namespace operations_research
