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

#ifndef OR_TOOLS_MATH_OPT_VALIDATORS_SOLVE_STATS_VALIDATOR_H_
#define OR_TOOLS_MATH_OPT_VALIDATORS_SOLVE_STATS_VALIDATOR_H_

#include "absl/status/status.h"
#include "ortools/math_opt/result.pb.h"

namespace operations_research {
namespace math_opt {

absl::Status ValidateProblemStatus(const ProblemStatusProto& status);
absl::Status ValidateSolveStats(const SolveStatsProto& solve_stats);

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

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_VALIDATORS_SOLVE_STATS_VALIDATOR_H_
