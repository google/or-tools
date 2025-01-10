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

#include "ortools/math_opt/constraints/quadratic/quadratic_constraint.h"

#include <utility>

#include "ortools/base/strong_int.h"
#include "ortools/math_opt/cpp/variable_and_expressions.h"
#include "ortools/math_opt/storage/model_storage.h"
#include "ortools/math_opt/storage/sparse_coefficient_map.h"
#include "ortools/math_opt/storage/sparse_matrix.h"

namespace operations_research::math_opt {

BoundedQuadraticExpression QuadraticConstraint::AsBoundedQuadraticExpression()
    const {
  QuadraticExpression expression;
  const QuadraticConstraintData& data = storage()->constraint_data(id_);
  for (const auto [var, coeff] : data.linear_terms.terms()) {
    expression += coeff * Variable(storage(), var);
  }
  for (const auto& [first_var, second_var, coeff] :
       data.quadratic_terms.Terms()) {
    expression += coeff * Variable(storage(), first_var) *
                  Variable(storage(), second_var);
  }
  return data.lower_bound <= std::move(expression) <= data.upper_bound;
}

}  // namespace operations_research::math_opt
