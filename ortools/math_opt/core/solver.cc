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

#include "absl/base/thread_annotations.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/core/model_summary.h"
#include "ortools/math_opt/core/non_streamable_solver_init_arguments.h"
#include "ortools/math_opt/core/solver_debug.h"
#include "ortools/math_opt/core/solver_interface.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/parameters.pb.h"
#include "ortools/math_opt/result.pb.h"
#include "ortools/math_opt/validators/callback_validator.h"
#include "ortools/math_opt/validators/model_parameters_validator.h"
#include "ortools/math_opt/validators/model_validator.h"
#include "ortools/math_opt/validators/result_validator.h"
#include "ortools/math_opt/validators/solve_parameters_validator.h"
#include "ortools/port/proto_utils.h"

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

// RAII class that is used to return an error when concurrent calls to some
// functions are made.
//
// Usage:
//
//   // Calling f() and/or g() concurrently will return an error.
//   class A {
//    public:
//     absl::StatusOr<...> f() {
//       ASSIGN_OR_RETURN(const auto guard,
//                        ConcurrentCallsGuard::TryAcquire(mutex_));
//       ...
//     }
//
//     absl::StatusOr<...> g() {
//       ASSIGN_OR_RETURN(const auto guard,
//                        ConcurrentCallsGuard::TryAcquire(mutex_));
//       ...
//     }

//    private:
//     absl::Mutex mutex_;
//   };
//
class ConcurrentCallsGuard {
 public:
  // Returns an errors status when concurrent calls are made, or a guard that
  // must only be kept on stack during the execution of the call.
  static absl::StatusOr<ConcurrentCallsGuard> TryAcquire(absl::Mutex& mutex)
      ABSL_NO_THREAD_SAFETY_ANALYSIS {
    // ABSL_NO_THREAD_SAFETY_ANALYSIS is needed since the analyser is confused
    // by TryLock. See b/34113867, b/16712284.

    if (!mutex.TryLock()) {
      return absl::InvalidArgumentError("concurrent calls are forbidden");
    }
    return ConcurrentCallsGuard(mutex);
  }

  ConcurrentCallsGuard(const ConcurrentCallsGuard&) = delete;
  ConcurrentCallsGuard& operator=(const ConcurrentCallsGuard&) = delete;
  ConcurrentCallsGuard& operator=(ConcurrentCallsGuard&&) = delete;

  ConcurrentCallsGuard(ConcurrentCallsGuard&& other)
      : mutex_(std::exchange(other.mutex_, nullptr)) {}

  // Release the guard.
  ~ConcurrentCallsGuard() {
    if (mutex_ != nullptr) {
      mutex_->Unlock();
    }
  }

 private:
  explicit ConcurrentCallsGuard(absl::Mutex& mutex) : mutex_(&mutex) {
    mutex_->AssertHeld();
  }

  // Reset to nullptr when the class is moved by the move constructor.
  absl::Mutex* mutex_;
};

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
  RETURN_IF_ERROR(ValidateModel(model));
  ASSIGN_OR_RETURN(
      auto underlying_solver,
      AllSolversRegistry::Instance()->Create(solver_type, model, arguments));
  auto result = absl::WrapUnique(
      new Solver(std::move(underlying_solver), MakeSummary(model)));
  return result;
}

absl::StatusOr<SolveResultProto> Solver::Solve(const SolveArgs& arguments) {
  ASSIGN_OR_RETURN(const auto guard, ConcurrentCallsGuard::TryAcquire(mutex_));

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

  ASSIGN_OR_RETURN(const SolveResultProto result,
                   underlying_solver_->Solve(arguments.parameters,
                                             arguments.model_parameters,
                                             arguments.message_callback,
                                             arguments.callback_registration,
                                             cb, arguments.interrupter));

  // We consider errors in `result` to be internal errors, but
  // `ValidateResult()` will return an InvalidArgumentError. So here we convert
  // the error.
  RETURN_IF_ERROR(ToInternalError(
      ValidateResult(result, arguments.model_parameters, model_summary_)));

  return result;
}

absl::StatusOr<bool> Solver::Update(const ModelUpdateProto& model_update) {
  ASSIGN_OR_RETURN(const auto guard, ConcurrentCallsGuard::TryAcquire(mutex_));

  RETURN_IF_ERROR(ValidateModelUpdateAndSummary(model_update, model_summary_));
  if (!underlying_solver_->CanUpdate(model_update)) {
    return false;
  }
  UpdateSummaryFromModelUpdate(model_update, model_summary_);
  RETURN_IF_ERROR(underlying_solver_->Update(model_update));
  return true;
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
