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

#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/math_opt/solver_tests/callback_tests.h"
#include "ortools/math_opt/solver_tests/generic_tests.h"
#include "ortools/math_opt/solver_tests/infeasible_subsystem_tests.h"
#include "ortools/math_opt/solver_tests/invalid_input_tests.h"
#include "ortools/math_opt/solver_tests/logical_constraint_tests.h"
#include "ortools/math_opt/solver_tests/lp_incomplete_solve_tests.h"
#include "ortools/math_opt/solver_tests/lp_initial_basis_tests.h"
#include "ortools/math_opt/solver_tests/lp_model_solve_parameters_tests.h"
#include "ortools/math_opt/solver_tests/lp_parameter_tests.h"
#include "ortools/math_opt/solver_tests/lp_tests.h"
#include "ortools/math_opt/solver_tests/multi_objective_tests.h"
#include "ortools/math_opt/solver_tests/qc_tests.h"
#include "ortools/math_opt/solver_tests/qp_tests.h"
#include "ortools/math_opt/solver_tests/second_order_cone_tests.h"
#include "ortools/math_opt/solver_tests/status_tests.h"

namespace operations_research {
namespace math_opt {
namespace {
using testing::ValuesIn;

// TODO: implement missing LP features
INSTANTIATE_TEST_SUITE_P(
    XpressSolverLpTest, SimpleLpTest,
    testing::Values(SimpleLpTestParameters(
        SolverType::kXpress, SolveParameters(), /*supports_duals=*/true,
        /*supports_basis=*/true,
        /*ensures_primal_ray=*/false, /*ensures_dual_ray=*/false,
        /*disallows_infeasible_or_unbounded=*/true)));

// TODO: implement message callbacks
INSTANTIATE_TEST_SUITE_P(XpressMessageCallbackTest, MessageCallbackTest,
                         testing::Values(MessageCallbackTestParams(
                             SolverType::kXpress,
                             /*support_message_callback=*/false,
                             /*support_interrupter=*/false,
                             /*integer_variables=*/false, "")));

// TODO: implement callbacks
INSTANTIATE_TEST_SUITE_P(
    XpressCallbackTest, CallbackTest,
    testing::Values(CallbackTestParams(SolverType::kXpress,
                                       /*integer_variables=*/false,
                                       /*add_lazy_constraints=*/false,
                                       /*add_cuts=*/false,
                                       /*supported_events=*/{},
                                       /*all_solutions=*/std::nullopt,
                                       /*reaches_cut_callback*/ std::nullopt)));

INSTANTIATE_TEST_SUITE_P(XpressInvalidInputTest, InvalidInputTest,
                         testing::Values(InvalidInputTestParameters(
                             SolverType::kXpress,
                             /*use_integer_variables=*/false)));

InvalidParameterTestParams InvalidThreadsParameters() {
  SolveParameters params;
  params.threads = 2;
  return InvalidParameterTestParams(SolverType::kXpress, std::move(params),
                                    {"only supports parameters.threads = 1"});
}

// TODO: add all invalid parameters combinations
INSTANTIATE_TEST_SUITE_P(XpressInvalidParameterTest, InvalidParameterTest,
                         ValuesIn({InvalidThreadsParameters()}));

INSTANTIATE_TEST_SUITE_P(XpressGenericTest, GenericTest,
                         testing::Values(GenericTestParameters(
                             SolverType::kXpress, /*support_interrupter=*/false,
                             /*integer_variables=*/false,
                             /*expected_log=*/"Optimal solution found")));

// TODO: When XPRESS callbacks are supported, enable this test.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(TimeLimitTest);

// TODO: implement IIS support
INSTANTIATE_TEST_SUITE_P(XpressInfeasibleSubsystemTest, InfeasibleSubsystemTest,
                         testing::Values(InfeasibleSubsystemTestParameters(
                             {.solver_type = SolverType::kXpress,
                              .support_menu = {
                                  .supports_infeasible_subsystems = false}})));

}  // namespace
}  // namespace math_opt
}  // namespace operations_research