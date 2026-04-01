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

// Tests types for library based on solve_impl.h.
#ifndef ORTOOLS_MATH_OPT_CPP_SOLVE_IMPL_TESTING_H_
#define ORTOOLS_MATH_OPT_CPP_SOLVE_IMPL_TESTING_H_

#include <memory>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/functional/function_ref.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/cpp/compute_infeasible_subsystem_arguments.h"
#include "ortools/math_opt/cpp/compute_infeasible_subsystem_result.h"
#include "ortools/math_opt/cpp/incremental_solver.h"
#include "ortools/math_opt/cpp/model.h"
#include "ortools/math_opt/cpp/solve_arguments.h"
#include "ortools/math_opt/cpp/solve_result.h"

namespace operations_research::math_opt {

// Control if parametric tests should use incremental solve or not.
enum class Incremental {
  // Use XxxSolve() or XxxComputeInfeasibleSubsystem().
  kNo,
  // Use NewXxxIncrementalSolver()->Solve() or
  // NewXxxIncrementalSolver()->ComputeInfeasibleSubsystem().
  kYes,
};

template <typename Sink>
void AbslStringify(Sink& sink, const Incremental incremental) {
  switch (incremental) {
    case Incremental::kYes:
      sink.Append("yes");
      break;
    case Incremental::kNo:
      sink.Append("no");
      break;
  }
}

// Which "solve" function to call in the test.
enum class TestedSolveFunction { kSolve, kComputeInfeasibleSubsystem };

template <typename Sink>
void AbslStringify(Sink& sink, const TestedSolveFunction function) {
  switch (function) {
    case TestedSolveFunction::kSolve:
      sink.Append("Solve");
      break;
    case TestedSolveFunction::kComputeInfeasibleSubsystem:
      sink.Append("ComputeInfeasibleSubsystem");
      break;
  }
}

// Common parameters between Solve() and ComputeInfeasibleSubsystem().
//
// This is used for common tests that apply to both functions.
struct TestedSolveFunctionArguments {
  SolveParameters parameters;
  MessageCallback message_callback = nullptr;
  const SolveInterrupter* absl_nullable interrupter = nullptr;

  // Returns either SolveArguments or ComputeInfeasibleSubsystemArguments, based
  // on Args.
  template <typename Args>
  Args ToArgs() const {
    return {
        .parameters = parameters,
        .message_callback = message_callback,
        .interrupter = interrupter,
    };
  }
};

// An object automatically calling Solve() or ComputeInfeasibleSubsystem(),
// either incrementally or not in parameterized tests.
//
// It is parameterized with fields:
// * .solve
// * .compute_infeasible_subsystem
// * .new_incremental_solver
//
// Then functions:
// * CallFunction()
// * CallSolve()
// * CallComputeInfeasibleSubsystem()
// can be used in parameterized tests.
template <typename InitArguments>
struct SolveImplementationCaller {
  // The non-incremental Solve() function.
  absl::FunctionRef<absl::StatusOr<SolveResult>(
      const Model& model, SolverType solver_type,
      const SolveArguments& solve_args, const InitArguments& init_args)>
      solve ABSL_REQUIRE_EXPLICIT_INIT;

  // The non-incremental ComputeInfeasibleSubsystem() function.
  absl::FunctionRef<absl::StatusOr<ComputeInfeasibleSubsystemResult>(
      const Model& model, SolverType solver_type,
      const ComputeInfeasibleSubsystemArguments& infeasible_subsystem_args,
      const InitArguments& init_args)>
      compute_infeasible_subsystem ABSL_REQUIRE_EXPLICIT_INIT;

  // The incremental solver factory.
  absl::FunctionRef<
      absl::StatusOr<absl_nonnull std::unique_ptr<IncrementalSolver>>(
          Model* absl_nonnull model, SolverType solver_type,
          InitArguments arguments)>
      new_incremental_solver ABSL_REQUIRE_EXPLICIT_INIT;

  // Either call CallSolve() or CallComputeInfeasibleSubsystem() based on
  // `function` parameter.
  absl::Status CallFunction(const TestedSolveFunction function,
                            const Incremental incremental, Model& model,
                            SolverType solver_type,
                            const TestedSolveFunctionArguments& solve_args = {},
                            const InitArguments& init_args = {}) const {
    switch (function) {
      case TestedSolveFunction::kSolve:
        return CallSolve(incremental, model, solver_type,
                         solve_args.ToArgs<SolveArguments>(), init_args)
            .status();
      case TestedSolveFunction::kComputeInfeasibleSubsystem:
        return CallComputeInfeasibleSubsystem(
                   incremental, model, solver_type,
                   solve_args.ToArgs<ComputeInfeasibleSubsystemArguments>(),
                   init_args)
            .status();
    }
  }

  // Either call XxxSolve() or NewXxxIncrementalSolver()->Solve(), based
  // on the `incremental` parameter.
  absl::StatusOr<SolveResult> CallSolve(
      const Incremental incremental, Model& model, SolverType solver_type,
      const SolveArguments& solve_args = {},
      const InitArguments& init_args = {}) const {
    switch (incremental) {
      case Incremental::kNo:
        return solve(model, solver_type, solve_args, init_args);
      case Incremental::kYes: {
        ASSIGN_OR_RETURN(
            const std::unique_ptr<IncrementalSolver> incremental_solver,
            new_incremental_solver(&model, solver_type, init_args),
            _ << "NewXxxIncrementalSolver() failed");
        return incremental_solver->Solve(solve_args);
      }
    }
  }

  // Either call XxxComputeInfeasibleSubsystem() or
  // NewXxxIncrementalSolver()->ComputeInfeasibleSubsystem(), based on
  // `incremental` parameter.
  absl::StatusOr<ComputeInfeasibleSubsystemResult>
  CallComputeInfeasibleSubsystem(
      const Incremental incremental, Model& model, SolverType solver_type,
      const ComputeInfeasibleSubsystemArguments& compute_args = {},
      const InitArguments& init_args = {}) const {
    switch (incremental) {
      case Incremental::kNo:
        return compute_infeasible_subsystem(model, solver_type, compute_args,
                                            init_args);
      case Incremental::kYes: {
        ASSIGN_OR_RETURN(
            const std::unique_ptr<IncrementalSolver> incremental_solver,
            new_incremental_solver(&model, solver_type, init_args),
            _ << "NewXxxIncrementalSolver() failed");
        return incremental_solver->ComputeInfeasibleSubsystem(compute_args);
      }
    }
  }
};

}  // namespace operations_research::math_opt

#endif  // ORTOOLS_MATH_OPT_CPP_SOLVE_IMPL_TESTING_H_
