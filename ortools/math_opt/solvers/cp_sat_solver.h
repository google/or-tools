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

#ifndef OR_TOOLS_MATH_OPT_SOLVERS_CP_SAT_SOLVER_H_
#define OR_TOOLS_MATH_OPT_SOLVERS_CP_SAT_SOLVER_H_

#include <stdint.h>

#include <cstdint>
#include <memory>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/core/solver_interface.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_parameters.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/parameters.pb.h"
#include "ortools/math_opt/result.pb.h"
#include "ortools/math_opt/solution.pb.h"

namespace operations_research {
namespace math_opt {

class CpSatSolver : public SolverInterface {
 public:
  static absl::StatusOr<std::unique_ptr<SolverInterface>> New(
      const ModelProto& model, const SolverInitializerProto& initializer);

  absl::StatusOr<SolveResultProto> Solve(
      const SolveParametersProto& parameters,
      const ModelSolveParametersProto& model_parameters,
      const CallbackRegistrationProto& callback_registration,
      Callback cb) override;
  absl::Status Update(const ModelUpdateProto& model_update) override;
  bool CanUpdate(const ModelUpdateProto& model_update) override;

 private:
  CpSatSolver(MPModelProto cp_sat_model, std::vector<int64_t> variable_ids);

  // Extract the solution from CP-SAT's response.
  //
  // This function assumes it exists, i.e. that the input `response.status` is
  // feasible or optimal.
  PrimalSolutionProto ExtractSolution(
      const MPSolutionResponse& response,
      const ModelSolveParametersProto& model_parameters) const;

  const MPModelProto cp_sat_model_;

  // For the i-th variable in `cp_sat_model_`, `variable_ids_[i]` contains the
  // corresponding id in the input `Model`.
  const std::vector<int64_t> variable_ids_;
};

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_SOLVERS_CP_SAT_SOLVER_H_
