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
#include "ortools/math_opt/solver_tests/ip_model_solve_parameters_tests.h"
#include "ortools/math_opt/solver_tests/logical_constraint_tests.h"
#include "ortools/math_opt/solver_tests/lp_incomplete_solve_tests.h"
#include "ortools/math_opt/solver_tests/lp_model_solve_parameters_tests.h"
#include "ortools/math_opt/solver_tests/lp_parameter_tests.h"
#include "ortools/math_opt/solver_tests/lp_tests.h"
#include "ortools/math_opt/solver_tests/mip_tests.h"
#include "ortools/math_opt/solver_tests/multi_objective_tests.h"
#include "ortools/math_opt/solver_tests/qc_tests.h"
#include "ortools/math_opt/solver_tests/qp_tests.h"
#include "ortools/math_opt/solver_tests/second_order_cone_tests.h"
#include "ortools/math_opt/solver_tests/status_tests.h"
#include "ortools/third_party_solvers/xpress_environment.h"

/** A string in the log file that indicates that the solution process
 * finished successfully and found the optimal solution for LPs.
 */
#define OPTIMAL_SOLUTION_FOUND_LP "Optimal solution found"
/** A string in the log file that indicates that the solution process
 * finished successfully and found the optimal solution for MIPs.
 */
#define OPTIMAL_SOLUTION_FOUND_MIP "*** Search completed ***"

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
    testing::Values(LpParameterTestParams(
        SolverType::kXpress,
        /*supports_simplex=*/true,
        /*supports_barrier=*/true,
        /*supports_first_order=*/true,
        /*supports_random_seed=*/false,  // Xpress supports this but it does not
                                         // generate enough variability for this
        /*supports_presolve=*/true,
        /*supports_cutoff=*/true,
        /*supports_objective_limit=*/false,  // See comments in xpress_solver.cc
        /*supports_best_bound_limit=*/false,
        /*reports_limits=*/false)));

INSTANTIATE_TEST_SUITE_P(
    XpressPrimalSimplexLpIncompleteSolveTest, LpIncompleteSolveTest,
    testing::Values(LpIncompleteSolveTestParams(
        SolverType::kXpress,
        /*lp_algorithm=*/LPAlgorithm::kPrimalSimplex,
        /*supports_iteration_limit=*/true, /*supports_initial_basis=*/false,
        /*supports_incremental_solve=*/false, /*supports_basis=*/true,
        /*supports_presolve=*/true, /*check_primal_objective=*/true,
        /*primal_solution_status_always_set=*/true,
        /*dual_solution_status_always_set=*/true)));
INSTANTIATE_TEST_SUITE_P(
    XpressDualSimplexLpIncompleteSolveTest, LpIncompleteSolveTest,
    testing::Values(LpIncompleteSolveTestParams(
        SolverType::kXpress,
        /*lp_algorithm=*/LPAlgorithm::kDualSimplex,
        /*supports_iteration_limit=*/true, /*supports_initial_basis=*/false,
        /*supports_incremental_solve=*/false, /*supports_basis=*/true,
        /*supports_presolve=*/true, /*check_primal_objective=*/true,
        /*primal_solution_status_always_set=*/true,
        /*dual_solution_status_always_set=*/true)));

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(IncrementalLpTest);

INSTANTIATE_TEST_SUITE_P(
    XpressMessageCallbackTest, MessageCallbackTest,
    testing::ValuesIn({MessageCallbackTestParams(
                           SolverType::kXpress,
                           /*support_message_callback=*/true,
                           /*support_interrupter=*/true,
                           /*integer_variables=*/false,
                           /*ending_substring*/ OPTIMAL_SOLUTION_FOUND_LP),
                       MessageCallbackTestParams(
                           SolverType::kXpress,
                           /*support_message_callback=*/true,
                           /*support_interrupter=*/true,
                           /*integer_variables=*/true,
                           /*ending_substring*/ OPTIMAL_SOLUTION_FOUND_MIP)}));

INSTANTIATE_TEST_SUITE_P(
    XpressCallbackTest, CallbackTest,
    testing::ValuesIn(
        {CallbackTestParams(SolverType::kXpress,
                            /*integer_variables=*/false,
                            /*add_lazy_constraints=*/false,
                            /*add_cuts=*/false,
                            /*supported_events=*/{},
                            /*all_solutions=*/std::nullopt,
                            /*reaches_cut_callback*/ std::nullopt),
         CallbackTestParams(SolverType::kXpress,
                            /*integer_variables=*/true,
                            /*add_lazy_constraints=*/false,
                            /*add_cuts=*/false,
                            /*supported_events=*/{},
                            /*all_solutions=*/std::nullopt,
                            /*reaches_cut_callback*/ std::nullopt)}));

INSTANTIATE_TEST_SUITE_P(XpressInvalidInputTest, InvalidInputTest,
                         testing::Values(InvalidInputTestParameters(
                             SolverType::kXpress,
                             // Invalid parameters do not depend on integer
                             // variables
                             /*use_integer_variables=*/false)));

InvalidParameterTestParams InvalidObjectiveLimitParameters() {
  SolveParameters params;
  params.objective_limit = 1.5;
  return InvalidParameterTestParams(
      SolverType::kXpress, std::move(params),
      {"XpressSolver does not support objective_limit"});
}

InvalidParameterTestParams InvalidBestBoundLimitParameters() {
  SolveParameters params;
  params.best_bound_limit = 1.5;
  return InvalidParameterTestParams(
      SolverType::kXpress, std::move(params),
      {"XpressSolver does not support best_bound_limit"});
}

InvalidParameterTestParams InvalidSolutionPoolSizeParameters() {
  SolveParameters params;
  params.solution_pool_size = 2;
  return InvalidParameterTestParams(
      SolverType::kXpress, std::move(params),
      {"XpressSolver does not support solution_pool_size"});
}

INSTANTIATE_TEST_SUITE_P(XpressInvalidParameterTest, InvalidParameterTest,
                         ValuesIn({InvalidObjectiveLimitParameters(),
                                   InvalidBestBoundLimitParameters(),
                                   InvalidSolutionPoolSizeParameters()}));

INSTANTIATE_TEST_SUITE_P(
    XpressGenericTest, GenericTest,
    testing::ValuesIn(
        {GenericTestParameters(SolverType::kXpress,
                               /*support_interrupter=*/true,
                               /*integer_variables=*/false,
                               /*expected_log=*/OPTIMAL_SOLUTION_FOUND_LP),
         GenericTestParameters(SolverType::kXpress,
                               /*support_interrupter=*/true,
                               /*integer_variables=*/true,
                               /*expected_log=*/OPTIMAL_SOLUTION_FOUND_MIP)}));

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(TimeLimitTest);

INSTANTIATE_TEST_SUITE_P(XpressInfeasibleSubsystemTest, InfeasibleSubsystemTest,
                         testing::Values(InfeasibleSubsystemTestParameters(
                             {.solver_type = SolverType::kXpress,
                              .support_menu = {
                                  .supports_infeasible_subsystems = false}})));

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(IpModelSolveParametersTest);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(IpParameterTest);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(LargeInstanceIpParameterTest);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(IncrementalMipTest);

INSTANTIATE_TEST_SUITE_P(XpressSimpleMipTest, SimpleMipTest,
                         testing::Values(SolverType::kXpress));

SolveParameters GetXpressSingleHintParams() {
  // Parameters to stop on the first solution that is created from extending
  // the solution hints.
  SolveParameters params;
  params.solution_limit = 1;
  params.xpress.param_values["PRESOLVE"] = "0";
  params.xpress.param_values["HEUREMPHASIS"] = "0";
  return params;
}
SolveParameters GetXpressTwoHintParams() {
  // Parameters to stop on the second solution that is created from extending
  // the solution hints.
  SolveParameters params;
  params.solution_limit = 2;
  params.xpress.param_values["PRESOLVE"] = "0";
  params.xpress.param_values["HEUREMPHASIS"] = "0";
  return params;
}

INSTANTIATE_TEST_SUITE_P(XpressMipSolutionHintTest, MipSolutionHintTest,
                         testing::Values(SolutionHintTestParams(
                             SolverType::kXpress, GetXpressSingleHintParams(),
                             GetXpressTwoHintParams(),
                             "User solution (.*) stored")));

SolveParameters GetXpressLazyConstraintsParams() {
  SolveParameters params;
  // Disable heuristics since they may interfere with expected results.
  params.xpress.param_values["HEUREMPHASIS"] = "0";
  params.xpress.param_values["PRESOLVE"] = "0";
  params.xpress.param_values["CUTSTRATEGY"] = "0";
  // Without STOP_AFTER_LP Xpress will not stop right after the relaxation
  // but will start the cut loop and inject the lazy constraints, which is
  // unexpected in .
  // On the other hand, these parameters are also used for test
  // LazyConstraintsTest.AnnotationsAreClearedAfterSolve/0 which then fails
  // because that test expects to finish of the root node. Therefore that
  // test is disabled.
  params.xpress.param_values["STOP_AFTER_LP"] = "1";
  return params;
}

INSTANTIATE_TEST_SUITE_P(XpressLazyConstraintsTest, LazyConstraintsTest,
                         testing::Values(LazyConstraintsTestParams(
                             SolverType::kXpress,
                             GetXpressLazyConstraintsParams())));

SolveParameters GetXpressBranchPrioritiesParams() {
  SolveParameters params;
  // Disable anything that is different from plain branch & bound
  params.xpress.param_values["HEUREMPHASIS"] = "0";
  params.xpress.param_values["PRESOLVE"] = "0";
  params.xpress.param_values["CUTSTRATEGY"] = "0";
  params.xpress.param_values["NODEPROBINGEFFORT"] = "0.0";
  // For BranchPrioritiesTest.PrioritiesClearedAfterIncrementalSolve, otherwise
  // we attempt to set branching priorities on a problem in presolved state,
  // which is not allowed.
  params.xpress.param_values["FORCE_POSTSOLVE"] = "1";
  return params;
}

INSTANTIATE_TEST_SUITE_P(XpressBranchPrioritiesTest, BranchPrioritiesTest,
                         testing::Values(BranchPrioritiesTestParams(
                             SolverType::kXpress,
                             GetXpressBranchPrioritiesParams())));

LogicalConstraintTestParameters GetXpressLogicalConstraintTestParameters() {
  return LogicalConstraintTestParameters(
      SolverType::kXpress, SolveParameters(),
      /*supports_integer_variables=*/true,
      // Note: Xpress supports SOS, but it only supports SOSs that comprise
      //       solely of variables (not expressions) and it does not support
      //       duplicate entries. Many of the SOS tests construct things
      //       like this, so we skip them.
      /*supports_sos1=*/false,
      /*supports_sos2=*/false,
      /*supports_indicator_constraints=*/true,
      /*supports_incremental_add_and_deletes=*/false,
      /*supports_incremental_variable_deletions=*/false,
      /*supports_deleting_indicator_variables=*/false,
      /*supports_updating_binary_variables=*/false,
      /*supports_sos_on_expressions=*/false);
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
      /*supports_auxiliary_objectives=*/true,
      /*supports_incremental_objective_add_and_delete=*/false,
      /*supports_incremental_objective_modification=*/false,
      /*supports_integer_variables=*/true);
}

INSTANTIATE_TEST_SUITE_P(
    XpressSimpleMultiObjectiveTest, SimpleMultiObjectiveTest,
    testing::Values(GetXpressMultiObjectiveTestParameters()));

INSTANTIATE_TEST_SUITE_P(
    XpressIncrementalMultiObjectiveTest, IncrementalMultiObjectiveTest,
    testing::Values(GetXpressMultiObjectiveTestParameters()));

std::vector<QpTestParameters> GetXpressQpTestParameters() {
  // TODO: Xpress also supports non-convex QP.
  return {QpTestParameters(SolverType::kXpress, SolveParameters(),
                           /*qp_support=*/QpSupportType::kConvexQp,
                           /*supports_incrementalism_not_modifying_qp=*/false,
                           /*supports_qp_incrementalism=*/false,
                           /*use_integer_variables=*/true),
          QpTestParameters(SolverType::kXpress, SolveParameters(),
                           /*qp_support=*/QpSupportType::kConvexQp,
                           /*supports_incrementalism_not_modifying_qp=*/false,
                           /*supports_qp_incrementalism=*/false,
                           /*use_integer_variables=*/false)};
}
INSTANTIATE_TEST_SUITE_P(XpressSimpleQpTest, SimpleQpTest,
                         testing::ValuesIn(GetXpressQpTestParameters()));
INSTANTIATE_TEST_SUITE_P(XpressIncrementalQpTest, IncrementalQpTest,
                         testing::ValuesIn(GetXpressQpTestParameters()));
INSTANTIATE_TEST_SUITE_P(XpressQpDualsTest, QpDualsTest,
                         testing::ValuesIn(GetXpressQpTestParameters()));

std::vector<QcTestParameters> GetXpressQcTestParameters() {
  return {QcTestParameters(SolverType::kXpress, SolveParameters(),
                           /*supports_qc=*/true,
                           /*supports_incremental_add_and_deletes=*/false,
                           /*supports_incremental_variable_deletions=*/false,
                           /*use_integer_variables=*/true),
          QcTestParameters(SolverType::kXpress, SolveParameters(),
                           /*supports_qc=*/true,
                           /*supports_incremental_add_and_deletes=*/false,
                           /*supports_incremental_variable_deletions=*/false,
                           /*use_integer_variables=*/false)};
}
INSTANTIATE_TEST_SUITE_P(XpressSimpleQcTest, SimpleQcTest,
                         testing::ValuesIn(GetXpressQcTestParameters()));
INSTANTIATE_TEST_SUITE_P(XpressIncrementalQcTest, IncrementalQcTest,
                         testing::ValuesIn(GetXpressQcTestParameters()));
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(QcDualsTest);

SecondOrderConeTestParameters GetXpressSecondOrderConeTestParameters() {
  return SecondOrderConeTestParameters(
      SolverType::kXpress, SolveParameters(),
      /*supports_soc_constraints=*/true,
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
        /*supports_iteration_limit=*/true,
        /*use_integer_variables=*/false,
        /*supports_node_limit=*/true,
        /*support_interrupter=*/true, /*supports_one_thread=*/true));
  }
  // Add a test with default LP algorithm and integer variables
  SolveParameters solve_parameters = {.lp_algorithm = std::nullopt};
  test_parameters.push_back(StatusTestParameters(
      SolverType::kXpress, solve_parameters,
      /*disallow_primal_or_dual_infeasible=*/false,
      /*supports_iteration_limit=*/true,
      /*use_integer_variables=*/true,
      /*supports_node_limit=*/true,
      /*support_interrupter=*/true, /*supports_one_thread=*/true));
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
