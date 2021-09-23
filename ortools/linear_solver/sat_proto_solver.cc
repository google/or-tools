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

#include "ortools/linear_solver/sat_proto_solver.h"

#include <cstdint>
#include <vector>

#include "absl/status/statusor.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/linear_solver/model_validator.h"
#include "ortools/linear_solver/sat_solver_utils.h"
#include "ortools/port/proto_utils.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/lp_utils.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/logging.h"
#include "ortools/util/time_limit.h"

namespace operations_research {

namespace {

#if defined(PROTOBUF_INTERNAL_IMPL)
using google::protobuf::Message;
#else
using google::protobuf::Message;
#endif

// Proto-lite disables some features of protos (see
// go/abp-libraries/proto2-lite) and messages inherit from MessageLite directly
// instead of inheriting from Message (which is itself a specialization of
// MessageLite).
constexpr bool kProtoLiteSatParameters =
    !std::is_base_of<Message, sat::SatParameters>::value;

MPSolverResponseStatus ToMPSolverResponseStatus(sat::CpSolverStatus status,
                                                bool has_objective) {
  switch (status) {
    case sat::CpSolverStatus::UNKNOWN:
      return MPSOLVER_NOT_SOLVED;
    case sat::CpSolverStatus::MODEL_INVALID:
      return MPSOLVER_MODEL_INVALID;
    case sat::CpSolverStatus::FEASIBLE:
      return has_objective ? MPSOLVER_FEASIBLE : MPSOLVER_OPTIMAL;
    case sat::CpSolverStatus::INFEASIBLE:
      return MPSOLVER_INFEASIBLE;
    case sat::CpSolverStatus::OPTIMAL:
      return MPSOLVER_OPTIMAL;
    default: {
    }
  }
  return MPSOLVER_ABNORMAL;
}
}  // namespace

absl::StatusOr<MPSolutionResponse> SatSolveProto(
    MPModelRequest request, std::atomic<bool>* interrupt_solve,
    std::function<void(const std::string&)> logging_callback,
    std::function<void(const MPSolution&)> solution_callback) {
  sat::SatParameters params;
  params.set_log_search_progress(request.enable_internal_solver_output());
  if (request.has_solver_specific_parameters()) {
    // See EncodeSatParametersAsString() documentation.
    if (kProtoLiteSatParameters) {
      if (!params.MergeFromString(request.solver_specific_parameters())) {
        return absl::InvalidArgumentError(
            "solver_specific_parameters is not a valid binary stream of the "
            "SatParameters proto");
      }
    } else {
      if (!ProtobufTextFormatMergeFromString(
              request.solver_specific_parameters(), &params)) {
        return absl::InvalidArgumentError(
            "solver_specific_parameters is not a valid textual representation "
            "of the SatParameters proto");
      }
    }
  }
  if (request.has_solver_time_limit_seconds()) {
    params.set_max_time_in_seconds(request.solver_time_limit_seconds());
  }
  params.set_linearization_level(2);

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

  MPSolutionResponse response;
  if (!ExtractValidMPModelInPlaceOrPopulateResponseStatus(&request,
                                                          &response)) {
    if (logger.LoggingIsEnabled()) {
      // This is needed for our benchmark scripts.
      sat::CpSolverResponse cp_response;
      cp_response.set_status(sat::CpSolverStatus::MODEL_INVALID);
      SOLVER_LOG(&logger, CpSolverResponseStats(cp_response));
    }
    return response;
  }

  // Note(user): the LP presolvers API is a bit weird and keep a reference to
  // the given GlopParameters, so we need to make sure it outlive them.
  const glop::GlopParameters glop_params;
  MPModelProto* const mp_model = request.mutable_model();
  std::vector<std::unique_ptr<glop::Preprocessor>> for_postsolve;
  if (!params.enumerate_all_solutions()) {
    const auto status =
        ApplyMipPresolveSteps(glop_params, mp_model, &for_postsolve, &logger);
    if (status == MPSolverResponseStatus::MPSOLVER_INFEASIBLE) {
      if (params.log_search_progress()) {
        // This is needed for our benchmark scripts.
        sat::CpSolverResponse cp_response;
        cp_response.set_status(sat::CpSolverStatus::INFEASIBLE);
        LOG(INFO) << CpSolverResponseStats(cp_response);
      }
      response.set_status(MPSolverResponseStatus::MPSOLVER_INFEASIBLE);
      response.set_status_str("Problem proven infeasible during MIP presolve");
      return response;
    }
  }

  // We need to do that before the automatic detection of integers.
  RemoveNearZeroTerms(params, mp_model, &logger);

  SOLVER_LOG(&logger, "");
  SOLVER_LOG(&logger, "Scaling to pure integer problem.");

  const int num_variables = mp_model->variable_size();
  std::vector<double> var_scaling(num_variables, 1.0);
  if (params.mip_automatically_scale_variables()) {
    var_scaling = sat::DetectImpliedIntegers(mp_model, &logger);
  }
  if (params.mip_var_scaling() != 1.0) {
    const std::vector<double> other_scaling = sat::ScaleContinuousVariables(
        params.mip_var_scaling(), params.mip_max_bound(), mp_model);
    for (int i = 0; i < var_scaling.size(); ++i) {
      var_scaling[i] *= other_scaling[i];
    }
  }

  sat::CpModelProto cp_model;
  if (!ConvertMPModelProtoToCpModelProto(params, *mp_model, &cp_model,
                                         &logger)) {
    if (params.log_search_progress()) {
      // This is needed for our benchmark scripts.
      sat::CpSolverResponse cp_response;
      cp_response.set_status(sat::CpSolverStatus::MODEL_INVALID);
      LOG(INFO) << CpSolverResponseStats(cp_response);
    }
    response.set_status(MPSOLVER_MODEL_INVALID);
    response.set_status_str("Failed to convert model into CP-SAT model");
    return response;
  }
  DCHECK_EQ(cp_model.variables().size(), var_scaling.size());
  DCHECK_EQ(cp_model.variables().size(), mp_model->variable().size());

  // Copy and scale the hint if there is one.
  if (request.model().has_solution_hint()) {
    auto* cp_model_hint = cp_model.mutable_solution_hint();
    const int size = request.model().solution_hint().var_index().size();
    for (int i = 0; i < size; ++i) {
      const int var = request.model().solution_hint().var_index(i);
      if (var >= var_scaling.size()) continue;

      // To handle weird hint input values, we cap any large value to +/-
      // mip_max_bound() which is also the min/max value of any variable once
      // scaled.
      double value =
          request.model().solution_hint().var_value(i) * var_scaling[var];
      if (std::abs(value) > params.mip_max_bound()) {
        value = value > 0 ? params.mip_max_bound() : -params.mip_max_bound();
      }

      cp_model_hint->add_vars(var);
      cp_model_hint->add_values(static_cast<int64_t>(std::round(value)));
    }
  }

  // We no longer need the request. Reclaim its memory.
  const int old_num_variables = mp_model->variable().size();
  const int old_num_constraints = mp_model->constraint().size();
  request.Clear();

  // Configure model.
  sat::Model sat_model;
  sat_model.Register<SolverLogger>(&logger);
  sat_model.Add(NewSatParameters(params));
  if (interrupt_solve != nullptr) {
    sat_model.GetOrCreate<TimeLimit>()->RegisterExternalBooleanAsLimit(
        interrupt_solve);
  }

  auto post_solve = [&](const sat::CpSolverResponse& cp_response) {
    MPSolution mp_solution;
    mp_solution.set_objective_value(cp_response.objective_value());
    // Postsolve the bound shift and scaling.
    glop::ProblemSolution glop_solution((glop::RowIndex(old_num_constraints)),
                                        (glop::ColIndex(old_num_variables)));
    for (int v = 0; v < glop_solution.primal_values.size(); ++v) {
      glop_solution.primal_values[glop::ColIndex(v)] =
          static_cast<double>(cp_response.solution(v)) / var_scaling[v];
    }
    for (int i = for_postsolve.size(); --i >= 0;) {
      for_postsolve[i]->RecoverSolution(&glop_solution);
    }
    for (int v = 0; v < glop_solution.primal_values.size(); ++v) {
      mp_solution.add_variable_value(
          glop_solution.primal_values[glop::ColIndex(v)]);
    }
    return mp_solution;
  };

  if (solution_callback != nullptr) {
    sat_model.Add(sat::NewFeasibleSolutionObserver(
        [&](const sat::CpSolverResponse& cp_response) {
          solution_callback(post_solve(cp_response));
        }));
  }

  // Solve.
  const sat::CpSolverResponse cp_response =
      sat::SolveCpModel(cp_model, &sat_model);

  // Convert the response.
  //
  // TODO(user): Implement the row and column status.
  response.set_status(
      ToMPSolverResponseStatus(cp_response.status(), cp_model.has_objective()));
  if (response.status() == MPSOLVER_FEASIBLE ||
      response.status() == MPSOLVER_OPTIMAL) {
    response.set_objective_value(cp_response.objective_value());
    response.set_best_objective_bound(cp_response.best_objective_bound());
    MPSolution post_solved_solution = post_solve(cp_response);
    *response.mutable_variable_value() =
        std::move(*post_solved_solution.mutable_variable_value());
  }

  return response;
}

std::string EncodeSatParametersAsString(const sat::SatParameters& parameters) {
  if (kProtoLiteSatParameters) {
    // Here we use SerializeToString() instead of SerializeAsString() since the
    // later ignores errors and returns an empty string instead (which can be a
    // valid value when no fields are set).
    std::string bytes;
    CHECK(parameters.SerializeToString(&bytes));
    return bytes;
  }

  return parameters.ShortDebugString();
}

}  // namespace operations_research
