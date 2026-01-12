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

#ifndef ORTOOLS_MATH_OPT_CPP_SOLVE_IMPL_H_
#define ORTOOLS_MATH_OPT_CPP_SOLVE_IMPL_H_

#include <memory>

#include "absl/base/nullability.h"
#include "absl/functional/any_invocable.h"
#include "absl/status/statusor.h"
#include "ortools/math_opt/core/base_solver.h"
#include "ortools/math_opt/cpp/compute_infeasible_subsystem_arguments.h"
#include "ortools/math_opt/cpp/compute_infeasible_subsystem_result.h"
#include "ortools/math_opt/cpp/incremental_solver.h"
#include "ortools/math_opt/cpp/model.h"
#include "ortools/math_opt/cpp/parameters.h"
#include "ortools/math_opt/cpp/solve_arguments.h"
#include "ortools/math_opt/cpp/solve_result.h"
#include "ortools/math_opt/cpp/update_result.h"
#include "ortools/math_opt/cpp/update_tracker.h"
#include "ortools/math_opt/storage/model_storage.h"
#include "ortools/util/solve_interrupter.h"

namespace operations_research::math_opt::internal {

// A factory of solver.
//
// The local_canceller is a local interrupter that exists in the scope of
// SolveImpl(), ComputeInfeasibleSubsystemImpl() or IncrementalSolverImpl. It is
// triggered:
// * either when the user_canceller is triggered
// * or when the BaseSolver::Callback returns an invalid CallbackResultProto; in
//   that case a new CallbackResultProto with its `terminate` set to true is
//   also returned instead.
//
// Solvers that don't support cancellation (i.e. in-process solving) should
// ignore the local_canceller: this use case won't have a user_canceller and the
// CallbackResultProto.terminate will terminate the solve as soon as possible if
// the CallbackResultProto is invalid.
using BaseSolverFactory =
    absl::AnyInvocable<absl::StatusOr<std::unique_ptr<BaseSolver>>(
        SolverTypeProto solver_type, ModelProto model,
        SolveInterrupter* local_canceller) const>;

// Solves the input model.
//
// The `user_canceller` parameter is optional.
absl::StatusOr<SolveResult> SolveImpl(
    BaseSolverFactory solver_factory, const Model& model,
    SolverType solver_type, const SolveArguments& solve_args,
    const SolveInterrupter* absl_nullable user_canceller, bool remove_names);

// ComputeInfeasibleSubsystems the input model in a subprocess.
//
// The `user_canceller` parameter is optional.
absl::StatusOr<ComputeInfeasibleSubsystemResult> ComputeInfeasibleSubsystemImpl(
    BaseSolverFactory solver_factory, const Model& model,
    SolverType solver_type,
    const ComputeInfeasibleSubsystemArguments& compute_args,
    const SolveInterrupter* absl_nullable user_canceller, bool remove_names);

// Incremental solve of a model.
class IncrementalSolverImpl : public IncrementalSolver {
 public:
  // Creates a new incremental solve.
  //
  // The `user_canceller` parameter is optional.
  static absl::StatusOr<std::unique_ptr<IncrementalSolverImpl>> New(
      BaseSolverFactory solver_factory, Model* model, SolverType solver_type,
      const SolveInterrupter* absl_nullable user_canceller, bool remove_names);

  absl::StatusOr<UpdateResult> Update() override;

  absl::StatusOr<SolveResult> SolveWithoutUpdate(
      const SolveArguments& arguments) const override;

  absl::StatusOr<ComputeInfeasibleSubsystemResult>
  ComputeInfeasibleSubsystemWithoutUpdate(
      const ComputeInfeasibleSubsystemArguments& arguments) const override;

  SolverType solver_type() const override { return solver_type_; }

 private:
  IncrementalSolverImpl(
      BaseSolverFactory solver_factory, SolverType solver_type,
      bool remove_names, std::shared_ptr<SolveInterrupter> local_canceller,
      std::unique_ptr<const ScopedSolveInterrupterCallback> user_canceller_cb,
      ModelStorageCPtr expected_storage,
      std::unique_ptr<UpdateTracker> update_tracker,
      std::unique_ptr<BaseSolver> solver);

  const BaseSolverFactory solver_factory_;
  const SolverType solver_type_;
  const bool remove_names_;
  // Here we use a shared_ptr so that we don't have to make sure that
  // user_canceller_cb_, which points to local_canceller_ via a lambda-capture,
  // can be destroyed after local_canceller_ without risk.
  std::shared_ptr<SolveInterrupter> local_canceller_;
  std::unique_ptr<const ScopedSolveInterrupterCallback> user_canceller_cb_;
  const ModelStorageCPtr expected_storage_;
  const std::unique_ptr<UpdateTracker> update_tracker_;
  std::unique_ptr<BaseSolver> solver_;
};

}  // namespace operations_research::math_opt::internal

#endif  // ORTOOLS_MATH_OPT_CPP_SOLVE_IMPL_H_
