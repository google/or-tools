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

#ifndef OR_TOOLS_MATH_OPT_VALIDATORS_BOUNDS_AND_STATUS_VALIDATOR_H_
#define OR_TOOLS_MATH_OPT_VALIDATORS_BOUNDS_AND_STATUS_VALIDATOR_H_

#include "absl/status/status.h"
#include "ortools/math_opt/result.pb.h"

namespace operations_research::math_opt {

////////////////////////////////////////////////////////////////////////////////
// Problem status validators and utilities.
////////////////////////////////////////////////////////////////////////////////

absl::Status ValidateFeasibilityStatus(const FeasibilityStatusProto& status);
absl::Status ValidateProblemStatus(const ProblemStatusProto& status);

// Returns absl::Ok only if status.primal_status = required_status. Assumes
// validateProblemStatus(status) returns absl::Ok.
absl::Status CheckPrimalStatusIs(const ProblemStatusProto& status,
                                 FeasibilityStatusProto required_status);

// Returns absl::Ok only if status.primal_status != forbidden_status. Assumes
// validateProblemStatus(status) returns absl::Ok.
absl::Status CheckPrimalStatusIsNot(const ProblemStatusProto& status,
                                    FeasibilityStatusProto forbidden_status);

// If primal_or_dual_infeasible_also_ok is false, returns absl::Ok only if
// status.dual_status = required_status. If primal_or_dual_infeasible_also_ok
// is true, it returns absl::Ok when status.dual_status = required_status and
// when primal_or_dual_infeasible is true. Assumes validateProblemStatus(status)
// returns absl::Ok.
absl::Status CheckDualStatusIs(const ProblemStatusProto& status,
                               FeasibilityStatusProto required_status,
                               bool primal_or_dual_infeasible_also_ok = false);

// Returns absl::Ok only if status.dual_status != forbidden_status. Assumes
// validateProblemStatus(status) returns absl::Ok.
absl::Status CheckDualStatusIsNot(const ProblemStatusProto& status,
                                  FeasibilityStatusProto forbidden_status);

////////////////////////////////////////////////////////////////////////////////
// Objective bounds validators and utilities.
////////////////////////////////////////////////////////////////////////////////

absl::Status ValidateObjectiveBounds(const ObjectiveBoundsProto& bounds);

absl::Status CheckFinitePrimalBound(const ObjectiveBoundsProto& bounds);

////////////////////////////////////////////////////////////////////////////////
// Status-Bounds consistency validators.
////////////////////////////////////////////////////////////////////////////////

// Checks both bound-status compatibility rules.
// That is:
//    * If primal bound:
//        * is primal-unbounded (primal_bound = +infinity for max and
//          primal_bound = -infinity for min), then
//            * primal status is feasible,
//            * dual status is infeasible, and
//            * dual bound is equal to primal bound.
//        * is finite, then
//            * primal status is feasible.
//        * is finite or trivial (primal_bound = -infinity for max and
//          primal_bound = +infinity for min), then
//            * primal status feasible and dual status infeasible cannot hold at
//              the same time.
//    * If dual bound:
//        * is dual-unbounded (dual_bound = -infinity for max and
//          dual_bound = +infinity for min), then
//            * dual status is feasible,
//            * primal status is infeasible, and
//            * primal bound is equal to dual bound.
//        * is finite, then
//            * dual status is feasible.
//        * is finite or trivial (dual_bound = +infinity for max and
//          dual_bound = -infinity for min), then
//            * dual status feasible and primal status infeasible cannot hold at
//              the same time.
// Note that the rules for primal and dual bounds are symmetric.
absl::Status ValidateBoundStatusConsistency(
    const ObjectiveBoundsProto& objective_bounds,
    const ProblemStatusProto& status, bool is_maximize);

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_VALIDATORS_BOUNDS_AND_STATUS_VALIDATOR_H_
