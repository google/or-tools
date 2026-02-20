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

// Simple SOCP problem showing that minimizing the perimeter of a rectangle
// with fixed area results in equal width and height.
#include <cmath>
#include <iostream>
#include <limits>
#include <ostream>

#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "ortools/base/init_google.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/cpp/math_opt.h"

ABSL_FLAG(double, area, 9, "Area lower bound.");

namespace {

namespace math_opt = ::operations_research::math_opt;

constexpr double kInf = std::numeric_limits<double>::infinity();

// We want to minimize the width plus height of a rectangle with area A.
//
// First we can relax to the area being at least A:
//   min  width + height
//   s.t. width*height >= A                 (Area)
//              width  in [0.0, infinity)
//              height in [0.0, infinity)
//
// Next we need to reformulate the area constraint as a second order cone
// constraint:
//   min  width + height
//   s.t. ||((width - height)/2, sqrt(A))||_2 <= (width + height)/2  (Area-SOCP)
//              width  in [0.0, infinity)
//              height in [0.0, infinity)
//
// To see how these two problems are equivalent, first note that by squaring
// both sides of constraint (Area-SOCP) we can see that it is equivalent to:
//   (width - height)^2/4 + A <= (width + height)^2/4
// because width + height >= 0. Expanding the two squares and reordering shows
// the equivalence to constraint (Area).
absl::Status Main(const double target_area) {
  math_opt::Model model;
  const math_opt::Variable width =
      model.AddContinuousVariable(0.0, kInf, "width");
  const math_opt::Variable height =
      model.AddContinuousVariable(0.0, kInf, "height");

  model.AddSecondOrderConeConstraint(
      {(width - height) / 2, std::sqrt(target_area)}, (width + height) / 2);
  model.Minimize(width + height);
  ASSIGN_OR_RETURN(const math_opt::SolveResult result,
                   Solve(model, math_opt::SolverType::kEcos));
  RETURN_IF_ERROR(result.termination.EnsureIsOptimalOrFeasible());
  std::cout << "Target area: " << target_area << std::endl;
  std::cout << "Area: "
            << result.variable_values().at(width) *
                   result.variable_values().at(height)
            << std::endl;
  std::cout << "Perimeter = " << result.objective_value() << std::endl;
  std::cout << "Width: " << result.variable_values().at(width) << std::endl;
  std::cout << "Height: " << result.variable_values().at(height) << std::endl;
  return absl::OkStatus();
}
}  // namespace

int main(int argc, char** argv) {
  InitGoogle(argv[0], &argc, &argv, true);
  const absl::Status status = Main(absl::GetFlag(FLAGS_area));
  if (!status.ok()) {
    LOG(QFATAL) << status;
  }
  return 0;
}
