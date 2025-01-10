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

#include "ortools/math_opt/labs/general_constraint_to_mip.h"

#include <limits>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/math_opt/labs/linear_expr_util.h"

namespace operations_research::math_opt {

constexpr double kInf = std::numeric_limits<double>::infinity();

absl::Status FormulateIndicatorConstraintAsMip(
    Model& model, const IndicatorConstraint indicator_constraint) {
  if (indicator_constraint.storage() != model.storage()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Indicator constraint %s is associated with wrong "
                        "model (expected: %s, actual: %s)",
                        indicator_constraint.name(), model.name(),
                        indicator_constraint.storage()->name()));
  }
  if (!indicator_constraint.indicator_variable()) {
    model.DeleteIndicatorConstraint(indicator_constraint);
    return absl::OkStatus();
  }
  const Variable indicator_variable =
      *indicator_constraint.indicator_variable();
  if (!indicator_variable.is_integer() ||
      indicator_variable.lower_bound() < 0 ||
      indicator_variable.upper_bound() > 1) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "In indicator constraint %s: indicator variable %s is "
        "not a binary variable",
        indicator_constraint.name(), indicator_variable.name()));
  }
  const BoundedLinearExpression implied_constraint =
      indicator_constraint.ImpliedConstraint();
  // One if the implied constraint should hold; zero otherwise.
  const LinearExpression activated_expr =
      indicator_constraint.activate_on_zero() ? 1 - indicator_variable
                                              : indicator_variable;
  if (implied_constraint.lower_bound > -kInf) {
    const double expr_lower_bound = LowerBound(implied_constraint.expression);
    if (expr_lower_bound == -kInf) {
      return absl::InvalidArgumentError(
          absl::StrFormat("In indicator constraint %s: cannot prove that "
                          "implied constraint is unbounded from below",
                          indicator_constraint.name()));
    }
    model.AddLinearConstraint(
        implied_constraint.expression >=
        expr_lower_bound + (implied_constraint.lower_bound - expr_lower_bound) *
                               activated_expr);
  }
  if (implied_constraint.upper_bound < kInf) {
    const double expr_upper_bound = UpperBound(implied_constraint.expression);
    if (expr_upper_bound == kInf) {
      return absl::InvalidArgumentError(
          absl::StrFormat("In indicator constraint %s: cannot prove that "
                          "implied constraint is unbounded from above",
                          indicator_constraint.name()));
    }
    model.AddLinearConstraint(
        implied_constraint.expression <=
        expr_upper_bound + (implied_constraint.upper_bound - expr_upper_bound) *
                               activated_expr);
  }
  model.DeleteIndicatorConstraint(indicator_constraint);
  return absl::OkStatus();
}

}  // namespace operations_research::math_opt
