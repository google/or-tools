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

#include "ortools/math_opt/solvers/gscip/math_opt_gscip_solver_constraint_handler.h"

#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "ortools/base/protoutil.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/core/math_opt_proto_utils.h"
#include "ortools/math_opt/solvers/gscip/gscip.h"
#include "ortools/math_opt/solvers/gscip/gscip_callback_result.h"
#include "ortools/math_opt/solvers/gscip/gscip_constraint_handler.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/port/proto_utils.h"
#include "scip/type_var.h"

namespace operations_research::math_opt {
namespace {

// We set all priorities to -9'999'998, rather than the default of -1, so that
// our callback only checks constraints after all the constraints that are part
// of the model (e.g. linear constraints have enforcement priority -1'000'000).
// We still want to run before the count solutions constraint handler, which is
// -9'999'999. All the constraints appears to separate with priority >= 0, but
// if we want to run last, we can still pick -9'999'998. E.g. see:
// https://stackoverflow.com/questions/72921074/can-i-set-the-scip-constraint-handler-to-work-only-after-a-feasible-solution-is
//
// Note that these priorities are different from the GScip defaults in
// gscip_constraint_handler.h. Because we are forcing SCIPs API to look more
// like Gurobi's in MathOpt, the GScip defaults make less sense.
GScipConstraintHandlerProperties MakeHandlerProperties() {
  return {
      .name = "GScipSolverConstraintHandler",
      .description = "A single handler for all mathopt callbacks",
      .enforcement_priority = -9'999'998,
      .feasibility_check_priority = -9'999'998,
      .separation_priority = -9'999'998,
  };
}
}  // namespace

absl::Status GScipSolverConstraintData::Validate() const {
  if (user_callback == nullptr) {
    return absl::OkStatus();
  }
  if (variables == nullptr) {
    return absl::InternalError(
        "GScipSolverConstraintData::variables must be set when "
        "GScipSolverConstraintData::user_callback is not null");
  }
  if (variable_node_filter == nullptr) {
    return absl::InternalError(
        "GScipSolverConstraintData::variable_node_filter must be set when "
        "GScipSolverConstraintData::variable_node_filter is not null");
  }
  if (variable_solution_filter == nullptr) {
    return absl::InternalError(
        "GScipSolverConstraintData::variable_solution_filter must be set when "
        "GScipSolverConstraintData::user_callback is not null");
  }
  if (interrupter == nullptr) {
    return absl::InternalError(
        "GScipSolverConstraintData::interrupter must be set when "
        "GScipSolverConstraintData::user_callback is not null");
  }
  return absl::OkStatus();
}

void GScipSolverConstraintData::SetWhenRunAndAdds(
    const CallbackRegistrationProto& registration) {
  for (const int event_int : registration.request_registration()) {
    switch (static_cast<CallbackEventProto>(event_int)) {
      case CALLBACK_EVENT_MIP_NODE:
        run_at_nodes = true;
        break;
      case CALLBACK_EVENT_MIP_SOLUTION:
        run_at_solutions = true;
        break;
      default:
        break;
    }
  }
  adds_cuts = registration.add_cuts();
  adds_lazy_constraints = registration.add_lazy_constraints();
}

GScipSolverConstraintHandler::GScipSolverConstraintHandler()
    : GScipConstraintHandler(MakeHandlerProperties()) {}

absl::StatusOr<GScipCallbackResult> GScipSolverConstraintHandler::EnforceLp(
    GScipConstraintHandlerContext context,
    const GScipSolverConstraintData& constraint_data,
    bool solution_infeasible) {
  RETURN_IF_ERROR(constraint_data.Validate());
  if (!constraint_data.run_at_solutions ||
      constraint_data.user_callback == nullptr) {
    return GScipCallbackResult::kFeasible;
  }
  ASSIGN_OR_RETURN(
      const CallbackDataProto cb_data,
      MakeCbData(context, constraint_data, CALLBACK_EVENT_MIP_SOLUTION));

  ASSIGN_OR_RETURN(const CallbackResultProto result,
                   constraint_data.user_callback(cb_data));
  return ApplyCallback(result, context, constraint_data,
                       ConstraintHandlerCallbackType::kEnfoLp);
}

absl::StatusOr<bool> GScipSolverConstraintHandler::CheckIsFeasible(
    GScipConstraintHandlerContext context,
    const GScipSolverConstraintData& constraint_data,
    const bool check_integrality, const bool check_lp_rows,
    const bool print_reason, const bool check_completely) {
  if (check_completely) {
    return absl::InternalError(
        "check_completely inside of CONSCHECK not supported. This is called "
        "only if you have set some SCIP parameters manually, e.g. "
        "display/allviols=TRUE");
  }
  RETURN_IF_ERROR(constraint_data.Validate());
  if (!constraint_data.run_at_solutions ||
      constraint_data.user_callback == nullptr) {
    return true;
  }
  ASSIGN_OR_RETURN(
      const CallbackDataProto cb_data,
      MakeCbData(context, constraint_data, CALLBACK_EVENT_MIP_SOLUTION));
  ASSIGN_OR_RETURN(const CallbackResultProto result,
                   constraint_data.user_callback(cb_data));
  ASSIGN_OR_RETURN(const GScipCallbackResult cb_result,
                   ApplyCallback(result, context, constraint_data,
                                 ConstraintHandlerCallbackType::kConsCheck));
  return cb_result == GScipCallbackResult::kFeasible;
}

absl::StatusOr<GScipCallbackResult> GScipSolverConstraintHandler::SeparateLp(
    GScipConstraintHandlerContext context,
    const GScipSolverConstraintData& constraint_data) {
  RETURN_IF_ERROR(constraint_data.Validate());
  if (!constraint_data.run_at_nodes ||
      constraint_data.user_callback == nullptr) {
    return GScipCallbackResult::kDidNotFind;
  }
  ASSIGN_OR_RETURN(
      const CallbackDataProto cb_data,
      MakeCbData(context, constraint_data, CALLBACK_EVENT_MIP_NODE));
  ASSIGN_OR_RETURN(const CallbackResultProto result,
                   constraint_data.user_callback(cb_data));
  ASSIGN_OR_RETURN(const GScipCallbackResult cb_result,
                   ApplyCallback(result, context, constraint_data,
                                 ConstraintHandlerCallbackType::kSepaLp));
  if (cb_result == GScipCallbackResult::kFeasible) {
    return GScipCallbackResult::kDidNotFind;
  }
  return cb_result;
}

absl::StatusOr<GScipCallbackResult>
GScipSolverConstraintHandler::SeparateSolution(
    GScipConstraintHandlerContext context,
    const GScipSolverConstraintData& constraint_data) {
  RETURN_IF_ERROR(constraint_data.Validate());
  if (!constraint_data.run_at_solutions ||
      constraint_data.user_callback == nullptr) {
    return GScipCallbackResult::kDidNotRun;
  }
  ASSIGN_OR_RETURN(
      const CallbackDataProto cb_data,
      MakeCbData(context, constraint_data, CALLBACK_EVENT_MIP_SOLUTION));
  ASSIGN_OR_RETURN(const CallbackResultProto result,
                   constraint_data.user_callback(cb_data));
  ASSIGN_OR_RETURN(const GScipCallbackResult cb_result,
                   ApplyCallback(result, context, constraint_data,
                                 ConstraintHandlerCallbackType::kSepaSol));
  if (cb_result == GScipCallbackResult::kFeasible) {
    return GScipCallbackResult::kDidNotFind;
  }
  return cb_result;
}

absl::StatusOr<CallbackDataProto> GScipSolverConstraintHandler::MakeCbData(
    GScipConstraintHandlerContext& context,
    const GScipSolverConstraintData& constraint_data,
    const CallbackEventProto event) {
  if (event != CALLBACK_EVENT_MIP_NODE &&
      event != CALLBACK_EVENT_MIP_SOLUTION) {
    return util::InternalErrorBuilder()
           << "Only events MIP_NODE and MIP_SOLUTION are supported, but was "
              "invoked on event: "
           << ProtoEnumToString(event);
  }
  CallbackDataProto cb_data;
  cb_data.set_event(event);
  const SparseVectorFilterProto* filter =
      event == CALLBACK_EVENT_MIP_NODE
          ? constraint_data.variable_node_filter
          : constraint_data.variable_solution_filter;
  auto& var_values = *cb_data.mutable_primal_solution_vector();
  SparseVectorFilterPredicate predicate(*filter);
  for (const auto [var_id, scip_var] : *constraint_data.variables) {
    const double value = context.VariableValue(scip_var);
    if (predicate.AcceptsAndUpdate(var_id, value)) {
      var_values.add_ids(var_id);
      var_values.add_values(value);
    }
  }
  const GScipCallbackStats& stats = context.stats();
  CallbackDataProto::MipStats& cb_stats = *cb_data.mutable_mip_stats();
  cb_stats.set_primal_bound(stats.primal_bound);
  cb_stats.set_dual_bound(stats.dual_bound);
  cb_stats.set_explored_nodes(stats.num_processed_nodes_total);
  cb_stats.set_open_nodes(stats.num_nodes_left);
  // TODO(b/314630175): maybe this should include diving/probing iterations
  // and strong branching iterations as well, see SCIPgetNDivingLPIterations
  // and SCIPgetNStrongbranchLPIterations
  cb_stats.set_simplex_iterations(stats.primal_simplex_iterations +
                                  stats.dual_simplex_iterations);
  cb_stats.set_number_of_solutions_found(stats.num_solutions_found);
  cb_stats.set_cutting_planes_in_lp(stats.num_cuts_in_lp);
  const absl::Duration elapsed = absl::Now() - constraint_data.solve_start_time;
  ASSIGN_OR_RETURN(*cb_data.mutable_runtime(),
                   util_time::EncodeGoogleApiProto(elapsed));
  return cb_data;
}

absl::StatusOr<GScipCallbackResult> GScipSolverConstraintHandler::ApplyCallback(
    const CallbackResultProto& result, GScipConstraintHandlerContext& context,
    const GScipSolverConstraintData& constraint_data,
    ConstraintHandlerCallbackType scip_cb_type) {
  if (!result.suggested_solutions().empty()) {
    return absl::UnimplementedError(
        "suggested solution is not yet implemented for SCIP callbacks in "
        "MathOpt");
  }
  GScipCallbackResult cb_result = GScipCallbackResult::kFeasible;
  for (const CallbackResultProto::GeneratedLinearConstraint& cut :
       result.cuts()) {
    GScipLinearRange scip_constraint{.lower_bound = cut.lower_bound(),
                                     .upper_bound = cut.upper_bound()};
    for (int i = 0; i < cut.linear_expression().ids_size(); ++i) {
      scip_constraint.variables.push_back(
          constraint_data.variables->at(cut.linear_expression().ids(i)));
      scip_constraint.coefficients.push_back(cut.linear_expression().values(i));
    }
    if (cut.is_lazy()) {
      RETURN_IF_ERROR(context.AddLazyLinearConstraint(scip_constraint, ""));
      cb_result = MergeConstraintHandlerResults(
          cb_result, GScipCallbackResult::kConstraintAdded, scip_cb_type);
    } else {
      ASSIGN_OR_RETURN(const GScipCallbackResult cut_result,
                       context.AddCut(scip_constraint, ""));
      cb_result =
          MergeConstraintHandlerResults(cb_result, cut_result, scip_cb_type);
    }
  }
  if (result.terminate()) {
    // NOTE: we do not know what the current stage is, this is safer than
    // calling SCIPinterruptSolve() directly.
    constraint_data.interrupter->Interrupt();
  }
  return cb_result;
}

std::vector<std::pair<SCIP_VAR*, RoundingLockDirection>>
GScipSolverConstraintHandler::RoundingLock(
    GScip* gscip, const GScipSolverConstraintData& constraint_data,
    const bool lock_type_is_model) {
  // Warning: we do not call constraint_data.Validate() because this function
  // cannot propagate status errors. As implemented, this function does not
  // access the members of constraint_data checked by Validate().
  const bool generates_constraints =
      constraint_data.adds_cuts || constraint_data.adds_lazy_constraints;
  if (constraint_data.user_callback == nullptr || !generates_constraints) {
    return {};
  }
  std::vector<std::pair<SCIP_VAR*, RoundingLockDirection>> result;
  for (SCIP_VAR* var : gscip->variables()) {
    result.push_back({var, RoundingLockDirection::kBoth});
  }
  return result;
}

}  // namespace operations_research::math_opt
