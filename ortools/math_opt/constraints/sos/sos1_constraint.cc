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

#include "ortools/math_opt/constraints/sos/sos1_constraint.h"

#include "ortools/base/strong_int.h"
#include "ortools/math_opt/cpp/variable_and_expressions.h"
#include "ortools/math_opt/storage/linear_expression_data.h"
#include "ortools/math_opt/storage/model_storage.h"
#include "ortools/math_opt/storage/sparse_coefficient_map.h"

namespace operations_research::math_opt {

LinearExpression Sos1Constraint::Expression(int index) const {
  const LinearExpressionData& storage_expr =
      storage_->constraint_data(id_).expression(index);
  LinearExpression out_expr = storage_expr.offset;
  for (const auto [var_id, coeff] : storage_expr.coeffs.terms()) {
    out_expr += coeff * Variable(storage_, var_id);
  }
  return out_expr;
}

}  // namespace operations_research::math_opt
