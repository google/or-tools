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

#ifndef OR_TOOLS_MATH_OPT_SOLVERS_GLOP_SOLVER_H_
#define OR_TOOLS_MATH_OPT_SOLVERS_GLOP_SOLVER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "ortools/glop/lp_solver.h"
#include "ortools/glop/parameters.pb.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/core/solver_interface.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_parameters.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/parameters.pb.h"
#include "ortools/math_opt/result.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"

namespace operations_research {
namespace math_opt {

class GlopSolver : public SolverInterface {
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

  // Returns the merged parameters and a list of warnings from any parameter
  // settings that are invalid for this solver.
  static std::pair<glop::GlopParameters, std::vector<std::string>>
  MergeCommonParameters(
      const CommonSolveParametersProto& common_solver_parameters,
      const glop::GlopParameters& glop_parameters);

 private:
  GlopSolver();

  void AddVariables(const VariablesProto& variables);
  void AddLinearConstraints(const LinearConstraintsProto& linear_constraints);

  void DeleteVariables(absl::Span<const int64_t> ids_to_delete);
  void DeleteLinearConstraints(absl::Span<const int64_t> ids_to_delete);

  void SetOrUpdateObjectiveCoefficients(
      const SparseDoubleVectorProto& linear_objective_coefficients);
  void SetOrUpdateConstraintMatrix(
      const SparseDoubleMatrixProto& linear_constraint_matrix);

  void UpdateVariableBounds(const VariableUpdatesProto& variable_updates);
  void UpdateLinearConstraintBounds(
      const LinearConstraintUpdatesProto& linear_constraint_updates);

  void FillSolveResult(glop::ProblemStatus status,
                       const ModelSolveParametersProto& model_parameters,
                       SolveResultProto& solve_result);

  glop::LinearProgram linear_program_;
  glop::LPSolver lp_solver_;

  absl::flat_hash_map<int64_t, glop::ColIndex> variables_;
  absl::flat_hash_map<int64_t, glop::RowIndex> linear_constraints_;
};

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_SOLVERS_GLOP_SOLVER_H_
