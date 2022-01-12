// Copyright 2010-2021 Google LLC
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

// Simple integer programming example

#include <iostream>
#include <limits>

#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "ortools/base/logging.h"
#include "ortools/math_opt/cpp/math_opt.h"

namespace {
using ::operations_research::math_opt::Model;
using ::operations_research::math_opt::SolveResult;
using ::operations_research::math_opt::SolverType;
using ::operations_research::math_opt::TerminationReason;
using ::operations_research::math_opt::Variable;
using ::operations_research::math_opt::VariableMap;

constexpr double kInf = std::numeric_limits<double>::infinity();

// Model and solve the problem:
//   max x + 10 * y
//   s.t. x + 7 * y <= 17.5
//                x <= 3.5
//            x in {0.0, 1.0, 2.0, ...,
//            y in {0.0, 1.0, 2.0, ...,
//
void SolveSimpleMIP() {
  Model model("Integer programming example");

  // Variables
  const Variable x = model.AddIntegerVariable(0.0, kInf, "x");
  const Variable y = model.AddIntegerVariable(0.0, kInf, "y");

  // Constraints
  model.AddLinearConstraint(x + 7 * y <= 17.5, "c1");
  model.AddLinearConstraint(x <= 3.5, "c2");

  // Objective
  model.Maximize(x + 10 * y);

  std::cout << "Num variables: " << model.num_variables() << std::endl;
  std::cout << "Num constraints: " << model.num_linear_constraints()
            << std::endl;

  const SolveResult result = Solve(model, SolverType::kGscip).value();

  // Check for warnings.
  for (const auto& warning : result.warnings) {
    LOG(ERROR) << "Solver warning: " << warning << std::endl;
  }
  // Check that the problem has an optimal solution.
  QCHECK_EQ(result.termination.reason, TerminationReason::kOptimal)
      << "Failed to find an optimal solution: " << result.termination;

  std::cout << "Problem solved in " << result.solve_time() << std::endl;
  std::cout << "Objective value: " << result.objective_value() << std::endl;

  const double x_val = result.variable_values().at(x);
  const double y_val = result.variable_values().at(y);

  std::cout << "Variable values: [x=" << x_val << ", y=" << y_val << "]"
            << std::endl;
}
}  // namespace

int main(int argc, char** argv) {
  google::InitGoogleLogging(argv[0]);
  absl::ParseCommandLine(argc, argv);
  SolveSimpleMIP();
  return 0;
}
