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

#include "ortools/math_opt/solver_tests/second_order_cone_tests.h"

#include <cmath>
#include <memory>
#include <ostream>
#include <utility>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/cpp/matchers.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/port/proto_utils.h"

namespace operations_research::math_opt {

SecondOrderConeTestParameters::SecondOrderConeTestParameters(
    const SolverType solver_type, SolveParameters parameters,
    const bool supports_soc_constraints,
    const bool supports_incremental_add_and_deletes)
    : solver_type(solver_type),
      parameters(std::move(parameters)),
      supports_soc_constraints(supports_soc_constraints),
      supports_incremental_add_and_deletes(
          supports_incremental_add_and_deletes) {}

std::ostream& operator<<(std::ostream& out,
                         const SecondOrderConeTestParameters& params) {
  out << "{ solver_type: " << params.solver_type
      << ", parameters: " << ProtobufShortDebugString(params.parameters.Proto())
      << ", supports_soc_constraints: "
      << (params.supports_soc_constraints ? "true" : "false")
      << ", supports_incremental_add_and_deletes: "
      << (params.supports_incremental_add_and_deletes ? "true" : "false");
  return out;
}

namespace {

using ::testing::AnyOf;
using ::testing::HasSubstr;
using ::testing::status::IsOkAndHolds;
using ::testing::status::StatusIs;

// A bit larger than expected; as of 2023-01-31 Gurobi produces slightly
// inaccurate solutions on some of the tests.
constexpr double kTolerance = 1.0e-3;
constexpr absl::string_view kNoSocSupportMessage =
    "This test is disabled as the solver does not support second-order cone "
    "constraints";
constexpr absl::string_view kNoIncrementalAddAndDeletes =
    "This test is disabled as the solver does not support incremental add and "
    "deletes";

// Builds the simple (and uninteresting) SOC model:
//
// min  0
// s.t. ||x||_2 <= 2x
//      0 <= x <= 1.
TEST_P(SimpleSecondOrderConeTest, CanBuildSecondOrderConeModel) {
  if (GetParam().solver_type == SolverType::kXpress) {
    // For Xpress the second order cone constraint results in
    //     x^2 - 4x^2 <= 0
    // This has two entries for x and will thus be rejected by the library.
    // Hence we have to skip the test.
    GTEST_SKIP()
        << "This test is disabled as Xpress rejects duplicate Q entries";
  }
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 1.0, "x");
  model.AddSecondOrderConeConstraint({x}, 2 * x);
  if (GetParam().supports_soc_constraints) {
    EXPECT_OK(NewIncrementalSolver(&model, GetParam().solver_type, {}));
  } else {
    EXPECT_THAT(NewIncrementalSolver(&model, GetParam().solver_type, {}),
                StatusIs(AnyOf(absl::StatusCode::kInvalidArgument,
                               absl::StatusCode::kUnimplemented),
                         HasSubstr("second-order cone constraints")));
  }
}

// We model the second-order cone program:
//
// max  x + y + z
// s.t. ||(x, 2y, 3z)||_2 <= 1
//      0 <= x, y <= 1
//
// The unique optimal solution is (x*, y*, z*) = (6/7, 3/14, 2/21) with
// objective value 7/6.
TEST_P(SimpleSecondOrderConeTest, SolveSimpleSocModel) {
  if (!GetParam().supports_soc_constraints) {
    GTEST_SKIP() << kNoSocSupportMessage;
  }
  if (GetParam().solver_type == SolverType::kXpress) {
    GTEST_SKIP() << "This test is disabled as Xpress only supports second "
                    "order cone constraints on singletons";
  }
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 1.0, "x");
  const Variable y = model.AddContinuousVariable(0.0, 1.0, "y");
  const Variable z = model.AddContinuousVariable(0.0, 1.0, "z");
  model.Maximize(x + y + z);
  model.AddSecondOrderConeConstraint({x, 2.0 * y, 3.0 * z}, 1.0);
  EXPECT_THAT(SimpleSolve(model),
              IsOkAndHolds(IsOptimalWithSolution(
                  7.0 / 6.0, {{x, 6.0 / 7.0}, {y, 3.0 / 14.0}, {z, 2.0 / 21.0}},
                  kTolerance)));
}

// We model the second-order cone program:
//
// max  x + y
// s.t. ||(x, 2y)||_2 <= 2x + 3
//      ||(2x, y)||_2 <= 2y + 3
//
// The unique optimal solution is (x*, y*) = (1, 1) with objective value 2.
TEST_P(SimpleSecondOrderConeTest, SolveMultipleSocConstraintModel) {
  if (!GetParam().supports_soc_constraints) {
    GTEST_SKIP() << kNoSocSupportMessage;
  }
  if (GetParam().solver_type == SolverType::kXpress) {
    GTEST_SKIP() << "This test is disabled as Xpress only supports second "
                    "order cone constraints on singletons";
  }
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 1.0, "x");
  const Variable y = model.AddContinuousVariable(0.0, 1.0, "y");
  model.Maximize(x + y);
  model.AddSecondOrderConeConstraint({x, 2.0 * y}, 2.0 * x + 3.0);
  model.AddSecondOrderConeConstraint({2.0 * x, y}, 2.0 * y + 3.0);
  EXPECT_THAT(SimpleSolve(model),
              IsOkAndHolds(IsOptimalWithSolution(2.0, {{x, 1.0}, {y, 1.0}})));
}

// We model the second-order cone program:
//
// max  x
// s.t. x - y <= 1
//      ||(x, y)||_2 <= 2
//
// The unique optimal solution is (x*, y*) = ((sqrt(7)+1)/2, (sqrt(7)-1)/2) with
// objective value (sqrt(7)+1)/2.
TEST_P(SimpleSecondOrderConeTest, SolveModelWithSocAndLinearConstraints) {
  if (!GetParam().supports_soc_constraints) {
    GTEST_SKIP() << kNoSocSupportMessage;
  }
  Model model;
  const Variable x = model.AddVariable("x");
  const Variable y = model.AddVariable("y");
  model.Maximize(x);
  model.AddLinearConstraint(x - y <= 1.0);
  model.AddSecondOrderConeConstraint({x, y}, 2.0);
  const double sqrt_of_seven = std::sqrt(7.0);
  EXPECT_THAT(
      SimpleSolve(model),
      IsOkAndHolds(IsOptimalWithSolution(
          (sqrt_of_seven + 1.0) / 2.0,
          {{x, (sqrt_of_seven + 1.0) / 2.0}, {y, (sqrt_of_seven - 1.0) / 2.0}},
          kTolerance)));
}

// We start with the LP:
//
// max  x + y
// s.t. x + 0.5y <= 1
//      0 <= x, y <= 1
//
// The unique optimal solution is (x*, y*) = (0.5, 1) with objective value 1.5.
//
// We then add the second-order cone constraint
//
//      ||(x, y)||_2 <= sqrt(0.5)
//
// The unique optimal solution is then (x*, y*) = (0.5, 0.5) with objective
// value 1.
TEST_P(IncrementalSecondOrderConeTest, LinearToSecondOrderConeUpdate) {
  if (!GetParam().supports_incremental_add_and_deletes) {
    GTEST_SKIP() << kNoIncrementalAddAndDeletes;
  }
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 1.0, "x");
  const Variable y = model.AddContinuousVariable(0.0, 1.0, "y");
  model.AddLinearConstraint(x + 0.5 * y <= 1.0);
  model.Maximize(x + y);

  ASSERT_OK_AND_ASSIGN(
      const auto solver,
      NewIncrementalSolver(&model, GetParam().solver_type, {}));
  ASSERT_THAT(solver->Solve({.parameters = GetParam().parameters}),
              IsOkAndHolds(IsOptimalWithSolution(1.5, {{x, 0.5}, {y, 1.0}})));

  model.AddSecondOrderConeConstraint({x, y}, std::sqrt(0.5));

  if (!GetParam().supports_soc_constraints) {
    // Here we test that solvers that don't support second-order cone
    // constraints return false in SolverInterface::CanUpdate(). Thus they
    // should fail in their factory function instead of failing in their
    // SolverInterface::Update() function. To assert we rely on status
    //  annotations added by IncrementalSolver::Update() to the returned status
    // of Solver::Update() and Solver::New().
    EXPECT_THAT(
        solver->Update(),
        StatusIs(AnyOf(absl::StatusCode::kInvalidArgument,
                       absl::StatusCode::kUnimplemented),
                 AllOf(HasSubstr("second-order cone constraint"),
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
  EXPECT_THAT(solver->SolveWithoutUpdate({.parameters = GetParam().parameters}),
              IsOkAndHolds(IsOptimalWithSolution(1.0, {{x, 0.5}, {y, 0.5}},
                                                 kTolerance)));
}

// We start with the SOCP:
//
// max  x + y
// s.t. x + 0.5y <= 1
//      ||(x, y)||_2 <= sqrt(0.5)
//      0 <= x, y <= 1
//
// The unique optimal solution is then (x*, y*) = (0.5, 0.5) with objective
// value 1.
//
// We then delete the SOC constraint, leaving the LP:
//
// max  x + y
// s.t. x + 0.5y <= 1
//      0 <= x, y <= 1
//
// The unique optimal solution is (x*, y*) = (0.5, 1) with objective value 1.5.
TEST_P(IncrementalSecondOrderConeTest, UpdateDeletesSecondOrderConeConstraint) {
  if (!GetParam().supports_soc_constraints) {
    GTEST_SKIP() << kNoSocSupportMessage;
  }
  if (!GetParam().supports_incremental_add_and_deletes) {
    GTEST_SKIP() << kNoIncrementalAddAndDeletes;
  }
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 1.0, "x");
  const Variable y = model.AddContinuousVariable(0.0, 1.0, "y");
  model.AddLinearConstraint(x + 0.5 * y <= 1.0);
  const SecondOrderConeConstraint c =
      model.AddSecondOrderConeConstraint({x, y}, std::sqrt(0.5));
  model.Maximize(x + y);

  ASSERT_OK_AND_ASSIGN(
      const auto solver,
      NewIncrementalSolver(&model, GetParam().solver_type, {}));
  ASSERT_THAT(solver->Solve({.parameters = GetParam().parameters}),
              IsOkAndHolds(IsOptimalWithSolution(1.0, {{x, 0.5}, {y, 0.5}},
                                                 kTolerance)));

  model.DeleteSecondOrderConeConstraint(c);

  ASSERT_THAT(solver->Update(),
              IsOkAndHolds(GetParam().supports_incremental_add_and_deletes
                               ? DidUpdate()
                               : Not(DidUpdate())));
  EXPECT_THAT(solver->SolveWithoutUpdate({.parameters = GetParam().parameters}),
              IsOkAndHolds(IsOptimalWithSolution(1.5, {{x, 0.5}, {y, 1.0}})));
}

// We start with the SOCP:
//
// max  x + y
// s.t. ||x||_2 <= y
//      0 <= x, y <= 1
//
// The unique optimal solution is then (x*, y*) = (1, 1) with objective value 2.
//
// We then delete the y variable, leaving the SOCP:
//
// max  x
// s.t. ||x||_2 <= 0
//      0 <= x <= 1
//
// The unique optimal solution is x* = 0 with objective value 0.
TEST_P(IncrementalSecondOrderConeTest, UpdateDeletesUpperBoundingVariable) {
  if (!GetParam().supports_soc_constraints) {
    GTEST_SKIP() << kNoSocSupportMessage;
  }
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 1.0, "x");
  const Variable y = model.AddContinuousVariable(0.0, 1.0, "y");
  model.AddSecondOrderConeConstraint({x}, y);
  model.Maximize(x + y);

  ASSERT_OK_AND_ASSIGN(
      const auto solver,
      NewIncrementalSolver(&model, GetParam().solver_type, {}));
  ASSERT_THAT(solver->Solve({.parameters = GetParam().parameters}),
              IsOkAndHolds(IsOptimalWithSolution(2.0, {{x, 1.0}, {y, 1.0}})));

  model.DeleteVariable(y);

  ASSERT_THAT(solver->Update(),
              IsOkAndHolds(GetParam().supports_incremental_add_and_deletes
                               ? DidUpdate()
                               : Not(DidUpdate())));
  EXPECT_THAT(solver->SolveWithoutUpdate({.parameters = GetParam().parameters}),
              IsOkAndHolds(IsOptimalWithSolution(0.0, {{x, 0.0}})));
}

// We start with the SOCP:
//
// max  x + y
// s.t. ||x||_2 <= y + 1
//      0 <= x <= 2
//      0 <= y <= 1
//
// The unique optimal solution is then (x*, y*) = (2, 1) with objective value 3.
//
// We then delete the y variable, leaving the SOCP:
//
// max  x
// s.t. ||x||_2 <= 1
//      0 <= x <= 2
//
// The unique optimal solution is x* = 1 with objective value 1.
TEST_P(IncrementalSecondOrderConeTest,
       UpdateDeletesVariableInUpperBoundingExpression) {
  if (!GetParam().supports_soc_constraints) {
    GTEST_SKIP() << kNoSocSupportMessage;
  }
  if (!GetParam().supports_incremental_add_and_deletes) {
    GTEST_SKIP() << kNoIncrementalAddAndDeletes;
  }
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 2.0, "x");
  const Variable y = model.AddContinuousVariable(0.0, 1.0, "y");
  model.AddSecondOrderConeConstraint({x}, y + 1.0);
  model.Maximize(x + y);

  ASSERT_OK_AND_ASSIGN(
      const auto solver,
      NewIncrementalSolver(&model, GetParam().solver_type, {}));
  ASSERT_THAT(solver->Solve({.parameters = GetParam().parameters}),
              IsOkAndHolds(IsOptimalWithSolution(3.0, {{x, 2.0}, {y, 1.0}})));

  model.DeleteVariable(y);

  ASSERT_THAT(solver->Update(),
              IsOkAndHolds(GetParam().supports_incremental_add_and_deletes
                               ? DidUpdate()
                               : Not(DidUpdate())));
  EXPECT_THAT(solver->SolveWithoutUpdate({.parameters = GetParam().parameters}),
              IsOkAndHolds(IsOptimalWithSolution(1.0, {{x, 1.0}})));
}

// We start with the SOCP:
//
// min  y
// s.t. ||x||_2 <= y
//      1 <= x <= 1
//      0 <= y <= 1
//
// The unique optimal solution is then (x*, y*) = (1, 1) with objective value 1.
//
// We then delete the x variable, leaving the SOCP:
//
// max  y
// s.t. ||0||_2 <= y
//      0 <= y <= 1
//
// The unique optimal solution is y* = 0 with objective value 0.
TEST_P(IncrementalSecondOrderConeTest, UpdateDeletesVariableThatIsAnArgument) {
  if (!GetParam().supports_soc_constraints) {
    GTEST_SKIP() << kNoSocSupportMessage;
  }
  if (!GetParam().supports_incremental_add_and_deletes) {
    GTEST_SKIP() << kNoIncrementalAddAndDeletes;
  }
  Model model;
  const Variable x = model.AddContinuousVariable(1.0, 1.0, "x");
  const Variable y = model.AddContinuousVariable(0.0, 1.0, "y");
  model.AddSecondOrderConeConstraint({x}, y);
  model.Minimize(y);

  ASSERT_OK_AND_ASSIGN(
      const auto solver,
      NewIncrementalSolver(&model, GetParam().solver_type, {}));
  ASSERT_THAT(solver->Solve({.parameters = GetParam().parameters}),
              IsOkAndHolds(IsOptimalWithSolution(1.0, {{x, 1.0}, {y, 1.0}})));

  model.DeleteVariable(x);

  ASSERT_THAT(solver->Update(),
              IsOkAndHolds(GetParam().supports_incremental_add_and_deletes
                               ? DidUpdate()
                               : Not(DidUpdate())));
  EXPECT_THAT(solver->SolveWithoutUpdate({.parameters = GetParam().parameters}),
              IsOkAndHolds(IsOptimalWithSolution(0.0, {{y, 0.0}})));
}

// We start with the SOCP:
//
// min  y
// s.t. ||x + 1||_2 <= y
//      1 <= x <= 1
//      0 <= y <= 2
//
// The unique optimal solution is then (x*, y*) = (1, 2) with objective value 2.
//
// We then delete the x variable, leaving the SOCP:
//
// max  y
// s.t. ||1||_2 <= y
//      0 <= y <= 2
//
// The unique optimal solution is y* = 1 with objective value 1.
TEST_P(IncrementalSecondOrderConeTest, UpdateDeletesVariableInAnArgument) {
  if (!GetParam().supports_soc_constraints) {
    GTEST_SKIP() << kNoSocSupportMessage;
  }
  if (!GetParam().supports_incremental_add_and_deletes) {
    GTEST_SKIP() << kNoIncrementalAddAndDeletes;
  }
  Model model;
  const Variable x = model.AddContinuousVariable(1.0, 1.0, "x");
  const Variable y = model.AddContinuousVariable(0.0, 2.0, "y");
  model.AddSecondOrderConeConstraint({x + 1.0}, y);
  model.Minimize(y);

  ASSERT_OK_AND_ASSIGN(
      const auto solver,
      NewIncrementalSolver(&model, GetParam().solver_type, {}));
  ASSERT_THAT(solver->Solve({.parameters = GetParam().parameters}),
              IsOkAndHolds(IsOptimalWithSolution(2.0, {{x, 1.0}, {y, 2.0}})));

  model.DeleteVariable(x);

  ASSERT_THAT(solver->Update(),
              IsOkAndHolds(GetParam().supports_incremental_add_and_deletes
                               ? DidUpdate()
                               : Not(DidUpdate())));
  EXPECT_THAT(solver->SolveWithoutUpdate({.parameters = GetParam().parameters}),
              IsOkAndHolds(IsOptimalWithSolution(1.0, {{y, 1.0}})));
}

}  // namespace
}  // namespace operations_research::math_opt
