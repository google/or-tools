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

#if defined(USE_GUROBI)

#include "ortools/linear_solver/gurobi_proto_solver.h"

#include <limits>
#include <memory>
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

extern "C" {
#include "gurobi_c.h"
int __stdcall GRBisqp(GRBenv**, const char*, const char*, const char*, int,
                      const char*);
}

namespace operations_research {

namespace {
constexpr int GRB_OK = 0;

inline util::Status GurobiCodeToUtilStatus(int error_code,
                                           const char* source_file,
                                           int source_line,
                                           const char* statement,
                                           GRBenv* const env) {
  if (error_code == GRB_OK) return util::OkStatus();
  return util::InvalidArgumentError(absl::StrFormat(
      "Gurobi error code %d (file '%s', line %d) on '%s': %s", error_code,
      source_file, source_line, statement, GRBgeterrormsg(env)));
}

util::Status CreateEnvironment(GRBenv** gurobi) {
  const char kGurobiEnvErrorMsg[] =
      "Could not load Gurobi environment. Is gurobi correctly installed and "
      "licensed on this machine?";

  if (GRBloadenv(gurobi, nullptr) != 0 || *gurobi == nullptr) {
    return util::FailedPreconditionError(
        absl::StrFormat("%s %s", kGurobiEnvErrorMsg, GRBgeterrormsg(*gurobi)));
  }
  return util::OkStatus();
}
}  // namespace

util::StatusOr<MPSolutionResponse> GurobiSolveProto(
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
  if (model.general_constraint_size() > 0) {
    // TODO(user): Move indicator constraint logic from linear_solver.cc to
    // this file.
    return util::UnimplementedError("General constraints not supported.");
  }
  if (model.has_solution_hint()) {
    // TODO(user): Support solution hints.
    return util::UnimplementedError("Solution hint not supported.");
  }

  GRBenv* gurobi = nullptr;
  GRBmodel* gurobi_model = nullptr;

// `gurobi` references ther GRBenv variable defined above.
#define RETURN_IF_GUROBI_ERROR(x) \
  RETURN_IF_ERROR(GurobiCodeToUtilStatus(x, __FILE__, __LINE__, #x, gurobi));

  auto delete_gurobi_objects = [&]() -> util::Status {
    // Release all created pointers.
    if (gurobi_model) RETURN_IF_GUROBI_ERROR(GRBfreemodel(gurobi_model));
    if (gurobi) GRBfreeenv(gurobi);
    return util::OkStatus();
  };
  auto gurobi_deleter = gtl::MakeCleanup([delete_gurobi_objects]() {
    const util::Status deleter_status = delete_gurobi_objects();
    LOG_IF(DFATAL, !deleter_status.ok()) << deleter_status;
  });

  RETURN_IF_ERROR(CreateEnvironment(&gurobi));
  RETURN_IF_GUROBI_ERROR(GRBnewmodel(gurobi, &gurobi_model,
                                     model.name().c_str(),
                                     /*numvars=*/0,
                                     /*obj=*/nullptr,
                                     /*lb=*/nullptr,
                                     /*ub=*/nullptr,
                                     /*vtype=*/nullptr,
                                     /*varnames=*/nullptr));

  if (request.solver_time_limit_seconds() > 0) {
    RETURN_IF_GUROBI_ERROR(GRBsetdblparam(GRBgetenv(gurobi_model),
                                          GRB_DBL_PAR_TIMELIMIT,
                                          request.solver_time_limit_seconds()));
  }
  RETURN_IF_GUROBI_ERROR(
      GRBsetintparam(GRBgetenv(gurobi_model), GRB_INT_PAR_OUTPUTFLAG,
                     request.enable_internal_solver_output()));

  const int variable_size = model.variable_size();
  bool has_integer_variables = false;
  {
    std::vector<double> obj_coeffs(variable_size, 0);
    std::vector<double> lb(variable_size);
    std::vector<double> ub(variable_size);
    std::vector<char> ctype(variable_size);
    std::vector<const char*> varnames(variable_size);
    for (int v = 0; v < variable_size; ++v) {
      const MPVariableProto& variable = model.variable(v);
      obj_coeffs[v] = variable.objective_coefficient();
      lb[v] = variable.lower_bound();
      ub[v] = variable.upper_bound();
      ctype[v] = variable.is_integer() ? GRB_INTEGER : GRB_CONTINUOUS;
      if (variable.is_integer()) has_integer_variables = true;
      if (!variable.name().empty()) varnames[v] = variable.name().c_str();
    }

    RETURN_IF_GUROBI_ERROR(
        GRBaddvars(gurobi_model, variable_size, 0, nullptr, nullptr, nullptr,
                   /*obj=*/obj_coeffs.data(),
                   /*lb=*/lb.data(), /*ub=*/ub.data(), /*vtype=*/ctype.data(),
                   /*varnames=*/const_cast<char**>(varnames.data())));
  }

  {
    std::vector<int> ct_variables;
    std::vector<double> ct_coefficients;
    for (int c = 0; c < model.constraint_size(); ++c) {
      const MPConstraintProto& constraint = model.constraint(c);
      const int size = constraint.var_index_size();
      ct_variables.resize(size, 0);
      ct_coefficients.resize(size, 0);
      for (int i = 0; i < size; ++i) {
        ct_variables[i] = constraint.var_index(i);
        ct_coefficients[i] = constraint.coefficient(i);
      }
      RETURN_IF_GUROBI_ERROR(GRBaddrangeconstr(
          gurobi_model, /*numnz=*/size, /*cind=*/ct_variables.data(),
          /*cval=*/ct_coefficients.data(), /*lower=*/constraint.lower_bound(),
          /*upper=*/constraint.upper_bound(),
          /*constrname=*/constraint.name().c_str()));
    }
  }

  RETURN_IF_GUROBI_ERROR(GRBsetintattr(gurobi_model, GRB_INT_ATTR_MODELSENSE,
                                       model.maximize() ? -1 : 1));
  RETURN_IF_GUROBI_ERROR(GRBsetdblattr(gurobi_model, GRB_DBL_ATTR_OBJCON,
                                       model.objective_offset()));
  if (model.has_quadratic_objective()) {
    MPQuadraticObjective qobj = model.quadratic_objective();
    if (qobj.coefficient_size() > 0) {
      RETURN_IF_GUROBI_ERROR(
          GRBaddqpterms(gurobi_model, /*numqnz=*/qobj.coefficient_size(),
                        /*qrow=*/qobj.mutable_qvar1_index()->mutable_data(),
                        /*qcol=*/qobj.mutable_qvar2_index()->mutable_data(),
                        /*qval=*/qobj.mutable_coefficient()->mutable_data()));
    }
  }

  RETURN_IF_GUROBI_ERROR(GRBupdatemodel(gurobi_model));
  RETURN_IF_GUROBI_ERROR(GRBoptimize(gurobi_model));

  int optimization_status = 0;
  RETURN_IF_GUROBI_ERROR(
      GRBgetintattr(gurobi_model, GRB_INT_ATTR_STATUS, &optimization_status));
  int solution_count = 0;
  RETURN_IF_GUROBI_ERROR(
      GRBgetintattr(gurobi_model, GRB_INT_ATTR_SOLCOUNT, &solution_count));
  switch (optimization_status) {
    case GRB_OPTIMAL:
      response.set_status(MPSOLVER_OPTIMAL);
      break;
    case GRB_INF_OR_UNBD:
      DLOG(INFO) << "Gurobi solve returned GRB_INF_OR_UNBD, which we treat as "
                    "INFEASIBLE even though it may mean UNBOUNDED.";
      response.set_status_str(
          "The model may actually be unbounded: Gurobi returned "
          "GRB_INF_OR_UNBD");
      ABSL_FALLTHROUGH_INTENDED;
    case GRB_INFEASIBLE:
      response.set_status(MPSOLVER_INFEASIBLE);
      break;
    case GRB_UNBOUNDED:
      response.set_status(MPSOLVER_UNBOUNDED);
      break;
    default: {
      if (solution_count > 0) {
        response.set_status(MPSOLVER_FEASIBLE);
      } else {
        response.set_status(MPSOLVER_NOT_SOLVED);
        response.set_status_str(
            absl::StrFormat("Gurobi status code %d", optimization_status));
      }
      break;
    }
  }

  if (solution_count > 0 && (response.status() == MPSOLVER_FEASIBLE ||
                             response.status() == MPSOLVER_OPTIMAL)) {
    double objective_value = 0;
    RETURN_IF_GUROBI_ERROR(
        GRBgetdblattr(gurobi_model, GRB_DBL_ATTR_OBJVAL, &objective_value));
    response.set_objective_value(objective_value);
    if (has_integer_variables) {
      double best_objective_bound = 0;
      const int error = GRBgetdblattr(gurobi_model, GRB_DBL_ATTR_OBJBOUND,
                                      &best_objective_bound);
      if (response.status() == MPSOLVER_OPTIMAL &&
          error == GRB_ERROR_DATA_NOT_AVAILABLE) {
        // If the presolve deletes all variables, there's no best bound.
        response.set_best_objective_bound(objective_value);
      } else {
        RETURN_IF_GUROBI_ERROR(error);
        response.set_best_objective_bound(best_objective_bound);
      }
    }

    response.mutable_variable_value()->Resize(variable_size, 0);
    RETURN_IF_GUROBI_ERROR(
        GRBgetdblattrarray(gurobi_model, GRB_DBL_ATTR_X, 0, variable_size,
                           response.mutable_variable_value()->mutable_data()));
    if (!has_integer_variables) {
      response.mutable_dual_value()->Resize(model.constraint_size(), 0);
      RETURN_IF_GUROBI_ERROR(GRBgetdblattrarray(
          gurobi_model, GRB_DBL_ATTR_PI, 0, model.constraint_size(),
          response.mutable_dual_value()->mutable_data()));
    }
  }
#undef RETURN_IF_GUROBI_ERROR

  return response;
}

}  // namespace operations_research

#endif  //  #if defined(USE_GUROBI)
