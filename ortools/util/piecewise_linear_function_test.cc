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

#include "ortools/util/piecewise_linear_function.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "absl/random/distributions.h"
#include "gtest/gtest.h"
#include "ortools/base/log_severity.h"
#include "ortools/base/stl_util.h"
#include "ortools/base/types.h"
#include "ortools/util/piecewise_linear_function_test_utils.h"
#include "ortools/util/random_engine.h"
#include "ortools/util/saturated_arithmetic.h"

namespace operations_research {
namespace {

TEST(PiecewiseLinearFunctionTest, StepSegmentIsNonDecreasing) {
  std::unique_ptr<PiecewiseLinearFunction> pwl(
      PiecewiseLinearFunction::CreateOneSegmentFunction(0, 10, 0, 2));
  EXPECT_TRUE(pwl->IsNonDecreasing());
  EXPECT_TRUE(pwl->IsNonIncreasing());
}

TEST(PiecewiseLinearFunctionTest, IncreasingSegmentIsNonDecreasing) {
  std::unique_ptr<PiecewiseLinearFunction> pwl(
      PiecewiseLinearFunction::CreateOneSegmentFunction(0, 10, 1, 2));
  EXPECT_TRUE(pwl->IsNonDecreasing());
  EXPECT_FALSE(pwl->IsNonIncreasing());
}

TEST(PiecewiseLinearFunctionTest, IncreasingEmptySegmentIsNonDecreasing) {
  std::unique_ptr<PiecewiseLinearFunction> pwl(
      PiecewiseLinearFunction::CreateOneSegmentFunction(0, 10, 1, 0));
  EXPECT_TRUE(pwl->IsNonDecreasing());
  EXPECT_TRUE(pwl->IsNonIncreasing());
}

TEST(PiecewiseLinearFunctionTest, DecreasingEmptySegmentIsNonDecreasing) {
  std::unique_ptr<PiecewiseLinearFunction> pwl(
      PiecewiseLinearFunction::CreateOneSegmentFunction(0, 10, -1, 0));
  EXPECT_TRUE(pwl->IsNonDecreasing());
  EXPECT_TRUE(pwl->IsNonIncreasing());
}

TEST(PiecewiseLinearFunctionTest, DecreasingSegmentIsNotNonDecreasing) {
  std::unique_ptr<PiecewiseLinearFunction> pwl(
      PiecewiseLinearFunction::CreateOneSegmentFunction(0, 10, -1, 2));
  EXPECT_FALSE(pwl->IsNonDecreasing());
  EXPECT_TRUE(pwl->IsNonIncreasing());
}

TEST(PiecewiseLinearFunctionTest, PeriodicIncreasingIsNotNonDecreasing) {
  std::unique_ptr<PiecewiseLinearFunction> pwl(
      PiecewiseLinearFunction::CreatePiecewiseLinearFunction({0, 1}, {0, 0},
                                                             {1, 1}, {1, 2}));
  EXPECT_FALSE(pwl->IsNonDecreasing());
  EXPECT_FALSE(pwl->IsNonIncreasing());
}

// Tests that a point piecewise linear function returns undefined (kint64max)
// for each value except of the point it represents.
TEST(PiecewiseLinearFunctionTest, OnePoint) {
  std::unique_ptr<PiecewiseLinearFunction> pwl(
      PiecewiseLinearFunction::CreateOneSegmentFunction(0, 10, 1, 0));
  EXPECT_EQ(kint64max, pwl->Value(kint64min));
  EXPECT_EQ(kint64max, pwl->Value(-1));
  EXPECT_EQ(10, pwl->Value(0));
  EXPECT_EQ(kint64max, pwl->Value(1));
  EXPECT_EQ(kint64max, pwl->Value(kint64max));
}

// Tests a function with one right ray. Before the reference point it is
// always undefined (kint64max), after it, the value from the ray is returned.
TEST(PiecewiseLinearFunctionTest, OneRightRay) {
  std::unique_ptr<PiecewiseLinearFunction> pwl(
      PiecewiseLinearFunction::CreateRightRayFunction(0, 10, 1));
  EXPECT_EQ(kint64max, pwl->Value(kint64min));
  EXPECT_EQ(kint64max, pwl->Value(-10));
  EXPECT_EQ(10, pwl->Value(0));
  EXPECT_EQ(20, pwl->Value(10));
  EXPECT_EQ(kint64max, pwl->Value(kint64max - 10));
  EXPECT_EQ(kint64max, pwl->Value(kint64max));
}

// Tests a function with one left ray. After the reference point it is always
// undefined (kint64max), before it, the value from the ray is returned.
TEST(PiecewiseLinearFunctionTest, OneLeftRay) {
  std::unique_ptr<PiecewiseLinearFunction> pwl(
      PiecewiseLinearFunction::CreateLeftRayFunction(0, 10, -1));
  EXPECT_EQ(kint64max, pwl->Value(kint64min));
  EXPECT_EQ(kint64max, pwl->Value(kint64min + 10));
  EXPECT_EQ(20, pwl->Value(-10));
  EXPECT_EQ(10, pwl->Value(0));
  EXPECT_EQ(kint64max, pwl->Value(10));
  EXPECT_EQ(kint64max, pwl->Value(kint64max));
}

// Tests a piecewise linear function with one right ray and checks the case of
// overflow on max due to the initial point value.
TEST(PiecewiseLinearFunctionTest, OneRightRayOverflowOnMaxDueToInitialValue) {
  std::unique_ptr<PiecewiseLinearFunction> pwl(
      PiecewiseLinearFunction::CreateRightRayFunction(-10, 10, 1));
  EXPECT_EQ(kint64max, pwl->Value(kint64min));
  EXPECT_EQ(kint64max, pwl->Value(-20));
  EXPECT_EQ(10, pwl->Value(-10));
  EXPECT_EQ(20, pwl->Value(0));
  EXPECT_EQ(kint64max - 10, pwl->Value(kint64max - 30));
  EXPECT_EQ(kint64max, pwl->Value(kint64max - 20));
  EXPECT_EQ(kint64max, pwl->Value(kint64max - 10));
  EXPECT_EQ(kint64max, pwl->Value(kint64max));
}

// Tests a piecewise linear function with one left ray and checks the case of
// overflow on min due to the initial point value.
TEST(PiecewiseLinearFunctionTest, OneLeftRayOverflowOnMinDueToInitialValue) {
  std::unique_ptr<PiecewiseLinearFunction> pwl(
      PiecewiseLinearFunction::CreateLeftRayFunction(10, 0, 1));
  EXPECT_EQ(kint64min, pwl->Value(kint64min));
  EXPECT_EQ(kint64min, pwl->Value(kint64min + 5));
  EXPECT_EQ(kint64min, pwl->Value(kint64min + 10));
  EXPECT_EQ(kint64min + 10, pwl->Value(kint64min + 20));
  EXPECT_EQ(-10, pwl->Value(0));
  EXPECT_EQ(0, pwl->Value(10));
  EXPECT_EQ(kint64max, pwl->Value(20));
  EXPECT_EQ(kint64max, pwl->Value(kint64max));
}

// Tests a piecewise linear function with one right ray and checks the case of
// overflow on max due to the slope.
TEST(PiecewiseLinearFunctionTest, OneRightRayOverflowOnMaxDueToSlope) {
  std::unique_ptr<PiecewiseLinearFunction> pwl(
      PiecewiseLinearFunction::CreateRightRayFunction(0, 0, 2));
  EXPECT_EQ(kint64max, pwl->Value(kint64min));
  EXPECT_EQ(kint64max, pwl->Value(-1));
  EXPECT_EQ(0, pwl->Value(0));
  EXPECT_EQ(2, pwl->Value(1));
  EXPECT_EQ(kint64max - 1, pwl->Value(kint64max / 2));
  EXPECT_EQ(kint64max, pwl->Value(kint64max / 2 + 1));
  EXPECT_EQ(kint64max, pwl->Value(kint64max));
}

// Tests a piecewise linear function with one left ray and checks the case of
// overflow on min due to the slope.
TEST(PiecewiseLinearFunctionTest, OneLeftRayOverflowOnMinDueToSlope) {
  std::unique_ptr<PiecewiseLinearFunction> pwl(
      PiecewiseLinearFunction::CreateLeftRayFunction(0, 0, 2));
  EXPECT_EQ(kint64min, pwl->Value(kint64min));
  EXPECT_EQ(kint64min, pwl->Value(kint64min / 2));
  EXPECT_EQ(kint64min + 2, pwl->Value(kint64min / 2 + 1));
  EXPECT_EQ(-2, pwl->Value(-1));
  EXPECT_EQ(0, pwl->Value(0));
  EXPECT_EQ(kint64max, pwl->Value(1));
  EXPECT_EQ(kint64max, pwl->Value(kint64max));
}

// Tests the identity function.
TEST(PiecewiseLinearFunctionTest, IdentityFunction) {
  std::unique_ptr<PiecewiseLinearFunction> pwl(
      PiecewiseLinearFunction::CreateOneSegmentFunction(kint64min, kint64min, 1,
                                                        kint64max));
  EXPECT_EQ(kint64min, pwl->Value(kint64min));
  EXPECT_EQ(-10, pwl->Value(-10));
  EXPECT_EQ(0, pwl->Value(0));
  EXPECT_EQ(10, pwl->Value(10));
  EXPECT_EQ(kint64max - 10, pwl->Value(kint64max - 10));
  EXPECT_EQ(kint64max, pwl->Value(kint64max));
}

// Tests the inverse identity function.
TEST(PiecewiseLinearFunctionTest, InverseIdentityFunction) {
  std::unique_ptr<PiecewiseLinearFunction> pwl(
      PiecewiseLinearFunction::CreateOneSegmentFunction(
          kint64max, CapOpp(kint64max), -1, kint64min));
  EXPECT_EQ(kint64max, pwl->Value(kint64min));
  EXPECT_EQ(kint64max, pwl->Value(kint64min + 1));
  EXPECT_EQ(10, pwl->Value(-10));
  EXPECT_EQ(0, pwl->Value(0));
  EXPECT_EQ(-10, pwl->Value(10));
  EXPECT_EQ(kint64min + 1, pwl->Value(kint64max));
}

// Tests the fixed charge function.
TEST(PiecewiseLinearFunctionTest, FixedChargeFunction) {
  std::unique_ptr<PiecewiseLinearFunction> pwl(
      PiecewiseLinearFunction::CreateFixedChargeFunction(2, 10));
  EXPECT_EQ(0, pwl->Value(kint64min));
  EXPECT_EQ(0, pwl->Value(kint64min / 2));
  EXPECT_EQ(0, pwl->Value(-1));
  EXPECT_EQ(10, pwl->Value(0));
  EXPECT_EQ(30, pwl->Value(10));
  EXPECT_EQ(kint64max, pwl->Value(kint64max));
}

// Tests the earliness-tardiness function.
TEST(PiecewiseLinearFunctionTest, EarlinessTardinessFunction) {
  std::unique_ptr<PiecewiseLinearFunction> pwl(
      PiecewiseLinearFunction::CreateEarlyTardyFunction(100, 2, 1));
  EXPECT_EQ(kint64max, pwl->Value(kint64min));
  EXPECT_EQ(kint64max, pwl->Value(kint64min / 2));
  EXPECT_EQ(220, pwl->Value(-10));
  EXPECT_EQ(200, pwl->Value(0));
  EXPECT_EQ(0, pwl->Value(100));
  EXPECT_EQ(100, pwl->Value(200));
  EXPECT_EQ(kint64max - 100, pwl->Value(kint64max));
}

// Tests the earliness-tardiness function with slack.
TEST(PiecewiseLinearFunctionTest, EarlinessTardinessFunctionWithSlack) {
  std::unique_ptr<PiecewiseLinearFunction> pwl(
      PiecewiseLinearFunction::CreateEarlyTardyFunctionWithSlack(75, 150, 1,
                                                                 2));
  EXPECT_EQ(kint64max, pwl->Value(kint64min));
  EXPECT_EQ(kint64max, pwl->Value(kint64min + 75));
  EXPECT_EQ(100, pwl->Value(-25));
  EXPECT_EQ(75, pwl->Value(0));
  EXPECT_EQ(50, pwl->Value(25));
  EXPECT_EQ(0, pwl->Value(75));
  EXPECT_EQ(0, pwl->Value(100));
  EXPECT_EQ(0, pwl->Value(150));
  EXPECT_EQ(100, pwl->Value(200));
  EXPECT_EQ(kint64max - 301, pwl->Value(kint64max / 2));
}

// Tests that the step function without overlapping points works correctly.
TEST(PiecewiseLinearFunctionTest, StepFunctionNonOverlappingPoints) {
  // [-10, 0]: 0, [10, 90]: 2, [100, kint64max]: 1
  std::vector<int64_t> points_x = {-10, 10, 100};
  std::vector<int64_t> other_points_x = {0, 90, kint64max};
  std::vector<int64_t> points_y = {0, 2, 1};

  std::unique_ptr<PiecewiseLinearFunction> pwl(
      PiecewiseLinearFunction::CreateStepFunction(points_x, points_y,
                                                  other_points_x));
  EXPECT_EQ(kint64max, pwl->Value(kint64min));
  EXPECT_EQ(kint64max, pwl->Value(-100));
  EXPECT_EQ(0, pwl->Value(-10));
  EXPECT_EQ(0, pwl->Value(-5));
  EXPECT_EQ(0, pwl->Value(0));
  EXPECT_EQ(kint64max, pwl->Value(5));
  EXPECT_EQ(2, pwl->Value(10));
  EXPECT_EQ(2, pwl->Value(45));
  EXPECT_EQ(2, pwl->Value(90));
  EXPECT_EQ(kint64max, pwl->Value(95));
  EXPECT_EQ(1, pwl->Value(100));
  EXPECT_EQ(1, pwl->Value(1000));
  EXPECT_EQ(1, pwl->Value(kint64max));
}

// Tests the unit step function, where the two segments overlap at zero.
TEST(PiecewiseLinearFunctionTest, UnitStepFunctionOverlapAtZero) {
  // [kint64min, 0]: 0, [0, kint64max]: 1
  std::vector<int64_t> points_x = {0, 0};
  std::vector<int64_t> other_points_x = {kint64min, kint64max};
  std::vector<int64_t> points_y = {0, 1};

  std::unique_ptr<PiecewiseLinearFunction> pwl(
      PiecewiseLinearFunction::CreateStepFunction(points_x, points_y,
                                                  other_points_x));
  EXPECT_EQ(0, pwl->Value(kint64min));
  EXPECT_EQ(0, pwl->Value(-100));
  EXPECT_EQ(0, pwl->Value(-10));
  EXPECT_EQ(1, pwl->Value(0));
  EXPECT_EQ(1, pwl->Value(10));
  EXPECT_EQ(1, pwl->Value(100));
  EXPECT_EQ(1, pwl->Value(kint64max));
}

// Tests the full domain function.
TEST(PiecewiseLinearFunctionTest, FullDomainFunction) {
  const int64_t initial_level = 10;
  std::vector<int64_t> points_x = {0, 10, 20};
  std::vector<int64_t> slopes = {2, -1, 1, -2};

  std::unique_ptr<PiecewiseLinearFunction> pwl(
      PiecewiseLinearFunction::CreateFullDomainFunction(initial_level, points_x,
                                                        slopes));
  EXPECT_EQ(kint64min, pwl->Value(kint64min));
  EXPECT_EQ(kint64min + 10, pwl->Value(kint64min / 2));
  EXPECT_EQ(0, pwl->Value(-5));
  EXPECT_EQ(10, pwl->Value(0));
  EXPECT_EQ(5, pwl->Value(5));
  EXPECT_EQ(0, pwl->Value(10));
  EXPECT_EQ(5, pwl->Value(15));
  EXPECT_EQ(10, pwl->Value(20));
  EXPECT_EQ(0, pwl->Value(25));
  EXPECT_EQ(kint64min, pwl->Value(kint64max));
}

// Tests a piecewise linear function with three separate continuous parts
// and that the minimal representation is done correctly.
TEST(PiecewiseLinearFunctionTest, MinimalPiecewiseLinearFunction) {
  std::vector<int64_t> points_x = {0, 1, 2, 3, 4, 5, 6};
  std::vector<int64_t> points_y = {0, 1, 4, 5, 5, 2, 1};
  std::vector<int64_t> slopes = {1, 1, 1, 0, -1, -1, -1};
  std::vector<int64_t> other_points_x = {1, 2, 3, 4, 5, 6, 7};

  std::unique_ptr<PiecewiseLinearFunction> pwl(
      PiecewiseLinearFunction::CreatePiecewiseLinearFunction(
          points_x, points_y, slopes, other_points_x));

  EXPECT_EQ(5, pwl->segments().size());

  EXPECT_EQ(kint64max, pwl->Value(kint64min));
  EXPECT_EQ(0, pwl->Value(0));
  EXPECT_EQ(1, pwl->Value(1));
  EXPECT_EQ(4, pwl->Value(2));
  EXPECT_EQ(5, pwl->Value(3));
  EXPECT_EQ(5, pwl->Value(4));
  EXPECT_EQ(2, pwl->Value(5));
  EXPECT_EQ(1, pwl->Value(6));
  EXPECT_EQ(0, pwl->Value(7));
  EXPECT_EQ(kint64max, pwl->Value(kint64max));
}

// Tests that a construction of piecewise linear function providing
// overlapping segments, fails.
TEST(PiecewiseLinearFunctionTest, OverlappingSegmentsDeath) {
  const char* kErrorMessage = "Overlapping segments:*";
  std::vector<int64_t> points_x = {0, 1};
  std::vector<int64_t> points_y = {0, 1};
  std::vector<int64_t> slopes = {1, 0};
  std::vector<int64_t> other_points_x = {kint64max, kint64max};

  EXPECT_DEATH(PiecewiseLinearFunction::CreatePiecewiseLinearFunction(
                   points_x, points_y, slopes, other_points_x),
               kErrorMessage);
}

// Tests the translation of a cup-shaped piecewise linear function
// on the x axis.
TEST(PiecewiseLinearFunctionTest, AddConstantToX) {
  std::vector<int64_t> points_x = {-1, 1};
  std::vector<int64_t> slopes = {-1, 0, 1};

  std::unique_ptr<PiecewiseLinearFunction> pwl(
      PiecewiseLinearFunction::CreateFullDomainFunction(0, points_x, slopes));

  EXPECT_EQ(kint64max, pwl->Value(kint64min));
  EXPECT_EQ(9, pwl->Value(-10));
  EXPECT_EQ(0, pwl->Value(-1));
  EXPECT_EQ(0, pwl->Value(0));
  EXPECT_EQ(0, pwl->Value(1));
  EXPECT_EQ(9, pwl->Value(10));
  EXPECT_EQ(kint64max - 1, pwl->Value(kint64max));

  pwl->AddConstantToX(3);

  EXPECT_EQ(kint64max, pwl->Value(kint64min + 3));
  EXPECT_EQ(12, pwl->Value(-10));
  EXPECT_EQ(3, pwl->Value(-1));
  EXPECT_EQ(2, pwl->Value(0));
  EXPECT_EQ(1, pwl->Value(1));
  EXPECT_EQ(0, pwl->Value(3));
  EXPECT_EQ(6, pwl->Value(10));
  EXPECT_EQ(kint64max - 4, pwl->Value(kint64max));
}

// Tests the translation of a single segment on the x and y axis
// when overflow occurs.
TEST(PiecewiseLinearFunctionTest, AddConstantOverflow) {
  std::unique_ptr<PiecewiseLinearFunction> pwl(
      PiecewiseLinearFunction::CreateOneSegmentFunction(1, 1, 1, 2));

  EXPECT_EQ(kint64max, pwl->Value(0));
  EXPECT_EQ(1, pwl->Value(1));
  EXPECT_EQ(2, pwl->Value(2));
  EXPECT_EQ(kint64max, pwl->Value(3));

  pwl->AddConstantToX(kint64max);

  EXPECT_EQ(kint64max, pwl->Value(0));
  EXPECT_EQ(1, pwl->Value(1));
  EXPECT_EQ(2, pwl->Value(2));
  EXPECT_EQ(kint64max, pwl->Value(3));

  pwl->AddConstantToY(kint64max);

  EXPECT_EQ(kint64max, pwl->Value(0));
  EXPECT_EQ(1, pwl->Value(1));
  EXPECT_EQ(2, pwl->Value(2));
  EXPECT_EQ(kint64max, pwl->Value(3));
}

// Tests the translation of an inverse cup-shaped piecewise linear
// function on the y axis.
TEST(PiecewiseLinearFunctionTest, AddConstantToY) {
  std::vector<int64_t> points_x = {-1, 1};
  std::vector<int64_t> slopes = {1, 0, -1};

  std::unique_ptr<PiecewiseLinearFunction> pwl(
      PiecewiseLinearFunction::CreateFullDomainFunction(0, points_x, slopes));

  EXPECT_EQ(kint64min + 1, pwl->Value(kint64min));
  EXPECT_EQ(-9, pwl->Value(-10));
  EXPECT_EQ(0, pwl->Value(-1));
  EXPECT_EQ(0, pwl->Value(0));
  EXPECT_EQ(0, pwl->Value(1));
  EXPECT_EQ(-9, pwl->Value(10));
  EXPECT_EQ(kint64min + 2, pwl->Value(kint64max));

  pwl->AddConstantToY(-10);

  EXPECT_EQ(kint64min + 1, pwl->Value(kint64min + 10));
  EXPECT_EQ(-11, pwl->Value(-2));
  EXPECT_EQ(-10, pwl->Value(-1));
  EXPECT_EQ(-10, pwl->Value(0));
  EXPECT_EQ(-10, pwl->Value(1));
  EXPECT_EQ(-12, pwl->Value(3));
  EXPECT_EQ(-19, pwl->Value(10));
  EXPECT_EQ(kint64min + 2, pwl->Value(kint64max - 10));
}

// Tests that the addition of an M and a W shape function with center (0, 0)
// is zero for their common domain and undefined everywhere else.
TEST(PiecewiseLinearFunctionTest, AddBigMBigW) {
  std::vector<int64_t> m_points_x = {-2, -1, 0, 1};
  std::vector<int64_t> m_points_y = {-1, 1, 0, 1};
  std::vector<int64_t> m_slopes = {2, -1, 1, -2};
  std::vector<int64_t> other_m_points_x = {-1, 0, 1, 2};

  std::unique_ptr<PiecewiseLinearFunction> m_pwl(
      PiecewiseLinearFunction::CreatePiecewiseLinearFunction(
          m_points_x, m_points_y, m_slopes, other_m_points_x));

  std::vector<int64_t> w_points_x = {-2, -1, 0, 1};
  std::vector<int64_t> w_points_y = {1, -1, 0, -1};
  std::vector<int64_t> w_slopes = {-2, 1, -1, 2};
  std::vector<int64_t> other_w_points_x = {-1, 0, 1, 2};

  std::unique_ptr<PiecewiseLinearFunction> w_pwl(
      PiecewiseLinearFunction::CreatePiecewiseLinearFunction(
          w_points_x, w_points_y, w_slopes, other_w_points_x));

  m_pwl->Add(*w_pwl);

  EXPECT_EQ(1, m_pwl->segments().size());

  EXPECT_EQ(kint64max, m_pwl->Value(kint64min));
  EXPECT_EQ(kint64max, m_pwl->Value(-3));
  EXPECT_EQ(0, m_pwl->Value(-2));
  EXPECT_EQ(0, m_pwl->Value(-1));
  EXPECT_EQ(0, m_pwl->Value(0));
  EXPECT_EQ(0, m_pwl->Value(1));
  EXPECT_EQ(0, m_pwl->Value(2));
  EXPECT_EQ(kint64max, m_pwl->Value(3));
  EXPECT_EQ(kint64max, m_pwl->Value(kint64max));
}

// Tests that the subtraction of an M and an M shape function with center (0, 0)
// is zero for their common domain and undefined everywhere else.
TEST(PiecewiseLinearFunctionTest, SubtractBigMBigM) {
  std::vector<int64_t> m_points_x = {-2, -1, 0, 1};
  std::vector<int64_t> m_points_y = {-1, 1, 0, 1};
  std::vector<int64_t> m_slopes = {2, -1, 1, -2};
  std::vector<int64_t> other_m_points_x = {-1, 0, 1, 2};

  std::unique_ptr<PiecewiseLinearFunction> m1_pwl(
      PiecewiseLinearFunction::CreatePiecewiseLinearFunction(
          m_points_x, m_points_y, m_slopes, other_m_points_x));
  std::unique_ptr<PiecewiseLinearFunction> m2_pwl(
      PiecewiseLinearFunction::CreatePiecewiseLinearFunction(
          m_points_x, m_points_y, m_slopes, other_m_points_x));

  m1_pwl->Subtract(*m2_pwl);

  EXPECT_EQ(1, m1_pwl->segments().size());

  EXPECT_EQ(kint64max, m1_pwl->Value(kint64min));
  EXPECT_EQ(kint64max, m1_pwl->Value(-3));
  EXPECT_EQ(0, m1_pwl->Value(-2));
  EXPECT_EQ(0, m1_pwl->Value(-1));
  EXPECT_EQ(0, m1_pwl->Value(0));
  EXPECT_EQ(0, m1_pwl->Value(1));
  EXPECT_EQ(0, m1_pwl->Value(2));
  EXPECT_EQ(kint64max, m1_pwl->Value(3));
  EXPECT_EQ(kint64max, m1_pwl->Value(kint64max));
}

// Tests addition with a function that causes overflow of the reference point.
TEST(PiecewiseLinearFunctionTest, OverflowReferenceOnAddition) {
  std::unique_ptr<PiecewiseLinearFunction> initial(
      PiecewiseLinearFunction::CreateOneSegmentFunction(kint64min + 10,
                                                        kint64min + 10, 1, 0));
  std::unique_ptr<PiecewiseLinearFunction> other(
      PiecewiseLinearFunction::CreateOneSegmentFunction(5, 15, 0, kint64min));

  initial->Subtract(*other);

  EXPECT_EQ(kint64max, initial->Value(kint64min));
  EXPECT_EQ(kint64min, initial->Value(kint64min + 10));
  EXPECT_EQ(kint64min, initial->Value(kint64min + 15));
  EXPECT_EQ(kint64min + 5, initial->Value(kint64min + 20));
  EXPECT_EQ(-15, initial->Value(0));
  EXPECT_EQ(kint64max, initial->Value(1));
  EXPECT_EQ(kint64max, initial->Value(kint64max));
}

// Tests that the addition of one left ray and one right ray with same slopes
// originated to zero results to (0, 0) point (and not the identity function).
TEST(PiecewiseLinearFunctionTest, AddTwoOppositeRays) {
  std::unique_ptr<PiecewiseLinearFunction> right_ray(
      PiecewiseLinearFunction::CreateRightRayFunction(0, 0, 1));
  std::unique_ptr<PiecewiseLinearFunction> left_ray(
      PiecewiseLinearFunction::CreateLeftRayFunction(0, 0, 1));

  right_ray->Add(*left_ray);

  EXPECT_EQ(1, right_ray->segments().size());

  EXPECT_EQ(kint64max, right_ray->Value(kint64min));
  EXPECT_EQ(kint64max, right_ray->Value(-100));
  EXPECT_EQ(kint64max, right_ray->Value(-1));
  EXPECT_EQ(0, right_ray->Value(0));
  EXPECT_EQ(kint64max, right_ray->Value(1));
  EXPECT_EQ(kint64max, right_ray->Value(100));
  EXPECT_EQ(kint64max, right_ray->Value(kint64max));
}

// Tests the local optimums in a W shaped full-domain function.
TEST(PiecewiseLinearFunctionTest, LocalOptimumsBigW) {
  std::vector<int64_t> points_x = {-2, 0, 2};
  std::vector<int64_t> slopes = {-2, 1, -1, 2};

  std::unique_ptr<PiecewiseLinearFunction> pwl(
      PiecewiseLinearFunction::CreateFullDomainFunction(-1, points_x, slopes));

  // Value Testing
  EXPECT_EQ(-1, pwl->Value(-2));
  EXPECT_EQ(1, pwl->Value(0));
  EXPECT_EQ(-1, pwl->Value(2));

  // Optimum Testing
  EXPECT_EQ(-1, pwl->GetMinimum());
  EXPECT_EQ(kint64max, pwl->GetMaximum());
  EXPECT_EQ(-1, pwl->GetMinimum(-2, 2));
  EXPECT_EQ(1, pwl->GetMaximum(-2, 2));
  EXPECT_EQ(0, pwl->GetMinimum(-1, 1));
  EXPECT_EQ(1, pwl->GetMaximum(-1, 1));
  EXPECT_EQ(3, pwl->GetMinimum(4, 5));
  EXPECT_EQ(5, pwl->GetMaximum(4, 5));
  EXPECT_EQ(std::make_pair(kint64max, kint64min),
            pwl->GetSmallestRangeGreaterThanValue(3, -3, 0));
  EXPECT_EQ(std::make_pair(kint64max, kint64min),
            pwl->GetSmallestRangeLessThanValue(3, -3, 0));
  EXPECT_EQ(std::make_pair(kint64max, kint64min),
            pwl->GetSmallestRangeInValueRange(3, -3, 0, 2));
  EXPECT_EQ(std::make_pair(int64_t{-3}, int64_t{3}),
            pwl->GetSmallestRangeGreaterThanValue(-3, 3, 0));
  EXPECT_EQ(std::make_pair(int64_t{-2}, int64_t{2}),
            pwl->GetSmallestRangeLessThanValue(-3, 3, 0));
  EXPECT_EQ(std::make_pair(int64_t{-4}, int64_t{4}),
            pwl->GetSmallestRangeInValueRange(-10, 10, 3, 3));
  EXPECT_EQ(std::make_pair(kint64max, kint64min),
            pwl->GetSmallestRangeInValueRange(-3, 3, 2, 2));
  EXPECT_EQ(std::make_pair(int64_t{-3}, int64_t{3}),
            pwl->GetSmallestRangeInValueRange(-4, 4, -1, 1));
  EXPECT_EQ(std::make_pair(int64_t{-3}, int64_t{3}),
            pwl->GetSmallestRangeGreaterThanValue(-3, 3, 1));
  EXPECT_EQ(std::make_pair(int64_t{-3}, int64_t{3}),
            pwl->GetSmallestRangeLessThanValue(-3, 3, 1));
  EXPECT_EQ(std::make_pair(int64_t{-1}, int64_t{1}),
            pwl->GetSmallestRangeGreaterThanValue(-2, 2, 0));
  EXPECT_EQ(std::make_pair(int64_t{-2}, int64_t{2}),
            pwl->GetSmallestRangeLessThanValue(-2, 2, 0));
  EXPECT_EQ(std::make_pair(int64_t{0}, int64_t{0}),
            pwl->GetSmallestRangeGreaterThanValue(-2, 2, 1));
  EXPECT_EQ(std::make_pair(int64_t{-2}, int64_t{2}),
            pwl->GetSmallestRangeLessThanValue(-2, 2, 1));
  EXPECT_EQ(std::make_pair(kint64max, kint64min),
            pwl->GetSmallestRangeGreaterThanValue(-2, 2, 2));
  EXPECT_EQ(std::make_pair(int64_t{-2}, int64_t{2}),
            pwl->GetSmallestRangeLessThanValue(-2, 2, 2));
  EXPECT_EQ(std::make_pair(kint64min, kint64max),
            pwl->GetSmallestRangeGreaterThanValue(kint64min, kint64max, 20));
  EXPECT_EQ(std::make_pair(int64_t{-12}, int64_t{12}),
            pwl->GetSmallestRangeLessThanValue(kint64min, kint64max, 20));
  EXPECT_EQ(std::make_pair(int64_t{-20}, int64_t{-10}),
            pwl->GetSmallestRangeGreaterThanValue(-20, -10, 5));
  EXPECT_EQ(std::make_pair(int64_t{-12}, int64_t{-10}),
            pwl->GetSmallestRangeLessThanValue(-20, -10, 20));
  EXPECT_EQ(std::make_pair(int64_t{-20}, int64_t{-13}),
            pwl->GetSmallestRangeGreaterThanValue(-20, -10, 20));
  EXPECT_EQ(std::make_pair(kint64max, kint64min),
            pwl->GetSmallestRangeLessThanValue(-20, -10, 5));
  EXPECT_EQ(std::make_pair(int64_t{10}, int64_t{20}),
            pwl->GetSmallestRangeGreaterThanValue(10, 20, 5));
  EXPECT_EQ(std::make_pair(kint64max, kint64min),
            pwl->GetSmallestRangeLessThanValue(10, 20, 5));
  EXPECT_EQ(std::make_pair(int64_t{13}, int64_t{20}),
            pwl->GetSmallestRangeGreaterThanValue(10, 20, 20));
  EXPECT_EQ(std::make_pair(int64_t{10}, int64_t{12}),
            pwl->GetSmallestRangeLessThanValue(10, 20, 20));
}

TEST(PiecewiseLinearFunctionTest, LocalOptimumsFlat) {
  std::unique_ptr<PiecewiseLinearFunction> pwl(
      PiecewiseLinearFunction::CreateStepFunction({kint64min}, {0},
                                                  {kint64max}));
  EXPECT_EQ(0, pwl->Value(0));
  EXPECT_EQ(0, pwl->GetMinimum());
  EXPECT_EQ(0, pwl->GetMaximum());
  EXPECT_EQ(std::make_pair(kint64min, kint64max),
            pwl->GetSmallestRangeGreaterThanValue(kint64min, kint64max, 0));
  EXPECT_EQ(std::make_pair(kint64min, kint64max),
            pwl->GetSmallestRangeLessThanValue(kint64min, kint64max, 0));
}

TEST(PiecewiseLinearFunctionTest, LocalOptimumsFlatBigM) {
  std::unique_ptr<PiecewiseLinearFunction> pwl(
      PiecewiseLinearFunction::CreateStepFunction({kint64min}, {kint64max},
                                                  {kint64max}));
  EXPECT_EQ(kint64max, pwl->Value(0));
  EXPECT_EQ(kint64max, pwl->GetMinimum());
  EXPECT_EQ(kint64max, pwl->GetMaximum());
  EXPECT_EQ(
      std::make_pair(kint64min, kint64max),
      pwl->GetSmallestRangeGreaterThanValue(kint64min, kint64max, kint64max));
  EXPECT_EQ(
      std::make_pair(kint64min, kint64max),
      pwl->GetSmallestRangeLessThanValue(kint64min, kint64max, kint64max));
}

// Tests the local optimums in a two finite segment piecewise
// linear function with domain holes.
TEST(PiecewiseLinearFunctionTest, LocalOptimumsLimitedDomain) {
  std::vector<int64_t> points_x = {-4, 2};
  std::vector<int64_t> points_y = {0, 0};
  std::vector<int64_t> slopes = {1, -1};
  std::vector<int64_t> other_points_x = {-2, 4};

  std::unique_ptr<PiecewiseLinearFunction> pwl(
      PiecewiseLinearFunction::CreatePiecewiseLinearFunction(
          points_x, points_y, slopes, other_points_x));

  // Value Testing
  EXPECT_EQ(1, pwl->Value(-3));
  EXPECT_EQ(-1, pwl->Value(3));

  // Optimum Testing
  EXPECT_EQ(-2, pwl->GetMinimum());
  EXPECT_EQ(2, pwl->GetMaximum());
  EXPECT_EQ(kint64max, pwl->GetMinimum(-20, -10));
  EXPECT_EQ(kint64max, pwl->GetMaximum(-20, -10));
  EXPECT_EQ(0, pwl->GetMinimum(-5, 0));
  EXPECT_EQ(2, pwl->GetMaximum(-5, 0));
  EXPECT_EQ(kint64max, pwl->GetMinimum(-1, 1));
  EXPECT_EQ(kint64max, pwl->GetMaximum(-1, 1));
  EXPECT_EQ(-2, pwl->GetMinimum(0, 5));
  EXPECT_EQ(0, pwl->GetMaximum(0, 5));
  EXPECT_EQ(kint64max, pwl->GetMinimum(10, 20));
  EXPECT_EQ(kint64max, pwl->GetMaximum(10, 20));
  EXPECT_EQ(std::make_pair(kint64max, kint64min),
            pwl->GetSmallestRangeGreaterThanValue(-1, 1, 0));
  EXPECT_EQ(std::make_pair(kint64max, kint64min),
            pwl->GetSmallestRangeLessThanValue(-1, 1, 0));
  EXPECT_EQ(std::make_pair(kint64max, kint64min),
            pwl->GetSmallestRangeGreaterThanValue(-10, -5, 0));
  EXPECT_EQ(std::make_pair(kint64max, kint64min),
            pwl->GetSmallestRangeLessThanValue(-10, -5, 0));
  EXPECT_EQ(std::make_pair(kint64max, kint64min),
            pwl->GetSmallestRangeGreaterThanValue(5, 10, 0));
  EXPECT_EQ(std::make_pair(kint64max, kint64min),
            pwl->GetSmallestRangeLessThanValue(5, 10, 0));
}

// Tests the decomposition of a non-convex function to a set of convex
// piecewise linear functions.
TEST(PiecewiseLinearFunctionTest, DecomposeNonConvex) {
  std::vector<int64_t> points_x = {-7, -6, -6, -4, -3, -2, -2, -1,
                                   0,  2,  3,  3,  5,  5,  6,  7};
  std::vector<int64_t> points_y = {4, 3, 3, 2, 1, 0, 0, 1,
                                   3, 5, 3, 3, 2, 2, 3, 5};
  std::vector<int64_t> slopes = {-2, -1, 1,  -2, -1, -1, 1, 2,
                                 2,  0,  -2, -1, 0,  1,  2, -1};
  std::vector<int64_t> other_points_x = {
      kint64min, -7, -5, -5, -4, -3, -1, 0, 1, 1, 2, 4, 4, 6, 7, kint64max};

  std::unique_ptr<PiecewiseLinearFunction> pwl(
      PiecewiseLinearFunction::CreatePiecewiseLinearFunction(
          points_x, points_y, slopes, other_points_x));

  std::vector<PiecewiseLinearFunction*> convex_functions =
      pwl->DecomposeToConvexFunctions();
  EXPECT_EQ(convex_functions.size(), 5);

  EXPECT_EQ(3, convex_functions[0]->GetMinimum());
  EXPECT_EQ(0, convex_functions[1]->GetMinimum());
  EXPECT_EQ(5, convex_functions[2]->GetMinimum());
  EXPECT_EQ(2, convex_functions[3]->GetMinimum());

  gtl::STLDeleteElements(&convex_functions);
}

// Tests that the decomposition of a convex function returns the same
// function.
TEST(PiecewiseLinearFunctionTest, DecomposeConvex) {
  std::vector<int64_t> points_x = {3, 3, 5, 5, 6};
  std::vector<int64_t> points_y = {3, 3, 2, 2, 3};
  std::vector<int64_t> slopes = {-2, -1, 0, 1, 2};
  std::vector<int64_t> other_points_x = {2, 4, 4, 6, 7};

  std::unique_ptr<PiecewiseLinearFunction> pwl(
      PiecewiseLinearFunction::CreatePiecewiseLinearFunction(
          points_x, points_y, slopes, other_points_x));

  std::vector<PiecewiseLinearFunction*> convex_functions =
      pwl->DecomposeToConvexFunctions();
  EXPECT_EQ(convex_functions.size(), 1);
  EXPECT_EQ(convex_functions[0]->segments().size(), 5);

  EXPECT_EQ(2, convex_functions[0]->GetMinimum());
  EXPECT_EQ(5, convex_functions[0]->GetMaximum());

  gtl::STLDeleteElements(&convex_functions);
}

// Tests the debug string.
TEST(PiecewiseLinearFunctionTest, DebugString) {
  std::unique_ptr<PiecewiseLinearFunction> pwl(
      PiecewiseLinearFunction::CreateLeftRayFunction(0, 10, 1));
  EXPECT_EQ(
      "PiecewiseLinearFunction(PiecewiseSegment("
      "<start: (-9223372036854775808, -9223372036854775798), "
      "end: (0, 10), reference: (0, 10), slope = 1>) ",
      pwl->DebugString());
}

// The following tests are related to the FloatSlopePiecewiseLinearFunction.

TEST(FloatSlopePiecewiseLinearFunctionTest, CorrectInBoundValues) {
  const FloatSlopePiecewiseLinearFunction pwl(
      /*x_anchors=*/{0, 10, 20, 40, 50, 70, 100},
      /*y_anchors=*/{20, 30, 25, 65, 45, 50, 20});
  const std::vector<int64_t> x = {0, 5, 15, 35, 45, 48, 60, 80, 100};
  const std::vector<int64_t> kExpectedResults = {20, 25, 28, 55, 55,
                                                 49, 48, 40, 20};
  for (int i = 0; i < x.size(); ++i) {
    EXPECT_EQ(pwl.ComputeInBoundsValue(x[i]), kExpectedResults[i]);
    EXPECT_EQ(pwl.ComputeConvexValue(x[i]), kExpectedResults[i]);
  }
}

TEST(FloatSlopePiecewiseLinearFunctionTest, SameInBoundsAndConvexValue) {
  random_engine_t random;
  constexpr int kNumPwl = DEBUG_MODE ? 50 : 100;
  constexpr int kNumTestsPerPwl = DEBUG_MODE ? 50 : 100;
  for (int i = 0; i < kNumPwl; ++i) {
    const int num_segments = absl::Uniform(random, 2, 50);
    const int64_t xmax = absl::Uniform(random, num_segments, 1000);
    auto pwl = RandomPiecewiseLinearFunctionFixedNbSegments(
        /*x_min=*/0, xmax, /*y_min=*/10, /*y_max=*/1000, num_segments, random);

    const auto& x_anchors = pwl.x_anchors();
    const int64_t dx = x_anchors.back() - x_anchors.front();
    const int64_t x_lower_bound = x_anchors.front() - dx;
    const int64_t x_upper_bound = x_anchors.back() + dx;
    for (int j = 0; j < kNumTestsPerPwl; ++j) {
      const int x = absl::Uniform(random, x_lower_bound, x_upper_bound);
      const int64_t value = pwl.ComputeInBoundsValue(x);
      if (x < x_anchors.front() || x > x_anchors.back()) {
        EXPECT_EQ(value, -1);
      } else {
        EXPECT_EQ(value, pwl.ComputeConvexValue(x));
      }
    }
  }
}

}  // namespace
}  // namespace operations_research
