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
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/core/solver.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/math_opt/model_parameters.pb.h"
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
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/pdlp/solve_log.pb.h"

namespace operations_research {
namespace math_opt {
namespace {

using ::testing::HasSubstr;
using ::testing::status::StatusIs;

StatusTestParameters StatusDefault() {
  SolveParameters parameters;
  return StatusTestParameters(SolverType::kPdlp, parameters,
                              /*disallow_primal_or_dual_infeasible=*/false,
                              /*supports_iteration_limit=*/false,
                              /*use_integer_variables=*/false,
                              /*supports_node_limit=*/false,
                              /*support_interrupter=*/true,
                              /*supports_one_thread=*/true);
}

INSTANTIATE_TEST_SUITE_P(PdlpStatusTest, StatusTest,
                         testing::Values(StatusDefault()));

INSTANTIATE_TEST_SUITE_P(PdlpSimpleLpTest, SimpleLpTest,
                         testing::Values(SimpleLpTestParameters(
                             SolverType::kPdlp, SolveParameters(),
                             /*supports_duals=*/true, /*supports_basis=*/false,
                             /*ensures_primal_ray=*/true,
                             /*ensures_dual_ray=*/true,
                             /*disallows_infeasible_or_unbounded=*/false)));

MultiObjectiveTestParameters GetPdlpMultiObjectiveTestParameters() {
  return MultiObjectiveTestParameters(
      /*solver_type=*/SolverType::kPdlp, /*parameters=*/SolveParameters(),
      /*supports_auxiliary_objectives=*/false,
      /*supports_incremental_objective_add_and_delete=*/false,
      /*supports_incremental_objective_modification=*/false,
      /*supports_integer_variables=*/false);
}

INSTANTIATE_TEST_SUITE_P(
    PdlpSimpleMultiObjectiveTest, SimpleMultiObjectiveTest,
    testing::Values(GetPdlpMultiObjectiveTestParameters()));

INSTANTIATE_TEST_SUITE_P(
    PdlpIncrementalMultiObjectiveTest, IncrementalMultiObjectiveTest,
    testing::Values(GetPdlpMultiObjectiveTestParameters()));

QpTestParameters GetPdlpQpTestParameters() {
  return QpTestParameters(SolverType::kPdlp, SolveParameters(),
                          /*qp_support=*/QpSupportType::kDiagonalQpOnly,
                          /*supports_incrementalism_not_modifying_qp=*/false,
                          /*supports_qp_incrementalism=*/false,
                          /*use_integer_variables=*/false);
}
INSTANTIATE_TEST_SUITE_P(PdlpSimpleQpTest, SimpleQpTest,
                         testing::Values(GetPdlpQpTestParameters()));
INSTANTIATE_TEST_SUITE_P(PdlpIncrementalQpTest, IncrementalQpTest,
                         testing::Values(GetPdlpQpTestParameters()));
INSTANTIATE_TEST_SUITE_P(PdlpQpDualsTest, QpDualsTest,
                         testing::Values(GetPdlpQpTestParameters()));

QcTestParameters GetPdlpQcTestParameters() {
  return QcTestParameters(SolverType::kPdlp, SolveParameters(),
                          /*supports_qc=*/false,
                          /*supports_incremental_add_and_deletes=*/false,
                          /*supports_incremental_variable_deletions=*/false,
                          /*use_integer_variables=*/false);
}
INSTANTIATE_TEST_SUITE_P(PdlpSimpleQcTest, SimpleQcTest,
                         testing::Values(GetPdlpQcTestParameters()));
INSTANTIATE_TEST_SUITE_P(PdlpIncrementalQcTest, IncrementalQcTest,
                         testing::Values(GetPdlpQcTestParameters()));
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(QcDualsTest);

SecondOrderConeTestParameters GetPdlpSecondOrderConeTestParameters() {
  return SecondOrderConeTestParameters(
      SolverType::kPdlp, SolveParameters(),
      /*supports_soc_constraints=*/false,
      /*supports_incremental_add_and_deletes=*/false);
}
INSTANTIATE_TEST_SUITE_P(
    PdlpSimpleSecondOrderConeTest, SimpleSecondOrderConeTest,
    testing::Values(GetPdlpSecondOrderConeTestParameters()));
INSTANTIATE_TEST_SUITE_P(
    PdlpIncrementalSecondOrderConeTest, IncrementalSecondOrderConeTest,
    testing::Values(GetPdlpSecondOrderConeTestParameters()));

LogicalConstraintTestParameters GetPdlpLogicalConstraintTestParameters() {
  return LogicalConstraintTestParameters(
      SolverType::kPdlp, SolveParameters(),
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
    PdlpSimpleLogicalConstraintTest, SimpleLogicalConstraintTest,
    testing::Values(GetPdlpLogicalConstraintTestParameters()));
INSTANTIATE_TEST_SUITE_P(
    PdlpIncrementalLogicalConstraintTest, IncrementalLogicalConstraintTest,
    testing::Values(GetPdlpLogicalConstraintTestParameters()));

INSTANTIATE_TEST_SUITE_P(PdlpInvalidInputTest, InvalidInputTest,
                         testing::Values(InvalidInputTestParameters(
                             SolverType::kPdlp,
                             /*use_integer_variables=*/false)));

INSTANTIATE_TEST_SUITE_P(
    PdlpLpParameterTest, LpParameterTest,
    testing::Values(LpParameterTestParams(SolverType::kPdlp,
                                          /*supports_simplex=*/false,
                                          /*supports_barrier=*/false,
                                          /*supports_first_order=*/true,
                                          /*supports_random_seed=*/false,
                                          /*supports_presolve=*/false,
                                          /*supports_cutoff=*/false,
                                          /*supports_objective_limit=*/false,
                                          /*supports_best_bound_limit=*/false,
                                          /*reports_limits=*/true)));

InvalidParameterTestParams MakeBadPdlpSpecificParams() {
  SolveParameters parameters;
  parameters.pdlp.set_major_iteration_frequency(-7);
  return InvalidParameterTestParams(
      SolverType::kPdlp, std::move(parameters),
      {"major_iteration_frequency must be positive"});
}

InvalidParameterTestParams MakeBadCommonParamsForPdlp() {
  SolveParameters parameters;
  parameters.cuts = Emphasis::kHigh;
  parameters.lp_algorithm = LPAlgorithm::kDualSimplex;
  return InvalidParameterTestParams(
      SolverType::kPdlp, std::move(parameters),
      /*expected_error_substrings=*/
      {"parameter cuts not supported for PDLP",
       "parameter lp_algorithm not supported for PDLP"});
}

INSTANTIATE_TEST_SUITE_P(PdlpInvalidParameterTest, InvalidParameterTest,
                         testing::Values(MakeBadPdlpSpecificParams(),
                                         MakeBadCommonParamsForPdlp()));

INSTANTIATE_TEST_SUITE_P(PdlpLpModelSolveParametersTest,
                         LpModelSolveParametersTest,
                         testing::Values(LpModelSolveParametersTestParameters(
                             SolverType::kPdlp, /*exact_zeros=*/false,
                             /*supports_duals=*/true,
                             /*supports_primal_only_warm_starts=*/false)));

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(IncrementalLpTest);
INSTANTIATE_TEST_SUITE_P(
    PdlpLpIncompleteSolveTest, LpIncompleteSolveTest,
    testing::Values(LpIncompleteSolveTestParams(
        SolverType::kPdlp,
        /*lp_algorithm=*/std::nullopt,
        /*supports_iteration_limit=*/true, /*supports_initial_basis=*/false,
        /*supports_incremental_solve=*/false, /*supports_basis=*/false,
        /*supports_presolve=*/false, /*check_primal_objective=*/false,
        /*primal_solution_status_always_set=*/false,
        /*dual_solution_status_always_set=*/false)));

INSTANTIATE_TEST_SUITE_P(
    PdlpGenericTest, GenericTest,
    testing::Values(GenericTestParameters(
        SolverType::kPdlp,
        /*support_interrupter=*/true,
        /*integer_variables=*/false,
        /*expected_log=*/"Termination reason: TERMINATION_REASON_OPTIMAL")));

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(TimeLimitTest);

INSTANTIATE_TEST_SUITE_P(
    PdlpMessageCallbackTest, MessageCallbackTest,
    testing::Values(MessageCallbackTestParams(
        SolverType::kPdlp,
        /*support_message_callback=*/true,
        /*support_interrupter=*/true,
        /*integer_variables=*/false,
        /*ending_substring=*/
        "Termination reason: TERMINATION_REASON_OPTIMAL")));

INSTANTIATE_TEST_SUITE_P(
    PdlpCallbackTest, CallbackTest,
    testing::Values(CallbackTestParams(SolverType::kPdlp,
                                       /*integer_variables=*/false,
                                       /*add_lazy_constraints=*/false,
                                       /*add_cuts=*/false,
                                       /*supported_events=*/{},
                                       /*all_solutions=*/std::nullopt,
                                       /*reaches_cut_callback*/ std::nullopt)));

INSTANTIATE_TEST_SUITE_P(PdlpInfeasibleSubsystemTest, InfeasibleSubsystemTest,
                         testing::Values(InfeasibleSubsystemTestParameters(
                             {.solver_type = SolverType::kPdlp})));

TEST(PdlpWarmSart, WarmStart) {
  constexpr int kNumVars = 16;
  // Build a model.
  Model model;
  std::vector<Variable> x;
  for (int k = 0; k < kNumVars; ++k) {
    x.push_back(model.AddContinuousVariable(0.0, 100.0));
  }
  model.AddLinearConstraint(Sum(x) <= 1.0);
  model.Minimize(-Sum(x));
  // Use optimal solution as warm-start.
  Solver::SolveArgs solve_args;
  auto* const optimality = solve_args.parameters.mutable_pdlp()
                               ->mutable_termination_criteria()
                               ->mutable_simple_optimality_criteria();
  optimality->set_eps_optimal_absolute(1.0e-9);
  optimality->set_eps_optimal_relative(1.0e-9);

  SolutionHintProto* const warm_start =
      solve_args.model_parameters.add_solution_hints();
  SparseDoubleVectorProto* const primal_solution =
      warm_start->mutable_variable_values();
  for (int k = 0; k < kNumVars; ++k) {
    primal_solution->add_ids(k);
    primal_solution->add_values(1.0 / kNumVars);
  }
  SparseDoubleVectorProto* const dual_solution =
      warm_start->mutable_dual_values();
  dual_solution->add_ids(0);
  dual_solution->add_values(-1.0);
  // Instantiate solver and solve.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Solver> solver,
                       Solver::New(SOLVER_TYPE_PDLP, model.ExportModel(), {}));
  ASSERT_OK_AND_ASSIGN(const SolveResultProto result,
                       solver->Solve(solve_args));
  // Check that we did 0 iterations and got an optimal status.
  EXPECT_EQ(result.termination().reason(), TERMINATION_REASON_OPTIMAL);
  EXPECT_EQ(result.solve_stats().first_order_iterations(), 0);
}

TEST(PdlpWarmSart, InvalidWarmStart) {
  Model model;
  const double kInf = std::numeric_limits<double>::infinity();
  Variable x = model.AddContinuousVariable(0.0, kInf);
  model.Minimize(x);

  Solver::SolveArgs solve_args;
  SolutionHintProto* const warm_start =
      solve_args.model_parameters.add_solution_hints();
  SparseDoubleVectorProto* const primal_solution =
      warm_start->mutable_variable_values();
  primal_solution->add_ids(0);
  // PDLP will reject initial values > 1.0e50 as overly large
  primal_solution->add_values(1.0e300);
  // Instantiate solver and solve.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Solver> solver,
                       Solver::New(SOLVER_TYPE_PDLP, model.ExportModel(), {}));
  EXPECT_THAT(solver->Solve(solve_args),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("PDLP solution hint invalid")));
}

TEST(PdlpOutput, FiniteCorrectedDual) {
  Model model;
  Variable x = model.AddContinuousVariable(0.0, 1.0);
  model.Maximize(x);
  ASSERT_OK_AND_ASSIGN(const math_opt::SolveResult result,
                       Solve(model, math_opt::SolverType::kPdlp));
  EXPECT_NEAR(result.pdlp_solver_specific_output.convergence_information()
                  .corrected_dual_objective(),
              1.0, 1e-6);
}

}  // namespace
}  // namespace math_opt
}  // namespace operations_research
