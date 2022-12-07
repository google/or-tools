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

#ifndef OR_TOOLS_MATH_OPT_SOLVERS_CP_SAT_SOLVER_H_
#define OR_TOOLS_MATH_OPT_SOLVERS_CP_SAT_SOLVER_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/core/inverted_bounds.h"
#include "ortools/math_opt/core/solve_interrupter.h"
#include "ortools/math_opt/core/solver_interface.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_parameters.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/parameters.pb.h"
#include "ortools/math_opt/result.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"

namespace operations_research {
namespace math_opt {

class CpSatSolver : public SolverInterface {
 public:
  static absl::StatusOr<std::unique_ptr<SolverInterface>> New(
      const ModelProto& model, const InitArgs& init_args);

  absl::StatusOr<SolveResultProto> Solve(
      const SolveParametersProto& parameters,
      const ModelSolveParametersProto& model_parameters,
      MessageCallback message_cb,
      const CallbackRegistrationProto& callback_registration, Callback cb,
      SolveInterrupter* interrupter) override;
  absl::StatusOr<bool> Update(const ModelUpdateProto& model_update) override;

 private:
  CpSatSolver(MPModelProto cp_sat_model, std::vector<int64_t> variable_ids,
              std::vector<int64_t> linear_constraint_ids);

  // Extract the solution from CP-SAT's response.
  SparseDoubleVectorProto ExtractSolution(
      absl::Span<const double> cp_sat_variable_values,
      const SparseVectorFilterProto& filter) const;

  // Returns the ids of variables and linear constraints with inverted bounds.
  InvertedBounds ListInvertedBounds() const;

  const MPModelProto cp_sat_model_;

  // For the i-th variable in `cp_sat_model_`, `variable_ids_[i]` contains the
  // corresponding id in the input `Model`.
  const std::vector<int64_t> variable_ids_;

  // For the i-th linear constraint in `cp_sat_model_`,
  // `linear_constraint_ids_[i]` contains the corresponding id in the input
  // `Model`.
  const std::vector<int64_t> linear_constraint_ids_;
};

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_SOLVERS_CP_SAT_SOLVER_H_
