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

#include "ortools/math_opt/core/solver.h"

#include <memory>
#include <utility>

#include "absl/log/check.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/core/concurrent_calls_guard.h"
#include "ortools/math_opt/core/math_opt_proto_utils.h"
#include "ortools/math_opt/core/model_summary.h"
#include "ortools/math_opt/core/non_streamable_solver_init_arguments.h"
#include "ortools/math_opt/core/solver_debug.h"
#include "ortools/math_opt/core/solver_interface.h"
#include "ortools/math_opt/infeasible_subsystem.pb.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/parameters.pb.h"
#include "ortools/math_opt/result.pb.h"
#include "ortools/math_opt/validators/callback_validator.h"
#include "ortools/math_opt/validators/infeasible_subsystem_validator.h"
#include "ortools/math_opt/validators/model_parameters_validator.h"
#include "ortools/math_opt/validators/model_validator.h"
#include "ortools/math_opt/validators/result_validator.h"
#include "ortools/math_opt/validators/solve_parameters_validator.h"
#include "ortools/port/proto_utils.h"

namespace operations_research {
namespace math_opt {

namespace {

// Returns an InternalError with the input status message if the input status is
// not OK.
absl::Status ToInternalError(absl::Status original) {
  if (original.ok()) {
    return original;
  }

  return absl::InternalError(original.message());
}

// Returns the Status returned by Solve() & Update() when called after a
// previous call to one of them failed.
absl::Status PreviousFatalFailureOccurred() {
  return absl::InvalidArgumentError(
      "a previous call to Solve(), ComputeInfeasibleSubsystem(), or Update() "
      "failed, the Solver can't be used anymore");
}

}  // namespace

absl::StatusOr<SolveResultProto> Solver::NonIncrementalSolve(
    const ModelProto& model, const SolverTypeProto solver_type,
    const InitArgs& init_args, const SolveArgs& solve_args) {
  ASSIGN_OR_RETURN(std::unique_ptr<Solver> solver,
                   Solver::New(solver_type, model, init_args));
  return solver->Solve(solve_args);
}

Solver::Solver(std::unique_ptr<SolverInterface> underlying_solver,
               ModelSummary model_summary)
    : underlying_solver_(std::move(underlying_solver)),
      model_summary_(std::move(model_summary)) {
  CHECK(underlying_solver_ != nullptr);
  ++internal::debug_num_solver;
}

Solver::~Solver() { --internal::debug_num_solver; }

absl::StatusOr<std::unique_ptr<Solver>> Solver::New(
    const SolverTypeProto solver_type, const ModelProto& model,
    const InitArgs& arguments) {
  RETURN_IF_ERROR(internal::ValidateInitArgs(arguments, solver_type));
  ASSIGN_OR_RETURN(ModelSummary summary, ValidateModel(model));
  ASSIGN_OR_RETURN(
      auto underlying_solver,
      AllSolversRegistry::Instance()->Create(solver_type, model, arguments));
  auto result = absl::WrapUnique(
      new Solver(std::move(underlying_solver), std::move(summary)));
  return result;
}

absl::StatusOr<SolveResultProto> Solver::Solve(const SolveArgs& arguments) {
  ASSIGN_OR_RETURN(const auto guard,
                   ConcurrentCallsGuard::TryAcquire(concurrent_calls_tracker_));

  if (fatal_failure_occurred_) {
    return PreviousFatalFailureOccurred();
  }
  CHECK(underlying_solver_ != nullptr);

  // We will reset it in code paths where no error occur.
  fatal_failure_occurred_ = true;

  // TODO(b/168037341): we should validate the result maths. Since the result
  // can be filtered, this should be included in the solver_interface
  // implementations.

  RETURN_IF_ERROR(ValidateSolveParameters(arguments.parameters))
      << "invalid parameters";
  RETURN_IF_ERROR(
      ValidateModelSolveParameters(arguments.model_parameters, model_summary_))
      << "invalid model_parameters";

  SolverInterface::Callback cb = nullptr;
  if (arguments.user_cb != nullptr) {
    RETURN_IF_ERROR(ValidateCallbackRegistration(
        arguments.callback_registration, model_summary_));
    cb = [&](const CallbackDataProto& callback_data)
        -> absl::StatusOr<CallbackResultProto> {
      RETURN_IF_ERROR(ValidateCallbackDataProto(
          callback_data, arguments.callback_registration, model_summary_));
      auto callback_result = arguments.user_cb(callback_data);
      RETURN_IF_ERROR(ValidateCallbackResultProto(
          callback_result, callback_data.event(),
          arguments.callback_registration, model_summary_));
      return callback_result;
    };
  }

  ASSIGN_OR_RETURN(SolveResultProto result,
                   underlying_solver_->Solve(arguments.parameters,
                                             arguments.model_parameters,
                                             arguments.message_callback,
                                             arguments.callback_registration,
                                             cb, arguments.interrupter));
  // TODO(b/290091715): Remove once language specific structs can use new
  // messages.
  UpgradeSolveResultProtoForStatsMigration(result);
  // We consider errors in `result` to be internal errors, but
  // `ValidateResult()` will return an InvalidArgumentError. So here we convert
  // the error.
  RETURN_IF_ERROR(ToInternalError(
      ValidateResult(result, arguments.model_parameters, model_summary_)));

  fatal_failure_occurred_ = false;
  return result;
}

absl::StatusOr<bool> Solver::Update(const ModelUpdateProto model_update) {
  ASSIGN_OR_RETURN(const auto guard,
                   ConcurrentCallsGuard::TryAcquire(concurrent_calls_tracker_));

  if (fatal_failure_occurred_) {
    return PreviousFatalFailureOccurred();
  }
  CHECK(underlying_solver_ != nullptr);

  // We will reset it in code paths where no error occur.
  fatal_failure_occurred_ = true;

  RETURN_IF_ERROR(ValidateModelUpdate(model_update, model_summary_));
  ASSIGN_OR_RETURN(const bool updated,
                   underlying_solver_->Update(model_update));
  if (!updated) {
    // We only destroy underlying_solver_ in this specific case as it would be
    // incorrect to destroy if the solver is GLPK and the error is that we are
    // trying to use it in a different thread. Here we know this is not the case
    // as Update() would have returned an error.
    underlying_solver_ = nullptr;
    return false;
  }

  fatal_failure_occurred_ = false;

  return true;
}

absl::StatusOr<ComputeInfeasibleSubsystemResultProto>
Solver::ComputeInfeasibleSubsystem(
    const ComputeInfeasibleSubsystemArgs& arguments) {
  ASSIGN_OR_RETURN(const auto guard,
                   ConcurrentCallsGuard::TryAcquire(concurrent_calls_tracker_));

  if (fatal_failure_occurred_) {
    return PreviousFatalFailureOccurred();
  }
  CHECK(underlying_solver_ != nullptr);

  // We will reset it in code paths where no error occur.
  fatal_failure_occurred_ = true;

  RETURN_IF_ERROR(ValidateSolveParameters(arguments.parameters))
      << "invalid parameters";

  ASSIGN_OR_RETURN(ComputeInfeasibleSubsystemResultProto result,
                   underlying_solver_->ComputeInfeasibleSubsystem(
                       arguments.parameters, arguments.message_callback,
                       arguments.interrupter));

  // We consider errors in `result` to be internal errors, but
  // `ValidateInfeasibleSubsystemResult()` will return an InvalidArgumentError.
  // So here we convert the error.
  RETURN_IF_ERROR(ToInternalError(
      ValidateComputeInfeasibleSubsystemResult(result, model_summary_)));

  fatal_failure_occurred_ = false;
  return result;
}

absl::StatusOr<ComputeInfeasibleSubsystemResultProto>
Solver::NonIncrementalComputeInfeasibleSubsystem(
    const ModelProto& model, const SolverTypeProto solver_type,
    const InitArgs& init_args,
    const ComputeInfeasibleSubsystemArgs& compute_infeasible_subsystem_args) {
  ASSIGN_OR_RETURN(std::unique_ptr<Solver> solver,
                   Solver::New(solver_type, model, init_args));
  return solver->ComputeInfeasibleSubsystem(compute_infeasible_subsystem_args);
}

namespace internal {

absl::Status ValidateInitArgs(const Solver::InitArgs& init_args,
                              const SolverTypeProto solver_type) {
  if (solver_type == SOLVER_TYPE_UNSPECIFIED) {
    return absl::InvalidArgumentError(
        "can't use SOLVER_TYPE_UNSPECIFIED as solver_type parameter");
  }

  if (init_args.non_streamable != nullptr &&
      init_args.non_streamable->solver_type() != solver_type) {
    return absl::InvalidArgumentError(
        absl::StrCat("input non_streamable init arguments are for ",
                     ProtoEnumToString(init_args.non_streamable->solver_type()),
                     " but solver_type is ", ProtoEnumToString(solver_type)));
  }

  return absl::OkStatus();
}

}  // namespace internal
}  // namespace math_opt
}  // namespace operations_research
