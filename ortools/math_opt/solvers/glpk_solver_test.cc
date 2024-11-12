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
#include <memory>
#include <optional>
#include <string>
#include <thread>  // NOLINT(build/c++11)
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/core/solver.h"
#include "ortools/math_opt/cpp/matchers.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/math_opt/solver_tests/callback_tests.h"
#include "ortools/math_opt/solver_tests/generic_tests.h"
#include "ortools/math_opt/solver_tests/infeasible_subsystem_tests.h"
#include "ortools/math_opt/solver_tests/invalid_input_tests.h"
#include "ortools/math_opt/solver_tests/ip_model_solve_parameters_tests.h"
#include "ortools/math_opt/solver_tests/ip_parameter_tests.h"
#include "ortools/math_opt/solver_tests/logical_constraint_tests.h"
#include "ortools/math_opt/solver_tests/lp_model_solve_parameters_tests.h"
#include "ortools/math_opt/solver_tests/lp_tests.h"
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
using ::testing::status::IsOkAndHolds;
using ::testing::status::StatusIs;

constexpr double kInf = std::numeric_limits<double>::infinity();

std::vector<StatusTestParameters> MakeStatusTestConfigs() {
  std::vector<StatusTestParameters> test_parameters;
  for (const auto algorithm : std::vector<std::optional<LPAlgorithm>>(
           {std::nullopt, LPAlgorithm::kBarrier, LPAlgorithm::kPrimalSimplex,
            LPAlgorithm::kDualSimplex})) {
    SolveParameters solve_parameters = {.lp_algorithm = algorithm};
    test_parameters.push_back(StatusTestParameters(
        SolverType::kGlpk, solve_parameters,
        /*disallow_primal_or_dual_infeasible=*/false,
        /*supports_iteration_limit=*/false,
        /*use_integer_variables=*/false,
        /*supports_node_limit=*/false,
        /*support_interrupter=*/false, /*supports_one_thread=*/true));
  }

  // Cannot set the lp_algorithm for with integer variables.
  test_parameters.push_back(StatusTestParameters(
      SolverType::kGlpk, SolveParameters{},
      /*disallow_primal_or_dual_infeasible=*/false,
      /*supports_iteration_limit=*/false,
      /*use_integer_variables=*/true,
      /*supports_node_limit=*/false,
      /*support_interrupter=*/true, /*supports_one_thread=*/true));
  return test_parameters;
}

INSTANTIATE_TEST_SUITE_P(GlpkStatusTest, StatusTest,
                         ValuesIn(MakeStatusTestConfigs()));

InvalidParameterTestParams InvalidThreadsParameters() {
  SolveParameters params;
  params.threads = 2;
  return InvalidParameterTestParams(SolverType::kGlpk, std::move(params),
                                    {"threads"});
}

INSTANTIATE_TEST_SUITE_P(
    GlpkInvalidInputTest, InvalidInputTest,
    Values(InvalidInputTestParameters(SolverType::kGlpk,
                                      /*use_integer_variables=*/false)));
INSTANTIATE_TEST_SUITE_P(GlpkInvalidParameterTest, InvalidParameterTest,
                         Values(InvalidThreadsParameters()));

INSTANTIATE_TEST_SUITE_P(GlpkIpModelSolveParametersTest,
                         IpModelSolveParametersTest, Values(SolverType::kGlpk));

// Glpk does not support MIP solution hints at this point.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(MipSolutionHintTest);

// Glpk does not support MIP branch priorities or lazy constraints at this
// point.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(BranchPrioritiesTest);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(LazyConstraintsTest);

SolveResultSupport GlpkMipSolveResultSupport() {
  return {
      .termination_limit = true,
      // This is not exposed by the API, it would require parsing the output.
      .iteration_stats = false,
      // `node_count` could perhaps be computed with callbacks.
      .node_count = false};
}

ParameterSupport GlpkMipParameterSupport() {
  return {.supports_one_thread = true};
}

SolveParameters StopBeforeOptimal() {
  // A deterministic limit would be better, but none are support.
  SolveParameters params = {.time_limit = absl::Microseconds(1)};
  return params;
}

INSTANTIATE_TEST_SUITE_P(
    GlpkIpParameterTest, IpParameterTest,
    Values(IpParameterTestParameters{
        .name = "default",
        .solver_type = SolverType::kGlpk,
        .parameter_support = GlpkMipParameterSupport(),
        .hint_supported = false,
        .solve_result_support = GlpkMipSolveResultSupport(),
        // .presolved_regexp since presolve is not supported.
        .stop_before_optimal = StopBeforeOptimal()}),
    ParamName{});

// TODO(b/270997189): get these tests working on ios.
#if defined(__APPLE__)
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(LargeInstanceIpParameterTest);
#else
INSTANTIATE_TEST_SUITE_P(GlpkLargeInstanceIpParameterTest,
                         LargeInstanceIpParameterTest,
                         Values(LargeInstanceTestParams{
                             .name = "default",
                             .solver_type = SolverType::kGlpk,
                             .parameter_support = GlpkMipParameterSupport()}),
                         ParamName{});
#endif

INSTANTIATE_TEST_SUITE_P(GlpkLpModelSolveParametersTest,
                         LpModelSolveParametersTest,
                         Values(LpModelSolveParametersTestParameters(
                             SolverType::kGlpk, /*exact_zeros=*/true,
                             /*supports_duals=*/true,
                             /*supports_primal_only_warm_starts=*/false)));

// TODO(b/187027049): see rationale in the TODO comment of IpParameterTest.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(LpParameterTest);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(LpIncompleteSolveTest);

std::vector<SimpleLpTestParameters> GetGlpkSimpleLpTestParameters() {
  std::vector<SimpleLpTestParameters> test_parameters;
  for (const auto algorithm :
       {LPAlgorithm::kBarrier, LPAlgorithm::kPrimalSimplex,
        LPAlgorithm::kDualSimplex}) {
    for (const auto presolve : {Emphasis::kMedium, Emphasis::kOff}) {
      // Rays and precise infeasible/unbounded information are only available
      // with presolve off.
      const bool ensures_primal_ray =
          algorithm == LPAlgorithm::kPrimalSimplex &&
          presolve == Emphasis::kOff;
      const bool ensures_dual_ray =
          algorithm == LPAlgorithm::kDualSimplex && presolve == Emphasis::kOff;
      const bool disallows_infeasible_or_unbounded =
          algorithm == LPAlgorithm::kPrimalSimplex &&
          presolve == Emphasis::kOff;
      test_parameters.push_back(SimpleLpTestParameters(
          SolverType::kGlpk,
          {.lp_algorithm = algorithm,
           .presolve = presolve,
           .glpk = {.compute_unbound_rays_if_possible =
                        ensures_primal_ray || ensures_dual_ray}},
          /*supports_duals=*/true, /*supports_basis=*/false,
          /*ensures_primal_ray=*/ensures_primal_ray,
          /*ensures_dual_ray=*/ensures_dual_ray,
          /*disallows_infeasible_or_unbounded=*/
          disallows_infeasible_or_unbounded));
    }
  }
  return test_parameters;
}

INSTANTIATE_TEST_SUITE_P(GlpkSimpleLpTest, SimpleLpTest,
                         ValuesIn(GetGlpkSimpleLpTestParameters()));

INSTANTIATE_TEST_SUITE_P(GlpkIncrementalLpTest, IncrementalLpTest,
                         Values(SolverType::kGlpk));

INSTANTIATE_TEST_SUITE_P(GlpkSimpleMipTest, SimpleMipTest,
                         Values(SolverType::kGlpk));
INSTANTIATE_TEST_SUITE_P(GlpkIncrementalMipTest, IncrementalMipTest,
                         Values(SolverType::kGlpk));

MultiObjectiveTestParameters GetGlpkMultiObjectiveTestParameters() {
  return MultiObjectiveTestParameters(
      /*solver_type=*/SolverType::kGlpk, /*parameters=*/SolveParameters(),
      /*supports_auxiliary_objectives=*/false,
      /*supports_incremental_objective_add_and_delete=*/false,
      /*supports_incremental_objective_modification=*/false,
      /*supports_integer_variables=*/true);
}
// TODO(b/270997189): get these tests working on ios.
#if defined(__APPLE__)
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(SimpleMultiObjectiveTest);
#else
INSTANTIATE_TEST_SUITE_P(GlpkSimpleMultiObjectiveTest, SimpleMultiObjectiveTest,
                         Values(GetGlpkMultiObjectiveTestParameters()));
#endif

INSTANTIATE_TEST_SUITE_P(GlpkIncrementalMultiObjectiveTest,
                         IncrementalMultiObjectiveTest,
                         Values(GetGlpkMultiObjectiveTestParameters()));

std::vector<QpTestParameters> GetGlpkQpTestParameters() {
  return {QpTestParameters(SolverType::kGlpk, SolveParameters(),
                           /*qp_support=*/QpSupportType::kNoQpSupport,
                           /*supports_incrementalism_not_modifying_qp=*/true,
                           /*supports_qp_incrementalism=*/false,
                           /*use_integer_variables=*/false),
          QpTestParameters(SolverType::kGlpk, SolveParameters(),
                           /*qp_support=*/QpSupportType::kNoQpSupport,
                           /*supports_incrementalism_not_modifying_qp=*/true,
                           /*supports_qp_incrementalism=*/false,
                           /*use_integer_variables=*/true)};
}

INSTANTIATE_TEST_SUITE_P(GlpkSimpleQpTest, SimpleQpTest,
                         ValuesIn(GetGlpkQpTestParameters()));
INSTANTIATE_TEST_SUITE_P(GlpkIncrementalQpTest, IncrementalQpTest,
                         ValuesIn(GetGlpkQpTestParameters()));
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(QpDualsTest);

std::vector<QcTestParameters> GetGlpkQcTestParameters() {
  return {QcTestParameters(SolverType::kGlpk, SolveParameters(),
                           /*supports_qc=*/false,
                           /*supports_incremental_add_and_deletes=*/false,
                           /*supports_incremental_variable_deletions=*/false,
                           /*use_integer_variables=*/false),
          QcTestParameters(SolverType::kGlpk, SolveParameters(),
                           /*supports_qc=*/false,
                           /*supports_incremental_add_and_deletes=*/false,
                           /*supports_incremental_variable_deletions=*/false,
                           /*use_integer_variables=*/true)};
}

INSTANTIATE_TEST_SUITE_P(GlpkSimpleQcTest, SimpleQcTest,
                         ValuesIn(GetGlpkQcTestParameters()));
INSTANTIATE_TEST_SUITE_P(GlpkIncrementalQcTest, IncrementalQcTest,
                         ValuesIn(GetGlpkQcTestParameters()));
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(QcDualsTest);

SecondOrderConeTestParameters GetGlpkSecondOrderConeTestParameters() {
  return SecondOrderConeTestParameters(
      SolverType::kGlpk, SolveParameters(),
      /*supports_soc_constraints=*/false,
      /*supports_incremental_add_and_deletes=*/false);
}

INSTANTIATE_TEST_SUITE_P(GlpkSimpleSecondOrderConeTest,
                         SimpleSecondOrderConeTest,
                         Values(GetGlpkSecondOrderConeTestParameters()));
INSTANTIATE_TEST_SUITE_P(GlpkIncrementalSecondOrderConeTest,
                         IncrementalSecondOrderConeTest,
                         Values(GetGlpkSecondOrderConeTestParameters()));

LogicalConstraintTestParameters GetGlpkLogicalConstraintTestParameters() {
  return LogicalConstraintTestParameters(
      SolverType::kGlpk, SolveParameters(),
      /*supports_integer_variables=*/true,
      /*supports_sos1=*/false,
      /*supports_sos2=*/false,
      /*supports_indicator_constraints=*/false,
      /*supports_incremental_add_and_deletes=*/false,
      /*supports_incremental_variable_deletions=*/false,
      /*supports_deleting_indicator_variables=*/false,
      /*supports_updating_binary_variables=*/true);
}
INSTANTIATE_TEST_SUITE_P(GlpkSimpleLogicalConstraintTest,
                         SimpleLogicalConstraintTest,
                         Values(GetGlpkLogicalConstraintTestParameters()));
INSTANTIATE_TEST_SUITE_P(GlpkIncrementalLogicalConstraintTest,
                         IncrementalLogicalConstraintTest,
                         Values(GetGlpkLogicalConstraintTestParameters()));

SolveParameters UseInteriorPointParameters() {
  return {.lp_algorithm = LPAlgorithm::kBarrier};
}

INSTANTIATE_TEST_SUITE_P(
    GlpkGenericTest, GenericTest,
    Values(GenericTestParameters(SolverType::kGlpk,
                                 /*support_interrupter=*/true,
                                 /*integer_variables=*/true,
                                 /*expected_log=*/"OPTIMAL SOLUTION FOUND"),
           // When GLPK solves linear programs, it does not support
           // interruption.
           GenericTestParameters(SolverType::kGlpk,
                                 /*support_interrupter=*/false,
                                 /*integer_variables=*/false,
                                 /*expected_log=*/"OPTIMAL SOLUTION FOUND"),
           // GLPK has different code path for interior point.
           GenericTestParameters(SolverType::kGlpk,
                                 /*support_interrupter=*/false,
                                 /*integer_variables=*/false,
                                 /*expected_log=*/"OPTIMAL SOLUTION FOUND",
                                 UseInteriorPointParameters())));

// TODO(b/187027049): When GLPK callbacks are supported, enable this test.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(TimeLimitTest);

INSTANTIATE_TEST_SUITE_P(
    GlpkMessageCallbackTest, MessageCallbackTest,
    Values(MessageCallbackTestParams(SolverType::kGlpk,
                                     /*support_message_callback=*/true,
                                     /*support_interrupter=*/true,
                                     /*integer_variables=*/true,
                                     "INTEGER OPTIMAL SOLUTION FOUND"),
           // When GLPK solves linear programs, it does not support
           // interruption.
           MessageCallbackTestParams(SolverType::kGlpk,
                                     /*support_message_callback=*/true,
                                     /*support_interrupter=*/false,
                                     /*integer_variables=*/false,
                                     "OPTIMAL SOLUTION FOUND")));

INSTANTIATE_TEST_SUITE_P(
    GlpkCallbackTest, CallbackTest,
    Values(CallbackTestParams(SolverType::kGlpk,
                              /*integer_variables=*/false,
                              /*add_lazy_constraints=*/false,
                              /*add_cuts=*/false,
                              /*supported_events=*/{},
                              /*all_solutions=*/std::nullopt,
                              /*reaches_cut_callback*/ std::nullopt)));

INSTANTIATE_TEST_SUITE_P(GlpkInfeasibleSubsystemTest, InfeasibleSubsystemTest,
                         testing::Values(InfeasibleSubsystemTestParameters(
                             {.solver_type = SolverType::kGlpk})));

// Validates that if `threads` parameters is set to 1 (the only valid value),
// the solver accepts it.
TEST(GlpkSolverTest, TestThreadsEqOne) {
  ASSERT_OK_AND_ASSIGN(
      const std::unique_ptr<Solver> solver,
      Solver::New(SOLVER_TYPE_GLPK, ModelProto{}, /*arguments=*/{}));
  Solver::SolveArgs solve_args;
  solve_args.parameters.set_threads(1);
  ASSERT_OK_AND_ASSIGN(const SolveResultProto result,
                       solver->Solve(solve_args));
}

// TODO(b/187027049): move this test in generic LP tests
TEST(GlpkSolverTest, InteriorPoint) {
  Model model("interior point");
  const Variable x = model.AddContinuousVariable(0.0, 2.5, "x");
  const LinearConstraint c = model.AddLinearConstraint(x <= 1.5, "c");
  model.Maximize(x);

  ASSERT_OK_AND_ASSIGN(
      const SolveResult result,
      Solve(model, SolverType::kGlpk,
            {.parameters = {.lp_algorithm = LPAlgorithm::kBarrier}}));
  EXPECT_THAT(result, IsOptimalWithSolution(1.5, {{x, 1.5}}));
  EXPECT_THAT(result, IsOptimalWithDualSolution(1.5, {{c, 1.0}}, {{x, 0.0}}));
}

// TODO(b/187027049): move this test in generic LP tests
TEST(GlpkSolverTest, InteriorPointNoCrossover) {
  Model model("interior point");
  const Variable x = model.AddContinuousVariable(0.0, 1.0, "x");
  const Variable y = model.AddContinuousVariable(0.0, 1.0, "y");
  model.Maximize(x);

  ASSERT_OK_AND_ASSIGN(
      const SolveResult result,
      Solve(model, SolverType::kGlpk,
            {.parameters = {.lp_algorithm = LPAlgorithm::kBarrier}}));
  EXPECT_THAT(result, IsOptimalWithSolution(1.0, {{x, 1.0}, {y, 0.5}}));
}

// TODO(b/187027049): move this in GenericTest.
TEST(GlpkSolverTest, InteriorPointOnlyColumns) {
  Model model("interior point");
  const Variable x = model.AddContinuousVariable(0.0, 2.5, "x");
  const Variable y = model.AddContinuousVariable(0.0, 1.5, "y");
  model.Maximize(x + y);

  ASSERT_OK_AND_ASSIGN(
      const SolveResult result,
      Solve(model, SolverType::kGlpk,
            {.parameters = {.lp_algorithm = LPAlgorithm::kBarrier}}));
  EXPECT_THAT(result, IsOptimalWithSolution(4.0, {{x, 2.5}, {y, 1.5}}));
  EXPECT_THAT(result, IsOptimalWithDualSolution(4.0, {}, {{x, 1.0}, {y, 1.0}}));
}

// TODO(b/187027049): move this in GenericTest.
TEST(GlpkSolverTest, InteriorPointOnlyRows) {
  Model model("interior point");
  const LinearConstraint c = model.AddLinearConstraint(-1.0, 1.5, "c");
  const LinearConstraint d = model.AddLinearConstraint(-1.5, 2.5, "d");
  model.Maximize(0);

  ASSERT_OK_AND_ASSIGN(
      const SolveResult result,
      Solve(model, SolverType::kGlpk,
            {.parameters = {.lp_algorithm = LPAlgorithm::kBarrier}}));
  EXPECT_THAT(result, IsOptimalWithSolution(0.0, {}));
  EXPECT_THAT(result, IsOptimalWithDualSolution(0.0, {{c, 0.0}, {d, 0.0}}, {}));
}

TEST(GlpkSolverDeathTest, DestroySolverFromAnotherThread) {
  Model model("model");

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<IncrementalSolver> incremental_solver,
                       NewIncrementalSolver(&model, SolverType::kGlpk));

#if 0
  EXPECT_DEATH_IF_SUPPORTED(
      // Destroy the solver from another thread. This crashes since GLPK detects
      // that the memory of the problem was allocated in another thread (and
      // thus another GLPK environment).
      std::thread([&]() { incremental_solver.reset(); }).join(),
#if !defined(__ANDROID__) || __ARM_ARCH < 7
      // We expect a stack to be visible with ~GlpkSolver.
      "~GlpkSolver()"
#else
      // On Android armeabi v7a the code crashes but nothing is captured in the
      // output that is considered by EXPECT_DEATH.
      ""
#endif
  );
#endif
}

TEST(GlpkSolverTest, SolveFromAnotherThread) {
  Model model("model");
  const Variable x = model.AddContinuousVariable(0.0, 2.5, "x");
  const Variable y = model.AddContinuousVariable(0.0, 1.5, "y");
  model.Maximize(x + y);

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<IncrementalSolver> incremental_solver,
                       NewIncrementalSolver(&model, SolverType::kGlpk));

  absl::StatusOr<SolveResult> solve_result_or;
  std::thread([&]() { solve_result_or = incremental_solver->Solve(); }).join();

  EXPECT_THAT(solve_result_or, StatusIs(absl::StatusCode::kInvalidArgument,
                                        HasSubstr("GLPK is not thread-safe")));
}

TEST(GlpkSolverTest, UpdateFromAnotherThread) {
  Model model("model");
  const Variable x = model.AddContinuousVariable(0.0, 2.5, "x");
  const Variable y = model.AddContinuousVariable(0.0, 1.5, "y");
  model.Maximize(x + y);

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<IncrementalSolver> incremental_solver,
                       NewIncrementalSolver(&model, SolverType::kGlpk));

  model.set_lower_bound(x, 1.2);

  absl::StatusOr<UpdateResult> update_result_or;
  std::thread([&]() {
    update_result_or = incremental_solver->Update();
  }).join();

  EXPECT_THAT(update_result_or, StatusIs(absl::StatusCode::kInvalidArgument,
                                         HasSubstr("GLPK is not thread-safe")));
}

TEST(GlpkSolverTest, FailedUpdateFromAnotherThread) {
  Model model("model");
  const Variable x = model.AddContinuousVariable(0.0, 2.5, "x");
  const Variable y = model.AddContinuousVariable(0.0, 1.5, "y");
  model.Maximize(x + y);

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<IncrementalSolver> incremental_solver,
                       NewIncrementalSolver(&model, SolverType::kGlpk));

  // Quadratic objectives are not supported by GLPK.
  model.Maximize(x * x);

  absl::StatusOr<UpdateResult> update_result_or;
  std::thread([&]() {
    update_result_or = incremental_solver->Update();
  }).join();

  EXPECT_THAT(update_result_or, StatusIs(absl::StatusCode::kInvalidArgument,
                                         HasSubstr("GLPK is not thread-safe")));
}

// TODO(b/290091715): Remove once new validators are added.
TEST(GlpkSolverTest, InfeasibleMax) {
  Model model("model");
  const Variable x = model.AddContinuousVariable(0.0, 1.0, "x");
  const Variable y = model.AddContinuousVariable(0.0, 1.0, "y");
  model.AddLinearConstraint(x + y >= 3.0, "c");
  model.Maximize(x);

  ASSERT_OK_AND_ASSIGN(const SolveResult result,
                       Solve(model, SolverType::kGlpk, {}));

  ASSERT_THAT(result, TerminatesWith(TerminationReason::kInfeasible));
  ASSERT_EQ(result.termination.problem_status.dual_status,
            FeasibilityStatus::kFeasible);
  EXPECT_EQ(result.termination.objective_bounds.dual_bound, -kInf);
}

// TODO(b/290091715): Remove once new validators are added.
TEST(GlpkSolverTest, InfeasibleMin) {
  Model model("model");
  const Variable x = model.AddContinuousVariable(0.0, 1.0, "x");
  const Variable y = model.AddContinuousVariable(0.0, 1.0, "y");
  model.AddLinearConstraint(x + y >= 3.0, "c");
  model.Minimize(-x);

  ASSERT_OK_AND_ASSIGN(const SolveResult result,
                       Solve(model, SolverType::kGlpk, {}));

  ASSERT_THAT(result, TerminatesWith(TerminationReason::kInfeasible));
  ASSERT_EQ(result.termination.problem_status.dual_status,
            FeasibilityStatus::kFeasible);
  EXPECT_EQ(result.termination.objective_bounds.dual_bound, +kInf);
}

// TODO(b/187027049): move this in LpTest.
TEST(GlpkSolverTest, PrimalRayMaximization) {
  Model model("model");
  const Variable x = model.AddContinuousVariable(-3.0, kInf, "x");
  const Variable y = model.AddContinuousVariable(-kInf, kInf, "y");
  model.AddLinearConstraint(2 * x == -y + 2, "c");
  model.Maximize(x);

  ASSERT_OK_AND_ASSIGN(
      const SolveResult result,
      Solve(model, SolverType::kGlpk,
            {.parameters = {
                 .glpk = {.compute_unbound_rays_if_possible = true},
             }}));
  EXPECT_THAT(result, TerminatesWith(TerminationReason::kUnbounded));
  EXPECT_THAT(result, HasPrimalRay({{x, 1}, {y, -2}}));
  // TODO(b/290091715): Remove once new validators are added.

  EXPECT_EQ(result.termination.objective_bounds.primal_bound, kInf);
}

// TODO(b/187027049): move this in LpTest.
TEST(GlpkSolverTest, PrimalRayMinimization) {
  Model model("model");
  const Variable x = model.AddContinuousVariable(-kInf, 3.0, "x");
  const Variable y = model.AddContinuousVariable(-kInf, kInf, "y");
  model.AddLinearConstraint(2 * x == -y + 2, "c");
  model.Minimize(x);

  ASSERT_OK_AND_ASSIGN(
      const SolveResult result,
      Solve(model, SolverType::kGlpk,
            {.parameters = {
                 .glpk = {.compute_unbound_rays_if_possible = true},
             }}));
  EXPECT_THAT(result, TerminatesWith(TerminationReason::kUnbounded));
  EXPECT_THAT(result, HasPrimalRay({{x, -1}, {y, 2}}));
  // TODO(b/290091715): Remove once new validators are added.
  EXPECT_EQ(result.termination.objective_bounds.primal_bound, -kInf);
}

// Test the case where the dual simplex is applied to a primal unbounded problem
// and the solver returns INFEAS for the primal solution and NOFEAS for the dual
// solution. For this case glp_get_status() would return INFEAS since it does
// not have a status for dual infeasible (GLP_NOFEAS from this function
// indicates that the primal has been proven infeasible).
TEST(GlpkSolverTest, PrimalUnboundedWithDualSimplex) {
  Model model("model");
  const Variable x = model.AddContinuousVariable(0, kInf, "x");
  const Variable y = model.AddContinuousVariable(0, kInf, "y");
  model.AddLinearConstraint(x + y >= 1, "c1");
  model.AddLinearConstraint(y <= 0, "c2");
  model.Maximize(2 * x - y);
  ASSERT_OK_AND_ASSIGN(
      const SolveResult result,
      Solve(model, SolverType::kGlpk,
            {.parameters = {
                 .lp_algorithm = LPAlgorithm::kDualSimplex,
                 .glpk = {.compute_unbound_rays_if_possible = true},
             }}));
  // We run the dual simplex, hence we will never return kUnbounded here since
  // the dual simplex can't lead to that conclusion.
  EXPECT_THAT(result,
              TerminatesWith(TerminationReason::kInfeasibleOrUnbounded));
}

TEST(GlpkSolverTest, UnboundedWithPresolve) {
  Model model("model");
  const Variable x = model.AddContinuousVariable(-3.0, kInf, "x");
  const Variable y = model.AddContinuousVariable(-kInf, kInf, "y");
  model.AddLinearConstraint(2 * x == -y + 2, "c");
  model.Maximize(x);

  ASSERT_OK_AND_ASSIGN(
      const SolveResult result,
      Solve(model, SolverType::kGlpk,
            {.parameters = {.presolve = Emphasis::kVeryHigh,
                            .glpk = {
                                .compute_unbound_rays_if_possible = true,
                            }}}));

  // GLPK presolver only returns a solution if the status is optimal. Thus we
  // expect no solution or rays.
  EXPECT_THAT(result,
              TerminatesWithOneOf({TerminationReason::kUnbounded,
                                   TerminationReason::kInfeasibleOrUnbounded}));
  EXPECT_FALSE(result.has_primal_feasible_solution());
  EXPECT_FALSE(result.has_ray());
}

TEST(GlpkSolverTest, TriviallyUnboundedLP) {
  // This model is trivial enough that we trigger GLPK's trivial_lp(). This does
  // not lead to the factorization of the problem matrix but since the problem
  // is unbounded, glp_get_unbnd_ray() will indeed return a non 0 value.
  //
  // With this test we make sure that the ray computation code does not crash
  // when the factorization does not exist but the problem is solved by
  // trivial_lp().
  Model model("model");
  const Variable x = model.AddContinuousVariable(/*lower_bound=*/0.0,
                                                 /*upper_bound=*/kInf, "x");
  model.Maximize(x);
  model.AddLinearConstraint(/*lower_bound=*/-kInf, /*upper_bound=*/0, "c");
  ASSERT_OK_AND_ASSIGN(
      const SolveResult result,
      Solve(model, SolverType::kGlpk,
            {
                .parameters = {.glpk = {.compute_unbound_rays_if_possible =
                                            true}},
                .message_callback = InfoLoggerMessageCallback("[solver] "),
            }));
  EXPECT_THAT(result, TerminatesWithOneOf({TerminationReason::kUnbounded}));
  ASSERT_FALSE(result.primal_rays.empty());
  EXPECT_THAT(result.primal_rays[0], PrimalRayIsNear({{x, 1.0}}));
}

// TODO(b/215739511): enable this test if the problem gets fixed.
TEST(GlpkSolverTest, DISABLED_InteriorPointTrivialBounds) {
  // For some specific non-empty models, glp_interior() returns GLP_EFAIL as-if
  // they were empty.
  //
  // This model below reproduces the issue only with some specific bounds on the
  // variable.
  // Using for example:
  //   model.AddContinuousVariable(-1.0, 2.0, "x");
  // does not result in the issue.
  //
  // When the issue reproduces, the GLPK logs reads:
  // ```
  // [glpk] Original LP has 0 row(s), 1 column(s), and 0 non-zero(s)
  // [glpk] Working LP has 0 row(s), 0 column(s), and 0 non-zero(s)
  // [glpk] glp_interior: unable to solve empty problem
  // ```
  //
  // And when they don't (for example with bounds -1.0 and 2.0):
  // ```
  // [glpk] Original LP has 0 row(s), 1 column(s), and 0 non-zero(s)
  // [glpk] Working LP has 1 row(s), 2 column(s), and 2 non-zero(s)
  // ```
  //
  // So it seems that the construction of this "Working LP" is the cause of this
  // issue. It is not clear if we can do something in MathOpt to bypass this
  // bug.
  //
  //
  // The same issue also reproduces with a single constraint with infinite
  // bounds and no variables in the model.
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 0.0, "x");
  model.Maximize(2.0 * x + 4.0);
  EXPECT_THAT(Solve(model, SolverType::kGlpk,
                    {.parameters = {.lp_algorithm = LPAlgorithm::kBarrier},
                     .message_callback = InfoLoggerMessageCallback("[glpk] ")}),
              IsOkAndHolds(IsOptimal(4.0)));
}

}  // namespace
}  // namespace math_opt
}  // namespace operations_research
