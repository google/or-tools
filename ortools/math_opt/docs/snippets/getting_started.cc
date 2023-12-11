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

// Minimal example for Getting Started.
// [START imports]
#include <iostream>
#include <ostream>

#include "absl/log/check.h"
#include "absl/status/statusor.h"
#include "ortools/base/init_google.h"
#include "ortools/math_opt/cpp/math_opt.h"
// [END imports]

int main(int argc, char** argv) {
  InitGoogle(argv[0], &argc, &argv, true);

  // [START build_model]
  // Build the model.
  namespace math_opt = ::operations_research::math_opt;
  math_opt::Model lp_model("getting_started_lp");
  const math_opt::Variable x = lp_model.AddContinuousVariable(-1.0, 1.5, "x");
  const math_opt::Variable y = lp_model.AddContinuousVariable(0.0, 1.0, "y");
  lp_model.AddLinearConstraint(x + y <= 1.5, "c");
  lp_model.Maximize(x + 2 * y);
  // [END build_model]

  // [START solve_args]
  // Set parameters, e.g. turn on logging.
  math_opt::SolveArguments args;
  args.parameters.enable_output = true;
  // [END solve_args]

  // [START solve]
  // Solve and ensure an optimal solution was found with no errors.
  const absl::StatusOr<math_opt::SolveResult> result =
      math_opt::Solve(lp_model, math_opt::SolverType::kGlop, args);
  CHECK_OK(result.status());
  CHECK_OK(result->termination.EnsureIsOptimal());
  // [END solve]

  // [START print_result]
  // Print some information from the result.
  std::cout << "MathOpt solve succeeded" << std::endl;
  std::cout << "Objective value: " << result->objective_value() << std::endl;
  std::cout << "x: " << result->variable_values().at(x) << std::endl;
  std::cout << "y: " << result->variable_values().at(y) << std::endl;
  // [END print_result]

  return 0;
}
