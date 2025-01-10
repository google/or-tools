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

#include "ortools/math_opt/core/empty_bounds.h"

#include <limits>

#include "absl/strings/str_cat.h"
#include "ortools/math_opt/result.pb.h"
#include "ortools/util/fp_roundtrip_conv.h"

namespace operations_research::math_opt {

constexpr double kInf = std::numeric_limits<double>::infinity();

SolveResultProto ResultForIntegerInfeasible(const bool is_maximize,
                                            const int64_t bad_variable_id,
                                            const double lb, const double ub) {
  SolveResultProto result;
  result.mutable_termination()->set_reason(TERMINATION_REASON_INFEASIBLE);
  result.mutable_termination()->set_detail(
      absl::StrCat("Problem had one or more integer variables with no integers "
                   "in domain, e.g. integer variable with id: ",
                   bad_variable_id, " had bounds: [", RoundTripDoubleFormat(lb),
                   ", ", RoundTripDoubleFormat(ub), "]."));
  result.mutable_solve_stats()->mutable_problem_status()->set_primal_status(
      FEASIBILITY_STATUS_INFEASIBLE);
  result.mutable_solve_stats()->mutable_problem_status()->set_dual_status(
      FEASIBILITY_STATUS_UNDETERMINED);
  const double objective_value = is_maximize ? -kInf : kInf;
  result.mutable_solve_stats()->set_best_primal_bound(objective_value);
  result.mutable_solve_stats()->set_best_dual_bound(-objective_value);
  return result;
}

}  // namespace operations_research::math_opt
