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

#include "ortools/math_opt/solver_tests/qc_tests.h"

#include <cmath>
#include <memory>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/cpp/matchers.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/port/proto_utils.h"

namespace operations_research::math_opt {

QcTestParameters::QcTestParameters(
    const SolverType solver_type, SolveParameters parameters,
    const bool supports_qc, const bool supports_incremental_add_and_deletes,
    const bool supports_incremental_variable_deletions,
    const bool use_integer_variables)
    : solver_type(solver_type),
      parameters(std::move(parameters)),
      supports_qc(supports_qc),
      supports_incremental_add_and_deletes(
          supports_incremental_add_and_deletes),
      supports_incremental_variable_deletions(
          supports_incremental_variable_deletions),
      use_integer_variables(use_integer_variables) {}

std::ostream& operator<<(std::ostream& out, const QcTestParameters& params) {
  out << "{ solver_type: " << params.solver_type
      << ", parameters: " << ProtobufShortDebugString(params.parameters.Proto())
      << ", supports_qc: " << (params.supports_qc ? "true" : "false")
      << ", supports_incremental_add_and_deletes: "
      << (params.supports_incremental_add_and_deletes ? "true" : "false")
      << ", supports_incremental_variable_deletions: "
      << (params.supports_incremental_variable_deletions ? "true" : "false")
      << ", use_integer_variables: "
      << (params.use_integer_variables ? "true" : "false") << " }";
  return out;
}

namespace {

using ::testing::AnyOf;
using ::testing::HasSubstr;
using ::testing::status::IsOkAndHolds;
using ::testing::status::StatusIs;

constexpr double kTolerance = 1.0e-4;
constexpr absl::string_view no_qc_support_message =
    "This test is disabled as the solver does not support quadratic "
    "constraints";

// Models the following problem:
//   min_x x + 5
//   s.t.  x^2 - x <= 1
//         -1 <= x <= 1
//
// along with, if use_integer_variables = true, integrality on x.
//
// If use_integer_variables = false, the unique optimal solution is attained at
// x = (1 - sqrt(5)) / 2 with objective value (1 - sqrt(5)) / 2 + 5. Otherwise,
// the unique optimal solution is x = 0 with objective value 5.
struct UnivariateQcProblem {
  explicit UnivariateQcProblem(bool use_integer_variables)
      : model(), x(model.AddVariable(-1.0, 1.0, use_integer_variables, "x")) {
    model.AddQuadraticConstraint(x * x - x <= 1.0);
    model.Minimize(x + 5.0);
  }

  Model model;
  const Variable x;
};

TEST_P(SimpleQcTest, CanBuildQcModel) {
  UnivariateQcProblem qc_problem(GetParam().use_integer_variables);
  if (GetParam().supports_qc) {
    EXPECT_OK(
        IncrementalSolver::New(&qc_problem.model, GetParam().solver_type, {}));
  } else {
    EXPECT_THAT(
        IncrementalSolver::New(&qc_problem.model, GetParam().solver_type, {}),
        StatusIs(AnyOf(absl::StatusCode::kInvalidArgument,
                       absl::StatusCode::kUnimplemented),
                 HasSubstr("quadratic constraints")));
  }
}

TEST_P(SimpleQcTest, SolveSimpleQc) {
  if (!GetParam().supports_qc) {
    GTEST_SKIP() << no_qc_support_message;
  }
  const UnivariateQcProblem qc_problem(GetParam().use_integer_variables);
  const double x_expected =
      GetParam().use_integer_variables ? 0.0 : -0.618033988748785;
  EXPECT_THAT(SimpleSolve(qc_problem.model),
              IsOkAndHolds(IsOptimalWithSolution(
                  5.0 + x_expected, {{qc_problem.x, x_expected}})));
}

// Models the following problem:
//   min_{x,y} y
//   s.t.      (x - 1)^2 + (y - 1)^2 + xy == x^2 + xy + y^2 - 2x - 2y + 2 <= 1
//             x <= y
//             0 <= x <= 0.5
//             0 <= y <= 1
//
// along with, if use_integer_variables = true, integrality on x and y.
//
// If use_integer_variables = false, the unique optimal solution is attained at
// (x, y) = (1/3, 1/3) with objective value 1/3. Otherwise, the unique optimal
// solution is (x, y) = (0, 1) with objective value 1.
struct HalfEllipseProblem {
  explicit HalfEllipseProblem(bool use_integer_variables)
      : model(),
        x(model.AddVariable(0.0, 0.5, use_integer_variables, "x")),
        y(model.AddVariable(0.0, 1.0, use_integer_variables, "y")),
        q(model.AddQuadraticConstraint(x * x + x * y + y * y - 2 * x - 2 * y <=
                                       -1)) {
    model.Minimize(y);
    model.AddLinearConstraint(x <= y);
  }

  Model model;
  const Variable x;
  const Variable y;
  const QuadraticConstraint q;
};

TEST_P(SimpleQcTest, SolveHalfEllipseQc) {
  if (!GetParam().supports_qc) {
    GTEST_SKIP() << no_qc_support_message;
  }
  const HalfEllipseProblem qc_problem(GetParam().use_integer_variables);
  if (GetParam().use_integer_variables) {
    EXPECT_THAT(SimpleSolve(qc_problem.model),
                IsOkAndHolds(IsOptimalWithSolution(
                    1.0, {{qc_problem.x, 0.0}, {qc_problem.y, 1.0}})));
  } else {
    const double value = 1.0 / 3.0;
    EXPECT_THAT(SimpleSolve(qc_problem.model),
                IsOkAndHolds(IsOptimalWithSolution(
                    value, {{qc_problem.x, value}, {qc_problem.y, value}})));
  }
}

// We start with the simple LP:
//   max  x + 1
//   s.t. 0 <= x <= 1
//
// The optimal value is 2. We then add a quadratic constraint:
//   x^2 <= 0.5
//
// The optimal solution is x = sqrt(0.5) with objective value 1 + sqrt(0.5).
// Additionally, if we impose integrality on x, then the optimal solution is
// x = 0 with objective value 1.
TEST_P(IncrementalQcTest, LinearToQuadraticUpdate) {
  Model model;
  const Variable x =
      model.AddVariable(0.0, 1.0, GetParam().use_integer_variables, "x");
  model.Maximize(x + 1.0);

  ASSERT_OK_AND_ASSIGN(
      const auto solver,
      IncrementalSolver::New(&model, GetParam().solver_type, {}));
  ASSERT_THAT(solver->Solve({.parameters = GetParam().parameters}),
              IsOkAndHolds(IsOptimalWithSolution(2.0, {{x, 1.0}})));

  model.AddQuadraticConstraint(x * x <= 0.5);

  if (!GetParam().supports_qc) {
    // Here we test that solvers that don't support quadratic constraints return
    // false in SolverInterface::CanUpdate(). Thus they should fail in their
    // factory function instead of failing in their SolverInterface::Update()
    // function. To assert we rely on status annotations added by
    // IncrementalSolver::Update() to the returned status of Solver::Update()
    // and Solver::New().
    EXPECT_THAT(
        solver->Update(),
        StatusIs(AnyOf(absl::StatusCode::kInvalidArgument,
                       absl::StatusCode::kUnimplemented),
                 AllOf(HasSubstr("quadratic constraint"),
                       // Sub-string expected for Solver::Update() error.
                       Not(HasSubstr("update failed")),
                       // Sub-string expected for Solver::New() error.
                       HasSubstr("solver re-creation failed"))));
    return;
  }

  ASSERT_THAT(solver->Update(),
              IsOkAndHolds(GetParam().supports_incremental_add_and_deletes
                               ? DidUpdate()
                               : Not(DidUpdate())));
  const double expected_x =
      GetParam().use_integer_variables ? 0.0 : std::sqrt(0.5);
  EXPECT_THAT(
      solver->SolveWithoutUpdate({.parameters = GetParam().parameters}),
      IsOkAndHolds(IsOptimalWithSolution(1.0 + expected_x, {{x, expected_x}})));
}

// We start with the QCP:
//   min_{x,y} y
//   s.t.      (x - 1)^2 + (y - 1)^2 + xy <= 1
//             x <= y
//             0 <= x <= 0.5
//             0 <= y <= 1
//
// We then delete the quadratic constraint, leaving the LP:
//   min_{x,y} y
//   s.t.      x <= y
//             0 <= x <= 0.5
//             0 <= y <= 1
//
// The optimal solution is attained at (x, y) = (0, 0).
TEST_P(IncrementalQcTest, UpdateDeletesQuadraticConstraint) {
  if (!GetParam().supports_qc) {
    GTEST_SKIP() << no_qc_support_message;
  }
  HalfEllipseProblem qc_problem(GetParam().use_integer_variables);
  ASSERT_OK_AND_ASSIGN(
      const auto solver,
      IncrementalSolver::New(&qc_problem.model, GetParam().solver_type, {}));
  // We test that the solution is correct elsewhere.
  ASSERT_OK(solver->Solve({.parameters = GetParam().parameters}));

  qc_problem.model.DeleteQuadraticConstraint(qc_problem.q);

  ASSERT_THAT(solver->Update(),
              IsOkAndHolds(GetParam().supports_incremental_add_and_deletes
                               ? DidUpdate()
                               : Not(DidUpdate())));
  EXPECT_THAT(solver->SolveWithoutUpdate({.parameters = GetParam().parameters}),
              IsOkAndHolds(IsOptimalWithSolution(
                  0.0, {{qc_problem.x, 0.0}, {qc_problem.y, 0.0}})));
}

// We start with the QCP:
//   min_{x,y} y
//   s.t.      (x - 1)^2 + (y - 1)^2 + xy <= 1
//             x <= y
//             0 <= x, y <= 2
//
// We then delete the x variable, leaving the QCP:
//   min_{y} y
//   s.t.   1 + (y - 1)^2 == y^2 - 2y + 2 <= 1
//          0 <= y <= 2
//
// The optimal solution is attained at y = 1 with objective value 1.
TEST_P(IncrementalQcTest, UpdateDeletesVariableInQuadraticConstraint) {
  if (!GetParam().supports_qc) {
    GTEST_SKIP() << no_qc_support_message;
  }
  HalfEllipseProblem qc_problem(GetParam().use_integer_variables);

  ASSERT_OK_AND_ASSIGN(
      const auto solver,
      IncrementalSolver::New(&qc_problem.model, GetParam().solver_type, {}));
  // We test that the solution is correct elsewhere.
  ASSERT_OK(solver->Solve({.parameters = GetParam().parameters}));

  qc_problem.model.DeleteVariable(qc_problem.x);

  ASSERT_THAT(solver->Update(),
              IsOkAndHolds(GetParam().supports_incremental_variable_deletions
                               ? DidUpdate()
                               : Not(DidUpdate())));
  EXPECT_THAT(solver->SolveWithoutUpdate({.parameters = GetParam().parameters}),
              IsOkAndHolds(IsOptimalWithSolution(1.0, {{qc_problem.y, 1.0}},
                                                 kTolerance)));
}

}  // namespace
}  // namespace operations_research::math_opt
