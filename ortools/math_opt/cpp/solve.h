// Copyright 2010-2021 Google LLC
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

// Functions and classes used to solve a Model.
//
// The main entry point is the Solve() function.
//
// For users that need incremental solving, there is the IncrementalSolver
// class.
#ifndef OR_TOOLS_MATH_OPT_CPP_SOLVE_H_
#define OR_TOOLS_MATH_OPT_CPP_SOLVE_H_

#include <memory>
#include <optional>
#include <utility>

#include "absl/status/statusor.h"
#include "ortools/math_opt/core/model_storage.h"
#include "ortools/math_opt/core/solver.h"
#include "ortools/math_opt/cpp/model.h"
#include "ortools/math_opt/cpp/parameters.h"             // IWYU pragma: export
#include "ortools/math_opt/cpp/solve_arguments.h"        // IWYU pragma: export
#include "ortools/math_opt/cpp/solve_result.h"           // IWYU pragma: export
#include "ortools/math_opt/cpp/solver_init_arguments.h"  // IWYU pragma: export
#include "ortools/math_opt/parameters.pb.h"              // IWYU pragma: export

namespace operations_research {
namespace math_opt {

// Solves the input model.
//
// A Status error will be returned if there is an unexpected failure in an
// underlying solver or for some internal math_opt errors. Otherwise, check
// SolveResult::termination.reason to see if an optimal solution was found.
//
// Memory model: the returned SolveResult owns its own memory (for solutions,
// solve stats, etc.), EXPECT for a pointer back to the model. As a result:
//  * Keep the model alive to access SolveResult,
//  * Avoid unnecessarily copying SolveResult,
//  * The result is generally accessible after mutating the model, but some care
//    is needed if variables or linear constraints are added or deleted.
//
// Asserts (using CHECK) that the inputs solve_args.model_parameters and
// solve_args.callback_registration only contain variables and constraints from
// the input model.
//
// Thread-safety: this method is safe to call concurrently on the same Model.
//
// Some solvers may add more restrictions regarding threading. Please see
// SolverType::kXxx documentation for details.
absl::StatusOr<SolveResult> Solve(const Model& model, SolverType solver_type,
                                  const SolveArguments& solve_args = {},
                                  const SolverInitArguments& init_args = {});

// Incremental solve of a model.
//
// This is a feature for advance users. Most users should only use the Solve()
// function above.
//
// Here incremental means that the we try to reuse the existing underlying
// solver internals between each solve. There is no guarantee though that the
// solver supports all possible model changes. Hence there is not guarantee that
// performances will be improved when using this class; this is solver
// dependent. Typically LPs have more to gain from incremental solve than
// MIPs. In both cases, even if the solver supports the model changes,
// incremental solve may actually be slower.
//
// The New() function instantiates the solver, setup it from the current state
// of the Model and register on it to listen to changes. Calling Solve() will
// update the underlying solver with latest model changes and solve this model.
//
// Usage:
//   Model model = ...;
//   ASSIGN_OR_RETURN(
//     const std::unique_ptr<IncrementalSolver> incremental_solve,
//     IncrementalSolver::New(&model, SolverType::kXxx));
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
// different IncrementalSolver instances on the same Model concurrently.
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
  struct UpdateResult {
    UpdateResult(const bool did_update, std::optional<ModelUpdateProto> update)
        : did_update(did_update), update(std::move(update)) {}

    // True if the solver has been successfully updated or if no update was
    // necessary (in which case `update` will be nullopt). False if the solver
    // had to be recreated.
    bool did_update;

    // The update that was attempted on the solver. Can be nullopt when no
    // update was needed (the model was not changed).
    std::optional<ModelUpdateProto> update;
  };

  // Creates a new incremental solve for the given model. It may returns an
  // error if the parameters are invalid (for example if the selected solver is
  // not linked in the binary).
  //
  // The returned IncrementalSolver keeps a copy of `arguments`. Thus the
  // content of arguments.non_streamable (for example pointers to solver
  // specific struct) must be valid until the destruction of the
  // IncrementalSolver. It also registers on the Model to keep track of updates
  // (see class documentation for details).
  static absl::StatusOr<std::unique_ptr<IncrementalSolver>> New(
      Model* model, SolverType solver_type, SolverInitArguments arguments = {});

  // Updates the underlying solver with latest model changes and runs the solve.
  //
  // A Status error will be returned if there is an unexpected failure in an
  // underlying solver or for some internal math_opt errors. Otherwise, check
  // SolveResult::termination.reason to see if an optimal solution was found.
  //
  // Memory model: the returned SolveResult owns its own memory (for solutions,
  // solve stats, etc.), EXPECT for a pointer back to the model. As a result:
  //  * Keep the model alive to access SolveResult,
  //  * Avoid unnecessarily copying SolveResult,
  //  * The result is generally accessible after mutating this, but some care
  //    is needed if variables or linear constraints are added or deleted.
  //
  // Asserts (using CHECK) that the inputs arguments.model_parameters and
  // arguments.callback_registration only contain variables and constraints from
  // the input model.
  //
  // See callback.h for documentation on arguments.callback and
  // arguments.callback_registration.
  absl::StatusOr<SolveResult> Solve(const SolveArguments& arguments = {});

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
  absl::StatusOr<UpdateResult> Update();

  // Same as Solve() but does not update the underlying solver with the latest
  // changes to the model.
  //
  // This is an advanced API, most users should use Solve().
  absl::StatusOr<SolveResult> SolveWithoutUpdate(
      const SolveArguments& arguments = {}) const;

 private:
  IncrementalSolver(SolverType solver_type, SolverInitArguments init_args,
                    const ModelStorage* expected_storage,
                    std::unique_ptr<UpdateTracker> update_tracker,
                    std::unique_ptr<Solver> solver);

  const SolverType solver_type_;
  const SolverInitArguments init_args_;
  const ModelStorage* const expected_storage_;
  const std::unique_ptr<UpdateTracker> update_tracker_;
  std::unique_ptr<Solver> solver_;
};

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_CPP_SOLVE_H_
