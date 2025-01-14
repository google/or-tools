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

#include "ortools/math_opt/cpp/solve.h"

#include <memory>
#include <utility>

#include "absl/status/statusor.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/core/base_solver.h"
#include "ortools/math_opt/core/solver.h"
#include "ortools/math_opt/cpp/compute_infeasible_subsystem_arguments.h"
#include "ortools/math_opt/cpp/compute_infeasible_subsystem_result.h"
#include "ortools/math_opt/cpp/model.h"
#include "ortools/math_opt/cpp/parameters.h"
#include "ortools/math_opt/cpp/solve_arguments.h"
#include "ortools/math_opt/cpp/solve_impl.h"
#include "ortools/math_opt/cpp/solve_result.h"
#include "ortools/math_opt/cpp/solver_init_arguments.h"
#include "ortools/math_opt/cpp/streamable_solver_init_arguments.h"
#include "ortools/math_opt/infeasible_subsystem.pb.h"

namespace operations_research {
namespace math_opt {

namespace {

Solver::InitArgs ToSolverInitArgs(const SolverInitArguments& arguments) {
  return {
      .streamable = arguments.streamable.Proto(),
      .non_streamable = arguments.non_streamable.get(),
  };
}

internal::BaseSolverFactory FactoryFromInitArguments(
    const SolverInitArguments& arguments) {
  return [arguments](const SolverTypeProto solver_type, ModelProto model,
                     SolveInterrupter* const local_canceller)
             -> absl::StatusOr<std::unique_ptr<BaseSolver>> {
    // We don't use the local_canceller as in-process solve can't be
    // cancelled. If an error happens in the callback, the solve_impl code will
    // use CallbackResultProto::set_terminate() to trigger a cooperative
    // interruption.
    return Solver::New(solver_type, std::move(model),
                       ToSolverInitArgs(arguments));
  };
}

}  // namespace

absl::StatusOr<SolveResult> Solve(const Model& model,
                                  const SolverType solver_type,
                                  const SolveArguments& solve_args,
                                  const SolverInitArguments& init_args) {
  return internal::SolveImpl(FactoryFromInitArguments(init_args), model,
                             solver_type, solve_args,
                             /*user_canceller=*/nullptr,
                             /*remove_names=*/init_args.remove_names);
}

absl::StatusOr<ComputeInfeasibleSubsystemResult> ComputeInfeasibleSubsystem(
    const Model& model, const SolverType solver_type,
    const ComputeInfeasibleSubsystemArguments& compute_args,
    const SolverInitArguments& init_args) {
  return internal::ComputeInfeasibleSubsystemImpl(
      FactoryFromInitArguments(init_args), model, solver_type, compute_args,
      /*user_canceller=*/nullptr,
      /*remove_names=*/init_args.remove_names);
}

absl::StatusOr<std::unique_ptr<IncrementalSolver>> NewIncrementalSolver(
    Model* model, SolverType solver_type, SolverInitArguments arguments) {
  return internal::IncrementalSolverImpl::New(
      FactoryFromInitArguments(arguments), model, solver_type,
      /*user_canceller=*/nullptr,
      /*remove_names=*/arguments.remove_names);
}

}  // namespace math_opt
}  // namespace operations_research
