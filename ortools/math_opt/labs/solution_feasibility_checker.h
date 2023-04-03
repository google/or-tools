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

#ifndef OR_TOOLS_MATH_OPT_LABS_SOLUTION_FEASIBILITY_CHECKER_H_
#define OR_TOOLS_MATH_OPT_LABS_SOLUTION_FEASIBILITY_CHECKER_H_

#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "ortools/math_opt/cpp/math_opt.h"

namespace operations_research::math_opt {

struct FeasibilityCheckerOptions {
  // Used for evaluating the feasibility of primal solution values with respect
  // to linear constraints and variable bounds.
  //
  // For example, variable values x are considered feasible with respect to a
  // constraint <a, x> ≤ b iff <a, x> ≤ b + absolute_constraint_tolerance.
  //
  // Cannot be negative or NaN.
  double absolute_constraint_tolerance = 1.0e-6;

  // An absolute tolerance used for evaluating the feasibility of a variable's
  // value with respect to integrality constraints on that variable, if present.
  //
  // For example, a value x for an integer variable is considered feasible with
  // respect to its integrality constraints iff
  // |x - round(x)| ≤ integrality_tolerance.
  //
  // Cannot be negative or NaN.
  double integrality_tolerance = 1.0e-5;

  // Absolute tolerance for evaluating if an expression is sufficiently close to
  // a particular value (usually zero, hence the name).
  //
  // This is used for evaluating if SOS1 and SOS2 constraints are satisfied, as
  // well as for evaluating indicator constraint feasibility (i.e., is the
  // indicator variable at its "activation value").
  //
  // For example, variable values x are considered feasible with respect to an
  // SOS1 constraint {expr_1(x), ..., expr_d(x)}-is-SOS1 iff there is at most
  // one j such that |expr_j(x)| > nonzero_tolerance.
  //
  // Cannot be negative or NaN.
  double nonzero_tolerance = 1.0e-5;
};

// Returns a subset of `model`s constraints that are violated at the point in
// `variable_values`. A point feasible with respect to all constraints will
// return an empty subset, which can be checked via ModelSubset::empty().
//
// Feasibility is checked within tolerances that can be configured in `options`.
//
// Returns an InvalidArgument error if `variable_values` does not contain an
// entry for each variable in `model` (and no extras).
absl::StatusOr<ModelSubset> CheckPrimalSolutionFeasibility(
    const Model& model, const VariableMap<double>& variable_values,
    const FeasibilityCheckerOptions& options = {});

// Returns a collection of strings that provide a human-readable representation
// of the `violated_constraints` (one string for each violated constraint).
// Useful for logging.
//
// Returns an InvalidArgument error if `variable_values` does not contain an
// entry for each variable in `model` (and no extras).
absl::StatusOr<std::vector<std::string>> ViolatedConstraintsAsStrings(
    const Model& model, const ModelSubset& violated_constraints,
    const VariableMap<double>& variable_values);

}  // namespace  operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_LABS_SOLUTION_FEASIBILITY_CHECKER_H_
