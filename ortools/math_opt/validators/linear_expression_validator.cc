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

#include "ortools/math_opt/validators/linear_expression_validator.h"

#include <cstdint>

#include "absl/status/status.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/core/model_summary.h"
#include "ortools/math_opt/core/sparse_vector_view.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/validators/scalar_validator.h"
#include "ortools/math_opt/validators/sparse_vector_validator.h"

namespace operations_research::math_opt {

absl::Status ValidateLinearExpression(const LinearExpressionProto& expression,
                                      const IdNameBiMap& variable_universe) {
  RETURN_IF_ERROR(CheckIdsAndValues(
      MakeView(expression.ids(), expression.coefficients()),
      {.allow_positive_infinity = false, .allow_negative_infinity = false}))
      << "invalid linear expression terms";
  for (const int64_t var_id : expression.ids()) {
    if (!variable_universe.HasId(var_id)) {
      return util::InvalidArgumentErrorBuilder()
             << "invalid variable id: " << var_id;
    }
  }
  RETURN_IF_ERROR(CheckScalarNoNanNoInf(expression.offset()))
      << "invalid linear expression offset";
  return absl::OkStatus();
}

}  // namespace operations_research::math_opt
