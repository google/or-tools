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

#include "ortools/math_opt/labs/linear_expr_util.h"

#include <limits>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/cpp/math_opt.h"

namespace operations_research::math_opt {
namespace {

using ::testing::AnyOf;
using ::testing::DoubleEq;
using ::testing::IsNan;

constexpr double kInf = std::numeric_limits<double>::infinity();
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kMaxDouble = std::numeric_limits<double>::max();

TEST(LinearExpressionBoundTest, SimpleSum) {
  Model model;
  const Variable x = model.AddContinuousVariable(2.0, 3.0);
  const Variable y = model.AddContinuousVariable(-5.0, -4.0);

  {
    const LinearExpression pos_coef = 10.0 * x + 100.0 * y + 1000.0;
    EXPECT_NEAR(LowerBound(pos_coef), 20.0 - 500.0 + 1000.0, 1e-10);
    EXPECT_NEAR(UpperBound(pos_coef), 30.0 - 400.0 + 1000.0, 1e-10);
  }

  {
    const LinearExpression neg_coef = -10.0 * x + -100.0 * y + -1000.0;
    EXPECT_NEAR(LowerBound(neg_coef), -30.0 + 400.0 - 1000.0, 1e-10);
    EXPECT_NEAR(UpperBound(neg_coef), -20.0 + 500.0 - 1000.0, 1e-10);
  }
}

TEST(LinearExpressionBoundTest, InfiniteBounds) {
  Model model;
  const Variable x = model.AddContinuousVariable(-kInf, 1.0);
  const Variable y = model.AddContinuousVariable(-1.0, kInf);

  {
    const LinearExpression pos_coef = 10.0 * x + 100.0 * y;
    EXPECT_DOUBLE_EQ(LowerBound(pos_coef), -kInf);
    EXPECT_DOUBLE_EQ(UpperBound(pos_coef), kInf);
  }

  {
    const LinearExpression neg_coef = -10.0 * x + -100.0 * y;
    EXPECT_DOUBLE_EQ(LowerBound(neg_coef), -kInf);
    EXPECT_DOUBLE_EQ(UpperBound(neg_coef), kInf);
  }

  {
    const LinearExpression finite_lb = -10.0 * x + 100.0 * y;
    EXPECT_NEAR(LowerBound(finite_lb), -110.0, 1e-10);
    EXPECT_DOUBLE_EQ(UpperBound(finite_lb), kInf);
  }

  {
    const LinearExpression finite_ub = 10.0 * x + -100.0 * y;
    EXPECT_DOUBLE_EQ(LowerBound(finite_ub), -kInf);
    EXPECT_NEAR(UpperBound(finite_ub), 110.0, 1e-10);
  }
}

TEST(LinearExpressionBoundTest, AccumulatesNearZeroFirst) {
  Model model;
  const Variable w = model.AddContinuousVariable(1.0, 1.0);
  const Variable x = model.AddContinuousVariable(1.0, 1.0);
  const Variable y = model.AddContinuousVariable(1.0, 1.0);
  const Variable z = model.AddContinuousVariable(1.0, 1.0);

  const LinearExpression differentScales =
      1e-20 * w - 1e-20 * x + 1e20 * y - 1e20 * z;
  // NOTE: the result should be exactly zero, we do not need any tolerance.
  EXPECT_EQ(LowerBound(differentScales), 0.0);
  EXPECT_EQ(UpperBound(differentScales), 0.0);
}

TEST(LinearExpressionBoundTest, InputHasNaNNoCrash) {
  Model model;
  const Variable x = model.AddContinuousVariable(kNaN, kNaN);

  const LinearExpression nanExpr = 2 * x;
  EXPECT_THAT(LowerBound(nanExpr), IsNan());
  EXPECT_THAT(UpperBound(nanExpr), IsNan());
}

TEST(LinearExpressionBoundTest, InputHasBothPlusAndMinusInfTermsNoCrash) {
  Model model;
  const Variable x = model.AddContinuousVariable(kInf, kInf);
  const Variable y = model.AddContinuousVariable(-kInf, 0.0);

  const LinearExpression nanExpr = x + y;
  EXPECT_THAT(LowerBound(nanExpr), testing::IsNan());
}

TEST(LinearExpressionBoundTest, OverflowWhileSummingNoCrash) {
  Model model;
  const Variable x = model.AddContinuousVariable(kMaxDouble / 2.0, kInf);
  const Variable y = model.AddContinuousVariable(kMaxDouble / 2.0, 0.0);

  const LinearExpression nanExpr = x + y + kMaxDouble / 2.0;
  EXPECT_THAT(LowerBound(nanExpr), AnyOf(IsNan(), DoubleEq(kInf)));
}

}  // namespace
}  // namespace operations_research::math_opt
