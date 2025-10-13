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

#include "ortools/math_opt/constraints/second_order_cone/second_order_cone_constraint.h"

#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "ortools/base/strong_int.h"
#include "ortools/math_opt/constraints/second_order_cone/storage.h"
#include "ortools/math_opt/constraints/util/model_util.h"
#include "ortools/math_opt/cpp/variable_and_expressions.h"
#include "ortools/math_opt/storage/linear_expression_data.h"
#include "ortools/math_opt/storage/model_storage.h"

namespace operations_research::math_opt {

LinearExpression SecondOrderConeConstraint::UpperBound() const {
  return ToLinearExpression(*storage(),
                            storage()->constraint_data(id_).upper_bound);
}

std::vector<LinearExpression> SecondOrderConeConstraint::ArgumentsToNorm()
    const {
  const SecondOrderConeConstraintData& data = storage()->constraint_data(id_);
  std::vector<LinearExpression> args;
  args.reserve(data.arguments_to_norm.size());
  for (const LinearExpressionData& arg_data : data.arguments_to_norm) {
    args.push_back(ToLinearExpression(*storage(), arg_data));
  }
  return args;
}

std::string SecondOrderConeConstraint::ToString() const {
  if (!storage()->has_constraint(id_)) {
    return std::string(kDeletedConstraintDefaultDescription);
  }
  const SecondOrderConeConstraintData& data = storage()->constraint_data(id_);
  std::stringstream str;
  str << "||{";
  bool leading_comma = false;
  for (const LinearExpressionData& arg_data : data.arguments_to_norm) {
    if (leading_comma) {
      str << ", ";
    }
    leading_comma = true;
    str << ToLinearExpression(*storage(), arg_data);
  }
  str << "}||₂ ≤ " << ToLinearExpression(*storage(), data.upper_bound);
  return str.str();
}

}  // namespace operations_research::math_opt
