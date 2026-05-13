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

#ifndef OR_TOOLS_MATH_OPT_LABS_DUALIZER_H_
#define OR_TOOLS_MATH_OPT_LABS_DUALIZER_H_

#include <utility>
#include <vector>

#include "absl/types/span.h"
#include "ortools/math_opt/cpp/math_opt.h"

namespace operations_research {
namespace math_opt {

// Uses LP duality to construct an extended formulation of
//
//   max_w{ a(w) * x : w in W} <= rhs
//
// where W is described by uncertainty_model (the variables of uncertainty_model
// are w). All the variables and constraints of the extended formulation are
// added to main_model.
//
// Requirements:
//   * x must be variables of main_model
//   * rhs must be a variable of main_model
//   * uncertainty_model must be an LP
//   * uncertain coefficient a(w)_i for x_i should be a LinearExpression of w.
//
// Input-only arguments:
//   * uncertainty_model
//   * rhs
//   * uncertain_coefficients: pairs [a(w)_i, x_i] for all i
// Input-output argument:
//   * main_model
void AddRobustConstraint(const Model& uncertainty_model, Variable rhs,
                         absl::Span<const std::pair<LinearExpression, Variable>>
                             uncertain_coefficients,
                         Model& main_model);

}  // namespace math_opt
}  // namespace operations_research
#endif  // OR_TOOLS_MATH_OPT_LABS_DUALIZER_H_
