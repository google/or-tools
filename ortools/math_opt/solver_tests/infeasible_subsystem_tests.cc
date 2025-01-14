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

#include "ortools/math_opt/solver_tests/infeasible_subsystem_tests.h"

#include <limits>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_join.h"
#include "absl/time/time.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/gurobi/gurobi_stdout_matchers.h"
#include "ortools/math_opt/cpp/matchers.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/math_opt/infeasible_subsystem.pb.h"
#include "ortools/port/scoped_std_stream_capture.h"

namespace operations_research::math_opt {

using ::testing::AnyOf;
using ::testing::HasSubstr;
using ::testing::status::IsOkAndHolds;
using ::testing::status::StatusIs;

constexpr double kInf = std::numeric_limits<double>::infinity();

std::ostream& operator<<(std::ostream& ostr,
                         const InfeasibleSubsystemSupport& support_menu) {
  ostr << "{ infeasible_subsystem_support: "
       << (support_menu.supports_infeasible_subsystems ? "true" : "false")
       << " }";
  return ostr;
}

std::ostream& operator<<(std::ostream& ostr,
                         const InfeasibleSubsystemTestParameters& params) {
  ostr << "{ solver_type: " << EnumToString(params.solver_type);
  ostr << ", support_menu: " << params.support_menu << " }";
  return ostr;
}

TEST_P(InfeasibleSubsystemTest, CanComputeInfeasibleSubsystem) {
  Model model;
  if (GetParam().support_menu.supports_infeasible_subsystems) {
    EXPECT_THAT(ComputeInfeasibleSubsystem(model, GetParam().solver_type),
                IsOkAndHolds(IsUndetermined()));
  } else {
    EXPECT_THAT(ComputeInfeasibleSubsystem(model, GetParam().solver_type),
                StatusIs(AnyOf(absl::StatusCode::kInvalidArgument,
                               absl::StatusCode::kUnimplemented),
                         HasSubstr("infeasible subsystem")));
  }
}

// The model is:
// min  0
// s.t. 1 ≤ x ≤ 0 (variable bounds)
//
// The entire model is an IIS.
TEST_P(InfeasibleSubsystemTest, InvertedVariableBounds) {
  if (!GetParam().support_menu.supports_infeasible_subsystems) {
    return;
  }
  Model model;
  const Variable x = model.AddContinuousVariable(1.0, 0.0);
  EXPECT_THAT(ComputeInfeasibleSubsystem(model, GetParam().solver_type),
              IsOkAndHolds(IsInfeasible(
                  /*expected_is_minimal=*/true,
                  ModelSubset{.variable_bounds = {
                                  {x, ModelSubset::Bounds{.lower = true,
                                                          .upper = true}}}})));
}

// The model is:
// min  0
// s.t. 0.2 ≤ x ≤ 0.8 (variable bounds)
//      x is integer
//
// The entire model is an IIS.
TEST_P(InfeasibleSubsystemTest, IntegerVariableWithInfeasibleBounds) {
  if (!GetParam().support_menu.supports_infeasible_subsystems) {
    return;
  }
  Model model;
  const Variable x = model.AddIntegerVariable(0.2, 0.8);
  EXPECT_THAT(
      ComputeInfeasibleSubsystem(model, GetParam().solver_type),
      IsOkAndHolds(IsInfeasible(
          /*expected_is_minimal=*/true,
          ModelSubset{
              .variable_bounds = {{x, ModelSubset::Bounds{.lower = true,
                                                          .upper = true}}},
              .variable_integrality = {x}})));
}

// The model is:
// min  0
// s.t. -∞ ≤ 1 ≤ 0
//
// An IIS is:
//           1 ≤ 0
TEST_P(InfeasibleSubsystemTest, InconsistentLessThanLinearConstraint) {
  if (!GetParam().support_menu.supports_infeasible_subsystems) {
    return;
  }
  Model model;
  const LinearConstraint c =
      model.AddLinearConstraint(BoundedLinearExpression(1.0, -kInf, 0.0));
  EXPECT_THAT(ComputeInfeasibleSubsystem(model, GetParam().solver_type),
              IsOkAndHolds(IsInfeasible(
                  /*expected_is_minimal=*/true,
                  ModelSubset{.linear_constraints = {
                                  {c, ModelSubset::Bounds{.lower = false,
                                                          .upper = true}}}})));
}

// The model is:
// min  0
// s.t. 1 ≤ 0 ≤ ∞
//
// An IIS is:
//      1 ≤ 0
TEST_P(InfeasibleSubsystemTest, InconsistentGreaterThanLinearConstraint) {
  if (!GetParam().support_menu.supports_infeasible_subsystems) {
    return;
  }
  Model model;
  const LinearConstraint c =
      model.AddLinearConstraint(BoundedLinearExpression(0.0, 1.0, kInf));
  EXPECT_THAT(ComputeInfeasibleSubsystem(model, GetParam().solver_type),
              IsOkAndHolds(IsInfeasible(
                  /*expected_is_minimal=*/true,
                  ModelSubset{.linear_constraints = {
                                  {c, ModelSubset::Bounds{.lower = true,
                                                          .upper = false}}}})));
}

// The model is:
// min  0
// s.t. 1 == 0
//
// The entire model is an IIS.
TEST_P(InfeasibleSubsystemTest, InconsistentEqualityLinearConstraint) {
  if (!GetParam().support_menu.supports_infeasible_subsystems) {
    return;
  }
  Model model;
  const LinearConstraint c =
      model.AddLinearConstraint(BoundedLinearExpression(1.0, 0.0, 0.0));
  EXPECT_THAT(ComputeInfeasibleSubsystem(model, GetParam().solver_type),
              IsOkAndHolds(IsInfeasible(
                  /*expected_is_minimal=*/true,
                  ModelSubset{.linear_constraints = {
                                  {c, ModelSubset::Bounds{.lower = true,
                                                          .upper = true}}}})));
}

// The model is:
// min  0
// s.t. 0 ≤ 2 ≤ 1
//
// The entire model is an IIS.
TEST_P(InfeasibleSubsystemTest, InconsistentRangedLinearConstraint) {
  if (!GetParam().support_menu.supports_infeasible_subsystems) {
    return;
  }
  Model model;
  const LinearConstraint c =
      model.AddLinearConstraint(BoundedLinearExpression(2.0, 0.0, 1.0));
  EXPECT_THAT(ComputeInfeasibleSubsystem(model, GetParam().solver_type),
              IsOkAndHolds(IsInfeasible(
                  /*expected_is_minimal=*/true,
                  ModelSubset{.linear_constraints = {
                                  {c, ModelSubset::Bounds{.lower = true,
                                                          .upper = true}}}})));
}

// The model is:
// min  0
// s.t. x ≥ 1 (linear constraint)
//      -∞ ≤ x ≤ 0 (variable bounds)
//
// The entire model is an IIS.
TEST_P(InfeasibleSubsystemTest, InconsistentVariableBoundsAndLinearConstraint) {
  if (!GetParam().support_menu.supports_infeasible_subsystems) {
    return;
  }
  Model model;
  const Variable x = model.AddContinuousVariable(-kInf, 0.0);
  const LinearConstraint c = model.AddLinearConstraint(x >= 1.0);
  EXPECT_THAT(
      ComputeInfeasibleSubsystem(model, GetParam().solver_type),
      IsOkAndHolds(IsInfeasible(
          /*expected_is_minimal=*/true,
          ModelSubset{
              .variable_bounds = {{x, ModelSubset::Bounds{.lower = false,
                                                          .upper = true}}},
              .linear_constraints = {
                  {c, ModelSubset::Bounds{.lower = true, .upper = false}}}})));
}

// The model is:
// min  0
// s.t. -∞ ≤ 1 ≤ 0 (quadratic constraint)
//
// An IIS is:
//           1 ≤ 0
TEST_P(InfeasibleSubsystemTest, InconsistentLessThanQuadraticConstraint) {
  if (!GetParam().support_menu.supports_infeasible_subsystems) {
    return;
  }
  Model model;
  const QuadraticConstraint c =
      model.AddQuadraticConstraint(BoundedQuadraticExpression(1.0, -kInf, 0.0));
  EXPECT_THAT(ComputeInfeasibleSubsystem(model, GetParam().solver_type),
              IsOkAndHolds(IsInfeasible(
                  /*expected_is_minimal=*/true,
                  ModelSubset{.quadratic_constraints = {
                                  {c, ModelSubset::Bounds{.lower = false,
                                                          .upper = true}}}})));
}

// The model is:
// min  0
// s.t. 1 ≤ 0 ≤ ∞ (quadratic constraint)
//
// An IIS is:
//      1 ≤ 0
TEST_P(InfeasibleSubsystemTest, InconsistentGreaterThanQuadraticConstraint) {
  if (!GetParam().support_menu.supports_infeasible_subsystems) {
    return;
  }
  Model model;
  const QuadraticConstraint c =
      model.AddQuadraticConstraint(BoundedQuadraticExpression(0.0, 1.0, kInf));
  EXPECT_THAT(ComputeInfeasibleSubsystem(model, GetParam().solver_type),
              IsOkAndHolds(IsInfeasible(
                  /*expected_is_minimal=*/true,
                  ModelSubset{.quadratic_constraints = {
                                  {c, ModelSubset::Bounds{.lower = true,
                                                          .upper = false}}}})));
}

// The model is:
// min  0
// s.t. 1 == 0 (quadratic constraint)
//
// The entire model is an IIS.
TEST_P(InfeasibleSubsystemTest, InconsistentEqualityQuadraticConstraint) {
  if (!GetParam().support_menu.supports_infeasible_subsystems) {
    return;
  }
  Model model;
  const QuadraticConstraint c =
      model.AddQuadraticConstraint(BoundedQuadraticExpression(1.0, 0.0, 0.0));
  EXPECT_THAT(ComputeInfeasibleSubsystem(model, GetParam().solver_type),
              IsOkAndHolds(IsInfeasible(
                  /*expected_is_minimal=*/true,
                  ModelSubset{.quadratic_constraints = {
                                  {c, ModelSubset::Bounds{.lower = true,
                                                          .upper = true}}}})));
}

// TODO(b/227217735): Test ranged quadratic constraints when supported.

// The model is:
// min  0
// s.t. x² ≥ 1
//      -0.5 ≤ x ≤ 0.5 (variable bounds)
//
// The entire model is an IIS.
TEST_P(InfeasibleSubsystemTest,
       InconsistentVariableBoundsAndQuadraticConstraint) {
  if (!GetParam().support_menu.supports_infeasible_subsystems) {
    return;
  }
  Model model;
  const Variable x = model.AddContinuousVariable(-0.5, 0.5);
  const QuadraticConstraint c = model.AddQuadraticConstraint(x * x >= 1.0);
  EXPECT_THAT(
      ComputeInfeasibleSubsystem(model, GetParam().solver_type),
      IsOkAndHolds(IsInfeasible(
          /*expected_is_minimal=*/true,
          ModelSubset{
              .variable_bounds = {{x, ModelSubset::Bounds{.lower = true,
                                                          .upper = true}}},
              .quadratic_constraints = {
                  {c, ModelSubset::Bounds{.lower = true, .upper = false}}}})));
}

// The model is:
// min  0
// s.t. ||{x}||₂ ≤ 1
//      2 ≤ x ≤ 2 (variable bounds)
//
// An IIS is:
//      ||{x}||₂ ≤ 1
//      2 ≤ x
TEST_P(InfeasibleSubsystemTest, InconsistentSecondOrderConeConstraint) {
  if (!GetParam().support_menu.supports_infeasible_subsystems) {
    return;
  }
  Model model;
  const Variable x = model.AddContinuousVariable(2.0, 2.0);
  const SecondOrderConeConstraint c =
      model.AddSecondOrderConeConstraint({x}, 1.0);
  EXPECT_THAT(
      ComputeInfeasibleSubsystem(model, GetParam().solver_type),
      IsOkAndHolds(IsInfeasible(
          /*expected_is_minimal=*/true,
          ModelSubset{
              .variable_bounds = {{x, ModelSubset::Bounds{.lower = true,
                                                          .upper = false}}},
              .second_order_cone_constraints = {c}})));
}

// The model is:
// min  0
// s.t. ||{2x}||₂ ≤ 1
//      1 ≤ x ≤ 1 (variable bounds)
//
// An IIS is:
//      ||{2x}||₂ ≤ 1
//      1 ≤ x
TEST_P(InfeasibleSubsystemTest,
       InconsistentSecondOrderConeConstraintWithExpressionUnderNorm) {
  if (!GetParam().support_menu.supports_infeasible_subsystems) {
    return;
  }
  Model model;
  const Variable x = model.AddContinuousVariable(1.0, 1.0);
  const SecondOrderConeConstraint c =
      model.AddSecondOrderConeConstraint({2.0 * x}, 1.0);
  EXPECT_THAT(
      ComputeInfeasibleSubsystem(model, GetParam().solver_type),
      IsOkAndHolds(IsInfeasible(
          /*expected_is_minimal=*/true,
          ModelSubset{
              .variable_bounds = {{x, ModelSubset::Bounds{.lower = true,
                                                          .upper = false}}},
              .second_order_cone_constraints = {c}})));
}

// The model is:
// min  0
// s.t. ||{x}||₂ ≤ 2x - 2
//      1 ≤ x ≤ 1 (variable bounds)
//
// An IIS is:
//      ||{x}||₂ ≤ 2x - 2
//      1 ≤ x
TEST_P(InfeasibleSubsystemTest,
       InconsistentSecondOrderConeConstraintWithExpressionInUpperBound) {
  if (!GetParam().support_menu.supports_infeasible_subsystems) {
    return;
  }
  Model model;
  const Variable x = model.AddContinuousVariable(1.0, 1.0);
  const SecondOrderConeConstraint c =
      model.AddSecondOrderConeConstraint({x}, 2.0 * x - 2.0);
  EXPECT_THAT(
      ComputeInfeasibleSubsystem(model, GetParam().solver_type),
      IsOkAndHolds(IsInfeasible(
          /*expected_is_minimal=*/true,
          ModelSubset{
              .variable_bounds = {{x, ModelSubset::Bounds{.lower = false,
                                                          .upper = true}}},
              .second_order_cone_constraints = {c}})));
}

// The model is:
// min  0
// s.t. {x, y} is SOS1
//      1 ≤ x, y ≤ 1 (variable bounds)
//
// An IIS is:
//      {x, y} is SOS1
//      1 ≤ x, y
TEST_P(InfeasibleSubsystemTest, InconsistentSos1Constraint) {
  if (!GetParam().support_menu.supports_infeasible_subsystems) {
    return;
  }
  Model model;
  const Variable x = model.AddContinuousVariable(1.0, 1.0);
  const Variable y = model.AddContinuousVariable(1.0, 1.0);
  const Sos1Constraint c = model.AddSos1Constraint({x, y});
  EXPECT_THAT(
      ComputeInfeasibleSubsystem(model, GetParam().solver_type),
      IsOkAndHolds(IsInfeasible(
          /*expected_is_minimal=*/true,
          ModelSubset{
              .variable_bounds =
                  {{x, ModelSubset::Bounds{.lower = true, .upper = false}},
                   {y, ModelSubset::Bounds{.lower = true, .upper = false}}},
              .sos1_constraints = {c}})));
}

// The model is:
// min  0
// s.t. {1, 1} is SOS1
//
// The entire problem is an IIS.
TEST_P(InfeasibleSubsystemTest, InconsistentSos1ConstraintWithExpressions) {
  if (!GetParam().support_menu.supports_infeasible_subsystems) {
    return;
  }
  Model model;
  const Sos1Constraint c = model.AddSos1Constraint({1.0, 1.0});
  EXPECT_THAT(ComputeInfeasibleSubsystem(model, GetParam().solver_type),
              IsOkAndHolds(IsInfeasible(/*expected_is_minimal=*/true,
                                        ModelSubset{.sos1_constraints = {c}})));
}

// The model is:
// min  0
// s.t. {x, y, z} is SOS2
//      1 ≤ x, z ≤ 1 (variable bounds)
//      0 ≤ y ≤ 1 (variable bounds)
//
// An IIS is:
//      {x, y, z} is SOS2
//      1 ≤ x, z
//      0 ≤ y
TEST_P(InfeasibleSubsystemTest, InconsistentSos2Constraint) {
  if (!GetParam().support_menu.supports_infeasible_subsystems) {
    return;
  }
  Model model;
  const Variable x = model.AddContinuousVariable(1.0, 1.0);
  const Variable y = model.AddContinuousVariable(0.0, 1.0);
  const Variable z = model.AddContinuousVariable(1.0, 1.0);
  const Sos2Constraint c = model.AddSos2Constraint({x, y, z});
  EXPECT_THAT(
      ComputeInfeasibleSubsystem(model, GetParam().solver_type),
      IsOkAndHolds(IsInfeasible(
          /*expected_is_minimal=*/true,
          ModelSubset{
              .variable_bounds =
                  {{x, ModelSubset::Bounds{.lower = true, .upper = false}},
                   {z, ModelSubset::Bounds{.lower = true, .upper = false}}},
              .sos2_constraints = {c}})));
}

// The model is:
// min  0
// s.t. {1, 0, 1} is SOS2
//
// The entire model is an IIS.
TEST_P(InfeasibleSubsystemTest, InconsistentSos2ConstraintWithExpressions) {
  if (!GetParam().support_menu.supports_infeasible_subsystems) {
    return;
  }
  Model model;
  const Sos2Constraint c = model.AddSos2Constraint({1.0, 0.0, 1.0});
  EXPECT_THAT(ComputeInfeasibleSubsystem(model, GetParam().solver_type),
              IsOkAndHolds(IsInfeasible(/*expected_is_minimal=*/true,
                                        ModelSubset{.sos2_constraints = {c}})));
}

// The model is:
// min  0
// s.t. x == 1 --> 1 ≤ 0
//      1 ≤ x ≤ 1 (variable bounds)
//      x is integer
//
// An IIS is:
//      x == 1 --> 1 ≤ 0
//      1 ≤ x
//      x is integer
TEST_P(InfeasibleSubsystemTest, InconsistentIndicatorConstraint) {
  if (!GetParam().support_menu.supports_infeasible_subsystems) {
    return;
  }
  Model model;
  const Variable x = model.AddIntegerVariable(1.0, 1.0);
  const IndicatorConstraint c =
      model.AddIndicatorConstraint(x, BoundedLinearExpression(0.0, 1.0, kInf));
  EXPECT_THAT(
      ComputeInfeasibleSubsystem(model, GetParam().solver_type),
      IsOkAndHolds(IsInfeasible(
          /*expected_is_minimal=*/true,
          ModelSubset{
              .variable_bounds = {{x, ModelSubset::Bounds{.lower = true,
                                                          .upper = false}}},
              .variable_integrality = {x},
              .indicator_constraints = {c}})));
}

// The model is:
// min  0
// s.t. {null} --> 1 ≤ 0
//
// The model is feasible.
TEST_P(InfeasibleSubsystemTest,
       IndicatorConstraintOkInconsistentImpliedNullIndicator) {
  if (!GetParam().support_menu.supports_infeasible_subsystems) {
    return;
  }
  Model model;
  // To get a null indicator variable, we: add a placeholder indicator variable,
  // add the indicator constraint, and then delete the variable.
  const Variable x = model.AddIntegerVariable(1.0, 1.0);
  model.AddIndicatorConstraint(x, BoundedLinearExpression(0.0, 1.0, kInf));
  model.DeleteVariable(x);
  EXPECT_THAT(ComputeInfeasibleSubsystem(model, GetParam().solver_type),
              IsOkAndHolds(IsUndetermined()));
}

// The model is
//  2*x + 2*y + 2*z >= 3
//  x + y <= 1
//  y + z <= 1
//  x + z <= 1
//  x, y, z in {0, 1}
//
// The IIS has no variable bounds and all other constraints. In particular, the
// LP relaxation is feasible.
struct NontrivialInfeasibleIp {
  Model model;
  Variable x;
  Variable y;
  Variable z;
  LinearConstraint a;
  LinearConstraint b;
  LinearConstraint c;
  LinearConstraint d;

  NontrivialInfeasibleIp()
      : model(),
        x(model.AddBinaryVariable("x")),
        y(model.AddBinaryVariable("y")),
        z(model.AddBinaryVariable("z")),
        a(model.AddLinearConstraint(2.0 * x + 2.0 * y + 2.0 * z >= 3.0)),
        b(model.AddLinearConstraint(x + y <= 1.0)),
        c(model.AddLinearConstraint(y + z <= 1.0)),
        d(model.AddLinearConstraint(x + z <= 1.0)) {}
};

TEST_P(InfeasibleSubsystemTest,
       NontrivialInfeasibleIpSolveWithoutLimitsFindsIIS) {
  if (!GetParam().support_menu.supports_infeasible_subsystems) {
    return;
  }
  const NontrivialInfeasibleIp ip;
  const ModelSubset expected = {
      .variable_integrality = {ip.x, ip.y, ip.z},
      .linear_constraints = {{ip.a, {.lower = true}},
                             {ip.b, {.upper = true}},
                             {ip.c, {.upper = true}},
                             {ip.d, {.upper = true}}}};
  EXPECT_THAT(
      ComputeInfeasibleSubsystem(ip.model, GetParam().solver_type),
      IsOkAndHolds(IsInfeasible(/*expected_is_minimal=*/true, expected)));
}

TEST_P(InfeasibleSubsystemTest,
       NontrivialInfeasibleIpSolveTimeLimitZeroIsUndetermined) {
  if (!GetParam().support_menu.supports_infeasible_subsystems) {
    return;
  }
  const NontrivialInfeasibleIp ip;
  EXPECT_THAT(ComputeInfeasibleSubsystem(
                  ip.model, GetParam().solver_type,
                  {.parameters = {.time_limit = absl::Seconds(0)}}),
              IsOkAndHolds(IsUndetermined()));
}

TEST_P(InfeasibleSubsystemTest,
       NontrivialInfeasibleIpSolveInterruptedBeforeStartIsUndetermined) {
  if (!GetParam().support_menu.supports_infeasible_subsystems) {
    return;
  }
  const NontrivialInfeasibleIp ip;
  SolveInterrupter interrupter;
  interrupter.Interrupt();
  EXPECT_THAT(ComputeInfeasibleSubsystem(ip.model, GetParam().solver_type,
                                         {.interrupter = &interrupter}),
              IsOkAndHolds(IsUndetermined()));
}

TEST_P(InfeasibleSubsystemTest,
       NontrivialInfeasibleIpSolveWithMessageCallbackIsInvoked) {
  if (!GetParam().support_menu.supports_infeasible_subsystems) {
    return;
  }
  const NontrivialInfeasibleIp ip;
  std::vector<std::string> logs;
  ASSERT_OK_AND_ASSIGN(const ComputeInfeasibleSubsystemResult result,
                       ComputeInfeasibleSubsystem(
                           ip.model, GetParam().solver_type,
                           {.message_callback = VectorMessageCallback(&logs)}));
  ASSERT_EQ(result.feasibility, FeasibilityStatus::kInfeasible);
  EXPECT_THAT(absl::StrJoin(logs, "\n"), HasSubstr("IIS computed"));
}

#ifdef OPERATIONS_RESEARCH_OUTPUT_CAPTURE_SUPPORTED
TEST_P(InfeasibleSubsystemTest, NoStdoutOutputByDefault) {
  if (!GetParam().support_menu.supports_infeasible_subsystems) {
    return;
  }
  const NontrivialInfeasibleIp ip;
  absl::StatusOr<ComputeInfeasibleSubsystemResult> result;
  // DO NOT ASSERT until after calling StopCaptureAndReturnContents.
  ScopedStdStreamCapture stdout_capture(CapturedStream::kStdout);
  result = ComputeInfeasibleSubsystem(ip.model, GetParam().solver_type);
  const std::string standard_output =
      std::move(stdout_capture).StopCaptureAndReturnContents();
  ASSERT_OK(result);
  EXPECT_THAT(standard_output,
              EmptyOrGurobiLicenseWarningIfGurobi(
                  /*is_gurobi=*/GetParam().solver_type == SolverType::kGurobi));
  EXPECT_EQ(result->feasibility, FeasibilityStatus::kInfeasible);
}

TEST_P(InfeasibleSubsystemTest, EnableOutputPrintsToStdOut) {
  if (!GetParam().support_menu.supports_infeasible_subsystems) {
    return;
  }
  const NontrivialInfeasibleIp ip;
  const SolveParameters params = {.enable_output = true};

  absl::StatusOr<ComputeInfeasibleSubsystemResult> result;
  // DO NOT ASSERT until after calling StopCaptureAndReturnContents.
  ScopedStdStreamCapture stdout_capture(CapturedStream::kStdout);
  result = ComputeInfeasibleSubsystem(ip.model, GetParam().solver_type,
                                      {.parameters = params});
  const std::string standard_output =
      std::move(stdout_capture).StopCaptureAndReturnContents();
  ASSERT_OK(result);
  EXPECT_THAT(standard_output, HasSubstr("IIS computed"));
  EXPECT_EQ(result->feasibility, FeasibilityStatus::kInfeasible);
}

TEST_P(InfeasibleSubsystemTest, EnableOutputIgnoredWithMessageCallback) {
  if (!GetParam().support_menu.supports_infeasible_subsystems) {
    return;
  }
  const NontrivialInfeasibleIp ip;
  const SolveParameters params = {.enable_output = true};
  std::vector<std::string> logs;

  absl::StatusOr<ComputeInfeasibleSubsystemResult> result;
  // DO NOT ASSERT until after calling StopCaptureAndReturnContents.
  ScopedStdStreamCapture stdout_capture(CapturedStream::kStdout);
  result = ComputeInfeasibleSubsystem(
      ip.model, GetParam().solver_type,
      {.parameters = params, .message_callback = VectorMessageCallback(&logs)});
  const std::string standard_output =
      std::move(stdout_capture).StopCaptureAndReturnContents();
  ASSERT_OK(result);
  EXPECT_THAT(standard_output,
              EmptyOrGurobiLicenseWarningIfGurobi(
                  /*is_gurobi=*/GetParam().solver_type == SolverType::kGurobi));
  EXPECT_EQ(result->feasibility, FeasibilityStatus::kInfeasible);
  EXPECT_THAT(absl::StrJoin(logs, "\n"), HasSubstr("IIS computed"));
}

#endif  // OPERATIONS_RESEARCH_OUTPUT_CAPTURE_SUPPORTED

}  // namespace operations_research::math_opt
