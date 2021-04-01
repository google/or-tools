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

#if defined(USE_SCIP)

#include "ortools/linear_solver/scip_callback.h"

#include <cstdint>

#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/linear_solver/scip_helper_macros.h"
#include "scip/cons_linear.h"
#include "scip/def.h"
#include "scip/pub_cons.h"
#include "scip/scip.h"
#include "scip/scip_cons.h"
#include "scip/scip_cut.h"
#include "scip/scip_general.h"
#include "scip/scip_lp.h"
#include "scip/scip_param.h"
#include "scip/scip_prob.h"
#include "scip/scip_sol.h"
#include "scip/scip_solvingstats.h"
#include "scip/scip_tree.h"
#include "scip/scipdefplugins.h"
#include "scip/struct_cons.h"
#include "scip/struct_tree.h"
#include "scip/struct_var.h"
#include "scip/type_cons.h"
#include "scip/type_lp.h"
#include "scip/type_result.h"
#include "scip/type_retcode.h"
#include "scip/type_scip.h"
#include "scip/type_sol.h"
#include "scip/type_tree.h"
#include "scip/type_var.h"

struct SCIP_ConshdlrData {
  std::unique_ptr<operations_research::internal::ScipCallbackRunner> runner;
};

struct SCIP_ConsData {
  void* data;
};

namespace operations_research {

namespace {
int ScipNumVars(SCIP* scip) { return SCIPgetNOrigVars(scip); }

SCIP_VAR* ScipGetVar(SCIP* scip, int var_index) {
  DCHECK_GE(var_index, 0);
  DCHECK_LT(var_index, ScipNumVars(scip));
  return SCIPgetOrigVars(scip)[var_index];
}

}  // namespace

ScipConstraintHandlerContext::ScipConstraintHandlerContext(
    SCIP* scip, SCIP_SOL* solution, bool is_pseudo_solution)
    : scip_(scip),
      solution_(solution),
      is_pseudo_solution_(is_pseudo_solution) {}

double ScipConstraintHandlerContext::VariableValue(
    const MPVariable* variable) const {
  return SCIPgetSolVal(scip_, solution_, ScipGetVar(scip_, variable->index()));
}

int64_t ScipConstraintHandlerContext::NumNodesProcessed() const {
  return SCIPgetNNodes(scip_);
}

int64_t ScipConstraintHandlerContext::CurrentNodeId() const {
  return SCIPgetCurrentNode(scip_)->number;
}

enum class ScipSeparationResult {
  kLazyConstraintAdded,
  kCuttingPlaneAdded,
  kDidNotFind
};

bool LinearConstraintIsViolated(const ScipConstraintHandlerContext& context,
                                const LinearRange& constraint) {
  double a_times_x = 0;
  for (const auto& coef_pair : constraint.linear_expr().terms()) {
    a_times_x += coef_pair.second * context.VariableValue(coef_pair.first);
  }
  double violation = std::max(a_times_x - constraint.upper_bound(),
                              constraint.lower_bound() - a_times_x);
  return violation > 0;
}

// If any violated lazy constraint is found:
//   returns kLazyConstraintAdded,
// else if any violated cutting plane is found:
//   returns kCuttingPlaneAdded,
// else:
//   returns kDidNotFind
ScipSeparationResult RunSeparation(internal::ScipCallbackRunner* runner,
                                   const ScipConstraintHandlerContext& context,
                                   absl::Span<SCIP_CONS*> constraints,
                                   bool is_integral) {
  ScipSeparationResult result = ScipSeparationResult::kDidNotFind;
  SCIP* scip = context.scip();
  for (SCIP_CONS* constraint : constraints) {
    SCIP_CONSDATA* consdata = SCIPconsGetData(constraint);
    CHECK(consdata != nullptr);
    std::vector<CallbackRangeConstraint> user_suggested_constraints;
    if (is_integral) {
      user_suggested_constraints =
          runner->SeparateIntegerSolution(context, consdata->data);
    } else {
      user_suggested_constraints =
          runner->SeparateFractionalSolution(context, consdata->data);
    }
    int num_constraints_added = 0;
    for (const CallbackRangeConstraint& user_suggested_constraint :
         user_suggested_constraints) {
      if (!LinearConstraintIsViolated(context,
                                      user_suggested_constraint.range)) {
        continue;
      }
      num_constraints_added++;
      // Two code paths, one for cuts, one for lazy constraints.  Cuts first:
      if (user_suggested_constraint.is_cut) {
        SCIP_ROW* row = nullptr;
        constexpr bool kModifiable = false;
        constexpr bool kRemovable = true;
        CHECK_OK(SCIP_TO_STATUS(SCIPcreateEmptyRowCons(
            scip, &row, constraint, user_suggested_constraint.name.c_str(),
            user_suggested_constraint.range.lower_bound(),
            user_suggested_constraint.range.upper_bound(),
            user_suggested_constraint.local, kModifiable, kRemovable)));
        CHECK_OK(SCIP_TO_STATUS(SCIPcacheRowExtensions(scip, row)));
        for (const auto& coef_pair :
             user_suggested_constraint.range.linear_expr().terms()) {
          // NOTE(user): the coefficients don't come out sorted.  I don't
          // think this matters.
          SCIP_VAR* var = ScipGetVar(scip, coef_pair.first->index());
          const double coef = coef_pair.second;
          CHECK_OK(SCIP_TO_STATUS(SCIPaddVarToRow(scip, row, var, coef)));
        }
        CHECK_OK(SCIP_TO_STATUS(SCIPflushRowExtensions(scip, row)));
        SCIP_Bool infeasible;
        constexpr bool kForceCut = false;
        CHECK_OK(SCIP_TO_STATUS(SCIPaddRow(scip, row, kForceCut, &infeasible)));
        CHECK_OK(SCIP_TO_STATUS(SCIPreleaseRow(scip, &row)));
        // TODO(user): when infeasible is true, it better to have the scip
        // return status be cutoff instead of cutting plane added (e.g. see
        // cs/scip/cons_knapsack.c). However, as we use
        // SCIPaddRow(), it isn't clear this will even happen.
        if (result != ScipSeparationResult::kLazyConstraintAdded) {
          // NOTE(user): if we have already found a violated lazy constraint,
          // we want to return kLazyConstraintAdded, not kCuttingPlaneAdded,
          // see function contract.
          result = ScipSeparationResult::kCuttingPlaneAdded;
        }
      } else {
        // Lazy constraint path:
        std::vector<SCIP_VAR*> vars;
        std::vector<double> coefs;
        for (const auto& coef_pair :
             user_suggested_constraint.range.linear_expr().terms()) {
          // NOTE(user): the coefficients don't come out sorted.  I don't
          // think this matters.
          vars.push_back(ScipGetVar(scip, coef_pair.first->index()));
          coefs.push_back(coef_pair.second);
        }

        const int num_vars = vars.size();
        SCIP_CONS* scip_cons;
        // TODO(user): Maybe it is better to expose more of these options,
        // potentially through user_suggested_constraint.
        CHECK_OK(SCIP_TO_STATUS(SCIPcreateConsLinear(
            scip, &scip_cons, user_suggested_constraint.name.c_str(), num_vars,
            vars.data(), coefs.data(),
            user_suggested_constraint.range.lower_bound(),
            user_suggested_constraint.range.upper_bound(), /*initial=*/true,
            /*separate=*/true, /*enforce=*/true, /*check=*/true,
            /*propagate=*/true, /*local=*/user_suggested_constraint.local,
            /*modifiable=*/false, /*dynamic=*/false, /*removable=*/true,
            /*stickingatnode=*/false)));
        if (user_suggested_constraint.local) {
          CHECK_OK(SCIP_TO_STATUS(SCIPaddConsLocal(scip, scip_cons, nullptr)));
        } else {
          CHECK_OK(SCIP_TO_STATUS(SCIPaddCons(scip, scip_cons)));
        }
        CHECK_OK(SCIP_TO_STATUS(SCIPreleaseCons(scip, &scip_cons)));
        result = ScipSeparationResult::kLazyConstraintAdded;
      }
    }
  }
  return result;
}

struct CallbackSetup {
  SCIP_CONSHDLRDATA* scip_handler_data;
  internal::ScipCallbackRunner* callback_runner;
  ScipConstraintHandlerContext context;
  absl::Span<SCIP_CONS*> useful_constraints;
  absl::Span<SCIP_CONS*> unlikely_useful_constraints;

  CallbackSetup(SCIP* scip, SCIP_CONSHDLR* scip_handler, SCIP_CONS** conss,
                int nconss, int nusefulconss, SCIP_SOL* sol,
                bool is_pseudo_solution)
      : scip_handler_data(SCIPconshdlrGetData(scip_handler)),
        callback_runner(scip_handler_data->runner.get()),
        context(scip, sol, is_pseudo_solution),
        useful_constraints(absl::MakeSpan(conss, nusefulconss)),
        unlikely_useful_constraints(
            absl::MakeSpan(conss, nconss).subspan(nusefulconss)) {
    CHECK(scip_handler_data != nullptr);
    CHECK(callback_runner != nullptr);
  }
};

}  // namespace operations_research

extern "C" {
/** destructor of constraint handler to free user data (called when SCIP is
 * exiting) */
static SCIP_DECL_CONSFREE(ConstraintHandlerFreeC) {
  VLOG(3) << "FreeC";
  CHECK(scip != nullptr);
  SCIP_CONSHDLRDATA* scip_handler_data = SCIPconshdlrGetData(conshdlr);
  CHECK(scip_handler_data != nullptr);
  delete scip_handler_data;
  SCIPconshdlrSetData(conshdlr, nullptr);
  return SCIP_OKAY;
}

static SCIP_DECL_CONSDELETE(ConstraintHandlerDeleteC) {
  VLOG(3) << "DeleteC";
  CHECK(consdata != nullptr);
  CHECK(*consdata != nullptr);
  delete *consdata;
  cons->consdata = nullptr;
  return SCIP_OKAY;
}

static SCIP_DECL_CONSENFOLP(EnforceLpC) {
  VLOG(3) << "EnforceC";
  operations_research::CallbackSetup setup(scip, conshdlr, conss, nconss,
                                           nusefulconss, nullptr, false);
  operations_research::ScipSeparationResult separation_result =
      operations_research::RunSeparation(setup.callback_runner, setup.context,
                                         setup.useful_constraints,
                                         /*is_integral=*/true);
  if (separation_result ==
      operations_research::ScipSeparationResult::kDidNotFind) {
    separation_result = operations_research::RunSeparation(
        setup.callback_runner, setup.context, setup.unlikely_useful_constraints,
        /*is_integral=*/true);
  }
  switch (separation_result) {
    case operations_research::ScipSeparationResult::kLazyConstraintAdded:
      *result = SCIP_CONSADDED;
      break;
    case operations_research::ScipSeparationResult::kCuttingPlaneAdded:
      *result = SCIP_SEPARATED;
      break;
    case operations_research::ScipSeparationResult::kDidNotFind:
      *result = SCIP_FEASIBLE;
      break;
  }
  return SCIP_OKAY;
}

static SCIP_DECL_CONSSEPALP(SeparateLpC) {
  VLOG(3) << "SeparateLpC";
  operations_research::CallbackSetup setup(scip, conshdlr, conss, nconss,
                                           nusefulconss, nullptr, false);
  operations_research::ScipSeparationResult separation_result =
      operations_research::RunSeparation(setup.callback_runner, setup.context,
                                         setup.useful_constraints,
                                         /*is_integral=*/false);
  if (separation_result ==
      operations_research::ScipSeparationResult::kDidNotFind) {
    separation_result = operations_research::RunSeparation(
        setup.callback_runner, setup.context, setup.unlikely_useful_constraints,
        /*is_integral=*/false);
  }
  switch (separation_result) {
    case operations_research::ScipSeparationResult::kLazyConstraintAdded:
      *result = SCIP_CONSADDED;
      break;
    case operations_research::ScipSeparationResult::kCuttingPlaneAdded:
      *result = SCIP_SEPARATED;
      break;
    case operations_research::ScipSeparationResult::kDidNotFind:
      *result = SCIP_DIDNOTFIND;
      break;
  }
  return SCIP_OKAY;
}

static SCIP_DECL_CONSSEPASOL(SeparatePrimalSolutionC) {
  VLOG(3) << "SeparatePrimalC";
  operations_research::CallbackSetup setup(scip, conshdlr, conss, nconss,
                                           nusefulconss, sol, false);
  operations_research::ScipSeparationResult separation_result =
      operations_research::RunSeparation(setup.callback_runner, setup.context,
                                         setup.useful_constraints,
                                         /*is_integral=*/true);
  if (separation_result ==
      operations_research::ScipSeparationResult::kDidNotFind) {
    separation_result = operations_research::RunSeparation(
        setup.callback_runner, setup.context, setup.unlikely_useful_constraints,
        /*is_integral=*/true);
  }
  switch (separation_result) {
    case operations_research::ScipSeparationResult::kLazyConstraintAdded:
      *result = SCIP_CONSADDED;
      break;
    case operations_research::ScipSeparationResult::kCuttingPlaneAdded:
      LOG(ERROR) << "Cutting planes cannot be added on integer solutions, "
                    "treating as a constraint.";
      *result = SCIP_CONSADDED;
      break;
    case operations_research::ScipSeparationResult::kDidNotFind:
      *result = SCIP_DIDNOTFIND;
      break;
  }
  return SCIP_OKAY;
}

static SCIP_DECL_CONSCHECK(CheckFeasibilityC) {
  VLOG(3) << "CheckFeasibilityC";
  operations_research::CallbackSetup setup(scip, conshdlr, conss, nconss,
                                           nconss, sol, false);
  // All constraints are "useful" for this callback.
  for (SCIP_CONS* constraint : setup.useful_constraints) {
    SCIP_CONSDATA* consdata = SCIPconsGetData(constraint);
    CHECK(consdata != nullptr);
    if (!setup.callback_runner->IntegerSolutionFeasible(setup.context,
                                                        consdata->data)) {
      *result = SCIP_INFEASIBLE;
      return SCIP_OKAY;
    }
  }
  *result = SCIP_FEASIBLE;
  return SCIP_OKAY;
}
static SCIP_DECL_CONSENFOPS(EnforcePseudoSolutionC) {
  VLOG(3) << "EnforcePseudoSolutionC";
  // TODO(user): are we sure the pseudo solution is LP feasible? It seems like
  // it doesn't need to be.  The code in RunSeparation might assume this?
  operations_research::CallbackSetup setup(scip, conshdlr, conss, nconss,
                                           nusefulconss, nullptr, true);
  operations_research::ScipSeparationResult separation_result =
      operations_research::RunSeparation(setup.callback_runner, setup.context,
                                         setup.useful_constraints,
                                         /*is_integral=*/false);
  if (separation_result ==
      operations_research::ScipSeparationResult::kDidNotFind) {
    separation_result = operations_research::RunSeparation(
        setup.callback_runner, setup.context, setup.unlikely_useful_constraints,
        /*is_integral=*/false);
  }
  switch (separation_result) {
    case operations_research::ScipSeparationResult::kLazyConstraintAdded:
      *result = SCIP_CONSADDED;
      break;
    case operations_research::ScipSeparationResult::kCuttingPlaneAdded:
      LOG(ERROR) << "Cutting planes cannot be added on pseudo solutions, "
                    "treating as a constraint.";
      *result = SCIP_CONSADDED;
      break;
    case operations_research::ScipSeparationResult::kDidNotFind:
      *result = SCIP_FEASIBLE;
      break;
  }
  return SCIP_OKAY;
}
static SCIP_DECL_CONSLOCK(VariableRoundingLockC) {
  // In this callback, we need to say, for a constraint class and an instance of
  // the constraint, for which variables could an {increase,decrease,either}
  // affect feasibility.  As a conservative overestimate, we say that any
  // change in any variable could cause an infeasibility for any instance of
  // any callback constraint.
  // TODO(user): this could be a little better, but we would need to add
  // another method to override on ScipConstraintHandler<ConstraintData>.

  const int num_vars = operations_research::ScipNumVars(scip);
  for (int i = 0; i < num_vars; ++i) {
    SCIP_VAR* var = operations_research::ScipGetVar(scip, i);
    SCIP_CALL(SCIPaddVarLocksType(scip, var, locktype, nlockspos + nlocksneg,
                                  nlockspos + nlocksneg));
  }
  return SCIP_OKAY;
}
}

namespace operations_research {
namespace internal {

void AddConstraintHandlerImpl(
    const ScipConstraintHandlerDescription& description,
    std::unique_ptr<ScipCallbackRunner> runner, SCIP* scip) {
  SCIP_CONSHDLR* c_scip_handler;
  SCIP_CONSHDLRDATA* scip_handler_data = new SCIP_CONSHDLRDATA;
  scip_handler_data->runner = std::move(runner);

  CHECK_OK(SCIP_TO_STATUS(SCIPincludeConshdlrBasic(
      scip, &c_scip_handler, description.name.c_str(),
      description.description.c_str(), description.enforcement_priority,
      description.feasibility_check_priority, description.eager_frequency,
      description.needs_constraints, EnforceLpC, EnforcePseudoSolutionC,
      CheckFeasibilityC, VariableRoundingLockC, scip_handler_data)));
  CHECK(c_scip_handler != nullptr);
  CHECK_OK(SCIP_TO_STATUS(SCIPsetConshdlrSepa(
      scip, c_scip_handler, SeparateLpC, SeparatePrimalSolutionC,
      description.separation_frequency, description.separation_priority,
      /*delaysepa=*/false)));
  CHECK_OK(SCIP_TO_STATUS(
      SCIPsetConshdlrFree(scip, c_scip_handler, ConstraintHandlerFreeC)));
  CHECK_OK(SCIP_TO_STATUS(
      SCIPsetConshdlrDelete(scip, c_scip_handler, ConstraintHandlerDeleteC)));
}

void AddCallbackConstraintImpl(SCIP* scip, const std::string& handler_name,
                               const std::string& constraint_name,
                               void* constraint_data,
                               const ScipCallbackConstraintOptions& options) {
  SCIP_CONSHDLR* conshdlr = SCIPfindConshdlr(scip, handler_name.c_str());
  CHECK(conshdlr != nullptr)
      << "Constraint handler " << handler_name << " not registered with scip.";
  SCIP_ConsData* consdata = new SCIP_ConsData;
  consdata->data = constraint_data;
  SCIP_CONS* constraint = nullptr;
  CHECK_OK(SCIP_TO_STATUS(SCIPcreateCons(
      scip, &constraint, constraint_name.c_str(), conshdlr, consdata,
      options.initial, options.separate, options.enforce, options.check,
      options.propagate, options.local, options.modifiable, options.dynamic,
      options.removable, options.stickingatnodes)));
  CHECK(constraint != nullptr);
  CHECK_OK(SCIP_TO_STATUS(SCIPaddCons(scip, constraint)));
  CHECK_OK(SCIP_TO_STATUS(SCIPreleaseCons(scip, &constraint)));
}

}  // namespace internal
}  // namespace operations_research
#endif  //  #if defined(USE_SCIP)
