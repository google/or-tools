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

#include "ortools/math_opt/cpp/solve.h"

#include <functional>
#include <memory>
#include <optional>
#include <utility>

#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/core/solver.h"
#include "ortools/math_opt/cpp/callback.h"
#include "ortools/math_opt/cpp/enums.h"
#include "ortools/math_opt/cpp/infeasible_subsystem_arguments.h"
#include "ortools/math_opt/cpp/infeasible_subsystem_result.h"
#include "ortools/math_opt/cpp/model.h"
#include "ortools/math_opt/cpp/model_solve_parameters.h"
#include "ortools/math_opt/cpp/parameters.h"
#include "ortools/math_opt/cpp/solve_arguments.h"
#include "ortools/math_opt/cpp/solve_result.h"
#include "ortools/math_opt/cpp/solver_init_arguments.h"
#include "ortools/math_opt/cpp/streamable_solver_init_arguments.h"
#include "ortools/math_opt/cpp/update_tracker.h"
#include "ortools/math_opt/infeasible_subsystem.pb.h"
#include "ortools/math_opt/storage/model_storage.h"
#include "ortools/util/status_macros.h"

namespace operations_research {
namespace math_opt {

namespace {

Solver::InitArgs ToSolverInitArgs(const SolverInitArguments& arguments) {
  Solver::InitArgs solver_init_args;
  solver_init_args.streamable = arguments.streamable.Proto();
  if (arguments.non_streamable != nullptr) {
    solver_init_args.non_streamable = arguments.non_streamable.get();
  }

  return solver_init_args;
}

absl::StatusOr<SolveResult> CallSolve(
    Solver& solver, const ModelStorage* const expected_storage,
    const SolveArguments& arguments) {
  RETURN_IF_ERROR(arguments.CheckModelStorageAndCallback(expected_storage));

  Solver::Callback cb = nullptr;
  absl::Mutex mutex;
  absl::Status cb_status;  // Guarded by `mutex`.
  if (arguments.callback != nullptr) {
    cb = [&](const CallbackDataProto& callback_data_proto) {
      const CallbackData data(expected_storage, callback_data_proto);
      // TODO(b/249995436): offer a way for user-callback to return a Status as
      // well and somehow label it as "user error" (annotation + payload?).
      const CallbackResult result = arguments.callback(data);

      if (const absl::Status status =
              result.CheckModelStorage(expected_storage);
          !status.ok()) {
        // Note that we use util::StatusBuilder() here as util::Annotate() is
        // not available in open-source code.
        util::StatusBuilder builder(status);
        builder << "invalid CallbackResult returned by user callback";

        const absl::MutexLock lock(&mutex);
        cb_status.Update(builder);

        // Trigger early termination of the solve.
        CallbackResultProto result_proto;
        result_proto.set_terminate(true);
        return result_proto;
      }

      return result.Proto();
    };
  }
  ASSIGN_OR_RETURN(
      const SolveResultProto solve_result,
      solver.Solve(
          {.parameters = arguments.parameters.Proto(),
           .model_parameters = arguments.model_parameters.Proto(),
           .message_callback = arguments.message_callback,
           .callback_registration = arguments.callback_registration.Proto(),
           .user_cb = std::move(cb),
           .interrupter = arguments.interrupter}));

  const absl::MutexLock lock(&mutex);
  RETURN_IF_ERROR(cb_status);

  return SolveResult::FromProto(expected_storage, solve_result);
}

}  // namespace

absl::StatusOr<SolveResult> Solve(const Model& model,
                                  const SolverType solver_type,
                                  const SolveArguments& solve_args,
                                  const SolverInitArguments& init_args) {
  ASSIGN_OR_RETURN(const std::unique_ptr<Solver> solver,
                   Solver::New(EnumToProto(solver_type), model.ExportModel(),
                               ToSolverInitArgs(init_args)));
  return CallSolve(*solver, model.storage(), solve_args);
}

absl::StatusOr<InfeasibleSubsystemResult> InfeasibleSubsystem(
    const Model& model, const SolverType solver_type,
    const InfeasibleSubsystemArguments& infeasible_subsystem_args,
    const SolverInitArguments& init_args) {
  ASSIGN_OR_RETURN(
      const InfeasibleSubsystemResultProto result_proto,
      Solver::NonIncrementalInfeasibleSubsystem(
          model.ExportModel(), EnumToProto(solver_type),
          ToSolverInitArgs(init_args),
          {.parameters = infeasible_subsystem_args.parameters.Proto(),
           .message_callback = infeasible_subsystem_args.message_callback,
           .interrupter = infeasible_subsystem_args.interrupter}));
  return InfeasibleSubsystemResult::FromProto(model.storage(), result_proto);
}

absl::StatusOr<std::unique_ptr<IncrementalSolver>> IncrementalSolver::New(
    Model* const model, const SolverType solver_type,
    SolverInitArguments arguments) {
  if (model == nullptr) {
    return absl::InvalidArgumentError("input model can't be null");
  }
  std::unique_ptr<UpdateTracker> update_tracker = model->NewUpdateTracker();
  ASSIGN_OR_RETURN(const ModelProto model_proto, update_tracker->ExportModel());
  ASSIGN_OR_RETURN(std::unique_ptr<Solver> solver,
                   Solver::New(EnumToProto(solver_type), model_proto,
                               ToSolverInitArgs(arguments)));
  return absl::WrapUnique<IncrementalSolver>(
      new IncrementalSolver(solver_type, std::move(arguments), model->storage(),
                            std::move(update_tracker), std::move(solver)));
}

IncrementalSolver::IncrementalSolver(
    SolverType solver_type, SolverInitArguments init_args,
    const ModelStorage* const expected_storage,
    std::unique_ptr<UpdateTracker> update_tracker,
    std::unique_ptr<Solver> solver)
    : solver_type_(solver_type),
      init_args_(std::move(init_args)),
      expected_storage_(expected_storage),
      update_tracker_(std::move(update_tracker)),
      solver_(std::move(solver)) {}

absl::StatusOr<SolveResult> IncrementalSolver::Solve(
    const SolveArguments& arguments) {
  RETURN_IF_ERROR(Update().status());
  return SolveWithoutUpdate(arguments);
}

absl::StatusOr<UpdateResult> IncrementalSolver::Update() {
  ASSIGN_OR_RETURN(std::optional<ModelUpdateProto> model_update,
                   update_tracker_->ExportModelUpdate());
  if (!model_update) {
    return UpdateResult(true, std::move(model_update));
  }

  OR_ASSIGN_OR_RETURN3(const bool did_update, solver_->Update(*model_update),
                       _ << "update failed");
  RETURN_IF_ERROR(update_tracker_->AdvanceCheckpoint());

  if (did_update) {
    return UpdateResult(true, std::move(model_update));
  }

  ASSIGN_OR_RETURN(const ModelProto model_proto,
                   update_tracker_->ExportModel());
  OR_ASSIGN_OR_RETURN3(solver_,
                       Solver::New(EnumToProto(solver_type_), model_proto,
                                   ToSolverInitArgs(init_args_)),
                       _ << "solver re-creation failed");

  return UpdateResult(false, std::move(model_update));
}

absl::StatusOr<SolveResult> IncrementalSolver::SolveWithoutUpdate(
    const SolveArguments& arguments) const {
  return CallSolve(*solver_, expected_storage_, arguments);
}

}  // namespace math_opt
}  // namespace operations_research
