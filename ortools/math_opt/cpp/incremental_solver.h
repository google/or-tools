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

// IWYU pragma: private, include "ortools/math_opt/cpp/math_opt.h"
// IWYU pragma: friend "ortools/math_opt/cpp/.*"

#ifndef ORTOOLS_MATH_OPT_CPP_INCREMENTAL_SOLVER_H_
#define ORTOOLS_MATH_OPT_CPP_INCREMENTAL_SOLVER_H_

#include "absl/status/statusor.h"
#include "ortools/math_opt/cpp/compute_infeasible_subsystem_arguments.h"  // IWYU pragma: export
#include "ortools/math_opt/cpp/compute_infeasible_subsystem_result.h"  // IWYU pragma: export
#include "ortools/math_opt/cpp/parameters.h"       // IWYU pragma: export
#include "ortools/math_opt/cpp/solve_arguments.h"  // IWYU pragma: export
#include "ortools/math_opt/cpp/solve_result.h"     // IWYU pragma: export
#include "ortools/math_opt/cpp/update_result.h"    // IWYU pragma: export

namespace operations_research::math_opt {

// Interface for incrementally solving of a model.
//
// This is a feature for advance users. Most users should only use the
// non-incremental Solve(), SubprocessSolve(),... functions.
//
// Here incremental means that the we try to reuse the existing underlying
// solver internals between each solve. There is no guarantee though that the
// solver supports all possible model changes. Hence there is not guarantee that
// performances will be improved when using this class; this is solver
// dependent. Typically LPs have more to gain from incremental solve than
// MIPs. In both cases, even if the solver supports the model changes,
// incremental solve may actually be slower.
//
// Implementation of this interface are returned by factories that can be found
// next to non-incremental solve functions. See NewIncrementalSolver() in
// solve.h and NewSubprocessIncrementalSolver() in subprocess_solve.h for
// example.
//
// Those factories function instantiates the solver, setup it from the current
// state of the Model and register on it to listen to changes. Calling Solve()
// will update the underlying solver with latest model changes and solve this
// model.
//
// Usage:
//   Model model = ...;
//   ASSIGN_OR_RETURN(
//     const std::unique_ptr<IncrementalSolver> incremental_solve,
//     NewIncrementalSolver(&model, SolverType::kXxx));
//
//   ASSIGN_OR_RETURN(const SolveResult result1, incremental_solve->Solve());
//
//   model.AddVariable(...);
//   ...
//
//   ASSIGN_OR_RETURN(const SolveResult result2, incremental_solve->Solve());
//
//   ...
//
// Lifecycle: The IncrementalSolver is only keeping a std::weak_ref on Model's
// internal data and thus it returns an error if Update() or Solve() are called
// after the Model has been destroyed. It is fine to destroy the
// IncrementalSolver after the associated Model though.
//
// Thread-safety: The New(), Solve() and Update() methods must not be called
// while modifying the Model() (adding variables...). The user is expected to
// use proper synchronization primitives to serialize changes to the model and
// the use of this object. Note though that it is safe to call methods from
// different IncrementalSolver instances on the same Model concurrently. Same
// for calling factories of IncrementalSolver. The destructor is thread-safe and
// can be called even during a modification of the Model.
//
// There is no problem calling SolveWithoutUpdate() concurrently on different
// instances of IncrementalSolver or while the model is being modified (unless
// of course the underlying solver itself is not thread-safe and can only be
// called from a single-thread).
//
// Note that Solve(), Update() and SolveWithoutUpdate() are not reentrant so
// they should not be called concurrently on the same instance of
// IncrementalSolver.
//
// Some solvers may add more restrictions regarding threading. Please see
// SolverType::kXxx documentation for details.
class IncrementalSolver {
 public:
  IncrementalSolver() = default;
  IncrementalSolver(const IncrementalSolver&) = delete;
  IncrementalSolver& operator=(const IncrementalSolver&) = delete;
  virtual ~IncrementalSolver() = default;

  // Updates the underlying solver with latest model changes and runs the solve.
  //
  // A Status error will be returned if inputs are invalid or there is an
  // unexpected failure in an underlying solver or for some internal math_opt
  // errors. Otherwise, check SolveResult::termination.reason to see if an
  // optimal solution was found.
  //
  // Memory model: the returned SolveResult owns its own memory (for solutions,
  // solve stats, etc.), EXPECT for a pointer back to the model. As a result:
  //  * Keep the model alive to access SolveResult,
  //  * Avoid unnecessarily copying SolveResult,
  //  * The result is generally accessible after mutating this, but some care
  //    is needed if variables or linear constraints are added or deleted.
  //
  // See callback.h for documentation on arguments.callback and
  // arguments.callback_registration.
  virtual absl::StatusOr<SolveResult> Solve(
      const SolveArguments& arguments) = 0;
  absl::StatusOr<SolveResult> Solve() { return Solve({}); }

  // Updates the underlying solver with latest model changes and runs the
  // computation.
  //
  // Same as Solve() but compute the infeasible subsystem.
  virtual absl::StatusOr<ComputeInfeasibleSubsystemResult>
  ComputeInfeasibleSubsystem(
      const ComputeInfeasibleSubsystemArguments& arguments) = 0;
  absl::StatusOr<ComputeInfeasibleSubsystemResult>
  ComputeInfeasibleSubsystem() {
    return ComputeInfeasibleSubsystem({});
  }

  // Updates the model to solve.
  //
  // This is an advanced API, most users should use Solve() above that does the
  // update and before calling the solver. Calling this function is only useful
  // for users that want to access to update data or users that need to use
  // SolveWithoutUpdate() (which should not be common).
  //
  // The returned value indicates if the update was possible or if the solver
  // had to be recreated from scratch (which may happen when the solver does not
  // support this specific update or any update at all). It also contains the
  // attempted update data.
  //
  // A status error will be returned if the underlying solver has an internal
  // error.
  virtual absl::StatusOr<UpdateResult> Update() = 0;

  // Same as Solve() but does not update the underlying solver with the latest
  // changes to the model.
  //
  // This is an advanced API, most users should use Solve().
  virtual absl::StatusOr<SolveResult> SolveWithoutUpdate(
      const SolveArguments& arguments) const = 0;
  absl::StatusOr<SolveResult> SolveWithoutUpdate() const {
    return SolveWithoutUpdate({});
  }

  // Same as ComputeInfeasibleSubsystem() but does not update the underlying
  // solver with the latest changes to the model.
  //
  // This is an advanced API, most users should use
  // ComputeInfeasibleSubsystem().
  virtual absl::StatusOr<ComputeInfeasibleSubsystemResult>
  ComputeInfeasibleSubsystemWithoutUpdate(
      const ComputeInfeasibleSubsystemArguments& arguments) const = 0;
  absl::StatusOr<ComputeInfeasibleSubsystemResult>
  ComputeInfeasibleSubsystemWithoutUpdate() const {
    return ComputeInfeasibleSubsystemWithoutUpdate({});
  }

  // Returns the underlying solver used.
  virtual SolverType solver_type() const = 0;
};

}  // namespace operations_research::math_opt

#endif  // ORTOOLS_MATH_OPT_CPP_INCREMENTAL_SOLVER_H_
