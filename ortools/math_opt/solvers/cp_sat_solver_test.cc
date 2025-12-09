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

#include <optional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/cpp/matchers.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/math_opt/solver_tests/callback_tests.h"
#include "ortools/math_opt/solver_tests/generic_tests.h"
#include "ortools/math_opt/solver_tests/infeasible_subsystem_tests.h"
#include "ortools/math_opt/solver_tests/invalid_input_tests.h"
#include "ortools/math_opt/solver_tests/ip_model_solve_parameters_tests.h"
#include "ortools/math_opt/solver_tests/ip_multiple_solutions_tests.h"
#include "ortools/math_opt/solver_tests/ip_parameter_tests.h"
#include "ortools/math_opt/solver_tests/logical_constraint_tests.h"
#include "ortools/math_opt/solver_tests/mip_tests.h"
#include "ortools/math_opt/solver_tests/multi_objective_tests.h"
#include "ortools/math_opt/solver_tests/qc_tests.h"
#include "ortools/math_opt/solver_tests/qp_tests.h"
#include "ortools/math_opt/solver_tests/second_order_cone_tests.h"
#include "ortools/math_opt/solver_tests/status_tests.h"
#include "ortools/math_opt/testing/param_name.h"

namespace operations_research {
namespace math_opt {
namespace {

using ::testing::HasSubstr;
using ::testing::Values;
using ::testing::ValuesIn;
using ::testing::status::StatusIs;

StatusTestParameters StatusDefault() {
  return StatusTestParameters(
      SolverType::kCpSat, SolveParameters(),
      /*disallow_primal_or_dual_infeasible=*/false,
      /*supports_iteration_limit=*/false, /*use_integer_variables=*/true,
      /*supports_node_limit=*/false,
      /*support_interrupter=*/true, /*supports_one_thread=*/true);
}

INSTANTIATE_TEST_SUITE_P(CpSatStatusTest, StatusTest, Values(StatusDefault()));

INSTANTIATE_TEST_SUITE_P(
    CpSatSimpleMipTest, SimpleMipTest,
    Values(SimpleMipTestParameters(SolverType::kCpSat,
                                   /*report_unboundness_correctly=*/true)));
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(IncrementalMipTest);

MultiObjectiveTestParameters GetCpSatMultiObjectiveTestParameters() {
  return MultiObjectiveTestParameters(
      /*solver_type=*/SolverType::kCpSat, /*parameters=*/SolveParameters(),
      /*supports_auxiliary_objectives=*/false,
      /*supports_incremental_objective_add_and_delete=*/false,
      /*supports_incremental_objective_modification=*/false,
      /*supports_integer_variables=*/true);
}

INSTANTIATE_TEST_SUITE_P(CpSatSimpleMultiObjectiveTest,
                         SimpleMultiObjectiveTest,
                         Values(GetCpSatMultiObjectiveTestParameters()));

INSTANTIATE_TEST_SUITE_P(CpSatIncrementalMultiObjectiveTest,
                         IncrementalMultiObjectiveTest,
                         Values(GetCpSatMultiObjectiveTestParameters()));

std::vector<QpTestParameters> GetCpSatQpTestParameters() {
  return {QpTestParameters(SolverType::kCpSat, SolveParameters(),
                           /*qp_support=*/QpSupportType::kNoQpSupport,
                           /*supports_incrementalism_not_modifying_qp=*/false,
                           /*supports_qp_incrementalism=*/false,
                           /*use_integer_variables=*/false),
          QpTestParameters(SolverType::kCpSat, SolveParameters(),
                           /*qp_support=*/QpSupportType::kNoQpSupport,
                           /*supports_incrementalism_not_modifying_qp=*/false,
                           /*supports_qp_incrementalism=*/false,
                           /*use_integer_variables=*/true)};
}
INSTANTIATE_TEST_SUITE_P(CpSatSimpleQpTest, SimpleQpTest,
                         ValuesIn(GetCpSatQpTestParameters()));
INSTANTIATE_TEST_SUITE_P(CpSatIncrementalQpTest, IncrementalQpTest,
                         ValuesIn(GetCpSatQpTestParameters()));
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(QpDualsTest);

std::vector<QcTestParameters> GetCpSatQcTestParameters() {
  return {QcTestParameters(SolverType::kCpSat, SolveParameters(),
                           /*supports_qc=*/false,
                           /*supports_incremental_add_and_deletes=*/false,
                           /*supports_incremental_variable_deletions=*/false,
                           /*use_integer_variables=*/false),
          QcTestParameters(SolverType::kCpSat, SolveParameters(),
                           /*supports_qc=*/false,
                           /*supports_incremental_add_and_deletes=*/false,
                           /*supports_incremental_variable_deletions=*/false,
                           /*use_integer_variables=*/true)};
}
INSTANTIATE_TEST_SUITE_P(CpSatSimpleQcTest, SimpleQcTest,
                         ValuesIn(GetCpSatQcTestParameters()));
INSTANTIATE_TEST_SUITE_P(CpSatIncrementalQcTest, IncrementalQcTest,
                         ValuesIn(GetCpSatQcTestParameters()));
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(QcDualsTest);

SecondOrderConeTestParameters GetCpSatSecondOrderConeTestParameters() {
  return SecondOrderConeTestParameters(
      SolverType::kCpSat, SolveParameters(),
      /*supports_soc_constraints=*/false,
      /*supports_incremental_add_and_deletes=*/false);
}
INSTANTIATE_TEST_SUITE_P(CpSatSimpleSecondOrderConeTest,
                         SimpleSecondOrderConeTest,
                         Values(GetCpSatSecondOrderConeTestParameters()));
INSTANTIATE_TEST_SUITE_P(CpSatIncrementalSecondOrderConeTest,
                         IncrementalSecondOrderConeTest,
                         Values(GetCpSatSecondOrderConeTestParameters()));

LogicalConstraintTestParameters GetCpSatLogicalConstraintTestParameters() {
  return LogicalConstraintTestParameters(
      SolverType::kCpSat, SolveParameters(),
      /*supports_integer_variables=*/true,
      /*supports_sos1=*/false,
      /*supports_sos2=*/false,
      /*supports_indicator_constraints=*/false,
      /*supports_incremental_add_and_deletes=*/false,
      /*supports_incremental_variable_deletions=*/false,
      /*supports_deleting_indicator_variables=*/false,
      /*supports_updating_binary_variables=*/false);
}
INSTANTIATE_TEST_SUITE_P(CpSatSimpleLogicalConstraintTest,
                         SimpleLogicalConstraintTest,
                         Values(GetCpSatLogicalConstraintTestParameters()));
INSTANTIATE_TEST_SUITE_P(CpSatIncrementalLogicalConstraintTest,
                         IncrementalLogicalConstraintTest,
                         Values(GetCpSatLogicalConstraintTestParameters()));

INSTANTIATE_TEST_SUITE_P(
    CpSatInvalidInputTest, InvalidInputTest,
    Values(InvalidInputTestParameters(SolverType::kCpSat,
                                      /*use_integer_variables=*/true)));

INSTANTIATE_TEST_SUITE_P(CpSatInvalidParameterTest, InvalidParameterTest,
                         Values(InvalidParameterTestParams(
                             SolverType::kCpSat,
                             {.objective_limit = 2.0, .best_bound_limit = 1.0},
                             {"objective_limit", "best_bound_limit"})));

SolveParameters StopBeforeOptimal() {
  SolveParameters params = {
      .threads = 1, .presolve = Emphasis::kOff, .cuts = Emphasis::kOff};
  params.cp_sat.set_max_deterministic_time(0.0);
  return params;
}

SolveResultSupport CpSatSolveResultSupport() { return {}; }

ParameterSupport CpSatParameterSupport() {
  return {.supports_cutoff = true,
          .supports_solution_limit_one = true,
          .supports_one_thread = true,
          .supports_n_threads = true,
          .supports_random_seed = true,
          .supports_absolute_gap_tolerance = true,
          .supports_presolve = true,
          .supports_cuts = true};
}

INSTANTIATE_TEST_SUITE_P(
    CpSatIpParameterTest, IpParameterTest,
    Values(IpParameterTestParameters{
        .name = "default",
        .solver_type = SolverType::kCpSat,
        .parameter_support = CpSatParameterSupport(),
        .hint_supported = true,
        .solve_result_support = CpSatSolveResultSupport(),
        .presolved_regexp =
            R"regexp(Presolve summary:(.|\n)*unused variables removed)regexp",
        .stop_before_optimal = StopBeforeOptimal()}),
    ParamName{});

INSTANTIATE_TEST_SUITE_P(CpSatLargeInstanceIpParameterTest,
                         LargeInstanceIpParameterTest,
                         Values(LargeInstanceTestParams{
                             .name = "default",
                             .solver_type = SolverType::kCpSat,
                             .parameter_support = CpSatParameterSupport(),
                             .allow_limit_undetermined = true}),
                         ParamName{});

INSTANTIATE_TEST_SUITE_P(CpSatIpModelSolveParametersTest,
                         IpModelSolveParametersTest,
                         Values(SolverType::kCpSat));

INSTANTIATE_TEST_SUITE_P(
    CpSatIpMultipleSolutionsTest, IpMultipleSolutionsTest,
    Values(IpMultipleSolutionsTestParams(SolverType::kCpSat,
                                         {.presolve = Emphasis::kOff})));

INSTANTIATE_TEST_SUITE_P(
    CpSatGenericTest, GenericTest,
    Values(GenericTestParameters(SolverType::kCpSat,
                                 /*support_interrupter=*/true,
                                 /*integer_variables=*/true,
                                 /*expected_log=*/"status: OPTIMAL")));

INSTANTIATE_TEST_SUITE_P(CpSatInfeasibleSubsystemTest, InfeasibleSubsystemTest,
                         testing::Values(InfeasibleSubsystemTestParameters(
                             {.solver_type = SolverType::kCpSat})));

SolutionHintTestParams MakeCpsatSolutionHintParams() {
  SolveParameters solve_params;
  solve_params.cuts = Emphasis::kOff;
  solve_params.presolve = Emphasis::kOff;
  solve_params.cp_sat.set_stop_after_first_solution(true);
  solve_params.cp_sat.set_num_workers(1);
  // Matches "best:", "next:" and "hint" appearing in the same line
  std::string hint_message_regex = "best:.*next:.*hint";
  return SolutionHintTestParams(SolverType::kCpSat, solve_params, std::nullopt,
                                hint_message_regex);
}

INSTANTIATE_TEST_SUITE_P(CpSatSolutionHintTest, MipSolutionHintTest,
                         Values(MakeCpsatSolutionHintParams()));

// CpSat does not support MIP branch priorities or lazy constraints at this
// point.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(BranchPrioritiesTest);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(LazyConstraintsTest);

INSTANTIATE_TEST_SUITE_P(CpSatTimeLimitTest, TimeLimitTest,
                         Values(TimeLimitTestParameters(
                             SolverType::kCpSat, /*integer_variables=*/true,
                             CallbackEvent::kMipSolution)));

TEST(CpSatSolverTest, Scaling) {
  // To test scaling we need a non-trivial model.
  //
  // Simple models like:
  //   maximize(x)
  //   s.t. 4 * x <= 3
  // are solved by pre-solve and we get the valid answer (0.75) independently
  // from the scaling value.
  //
  // Hence here we use two continuous variables and two constraints so that the
  // pre-solve can't find the solution.
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 5.0, "x");
  const Variable y = model.AddContinuousVariable(0.0, 5.0, "y");

  // The optimum is x = 2.75 (11/4) and y = 2.5 (5/2).
  //
  // The constraints and objective have been chosen so that the optimum requires
  // a scaling of 4, and so that a scaling of 2 won't output the same solution
  // as a scaling of 1.
  model.AddLinearConstraint(2 * x + 7 * y <= 23);
  model.AddLinearConstraint(2 * x + y <= 8);
  model.Maximize(3 * x + 4 * y);

  // Disable automatic scaling and keep default scaling (1.0). We expect to find
  // the closest integer point (x=3.0, y=2.0).
  {
    SolveArguments args;
    args.parameters.cp_sat.set_mip_automatically_scale_variables(false);
    args.parameters.cp_sat.set_only_solve_ip(false);

    ASSERT_OK_AND_ASSIGN(const SolveResult result,
                         Solve(model, SolverType::kCpSat, args));
    EXPECT_THAT(result, IsOptimalWithSolution(3.0 * 3.0 + 4.0 * 2.0,
                                              {{x, 3.0}, {y, 2.0}}));
  }

  // Disable automatic scaling and use a scaling of 2.0. We find a closer point
  // (x=2.5, y=2.5) but we can't reach the optimum since we need x=2.75 which is
  // not a multiple of 1/2.
  {
    SolveArguments args;
    args.parameters.cp_sat.set_mip_automatically_scale_variables(false);
    args.parameters.cp_sat.set_mip_var_scaling(2.0);
    args.parameters.cp_sat.set_only_solve_ip(false);

    ASSERT_OK_AND_ASSIGN(const SolveResult result,
                         Solve(model, SolverType::kCpSat, args));
    EXPECT_THAT(result, IsOptimalWithSolution(3.0 * 2.5 + 4.0 * 2.5,
                                              {{x, 2.5}, {y, 2.5}}));
  }

  // Disable automatic scaling and use a scaling of 4.0. The optimum is now an
  // integer point in the scaled problem (4*2.75 = 11 and 4*2.5 = 10). Hence we
  // expect to find it.
  {
    SolveArguments args;
    args.parameters.cp_sat.set_mip_automatically_scale_variables(false);
    args.parameters.cp_sat.set_mip_var_scaling(4.0);
    args.parameters.cp_sat.set_only_solve_ip(false);

    ASSERT_OK_AND_ASSIGN(const SolveResult result,
                         Solve(model, SolverType::kCpSat, args));
    EXPECT_THAT(result, IsOptimalWithSolution(3.0 * 2.75 + 4.0 * 2.5,
                                              {{x, 2.75}, {y, 2.5}}));
  }
}

INSTANTIATE_TEST_SUITE_P(CpSatMessageCallbackTest, MessageCallbackTest,
                         Values(MessageCallbackTestParams(
                             SolverType::kCpSat,
                             /*support_message_callback=*/true,
                             /*support_interrupter=*/true,
                             /*integer_variables=*/true, "status: OPTIMAL")));

SolveParameters AllSolutions() {
  SolveParameters result;
  // Presolve can eliminate suboptimal solutions with CP-SAT
  result.presolve = Emphasis::kOff;
  result.cp_sat.set_enumerate_all_solutions(true);
  return result;
}

INSTANTIATE_TEST_SUITE_P(
    CpSatCallbackTest, CallbackTest,
    Values(CallbackTestParams(
        SolverType::kCpSat,
        /*integer_variables=*/true,
        /*add_lazy_constraints=*/false,
        /*add_cuts=*/false,
        /*supported_events=*/{CallbackEvent::kMipSolution, CallbackEvent::kMip},
        /*all_solutions=*/AllSolutions(),
        /*reaches_cut_callback=*/std::nullopt)));

TEST(CpSatInvalidCallbackTest, RequestLazyConstraints) {
  Model model("model");
  const Variable x = model.AddBinaryVariable("x");
  const Variable y = model.AddBinaryVariable("y");
  model.Maximize(x + 2 * y);

  const SolveArguments args = {
      .callback_registration = {.events = {CallbackEvent::kMipSolution},
                                .add_lazy_constraints = true},
      .callback = [&](const CallbackData&) {
        CallbackResult result;
        return result;
      }};
  EXPECT_THAT(
      Solve(model, SolverType::kCpSat, args),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("add_lazy_constraints=true is not supported")));
}

TEST(CpSatInvalidArgumentTest, ParameterValidation) {
  Model model("model");
  SolveArguments args;
  args.parameters.cp_sat.set_mip_max_bound(-1);
  EXPECT_THAT(Solve(model, SolverType::kCpSat, args),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("parameter 'mip_max_bound' should be in")));
}

}  // namespace
}  // namespace math_opt
}  // namespace operations_research
