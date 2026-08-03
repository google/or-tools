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

#include "ortools/math_opt/solver_tests/qp_tests.h"

#include <limits>
#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/cpp/matchers.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/port/proto_utils.h"

// TODO(b/178702980): this should not be needed
// IWYU pragma: no_include <type_traits>

namespace operations_research::math_opt {

using ::testing::AnyOf;
using ::testing::HasSubstr;
using ::testing::status::IsOkAndHolds;
using ::testing::status::StatusIs;

constexpr std::string_view kNoQpSupportMessage =
    "This test is disabled as the solver does not support quadratic objectives";

constexpr std::string_view kNoNonDiagonalQpSupportMessage =
    "This test is disabled as the solver does not support non-diagonal "
    "quadratic objectives";

constexpr double kInf = std::numeric_limits<double>::infinity();
constexpr double kTolerance = 1.0e-3;

QpTestParameters::QpTestParameters(
    const SolverType solver_type, SolveParameters parameters,
    const QpSupportType qp_support,
    const bool supports_incrementalism_not_modifying_qp,
    const bool supports_qp_incrementalism, const bool use_integer_variables)
    : solver_type(solver_type),
      parameters(std::move(parameters)),
      qp_support(qp_support),
      supports_incrementalism_not_modifying_qp(
          supports_incrementalism_not_modifying_qp),
      supports_qp_incrementalism(supports_qp_incrementalism),
      use_integer_variables(use_integer_variables) {}

std::string ToString(QpSupportType qp_support) {
  switch (qp_support) {
    case QpSupportType::kNoQpSupport:
      return "No QP support";
    case QpSupportType::kDiagonalQpOnly:
      return "Diagonal QP only";
    case QpSupportType::kConvexQp:
      return "Convex QP";
  }
  LOG(FATAL) << "Invalid QpSupportType";
  return "";
}

std::ostream& operator<<(std::ostream& out, const QpTestParameters& params) {
  out << "{ solver_type: " << params.solver_type
      << ", parameters: " << ProtobufShortDebugString(params.parameters.Proto())
      << ", qp_support: " << ToString(params.qp_support)
      << ", supports_incrementalism_not_modifying_qp: "
      << (params.supports_incrementalism_not_modifying_qp ? "true" : "false")
      << ", supports_qp_incrementalism: "
      << (params.supports_qp_incrementalism ? "true" : "false")
      << ", use_integer_variables: "
      << (params.use_integer_variables ? "true" : "false") << " }";
  return out;
}

namespace {

TEST_P(SimpleQpTest, CanBuildQpModel) {
  Model model;
  const Variable x =
      model.AddVariable(0, 1, GetParam().use_integer_variables, "x");
  model.Minimize(x * x - 0.5 * x + 0.0625);

  if (GetParam().qp_support == QpSupportType::kDiagonalQpOnly ||
      GetParam().qp_support == QpSupportType::kConvexQp) {
    EXPECT_THAT(SimpleSolve(model),
                IsOkAndHolds(IsOptimal(
                    GetParam().use_integer_variables ? 0.0625 : 0.0)));
  } else {
    EXPECT_THAT(SimpleSolve(model),
                StatusIs(AnyOf(absl::StatusCode::kInvalidArgument,
                               absl::StatusCode::kUnimplemented),
                         HasSubstr("quadratic objective")));
  }
}

// Models the following problem:
//   min_x (x - 0.25)^2 = x^2 - 0.5x + 0.0625
//   s.t.  0 <= x <= 1
//
// along with, if use_integer_variables = true, integrality on x.
//
// The unique optimal solution is attained at x = 0.25 with objective value 0.
// If in addition you impose integrality on x, the unique optimal solution is
// x = 0 with objective value 0.0625.
struct UnivariateQpProblem {
  explicit UnivariateQpProblem(bool use_integer_variables)
      : model(), x(model.AddVariable(0, 1, use_integer_variables, "x")) {
    model.Minimize(x * x - 0.5 * x + 0.0625);
  }

  Model model;
  const Variable x;
};

// Models the following problem:
//   min_(x,y} Q(x,y) = (x-0.2)^2 + (y-0.8)^2 + (x-0.2)(y-0.8)
//                    = x^2 + xy - 1.2x + y^2 - 1.8y + 0.84
//   s.t.      x + y = 1
//             0 <= x, y <= 1
//
// along with, if use_integer_variables = true, integrality on x and y.
//
// The unique optimal solution is attained at (x,y) = (0.2, 0.8) with objective
// value 0. To see this, observe that our quadratic objective Q has:
//   - Jacobian = [2x + y - 1.2]   and   Hessian = [2 1]
//                [x + 2y - 1.8]                   [1 2].
// The Hessian shows that the Q is convex. Setting the Jacobian equal to zero
// and solving the linear system, we derive that (x,y) = (0.2, 0.8) is the
// unique global minimum of Q. It is also feasible for our constrained problem
// above, yielding the result.
//
// If integrality is imposed on x and y, the unique optimal solution is
// (x,y) = (0,1) with objective value 0.04.
struct SimplexConstrainedQpProblem {
  explicit SimplexConstrainedQpProblem(bool use_integer_variables)
      : model(),
        x(model.AddVariable(0, 1, use_integer_variables, "x")),
        y(model.AddVariable(0, 1, use_integer_variables, "y")) {
    model.Minimize(x * x + x * y - 1.2 * x + y * y - 1.8 * y + 0.84);
    model.AddLinearConstraint(x + y == 1);
  }

  Model model;
  const Variable x;
  const Variable y;
};

TEST_P(SimpleQpTest, SolveUnivariateQp) {
  if (GetParam().qp_support == QpSupportType::kNoQpSupport) {
    GTEST_SKIP() << kNoQpSupportMessage;
  }
  const UnivariateQpProblem qp_problem(GetParam().use_integer_variables);
  ASSERT_OK_AND_ASSIGN(const SolveResult result, SimpleSolve(qp_problem.model));
  if (GetParam().use_integer_variables) {
    EXPECT_THAT(result, IsOptimalWithSolution(0.0625, {{qp_problem.x, 0.0}},
                                              kTolerance));
  } else {
    EXPECT_THAT(result,
                IsOptimalWithSolution(0.0, {{qp_problem.x, 0.25}}, kTolerance));
  }
}

TEST_P(SimpleQpTest, SolveSimplexConstrainedQp) {
  if (GetParam().qp_support != QpSupportType::kConvexQp) {
    GTEST_SKIP() << kNoNonDiagonalQpSupportMessage;
  }

  const SimplexConstrainedQpProblem qp_problem(
      GetParam().use_integer_variables);

  ASSERT_OK_AND_ASSIGN(const SolveResult result, SimpleSolve(qp_problem.model));
  if (GetParam().use_integer_variables) {
    EXPECT_THAT(result, IsOptimalWithSolution(
                            0.04, {{qp_problem.x, 0.0}, {qp_problem.y, 1.0}},
                            kTolerance));
  } else {
    EXPECT_THAT(result, IsOptimalWithSolution(
                            0.0, {{qp_problem.x, 0.2}, {qp_problem.y, 0.8}},
                            kTolerance));
  }
}

TEST_P(IncrementalQpTest, EmptyUpdate) {
  if (GetParam().qp_support == QpSupportType::kNoQpSupport) {
    GTEST_SKIP() << kNoQpSupportMessage;
  }

  UnivariateQpProblem qp_problem(GetParam().use_integer_variables);

  ASSERT_OK_AND_ASSIGN(const std::unique_ptr<IncrementalSolver> solver,
                       NewIncrementalSolver(&qp_problem.model, TestedSolver()));
  ASSERT_OK_AND_ASSIGN(const SolveResult first_result,
                       solver->Solve({.parameters = GetParam().parameters}));
  ASSERT_THAT(first_result,
              IsOptimal(GetParam().use_integer_variables ? 0.0625 : 0.0));

  // NOTE: This should work even for a solver with no incrementalism support.
  ASSERT_THAT(solver->Update(), IsOkAndHolds(DidUpdate()));
  ASSERT_OK_AND_ASSIGN(const SolveResult second_result,
                       solver->SolveWithoutUpdate());

  if (GetParam().use_integer_variables) {
    EXPECT_THAT(second_result, IsOptimalWithSolution(
                                   0.0625, {{qp_problem.x, 0.0}}, kTolerance));
  } else {
    EXPECT_THAT(second_result,
                IsOptimalWithSolution(0.0, {{qp_problem.x, 0.25}}, kTolerance));
    if (GetParam().supports_incrementalism_not_modifying_qp &&
        GetParam().solver_type != SolverType::kGscip) {
      EXPECT_EQ(second_result.solve_stats.barrier_iterations, 0);
      EXPECT_EQ(second_result.solve_stats.simplex_iterations, 0);
      EXPECT_EQ(second_result.solve_stats.first_order_iterations, 0);
    }
  }
}

TEST_P(IncrementalQpTest, LinearToQuadraticUpdate) {
  // We remove the quadratic coefficient x * x from the objective, leaving an LP
  UnivariateQpProblem qp_problem(GetParam().use_integer_variables);
  qp_problem.model.set_objective_coefficient(qp_problem.x, qp_problem.x, 0);
  ASSERT_OK_AND_ASSIGN(const std::unique_ptr<IncrementalSolver> solver,
                       NewIncrementalSolver(&qp_problem.model, TestedSolver()));
  ASSERT_OK_AND_ASSIGN(const SolveResult first_result,
                       solver->Solve({.parameters = GetParam().parameters}));
  ASSERT_THAT(first_result, IsOptimal(0.0625 - 0.5));

  // We now reset the objective with the "missing" objective term to its
  // previous value, leaving a QP.
  qp_problem.model.set_objective_coefficient(qp_problem.x, qp_problem.x, 1);

  if (GetParam().qp_support == QpSupportType::kNoQpSupport) {
    // Here we test that solvers that don't support quadratic objective return
    // false in SolverInterface::CanUpdate(). Thus they should fail in their
    // factory function instead of failing in their SolverInterface::Update()
    // function. To assert we rely on status annotations added by
    // IncrementalSolver::Update() to the returned status of Solver::Update()
    // and Solver::New().
    EXPECT_THAT(
        solver->Update(),
        StatusIs(AnyOf(absl::StatusCode::kInvalidArgument,
                       absl::StatusCode::kUnimplemented),
                 AllOf(HasSubstr("quadratic objective"),
                       // Sub-string expected for Solver::Update() error.
                       Not(HasSubstr("update failed")),
                       // Sub-string expected for Solver::New() error.
                       HasSubstr("solver re-creation failed"))));
    return;
  }

  ASSERT_THAT(solver->Update(),
              IsOkAndHolds(GetParam().supports_incrementalism_not_modifying_qp
                               ? DidUpdate()
                               : Not(DidUpdate())));

  if (GetParam().use_integer_variables) {
    EXPECT_THAT(
        solver->SolveWithoutUpdate({.parameters = GetParam().parameters}),
        IsOkAndHolds(
            IsOptimalWithSolution(0.0625, {{qp_problem.x, 0.0}}, kTolerance)));
  } else {
    EXPECT_THAT(
        solver->SolveWithoutUpdate({.parameters = GetParam().parameters}),
        IsOkAndHolds(
            IsOptimalWithSolution(0.0, {{qp_problem.x, 0.25}}, kTolerance)));
  }
}

TEST_P(IncrementalQpTest, ModifyQuadraticObjective) {
  if (GetParam().qp_support == QpSupportType::kNoQpSupport) {
    GTEST_SKIP() << kNoQpSupportMessage;
  }

  UnivariateQpProblem qp_problem(GetParam().use_integer_variables);

  ASSERT_OK_AND_ASSIGN(const std::unique_ptr<IncrementalSolver> solver,
                       NewIncrementalSolver(&qp_problem.model, TestedSolver()));
  ASSERT_OK_AND_ASSIGN(const SolveResult first_result,
                       solver->Solve({.parameters = GetParam().parameters}));
  ASSERT_THAT(first_result,
              IsOptimal(GetParam().use_integer_variables ? 0.0625 : 0.0));

  // Now we change the objective to (x-0.75)^2 = x^2 - 1.5x + 0.5625. The new
  // optimal solution for the continuous problem is x=0.75 with objective value
  // 0; for the integer problem it is x=1 with objective value 0.0625.
  const Variable x = qp_problem.x;
  qp_problem.model.Minimize(x * x - 1.5 * x + 0.5625);

  if (!GetParam().supports_qp_incrementalism) {
    EXPECT_THAT(solver->Update(), IsOkAndHolds(Not(DidUpdate())));
    return;
  }
  ASSERT_THAT(solver->Update(), IsOkAndHolds(DidUpdate()));
  ASSERT_OK_AND_ASSIGN(const SolveResult second_result,
                       solver->SolveWithoutUpdate());

  if (GetParam().use_integer_variables) {
    EXPECT_THAT(second_result,
                IsOptimalWithSolution(0.0625, {{x, 1.0}}, kTolerance));
  } else {
    EXPECT_THAT(second_result,
                IsOptimalWithSolution(0.0, {{x, 0.75}}, kTolerance));
  }
}

TEST_P(IncrementalQpTest, DeleteVariable) {
  if (GetParam().qp_support != QpSupportType::kConvexQp) {
    GTEST_SKIP() << kNoNonDiagonalQpSupportMessage;
  }

  SimplexConstrainedQpProblem qp_problem(GetParam().use_integer_variables);

  ASSERT_OK_AND_ASSIGN(const std::unique_ptr<IncrementalSolver> solver,
                       NewIncrementalSolver(&qp_problem.model, TestedSolver()));
  ASSERT_OK_AND_ASSIGN(const SolveResult first_result,
                       solver->Solve({.parameters = GetParam().parameters}));
  ASSERT_THAT(first_result,
              IsOptimal(GetParam().use_integer_variables ? 0.04 : 0.0));

  // After deleting x, the only feasible solution is y=1 with objective
  // value 0.04.
  qp_problem.model.DeleteVariable(qp_problem.x);

  if (!GetParam().supports_qp_incrementalism) {
    EXPECT_THAT(solver->Update(), IsOkAndHolds(Not(DidUpdate())));
    return;
  }

  ASSERT_THAT(solver->Update(), IsOkAndHolds(DidUpdate()));
  ASSERT_OK_AND_ASSIGN(const SolveResult second_result,
                       solver->SolveWithoutUpdate());

  EXPECT_THAT(second_result,
              IsOptimalWithSolution(0.04, {{qp_problem.y, 1.0}}));
}

// Primal:
//   min  2x_0^2 + 0.5x_1^2 - x_0 - x_1 + 5
//   s.t. -inf <= x_0 + x_1 <= 1
//         1 <= x_0 <= 2
//        -2 <= x_1 <= 4
//
// Optimal solution: x* = (1, 0).
//
// Dual (go/mathopt-qp-dual):
//   max  -2x_0^2 - 0.5x_1^2 + y_0 + min{r_0, 2r_0} + min{-2r_1, 4r_1} + 5
//   s.t. y_0 + r_0 = 4x_0 - 1
//        y_0 + r_1 = x_1 - 1
//        y_0 <= 0
//
//  Optimal solution: x* = (1, 0), y* = (-1), r* = (4, 0).
// TODO(b/225196547): Show unique optimality of the primal/dual solutions.
TEST_P(QpDualsTest, DiagonalQp1) {
  if (GetParam().qp_support == QpSupportType::kNoQpSupport) {
    GTEST_SKIP() << kNoQpSupportMessage;
  }
  Model model;
  const Variable x0 = model.AddContinuousVariable(1, 2);
  const Variable x1 = model.AddContinuousVariable(-2, 4);
  const LinearConstraint y0 = model.AddLinearConstraint(x0 + x1 <= 1);
  model.Minimize(2 * x0 * x0 + 0.5 * x1 * x1 - x0 - x1 + 5);

  ASSERT_OK_AND_ASSIGN(const SolveResult solve_result, SimpleSolve(model));
  const double expected_objective_value = 6.0;
  EXPECT_THAT(solve_result, IsOptimalWithSolution(expected_objective_value,
                                                  {{x0, 1.0}, {x1, 0.0}}));
  EXPECT_THAT(solve_result,
              IsOptimalWithDualSolution(expected_objective_value, {{y0, -1.0}},
                                        {{x0, 4.0}, {x1, 0.0}}));
}

// Primal:
//   min  0.5x_0^2 + 0.5x_1^2 - 3x_0 - x_1
//   s.t. 2 <= x_0 - x_1 <= 2
//        0 <= x_0 <= inf
//        0 <= x_1 <= inf
//
// Optimal solution: x* = (3, 1).
//
// Dual (go/mathopt-qp-dual):
//   max  -0.5x_0^2 - 0.5x_1^2 + 2y_0
//   s.t.  y_0 + r_0 = x_0 - 3
//        -y_0 + r_1 = x_1 - 1
//        r_0 >= 0
//        r_1 >= 0
//
//  Optimal solution: x* = (3, 1), y* = (0), r* = (0, 0).
// TODO(b/225196547): Show unique optimality of the primal/dual solutions.
TEST_P(QpDualsTest, DiagonalQp2) {
  if (GetParam().qp_support == QpSupportType::kNoQpSupport) {
    GTEST_SKIP() << kNoQpSupportMessage;
  }
  Model model;
  const Variable x0 = model.AddContinuousVariable(0, kInf);
  const Variable x1 = model.AddContinuousVariable(0, kInf);
  const LinearConstraint y0 = model.AddLinearConstraint(x0 - x1 == 2);
  model.Minimize(0.5 * x0 * x0 + 0.5 * x1 * x1 - 3 * x0 - x1);

  ASSERT_OK_AND_ASSIGN(const SolveResult solve_result, SimpleSolve(model));
  const double expected_objective_value = -5.0;
  EXPECT_THAT(solve_result, IsOptimalWithSolution(expected_objective_value,
                                                  {{x0, 3.0}, {x1, 1.0}}));
  EXPECT_THAT(solve_result,
              IsOptimalWithDualSolution(expected_objective_value, {{y0, 0.0}},
                                        {{x0, 0.0}, {x1, 0.0}}));
}

// Primal:
//   min  0.5x_1^2 + x_2^2 + x_0 - x_2
//   s.t. 1 <= x_0 - x_2 <= 1
//        4 <= 2x_0 <= 4
//        0 <= x_0 <= inf
//        0 <= x_1 <= inf
//        0 <= x_2 <= inf
//
//  Optimal solution: x* = (2, 0, 1).
//
// Dual (go/mathopt-qp-dual):
//   max  -0.5x_1^2 - x_2^2 + y_0 + 4y_1
//   s.t.  y_0 + 2y_1 + r_0 = 1
//        r_1 = x_1
//        -y_0 + r_2 = 2x_2 - 1
//        r_0 >= 0
//        r_1 >= 0
//        r_2 >= 0
//
//  Optimal solution: x* = (2, 0, 1), y* = (-1, 1), r* = (0, 0, 0).
// TODO(b/225196547): Show unique optimality of the primal/dual solutions.
TEST_P(QpDualsTest, DiagonalQp3) {
  if (GetParam().qp_support == QpSupportType::kNoQpSupport) {
    GTEST_SKIP() << kNoQpSupportMessage;
  }
  Model model;
  const Variable x0 = model.AddContinuousVariable(0, kInf);
  const Variable x1 = model.AddContinuousVariable(0, kInf);
  const Variable x2 = model.AddContinuousVariable(0, kInf);
  const LinearConstraint y0 = model.AddLinearConstraint(x0 - x2 == 1);
  const LinearConstraint y1 = model.AddLinearConstraint(2 * x0 == 4);
  model.Minimize(0.5 * x1 * x1 + x2 * x2 + x0 - x2);

  ASSERT_OK_AND_ASSIGN(const SolveResult solve_result, SimpleSolve(model));
  const double expected_objective_value = 2.0;
  EXPECT_THAT(solve_result,
              IsOptimalWithSolution(expected_objective_value,
                                    {{x0, 2.0}, {x1, 0.0}, {x2, 1.0}}));
  EXPECT_THAT(solve_result,
              IsOptimalWithDualSolution(expected_objective_value,
                                        {{y0, -1.0}, {y1, 1.0}},
                                        {{x0, 0.0}, {x1, 0.0}, {x2, 0.0}}));
}

// Primal:
//   min  x_0^2 + x_0x_1 + 3x_1^2 - 2x_0
//   s.t. 2 <= x_0 + 2x_1 <= inf
//        0 <= x_0 <= inf
//        0 <= x_1 <= inf
//
//  Optimal solution: x* = (1.6, 0.2).
//
// Dual (go/mathopt-qp-dual):
//   max  -x_0^2 - x_0x_1 - 3x_1^2 + 2y_0
//   s.t.  y_0 + r_0 = 2x_0 + x_1 - 2
//        2y_0 + r_1 = x_0 + 6x_1
//        y_0 >= 0
//        r_0 >= 0
//        r_1 >= 0
//
//  Optimal solution: x* = (1.6, 0.2), y* = (1.4), r* = (0, 0).
TEST_P(QpDualsTest, GeneralQp1) {
  if (GetParam().qp_support != QpSupportType::kConvexQp) {
    GTEST_SKIP() << kNoNonDiagonalQpSupportMessage;
  }
  Model model;
  const Variable x0 = model.AddContinuousVariable(0, kInf);
  const Variable x1 = model.AddContinuousVariable(0, kInf);
  const LinearConstraint y0 = model.AddLinearConstraint(x0 + 2 * x1 >= 2);
  model.Minimize(x0 * x0 + x0 * x1 + 3 * x1 * x1 - 2 * x0);

  ASSERT_OK_AND_ASSIGN(const SolveResult solve_result, SimpleSolve(model));
  const double expected_objective_value = -0.2;
  EXPECT_THAT(solve_result, IsOptimalWithSolution(expected_objective_value,
                                                  {{x0, 1.6}, {x1, 0.2}}));
  EXPECT_THAT(solve_result,
              IsOptimalWithDualSolution(expected_objective_value, {{y0, 1.4}},
                                        {{x0, 0.0}, {x1, 0.0}}));
}

// Primal:
//   min  x_0^2 + x_0x_1 + 3x_1^2 - 2x_0
//   s.t. 2 <= x_0 + 2x_1 <= inf
//        0 <= x_0 <= 1
//        1 <= x_1 <= 2
//
//  Optimal solution: x* = (0.5, 1).
//
// Dual (go/mathopt-qp-dual):
//   max  -x_0^2 - x_0x_1 - 3x_1^2 + min{0, r_0} + min{r_1, 2r_1} + 2y_0
//   s.t.  y_0 + r_0 = 2x_0 + x_1 - 2
//        2y_0 + r_1 = x_0 + 6x_1
//        y_0 >= 0
//
//  Optimal solution: x* = (0.5, 1), y* = (0), r* = (0, 6.5).
TEST_P(QpDualsTest, GeneralQp2) {
  if (GetParam().qp_support != QpSupportType::kConvexQp) {
    GTEST_SKIP() << kNoNonDiagonalQpSupportMessage;
  }
  Model model;
  const Variable x0 = model.AddContinuousVariable(0, 1);
  const Variable x1 = model.AddContinuousVariable(1, 2);
  const LinearConstraint y0 = model.AddLinearConstraint(x0 + 2 * x1 >= 2);
  model.Minimize(x0 * x0 + x0 * x1 + 3 * x1 * x1 - 2 * x0);

  ASSERT_OK_AND_ASSIGN(const SolveResult solve_result, SimpleSolve(model));
  const double expected_objective_value = 2.75;
  EXPECT_THAT(solve_result, IsOptimalWithSolution(expected_objective_value,
                                                  {{x0, 0.5}, {x1, 1}}));
  EXPECT_THAT(solve_result,
              IsOptimalWithDualSolution(expected_objective_value, {{y0, 0.0}},
                                        {{x0, 0.0}, {x1, 6.5}}));
}

}  // namespace
}  // namespace operations_research::math_opt
