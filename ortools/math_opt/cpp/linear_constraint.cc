// Copyright 2010-2021 Google LLC
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

#include "ortools/math_opt/cpp/linear_constraint.h"

#include <utility>
#include <vector>

#include "ortools/math_opt/core/indexed_model.h"
#include "ortools/math_opt/cpp/variable_and_expressions.h"

namespace operations_research {
namespace math_opt {

std::vector<Variable> LinearConstraint::RowNonzeros() const {
  std::vector<Variable> result;
  for (const VariableId variable :
       model_->variables_in_linear_constraint(id_)) {
    result.push_back(Variable(model_, variable));
  }
  return result;
}

BoundedLinearExpression LinearConstraint::AsBoundedLinearExpression() const {
  LinearExpression terms;
  for (const VariableId var : model_->variables_in_linear_constraint(id_)) {
    terms +=
        Variable(model_, var) * model_->linear_constraint_coefficient(id_, var);
  }
  return lower_bound() <= std::move(terms) <= upper_bound();
}

}  // namespace math_opt
}  // namespace operations_research
