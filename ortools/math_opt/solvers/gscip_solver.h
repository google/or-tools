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

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "ortools/gscip/gscip.h"
#include "ortools/gscip/gscip.pb.h"
#include "ortools/gscip/gscip_event_handler.h"
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
#include "scip/type_cons.h"
#include "scip/type_var.h"

namespace operations_research {
namespace math_opt {

class GScipSolver : public SolverInterface {
 public:
  static absl::StatusOr<std::unique_ptr<SolverInterface>> New(
      const ModelProto& model, const InitArgs& init_args);

  absl::StatusOr<SolveResultProto> Solve(
      const SolveParametersProto& parameters,
      const ModelSolveParametersProto& model_parameters,
      MessageCallback message_cb,
      const CallbackRegistrationProto& callback_registration, Callback cb,
      SolveInterrupter* interrupter) override;
  absl::Status Update(const ModelUpdateProto& model_update) override;
  bool CanUpdate(const ModelUpdateProto& model_update) override;

  // Returns the merged parameters and a list of warnings for unsupported
  // parameters.
  static absl::StatusOr<GScipParameters> MergeParameters(
      const SolveParametersProto& solve_parameters);

 private:
  // Event handler that it used to call SCIPinterruptSolve() is a safe manner.
  //
  // At the start of SCIPsolve(), SCIP resets `userinterrupt` to false. It does
  // the same in SCIPpresolve(), which is called at the beginning of SCIPsolve()
  // but also at the beginning of each restart. the `userinterrupt` can also be
  // reset when the transformed problem is freed when the parameter
  // "misc/resetstat" is used. On top of that, it is not possible to call
  // SCIPinterruptSolve() in SCIP_STAGE_INITSOLVE stage; which occurs in the
  // middle of the solve and at restarts.
  //
  // If this was no enough, SCIPinterruptSolve() calls SCIPcheckStage() which is
  // not thread-safe.
  //
  // As a consequence, although it is possible to call SCIPinterruptSolve() from
  // another thread, it is unreliable at best. Here we take a safer approach: we
  // call it only from the Exec() of an even handler. This solves all thread
  // safety issues and, if we have been careful, also ensures we don't call it
  // in the wrong stage. This also solves the issue the multiple resets of the
  // `userinterrupt` flag since each time we are called after the interrupter
  // has been triggered, we simply call SCIPinterruptSolve() until SCIP finally
  // listens.
  struct InterruptEventHandler : public GScipEventHandler {
    InterruptEventHandler();

    SCIP_RETCODE Init(GScip* gscip) override;
    SCIP_RETCODE Execute(GScipEventHandlerContext) override;

    // Calls SCIPinterruptSolve() if the interrupter is set and triggered and
    // SCIP is in a valid stage for that.
    SCIP_RETCODE TryCallInterruptIfNeeded(GScip* gscip);

    // This will be set before SCIPsolve() is called and reset after the end of
    // the call. It may be null when the user does not provide an interrupter;
    // in that case we don't register any event.
    SolveInterrupter* interrupter = nullptr;
  };

  explicit GScipSolver(std::unique_ptr<GScip> gscip);

  // Adds the new variables.
  absl::Status AddVariables(const VariablesProto& variables,
                            const absl::flat_hash_map<int64_t, double>&
                                linear_objective_coefficients);

  // Update existing variables' parameters.
  absl::Status UpdateVariables(const VariableUpdatesProto& variable_updates);

  // Adds the new linear constraints.
  absl::Status AddLinearConstraints(
      const LinearConstraintsProto& linear_constraints,
      const SparseDoubleMatrixProto& linear_constraint_matrix);

  // Updates the existing constraints and the coefficients of the existing
  // variables of these constraints.
  absl::Status UpdateLinearConstraints(
      const LinearConstraintUpdatesProto linear_constraint_updates,
      const SparseDoubleMatrixProto& linear_constraint_matrix,
      std::optional<int64_t> first_new_var_id,
      std::optional<int64_t> first_new_cstr_id);

  absl::flat_hash_set<SCIP_VAR*> LookupAllVariables(
      absl::Span<const int64_t> variable_ids);
  absl::StatusOr<SolveResultProto> CreateSolveResultProto(
      GScipResult gscip_result,
      const ModelSolveParametersProto& model_parameters,
      std::optional<double> cutoff);

  // Returns the ids of variables and linear constraints with inverted bounds.
  InvertedBounds ListInvertedBounds() const;

  const std::unique_ptr<GScip> gscip_;
  InterruptEventHandler interrupt_event_handler_;
  absl::flat_hash_map<int64_t, SCIP_VAR*> variables_;
  absl::flat_hash_map<int64_t, SCIP_CONS*> linear_constraints_;
};

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_SOLVERS_GSCIP_SOLVER_H_
