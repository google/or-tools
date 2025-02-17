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

#include "ortools/math_opt/constraints/indicator/validator.h"

#include <cmath>

#include "absl/status/status.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/core/model_summary.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/validators/scalar_validator.h"
#include "ortools/math_opt/validators/sparse_vector_validator.h"

namespace operations_research::math_opt {

absl::Status ValidateConstraint(const IndicatorConstraintProto& constraint,
                                const IdNameBiMap& variable_universe) {
  if (constraint.has_indicator_id() &&
      !variable_universe.HasId(constraint.indicator_id())) {
    return util::InvalidArgumentErrorBuilder()
           << "Invalid indicator variable id in indicator constraint: "
           << constraint.indicator_id();
  }
  RETURN_IF_ERROR(CheckIdsAndValues(
      MakeView(constraint.expression()), /*options=*/
      {.allow_positive_infinity = false, .allow_negative_infinity = false}))
      << "expression of implied constraint in indicator constraint";
  for (const int64_t var_id : constraint.expression().ids()) {
    if (!variable_universe.HasId(var_id)) {
      return util::InvalidArgumentErrorBuilder()
             << "Invalid variable id in implied constraint in indicator "
                "constraint: "
             << var_id;
    }
  }
  RETURN_IF_ERROR(CheckScalar(
      constraint.lower_bound(),
      {.allow_positive_infinity = false, .allow_negative_infinity = true}))
      << "invalid lower bound in indicator constraint: "
      << constraint.lower_bound();
  RETURN_IF_ERROR(CheckScalar(
      constraint.upper_bound(),
      {.allow_positive_infinity = true, .allow_negative_infinity = false}))
      << "invalid upper bound in indicator constraint: "
      << constraint.upper_bound();
  return absl::OkStatus();
}

}  // namespace operations_research::math_opt
