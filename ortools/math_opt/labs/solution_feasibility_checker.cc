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

#include "ortools/math_opt/labs/solution_feasibility_checker.h"

#include <cmath>
#include <string>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "ortools/base/mathutil.h"
#include "ortools/base/status_builder.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/util/fp_roundtrip_conv.h"

namespace operations_research::math_opt {
namespace {

absl::Status ValidateOptions(const FeasibilityCheckerOptions& options) {
  const auto tolerance_is_valid = [](const double tolerance) {
    return tolerance >= 0 && !std::isnan(tolerance);
  };
  if (!tolerance_is_valid(options.absolute_constraint_tolerance)) {
    return util::InvalidArgumentErrorBuilder()
           << "invalid absolute_constraint_tolerance value: "
           << options.absolute_constraint_tolerance;
  }
  if (!tolerance_is_valid(options.integrality_tolerance)) {
    return util::InvalidArgumentErrorBuilder()
           << "invalid integrality_tolerance value: "
           << options.integrality_tolerance;
  }
  if (!tolerance_is_valid(options.nonzero_tolerance)) {
    return util::InvalidArgumentErrorBuilder()
           << "invalid nonzero_tolerance value: " << options.nonzero_tolerance;
  }
  return absl::OkStatus();
}

bool IsNearlyLessThan(const double lhs, const double rhs,
                      const double absolute_tolerance) {
  return lhs <= rhs + absolute_tolerance;
}

bool IsNearlyEqualTo(const double actual, const double target,
                     const double absolute_tolerance) {
  return std::fabs(actual - target) <= absolute_tolerance;
}

absl::Status ValidateVariables(const Model& model,
                               const VariableMap<double>& variable_values) {
  for (const Variable variable : model.Variables()) {
    if (!variable_values.contains(variable)) {
      return util::InvalidArgumentErrorBuilder()
             << "Variable present in `model` but not `variable_values`: "
             << variable;
    }
  }
  for (const auto [variable, unused] : variable_values) {
    if (variable.storage() != model.storage()) {
      return util::InvalidArgumentErrorBuilder()
             << "Variable present in `variable_values` but not `model`: "
             << variable;
    }
  }
  return absl::OkStatus();
}

ModelSubset::Bounds CheckBoundedConstraint(
    const double expr_value, const double lower_bound, const double upper_bound,
    const FeasibilityCheckerOptions& options) {
  return {.lower = !IsNearlyLessThan(lower_bound, expr_value,
                                     options.absolute_constraint_tolerance),
          .upper = !IsNearlyLessThan(expr_value, upper_bound,
                                     options.absolute_constraint_tolerance)};
}

// CHECK-fails if `variable` and `variable_values` come from different models.
ModelSubset::Bounds CheckVariableBounds(
    const Variable variable, const VariableMap<double>& variable_values,
    const FeasibilityCheckerOptions& options) {
  return CheckBoundedConstraint(variable_values.at(variable),
                                variable.lower_bound(), variable.upper_bound(),
                                options);
}

// CHECK-fails if `constraint` and `variable_values` come from different models.
ModelSubset::Bounds CheckLinearConstraint(
    const LinearConstraint constraint,
    const VariableMap<double>& variable_values,
    const FeasibilityCheckerOptions& options) {
  const BoundedLinearExpression bounded_expr =
      constraint.AsBoundedLinearExpression();
  return CheckBoundedConstraint(
      bounded_expr.expression.Evaluate(variable_values),
      bounded_expr.lower_bound, bounded_expr.upper_bound, options);
}

// CHECK-fails if `constraint` and `variable_values` come from different models.
ModelSubset::Bounds CheckQuadraticConstraint(
    const QuadraticConstraint constraint,
    const VariableMap<double>& variable_values,
    const FeasibilityCheckerOptions& options) {
  const BoundedQuadraticExpression bounded_expr =
      constraint.AsBoundedQuadraticExpression();
  return CheckBoundedConstraint(
      bounded_expr.expression.Evaluate(variable_values),
      bounded_expr.lower_bound, bounded_expr.upper_bound, options);
}

// CHECK-fails if `constraint` and `variable_values` come from different models.
bool CheckSecondOrderConeConstraint(const SecondOrderConeConstraint constraint,
                                    const VariableMap<double>& variable_values,
                                    const FeasibilityCheckerOptions& options) {
  double args_to_norm_value = 0.0;
  for (const LinearExpression& expr : constraint.ArgumentsToNorm()) {
    // This is liable to overflow, but if it does so it will return inf, which
    // will ultimately cause this function to return false.
    args_to_norm_value += MathUtil::IPow(expr.Evaluate(variable_values), 2);
  }
  return IsNearlyLessThan(std::sqrt(args_to_norm_value),
                          constraint.UpperBound().Evaluate(variable_values),
                          options.absolute_constraint_tolerance);
}

// CHECK-fails if `constraint` and `variable_values` come from different models.
bool CheckSos1Constraint(const Sos1Constraint constraint,
                         const VariableMap<double>& variable_values,
                         const FeasibilityCheckerOptions& options) {
  // For expression i: was any expression with index in [0, i) nonzero?
  bool previous_nonzero = false;
  for (int i = 0; i < constraint.num_expressions(); ++i) {
    const bool expr_is_nonzero =
        !IsNearlyEqualTo(constraint.Expression(i).Evaluate(variable_values),
                         0.0, options.nonzero_tolerance);
    if (expr_is_nonzero && previous_nonzero) {
      // We've seen two nonzero expressions, the SOS1 constraint is violated.
      return false;
    }
    previous_nonzero |= expr_is_nonzero;
  }
  return true;
}

// CHECK-fails if `constraint` and `variable_values` come from different models.
bool CheckSos2Constraint(const Sos2Constraint constraint,
                         const VariableMap<double>& variable_values,
                         const FeasibilityCheckerOptions& options) {
  // For expression i: was expression i-1 nonzero?
  bool preceding_was_nonzero = false;
  // For expression i: was any expression with index in [0, i-1) nonzero?
  bool any_other_before_was_nonzero = false;
  for (int i = 0; i < constraint.num_expressions(); ++i) {
    const bool expr_is_nonzero =
        !IsNearlyEqualTo(constraint.Expression(i).Evaluate(variable_values),
                         0.0, options.nonzero_tolerance);
    if (expr_is_nonzero && any_other_before_was_nonzero) {
      // Expressions i and j are nonzero, for some j in [0, i-1), so the SOS2
      // constraint is violated.
      return false;
    }
    // Update values for expression i+1.
    any_other_before_was_nonzero |= preceding_was_nonzero;
    preceding_was_nonzero = expr_is_nonzero;
  }
  return true;
}

// CHECK-fails if `constraint` and `variable_values` come from different models.
// Only check the implication, not that the indicator variable is binary.
bool CheckIndicatorConstraint(const IndicatorConstraint constraint,
                              const VariableMap<double>& variable_values,
                              const FeasibilityCheckerOptions& options) {
  if (!constraint.indicator_variable().has_value()) {
    // Null indicator variables mean the constraint is vacuously satisfied.
    return true;
  }
  if (!IsNearlyEqualTo(variable_values.at(*constraint.indicator_variable()),
                       constraint.activate_on_zero() ? 0.0 : 1.0,
                       options.nonzero_tolerance)) {
    // If the indicator variable is not (nearly) at its indication value, the
    // constraint holds (there is no implication).
    return true;
  }
  const BoundedLinearExpression bounded_expr = constraint.ImpliedConstraint();
  // At this point in the function, we know that the implication should hold.
  // So, the indicator constraint is satisfied iff both sides of the implied
  // constraints are satisfied.
  return CheckBoundedConstraint(
             bounded_expr.expression.Evaluate(variable_values),
             bounded_expr.lower_bound, bounded_expr.upper_bound, options)
      .empty();
}

}  // namespace

absl::StatusOr<ModelSubset> CheckPrimalSolutionFeasibility(
    const Model& model, const VariableMap<double>& variable_values,
    const FeasibilityCheckerOptions& options) {
  RETURN_IF_ERROR(ValidateOptions(options));
  RETURN_IF_ERROR(ValidateVariables(model, variable_values));

  ModelSubset violated_constraints;
  for (const Variable variable : model.Variables()) {
    const ModelSubset::Bounds violations =
        CheckVariableBounds(variable, variable_values, options);
    if (!violations.empty()) {
      violated_constraints.variable_bounds[variable] = violations;
    }
    if (variable.is_integer()) {
      const double variable_value = variable_values.at(variable);
      const double rounded_variable_value = std::round(variable_value);
      if (std::fabs(rounded_variable_value - variable_value) >
          options.integrality_tolerance) {
        violated_constraints.variable_integrality.insert(variable);
      }
    }
  }

  for (const LinearConstraint linear_constraint : model.LinearConstraints()) {
    const ModelSubset::Bounds violations =
        CheckLinearConstraint(linear_constraint, variable_values, options);
    if (!violations.empty()) {
      violated_constraints.linear_constraints[linear_constraint] = violations;
    }
  }

  for (const QuadraticConstraint quadratic_constraint :
       model.QuadraticConstraints()) {
    const ModelSubset::Bounds violations = CheckQuadraticConstraint(
        quadratic_constraint, variable_values, options);
    if (!violations.empty()) {
      violated_constraints.quadratic_constraints[quadratic_constraint] =
          violations;
    }
  }

  for (const SecondOrderConeConstraint soc_constraint :
       model.SecondOrderConeConstraints()) {
    if (!CheckSecondOrderConeConstraint(soc_constraint, variable_values,
                                        options)) {
      violated_constraints.second_order_cone_constraints.insert(soc_constraint);
    }
  }

  for (const Sos1Constraint sos1_constraint : model.Sos1Constraints()) {
    if (!CheckSos1Constraint(sos1_constraint, variable_values, options)) {
      violated_constraints.sos1_constraints.insert(sos1_constraint);
    }
  }

  for (const Sos2Constraint sos2_constraint : model.Sos2Constraints()) {
    if (!CheckSos2Constraint(sos2_constraint, variable_values, options)) {
      violated_constraints.sos2_constraints.insert(sos2_constraint);
    }
  }

  for (const IndicatorConstraint indicator_constraint :
       model.IndicatorConstraints()) {
    if (!CheckIndicatorConstraint(indicator_constraint, variable_values,
                                  options)) {
      violated_constraints.indicator_constraints.insert(indicator_constraint);
    }
  }
  return violated_constraints;
}

namespace {

// `variables` and `variable_values` must share a common Model.
std::string VariableValuesAsString(std::vector<Variable> variables,
                                   const VariableMap<double>& variable_values) {
  absl::c_sort(variables, [](const Variable lhs, const Variable rhs) {
    return lhs.typed_id() < rhs.typed_id();
  });
  return absl::StrCat(
      "{",
      absl::StrJoin(variables, ", ",
                    [&](std::string* const out, const Variable variable) {
                      absl::StrAppendFormat(
                          out, "{%s, %v}", absl::FormatStreamed(variable),
                          RoundTripDoubleFormat(variable_values.at(variable)));
                    }),
      "}");
}

// Requires T::ToString() and T::NonzeroVariables() for duck-typing.
template <typename T>
std::string ViolatedConstraintAsString(
    const T violated_constraint, const VariableMap<double>& variable_values,
    const absl::string_view constraint_type) {
  return absl::StrFormat(
      "violated %s %s: %s, with variable values %s", constraint_type,
      absl::FormatStreamed(violated_constraint), violated_constraint.ToString(),
      VariableValuesAsString(violated_constraint.NonzeroVariables(),
                             variable_values));
}

// Requires T::ToString() and T::NonzeroVariables() for duck-typing.
template <typename T>
void AppendViolatedConstraintsAsStrings(
    const std::vector<T>& violated_constraints,
    const VariableMap<double>& variable_values,
    const absl::string_view constraint_type, std::vector<std::string>& output) {
  for (const T violated_constraint : violated_constraints) {
    output.push_back(ViolatedConstraintAsString(
        violated_constraint, variable_values, constraint_type));
  }
}

}  // namespace

absl::StatusOr<std::vector<std::string>> ViolatedConstraintsAsStrings(
    const Model& model, const ModelSubset& violated_constraints,
    const VariableMap<double>& variable_values) {
  RETURN_IF_ERROR(violated_constraints.CheckModelStorage(model.storage()))
      << "violated_constraints and model are inconsistent";
  RETURN_IF_ERROR(ValidateVariables(model, variable_values));

  std::vector<std::string> result;
  for (const Variable variable :
       SortedKeys(violated_constraints.variable_bounds)) {
    result.push_back(absl::StrFormat(
        "violated variable bound: %v ≤ %s ≤ %v, with variable value %v",
        RoundTripDoubleFormat(variable.lower_bound()),
        absl::FormatStreamed(variable),
        RoundTripDoubleFormat(variable.upper_bound()),
        RoundTripDoubleFormat(variable_values.at(variable))));
  }
  for (const Variable variable :
       SortedElements(violated_constraints.variable_integrality)) {
    result.push_back(absl::StrFormat(
        "violated variable integrality: %s, with variable value %v",
        absl::FormatStreamed(variable),
        RoundTripDoubleFormat(variable_values.at(variable))));
  }
  for (const LinearConstraint linear_constraint :
       SortedKeys(violated_constraints.linear_constraints)) {
    result.push_back(absl::StrFormat(
        "violated linear constraint %s: %s, with variable values %s",
        absl::FormatStreamed(linear_constraint), linear_constraint.ToString(),
        VariableValuesAsString(model.RowNonzeros(linear_constraint),
                               variable_values)));
  }
  AppendViolatedConstraintsAsStrings(
      SortedKeys(violated_constraints.quadratic_constraints), variable_values,
      "quadratic constraint", result);
  AppendViolatedConstraintsAsStrings(
      SortedElements(violated_constraints.second_order_cone_constraints),
      variable_values, "second-order cone constraint", result);
  AppendViolatedConstraintsAsStrings(
      SortedElements(violated_constraints.sos1_constraints), variable_values,
      "SOS1 constraint", result);
  AppendViolatedConstraintsAsStrings(
      SortedElements(violated_constraints.sos2_constraints), variable_values,
      "SOS2 constraint", result);
  AppendViolatedConstraintsAsStrings(
      SortedElements(violated_constraints.indicator_constraints),
      variable_values, "indicator constraint", result);
  return result;
}

}  // namespace operations_research::math_opt
