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

#include "ortools/linear_solver/scip_callback.h"

#include <cmath>
#include <vector>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/linear_solver/linear_expr.h"
#include "ortools/linear_solver/linear_solver.h"

namespace operations_research {
namespace {

struct Point {
  double x1 = 0.0;
  double x2 = 0.0;
};

Point Difference(const Point left, const Point right) {
  Point result;
  result.x1 = left.x1 - right.x1;
  result.x2 = left.x2 - right.x2;
  return result;
}

Point Multiply(const Point left, double right) {
  Point result = left;
  result.x1 *= right;
  result.x2 *= right;
  return result;
}

double InnerProduct(const Point left, const Point right) {
  return left.x1 * right.x1 + left.x2 * right.x2;
}

double NormSquared(const Point p) { return InnerProduct(p, p); }

double Norm(const Point p) { return std::sqrt(NormSquared(p)); }

struct Circle {
  Point center;
  double r;
};

struct CircleConstraintData {
  MPVariable* x1;
  MPVariable* x2;
  Circle circle;
};

bool PointInCircle(const Point x, const Circle& circle) {
  return NormSquared(Difference(x, circle.center)) <= std::pow(circle.r, 2);
}

TEST(PointInCircleTest, UnitCircle) {
  Circle c;
  c.r = 1.0;
  EXPECT_TRUE(PointInCircle({0.0, 0.0}, c));
  EXPECT_TRUE(PointInCircle({1.0, 0.0}, c));
  EXPECT_TRUE(PointInCircle({0.5, 0.0}, c));
  EXPECT_TRUE(PointInCircle({0.0, 1.0}, c));
  EXPECT_TRUE(PointInCircle({0.5, 0.5}, c));
  EXPECT_TRUE(PointInCircle({-0.5, -0.5}, c));
  EXPECT_TRUE(PointInCircle({-0.9, 0.0}, c));
  EXPECT_TRUE(PointInCircle({0.0, 1.0}, c));

  EXPECT_FALSE(PointInCircle({0.0, 1.2}, c));
  EXPECT_FALSE(PointInCircle({1.0, 1.0}, c));
  EXPECT_FALSE(PointInCircle({-1.2, 0.1}, c));
}

TEST(PointInCircleTest, BigCircle) {
  Circle c;
  c.center = {4.0, 5.0};
  c.r = 3.0;
  EXPECT_TRUE(PointInCircle({4.0, 5.0}, c));
  EXPECT_TRUE(PointInCircle({1.0, 5.0}, c));
  EXPECT_TRUE(PointInCircle({6.0, 7.0}, c));

  EXPECT_FALSE(PointInCircle({0.0, 0.0}, c));
  EXPECT_FALSE(PointInCircle({10.0, 10.0}, c));
  EXPECT_FALSE(PointInCircle({-4.0, -5.0}, c));
}

// The hyperplane <a, x> <= b
struct Hyperplane {
  Point a;
  double b;
};

// Given a point p not in a circle, with center c and radius r, we can give a
// hyperplane <a, x> <= b that:
//   1. Is valid for the circle (<a, x> <= b for all x in circle)
//   2. Is invalid for p (<a, p> > b).
// The hyperplane is, letting d = r*(p - c)/ ||p -c||:
//   <d, x - c> <= r^2
// Or equivalently:
//   < d, x > <= r^2 + <d, c>
Hyperplane Separate(const Point p, const Circle& circle) {
  Hyperplane result;
  CHECK(!PointInCircle(p, circle));
  const Point p_centered = Difference(p, circle.center);
  const double p_centered_norm = Norm(p_centered);
  result.a = Multiply(p_centered, circle.r / p_centered_norm);
  result.b = std::pow(circle.r, 2) + InnerProduct(result.a, circle.center);
  return result;
}

TEST(SeparateTest, BigCircle) {
  Circle c;
  c.center = {4.0, 5.0};
  c.r = 3.0;
  const Hyperplane right = Separate({10.0, 5.0}, c);
  // We want the constraint x1 <= 7, or any multiple of this constraint.
  EXPECT_NEAR(right.a.x2, 0.0, 1e-5);
  EXPECT_GE(right.b, 0.0);
  EXPECT_NEAR(right.b / right.a.x1, 7.0, 1e-5);

  const Hyperplane below = Separate({4.0, 0.0}, c);
  // We want the constraint x2 >= 2, (i.e. -x2 <= -2.0)  or any multiple of this
  // constraint.
  EXPECT_NEAR(below.a.x1, 0.0, 1e-5);
  EXPECT_LT(below.b, 0.0);
  EXPECT_NEAR(below.b / below.a.x2, 2.0, 1e-5);
}

class CircleConstraintHandler
    : public ScipConstraintHandler<CircleConstraintData> {
 public:
  CircleConstraintHandler()
      : ScipConstraintHandler<
            CircleConstraintData>(ScipConstraintHandlerDescription{
            .name = "circle_callback",
            .description =
                "Handler for constraints of the form ||center -x||^2 <= r^2"}) {
  }
  std::vector<CallbackRangeConstraint> SeparateFractionalSolution(
      const ScipConstraintHandlerContext& context,
      const CircleConstraintData& circle_constraint) override {
    std::vector<CallbackRangeConstraint> result;
    Point var_val;
    var_val.x1 = context.VariableValue(circle_constraint.x1);
    var_val.x2 = context.VariableValue(circle_constraint.x2);
    if (!PointInCircle(var_val, circle_constraint.circle)) {
      const Hyperplane hyperplane = Separate(var_val, circle_constraint.circle);
      CallbackRangeConstraint linear_constraint;
      linear_constraint.range =
          hyperplane.a.x1 * LinearExpr(circle_constraint.x1) +
              hyperplane.a.x2 * LinearExpr(circle_constraint.x2) <=
          hyperplane.b;
      result.push_back(linear_constraint);
    }
    return result;
  }
};

TEST(ScipCallbackTest, DISABLED_UnitCircleTest) {
  MPSolver solver("scip_circle_test", MPSolver::SCIP_MIXED_INTEGER_PROGRAMMING);
  MPVariable* x1 = solver.MakeBoolVar("x1");
  MPVariable* x2 = solver.MakeBoolVar("x2");
  solver.MutableObjective()->MaximizeLinearExpr(2.0 * LinearExpr(x1) +
                                                3.0 * LinearExpr(x2));
  solver.EnableOutput();
  SCIP* scip = static_cast<SCIP*>(solver.underlying_solver());
  CircleConstraintHandler circle_constraint_handler;
  RegisterConstraintHandler(&circle_constraint_handler, scip);
  CircleConstraintData circle_constraint;
  circle_constraint.x1 = x1;
  circle_constraint.x2 = x2;
  circle_constraint.circle.r = 1.0;
  circle_constraint.circle.center.x1 = 0.0;
  circle_constraint.circle.center.x2 = 0.0;

  AddCallbackConstraint(scip, &circle_constraint_handler,
                        "unit_circle_constraint", &circle_constraint,
                        ScipCallbackConstraintOptions());
  ASSERT_EQ(solver.Solve(), MPSolver::OPTIMAL);
  EXPECT_NEAR(solver.Objective().Value(), 3.0, 1e-4);
  EXPECT_NEAR(x1->solution_value(), 0.0, 1e-4);
  EXPECT_NEAR(x2->solution_value(), 1.0, 1e-4);
}

TEST(ScipCallbackTest, DISABLED_BigCircleTest) {
  MPSolver solver("scip_circle_test", MPSolver::SCIP_MIXED_INTEGER_PROGRAMMING);
  MPVariable* x1 = solver.MakeIntVar(-10, 10, "x1");
  MPVariable* x2 = solver.MakeIntVar(-10, 10, "x2");
  solver.MutableObjective()->MaximizeLinearExpr(LinearExpr(x1) +
                                                LinearExpr(x2));
  solver.EnableOutput();
  SCIP* scip = static_cast<SCIP*>(solver.underlying_solver());
  CircleConstraintHandler circle_constraint_handler;
  RegisterConstraintHandler(&circle_constraint_handler, scip);
  CircleConstraintData circle_constraint;
  circle_constraint.x1 = x1;
  circle_constraint.x2 = x2;
  circle_constraint.circle.r = 3.0;
  circle_constraint.circle.center.x1 = 4.0;
  circle_constraint.circle.center.x2 = 5.0;

  AddCallbackConstraint(scip, &circle_constraint_handler,
                        "big_circle_constraint", &circle_constraint,
                        ScipCallbackConstraintOptions());
  ASSERT_EQ(solver.Solve(), MPSolver::OPTIMAL);
  EXPECT_NEAR(solver.Objective().Value(), 13.0, 1e-4);
  EXPECT_NEAR(x1->solution_value(), 6.0, 1e-4);
  EXPECT_NEAR(x2->solution_value(), 7.0, 1e-4);
}

TEST(ScipCallbackTest, DISABLED_CircleIntersectionTest) {
  MPSolver solver("scip_circle_test", MPSolver::SCIP_MIXED_INTEGER_PROGRAMMING);
  MPVariable* x1 = solver.MakeIntVar(-3, 3, "x1");
  MPVariable* x2 = solver.MakeIntVar(-3, 3, "x2");
  solver.MutableObjective()->MaximizeLinearExpr(LinearExpr(x2));
  solver.EnableOutput();
  SCIP* scip = static_cast<SCIP*>(solver.underlying_solver());
  CircleConstraintHandler circle_constraint_handler;
  RegisterConstraintHandler(&circle_constraint_handler, scip);
  CircleConstraintData circle_right;
  circle_right.x1 = x1;
  circle_right.x2 = x2;
  circle_right.circle.r = 2.0;
  circle_right.circle.center.x1 = 1.0;
  circle_right.circle.center.x2 = 0.0;

  AddCallbackConstraint(scip, &circle_constraint_handler, "right_constraint",
                        &circle_right, ScipCallbackConstraintOptions());

  CircleConstraintData circle_left;
  circle_left.x1 = x1;
  circle_left.x2 = x2;
  circle_left.circle.r = 2.0;
  circle_left.circle.center.x1 = -1.0;
  circle_left.circle.center.x2 = 0.0;

  AddCallbackConstraint(scip, &circle_constraint_handler, "left_constraint",
                        &circle_left, ScipCallbackConstraintOptions());

  ASSERT_EQ(solver.Solve(), MPSolver::OPTIMAL);
  EXPECT_NEAR(solver.Objective().Value(), 1.0, 1e-4);
  EXPECT_NEAR(x1->solution_value(), 0.0, 1e-4);
  EXPECT_NEAR(x2->solution_value(), 1.0, 1e-4);
}

class CircleMPCallback : public MPCallback {
 public:
  explicit CircleMPCallback(std::vector<CircleConstraintData> circles)
      : MPCallback(/*might_add_cuts=*/false,
                   /*might_add_lazy_constraints=*/true),
        circles_(circles) {}

  void RunCallback(MPCallbackContext* callback_context) override {
    if (callback_context->Event() == MPCallbackEvent::kMipNode ||
        callback_context->Event() == MPCallbackEvent::kMipSolution) {
      if (callback_context->CanQueryVariableValues()) {
        for (const CircleConstraintData& circle_constraint : circles_) {
          Point var_val;
          var_val.x1 = callback_context->VariableValue(circle_constraint.x1);
          var_val.x2 = callback_context->VariableValue(circle_constraint.x2);
          if (!PointInCircle(var_val, circle_constraint.circle)) {
            const Hyperplane hyperplane =
                Separate(var_val, circle_constraint.circle);
            callback_context->AddLazyConstraint(
                hyperplane.a.x1 * LinearExpr(circle_constraint.x1) +
                    hyperplane.a.x2 * LinearExpr(circle_constraint.x2) <=
                hyperplane.b);
          }
        }
      }
    }
  }

 private:
  std::vector<CircleConstraintData> circles_;
};

TEST(ScipCallbackTest, DISABLED_CircleIntersectionWithMPCallbackTest) {
  MPSolver solver("scip_circle_test", MPSolver::SCIP_MIXED_INTEGER_PROGRAMMING);
  MPVariable* x1 = solver.MakeIntVar(-3, 3, "x1");
  MPVariable* x2 = solver.MakeIntVar(-3, 3, "x2");
  solver.MutableObjective()->MaximizeLinearExpr(LinearExpr(x2));
  solver.EnableOutput();

  CircleConstraintData circle_right;
  circle_right.x1 = x1;
  circle_right.x2 = x2;
  circle_right.circle.r = 2.0;
  circle_right.circle.center.x1 = 1.0;
  circle_right.circle.center.x2 = 0.0;

  CircleConstraintData circle_left;
  circle_left.x1 = x1;
  circle_left.x2 = x2;
  circle_left.circle.r = 2.0;
  circle_left.circle.center.x1 = -1.0;
  circle_left.circle.center.x2 = 0.0;

  CircleMPCallback mp_callback({circle_right, circle_left});
  solver.SetCallback(&mp_callback);

  ASSERT_EQ(solver.Solve(), MPSolver::OPTIMAL);
  EXPECT_NEAR(solver.Objective().Value(), 1.0, 1e-4);
  EXPECT_NEAR(x1->solution_value(), 0.0, 1e-4);
  EXPECT_NEAR(x2->solution_value(), 1.0, 1e-4);
}

}  // namespace
}  // namespace operations_research
