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

// NOTE(user): these tests could be a bit more comprehensive, but maybe we
// should reconsider if these functions should exist. The code will be
// additionally tested by:
// cs/ortools/linear_solver/scip_proto_solver_test.cc
#include "ortools/math_opt/solvers/gscip/gscip_ext.h"

#include <cstdlib>
#include <limits>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/map_util.h"
#include "ortools/math_opt/solvers/gscip/gscip.h"
#include "ortools/math_opt/solvers/gscip/gscip.pb.h"
#include "ortools/math_opt/solvers/gscip/gscip_testing.h"
#include "scip/scip.h"

namespace operations_research {
namespace {

using ::testing::DoubleNear;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

constexpr double kInf = std::numeric_limits<double>::infinity();

MATCHER_P2(GScipLinearExprIsNear, expected_expr, tolerance, "") {
  GScipLinearExpr diff = GScipDifference(arg, expected_expr);
  if (std::abs(diff.offset) > tolerance) {
    *result_listener << "offsets actual: " << arg.offset
                     << " and expected: " << expected_expr.offset
                     << " are not within tolerance: " << tolerance;
    return false;
  }
  for (const auto& var_val : diff.terms) {
    if (std::abs(var_val.second) > tolerance) {
      *result_listener << "for variable: " << SCIPvarGetName(var_val.first)
                       << " coefficients actual: "
                       << gtl::FindWithDefault(arg.terms, var_val.first)
                       << " and expected: "
                       << gtl::FindWithDefault(expected_expr.terms,
                                               var_val.first)
                       << " are not within tolerance: " << tolerance;
      return false;
    }
  }
  return true;
}

TEST(GScipExtTest, TestDifference) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_ext_test"));
  SCIP_VAR* x =
      gscip->AddVariable(0.0, 1.0, 0.0, GScipVarType::kInteger, "x").value();
  SCIP_VAR* y =
      gscip->AddVariable(0.0, 1.0, 0.0, GScipVarType::kInteger, "y").value();
  SCIP_VAR* z =
      gscip->AddVariable(0.0, 1.0, 0.0, GScipVarType::kInteger, "z").value();
  GScipLinearExpr left;
  left.offset = 9.0;
  left.terms[x] = 2.0;
  left.terms[y] = -4.5;
  GScipLinearExpr right;
  right.offset = 2.0;
  right.terms[x] = 0.3;
  right.terms[z] = 3.3;
  GScipLinearExpr expected;
  expected.offset = 7.0;
  expected.terms[x] = 1.7;
  expected.terms[y] = -4.5;
  expected.terms[z] = -3.3;
  EXPECT_THAT(GScipDifference(left, right),
              GScipLinearExprIsNear(expected, 1e-7));
}

TEST(GScipExtTest, TestNegate) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_ext_test"));
  SCIP_VAR* x =
      gscip->AddVariable(0.0, 1.0, 0.0, GScipVarType::kInteger, "x").value();
  SCIP_VAR* y =
      gscip->AddVariable(0.0, 1.0, 0.0, GScipVarType::kInteger, "y").value();

  GScipLinearExpr input;
  input.offset = 9.0;
  input.terms[x] = 2.0;
  input.terms[y] = -4.5;

  GScipLinearExpr expected;
  expected.offset = -9.0;
  expected.terms[x] = -2.0;
  expected.terms[y] = 4.5;
  EXPECT_THAT(GScipNegate(input), GScipLinearExprIsNear(expected, 1e-7));
}

TEST(GScipExtTest, TestLe) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_ext_test"));
  SCIP_VAR* x =
      gscip->AddVariable(0.0, 1.0, 0.0, GScipVarType::kInteger, "x").value();
  SCIP_VAR* y =
      gscip->AddVariable(0.0, 1.0, 0.0, GScipVarType::kInteger, "y").value();
  SCIP_VAR* z =
      gscip->AddVariable(0.0, 1.0, 0.0, GScipVarType::kInteger, "z").value();
  GScipLinearExpr left;
  left.offset = 9.0;
  left.terms[x] = 2.0;
  left.terms[y] = -4.5;
  GScipLinearExpr right;
  right.offset = 2.0;
  right.terms[x] = 0.3;
  right.terms[z] = 3.3;
  GScipLinearRange range = GScipLe(left, right);
  EXPECT_EQ(range.lower_bound, -kInf);
  EXPECT_NEAR(range.upper_bound, -7.0, 1e-7);
  ASSERT_EQ(range.variables.size(), 3);
  ASSERT_EQ(range.coefficients.size(), 3);
  absl::flat_hash_map<SCIP_VAR*, double> terms;
  for (int i = 0; i < 3; ++i) {
    terms[range.variables[i]] = range.coefficients[i];
  }
  EXPECT_THAT(terms, UnorderedElementsAre(Pair(x, DoubleNear(1.7, 1e-7)),
                                          Pair(y, DoubleNear(-4.5, 1e-7)),
                                          Pair(z, DoubleNear(-3.3, 1e-7))));
}

// We want to minimize f(x) = 2x^2 - 8x + 3
//   First order conditions: df/dx = 4x - 8
// Solve for zero, x* = 2, f(x*) = -5
TEST(GScipExtTest, MinimizeConvexQuadratic) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_ext_test"));
  ASSERT_OK(gscip->SetMaximize(false));
  ASSERT_OK(gscip->SetObjectiveOffset(3.0));
  SCIP_VAR* x =
      gscip->AddVariable(-20.0, 20.0, -8.0, GScipVarType::kContinuous, "x")
          .value();
  ASSERT_OK(GScipAddQuadraticObjectiveTerm(gscip.get(), {x}, {x}, {2.0}));
  const GScipResult result = gscip->Solve(TestGScipParameters()).value();
  // NOTE(user): GScipAddQuadraticObjectiveTerm adds auxiliary variables.
  AssertOptimalWithPartialBestSolution(result, -5.0, {{x, 2.0}}, 0.01);
}

// We want to maximize f(x) = 2x^2 - 8x + 3, x in [-5, 5].
// Problem is convex, so optimal solution at boundary, x = -5, f(x) = 93.
TEST(GScipExtTest, MaximizeConvexQuadratic) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_ext_test"));
  ASSERT_OK(gscip->SetMaximize(true));
  ASSERT_OK(gscip->SetObjectiveOffset(3.0));
  SCIP_VAR* x =
      gscip->AddVariable(-5.0, 5.0, -8.0, GScipVarType::kContinuous, "x")
          .value();
  ASSERT_OK(GScipAddQuadraticObjectiveTerm(gscip.get(), {x}, {x}, {2.0}));
  const GScipResult result = gscip->Solve(TestGScipParameters()).value();
  // NOTE(user): GScipAddQuadraticObjectiveTerm adds auxiliary variables.
  AssertOptimalWithPartialBestSolution(result, 93.0, {{x, -5.0}}, 0.01);
}

// min y - 5*z
// y = min{x, 5-x}
// if z then 2 <= x <= 4
// z in {0, 1}
// y, w >= 0.
// 0 <= x <= 5
//
// encoding of y = min{x, 5-x}
// w in {0,1} indicates the x branch
// y <= x
// y <= 5-x
// y >= x - 5*w
// y >= 5-x - 5*(1-w)
//
// Optimal solution: x = 4, y = 1, w = 1, z = 1, obj = -4.
// (when z = 0, we can take x = 0, y = 0, w = 0 and get obj = 0).
TEST(GScipExtTest, IndicatorRangeConstraint) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_ext_test"));
  ASSERT_OK(gscip->SetMaximize(false));
  SCIP_VAR* w =
      gscip->AddVariable(0.0, 1.0, 0.0, GScipVarType::kInteger, "w").value();
  SCIP_VAR* x =
      gscip->AddVariable(0.0, 5.0, 0.0, GScipVarType::kContinuous, "x").value();
  SCIP_VAR* y =
      gscip->AddVariable(0.0, kInf, 1.0, GScipVarType::kContinuous, "y")
          .value();
  SCIP_VAR* z =
      gscip->AddVariable(0.0, 1.0, -5.0, GScipVarType::kInteger, "z").value();
  // if z then 2 <= x <= 6
  {
    GScipIndicatorRangeConstraint r;
    r.indicator_variable = z;
    r.negate_indicator = false;
    r.range.lower_bound = 2.0;
    r.range.upper_bound = 4.0;
    r.range.variables = {x};
    r.range.coefficients = {1.0};
    ASSERT_OK(GScipCreateIndicatorRange(gscip.get(), r));
  }
  // y <= x
  {
    GScipLinearRange range;
    range.lower_bound = -kInf;
    range.upper_bound = 0.0;
    range.variables = {y, x};
    range.coefficients = {1.0, -1.0};
    ASSERT_OK(gscip->AddLinearConstraint(range).status());
  }
  // y <= 5.0 - x
  {
    GScipLinearRange range;
    range.lower_bound = -kInf;
    range.upper_bound = 5.0;
    range.variables = {y, x};
    range.coefficients = {1.0, 1.0};
    ASSERT_OK(gscip->AddLinearConstraint(range).status());
  }
  // y >= x - 5*w
  {
    GScipLinearRange range;
    range.lower_bound = 0.0;
    range.upper_bound = kInf;
    range.variables = {y, x, w};
    range.coefficients = {1.0, -1.0, 5.0};
    ASSERT_OK(gscip->AddLinearConstraint(range).status());
  }
  // y >= 5-x - 5*(1-w)
  // y + x - 5w >= 0
  {
    GScipLinearRange range;
    range.lower_bound = 0.0;
    range.upper_bound = kInf;
    range.variables = {y, x, w};
    range.coefficients = {1.0, 1.0, -5.0};
    ASSERT_OK(gscip->AddLinearConstraint(range).status());
  }
  const GScipResult result = gscip->Solve(TestGScipParameters()).value();
  AssertOptimalWithBestSolution(result, -4.0,
                                {{w, 1.0}, {x, 4.0}, {y, 1.0}, {z, 1.0}});
}

// max 3y - x
// y = abs(x)
// -3 <= x <= 2
//
// Optimal solution: x = -3, y = 3, obj = 12.
TEST(GScipExtTest, AbsConstraint) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_ext_test"));
  ASSERT_OK(gscip->SetMaximize(true));
  SCIP_VAR* x =
      gscip->AddVariable(-3.0, 2.0, -1.0, GScipVarType::kContinuous, "x")
          .value();
  SCIP_VAR* y =
      gscip->AddVariable(-kInf, kInf, 3.0, GScipVarType::kContinuous, "y")
          .value();
  ASSERT_OK(GScipCreateAbs(gscip.get(), x, y, "a"));
  const GScipResult result = gscip->Solve(TestGScipParameters()).value();
  AssertOptimalWithPartialBestSolution(result, 12.0, {{x, -3.0}, {y, 3.0}});
}

// max y
// y = abs(x)
// -3 <= x <= -2
//
// Optimal solution: x = -3, y = 3, obj = 3.
TEST(GScipExtTest, AbsConstraintAlwaysNegative) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_ext_test"));
  ASSERT_OK(gscip->SetMaximize(true));
  SCIP_VAR* x =
      gscip->AddVariable(-3.0, -2.0, 0.0, GScipVarType::kContinuous, "x")
          .value();
  SCIP_VAR* y =
      gscip->AddVariable(-kInf, kInf, 1.0, GScipVarType::kContinuous, "y")
          .value();
  ASSERT_OK(GScipCreateAbs(gscip.get(), x, y, "a"));
  const GScipResult result = gscip->Solve(TestGScipParameters()).value();
  AssertOptimalWithPartialBestSolution(result, 3.0, {{x, -3.0}, {y, 3.0}});
}

// max 2*y
// y = abs(x)
// 4 <= x <= 7
//
// Optimal solution: x = 7, y = 7, obj = 14.
TEST(GScipExtTest, AbsConstraintAlwaysPositive) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_ext_test"));
  ASSERT_OK(gscip->SetMaximize(true));
  SCIP_VAR* x =
      gscip->AddVariable(4.0, 7.0, 0.0, GScipVarType::kContinuous, "x").value();
  SCIP_VAR* y =
      gscip->AddVariable(-kInf, kInf, 2.0, GScipVarType::kContinuous, "y")
          .value();
  ASSERT_OK(GScipCreateAbs(gscip.get(), x, y, "a"));
  const GScipResult result = gscip->Solve(TestGScipParameters()).value();
  AssertOptimalWithPartialBestSolution(result, 14.0, {{x, 7.0}, {y, 7.0}});
}

// max x + y
// y = abs(x)
// x + y <= 10
// x, y unbounded
//
// Optimal solution: x = 5, y = 5, obj = 10.
TEST(GScipExtTest, Unbounded) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_ext_test"));
  ASSERT_OK(gscip->SetMaximize(true));
  SCIP_VAR* x =
      gscip->AddVariable(-kInf, kInf, 1.0, GScipVarType::kContinuous, "x")
          .value();
  SCIP_VAR* y =
      gscip->AddVariable(-kInf, kInf, 1.0, GScipVarType::kContinuous, "y")
          .value();
  GScipLinearRange range;
  range.upper_bound = 10.0;
  range.variables = {x, y};
  range.coefficients = {1.0, 1.0};
  ASSERT_OK(gscip->AddLinearConstraint(range, "c").status());
  ASSERT_OK(GScipCreateAbs(gscip.get(), x, y, "a"));
  const GScipResult result = gscip->Solve(TestGScipParameters()).value();
  AssertOptimalWithPartialBestSolution(result, 10.0, {{x, 5.0}, {y, 5.0}});
}

// max z
// z = min{x, y, 5}
// x + y = 8
// x,  y >= 0
//
// Opt: x = 4, y = 4, z = 4
TEST(GScipExtTest, MinConstraint) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_ext_test"));
  ASSERT_OK(gscip->SetMaximize(true));
  SCIP_VAR* x =
      gscip->AddVariable(0, kInf, 0.0, GScipVarType::kContinuous, "x").value();
  SCIP_VAR* y =
      gscip->AddVariable(0, kInf, 0.0, GScipVarType::kContinuous, "y").value();
  SCIP_VAR* z =
      gscip->AddVariable(-kInf, kInf, 1.0, GScipVarType::kContinuous, "z")
          .value();
  ASSERT_OK(GScipCreateMinimum(
      gscip.get(), GScipLinearExpr(z),
      {GScipLinearExpr(x), GScipLinearExpr(y), GScipLinearExpr(5.0)}));
  GScipLinearRange r;
  r.lower_bound = 8.0;
  r.upper_bound = 8.0;
  r.variables = {x, y};
  r.coefficients = {1.0, 1.0};
  ASSERT_OK(gscip->AddLinearConstraint(r).status());
  const GScipResult result = gscip->Solve(TestGScipParameters()).value();
  AssertOptimalWithPartialBestSolution(result, 4.0,
                                       {{x, 4.0}, {y, 4.0}, {z, 4.0}});
}

// min z
// z = max{x, y, 3}
// x + y = 8
// x,  y >= 0
//
// Opt: x = 4, y = 4, z = 4
TEST(GScipExtTest, MaxConstraint) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_ext_test"));
  ASSERT_OK(gscip->SetMaximize(false));
  SCIP_VAR* x =
      gscip->AddVariable(0, kInf, 0.0, GScipVarType::kContinuous, "x").value();
  SCIP_VAR* y =
      gscip->AddVariable(0, kInf, 0.0, GScipVarType::kContinuous, "y").value();
  SCIP_VAR* z =
      gscip->AddVariable(-kInf, kInf, 1.0, GScipVarType::kContinuous, "z")
          .value();
  ASSERT_OK(GScipCreateMaximum(
      gscip.get(), GScipLinearExpr(z),
      {GScipLinearExpr(x), GScipLinearExpr(y), GScipLinearExpr(3.0)}));
  GScipLinearRange r;
  r.lower_bound = 8.0;
  r.upper_bound = 8.0;
  r.variables = {x, y};
  r.coefficients = {1.0, 1.0};
  ASSERT_OK(gscip->AddLinearConstraint(r).status());
  const GScipResult result = gscip->Solve(TestGScipParameters()).value();
  AssertOptimalWithPartialBestSolution(result, 4.0,
                                       {{x, 4.0}, {y, 4.0}, {z, 4.0}});
}

}  // namespace
}  // namespace operations_research
