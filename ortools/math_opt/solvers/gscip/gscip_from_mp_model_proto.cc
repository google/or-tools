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

#include "ortools/math_opt/solvers/gscip/gscip_from_mp_model_proto.h"

#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/base/status_macros.h"
#include "ortools/base/stl_util.h"
#include "ortools/math_opt/solvers/gscip/gscip.h"
#include "ortools/math_opt/solvers/gscip/gscip_ext.h"
#include "scip/type_var.h"
#include "ortools/port/proto_utils.h"

namespace operations_research {

absl::StatusOr<GScipAndVariables> GScipAndVariables::FromMPModelProto(
    const MPModelProto& model) {
  GScipAndVariables result;
  ASSIGN_OR_RETURN(result.gscip, GScip::Create(model.name()));
  RETURN_IF_ERROR(result.gscip->SetMaximize(model.maximize()));
  RETURN_IF_ERROR(result.gscip->SetObjectiveOffset(model.objective_offset()));
  for (const MPVariableProto& variable : model.variable()) {
    ASSIGN_OR_RETURN(SCIP_VAR * v,
                     result.gscip->AddVariable(
                         variable.lower_bound(), variable.upper_bound(),
                         variable.objective_coefficient(),
                         variable.is_integer() ? GScipVarType::kInteger
                                               : GScipVarType::kContinuous,
                         variable.name()));
    result.variables.push_back(v);
  }
  for (const MPConstraintProto& linear_constraint : model.constraint()) {
    RETURN_IF_ERROR(result.AddLinearConstraint(linear_constraint));
  }
  for (const MPGeneralConstraintProto& gen_constraint :
       model.general_constraint()) {
    RETURN_IF_ERROR(result.AddGeneralConstraint(gen_constraint));
  }
  if (model.has_quadratic_objective()) {
    RETURN_IF_ERROR(result.AddQuadraticObjective(model.quadratic_objective()));
  }
  return result;
}

std::vector<SCIP_VAR*> GScipAndVariables::TranslateMPVars(
    absl::Span<const int32_t> mp_vars) {
  std::vector<SCIP_VAR*> result;
  result.reserve(mp_vars.size());
  for (const int32_t var : mp_vars) {
    result.push_back(variables[var]);
  }
  return result;
}

absl::Status GScipAndVariables::AddLinearConstraint(
    const MPConstraintProto& lin_constraint) {
  GScipLinearRange range;
  range.lower_bound = lin_constraint.lower_bound();
  range.upper_bound = lin_constraint.upper_bound();
  range.coefficients = {lin_constraint.coefficient().begin(),
                        lin_constraint.coefficient().end()};
  range.variables = TranslateMPVars(lin_constraint.var_index());
  return gscip->AddLinearConstraint(range, lin_constraint.name()).status();
}

absl::Status GScipAndVariables::AddGeneralConstraint(
    const MPGeneralConstraintProto& mp_gen) {
  const std::string& name = mp_gen.name();
  switch (mp_gen.general_constraint_case()) {
    case MPGeneralConstraintProto::kIndicatorConstraint:
      return AddIndicatorConstraint(name, mp_gen.indicator_constraint());
    case MPGeneralConstraintProto::kSosConstraint:
      return AddSosConstraint(name, mp_gen.sos_constraint());
    case MPGeneralConstraintProto::kQuadraticConstraint:
      return AddQuadraticConstraint(name, mp_gen.quadratic_constraint());
    case MPGeneralConstraintProto::kAbsConstraint:
      return AddAbsConstraint(name, mp_gen.abs_constraint());
    case MPGeneralConstraintProto::kAndConstraint:
      return AddAndConstraint(name, mp_gen.and_constraint());
    case MPGeneralConstraintProto::kOrConstraint:
      return AddOrConstraint(name, mp_gen.or_constraint());
    case MPGeneralConstraintProto::kMaxConstraint:
      return AddMaxConstraint(name, mp_gen.max_constraint());
    case MPGeneralConstraintProto::kMinConstraint:
      return AddMinConstraint(name, mp_gen.min_constraint());
    default:
      break;
  }
  return absl::UnimplementedError(
      absl::StrFormat("General constraints of type %i not supported.",
                      mp_gen.general_constraint_case()));
}

absl::Status GScipAndVariables::AddIndicatorConstraint(
    absl::string_view name, const MPIndicatorConstraint& mp_ind) {
  GScipIndicatorRangeConstraint ind_range;
  ind_range.indicator_variable = variables[mp_ind.var_index()];
  ind_range.negate_indicator = mp_ind.var_value() == 0;
  ind_range.range.lower_bound = mp_ind.constraint().lower_bound();
  ind_range.range.upper_bound = mp_ind.constraint().upper_bound();
  ind_range.range.coefficients = {mp_ind.constraint().coefficient().begin(),
                                  mp_ind.constraint().coefficient().end()};
  ind_range.range.variables = TranslateMPVars(mp_ind.constraint().var_index());
  return GScipCreateIndicatorRange(gscip.get(), ind_range, name);
}

absl::Status GScipAndVariables::AddSosConstraint(
    const std::string& name, const MPSosConstraint& mp_sos) {
  GScipSOSData sos;
  sos.weights = {mp_sos.weight().begin(), mp_sos.weight().end()};
  sos.variables = TranslateMPVars(mp_sos.var_index());
  // SOS constraints of type N indicate at most N variables are non-zero.
  // Constraints with N variables or less are valid, but useless. They also
  // crash SCIP, so we skip them.
  switch (mp_sos.type()) {
    case MPSosConstraint::SOS2:
      if (sos.variables.size() <= 2) {
        return absl::OkStatus();  // Skip trivial constraint
      }
      return gscip->AddSOS2Constraint(sos, name).status();
    case MPSosConstraint::SOS1_DEFAULT:
      if (sos.variables.size() <= 1) {
        return absl::OkStatus();  // Skip trivial constraint
      }
      return gscip->AddSOS1Constraint(sos, name).status();
  }
  return absl::UnimplementedError(
      absl::StrCat("Unknown SOS constraint type: ", mp_sos.type(), " (",
                   ProtoEnumToString(mp_sos.type()), ")"));
}

absl::Status GScipAndVariables::AddQuadraticConstraint(
    const std::string& name, const MPQuadraticConstraint& mp_quad) {
  GScipQuadraticRange range;
  range.lower_bound = mp_quad.lower_bound();
  range.upper_bound = mp_quad.upper_bound();
  range.linear_coefficients = {mp_quad.coefficient().begin(),
                               mp_quad.coefficient().end()};
  range.linear_variables = TranslateMPVars(mp_quad.var_index());
  range.quadratic_coefficients = {mp_quad.qcoefficient().begin(),
                                  mp_quad.qcoefficient().end()};
  range.quadratic_variables1 = TranslateMPVars(mp_quad.qvar1_index());
  range.quadratic_variables2 = TranslateMPVars(mp_quad.qvar2_index());
  return gscip->AddQuadraticConstraint(range, name).status();
}

absl::Status GScipAndVariables::AddAbsConstraint(
    absl::string_view name, const MPAbsConstraint& mp_abs) {
  return GScipCreateAbs(gscip.get(), variables[mp_abs.var_index()],
                        variables[mp_abs.resultant_var_index()], name);
}

absl::Status GScipAndVariables::AddAndConstraint(
    const std::string& name, const MPArrayConstraint& mp_and) {
  GScipLogicalConstraintData and_args;
  and_args.resultant = variables[mp_and.resultant_var_index()];
  and_args.operators = TranslateMPVars(mp_and.var_index());
  return gscip->AddAndConstraint(and_args, name).status();
}

absl::Status GScipAndVariables::AddOrConstraint(
    const std::string& name, const MPArrayConstraint& mp_or) {
  GScipLogicalConstraintData or_args;
  or_args.resultant = variables[mp_or.resultant_var_index()];
  or_args.operators = TranslateMPVars(mp_or.var_index());
  return gscip->AddOrConstraint(or_args, name).status();
}

std::vector<GScipLinearExpr>
GScipAndVariables::MpArrayWithConstantToGScipLinearExprs(
    const MPArrayWithConstantConstraint& mp_array_with_constant) {
  std::vector<int32_t> unique_vars(mp_array_with_constant.var_index().begin(),
                                   mp_array_with_constant.var_index().end());
  gtl::STLSortAndRemoveDuplicates(&unique_vars);
  std::vector<GScipLinearExpr> result;
  for (SCIP_VAR* input_var : TranslateMPVars(unique_vars)) {
    result.push_back(GScipLinearExpr(input_var));
  }
  if (mp_array_with_constant.has_constant()) {
    result.push_back(GScipLinearExpr(mp_array_with_constant.constant()));
  }
  return result;
}
absl::Status GScipAndVariables::AddMinConstraint(
    absl::string_view name, const MPArrayWithConstantConstraint& mp_min) {
  return GScipCreateMinimum(
      gscip.get(), GScipLinearExpr(variables[mp_min.resultant_var_index()]),
      MpArrayWithConstantToGScipLinearExprs(mp_min), name);
}
absl::Status GScipAndVariables::AddMaxConstraint(
    absl::string_view name, const MPArrayWithConstantConstraint& mp_max) {
  return GScipCreateMaximum(
      gscip.get(), GScipLinearExpr(variables[mp_max.resultant_var_index()]),
      MpArrayWithConstantToGScipLinearExprs(mp_max), name);
}

absl::Status GScipAndVariables::AddQuadraticObjective(
    const MPQuadraticObjective& quad_obj) {
  return GScipAddQuadraticObjectiveTerm(
      gscip.get(), TranslateMPVars(quad_obj.qvar1_index()),
      TranslateMPVars(quad_obj.qvar2_index()),
      {quad_obj.coefficient().begin(), quad_obj.coefficient().end()});
}

absl::Status GScipAndVariables::AddHint(
    const PartialVariableAssignment& mp_hint) {
  GScipSolution hint;
  for (int i = 0; i < mp_hint.var_index_size(); ++i) {
    hint[variables[mp_hint.var_index(i)]] = mp_hint.var_value(i);
  }
  return gscip->SuggestHint(hint).status();
}

}  // namespace operations_research
