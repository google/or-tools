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

// Simple linear programming example

#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/time/time.h"
#include "ortools/base/init_google.h"
#include "ortools/base/logging.h"
#include "ortools/base/status_builder.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/cpp/math_opt.h"

namespace {

namespace math_opt = ::operations_research::math_opt;

constexpr double kInf = std::numeric_limits<double>::infinity();

// Model and solve the problem:
//   max  10 * x0 + 6 * x1 + 4 * x2
//   s.t. 10 * x0 + 4 * x1 + 5 * x2 <= 600
//         2 * x0 + 2 * x1 + 6 * x2 <= 300
//                     x0 + x1 + x2 <= 100
//            x0 in [0, infinity)
//            x1 in [0, infinity)
//            x2 in [0, infinity)
//
absl::Status Main() {
  math_opt::Model model("Linear programming example");

  // Variables
  std::vector<math_opt::Variable> x;
  for (int j = 0; j < 3; j++) {
    x.push_back(model.AddContinuousVariable(0.0, kInf, absl::StrCat("x", j)));
  }

  // Constraints
  std::vector<math_opt::LinearConstraint> constraints;
  constraints.push_back(
      model.AddLinearConstraint(10 * x[0] + 4 * x[1] + 5 * x[2] <= 600, "c1"));
  constraints.push_back(
      model.AddLinearConstraint(2 * x[0] + 2 * x[1] + 6 * x[2] <= 300, "c2"));
  // sum(x[i]) <= 100
  constraints.push_back(model.AddLinearConstraint(Sum(x) <= 100, "c3"));

  // Objective
  model.Maximize(10 * x[0] + 6 * x[1] + 4 * x[2]);

  ASSIGN_OR_RETURN(const math_opt::SolveResult result,
                   Solve(model, math_opt::SolverType::kGlop));
  if (result.termination.reason != math_opt::TerminationReason::kOptimal) {
    return util::InternalErrorBuilder()
           << "model failed to solve to optimality" << result.termination;
  }

  std::cout << "Problem solved in " << result.solve_time() << std::endl;
  std::cout << "Objective value: " << result.objective_value() << std::endl;

  std::cout << "Variable values: ["
            << absl::StrJoin(result.variable_values().Values(x), ", ") << "]"
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
