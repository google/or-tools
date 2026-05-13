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

// Functions and classes used to solve a Model.
//
// The main entry point is the Solve() function.
//
// For users that need incremental solving, there is the IncrementalSolver
// class.

#ifndef OR_TOOLS_MATH_OPT_CPP_SOLVE_H_
#define OR_TOOLS_MATH_OPT_CPP_SOLVE_H_

#include <functional>
#include <memory>

#include "absl/status/statusor.h"
#include "ortools/math_opt/cpp/compute_infeasible_subsystem_arguments.h"  // IWYU pragma: export
#include "ortools/math_opt/cpp/compute_infeasible_subsystem_result.h"  // IWYU pragma: export
#include "ortools/math_opt/cpp/incremental_solver.h"  // IWYU pragma: export
#include "ortools/math_opt/cpp/model.h"
#include "ortools/math_opt/cpp/parameters.h"             // IWYU pragma: export
#include "ortools/math_opt/cpp/solve_arguments.h"        // IWYU pragma: export
#include "ortools/math_opt/cpp/solve_result.h"           // IWYU pragma: export
#include "ortools/math_opt/cpp/solver_init_arguments.h"  // IWYU pragma: export
#include "ortools/math_opt/cpp/update_result.h"          // IWYU pragma: export
#include "ortools/math_opt/parameters.pb.h"              // IWYU pragma: export

namespace operations_research {
namespace math_opt {

// Solves the input model.
//
// A Status error will be returned if the inputs are invalid or there is an
// unexpected failure in an underlying solver or for some internal math_opt
// errors. Otherwise, check SolveResult::termination.reason to see if an optimal
// solution was found.
//
// Memory model: the returned SolveResult owns its own memory (for solutions,
// solve stats, etc.), EXCEPT for a pointer back to the model. As a result:
//  * Keep the model alive to access SolveResult,
//  * Avoid unnecessarily copying SolveResult,
//  * The result is generally accessible after mutating the model, but some care
//    is needed if variables or linear constraints are added or deleted.
//
// Thread-safety: this method is safe to call concurrently on the same Model.
//
// Some solvers may add more restrictions regarding threading. Please see
// SolverType::kXxx documentation for details.
absl::StatusOr<SolveResult> Solve(const Model& model, SolverType solver_type,
                                  const SolveArguments& solve_args = {},
                                  const SolverInitArguments& init_args = {});

// The type of a standard function with the same signature as Solve() above.
//
// If you want mock the Solve() for testing, you can take a SolveFunction as
// an argument, e.g.
//    absl::Status DoMySolve(SolveFunction solve_function=Solve) {
//      Model model;
//      // fill in model...
//      SolveArguments args;
//      SolveInitArguments init_args;
//      ASSIGN_OR_RETURN(
//        const SolveResult result,
//        solve_function(model, SolverType::kGscip, args, init_args));
//      // process result...
//      return absl::OkStatus();
//    }
using SolveFunction =
    std::function<absl::StatusOr<operations_research::math_opt::SolveResult>(
        const operations_research::math_opt::Model&,
        operations_research::math_opt::SolverType,
        const operations_research::math_opt::SolveArguments&,
        const operations_research::math_opt::SolverInitArguments&)>;

// Computes an infeasible subsystem of the input model.
//
// A Status error will be returned if the inputs are invalid or there is an
// unexpected failure in an underlying solver or for some internal math_opt
// errors. Otherwise, check ComputeInfeasibleSubsystemResult::feasibility to see
// if an infeasible subsystem was found.
//
// Memory model: the returned ComputeInfeasibleSubsystemResult owns its own
// memory (for subsystems, solve stats, etc.), EXCEPT for a pointer back to the
// model. As a result:
//  * Keep the model alive to access ComputeInfeasibleSubsystemResult,
//  * Avoid unnecessarily copying ComputeInfeasibleSubsystemResult,
//  * The result is generally accessible after mutating the model, but some care
//    is needed if variables or linear constraints are added or deleted.
//
// Thread-safety: this method is safe to call concurrently on the same Model.
absl::StatusOr<ComputeInfeasibleSubsystemResult> ComputeInfeasibleSubsystem(
    const Model& model, SolverType solver_type,
    const ComputeInfeasibleSubsystemArguments& compute_args = {},
    const SolverInitArguments& init_args = {});

// Creates a new incremental solve for the given model. It may returns an
// error if the parameters are invalid (for example if the selected solver is
// not linked in the binary).
//
// The returned IncrementalSolver keeps a copy of `arguments`. Thus the
// content of arguments.non_streamable (for example pointers to solver
// specific struct) must be valid until the destruction of the
// IncrementalSolver. It also registers on the Model to keep track of updates
// (see class documentation for details).
absl::StatusOr<std::unique_ptr<IncrementalSolver>> NewIncrementalSolver(
    Model* model, SolverType solver_type, SolverInitArguments arguments = {});

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_CPP_SOLVE_H_
