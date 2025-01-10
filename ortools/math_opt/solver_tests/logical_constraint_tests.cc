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

#include "ortools/math_opt/solver_tests/logical_constraint_tests.h"

#include <memory>
#include <ostream>
#include <utility>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/cpp/matchers.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/result.pb.h"
#include "ortools/port/proto_utils.h"

namespace operations_research::math_opt {

LogicalConstraintTestParameters::LogicalConstraintTestParameters(
    const SolverType solver_type, SolveParameters parameters,
    const bool supports_integer_variables, const bool supports_sos1,
    const bool supports_sos2, const bool supports_indicator_constraints,
    const bool supports_incremental_add_and_deletes,
    const bool supports_incremental_variable_deletions,
    const bool supports_deleting_indicator_variables,
    const bool supports_updating_binary_variables)
    : solver_type(solver_type),
      parameters(std::move(parameters)),
      supports_integer_variables(supports_integer_variables),
      supports_sos1(supports_sos1),
      supports_sos2(supports_sos2),
      supports_indicator_constraints(supports_indicator_constraints),
      supports_incremental_add_and_deletes(
          supports_incremental_add_and_deletes),
      supports_incremental_variable_deletions(
          supports_incremental_variable_deletions),
      supports_deleting_indicator_variables(
          supports_deleting_indicator_variables),
      supports_updating_binary_variables(supports_updating_binary_variables) {}

std::ostream& operator<<(std::ostream& out,
                         const LogicalConstraintTestParameters& params) {
  out << "{ solver_type: " << params.solver_type
      << ", parameters: " << ProtobufShortDebugString(params.parameters.Proto())
      << ", supports_integer_variables: "
      << (params.supports_integer_variables ? "true" : "false")
      << ", supports_sos1: " << (params.supports_sos1 ? "true" : "false")
      << ", supports_sos2: " << (params.supports_sos2 ? "true" : "false")
      << ", supports_indicator_constraints: "
      << (params.supports_indicator_constraints ? "true" : "false")
      << ", supports_incremental_add_and_deletes: "
      << (params.supports_incremental_add_and_deletes ? "true" : "false")
      << ", supports_incremental_variable_deletions: "
      << (params.supports_incremental_variable_deletions ? "true" : "false")
      << ", supports_deleting_indicator_variables: "
      << (params.supports_deleting_indicator_variables ? "true" : "false")
      << ", supports_updating_binary_variables: "
      << (params.supports_updating_binary_variables ? "true" : "false") << " }";
  return out;
}

namespace {

using ::testing::AnyOf;
using ::testing::HasSubstr;
using ::testing::status::IsOkAndHolds;
using ::testing::status::StatusIs;

constexpr absl::string_view no_sos1_support_message =
    "This test is disabled as the solver does not support sos1 constraints";
constexpr absl::string_view no_sos2_support_message =
    "This test is disabled as the solver does not support sos2 constraints";
constexpr absl::string_view no_indicator_support_message =
    "This test is disabled as the solver does not support indicator "
    "constraints";

// We test SOS1 constraints with both explicit weights and default weights.
TEST_P(SimpleLogicalConstraintTest, CanBuildSos1Model) {
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 1.0, "x");
  model.AddSos1Constraint({3.0 * x + 2.0}, {3.0});
  model.AddSos1Constraint({2.0 * x + 1.0}, {});
  if (GetParam().supports_sos1) {
    EXPECT_OK(NewIncrementalSolver(&model, GetParam().solver_type, {}));
  } else {
    EXPECT_THAT(NewIncrementalSolver(&model, GetParam().solver_type, {}),
                StatusIs(AnyOf(absl::StatusCode::kInvalidArgument,
                               absl::StatusCode::kUnimplemented),
                         HasSubstr("sos1 constraints")));
  }
}

// We test SOS2 constraints with both explicit weights and default weights.
TEST_P(SimpleLogicalConstraintTest, CanBuildSos2Model) {
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 1.0, "x");
  model.AddSos2Constraint({3.0 * x + 2.0}, {3.0});
  model.AddSos2Constraint({2.0 * x + 1.0}, {});
  if (GetParam().supports_sos2) {
    EXPECT_OK(NewIncrementalSolver(&model, GetParam().solver_type, {}));
  } else {
    EXPECT_THAT(NewIncrementalSolver(&model, GetParam().solver_type, {}),
                StatusIs(AnyOf(absl::StatusCode::kInvalidArgument,
                               absl::StatusCode::kUnimplemented),
                         HasSubstr("sos2 constraints")));
  }
}

// We solve
//
// max  x + 2y
// s.t. {x, y} is SOS-1
//      0 <= x, y <= 1
//
// The optimal solution is (x*, y*) = (0, 1) with objective value 2.
TEST_P(SimpleLogicalConstraintTest, SimpleSos1Instance) {
  if (!GetParam().supports_sos1) {
    GTEST_SKIP() << no_sos1_support_message;
  }
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 1.0, "x");
  const Variable y = model.AddContinuousVariable(0.0, 1.0, "y");
  model.Maximize(x + 2.0 * y);
  model.AddSos1Constraint({x, y}, {});

  EXPECT_THAT(SimpleSolve(model),
              IsOkAndHolds(IsOptimalWithSolution(2.0, {{x, 0.0}, {y, 1.0}})));
}

// We solve
//
// max  2x + y + 3z
// s.t. {x, y, z} is SOS-2
//      0 <= x, y, z <= 1
//
// The optimal solution is (x*, y*, z*) = (0, 1, 1) with objective value 4.
TEST_P(SimpleLogicalConstraintTest, SimpleSos2Instance) {
  if (!GetParam().supports_sos2) {
    GTEST_SKIP() << no_sos2_support_message;
  }
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 1.0, "x");
  const Variable y = model.AddContinuousVariable(0.0, 1.0, "y");
  const Variable z = model.AddContinuousVariable(0.0, 1.0, "z");
  model.Maximize(2.0 * x + 1.0 * y + 3.0 * z);
  model.AddSos2Constraint({x, y, z}, {});

  EXPECT_THAT(SimpleSolve(model), IsOkAndHolds(IsOptimalWithSolution(
                                      4.0, {{x, 0.0}, {y, 1.0}, {z, 1.0}})));
}

// We solve
//
// max 2x + 1.5y + 3z
// s.t. {y, z} is SOS-1
//      {x, y, z} is SOS-2
//      0 <= x, y, z <= 1
//
// The optimal solution is (x*, y*, z*) = (1, 1, 0) with objective value 3.5.
TEST_P(SimpleLogicalConstraintTest, InstanceWithSos1AndSos2) {
  if (!GetParam().supports_sos1) {
    GTEST_SKIP() << no_sos1_support_message;
  }
  if (!GetParam().supports_sos2) {
    GTEST_SKIP() << no_sos2_support_message;
  }
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 1.0, "x");
  const Variable y = model.AddContinuousVariable(0.0, 1.0, "y");
  const Variable z = model.AddContinuousVariable(0.0, 1.0, "z");
  model.Maximize(2.0 * x + 1.5 * y + 3.0 * z);
  model.AddSos1Constraint({y, z}, {});
  model.AddSos2Constraint({x, y, z}, {});

  EXPECT_THAT(SimpleSolve(model), IsOkAndHolds(IsOptimalWithSolution(
                                      3.5, {{x, 1.0}, {y, 1.0}, {z, 0.0}})));
}

// We solve
//
// min x + y
// s.t. {2x - 1, y - 0.75} is SOS-1
//      0 <= x, y <= 1
//
// The optimal solution is (x*, y*) = (0.5, 0) with objective value 0.5.
TEST_P(SimpleLogicalConstraintTest, Sos1WithExpressions) {
  if (!GetParam().supports_sos1) {
    GTEST_SKIP() << no_sos1_support_message;
  }
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 1.0, "x");
  const Variable y = model.AddContinuousVariable(0.0, 1.0, "y");
  model.Minimize(x + y);
  model.AddSos1Constraint({2 * x - 1, y - 0.75}, {});

  EXPECT_THAT(SimpleSolve(model),
              IsOkAndHolds(IsOptimalWithSolution(0.5, {{x, 0.5}, {y, 0.0}})));
}

// We solve
//
// max x + y + z
// s.t. {2x + 1, 8y + 1, 4z + 1} is SOS-2
//      -1 <= x, y, z <= 1
//
// The optimal solution is (x*, y*) = (1, 1, -0.25) with objective value 1.75.
TEST_P(SimpleLogicalConstraintTest, Sos2WithExpressions) {
  if (!GetParam().supports_sos2) {
    GTEST_SKIP() << no_sos2_support_message;
  }
  Model model;
  const Variable x = model.AddContinuousVariable(-1.0, 1.0, "x");
  const Variable y = model.AddContinuousVariable(-1.0, 1.0, "y");
  const Variable z = model.AddContinuousVariable(-1.0, 1.0, "z");
  model.Maximize(x + y + z);
  model.AddSos2Constraint({2 * x + 1, 8 * y + 1, 4 * z + 1}, {});

  EXPECT_THAT(SimpleSolve(model), IsOkAndHolds(IsOptimalWithSolution(
                                      1.75, {{x, 1.0}, {y, 1.0}, {z, -0.25}})));
}

// We solve
//
// min  x
// s.t. {x, x} is SOS-1
//      -1 <= x <= 1
//
// The optimal solution is x* = 0 with objective value 0.
TEST_P(SimpleLogicalConstraintTest, Sos1VariableInMultipleTerms) {
  if (!GetParam().supports_sos1) {
    GTEST_SKIP() << no_sos2_support_message;
  }
  Model model;
  const Variable x = model.AddContinuousVariable(-1.0, 1.0, "x");
  model.Minimize(x);
  model.AddSos1Constraint({x, x});

  EXPECT_THAT(SimpleSolve(model),
              IsOkAndHolds(IsOptimalWithSolution(0.0, {{x, 0.0}})));
}

// We solve
//
// min  x
// s.t. {x, 0, x} is SOS-2
//      -1 <= x <= 1
//
// The optimal solution is x* = 0 with objective value 0.
TEST_P(SimpleLogicalConstraintTest, Sos2VariableInMultipleTerms) {
  if (!GetParam().supports_sos2) {
    GTEST_SKIP() << no_sos2_support_message;
  }
  Model model;
  const Variable x = model.AddContinuousVariable(-1.0, 1.0, "x");
  model.Minimize(x);
  model.AddSos2Constraint({x, 0.0, x});

  EXPECT_THAT(SimpleSolve(model),
              IsOkAndHolds(IsOptimalWithSolution(0.0, {{x, 0.0}})));
}

// We start with the LP
//
// max  x + 2y
// s.t. 0 <= x, y <= 1
//
// We then add the SOS1 constraint
//
// {x, y} is SOS-1
//
// The optimal solution for the modified problem is (x*, y*) = (0, 1) with
// objective value 2.
TEST_P(IncrementalLogicalConstraintTest, LinearToSos1Update) {
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 1.0, "x");
  const Variable y = model.AddContinuousVariable(0.0, 1.0, "y");
  model.Maximize(x + 2.0 * y);

  ASSERT_OK_AND_ASSIGN(
      const auto solver,
      NewIncrementalSolver(&model, GetParam().solver_type, {}));
  ASSERT_THAT(solver->Solve({.parameters = GetParam().parameters}),
              IsOkAndHolds(IsOptimalWithSolution(3.0, {{x, 1.0}, {y, 1.0}})));

  model.AddSos1Constraint({x, y}, {});

  if (!GetParam().supports_sos1) {
    // Here we test that solvers that don't support SOS1 constraints return
    // false in SolverInterface::CanUpdate(). Thus they should fail in their
    // factory function instead of failing in their SolverInterface::Update()
    // function. To assert we rely on status annotations added by
    // IncrementalSolver::Update() to the returned status of Solver::Update()
    // and Solver::New().
    EXPECT_THAT(
        solver->Update(),
        StatusIs(AnyOf(absl::StatusCode::kInvalidArgument,
                       absl::StatusCode::kUnimplemented),
                 AllOf(HasSubstr("sos1 constraint"),
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
              IsOkAndHolds(IsOptimalWithSolution(2.0, {{x, 0.0}, {y, 1.0}})));
}

// We start with the LP
//
// max  2x + y + 3z
// s.t. 0 <= x, y, z <= 1
//
// We then add the SOS2 constraint
//
// {x, y, z} is SOS-2
//
// The optimal solution for the modified problem is (x*, y*, z*) = (0, 1, 1)
// with objective value 4.
TEST_P(IncrementalLogicalConstraintTest, LinearToSos2Update) {
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 1.0, "x");
  const Variable y = model.AddContinuousVariable(0.0, 1.0, "y");
  const Variable z = model.AddContinuousVariable(0.0, 1.0, "z");
  model.Maximize(2.0 * x + 1.0 * y + 3.0 * z);

  ASSERT_OK_AND_ASSIGN(
      const auto solver,
      NewIncrementalSolver(&model, GetParam().solver_type, {}));
  ASSERT_THAT(
      solver->Solve({.parameters = GetParam().parameters}),
      IsOkAndHolds(IsOptimalWithSolution(6.0, {{x, 1.0}, {y, 1.0}, {z, 1.0}})));

  model.AddSos2Constraint({x, y, z}, {});

  if (!GetParam().supports_sos2) {
    // Here we test that solvers that don't support SOS2 constraints return
    // false in SolverInterface::CanUpdate(). Thus they should fail in their
    // factory function instead of failing in their SolverInterface::Update()
    // function. To assert we rely on status annotations added by
    // IncrementalSolver::Update() to the returned status of Solver::Update()
    // and Solver::New().
    EXPECT_THAT(
        solver->Update(),
        StatusIs(AnyOf(absl::StatusCode::kInvalidArgument,
                       absl::StatusCode::kUnimplemented),
                 AllOf(HasSubstr("sos2 constraint"),
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
  EXPECT_THAT(
      solver->SolveWithoutUpdate({.parameters = GetParam().parameters}),
      IsOkAndHolds(IsOptimalWithSolution(4.0, {{x, 0.0}, {y, 1.0}, {z, 1.0}})));
}

// We start with:
//
// max  x + 3y
// s.t. {2x - 1, 4y - 1} is SOS-1
//      x + y <= 1
//      0 <= x, y <= 1
//
// The  optimal solution is (x*, y*) = (0.25, 0.75) with objective value 2.5.
//
// Then we delete the SOS-1 constraint, leaving the LP:
//
// max  x + 3y
// s.t. x + y <= 1
//      0 <= x, y <= 1
//
// The optimal solution is (x*, y*) = (0, 1) with objective value 3.
TEST_P(IncrementalLogicalConstraintTest, UpdateDeletesSos1Constraint) {
  if (!GetParam().supports_sos1) {
    GTEST_SKIP() << no_sos1_support_message;
  }
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 1.0, "x");
  const Variable y = model.AddContinuousVariable(0.0, 1.0, "y");
  model.Maximize(x + 3 * y);
  model.AddLinearConstraint(x + y <= 1);
  const Sos1Constraint c = model.AddSos1Constraint({2 * x - 1, 4 * y - 3}, {});

  ASSERT_OK_AND_ASSIGN(
      const auto solver,
      NewIncrementalSolver(&model, GetParam().solver_type, {}));
  ASSERT_THAT(solver->Solve({.parameters = GetParam().parameters}),
              IsOkAndHolds(IsOptimalWithSolution(2.5, {{x, 0.25}, {y, 0.75}})));

  model.DeleteSos1Constraint(c);

  ASSERT_THAT(solver->Update(),
              IsOkAndHolds(GetParam().supports_incremental_add_and_deletes
                               ? DidUpdate()
                               : Not(DidUpdate())));
  EXPECT_THAT(solver->SolveWithoutUpdate({.parameters = GetParam().parameters}),
              IsOkAndHolds(IsOptimalWithSolution(3.0, {{x, 0.0}, {y, 1.0}})));
}

// We start with:
//
// max  x + 3y + 2z
// s.t. {2x - 1, 8y - 1, 4z - 1} is SOS-2
//      x + y + z <= 2
//      0 <= x, y, z <= 1
//
// The optimal solution is (x*, y*, z*) = (0.5, 1, 0.5) with objective value
// 4.5.
//
// Then we delete the SOS-2 constraint, leaving the LP:
//
// max  x + 3y + 2z
// s.t. x + y + z <= 2
//      0 <= x, y, z <= 1
//
// The optimal solution is (x*, y*, z*) = (0, 1, 1) with objective value 5.
TEST_P(IncrementalLogicalConstraintTest, UpdateDeletesSos2Constraint) {
  if (!GetParam().supports_sos2) {
    GTEST_SKIP() << no_sos1_support_message;
  }
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 1.0, "x");
  const Variable y = model.AddContinuousVariable(0.0, 1.0, "y");
  const Variable z = model.AddContinuousVariable(0.0, 1.0, "z");
  model.Maximize(x + 3 * y + 2 * z);
  model.AddLinearConstraint(x + y + z <= 2);
  const Sos2Constraint c =
      model.AddSos2Constraint({2 * x - 1, 8 * y - 1, 4 * z - 1}, {});

  ASSERT_OK_AND_ASSIGN(
      const auto solver,
      NewIncrementalSolver(&model, GetParam().solver_type, {}));
  ASSERT_THAT(
      solver->Solve({.parameters = GetParam().parameters}),
      IsOkAndHolds(IsOptimalWithSolution(4.5, {{x, 0.5}, {y, 1.0}, {z, 0.5}})));

  model.DeleteSos2Constraint(c);

  ASSERT_THAT(solver->Update(),
              IsOkAndHolds(GetParam().supports_incremental_add_and_deletes
                               ? DidUpdate()
                               : Not(DidUpdate())));
  EXPECT_THAT(
      solver->SolveWithoutUpdate({.parameters = GetParam().parameters}),
      IsOkAndHolds(IsOptimalWithSolution(5.0, {{x, 0.0}, {y, 1.0}, {z, 1.0}})));
}

// We start with:
//
// max  2x + 2y + z + w
// s.t. {x, y + w, z} is SOS-1
//      0 <= x, y, z, w <= 1
//
// The optimal solution is (x*, y*, z*, w) = (0, 1, 0, 1) with objective value
// 3.
//
// We then delete the y variable, leaving the problem:
//
// max  2x + z + w
// s.t. {x, w, z} is SOS-1
//      0 <= x, z, w <= 1
//
// The optimal solution is (x*, z*, w*) = (1, 0, 0) with objective value 2.
// TODO(b/237076465): With C++ API, also test deletion of single variable term.
TEST_P(IncrementalLogicalConstraintTest,
       UpdateDeletesVariableInSos1Constraint) {
  if (!GetParam().supports_sos1) {
    GTEST_SKIP() << no_sos1_support_message;
  }
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 1.0, "x");
  const Variable y = model.AddContinuousVariable(0.0, 1.0, "y");
  const Variable z = model.AddContinuousVariable(0.0, 1.0, "z");
  const Variable w = model.AddContinuousVariable(0.0, 1.0, "w");
  model.Maximize(2.0 * x + 2.0 * y + z + w);
  model.AddSos1Constraint({x, y + w, z}, {});

  ASSERT_OK_AND_ASSIGN(
      const auto solver,
      NewIncrementalSolver(&model, GetParam().solver_type, {}));
  ASSERT_THAT(solver->Solve({.parameters = GetParam().parameters}),
              IsOkAndHolds(IsOptimalWithSolution(
                  3.0, {{x, 0.0}, {y, 1.0}, {z, 0.0}, {w, 1.0}})));

  model.DeleteVariable(y);

  ASSERT_THAT(solver->Update(),
              IsOkAndHolds(GetParam().supports_incremental_variable_deletions
                               ? DidUpdate()
                               : Not(DidUpdate())));
  EXPECT_THAT(
      solver->SolveWithoutUpdate({.parameters = GetParam().parameters}),
      IsOkAndHolds(IsOptimalWithSolution(2.0, {{x, 1.0}, {z, 0.0}, {w, 0.0}})));
}

// We start with:
//
// max  2x + 2y + 2z + w
// s.t. {x, y, z + w} is SOS-2
//      0 <= x, y, z, w <= 1
//
// The optimal solution is (x*, y*, z*, w*) = (0, 1, 1, 1) with objective value
// 5.
//
// We then delete the z variable, leaving the problem:
//
// max  2x + 2y + w
// s.t. {x, y, w} is SOS-2
//      0 <= x, y, w <= 1
//
// The optimal solution is (x*, y*, w*) = (1, 1, 0) with objective value 4.
// TODO(b/237076465): With C++ API, also test deletion of single variable term.
TEST_P(IncrementalLogicalConstraintTest,
       UpdateDeletesVariableInSos2Constraint) {
  if (!GetParam().supports_sos2) {
    GTEST_SKIP() << no_sos2_support_message;
  }
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 1.0, "x");
  const Variable y = model.AddContinuousVariable(0.0, 1.0, "y");
  const Variable z = model.AddContinuousVariable(0.0, 1.0, "z");
  const Variable w = model.AddContinuousVariable(0.0, 1.0, "w");
  model.Maximize(2.0 * x + 2.0 * y + 2.0 * z + w);
  model.AddSos2Constraint({x, y, z + w}, {});

  ASSERT_OK_AND_ASSIGN(
      const auto solver,
      NewIncrementalSolver(&model, GetParam().solver_type, {}));
  ASSERT_THAT(solver->Solve({.parameters = GetParam().parameters}),
              IsOkAndHolds(IsOptimalWithSolution(
                  5.0, {{x, 0.0}, {y, 1.0}, {z, 1.0}, {w, 1.0}})));

  model.DeleteVariable(z);

  ASSERT_THAT(solver->Update(),
              IsOkAndHolds(GetParam().supports_incremental_variable_deletions
                               ? DidUpdate()
                               : Not(DidUpdate())));
  EXPECT_THAT(
      solver->SolveWithoutUpdate({.parameters = GetParam().parameters}),
      IsOkAndHolds(IsOptimalWithSolution(4.0, {{x, 1.0}, {y, 1.0}, {w, 0.0}})));
}

// We start with:
//
// max  2x + 1.5y + 3z
// s.t. {y, z} is SOS-1
//      {x, y, z} is SOS-2
//      0 <= x, y, z <= 1
//
// The optimal solution is (x*, y*, z*) = (1, 1, 0) with objective value 3.5.
//
// We then delete the SOS-1 constraint, leaving:
//
// max  x + 1.5y + 3z
// s.t. {x, y, z} is SOS-2
//      0 <= x, y, z <= 1
//
// The optimal solution is (x*, y*, z*) = (0, 1, 1) with objective value 4.5.
TEST_P(IncrementalLogicalConstraintTest, InstanceWithSos1AndSos2AndDeletion) {
  if (!GetParam().supports_sos1) {
    GTEST_SKIP() << no_sos1_support_message;
  }
  if (!GetParam().supports_sos2) {
    GTEST_SKIP() << no_sos2_support_message;
  }
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 1.0, "x");
  const Variable y = model.AddContinuousVariable(0.0, 1.0, "y");
  const Variable z = model.AddContinuousVariable(0.0, 1.0, "z");
  model.Maximize(2.0 * x + 1.5 * y + 3.0 * z);
  const Sos1Constraint c = model.AddSos1Constraint({y, z}, {});
  model.AddSos2Constraint({x, y, z}, {});

  ASSERT_OK_AND_ASSIGN(
      const auto solver,
      NewIncrementalSolver(&model, GetParam().solver_type, {}));
  ASSERT_THAT(
      solver->Solve({.parameters = GetParam().parameters}),
      IsOkAndHolds(IsOptimalWithSolution(3.5, {{x, 1.0}, {y, 1.0}, {z, 0.0}})));

  model.DeleteSos1Constraint(c);

  ASSERT_THAT(solver->Update(),
              IsOkAndHolds(GetParam().supports_incremental_add_and_deletes
                               ? DidUpdate()
                               : Not(DidUpdate())));
  EXPECT_THAT(
      solver->SolveWithoutUpdate({.parameters = GetParam().parameters}),
      IsOkAndHolds(IsOptimalWithSolution(4.5, {{x, 0.0}, {y, 1.0}, {z, 1.0}})));
}

TEST_P(SimpleLogicalConstraintTest, CanBuildIndicatorModel) {
  Model model;
  // Technically `x` should be binary, but the validator will not enforce this.
  // Instead, we expect that solvers will reject solving any models containing
  // non-binary indicator variables (this is tested elsewhere). Therefore, here
  // we want to test that solvers that do not support either indicator
  // constraints or integer variables will reject indicator constraints with a
  // useful message, regardless if the indicator is binary.
  const Variable x = model.AddContinuousVariable(0.0, 1.0, "x");
  const Variable y = model.AddContinuousVariable(0.0, 1.0, "y");
  model.AddIndicatorConstraint(x, y <= 0.5);

  if (GetParam().supports_indicator_constraints) {
    EXPECT_OK(NewIncrementalSolver(&model, GetParam().solver_type, {}));
  } else {
    EXPECT_THAT(NewIncrementalSolver(&model, GetParam().solver_type, {}),
                StatusIs(AnyOf(absl::StatusCode::kInvalidArgument,
                               absl::StatusCode::kUnimplemented),
                         HasSubstr("indicator constraints")));
  }
}

// Here we test that each solver supporting indicator constraints will raise an
// error when attempting to solve a model containing non-binary indicator
// variables.
TEST_P(SimpleLogicalConstraintTest, SolveFailsWithNonBinaryIndicatorVariable) {
  if (!GetParam().supports_indicator_constraints) {
    GTEST_SKIP() << no_indicator_support_message;
  }
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 1.0, "x");
  const Variable y = model.AddContinuousVariable(0.0, 1.0, "y");
  model.AddIndicatorConstraint(x, y >= 0.5);

  EXPECT_THAT(SimpleSolve(model),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("indicator variable is not binary")));
}

// We solve
//
// min  -x + y
// s.t. x = 1 --> y >= 0.5
//      x in {0,1}
//      0 <= y <= 1
//
// The optimal solution is (x*, y*) = (1, 0.5) with objective value -0.5.
TEST_P(SimpleLogicalConstraintTest, SimpleIndicatorInstance) {
  if (!GetParam().supports_indicator_constraints) {
    GTEST_SKIP() << no_indicator_support_message;
  }
  Model model;
  const Variable x = model.AddBinaryVariable("x");
  const Variable y = model.AddContinuousVariable(0.0, 1.0, "y");
  model.Minimize(-x + y);
  model.AddIndicatorConstraint(x, y >= 0.5);

  EXPECT_THAT(SimpleSolve(model),
              IsOkAndHolds(IsOptimalWithSolution(-0.5, {{x, 1.0}, {y, 0.5}})));
}

// We solve
//
// min  x + y
// s.t. x = 0 --> y >= 0.5
//      x in {0,1}
//      0 <= y <= 1
//
// The optimal solution is (x*, y*) = (0, 0.5) with objective value 0.5.
TEST_P(SimpleLogicalConstraintTest, ActivationOnZero) {
  if (!GetParam().supports_indicator_constraints) {
    GTEST_SKIP() << no_indicator_support_message;
  }
  Model model;
  const Variable x = model.AddBinaryVariable("x");
  const Variable y = model.AddContinuousVariable(0.0, 1.0, "y");
  model.Minimize(x + y);
  model.AddIndicatorConstraint(x, y >= 0.5, /*activate_on_zero=*/true, "c");

  EXPECT_THAT(SimpleSolve(model),
              IsOkAndHolds(IsOptimalWithSolution(0.5, {{x, 0.0}, {y, 0.5}})));
}

// As of 2022-08-30, ModelProto supports indicator constraints with ranged
// implied constraints, although no solver supports this functionality. If a
// solver does add support in the future, this test should be updated and the
// test parameters should be suitably modified to track this support.
TEST_P(SimpleLogicalConstraintTest, IndicatorWithRangedImpliedConstraint) {
  if (!GetParam().supports_indicator_constraints) {
    GTEST_SKIP() << no_indicator_support_message;
  }
  Model model;
  const Variable x = model.AddBinaryVariable("x");
  const Variable y = model.AddContinuousVariable(0.0, 1.0, "y");
  model.AddIndicatorConstraint(x, 0.25 <= y <= 0.75);

  EXPECT_THAT(NewIncrementalSolver(&model, GetParam().solver_type, {}),
              StatusIs(AnyOf(absl::StatusCode::kInvalidArgument,
                             absl::StatusCode::kUnimplemented),
                       HasSubstr("ranged")));
}

// We write the model:
//
// max  x
// s.t. (unset variable id) = 1 --> x = 0.5
//      0 <= x <= 1
//
// As the indicator variable is unset, the indicator constraint should be
// ignored, and the optimal solution is x* = 1 with objective value 1.
//
// To get an unset indicator variable, we simply add an indicator variable, add
// the constraint, and then delete the indicator variable.
TEST_P(SimpleLogicalConstraintTest, UnsetIndicatorVariable) {
  if (!GetParam().supports_indicator_constraints) {
    GTEST_SKIP() << no_indicator_support_message;
  }
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 1.0, "x");
  const Variable indicator = model.AddBinaryVariable("indicator");
  model.Maximize(x);
  model.AddIndicatorConstraint(indicator, x == 0.5);
  model.DeleteVariable(indicator);

  EXPECT_THAT(SimpleSolve(model),
              IsOkAndHolds(IsOptimalWithSolution(1.0, {{x, 1.0}})));
}

// We test that indicator variables may have custom bounds set as long as the
// variables are integer and those bounds are contained in [0, 1]. The model is
//
// max  v + w
// s.t. x = 1 --> w >= 1.5
//      y = 1 --> v <= 0.6
//      z = 1 --> w <= 0.4
//      x == 0
//      y == 1
//      0.5 <= z <= 1
//      0 <= v, w <= 1
//      x, y, z in {0, 1}.
//
// The unique optimal solution is (x, y, z, v, w) = (0, 1, 1, 0.6, 0.4) with
// objective value 1.0.
TEST_P(SimpleLogicalConstraintTest, IndicatorsWithOddButValidBounds) {
  if (!GetParam().supports_indicator_constraints) {
    GTEST_SKIP() << no_indicator_support_message;
  }
  Model model;
  const Variable x = model.AddIntegerVariable(0.0, 0.0, "x");
  const Variable y = model.AddIntegerVariable(1.0, 1.0, "y");
  const Variable z = model.AddIntegerVariable(0.5, 1.0, "z");
  const Variable v = model.AddContinuousVariable(0.0, 1.0, "v");
  const Variable w = model.AddContinuousVariable(0.0, 1.0, "w");
  model.Maximize(v + w);
  model.AddIndicatorConstraint(x, w >= 1.5);
  model.AddIndicatorConstraint(y, v <= 0.6);
  model.AddIndicatorConstraint(z, w <= 0.4);

  EXPECT_THAT(SimpleSolve(model),
              IsOkAndHolds(IsOptimalWithSolution(
                  1.0, {{x, 0.0}, {y, 1.0}, {z, 1.0}, {v, 0.6}, {w, 0.4}})));
}

// We start with the LP
//
// max  x + y
// s.t. x in {0,1}
//      0 <= y <= 1
//
// The optimal solution is (x*, y*) = (1, 1) with objective value 2.
//
// We then add the indicator constraint
//
// x = 1 --> y <= 0.5
//
// The optimal solution for the modified problem is (x*, y*) = (1, 0.5) with
// objective value 1.5.
TEST_P(IncrementalLogicalConstraintTest, LinearToIndicatorUpdate) {
  Model model;
  // We want to test that, even for solvers that do not support either integer
  // variables or indicator constraints, that we get a meaningful error message.
  const Variable x = GetParam().supports_integer_variables
                         ? model.AddBinaryVariable("x")
                         : model.AddContinuousVariable(0.0, 1.0, "x");
  const Variable y = model.AddContinuousVariable(0.0, 1.0, "y");
  model.Maximize(x + y);

  ASSERT_OK_AND_ASSIGN(
      const auto solver,
      NewIncrementalSolver(&model, GetParam().solver_type, {}));
  ASSERT_THAT(solver->Solve({.parameters = GetParam().parameters}),
              IsOkAndHolds(IsOptimalWithSolution(2.0, {{x, 1.0}, {y, 1.0}})));

  model.AddIndicatorConstraint(x, y <= 0.5);

  if (!GetParam().supports_indicator_constraints) {
    // Here we test that solvers that don't support indicator constraints return
    // false in SolverInterface::CanUpdate(). Thus they should fail in their
    // factory function instead of failing in their SolverInterface::Update()
    // function. To assert we rely on status annotations added by
    // IncrementalSolver::Update() to the returned status of Solver::Update()
    // and Solver::New().
    EXPECT_THAT(
        solver->Update(),
        StatusIs(AnyOf(absl::StatusCode::kInvalidArgument,
                       absl::StatusCode::kUnimplemented),
                 AllOf(HasSubstr("indicator constraint"),
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
              IsOkAndHolds(IsOptimalWithSolution(1.5, {{x, 1.0}, {y, 0.5}})));
}

// We start with the problem:
//
// max  x + y
// s.t. x = 1 --> y <= 0.5
//      x in {0,1}
//      0 <= y <= 1
//
// The optimal solution is (x*, y*) = (1, 0.5) with objective value 1.5.
//
// We then delete the indicator constraint, leaving the LP:
//
// max  x + y
// s.t. x in {0,1}
//      0 <= y <= 1
//
// The optimal solution for the modified problem is (x*, y*) = (1, 1) with
// objective value 2.
TEST_P(IncrementalLogicalConstraintTest, UpdateDeletesIndicatorConstraint) {
  if (!GetParam().supports_indicator_constraints) {
    GTEST_SKIP() << no_indicator_support_message;
  }
  Model model;
  const Variable x = model.AddBinaryVariable("x");
  const Variable y = model.AddContinuousVariable(0.0, 1.0, "y");
  model.Maximize(x + y);
  const IndicatorConstraint c = model.AddIndicatorConstraint(x, y <= 0.5);
  ASSERT_OK_AND_ASSIGN(
      const auto solver,
      NewIncrementalSolver(&model, GetParam().solver_type, {}));
  ASSERT_THAT(solver->Solve({.parameters = GetParam().parameters}),
              IsOkAndHolds(IsOptimalWithSolution(1.5, {{x, 1.0}, {y, 0.5}})));

  model.DeleteIndicatorConstraint(c);

  ASSERT_THAT(solver->Update(),
              IsOkAndHolds(GetParam().supports_incremental_add_and_deletes
                               ? DidUpdate()
                               : Not(DidUpdate())));
  EXPECT_THAT(solver->SolveWithoutUpdate({.parameters = GetParam().parameters}),
              IsOkAndHolds(IsOptimalWithSolution(2.0, {{x, 1.0}, {y, 1.0}})));
}

// We start with the problem:
//
// max  x
// s.t. (unset variable id) = 1 --> x <= 0.5
//      0 <= x <= 1
//
// The optimal solution is x* = 1 with objective value 1. To write this model,
// we add a placeholder indicator variable, add the indicator constraint, delete
// that constraint, and only then initialize the solver.
//
// We then delete the indicator constraint, leaving the LP:
//
// max  x
// s.t. 0 <= x <= 1
//
// The optimal solution for the modified problem is also x* = 1 with objective
// value 1.
TEST_P(IncrementalLogicalConstraintTest,
       UpdateDeletesIndicatorConstraintWithUnsetIndicatorVariable) {
  if (!GetParam().supports_indicator_constraints) {
    GTEST_SKIP() << no_indicator_support_message;
  }
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 1.0, "x");
  const Variable indicator = model.AddBinaryVariable("indicator");
  model.Maximize(x);
  const IndicatorConstraint c =
      model.AddIndicatorConstraint(indicator, x <= 0.5);
  model.DeleteVariable(indicator);

  ASSERT_OK_AND_ASSIGN(
      const auto solver,
      NewIncrementalSolver(&model, GetParam().solver_type, {}));
  ASSERT_THAT(solver->Solve({.parameters = GetParam().parameters}),
              IsOkAndHolds(IsOptimalWithSolution(1.0, {{x, 1.0}})));

  model.DeleteIndicatorConstraint(c);

  ASSERT_THAT(solver->Update(),
              IsOkAndHolds(GetParam().supports_incremental_add_and_deletes
                               ? DidUpdate()
                               : Not(DidUpdate())));
  EXPECT_THAT(solver->SolveWithoutUpdate({.parameters = GetParam().parameters}),
              IsOkAndHolds(IsOptimalWithSolution(1.0, {{x, 1.0}})));
}

// We start with the problem:
//
// max  x + y
// s.t. x = 1 --> y <= 0.5
//      x in {0,1}
//      0 <= y <= 1
//
// The optimal solution is (x*, y*) = (1, 0.5) with objective value 1.5.
//
// We then delete the indicator variable x. If the solver supports this form of
// update, we then solve the problem:
//
// max  y
// s.t. 0 <= y <= 1
//
// The optimal solution is y* = 1 with objective value 1.
TEST_P(IncrementalLogicalConstraintTest, UpdateDeletesIndicatorVariable) {
  if (!GetParam().supports_indicator_constraints) {
    GTEST_SKIP() << no_indicator_support_message;
  }
  Model model;
  const Variable x = model.AddBinaryVariable("x");
  const Variable y = model.AddContinuousVariable(0.0, 1.0, "y");
  model.Maximize(x + y);
  model.AddIndicatorConstraint(x, y <= 0.5);

  ASSERT_OK_AND_ASSIGN(
      const auto solver,
      NewIncrementalSolver(&model, GetParam().solver_type, {}));
  ASSERT_THAT(solver->Solve({.parameters = GetParam().parameters}),
              IsOkAndHolds(IsOptimalWithSolution(1.5, {{x, 1.0}, {y, 0.5}})));

  model.DeleteVariable(x);

  ASSERT_THAT(solver->Update(),
              IsOkAndHolds(GetParam().supports_deleting_indicator_variables
                               ? DidUpdate()
                               : Not(DidUpdate())));
  EXPECT_THAT(solver->SolveWithoutUpdate({.parameters = GetParam().parameters}),
              IsOkAndHolds(IsOptimalWithSolution(1.0, {{y, 1.0}})));
}

// We start with the problem:
//
// max  x + 2y + z
// s.t. x = 1 --> y <= 0.5
//      x = 1 --> z <= 0.5
//      x in {0,1}
//      0 <= y, z <= 1
//
// The optimal solution is (x*, y*, z*) = (0, 1, 1) with objective value 3.
//
// We then delete the variable y, leaving the problem:
//
// max  x + z
// s.t. x = 1 --> 0 <= 0.5
//      x = 1 --> z <= 0.5
//      x in {0,1}
//      0 <= z <= 1
//
// The optimal solution for the modified problem is (x*, z*) = (1, 0.5) with
// objective value 1.5.
TEST_P(IncrementalLogicalConstraintTest,
       UpdateDeletesVariableInImpliedExpression) {
  if (!GetParam().supports_indicator_constraints) {
    GTEST_SKIP() << no_indicator_support_message;
  }
  Model model;
  const Variable x = model.AddBinaryVariable("x");
  const Variable y = model.AddContinuousVariable(0.0, 1.0, "y");
  const Variable z = model.AddContinuousVariable(0.0, 1.0, "z");
  model.Maximize(x + 2.0 * y + z);
  model.AddIndicatorConstraint(x, y <= 0.5);
  model.AddIndicatorConstraint(x, z <= 0.5);

  ASSERT_OK_AND_ASSIGN(
      const auto solver,
      NewIncrementalSolver(&model, GetParam().solver_type, {}));
  ASSERT_THAT(
      solver->Solve({.parameters = GetParam().parameters}),
      IsOkAndHolds(IsOptimalWithSolution(3.0, {{x, 0.0}, {y, 1.0}, {z, 1.0}})));

  model.DeleteVariable(y);

  ASSERT_THAT(solver->Update(),
              IsOkAndHolds(GetParam().supports_incremental_variable_deletions
                               ? DidUpdate()
                               : Not(DidUpdate())));
  EXPECT_THAT(solver->SolveWithoutUpdate({.parameters = GetParam().parameters}),
              IsOkAndHolds(IsOptimalWithSolution(1.5, {{x, 1.0}, {z, 0.5}})));
}

// We start with a simple, valid indicator constraint with binary indicator
// variable. We then update the indicator variable to be continuous. The solver
// should permit the model update, but return an error when solving.
TEST_P(IncrementalLogicalConstraintTest,
       UpdateMakesIndicatorVariableTypeInvalid) {
  if (!GetParam().supports_indicator_constraints) {
    GTEST_SKIP() << no_indicator_support_message;
  }
  Model model;
  const Variable x = model.AddBinaryVariable("x");
  const Variable y = model.AddContinuousVariable(0.0, 1.0, "y");
  model.AddIndicatorConstraint(x, y <= 0.5);

  ASSERT_OK_AND_ASSIGN(
      const auto solver,
      NewIncrementalSolver(&model, GetParam().solver_type, {}));
  ASSERT_OK(solver->Solve({.parameters = GetParam().parameters}));

  model.set_continuous(x);

  ASSERT_THAT(solver->Update(),
              IsOkAndHolds(GetParam().supports_updating_binary_variables
                               ? DidUpdate()
                               : Not(DidUpdate())));
  EXPECT_THAT(solver->SolveWithoutUpdate({.parameters = GetParam().parameters}),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("indicator variable is not binary")));
}

// We test that we can update indicator variable bounds as long as they are
// still contained in [0, 1]. The model is:
//
// max  u + v
// s.t. x + y == 1
//      x = 1 --> u <= 0.6
//      y = 1 --> v <= 0.4
//      x, y in {0, 1}
//      0 <= u, v <= 1
//
// The optimal solution is (x, y, u, v) = (1, 0, 0.6, 1.0) with objective value
// 1.6.
//
// If we update bounds to x == 0, the optimal solution is then (x, y, u, v) =
// (0, 1, 1, 0.4) with objective value 1.4.
//
// Alternatively, if we update bounds to 0.5 <= x <= 1 and 0 <= y <= 0.5, the
// optimal solution is then (x, y, u, v) = (1, 0, 0.6, 1.0) with objective
// value 1.6.
//
// Alternatively, if we update bounds to y == 1, the optimal solution is then
// (x, y, u, v) = (0, 1, 1, 0.4) with objective value 1.4.
TEST_P(IncrementalLogicalConstraintTest, UpdateChangesIndicatorVariableBound) {
  if (!GetParam().supports_indicator_constraints) {
    GTEST_SKIP() << no_indicator_support_message;
  }
  Model model;
  const Variable x = model.AddIntegerVariable(0.0, 1.0, "x");
  const Variable y = model.AddIntegerVariable(0.0, 1.0, "y");
  const Variable u = model.AddContinuousVariable(0.0, 1.0, "u");
  const Variable v = model.AddContinuousVariable(0.0, 1.0, "z");
  model.Maximize(u + v);
  model.AddLinearConstraint(x + y == 1.0);
  model.AddIndicatorConstraint(x, u <= 0.6);
  model.AddIndicatorConstraint(y, v <= 0.4);

  ASSERT_OK_AND_ASSIGN(
      const auto solver,
      NewIncrementalSolver(&model, GetParam().solver_type, {}));
  EXPECT_THAT(solver->Solve({.parameters = GetParam().parameters}),
              IsOkAndHolds(IsOptimalWithSolution(
                  1.6, {{x, 1.0}, {y, 0.0}, {u, 0.6}, {v, 1.0}})));

  model.set_lower_bound(x, 0.0);
  model.set_upper_bound(x, 0.0);

  ASSERT_THAT(solver->Update(),
              IsOkAndHolds(GetParam().supports_updating_binary_variables
                               ? DidUpdate()
                               : Not(DidUpdate())));
  EXPECT_THAT(solver->SolveWithoutUpdate({.parameters = GetParam().parameters}),
              IsOkAndHolds(IsOptimalWithSolution(
                  1.4, {{x, 0.0}, {y, 1.0}, {u, 1.0}, {v, 0.4}})));

  model.set_lower_bound(x, 0.5);
  model.set_upper_bound(x, 1.0);
  model.set_lower_bound(y, 0.0);
  model.set_upper_bound(y, 0.5);

  ASSERT_THAT(solver->Update(),
              IsOkAndHolds(GetParam().supports_updating_binary_variables
                               ? DidUpdate()
                               : Not(DidUpdate())));
  EXPECT_THAT(solver->SolveWithoutUpdate({.parameters = GetParam().parameters}),
              IsOkAndHolds(IsOptimalWithSolution(
                  1.6, {{x, 1.0}, {y, 0.0}, {u, 0.6}, {v, 1.0}})));

  model.set_lower_bound(x, 0.0);
  model.set_upper_bound(x, 1.0);
  model.set_lower_bound(y, 1.0);
  model.set_upper_bound(y, 1.0);

  ASSERT_THAT(solver->Update(),
              IsOkAndHolds(GetParam().supports_updating_binary_variables
                               ? DidUpdate()
                               : Not(DidUpdate())));
  EXPECT_THAT(solver->SolveWithoutUpdate({.parameters = GetParam().parameters}),
              IsOkAndHolds(IsOptimalWithSolution(
                  1.4, {{x, 0.0}, {y, 1.0}, {u, 1.0}, {v, 0.4}})));
}

// We start with a simple, valid indicator constraint with binary indicator
// variable. We then update the indicator variable to have a larger upper bound,
// meaning it is integer but no longer binary. The solver should permit the
// model update, but return an error when solving.
TEST_P(IncrementalLogicalConstraintTest,
       UpdateMakesIndicatorVariableBoundsInvalid) {
  if (!GetParam().supports_indicator_constraints) {
    GTEST_SKIP() << no_indicator_support_message;
  }
  Model model;
  const Variable x = model.AddIntegerVariable(0.0, 1.0, "x");
  const Variable y = model.AddContinuousVariable(0.0, 1.0, "y");
  model.AddIndicatorConstraint(x, y <= 0.5);

  ASSERT_OK_AND_ASSIGN(
      const auto solver,
      NewIncrementalSolver(&model, GetParam().solver_type, {}));
  ASSERT_OK(solver->Solve({.parameters = GetParam().parameters}));

  model.set_upper_bound(x, 2.0);

  ASSERT_THAT(solver->Update(),
              IsOkAndHolds(GetParam().supports_updating_binary_variables
                               ? DidUpdate()
                               : Not(DidUpdate())));
  EXPECT_THAT(solver->SolveWithoutUpdate({.parameters = GetParam().parameters}),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("indicator variable is not binary")));
}

}  // namespace
}  // namespace operations_research::math_opt
