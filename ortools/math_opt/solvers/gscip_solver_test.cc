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

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
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
#include "ortools/math_opt/solvers/gscip/gscip_parameters.h"
#include "ortools/math_opt/testing/param_name.h"
#include "ortools/port/scoped_std_stream_capture.h"

namespace operations_research {
namespace math_opt {
namespace {

using ::testing::HasSubstr;
using ::testing::Values;
using ::testing::ValuesIn;
using ::testing::status::IsOkAndHolds;
using ::testing::status::StatusIs;

StatusTestParameters StatusDefault() {
  return StatusTestParameters(SolverType::kGscip, SolveParameters(),
                              /*disallow_primal_or_dual_infeasible=*/false,
                              /*supports_iteration_limit=*/false,
                              /*use_integer_variables=*/true,
                              /*supports_node_limit=*/true,
                              /*support_interrupter=*/true,
                              /*supports_one_thread=*/true);
}

INSTANTIATE_TEST_SUITE_P(GScipStatusTest, StatusTest, Values(StatusDefault()));

INSTANTIATE_TEST_SUITE_P(GScipSimpleMipTest, SimpleMipTest,
                         Values(SolverType::kGscip));

INSTANTIATE_TEST_SUITE_P(GScipIncrementalMipTest, IncrementalMipTest,
                         Values(SolverType::kGscip));

MultiObjectiveTestParameters GetGscipMultiObjectiveTestParameters() {
  return MultiObjectiveTestParameters(
      /*solver_type=*/SolverType::kGscip, /*parameters=*/SolveParameters(),
      /*supports_auxiliary_objectives=*/false,
      /*supports_incremental_objective_add_and_delete=*/false,
      /*supports_incremental_objective_modification=*/false,
      /*supports_integer_variables=*/true);
}

INSTANTIATE_TEST_SUITE_P(GscipSimpleMultiObjectiveTest,
                         SimpleMultiObjectiveTest,
                         Values(GetGscipMultiObjectiveTestParameters()));

INSTANTIATE_TEST_SUITE_P(GscipIncrementalMultiObjectiveTest,
                         IncrementalMultiObjectiveTest,
                         Values(GetGscipMultiObjectiveTestParameters()));

std::vector<QpTestParameters> GetGscipQpTestParameters() {
  return {QpTestParameters(SolverType::kGscip, SolveParameters(),
                           /*qp_support=*/QpSupportType::kConvexQp,
                           /*supports_incrementalism_not_modifying_qp=*/true,
                           /*supports_qp_incrementalism=*/false,
                           /*use_integer_variables=*/false),
          QpTestParameters(SolverType::kGscip, SolveParameters(),
                           /*qp_support=*/QpSupportType::kConvexQp,
                           /*supports_incrementalism_not_modifying_qp=*/true,
                           /*supports_qp_incrementalism=*/false,
                           /*use_integer_variables=*/true)};
}
INSTANTIATE_TEST_SUITE_P(GscipSimpleQpTest, SimpleQpTest,
                         ValuesIn(GetGscipQpTestParameters()));
INSTANTIATE_TEST_SUITE_P(GscipIncrementalQpTest, IncrementalQpTest,
                         ValuesIn(GetGscipQpTestParameters()));
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(QpDualsTest);

std::vector<QcTestParameters> GetGscipQcTestParameters() {
  return {QcTestParameters(SolverType::kGscip, SolveParameters(),
                           /*supports_qc=*/true,
                           /*supports_incremental_add_and_deletes=*/true,
                           /*supports_incremental_variable_deletions=*/false,
                           /*use_integer_variables=*/false),
          QcTestParameters(SolverType::kGscip, SolveParameters(),
                           /*supports_qc=*/true,
                           /*supports_incremental_add_and_deletes=*/true,
                           /*supports_incremental_variable_deletions=*/false,
                           /*use_integer_variables=*/true)};
}
INSTANTIATE_TEST_SUITE_P(GscipSimpleQcTest, SimpleQcTest,
                         ValuesIn(GetGscipQcTestParameters()));
INSTANTIATE_TEST_SUITE_P(GscipIncrementalQcTest, IncrementalQcTest,
                         ValuesIn(GetGscipQcTestParameters()));
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(QcDualsTest);

SecondOrderConeTestParameters GetGscipSecondOrderConeTestParameters() {
  return SecondOrderConeTestParameters(
      SolverType::kGscip, SolveParameters(),
      /*supports_soc_constraints=*/false,
      /*supports_incremental_add_and_deletes=*/false);
}
INSTANTIATE_TEST_SUITE_P(GscipSimpleSecondOrderConeTest,
                         SimpleSecondOrderConeTest,
                         Values(GetGscipSecondOrderConeTestParameters()));
INSTANTIATE_TEST_SUITE_P(GscipIncrementalSecondOrderConeTest,
                         IncrementalSecondOrderConeTest,
                         Values(GetGscipSecondOrderConeTestParameters()));

LogicalConstraintTestParameters GetGscipLogicalConstraintTestParameters() {
  return LogicalConstraintTestParameters(
      SolverType::kGscip, SolveParameters(),
      /*supports_integer_variables=*/true,
      /*supports_sos1=*/true,
      /*supports_sos2=*/true,
      /*supports_indicator_constraints=*/true,
      /*supports_incremental_add_and_deletes=*/true,
      /*supports_incremental_variable_deletions=*/false,
      /*supports_deleting_indicator_variables=*/false,
      /*supports_updating_binary_variables=*/false);
}
INSTANTIATE_TEST_SUITE_P(GscipSimpleLogicalConstraintTest,
                         SimpleLogicalConstraintTest,
                         Values(GetGscipLogicalConstraintTestParameters()));
INSTANTIATE_TEST_SUITE_P(GscipIncrementalLogicalConstraintTest,
                         IncrementalLogicalConstraintTest,
                         Values(GetGscipLogicalConstraintTestParameters()));

INSTANTIATE_TEST_SUITE_P(
    GScipInvalidInputTest, InvalidInputTest,
    Values(InvalidInputTestParameters(SolverType::kGscip,
                                      /*use_integer_variables=*/true)));

SolveParameters StopBeforeOptimal() {
  return {.node_limit = 1,
          .presolve = Emphasis::kOff,
          .cuts = Emphasis::kOff,
          .heuristics = Emphasis::kOff};
}

ParameterSupport GScipParameterSupport() {
  return {.supports_node_limit = true,
          .supports_cutoff = true,
          .supports_solution_limit_one = true,
          .supports_one_thread = true,
          .supports_n_threads = true,
          .supports_random_seed = true,
          .supports_absolute_gap_tolerance = true,
          .supports_lp_algorithm_simplex = true,
          .supports_presolve = true,
          .supports_cuts = true,
          .supports_heuristics = true,
          .supports_scaling = true};
}

SolveResultSupport GScipSolveResultSupport() {
  return {
      .termination_limit = true, .iteration_stats = true, .node_count = true};
}

// NOTE: we should also be able to use the LP tests, but many of them don't work
// for gSCIP.
INSTANTIATE_TEST_SUITE_P(
    GScipIpParameterTest, IpParameterTest,
    Values(IpParameterTestParameters{
        .name = "default",
        .solver_type = SolverType::kGscip,
        .parameter_support = GScipParameterSupport(),
        .hint_supported = true,
        .solve_result_support = GScipSolveResultSupport(),
        .presolved_regexp = R"regexp(presolving \([^0][0-9]* rounds)regexp",
        .stop_before_optimal = StopBeforeOptimal()}),
    ParamName{});

INSTANTIATE_TEST_SUITE_P(GScipLargeInstanceIpParameterTest,
                         LargeInstanceIpParameterTest,
                         Values(LargeInstanceTestParams{
                             .name = "default",
                             .solver_type = SolverType::kGscip,
                             .parameter_support = GScipParameterSupport()}),
                         ParamName{});

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(TimeLimitTest);

InvalidParameterTestParams MakeGScipBadParams() {
  SolveParameters parameters;
  (*parameters.gscip.mutable_bool_params())["dog"] = false;

  // TODO(b/168069105): for solver specific errors, we should collect all
  //  errors, not just the first. Then set int_param "parallel/maxnthreads" to
  //  -4 (an invalid value).
  return InvalidParameterTestParams(SolverType::kGscip, std::move(parameters),
                                    {"SCIP error code -12"});
}

INSTANTIATE_TEST_SUITE_P(GScipInvalidParameterTest, InvalidParameterTest,
                         Values(MakeGScipBadParams()));

INSTANTIATE_TEST_SUITE_P(GScipIpModelSolveParametersTest,
                         IpModelSolveParametersTest,
                         Values(SolverType::kGscip));

INSTANTIATE_TEST_SUITE_P(
    GScipIpMultipleSolutionsTest, IpMultipleSolutionsTest,
    Values(IpMultipleSolutionsTestParams(SolverType::kGscip, {})));

INSTANTIATE_TEST_SUITE_P(
    GScipMessageCallbackTest, MessageCallbackTest,
    Values(MessageCallbackTestParams(SolverType::kGscip,
                                     /*support_message_callback=*/true,
                                     /*support_interrupter=*/true,
                                     /*integer_variables=*/false, "Gap")));

// Solve parameters to ensure a small MIP won't be solved before the MIP_NODE
// callback is invoked.
SolveParameters ReachEventNode() {
  SolveParameters result = {.presolve = Emphasis::kOff,
                            .heuristics = Emphasis::kOff};
  DisableAllCutsExceptUserDefined(&result.gscip);
  return result;
}

INSTANTIATE_TEST_SUITE_P(
    GscipCallbackTest, CallbackTest,
    Values(CallbackTestParams(SolverType::kGscip,
                              /*integer_variables=*/true,
                              /*add_lazy_constraints=*/true,
                              /*add_cuts=*/true,
                              /*supported_events=*/
                              {CallbackEvent::kMipNode,
                               CallbackEvent::kMipSolution},
                              /*all_solutions=*/std::nullopt,
                              /*reaches_cut_callback=*/ReachEventNode())));

SolutionHintTestParams MakeGscipSolutionHintParams() {
  SolveParameters single_hint_params;
  single_hint_params.cuts = Emphasis::kOff;
  single_hint_params.presolve = Emphasis::kOff;
  (*single_hint_params.gscip.mutable_int_params())["limits/solutions"] = 1;
  (*single_hint_params.gscip.mutable_int_params())["heuristics/trivial/freq"] =
      -1;
  SolveParameters two_hint_params = single_hint_params;
  two_hint_params.gscip.set_num_solutions(2);
  (*two_hint_params.gscip.mutable_int_params())["limits/solutions"] = 2;
  std::string hint_message_regex =
      "feasible solution found by completesol heuristic";

  return SolutionHintTestParams(SolverType::kGscip, single_hint_params,
                                two_hint_params, hint_message_regex);
}

INSTANTIATE_TEST_SUITE_P(GScipSolutionHintTest, MipSolutionHintTest,
                         Values(MakeGscipSolutionHintParams()));

BranchPrioritiesTestParams MakeGScipBranchPrioritiesParams() {
  SolveParameters solve_params;
  solve_params.cuts = Emphasis::kOff;
  solve_params.presolve = Emphasis::kOff;
  solve_params.heuristics = Emphasis::kOff;
  solve_params.threads = 1;
  return BranchPrioritiesTestParams(SolverType::kGscip, solve_params);
}

INSTANTIATE_TEST_SUITE_P(GScipBranchPrioritiesTest, BranchPrioritiesTest,
                         Values(MakeGScipBranchPrioritiesParams()));

// Gscip does not support lazy constraints at this point.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(LazyConstraintsTest);

INSTANTIATE_TEST_SUITE_P(
    GscipGenericTest, GenericTest,
    Values(GenericTestParameters(SolverType::kGscip,
                                 /*support_interrupter=*/true,
                                 /*integer_variables=*/false,
                                 /*expected_log=*/"[optimal solution found]"),
           GenericTestParameters(SolverType::kGscip,
                                 /*support_interrupter=*/true,
                                 /*integer_variables=*/true,
                                 /*expected_log=*/"[optimal solution found]")));

INSTANTIATE_TEST_SUITE_P(GscipInfeasibleSubsystemTest, InfeasibleSubsystemTest,
                         testing::Values(InfeasibleSubsystemTestParameters(
                             {.solver_type = SolverType::kGscip})));

#ifdef OPERATIONS_RESEARCH_OUTPUT_CAPTURE_SUPPORTED
// TODO(b/207472017): Enable this test once the issue of warning/error messages
// redirection has been addressed.
TEST(GScipSolverTest, DISABLED_WarningsDuringModelBuilding) {
  // Using an unknown parameters triggers calls to SCIPerrorMessage() and before
  // return of SCIP_PARAMETERUNKNOWN error.
  GScipParameters gscip_params;
  (*gscip_params.mutable_bool_params())["unknown"] = false;
  Model model;
  ScopedStdStreamCapture stdout_capture(CapturedStream::kStdout);
  ScopedStdStreamCapture stderr_capture(CapturedStream::kStderr);
  const absl::StatusOr<SolveResult> result =
      Solve(model, SolverType::kGscip, {.parameters = {.gscip = gscip_params}});
  EXPECT_EQ(std::move(stdout_capture).StopCaptureAndReturnContents(), "");
  EXPECT_EQ(std::move(stderr_capture).StopCaptureAndReturnContents(), "");
  // TODO(b/207474460): Update the test to validate that the offending parameter
  // is listed in the error (it is not at the time of writing this).
  EXPECT_THAT(result, StatusIs(absl::StatusCode::kInvalidArgument));
}
#endif  // OPERATIONS_RESEARCH_OUTPUT_CAPTURE_SUPPORTED

TEST(GScipSolverTest, InvalidCoefficient) {
  Model model;
  const Variable x = model.AddVariable("x");
  model.Maximize(x);
  model.AddLinearConstraint(1.0e123 * x <= 2.0, "broken constraint");
  EXPECT_THAT(Solve(model, SolverType::kGscip),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("broken constraint")));
}

TEST(GScipSolverTest, UpdatingLowerBoundNotAllowedOnBinaryVariables) {
  Model model;
  const Variable x = model.AddBinaryVariable("x");
  ASSERT_OK_AND_ASSIGN(auto solver,
                       NewIncrementalSolver(&model, SolverType::kGscip, {}));

  model.set_lower_bound(x, -1.0);
  EXPECT_THAT(solver->Update(), IsOkAndHolds(Not(DidUpdate())));
}

TEST(GScipSolverTest, UpdatingUpperBoundNotAllowedOnBinaryVariables) {
  Model model;
  const Variable x = model.AddBinaryVariable("x");
  ASSERT_OK_AND_ASSIGN(auto solver,
                       NewIncrementalSolver(&model, SolverType::kGscip, {}));

  model.set_upper_bound(x, 2.0);
  EXPECT_THAT(solver->Update(), IsOkAndHolds(Not(DidUpdate())));
}

TEST(GScipSolverTest, UpdatingLowerBoundNotAllowedOnImplicitBinaryVariables) {
  Model model;
  // This will be silently converted to a binary variable in SCIP.
  const Variable y = model.AddIntegerVariable(0.0, 1.0, "y");
  ASSERT_OK_AND_ASSIGN(auto solver,
                       NewIncrementalSolver(&model, SolverType::kGscip, {}));

  model.set_lower_bound(y, -1.0);
  EXPECT_THAT(solver->Update(), IsOkAndHolds(Not(DidUpdate())));
}

TEST(GScipSolverTest, UpdatingUpperBoundNotAllowedOnImplicitBinaryVariables) {
  Model model;
  // This will be silently converted to a binary variable in SCIP.
  const Variable y = model.AddIntegerVariable(0.0, 1.0, "y");
  ASSERT_OK_AND_ASSIGN(auto solver,
                       NewIncrementalSolver(&model, SolverType::kGscip, {}));

  model.set_upper_bound(y, 2.0);
  EXPECT_THAT(solver->Update(), IsOkAndHolds(Not(DidUpdate())));
}

}  // namespace
}  // namespace math_opt
}  // namespace operations_research
