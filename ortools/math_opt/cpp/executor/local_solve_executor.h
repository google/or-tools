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

#ifndef ORTOOLS_MATH_OPT_CPP_EXECUTOR_LOCAL_SOLVE_EXECUTOR_H_
#define ORTOOLS_MATH_OPT_CPP_EXECUTOR_LOCAL_SOLVE_EXECUTOR_H_

#include <memory>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "ortools/math_opt/cpp/executor/executor_init_args.h"
#include "ortools/math_opt/cpp/executor/solve_executor.h"
#include "ortools/math_opt/cpp/math_opt.h"

namespace operations_research::math_opt {

// An ExecutorIncrementalSolver that runs in process on the same thread.
//
// Create with LocalSolveExecutor::New().
//
// This class is a thin wrapper around IncrementalSolver, with the same
// modifications on deadline and cancellation as LocalSolveExecutor.
class LocalIncrementalSolver : public ExecutorIncrementalSolver {
 public:
  using ExecutorIncrementalSolver::Solve;
  absl::StatusOr<SolveResult> Solve(SolveArguments args) override;

  // Access via LocalSolveExecutor::New().
  explicit LocalIncrementalSolver(
      std::unique_ptr<IncrementalSolver> solver,
      const absl::Time absolute_deadline,
      const SolveInterrupter* absl_nullable const canceller)
      : solver_(std::move(solver)),
        absolute_deadline_(absolute_deadline),
        canceller_(canceller) {}

  SolverType solver_type() const override { return solver_->solver_type(); }

 private:
  std::unique_ptr<IncrementalSolver> solver_;
  absl::Time absolute_deadline_;
  const SolveInterrupter* absl_nullable canceller_;
};

// A `SolveExecutor` where all solves run in process and are started from the
// same thread (some solvers create more threads).
//
// Features not found on ::operations_research::math_opt::Solve():
//  * If an explicit deadline is provided, the time limit is set to the minimum
//    of the existing time limit and the deadline.
//  * If a canceller is provided, we interrupt if the canceller is triggered.
//
// This class is stateless, copyable, and movable. It is threadsafe to call the
// functions on this class concurrently. This class is not aware of fiber
// cancellations or base context deadlines.
class LocalSolveExecutor : public SolveExecutor {
 public:
  LocalSolveExecutor() = default;

  using SolveExecutor::Solve;
  absl::StatusOr<SolveResult> Solve(
      const Model& model, SolverType solver_type, SolveArguments args,
      ExecutorSolverInitArguments init_args) override;

  using SolveExecutor::ComputeInfeasibleSubsystem;
  absl::StatusOr<ComputeInfeasibleSubsystemResult> ComputeInfeasibleSubsystem(
      const Model& model, SolverType solver_type,
      ComputeInfeasibleSubsystemArguments args,
      ExecutorSolverInitArguments init_args) override;

  using SolveExecutor::New;
  absl::StatusOr<std::unique_ptr<ExecutorIncrementalSolver>> New(
      Model* model, SolverType solver_type,
      ExecutorSolverInitArguments init_args) override;
};

}  // namespace operations_research::math_opt

#endif  // ORTOOLS_MATH_OPT_CPP_EXECUTOR_LOCAL_SOLVE_EXECUTOR_H_
