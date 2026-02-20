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

// Testing correctness of the code snippets in the comments of math_opt.h.

#include <iostream>
#include <limits>
#include <ostream>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "ortools/base/init_google.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/cpp/math_opt.h"

namespace {

namespace math_opt = ::operations_research::math_opt;

// Model the problem:
//   max 2.0 * x + y
//   s.t. x + y <= 1.5
//            x in {0.0, 1.0}
//            y in [0.0, 2.5]
//
absl::Status Main() {
  math_opt::Model model("my_model");
  const math_opt::Variable x = model.AddBinaryVariable("x");
  const math_opt::Variable y = model.AddContinuousVariable(0.0, 2.5, "y");
  // We can directly use linear combinations of variables ...
  model.AddLinearConstraint(x + y <= 1.5, "c");
  // ... or build them incrementally.
  math_opt::LinearExpression objective_expression;
  objective_expression += 2 * x;
  objective_expression += y;
  model.Maximize(objective_expression);
  ASSIGN_OR_RETURN(const math_opt::SolveResult result,
                   Solve(model, math_opt::SolverType::kGscip));
  RETURN_IF_ERROR(result.termination.EnsureIsOptimalOrFeasible());
  std::cout << "Objective value: " << result.objective_value() << std::endl
            << "Value for variable x: " << result.variable_values().at(x)
            << std::endl;
  return absl::OkStatus();
}
}  // namespace

int main(int argc, char** argv) {
  InitGoogle(argv[0], &argc, &argv, true);
  const absl::Status status = Main();
  if (!status.ok()) {
    LOG(QFATAL) << status;
  }
  return 0;
}
