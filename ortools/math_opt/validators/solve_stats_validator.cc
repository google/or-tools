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

#include "ortools/math_opt/validators/solve_stats_validator.h"

#include <cmath>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/time/time.h"
#include "ortools/base/protoutil.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/result.pb.h"
#include "ortools/math_opt/validators/scalar_validator.h"
#include "ortools/port/proto_utils.h"

namespace operations_research {
namespace math_opt {

absl::Status ValidateFeasibilityStatus(const FeasibilityStatusProto& status) {
  if (!FeasibilityStatusProto_IsValid(status)) {
    return absl::InvalidArgumentError(absl::StrCat("invalid status ", status));
  }
  if (status == FEASIBILITY_STATUS_UNSPECIFIED) {
    return absl::InvalidArgumentError(
        "invalid status FEASIBILITY_STATUS_UNSPECIFIED");
  }
  return absl::OkStatus();
}

absl::Status ValidateProblemStatus(const ProblemStatusProto& status) {
  RETURN_IF_ERROR(ValidateFeasibilityStatus(status.primal_status()))
      << "invalid primal_status";
  RETURN_IF_ERROR(ValidateFeasibilityStatus(status.dual_status()))
      << "invalid dual_status";
  if (status.primal_or_dual_infeasible() &&
      (status.primal_status() != FEASIBILITY_STATUS_UNDETERMINED ||
       status.dual_status() != FEASIBILITY_STATUS_UNDETERMINED)) {
    return absl::InvalidArgumentError(absl::StrCat(
        "primal_or_dual_infeasible can be true only when primal status = dual "
        "status = FEASIBILITY_STATUS_UNDETERMINED, and we have primal status "
        "= ",
        ProtoEnumToString(status.primal_status()),
        " and dual status = ", ProtoEnumToString(status.dual_status())));
  }
  return absl::OkStatus();
}

// Assumes ValidateProblemStatus(status) is ok.
absl::Status CheckPrimalStatusIs(const ProblemStatusProto& status,
                                 const FeasibilityStatusProto required_status) {
  const FeasibilityStatusProto actual_status = status.primal_status();
  if (actual_status == required_status) {
    return absl::OkStatus();
  }
  return absl::InvalidArgumentError(
      absl::StrCat("expected problem_status.primal_status = ",
                   ProtoEnumToString(required_status), ", but was ",
                   ProtoEnumToString(actual_status)));
}

// Assumes ValidateProblemStatus(status) is ok.
absl::Status CheckPrimalStatusIsNot(
    const ProblemStatusProto& status,
    const FeasibilityStatusProto forbidden_status) {
  const FeasibilityStatusProto actual_status = status.primal_status();
  if (actual_status != forbidden_status) {
    return absl::OkStatus();
  }
  return absl::InvalidArgumentError(
      absl::StrCat("expected problem_status.primal_status != ",
                   ProtoEnumToString(forbidden_status)));
}

// Assumes ValidateProblemStatus(status) is ok.
absl::Status CheckDualStatusIsNot(
    const ProblemStatusProto& status,
    const FeasibilityStatusProto forbidden_status) {
  const FeasibilityStatusProto actual_status = status.dual_status();
  if (actual_status != forbidden_status) {
    return absl::OkStatus();
  }
  return absl::InvalidArgumentError(
      absl::StrCat("expected problem_status.dual_status != ",
                   ProtoEnumToString(forbidden_status)));
}

// Assumes ValidateProblemStatus(status) is ok.
absl::Status CheckDualStatusIs(const ProblemStatusProto& status,
                               const FeasibilityStatusProto required_status,
                               const bool primal_or_dual_infeasible_also_ok) {
  const FeasibilityStatusProto actual_status = status.dual_status();
  if (actual_status == required_status) {
    return absl::OkStatus();
  }
  if (primal_or_dual_infeasible_also_ok && status.primal_or_dual_infeasible()) {
    // ValidateProblemStatus call above guarantees primal and dual statuses
    // are FEASIBILITY_STATUS_UNDETERMINED here.
    return absl::OkStatus();
  }
  if (primal_or_dual_infeasible_also_ok) {
    return absl::InvalidArgumentError(absl::StrCat(
        "expected either problem_status.dual_status = ",
        ProtoEnumToString(required_status), " (and was ",
        ProtoEnumToString(actual_status),
        ") or problem_status.primal_or_dual_infeasible = true (and "
        "was false)"));
  }
  return absl::InvalidArgumentError(
      absl::StrCat("expected problem_status.dual_status = ",
                   ProtoEnumToString(required_status), ", but was ",
                   ProtoEnumToString(actual_status)));
}

namespace {
// Assumes ValidateSolveStats(solve_stats) is ok.
absl::Status ValidateSolveStatsConsistency(const SolveStatsProto& solve_stats) {
  // TODO(b/204457524): refine validator once optimization direction is in
  // model summary (i.e. avoid the absl::abs).
  if (solve_stats.problem_status().primal_or_dual_infeasible() &&
      std::isfinite(solve_stats.best_primal_bound())) {
    return absl::InvalidArgumentError(
        "best_primal_bound is finite, but problem status is "
        "primal_or_dual_infeasible");
  }
  if (solve_stats.problem_status().primal_or_dual_infeasible() &&
      std::isfinite(solve_stats.best_dual_bound())) {
    return absl::InvalidArgumentError(
        "best_dual_bound is finite, but problem status is "
        "primal_or_dual_infeasible");
  }
  if (solve_stats.problem_status().primal_status() !=
          FEASIBILITY_STATUS_FEASIBLE &&
      std::isfinite(solve_stats.best_primal_bound())) {
    return absl::InvalidArgumentError(
        absl::StrCat("best_primal_bound is finite, but primal_status is not "
                     "feasible (primal_status = ",
                     solve_stats.problem_status().primal_status(), ")"));
  }
  if (solve_stats.problem_status().dual_status() !=
          FEASIBILITY_STATUS_FEASIBLE &&
      std::isfinite(solve_stats.best_dual_bound())) {
    return absl::InvalidArgumentError(
        absl::StrCat("best_dual_bound is finite, but dual_status is not "
                     "feasible (dual_status = ",
                     solve_stats.problem_status().dual_status(), ")"));
  }
  return absl::OkStatus();
}
}  // namespace

absl::Status ValidateSolveStats(const SolveStatsProto& solve_stats) {
  const absl::StatusOr<absl::Duration> solve_time =
      util_time::DecodeGoogleApiProto(solve_stats.solve_time());
  if (!solve_time.ok()) {
    return absl::InvalidArgumentError(
        absl::StrCat("invalid solve_time, ", solve_time.status().message()));
  }
  if (solve_time.value() < absl::ZeroDuration()) {
    return absl::InvalidArgumentError("solve_time must be non-negative");
  }
  if (solve_stats.simplex_iterations() < 0) {
    return absl::InvalidArgumentError(
        "simplex_iterations must be non-negative");
  }
  if (solve_stats.barrier_iterations() < 0) {
    return absl::InvalidArgumentError(
        "barrier_iterations must be non-negative");
  }
  if (solve_stats.node_count() < 0) {
    return absl::InvalidArgumentError("node_count must be non-negative");
  }
  RETURN_IF_ERROR(ValidateProblemStatus(solve_stats.problem_status()));
  const DoubleOptions nonan;
  RETURN_IF_ERROR(CheckScalar(solve_stats.best_primal_bound(), nonan))
      << "in best_primal_bound";
  RETURN_IF_ERROR(CheckScalar(solve_stats.best_dual_bound(), nonan))
      << "in best_dual_bound";
  RETURN_IF_ERROR(ValidateSolveStatsConsistency(solve_stats));
  return absl::OkStatus();
}

}  // namespace math_opt
}  // namespace operations_research
