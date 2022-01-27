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

#ifndef OR_TOOLS_MATH_OPT_CORE_SOLVER_H_
#define OR_TOOLS_MATH_OPT_CORE_SOLVER_H_

#include <functional>
#include <memory>

#include "absl/status/statusor.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/core/model_summary.h"
#include "ortools/math_opt/core/solver_interface.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_parameters.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/parameters.pb.h"
#include "ortools/math_opt/result.pb.h"

namespace operations_research {
namespace math_opt {

// A solver for a given model and solver implementation.
//
// Use the New() function to build a new solver instance; then call Solve() to
// solve the model. You can then update the model using Update() and resolve.
//
// Usage:
//   const ModelProto model = ...;
//   const auto solver = Solver::New(SOLVER_TYPE_GSCIP,
//                                   model,
//                                   SolverInitializerProto{});
//   CHECK_OK(solver.status());
//   const SolveParametersProto solve_params = ...;
//
//   // First solve of the initial Model.
//   const auto first_solution = (*solver)->Solve(solve_params);
//   CHECK_OK(first_solution.status());
//   // Use the first_solution here.
//
//   // Update the Model with a ModelUpdate.
//   const ModelUpdate update = ...;
//   CHECK_OK((*solver)->Update(update));
//   const auto second_solution = (*solver)->Solve(solve_params);
//   CHECK_OK(second_solution.status());
//   // Use the second_solution of the updated problem here.
//
class Solver {
 public:
  // Callback function type.
  using Callback = std::function<CallbackResultProto(const CallbackDataProto&)>;

  // Builds a solver of the given type with the provided model and
  // initialization parameters.
  static absl::StatusOr<std::unique_ptr<Solver>> New(
      SolverType solver_type, const ModelProto& model,
      const SolverInitializerProto& initializer);

  Solver(const Solver&) = delete;
  Solver& operator=(const Solver&) = delete;

  // Solves the current model (included all updates).
  absl::StatusOr<SolveResultProto> Solve(
      const SolveParametersProto& parameters,
      const ModelSolveParametersProto& model_parameters = {},
      const CallbackRegistrationProto& callback_registration = {},
      Callback user_cb = nullptr);

  // Updates the model to solve and returns true, or returns false if this
  // update is not supported by the underlying solver.
  //
  // A status error will be returned if the model_update is invalid or the
  // underlying solver has an internal error.
  absl::StatusOr<bool> Update(const ModelUpdateProto& model_update);

 private:
  Solver(std::unique_ptr<SolverInterface> underlying_solver,
         ModelSummary model_summary);

  const std::unique_ptr<SolverInterface> underlying_solver_;
  ModelSummary model_summary_;
};

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_CORE_SOLVER_H_
