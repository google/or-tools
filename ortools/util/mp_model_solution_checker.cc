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

#include "ortools/util/mp_model_solution_checker.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/status_macros.h"
#include "absl/strings/string_view.h"
#include "absl/types/source_location.h"
#include "absl/types/span.h"
#include "ortools/base/status_builder.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/util/fp_roundtrip_conv.h"
#include "ortools/util/full_precision_inequalities.h"
#include "ortools/util/logging.h"

namespace operations_research {

constexpr double kInf = std::numeric_limits<double>::infinity();

namespace {

double MaxPropagatesNan(const double a, const double b) {
  if (std::isnan(a) || std::isnan(b)) {
    return std::numeric_limits<double>::quiet_NaN();
  }
  return std::max(a, b);
}

}  // namespace

absl::StatusOr<bool> SolutionIsFeasible(const MPModelProto& model,
                                        const absl::Span<const double> solution,
                                        const CheckerOptions& options) {
  if (options.constraint_tolerance < 0) {
    return ortools::InvalidArgumentErrorBuilder()
           << "Constraint tolerance (" << options.constraint_tolerance
           << ") is negative.";
  }
  if (options.variable_tolerance < 0) {
    return ortools::InvalidArgumentErrorBuilder()
           << "Variable tolerance (" << options.variable_tolerance
           << ") is negative.";
  }
  if (options.integer_tolerance < 0) {
    return ortools::InvalidArgumentErrorBuilder()
           << "Integer tolerance (" << options.integer_tolerance
           << ") is negative.";
  }
  if (solution.size() != model.variable_size()) {
    return ortools::InvalidArgumentErrorBuilder()
           << "Solution size (" << solution.size()
           << ") does not match model variable size (" << model.variable_size()
           << ")";
  }
  for (const MPGeneralConstraintProto& general_constraint :
       model.general_constraint()) {
    if (!general_constraint.has_indicator_constraint()) {
      return ortools::InvalidArgumentErrorBuilder()
             << "General constraint " << general_constraint
             << " is not an indicator constraint.";
    }
  }
  int num_violations = 0;
  double max_observed_error = 0.0;
  const auto add_violation = [&](const std::string& message, const double error,
                                 const absl::SourceLocation loc =
                                     absl::SourceLocation::current()) {
    ++num_violations;
    max_observed_error = MaxPropagatesNan(max_observed_error, error);
    if (options.logger != nullptr &&
        num_violations <= options.max_logged_violations) {
      options.logger->LogInfo(loc.file_name(), loc.line(), message);
    }
  };
  for (int i = 0; i < model.variable_size(); ++i) {
    const double v = solution[i];
    if (std::isnan(v)) {
      add_violation(absl::StrCat("Variable ", model.variable(i).name(), "(", i,
                                 ") is NaN"),
                    kInf);
      continue;
    }
    const MPVariableProto& var = model.variable(i);
    if (SumIsNegative({v, -var.lower_bound(), options.variable_tolerance}) ||
        SumIsPositive({v, -var.upper_bound(), -options.variable_tolerance})) {
      add_violation(
          absl::StrCat("Variable ", var.name(), "(", i,
                       ") is out of bounds: ", RoundTripDoubleFormat(v),
                       " not in [", RoundTripDoubleFormat(var.lower_bound()),
                       ", ", RoundTripDoubleFormat(var.upper_bound()), "]"),
          MaxPropagatesNan(std::abs(v - var.lower_bound()),
                           std::abs(v - var.upper_bound())));
    }
    if (var.is_integer() &&
        (SumIsPositive({v, -std::round(v), -options.integer_tolerance}) ||
         SumIsPositive({std::round(v), -v, -options.integer_tolerance}))) {
      add_violation(
          absl::StrCat("Variable ", var.name(), "(", i,
                       ") is not an integer: ", RoundTripDoubleFormat(v)),
          std::abs(v - std::round(v)));
    }
  }
  const auto check_constraint =
      [&](const MPConstraintProto& constraint,
          const absl::string_view message_prefix) -> absl::Status {
    // Keep the vectors in scope to avoid reallocations.
    static std::vector<double> coefficients;
    coefficients.clear();
    coefficients.reserve(constraint.var_index_size() + 1);
    static std::vector<double> var_values;
    var_values.clear();
    var_values.reserve(constraint.var_index_size() + 1);
    for (int j = 0; j < constraint.var_index_size(); ++j) {
      coefficients.push_back(constraint.coefficient(j));

      auto var_index = constraint.var_index(j);
      if (var_index < 0 || var_index >= solution.size()) {
        return ortools::InvalidArgumentErrorBuilder()
               << "Invalid variable index " << var_index << " in "
               << message_prefix << " " << constraint.name();
      }
      var_values.push_back(solution[constraint.var_index(j)]);
    }
    var_values.push_back(options.constraint_tolerance);

    coefficients.push_back(1.0);
    if (constraint.has_lower_bound() &&
        !DotProductIsGreaterOrEqual(coefficients, var_values,
                                    constraint.lower_bound())) {
      // Skip tolerance.
      coefficients.back() = 0.0;

      const auto [lb, ub] = GetTightDotProductBounds(coefficients, var_values);
      add_violation(
          absl::StrCat(message_prefix, " '", constraint.name(), "'",
                       " >= ", RoundTripDoubleFormat(constraint.lower_bound()),
                       " violated by ",
                       RoundTripDoubleFormat(constraint.lower_bound() - ub),
                       " exact activity in [", RoundTripDoubleFormat(lb), ", ",
                       RoundTripDoubleFormat(ub), "]"),
          constraint.lower_bound() - ub);
    }

    coefficients.back() = -1.0;
    if (constraint.has_upper_bound() &&
        !DotProductIsSmallerOrEqual(coefficients, var_values,
                                    constraint.upper_bound())) {
      // Skip tolerance.
      coefficients.back() = 0.0;

      const auto [lb, ub] = GetTightDotProductBounds(coefficients, var_values);
      add_violation(
          absl::StrCat(message_prefix, " '", constraint.name(), "'",
                       " <= ", RoundTripDoubleFormat(constraint.upper_bound()),
                       " violated by ",
                       RoundTripDoubleFormat(lb - constraint.upper_bound()),
                       " exact activity in [", RoundTripDoubleFormat(lb), ", ",
                       RoundTripDoubleFormat(ub), "]"),
          lb - constraint.upper_bound());
    }
    return absl::OkStatus();
  };
  for (int i = 0; i < model.constraint_size(); ++i) {
    ABSL_RETURN_IF_ERROR(check_constraint(
        model.constraint(i), absl::StrCat("linear constraint ", i)));
  }
  for (int i = 0; i < model.general_constraint_size(); ++i) {
    const MPIndicatorConstraint& indicator_constraint =
        model.general_constraint(i).indicator_constraint();
    if (static_cast<int>(
            std::round(solution[indicator_constraint.var_index()])) ==
        indicator_constraint.var_value()) {
      ABSL_RETURN_IF_ERROR(
          check_constraint(indicator_constraint.constraint(),
                           absl::StrCat("indicator constraint ", i)));
    }
  }
  if (num_violations > 0 && options.logger != nullptr) {
    if (num_violations > options.max_logged_violations) {
      SOLVER_LOG(options.logger, " ... ",
                 num_violations - options.max_logged_violations,
                 " more violations not shown");
    }
    SOLVER_LOG(options.logger, "solution has ", num_violations,
               " violations with max error ",
               RoundTripDoubleFormat(max_observed_error),
               " with variable tolerance ",
               RoundTripDoubleFormat(options.variable_tolerance),
               " and constraint tolerance ",
               RoundTripDoubleFormat(options.constraint_tolerance));
  }
  return num_violations == 0;
}

absl::StatusOr<std::pair<double, double>> GetTightObjectiveBounds(
    const MPModelProto& model, const absl::Span<const double> solution) {
  if (solution.size() != model.variable_size()) {
    return ortools::InvalidArgumentErrorBuilder()
           << "Solution size (" << solution.size()
           << ") does not match model variable size (" << model.variable_size()
           << ")";
  }
  std::vector<double> coefficients;
  coefficients.reserve(model.variable_size() + 1);
  std::vector<double> var_values;
  var_values.reserve(model.variable_size() + 1);
  for (int i = 0; i < model.variable_size(); ++i) {
    coefficients.push_back(model.variable(i).objective_coefficient());
    var_values.push_back(solution[i]);
  }
  coefficients.push_back(1.0);
  var_values.push_back(model.objective_offset());

  return GetTightDotProductBounds(coefficients, var_values);
}

}  // namespace operations_research
