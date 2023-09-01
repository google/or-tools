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

// This file contains primal solution improvement heuristics.
#ifndef OR_TOOLS_MATH_OPT_LABS_SOLUTION_IMPROVEMENT_H_
#define OR_TOOLS_MATH_OPT_LABS_SOLUTION_IMPROVEMENT_H_

#include <algorithm>
#include <cmath>
#include <vector>

#include "absl/status/statusor.h"
#include "ortools/math_opt/cpp/math_opt.h"

namespace operations_research::math_opt {

// Maximum value for `integrality_tolerance` and for RoundedLowerBound() and
// RoundedUpperBound().
inline constexpr double kMaxIntegralityTolerance = 0.25;

// Options for MoveVariablesToTheirBestFeasibleValue.
struct MoveVariablesToTheirBestFeasibleValueOptions {
  // An absolute tolerance used for rounding the bounds of integer variables.
  //
  // It should be in [0, kMaxIntegralityTolerance] range; an error is returned
  // if the input tolerance is outside this range.
  //
  // See RoundedLowerBound() and RoundedUpperBound() for details.
  double integrality_tolerance = 0.0;
};

// Returns a solution that improves the objective value of the input model by
// moving the input variables' values to the their best feasible value (as
// defined by the objective) based on the constraints and other variables'
// values.
//
// The `input_solution` has to contain a value for each variable in the
// `model`. The input model must not be unbounded (and error is returned if this
// is the case).
//
// Only the value of the variables listed in `variables` are modified. The
// variables are considered in the order they appear in the vector. Thus the end
// result depends on this ordering:
//
// - If multiple variables appear in the same constraint, the first variable may
//   use up all the constraint's slack; preventing next variables to improve the
//   objective as much as they could.
//
//   This issue can be fixed by sorting variables by their objective
//   coefficient. But this may conflict with the order picked to solve
//   dependencies as explained below.
//
// - A variable improvement may be limited by another variable it depends on. If
//   it appears first and the second variable's value changes, we may end up
//   with some slack that the first variable could use.
//
//   This issue can be solved by either:
//
//   * Calling this function multiple times until no more variables are changed.
//
//   * Sorting the input `variables` in a correct order so that the limiting
//     variable appear first.
//
// The variables' values are changed in the direction that improves the
// objective. Variables that are not in the objective are not modified.
//
// This function is typically useful when solving MIP with a non-zero gap or
// when the time limit interrupts the solve early. In those cases a MIP solver
// can return a solution where some variables can trivially be changed to
// improve the objective but since the solution fits in the termination criteria
// (either the gap or the time limit) the solver did not do it.
absl::StatusOr<VariableMap<double>> MoveVariablesToTheirBestFeasibleValue(
    const Model& model, const VariableMap<double>& input_solution,
    const std::vector<Variable>& variables,
    const MoveVariablesToTheirBestFeasibleValueOptions& options = {});

// Returns the lower bound of the variable, rounding it up when the variable is
// integral and the bound's fractional value is outside the tolerance.
//
// For example if the lower bound of an integer variable is 1.0000000000000002
// and the tolerance is 0.0 this function will return 2.0. If the tolerance is
// 1e-6 though this function will return 1.0.
//
// Tolerance should be a non-negative value < kMaxIntegralityTolerance (usually
// much smaller). A negative input value (or NaN) will be considered 0.0, a
// value >= kMaxIntegralityTolerance will be considered kMaxIntegralityTolerance
// (using a tolerance like 0.5 would lead to odd behavior for ties as integral
// bounds could be rounded to the next integer. For example with the integer
// 2^53 - 1, 2^53 - 1 + 0.5 = 2^53)
inline double RoundedLowerBound(const Variable v, const double tolerance) {
  // We use std::fmax() to treat NaN as 0.0.
  const double offset =
      std::min(std::fmax(0.0, tolerance), kMaxIntegralityTolerance);
  return v.is_integer() ? std::ceil(v.lower_bound() - offset) : v.lower_bound();
}

// Same as RoundedLowerBound() but for upper-bound.
inline double RoundedUpperBound(const Variable v, const double tolerance) {
  // See comment in RoundedLowerBound().
  const double offset =
      std::min(std::fmax(0.0, tolerance), kMaxIntegralityTolerance);
  return v.is_integer() ? std::floor(v.upper_bound() + offset)
                        : v.upper_bound();
}

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_LABS_SOLUTION_IMPROVEMENT_H_
