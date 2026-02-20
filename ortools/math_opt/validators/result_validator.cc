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

#include "ortools/math_opt/validators/result_validator.h"

#include <limits>

#include "absl/status/status.h"
#include "google/protobuf/repeated_ptr_field.h"
#include "ortools/base/status_builder.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/core/math_opt_proto_utils.h"
#include "ortools/math_opt/core/model_summary.h"
#include "ortools/math_opt/model_parameters.pb.h"
#include "ortools/math_opt/result.pb.h"
#include "ortools/math_opt/solution.pb.h"
#include "ortools/math_opt/validators/solution_validator.h"
#include "ortools/math_opt/validators/solve_stats_validator.h"
#include "ortools/math_opt/validators/termination_validator.h"

namespace operations_research {
namespace math_opt {
namespace {
constexpr double kInf = std::numeric_limits<double>::infinity();

bool HasPrimalFeasibleSolution(const SolutionProto& solution) {
  return solution.has_primal_solution() &&
         solution.primal_solution().feasibility_status() ==
             SOLUTION_STATUS_FEASIBLE;
}

bool HasPrimalFeasibleSolution(
    const google::protobuf::RepeatedPtrField<SolutionProto>& solutions) {
  // Assumes first solution is primal feasible if there is any primal solution.
  return !solutions.empty() && HasPrimalFeasibleSolution(solutions.at(0));
}

bool HasDualFeasibleSolution(const SolutionProto& solution) {
  return solution.has_dual_solution() &&
         solution.dual_solution().feasibility_status() ==
             SOLUTION_STATUS_FEASIBLE;
}

bool HasDualFeasibleSolution(
    const google::protobuf::RepeatedPtrField<SolutionProto>& solutions) {
  for (const auto& solution : solutions) {
    if (HasDualFeasibleSolution(solution)) {
      return true;
    }
  }
  return false;
}
}  // namespace

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

namespace {
absl::Status RequireNoPrimalFeasibleSolution(const SolveResultProto& result) {
  if (HasPrimalFeasibleSolution(result.solutions())) {
    return absl::InvalidArgumentError(
        "expected no primal feasible solution, but one was returned");
  }

  return absl::OkStatus();
}

bool FirstPrimalObjectiveIsStrictlyBetter(const double first_objective,
                                          const double second_objective,
                                          const bool maximize) {
  if (maximize) {
    return first_objective > second_objective;
  }
  return first_objective < second_objective;
}

bool FirstDualObjectiveIsStrictlyBetter(const double first_objective,
                                        const double second_objective,
                                        const bool maximize) {
  if (maximize) {
    return second_objective > first_objective;
  }
  return second_objective < first_objective;
}

double GetBestPrimalObjective(
    const google::protobuf::RepeatedPtrField<SolutionProto>& solutions,
    const bool maximize) {
  double best_objective = maximize ? -kInf : kInf;
  for (int i = 0; i < solutions.size(); ++i) {
    if (HasPrimalFeasibleSolution(solutions[i])) {
      const double primal_objective =
          solutions[i].primal_solution().objective_value();
      if (FirstPrimalObjectiveIsStrictlyBetter(primal_objective, best_objective,
                                               maximize)) {
        best_objective = primal_objective;
      }
    }
  }
  return best_objective;
}

double GetBestDualObjective(
    const google::protobuf::RepeatedPtrField<SolutionProto>& solutions,
    const bool maximize) {
  double best_objective = maximize ? kInf : -kInf;
  for (int i = 0; i < solutions.size(); ++i) {
    if (HasDualFeasibleSolution(solutions[i])) {
      const DualSolutionProto& dual_solution = solutions[i].dual_solution();
      if (dual_solution.has_objective_value() &&
          FirstDualObjectiveIsStrictlyBetter(dual_solution.objective_value(),
                                             best_objective, maximize)) {
        best_objective = dual_solution.objective_value();
      }
    }
  }
  return best_objective;
}
}  // namespace

absl::Status CheckHasPrimalSolution(const SolveResultProto& result) {
  if (!HasPrimalFeasibleSolution(result.solutions())) {
    return absl::InvalidArgumentError(
        "primal feasible solution expected, but not found");
  }

  return absl::OkStatus();
}

absl::Status CheckPrimalSolutionAndTerminationConsistency(
    const TerminationProto& termination,
    const google::protobuf::RepeatedPtrField<SolutionProto>& solutions,
    const bool maximize) {
  if (!HasPrimalFeasibleSolution(solutions)) {
    return absl::OkStatus();
  }
  if (termination.problem_status().primal_status() !=
      FEASIBILITY_STATUS_FEASIBLE) {
    return absl::InvalidArgumentError(
        "primal feasibility status is not FEASIBILITY_STATUS_FEASIBLE, but "
        "primal feasible solution is returned.");
  }
  const double best_objective = GetBestPrimalObjective(solutions, maximize);
  const double primal_bound = termination.objective_bounds().primal_bound();
  if (FirstPrimalObjectiveIsStrictlyBetter(best_objective, primal_bound,
                                           maximize)) {
    return util::InvalidArgumentErrorBuilder()
           << "best primal feasible solution objective = " << best_objective
           << " is better than primal_bound = " << primal_bound;
  }
  return absl::OkStatus();
}

absl::Status CheckDualSolutionAndStatusConsistency(
    const TerminationProto& termination,
    const google::protobuf::RepeatedPtrField<SolutionProto>& solutions,
    const bool maximize) {
  if (termination.problem_status().dual_status() !=
          FEASIBILITY_STATUS_FEASIBLE &&
      HasDualFeasibleSolution(solutions)) {
    return absl::InvalidArgumentError(
        "dual feasibility status is not FEASIBILITY_STATUS_FEASIBLE, but "
        "dual feasible solution is returned.");
  }
  const double best_objective = GetBestDualObjective(solutions, maximize);
  const double dual_bound = termination.objective_bounds().dual_bound();
  if (FirstDualObjectiveIsStrictlyBetter(best_objective, dual_bound,
                                         maximize)) {
    return util::InvalidArgumentErrorBuilder()
           << "best dual feasible solution objective = " << best_objective
           << " is better than dual_bound = " << dual_bound;
  }
  return absl::OkStatus();
}

namespace {
// TODO(b/290091715): Delete once problem_status and objective bounds are
// removed from solve_stats and their presence is guaranteed in termination.
absl::Status ValidateSolveStatsTerminationEqualities(
    const SolveResultProto& solve_result) {
  const ObjectiveBoundsProto objective_bounds =
      GetObjectiveBounds(solve_result);
  const SolveStatsProto& solve_stats = solve_result.solve_stats();
  const ProblemStatusProto problem_status = GetProblemStatus(solve_result);
  if (problem_status.primal_status() !=
      solve_stats.problem_status().primal_status()) {
    return util::InvalidArgumentErrorBuilder()
           << problem_status.primal_status()
           << " = termination.problem_status.primal_status != "
              "solve_stats.problem_status.primal_status = "
           << solve_stats.problem_status().primal_status();
  }
  if (problem_status.dual_status() !=
      solve_stats.problem_status().dual_status()) {
    return util::InvalidArgumentErrorBuilder()
           << problem_status.dual_status()
           << " = termination.problem_status.dual_status != "
              "solve_stats.problem_status.dual_status = "
           << solve_stats.problem_status().dual_status();
  }
  if (problem_status.primal_or_dual_infeasible() !=
      solve_stats.problem_status().primal_or_dual_infeasible()) {
    return util::InvalidArgumentErrorBuilder()
           << problem_status.primal_or_dual_infeasible()
           << " = termination.problem_status.primal_or_dual_infeasible != "
              "solve_stats.problem_status.primal_or_dual_infeasible = "
           << solve_stats.problem_status().primal_or_dual_infeasible();
  }
  if (objective_bounds.primal_bound() != solve_stats.best_primal_bound()) {
    return util::InvalidArgumentErrorBuilder()
           << objective_bounds.primal_bound()
           << " = termination.objective_bounds.primal_bound != "
              "solve_stats.best_primal_bound = "
           << solve_stats.best_primal_bound();
  }
  if (objective_bounds.dual_bound() != solve_stats.best_dual_bound()) {
    return util::InvalidArgumentErrorBuilder()
           << objective_bounds.dual_bound()
           << " = termination.objective_bounds.dual_bound != "
              "solve_stats.best_dual_bound = "
           << solve_stats.best_dual_bound();
  }
  return absl::OkStatus();
}

}  // namespace

absl::Status ValidateResult(const SolveResultProto& result,
                            const ModelSolveParametersProto& parameters,
                            const ModelSummary& model_summary) {
  // TODO(b/290091715): Remove once problem_status and objective bounds are
  // removed from solve_stats and their presence is guaranteed in termination.
  RETURN_IF_ERROR(ValidateSolveStatsTerminationEqualities(result));
  // TODO(b/290091715): Replace by
  // TerminationProto termination = result.termination();
  // once problem_status and objective bounds are removed from solve_stats and
  // their presence is guaranteed in termination.
  TerminationProto termination = result.termination();
  *termination.mutable_objective_bounds() = GetObjectiveBounds(result);
  *termination.mutable_problem_status() = GetProblemStatus(result);

  RETURN_IF_ERROR(ValidateTermination(termination, model_summary.maximize));
  RETURN_IF_ERROR(ValidateSolveStats(result.solve_stats()));
  RETURN_IF_ERROR(
      ValidateSolutions(result.solutions(), parameters, model_summary));

  if (termination.reason() == TERMINATION_REASON_OPTIMAL ||
      termination.reason() == TERMINATION_REASON_FEASIBLE) {
    RETURN_IF_ERROR(CheckHasPrimalSolution(result))
        << "inconsistent termination reason "
        << TerminationReasonProto_Name(termination.reason());
  }
  if (termination.reason() == TERMINATION_REASON_NO_SOLUTION_FOUND) {
    RETURN_IF_ERROR(RequireNoPrimalFeasibleSolution(result))
        << "inconsistent termination reason "
        << TerminationReasonProto_Name(termination.reason());
  }

  RETURN_IF_ERROR(CheckPrimalSolutionAndTerminationConsistency(
      result.termination(), result.solutions(), model_summary.maximize));
  RETURN_IF_ERROR(CheckDualSolutionAndStatusConsistency(
      result.termination(), result.solutions(), model_summary.maximize));

  if (result.primal_rays_size() > 0 &&
      termination.problem_status().dual_status() ==
          FEASIBILITY_STATUS_FEASIBLE) {
    return absl::InvalidArgumentError(
        "termination.problem_status.dual_status = "
        "FEASIBILITY_STATUS_FEASIBLE, "
        "but a primal ray is returned");
  }
  for (int i = 0; i < result.primal_rays_size(); ++i) {
    RETURN_IF_ERROR(ValidatePrimalRay(result.primal_rays(i),
                                      parameters.variable_values_filter(),
                                      model_summary))
        << "Invalid primal_rays[" << i << "]";
  }
  if (result.dual_rays_size() > 0 &&
      termination.problem_status().primal_status() ==
          FEASIBILITY_STATUS_FEASIBLE) {
    return absl::InvalidArgumentError(
        "termination.problem_status.primal_status = "
        "FEASIBILITY_STATUS_FEASIBLE, but a dual ray is returned");
  }
  for (int i = 0; i < result.dual_rays_size(); ++i) {
    RETURN_IF_ERROR(
        ValidateDualRay(result.dual_rays(i), parameters, model_summary))
        << "Invalid dual_rays[" << i << "]";
  }

  return absl::OkStatus();
}

}  // namespace math_opt
}  // namespace operations_research
