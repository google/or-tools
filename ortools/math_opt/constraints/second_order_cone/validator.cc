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

#include "ortools/math_opt/constraints/second_order_cone/validator.h"

#include <cstdint>

#include "absl/status/status.h"
#include "ortools/base/status_builder.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/core/model_summary.h"
#include "ortools/math_opt/core/sparse_vector_view.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/validators/linear_expression_validator.h"

namespace operations_research::math_opt {

absl::Status ValidateConstraint(
    const SecondOrderConeConstraintProto& constraint,
    const IdNameBiMap& variable_universe) {
  RETURN_IF_ERROR(
      ValidateLinearExpression(constraint.upper_bound(), variable_universe))
      << "invalid `upper_bound`";
  for (int i = 0; i < constraint.arguments_to_norm_size(); ++i) {
    const LinearExpressionProto& expression = constraint.arguments_to_norm(i);
    RETURN_IF_ERROR(ValidateLinearExpression(expression, variable_universe))
        << "invalid `arguments_to_norm` at index: " << i;
  }
  return absl::OkStatus();
}

}  // namespace operations_research::math_opt
