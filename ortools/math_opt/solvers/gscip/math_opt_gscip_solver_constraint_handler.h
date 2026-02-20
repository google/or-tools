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

#ifndef ORTOOLS_MATH_OPT_SOLVERS_GSCIP_MATH_OPT_GSCIP_SOLVER_CONSTRAINT_HANDLER_H_
#define ORTOOLS_MATH_OPT_SOLVERS_GSCIP_MATH_OPT_GSCIP_SOLVER_CONSTRAINT_HANDLER_H_

#include <cstdint>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "ortools/base/linked_hash_map.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/core/solver_interface.h"
#include "ortools/math_opt/solvers/gscip/gscip.h"
#include "ortools/math_opt/solvers/gscip/gscip_callback_result.h"
#include "ortools/math_opt/solvers/gscip/gscip_constraint_handler.h"
#include "scip/type_var.h"

namespace operations_research::math_opt {

struct GScipSolverConstraintData {
  SolverInterface::Callback user_callback = nullptr;
  const gtl::linked_hash_map<int64_t, SCIP_VAR*>* variables = nullptr;
  const SparseVectorFilterProto* variable_node_filter = nullptr;
  const SparseVectorFilterProto* variable_solution_filter = nullptr;
  absl::Time solve_start_time = absl::UnixEpoch();
  bool run_at_nodes = false;
  bool run_at_solutions = false;
  bool adds_cuts = false;
  bool adds_lazy_constraints = false;
  GScip::Interrupter* interrupter = nullptr;

  void SetWhenRunAndAdds(const CallbackRegistrationProto& registration);

  // Ensures that when GScipSolverConstraintData::user_callback != nullptr, we
  // also have that variables, variable_node_filter, variable_solution_filter,
  // and interrupter are not nullptr as well. In a callback, when user_callback
  // is nullptr, do not access these fields!
  absl::Status Validate() const;
};

class GScipSolverConstraintHandler
    : public GScipConstraintHandler<GScipSolverConstraintData> {
 public:
  GScipSolverConstraintHandler();

 private:
  absl::StatusOr<GScipCallbackResult> EnforceLp(
      GScipConstraintHandlerContext context,
      const GScipSolverConstraintData& constraint_data,
      bool solution_infeasible) override;

  absl::StatusOr<bool> CheckIsFeasible(
      GScipConstraintHandlerContext context,
      const GScipSolverConstraintData& constraint_data, bool check_integrality,
      bool check_lp_rows, bool print_reason, bool check_completely) override;

  absl::StatusOr<GScipCallbackResult> SeparateLp(
      GScipConstraintHandlerContext context,
      const GScipSolverConstraintData& constraint_data) override;

  absl::StatusOr<GScipCallbackResult> SeparateSolution(
      GScipConstraintHandlerContext context,
      const GScipSolverConstraintData& constraint_data) override;

  std::vector<std::pair<SCIP_VAR*, RoundingLockDirection>> RoundingLock(
      GScip* gscip, const GScipSolverConstraintData& constraint_data,
      bool lock_type_is_model) override;

  // Requires that constraint_data.Validate() has already been called.
  absl::StatusOr<CallbackDataProto> MakeCbData(
      GScipConstraintHandlerContext& context,
      const GScipSolverConstraintData& constraint_data,
      CallbackEventProto event);

  // If ok, returned value will be one of {cutoff, lazy, cut, feasible}.
  //
  // Requires that constraint_data.Validate() has already been called.
  absl::StatusOr<GScipCallbackResult> ApplyCallback(
      const CallbackResultProto& result, GScipConstraintHandlerContext& context,
      const GScipSolverConstraintData& constraint_data,
      ConstraintHandlerCallbackType scip_cb_type);
};

}  // namespace operations_research::math_opt

#endif  // ORTOOLS_MATH_OPT_SOLVERS_GSCIP_MATH_OPT_GSCIP_SOLVER_CONSTRAINT_HANDLER_H_
