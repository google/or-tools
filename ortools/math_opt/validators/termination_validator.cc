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

#include "ortools/math_opt/validators/termination_validator.h"

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "ortools/base/logging.h"
#include "ortools/base/status_builder.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/result.pb.h"
#include "ortools/math_opt/validators/bounds_and_status_validator.h"
#include "ortools/port/proto_utils.h"

namespace operations_research {
namespace math_opt {

namespace {
absl::Status CheckNotPrimalDualInfeasible(const ProblemStatusProto& status) {
  if (status.primal_or_dual_infeasible()) {
    return util::InvalidArgumentErrorBuilder()
           << "expected primal_or_dual_infeasible = false";
  }
  return absl::OkStatus();
}

// Assumes ValidateObjectiveBounds(objective_bounds, is_maximize) and
// ValidateProblemStatus(status) are ok.
absl::Status ValidateTerminationReasonConsistency(
    const TerminationProto& termination) {
  switch (termination.reason()) {
    case TERMINATION_REASON_OPTIMAL: {
      RETURN_IF_ERROR(CheckPrimalStatusIs(termination.problem_status(),
                                          FEASIBILITY_STATUS_FEASIBLE));
      RETURN_IF_ERROR(CheckDualStatusIs(termination.problem_status(),
                                        FEASIBILITY_STATUS_FEASIBLE));
      RETURN_IF_ERROR(CheckFinitePrimalBound(termination.objective_bounds()));
      // TODO(b/290359402): Add CheckFiniteDualBounds() to enforce finite dual
      // bounds when possible.
      return absl::OkStatus();
    }
    case TERMINATION_REASON_INFEASIBLE: {
      RETURN_IF_ERROR(CheckPrimalStatusIs(termination.problem_status(),
                                          FEASIBILITY_STATUS_INFEASIBLE));
      return absl::OkStatus();
    }
    case TERMINATION_REASON_UNBOUNDED: {
      RETURN_IF_ERROR(CheckPrimalStatusIs(termination.problem_status(),
                                          FEASIBILITY_STATUS_FEASIBLE));
      RETURN_IF_ERROR(CheckDualStatusIs(termination.problem_status(),
                                        FEASIBILITY_STATUS_INFEASIBLE));
      return absl::OkStatus();
    }
    case TERMINATION_REASON_INFEASIBLE_OR_UNBOUNDED:
      RETURN_IF_ERROR(CheckPrimalStatusIs(termination.problem_status(),
                                          FEASIBILITY_STATUS_UNDETERMINED));
      RETURN_IF_ERROR(CheckDualStatusIs(
          termination.problem_status(), FEASIBILITY_STATUS_INFEASIBLE,
          /*primal_or_dual_infeasible_also_ok=*/true));
      // Note that if primal status was not FEASIBILITY_STATUS_UNDETERMINED,
      // then primal_or_dual_infeasible must be false and dual status would be
      // FEASIBILITY_STATUS_INFEASIBLE. Then if primal status was
      // FEASIBILITY_STATUS_INFEASIBLE we would have
      // TERMINATION_REASON_INFEASIBLE and if it was
      // FEASIBILITY_STATUS_FEASIBLE we would have
      // TERMINATION_REASON_UNBOUNDED.
      return absl::OkStatus();
    case TERMINATION_REASON_IMPRECISE:
      RETURN_IF_ERROR(CheckPrimalStatusIs(termination.problem_status(),
                                          FEASIBILITY_STATUS_UNDETERMINED));
      RETURN_IF_ERROR(CheckDualStatusIs(termination.problem_status(),
                                        FEASIBILITY_STATUS_UNDETERMINED));
      RETURN_IF_ERROR(
          CheckNotPrimalDualInfeasible(termination.problem_status()));
      // TODO(b/211679884): update when imprecise solutions are added.
      return absl::OkStatus();
    case TERMINATION_REASON_FEASIBLE: {
      RETURN_IF_ERROR(CheckPrimalStatusIs(termination.problem_status(),
                                          FEASIBILITY_STATUS_FEASIBLE));
      RETURN_IF_ERROR(CheckDualStatusIsNot(termination.problem_status(),
                                           FEASIBILITY_STATUS_INFEASIBLE));
      // Note that if dual status was FEASIBILITY_STATUS_INFEASIBLE, then we
      // would have TERMINATION_REASON_UNBOUNDED (For MIP this follows the
      // assumption that every floating point ray can be scaled to be
      // integer).
      RETURN_IF_ERROR(CheckFinitePrimalBound(termination.objective_bounds()));
      return absl::OkStatus();
    }
    case TERMINATION_REASON_NO_SOLUTION_FOUND: {
      // Primal status may be feasible as long as no solutions are returned.
      RETURN_IF_ERROR(CheckPrimalStatusIsNot(termination.problem_status(),
                                             FEASIBILITY_STATUS_INFEASIBLE));
      // Note if primal status was FEASIBILITY_STATUS_INFEASIBLE, then we
      // would have TERMINATION_REASON_INFEASIBLE.
      return absl::OkStatus();
    }
    case TERMINATION_REASON_NUMERICAL_ERROR:
    case TERMINATION_REASON_OTHER_ERROR: {
      RETURN_IF_ERROR(CheckPrimalStatusIs(termination.problem_status(),
                                          FEASIBILITY_STATUS_UNDETERMINED));
      RETURN_IF_ERROR(CheckDualStatusIs(termination.problem_status(),
                                        FEASIBILITY_STATUS_UNDETERMINED));
      RETURN_IF_ERROR(
          CheckNotPrimalDualInfeasible(termination.problem_status()));
    }
      return absl::OkStatus();
    default:
      LOG(FATAL) << ProtoEnumToString(termination.reason())
                 << " not implemented";
  }

  return absl::OkStatus();
}

}  // namespace

absl::Status ValidateTermination(const TerminationProto& termination,
                                 const bool is_maximize) {
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
  RETURN_IF_ERROR(ValidateObjectiveBounds(termination.objective_bounds()));
  RETURN_IF_ERROR(ValidateProblemStatus(termination.problem_status()));
  RETURN_IF_ERROR(ValidateBoundStatusConsistency(termination.objective_bounds(),
                                                 termination.problem_status(),
                                                 is_maximize));
  RETURN_IF_ERROR(ValidateTerminationReasonConsistency(termination))
      << "for termination reason " << ProtoEnumToString(termination.reason());
  return absl::OkStatus();
}

}  // namespace math_opt
}  // namespace operations_research
