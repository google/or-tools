// Copyright 2010-2022 Google LLC
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

#ifndef OR_TOOLS_MATH_OPT_CPP_SOLVER_INIT_ARGUMENTS_H_
#define OR_TOOLS_MATH_OPT_CPP_SOLVER_INIT_ARGUMENTS_H_

#include <memory>

#include "ortools/math_opt/core/non_streamable_solver_init_arguments.h"  // IWYU pragma: export
#include "ortools/math_opt/cpp/streamable_solver_init_arguments.h"  // IWYU pragma: export

namespace operations_research::math_opt {

// Arguments passed to Solve() and IncrementalSolver::New() to control the
// instantiation of the solver.
//
// For convenience, constructors with streamable or/and non-streamable arguments
// are provided. The non-streamable arguments are cloned so any change made
// after passing them to this class are ignored.
//
// Usage with streamable arguments:
//
//   Solve(model, SOLVER_TYPE_GUROBI, /*solver_args=*/{},
//         SolverInitArguments({
//           .gurobi = StreamableGurobiInitArguments{
//             .isv_key = GurobiISVKey{
//               .name = "some name",
//               .application_name = "some app name",
//               .expiration = -1,
//               .key = "random",
//             }
//           }
//         });
//
// Usage with non-streamable arguments:
//
//   NonStreamableGurobiInitArguments gurobi_args;
//   gurobi_args.primary_env = primary_env.get();
//
//   Solve(model, SOLVER_TYPE_GUROBI, /*solver_args=*/{},
//         SolverInitArguments(gurobi_args));
//
struct SolverInitArguments {
  SolverInitArguments() = default;

  // Initializes this class with the provided streamable arguments.
  explicit SolverInitArguments(StreamableSolverInitArguments streamable);

  // Initializes this class with a clone of the provided non-streamable
  // arguments.
  //
  // Note that since this constructors calls Clone() to initialize the
  // non_streamable_solver_init_arguments field, changes made after calling it
  // to the input non_streamable are ignored.
  explicit SolverInitArguments(
      const NonStreamableSolverInitArguments& non_streamable);

  // Initializes this class with both the provided streamable arguments and a
  // clone of the non-streamable ones.
  SolverInitArguments(StreamableSolverInitArguments streamable,
                      const NonStreamableSolverInitArguments& non_streamable);

  // Initializes this class as a copy of the provided arguments. The
  // non_streamable field is cloned if not nullptr.
  SolverInitArguments(const SolverInitArguments& other);

  // Sets this class as a copy of the provided arguments. The non_streamable
  // field is cloned if not nullptr.
  SolverInitArguments& operator=(const SolverInitArguments& other);

  SolverInitArguments(SolverInitArguments&&) = default;
  SolverInitArguments& operator=(SolverInitArguments&&) = default;

  StreamableSolverInitArguments streamable;

  // This should either be the solver specific class or nullptr.
  //
  // Solvers will fail (by returning an absl::Status) if called with arguments
  // for another solver.
  std::unique_ptr<const NonStreamableSolverInitArguments> non_streamable;
};

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_CPP_SOLVER_INIT_ARGUMENTS_H_
