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

#include "ortools/gscip/gscip_constraint_handler.h"

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/types/span.h"
#include "ortools/base/status_macros.h"
#include "ortools/gscip/gscip.h"
#include "ortools/gscip/gscip_callback_result.h"
#include "ortools/linear_solver/scip_helper_macros.h"
#include "scip/def.h"
#include "scip/pub_cons.h"
#include "scip/pub_message.h"
#include "scip/pub_var.h"
#include "scip/scip_cons.h"
#include "scip/scip_cut.h"
#include "scip/scip_general.h"
#include "scip/scip_lp.h"
#include "scip/scip_sol.h"
#include "scip/scip_solvingstats.h"
#include "scip/scip_tree.h"
#include "scip/scip_var.h"
#include "scip/struct_cons.h"
#include "scip/struct_tree.h"  // IWYU pragma: keep
#include "scip/type_cons.h"
#include "scip/type_lp.h"
#include "scip/type_retcode.h"
#include "scip/type_scip.h"
#include "scip/type_set.h"
#include "scip/type_tree.h"
#include "scip/type_var.h"

struct SCIP_ConshdlrData {
  std::unique_ptr<operations_research::internal::UntypedGScipConstraintHandler>
      gscip_handler;
  operations_research::GScip* gscip = nullptr;
};

struct SCIP_ConsData {
  void* data;
};

using GScipHandler =
    operations_research::internal::UntypedGScipConstraintHandler;

namespace operations_research {
namespace {

// Returns the "do nothing" callback result. Used to handle the edge case where
// Enfo* callbacks need to return kFeasible instead of kDidNotRun.
GScipCallbackResult DidNotRunCallbackResult(
    const ConstraintHandlerCallbackType callback_type) {
  // TODO(user): Add kEnfoRelax when we support it.
  if (callback_type == ConstraintHandlerCallbackType::kEnfoLp ||
      callback_type == ConstraintHandlerCallbackType::kEnfoPs) {
    return GScipCallbackResult::kFeasible;
  }
  return GScipCallbackResult::kDidNotRun;
}

constexpr GScipCallbackResult kMinPriority = GScipCallbackResult::kDelayNode;

// Calls the callback function over a span of constraints, returning the highest
// priority callback result, along with a SCIP return code.
absl::StatusOr<GScipCallbackResult> ApplyCallback(
    absl::Span<SCIP_Cons*> constraints,
    std::function<GScipCallbackResult(void*)> callback_function,
    const ConstraintHandlerCallbackType callback_type) {
  if (constraints.empty()) {
    return DidNotRunCallbackResult(callback_type);
  }
  GScipCallbackResult callback_result = kMinPriority;
  for (SCIP_Cons* cons : constraints) {
    if (cons == nullptr) {
      return absl::InternalError("Constraint handler has null constraint");
    }
    SCIP_CONSDATA* consdata = SCIPconsGetData(cons);
    if (consdata == nullptr || consdata->data == nullptr) {
      return absl::InternalError("Constraint handler has null constraint data");
    }
    const GScipCallbackResult cons_result = callback_function(consdata->data);
    if (ConstraintHandlerResultPriority(cons_result, callback_type) >
        ConstraintHandlerResultPriority(callback_result, callback_type)) {
      callback_result = cons_result;
    }
  }
  return callback_result;
}

// Calls the callback function over all the constraints of a constraint handler,
// prioritizing the ones SCIP deems more useful. Returns the highest priority
// callback result, along with a SCIP return code.
absl::StatusOr<GScipCallbackResult> ApplyCallback(
    SCIP_Cons** constraints, const int num_useful_constraints,
    const int total_num_constraints,
    std::function<GScipCallbackResult(void*)> callback_function,
    const ConstraintHandlerCallbackType callback_type) {
  absl::Span<SCIP_Cons*> all_constraints =
      absl::MakeSpan(constraints, total_num_constraints);
  absl::Span<SCIP_Cons*> useful_constraints =
      all_constraints.subspan(0, num_useful_constraints);
  ASSIGN_OR_RETURN(
      const GScipCallbackResult result,
      ApplyCallback(useful_constraints, callback_function, callback_type));
  // The first num_useful_constraints are the ones that are more likely to be
  // violated. If no violation was found, we consider the remaining
  // constraints.
  if (result == GScipCallbackResult::kDidNotFind ||
      result == GScipCallbackResult::kDidNotRun ||
      result == GScipCallbackResult::kFeasible) {
    absl::Span<SCIP_Cons*> remaining_constraints =
        all_constraints.subspan(num_useful_constraints);
    ASSIGN_OR_RETURN(
        const GScipCallbackResult remaining_result,
        ApplyCallback(remaining_constraints, callback_function, callback_type));
    if (remaining_result != GScipCallbackResult::kDidNotFind &&
        remaining_result != GScipCallbackResult::kDidNotRun &&
        remaining_result != GScipCallbackResult::kFeasible) {
      return remaining_result;
    }
  }
  return result;
}

GScipCallbackStats GetCallbackStats(GScip* gscip) {
  SCIP* scip = gscip->scip();
  const SCIP_STAGE stage = SCIPgetStage(scip);
  GScipCallbackStats stats;
  switch (stage) {
    case SCIP_STAGE_PROBLEM:
    case SCIP_STAGE_TRANSFORMING:
    case SCIP_STAGE_TRANSFORMED:
    case SCIP_STAGE_INITPRESOLVE:
    case SCIP_STAGE_PRESOLVING:
    case SCIP_STAGE_EXITPRESOLVE:
    case SCIP_STAGE_PRESOLVED:
    case SCIP_STAGE_INITSOLVE:
    case SCIP_STAGE_SOLVING:
    case SCIP_STAGE_SOLVED:
    case SCIP_STAGE_EXITSOLVE:
    case SCIP_STAGE_FREETRANS:
      stats.num_processed_nodes = SCIPgetNNodes(scip);
      stats.num_processed_nodes_total = SCIPgetNTotalNodes(scip);
      break;
    default:
      break;
  }

  switch (stage) {
    case SCIP_STAGE_INITPRESOLVE:
    case SCIP_STAGE_PRESOLVING:
    case SCIP_STAGE_EXITPRESOLVE:
    case SCIP_STAGE_SOLVING: {
      SCIP_NODE* node = SCIPgetCurrentNode(scip);
      stats.current_node_id = node == nullptr ? -1 : node->number;
      break;
    }
    default:
      stats.current_node_id = stats.num_processed_nodes;
  }
  switch (stage) {
    case SCIP_STAGE_TRANSFORMED:
    case SCIP_STAGE_INITPRESOLVE:
    case SCIP_STAGE_PRESOLVING:
    case SCIP_STAGE_EXITPRESOLVE:
    case SCIP_STAGE_PRESOLVED:
    case SCIP_STAGE_INITSOLVE:
    case SCIP_STAGE_SOLVING:
    case SCIP_STAGE_SOLVED:
    case SCIP_STAGE_EXITSOLVE:
      stats.primal_bound = gscip->ScipInfUnclamp(SCIPgetPrimalbound(scip));
      stats.dual_bound = gscip->ScipInfUnclamp(SCIPgetDualbound(scip));
      // Note: SCIPgetNLimSolsFound() docs claim it can be called in more
      // stages, but that appears to be a typo in the docs.
      stats.num_solutions_found = static_cast<int>(SCIPgetNLimSolsFound(scip));

      break;
    default:
      break;
  }

  switch (stage) {
    case SCIP_STAGE_PRESOLVED:
    case SCIP_STAGE_SOLVING:
    case SCIP_STAGE_SOLVED:
      stats.primal_simplex_iterations = SCIPgetNPrimalLPIterations(scip);
      stats.dual_simplex_iterations = SCIPgetNDualLPIterations(scip);
      stats.num_nodes_left = SCIPgetNNodesLeft(scip);
      break;
    default:
      break;
  }
  // SCIP counts the focus node (the current node) as explored, but to be
  // consistent with gurobi, we want to count it as open instead. In particular,
  // for callbacks at the root, we want num_processed_nodes=0
  if (stats.num_processed_nodes > 0) {
    stats.num_processed_nodes--;
    stats.num_processed_nodes_total--;
    stats.num_nodes_left++;
  }
  switch (stage) {
    case SCIP_STAGE_SOLVING:
    case SCIP_STAGE_SOLVED:
    case SCIP_STAGE_EXITSOLVE:
      stats.num_cuts_in_lp = SCIPgetNPoolCuts(scip);
      break;
    default:
      break;
  }

  return stats;
}

GScipConstraintOptions CallbackLazyConstraintOptions(const bool local,
                                                     const bool dynamic) {
  GScipConstraintOptions result;
  result.initial = true;
  result.separate = true;
  result.enforce = true;
  result.check = true;
  result.propagate = true;
  result.local = local;
  result.modifiable = false;
  result.dynamic = dynamic;
  result.removable = true;
  result.sticking_at_node = false;
  result.keep_alive = false;
  return result;
}

}  // namespace

int ConstraintHandlerResultPriority(
    const GScipCallbackResult result,
    const ConstraintHandlerCallbackType callback_type) {
  // In type_cons.h, callback results are consistently ordered across all
  // constraint handler callback methods except that SCIP_SOLVELP (kSolveLp)
  // takes higher priority than SCIP_BRANCHED (kBranched) in CONSENFOLP, and the
  // reverse is true for CONSENFORELAX and CONSENFOLP.
  switch (result) {
    case GScipCallbackResult::kUnbounded:
      return 14;
    case GScipCallbackResult::kCutOff:
      return 13;
    case GScipCallbackResult::kSuccess:
      return 12;
    case GScipCallbackResult::kConstraintAdded:
      return 11;
    case GScipCallbackResult::kReducedDomain:
      return 10;
    case GScipCallbackResult::kSeparated:
      return 9;
    case GScipCallbackResult::kBranched:
      return callback_type == ConstraintHandlerCallbackType::kEnfoLp ? 7 : 8;
    case GScipCallbackResult::kSolveLp:
      return callback_type == ConstraintHandlerCallbackType::kEnfoLp ? 8 : 7;
    case GScipCallbackResult::kInfeasible:
      return 6;
    case GScipCallbackResult::kFeasible:
      return 5;
    case GScipCallbackResult::kNewRound:
      return 4;
    case GScipCallbackResult::kDidNotFind:
      return 3;
    case GScipCallbackResult::kDidNotRun:
      return 2;
    case GScipCallbackResult::kDelayed:
      return 1;
    case GScipCallbackResult::kDelayNode:
      return 0;
    default:
      // kConstraintChanged, kFoundSolution, and kSuspend are not used in
      // constraint handlers.
      return -1;
  }
}

GScipCallbackResult MergeConstraintHandlerResults(
    const GScipCallbackResult result1, const GScipCallbackResult result2,
    const ConstraintHandlerCallbackType callback_type) {
  const int priority1 = ConstraintHandlerResultPriority(result1, callback_type);
  const int priority2 = ConstraintHandlerResultPriority(result2, callback_type);
  if (priority2 > priority1) {
    return result2;
  }
  return result1;
}

double GScipConstraintHandlerContext::VariableValue(SCIP_VAR* variable) const {
  return SCIPgetSolVal(gscip_->scip(), current_solution_, variable);
}

absl::StatusOr<GScipCallbackResult> GScipConstraintHandlerContext::AddCut(
    const GScipLinearRange& range, const std::string& name,
    const GScipCutOptions& options) {
  SCIP* scip = gscip_->scip();
  SCIP_ROW* row = nullptr;
  RETURN_IF_SCIP_ERROR(SCIPcreateEmptyRowConshdlr(
      scip, &row, current_handler_, name.data(), range.lower_bound,
      range.upper_bound, options.local, options.modifiable, options.removable));
  RETURN_IF_SCIP_ERROR(SCIPcacheRowExtensions(scip, row));
  if (range.coefficients.size() != range.variables.size()) {
    return absl::InternalError(
        "GScipLinearRange variables and coefficients do not match in size");
  }
  for (int i = 0; i < range.variables.size(); ++i) {
    RETURN_IF_SCIP_ERROR(SCIPaddVarToRow(scip, row, range.variables.at(i),
                                         range.coefficients.at(i)));
  }
  RETURN_IF_SCIP_ERROR(SCIPflushRowExtensions(scip, row));
  SCIP_Bool infeasible;
  RETURN_IF_SCIP_ERROR(SCIPaddRow(scip, row, options.force_cut, &infeasible));
  RETURN_IF_SCIP_ERROR(SCIPreleaseRow(scip, &row));
  return infeasible ? GScipCallbackResult::kCutOff
                    : GScipCallbackResult::kSeparated;
}

absl::Status GScipConstraintHandlerContext::AddLazyLinearConstraint(
    const GScipLinearRange& range, const std::string& name,
    const GScipLazyConstraintOptions& options) {
  return gscip_
      ->AddLinearConstraint(
          range, name,
          CallbackLazyConstraintOptions(options.local, options.dynamic))
      .status();
}

double GScipConstraintHandlerContext::LocalVarLb(SCIP_VAR* var) const {
  return SCIPvarGetLbLocal(var);
}
double GScipConstraintHandlerContext::LocalVarUb(SCIP_VAR* var) const {
  return SCIPvarGetUbLocal(var);
}
double GScipConstraintHandlerContext::GlobalVarLb(SCIP_VAR* var) const {
  return SCIPvarGetLbGlobal(var);
}
double GScipConstraintHandlerContext::GlobalVarUb(SCIP_VAR* var) const {
  return SCIPvarGetUbGlobal(var);
}

absl::Status GScipConstraintHandlerContext::SetLocalVarLb(SCIP_VAR* var,
                                                          double value) {
  return SCIP_TO_STATUS(
      SCIPchgVarLbNode(gscip_->scip(), /*node=*/nullptr, var, value));
}
absl::Status GScipConstraintHandlerContext::SetLocalVarUb(SCIP_VAR* var,
                                                          double value) {
  return SCIP_TO_STATUS(
      SCIPchgVarUbNode(gscip_->scip(), /*node=*/nullptr, var, value));
}
absl::Status GScipConstraintHandlerContext::SetGlobalVarLb(SCIP_VAR* var,
                                                           double value) {
  return SCIP_TO_STATUS(SCIPchgVarLbGlobal(gscip_->scip(), var, value));
}
absl::Status GScipConstraintHandlerContext::SetGlobalVarUb(SCIP_VAR* var,
                                                           double value) {
  return SCIP_TO_STATUS(SCIPchgVarUbGlobal(gscip_->scip(), var, value));
}

}  // namespace operations_research

// ///////////////////////////////////////////////////////////////////////////
// SCIP callback implementation
// //////////////////////////////////////////////////////////////////////////

extern "C" {

// Destructor of the constraint handler to free user data (called when SCIP is
// exiting).
static SCIP_DECL_CONSFREE(ConstraintHandlerFreeC) {
  if (scip == nullptr) {
    SCIPerrorMessage("SCIP not found in SCIP_DECL_CONSFREE");
    return SCIP_ERROR;
  }
  SCIP_CONSHDLRDATA* scip_handler_data = SCIPconshdlrGetData(conshdlr);
  if (scip_handler_data == nullptr) {
    SCIPerrorMessage("SCIP handler data not found in SCIP_DECL_CONSFREE");
    return SCIP_ERROR;
  }
  delete scip_handler_data;
  SCIPconshdlrSetData(conshdlr, nullptr);
  return SCIP_OKAY;
}

static SCIP_DECL_CONSDELETE(ConstraintDataDeleteC) {
  if (consdata == nullptr || *consdata == nullptr) {
    SCIPerrorMessage("SCIP constraint data not found in SCIP_DECL_CONSDELETE");
    return SCIP_ERROR;
  }
  delete *consdata;
  cons->consdata = nullptr;
  return SCIP_OKAY;
}

static SCIP_DECL_CONSENFOLP(EnforceLpC) {
  SCIP_CONSHDLRDATA* scip_handler_data = SCIPconshdlrGetData(conshdlr);
  operations_research::GScip* gscip = scip_handler_data->gscip;
  operations_research::GScipCallbackStats stats =
      operations_research::GetCallbackStats(gscip);
  operations_research::GScipConstraintHandlerContext context(gscip, &stats,
                                                             conshdlr, nullptr);
  const bool solution_known_infeasible = static_cast<bool>(solinfeasible);
  GScipHandler* gscip_handler = scip_handler_data->gscip_handler.get();
  auto DoEnforceLp = [=](void* constraint_data) {
    return gscip_handler->CallEnforceLp(context, constraint_data,
                                        solution_known_infeasible);
  };
  const absl::StatusOr<operations_research::GScipCallbackResult> gresult =
      operations_research::ApplyCallback(
          conss, nusefulconss, nconss, DoEnforceLp,
          operations_research::ConstraintHandlerCallbackType::kEnfoLp);
  if (!gresult.ok()) {
    SCIPerrorMessage(gresult.status().ToString().c_str());
    return SCIP_ERROR;
  }
  *result = operations_research::ConvertGScipCallbackResult(*gresult);
  return SCIP_OKAY;
}

static SCIP_DECL_CONSENFOPS(EnforcePseudoSolutionC) {
  SCIP_CONSHDLRDATA* scip_handler_data = SCIPconshdlrGetData(conshdlr);
  operations_research::GScip* gscip = scip_handler_data->gscip;
  operations_research::GScipCallbackStats stats =
      operations_research::GetCallbackStats(gscip);
  operations_research::GScipConstraintHandlerContext context(gscip, &stats,
                                                             conshdlr, nullptr);
  const bool solution_known_infeasible = static_cast<bool>(solinfeasible);
  const bool solution_infeasible_by_objective =
      static_cast<bool>(objinfeasible);
  GScipHandler* gscip_handler = scip_handler_data->gscip_handler.get();
  auto DoEnforcePseudoSolution = [=](void* constraint_data) {
    return gscip_handler->CallEnforcePseudoSolution(
        context, constraint_data, solution_known_infeasible,
        solution_infeasible_by_objective);
  };
  const absl::StatusOr<operations_research::GScipCallbackResult> gresult =
      operations_research::ApplyCallback(
          conss, nusefulconss, nconss, DoEnforcePseudoSolution,
          operations_research::ConstraintHandlerCallbackType::kEnfoPs);
  if (!gresult.ok()) {
    SCIPerrorMessage(gresult.status().ToString().c_str());
    return SCIP_ERROR;
  }
  *result = operations_research::ConvertGScipCallbackResult(*gresult);
  return SCIP_OKAY;
}

static SCIP_DECL_CONSCHECK(CheckFeasibilityC) {
  SCIP_CONSHDLRDATA* scip_handler_data = SCIPconshdlrGetData(conshdlr);
  operations_research::GScip* gscip = scip_handler_data->gscip;
  operations_research::GScipCallbackStats stats =
      operations_research::GetCallbackStats(gscip);
  operations_research::GScipConstraintHandlerContext context(gscip, &stats,
                                                             conshdlr, sol);
  const bool check_integrality = static_cast<bool>(checkintegrality);
  const bool check_lp_rows = static_cast<bool>(checklprows);
  const bool print_reason = static_cast<bool>(printreason);
  const bool complete = static_cast<bool>(completely);
  GScipHandler* gscip_handler = scip_handler_data->gscip_handler.get();
  auto DoCheckIsFeasible = [=](void* constraint_data) {
    return gscip_handler->CallCheckIsFeasible(context, constraint_data,
                                              check_integrality, check_lp_rows,
                                              print_reason, complete);
  };
  const absl::StatusOr<operations_research::GScipCallbackResult> gresult =
      operations_research::ApplyCallback(
          conss, nconss, nconss, DoCheckIsFeasible,
          operations_research::ConstraintHandlerCallbackType::kEnfoPs);
  if (!gresult.ok()) {
    SCIPerrorMessage(gresult.status().ToString().c_str());
    return SCIP_ERROR;
  }
  *result = operations_research::ConvertGScipCallbackResult(*gresult);
  return SCIP_OKAY;
}

static SCIP_DECL_CONSSEPALP(SeparateLpC) {
  SCIP_CONSHDLRDATA* scip_handler_data = SCIPconshdlrGetData(conshdlr);
  operations_research::GScip* gscip = scip_handler_data->gscip;
  operations_research::GScipCallbackStats stats =
      operations_research::GetCallbackStats(gscip);
  operations_research::GScipConstraintHandlerContext context(gscip, &stats,
                                                             conshdlr, nullptr);
  GScipHandler* gscip_handler = scip_handler_data->gscip_handler.get();
  auto DoSeparateLp = [=](void* constraint_data) {
    return gscip_handler->CallSeparateLp(context, constraint_data);
  };
  const absl::StatusOr<operations_research::GScipCallbackResult> gresult =
      operations_research::ApplyCallback(
          conss, nusefulconss, nconss, DoSeparateLp,
          operations_research::ConstraintHandlerCallbackType::kSepaLp);
  if (!gresult.ok()) {
    SCIPerrorMessage(gresult.status().ToString().c_str());
    return SCIP_ERROR;
  }
  *result = operations_research::ConvertGScipCallbackResult(*gresult);
  return SCIP_OKAY;
}

static SCIP_DECL_CONSSEPASOL(SeparatePrimalSolutionC) {
  SCIP_CONSHDLRDATA* scip_handler_data = SCIPconshdlrGetData(conshdlr);
  operations_research::GScip* gscip = scip_handler_data->gscip;
  operations_research::GScipCallbackStats stats =
      operations_research::GetCallbackStats(gscip);
  operations_research::GScipConstraintHandlerContext context(gscip, &stats,
                                                             conshdlr, sol);
  GScipHandler* gscip_handler = scip_handler_data->gscip_handler.get();
  auto DoSeparateSolution = [=](void* constraint_data) {
    return gscip_handler->CallSeparateSolution(context, constraint_data);
  };
  const absl::StatusOr<operations_research::GScipCallbackResult> gresult =
      operations_research::ApplyCallback(
          conss, nusefulconss, nconss, DoSeparateSolution,
          operations_research::ConstraintHandlerCallbackType::kSepaSol);
  if (!gresult.ok()) {
    SCIPerrorMessage(gresult.status().ToString().c_str());
    return SCIP_ERROR;
  }
  *result = operations_research::ConvertGScipCallbackResult(*gresult);
  return SCIP_OKAY;
}

static SCIP_DECL_CONSLOCK(VariableRoundingLockC) {
  SCIP_CONSHDLRDATA* scip_handler_data = SCIPconshdlrGetData(conshdlr);
  operations_research::GScip* gscip = scip_handler_data->gscip;
  GScipHandler* gscip_handler = scip_handler_data->gscip_handler.get();
  SCIP_CONSDATA* consdata = SCIPconsGetData(cons);
  if (consdata == nullptr || consdata->data == nullptr) {
    SCIPerrorMessage(
        "consdata or consdata->data was null in SCIP_DECL_CONSLOCK");
    return SCIP_ERROR;
  }
  const bool lock_type_is_model = locktype == SCIP_LOCKTYPE_MODEL;
  for (const auto [locked_var, lock_direction] :
       gscip_handler->RoundingLock(gscip, consdata->data, lock_type_is_model)) {
    int lock_down;
    int lock_up;
    switch (lock_direction) {
      case operations_research::RoundingLockDirection::kUp:
        lock_down = nlocksneg;
        lock_up = nlockspos;
        break;
      case operations_research::RoundingLockDirection::kDown:
        lock_down = nlockspos;
        lock_up = nlocksneg;
        break;
      case operations_research::RoundingLockDirection::kBoth:
        lock_down = nlocksneg + nlockspos;
        lock_up = nlocksneg + nlockspos;
        break;
    }
    SCIP_CALL(
        SCIPaddVarLocksType(scip, locked_var, locktype, lock_down, lock_up));
  }
  return SCIP_OKAY;
}
}

namespace operations_research {

namespace internal {

absl::Status RegisterConstraintHandler(
    GScip* gscip,
    std::unique_ptr<UntypedGScipConstraintHandler> constraint_handler) {
  SCIP_CONSHDLR* c_scip_handler;
  SCIP_CONSHDLRDATA* scip_handler_data = new SCIP_CONSHDLRDATA;
  scip_handler_data->gscip_handler = std::move(constraint_handler);
  scip_handler_data->gscip = gscip;
  SCIP* scip = gscip->scip();
  const GScipConstraintHandlerProperties& properties =
      scip_handler_data->gscip_handler->properties();

  RETURN_IF_SCIP_ERROR(SCIPincludeConshdlrBasic(
      scip, &c_scip_handler, properties.name.c_str(),
      properties.description.c_str(), properties.enforcement_priority,
      properties.feasibility_check_priority, properties.eager_frequency,
      properties.needs_constraints, EnforceLpC, EnforcePseudoSolutionC,
      CheckFeasibilityC, VariableRoundingLockC, scip_handler_data));
  if (c_scip_handler == nullptr) {
    return absl::InternalError("SCIP failed to add constraint handler");
  }
  RETURN_IF_SCIP_ERROR(SCIPsetConshdlrSepa(
      scip, c_scip_handler, SeparateLpC, SeparatePrimalSolutionC,
      properties.separation_frequency, properties.separation_priority,
      properties.delay_separation));
  RETURN_IF_SCIP_ERROR(
      SCIPsetConshdlrFree(scip, c_scip_handler, ConstraintHandlerFreeC));
  RETURN_IF_SCIP_ERROR(
      SCIPsetConshdlrDelete(scip, c_scip_handler, ConstraintDataDeleteC));

  return absl::OkStatus();
}

absl::StatusOr<SCIP_CONS*> AddCallbackConstraint(
    GScip* gscip, const std::string& handler_name,
    const std::string& constraint_name, void* constraint_data,
    const GScipConstraintOptions& options) {
  if (constraint_data == nullptr) {
    return absl::InvalidArgumentError(
        "Constraint data missing when adding a constraint handler callback");
  }
  SCIP* scip = gscip->scip();
  SCIP_CONSHDLR* conshdlr = SCIPfindConshdlr(scip, handler_name.data());
  if (conshdlr == nullptr) {
    return util::InternalErrorBuilder()
           << "Constraint handler " << handler_name
           << " not registered with SCIP. Check if you "
              "registered the constraint handler before adding constraints.";
  }
  SCIP_CONSDATA* consdata = new SCIP_CONSDATA;
  consdata->data = constraint_data;

  return gscip->AddConstraintForHandler(conshdlr, consdata, constraint_name,
                                        options);
}

}  // namespace internal

}  // namespace operations_research
