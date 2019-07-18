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

#if defined(SCIP)

#include "ortools/linear_solver/scip_proto_solver.h"

#include <cmath>
#include <limits>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "ortools/base/canonical_errors.h"
#include "ortools/base/cleanup.h"
#include "ortools/base/status.h"
#include "ortools/base/status_macros.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/linear_solver/model_validator.h"
#include "ortools/linear_solver/scip_helper_macros.h"
#include "scip/scip.h"
#include "scip/scipdefplugins.h"
#include "scip/type_cons.h"

namespace operations_research {

namespace {
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
}  // namespace

util::StatusOr<MPSolutionResponse> ScipSolveProto(
    const MPModelRequest& request) {
  MPSolutionResponse response;
  if (MPRequestIsEmptyOrInvalid(request, &response)) {
    return response;
  }

  const MPModelProto& model = request.model();
  if (request.has_solver_specific_parameters()) {
    // TODO(user): Support solver-specific parameters without duplicating
    // all of the write file / read file code in linear_solver.cc.
    return util::UnimplementedError(
        "Solver-specific parameters not supported.");
  }
  if (model.has_solution_hint()) {
    // TODO(user): Support solution hints.
    return util::UnimplementedError("Solution hint not supported.");
  }

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

    const int lincst_size = model.constraint_size();
    for (int c = 0; c < model.general_constraint_size(); ++c) {
      const MPGeneralConstraintProto& gen_cst = model.general_constraint(c);
      // TODO(user): Move indicator constraint logic from linear_solver.cc
      // to this file.
      switch (gen_cst.general_constraint_case()) {
        case MPGeneralConstraintProto::kSosConstraint: {
          RETURN_IF_ERROR(AddSosConstraint(gen_cst, scip_variables, scip,
                                           &scip_constraints[lincst_size + c],
                                           &ct_variables, &ct_coefficients));
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