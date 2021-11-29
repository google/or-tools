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

#ifndef OR_TOOLS_MATH_OPT_SOLVERS_GSCIP_SOLVER_H_
#define OR_TOOLS_MATH_OPT_SOLVERS_GSCIP_SOLVER_H_

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "ortools/base/integral_types.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "scip/type_cons.h"
#include "scip/type_var.h"
#include "ortools/gscip/gscip.h"
#include "ortools/gscip/gscip.pb.h"
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

class GScipSolver : public SolverInterface {
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

  static GScipParameters MergeCommonParameters(
      const CommonSolveParametersProto& common_solver_parameters,
      const GScipParameters& gscip_parameters);

 private:
  GScipSolver() = default;

  absl::Status AddVariables(const VariablesProto& variables,
                            const absl::flat_hash_map<int64_t, double>&
                                linear_objective_coefficients);
  absl::Status UpdateVariables(const VariableUpdatesProto& variable_updates);
  absl::Status AddLinearConstraints(
      const LinearConstraintsProto& linear_constraints,
      const SparseDoubleMatrixProto& linear_constraint_matrix);
  absl::Status UpdateLinearConstraints(
      const LinearConstraintUpdatesProto linear_constraint_updates,
      const SparseDoubleMatrixProto& linear_constraint_matrix);
  absl::flat_hash_set<SCIP_VAR*> LookupAllVariables(
      absl::Span<const int64_t> variable_ids);
  static absl::StatusOr<
      std::pair<SolveResultProto::TerminationReason, std::string>>
  ConvertTerminationReason(GScipOutput::Status gscip_status,
                           const std::string& gscip_status_detail,
                           bool has_feasible_solution);
  absl::StatusOr<SolveResultProto> CreateSolveResultProto(
      GScipResult gscip_result,
      const ModelSolveParametersProto& model_parameters);

  std::unique_ptr<GScip> gscip_;
  absl::flat_hash_map<int64_t, SCIP_VAR*> variables_;
  absl::flat_hash_map<int64_t, SCIP_CONS*> linear_constraints_;
  int64_t next_unused_variable_id_ = 0;
  int64_t next_unused_linear_constraint_id_ = 0;
};

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_SOLVERS_GSCIP_SOLVER_H_
