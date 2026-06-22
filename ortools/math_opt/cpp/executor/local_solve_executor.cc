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

#include "ortools/math_opt/cpp/executor/local_solve_executor.h"

#include <memory>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/status/statusor.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/cpp/executor/executor_init_args.h"
#include "ortools/math_opt/cpp/executor/solve_executor.h"
#include "ortools/math_opt/cpp/executor/time_limit_util.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/util/solve_interrupter.h"

namespace operations_research::math_opt {
namespace {

// Creates a new interrupter that is interrupted if either of the input
// interrupters is interrupted.
//
// Either input interrupter can be null. When both inputs are null,
// local_interrupter_if_nonempty() will return null instead of an interrupter.
class ChainedInterrupter {
 public:
  ChainedInterrupter(const SolveInterrupter* absl_nullable i1,
                     const SolveInterrupter* absl_nullable i2)
      : from_i1_(i1, [this]() { local_interrupter_.Interrupt(); }),
        from_i2_(i2, [this]() { local_interrupter_.Interrupt(); }),
        empty_(i1 == nullptr && i2 == nullptr) {}

  bool empty() const { return empty_; }
  const SolveInterrupter* absl_nonnull local_interrupter() const {
    return &local_interrupter_;
  }
  const SolveInterrupter* absl_nullable local_interrupter_if_nonempty() const {
    return empty_ ? nullptr : &local_interrupter_;
  }

 private:
  SolveInterrupter local_interrupter_;
  ScopedSolveInterrupterCallback from_i1_;
  ScopedSolveInterrupterCallback from_i2_;
  const bool empty_;
};

}  // namespace

absl::StatusOr<SolveResult> LocalIncrementalSolver::Solve(SolveArguments args) {
  UpdateTimeLimit(absolute_deadline_, args.parameters);
  const ChainedInterrupter chained_interrupter(canceller_, args.interrupter);
  args.interrupter = chained_interrupter.local_interrupter_if_nonempty();
  return solver_->Solve(args);
}

absl::StatusOr<SolveResult> LocalSolveExecutor::Solve(
    const Model& model, SolverType solver_type, SolveArguments args,
    ExecutorSolverInitArguments init_args) {
  UpdateTimeLimit(
      init_args.explicit_deadline.value_or(absl::InfiniteDuration()),
      args.parameters);
  const ChainedInterrupter chained_interrupter(init_args.canceller,
                                               args.interrupter);
  args.interrupter = chained_interrupter.local_interrupter_if_nonempty();
  return ::operations_research::math_opt::Solve(
      model, solver_type, args,
      SolverInitArguments{.streamable = std::move(init_args.streamable),
                          .remove_names = init_args.remove_names});
}

absl::StatusOr<ComputeInfeasibleSubsystemResult>
LocalSolveExecutor::ComputeInfeasibleSubsystem(
    const Model& model, SolverType solver_type,
    ComputeInfeasibleSubsystemArguments args,
    ExecutorSolverInitArguments init_args) {
  UpdateTimeLimit(
      init_args.explicit_deadline.value_or(absl::InfiniteDuration()),
      args.parameters);
  const ChainedInterrupter chained_interrupter(init_args.canceller,
                                               args.interrupter);
  args.interrupter = chained_interrupter.local_interrupter_if_nonempty();
  return ::operations_research::math_opt::ComputeInfeasibleSubsystem(
      model, solver_type, args);
}

absl::StatusOr<std::unique_ptr<ExecutorIncrementalSolver>>
LocalSolveExecutor::New(Model* model, SolverType solver_type,
                        ExecutorSolverInitArguments init_args) {
  absl::Time absolute_deadline =
      absl::Now() +
      init_args.explicit_deadline.value_or(absl::InfiniteDuration());
  OR_ASSIGN_OR_RETURN(
      std::unique_ptr<IncrementalSolver> solver,
      NewIncrementalSolver(model, solver_type,
                           {.streamable = std::move(init_args.streamable),
                            .remove_names = init_args.remove_names}));
  return std::make_unique<LocalIncrementalSolver>(
      std::move(solver), absolute_deadline, init_args.canceller);
}

}  // namespace operations_research::math_opt
