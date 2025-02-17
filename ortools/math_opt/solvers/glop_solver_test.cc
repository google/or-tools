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

SimpleLpTestParameters GlopDefaults() {
  return SimpleLpTestParameters(
      SolverType::kGlop, SolveParameters(), /*supports_duals=*/true,
      /*supports_basis=*/true,
      /*ensures_primal_ray=*/false, /*ensures_dual_ray=*/false,
      /*disallows_infeasible_or_unbounded=*/false);
}

SimpleLpTestParameters ForcePrimalRays() {
  SolveParameters parameters;
  parameters.presolve = Emphasis::kOff;
  parameters.scaling = Emphasis::kOff;
  parameters.lp_algorithm = LPAlgorithm::kPrimalSimplex;
  parameters.enable_output = true;
  return SimpleLpTestParameters(
      SolverType::kGlop, parameters, /*supports_duals=*/true,
      /*supports_basis=*/true,
      /*ensures_primal_ray=*/true, /*ensures_dual_ray=*/false,
      /*disallows_infeasible_or_unbounded=*/true);
}

SimpleLpTestParameters ForceDualRays() {
  SolveParameters parameters;
  parameters.presolve = Emphasis::kOff;
  parameters.scaling = Emphasis::kOff;
  parameters.lp_algorithm = LPAlgorithm::kDualSimplex;
  parameters.enable_output = true;
  return SimpleLpTestParameters(
      SolverType::kGlop, parameters, /*supports_duals=*/true,
      /*supports_basis=*/true,
      /*ensures_primal_ray=*/false, /*ensures_dual_ray=*/true,
      /*disallows_infeasible_or_unbounded=*/false);
}

INSTANTIATE_TEST_SUITE_P(GlopSimpleLpTest, SimpleLpTest,
                         testing::Values(GlopDefaults(), ForcePrimalRays(),
                                         ForceDualRays()));

std::vector<StatusTestParameters> MakeStatusTestConfigs() {
  std::vector<StatusTestParameters> test_parameters;
  for (bool skip_presolve : {true, false}) {
    for (auto algorithm : std::vector<std::optional<LPAlgorithm>>(
             {std::nullopt, LPAlgorithm::kPrimalSimplex,
              LPAlgorithm::kDualSimplex})) {
      SolveParameters solve_parameters = {
          .lp_algorithm = algorithm,
          .presolve = skip_presolve ? std::optional<Emphasis>(Emphasis::kOff)
                                    : std::nullopt};
      test_parameters.push_back(StatusTestParameters(
          SolverType::kGlop, solve_parameters,
          /*disallow_primal_or_dual_infeasible=*/skip_presolve,
          /*supports_iteration_limit=*/true,
          /*use_integer_variables=*/false,
          /*supports_node_limit=*/false,
          /*support_interrupter=*/true, /*supports_one_thread=*/true));
    }
  }
  return test_parameters;
}

INSTANTIATE_TEST_SUITE_P(GlopStatusTest, StatusTest,
                         ValuesIn(MakeStatusTestConfigs()));

INSTANTIATE_TEST_SUITE_P(GlopIncrementalLpTest, IncrementalLpTest,
                         testing::Values(SolverType::kGlop));

MultiObjectiveTestParameters GetGlopMultiObjectiveTestParameters() {
  return MultiObjectiveTestParameters(
      /*solver_type=*/SolverType::kGlop, /*parameters=*/SolveParameters(),
      /*supports_auxiliary_objectives=*/false,
      /*supports_incremental_objective_add_and_delete=*/false,
      /*supports_incremental_objective_modification=*/false,
      /*supports_integer_variables=*/false);
}

INSTANTIATE_TEST_SUITE_P(
    GlopSimpleMultiObjectiveTest, SimpleMultiObjectiveTest,
    testing::Values(GetGlopMultiObjectiveTestParameters()));

INSTANTIATE_TEST_SUITE_P(
    GlopIncrementalMultiObjectiveTest, IncrementalMultiObjectiveTest,
    testing::Values(GetGlopMultiObjectiveTestParameters()));

QpTestParameters GetGlopQpTestParameters() {
  return QpTestParameters(SolverType::kGlop, SolveParameters(),
                          /*qp_support=*/QpSupportType::kNoQpSupport,
                          /*supports_incrementalism_not_modifying_qp=*/true,
                          /*supports_qp_incrementalism=*/false,
                          /*use_integer_variables=*/false);
}
INSTANTIATE_TEST_SUITE_P(GlopSimpleQpTest, SimpleQpTest,
                         testing::Values(GetGlopQpTestParameters()));
INSTANTIATE_TEST_SUITE_P(GlopIncrementalQpTest, IncrementalQpTest,
                         testing::Values(GetGlopQpTestParameters()));
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(QpDualsTest);

QcTestParameters GetGlopQcTestParameters() {
  return QcTestParameters(SolverType::kGlop, SolveParameters(),
                          /*supports_qc=*/false,
                          /*supports_incremental_add_and_deletes=*/false,
                          /*supports_incremental_variable_deletions=*/false,
                          /*use_integer_variables=*/false);
}
INSTANTIATE_TEST_SUITE_P(GlopSimpleQcTest, SimpleQcTest,
                         testing::Values(GetGlopQcTestParameters()));
INSTANTIATE_TEST_SUITE_P(GlopIncrementalQcTest, IncrementalQcTest,
                         testing::Values(GetGlopQcTestParameters()));
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(QcDualsTest);

SecondOrderConeTestParameters GetGlopSecondOrderConeTestParameters() {
  return SecondOrderConeTestParameters(
      SolverType::kGlop, SolveParameters(),
      /*supports_soc_constraints=*/false,
      /*supports_incremental_add_and_deletes=*/false);
}
INSTANTIATE_TEST_SUITE_P(
    GlopSimpleSecondOrderConeTest, SimpleSecondOrderConeTest,
    testing::Values(GetGlopSecondOrderConeTestParameters()));
INSTANTIATE_TEST_SUITE_P(
    GlopIncrementalSecondOrderConeTest, IncrementalSecondOrderConeTest,
    testing::Values(GetGlopSecondOrderConeTestParameters()));

LogicalConstraintTestParameters GetGlopLogicalConstraintTestParameters() {
  return LogicalConstraintTestParameters(
      SolverType::kGlop, SolveParameters(),
      /*supports_integer_variables=*/false,
      /*supports_sos1=*/false,
      /*supports_sos2=*/false,
      /*supports_indicator_constraints=*/false,
      /*supports_incremental_add_and_deletes=*/false,
      /*supports_incremental_variable_deletions=*/true,
      /*supports_deleting_indicator_variables=*/false,
      /*supports_updating_binary_variables=*/false);
}
INSTANTIATE_TEST_SUITE_P(
    GLopSimpleLogicalConstraintTest, SimpleLogicalConstraintTest,
    testing::Values(GetGlopLogicalConstraintTestParameters()));
INSTANTIATE_TEST_SUITE_P(
    GLopIncrementalLogicalConstraintTest, IncrementalLogicalConstraintTest,
    testing::Values(GetGlopLogicalConstraintTestParameters()));

// Note: supports_incremental_solve = true, requires supports_presolve = true,
// so presolve is disabled in the tests.
INSTANTIATE_TEST_SUITE_P(
    GlopPrimalSimplexLpIncompleteSolveTest, LpIncompleteSolveTest,
    testing::Values(LpIncompleteSolveTestParams(
        SolverType::kGlop,
        /*lp_algorithm=*/LPAlgorithm::kPrimalSimplex,
        /*supports_iteration_limit=*/true, /*supports_initial_basis=*/true,
        /*supports_incremental_solve=*/true, /*supports_basis=*/false,
        /*supports_presolve=*/true, /*check_primal_objective=*/true,
        /*primal_solution_status_always_set=*/true,
        /*dual_solution_status_always_set=*/true)));
INSTANTIATE_TEST_SUITE_P(
    GlopDualSimplexLpIncompleteSolveTest, LpIncompleteSolveTest,
    testing::Values(LpIncompleteSolveTestParams(
        SolverType::kGlop,
        /*lp_algorithm=*/LPAlgorithm::kDualSimplex,
        /*supports_iteration_limit=*/true, /*supports_initial_basis=*/true,
        /*supports_incremental_solve=*/true, /*supports_basis=*/true,
        /*supports_presolve=*/true, /*check_primal_objective=*/true,
        /*primal_solution_status_always_set=*/true,
        /*dual_solution_status_always_set=*/true)));

INSTANTIATE_TEST_SUITE_P(GlopInvalidInputTest, InvalidInputTest,
                         testing::Values(InvalidInputTestParameters(
                             SolverType::kGlop,
                             /*use_integer_variables=*/false)));

INSTANTIATE_TEST_SUITE_P(
    GlopInvalidParameterTest, InvalidParameterTest,
    testing::Values(
        InvalidParameterTestParams(SolverType::kGlop,
                                   {
                                       .solution_limit = 3,
                                       .heuristics = Emphasis::kVeryHigh,
                                   },
                                   {"solution_limit", "heuristics"}),
        InvalidParameterTestParams(
            SolverType::kGlop,
            {
                .glop =
                    []() {
                      glop::GlopParameters params;
                      params.set_objective_upper_limit(
                          std::numeric_limits<double>::quiet_NaN());
                      return params;
                    }(),
            },
            {"SolveParametersProto.glop", "objective_upper_limit", "NaN"})));

INSTANTIATE_TEST_SUITE_P(
    GlopLpParameterTest, LpParameterTest,
    testing::Values(LpParameterTestParams(SolverType::kGlop,
                                          /*supports_simplex=*/true,
                                          /*supports_barrier=*/false,
                                          /*supports_first_order=*/false,
                                          /*supports_random_seed=*/true,
                                          /*supports_presolve=*/true,
                                          /*supports_cutoff=*/false,
                                          /*supports_objective_limit=*/true,
                                          /*supports_best_bound_limit=*/true,
                                          /*reports_limits=*/false)));

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(TimeLimitTest);

INSTANTIATE_TEST_SUITE_P(GlopLpModelSolveParametersTest,
                         LpModelSolveParametersTest,
                         testing::Values(LpModelSolveParametersTestParameters(
                             SolverType::kGlop, /*exact_zeros=*/true,
                             /*supports_duals=*/true,
                             /*supports_primal_only_warm_starts=*/false)));

INSTANTIATE_TEST_SUITE_P(GlopLpBasisStartTest, LpBasisStartTest,
                         testing::Values(SOLVER_TYPE_GLOP));

INSTANTIATE_TEST_SUITE_P(GlopGenericTest, GenericTest,
                         testing::Values(GenericTestParameters(
                             SolverType::kGlop, /*support_interrupter=*/true,
                             /*integer_variables=*/false,
                             /*expected_log=*/"status: OPTIMAL")));

INSTANTIATE_TEST_SUITE_P(GlopMessageCallbackTest, MessageCallbackTest,
                         testing::Values(MessageCallbackTestParams(
                             SolverType::kGlop,
                             /*support_message_callback=*/true,
                             /*support_interrupter=*/true,
                             /*integer_variables=*/false, "status: OPTIMAL")));

INSTANTIATE_TEST_SUITE_P(
    GlopCallbackTest, CallbackTest,
    testing::Values(CallbackTestParams(SolverType::kGlop,
                                       /*integer_variables=*/false,
                                       /*add_lazy_constraints=*/false,
                                       /*add_cuts=*/false,
                                       /*supported_events=*/{},
                                       /*all_solutions=*/std::nullopt,
                                       /*reaches_cut_callback*/ std::nullopt)));

INSTANTIATE_TEST_SUITE_P(GlopInfeasibleSubsystemTest, InfeasibleSubsystemTest,
                         testing::Values(InfeasibleSubsystemTestParameters(
                             {.solver_type = SolverType::kGlop})));

}  // namespace
}  // namespace math_opt
}  // namespace operations_research
