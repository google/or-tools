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

#include "ortools/math_opt/solver_tests/status_tests.h"

#include <limits>
#include <memory>
#include <optional>
#include <ostream>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/cpp/matchers.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/math_opt/io/mps_converter.h"
#include "ortools/math_opt/solver_tests/test_models.h"
#include "ortools/port/proto_utils.h"

namespace operations_research::math_opt {

using ::testing::AnyOf;
using ::testing::Field;
using ::testing::Matcher;

constexpr double kInf = std::numeric_limits<double>::infinity();

std::ostream& operator<<(std::ostream& out,
                         const StatusTestParameters& params) {
  out << "{ solver_type: " << params.solver_type
      << ", parameters: " << ProtobufShortDebugString(params.parameters.Proto())
      << ", disallow_primal_or_dual_infeasible: "
      << (params.disallow_primal_or_dual_infeasible ? "true" : "false")
      << ", supports_iteration_limit: "
      << (params.supports_iteration_limit ? "true" : "false")
      << ", use_integer_variables: "
      << (params.use_integer_variables ? "true" : "false")
      << ", supports_node_limit: "
      << (params.supports_node_limit ? "true" : "false")
      << ", support_interrupter: "
      << (params.support_interrupter ? "true" : "false")
      << ", supports_one_thread: "
      << (params.supports_one_thread ? "true" : "false") << " }";
  return out;
}

namespace {

absl::StatusOr<std::unique_ptr<Model>> LoadMiplibInstance(
    absl::string_view name) {
  ASSIGN_OR_RETURN(
      const ModelProto model_proto,
      ReadMpsFile(absl::StrCat("ortools/math_opt/solver_tests/testdata/", name,
                               ".mps")));
  return Model::FromModelProto(model_proto);
}

absl::StatusOr<std::unique_ptr<Model>> Load23588() {
  return LoadMiplibInstance("23588");
}

Matcher<ProblemStatus> PrimalStatusIs(FeasibilityStatus expected) {
  return Field("primal_status", &ProblemStatus::primal_status, expected);
}
Matcher<ProblemStatus> DualStatusIs(FeasibilityStatus expected) {
  return Field("dual_status", &ProblemStatus::dual_status, expected);
}
Matcher<ProblemStatus> StatusIsPrimalOrDualInfeasible() {
  return Field("primal_or_dual_infeasible ",
               &ProblemStatus::primal_or_dual_infeasible, testing::IsTrue());
}

TEST_P(StatusTest, EmptyModel) {
  const Model model;
  ASSERT_OK_AND_ASSIGN(const SolveResult result, SimpleSolve(model));
  EXPECT_THAT(result, IsOptimal());
  // Result validators imply primal and dual problem statuses are kFeasible.
}

TEST_P(StatusTest, PrimalAndDualInfeasible) {
  if (GetParam().use_integer_variables &&
      GetParam().solver_type == SolverType::kGlpk) {
    GTEST_SKIP()
        << "Ignoring this test as GLPK gets stuck in presolve for IP's "
           "with a primal-dual infeasible LP relaxation.";
  }

  Model model;
  const Variable x1 =
      model.AddVariable(0, kInf, GetParam().use_integer_variables, "x1");
  const Variable x2 =
      model.AddVariable(0, kInf, GetParam().use_integer_variables, "x2");

  model.Maximize(2 * x1 - x2);
  model.AddLinearConstraint(x1 - x2 <= 1, "c1");
  model.AddLinearConstraint(x1 - x2 >= 2, "c2");
  ASSERT_OK_AND_ASSIGN(const SolveResult result, SimpleSolve(model));

  // Baseline reason and status checks.
  EXPECT_THAT(result,
              TerminatesWithOneOf({TerminationReason::kInfeasible,
                                   TerminationReason::kInfeasibleOrUnbounded}));
  EXPECT_THAT(result.termination.problem_status,
              AnyOf(PrimalStatusIs(FeasibilityStatus::kInfeasible),
                    DualStatusIs(FeasibilityStatus::kInfeasible),
                    StatusIsPrimalOrDualInfeasible()));

  // More detailed reason and status checks.
  if (GetParam().disallow_primal_or_dual_infeasible) {
    // Solver may only detect the dual infeasibility so we cannot guatantee
    // TerminationReason::kInfeasible (dual infeasibility is one of cases in
    // kInfeasibleOrUnbounded go/mathopt-termination-and-statuses#inf-or-unb).
    // However, the status check can be refined.
    EXPECT_THAT(result.termination.problem_status,
                AnyOf(PrimalStatusIs(FeasibilityStatus::kInfeasible),
                      DualStatusIs(FeasibilityStatus::kInfeasible)));
  }

  // Even more detailed reason and status checks for primal simplex.
  if (GetParam().disallow_primal_or_dual_infeasible &&
      GetParam().parameters.lp_algorithm == LPAlgorithm::kPrimalSimplex) {
    EXPECT_THAT(result, TerminatesWith(TerminationReason::kInfeasible));
    // Result validators imply primal problem status is infeasible.
    EXPECT_THAT(result.termination.problem_status,
                Not(DualStatusIs(FeasibilityStatus::kFeasible)));
  }
}

TEST_P(StatusTest, PrimalFeasibleAndDualInfeasible) {
  if (GetParam().solver_type == SolverType::kCpSat) {
    GTEST_SKIP() << "Ignoring this test as CpSat bounds all variables";
  }

  Model model;
  const Variable x1 =
      model.AddVariable(0, kInf, GetParam().use_integer_variables, "x1");
  const Variable x2 =
      model.AddVariable(0, kInf, GetParam().use_integer_variables, "x2");
  model.Maximize(x1 + x2);
  // When there is a unique (up to scaling) primal ray SCIP gets stuck (possibly
  // having trouble scaling the ray to be integer?)
  if (GetParam().solver_type == SolverType::kGscip) {
    model.AddLinearConstraint(100 <= x1 - 2 * x2, "c1");
  } else {
    model.AddLinearConstraint(100 <= x1 - 2 * x2 <= 200, "c1");
  }
  ASSERT_OK_AND_ASSIGN(const SolveResult result, SimpleSolve(model));

  // Baseline reason and status checks.
  EXPECT_THAT(result,
              TerminatesWithOneOf({TerminationReason::kUnbounded,
                                   TerminationReason::kInfeasibleOrUnbounded}));
  EXPECT_THAT(result.termination.problem_status,
              Not(PrimalStatusIs(FeasibilityStatus::kInfeasible)));
  EXPECT_THAT(result.termination.problem_status,
              AnyOf(DualStatusIs(FeasibilityStatus::kInfeasible),
                    StatusIsPrimalOrDualInfeasible()));

  // More detailed reason and status checks.
  if (GetParam().disallow_primal_or_dual_infeasible) {
    // Solver may only detect the dual infeasibility so we cannot guatantee
    // TerminationReason::kInfeasible (dual infeasibility is one of cases in
    // kInfeasibleOrUnbounded go/mathopt-termination-and-statuses#inf-or-unb).
    // However, the dual status check can be refined.
    EXPECT_EQ(result.termination.problem_status.dual_status,
              FeasibilityStatus::kInfeasible);
  }

  // Even more detailed reason and status checks for pure primal simplex.
  if (GetParam().parameters.lp_algorithm == LPAlgorithm::kPrimalSimplex &&
      GetParam().parameters.presolve == Emphasis::kOff) {
    // For pure primal simplex we expect to have a primal feasible solution.
    EXPECT_THAT(result, TerminatesWith(TerminationReason::kUnbounded));
    // Result validators imply primal status is kFeasible and dual problem
    // statuse is kInfeasible.
  }
}

TEST_P(StatusTest, PrimalInfeasibleAndDualFeasible) {
  Model model;
  const Variable x1 =
      model.AddVariable(0, kInf, GetParam().use_integer_variables, "x1");
  const Variable x2 =
      model.AddVariable(0, kInf, GetParam().use_integer_variables, "x2");
  model.Minimize(x1 + x2);
  model.AddLinearConstraint(x1 + x2 <= -1, "c1");
  ASSERT_OK_AND_ASSIGN(const SolveResult result, SimpleSolve(model));

  // Baseline reason and status checks.
  EXPECT_THAT(result,
              TerminatesWithOneOf({TerminationReason::kInfeasible,
                                   TerminationReason::kInfeasibleOrUnbounded}));
  EXPECT_THAT(result.termination.problem_status,
              AnyOf(PrimalStatusIs(FeasibilityStatus::kInfeasible),
                    StatusIsPrimalOrDualInfeasible()));
  EXPECT_THAT(result.termination.problem_status,
              Not(DualStatusIs(FeasibilityStatus::kInfeasible)));

  // More detailed reason and status checks.
  if (GetParam().disallow_primal_or_dual_infeasible) {
    EXPECT_THAT(result, TerminatesWith(TerminationReason::kInfeasible));
    // Result validators imply primal status is kInfeasible.
  }

  // Even more detailed reason and status checks for pure duak simplex.
  if (GetParam().parameters.lp_algorithm == LPAlgorithm::kDualSimplex &&
      GetParam().parameters.presolve == Emphasis::kOff) {
    // For pure dual simplex we expect to have a dual feasible solution, so
    // primal infeasibility must have been detected.
    EXPECT_THAT(result, TerminatesWith(TerminationReason::kInfeasible));
    // Result validators imply primal status is kInfeasible.
    EXPECT_EQ(result.termination.problem_status.dual_status,
              FeasibilityStatus::kFeasible);
  }
}

TEST_P(StatusTest, PrimalFeasibleAndDualFeasible) {
  Model model;
  const Variable x1 =
      model.AddVariable(0, kInf, GetParam().use_integer_variables, "x1");
  const Variable x2 =
      model.AddVariable(0, kInf, GetParam().use_integer_variables, "x2");
  model.Minimize(x1 + x2);
  model.AddLinearConstraint(x1 + x2 <= 1, "c1");
  SolveParameters params = GetParam().parameters;
  ASSERT_OK_AND_ASSIGN(const SolveResult result,
                       Solve(model, TestedSolver(), {.parameters = params}));

  EXPECT_THAT(result, IsOptimal());
  // Result validators imply primal and dual problem statuses are kFeasible.
}

TEST_P(StatusTest, PrimalFeasibleAndDualFeasibleLpIncomplete) {
  if (!GetParam().supports_iteration_limit ||
      GetParam().use_integer_variables) {
    GTEST_SKIP() << "Ignoring this test as it is an LP-only test and requires "
                    "support for iteration limit.";
  }
  const int n = 10;
  const std::unique_ptr<Model> model =
      IndependentSetCompleteGraph(/*integer=*/false, n);

  SolveParameters params = GetParam().parameters;
  if (GetParam().supports_one_thread) {
    params.threads = 1;
  }
  params.iteration_limit = 2;
  ASSERT_OK_AND_ASSIGN(const SolveResult result,
                       Solve(*model, TestedSolver(), {.parameters = params}));

  // Baseline reason and status checks.
  EXPECT_THAT(result, TerminatesWithLimit(Limit::kIteration,
                                          /*allow_limit_undetermined=*/true));
  EXPECT_THAT(result.termination.problem_status,
              Not(PrimalStatusIs(FeasibilityStatus::kInfeasible)));
  EXPECT_THAT(result.termination.problem_status,
              Not(DualStatusIs(FeasibilityStatus::kInfeasible)));

  // More detailed reason and status checks for pure primal simplex.
  if (GetParam().parameters.lp_algorithm == LPAlgorithm::kPrimalSimplex &&
      GetParam().parameters.presolve == Emphasis::kOff) {
    // For pure primal simplex we shouldn't have a dual solution (or a dual
    // feasible status) on early termination, but existence of a primal solution
    // depends on the phase where the algorithm was terminated.
    EXPECT_THAT(result.termination.problem_status,
                DualStatusIs(FeasibilityStatus::kUndetermined));
  }

  // More detailed detailed reason and status checks for pure dual simplex.
  if (GetParam().parameters.lp_algorithm == LPAlgorithm::kDualSimplex &&
      GetParam().parameters.presolve == Emphasis::kOff) {
    // For pure dual simplex we shouldn't have a primal solution (or a primal
    // feasible status) on early termination, but existence of a dual solution
    // depends on the phase where the algorithm was terminated.
    ASSERT_THAT(result, TerminatesWithReasonNoSolutionFound(
                            Limit::kIteration,
                            /*allow_limit_undetermined=*/true));
    EXPECT_THAT(result.termination.problem_status,
                PrimalStatusIs(FeasibilityStatus::kUndetermined));
  }
}

TEST_P(StatusTest, InfeasibleIpWithPrimalDualFeasibleRelaxation) {
  if (!GetParam().use_integer_variables) {
    GTEST_SKIP() << "Ignoring this test as it is an IP-only test.";
  }
  Model model;
  const Variable x1 = model.AddIntegerVariable(0.5, kInf, "x1");
  const Variable x2 = model.AddIntegerVariable(0.5, kInf, "x2");
  model.Minimize(x1 + x2);
  model.AddLinearConstraint(x1 + x2 <= 1, "c1");

  ASSERT_OK_AND_ASSIGN(const SolveResult result, SimpleSolve(model));

  EXPECT_THAT(result, TerminatesWith(TerminationReason::kInfeasible));
  // Result validators imply primal problem status is kInfeasible.
  EXPECT_THAT(result.termination.problem_status,
              Not(DualStatusIs(FeasibilityStatus::kInfeasible)));
}

// Some solvers will round the variable bounds of integer variables before
// starting, which makes the LP relaxation of the above problem infeasible. In
// the second version of the above test, we make sure the LP relaxation is
// feasible with integer bounds.
TEST_P(StatusTest, InfeasibleIpWithPrimalDualFeasibleRelaxation2) {
  if (!GetParam().use_integer_variables) {
    GTEST_SKIP() << "Ignoring this test as it is an IP-only test.";
  }
  // LP relaxation has optimal solution (0.5, 1.0), while MIP is infeasible.
  Model model;
  const Variable x1 = model.AddBinaryVariable("x1");
  const Variable x2 = model.AddBinaryVariable("x2");
  model.Minimize(x1);
  model.AddLinearConstraint(x1 + x2 == 1.5, "c1");

  if (GetParam().solver_type != SolverType::kCpSat) {
    ASSERT_OK_AND_ASSIGN(const SolveResult result, SimpleSolve(model));

    EXPECT_THAT(result, TerminatesWith(TerminationReason::kInfeasible));
    // Result validators imply primal problem status is kInfeasible.
    EXPECT_THAT(result.termination.problem_status,
                Not(DualStatusIs(FeasibilityStatus::kInfeasible)));
  }
}

TEST_P(StatusTest, InfeasibleIpWithPrimalFeasibleDualInfeasibleRelaxation) {
  if (!GetParam().use_integer_variables) {
    GTEST_SKIP() << "Ignoring this test as it is an IP-only test.";
  }
  if (GetParam().solver_type == SolverType::kGlpk) {
    GTEST_SKIP()
        << "Ignoring this test as GLPK gets stuck in presolve searching "
           "for an integer point in the unbounded feasible region of the"
           "LP relaxation.";
  }
  if (GetParam().solver_type == SolverType::kCpSat) {
    GTEST_SKIP() << "Ignoring this test as CpSat as it returns MODEL_INVALID";
  }
  if (GetParam().solver_type == SolverType::kSantorini) {
    GTEST_SKIP() << "Infinite loop for santorini.";
  }

  Model model;
  const Variable x1 = model.AddIntegerVariable(1, kInf, "x1");
  const Variable x2 = model.AddIntegerVariable(1, kInf, "x2");
  model.Minimize(x1 + x2);
  model.AddLinearConstraint(2 * x2 == 2 * x1 + 1, "c1");
  ASSERT_OK_AND_ASSIGN(const SolveResult result, SimpleSolve(model));

  EXPECT_THAT(result, TerminatesWith(TerminationReason::kInfeasible));
  // Result validators imply primal problem status is kInfeasible.
  EXPECT_THAT(result.termination.problem_status,
              Not(DualStatusIs(FeasibilityStatus::kInfeasible)));
}

TEST_P(StatusTest, IncompleteIpSolve) {
  if (!GetParam().use_integer_variables || !GetParam().supports_node_limit) {
    GTEST_SKIP() << "Ignoring this test as it is an IP-only test and requires "
                    "support for node_limit.";
  }
  if (GetParam().solver_type == SolverType::kHighs) {
    GTEST_SKIP() << "Ignoring this test as Highs 1.7+ returns MODEL_INVALID";
  }
  ASSERT_OK_AND_ASSIGN(const std::unique_ptr<const Model> model, Load23588());
  SolveArguments args = {
      .parameters = {.enable_output = true, .node_limit = 1}};
  ASSERT_OK_AND_ASSIGN(const SolveResult result,
                       Solve(*model, GetParam().solver_type, args));

  EXPECT_THAT(result, TerminatesWithLimit(Limit::kNode,
                                          /*allow_limit_undetermined=*/true));
  // Result validators imply primal problem status is kFeasible.
  EXPECT_THAT(result.termination.problem_status,
              DualStatusIs(FeasibilityStatus::kFeasible));
}

TEST_P(StatusTest, IncompleteIpSolveNoSolution) {
  if (!GetParam().use_integer_variables) {
    GTEST_SKIP() << "Ignoring this test as it is an IP-only test.";
  }
  // A model where we will not prove optimality immediately.
  const std::unique_ptr<const Model> model =
      DenseIndependentSet(/*integer=*/true);
  // Set additional parameters to ensure we don't even find a feasible solution.
  SolveInterrupter interrupter;
  SolveArguments args = {.parameters = {.time_limit = absl::Microseconds(1)}};
  if (GetParam().supports_one_thread) {
    args.parameters.threads = 1;
  }
  // TODO(b/196132970): support turning off errors for a single parameter, i.e.
  // set parameter if supported.
  if (GetParam().solver_type != SolverType::kCpSat &&
      GetParam().solver_type != SolverType::kGlpk &&
      GetParam().solver_type != SolverType::kSantorini) {
    args.parameters.heuristics = Emphasis::kOff;
  }
  if (GetParam().solver_type != SolverType::kGlpk &&
      GetParam().solver_type != SolverType::kHighs &&
      GetParam().solver_type != SolverType::kSantorini) {
    args.parameters.cuts = Emphasis::kOff;
  }
  if (GetParam().solver_type != SolverType::kGlpk &&
      GetParam().solver_type != SolverType::kSantorini) {
    args.parameters.presolve = Emphasis::kOff;
  }
  if (GetParam().support_interrupter) {
    interrupter.Interrupt();
    args.interrupter = &interrupter;
  }
  ASSERT_OK_AND_ASSIGN(const SolveResult result,
                       Solve(*model, GetParam().solver_type, args));
  EXPECT_THAT(result, AnyOf(TerminatesWithReasonNoSolutionFound(
                                Limit::kInterrupted,
                                /*allow_limit_undetermined=*/true),
                            TerminatesWithReasonNoSolutionFound(
                                Limit::kTime,
                                /*allow_limit_undetermined=*/true)));
  EXPECT_THAT(result.termination.problem_status,
              PrimalStatusIs(FeasibilityStatus::kUndetermined));
  EXPECT_THAT(result.termination.problem_status,
              Not(DualStatusIs(FeasibilityStatus::kInfeasible)));
}

}  // namespace
}  // namespace operations_research::math_opt
