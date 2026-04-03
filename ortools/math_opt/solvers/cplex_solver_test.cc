// Copyright 2010-2026 Google LLC
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

#include "ortools/math_opt/solvers/cplex_solver.h"

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
#include "ortools/math_opt/solver_tests/invalid_input_tests.h"
#include "ortools/math_opt/solver_tests/ip_model_solve_parameters_tests.h"
#include "ortools/math_opt/solver_tests/ip_multiple_solutions_tests.h"
#include "ortools/math_opt/solver_tests/ip_parameter_tests.h"
#include "ortools/math_opt/solver_tests/lp_incomplete_solve_tests.h"
#include "ortools/math_opt/solver_tests/lp_model_solve_parameters_tests.h"
#include "ortools/math_opt/solver_tests/lp_parameter_tests.h"
#include "ortools/math_opt/solver_tests/lp_tests.h"
#include "ortools/math_opt/solver_tests/mip_tests.h"
#include "ortools/math_opt/solver_tests/status_tests.h"
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

SimpleLpTestParameters CplexDefaults() {
  return SimpleLpTestParameters(
      SolverType::kCplex, SolveParameters(), /*supports_duals=*/true,
      /*supports_basis=*/false,
      /*ensures_primal_ray=*/false, /*ensures_dual_ray=*/false,
      /*disallows_infeasible_or_unbounded=*/false);
}

INSTANTIATE_TEST_SUITE_P(CplexSimpleLpTest, SimpleLpTest,
                         testing::Values(CplexDefaults()));

INSTANTIATE_TEST_SUITE_P(CplexLpModelSolveParametersTest,
                         LpModelSolveParametersTest,
                         testing::Values(LpModelSolveParametersTestParameters(
                             SolverType::kCplex,
                             /*exact_zeros=*/true,
                             /*supports_duals=*/true,
                             /*supports_primal_only_warm_starts=*/false)));

INSTANTIATE_TEST_SUITE_P(
    CplexLpParameterTest, LpParameterTest,
    testing::Values(LpParameterTestParams(
        SolverType::kCplex,
        /*supports_simplex=*/true,
        /*supports_barrier=*/true,
        /*supports_first_order=*/false,
        /*supports_random_seed=*/true,
        /*supports_presolve=*/true,
        /*supports_cutoff=*/false,
        /*supports_objective_limit=*/CplexSupportsObjectiveLimit(),
        /*supports_best_bound_limit=*/false,
        /*reports_limits=*/false)));

StatusTestParameters StatusDefault() {
  return StatusTestParameters(SolverType::kCplex,
                              SolveParameters{.cuts = Emphasis::kOff},
                              /*disallow_primal_or_dual_infeasible=*/false,
                              /*supports_iteration_limit=*/true,
                              /*use_integer_variables=*/true,
                              /*supports_node_limit=*/true,
                              /*support_interrupter=*/true,
                              /*supports_one_thread=*/true);
}

INSTANTIATE_TEST_SUITE_P(CplexStatusTest, StatusTest, Values(StatusDefault()));

INSTANTIATE_TEST_SUITE_P(CplexIncrementalLpTest, IncrementalLpTest,
                         testing::Values(SolverType::kCplex));

INSTANTIATE_TEST_SUITE_P(CplexSimpleMipTest, SimpleMipTest,
                         Values(SolverType::kCplex));

INSTANTIATE_TEST_SUITE_P(CplexIncrementalMipTest, IncrementalMipTest,
                         Values(SolverType::kCplex));

// so presolve is disabled in the tests.
INSTANTIATE_TEST_SUITE_P(
    CplexPrimalSimplexLpIncompleteSolveTest, LpIncompleteSolveTest,
    testing::Values(LpIncompleteSolveTestParams(
        SolverType::kCplex,
        /*lp_algorithm=*/LPAlgorithm::kPrimalSimplex,
        /*supports_iteration_limit=*/true, /*supports_initial_basis=*/false,
        /*supports_incremental_solve=*/true, /*supports_basis=*/false,
        /*supports_presolve=*/true, /*check_primal_objective=*/true,
        /*primal_solution_status_always_set=*/true,
        /*dual_solution_status_always_set=*/true)));
INSTANTIATE_TEST_SUITE_P(
    CplexDualSimplexLpIncompleteSolveTest, LpIncompleteSolveTest,
    testing::Values(LpIncompleteSolveTestParams(
        SolverType::kCplex,
        /*lp_algorithm=*/LPAlgorithm::kDualSimplex,
        /*supports_iteration_limit=*/true, /*supports_initial_basis=*/false,
        /*supports_incremental_solve=*/true, /*supports_basis=*/false,
        /*supports_presolve=*/true, /*check_primal_objective=*/true,
        /*primal_solution_status_always_set=*/true,
        /*dual_solution_status_always_set=*/true)));

INSTANTIATE_TEST_SUITE_P(
    CplexInvalidInputTest, InvalidInputTest,
    Values(InvalidInputTestParameters(SolverType::kCplex,
                                      /*use_integer_variables=*/true)));

SolveParameters StopBeforeOptimal() {
  return {.node_limit = 1,
          .presolve = Emphasis::kOff,
          .cuts = Emphasis::kOff,
          .heuristics = Emphasis::kOff};
}

SolveParameters CplexLargeInstanceParams() {
  return {.presolve = Emphasis::kOff,
          .cuts = Emphasis::kOff,
          .heuristics = Emphasis::kOff};
}

ParameterSupport CplexParameterSupport() {
  return {.supports_iteration_limit = true,
          .supports_node_limit = true,
          .supports_cutoff = true,
          .supports_objective_limit = CplexSupportsObjectiveLimit(),
          .supports_solution_limit_one = true,
          .supports_one_thread = true,
          .supports_n_threads = true,
          .supports_random_seed = true,
          .supports_absolute_gap_tolerance = true,
          .supports_lp_algorithm_simplex = true,
          .supports_lp_algorithm_barrier = true,
          .supports_presolve = true,
          .supports_cuts = true,
          .supports_heuristics = true,
          .supports_scaling = true};
}

SolveResultSupport CplexSolveResultSupport() {
  return {
      .termination_limit = true, .iteration_stats = true, .node_count = true};
}

// NOTE: we should also be able to use the LP tests, but many of them don't work
// for gSCIP.
INSTANTIATE_TEST_SUITE_P(
    CplexIpParameterTest, IpParameterTest,
    Values(IpParameterTestParameters{
        .name = "default",
        .solver_type = SolverType::kCplex,
        .parameter_support = CplexParameterSupport(),
        .hint_supported = false,
        .solve_result_support = CplexSolveResultSupport(),
        .presolved_regexp = R"regexp(MIP Presolve eliminated)regexp",
        .stop_before_optimal = StopBeforeOptimal()}),
    ParamName{});

INSTANTIATE_TEST_SUITE_P(CplexLargeInstanceIpParameterTest,
                         LargeInstanceIpParameterTest,
                         Values(LargeInstanceTestParams{
                             .name = "default",
                             .solver_type = SolverType::kCplex,
                             .base_parameters = CplexLargeInstanceParams(),
                             .parameter_support = CplexParameterSupport()}),
                         ParamName{});

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(TimeLimitTest);

InvalidParameterTestParams MakeCplexBadParams() {
  SolveParameters parameters;
  parameters.cplex.param_bool_values["dog"] = false;

  // TODO(b/168069105): for solver specific errors, we should collect all
  //  errors, not just the first. Then set int_param "parallel/maxnthreads" to
  //  -4 (an invalid value).
  return InvalidParameterTestParams(SolverType::kCplex, std::move(parameters),
                                    {"CPLEX Error  1028"});
}

INSTANTIATE_TEST_SUITE_P(CplexInvalidParameterTest, InvalidParameterTest,
                         Values(MakeCplexBadParams()));

INSTANTIATE_TEST_SUITE_P(CplexIpModelSolveParametersTest,
                         IpModelSolveParametersTest,
                         Values(SolverType::kCplex));

INSTANTIATE_TEST_SUITE_P(
    CplexIpMultipleSolutionsTest, IpMultipleSolutionsTest,
    Values(IpMultipleSolutionsTestParams(SolverType::kCplex, {})));

INSTANTIATE_TEST_SUITE_P(
    CplexMessageCallbackTest, MessageCallbackTest,
    Values(MessageCallbackTestParams(SolverType::kCplex,
                                     /*support_message_callback=*/true,
                                     /*support_interrupter=*/true,
                                     /*integer_variables=*/true, "Gap",
                                     {.presolve = Emphasis::kOff,
                                      .heuristics = Emphasis::kOff})));

// Cplex wrapper does not support lazy constraints at this point.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(LazyConstraintsTest);

INSTANTIATE_TEST_SUITE_P(
    CplexGenericTest, GenericTest,
    Values(GenericTestParameters(SolverType::kCplex,
                                 /*support_interrupter=*/true,
                                 /*integer_variables=*/false,
                                 /*expected_log=*/"[optimal solution found]"),
           GenericTestParameters(SolverType::kCplex,
                                 /*support_interrupter=*/true,
                                 /*integer_variables=*/true,
                                 /*expected_log=*/"[optimal solution found]")));

TEST(CplexSolverTest, InvalidCoefficient) {
  Model model;
  const Variable x = model.AddVariable("x");
  model.Maximize(x);
  model.AddLinearConstraint(1.0e123 * x <= 2.0, "broken constraint");
  EXPECT_THAT(Solve(model, SolverType::kCplex),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("will be treated as infinite by CPLEX")));
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(CallbackTest);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(InfeasibleSubsystemTest);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(MipSolutionHintTest);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(BranchPrioritiesTest);

}  // namespace
}  // namespace math_opt
}  // namespace operations_research
