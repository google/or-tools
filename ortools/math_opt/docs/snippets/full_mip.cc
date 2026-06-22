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

// Simple integer programming example for users manual

#include <iostream>
#include <limits>
#include <ostream>

#include "absl/log/check.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "ortools/base/init_google.h"
#include "ortools/math_opt/cpp/math_opt.h"

namespace {
using ::operations_research::math_opt::Model;
using ::operations_research::math_opt::SolveResult;
using ::operations_research::math_opt::SolverType;
using ::operations_research::math_opt::Variable;
using ::operations_research::math_opt::VariableMap;

constexpr double kInf = std::numeric_limits<double>::infinity();

void SolveMIP() {
  // [START_full_mip_example]
  Model mip_model("server allocation");

  // Variables
  const Variable x_1 = mip_model.AddContinuousVariable(-1.0, 1.5, "x_1");
  const Variable x_2 = mip_model.AddIntegerVariable(0.0, kInf, "x_2");
  const Variable x_3 = mip_model.AddIntegerVariable(-kInf, 10, "x_3");

  // Constraints, no need to save LinearConstraint objects if we don't use them.
  mip_model.AddLinearConstraint(x_1 + x_2 + x_3 <= 12.5, "c_1");
  mip_model.AddLinearConstraint(10 <= x_2 + x_3 <= 11, "c_2");
  mip_model.AddLinearConstraint(x_1 + x_2 >= 2.5, "c_3");

  // Objective
  mip_model.Maximize(3 * x_1 + x_2 + 2 * x_3);

  const absl::StatusOr<SolveResult> status_or_result =
      Solve(mip_model, SolverType::kGscip);
  CHECK_OK(status_or_result.status());
  const SolveResult& result = status_or_result.value();

  // Check that the problem has an optimal solution.
  QCHECK_OK(result.termination.EnsureIsOptimal());

  // Get some result information
  std::cout << "Problem solved in " << result.solve_time() << std::endl;
  std::cout << "Objective value: " << result.objective_value() << std::endl;

  // Get solution values
  const double x_1_value = result.variable_values().at(x_1);
  const double x_2_value = result.variable_values().at(x_2);
  const double x_3_value = result.variable_values().at(x_3);
  std::cout << "Variable values: [x_1=" << x_1_value << ", x_2=" << x_2_value
            << ", x_3=" << x_3_value << "]" << std::endl;
  // [END_full_mip_example]
}
}  // namespace

int main(int argc, char** argv) {
  InitGoogle(argv[0], &argc, &argv, true);
  SolveMIP();
  return 0;
}
