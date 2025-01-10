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

#include "ortools/math_opt/cpp/statistics.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <ios>
#include <optional>
#include <ostream>
#include <type_traits>
#include <utility>

#include "ortools/math_opt/cpp/model.h"
#include "ortools/math_opt/storage/model_storage.h"

namespace operations_research::math_opt {
namespace {

// Formatter for std::optional<Range> that uses the given width for std::setw().
struct OptionalRangeFormatter {
  OptionalRangeFormatter(const std::optional<Range>& range, const int width)
      : range(range), width(width) {}

  const std::optional<Range>& range;
  const int width;
};

std::ostream& operator<<(std::ostream& out, const OptionalRangeFormatter& fmt) {
  if (!fmt.range.has_value()) {
    out << "no finite values";
    return out;
  }

  out << '[' << std::setw(fmt.width) << fmt.range->first << ", "
      << std::setw(fmt.width) << fmt.range->second << ']';

  return out;
}

// Updates the input optional range with abs(v) if it is finite and non-zero.
void UpdateOptionalRange(std::optional<Range>& range, const double v) {
  if (std::isinf(v) || v == 0.0) {
    return;
  }

  const double abs_v = std::abs(v);
  if (range.has_value()) {
    range->first = std::min(range->first, abs_v);
    range->second = std::max(range->second, abs_v);
  } else {
    range = std::make_pair(abs_v, abs_v);
  }
}

}  // namespace

std::ostream& operator<<(std::ostream& out, const ModelRanges& ranges) {
  const auto last_precision = out.precision(2);
  const auto last_flags = out.flags();
  out.setf(std::ios_base::scientific, std::ios_base::floatfield);
  out.setf(std::ios_base::left, std::ios_base::adjustfield);
  // Numbers are printed in scientific notation with a precision of 2. Since
  // they are expected to be positive we can ignore the optional leading minus
  // sign. We thus expects `d.dde[+-]dd(d)?` (the exponent is at least 2 digits
  // but double can require 3 digits, with max +308 and min -308). Thus we can
  // use a width of 9 to align the ranges properly.
  constexpr int kWidth = 9;

  out << "Objective terms           : "
      << OptionalRangeFormatter(ranges.objective_terms, kWidth)
      << "\nVariable bounds           : "
      << OptionalRangeFormatter(ranges.variable_bounds, kWidth)
      << "\nLinear constraints bounds : "
      << OptionalRangeFormatter(ranges.linear_constraint_bounds, kWidth)
      << "\nLinear constraints coeffs : "
      << OptionalRangeFormatter(ranges.linear_constraint_coefficients, kWidth);

  out.precision(last_precision);
  out.flags(last_flags);

  return out;
}

ModelRanges ComputeModelRanges(const Model& model) {
  ModelRanges ranges;
  const auto objective = model.ObjectiveAsQuadraticExpression();
  for (const auto& [_, coeff] : objective.linear_terms()) {
    UpdateOptionalRange(ranges.objective_terms, coeff);
  }
  for (const auto& [_, coeff] : objective.quadratic_terms()) {
    UpdateOptionalRange(ranges.objective_terms, coeff);
  }
  for (const Variable& v : model.Variables()) {
    UpdateOptionalRange(ranges.variable_bounds, v.lower_bound());
    UpdateOptionalRange(ranges.variable_bounds, v.upper_bound());
  }
  for (const LinearConstraint& c : model.LinearConstraints()) {
    UpdateOptionalRange(ranges.linear_constraint_bounds, c.lower_bound());
    UpdateOptionalRange(ranges.linear_constraint_bounds, c.upper_bound());
  }
  for (const auto& [_row, _col, coeff] :
       model.storage()->linear_constraint_matrix()) {
    UpdateOptionalRange(ranges.linear_constraint_coefficients, coeff);
  }
  return ranges;
}

}  // namespace operations_research::math_opt
