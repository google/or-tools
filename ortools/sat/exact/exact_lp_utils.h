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

#ifndef ORTOOLS_SAT_EXACT_EXACT_LP_UTILS_H_
#define ORTOOLS_SAT_EXACT_EXACT_LP_UTILS_H_

#include <cstdint>
#include <optional>

#include "absl/types/span.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/sat/cp_model.pb.h"

namespace operations_research {
namespace sat {

// Converts an MPConstraintProto over integer variables to a
// LinearConstraintProto such that:
// * They are equivalent mathematically (i.e., all solutions of the first are
//   solutions of the second and vice versa). This is only possible if
//   all variables are integers.
// * If we write the MPConstraintProto as Sum_i c_i * X_i the returned
//   LinearConstraintProto satisfies, for every X_i in [lb_i, ub_i]:
//      Sum |new_coeff_i * X_i| <= max_sum_of_abs_activity.
//
// Returns std::nullopt if this is impossible.
//
// It is up to the caller to ensure that all variables of the MPConstraintProto
// are integers.
//
// `int_var_lower_bounds` and `int_var_upper_bounds` are the lower and upper
// bounds of the integer variables in the MPModelProto clamped to [kint64min,
// kint64max].
std::optional<LinearConstraintProto> ConvertConstraintToIntegerIfPossible(
    const MPConstraintProto& mp_constraint,
    absl::Span<const int64_t> int_var_lower_bounds,
    absl::Span<const int64_t> int_var_upper_bounds,
    int64_t max_sum_of_abs_activity);

}  // namespace sat
}  // namespace operations_research

#endif  // ORTOOLS_SAT_EXACT_EXACT_LP_UTILS_H_
