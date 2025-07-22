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

#include <cstdint>
#include <limits>
#include <memory>
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
#include "ortools/math_opt/solver_tests/ip_model_solve_parameters_tests.h"
#include "ortools/math_opt/solver_tests/ip_parameter_tests.h"
#include "ortools/math_opt/solver_tests/lp_model_solve_parameters_tests.h"
#include "ortools/math_opt/solver_tests/lp_parameter_tests.h"
#include "ortools/math_opt/solver_tests/lp_tests.h"
#include "ortools/math_opt/solver_tests/mip_tests.h"
#include "ortools/math_opt/solver_tests/multi_objective_tests.h"
#include "ortools/math_opt/solver_tests/status_tests.h"
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

constexpr double kInf = std::numeric_limits<double>::infinity();

SimpleLpTestParameters HighsDefaults() {
  return SimpleLpTestParameters(
      SolverType::kHighs, {}, /*supports_duals=*/true,
      /*supports_basis=*/true,
      /*ensures_primal_ray=*/false, /*ensures_dual_ray=*/false,
      // Note: there is a highs specific option we can use to turn this to true
      // "allow_unbounded_or_infeasible"
      // https://github.com/ERGO-Code/HiGHS/blob/master/src/lp_data/HighsOptions.h#L321
      /*disallows_infeasible_or_unbounded=*/false);
}

INSTANTIATE_TEST_SUITE_P(HighsSimpleLpTest, SimpleLpTest,
                         Values(HighsDefaults()));

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(IncrementalLpTest);

INSTANTIATE_TEST_SUITE_P(HighsSimpleMipTest, SimpleMipTest,
                         Values(SolverType::kHighs));

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(IncrementalMipTest);

INSTANTIATE_TEST_SUITE_P(
    HighsGenericTest, GenericTest,
    Values(GenericTestParameters(SolverType::kHighs,
                                 /*support_interrupter=*/false,
                                 /*integer_variables=*/false,
                                 /*expected_log=*/"HiGHS run time"),
           GenericTestParameters(SolverType::kHighs,
                                 /*support_interrupter=*/false,
                                 /*integer_variables=*/true,
                                 /*expected_log=*/"Solving report")));

// These tests require callback support.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(TimeLimitTest);

INSTANTIATE_TEST_SUITE_P(
    HighsLpParameterTest, LpParameterTest,
    Values(LpParameterTestParams(SolverType::kHighs,
                                 /*supports_simplex=*/true,
                                 /*supports_barrier=*/true,
                                 /*supports_first_order=*/false,
                                 /*supports_random_seed=*/true,
                                 /*supports_presolve=*/true,
                                 /*supports_cutoff=*/false,
                                 /*supports_objective_limit=*/false,
                                 /*supports_best_bound_limit=*/true,
                                 /*reports_limits=*/true)));

ParameterSupport HighsMipParameterSupport() {
  return {.supports_node_limit = true,
          .supports_solution_limit_one = true,
          .supports_random_seed = true,
          .supports_absolute_gap_tolerance = true,
          .supports_presolve = true,
          .supports_heuristics = true,
          .supports_scaling = true};
}

SolveResultSupport HighsMipSolveResultSupport() {
  return {.termination_limit = true,
          // See TODO in highs_solver.cc, the iteration stats are tracked but
          // not accessible at the end of solve.
          .iteration_stats = false,
          .node_count = true};
}

SolveParameters StopBeforeOptimal() {
  return {.node_limit = 1,
          .presolve = Emphasis::kOff,
          .heuristics = Emphasis::kOff};
}

IpParameterTestParameters HighsIpParameterParams() {
  return IpParameterTestParameters{
      .name = "default",
      .solver_type = SolverType::kHighs,
      .parameter_support = HighsMipParameterSupport(),
      .hint_supported = false,
      .solve_result_support = HighsMipSolveResultSupport(),
      .presolved_regexp = "Presolve: Optimal",
      .stop_before_optimal = StopBeforeOptimal()};
}

INSTANTIATE_TEST_SUITE_P(HighsIpParameterTest, IpParameterTest,
                         Values(HighsIpParameterParams()), ParamName{});

INSTANTIATE_TEST_SUITE_P(HighsLargeInstanceIpParameterTest,
                         LargeInstanceIpParameterTest,
                         Values(LargeInstanceTestParams{
                             .name = "default",
                             .solver_type = SolverType::kHighs,
                             .parameter_support = HighsMipParameterSupport()}),
                         ParamName{});

INSTANTIATE_TEST_SUITE_P(HighsIpModelSolveParametersTest,
                         IpModelSolveParametersTest,
                         Values(SolverType::kHighs));

INSTANTIATE_TEST_SUITE_P(HighsLpModelSolveParametersTest,
                         LpModelSolveParametersTest,
                         Values(LpModelSolveParametersTestParameters(
                             SolverType::kHighs, /*exact_zeros=*/true,
                             /*supports_duals=*/true,
                             /*supports_primal_only_warm_starts=*/false)));

// Highs::setSolution is implemented, but it only accepts complete solutions.
// The test below generates partial solutuions, so we skip it.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(MipSolutionHintTest);

// HiGHS does not support branching priority.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(BranchPrioritiesTest);
// HiGHS does not support lazy constraints at this point.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(LazyConstraintsTest);

std::vector<StatusTestParameters> MakeStatusTestConfigs() {
  std::vector<StatusTestParameters> test_parameters;
  // Test specific lp algorithms in their default and pure forms (i.e. without
  // running preprocessor).
  for (bool skip_presolve : {true, false}) {
    for (auto algorithm : std::vector<std::optional<LPAlgorithm>>(
             {std::nullopt, LPAlgorithm::kBarrier, LPAlgorithm::kPrimalSimplex,
              LPAlgorithm::kDualSimplex})) {
      SolveParameters solve_parameters = {.lp_algorithm = algorithm};
      if (skip_presolve) {
        solve_parameters.presolve = Emphasis::kOff;
      }
      test_parameters.push_back(StatusTestParameters(
          SolverType::kHighs, solve_parameters,
          // TODO(b/271465390): highs has a parameter for this
          /*disallow_primal_or_dual_infeasible=*/false,
          /*supports_iteration_limit=*/true,
          /*use_integer_variables=*/false,
          /*supports_node_limit=*/false,
          /*support_interrupter=*/false,
          /*supports_one_thread=*/false));
    }
  }
  // Now add MIP configuration:
  test_parameters.push_back(StatusTestParameters(
      SolverType::kHighs, {},
      // TODO(b/271465390): highs has a parameter for this
      /*disallow_primal_or_dual_infeasible=*/false,
      // Highs does not support iteration limit for integer problems
      /*supports_iteration_limit=*/false,
      /*use_integer_variables=*/true,
      /*supports_node_limit=*/true,
      /*support_interrupter=*/false,
      /*supports_one_thread=*/false));
  return test_parameters;
}

INSTANTIATE_TEST_SUITE_P(HighsStatusTest, StatusTest,
                         ValuesIn(MakeStatusTestConfigs()));

INSTANTIATE_TEST_SUITE_P(
    HighsMessageCallbackTest, MessageCallbackTest,
    Values(MessageCallbackTestParams(SolverType::kHighs,
                                     /*support_message_callback=*/true,
                                     /*support_interrupter=*/false,
                                     /*integer_variables=*/false,
                                     /*ending_substring=*/"HiGHS run time"),
           MessageCallbackTestParams(SolverType::kHighs,
                                     /*support_message_callback=*/true,
                                     /*support_interrupter=*/false,
                                     /*integer_variables=*/true,
                                     /*ending_substring=*/"(heuristics)")));

// HiGHS does not support callbacks other than message callback.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(CallbackTest);

INSTANTIATE_TEST_SUITE_P(HighsInfeasibleSubsystemTest, InfeasibleSubsystemTest,
                         testing::Values(InfeasibleSubsystemTestParameters(
                             {.solver_type = SolverType::kHighs})));

MultiObjectiveTestParameters GetHighsMultiObjectiveTestParameters() {
  return MultiObjectiveTestParameters(
      /*solver_type=*/SolverType::kHighs, /*parameters=*/SolveParameters(),
      /*supports_auxiliary_objectives=*/false,
      /*supports_incremental_objective_add_and_delete=*/false,
      /*supports_incremental_objective_modification=*/false,
      /*supports_integer_variables=*/true);
}

INSTANTIATE_TEST_SUITE_P(HighsSimpleMultiObjectiveTest,
                         SimpleMultiObjectiveTest,
                         Values(GetHighsMultiObjectiveTestParameters()));

INSTANTIATE_TEST_SUITE_P(HighsIncrementalMultiObjectiveTest,
                         IncrementalMultiObjectiveTest,
                         Values(GetHighsMultiObjectiveTestParameters()));

TEST(HighsSolverTest, FractionalBoundsForIntegerVariables) {
  Model model;
  const Variable x = model.AddIntegerVariable(0.0, 1.5);
  model.Maximize(x);
  EXPECT_THAT(Solve(model, SolverType::kHighs),
              IsOkAndHolds(IsOptimalWithSolution(1.0, {{x, 1.0}})));
}

TEST(HighsSolverTest, IterationLimitTooLarge) {
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 1.0);
  model.Maximize(x);
  SolveParameters params = {.iteration_limit =
                                std::numeric_limits<int64_t>::max()};
  EXPECT_THAT(Solve(model, SolverType::kHighs, {.parameters = params}),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("iteration_limit")));
  params.iteration_limit = std::numeric_limits<int32_t>::max();
  EXPECT_THAT(Solve(model, SolverType::kHighs, {.parameters = params}),
              IsOkAndHolds(IsOptimal(1.0)));
}

// Highs treats "empty" models with no variables differently.
TEST(HighsSolverEmptyModelsTest, OffsetOnlyPrimalAndDualBoundsCorrect) {
  Model model;
  model.Maximize(3.0);
  ASSERT_OK_AND_ASSIGN(const SolveResult result,
                       Solve(model, SolverType::kHighs));
  EXPECT_THAT(result, IsOptimal(3.0));
  EXPECT_NEAR(result.termination.objective_bounds.primal_bound, 3.0, 1e-8);
  EXPECT_NEAR(result.termination.objective_bounds.dual_bound, 3.0, 1e-8);
}

// Highs treats "empty" models with no variables differently.
TEST(HighsSolverEmptyModelsTest,
     InfeasibleWithoutVariablesBoundsCorrectMinimize) {
  Model model;
  model.Minimize(3.0);
  model.AddLinearConstraint(/*lower_bound=*/3.0, /*upper_bound=*/3.0);
  ASSERT_OK_AND_ASSIGN(const SolveResult result,
                       Solve(model, SolverType::kHighs));
  EXPECT_THAT(result, TerminatesWith(TerminationReason::kInfeasible));
  EXPECT_DOUBLE_EQ(result.termination.objective_bounds.primal_bound, kInf);
  EXPECT_DOUBLE_EQ(result.termination.objective_bounds.dual_bound, kInf);
}

// Highs treats "empty" models with no variables differently.
TEST(HighsSolverEmptyModelsTest,
     InfeasibleWithoutVariablesBoundsCorrectMaximize) {
  Model model;
  model.Maximize(3.0);
  model.AddLinearConstraint(/*lower_bound=*/3.0, /*upper_bound=*/3.0);
  ASSERT_OK_AND_ASSIGN(const SolveResult result,
                       Solve(model, SolverType::kHighs));
  EXPECT_THAT(result, TerminatesWith(TerminationReason::kInfeasible));
  EXPECT_DOUBLE_EQ(result.termination.objective_bounds.primal_bound, -kInf);
  EXPECT_DOUBLE_EQ(result.termination.objective_bounds.dual_bound, -kInf);
}

TEST(HighsSolverTest,
     HighsOptions_BoolOptionLpRelaxation_ReturnsRelaxedObjective) {
  Model model;

  const Variable x = model.AddBinaryVariable();
  const Variable y = model.AddBinaryVariable();
  model.Maximize(x + y);
  model.AddLinearConstraint(x + y <= 1.5);

  // The MIP has objective 1.0
  EXPECT_THAT(Solve(model, SolverType::kHighs), IsOkAndHolds(IsOptimal(1.0)));

  // The LP relaxation has objective 1.5.
  SolveParameters params;
  (*params.highs.mutable_bool_options())["solve_relaxation"] = true;
  EXPECT_THAT(Solve(model, SolverType::kHighs, {.parameters = params}),
              IsOkAndHolds(IsOptimal(1.5)));
}

TEST(HighsSolverTest,
     HighsOptions_BoolOptionLpRelaxation_SetsIsIntegerCorrectly) {
  Model model;

  const Variable x = model.AddBinaryVariable();
  const Variable y = model.AddBinaryVariable();
  model.Maximize(x + y);
  model.AddLinearConstraint(x + y <= 1.5);

  SolveParameters params;
  (*params.highs.mutable_bool_options())["solve_relaxation"] = true;
  params.lp_algorithm = LPAlgorithm::kPrimalSimplex;
  // If is_integer is set to true we would get an INVALID_ARGUMENT error stating
  // that lp_algorithm is not supported for HiGHS on problems with integer
  // variables.
  EXPECT_THAT(Solve(model, SolverType::kHighs, {.parameters = params}),
              IsOkAndHolds(IsOptimal(1.5)));
}

TEST(HighsSolverTest, HighsOptions_BoolOptionBadName_InvalidArgument) {
  Model model;
  SolveParameters params;
  (*params.highs.mutable_bool_options())["brown_dog"] = true;
  EXPECT_THAT(Solve(model, SolverType::kHighs, {.parameters = params}),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("option name was unknown"),
                             HasSubstr("brown_dog"))));
}

// The problem:
//
//  max  sum_{i=1}^100 x_i
//  s.t. x_i + x_j <= 1 for 1 <= i < j <= 100
//       x_i in {0, 1} for i = 1, ..., 100
//
// The problem has an LP relaxation of 50 and an optimal solution of 1. MIP
// solvers generally cannot solve it instantly.
//
// If `is_integer` is false, returns a model of the LP relaxation instead.
std::unique_ptr<Model> BigModel(const bool is_integer = true) {
  auto model = std::make_unique<Model>();
  std::vector<Variable> xs;
  for (int i = 0; i < 100; ++i) {
    xs.push_back(model->AddVariable(0.0, 1.0, is_integer));
  }
  for (int i = 0; i < 100; ++i) {
    for (int j = i + 1; j < 100; ++j) {
      model->AddLinearConstraint(xs[i] + xs[j] <= 1);
    }
  }
  model->Maximize(Sum(xs));
  return model;
}

TEST(HighsSolverTest, HighsOptions_DoubleOptionTimeLimit_NoSolutionInTime) {
  std::unique_ptr<Model> model = BigModel();

  EXPECT_THAT(Solve(*model, SolverType::kHighs), IsOkAndHolds(IsOptimal(1.0)));

  // The problem times out with short time limit as long as presolve is off.
  SolveParameters params = {.presolve = Emphasis::kOff};
  (*params.highs.mutable_double_options())["time_limit"] = 1.0e-4;
  ASSERT_OK_AND_ASSIGN(SolveResult result, Solve(*model, SolverType::kHighs,
                                                 {.parameters = params}));
  EXPECT_THAT(result.termination, LimitIs(Limit::kTime));
}

TEST(HighsSolverTest, HighsOptions_DoubleOptionBadName_InvalidArgument) {
  Model model;
  SolveParameters params;
  (*params.highs.mutable_double_options())["brown_dog"] = 3.0;
  EXPECT_THAT(Solve(model, SolverType::kHighs, {.parameters = params}),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("option name was unknown"),
                             HasSubstr("brown_dog"))));
}

TEST(HighsSolverTest, HighsOptions_DoubleOptionBadValue_InvalidArgument) {
  Model model;
  SolveParameters params;
  (*params.highs.mutable_double_options())["time_limit"] = -3.0;
  EXPECT_THAT(
      Solve(model, SolverType::kHighs, {.parameters = params}),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("value not valid"), HasSubstr("time_limit"))));
}

TEST(HighsSolverTest, HighsOptions_IntOptionPivotLimit_NotOptimal) {
  std::unique_ptr<Model> model = BigModel(/*is_integer=*/false);

  SolveParameters params = {.lp_algorithm = LPAlgorithm::kPrimalSimplex};
  // The LP has objective 50.0, we should find it without a pivot limit.
  EXPECT_THAT(Solve(*model, SolverType::kHighs, {.parameters = params}),
              IsOkAndHolds(IsOptimal(50.0)));

  // Add a pivot limit, now we should terminate suboptimally.
  (*params.highs.mutable_int_options())["simplex_iteration_limit"] = 3;
  ASSERT_OK_AND_ASSIGN(SolveResult result, Solve(*model, SolverType::kHighs,
                                                 {.parameters = params}));
  EXPECT_THAT(result.termination, LimitIs(Limit::kIteration));
}

TEST(HighsSolverTest, HighsOptions_IntOptionBadName_InvalidArgument) {
  Model model;
  SolveParameters params;
  (*params.highs.mutable_int_options())["brown_dog"] = 3;
  EXPECT_THAT(Solve(model, SolverType::kHighs, {.parameters = params}),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("option name was unknown"),
                             HasSubstr("brown_dog"))));
}

TEST(HighsSolverTest, HighsOptions_StringOptionPresolve_PivotCount) {
  // Model is:
  //    max x + y
  //    s.t. x + y < = 1
  //         x, y in [0, 1]
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 1.0);
  const Variable y = model.AddContinuousVariable(0.0, 1.0);
  const Variable z = model.AddContinuousVariable(0.0, 1.0);
  model.Maximize(x + y + z);
  model.AddLinearConstraint(x + y + z <= 1.5);

  {
    SolveParameters params;
    (*params.highs.mutable_string_options())["presolve"] = "off";
    ASSERT_OK_AND_ASSIGN(
        const SolveResult result,
        Solve(model, SolverType::kHighs, {.parameters = params}));
    ASSERT_THAT(result, IsOptimal(1.5));
    EXPECT_GT(result.solve_stats.simplex_iterations, 0);
  }
  // With presolve on, we solve in presolve and do not pivot.
  {
    ASSERT_OK_AND_ASSIGN(const SolveResult result,
                         Solve(model, SolverType::kHighs, {.parameters = {}}));
    ASSERT_THAT(result, IsOptimal(1.5));
    EXPECT_EQ(result.solve_stats.simplex_iterations, 0);
  }
}

TEST(HighsSolverTest, HighsOptions_StringOptionBadName_InvalidArgument) {
  Model model;
  SolveParameters params;
  (*params.highs.mutable_string_options())["brown_dog"] = "cow";
  EXPECT_THAT(Solve(model, SolverType::kHighs, {.parameters = params}),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("option name was unknown"),
                             HasSubstr("brown_dog"))));
}

}  //  namespace
}  // namespace operations_research::math_opt
