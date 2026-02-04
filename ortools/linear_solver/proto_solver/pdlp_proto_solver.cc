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

#include "ortools/linear_solver/proto_solver/pdlp_proto_solver.h"

#include <atomic>
#include <optional>
#include <string>
#include <utility>

#include "absl/log/check.h"
#include "absl/status/statusor.h"
#include "ortools/base/status_macros.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/linear_solver/model_validator.h"
#include "ortools/pdlp/iteration_stats.h"
#include "ortools/pdlp/primal_dual_hybrid_gradient.h"
#include "ortools/pdlp/quadratic_program.h"
#include "ortools/pdlp/solve_log.pb.h"
#include "ortools/pdlp/solvers.pb.h"
#include "ortools/port/proto_utils.h"
#include "ortools/util/lazy_mutable_copy.h"

namespace operations_research {

absl::StatusOr<MPSolutionResponse> PdlpSolveProto(
    LazyMutableCopy<MPModelRequest> request, const bool relax_integer_variables,
    const std::atomic<bool>* interrupt_solve) {
  pdlp::PrimalDualHybridGradientParams params;
  if (request->enable_internal_solver_output()) {
    params.set_verbosity_level(3);
  } else {
    params.set_verbosity_level(0);
  }

  MPSolutionResponse response;
  if (!ProtobufTextFormatMergeFromString(request->solver_specific_parameters(),
                                         &params)) {
    response.set_status(
        MPSolverResponseStatus::MPSOLVER_MODEL_INVALID_SOLVER_PARAMETERS);
    return response;
  }
  if (interrupt_solve != nullptr && interrupt_solve->load() == true) {
    response.set_status(MPSolverResponseStatus::MPSOLVER_NOT_SOLVED);
    return response;
  }
  if (request->has_solver_time_limit_seconds()) {
    params.mutable_termination_criteria()->set_time_sec_limit(
        request->solver_time_limit_seconds());
  }

  std::optional<LazyMutableCopy<MPModelProto>> optional_model =
      GetMPModelOrPopulateResponse(request, &response);
  if (!optional_model) return response;

  ASSIGN_OR_RETURN(
      pdlp::QuadraticProgram qp,
      pdlp::QpFromMpModelProto(**optional_model, relax_integer_variables));

  // We can now clear the request and optional_model.
  std::move(request).dispose();
  optional_model.reset();

  const double objective_scaling_factor = qp.objective_scaling_factor;
  pdlp::SolverResult pdhg_result =
      pdlp::PrimalDualHybridGradient(std::move(qp), params, interrupt_solve);

  // PDLP's statuses don't map very cleanly to MPSolver statuses. Do the best
  // we can for now.
  switch (pdhg_result.solve_log.termination_reason()) {
    case pdlp::TERMINATION_REASON_OPTIMAL:
      response.set_status(MPSOLVER_OPTIMAL);
      break;
    case pdlp::TERMINATION_REASON_NUMERICAL_ERROR:
      response.set_status(MPSOLVER_ABNORMAL);
      break;
    case pdlp::TERMINATION_REASON_PRIMAL_INFEASIBLE:
      response.set_status(MPSOLVER_INFEASIBLE);
      break;
    case pdlp::TERMINATION_REASON_INTERRUPTED_BY_USER:
      response.set_status(MPSOLVER_CANCELLED_BY_USER);
      break;
    default:
      response.set_status(MPSOLVER_NOT_SOLVED);
      break;
  }
  if (pdhg_result.solve_log.has_termination_string()) {
    response.set_status_str(pdhg_result.solve_log.termination_string());
  }

  const std::optional<pdlp::ConvergenceInformation> convergence_information =
      pdlp::GetConvergenceInformation(pdhg_result.solve_log.solution_stats(),
                                      pdhg_result.solve_log.solution_type());

  if (convergence_information.has_value()) {
    response.set_objective_value(convergence_information->primal_objective());
  }
  // variable_value and dual_value are supposed to be set iff 'status' is
  // OPTIMAL or FEASIBLE. However, we set them in all cases.

  for (const double v : pdhg_result.primal_solution) {
    response.add_variable_value(v);
  }

  // QpFromMpModelProto converts maximization problems to minimization problems
  // for PDLP by negating the objective and setting objective_scaling_factor to
  // -1. This maintains the same set of primal solutions. Dual solutions need to
  // be negated if objective_scaling_factor is -1.
  for (const double v : pdhg_result.dual_solution) {
    response.add_dual_value(objective_scaling_factor * v);
  }

  for (const double v : pdhg_result.reduced_costs) {
    response.add_reduced_cost(objective_scaling_factor * v);
  }

  response.set_solver_specific_info(pdhg_result.solve_log.SerializeAsString());

  return response;
}

}  // namespace operations_research
