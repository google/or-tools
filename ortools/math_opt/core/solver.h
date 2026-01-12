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

#ifndef ORTOOLS_MATH_OPT_CORE_SOLVER_H_
#define ORTOOLS_MATH_OPT_CORE_SOLVER_H_

#include <memory>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/core/base_solver.h"
#include "ortools/math_opt/core/concurrent_calls_guard.h"
#include "ortools/math_opt/core/model_summary.h"
#include "ortools/math_opt/core/solver_interface.h"
#include "ortools/math_opt/infeasible_subsystem.pb.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_parameters.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/parameters.pb.h"
#include "ortools/math_opt/result.pb.h"

namespace operations_research::math_opt {

// A solver for a given model and solver implementation.
//
// Use the New() function to build a new solver instance; then call Solve() to
// solve the model. You can then update the model using Update() and resolve.
//
// Thread-safety: methods Solve() and Update() must not be called concurrently;
// they will immediately return with an error status if this happens. Some
// solvers may add more restriction regarding threading. Please see
// SOLVER_TYPE_XXX documentation for details.
//
// Usage:
//   const ModelProto model = ...;
//   const auto solver = Solver::New(SOLVER_TYPE_GSCIP,
//                                   model,
//                                   /*arguments=*/{});
//   CHECK_OK(solver.status());
//   Solver::SolveArgs solve_arguments;
//   ...
//
//   // First solve of the initial Model.
//   const auto first_solution = (*solver)->Solve(solve_arguments);
//   CHECK_OK(first_solution.status());
//   // Use the first_solution here.
//
//   // Update the Model with a ModelUpdate.
//   const ModelUpdate update = ...;
//   CHECK_OK((*solver)->Update(update));
//   const auto second_solution = (*solver)->Solve(solve_arguments);
//   CHECK_OK(second_solution.status());
//   // Use the second_solution of the updated problem here.
//
class Solver : public BaseSolver {
 public:
  using InitArgs = SolverInterface::InitArgs;

  // A shortcut for calling Solver::New() and then Solver::Solve().
  static absl::StatusOr<SolveResultProto> NonIncrementalSolve(
      const ModelProto& model, SolverTypeProto solver_type,
      const InitArgs& init_args, const SolveArgs& solve_args);

  // A shortcut for calling Solver::New() and then
  // Solver()::ComputeInfeasibleSubsystem()
  static absl::StatusOr<ComputeInfeasibleSubsystemResultProto>
  NonIncrementalComputeInfeasibleSubsystem(
      const ModelProto& model, SolverTypeProto solver_type,
      const InitArgs& init_args,
      const ComputeInfeasibleSubsystemArgs& compute_infeasible_subsystem_args);

  // Builds a solver of the given type with the provided model and
  // initialization parameters.
  static absl::StatusOr<std::unique_ptr<Solver>> New(
      SolverTypeProto solver_type, const ModelProto& model,
      const InitArgs& arguments);

  ~Solver() override;

  absl::StatusOr<SolveResultProto> Solve(const SolveArgs& arguments) override;

  // See BaseSolver::Update.
  //
  // When this function returns false, the Solver object is in a failed
  // state. In that case the underlying SolverInterface implementation has been
  // destroyed (this enables the caller to instantiate a new Solver without
  // destroying the previous one first even if they use Gurobi with a single-use
  // license).
  absl::StatusOr<bool> Update(ModelUpdateProto model_update) override;

  absl::StatusOr<ComputeInfeasibleSubsystemResultProto>
  ComputeInfeasibleSubsystem(
      const ComputeInfeasibleSubsystemArgs& arguments) override;

 private:
  Solver(std::unique_ptr<SolverInterface> underlying_solver,
         ModelSummary model_summary);

  // Tracker used to ensure that Solve() and Update() are not called
  // concurrently.
  ConcurrentCallsGuard::Tracker concurrent_calls_tracker_;

  // Can be nullptr only if fatal_failure_occurred_ is true (but the contrary is
  // not true). This happens when Update() returns false.
  std::unique_ptr<SolverInterface> underlying_solver_;

  ModelSummary model_summary_;

  // Set to true if a previous call to Solve() or Update() returned a failing
  // status (or if Update() returned false).
  //
  // This is guarded by concurrent_calls_tracker_.
  bool fatal_failure_occurred_ = false;
};

namespace internal {

// Validates that the input streamable and non_streamable init arguments are
// either not set or are the one of solver_type.
absl::Status ValidateInitArgs(const Solver::InitArgs& init_args,
                              SolverTypeProto solver_type);

}  // namespace internal
}  // namespace operations_research::math_opt

#endif  // ORTOOLS_MATH_OPT_CORE_SOLVER_H_
