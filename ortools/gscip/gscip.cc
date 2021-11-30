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

#include "ortools/gscip/gscip.h"

#include <stdio.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/base/status_macros.h"
#include "ortools/gscip/gscip.pb.h"
#include "ortools/gscip/gscip_parameters.h"
#include "ortools/gscip/legacy_scip_params.h"
#include "ortools/linear_solver/scip_helper_macros.h"
#include "ortools/port/proto_utils.h"
#include "scip/cons_linear.h"
#include "scip/cons_quadratic.h"
#include "scip/scip.h"
#include "scip/scip_general.h"
#include "scip/scip_param.h"
#include "scip/scip_prob.h"
#include "scip/scip_solvingstats.h"
#include "scip/scipdefplugins.h"
#include "scip/type_cons.h"
#include "scip/type_scip.h"
#include "scip/type_var.h"

namespace operations_research {

#define RETURN_ERROR_UNLESS(x)                                           \
  if (!(x))                                                              \
  return util::StatusBuilder(absl::InvalidArgumentError(absl::StrFormat( \
      "Condition violated at %s:%d: %s", __FILE__, __LINE__, #x)))

namespace {

constexpr absl::string_view kLinearConstraintHandlerName = "linear";

SCIP_VARTYPE ConvertVarType(const GScipVarType var_type) {
  switch (var_type) {
    case GScipVarType::kContinuous:
      return SCIP_VARTYPE_CONTINUOUS;
    case GScipVarType::kImpliedInteger:
      return SCIP_VARTYPE_IMPLINT;
    case GScipVarType::kInteger:
      return SCIP_VARTYPE_INTEGER;
  }
}

GScipVarType ConvertVarType(const SCIP_VARTYPE var_type) {
  switch (var_type) {
    case SCIP_VARTYPE_CONTINUOUS:
      return GScipVarType::kContinuous;
    case SCIP_VARTYPE_IMPLINT:
      return GScipVarType::kImpliedInteger;
    case SCIP_VARTYPE_INTEGER:
    case SCIP_VARTYPE_BINARY:
      return GScipVarType::kInteger;
  }
}

GScipOutput::Status ConvertStatus(const SCIP_STATUS scip_status) {
  switch (scip_status) {
    case SCIP_STATUS_UNKNOWN:
      return GScipOutput::UNKNOWN;
    case SCIP_STATUS_USERINTERRUPT:
      return GScipOutput::USER_INTERRUPT;
    case SCIP_STATUS_BESTSOLLIMIT:
      return GScipOutput::BEST_SOL_LIMIT;
    case SCIP_STATUS_MEMLIMIT:
      return GScipOutput::MEM_LIMIT;
    case SCIP_STATUS_NODELIMIT:
      return GScipOutput::NODE_LIMIT;
    case SCIP_STATUS_RESTARTLIMIT:
      return GScipOutput::RESTART_LIMIT;
    case SCIP_STATUS_SOLLIMIT:
      return GScipOutput::SOL_LIMIT;
    case SCIP_STATUS_STALLNODELIMIT:
      return GScipOutput::STALL_NODE_LIMIT;
    case SCIP_STATUS_TIMELIMIT:
      return GScipOutput::TIME_LIMIT;
    case SCIP_STATUS_TOTALNODELIMIT:
      return GScipOutput::TOTAL_NODE_LIMIT;
    case SCIP_STATUS_OPTIMAL:
      return GScipOutput::OPTIMAL;
    case SCIP_STATUS_GAPLIMIT:
      return GScipOutput::GAP_LIMIT;
    case SCIP_STATUS_INFEASIBLE:
      return GScipOutput::INFEASIBLE;
    case SCIP_STATUS_UNBOUNDED:
      return GScipOutput::UNBOUNDED;
    case SCIP_STATUS_INFORUNBD:
      return GScipOutput::INF_OR_UNBD;
    case SCIP_STATUS_TERMINATE:
      return GScipOutput::TERMINATE;
    default:
      LOG(FATAL) << "Unrecognized scip status: " << scip_status;
  }
}

SCIP_PARAMEMPHASIS ConvertEmphasis(
    const GScipParameters::Emphasis gscip_emphasis) {
  switch (gscip_emphasis) {
    case GScipParameters::DEFAULT_EMPHASIS:
      return SCIP_PARAMEMPHASIS_DEFAULT;
    case GScipParameters::CP_SOLVER:
      return SCIP_PARAMEMPHASIS_CPSOLVER;
    case GScipParameters::EASY_CIP:
      return SCIP_PARAMEMPHASIS_EASYCIP;
    case GScipParameters::FEASIBILITY:
      return SCIP_PARAMEMPHASIS_FEASIBILITY;
    case GScipParameters::HARD_LP:
      return SCIP_PARAMEMPHASIS_HARDLP;
    case GScipParameters::OPTIMALITY:
      return SCIP_PARAMEMPHASIS_OPTIMALITY;
    case GScipParameters::COUNTER:
      return SCIP_PARAMEMPHASIS_COUNTER;
    case GScipParameters::PHASE_FEAS:
      return SCIP_PARAMEMPHASIS_PHASEFEAS;
    case GScipParameters::PHASE_IMPROVE:
      return SCIP_PARAMEMPHASIS_PHASEIMPROVE;
    case GScipParameters::PHASE_PROOF:
      return SCIP_PARAMEMPHASIS_PHASEPROOF;
    default:
      LOG(FATAL) << "Unrecognized gscip_emphasis: "
                 << ProtoEnumToString(gscip_emphasis);
  }
}

SCIP_PARAMSETTING ConvertMetaParamValue(
    const GScipParameters::MetaParamValue gscip_meta_param_value) {
  switch (gscip_meta_param_value) {
    case GScipParameters::DEFAULT_META_PARAM_VALUE:
      return SCIP_PARAMSETTING_DEFAULT;
    case GScipParameters::AGGRESSIVE:
      return SCIP_PARAMSETTING_AGGRESSIVE;
    case GScipParameters::FAST:
      return SCIP_PARAMSETTING_FAST;
    case GScipParameters::OFF:
      return SCIP_PARAMSETTING_OFF;
    default:
      LOG(FATAL) << "Unrecognized gscip_meta_param_value: "
                 << ProtoEnumToString(gscip_meta_param_value);
  }
}

}  // namespace

const GScipVariableOptions& DefaultGScipVariableOptions() {
  static GScipVariableOptions var_options;
  return var_options;
}

const GScipConstraintOptions& DefaultGScipConstraintOptions() {
  static GScipConstraintOptions constraint_options;
  return constraint_options;
}

absl::Status GScip::SetParams(const GScipParameters& params,
                              const std::string& legacy_params) {
  if (params.has_silence_output()) {
    SCIPsetMessagehdlrQuiet(scip_, params.silence_output());
  }
  if (!params.search_logs_filename().empty()) {
    SCIPsetMessagehdlrLogfile(scip_, params.search_logs_filename().c_str());
  }

  const SCIP_Bool set_param_quiet =
      static_cast<SCIP_Bool>(!params.silence_output());

  RETURN_IF_SCIP_ERROR(SCIPsetEmphasis(
      scip_, ConvertEmphasis(params.emphasis()), set_param_quiet));
  if (params.has_heuristics()) {
    RETURN_IF_SCIP_ERROR(SCIPsetHeuristics(
        scip_, ConvertMetaParamValue(params.heuristics()), set_param_quiet));
  }
  if (params.has_presolve()) {
    RETURN_IF_SCIP_ERROR(SCIPsetPresolving(
        scip_, ConvertMetaParamValue(params.presolve()), set_param_quiet));
  }
  if (params.has_separating()) {
    RETURN_IF_SCIP_ERROR(SCIPsetSeparating(
        scip_, ConvertMetaParamValue(params.separating()), set_param_quiet));
  }
  for (const auto& bool_param : params.bool_params()) {
    RETURN_IF_SCIP_ERROR(
        (SCIPsetBoolParam(scip_, bool_param.first.c_str(), bool_param.second)));
  }
  for (const auto& int_param : params.int_params()) {
    RETURN_IF_SCIP_ERROR(
        (SCIPsetIntParam(scip_, int_param.first.c_str(), int_param.second)));
  }
  for (const auto& long_param : params.long_params()) {
    RETURN_IF_SCIP_ERROR((SCIPsetLongintParam(scip_, long_param.first.c_str(),
                                              long_param.second)));
  }
  for (const auto& char_param : params.char_params()) {
    if (char_param.second.size() != 1) {
      return absl::InvalidArgumentError(
          absl::StrCat("Character parameters must be single character strings, "
                       "but parameter: ",
                       char_param.first, " was: ", char_param.second));
    }
    RETURN_IF_SCIP_ERROR((SCIPsetCharParam(scip_, char_param.first.c_str(),
                                           char_param.second[0])));
  }
  for (const auto& string_param : params.string_params()) {
    RETURN_IF_SCIP_ERROR((SCIPsetStringParam(scip_, string_param.first.c_str(),
                                             string_param.second.c_str())));
  }
  for (const auto& real_param : params.real_params()) {
    RETURN_IF_SCIP_ERROR(
        (SCIPsetRealParam(scip_, real_param.first.c_str(), real_param.second)));
  }
  if (!legacy_params.empty()) {
    RETURN_IF_ERROR(
        LegacyScipSetSolverSpecificParameters(legacy_params, scip_));
  }
  return absl::OkStatus();
}

absl::StatusOr<std::unique_ptr<GScip>> GScip::Create(
    const std::string& problem_name) {
  SCIP* scip = nullptr;
  RETURN_IF_SCIP_ERROR(SCIPcreate(&scip));
  RETURN_IF_SCIP_ERROR(SCIPincludeDefaultPlugins(scip));
  RETURN_IF_SCIP_ERROR(SCIPcreateProbBasic(scip, problem_name.c_str()));
  // NOTE(user): the constructor is private, so we cannot call make_unique.
  return absl::WrapUnique(new GScip(scip));
}

GScip::GScip(SCIP* scip) : scip_(scip) {}

double GScip::ScipInf() { return SCIPinfinity(scip_); }

absl::Status GScip::FreeTransform() {
  return SCIP_TO_STATUS(SCIPfreeTransform(scip_));
}

std::string GScip::ScipVersion() {
  return absl::StrFormat("SCIP %d.%d.%d [LP solver: %s]", SCIPmajorVersion(),
                         SCIPminorVersion(), SCIPtechVersion(),
                         SCIPlpiGetSolverName());
}

bool GScip::InterruptSolve() {
  if (scip_ == nullptr) {
    return true;
  }
  return SCIPinterruptSolve(scip_) == SCIP_OKAY;
}

absl::Status GScip::CleanUp() {
  if (scip_ != nullptr) {
    for (SCIP_VAR* variable : variables_) {
      if (variable != nullptr) {
        RETURN_IF_SCIP_ERROR(SCIPreleaseVar(scip_, &variable));
      }
    }
    for (SCIP_CONS* constraint : constraints_) {
      if (constraint != nullptr) {
        RETURN_IF_SCIP_ERROR(SCIPreleaseCons(scip_, &constraint));
      }
    }
    RETURN_IF_SCIP_ERROR(SCIPfree(&scip_));
  }
  return absl::OkStatus();
}

GScip::~GScip() {
  const absl::Status clean_up_status = CleanUp();
  LOG_IF(DFATAL, !clean_up_status.ok()) << clean_up_status;
}

absl::StatusOr<SCIP_VAR*> GScip::AddVariable(
    double lb, double ub, double obj_coef, GScipVarType var_type,
    const std::string& var_name, const GScipVariableOptions& options) {
  SCIP_VAR* var = nullptr;
  lb = ScipInfClamp(lb);
  ub = ScipInfClamp(ub);
  RETURN_IF_SCIP_ERROR(SCIPcreateVarBasic(scip_, /*var=*/&var,
                                          /*name=*/var_name.c_str(),
                                          /*lb=*/lb, /*ub=*/ub,
                                          /*obj=*/obj_coef,
                                          ConvertVarType(var_type)));
  RETURN_IF_SCIP_ERROR(SCIPvarSetInitial(var, options.initial));
  RETURN_IF_SCIP_ERROR(SCIPvarSetRemovable(var, options.removable));
  RETURN_IF_SCIP_ERROR(SCIPaddVar(scip_, var));
  if (options.keep_alive) {
    variables_.insert(var);
  } else {
    RETURN_IF_SCIP_ERROR(SCIPreleaseVar(scip_, &var));
  }
  return var;
}

absl::Status GScip::MaybeKeepConstraintAlive(
    SCIP_CONS* constraint, const GScipConstraintOptions& options) {
  if (options.keep_alive) {
    constraints_.insert(constraint);
  } else {
    RETURN_IF_SCIP_ERROR(SCIPreleaseCons(scip_, &constraint));
  }
  return absl::OkStatus();
}

absl::StatusOr<SCIP_CONS*> GScip::AddLinearConstraint(
    const GScipLinearRange& range, const std::string& name,
    const GScipConstraintOptions& options) {
  SCIP_CONS* constraint = nullptr;
  RETURN_ERROR_UNLESS(range.variables.size() == range.coefficients.size())
      << "Error adding constraint: " << name << ".";
  RETURN_IF_SCIP_ERROR(SCIPcreateConsLinear(
      scip_, &constraint, name.c_str(), range.variables.size(),
      const_cast<SCIP_VAR**>(range.variables.data()),
      const_cast<double*>(range.coefficients.data()),
      ScipInfClamp(range.lower_bound), ScipInfClamp(range.upper_bound),
      /*initial=*/options.initial,
      /*separate=*/options.separate,
      /*enforce=*/options.enforce,
      /*check=*/options.check,
      /*propagate=*/options.propagate,
      /*local=*/options.local,
      /*modifiable=*/options.modifiable,
      /*dynamic=*/options.dynamic,
      /*removable=*/options.removable,
      /*stickingatnode=*/options.sticking_at_node));
  RETURN_IF_SCIP_ERROR(SCIPaddCons(scip_, constraint));
  RETURN_IF_ERROR(MaybeKeepConstraintAlive(constraint, options));
  return constraint;
}

absl::StatusOr<SCIP_CONS*> GScip::AddQuadraticConstraint(
    const GScipQuadraticRange& range, const std::string& name,
    const GScipConstraintOptions& options) {
  SCIP_CONS* constraint = nullptr;
  const int num_lin_vars = range.linear_variables.size();
  RETURN_ERROR_UNLESS(num_lin_vars == range.linear_coefficients.size())
      << "Error adding quadratic constraint: " << name << " in linear term.";
  const int num_quad_vars = range.quadratic_variables1.size();
  RETURN_ERROR_UNLESS(num_quad_vars == range.quadratic_variables2.size())
      << "Error adding quadratic constraint: " << name << " in quadratic term.";
  RETURN_ERROR_UNLESS(num_quad_vars == range.quadratic_coefficients.size())
      << "Error adding quadratic constraint: " << name << " in quadratic term.";
  RETURN_IF_SCIP_ERROR(SCIPcreateConsQuadratic(
      scip_, &constraint, name.c_str(), num_lin_vars,
      const_cast<SCIP_Var**>(range.linear_variables.data()),
      const_cast<double*>(range.linear_coefficients.data()), num_quad_vars,
      const_cast<SCIP_Var**>(range.quadratic_variables1.data()),
      const_cast<SCIP_Var**>(range.quadratic_variables2.data()),
      const_cast<double*>(range.quadratic_coefficients.data()),
      ScipInfClamp(range.lower_bound), ScipInfClamp(range.upper_bound),
      /*initial=*/options.initial,
      /*separate=*/options.separate,
      /*enforce=*/options.enforce,
      /*check=*/options.check,
      /*propagate=*/options.propagate,
      /*local=*/options.local,
      /*modifiable=*/options.modifiable,
      /*dynamic=*/options.dynamic,
      /*removable=*/options.removable));
  RETURN_IF_SCIP_ERROR(SCIPaddCons(scip_, constraint));
  RETURN_IF_ERROR(MaybeKeepConstraintAlive(constraint, options));
  return constraint;
}

absl::StatusOr<SCIP_CONS*> GScip::AddIndicatorConstraint(
    const GScipIndicatorConstraint& indicator_constraint,
    const std::string& name, const GScipConstraintOptions& options) {
  SCIP_VAR* indicator = indicator_constraint.indicator_variable;
  RETURN_ERROR_UNLESS(indicator != nullptr)
      << "Error adding indicator constraint: " << name << ".";
  if (indicator_constraint.negate_indicator) {
    RETURN_IF_SCIP_ERROR(SCIPgetNegatedVar(scip_, indicator, &indicator));
  }

  SCIP_CONS* constraint = nullptr;
  RETURN_ERROR_UNLESS(indicator_constraint.variables.size() ==
                      indicator_constraint.coefficients.size())
      << "Error adding indicator constraint: " << name << ".";
  RETURN_IF_SCIP_ERROR(SCIPcreateConsIndicator(
      scip_, &constraint, name.c_str(), indicator,
      indicator_constraint.variables.size(),
      const_cast<SCIP_Var**>(indicator_constraint.variables.data()),
      const_cast<double*>(indicator_constraint.coefficients.data()),
      ScipInfClamp(indicator_constraint.upper_bound),
      /*initial=*/options.initial,
      /*separate=*/options.separate,
      /*enforce=*/options.enforce,
      /*check=*/options.check,
      /*propagate=*/options.propagate,
      /*local=*/options.local,
      /*dynamic=*/options.dynamic,
      /*removable=*/options.removable,
      /*stickingatnode=*/options.sticking_at_node));
  RETURN_IF_SCIP_ERROR(SCIPaddCons(scip_, constraint));
  RETURN_IF_ERROR(MaybeKeepConstraintAlive(constraint, options));
  return constraint;
}

absl::StatusOr<SCIP_CONS*> GScip::AddAndConstraint(
    const GScipLogicalConstraintData& logical_data, const std::string& name,
    const GScipConstraintOptions& options) {
  RETURN_ERROR_UNLESS(logical_data.resultant != nullptr)
      << "Error adding and constraint: " << name << ".";
  SCIP_CONS* constraint = nullptr;
  RETURN_IF_SCIP_ERROR(
      SCIPcreateConsAnd(scip_, &constraint, name.c_str(),
                        logical_data.resultant, logical_data.operators.size(),
                        const_cast<SCIP_VAR**>(logical_data.operators.data()),
                        /*initial=*/options.initial,
                        /*separate=*/options.separate,
                        /*enforce=*/options.enforce,
                        /*check=*/options.check,
                        /*propagate=*/options.propagate,
                        /*local=*/options.local,
                        /*modifiable=*/options.modifiable,
                        /*dynamic=*/options.dynamic,
                        /*removable=*/options.removable,
                        /*stickingatnode=*/options.sticking_at_node));
  RETURN_IF_SCIP_ERROR(SCIPaddCons(scip_, constraint));
  RETURN_IF_ERROR(MaybeKeepConstraintAlive(constraint, options));
  return constraint;
}

absl::StatusOr<SCIP_CONS*> GScip::AddOrConstraint(
    const GScipLogicalConstraintData& logical_data, const std::string& name,
    const GScipConstraintOptions& options) {
  RETURN_ERROR_UNLESS(logical_data.resultant != nullptr)
      << "Error adding or constraint: " << name << ".";
  SCIP_CONS* constraint = nullptr;
  RETURN_IF_SCIP_ERROR(
      SCIPcreateConsOr(scip_, &constraint, name.c_str(), logical_data.resultant,
                       logical_data.operators.size(),
                       const_cast<SCIP_Var**>(logical_data.operators.data()),
                       /*initial=*/options.initial,
                       /*separate=*/options.separate,
                       /*enforce=*/options.enforce,
                       /*check=*/options.check,
                       /*propagate=*/options.propagate,
                       /*local=*/options.local,
                       /*modifiable=*/options.modifiable,
                       /*dynamic=*/options.dynamic,
                       /*removable=*/options.removable,
                       /*stickingatnode=*/options.sticking_at_node));
  RETURN_IF_SCIP_ERROR(SCIPaddCons(scip_, constraint));
  RETURN_IF_ERROR(MaybeKeepConstraintAlive(constraint, options));
  return constraint;
}

namespace {

absl::Status ValidateSOSData(const GScipSOSData& sos_data,
                             const std::string& name) {
  RETURN_ERROR_UNLESS(!sos_data.variables.empty())
      << "Error adding SOS constraint: " << name << ".";
  if (!sos_data.weights.empty()) {
    RETURN_ERROR_UNLESS(sos_data.variables.size() == sos_data.weights.size())
        << " Error adding SOS constraint: " << name << ".";
  }
  absl::flat_hash_set<double> distinct_weights;
  for (const double w : sos_data.weights) {
    RETURN_ERROR_UNLESS(!distinct_weights.contains(w))
        << "Error adding SOS constraint: " << name
        << ", weights must be distinct, but found value " << w << " twice.";
    distinct_weights.insert(w);
  }
  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<SCIP_CONS*> GScip::AddSOS1Constraint(
    const GScipSOSData& sos_data, const std::string& name,
    const GScipConstraintOptions& options) {
  RETURN_IF_ERROR(ValidateSOSData(sos_data, name));
  SCIP_CONS* constraint = nullptr;
  double* weights = nullptr;
  if (!sos_data.weights.empty()) {
    weights = const_cast<double*>(sos_data.weights.data());
  }

  RETURN_IF_SCIP_ERROR(SCIPcreateConsSOS1(
      scip_, &constraint, name.c_str(), sos_data.variables.size(),
      const_cast<SCIP_Var**>(sos_data.variables.data()), weights,
      /*initial=*/options.initial,
      /*separate=*/options.separate,
      /*enforce=*/options.enforce,
      /*check=*/options.check,
      /*propagate=*/options.propagate,
      /*local=*/options.local,
      /*dynamic=*/options.dynamic,
      /*removable=*/options.removable,
      /*stickingatnode=*/options.sticking_at_node));
  RETURN_IF_SCIP_ERROR(SCIPaddCons(scip_, constraint));
  RETURN_IF_ERROR(MaybeKeepConstraintAlive(constraint, options));
  return constraint;
}

absl::StatusOr<SCIP_CONS*> GScip::AddSOS2Constraint(
    const GScipSOSData& sos_data, const std::string& name,
    const GScipConstraintOptions& options) {
  RETURN_IF_ERROR(ValidateSOSData(sos_data, name));
  SCIP_CONS* constraint = nullptr;
  double* weights = nullptr;
  if (!sos_data.weights.empty()) {
    weights = const_cast<double*>(sos_data.weights.data());
  }
  RETURN_IF_SCIP_ERROR(SCIPcreateConsSOS2(
      scip_, &constraint, name.c_str(), sos_data.variables.size(),
      const_cast<SCIP_Var**>(sos_data.variables.data()), weights,
      /*initial=*/options.initial,
      /*separate=*/options.separate,
      /*enforce=*/options.enforce,
      /*check=*/options.check,
      /*propagate=*/options.propagate,
      /*local=*/options.local,
      /*dynamic=*/options.dynamic,
      /*removable=*/options.removable,
      /*stickingatnode=*/options.sticking_at_node));
  RETURN_IF_SCIP_ERROR(SCIPaddCons(scip_, constraint));
  RETURN_IF_ERROR(MaybeKeepConstraintAlive(constraint, options));
  return constraint;
}

absl::Status GScip::SetMaximize(bool is_maximize) {
  RETURN_IF_SCIP_ERROR(SCIPsetObjsense(
      scip_, is_maximize ? SCIP_OBJSENSE_MAXIMIZE : SCIP_OBJSENSE_MINIMIZE));
  return absl::OkStatus();
}

absl::Status GScip::SetObjectiveOffset(double offset) {
  double old_offset = SCIPgetOrigObjoffset(scip_);
  double delta_offset = offset - old_offset;
  RETURN_IF_SCIP_ERROR(SCIPaddOrigObjoffset(scip_, delta_offset));
  return absl::OkStatus();
}

bool GScip::ObjectiveIsMaximize() {
  return SCIPgetObjsense(scip_) == SCIP_OBJSENSE_MAXIMIZE;
}

double GScip::ObjectiveOffset() { return SCIPgetOrigObjoffset(scip_); }

absl::Status GScip::SetBranchingPriority(SCIP_VAR* var, int priority) {
  RETURN_IF_SCIP_ERROR(SCIPchgVarBranchPriority(scip_, var, priority));
  return absl::OkStatus();
}

absl::Status GScip::SetLb(SCIP_VAR* var, double lb) {
  lb = ScipInfClamp(lb);
  RETURN_IF_SCIP_ERROR(SCIPchgVarLb(scip_, var, lb));
  return absl::OkStatus();
}

absl::Status GScip::SetUb(SCIP_VAR* var, double ub) {
  ub = ScipInfClamp(ub);
  RETURN_IF_SCIP_ERROR(SCIPchgVarUb(scip_, var, ub));
  return absl::OkStatus();
}

absl::Status GScip::SetObjCoef(SCIP_VAR* var, double obj_coef) {
  RETURN_IF_SCIP_ERROR(SCIPchgVarObj(scip_, var, obj_coef));
  return absl::OkStatus();
}

absl::Status GScip::SetVarType(SCIP_VAR* var, GScipVarType var_type) {
  SCIP_Bool infeasible;
  RETURN_IF_SCIP_ERROR(
      SCIPchgVarType(scip_, var, ConvertVarType(var_type), &infeasible));
  return absl::OkStatus();
}

absl::Status GScip::DeleteVariable(SCIP_VAR* var) {
  SCIP_Bool did_delete;
  RETURN_IF_SCIP_ERROR(SCIPdelVar(scip_, var, &did_delete));
  RETURN_ERROR_UNLESS(static_cast<bool>(did_delete))
      << "Failed to delete variable named: " << Name(var);
  variables_.erase(var);
  RETURN_IF_SCIP_ERROR(SCIPreleaseVar(scip_, &var));
  return absl::OkStatus();
}

absl::Status GScip::CanSafeBulkDelete(
    const absl::flat_hash_set<SCIP_VAR*>& vars) {
  for (SCIP_CONS* constraint : constraints_) {
    if (!IsConstraintLinear(constraint)) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Model contains nonlinear constraint: ", Name(constraint)));
    }
  }
  return absl::OkStatus();
}

absl::Status GScip::SafeBulkDelete(const absl::flat_hash_set<SCIP_VAR*>& vars) {
  RETURN_IF_ERROR(CanSafeBulkDelete(vars));
  // Now, we can assume that all constraints are linear.
  for (SCIP_CONS* constraint : constraints_) {
    const absl::Span<SCIP_VAR* const> nonzeros =
        LinearConstraintVariables(constraint);
    const std::vector<SCIP_VAR*> nonzeros_copy(nonzeros.begin(),
                                               nonzeros.end());
    for (SCIP_VAR* var : nonzeros_copy) {
      if (vars.contains(var)) {
        RETURN_IF_ERROR(SetLinearConstraintCoef(constraint, var, 0.0));
      }
    }
  }
  for (SCIP_VAR* const var : vars) {
    RETURN_IF_ERROR(DeleteVariable(var));
  }
  return absl::OkStatus();
}

double GScip::Lb(SCIP_VAR* var) {
  return ScipInfUnclamp(SCIPvarGetLbOriginal(var));
}

double GScip::Ub(SCIP_VAR* var) {
  return ScipInfUnclamp(SCIPvarGetUbOriginal(var));
}

double GScip::ObjCoef(SCIP_VAR* var) { return SCIPvarGetObj(var); }

GScipVarType GScip::VarType(SCIP_VAR* var) {
  return ConvertVarType(SCIPvarGetType(var));
}

absl::string_view GScip::Name(SCIP_VAR* var) { return SCIPvarGetName(var); }

absl::string_view GScip::ConstraintType(SCIP_CONS* constraint) {
  return absl::string_view(SCIPconshdlrGetName(SCIPconsGetHdlr(constraint)));
}

bool GScip::IsConstraintLinear(SCIP_CONS* constraint) {
  return ConstraintType(constraint) == kLinearConstraintHandlerName;
}

absl::Span<const double> GScip::LinearConstraintCoefficients(
    SCIP_CONS* constraint) {
  int num_vars = SCIPgetNVarsLinear(scip_, constraint);
  return absl::MakeConstSpan(SCIPgetValsLinear(scip_, constraint), num_vars);
}

absl::Span<SCIP_VAR* const> GScip::LinearConstraintVariables(
    SCIP_CONS* constraint) {
  int num_vars = SCIPgetNVarsLinear(scip_, constraint);
  return absl::MakeConstSpan(SCIPgetVarsLinear(scip_, constraint), num_vars);
}

double GScip::LinearConstraintLb(SCIP_CONS* constraint) {
  return ScipInfUnclamp(SCIPgetLhsLinear(scip_, constraint));
}

double GScip::LinearConstraintUb(SCIP_CONS* constraint) {
  return ScipInfUnclamp(SCIPgetRhsLinear(scip_, constraint));
}

absl::string_view GScip::Name(SCIP_CONS* constraint) {
  return SCIPconsGetName(constraint);
}

absl::Status GScip::SetLinearConstraintLb(SCIP_CONS* constraint, double lb) {
  lb = ScipInfClamp(lb);
  RETURN_IF_SCIP_ERROR(SCIPchgLhsLinear(scip_, constraint, lb));
  return absl::OkStatus();
}

absl::Status GScip::SetLinearConstraintUb(SCIP_CONS* constraint, double ub) {
  ub = ScipInfClamp(ub);
  RETURN_IF_SCIP_ERROR(SCIPchgRhsLinear(scip_, constraint, ub));
  return absl::OkStatus();
}

absl::Status GScip::DeleteConstraint(SCIP_CONS* constraint) {
  RETURN_IF_SCIP_ERROR(SCIPdelCons(scip_, constraint));
  constraints_.erase(constraint);
  RETURN_IF_SCIP_ERROR(SCIPreleaseCons(scip_, &constraint));
  return absl::OkStatus();
}

absl::Status GScip::SetLinearConstraintCoef(SCIP_CONS* constraint,
                                            SCIP_VAR* var, double value) {
  // TODO(user): this operation is slow (linear in the nnz in the constraint).
  // It would be better to just use a bulk operation, but there doesn't appear
  // to be any?
  RETURN_IF_SCIP_ERROR(SCIPchgCoefLinear(scip_, constraint, var, value));
  return absl::OkStatus();
}

absl::StatusOr<GScipHintResult> GScip::SuggestHint(
    const GScipSolution& partial_solution) {
  SCIP_SOL* solution;
  const int scip_num_vars = SCIPgetNOrigVars(scip_);
  const bool is_solution_partial = partial_solution.size() < scip_num_vars;
  if (is_solution_partial) {
    RETURN_IF_SCIP_ERROR(SCIPcreatePartialSol(scip_, &solution, nullptr));
  } else {
    // This is actually a full solution
    RETURN_ERROR_UNLESS(partial_solution.size() == scip_num_vars)
        << "Error suggesting hint.";
    RETURN_IF_SCIP_ERROR(SCIPcreateSol(scip_, &solution, nullptr));
  }
  for (const auto& var_value_pair : partial_solution) {
    RETURN_IF_SCIP_ERROR(SCIPsetSolVal(scip_, solution, var_value_pair.first,
                                       var_value_pair.second));
  }
  if (!is_solution_partial) {
    SCIP_Bool is_feasible;
    RETURN_IF_SCIP_ERROR(SCIPcheckSol(
        scip_, solution, /*printreason=*/false, /*completely=*/true,
        /*checkbounds=*/true, /*checkintegrality=*/true, /*checklprows=*/true,
        &is_feasible));
    if (!static_cast<bool>(is_feasible)) {
      RETURN_IF_SCIP_ERROR(SCIPfreeSol(scip_, &solution));
      return GScipHintResult::kInfeasible;
    }
  }
  SCIP_Bool is_stored;
  RETURN_IF_SCIP_ERROR(SCIPaddSolFree(scip_, &solution, &is_stored));
  if (static_cast<bool>(is_stored)) {
    return GScipHintResult::kAccepted;
  } else {
    return GScipHintResult::kRejected;
  }
}

absl::StatusOr<GScipResult> GScip::Solve(
    const GScipParameters& params, const std::string& legacy_params,
    const GScipMessageHandler message_handler) {
  // A four step process:
  //  1. Apply parameters.
  //  2. Solve the problem.
  //  3. Extract solution and solve statistics.
  //  4. Prepare the solver for further modification/solves (reset parameters,
  //     free the solutions found).
  GScipResult result;

  // Step 1: apply parameters.
  const absl::Status param_status = SetParams(params, legacy_params);
  if (!param_status.ok()) {
    result.gscip_output.set_status(GScipOutput::INVALID_SOLVER_PARAMETERS);
    // Conversion to std::string for open source build.
    result.gscip_output.set_status_detail(
        std::string(param_status.message()));  // NOLINT
    return result;
  }
  if (params.print_scip_model()) {
    RETURN_IF_SCIP_ERROR(SCIPwriteOrigProblem(scip_, nullptr, "cip", FALSE));
  }
  if (!params.scip_model_filename().empty()) {
    RETURN_IF_SCIP_ERROR(SCIPwriteOrigProblem(
        scip_, params.scip_model_filename().c_str(), "cip", FALSE));
  }

  // Install the message handler if necessary. We do this after setting the
  // parameters so that parameters that applies to the default message handler
  // like `quiet` are indeed applied to it and not to our temporary
  // handler.
  using internal::CaptureMessageHandlerPtr;
  using internal::MessageHandlerPtr;
  MessageHandlerPtr previous_handler;
  MessageHandlerPtr new_handler;
  if (message_handler != nullptr) {
    previous_handler = CaptureMessageHandlerPtr(SCIPgetMessagehdlr(scip_));
    ASSIGN_OR_RETURN(new_handler,
                     internal::MakeSCIPMessageHandler(message_handler));
    SCIPsetMessagehdlr(scip_, new_handler.get());
  }
  // Make sure we prevent any call of message_handler after this function has
  // returned, until the new_handler is reset (see below).
  const internal::ScopedSCIPMessageHandlerDisabler new_handler_disabler(
      new_handler);

  // Step 2: Solve.
  // NOTE(user): after solve, SCIP will either be in stage PRESOLVING,
  // SOLVING, OR SOLVED.
  if (GScipMaxNumThreads(params) > 1) {
    RETURN_IF_SCIP_ERROR(SCIPsolveConcurrent(scip_));
  } else {
    RETURN_IF_SCIP_ERROR(SCIPsolve(scip_));
  }
  const SCIP_STAGE stage = SCIPgetStage(scip_);
  if (stage != SCIP_STAGE_PRESOLVING && stage != SCIP_STAGE_SOLVING &&
      stage != SCIP_STAGE_SOLVED) {
    result.gscip_output.set_status(GScipOutput::UNKNOWN);
    result.gscip_output.set_status_detail(
        absl::StrCat("Unpexpected SCIP final stage= ", stage,
                     " was expected to be either SCIP_STAGE_PRESOLVING, "
                     "SCIP_STAGE_SOLVING, or SCIP_STAGE_SOLVED"));
    return result;
  }
  if (params.print_detailed_solving_stats()) {
    RETURN_IF_SCIP_ERROR(SCIPprintStatistics(scip_, nullptr));
  }
  if (!params.detailed_solving_stats_filename().empty()) {
    FILE* file = fopen(params.detailed_solving_stats_filename().c_str(), "w");
    if (file == nullptr) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Could not open file: ", params.detailed_solving_stats_filename(),
          " to write SCIP solve stats."));
    }
    RETURN_IF_SCIP_ERROR(SCIPprintStatistics(scip_, file));
    int close_result = fclose(file);
    if (close_result != 0) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Error: ", close_result,
          " closing file: ", params.detailed_solving_stats_filename(),
          " when writing solve stats."));
    }
  }
  // Step 3: Extract solution information.
  // Some outputs are available unconditionally, and some are only ready if at
  // least presolve succeeded.
  GScipSolvingStats* stats = result.gscip_output.mutable_stats();
  const int num_scip_solutions = SCIPgetNSols(scip_);
  const int num_returned_solutions =
      std::min(num_scip_solutions, std::max(1, params.num_solutions()));
  SCIP_SOL** all_solutions = SCIPgetSols(scip_);
  stats->set_best_objective(ScipInfUnclamp(SCIPgetPrimalbound(scip_)));
  for (int i = 0; i < num_returned_solutions; ++i) {
    SCIP_SOL* scip_sol = all_solutions[i];
    const double obj_value = ScipInfUnclamp(SCIPgetSolOrigObj(scip_, scip_sol));
    GScipSolution solution;
    for (SCIP_VAR* v : variables_) {
      solution[v] = SCIPgetSolVal(scip_, scip_sol, v);
    }
    result.solutions.push_back(solution);
    result.objective_values.push_back(obj_value);
  }
  // Can only check for primal ray if we made it past presolve.
  if (stage != SCIP_STAGE_PRESOLVING && SCIPhasPrimalRay(scip_)) {
    for (SCIP_VAR* v : variables_) {
      result.primal_ray[v] = SCIPgetPrimalRayVal(scip_, v);
    }
  }
  // TODO(user): refactor this into a new method.
  stats->set_best_bound(ScipInfUnclamp(SCIPgetDualbound(scip_)));
  stats->set_node_count(SCIPgetNTotalNodes(scip_));
  stats->set_first_lp_relaxation_bound(SCIPgetFirstLPDualboundRoot(scip_));
  stats->set_root_node_bound(SCIPgetDualboundRoot(scip_));
  if (stage != SCIP_STAGE_PRESOLVING) {
    stats->set_total_lp_iterations(SCIPgetNLPIterations(scip_));
    stats->set_primal_simplex_iterations(SCIPgetNPrimalLPIterations(scip_));
    stats->set_dual_simplex_iterations(SCIPgetNDualLPIterations(scip_));
    stats->set_deterministic_time(SCIPgetDeterministicTime(scip_));
  }
  result.gscip_output.set_status(ConvertStatus(SCIPgetStatus(scip_)));

  // Step 4: clean up.
  RETURN_IF_ERROR(FreeTransform());

  // Restore the previous message handler. We must do so AFTER we reset the
  // stage of the problem with FreeTransform(). Doing so before will fail since
  // changing the message handler is only possible in INIT and PROBLEM stages.
  if (message_handler != nullptr) {
    RETURN_IF_SCIP_ERROR(SCIPsetMessagehdlr(scip_, previous_handler.get()));

    // Resetting the unique_ptr will free the associated handler which will
    // flush the buffer if the last log line was unfinished. If we were not
    // resetting it, the last new_handler_disabler would disable the handler and
    // the remainder of the buffer content would be lost.
    new_handler.reset();
  }

  RETURN_IF_SCIP_ERROR(SCIPresetParams(scip_));
  // The `silence_output` and `search_logs_filename` parameters are special
  // since those are not parameters but properties of the SCIP message
  // handler. Hence we reset them explicitly.
  SCIPsetMessagehdlrQuiet(scip_, false);
  SCIPsetMessagehdlrLogfile(scip_, nullptr);

  return result;
}

absl::StatusOr<bool> GScip::DefaultBoolParamValue(
    const std::string& parameter_name) {
  SCIP_Bool default_value;
  RETURN_IF_SCIP_ERROR(
      SCIPgetBoolParam(scip_, parameter_name.c_str(), &default_value));
  return static_cast<bool>(default_value);
}

absl::StatusOr<int> GScip::DefaultIntParamValue(
    const std::string& parameter_name) {
  int default_value;
  RETURN_IF_SCIP_ERROR(
      SCIPgetIntParam(scip_, parameter_name.c_str(), &default_value));
  return default_value;
}

absl::StatusOr<int64_t> GScip::DefaultLongParamValue(
    const std::string& parameter_name) {
  SCIP_Longint result;
  RETURN_IF_SCIP_ERROR(
      SCIPgetLongintParam(scip_, parameter_name.c_str(), &result));
  return static_cast<int64_t>(result);
}

absl::StatusOr<double> GScip::DefaultRealParamValue(
    const std::string& parameter_name) {
  double result;
  RETURN_IF_SCIP_ERROR(
      SCIPgetRealParam(scip_, parameter_name.c_str(), &result));
  return result;
}

absl::StatusOr<char> GScip::DefaultCharParamValue(
    const std::string& parameter_name) {
  char result;
  RETURN_IF_SCIP_ERROR(
      SCIPgetCharParam(scip_, parameter_name.c_str(), &result));
  return result;
}

absl::StatusOr<std::string> GScip::DefaultStringParamValue(
    const std::string& parameter_name) {
  char* result;
  RETURN_IF_SCIP_ERROR(
      SCIPgetStringParam(scip_, parameter_name.c_str(), &result));
  return std::string(result);
}

double GScip::ScipInfClamp(double d) {
  const double kScipInf = ScipInf();
  if (d > kScipInf) return kScipInf;
  if (d < -kScipInf) return -kScipInf;
  return d;
}

double GScip::ScipInfUnclamp(double d) {
  const double kScipInf = ScipInf();
  if (d >= kScipInf) return std::numeric_limits<double>::infinity();
  if (d <= -kScipInf) return -std::numeric_limits<double>::infinity();
  return d;
}

#undef RETURN_ERROR_UNLESS

}  // namespace operations_research
