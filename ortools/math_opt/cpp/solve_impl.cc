// Copyright 2010-2024 Google LLC
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

#include "ortools/math_opt/cpp/solve_impl.h"

#include <memory>
#include <optional>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "absl/log/check.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/core/base_solver.h"
#include "ortools/math_opt/cpp/compute_infeasible_subsystem_arguments.h"
#include "ortools/math_opt/cpp/compute_infeasible_subsystem_result.h"
#include "ortools/math_opt/cpp/model.h"
#include "ortools/math_opt/cpp/parameters.h"
#include "ortools/math_opt/cpp/solve_arguments.h"
#include "ortools/math_opt/cpp/solve_result.h"
#include "ortools/math_opt/cpp/update_result.h"
#include "ortools/math_opt/cpp/update_tracker.h"
#include "ortools/math_opt/storage/model_storage.h"
#include "ortools/util/solve_interrupter.h"
#include "ortools/util/status_macros.h"

namespace operations_research::math_opt::internal {
namespace {

absl::StatusOr<SolveResult> CallSolve(
    BaseSolver& solver, const ModelStorage* const expected_storage,
    const SolveArguments& arguments, SolveInterrupter& local_canceller) {
  RETURN_IF_ERROR(arguments.CheckModelStorageAndCallback(expected_storage));

  BaseSolver::Callback cb = nullptr;
  absl::Mutex mutex;
  absl::Status cb_status;  // Guarded by `mutex`.
  if (arguments.callback != nullptr) {
    cb = [&](const CallbackDataProto& callback_data_proto) {
      const CallbackData data(expected_storage, callback_data_proto);
      const CallbackResult result = arguments.callback(data);
      if (const absl::Status status =
              result.CheckModelStorage(expected_storage);
          !status.ok()) {
        // Note that we use util::StatusBuilder() here as util::Annotate() is
        // not available in open-source code.
        util::StatusBuilder builder(status);
        builder << "invalid CallbackResult returned by user callback";

        {  // Limit `lock` scope.
          const absl::MutexLock lock(&mutex);
          cb_status.Update(builder);
        }

        // Trigger subprocess cancellation.
        local_canceller.Interrupt();

        // Trigger early termination of the solve if cancellation is not
        // supported (i.e. in in-process solve).
        CallbackResultProto result_proto;
        result_proto.set_terminate(true);
        return result_proto;
      }
      return result.Proto();
    };
  }

  ASSIGN_OR_RETURN(ModelSolveParametersProto model_parameters,
                   arguments.model_parameters.Proto());
  const absl::StatusOr<SolveResultProto> solve_result_proto = solver.Solve(
      {.parameters = arguments.parameters.Proto(),
       .model_parameters = std::move(model_parameters),
       .message_callback = arguments.message_callback,
       .callback_registration = arguments.callback_registration.Proto(),
       .user_cb = std::move(cb),
       .interrupter = arguments.interrupter});

  // solver.Solve() returns an error on cancellation by local_canceller but in
  // that case we want to ignore this error and return status generated in the
  // callback instead.
  {  // Limit `lock` scope.
    const absl::MutexLock lock(&mutex);
    RETURN_IF_ERROR(cb_status);
  }

  if (!solve_result_proto.ok()) {
    return solve_result_proto.status();
  }

  return SolveResult::FromProto(expected_storage, solve_result_proto.value());
}

absl::StatusOr<ComputeInfeasibleSubsystemResult> CallComputeInfeasibleSubsystem(
    BaseSolver& solver, const ModelStorage* const expected_storage,
    const ComputeInfeasibleSubsystemArguments& arguments,
    SolveInterrupter& local_canceller) {
  ASSIGN_OR_RETURN(
      const ComputeInfeasibleSubsystemResultProto compute_result_proto,
      solver.ComputeInfeasibleSubsystem(
          {.parameters = arguments.parameters.Proto(),
           .message_callback = arguments.message_callback,
           .interrupter = arguments.interrupter}));

  return ComputeInfeasibleSubsystemResult::FromProto(expected_storage,
                                                     compute_result_proto);
}

}  // namespace

absl::StatusOr<SolveResult> SolveImpl(
    const BaseSolverFactory solver_factory, const Model& model,
    const SolverType solver_type, const SolveArguments& solve_args,
    const SolveInterrupter* const user_canceller, const bool remove_names) {
  SolveInterrupter local_canceller;
  const ScopedSolveInterrupterCallback user_canceller_cb(
      user_canceller, [&]() { local_canceller.Interrupt(); });
  ASSIGN_OR_RETURN(
      const std::unique_ptr<BaseSolver> solver,
      solver_factory(EnumToProto(solver_type), model.ExportModel(remove_names),
                     &local_canceller));
  return CallSolve(*solver, model.storage(), solve_args, local_canceller);
}

absl::StatusOr<ComputeInfeasibleSubsystemResult> ComputeInfeasibleSubsystemImpl(
    const BaseSolverFactory solver_factory, const Model& model,
    const SolverType solver_type,
    const ComputeInfeasibleSubsystemArguments& compute_args,
    const SolveInterrupter* const user_canceller, const bool remove_names) {
  SolveInterrupter local_canceller;
  const ScopedSolveInterrupterCallback user_canceller_cb(
      user_canceller, [&]() { local_canceller.Interrupt(); });
  ASSIGN_OR_RETURN(
      const std::unique_ptr<BaseSolver> subprocess_solver,
      solver_factory(EnumToProto(solver_type), model.ExportModel(remove_names),
                     &local_canceller));
  return CallComputeInfeasibleSubsystem(*subprocess_solver, model.storage(),
                                        compute_args, local_canceller);
}

absl::StatusOr<std::unique_ptr<IncrementalSolverImpl>>
IncrementalSolverImpl::New(BaseSolverFactory solver_factory, Model* const model,
                           const SolverType solver_type,
                           const SolveInterrupter* const user_canceller,
                           const bool remove_names) {
  if (model == nullptr) {
    return absl::InvalidArgumentError("input model can't be null");
  }
  auto local_canceller = std::make_shared<SolveInterrupter>();
  auto user_canceller_cb =
      std::make_unique<const ScopedSolveInterrupterCallback>(
          user_canceller,
          [local_canceller]() { local_canceller->Interrupt(); });
  std::unique_ptr<UpdateTracker> update_tracker = model->NewUpdateTracker();
  ASSIGN_OR_RETURN(ModelProto model_proto,
                   update_tracker->ExportModel(remove_names));
  ASSIGN_OR_RETURN(
      std::unique_ptr<BaseSolver> solver,
      solver_factory(EnumToProto(solver_type), std::move(model_proto),
                     local_canceller.get()));
  return absl::WrapUnique<IncrementalSolverImpl>(new IncrementalSolverImpl(
      std::move(solver_factory), solver_type, remove_names,
      std::move(local_canceller), std::move(user_canceller_cb),
      model->storage(), std::move(update_tracker), std::move(solver)));
}

IncrementalSolverImpl::IncrementalSolverImpl(
    BaseSolverFactory solver_factory, SolverType solver_type,
    const bool remove_names, std::shared_ptr<SolveInterrupter> local_canceller,
    std::unique_ptr<const ScopedSolveInterrupterCallback> user_canceller_cb,
    const ModelStorage* const expected_storage,
    std::unique_ptr<UpdateTracker> update_tracker,
    std::unique_ptr<BaseSolver> solver)
    : solver_factory_(std::move(solver_factory)),
      solver_type_(solver_type),
      remove_names_(remove_names),
      local_canceller_(std::move(local_canceller)),
      user_canceller_cb_(std::move(user_canceller_cb)),
      expected_storage_(expected_storage),
      update_tracker_(std::move(update_tracker)),
      solver_(std::move(solver)) {}

absl::StatusOr<SolveResult> IncrementalSolverImpl::Solve(
    const SolveArguments& arguments) {
  // TODO: b/260337466 - Add permanent errors and concurrency protection.
  RETURN_IF_ERROR(Update().status());
  return SolveWithoutUpdate(arguments);
}

absl::StatusOr<ComputeInfeasibleSubsystemResult>
IncrementalSolverImpl::ComputeInfeasibleSubsystem(
    const ComputeInfeasibleSubsystemArguments& arguments) {
  // TODO: b/260337466 - Add permanent errors and concurrency protection.
  RETURN_IF_ERROR(Update().status());
  return ComputeInfeasibleSubsystemWithoutUpdate(arguments);
}

absl::StatusOr<UpdateResult> IncrementalSolverImpl::Update() {
  // TODO: b/260337466 - Add permanent errors and concurrency protection.
  ASSIGN_OR_RETURN(std::optional<ModelUpdateProto> model_update,
                   update_tracker_->ExportModelUpdate(remove_names_));
  if (!model_update.has_value()) {
    return UpdateResult(true);
  }

  OR_ASSIGN_OR_RETURN3(const bool did_update,
                       solver_->Update(*std::move(model_update)),
                       _ << "update failed");
  RETURN_IF_ERROR(update_tracker_->AdvanceCheckpoint());

  if (did_update) {
    return UpdateResult(true);
  }

  ASSIGN_OR_RETURN(ModelProto model_proto,
                   update_tracker_->ExportModel(remove_names_));
  OR_ASSIGN_OR_RETURN3(
      solver_,
      solver_factory_(EnumToProto(solver_type_), std::move(model_proto),
                      local_canceller_.get()),
      _ << "solver re-creation failed");

  return UpdateResult(false);
}

absl::StatusOr<SolveResult> IncrementalSolverImpl::SolveWithoutUpdate(
    const SolveArguments& arguments) const {
  // TODO: b/260337466 - Add permanent errors and concurrency protection.
  return CallSolve(*solver_, expected_storage_, arguments, *local_canceller_);
}

absl::StatusOr<ComputeInfeasibleSubsystemResult>
IncrementalSolverImpl::ComputeInfeasibleSubsystemWithoutUpdate(
    const ComputeInfeasibleSubsystemArguments& arguments) const {
  // TODO: b/260337466 - Add permanent errors and concurrency protection.
  return CallComputeInfeasibleSubsystem(*solver_, expected_storage_, arguments,
                                        *local_canceller_);
}

}  // namespace operations_research::math_opt::internal
