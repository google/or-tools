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

// Advanced linear programming example

#include <iostream>
#include <limits>
#include <ostream>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/time/time.h"
#include "ortools/base/init_google.h"
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
  math_opt::Model model("Advanced linear programming example");

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
  RETURN_IF_ERROR(result.termination.EnsureIsOptimal());

  std::cout << "Problem solved in " << result.solve_time() << std::endl;
  std::cout << "Objective value: " << result.objective_value() << std::endl;

  std::cout << "Variable values: ["
            << absl::StrJoin(Values(result.variable_values(), x), ", ") << "]"
            << std::endl;

  if (!result.has_dual_feasible_solution()) {
    // MathOpt does not require solvers to return a dual solution on optimal,
    // but most LP solvers always will.
    return util::InternalErrorBuilder()
           << "no dual solution was returned on optimal";
  }

  std::cout << "Constraint duals: ["
            << absl::StrJoin(Values(result.dual_values(), constraints), ", ")
            << "]" << std::endl;
  std::cout << "Reduced costs: ["
            << absl::StrJoin(Values(result.reduced_costs(), x), ", ") << "]"
            << std::endl;

  if (!result.has_basis()) {
    // MathOpt does not require solvers to return a basis on optimal, but most
    // Simplex LP solvers (like Glop) always will.
    return util::InternalErrorBuilder() << "no basis was returned on optimal";
  }

  std::cout << "Constraint basis status: ["
            << absl::StrJoin(Values(result.constraint_status(), constraints),
                             ", ", absl::StreamFormatter())
            << "]" << std::endl;

  std::cout << "Variable basis status: ["
            << absl::StrJoin(Values(result.variable_status(), x), ", ",
                             absl::StreamFormatter())
            << "]" << std::endl;

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
