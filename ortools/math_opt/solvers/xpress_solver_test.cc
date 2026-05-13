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

#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/log.h"
#include "gtest/gtest.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/math_opt/solver_tests/callback_tests.h"
#include "ortools/math_opt/solver_tests/generic_tests.h"
#include "ortools/math_opt/solver_tests/infeasible_subsystem_tests.h"
#include "ortools/math_opt/solver_tests/invalid_input_tests.h"
#include "ortools/math_opt/solver_tests/logical_constraint_tests.h"
#include "ortools/math_opt/solver_tests/lp_incomplete_solve_tests.h"
#include "ortools/math_opt/solver_tests/lp_model_solve_parameters_tests.h"
#include "ortools/math_opt/solver_tests/lp_parameter_tests.h"
#include "ortools/math_opt/solver_tests/lp_tests.h"
#include "ortools/math_opt/solver_tests/multi_objective_tests.h"
#include "ortools/math_opt/solver_tests/qc_tests.h"
#include "ortools/math_opt/solver_tests/qp_tests.h"
#include "ortools/math_opt/solver_tests/second_order_cone_tests.h"
#include "ortools/math_opt/solver_tests/status_tests.h"
#include "ortools/xpress/environment.h"

namespace operations_research {
namespace math_opt {
namespace {
using testing::ValuesIn;

INSTANTIATE_TEST_SUITE_P(
    XpressSolverLpTest, SimpleLpTest,
    testing::Values(SimpleLpTestParameters(
        SolverType::kXpress, SolveParameters(), /*supports_duals=*/true,
        /*supports_basis=*/true,
        /*ensures_primal_ray=*/false, /*ensures_dual_ray=*/false,
        /*disallows_infeasible_or_unbounded=*/true)));

INSTANTIATE_TEST_SUITE_P(XpressLpModelSolveParametersTest,
                         LpModelSolveParametersTest,
                         testing::Values(LpModelSolveParametersTestParameters(
                             SolverType::kXpress, /*exact_zeros=*/true,
                             /*supports_duals=*/true,
                             /*supports_primal_only_warm_starts=*/false)));

INSTANTIATE_TEST_SUITE_P(
    XpressLpParameterTest, LpParameterTest,
    testing::Values(LpParameterTestParams(SolverType::kXpress,
                                          /*supports_simplex=*/true,
                                          /*supports_barrier=*/true,
                                          /*supports_first_order=*/false,
                                          /*supports_random_seed=*/false,
                                          /*supports_presolve=*/false,
                                          /*supports_cutoff=*/false,
                                          /*supports_objective_limit=*/false,
                                          /*supports_best_bound_limit=*/false,
                                          /*reports_limits=*/false)));

INSTANTIATE_TEST_SUITE_P(
    XpressPrimalSimplexLpIncompleteSolveTest, LpIncompleteSolveTest,
    testing::Values(LpIncompleteSolveTestParams(
        SolverType::kXpress,
        /*lp_algorithm=*/LPAlgorithm::kPrimalSimplex,
        /*supports_iteration_limit=*/true, /*supports_initial_basis=*/false,
        /*supports_incremental_solve=*/false, /*supports_basis=*/true,
        /*supports_presolve=*/false, /*check_primal_objective=*/true,
        /*primal_solution_status_always_set=*/true,
        /*dual_solution_status_always_set=*/true)));
INSTANTIATE_TEST_SUITE_P(
    XpressDualSimplexLpIncompleteSolveTest, LpIncompleteSolveTest,
    testing::Values(LpIncompleteSolveTestParams(
        SolverType::kXpress,
        /*lp_algorithm=*/LPAlgorithm::kDualSimplex,
        /*supports_iteration_limit=*/true, /*supports_initial_basis=*/false,
        /*supports_incremental_solve=*/false, /*supports_basis=*/true,
        /*supports_presolve=*/false, /*check_primal_objective=*/true,
        /*primal_solution_status_always_set=*/true,
        /*dual_solution_status_always_set=*/true)));

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(IncrementalLpTest);

INSTANTIATE_TEST_SUITE_P(XpressMessageCallbackTest, MessageCallbackTest,
                         testing::Values(MessageCallbackTestParams(
                             SolverType::kXpress,
                             /*support_message_callback=*/false,
                             /*support_interrupter=*/false,
                             /*integer_variables=*/false, "")));

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

INSTANTIATE_TEST_SUITE_P(XpressInvalidParameterTest, InvalidParameterTest,
                         ValuesIn({InvalidThreadsParameters()}));

INSTANTIATE_TEST_SUITE_P(XpressGenericTest, GenericTest,
                         testing::Values(GenericTestParameters(
                             SolverType::kXpress, /*support_interrupter=*/false,
                             /*integer_variables=*/false,
                             /*expected_log=*/"Optimal solution found")));

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(TimeLimitTest);

INSTANTIATE_TEST_SUITE_P(XpressInfeasibleSubsystemTest, InfeasibleSubsystemTest,
                         testing::Values(InfeasibleSubsystemTestParameters(
                             {.solver_type = SolverType::kXpress,
                              .support_menu = {
                                  .supports_infeasible_subsystems = false}})));

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(IpModelSolveParametersTest);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(IpParameterTest);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(LargeInstanceIpParameterTest);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(SimpleMipTest);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(IncrementalMipTest);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(MipSolutionHintTest);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(BranchPrioritiesTest);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(LazyConstraintsTest);

LogicalConstraintTestParameters GetXpressLogicalConstraintTestParameters() {
  return LogicalConstraintTestParameters(
      SolverType::kXpress, SolveParameters(),
      /*supports_integer_variables=*/false,
      /*supports_sos1=*/false,
      /*supports_sos2=*/false,
      /*supports_indicator_constraints=*/false,
      /*supports_incremental_add_and_deletes=*/false,
      /*supports_incremental_variable_deletions=*/false,
      /*supports_deleting_indicator_variables=*/false,
      /*supports_updating_binary_variables=*/false);
}

INSTANTIATE_TEST_SUITE_P(
    XpressSimpleLogicalConstraintTest, SimpleLogicalConstraintTest,
    testing::Values(GetXpressLogicalConstraintTestParameters()));
INSTANTIATE_TEST_SUITE_P(
    XpressIncrementalLogicalConstraintTest, IncrementalLogicalConstraintTest,
    testing::Values(GetXpressLogicalConstraintTestParameters()));

MultiObjectiveTestParameters GetXpressMultiObjectiveTestParameters() {
  return MultiObjectiveTestParameters(
      /*solver_type=*/SolverType::kXpress, /*parameters=*/SolveParameters(),
      /*supports_auxiliary_objectives=*/false,
      /*supports_incremental_objective_add_and_delete=*/false,
      /*supports_incremental_objective_modification=*/false,
      /*supports_integer_variables=*/false);
}

INSTANTIATE_TEST_SUITE_P(
    XpressSimpleMultiObjectiveTest, SimpleMultiObjectiveTest,
    testing::Values(GetXpressMultiObjectiveTestParameters()));

INSTANTIATE_TEST_SUITE_P(
    XpressIncrementalMultiObjectiveTest, IncrementalMultiObjectiveTest,
    testing::Values(GetXpressMultiObjectiveTestParameters()));

QpTestParameters GetXpressQpTestParameters() {
  return QpTestParameters(SolverType::kXpress, SolveParameters(),
                          /*qp_support=*/QpSupportType::kConvexQp,
                          /*supports_incrementalism_not_modifying_qp=*/false,
                          /*supports_qp_incrementalism=*/false,
                          /*use_integer_variables=*/false);
}
INSTANTIATE_TEST_SUITE_P(XpressSimpleQpTest, SimpleQpTest,
                         testing::Values(GetXpressQpTestParameters()));
INSTANTIATE_TEST_SUITE_P(XpressIncrementalQpTest, IncrementalQpTest,
                         testing::Values(GetXpressQpTestParameters()));
INSTANTIATE_TEST_SUITE_P(XpressQpDualsTest, QpDualsTest,
                         testing::Values(GetXpressQpTestParameters()));

QcTestParameters GetXpressQcTestParameters() {
  return QcTestParameters(SolverType::kXpress, SolveParameters(),
                          /*supports_qc=*/false,
                          /*supports_incremental_add_and_deletes=*/false,
                          /*supports_incremental_variable_deletions=*/false,
                          /*use_integer_variables=*/false);
}
INSTANTIATE_TEST_SUITE_P(XpressSimpleQcTest, SimpleQcTest,
                         testing::Values(GetXpressQcTestParameters()));
INSTANTIATE_TEST_SUITE_P(XpressIncrementalQcTest, IncrementalQcTest,
                         testing::Values(GetXpressQcTestParameters()));
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(QcDualsTest);

SecondOrderConeTestParameters GetXpressSecondOrderConeTestParameters() {
  return SecondOrderConeTestParameters(
      SolverType::kXpress, SolveParameters(),
      /*supports_soc_constraints=*/false,
      /*supports_incremental_add_and_deletes=*/false);
}
INSTANTIATE_TEST_SUITE_P(
    XpressSimpleSecondOrderConeTest, SimpleSecondOrderConeTest,
    testing::Values(GetXpressSecondOrderConeTestParameters()));
INSTANTIATE_TEST_SUITE_P(
    XpressIncrementalSecondOrderConeTest, IncrementalSecondOrderConeTest,
    testing::Values(GetXpressSecondOrderConeTestParameters()));

std::vector<StatusTestParameters> MakeStatusTestConfigs() {
  std::vector<StatusTestParameters> test_parameters;
  for (const auto algorithm : std::vector<std::optional<LPAlgorithm>>(
           {std::nullopt, LPAlgorithm::kBarrier, LPAlgorithm::kPrimalSimplex,
            LPAlgorithm::kDualSimplex})) {
    SolveParameters solve_parameters = {.lp_algorithm = algorithm};
    test_parameters.push_back(StatusTestParameters(
        SolverType::kXpress, solve_parameters,
        /*disallow_primal_or_dual_infeasible=*/false,
        /*supports_iteration_limit=*/false,
        /*use_integer_variables=*/false,
        /*supports_node_limit=*/false,
        /*support_interrupter=*/false, /*supports_one_thread=*/true));
  }
  return test_parameters;
}

INSTANTIATE_TEST_SUITE_P(XpressStatusTest, StatusTest,
                         ValuesIn(MakeStatusTestConfigs()));

}  // namespace
}  // namespace math_opt
}  // namespace operations_research

int main(int argc, char** argv) {
  printf("Running main() from %s\n", __FILE__);
  testing::InitGoogleTest(&argc, argv);
  if (operations_research::XpressIsCorrectlyInstalled()) {
    return RUN_ALL_TESTS();
  } else {
    LOG(INFO) << "XPress MP is not correctly installed, skipping";
    return EXIT_SUCCESS;
  }
}
