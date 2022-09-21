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

#ifndef OR_TOOLS_MATH_OPT_SOLVERS_GLOP_SOLVER_H_
#define OR_TOOLS_MATH_OPT_SOLVERS_GLOP_SOLVER_H_

#include <stdint.h>

#include <atomic>
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
#include "ortools/math_opt/core/inverted_bounds.h"
#include "ortools/math_opt/core/solve_interrupter.h"
#include "ortools/math_opt/core/solver_interface.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_parameters.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/parameters.pb.h"
#include "ortools/math_opt/result.pb.h"
#include "ortools/math_opt/solution.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"

namespace operations_research {
namespace math_opt {

class GlopSolver : public SolverInterface {
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

  // Returns the merged parameters and a list of warnings from any parameter
  // settings that are invalid for this solver.
  static absl::StatusOr<glop::GlopParameters> MergeSolveParameters(
      const SolveParametersProto& solver_parameters, bool setting_initial_basis,
      bool has_message_callback);

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

  // Returns the ids of variables and linear constraints with inverted bounds.
  InvertedBounds ListInvertedBounds() const;

  void FillSolution(glop::ProblemStatus status,
                    const ModelSolveParametersProto& model_parameters,
                    SolveResultProto& solve_result);
  absl::StatusOr<SolveResultProto> MakeSolveResult(
      glop::ProblemStatus status,
      const ModelSolveParametersProto& model_parameters,
      const SolveInterrupter* interrupter, absl::Duration solve_time);

  absl::Status FillSolveStats(const glop::ProblemStatus status,
                              absl::Duration solve_time,
                              SolveStatsProto& solve_stats);

  void SetGlopBasis(const BasisProto& basis);

  glop::LinearProgram linear_program_;
  glop::LPSolver lp_solver_;

  absl::flat_hash_map<int64_t, glop::ColIndex> variables_;
  absl::flat_hash_map<int64_t, glop::RowIndex> linear_constraints_;
};

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_SOLVERS_GLOP_SOLVER_H_
