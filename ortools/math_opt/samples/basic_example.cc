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

#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/status/statusor.h"
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
  using ::operations_research::math_opt::MathOpt;
  using ::operations_research::math_opt::Objective;
  using ::operations_research::math_opt::Result;
  using ::operations_research::math_opt::SolveParametersProto;
  using ::operations_research::math_opt::SolveResultProto;
  using ::operations_research::math_opt::Variable;

  MathOpt optimizer(operations_research::math_opt::SOLVER_TYPE_GSCIP,
                    "my_model");
  const Variable x = optimizer.AddBinaryVariable("x");
  const Variable y = optimizer.AddContinuousVariable(0.0, 2.5, "y");
  const LinearConstraint c = optimizer.AddLinearConstraint(
      -std::numeric_limits<double>::infinity(), 1.5, "c");
  c.set_coefficient(x, 1.0);
  c.set_coefficient(y, 1.0);
  const Objective obj = optimizer.objective();
  obj.set_linear_coefficient(x, 2.0);
  obj.set_linear_coefficient(y, 1.0);
  obj.set_maximize();
  const Result result = optimizer.Solve(SolveParametersProto()).value();
  for (const auto& warning : result.warnings) {
    std::cerr << "Solver warning: " << warning << std::endl;
  }
  CHECK_EQ(result.termination_reason, SolveResultProto::OPTIMAL)
      << result.termination_detail;
  // The following code will print:
  //  objective value: 2.5
  //  value for variable x: 1
  std::cout << "objective value: " << result.objective_value()
            << "\nvalue for variable x: " << result.variable_values().at(x)
            << std::endl;
}

void SolveVersion2() {
  using ::operations_research::math_opt::LinearExpression;
  using ::operations_research::math_opt::MathOpt;
  using ::operations_research::math_opt::Result;
  using ::operations_research::math_opt::SolveParametersProto;
  using ::operations_research::math_opt::SolveResultProto;
  using ::operations_research::math_opt::Variable;

  MathOpt optimizer(operations_research::math_opt::SOLVER_TYPE_GSCIP,
                    "my_model");
  const Variable x = optimizer.AddBinaryVariable("x");
  const Variable y = optimizer.AddContinuousVariable(0.0, 2.5, "y");
  // We can directly use linear combinations of variables ...
  optimizer.AddLinearConstraint(x + y <= 1.5, "c");
  // ... or build them incrementally.
  LinearExpression objective_expression;
  objective_expression += 2 * x;
  objective_expression += y;
  optimizer.objective().Maximize(objective_expression);
  const Result result = optimizer.Solve(SolveParametersProto()).value();
  for (const auto& warning : result.warnings) {
    std::cerr << "Solver warning: " << warning << std::endl;
  }
  CHECK_EQ(result.termination_reason, SolveResultProto::OPTIMAL)
      << result.termination_detail;
  // The following code will print:
  //  objective value: 2.5
  //  value for variable x: 1
  std::cout << "objective value: " << result.objective_value()
            << "\nvalue for variable x: " << result.variable_values().at(x)
            << std::endl;
}
}  // namespace

int main(int argc, char** argv) {
  google::InitGoogleLogging(argv[0]);
  absl::ParseCommandLine(argc, argv);
  SolveVersion1();
  SolveVersion2();
  return 0;
}
