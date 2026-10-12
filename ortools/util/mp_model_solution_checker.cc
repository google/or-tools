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
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/status_macros.h"
#include "absl/strings/string_view.h"
#include "absl/types/source_location.h"
#include "absl/types/span.h"
#include "ortools/base/status_builder.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/sat/mp_model_wrapper.h"
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

// Class that checks a single constraint on a fixed solution and adds violations
// to the violation adder.
//
// We use a class to reuse memory across multiple constraints.
template <typename ViolationAdder>
class ConstraintChecker {
 public:
  ConstraintChecker(
      const ViolationAdder& add_violation ABSL_ATTRIBUTE_LIFETIME_BOUND,
      const CheckerOptions& options ABSL_ATTRIBUTE_LIFETIME_BOUND,
      const absl::Span<const double> solution ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : add_violation_(add_violation), options_(options), solution_(solution) {}

  absl::Status Check(const MPConstraintProto& constraint,
                     const absl::string_view message_prefix) {
    coefficients_.clear();
    coefficients_.reserve(constraint.var_index_size() + 1);

    var_values_.clear();
    var_values_.reserve(constraint.var_index_size() + 1);
    for (int j = 0; j < constraint.var_index_size(); ++j) {
      coefficients_.push_back(constraint.coefficient(j));

      auto var_index = constraint.var_index(j);
      if (var_index < 0 || var_index >= solution_.size()) {
        return ortools::InvalidArgumentErrorBuilder()
               << "Invalid variable index " << var_index << " in "
               << message_prefix << " " << constraint.name();
      }
      var_values_.push_back(solution_[constraint.var_index(j)]);
    }
    var_values_.push_back(options_.constraint_tolerance);

    coefficients_.push_back(1.0);
    if (constraint.has_lower_bound() &&
        !DotProductIsGreaterOrEqual(coefficients_, var_values_,
                                    constraint.lower_bound())) {
      // Skip tolerance.
      coefficients_.back() = 0.0;

      const auto [lb, ub] =
          GetTightDotProductBounds(coefficients_, var_values_);
      add_violation_(
          absl::StrCat(message_prefix, " '", constraint.name(), "'",
                       " >= ", RoundTripDoubleFormat(constraint.lower_bound()),
                       " violated by ",
                       RoundTripDoubleFormat(constraint.lower_bound() - ub),
                       " exact activity in [", RoundTripDoubleFormat(lb), ", ",
                       RoundTripDoubleFormat(ub), "]"),
          constraint.lower_bound() - ub);
    }

    coefficients_.back() = -1.0;
    if (constraint.has_upper_bound() &&
        !DotProductIsSmallerOrEqual(coefficients_, var_values_,
                                    constraint.upper_bound())) {
      // Skip tolerance.
      coefficients_.back() = 0.0;

      const auto [lb, ub] =
          GetTightDotProductBounds(coefficients_, var_values_);
      add_violation_(
          absl::StrCat(message_prefix, " '", constraint.name(), "'",
                       " <= ", RoundTripDoubleFormat(constraint.upper_bound()),
                       " violated by ",
                       RoundTripDoubleFormat(lb - constraint.upper_bound()),
                       " exact activity in [", RoundTripDoubleFormat(lb), ", ",
                       RoundTripDoubleFormat(ub), "]"),
          lb - constraint.upper_bound());
    }
    return absl::OkStatus();
  }

 private:
  const ViolationAdder& add_violation_;
  const CheckerOptions& options_;
  const absl::Span<const double> solution_;

  // Avoid reallocations.
  std::vector<double> coefficients_;
  std::vector<double> var_values_;
};

// Deduction guide for ConstraintChecker.
template <class ViolationAdder>
ConstraintChecker(const ViolationAdder&, const CheckerOptions&,
                  absl::Span<const double>)
    -> ConstraintChecker<ViolationAdder>;

}  // namespace

absl::StatusOr<bool> SolutionIsFeasible(const sat::MPModelProtoWrapper& model,
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
  if (solution.size() != model.num_variables()) {
    return ortools::InvalidArgumentErrorBuilder()
           << "Solution size (" << solution.size()
           << ") does not match model variable size (" << model.num_variables()
           << ")";
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
  for (int i = 0; i < model.num_variables(); ++i) {
    const double v = solution[i];
    if (std::isnan(v)) {
      add_violation(absl::StrCat("Variable ", model.variables(i).name(), "(", i,
                                 ") is NaN"),
                    kInf);
      continue;
    }
    const MPVariableProto& var = model.variables(i);
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

  ConstraintChecker checker(add_violation, options, solution);
  for (int i = 0; i < model.num_constraints(); ++i) {
    const sat::EnforcedLinearConstraint& ct = model.constraints(i);
    bool is_active = true;
    for (int k = 0; k < ct.enforcement_vars().size(); ++k) {
      const int enf_var = ct.enforcement_vars()[k];
      if (static_cast<int64_t>(std::round(solution[enf_var])) !=
          ct.enforcement_values()[k]) {
        is_active = false;
        break;
      }
    }
    if (!is_active) continue;
    ABSL_RETURN_IF_ERROR(
        checker.Check(ct.proto(), absl::StrCat("indicator constraint ", i)));
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
    const sat::MPModelProtoWrapper& model,
    const absl::Span<const double> solution) {
  if (solution.size() != model.num_variables()) {
    return ortools::InvalidArgumentErrorBuilder()
           << "Solution size (" << solution.size()
           << ") does not match model variable size (" << model.num_variables()
           << ")";
  }
  std::vector<double> coefficients;
  coefficients.reserve(model.num_variables() + 1);
  std::vector<double> var_values;
  var_values.reserve(model.num_variables() + 1);
  for (int i = 0; i < model.num_variables(); ++i) {
    coefficients.push_back(model.variables(i).objective_coefficient());
    var_values.push_back(solution[i]);
  }
  coefficients.push_back(1.0);
  var_values.push_back(model.objective_offset());

  return GetTightDotProductBounds(coefficients, var_values);
}

}  // namespace operations_research
