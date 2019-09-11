// Copyright 2010-2018 Google LLC
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

#include "ortools/linear_solver/scip_proto_solver.h"

#include <cmath>
#include <limits>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

#include "absl/strings/ascii.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "absl/types/optional.h"
#include "ortools/base/canonical_errors.h"
#include "ortools/base/cleanup.h"
#include "ortools/base/status.h"
#include "ortools/base/status_macros.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/linear_solver/model_validator.h"
#include "ortools/linear_solver/scip_helper_macros.h"
#include "ortools/util/lazy_mutable_copy.h"
#include "scip/cons_disjunction.h"
#include "scip/cons_linear.h"
#include "scip/pub_var.h"
#include "scip/scip.h"
#include "scip/scip_param.h"
#include "scip/scip_prob.h"
#include "scip/scip_var.h"
#include "scip/scipdefplugins.h"
#include "scip/set.h"
#include "scip/struct_paramset.h"
#include "scip/type_cons.h"
#include "scip/type_paramset.h"
#include "scip/type_var.h"

namespace operations_research {

util::Status ScipSetSolverSpecificParameters(const std::string& parameters,
                                             SCIP* scip) {
  for (const auto parameter :
       absl::StrSplit(parameters, '\n', absl::SkipWhitespace())) {
    std::vector<std::string> key_value =
        absl::StrSplit(parameter, '=', absl::SkipWhitespace());
    if (key_value.size() != 2) {
      return util::InvalidArgumentError(
          absl::StrFormat("Cannot parse parameter '%s'. Expected format is "
                          "'parameter/name = value'",
                          parameter));
    }

    std::string name = key_value[0];
    absl::RemoveExtraAsciiWhitespace(&name);
    std::string value = key_value[1];
    absl::RemoveExtraAsciiWhitespace(&value);

    SCIP_PARAM* param = SCIPgetParam(scip, name.c_str());
    if (param == nullptr) {
      return util::InvalidArgumentError(
          absl::StrFormat("Invalid parameter name '%s'", name));
    }
    switch (param->paramtype) {
      case SCIP_PARAMTYPE_BOOL: {
        bool parsed_value;
        if (absl::SimpleAtob(value, &parsed_value)) {
          RETURN_IF_SCIP_ERROR(
              SCIPsetBoolParam(scip, name.c_str(), parsed_value));
          continue;
        }
        break;
      }
      case SCIP_PARAMTYPE_INT: {
        int parsed_value;
        if (absl::SimpleAtoi(value, &parsed_value)) {
          RETURN_IF_SCIP_ERROR(
              SCIPsetIntParam(scip, name.c_str(), parsed_value));
          continue;
        }
        break;
      }
      case SCIP_PARAMTYPE_LONGINT: {
        int64 parsed_value;
        if (absl::SimpleAtoi(value, &parsed_value)) {
          RETURN_IF_SCIP_ERROR(
              SCIPsetLongintParam(scip, name.c_str(), parsed_value));
          continue;
        }
        break;
      }
      case SCIP_PARAMTYPE_REAL: {
        double parsed_value;
        if (absl::SimpleAtod(value, &parsed_value)) {
          RETURN_IF_SCIP_ERROR(
              SCIPsetRealParam(scip, name.c_str(), parsed_value));
          continue;
        }
        break;
      }
      case SCIP_PARAMTYPE_CHAR: {
        if (value.size() == 1) {
          RETURN_IF_SCIP_ERROR(SCIPsetCharParam(scip, name.c_str(), value[0]));
          continue;
        }
        break;
      }
      case SCIP_PARAMTYPE_STRING: {
        if (value.front() == '"' && value.back() == '"') {
          value.erase(value.begin());
          value.erase(value.end() - 1);
        }
        RETURN_IF_SCIP_ERROR(
            SCIPsetStringParam(scip, name.c_str(), value.c_str()));
        continue;
      }
    }
    return util::InvalidArgumentError(
        absl::StrFormat("Invalid parameter value '%s'", parameter));
  }
  return util::OkStatus();
}

namespace {
// This function will create a new constraint if the indicator constraint has
// both a lower bound and an upper bound.
util::Status AddIndicatorConstraint(
    const MPGeneralConstraintProto& gen_cst,
    const std::vector<SCIP_VAR*>& scip_variables, SCIP* scip,
    SCIP_CONS** scip_cst, std::vector<SCIP_CONS*>* scip_constraints,
    std::vector<SCIP_VAR*>* tmp_variables,
    std::vector<double>* tmp_coefficients) {
  CHECK(scip != nullptr);
  CHECK(scip_cst != nullptr);
  CHECK(scip_constraints != nullptr);
  CHECK(tmp_variables != nullptr);
  CHECK(tmp_coefficients != nullptr);
  CHECK(gen_cst.has_indicator_constraint());
  constexpr double kInfinity = std::numeric_limits<double>::infinity();

  const auto& ind = gen_cst.indicator_constraint();
  if (!ind.has_constraint()) return util::OkStatus();

  const MPConstraintProto& constraint = ind.constraint();
  const int size = constraint.var_index_size();
  tmp_variables->resize(size, nullptr);
  tmp_coefficients->resize(size, 0);
  for (int i = 0; i < size; ++i) {
    (*tmp_variables)[i] = scip_variables[constraint.var_index(i)];
    (*tmp_coefficients)[i] = constraint.coefficient(i);
  }

  SCIP_VAR* ind_var = scip_variables[ind.var_index()];
  if (ind.var_value() == 0) {
    RETURN_IF_SCIP_ERROR(
        SCIPgetNegatedVar(scip, scip_variables[ind.var_index()], &ind_var));
  }

  if (ind.constraint().upper_bound() < kInfinity) {
    RETURN_IF_SCIP_ERROR(SCIPcreateConsIndicator(
        scip, scip_cst, gen_cst.name().c_str(), ind_var, size,
        tmp_variables->data(), tmp_coefficients->data(),
        ind.constraint().upper_bound(),
        /*initial=*/!ind.constraint().is_lazy(),
        /*separate=*/true,
        /*enforce=*/true,
        /*check=*/true,
        /*propagate=*/true,
        /*local=*/false,
        /*dynamic=*/false,
        /*removable=*/ind.constraint().is_lazy(),
        /*stickingatnode=*/false));
    RETURN_IF_SCIP_ERROR(SCIPaddCons(scip, *scip_cst));
    scip_constraints->push_back(nullptr);
    scip_cst = &scip_constraints->back();
  }
  if (ind.constraint().lower_bound() > -kInfinity) {
    for (int i = 0; i < size; ++i) {
      (*tmp_coefficients)[i] *= -1;
    }
    RETURN_IF_SCIP_ERROR(SCIPcreateConsIndicator(
        scip, scip_cst, gen_cst.name().c_str(), ind_var, size,
        tmp_variables->data(), tmp_coefficients->data(),
        -ind.constraint().lower_bound(),
        /*initial=*/!ind.constraint().is_lazy(),
        /*separate=*/true,
        /*enforce=*/true,
        /*check=*/true,
        /*propagate=*/true,
        /*local=*/false,
        /*dynamic=*/false,
        /*removable=*/ind.constraint().is_lazy(),
        /*stickingatnode=*/false));
    RETURN_IF_SCIP_ERROR(SCIPaddCons(scip, *scip_cst));
  }

  return util::OkStatus();
}

util::Status AddSosConstraint(const MPGeneralConstraintProto& gen_cst,
                              const std::vector<SCIP_VAR*>& scip_variables,
                              SCIP* scip, SCIP_CONS** scip_cst,
                              std::vector<SCIP_VAR*>* tmp_variables,
                              std::vector<double>* tmp_weights) {
  CHECK(scip != nullptr);
  CHECK(scip_cst != nullptr);
  CHECK(tmp_variables != nullptr);
  CHECK(tmp_weights != nullptr);

  CHECK(gen_cst.has_sos_constraint());
  const MPSosConstraint& sos_cst = gen_cst.sos_constraint();

  // SOS constraints of type N indicate at most N variables are non-zero.
  // Constraints with N variables or less are valid, but useless. They also
  // crash SCIP, so we skip them.
  if (sos_cst.var_index_size() <= 1) return util::OkStatus();
  if (sos_cst.type() == MPSosConstraint::SOS2 &&
      sos_cst.var_index_size() <= 2) {
    return util::OkStatus();
  }

  tmp_variables->resize(sos_cst.var_index_size(), nullptr);
  for (int v = 0; v < sos_cst.var_index_size(); ++v) {
    (*tmp_variables)[v] = scip_variables[sos_cst.var_index(v)];
  }
  tmp_weights->resize(sos_cst.var_index_size(), 0);
  if (sos_cst.weight_size() == sos_cst.var_index_size()) {
    for (int w = 0; w < sos_cst.weight_size(); ++w) {
      (*tmp_weights)[w] = sos_cst.weight(w);
    }
  } else {
    // In theory, SCIP should accept empty weight arrays and use natural
    // ordering, but in practice, this crashes their code.
    std::iota(tmp_weights->begin(), tmp_weights->end(), 1);
  }
  switch (sos_cst.type()) {
    case MPSosConstraint::SOS1_DEFAULT:
      RETURN_IF_SCIP_ERROR(
          SCIPcreateConsBasicSOS1(scip,
                                  /*cons=*/scip_cst,
                                  /*name=*/gen_cst.name().c_str(),
                                  /*nvars=*/sos_cst.var_index_size(),
                                  /*vars=*/tmp_variables->data(),
                                  /*weights=*/tmp_weights->data()));
      break;
    case MPSosConstraint::SOS2:
      RETURN_IF_SCIP_ERROR(
          SCIPcreateConsBasicSOS2(scip,
                                  /*cons=*/scip_cst,
                                  /*name=*/gen_cst.name().c_str(),
                                  /*nvars=*/sos_cst.var_index_size(),
                                  /*vars=*/tmp_variables->data(),
                                  /*weights=*/tmp_weights->data()));
      break;
  }
  RETURN_IF_SCIP_ERROR(SCIPaddCons(scip, *scip_cst));
  return util::OkStatus();
}

util::Status AddQuadraticConstraint(
    const MPGeneralConstraintProto& gen_cst,
    const std::vector<SCIP_VAR*>& scip_variables, SCIP* scip,
    SCIP_CONS** scip_cst, std::vector<SCIP_VAR*>* tmp_variables,
    std::vector<double>* tmp_coefficients,
    std::vector<SCIP_VAR*>* tmp_qvariables1,
    std::vector<SCIP_VAR*>* tmp_qvariables2,
    std::vector<double>* tmp_qcoefficients) {
  CHECK(scip != nullptr);
  CHECK(scip_cst != nullptr);
  CHECK(tmp_variables != nullptr);
  CHECK(tmp_coefficients != nullptr);
  CHECK(tmp_qvariables1 != nullptr);
  CHECK(tmp_qvariables2 != nullptr);
  CHECK(tmp_qcoefficients != nullptr);

  CHECK(gen_cst.has_quadratic_constraint());
  const MPQuadraticConstraint& quad_cst = gen_cst.quadratic_constraint();

  // Process linear part of the constraint.
  const int lsize = quad_cst.var_index_size();
  CHECK_EQ(quad_cst.coefficient_size(), lsize);
  tmp_variables->resize(lsize, nullptr);
  tmp_coefficients->resize(lsize, 0.0);
  for (int i = 0; i < lsize; ++i) {
    (*tmp_variables)[i] = scip_variables[quad_cst.var_index(i)];
    (*tmp_coefficients)[i] = quad_cst.coefficient(i);
  }

  // Process quadratic part of the constraint.
  const int qsize = quad_cst.qvar1_index_size();
  CHECK_EQ(quad_cst.qvar2_index_size(), qsize);
  CHECK_EQ(quad_cst.qcoefficient_size(), qsize);
  tmp_qvariables1->resize(qsize, nullptr);
  tmp_qvariables2->resize(qsize, nullptr);
  tmp_qcoefficients->resize(qsize, 0.0);
  for (int i = 0; i < qsize; ++i) {
    (*tmp_qvariables1)[i] = scip_variables[quad_cst.qvar1_index(i)];
    (*tmp_qvariables2)[i] = scip_variables[quad_cst.qvar2_index(i)];
    (*tmp_qcoefficients)[i] = quad_cst.qcoefficient(i);
  }

  RETURN_IF_SCIP_ERROR(
      SCIPcreateConsBasicQuadratic(scip,
                                   /*cons=*/scip_cst,
                                   /*name=*/gen_cst.name().c_str(),
                                   /*nlinvars=*/lsize,
                                   /*linvars=*/tmp_variables->data(),
                                   /*lincoefs=*/tmp_coefficients->data(),
                                   /*nquadterms=*/qsize,
                                   /*quadvars1=*/tmp_qvariables1->data(),
                                   /*quadvars2=*/tmp_qvariables2->data(),
                                   /*quadcoefs=*/tmp_qcoefficients->data(),
                                   /*lhs=*/quad_cst.lower_bound(),
                                   /*rhs=*/quad_cst.upper_bound()));
  RETURN_IF_SCIP_ERROR(SCIPaddCons(scip, *scip_cst));
  return util::OkStatus();
}

// Models the constraint y = |x| as y >= 0 plus one disjunction constraint:
//   y = x OR y = -x
util::Status AddAbsConstraint(const MPGeneralConstraintProto& gen_cst,
                              const std::vector<SCIP_VAR*>& scip_variables,
                              SCIP* scip, SCIP_CONS** scip_cst) {
  CHECK(scip != nullptr);
  CHECK(scip_cst != nullptr);
  CHECK(gen_cst.has_abs_constraint());
  const auto& abs = gen_cst.abs_constraint();
  SCIP_VAR* scip_var = scip_variables[abs.var_index()];
  SCIP_VAR* scip_resultant_var = scip_variables[abs.resultant_var_index()];

  // Set the resultant variable's lower bound to zero if it's negative.
  if (SCIPvarGetLbLocal(scip_resultant_var) < 0.0) {
    RETURN_IF_SCIP_ERROR(SCIPchgVarLb(scip, scip_resultant_var, 0.0));
  }

  std::vector<SCIP_VAR*> vars;
  std::vector<double> vals;
  std::vector<SCIP_CONS*> cons;
  auto add_abs_constraint =
      [&](const std::string& name_prefix) -> util::Status {
    SCIP_CONS* scip_cons = nullptr;
    CHECK(vars.size() == vals.size());
    const std::string name =
        gen_cst.has_name() ? absl::StrCat(gen_cst.name(), name_prefix) : "";
    RETURN_IF_SCIP_ERROR(SCIPcreateConsBasicLinear(
        scip, /*cons=*/&scip_cons,
        /*name=*/name.c_str(), /*nvars=*/vars.size(), /*vars=*/vars.data(),
        /*vals=*/vals.data(), /*lhs=*/0.0, /*rhs=*/0.0));
    // Note that the constraints are, by design, not added into the model using
    // SCIPaddCons.
    cons.push_back(scip_cons);
    return util::OkStatus();
  };

  // Create an intermediary constraint such that y = -x
  vars = {scip_resultant_var, scip_var};
  vals = {1, 1};
  RETURN_IF_ERROR(add_abs_constraint("_neg"));

  // Create an intermediary constraint such that y = x
  vals = {1, -1};
  RETURN_IF_ERROR(add_abs_constraint("_pos"));

  // Activate at least one of the two above constraints.
  const std::string name =
      gen_cst.has_name() ? absl::StrCat(gen_cst.name(), "_disj") : "";
  RETURN_IF_SCIP_ERROR(SCIPcreateConsBasicDisjunction(
      scip, /*cons=*/scip_cst, /*name=*/name.c_str(),
      /*nconss=*/cons.size(), /*conss=*/cons.data(), /*relaxcons=*/nullptr));
  RETURN_IF_SCIP_ERROR(SCIPaddCons(scip, *scip_cst));

  return util::OkStatus();
}

util::Status AddAndConstraint(const MPGeneralConstraintProto& gen_cst,
                              const std::vector<SCIP_VAR*>& scip_variables,
                              SCIP* scip, SCIP_CONS** scip_cst,
                              std::vector<SCIP_VAR*>* tmp_variables) {
  CHECK(scip != nullptr);
  CHECK(scip_cst != nullptr);
  CHECK(tmp_variables != nullptr);
  CHECK(gen_cst.has_and_constraint());
  const auto& andcst = gen_cst.and_constraint();

  tmp_variables->resize(andcst.var_index_size(), nullptr);
  for (int i = 0; i < andcst.var_index_size(); ++i) {
    (*tmp_variables)[i] = scip_variables[andcst.var_index(i)];
  }
  RETURN_IF_SCIP_ERROR(SCIPcreateConsBasicAnd(
      scip, /*cons=*/scip_cst,
      /*name=*/gen_cst.name().c_str(),
      /*resvar=*/scip_variables[andcst.resultant_var_index()],
      /*nvars=*/andcst.var_index_size(),
      /*vars=*/tmp_variables->data()));
  RETURN_IF_SCIP_ERROR(SCIPaddCons(scip, *scip_cst));
  return util::OkStatus();
}

util::Status AddOrConstraint(const MPGeneralConstraintProto& gen_cst,
                             const std::vector<SCIP_VAR*>& scip_variables,
                             SCIP* scip, SCIP_CONS** scip_cst,
                             std::vector<SCIP_VAR*>* tmp_variables) {
  CHECK(scip != nullptr);
  CHECK(scip_cst != nullptr);
  CHECK(tmp_variables != nullptr);
  CHECK(gen_cst.has_or_constraint());
  const auto& orcst = gen_cst.or_constraint();

  tmp_variables->resize(orcst.var_index_size(), nullptr);
  for (int i = 0; i < orcst.var_index_size(); ++i) {
    (*tmp_variables)[i] = scip_variables[orcst.var_index(i)];
  }
  RETURN_IF_SCIP_ERROR(SCIPcreateConsBasicOr(
      scip, /*cons=*/scip_cst,
      /*name=*/gen_cst.name().c_str(),
      /*resvar=*/scip_variables[orcst.resultant_var_index()],
      /*nvars=*/orcst.var_index_size(),
      /*vars=*/tmp_variables->data()));
  RETURN_IF_SCIP_ERROR(SCIPaddCons(scip, *scip_cst));
  return util::OkStatus();
}

// Models the constraint y = min(x1, x2, ... xn, c) with c being a constant with
//  - n + 1 constraints to ensure y <= min(x1, x2, ... xn, c)
//  - one disjunction constraint among all of the possible y = x1, y = x2, ...
//    y = xn, y = c constraints
// Does the equivalent thing for max (with y >= max(...) instead).
util::Status AddMinMaxConstraint(const MPGeneralConstraintProto& gen_cst,
                                 const std::vector<SCIP_VAR*>& scip_variables,
                                 SCIP* scip, SCIP_CONS** scip_cst,
                                 std::vector<SCIP_CONS*>* scip_constraints,
                                 std::vector<SCIP_VAR*>* tmp_variables) {
  CHECK(scip != nullptr);
  CHECK(scip_cst != nullptr);
  CHECK(tmp_variables != nullptr);
  CHECK(gen_cst.has_min_constraint() || gen_cst.has_max_constraint());
  const auto& minmax = gen_cst.has_min_constraint() ? gen_cst.min_constraint()
                                                    : gen_cst.max_constraint();
  SCIP_VAR* scip_resultant_var = scip_variables[minmax.resultant_var_index()];

  std::vector<SCIP_VAR*> vars;
  std::vector<double> vals;
  std::vector<SCIP_CONS*> cons;
  auto add_lin_constraint = [&](const std::string& name_prefix,
                                double lower_bound = 0.0,
                                double upper_bound = 0.0) -> util::Status {
    SCIP_CONS* scip_cons = nullptr;
    CHECK(vars.size() == vals.size());
    const std::string name =
        gen_cst.has_name() ? absl::StrCat(gen_cst.name(), name_prefix) : "";
    RETURN_IF_SCIP_ERROR(SCIPcreateConsBasicLinear(
        scip, /*cons=*/&scip_cons,
        /*name=*/name.c_str(), /*nvars=*/vars.size(), /*vars=*/vars.data(),
        /*vals=*/vals.data(), /*lhs=*/lower_bound, /*rhs=*/upper_bound));
    // Note that the constraints are, by design, not added into the model using
    // SCIPaddCons.
    cons.push_back(scip_cons);
    return util::OkStatus();
  };

  // Create intermediary constraints such that y = xi
  for (const int var_index : minmax.var_index()) {
    vars = {scip_resultant_var, scip_variables[var_index]};
    vals = {1, -1};
    RETURN_IF_ERROR(add_lin_constraint(absl::StrCat("_", var_index)));
  }

  // Create an intermediary constraint such that y = c
  if (minmax.has_constant()) {
    vars = {scip_resultant_var};
    vals = {1};
    RETURN_IF_ERROR(
        add_lin_constraint("_constant", minmax.constant(), minmax.constant()));
  }

  // Activate at least one of the above constraints.
  const std::string name =
      gen_cst.has_name() ? absl::StrCat(gen_cst.name(), "_disj") : "";
  RETURN_IF_SCIP_ERROR(SCIPcreateConsBasicDisjunction(
      scip, /*cons=*/scip_cst, /*name=*/name.c_str(),
      /*nconss=*/cons.size(), /*conss=*/cons.data(), /*relaxcons=*/nullptr));
  RETURN_IF_SCIP_ERROR(SCIPaddCons(scip, *scip_cst));

  // Add all of the inequality constraints.
  constexpr double kInfinity = std::numeric_limits<double>::infinity();
  cons.clear();
  for (const int var_index : minmax.var_index()) {
    vars = {scip_resultant_var, scip_variables[var_index]};
    vals = {1, -1};
    if (gen_cst.has_min_constraint()) {
      RETURN_IF_ERROR(add_lin_constraint(absl::StrCat("_ineq_", var_index),
                                         -kInfinity, 0.0));
    } else {
      RETURN_IF_ERROR(add_lin_constraint(absl::StrCat("_ineq_", var_index), 0.0,
                                         kInfinity));
    }
  }
  if (minmax.has_constant()) {
    vars = {scip_resultant_var};
    vals = {1};
    if (gen_cst.has_min_constraint()) {
      RETURN_IF_ERROR(add_lin_constraint(absl::StrCat("_ineq_constant"),
                                         -kInfinity, minmax.constant()));
    } else {
      RETURN_IF_ERROR(add_lin_constraint(absl::StrCat("_ineq_constant"),
                                         minmax.constant(), kInfinity));
    }
  }
  for (SCIP_CONS* scip_cons : cons) {
    scip_constraints->push_back(scip_cons);
    RETURN_IF_SCIP_ERROR(SCIPaddCons(scip, scip_cons));
  }
  return util::OkStatus();
}

util::Status AddQuadraticObjective(const MPQuadraticObjective& quadobj,
                                   SCIP* scip,
                                   std::vector<SCIP_VAR*>* scip_variables,
                                   std::vector<SCIP_CONS*>* scip_constraints) {
  CHECK(scip != nullptr);
  CHECK(scip_variables != nullptr);
  CHECK(scip_constraints != nullptr);

  constexpr double kInfinity = std::numeric_limits<double>::infinity();

  const int size = quadobj.coefficient_size();
  if (size == 0) return util::OkStatus();

  // SCIP supports quadratic objectives by adding a quadratic constraint. We
  // need to create an extra variable to hold this quadratic objective.
  scip_variables->push_back(nullptr);
  RETURN_IF_SCIP_ERROR(SCIPcreateVarBasic(scip, /*var=*/&scip_variables->back(),
                                          /*name=*/"quadobj",
                                          /*lb=*/-kInfinity, /*ub=*/kInfinity,
                                          /*obj=*/1,
                                          /*vartype=*/SCIP_VARTYPE_CONTINUOUS));
  RETURN_IF_SCIP_ERROR(SCIPaddVar(scip, scip_variables->back()));

  scip_constraints->push_back(nullptr);
  SCIP_VAR* linvars[1] = {scip_variables->back()};
  double lincoefs[1] = {-1};
  std::vector<SCIP_VAR*> quadvars1(size, nullptr);
  std::vector<SCIP_VAR*> quadvars2(size, nullptr);
  std::vector<double> quadcoefs(size, 0);
  for (int i = 0; i < size; ++i) {
    quadvars1[i] = scip_variables->at(quadobj.qvar1_index(i));
    quadvars2[i] = scip_variables->at(quadobj.qvar2_index(i));
    quadcoefs[i] = quadobj.coefficient(i);
  }
  RETURN_IF_SCIP_ERROR(SCIPcreateConsBasicQuadratic(
      scip, /*cons=*/&scip_constraints->back(), /*name=*/"quadobj",
      /*nlinvars=*/1, /*linvars=*/linvars, /*lincoefs=*/lincoefs,
      /*nquadterms=*/size, /*quadvars1=*/quadvars1.data(),
      /*quadvars2=*/quadvars2.data(), /*quadcoefs=*/quadcoefs.data(),
      /*lhs=*/0, /*rhs=*/0));
  RETURN_IF_SCIP_ERROR(SCIPaddCons(scip, scip_constraints->back()));

  return util::OkStatus();
}

util::Status AddSolutionHint(const MPModelProto& model, SCIP* scip,
                             const std::vector<SCIP_VAR*>& scip_variables) {
  CHECK(scip != nullptr);
  if (!model.has_solution_hint()) return util::OkStatus();

  const PartialVariableAssignment& solution_hint = model.solution_hint();
  SCIP_SOL* solution;
  bool is_solution_partial =
      solution_hint.var_index_size() != model.variable_size();
  if (is_solution_partial) {
    RETURN_IF_SCIP_ERROR(
        SCIPcreatePartialSol(scip, /*sol=*/&solution, /*heur=*/nullptr));
  } else {
    RETURN_IF_SCIP_ERROR(
        SCIPcreateSol(scip, /*sol=*/&solution, /*heur=*/nullptr));
  }

  for (int i = 0; i < solution_hint.var_index_size(); ++i) {
    RETURN_IF_SCIP_ERROR(SCIPsetSolVal(
        scip, solution, scip_variables[solution_hint.var_index(i)],
        solution_hint.var_value(i)));
  }

  SCIP_Bool is_stored;
  RETURN_IF_SCIP_ERROR(SCIPaddSolFree(scip, &solution, &is_stored));

  return util::OkStatus();
}

bool MPModelIsInvalidForScip(const MPModelProto& model, SCIP* scip,
                             MPSolutionResponse* response) {
  CHECK(scip != nullptr);
  CHECK(response != nullptr);

  const double infinity = SCIPinfinity(scip);
  for (int v = 0; v < model.variable_size(); ++v) {
    const MPVariableProto& variable = model.variable(v);
    if (variable.lower_bound() >= infinity) {
      response->set_status(MPSOLVER_MODEL_INVALID);
      response->set_status_str(absl::StrFormat(
          "Variable %i's lower bound is considered +infinity", v));
      return true;
    }
    if (variable.upper_bound() <= -infinity) {
      response->set_status(MPSOLVER_MODEL_INVALID);
      response->set_status_str(absl::StrFormat(
          "Variable %i's lower bound is considered -infinity", v));
      return true;
    }
    const double coeff = variable.objective_coefficient();
    if (coeff >= infinity || coeff <= -infinity) {
      response->set_status(MPSOLVER_MODEL_INVALID);
      response->set_status_str(absl::StrFormat(
          "Variable %i's objective coefficient is considered infinite", v));
      return true;
    }
  }

  if (model.has_solution_hint()) {
    for (int i = 0; i < model.solution_hint().var_value_size(); ++i) {
      const double value = model.solution_hint().var_value(i);
      if (value >= infinity || value <= -infinity) {
        response->set_status(MPSOLVER_MODEL_INVALID);
        response->set_status_str(absl::StrFormat(
            "Variable %i's solution hint is considered infinite",
            model.solution_hint().var_index(i)));
        return true;
      }
    }
  }

  if (model.objective_offset() >= infinity ||
      model.objective_offset() <= -infinity) {
    response->set_status(MPSOLVER_MODEL_INVALID);
    response->set_status_str(
        "Model's objective offset is considered infinite.");
    return true;
  }

  return false;
}
}  // namespace

util::StatusOr<MPSolutionResponse> ScipSolveProto(
    const MPModelRequest& request) {
  MPSolutionResponse response;
  const absl::optional<LazyMutableCopy<MPModelProto>> optional_model =
      ExtractValidMPModelOrPopulateResponseStatus(request, &response);
  if (!optional_model) return response;
  const MPModelProto& model = optional_model->get();
  SCIP* scip = nullptr;
  std::vector<SCIP_VAR*> scip_variables(model.variable_size(), nullptr);
  std::vector<SCIP_CONS*> scip_constraints(
      model.constraint_size() + model.general_constraint_size(), nullptr);

  auto delete_scip_objects = [&]() -> util::Status {
    // Release all created pointers.
    if (scip == nullptr) return util::OkStatus();
    for (SCIP_VAR* variable : scip_variables) {
      if (variable != nullptr) {
        RETURN_IF_SCIP_ERROR(SCIPreleaseVar(scip, &variable));
      }
    }
    for (SCIP_CONS* constraint : scip_constraints) {
      if (constraint != nullptr) {
        RETURN_IF_SCIP_ERROR(SCIPreleaseCons(scip, &constraint));
      }
    }
    RETURN_IF_SCIP_ERROR(SCIPfree(&scip));
    return util::OkStatus();
  };

  auto scip_deleter = gtl::MakeCleanup([delete_scip_objects]() {
    const util::Status deleter_status = delete_scip_objects();
    LOG_IF(DFATAL, !deleter_status.ok()) << deleter_status;
  });

  RETURN_IF_SCIP_ERROR(SCIPcreate(&scip));
  RETURN_IF_SCIP_ERROR(SCIPincludeDefaultPlugins(scip));
  if (MPModelIsInvalidForScip(model, scip, &response)) return response;

  const auto parameters_status = ScipSetSolverSpecificParameters(
      request.solver_specific_parameters(), scip);
  if (!parameters_status.ok()) {
    response.set_status(MPSOLVER_MODEL_INVALID_SOLVER_PARAMETERS);
    response.set_status_str(parameters_status.error_message());
    return response;
  }
  // Default clock type. We use wall clock time because getting CPU user seconds
  // involves calling times() which is very expensive.
  // NOTE(user): Also, time limit based on CPU user seconds is *NOT* thread
  // safe. We observed that different instances of SCIP running concurrently
  // in different threads consume the time limit *together*. E.g., 2 threads
  // running SCIP with time limit 10s each will both terminate after ~5s.
  RETURN_IF_SCIP_ERROR(
      SCIPsetIntParam(scip, "timing/clocktype", SCIP_CLOCKTYPE_WALL));
  if (request.solver_time_limit_seconds() > 0 &&
      request.solver_time_limit_seconds() < 1e20) {
    RETURN_IF_SCIP_ERROR(SCIPsetRealParam(scip, "limits/time",
                                          request.solver_time_limit_seconds()));
  }
  SCIPsetMessagehdlrQuiet(scip, !request.enable_internal_solver_output());

  RETURN_IF_SCIP_ERROR(SCIPcreateProbBasic(scip, model.name().c_str()));
  if (model.maximize()) {
    RETURN_IF_SCIP_ERROR(SCIPsetObjsense(scip, SCIP_OBJSENSE_MAXIMIZE));
  }

  for (int v = 0; v < model.variable_size(); ++v) {
    const MPVariableProto& variable = model.variable(v);
    RETURN_IF_SCIP_ERROR(SCIPcreateVarBasic(
        scip, /*var=*/&scip_variables[v], /*name=*/variable.name().c_str(),
        /*lb=*/variable.lower_bound(), /*ub=*/variable.upper_bound(),
        /*obj=*/variable.objective_coefficient(),
        /*vartype=*/variable.is_integer() ? SCIP_VARTYPE_INTEGER
                                          : SCIP_VARTYPE_CONTINUOUS));
    RETURN_IF_SCIP_ERROR(SCIPaddVar(scip, scip_variables[v]));
  }

  {
    std::vector<SCIP_VAR*> ct_variables;
    std::vector<double> ct_coefficients;
    for (int c = 0; c < model.constraint_size(); ++c) {
      const MPConstraintProto& constraint = model.constraint(c);
      const int size = constraint.var_index_size();
      ct_variables.resize(size, nullptr);
      ct_coefficients.resize(size, 0);
      for (int i = 0; i < size; ++i) {
        ct_variables[i] = scip_variables[constraint.var_index(i)];
        ct_coefficients[i] = constraint.coefficient(i);
      }
      RETURN_IF_SCIP_ERROR(SCIPcreateConsLinear(
          scip, /*cons=*/&scip_constraints[c],
          /*name=*/constraint.name().c_str(),
          /*nvars=*/constraint.var_index_size(), /*vars=*/ct_variables.data(),
          /*vals=*/ct_coefficients.data(),
          /*lhs=*/constraint.lower_bound(), /*rhs=*/constraint.upper_bound(),
          /*initial=*/!constraint.is_lazy(),
          /*separate=*/true,
          /*enforce=*/true,
          /*check=*/true,
          /*propagate=*/true,
          /*local=*/false,
          /*modifiable=*/false,
          /*dynamic=*/false,
          /*removable=*/constraint.is_lazy(),
          /*stickingatnode=*/false));
      RETURN_IF_SCIP_ERROR(SCIPaddCons(scip, scip_constraints[c]));
    }

    // These extra arrays are used by quadratic constraints.
    std::vector<SCIP_VAR*> ct_qvariables1;
    std::vector<SCIP_VAR*> ct_qvariables2;
    std::vector<double> ct_qcoefficients;
    const int lincst_size = model.constraint_size();
    for (int c = 0; c < model.general_constraint_size(); ++c) {
      const MPGeneralConstraintProto& gen_cst = model.general_constraint(c);
      switch (gen_cst.general_constraint_case()) {
        case MPGeneralConstraintProto::kIndicatorConstraint: {
          RETURN_IF_ERROR(AddIndicatorConstraint(
              gen_cst, scip_variables, scip, &scip_constraints[lincst_size + c],
              &scip_constraints, &ct_variables, &ct_coefficients));
          break;
        }
        case MPGeneralConstraintProto::kSosConstraint: {
          RETURN_IF_ERROR(AddSosConstraint(gen_cst, scip_variables, scip,
                                           &scip_constraints[lincst_size + c],
                                           &ct_variables, &ct_coefficients));
          break;
        }
        case MPGeneralConstraintProto::kQuadraticConstraint: {
          RETURN_IF_ERROR(AddQuadraticConstraint(
              gen_cst, scip_variables, scip, &scip_constraints[lincst_size + c],
              &ct_variables, &ct_coefficients, &ct_qvariables1, &ct_qvariables2,
              &ct_qcoefficients));
          break;
        }
        case MPGeneralConstraintProto::kAbsConstraint: {
          RETURN_IF_ERROR(AddAbsConstraint(gen_cst, scip_variables, scip,
                                           &scip_constraints[lincst_size + c]));
          break;
        }
        case MPGeneralConstraintProto::kAndConstraint: {
          RETURN_IF_ERROR(AddAndConstraint(gen_cst, scip_variables, scip,
                                           &scip_constraints[lincst_size + c],
                                           &ct_variables));
          break;
        }
        case MPGeneralConstraintProto::kOrConstraint: {
          RETURN_IF_ERROR(AddOrConstraint(gen_cst, scip_variables, scip,
                                          &scip_constraints[lincst_size + c],
                                          &ct_variables));
          break;
        }
        case MPGeneralConstraintProto::kMinConstraint:
        case MPGeneralConstraintProto::kMaxConstraint: {
          RETURN_IF_ERROR(AddMinMaxConstraint(
              gen_cst, scip_variables, scip, &scip_constraints[lincst_size + c],
              &scip_constraints, &ct_variables));
          break;
        }
        default:
          return util::UnimplementedError(
              absl::StrFormat("General constraints of type %i not supported.",
                              gen_cst.general_constraint_case()));
      }
    }
  }

  if (model.has_quadratic_objective()) {
    RETURN_IF_ERROR(AddQuadraticObjective(model.quadratic_objective(), scip,
                                          &scip_variables, &scip_constraints));
  }
  RETURN_IF_SCIP_ERROR(SCIPaddOrigObjoffset(scip, model.objective_offset()));
  RETURN_IF_ERROR(AddSolutionHint(model, scip, scip_variables));

  RETURN_IF_SCIP_ERROR(SCIPsolve(scip));

  SCIP_SOL* const solution = SCIPgetBestSol(scip);
  if (solution != nullptr) {
    response.set_objective_value(SCIPgetSolOrigObj(scip, solution));
    response.set_best_objective_bound(SCIPgetDualbound(scip));
    for (int v = 0; v < model.variable_size(); ++v) {
      double value = SCIPgetSolVal(scip, solution, scip_variables[v]);
      if (model.variable(v).is_integer()) value = std::round(value);
      response.add_variable_value(value);
    }
  }

  const SCIP_STATUS scip_status = SCIPgetStatus(scip);
  switch (scip_status) {
    case SCIP_STATUS_OPTIMAL:
      response.set_status(MPSOLVER_OPTIMAL);
      break;
    case SCIP_STATUS_GAPLIMIT:
      // To be consistent with the other solvers.
      response.set_status(MPSOLVER_OPTIMAL);
      break;
    case SCIP_STATUS_INFORUNBD:
      // NOTE(user): After looking at the SCIP code on 2019-06-14, it seems
      // that this will mostly happen for INFEASIBLE problems in practice.
      // Since most (all?) users shouldn't have their application behave very
      // differently upon INFEASIBLE or UNBOUNDED, the potential error that we
      // are making here seems reasonable (and not worth a LOG, unless in
      // debug mode).
      DLOG(INFO) << "SCIP solve returned SCIP_STATUS_INFORUNBD, which we treat "
                    "as INFEASIBLE even though it may mean UNBOUNDED.";
      response.set_status_str(
          "The model may actually be unbounded: SCIP returned "
          "SCIP_STATUS_INFORUNBD");
      ABSL_FALLTHROUGH_INTENDED;
    case SCIP_STATUS_INFEASIBLE:
      response.set_status(MPSOLVER_INFEASIBLE);
      break;
    case SCIP_STATUS_UNBOUNDED:
      response.set_status(MPSOLVER_UNBOUNDED);
      break;
    default:
      if (solution != nullptr) {
        response.set_status(MPSOLVER_FEASIBLE);
      } else {
        response.set_status(MPSOLVER_NOT_SOLVED);
        response.set_status_str(absl::StrFormat("SCIP status code %d",
                                                static_cast<int>(scip_status)));
      }
      break;
  }

  return response;
}

}  // namespace operations_research

#endif  //  #if defined(USE_SCIP)
