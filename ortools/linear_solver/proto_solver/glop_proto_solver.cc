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

#include "ortools/linear_solver/proto_solver/glop_proto_solver.h"

#include <atomic>
#include <cstdlib>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "ortools/base/logging.h"
#include "ortools/glop/lp_solver.h"
#include "ortools/glop/parameters_validation.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/linear_solver/model_validator.h"
#include "ortools/linear_solver/proto_solver/proto_utils.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/proto_utils.h"
#include "ortools/port/proto_utils.h"
#include "ortools/util/lazy_mutable_copy.h"
#include "ortools/util/logging.h"
#include "ortools/util/time_limit.h"

namespace operations_research {

namespace {

MPSolutionResponse ModelInvalidResponse(SolverLogger& logger,
                                        std::string message) {
  SOLVER_LOG(&logger, "Invalid model in glop_solve_proto.\n", message);

  MPSolutionResponse response;
  response.set_status(MPSolverResponseStatus::MPSOLVER_MODEL_INVALID);
  response.set_status_str(message);
  return response;
}

MPSolutionResponse ModelInvalidParametersResponse(SolverLogger& logger,
                                                  std::string message) {
  SOLVER_LOG(&logger, "Invalid parameters in glop_solve_proto.\n", message);

  MPSolutionResponse response;
  response.set_status(
      MPSolverResponseStatus::MPSOLVER_MODEL_INVALID_SOLVER_PARAMETERS);
  response.set_status_str(message);
  return response;
}

MPSolverResponseStatus ToMPSolverResultStatus(glop::ProblemStatus s) {
  switch (s) {
    case glop::ProblemStatus::OPTIMAL:
      return MPSOLVER_OPTIMAL;
    case glop::ProblemStatus::PRIMAL_FEASIBLE:
      return MPSOLVER_FEASIBLE;

    // Note(user): MPSolver does not have the equivalent of
    // INFEASIBLE_OR_UNBOUNDED however UNBOUNDED is almost never relevant in
    // applications, so we decided to report this status as INFEASIBLE since
    // it should almost always be the case. Historically, we where reporting
    // ABNORMAL, but that was more confusing than helpful.
    //
    // TODO(user): We could argue that it is infeasible to find the optimal of
    // an unbounded problem. So it might just be simpler to completely get rid
    // of the MpSolver::UNBOUNDED status that seems to never be used
    // programmatically.
    case glop::ProblemStatus::INFEASIBLE_OR_UNBOUNDED:  // PASS_THROUGH_INTENDED
    case glop::ProblemStatus::PRIMAL_INFEASIBLE:        // PASS_THROUGH_INTENDED
    case glop::ProblemStatus::DUAL_UNBOUNDED:
      return MPSOLVER_INFEASIBLE;

    case glop::ProblemStatus::DUAL_INFEASIBLE:  // PASS_THROUGH_INTENDED
    case glop::ProblemStatus::PRIMAL_UNBOUNDED:
      return MPSOLVER_UNBOUNDED;

    case glop::ProblemStatus::DUAL_FEASIBLE:  // PASS_THROUGH_INTENDED
    case glop::ProblemStatus::INIT:
      return MPSOLVER_NOT_SOLVED;

    case glop::ProblemStatus::ABNORMAL:   // PASS_THROUGH_INTENDED
    case glop::ProblemStatus::IMPRECISE:  // PASS_THROUGH_INTENDED
    case glop::ProblemStatus::INVALID_PROBLEM:
      return MPSOLVER_ABNORMAL;
  }
  LOG(DFATAL) << "Invalid glop::ProblemStatus " << s;
  return MPSOLVER_ABNORMAL;
}

}  // namespace

MPSolutionResponse GlopSolveProto(
    LazyMutableCopy<MPModelRequest> request, std::atomic<bool>* interrupt_solve,
    std::function<void(const std::string&)> logging_callback) {
  glop::GlopParameters params;
  params.set_log_search_progress(request->enable_internal_solver_output());

  // TODO(user): We do not support all the parameters here. In particular the
  // logs before the solver is called will not be appended to the response. Fix
  // that, and remove code duplication for the logger config. One way should be
  // to not touch/configure anything if the logger is already created while
  // calling SolveCpModel() and call a common config function from here or from
  // inside Solve()?
  SolverLogger logger;
  if (logging_callback != nullptr) {
    logger.AddInfoLoggingCallback(logging_callback);
  }
  logger.EnableLogging(params.log_search_progress());
  logger.SetLogToStdOut(params.log_to_stdout());

  // Set it now so that it can be overwritten by the solver specific parameters.
  if (request->has_solver_specific_parameters()) {
    // See EncodeParametersAsString() documentation.
    if (!std::is_base_of<Message, glop::GlopParameters>::value) {
      if (!params.MergeFromString(request->solver_specific_parameters())) {
        return ModelInvalidParametersResponse(
            logger,
            "solver_specific_parameters is not a valid binary stream of the "
            "GLOPParameters proto");
      }
    } else {
      if (!ProtobufTextFormatMergeFromString(
              request->solver_specific_parameters(), &params)) {
        return ModelInvalidParametersResponse(
            logger,
            "solver_specific_parameters is not a valid textual representation "
            "of the GlopParameters proto");
      }
    }
  }
  if (request->has_solver_time_limit_seconds()) {
    params.set_max_time_in_seconds(request->solver_time_limit_seconds());
  }

  {
    const std::string error = glop::ValidateParameters(params);
    if (!error.empty()) {
      return ModelInvalidParametersResponse(
          logger, absl::StrCat("Invalid Glop parameters: ", error));
    }
  }

  MPSolutionResponse response;
  glop::LinearProgram linear_program;

  // Model validation and delta handling.
  {
    std::optional<LazyMutableCopy<MPModelProto>> optional_model =
        GetMPModelOrPopulateResponse(request, &response);
    if (!optional_model) return response;

    const MPModelProto& mp_model = **optional_model;
    if (!mp_model.general_constraint().empty()) {
      return ModelInvalidResponse(logger,
                                  "GLOP does not support general constraints");
    }

    // Convert and clear the request and mp_model as it is no longer needed.
    MPModelProtoToLinearProgram(mp_model, &linear_program);
    std::move(request).dispose();
  }

  glop::LPSolver lp_solver;
  lp_solver.SetParameters(params);

  // TimeLimit and interrupt solve.
  std::unique_ptr<TimeLimit> time_limit =
      TimeLimit::FromParameters(lp_solver.GetParameters());
  if (interrupt_solve != nullptr) {
    if (interrupt_solve->load()) {
      response.set_status(MPSOLVER_CANCELLED_BY_USER);
      response.set_status_str(
          "Solve not started, because the user set the atomic<bool> in "
          "MPSolver::SolveWithProto() to true before solving could "
          "start.");
      return response;
    } else {
      time_limit->RegisterExternalBooleanAsLimit(interrupt_solve);
    }
  }

  // Solve and set response status.
  const glop::ProblemStatus status =
      lp_solver.SolveWithTimeLimit(linear_program, time_limit.get());
  const MPSolverResponseStatus result_status = ToMPSolverResultStatus(status);
  response.set_status(result_status);

  // Fill in solution.
  if (result_status == MPSOLVER_OPTIMAL || result_status == MPSOLVER_FEASIBLE) {
    response.set_objective_value(lp_solver.GetObjectiveValue());

    const int num_vars = linear_program.num_variables().value();
    for (int var_id = 0; var_id < num_vars; ++var_id) {
      const glop::Fractional solution_value =
          lp_solver.variable_values()[glop::ColIndex(var_id)];
      response.add_variable_value(solution_value);

      const glop::Fractional reduced_cost =
          lp_solver.reduced_costs()[glop::ColIndex(var_id)];
      response.add_reduced_cost(reduced_cost);
    }
  }

  if (result_status == MPSOLVER_UNKNOWN_STATUS && interrupt_solve != nullptr &&
      interrupt_solve->load()) {
    response.set_status(MPSOLVER_CANCELLED_BY_USER);
  }

  const int num_constraints = linear_program.num_constraints().value();
  for (int ct_id = 0; ct_id < num_constraints; ++ct_id) {
    const glop::Fractional dual_value =
        lp_solver.dual_values()[glop::RowIndex(ct_id)];
    response.add_dual_value(dual_value);
  }

  return response;
}

std::string GlopSolverVersion() { return glop::LPSolver::GlopVersion(); }

}  // namespace operations_research
