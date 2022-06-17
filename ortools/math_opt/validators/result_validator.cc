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

#include "ortools/math_opt/validators/result_validator.h"

#include <limits>
#include <string>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/core/model_summary.h"
#include "ortools/math_opt/model_parameters.pb.h"
#include "ortools/math_opt/result.pb.h"
#include "ortools/math_opt/solution.pb.h"
#include "ortools/math_opt/validators/solution_validator.h"
#include "ortools/math_opt/validators/solve_stats_validator.h"
#include "ortools/port/proto_utils.h"

namespace operations_research {
namespace math_opt {
namespace {

bool HasPrimalFeasibleSolution(const SolutionProto& solution) {
  return solution.has_primal_solution() &&
         solution.primal_solution().feasibility_status() ==
             SOLUTION_STATUS_FEASIBLE;
}

bool HasPrimalFeasibleSolution(const SolveResultProto& result) {
  // Assumes first solution is primal feasible if there is any primal solution.
  return !result.solutions().empty() &&
         HasPrimalFeasibleSolution(result.solutions(0));
}

bool HasDualFeasibleSolution(const SolutionProto& solution) {
  return solution.has_dual_solution() &&
         solution.dual_solution().feasibility_status() ==
             SOLUTION_STATUS_FEASIBLE;
}

bool HasDualFeasibleSolution(const SolveResultProto& result) {
  for (const auto& solution : result.solutions()) {
    if (HasDualFeasibleSolution(solution)) {
      return true;
    }
  }
  return false;
}

absl::Status ValidateSolutions(
    const google::protobuf::RepeatedPtrField<SolutionProto>& solutions,
    const ModelSolveParametersProto& parameters,
    const ModelSummary& model_summary) {
  // Validate individual solutions
  for (int i = 0; i < solutions.size(); ++i) {
    RETURN_IF_ERROR(ValidateSolution(solutions[i], parameters, model_summary))
        << "invalid solutions[" << i << "]";
  }

  if (solutions.empty()) return absl::OkStatus();

  // Validate solution order.
  // TODO(b/204457524): check objective ordering when possible.
  bool previous_primal_feasible = HasPrimalFeasibleSolution(solutions[0]);
  bool previous_dual_feasible = HasDualFeasibleSolution(solutions[0]);
  for (int i = 1; i < solutions.size(); ++i) {
    const bool current_primal_feasible =
        HasPrimalFeasibleSolution(solutions[i]);
    const bool current_dual_feasible = HasDualFeasibleSolution(solutions[i]);
    // Primal-feasible solutions must appear first.
    if (current_primal_feasible && !previous_primal_feasible) {
      return absl::InvalidArgumentError(
          "primal solution ordering not satisfied");
    }
    // Dual-feasible solutions must appear first within the groups of
    // primal-feasible and other solutions. Equivalently, a dual-feasible
    // solution must be preceded by a dual-feasible solution, except when we
    // switch from the group of primal-feasible solutions to the group of other
    // solutions.
    if (current_dual_feasible && !previous_dual_feasible) {
      if (!(previous_primal_feasible && !current_primal_feasible)) {
        return absl::InvalidArgumentError(
            "dual solution ordering not satisfied");
      }
    }
    previous_primal_feasible = current_primal_feasible;
    previous_dual_feasible = current_dual_feasible;
  }
  return absl::OkStatus();
}

absl::Status RequireNoPrimalFeasibleSolution(const SolveResultProto& result) {
  if (HasPrimalFeasibleSolution(result)) {
    return absl::InvalidArgumentError(
        "expected no primal feasible solution, but one was returned");
  }

  return absl::OkStatus();
}

absl::Status RequireNoDualFeasibleSolution(const SolveResultProto& result) {
  if (HasDualFeasibleSolution(result)) {
    return absl::InvalidArgumentError(
        "expected no dual feasible solution, but one was returned");
  }

  return absl::OkStatus();
}
}  // namespace

absl::Status ValidateTermination(const TerminationProto& termination) {
  if (termination.reason() == TERMINATION_REASON_UNSPECIFIED) {
    return absl::InvalidArgumentError("termination reason must be specified");
  }
  if (termination.reason() == TERMINATION_REASON_FEASIBLE ||
      termination.reason() == TERMINATION_REASON_NO_SOLUTION_FOUND) {
    if (termination.limit() == LIMIT_UNSPECIFIED) {
      return absl::InvalidArgumentError(
          absl::StrCat("for reason ", ProtoEnumToString(termination.reason()),
                       ", limit must be specified"));
    }
    if (termination.limit() == LIMIT_CUTOFF &&
        termination.reason() == TERMINATION_REASON_FEASIBLE) {
      return absl::InvalidArgumentError(
          "For LIMIT_CUTOFF expected no solutions");
    }
  } else {
    if (termination.limit() != LIMIT_UNSPECIFIED) {
      return absl::InvalidArgumentError(
          absl::StrCat("for reason:", ProtoEnumToString(termination.reason()),
                       ", limit should be unspecified, but was set to: ",
                       ProtoEnumToString(termination.limit())));
    }
  }
  return absl::OkStatus();
}

absl::Status CheckHasPrimalSolution(const SolveResultProto& result) {
  if (!HasPrimalFeasibleSolution(result)) {
    return absl::InvalidArgumentError(
        "primal feasible solution expected, but not found");
  }

  return absl::OkStatus();
}

absl::Status CheckPrimalSolutionAndStatusConsistency(
    const SolveResultProto& result) {
  if (result.solve_stats().problem_status().primal_status() !=
          FEASIBILITY_STATUS_FEASIBLE &&
      HasPrimalFeasibleSolution(result)) {
    return absl::InvalidArgumentError(
        "primal feasibility status is not FEASIBILITY_STATUS_FEASIBLE, but "
        "primal feasible solution is returned.");
  }
  return absl::OkStatus();
}

absl::Status CheckDualSolutionAndStatusConsistency(
    const SolveResultProto& result) {
  if (result.solve_stats().problem_status().dual_status() !=
          FEASIBILITY_STATUS_FEASIBLE &&
      HasDualFeasibleSolution(result)) {
    return absl::InvalidArgumentError(
        "dual feasibility status is not FEASIBILITY_STATUS_FEASIBLE, but "
        "dual feasible solution is returned.");
  }
  return absl::OkStatus();
}

// Assumes ValidateTermination has been called and ValidateProblemStatusProto
// has been called on result.solve_stats.problem_status.
absl::Status ValidateTerminationConsistency(const SolveResultProto& result) {
  const ProblemStatusProto status = result.solve_stats().problem_status();
  switch (result.termination().reason()) {
    case TERMINATION_REASON_OPTIMAL:
      RETURN_IF_ERROR(CheckPrimalStatusIs(status, FEASIBILITY_STATUS_FEASIBLE));
      RETURN_IF_ERROR(CheckDualStatusIs(status, FEASIBILITY_STATUS_FEASIBLE));
      RETURN_IF_ERROR(CheckHasPrimalSolution(result));
      // Dual feasible solution is not required.
      // Primal/dual requirements imply primal/dual solution-status consistency.
      return absl::OkStatus();
    case TERMINATION_REASON_INFEASIBLE:
      RETURN_IF_ERROR(
          CheckPrimalStatusIs(status, FEASIBILITY_STATUS_INFEASIBLE));
      RETURN_IF_ERROR(RequireNoPrimalFeasibleSolution(result));
      // Primal requirements imply primal solution-status consistency.
      // No dual requirements so we check consistency.
      RETURN_IF_ERROR(CheckDualSolutionAndStatusConsistency(result));
      return absl::OkStatus();
    case TERMINATION_REASON_UNBOUNDED:
      RETURN_IF_ERROR(CheckPrimalStatusIs(status, FEASIBILITY_STATUS_FEASIBLE));
      RETURN_IF_ERROR(CheckDualStatusIs(status, FEASIBILITY_STATUS_INFEASIBLE));
      // No primal feasible solution is required.
      RETURN_IF_ERROR(RequireNoDualFeasibleSolution(result));
      // Primal/dual requirements imply primal/dual solution-status consistency.
      return absl::OkStatus();
    case TERMINATION_REASON_INFEASIBLE_OR_UNBOUNDED:
      RETURN_IF_ERROR(
          CheckPrimalStatusIs(status, FEASIBILITY_STATUS_UNDETERMINED));
      RETURN_IF_ERROR(
          CheckDualStatusIs(status, FEASIBILITY_STATUS_INFEASIBLE,
                            /*primal_or_dual_infeasible_also_ok=*/true));
      RETURN_IF_ERROR(RequireNoPrimalFeasibleSolution(result));
      RETURN_IF_ERROR(RequireNoDualFeasibleSolution(result));
      // Primal/dual requirements imply primal/dual solution-status consistency.
      // Note if primal status was not FEASIBILITY_STATUS_UNDETERMINED, then
      // primal_or_dual_infeasible must be false and dual status would be
      // FEASIBILITY_STATUS_INFEASIBLE. Then if primal status was
      // FEASIBILITY_STATUS_INFEASIBLE we would have
      // TERMINATION_REASON_INFEASIBLE and if it was FEASIBILITY_STATUS_FEASIBLE
      // we would have TERMINATION_REASON_UNBOUNDED.
      return absl::OkStatus();
    case TERMINATION_REASON_IMPRECISE:
      // TODO(b/211679884): update when imprecise solutions are added.
      return absl::OkStatus();
    case TERMINATION_REASON_FEASIBLE:
      RETURN_IF_ERROR(CheckPrimalStatusIs(status, FEASIBILITY_STATUS_FEASIBLE));
      RETURN_IF_ERROR(CheckHasPrimalSolution(result));
      RETURN_IF_ERROR(
          CheckDualStatusIsNot(status, FEASIBILITY_STATUS_INFEASIBLE));
      // Primal requirement implies primal solution-status consistency so we
      // only check dual consistency.
      RETURN_IF_ERROR(CheckDualSolutionAndStatusConsistency(result));
      // Note if dual status was FEASIBILITY_STATUS_INFEASIBLE, then we would
      // have TERMINATION_REASON_UNBOUNDED (For MIP this follows tha assumption
      // that every floating point ray can be scaled to be integer).
      return absl::OkStatus();
    case TERMINATION_REASON_NO_SOLUTION_FOUND:
      RETURN_IF_ERROR(RequireNoPrimalFeasibleSolution(result));
      RETURN_IF_ERROR(
          CheckPrimalStatusIsNot(status, FEASIBILITY_STATUS_INFEASIBLE));
      // Primal requirement implies primal solution-status consistency so we
      // only check dual consistency.
      RETURN_IF_ERROR(CheckDualSolutionAndStatusConsistency(result));
      // Note if primal status was FEASIBILITY_STATUS_INFEASIBLE, then we would
      // have TERMINATION_REASON_INFEASIBLE.
      return absl::OkStatus();
    case TERMINATION_REASON_NUMERICAL_ERROR:
    case TERMINATION_REASON_OTHER_ERROR: {
      RETURN_IF_ERROR(
          CheckPrimalStatusIs(status, FEASIBILITY_STATUS_UNDETERMINED));
      RETURN_IF_ERROR(
          CheckDualStatusIs(status, FEASIBILITY_STATUS_UNDETERMINED));
      if (!result.solutions().empty()) {
        return absl::InvalidArgumentError(
            absl::StrCat("termination reason is ",
                         ProtoEnumToString(result.termination().reason()),
                         ", but solutions are available"));
      }
      if (result.solve_stats().problem_status().primal_or_dual_infeasible()) {
        return absl::InvalidArgumentError(absl::StrCat(
            "termination reason is ",
            ProtoEnumToString(result.termination().reason()),
            ", but solve_stats.problem_status.primal_or_dual_infeasible = "
            "true"));
      }
      // Primal/dual requirements imply primal/dual solution-status consistency.
    }
      return absl::OkStatus();
    default:
      LOG(FATAL) << ProtoEnumToString(result.termination().reason())
                 << " not implemented";
  }

  return absl::OkStatus();
}

absl::Status ValidateResult(const SolveResultProto& result,
                            const ModelSolveParametersProto& parameters,
                            const ModelSummary& model_summary) {
  RETURN_IF_ERROR(ValidateTermination(result.termination()));
  RETURN_IF_ERROR(ValidateSolveStats(result.solve_stats()));
  RETURN_IF_ERROR(
      ValidateSolutions(result.solutions(), parameters, model_summary));

  if (result.primal_rays_size() > 0 &&
      result.solve_stats().problem_status().dual_status() ==
          FEASIBILITY_STATUS_FEASIBLE) {
    return absl::InvalidArgumentError(
        "solve_stats.problem_status.dual_status = FEASIBILITY_STATUS_FEASIBLE, "
        "but a primal ray is returned");
  }
  for (int i = 0; i < result.primal_rays_size(); ++i) {
    RETURN_IF_ERROR(ValidatePrimalRay(result.primal_rays(i),
                                      parameters.variable_values_filter(),
                                      model_summary))
        << "Invalid primal_rays[" << i << "]";
  }
  if (result.dual_rays_size() > 0 &&
      result.solve_stats().problem_status().primal_status() ==
          FEASIBILITY_STATUS_FEASIBLE) {
    return absl::InvalidArgumentError(
        "solve_stats.problem_status.primal_status = "
        "FEASIBILITY_STATUS_FEASIBLE, but a dual ray is returned");
  }
  for (int i = 0; i < result.dual_rays_size(); ++i) {
    RETURN_IF_ERROR(
        ValidateDualRay(result.dual_rays(i), parameters, model_summary))
        << "Invalid dual_rays[" << i << "]";
  }

  RETURN_IF_ERROR(ValidateTerminationConsistency(result))
      << "inconsistent termination reason "
      << ProtoEnumToString(result.termination().reason());

  return absl::OkStatus();
}

}  // namespace math_opt
}  // namespace operations_research
