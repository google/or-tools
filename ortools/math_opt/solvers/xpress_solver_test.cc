// Copyright 2010-2024 Google LLC
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

#include <ortools/base/status_matchers.h>

#include <ostream>

#include "absl/log/check.h"
#include "absl/status/statusor.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/init_google.h"
#include "ortools/math_opt/core/solver.h"
#include "ortools/math_opt/cpp/matchers.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/math_opt/solver_tests/lp_model_solve_parameters_tests.h"
#include "ortools/math_opt/solver_tests/lp_tests.h"
#include "ortools/port/proto_utils.h"

using namespace operations_research::math_opt;

SimpleLpTestParameters XpressDefaults() {
  return SimpleLpTestParameters(
      SolverType::kXpress, SolveParameters(), /*supports_duals=*/true,
      /*supports_basis=*/false,
      /*ensures_primal_ray=*/false, /*ensures_dual_ray=*/false,
      /*disallows_infeasible_or_unbounded=*/true);
}

TEST(XpressSolverTest, SimpleLpTwoVar) {
  double p = 0.0;
  auto model = std::make_unique<Model>();
  const Variable x_1 = model->AddVariable(0.0, 1.0, false, "x_1");
  const Variable x_2 = model->AddVariable(0.0, 1.0, false, "x_2");
  model->Maximize(2 * x_1 + x_2);
  const LinearConstraint y = model->AddLinearConstraint(x_1 + x_2 <= 1.5, "y");
  SolveResult result{Termination::Optimal(2.5)};
  result.solutions.push_back(Solution{
      .primal_solution =
          PrimalSolution{.variable_values = {{x_1, 1.0}, {x_2, 0.5}},
                         .objective_value = 2.5,
                         .feasibility_status = SolutionStatus::kFeasible},
      .dual_solution =
          DualSolution{.dual_values = {{y, 1.0}},
                       .reduced_costs = {{x_1, 1.0}, {x_2, 0.0}},
                       .objective_value = 2.5,
                       .feasibility_status = SolutionStatus::kFeasible}});

  // Set parameters, e.g. turn on logging.
  SolveArguments args;
  args.parameters.enable_output = true;

  // Solve and ensure an optimal solution was found with no errors.
  const absl::StatusOr<SolveResult> resultX =
      Solve(*model, SolverType::kXpress, args);
  for (auto vb : resultX->solutions[0].basis->variable_status) {
    std::cout << vb.first << " = " << vb.second << "\n";
  }
  for (auto vb : resultX->solutions[0].basis->constraint_status) {
    std::cout << vb.first << " = " << vb.second << "\n";
  }
}

INSTANTIATE_TEST_SUITE_P(XpressSolverLpTest, SimpleLpTest,
                         testing::Values(XpressDefaults()));

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  RUN_ALL_TESTS();
}
