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

// Missing tests that HiGHS could support (may require unimplemented features):
//  * invalid_input_tests
//  * non-message callback tests
//  * lp_incomplete_solve_tests
//  * lp_initial_basis_tests
//  * qp_tests
//
// There is no way to turn off cuts when using highs as a MIP solver, this
// forces us to disable some tests.

#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/types.h"
#include "ortools/math_opt/cpp/matchers.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/math_opt/solver_tests/callback_tests.h"
#include "ortools/math_opt/solver_tests/generic_tests.h"
#include "ortools/math_opt/solver_tests/infeasible_subsystem_tests.h"
#include "ortools/math_opt/solver_tests/ip_model_solve_parameters_tests.h"
#include "ortools/math_opt/solver_tests/ip_parameter_tests.h"
#include "ortools/math_opt/solver_tests/lp_model_solve_parameters_tests.h"
#include "ortools/math_opt/solver_tests/lp_parameter_tests.h"
#include "ortools/math_opt/solver_tests/lp_tests.h"
#include "ortools/math_opt/solver_tests/min_cost_flow_tests.h"
#include "ortools/math_opt/solver_tests/mip_tests.h"
#include "ortools/math_opt/solver_tests/multi_objective_tests.h"
#include "ortools/math_opt/solver_tests/status_tests.h"
#include "ortools/math_opt/solver_tests/test_models.h"
#include "ortools/math_opt/solvers/highs.pb.h"
#include "ortools/math_opt/testing/param_name.h"

namespace operations_research::math_opt {
namespace {

using ::testing::AllOf;
using ::testing::HasSubstr;
using ::testing::Values;
using ::testing::ValuesIn;
using ::testing::status::IsOkAndHolds;
using ::testing::status::StatusIs;


SimpleLpTestParameters MosekDefaults() {
  return SimpleLpTestParameters(
      SolverType::kMosek, {}, /*supports_duals=*/true,
      /*supports_basis=*/true,
      /*ensures_primal_ray=*/false, /*ensures_dual_ray=*/false,
      // Note: there is a highs specific option we can use to turn this to true
      // "allow_unbounded_or_infeasible"
      // https://github.com/ERGO-Code/HiGHS/blob/master/src/lp_data/HighsOptions.h#L321
      /*disallows_infeasible_or_unbounded=*/false);
}

INSTANTIATE_TEST_SUITE_P(MosekSolverTest,
                         SimpleLpTest,
                         Values(MosekDefaults()));

TEST(MosekSolverTest, FractionalBoundsForIntegerVariables) {
  Model model;
  const Variable x = model.AddIntegerVariable(0.0, 1.5);
  model.Maximize(x);

  EXPECT_THAT(Solve(model, SolverType::kMosek),
              IsOkAndHolds(IsOptimalWithSolution(1.0, {{x, 1.0}})));
}

TEST(MosekSolverTest, SimpleLinear1) {
  Model model;

  // Solve the problem:
  //  max  3 x0 + x1 + 5 x2 + x3
  //  such that
  //               3 x0 + x1 + 2 x2      = 30.0
  //       15.0 <  2 x0 + x1 + 3 x2 + x3
  //                    2 x1 +      3 x3 < 25.0
  //       0 < x0,x1,x2,x3
  //       x1 < 10


  auto x0 = model.AddContinuousVariable(0.0, std::numeric_limits<double>::infinity(),"x0");
  auto x1 = model.AddContinuousVariable(0.0, 1.0, "x1");
  auto x2 = model.AddContinuousVariable(0.0, std::numeric_limits<double>::infinity(),"x2");
  auto x3 = model.AddContinuousVariable(0.0, std::numeric_limits<double>::infinity(),"x3");

  model.SetObjective(3 * x0 + x1 + 5 * x2 + x3, true);

  //model.set_objective_coefficient(x0,3.0);
  //model.set_objective_coefficient(x1,1.0);
  //model.set_objective_coefficient(x2,5.0);
  //model.set_objective_coefficient(x3,1.0);

  model.AddLinearConstraint(3 * x0 +     x1 + 2 * x2          == 30.0);
  model.AddLinearConstraint(2 * x0 +     x1 + 3 * x2 +     x3 >= 15.0);
  model.AddLinearConstraint(         2 * x1 +        + 3 * x3 <= 25.0);

  auto res = Solve(model, SolverType::kMosek);

  auto sol = res->solutions[0];
  std::cerr << __FILE__ << ":" << __LINE__ << ": " << sol.primal_solution->objective_value << std::endl;

  EXPECT_THAT(res,
              IsOkAndHolds(IsOptimalWithSolution(83.3333333333333333, {{x0, 0.0},{x1, 0.0}, {x2, 15.0}, {x3, 8.333333333333333333}})));
}

TEST(MosekSolverTest, SimpleLinear2) {
  Model model;

  const Variable x_1 = model.AddVariable(0.0, 1.0, false, "x_1");
  const Variable x_2 = model.AddVariable(0.0, 1.0, false, "x_2");
  model.Maximize(2 * x_1 + x_2);
  const LinearConstraint y =
      model.AddLinearConstraint(0.0 <= x_1 + x_2 <= 1.5, "y");

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

  auto sol = Solve(model, SolverType::kMosek);

  // std::cerr << __FILE__ << ":" << __LINE__ << ": Primal " << sol->solutions[0].primal_solution->objective_value << std::endl;
  // for (auto & v : sol->solutions[0].primal_solution->variable_values) {
  //     std::cerr << __FILE__ << ":" << __LINE__ << "     " << v.first << " : " << v.second << std::endl;
  // }
  // if (sol->solutions[0].dual_solution.has_value()) {
  //     std::cerr << __FILE__ << ":" << __LINE__ << ": Dual " << sol->solutions[0].dual_solution.has_value() << std::endl;
  //     for (auto & v : sol->solutions[0].dual_solution->dual_values) {
  //         std::cerr << __FILE__ << ":" << __LINE__ << "     " << v.first << " : " << v.second << std::endl;
  //     }
  // }

  EXPECT_THAT(sol,IsOkAndHolds(IsConsistentWith(result,{.check_dual = 1})));
}



TEST(MosekSolverTest, SimpleLinear3) {
    Model model;

    const Variable x_1 = model.AddVariable(0.0, 1.0, false, "x_1");
    const Variable x_2 = model.AddVariable(0.0, 1.0, false, "x_2");
    model.Minimize(2 * x_1 + x_2);
    const LinearConstraint y = model.AddLinearConstraint(0.5 <= x_1 + x_2 <= 1.5, "y");
    SolveResult result{Termination::Optimal(0.5)};
    result.solutions.push_back(Solution{
        .primal_solution =
            PrimalSolution{.variable_values = {{x_1, 0.0}, {x_2, 0.5}},
                           .objective_value = 0.5,
                           .feasibility_status = SolutionStatus::kFeasible},
        .dual_solution =
            DualSolution{.dual_values = {{y, 1.0}},
                         .reduced_costs = {{x_1, 1.0}, {x_2, 0.0}},
                         .objective_value = 0.5,
                         .feasibility_status = SolutionStatus::kFeasible}});


    auto sol = Solve(model, SolverType::kMosek);

    EXPECT_THAT(sol,IsOkAndHolds( IsConsistentWith(result, {.check_dual = 1})));
}


}
}
