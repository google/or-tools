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

// Simple integer programming example

#include <cmath>
#include <iostream>
#include <limits>
#include <ostream>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/time/time.h"
#include "ortools/base/init_google.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/cpp/math_opt.h"

namespace {

namespace math_opt = ::operations_research::math_opt;

constexpr double kInf = std::numeric_limits<double>::infinity();

// Model and solve the problem:
//   max x + 10 * y
//   s.t. x + 7 * y <= 17.5
//                x <= 3.5
//            x in {0.0, 1.0, 2.0, ...,
//            y in {0.0, 1.0, 2.0, ...,
//
absl::Status Main() {
  math_opt::Model model("Integer programming example");

  // Variables
  const math_opt::Variable x = model.AddIntegerVariable(0.0, kInf, "x");
  const math_opt::Variable y = model.AddIntegerVariable(0.0, kInf, "y");

  // Constraints
  model.AddLinearConstraint(x + 7 * y <= 17.5, "c1");
  model.AddLinearConstraint(x <= 3.5, "c2");

  // Objective
  model.Maximize(x + 10 * y);

  ASSIGN_OR_RETURN(const math_opt::SolveResult result,
                   Solve(model, math_opt::SolverType::kGscip));
  RETURN_IF_ERROR(result.termination.EnsureIsOptimalOrFeasible());
  // A feasible solution is always available on termination reason kOptimal, and
  // kFeasible, but in the later case the solution may be sub-optimal.
  std::cout << "Problem solved in " << result.solve_time() << std::endl;
  std::cout << "Objective value: " << result.objective_value() << std::endl;
  std::cout << "Variable values: [x="
            << std::round(result.variable_values().at(x))
            << ", y=" << std::round(result.variable_values().at(y)) << "]"
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
