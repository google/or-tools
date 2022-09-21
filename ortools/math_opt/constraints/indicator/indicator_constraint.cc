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

#include "ortools/math_opt/constraints/indicator/indicator_constraint.h"

#include <optional>
#include <sstream>
#include <string>
#include <utility>

#include "ortools/base/strong_int.h"
#include "ortools/math_opt/constraints/util/model_util.h"
#include "ortools/math_opt/cpp/variable_and_expressions.h"
#include "ortools/math_opt/storage/model_storage.h"

namespace operations_research::math_opt {

BoundedLinearExpression IndicatorConstraint::ImplicatedConstraint() const {
  const IndicatorConstraintData& data = storage()->constraint_data(id_);
  LinearExpression expr = ToLinearExpression(*storage_, data.linear_terms, 0.0);
  return data.lower_bound <= std::move(expr) <= data.upper_bound;
}

std::string IndicatorConstraint::ToString() const {
  if (!storage()->has_constraint(id_)) {
    return std::string(kDeletedConstraintDefaultName);
  }
  const IndicatorConstraintData& data = storage()->constraint_data(id_);
  std::stringstream str;
  if (data.indicator.has_value()) {
    str << Variable(storage_, *data.indicator) << " = 1";
  } else {
    str << "[unset indicator variable]";
  }
  str << " ⇒ " << ImplicatedConstraint();
  return str.str();
}

}  // namespace operations_research::math_opt
