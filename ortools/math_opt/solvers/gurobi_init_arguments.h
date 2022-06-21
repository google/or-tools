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

#ifndef OR_TOOLS_MATH_OPT_SOLVERS_GUROBI_INIT_ARGUMENTS_H_
#define OR_TOOLS_MATH_OPT_SOLVERS_GUROBI_INIT_ARGUMENTS_H_

#include <optional>

#include "absl/status/statusor.h"
#include "ortools/math_opt/core/non_streamable_solver_init_arguments.h"
#include "ortools/math_opt/parameters.pb.h"
#include "ortools/math_opt/solvers/gurobi.pb.h"
#include "ortools/math_opt/solvers/gurobi/g_gurobi.h"

namespace operations_research {
namespace math_opt {

// Returns a new primary environment.
//
// The typical use of this function is to share the same environment between
// multiple solver instances. This is necessary when a single-use license is
// used since only one primary environment can exists in that case.
//
// A single primary environment is not thread-safe and thus it should only be
// used in a single thread. Even if the user has a license that authorizes
// multiple primary environments, Gurobi still recommends to use only one and to
// share it as it is more efficient (see GRBloadenv() documentation).
//
// Of course, if the user wants to run multiple solves in parallel and has a
// license that authorizes that, one environment should be used per thread.
//
// The primary environment can be passed to MathOpt via the
// NonStreamableGurobiInitArguments structure and its primary_env field.
//
// The optional ISV key can be used to build the environment from an ISV key
// instead of using the default license file. See
// http://www.gurobi.com/products/licensing-pricing/isv-program for details.
//
// Example with default license file:
//
//   // Solving two models on the same thread, sharing the same primary
//   // environment.
//   Model model_1;
//   Model model_2;
//
//   ...
//
//   ASSIGN_OR_RETURN(const GRBenvUniquePtr primary_env,
//                    NewPrimaryEnvironment());
//
//   NonStreamableGurobiInitArguments gurobi_args;
//   gurobi_args.primary_env = primary_env.get();
//
//   ASSIGN_OR_RETURN(
//       const std::unique_ptr<IncrementalSolver> incremental_solve_1,
//       IncrementalSolver::New(model, SOLVER_TYPE_GUROBI,
//                              SolverInitArguments(gurobi_args)));
//   ASSIGN_OR_RETURN(
//       const std::unique_ptr<IncrementalSolver> incremental_solve_2,
//       IncrementalSolver::New(model, SOLVER_TYPE_GUROBI,
//                              SolverInitArguments(gurobi_args)));
//
//   ASSIGN_OR_RETURN(const SolveResult result_1, incremental_solve_1->Solve());
//   ASSIGN_OR_RETURN(const SolveResult result_2, incremental_solve_2->Solve());
//
//
// With ISV key:
//
//   ASSIGN_OR_RETURN(const GRBenvUniquePtr primary_env,
//                    NewPrimaryEnvironment(GurobiISVKey{
//                        .name = "the name",
//                        .application_name = "the application",
//                        .expiration = 0,
//                        .key = "...",
//                    }.Proto()));
//
absl::StatusOr<GRBenvUniquePtr> NewPrimaryEnvironment(
    std::optional<GurobiInitializerProto::ISVKey> proto_isv_key = {});

// Non-streamable Gurobi specific parameters for solver instantiation.
//
// See NonStreamableSolverInitArguments for details.
struct NonStreamableGurobiInitArguments
    : public NonStreamableSolverInitArgumentsHelper<
          NonStreamableGurobiInitArguments, SOLVER_TYPE_GUROBI> {
  // Primary environment to use. This is only useful to pass when either the
  // default primary environment created by the solver implementation is not
  // enough or when multiple Gurobi solvers are used with a single-use
  // license. In the latter case, only one primary environment can be created so
  // it must be shared.
  //
  // The solver does not take ownership of the environment, it is the
  // responsibility of the caller to properly dispose of it after all solvers
  // that used it have been destroyed.
  GRBenv* primary_env = nullptr;

  const NonStreamableGurobiInitArguments* ToNonStreamableGurobiInitArguments()
      const override {
    return this;
  }
};

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_SOLVERS_GUROBI_INIT_ARGUMENTS_H_
