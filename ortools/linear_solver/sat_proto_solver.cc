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

#include "ortools/linear_solver/sat_proto_solver.h"

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
#include "ortools/util/time_limit.h"

namespace operations_research {

namespace {

#if defined(PROTOBUF_INTERNAL_IMPL)
using google::protobuf::Message;
#else
using google::protobuf::Message;
#endif

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
    MPModelRequest request, std::atomic<bool>* interrupt_solve) {
  // By default, we use 8 threads as it allows to try a good set of orthogonal
  // parameters. This can be overridden by the user.
  sat::SatParameters params;
  params.set_num_search_workers(8);
  params.set_log_search_progress(request.enable_internal_solver_output());
  if (request.has_solver_specific_parameters()) {
    // If code is compiled with proto-lite runtime, `solver_specific_parameters`
    // should be encoded as non-human readable string from `SerializeAsString`.
    if (!std::is_base_of<Message, sat::SatParameters>::value) {
      CHECK(params.MergeFromString(request.solver_specific_parameters()));
    } else {
      ProtobufTextFormatMergeFromString(request.solver_specific_parameters(),
                                        &params);
    }
  }
  if (request.has_solver_time_limit_seconds()) {
    params.set_max_time_in_seconds(
        static_cast<double>(request.solver_time_limit_seconds()) / 1000.0);
  }

  MPSolutionResponse response;
  if (!ExtractValidMPModelInPlaceOrPopulateResponseStatus(&request,
                                                          &response)) {
    if (params.log_search_progress()) {
      // This is needed for our benchmark scripts.
      sat::CpSolverResponse cp_response;
      cp_response.set_status(sat::CpSolverStatus::MODEL_INVALID);
      LOG(INFO) << CpSolverResponseStats(cp_response);
    }
    return response;
  }

  // Note(user): the LP presolvers API is a bit weird and keep a reference to
  // the given GlopParameters, so we need to make sure it outlive them.
  const glop::GlopParameters glop_params;
  MPModelProto* const mp_model = request.mutable_model();
  std::vector<std::unique_ptr<glop::Preprocessor>> for_postsolve;
  const bool log_info = VLOG_IS_ON(1) || params.log_search_progress();
  const auto status =
      ApplyMipPresolveSteps(log_info, glop_params, mp_model, &for_postsolve);
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

  // We need to do that before the automatic detection of integers.
  RemoveNearZeroTerms(params, mp_model);

  const int num_variables = mp_model->variable_size();
  std::vector<double> var_scaling(num_variables, 1.0);
  if (params.mip_automatically_scale_variables()) {
    var_scaling = sat::DetectImpliedIntegers(log_info, mp_model);
  }
  if (params.mip_var_scaling() != 1.0) {
    const std::vector<double> other_scaling = sat::ScaleContinuousVariables(
        params.mip_var_scaling(), params.mip_max_bound(), mp_model);
    for (int i = 0; i < var_scaling.size(); ++i) {
      var_scaling[i] *= other_scaling[i];
    }
  }

  sat::CpModelProto cp_model;
  if (!ConvertMPModelProtoToCpModelProto(params, *mp_model, &cp_model)) {
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
      cp_model_hint->add_vars(var);
      cp_model_hint->add_values(static_cast<int64>(std::round(
          (request.model().solution_hint().var_value(i)) * var_scaling[var])));
    }
  }

  // We no longer need the request. Reclaim its memory.
  const int old_num_variables = mp_model->variable().size();
  const int old_num_constraints = mp_model->constraint().size();
  request.Clear();

  // Solve.
  sat::Model sat_model;
  sat_model.Add(NewSatParameters(params));
  if (interrupt_solve != nullptr) {
    sat_model.GetOrCreate<TimeLimit>()->RegisterExternalBooleanAsLimit(
        interrupt_solve);
  }
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

    // Postsolve the bound shift and scaling.
    glop::ProblemSolution solution((glop::RowIndex(old_num_constraints)),
                                   (glop::ColIndex(old_num_variables)));
    for (int v = 0; v < solution.primal_values.size(); ++v) {
      solution.primal_values[glop::ColIndex(v)] =
          static_cast<double>(cp_response.solution(v)) / var_scaling[v];
    }
    for (int i = for_postsolve.size(); --i >= 0;) {
      for_postsolve[i]->RecoverSolution(&solution);
    }
    for (int v = 0; v < solution.primal_values.size(); ++v) {
      response.add_variable_value(solution.primal_values[glop::ColIndex(v)]);
    }
  }

  return response;
}
}  // namespace operations_research
