// Copyright 2010-2022 Google LLC
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

#include "ortools/math_opt/solvers/pdlp_solver.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/time/time.h"
#include "google/protobuf/duration.pb.h"
#include "ortools/base/logging.h"
#include "ortools/base/protoutil.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/core/inverted_bounds.h"
#include "ortools/math_opt/core/math_opt_proto_utils.h"
#include "ortools/math_opt/core/solve_interrupter.h"
#include "ortools/math_opt/core/solver_interface.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_parameters.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/parameters.pb.h"
#include "ortools/math_opt/result.pb.h"
#include "ortools/math_opt/solution.pb.h"
#include "ortools/math_opt/solvers/pdlp_bridge.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/validators/callback_validator.h"
#include "ortools/pdlp/iteration_stats.h"
#include "ortools/pdlp/primal_dual_hybrid_gradient.h"
#include "ortools/pdlp/quadratic_program.h"
#include "ortools/pdlp/solve_log.pb.h"
#include "ortools/pdlp/solvers.pb.h"
#include "ortools/port/proto_utils.h"

namespace operations_research {
namespace math_opt {

using pdlp::PrimalDualHybridGradientParams;
using pdlp::SolverResult;

absl::StatusOr<std::unique_ptr<SolverInterface>> PdlpSolver::New(
    const ModelProto& model, const InitArgs& init_args) {
  auto result = absl::WrapUnique(new PdlpSolver);
  ASSIGN_OR_RETURN(result->pdlp_bridge_, PdlpBridge::FromProto(model));
  return result;
}

absl::StatusOr<PrimalDualHybridGradientParams> PdlpSolver::MergeParameters(
    const SolveParametersProto& parameters) {
  PrimalDualHybridGradientParams result;
  std::vector<std::string> warnings;
  if (parameters.enable_output()) {
    result.set_verbosity_level(3);
  }
  if (parameters.has_threads()) {
    result.set_num_threads(parameters.threads());
  }
  if (parameters.has_time_limit()) {
    result.mutable_termination_criteria()->set_time_sec_limit(
        absl::ToDoubleSeconds(
            util_time::DecodeGoogleApiProto(parameters.time_limit()).value()));
  }
  if (parameters.has_node_limit()) {
    warnings.push_back("parameter node_limit not supported for PDLP");
  }
  if (parameters.has_cutoff_limit()) {
    warnings.push_back("parameter cutoff_limit not supported for PDLP");
  }
  if (parameters.has_objective_limit()) {
    warnings.push_back("parameter best_objective_limit not supported for PDLP");
  }
  if (parameters.has_best_bound_limit()) {
    warnings.push_back("parameter best_bound_limit not supported for PDLP");
  }
  if (parameters.has_solution_limit()) {
    warnings.push_back("parameter solution_limit not supported for PDLP");
  }
  if (parameters.has_random_seed()) {
    warnings.push_back("parameter random_seed not supported for PDLP");
  }
  if (parameters.lp_algorithm() != LP_ALGORITHM_UNSPECIFIED) {
    warnings.push_back("parameter lp_algorithm not supported for PDLP");
  }
  if (parameters.presolve() != EMPHASIS_UNSPECIFIED) {
    warnings.push_back("parameter presolve not supported for PDLP");
  }
  if (parameters.cuts() != EMPHASIS_UNSPECIFIED) {
    warnings.push_back("parameter cuts not supported for PDLP");
  }
  if (parameters.heuristics() != EMPHASIS_UNSPECIFIED) {
    warnings.push_back("parameter heuristics not supported for PDLP");
  }
  if (parameters.scaling() != EMPHASIS_UNSPECIFIED) {
    warnings.push_back("parameter scaling not supported for PDLP");
  }
  if (parameters.has_iteration_limit()) {
    const int64_t limit = std::min<int64_t>(std::numeric_limits<int32_t>::max(),
                                            parameters.iteration_limit());
    result.mutable_termination_criteria()->set_iteration_limit(
        static_cast<int32_t>(limit));
  }
  result.MergeFrom(parameters.pdlp());
  if (!warnings.empty()) {
    return absl::InvalidArgumentError(absl::StrJoin(warnings, "; "));
  }
  return result;
}

namespace {

absl::StatusOr<TerminationProto> ConvertReason(
    const pdlp::TerminationReason pdlp_reason, const std::string& pdlp_detail) {
  switch (pdlp_reason) {
    case pdlp::TERMINATION_REASON_UNSPECIFIED:
      return TerminateForReason(TERMINATION_REASON_UNSPECIFIED, pdlp_detail);
    case pdlp::TERMINATION_REASON_OPTIMAL:
      return TerminateForReason(TERMINATION_REASON_OPTIMAL, pdlp_detail);
    case pdlp::TERMINATION_REASON_PRIMAL_INFEASIBLE:
      return TerminateForReason(TERMINATION_REASON_INFEASIBLE, pdlp_detail);
    case pdlp::TERMINATION_REASON_DUAL_INFEASIBLE:
      return TerminateForReason(TERMINATION_REASON_INFEASIBLE_OR_UNBOUNDED,
                                pdlp_detail);
    case pdlp::TERMINATION_REASON_TIME_LIMIT:
      return NoSolutionFoundTermination(LIMIT_TIME, pdlp_detail);
    case pdlp::TERMINATION_REASON_ITERATION_LIMIT:
      return NoSolutionFoundTermination(LIMIT_ITERATION, pdlp_detail);
    case pdlp::TERMINATION_REASON_KKT_MATRIX_PASS_LIMIT:
      return NoSolutionFoundTermination(LIMIT_OTHER, pdlp_detail);
    case pdlp::TERMINATION_REASON_NUMERICAL_ERROR:
      return TerminateForReason(TERMINATION_REASON_NUMERICAL_ERROR,
                                pdlp_detail);
    case pdlp::TERMINATION_REASON_INTERRUPTED_BY_USER:
      return NoSolutionFoundTermination(LIMIT_INTERRUPTED, pdlp_detail);
    case pdlp::TERMINATION_REASON_INVALID_PROBLEM:
      // Indicates that the solver detected invalid problem data, e.g.,
      // inconsistent bounds.
      return absl::InternalError(
          absl::StrCat("Invalid problem sent to PDLP solver "
                       "(TERMINATION_REASON_INVALID_PROBLEM): ",
                       pdlp_detail));
      // Indicates that an invalid value for the parameters was detected.
    case pdlp::TERMINATION_REASON_INVALID_PARAMETER:
      return absl::InvalidArgumentError(absl::StrCat(
          "PDLP parameters invalid (TERMINATION_REASON_INVALID_PARAMETER): ",
          pdlp_detail));
    case pdlp::TERMINATION_REASON_OTHER:
      return TerminateForReason(TERMINATION_REASON_OTHER_ERROR, pdlp_detail);
    default:
      LOG(FATAL) << "PDLP status: " << ProtoEnumToString(pdlp_reason)
                 << " not implemented.";
  }
}

ProblemStatusProto GetProblemStatus(const pdlp::TerminationReason pdlp_reason,
                                    const bool has_finite_dual_bound) {
  ProblemStatusProto problem_status;

  switch (pdlp_reason) {
    case pdlp::TERMINATION_REASON_OPTIMAL:
      problem_status.set_primal_status(FEASIBILITY_STATUS_FEASIBLE);
      problem_status.set_dual_status(FEASIBILITY_STATUS_FEASIBLE);
      break;
    case pdlp::TERMINATION_REASON_PRIMAL_INFEASIBLE:
      problem_status.set_primal_status(FEASIBILITY_STATUS_INFEASIBLE);
      problem_status.set_dual_status(FEASIBILITY_STATUS_UNDETERMINED);
      break;
    case pdlp::TERMINATION_REASON_DUAL_INFEASIBLE:
      problem_status.set_primal_status(FEASIBILITY_STATUS_UNDETERMINED);
      problem_status.set_dual_status(FEASIBILITY_STATUS_INFEASIBLE);
      break;
    case pdlp::TERMINATION_REASON_PRIMAL_OR_DUAL_INFEASIBLE:
      problem_status.set_primal_status(FEASIBILITY_STATUS_UNDETERMINED);
      problem_status.set_dual_status(FEASIBILITY_STATUS_UNDETERMINED);
      problem_status.set_primal_or_dual_infeasible(true);
      break;
    default:
      problem_status.set_primal_status(FEASIBILITY_STATUS_UNDETERMINED);
      problem_status.set_dual_status(FEASIBILITY_STATUS_UNDETERMINED);
      break;
  }
  if (has_finite_dual_bound) {
    problem_status.set_dual_status(FEASIBILITY_STATUS_FEASIBLE);
  }
  return problem_status;
}

}  // namespace

absl::StatusOr<SolveResultProto> PdlpSolver::MakeSolveResult(
    const pdlp::SolverResult& pdlp_result,
    const ModelSolveParametersProto& model_params) {
  SolveResultProto result;
  ASSIGN_OR_RETURN(*result.mutable_termination(),
                   ConvertReason(pdlp_result.solve_log.termination_reason(),
                                 pdlp_result.solve_log.termination_string()));
  ASSIGN_OR_RETURN(*result.mutable_solve_stats()->mutable_solve_time(),
                   util_time::EncodeGoogleApiProto(
                       absl::Seconds(pdlp_result.solve_log.solve_time_sec())));
  result.mutable_solve_stats()->set_first_order_iterations(
      pdlp_result.solve_log.iteration_count());
  const std::optional<pdlp::ConvergenceInformation> convergence_information =
      pdlp::GetConvergenceInformation(pdlp_result.solve_log.solution_stats(),
                                      pdlp_result.solve_log.solution_type());

  // TODO(b/195295177): update description after changes to bounds below.
  // Set default infinite primal/dual bounds. PDLP's default is a minimization
  // problem for which the default primal and dual bounds are infinity and
  // -infinity respectively. PDLP provides a scaling factor to flip the signs
  // for maximization problems. Note that PDLP does not consider solutions that
  // are feasible up to the solver's tolerances to update these bounds. PDLP
  // provides a correction function for dual solutions that yields a true dual
  // bound, but does not provide this function for primal solutions.
  const double objective_scaling_factor =
      pdlp_bridge_.pdlp_lp().objective_scaling_factor;
  result.mutable_solve_stats()->set_best_primal_bound(
      objective_scaling_factor * std::numeric_limits<double>::infinity());
  result.mutable_solve_stats()->set_best_dual_bound(
      -objective_scaling_factor * std::numeric_limits<double>::infinity());

  switch (pdlp_result.solve_log.termination_reason()) {
    case pdlp::TERMINATION_REASON_OPTIMAL:
    case pdlp::TERMINATION_REASON_TIME_LIMIT:
    case pdlp::TERMINATION_REASON_ITERATION_LIMIT:
    case pdlp::TERMINATION_REASON_KKT_MATRIX_PASS_LIMIT:
    case pdlp::TERMINATION_REASON_NUMERICAL_ERROR:
    case pdlp::TERMINATION_REASON_INTERRUPTED_BY_USER: {
      SolutionProto* solution_proto = result.add_solutions();
      {
        auto maybe_primal = pdlp_bridge_.PrimalVariablesToProto(
            pdlp_result.primal_solution, model_params.variable_values_filter());
        RETURN_IF_ERROR(maybe_primal.status());
        PrimalSolutionProto* primal_proto =
            solution_proto->mutable_primal_solution();
        primal_proto->set_feasibility_status(SOLUTION_STATUS_UNDETERMINED);
        *primal_proto->mutable_variable_values() = *std::move(maybe_primal);
        // Note: the solution could be primal feasible for termination reasons
        // other than TERMINATION_REASON_OPTIMAL, but in theory, PDLP could
        // also be modified to return the best feasible solution encounered in
        // an early termination run (if any).
        if (pdlp_result.solve_log.termination_reason() ==
            pdlp::TERMINATION_REASON_OPTIMAL) {
          primal_proto->set_feasibility_status(SOLUTION_STATUS_FEASIBLE);
        }
        if (convergence_information.has_value()) {
          primal_proto->set_objective_value(
              convergence_information->primal_objective());
          // TODO(b/195295177): update to return bounds.
          // PDLP does not have a primal objective correction function so we
          // skip the primal bound update.
        }
      }
      {
        auto maybe_dual = pdlp_bridge_.DualVariablesToProto(
            pdlp_result.dual_solution, model_params.dual_values_filter());
        RETURN_IF_ERROR(maybe_dual.status());
        auto maybe_reduced = pdlp_bridge_.ReducedCostsToProto(
            pdlp_result.reduced_costs, model_params.reduced_costs_filter());
        RETURN_IF_ERROR(maybe_reduced.status());
        DualSolutionProto* dual_proto = solution_proto->mutable_dual_solution();
        dual_proto->set_feasibility_status(SOLUTION_STATUS_UNDETERMINED);
        *dual_proto->mutable_dual_values() = *std::move(maybe_dual);
        *dual_proto->mutable_reduced_costs() = *std::move(maybe_reduced);
        // Note: same comment on primal solution status holds here.
        if (pdlp_result.solve_log.termination_reason() ==
            pdlp::TERMINATION_REASON_OPTIMAL) {
          dual_proto->set_feasibility_status(SOLUTION_STATUS_FEASIBLE);
        }
        if (convergence_information.has_value()) {
          const double dual_obj = convergence_information->dual_objective();
          dual_proto->set_objective_value(dual_obj);
          // TODO(b/195295177): update to use dual_obj or corrected_dual_bound.
          // Using PDLP's corrected dual objective to update dual bounds.
          const double corrected_dual_bound =
              convergence_information->corrected_dual_objective();
          result.mutable_solve_stats()->set_best_dual_bound(
              corrected_dual_bound);
        }
      }
      break;
    }
    case pdlp::TERMINATION_REASON_PRIMAL_INFEASIBLE: {
      // NOTE: for primal infeasible problems, PDLP stores the infeasibility
      // certificate (dual ray) in the dual variables and reduced costs.
      auto maybe_dual = pdlp_bridge_.DualVariablesToProto(
          pdlp_result.dual_solution, model_params.dual_values_filter());
      RETURN_IF_ERROR(maybe_dual.status());
      auto maybe_reduced = pdlp_bridge_.ReducedCostsToProto(
          pdlp_result.reduced_costs, model_params.reduced_costs_filter());
      RETURN_IF_ERROR(maybe_reduced.status());
      DualRayProto* dual_ray_proto = result.add_dual_rays();
      *dual_ray_proto->mutable_dual_values() = *std::move(maybe_dual);
      *dual_ray_proto->mutable_reduced_costs() = *std::move(maybe_reduced);
      break;
    }
    case pdlp::TERMINATION_REASON_DUAL_INFEASIBLE: {
      // NOTE: for dual infeasible problems, PDLP stores the infeasibility
      // certificate (primal ray) in the primal variables.
      auto maybe_primal = pdlp_bridge_.PrimalVariablesToProto(
          pdlp_result.primal_solution, model_params.variable_values_filter());
      RETURN_IF_ERROR(maybe_primal.status());
      PrimalRayProto* primal_ray_proto = result.add_primal_rays();
      *primal_ray_proto->mutable_variable_values() = *std::move(maybe_primal);
      break;
    }
    default:
      break;
  }
  *result.mutable_solve_stats()->mutable_problem_status() =
      GetProblemStatus(pdlp_result.solve_log.termination_reason(),
                       std::isfinite(result.solve_stats().best_dual_bound()));
  return result;
}

absl::StatusOr<SolveResultProto> PdlpSolver::Solve(
    const SolveParametersProto& parameters,
    const ModelSolveParametersProto& model_parameters,
    const MessageCallback message_cb,
    const CallbackRegistrationProto& callback_registration, const Callback cb,
    SolveInterrupter* const interrupter) {
  // TODO(b/183502493): Implement message callback when PDLP supports that.
  if (message_cb != nullptr) {
    return absl::InvalidArgumentError(internal::kMessageCallbackNotSupported);
  }

  RETURN_IF_ERROR(CheckRegisteredCallbackEvents(callback_registration,
                                                /*supported_events=*/{}));

  ASSIGN_OR_RETURN(auto pdlp_params, MergeParameters(parameters));

  // PDLP returns `(TERMINATION_REASON_INVALID_PROBLEM): The input problem has
  // inconsistent bounds.` but we want a more detailed error.
  RETURN_IF_ERROR(pdlp_bridge_.ListInvertedBounds().ToStatus());

  std::atomic<bool> interrupt = false;
  const ScopedSolveInterrupterCallback set_interrupt(
      interrupter, [&]() { interrupt = true; });

  const SolverResult pdlp_result =
      PrimalDualHybridGradient(pdlp_bridge_.pdlp_lp(), pdlp_params, &interrupt);
  return MakeSolveResult(pdlp_result, model_parameters);
}

absl::StatusOr<bool> PdlpSolver::Update(const ModelUpdateProto& model_update) {
  return false;
}

MATH_OPT_REGISTER_SOLVER(SOLVER_TYPE_PDLP, PdlpSolver::New);

}  // namespace math_opt
}  // namespace operations_research
