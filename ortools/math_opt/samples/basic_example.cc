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

// Testing correctness of the code snippets in the comments of math_opt.h.

#include <iostream>
#include <limits>

#include "absl/status/statusor.h"
#include "ortools/base/init_google.h"
#include "ortools/base/logging.h"
#include "ortools/math_opt/cpp/math_opt.h"

namespace {

// Model the problem:
//   max 2.0 * x + y
//   s.t. x + y <= 1.5
//            x in {0.0, 1.0}
//            y in [0.0, 2.5]
//

void SolveVersion1() {
  using ::operations_research::math_opt::LinearConstraint;
  using ::operations_research::math_opt::Model;
  using ::operations_research::math_opt::SolveResult;
  using ::operations_research::math_opt::SolverType;
  using ::operations_research::math_opt::TerminationReason;
  using ::operations_research::math_opt::Variable;

  Model model("my_model");
  const Variable x = model.AddBinaryVariable("x");
  const Variable y = model.AddContinuousVariable(0.0, 2.5, "y");
  const LinearConstraint c = model.AddLinearConstraint(
      -std::numeric_limits<double>::infinity(), 1.5, "c");
  model.set_coefficient(c, x, 1.0);
  model.set_coefficient(c, y, 1.0);
  model.set_objective_coefficient(x, 2.0);
  model.set_objective_coefficient(y, 1.0);
  model.set_maximize();
  const SolveResult result = Solve(model, SolverType::kGscip).value();
  CHECK_EQ(result.termination.reason, TerminationReason::kOptimal)
      << result.termination;
  // The following code will print:
  //  objective value: 2.5
  //  value for variable x: 1
  std::cout << "objective value: " << result.objective_value()
            << "\nvalue for variable x: " << result.variable_values().at(x)
            << std::endl;
}

void SolveVersion2() {
  using ::operations_research::math_opt::LinearExpression;
  using ::operations_research::math_opt::Model;
  using ::operations_research::math_opt::SolveResult;
  using ::operations_research::math_opt::SolverType;
  using ::operations_research::math_opt::TerminationReason;
  using ::operations_research::math_opt::Variable;

  Model model("my_model");
  const Variable x = model.AddBinaryVariable("x");
  const Variable y = model.AddContinuousVariable(0.0, 2.5, "y");
  // We can directly use linear combinations of variables ...
  model.AddLinearConstraint(x + y <= 1.5, "c");
  // ... or build them incrementally.
  LinearExpression objective_expression;
  objective_expression += 2 * x;
  objective_expression += y;
  model.Maximize(objective_expression);
  const SolveResult result = Solve(model, SolverType::kGscip).value();
  CHECK_EQ(result.termination.reason, TerminationReason::kOptimal)
      << result.termination;
  // The following code will print:
  //  objective value: 2.5
  //  value for variable x: 1
  std::cout << "objective value: " << result.objective_value()
            << "\nvalue for variable x: " << result.variable_values().at(x)
            << std::endl;
}
}  // namespace

int main(int argc, char** argv) {
  InitGoogle(argv[0], &argc, &argv, true);
  SolveVersion1();
  SolveVersion2();
  return 0;
}
