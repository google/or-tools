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

#include "ortools/math_opt/validators/bounds_and_status_validator.h"

#include <cmath>
#include <ios>
#include <limits>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "ortools/base/status_builder.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/result.pb.h"
#include "ortools/math_opt/validators/scalar_validator.h"
#include "ortools/port/proto_utils.h"

namespace operations_research::math_opt {
namespace {
constexpr double kInf = std::numeric_limits<double>::infinity();
}  // namespace
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

absl::Status ValidateObjectiveBounds(const ObjectiveBoundsProto& bounds) {
  const DoubleOptions nonan;
  RETURN_IF_ERROR(CheckScalar(bounds.primal_bound(), nonan))
      << "in primal_bound";
  RETURN_IF_ERROR(CheckScalar(bounds.dual_bound(), nonan)) << "in dual_bound";
  return absl::OkStatus();
}

absl::Status CheckFinitePrimalBound(const ObjectiveBoundsProto& bounds) {
  if (!std::isfinite(bounds.primal_bound())) {
    return util::InvalidArgumentErrorBuilder()
           << "expected finite primal bound, but found "
           << bounds.primal_bound();
  }
  return absl::OkStatus();
}

namespace {

absl::Status ValidateFiniteBoundImpliesFeasibleStatus(
    const double first_bound, const FeasibilityStatusProto first_status,
    absl::string_view first_name) {
  if (!std::isfinite(first_bound)) {
    return absl::OkStatus();
  }
  if (first_status == FEASIBILITY_STATUS_FEASIBLE) {
    return absl::OkStatus();
  }
  return util::InvalidArgumentErrorBuilder()
         << "expected " << first_name
         << " status = FEASIBILITY_STATUS_FEASIBLE for finite " << first_name
         << " bound = " << first_bound << ", but found " << first_name
         << " status = " << ProtoEnumToString(first_status);
}

absl::Status ValidateNotUnboundedBoundImpliesNotUnboundedStatus(
    const double first_bound, const FeasibilityStatusProto first_status,
    const FeasibilityStatusProto second_status, absl::string_view first_name,
    absl::string_view second_name, const double unbounded_bound) {
  if (first_bound == unbounded_bound) {
    return absl::OkStatus();
  }
  if (first_status != FEASIBILITY_STATUS_FEASIBLE ||
      second_status != FEASIBILITY_STATUS_INFEASIBLE) {
    return absl::OkStatus();
  }
  return util::InvalidArgumentErrorBuilder()
         << "unexpected (" << first_name << " status, " << second_name
         << " status) = (FEASIBILITY_STATUS_FEASIBLE, "
            "FEASIBILITY_STATUS_INFEASIBLE) for not-unbounded "
         << first_name << " bound = " << first_bound;
}

absl::Status ValidateUnboundedBoundImpliesUnboundedStatus(
    const double first_bound, const double second_bound,
    const FeasibilityStatusProto first_status,
    const FeasibilityStatusProto second_status, absl::string_view first_name,
    absl::string_view second_name, const double unbounded_bound) {
  if (first_bound != unbounded_bound) {
    return absl::OkStatus();
  }
  if (first_status != FEASIBILITY_STATUS_FEASIBLE) {
    return util::InvalidArgumentErrorBuilder()
           << "expected " << first_name
           << " status = FEASIBILITY_STATUS_FEASIBLE for unbounded "
           << first_name << " bound = " << first_bound << ", but found "
           << first_name << " status = " << ProtoEnumToString(first_status);
  }
  if (second_status != FEASIBILITY_STATUS_INFEASIBLE) {
    return util::InvalidArgumentErrorBuilder()
           << "expected " << second_name
           << " status = FEASIBILITY_STATUS_INFEASIBLE for unbounded "
           << first_name << " bound = " << first_bound << ", but found "
           << second_name << " status = " << ProtoEnumToString(second_status);
  }
  if (second_bound != first_bound) {
    return util::InvalidArgumentErrorBuilder()
           << "expected " << second_name << " bound = " << first_name
           << " bound for unbounded " << first_name
           << " bound = " << first_bound << ", but found " << second_name
           << " bound = " << second_bound;
  }
  return absl::OkStatus();
}

}  // namespace

absl::Status ValidateBoundStatusConsistency(
    const ObjectiveBoundsProto& objective_bounds,
    const ProblemStatusProto& status, bool is_maximize) {
  RETURN_IF_ERROR(ValidateUnboundedBoundImpliesUnboundedStatus(
      /*first_bound=*/objective_bounds.primal_bound(),
      /*second_bound=*/objective_bounds.dual_bound(),
      /*first_status=*/status.primal_status(),
      /*second_status=*/status.dual_status(), "primal", "dual",
      /*unbounded_bound=*/is_maximize ? kInf : -kInf))
      << "for is_maximize = " << std::boolalpha << is_maximize;
  RETURN_IF_ERROR(ValidateUnboundedBoundImpliesUnboundedStatus(
      /*first_bound=*/objective_bounds.dual_bound(),
      /*second_bound=*/objective_bounds.primal_bound(),
      /*first_status=*/status.dual_status(),
      /*second_status=*/status.primal_status(), "dual", "primal",
      /*unbounded_bound=*/is_maximize ? -kInf : kInf))
      << "for is_maximize = " << std::boolalpha << is_maximize;

  RETURN_IF_ERROR(ValidateFiniteBoundImpliesFeasibleStatus(
      /*first_bound=*/objective_bounds.primal_bound(),
      /*first_status=*/status.primal_status(), /*first_name=*/"primal"));
  RETURN_IF_ERROR(ValidateFiniteBoundImpliesFeasibleStatus(
      /*first_bound=*/objective_bounds.dual_bound(),
      /*first_status=*/status.dual_status(), /*first_name=*/"dual"));

  RETURN_IF_ERROR(ValidateNotUnboundedBoundImpliesNotUnboundedStatus(
      /*first_bound=*/objective_bounds.primal_bound(),
      /*first_status=*/status.primal_status(),
      /*second_status=*/status.dual_status(), "primal", "dual",
      /*unbounded_bound=*/is_maximize ? kInf : -kInf))
      << "for is_maximize = " << std::boolalpha << is_maximize;
  RETURN_IF_ERROR(ValidateNotUnboundedBoundImpliesNotUnboundedStatus(
      /*first_bound=*/objective_bounds.dual_bound(),
      /*first_status=*/status.dual_status(),
      /*second_status=*/status.primal_status(), "dual", "primal",
      /*unbounded_bound=*/is_maximize ? -kInf : kInf))
      << "for is_maximize = " << std::boolalpha << is_maximize;
  return absl::OkStatus();
}

}  // namespace operations_research::math_opt
