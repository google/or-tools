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
// Usage with streamable arguments:
//
//   Solve(model, SOLVER_TYPE_GUROBI, /*solver_args=*/{},
//         SolverInitArguments{
//           .streamable = {
//             .gurobi = StreamableGurobiInitArguments{
//               .isv_key = GurobiISVKey{
//                 .name = "some name",
//                 .application_name = "some app name",
//                 .expiration = -1,
//                 .key = "random",
//               }
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
//         SolverInitArguments{.non_streamable = gurobi_args});
//
struct SolverInitArguments {
  // Solver initialization parameters that can be streamed to be exchanged with
  // another process.
  StreamableSolverInitArguments streamable;

  // Solver specific initialization parameters that can't be streamed. This
  // should either be the solver specific class or be unset.
  //
  // Solvers will fail (by returning an absl::Status) if called with arguments
  // for another solver.
  NonStreamableSolverInitArgumentsValue non_streamable;

  // If true, the names of variables and constraints are discarded before
  // sending them to the solver. This is particularly useful for models that
  // need to be serialized and are near the two gigabyte limit in proto form.
  bool remove_names = false;
};

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_CPP_SOLVER_INIT_ARGUMENTS_H_
