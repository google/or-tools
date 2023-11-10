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

#ifndef OR_TOOLS_MATH_OPT_LABS_GENERAL_CONSTRAINT_TO_MIP_H_
#define OR_TOOLS_MATH_OPT_LABS_GENERAL_CONSTRAINT_TO_MIP_H_

#include "absl/status/status.h"
#include "ortools/math_opt/cpp/math_opt.h"

namespace operations_research::math_opt {

// Takes a `model` and an `indicator_constraint` from that same model, and
// models that constraint using mixed-integer programming (MIP). This entails
// deleting `indicator_constraint` from `model` and adding new linear
// constraints.
//
// As of 2023-10-03, this formulation is a simple big-M formulation:
//
// Indicator constraint: x = 1  -->  lb ≤ <a, y> ≤ ub
// Becomes: if lb > -∞:  <a, y> ≥ lb + (LowerBound(<a, y>) - lb) (1 - x)
//          if ub < +∞:  <a, y> ≤ ub + (UpperBound(<a, y>) - ub) (1 - x),
//
// where LowerBound() and UpperBound() are from linear_expr_util.
//
// Will return an error if `indicator_constraint` is not valid or associated
// with `model`, or if the simple bound computations are not able to prove that
// the indicator constraint is MIP representable (namely, if LowerBound() and/or
// UpperBound() return -∞ or +∞, respectively).
absl::Status FormulateIndicatorConstraintAsMip(
    Model& model, IndicatorConstraint indicator_constraint);

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_LABS_GENERAL_CONSTRAINT_TO_MIP_H_
