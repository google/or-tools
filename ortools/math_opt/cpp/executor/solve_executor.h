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

#ifndef ORTOOLS_MATH_OPT_CPP_EXECUTOR_SOLVE_EXECUTOR_H_
#define ORTOOLS_MATH_OPT_CPP_EXECUTOR_SOLVE_EXECUTOR_H_

#include <memory>
#include <utility>

#include "absl/status/statusor.h"
#include "ortools/math_opt/cpp/executor/executor_init_args.h"
#include "ortools/math_opt/cpp/math_opt.h"

namespace operations_research::math_opt {

// An optimization solver that can solve an input `Model` and repeatedly resolve
// the model after modifications.
//
// Create with `SolveExecutor::New()`.
//
// Similar to `IncrementalSolver`, but the underlying solver may run in a
// different process or even a different machine. See docs on IncrementalSolver
// (../solve.h) for more details.
//
// Implementations of this class are generally not threadsafe (do not run
// Solve() concurrently with another Solve() or with any modifications to the
// underlying model). However, the destructor and modifications to the
// underlying Model can run concurrently.
class ExecutorIncrementalSolver {
 public:
  virtual ~ExecutorIncrementalSolver() = default;

  // Calls Solve(SolveArguments{}), see that function for details.
  absl::StatusOr<SolveResult> Solve() { return Solve({}); }

  // Solves the Model and returns the result.
  //
  // Ensure that the underlying model outlives the returned SolveResult.
  // Modification of the Model (in particular, deleting variables/constraints)
  // can also invalidate the SolveResult.
  //
  // Generally, any Status error returned indicates that object is in a bad
  // state and that subsequent calls to Solve() will also fail. For some
  // subclasses solving by RPC, there may be an exception for retrying on
  // network errors, the exceptions are documented in each implementation.
  virtual absl::StatusOr<SolveResult> Solve(SolveArguments args) = 0;

  virtual SolverType solver_type() const = 0;
};

// An interface for solving optimization models and creating incremental
// solvers for a model.
//
// Implementations differ on thread-safety, supporting move/copy, being fiber
// aware, and respecting base context deadlines.
class SolveExecutor {
 public:
  virtual ~SolveExecutor() = default;

  // Solves the input model, similar to the function
  // ::operations_research::math_opt::Solve().
  //
  // Ensure `model` outlives the returned SolveResult. Deleting variables or
  // constraints from `model` may invalidate the returned SolveResult.
  virtual absl::StatusOr<SolveResult> Solve(
      const Model& model, SolverType solver_type, SolveArguments args,
      ExecutorSolverInitArguments init_args) = 0;

  // Calls Solve(model, solver_type, args, ExecutorSolverInitArguments{}), see
  // overload for details.
  absl::StatusOr<SolveResult> Solve(const Model& model,
                                    const SolverType solver_type,
                                    SolveArguments args) {
    return Solve(model, solver_type, std::move(args), {});
  }

  // Calls Solve(model, solver_type, SolveArguments{}), see overload for
  // details.
  absl::StatusOr<SolveResult> Solve(const Model& model,
                                    const SolverType solver_type) {
    return Solve(model, solver_type, {});
  }

  // Checks if a model is infeasible, and computes an infeasible subsystem
  // (a subset of the variables/constraints that causes infeasibility) if one
  // exists (generally trying to find a small explanation). Similar to
  // ::operations_research::math_opt::ComputeInfeasibleSubsystem().
  //
  // Ensure `model` outlives the returned ComputeInfeasibleSubsystemResult.
  // Deleting variables or constraints from `model` may invalidate the returned
  // ComputeInfeasibleSubsystemResult.
  virtual absl::StatusOr<ComputeInfeasibleSubsystemResult>
  ComputeInfeasibleSubsystem(const Model& model, SolverType solver_type,
                             ComputeInfeasibleSubsystemArguments args,
                             ExecutorSolverInitArguments init_args) = 0;

  // Calls ComputeInfeasibleSubsystem(model, solver_type, args,
  // ExecutorSolverInitArguments{}), see overload for details.
  absl::StatusOr<ComputeInfeasibleSubsystemResult> ComputeInfeasibleSubsystem(
      const Model& model, const SolverType solver_type,
      ComputeInfeasibleSubsystemArguments args) {
    return ComputeInfeasibleSubsystem(model, solver_type, std::move(args), {});
  }

  // Calls ComputeInfeasibleSubsystem(model, solver_type,
  // ComputeInfeasibleSubsystemArguments{}), see overload for details.
  absl::StatusOr<ComputeInfeasibleSubsystemResult> ComputeInfeasibleSubsystem(
      const Model& model, const SolverType solver_type) {
    return ComputeInfeasibleSubsystem(model, solver_type, {});
  }

  // Returns a solver that solves `model`, updating for any modifications to
  // model between calls to Solve().
  //
  // Requires that `model` outlive the returned ExecutorIncrementalSolver.
  //
  // The underlying solver and SolverExecutor generally try to make sequential
  // calls to solve more efficient than solving from scratch. The details are
  // different for each implementation and in some cases there is no gain.
  virtual absl::StatusOr<std::unique_ptr<ExecutorIncrementalSolver>> New(
      Model* model, SolverType solver_type,
      ExecutorSolverInitArguments init_args) = 0;

  // Calls New(model, solver_type, ExecutorSolverInitArguments{}), see overload
  // for details.
  absl::StatusOr<std::unique_ptr<ExecutorIncrementalSolver>> New(
      Model* model, const SolverType solver_type) {
    return New(model, solver_type, {});
  }
};

}  // namespace operations_research::math_opt

#endif  // ORTOOLS_MATH_OPT_CPP_EXECUTOR_SOLVE_EXECUTOR_H_
