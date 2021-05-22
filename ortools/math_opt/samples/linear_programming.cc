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

// Simple linear programming example

#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/time/time.h"
#include "ortools/base/logging.h"
#include "ortools/math_opt/cpp/math_opt.h"

namespace {
using ::operations_research::math_opt::LinearConstraint;
using ::operations_research::math_opt::LinearExpression;
using ::operations_research::math_opt::MathOpt;
using ::operations_research::math_opt::Result;
using ::operations_research::math_opt::SolveParametersProto;
using ::operations_research::math_opt::SOLVER_TYPE_GLOP;
using ::operations_research::math_opt::SolveResultProto;
using ::operations_research::math_opt::SolveStatsProto;
using ::operations_research::math_opt::Sum;
using ::operations_research::math_opt::Variable;

constexpr double kInf = std::numeric_limits<double>::infinity();

// Model and solve the problem:
//   max 10 * x0 + 6 * x1 + 4 * x2
//   s.t. 10 * x0 + 4 * x1 + 5 * x2 <= 600
//         2 * x0 + 2 * x1 + 6 * x2 <= 300
//                     x0 + x1 + x2 <= 100
//            x0 in [0, infinity)
//            x1 in [0, infinity)
//            x2 in [0, infinity)
//
void SolveSimpleLp() {
  MathOpt optimizer(SOLVER_TYPE_GLOP, "Linear programming example");

  // Variables
  std::vector<Variable> x;
  for (int j = 0; j < 3; j++) {
    x.push_back(
        optimizer.AddContinuousVariable(0.0, kInf, absl::StrCat("x", j)));
  }

  // Constraints
  std::vector<LinearConstraint> constraints;
  constraints.push_back(optimizer.AddLinearConstraint(
      10 * x[0] + 4 * x[1] + 5 * x[2] <= 600, "c1"));
  constraints.push_back(optimizer.AddLinearConstraint(
      2 * x[0] + 2 * x[1] + 6 * x[2] <= 300, "c2"));
  // sum(x[i]) <= 100
  constraints.push_back(optimizer.AddLinearConstraint(Sum(x) <= 100, "c3"));

  // Objective
  optimizer.objective().Maximize(10 * x[0] + 6 * x[1] + 4 * x[2]);

  std::cout << "Num variables: " << optimizer.num_variables() << std::endl;
  std::cout << "Num constraints: " << optimizer.num_linear_constraints()
            << std::endl;

  const Result result = optimizer.Solve(SolveParametersProto()).value();

  // Check for warnings.
  for (const auto& warning : result.warnings) {
    LOG(ERROR) << "Solver warning: " << warning << std::endl;
  }
  // Check that the problem has an optimal solution.
  QCHECK_EQ(result.termination_reason, SolveResultProto::OPTIMAL)
      << "Failed to find an optimal solution: " << result.termination_detail;

  std::cout << "Problem solved in " << result.solve_time() << std::endl;
  std::cout << "Objective value: " << result.objective_value() << std::endl;

  std::cout << "Variable values: ["
            << absl::StrJoin(result.variable_values().Values(x), ", ") << "]"
            << std::endl;
  std::cout << "Constraint duals: ["
            << absl::StrJoin(result.dual_values().Values(constraints), ", ")
            << "]" << std::endl;
  std::cout << "Reduced costs: ["
            << absl::StrJoin(result.reduced_costs().Values(x), ", ") << "]"
            << std::endl;
  const SolveStatsProto& stat = result.solve_stats;
  std::cout << "Simplex iterations: " << stat.simplex_iterations() << std::endl;
  std::cout << "Barrier iterations: " << stat.barrier_iterations() << std::endl;

  // TODO(user): add basis statuses when they are included in Result
}
}  // namespace

int main(int argc, char** argv) {
  google::InitGoogleLogging(argv[0]);
  absl::ParseCommandLine(argc, argv);
  SolveSimpleLp();
  return 0;
}
