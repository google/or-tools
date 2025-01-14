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

#include "ortools/math_opt/solver_tests/mip_tests.h"

#include <cmath>
#include <initializer_list>
#include <limits>
#include <ostream>

#include "absl/log/check.h"
#include "absl/status/statusor.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/logging.h"
#include "ortools/math_opt/cpp/matchers.h"
#include "ortools/math_opt/cpp/math_opt.h"

namespace operations_research {
namespace math_opt {
namespace {

using ::testing::status::IsOkAndHolds;

constexpr double kTolerance = 1e-5;
constexpr double kInf = std::numeric_limits<double>::infinity();

}  // namespace

SimpleMipTestParameters::SimpleMipTestParameters(
    SolverType solver_type, bool report_unboundness_correctly)
    : solver_type(solver_type),
      report_unboundness_correctly(report_unboundness_correctly) {}

std::ostream& operator<<(std::ostream& out,
                         const SimpleMipTestParameters& params) {
  out << "{ solver_type: " << params.solver_type
      << ", report_unboundness_correctly: "
      << (params.report_unboundness_correctly ? "true" : "false") << "}";
  return out;
}

//   max 3.0 *x + 2.0 * y + 0.1
//   s.t. 0 <= x + y <= 1.5 (c)
//        0 <= x <= 1
//             y in {0, 1, 2}
//
// Optimal solution is (0.5, 1.0), objective value 3.6
IncrementalMipTest::IncrementalMipTest()
    : model_("incremental_solve_test"),
      x_(model_.AddContinuousVariable(0.0, 1.0, "x")),
      y_(model_.AddIntegerVariable(0.0, 2.0, "y")),
      c_(model_.AddLinearConstraint(0 <= x_ + y_ <= 1.5, "c")) {
  model_.Maximize(3.0 * x_ + 2.0 * y_ + 0.1);
  solver_ = NewIncrementalSolver(&model_, TestedSolver()).value();
  const SolveResult first_solve = solver_->Solve().value();
  CHECK(first_solve.has_primal_feasible_solution());
  CHECK_LE(std::abs(first_solve.objective_value() - 3.6), kTolerance)
      << first_solve.objective_value();
}

namespace {

TEST_P(SimpleMipTest, OneVarMax) {
  Model model;
  const Variable x = model.AddVariable(0.0, 4.0, false, "x");
  model.Maximize(2.0 * x);
  ASSERT_OK_AND_ASSIGN(const SolveResult result,
                       Solve(model, GetParam().solver_type));
  ASSERT_THAT(result, IsOptimal(8.0));
  EXPECT_THAT(result.variable_values(), IsNear({{x, 4.0}}));
}

TEST_P(SimpleMipTest, OneVarMin) {
  Model model;
  const Variable x = model.AddVariable(-2.4, 4.0, false, "x");
  model.Minimize(2.0 * x);
  ASSERT_OK_AND_ASSIGN(const SolveResult result,
                       Solve(model, GetParam().solver_type));
  ASSERT_THAT(result, IsOptimal(-4.8));
  EXPECT_THAT(result.variable_values(), IsNear({{x, -2.4}}));
}

TEST_P(SimpleMipTest, OneIntegerVar) {
  Model model;
  const Variable x = model.AddVariable(0.0, 4.5, true, "x");
  model.Maximize(2.0 * x);
  ASSERT_OK_AND_ASSIGN(const SolveResult result,
                       Solve(model, GetParam().solver_type));
  ASSERT_THAT(result, IsOptimal(8.0));
  EXPECT_THAT(result.variable_values(), IsNear({{x, 4.0}}));
}

TEST_P(SimpleMipTest, SimpleLinearConstraint) {
  Model model;
  const Variable x = model.AddBinaryVariable("x");
  const Variable y = model.AddBinaryVariable("y");
  model.Maximize(2.0 * x + y);
  model.AddLinearConstraint(0.0 <= x + y <= 1.5, "c");
  ASSERT_OK_AND_ASSIGN(const SolveResult result,
                       Solve(model, GetParam().solver_type));
  ASSERT_THAT(result, IsOptimal(2.0));
  EXPECT_THAT(result.variable_values(), IsNear({{x, 1}, {y, 0}}));
}

TEST_P(SimpleMipTest, Unbounded) {
  Model model;
  const Variable x = model.AddVariable(0.0, kInf, true, "x");
  model.Maximize(2.0 * x);
  ASSERT_OK_AND_ASSIGN(const SolveResult result,
                       Solve(model, GetParam().solver_type));
  if (GetParam().report_unboundness_correctly) {
    ASSERT_THAT(result, TerminatesWithOneOf(
                            {TerminationReason::kUnbounded,
                             TerminationReason::kInfeasibleOrUnbounded}));
  } else {
    ASSERT_THAT(result, TerminatesWith(TerminationReason::kOtherError));
  }
}

TEST_P(SimpleMipTest, Infeasible) {
  Model model;
  const Variable x = model.AddVariable(0.0, 3.0, true, "x");
  model.Maximize(2.0 * x);
  model.AddLinearConstraint(x >= 4.0);
  ASSERT_OK_AND_ASSIGN(const SolveResult result,
                       Solve(model, GetParam().solver_type));
  ASSERT_THAT(result, TerminatesWith(TerminationReason::kInfeasible));
}

TEST_P(SimpleMipTest, FractionalBoundsContainNoInteger) {
  if (GetParam().solver_type == SolverType::kGurobi) {
    // TODO(b/272298816): Gurobi bindings are broken here.
    GTEST_SKIP() << "TODO(b/272298816): Gurobi bindings are broken here.";
  }
  Model model;
  const Variable x = model.AddIntegerVariable(0.5, 0.6, "x");
  model.Maximize(x);
  EXPECT_THAT(Solve(model, GetParam().solver_type),
              IsOkAndHolds(TerminatesWith(TerminationReason::kInfeasible)));
}

TEST_P(IncrementalMipTest, EmptyUpdate) {
  ASSERT_THAT(solver_->Update(), IsOkAndHolds(DidUpdate()));
  ASSERT_OK_AND_ASSIGN(const SolveResult result, solver_->SolveWithoutUpdate());
  ASSERT_THAT(result, IsOptimal(3.6));
  EXPECT_THAT(result.variable_values(), IsNear({{x_, 0.5}, {y_, 1.0}}));
}

TEST_P(IncrementalMipTest, MakeContinuous) {
  model_.set_continuous(y_);
  ASSERT_THAT(solver_->Update(), IsOkAndHolds(DidUpdate()));
  ASSERT_OK_AND_ASSIGN(const SolveResult result, solver_->SolveWithoutUpdate());
  ASSERT_THAT(result, IsOptimal(4.1));
  EXPECT_THAT(result.variable_values(), IsNear({{x_, 1.0}, {y_, 0.5}}));
}

// TODO(b/202494808): Enable this test once this bug is resolved. Today Gurobi
// and Scip both fail in that case. See the bug for details why.
TEST_P(IncrementalMipTest, DISABLED_MakeContinuousWithNonIntegralBounds) {
  // With Gurobi we can only have one solver at a time.
  solver_.reset();

  Model model("bounds");
  const Variable x = model.AddIntegerVariable(0.5, 1.5, "x");
  model.Maximize(x);

  ASSERT_OK_AND_ASSIGN(const auto solver,
                       NewIncrementalSolver(&model, TestedSolver()));
  ASSERT_THAT(solver->Solve(), IsOkAndHolds(IsOptimal(1.0)));

  // Switching to continuous should use the fractional bound. For solvers that
  // mandates integral bounds for integer variables this may require updating
  // the bound to its actual fractional value.
  model.set_continuous(x);
  ASSERT_THAT(solver->Update(), IsOkAndHolds(DidUpdate()));
  ASSERT_THAT(solver->SolveWithoutUpdate(), IsOkAndHolds(IsOptimal(1.5)));

  model.Minimize(x);
  ASSERT_THAT(solver->Update(), IsOkAndHolds(DidUpdate()));
  ASSERT_THAT(solver->SolveWithoutUpdate(), IsOkAndHolds(IsOptimal(0.5)));
}

TEST_P(IncrementalMipTest, MakeIntegralWithNonIntegralBounds) {
  // With Gurobi we can only have one solver at a time.
  solver_.reset();

  Model model("bounds");
  const Variable x = model.AddContinuousVariable(0.5, 1.5, "x");
  model.Maximize(x);

  ASSERT_OK_AND_ASSIGN(const auto solver,
                       NewIncrementalSolver(&model, TestedSolver()));
  ASSERT_THAT(solver->Solve(), IsOkAndHolds(IsOptimal(1.5)));

  model.set_integer(x);
  ASSERT_THAT(solver->Update(), IsOkAndHolds(DidUpdate()));
  ASSERT_THAT(solver->SolveWithoutUpdate(), IsOkAndHolds(IsOptimal(1.0)));

  model.Minimize(x);
  ASSERT_THAT(solver->Update(), IsOkAndHolds(DidUpdate()));
  ASSERT_THAT(solver->SolveWithoutUpdate(), IsOkAndHolds(IsOptimal(1.0)));
}

TEST_P(IncrementalMipTest, MakeInteger) {
  model_.set_integer(x_);
  ASSERT_THAT(solver_->Update(), IsOkAndHolds(DidUpdate()));
  ASSERT_OK_AND_ASSIGN(const SolveResult result, solver_->SolveWithoutUpdate());
  ASSERT_THAT(result, IsOptimal(3.1));
  EXPECT_THAT(result.variable_values(), IsNear({{x_, 1.0}, {y_, 0.0}}));
}

TEST_P(IncrementalMipTest, ObjDir) {
  model_.set_minimize();
  ASSERT_THAT(solver_->Update(), IsOkAndHolds(DidUpdate()));
  ASSERT_OK_AND_ASSIGN(const SolveResult result, solver_->SolveWithoutUpdate());
  ASSERT_THAT(result, IsOptimal(0.1));
  EXPECT_THAT(result.variable_values(), IsNear({{x_, 0.0}, {y_, 0.0}}));
}

TEST_P(IncrementalMipTest, ObjOffset) {
  model_.set_objective_offset(0.2);
  ASSERT_THAT(solver_->Update(), IsOkAndHolds(DidUpdate()));
  ASSERT_OK_AND_ASSIGN(const SolveResult result, solver_->SolveWithoutUpdate());
  ASSERT_THAT(result, IsOptimal(3.7));
  EXPECT_THAT(result.variable_values(), IsNear({{x_, 0.5}, {y_, 1.0}}));
}

TEST_P(IncrementalMipTest, LinearObjCoef) {
  model_.set_objective_coefficient(x_, 5.0);
  ASSERT_THAT(solver_->Update(), IsOkAndHolds(DidUpdate()));
  ASSERT_OK_AND_ASSIGN(const SolveResult result, solver_->SolveWithoutUpdate());
  ASSERT_THAT(result, IsOptimal(5.1));
  EXPECT_THAT(result.variable_values(), IsNear({{x_, 1.0}, {y_, 0.0}}));
}

TEST_P(IncrementalMipTest, VariableLb) {
  model_.set_lower_bound(x_, 0.75);
  ASSERT_THAT(solver_->Update(), IsOkAndHolds(DidUpdate()));
  ASSERT_OK_AND_ASSIGN(const SolveResult result, solver_->SolveWithoutUpdate());
  ASSERT_THAT(result, IsOptimal(3.1));
  EXPECT_THAT(result.variable_values(), IsNear({{x_, 1.0}, {y_, 0.0}}));
}

TEST_P(IncrementalMipTest, VariableUb) {
  model_.set_upper_bound(x_, 2.0);
  ASSERT_THAT(solver_->Update(), IsOkAndHolds(DidUpdate()));
  ASSERT_OK_AND_ASSIGN(const SolveResult result, solver_->SolveWithoutUpdate());
  ASSERT_THAT(result, IsOptimal(4.6));
  EXPECT_THAT(result.variable_values(), IsNear({{x_, 1.5}, {y_, 0.0}}));
}

TEST_P(IncrementalMipTest, LinearConstraintLb) {
  model_.set_lower_bound(c_, 1.0);
  ASSERT_THAT(solver_->Update(), IsOkAndHolds(DidUpdate()));
  ASSERT_OK_AND_ASSIGN(const SolveResult result, solver_->SolveWithoutUpdate());
  ASSERT_THAT(result, IsOptimal(3.6));
  EXPECT_THAT(result.variable_values(), IsNear({{x_, 0.5}, {y_, 1.0}}));
  // For this change, feasibility is preserved, so the solver should do no extra
  // work (SCIP enumerates one node, though).
  if (TestedSolver() != SolverType::kGscip) {
    EXPECT_EQ(result.solve_stats.node_count, 0);
  }
  EXPECT_EQ(result.solve_stats.simplex_iterations, 0);
  EXPECT_EQ(result.solve_stats.barrier_iterations, 0);
}

TEST_P(IncrementalMipTest, LinearConstraintUb) {
  model_.set_upper_bound(c_, 1.0);
  ASSERT_THAT(solver_->Update(), IsOkAndHolds(DidUpdate()));
  ASSERT_OK_AND_ASSIGN(const SolveResult result, solver_->SolveWithoutUpdate());
  ASSERT_THAT(result, IsOptimal(3.1));
  EXPECT_THAT(result.variable_values(), IsNear({{x_, 1.0}, {y_, 0.0}}));
}

TEST_P(IncrementalMipTest, LinearConstraintCoefficient) {
  model_.set_coefficient(c_, x_, 0.5);
  ASSERT_THAT(solver_->Update(), IsOkAndHolds(DidUpdate()));
  ASSERT_OK_AND_ASSIGN(const SolveResult result, solver_->SolveWithoutUpdate());
  ASSERT_THAT(result, IsOptimal(5.1));
  EXPECT_THAT(result.variable_values(), IsNear({{x_, 1.0}, {y_, 1.0}}));
}

TEST_P(IncrementalMipTest, AddVariable) {
  Variable z = model_.AddVariable(0.0, 1.0, true, "z");
  model_.set_objective_coefficient(z, 10.0);
  model_.set_coefficient(c_, z, 1.0);
  ASSERT_THAT(solver_->Update(), IsOkAndHolds(DidUpdate()));
  ASSERT_OK_AND_ASSIGN(const SolveResult result, solver_->SolveWithoutUpdate());
  ASSERT_THAT(result, IsOptimal(11.6));
  EXPECT_THAT(result.variable_values(),
              IsNear({{x_, 0.5}, {y_, 0.0}, {z, 1.0}}));
}

TEST_P(IncrementalMipTest, AddLinearConstraint) {
  model_.AddLinearConstraint(0.0 <= x_ + 2.0 * y_ <= 2.0, "d");
  ASSERT_THAT(solver_->Update(), IsOkAndHolds(DidUpdate()));
  ASSERT_OK_AND_ASSIGN(const SolveResult result, solver_->SolveWithoutUpdate());
  ASSERT_THAT(result, IsOptimal(3.1));
  EXPECT_THAT(result.variable_values(), IsNear({{x_, 1.0}, {y_, 0.0}}));
}

TEST_P(IncrementalMipTest, DeleteVariable) {
  model_.DeleteVariable(x_);
  ASSERT_THAT(solver_->Update(), IsOkAndHolds(DidUpdate()));
  ASSERT_OK_AND_ASSIGN(const SolveResult result, solver_->SolveWithoutUpdate());
  ASSERT_THAT(result, IsOptimal(2.1));
  EXPECT_THAT(result.variable_values(), IsNear({{y_, 1.0}}));
}

TEST_P(IncrementalMipTest, DeleteLinearConstraint) {
  model_.DeleteLinearConstraint(c_);
  ASSERT_THAT(solver_->Update(), IsOkAndHolds(DidUpdate()));
  ASSERT_OK_AND_ASSIGN(const SolveResult result, solver_->SolveWithoutUpdate());
  ASSERT_THAT(result, IsOptimal(7.1));
  EXPECT_THAT(result.variable_values(), IsNear({{x_, 1.0}, {y_, 2.0}}));
}

TEST_P(IncrementalMipTest, ChangeBoundsWithTemporaryInversion) {
  model_.set_lower_bound(x_, 3.0);
  // At this point x_ lower bound is 3.0 and upper bound is 1.0.
  ASSERT_THAT(solver_->Update(), IsOkAndHolds(DidUpdate()));

  model_.set_upper_bound(x_, 5.0);
  ASSERT_THAT(solver_->Update(), IsOkAndHolds(DidUpdate()));
  // At this point x_ upper bound is 5.0 and so is greater than the new lower
  // bound.

  // To make the problem feasible we update the bound of the constraint that
  // contains x_; we take this opportunity to also test inverting bounds of
  // constraints.
  model_.set_lower_bound(c_, 4.0);
  // At this point c_ lower bound is 4.0 and upper bound is 1.5.
  ASSERT_THAT(solver_->Update(), IsOkAndHolds(DidUpdate()));

  // We restore valid bounds by setting c_ upper bound to 5.5.
  model_.set_upper_bound(c_, 5.5);
  ASSERT_THAT(solver_->Update(), IsOkAndHolds(DidUpdate()));

  EXPECT_THAT(solver_->SolveWithoutUpdate(),
              IsOkAndHolds(IsOptimal(3 * 4.5 + 2 * 1 + 0.1)));
}

}  // namespace
}  // namespace math_opt
}  // namespace operations_research
