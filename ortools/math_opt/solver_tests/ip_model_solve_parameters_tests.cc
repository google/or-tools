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

#include "ortools/math_opt/solver_tests/ip_model_solve_parameters_tests.h"

#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/cpp/matchers.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/math_opt/parameters.pb.h"
#include "ortools/port/proto_utils.h"

namespace operations_research {
namespace math_opt {
namespace {

using ::testing::DoubleNear;
using ::testing::status::IsOkAndHolds;

std::string PrintParams(const std::optional<SolveParameters>& params) {
  return params.has_value() ? ProtobufShortDebugString(params->Proto())
                            : "nullopt";
}

}  // namespace

std::ostream& operator<<(std::ostream& out,
                         const SolutionHintTestParams& params) {
  out << "{ solver_type: " << params.solver_type
      << " single_hint_params: " << PrintParams(params.single_hint_params)
      << " two_hint_params: " << PrintParams(params.two_hint_params)
      << ", hint_message_regex: " << params.hint_accepted_message_regex << " }";
  return out;
}

std::ostream& operator<<(std::ostream& out,
                         const BranchPrioritiesTestParams& params) {
  out << "{ solver_type: " << params.solver_type << " solve_params: "
      << ProtobufShortDebugString(params.solve_params.Proto()) << " }";
  return out;
}

std::ostream& operator<<(std::ostream& out,
                         const LazyConstraintsTestParams& params) {
  out << "{ solver_type: " << params.solver_type << " solve_params: "
      << ProtobufShortDebugString(params.nerfed_solve_params.Proto()) << " }";
  return out;
}

namespace {

TEST_P(IpModelSolveParametersTest, SolutionFilterSkipZeros) {
  Model model;
  const Variable x = model.AddBinaryVariable("x");
  const Variable y = model.AddBinaryVariable("y");
  model.Maximize(2.0 * x + y);
  model.AddLinearConstraint(0.0 <= x + y <= 1.5, "c");
  ASSERT_OK_AND_ASSIGN(
      const SolveResult result,
      Solve(model, TestedSolver(),
            {.model_parameters = {
                 .variable_values_filter = {.skip_zero_values = true}}}));
  ASSERT_THAT(result, IsOptimal(2.0));
  EXPECT_THAT(result.variable_values(), IsNear({{x, 1.0}}));
}

TEST_P(IpModelSolveParametersTest, SolutionFilterByKey) {
  Model model;
  const Variable x = model.AddBinaryVariable("x");
  const Variable y = model.AddBinaryVariable("y");
  model.Maximize(2.0 * x + y);
  model.AddLinearConstraint(0.0 <= x + y <= 1.5, "c");

  ASSERT_OK_AND_ASSIGN(
      const SolveResult result,
      Solve(model, TestedSolver(),
            {.model_parameters =
                 ModelSolveParameters::OnlySomePrimalVariables({y})}));
  ASSERT_THAT(result, IsOptimal(2.0));
  EXPECT_THAT(result.variable_values(), IsNear({{y, 0.0}}));
}

TEST_P(MipSolutionHintTest, SingleHintTest) {
  if (!SingleHintParams().has_value()) {
    GTEST_SKIP() << "Single hints not supported. Ignoring this test.";
  }

  ModelSolveParameters model_parameters;

  Model model("Solution Hint MIP");

  Variable x1 = model.AddBinaryVariable("x1");
  Variable x2 = model.AddBinaryVariable("x2");
  model.AddLinearConstraint(x1 + x2 == 1);

  Variable x3 = model.AddBinaryVariable("x3");
  Variable x4 = model.AddBinaryVariable("x4");
  model.AddLinearConstraint(x3 + x4 == 1);

  model.Maximize(x1 + 3 * x2 + 2 * x3 + 4 * x4);

  // Only feasible completion of this hint has (x1, x2, x3, x4) = (1, 0, 1, 0)
  // with objective value equal to 3.
  ModelSolveParameters::SolutionHint hint;
  hint.variable_values = {{x1, 1.0}, {x4, 0.0}};
  model_parameters.solution_hints.emplace_back(hint);

  std::ostringstream log;
  const SolveArguments args = {
      // SingleHintParams() is expected to set (possibly solver-specific)
      // parameters to ensure the optimization stops after the first feasible
      // solution (e.g.  solution limit of 1) and that this solution is the one
      // associated to the hint and not the optimal solution with objective
      // value 7.
      .parameters = *SingleHintParams(),
      .model_parameters = model_parameters,
      .message_callback = PrinterMessageCallback(log)};
  ASSERT_OK_AND_ASSIGN(const SolveResult result,
                       Solve(model, TestedSolver(), args));
  EXPECT_THAT(result,
              TerminatesWithReasonFeasible(Limit::kSolution,
                                           /*allow_limit_undetermined=*/true));
  EXPECT_THAT(
      result,
      HasSolution(PrimalSolution{
          .variable_values = {{x1, 1.0}, {x2, 0.0}, {x3, 1.0}, {x4, 0.0}},
          .objective_value = 3.0,
          .feasibility_status = SolutionStatus::kFeasible}));
  EXPECT_THAT(log.str(), testing::ContainsRegex(HintAcceptedMessageRegex()));
}

TEST_P(MipSolutionHintTest, TwoHintTest) {
  if (!TwoHintParams().has_value()) {
    GTEST_SKIP() << "Multiple hints not supported. Ignoring this test.";
  }
  if (GetParam().solver_type == SolverType::kXpress) {
    // Xpress has no configuration options to "just complete" a partial
    // solution hint. For an incomplete solution it will always run simple
    // heuristics to find a solution. The effort of this heuristic can be
    // controlled via the USERSOLHEURISTIC control, but both values
    // 0 (off) and 1 (light) make the test fail: with off no heuristic
    // is applied on the provided solution and hence the expected solutions
    // are not found. With light the heuristic finds the optimal solution
    // from the solution hint.
    GTEST_SKIP() << "Xpress cannot be forced to only complete a solution.";
  }

  ModelSolveParameters model_parameters;

  Model model("Solution Hint MIP");

  Variable x1 = model.AddBinaryVariable("x1");
  Variable x2 = model.AddBinaryVariable("x2");
  model.AddLinearConstraint(x1 + x2 == 1);

  Variable x3 = model.AddBinaryVariable("x3");
  Variable x4 = model.AddBinaryVariable("x4");
  model.AddLinearConstraint(x3 + x4 == 1);

  Variable x5 = model.AddBinaryVariable("x5");
  Variable x6 = model.AddBinaryVariable("x6");
  model.AddLinearConstraint(x5 + x6 == 1);

  model.Maximize(x1 + 3 * x2 + 2 * x3 + 4 * x4 + x5 + 2 * x6);

  // Only feasible completion of this hint has
  // (x1, x2, x3, x4, x5, x6) = (1, 0, 1, 0, 1, 0)
  // with objective value equal to 4.
  ModelSolveParameters::SolutionHint first_hint;
  first_hint.variable_values = {{x1, 1.0}, {x4, 0.0}, {x5, 1.0}};
  model_parameters.solution_hints.emplace_back(first_hint);
  const Solution first_solution{
      .primal_solution = {{.variable_values = {{x1, 1.0},
                                               {x2, 0.0},
                                               {x3, 1.0},
                                               {x4, 0.0},
                                               {x5, 1.0},
                                               {x6, 0.0}},
                           .objective_value = 4,
                           .feasibility_status = SolutionStatus::kFeasible}}};

  // Only feasible completion of this hint has
  // (x1, x2, x3, x4, x5, x6) = (1, 0, 1, 0, 0, 1)
  // with objective value equal to 5.
  ModelSolveParameters::SolutionHint second_hint;
  second_hint.variable_values = {{x1, 1.0}, {x4, 0.0}, {x6, 1.0}};
  model_parameters.solution_hints.emplace_back(second_hint);
  const Solution second_solution{
      .primal_solution =
          PrimalSolution{.variable_values = {{x1, 1.0},
                                             {x2, 0.0},
                                             {x3, 1.0},
                                             {x4, 0.0},
                                             {x5, 0.0},
                                             {x6, 1.0}},
                         .objective_value = 5,
                         .feasibility_status = SolutionStatus::kFeasible}};
  std::ostringstream log;
  const SolveArguments args = {
      // TwoHintParams() is expected to set (possibly solver-specific)
      // parameters to ensure the optimization stops after the second feasible
      // solution (e.g.  solution limit of 2) and that these solutions are the
      // ones associated to the hints and not the optimal solution with
      // objective value 9.
      .parameters = *TwoHintParams(),
      .model_parameters = model_parameters,
      .message_callback = PrinterMessageCallback(log)};
  ASSERT_OK_AND_ASSIGN(const SolveResult result,
                       Solve(model, TestedSolver(), args));
  EXPECT_THAT(result, TerminatesWithReasonFeasible(Limit::kSolution));
  // Solutions should be objective-ordered and not hint-ordered.
  // Gurobi does not guarantee that all solution pool entries are feasible, so
  // we also accept undetermined feasibility status.
  EXPECT_THAT(
      result.solutions,
      ElementsAre(IsNear(second_solution),
                  IsNear(first_solution, {.allow_undetermined = true})));
  EXPECT_THAT(log.str(), testing::ContainsRegex(HintAcceptedMessageRegex()));
}

TEST_P(BranchPrioritiesTest, PrioritiesAreSetProperly) {
  // We solve min{ |x| : x in {-2, -1, 1}} = 1 through the following simple
  // MIP formulation.
  Model model("Solution Hint MIP");
  Variable x = model.AddContinuousVariable(-3.0, 1.0, "x");
  Variable y = model.AddContinuousVariable(0.0, 3.0, "y");
  Variable zminus2 = model.AddBinaryVariable("zminus2");
  Variable zminus1 = model.AddBinaryVariable("zminus1");
  Variable zplus1 = model.AddBinaryVariable("zplus1");
  model.AddLinearConstraint(zminus2 + zminus1 + zplus1 == 1);
  model.AddLinearConstraint(-2 * zminus2 - zminus1 + zplus1 == x);
  model.AddLinearConstraint(x <= y);
  model.AddLinearConstraint(-x <= y);
  model.Minimize(y);
  // The optimal value of the LP relaxation of this formulation is zero and (in
  // the absence of cuts and preprocessing) the best bound will remain at zero
  // after branching on variables zminus3 or zminus2. The problem can be solved
  // by branching on zminus3 and zminus2. However, it can also be solved by
  // just branching on zplus1. Hence, adding higher branch priority to zplus1
  // should result in fewer branch-and-bound nodes than adding higher priorities
  // to zminus3 and zminus2.

  // SolveParams() is expected to set (possibly solver-specific) parameters
  // to ensure the solver behaves as close as possible to a pure
  // branch-and-bound solver (e.g. turn presolve, heuristics and cuts off).
  // Major deviations from this could cause the test to fail.
  const SolveParameters solve_params = SolveParams();

  // We first solve giving higher branch priority to zplus1
  // Note: we only store the node count instead of testing its value as this
  // could be brittle (solvers often differ by one unit on the meaning of node
  // count).
  ModelSolveParameters model_parameters;
  model_parameters.branching_priorities = {
      {zminus2, 1}, {zminus1, 1}, {zplus1, 2}};
  const SolveArguments good_args = {.parameters = solve_params,
                                    .model_parameters = model_parameters};
  ASSERT_OK_AND_ASSIGN(const SolveResult good_result,
                       Solve(model, TestedSolver(), good_args));
  ASSERT_THAT(good_result, IsOptimal());
  const int good_node_count = good_result.solve_stats.node_count;

  // We then give higher priorities to zminus2 and zminus1 and check it takes
  // more nodes to solve.
  model_parameters.branching_priorities = {
      {zminus2, 2}, {zminus1, 2}, {zplus1, 1}};
  const SolveArguments bad_args = {.parameters = solve_params,
                                   .model_parameters = model_parameters};
  ASSERT_OK_AND_ASSIGN(const SolveResult bad_result,
                       Solve(model, TestedSolver(), bad_args));
  ASSERT_THAT(bad_result, IsOptimal());
  EXPECT_GT(bad_result.solve_stats.node_count, good_node_count);
}

// See PrioritiesAreSetProperly for details on the model and solve parameters.
TEST_P(BranchPrioritiesTest, PrioritiesClearedAfterIncrementalSolve) {
  if (GetParam().solver_type == SolverType::kXpress) {
    // This test does not work with Xpress since Xpress does not clear/reset
    // model parameters after a solve. See the comment in XpressSolver::Solve
    // in xpress_solver.cc.
    GTEST_SKIP() << "Xpress does not clear model parameters in Solve().";
  }
  Model model;
  Variable x = model.AddContinuousVariable(-3.0, 1.0, "x");
  Variable y = model.AddContinuousVariable(0.0, 3.0, "y");
  Variable zminus2 = model.AddBinaryVariable("zminus2");
  Variable zminus1 = model.AddBinaryVariable("zminus1");
  Variable zplus1 = model.AddBinaryVariable("zplus1");
  model.AddLinearConstraint(zminus2 + zminus1 + zplus1 == 1);
  model.AddLinearConstraint(-2 * zminus2 - zminus1 + zplus1 == x);
  model.AddLinearConstraint(x <= y);
  model.AddLinearConstraint(-x <= y);
  model.Minimize(y);

  // First, we do a static solve with "good" branching priorities as a baseline.
  ASSERT_OK_AND_ASSIGN(
      const int node_count_good_priorities, ([&]() -> absl::StatusOr<int> {
        const SolveArguments args = {
            .parameters = SolveParams(),
            .model_parameters = {.branching_priorities = {
                                     {zminus1, 1}, {zminus2, 1}, {zplus1, 3}}}};
        ASSIGN_OR_RETURN(const SolveResult result,
                         Solve(model, TestedSolver(), args));
        RETURN_IF_ERROR(result.termination.EnsureIsOptimal());
        return result.solve_stats.node_count;
      }()));

  // Next, we solve incrementally with "good" branching priorities, but a very
  // tight node limit. We expect the solver to load the priorities, but not to
  // make any progress towards the optimal solution.
  ASSERT_OK_AND_ASSIGN(const auto solver,
                       NewIncrementalSolver(&model, TestedSolver()));
  {
    SolveParameters params = SolveParams();
    params.node_limit = 0;
    const SolveArguments args = {
        .parameters = params,
        .model_parameters = {
            .branching_priorities = {{zminus1, 1}, {zminus2, 1}, {zplus1, 3}}}};
    ASSERT_OK_AND_ASSIGN(const SolveResult good_result, solver->Solve(args));
    ASSERT_THAT(good_result, TerminatesWithLimit(Limit::kNode));
  }

  // Finally, using the same incremental solver we solve with partial branching
  // priorities, and record the node count. If the previously set branching
  // priorities are overwritten, these are "good" priorities (zplus1 will be
  // highest priority); if they were cleared previously, then these are "bad"
  // priorities (zplus has the lowest priority with a default value of 0).
  ASSERT_OK_AND_ASSIGN(
      const int node_count_no_priorities, ([&]() -> absl::StatusOr<int> {
        const SolveArguments args{
            .parameters = SolveParams(),
            .model_parameters = {
                .branching_priorities = {{zminus1, 2}, {zminus2, 2}}}};
        ASSIGN_OR_RETURN(const SolveResult result, solver->Solve(args));
        RETURN_IF_ERROR(result.termination.EnsureIsOptimal());
        return result.solve_stats.node_count;
      }()));

  // If priorities were properly cleared for the second incremental solve, it
  // should take more nodes to solve than with the "good" branching priorities.
  EXPECT_GT(node_count_no_priorities, node_count_good_priorities);
}

// The problem is:
// min  x
// s.t. x >= 1      (c)
//      0 <= x <= 2
//      x integer
//
// We mark (c) as a lazy constraint, solve, and verify that the optimal solution
// returned respects it (i.e., x^* = 1).
TEST_P(LazyConstraintsTest, LazyConstraintsImposedOnModel) {
  Model model;
  Variable x = model.AddIntegerVariable(0, 2, "x");
  const LinearConstraint c = model.AddLinearConstraint(x >= 1);
  model.Minimize(x);

  // We intentionally do not use NerfedSolveParams() here: Gurobi produces the
  // wrong solution with presolve disabled (!), and we only want to test that
  // the lazy constraint is respected.
  SolveArguments args = {.model_parameters = {.lazy_linear_constraints = {c}}};
  args.parameters.enable_output = true;
  EXPECT_THAT(Solve(model, TestedSolver(), args),
              IsOkAndHolds(IsOptimalWithSolution(1, {{x, 1}})));
}

// The problem is:
// min  y
// s.t. y >= x          (c)
//      y >= -x         (d)
//      -1 <= x, y <= 1
//      x, y integer
//
// With a node limit of 0 and solver parameters set to disable presolve, we
// expect a dual bound equal to the LP relaxation bound (which is 0). However,
// if c and d are lazy constraints, they are not included in the LP relaxation,
// and the bound instead is -1.
TEST_P(LazyConstraintsTest, AnnotationsAreSetProperly) {
  Model model;
  Variable x = model.AddIntegerVariable(-1, 1, "x");
  Variable y = model.AddIntegerVariable(-1, 1, "y");
  const LinearConstraint c = model.AddLinearConstraint(y >= x);
  const LinearConstraint d = model.AddLinearConstraint(y >= -x);
  model.Minimize(y);

  SolveArguments args = {
      .parameters = NerfedSolveParams(),
      .model_parameters = {.lazy_linear_constraints = {c, d}}};
  args.parameters.node_limit = 0;
  ASSERT_OK_AND_ASSIGN(const SolveResult result,
                       Solve(model, TestedSolver(), args));
  ASSERT_THAT(result, TerminatesWithReasonNoSolutionFound(Limit::kNode));
  EXPECT_THAT(result.best_objective_bound(), DoubleNear(-1, 1.0e-5));
}

// Same setting as in AnnotationsAreSetProperly above, but we solve twice with
// an incremental solver: first with the lazy constraint annotations, and then
// without. If the annotations are cleared after the first, then we expect the
// second to solve the entire LP (including c and d), giving a dual bound of 0.
TEST_P(LazyConstraintsTest, AnnotationsAreClearedAfterSolve) {
  if (GetParam().solver_type == SolverType::kXpress) {
    // For the AnnotationsAreSetProperly we set STOP_AFTER_LP=1 which stops
    // Xpress right after the relaxation. Since the same parameters are
    // also used for the test here, this settings kills the test.
    GTEST_SKIP() << "Xpress stops too early with shared parameter settings.";
  }
  Model model;
  Variable x = model.AddIntegerVariable(-1, 1, "x");
  Variable y = model.AddIntegerVariable(-1, 1, "y");
  const LinearConstraint c = model.AddLinearConstraint(y >= x);
  const LinearConstraint d = model.AddLinearConstraint(y >= -x);
  model.Minimize(y);
  ASSERT_OK_AND_ASSIGN(const auto solver,
                       NewIncrementalSolver(&model, TestedSolver()));

  SolveArguments args = {
      .parameters = NerfedSolveParams(),
      .model_parameters = {.lazy_linear_constraints = {c, d}}};
  args.parameters.node_limit = 0;
  ASSERT_OK_AND_ASSIGN(const SolveResult bad_result, solver->Solve(args));
  ASSERT_THAT(bad_result, TerminatesWithReasonNoSolutionFound(Limit::kNode));
  ASSERT_THAT(bad_result.best_objective_bound(), DoubleNear(-1, 1.0e-5));

  args.model_parameters.lazy_linear_constraints.clear();
  ASSERT_OK_AND_ASSIGN(const SolveResult good_result, solver->Solve(args));
  ASSERT_THAT(good_result, TerminatesWithReasonNoSolutionFound(Limit::kNode));
  EXPECT_THAT(good_result.best_objective_bound(), DoubleNear(0, 1.0e-5));
}

}  // namespace
}  // namespace math_opt
}  // namespace operations_research
