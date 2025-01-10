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

#include "ortools/pdlp/test_util.h"

#include <cmath>
#include <deque>
#include <limits>
#include <list>
#include <vector>

#include "Eigen/Core"
#include "absl/log/check.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"

namespace operations_research::pdlp {
namespace {

using ::absl::Span;
using ::Eigen::ColMajor;
using ::Eigen::Dynamic;
using ::Eigen::RowMajor;
using ::testing::Matcher;
using ::testing::Not;

TEST(FloatArrayNearTest, TypicalUse) {
  // M_PI is problematic on windows (requires _USE_MATH_DEFINES).
  // std::numbers::pi is C++20 (incompatible with OR-Tools).
  const double kPi = 3.14159265358979323846;
  std::vector<double> test_vector({0.998, -1.414, 3.142});
  std::vector<double> reference_vector({1.0, -std::sqrt(2), kPi});
  EXPECT_THAT(test_vector, FloatArrayNear(reference_vector, 1.0e-2));
  EXPECT_THAT(test_vector, Not(FloatArrayNear(reference_vector, 1.0e-4)));
}

template <typename ContainerType>
class FloatArrayNearContainerTypeTest : public testing::Test {};

using TestContainerTypes = testing::Types<std::vector<float>, std::deque<float>,
                                          std::list<float>, Span<const float>>;
TYPED_TEST_SUITE(FloatArrayNearContainerTypeTest, TestContainerTypes);

TYPED_TEST(FloatArrayNearContainerTypeTest, MatchesApproximately) {
  using ContainerType = TypeParam;
  auto test_values = {0.505f, 1.0f, -0.992f, 1.995f};
  ContainerType test_container(test_values);
  auto reference_values = {0.5f, 1.0f, -1.0f, 2.0f};
  ContainerType reference_container(reference_values);

  const Matcher<ContainerType> m1 = FloatArrayNear(reference_container, 1.0e-2);
  EXPECT_TRUE(m1.Matches(test_container));
  const Matcher<ContainerType> m2 = FloatArrayNear(reference_container, 1.0e-3);
  EXPECT_FALSE(m2.Matches(test_container));
}

TYPED_TEST(FloatArrayNearContainerTypeTest, DoesNotMatchWrongSize) {
  using ContainerType = TypeParam;
  EXPECT_THAT(ContainerType({1.0f, 2.0f}),
              Not(FloatArrayNear(ContainerType({1.0f, 2.0f, 3.0f}), 1.0e-2)));
}

TYPED_TEST(FloatArrayNearContainerTypeTest, DoesNotMatchWrongOrder) {
  using ContainerType = TypeParam;
  EXPECT_THAT(ContainerType({1.0f, 3.0f, 2.0f}),
              Not(FloatArrayNear(ContainerType({1.0f, 2.0f, 3.0f}), 1.0e-2)));
}

TYPED_TEST(FloatArrayNearContainerTypeTest, DoesNotMatchNaNs) {
  using ContainerType = TypeParam;
  auto test_values = {1.0f, std::numeric_limits<float>::quiet_NaN()};
  ContainerType test_container(test_values);

  EXPECT_THAT(test_container,
              Not(FloatArrayNear(ContainerType({1.0f, 2.0f}), 1.0e0)));
  EXPECT_THAT(test_container, Not(FloatArrayNear(test_container, 1.0e0)));
}

TEST(FloatArrayNearTest, WithIntegerElements) {
  std::vector<int> test_vector({505, 1000, -992, 1990});
  std::vector<int> reference_vector({500, 1000, -1000, 2000});

  const Matcher<std::vector<int>> m1 = FloatArrayNear(reference_vector, 10);
  EXPECT_TRUE(m1.Matches(test_vector));
  const Matcher<std::vector<int>> m2 = FloatArrayNear(reference_vector, 1);
  EXPECT_FALSE(m2.Matches(test_vector));
}

TEST(FloatArrayEqTest, TypicalUse) {
  // M_PI is problematic on windows (requires _USE_MATH_DEFINES).
  // std::numbers::pi is C++20 (incompatible with OR-Tools).
  const float kPi = 3.14159265358979323846;
  const float kSqrt2 = std::sqrt(2);
  std::vector<float> reference_vector({1.0e6, -kSqrt2, kPi});
  // Values are within 4 ULPs.
  std::vector<float> test_vector({1.0e6 + 0.25, -1.41421323, 3.14159262});
  EXPECT_THAT(test_vector, FloatArrayEq(reference_vector));
  // Create a difference of 5 ULPs in the first element.
  test_vector[0] = 1.0e6 + 0.3125;
  EXPECT_THAT(test_vector, Not(FloatArrayEq(reference_vector)));
}

template <typename ContainerType>
class FloatArrayEqContainerTypeTest : public testing::Test {};

TYPED_TEST_SUITE(FloatArrayEqContainerTypeTest, TestContainerTypes);

TYPED_TEST(FloatArrayEqContainerTypeTest, MatchesApproximately) {
  using ContainerType = TypeParam;
  auto reference_values = {-1.0e6f, 0.0f, 1.0f};
  ContainerType reference_container(reference_values);
  const Matcher<ContainerType> m = FloatArrayEq(reference_container);
  EXPECT_TRUE(m.Matches(reference_container));
  EXPECT_TRUE(m.Matches(ContainerType({-1.0e6 + 0.25, 5.0e-45, 1.0000002})));
  EXPECT_TRUE(m.Matches(ContainerType({-1.0e6 - 0.25, -5.0e-45, 0.9999998})));
  EXPECT_FALSE(m.Matches(ContainerType({-1.0e6 + 0.3125, 0.0, 1.0})));
  EXPECT_FALSE(m.Matches(ContainerType({-1.0e6, 1.0e-44, 1.0})));
  EXPECT_FALSE(m.Matches(ContainerType({-1.0e6, 0.0, 1.0000006})));
}

TYPED_TEST(FloatArrayEqContainerTypeTest, DoesNotMatchWrongSize) {
  using ContainerType = TypeParam;
  EXPECT_THAT(ContainerType({1.0f, 2.0f}),
              Not(FloatArrayEq(ContainerType({1.0f, 2.0f, 3.0f}))));
}

TYPED_TEST(FloatArrayEqContainerTypeTest, DoesNotMatchWrongOrder) {
  using ContainerType = TypeParam;
  EXPECT_THAT(ContainerType({1.0f, 3.0f, 2.0f}),
              Not(FloatArrayEq(ContainerType({1.0f, 2.0f, 3.0f}))));
}

TYPED_TEST(FloatArrayEqContainerTypeTest, DoesNotMatchNaNs) {
  using ContainerType = TypeParam;
  auto reference_values = {1.0f, std::numeric_limits<float>::quiet_NaN()};
  ContainerType reference_container(reference_values);
  const Matcher<ContainerType> m = FloatArrayEq(reference_container);
  EXPECT_FALSE(m.Matches(reference_container));
  EXPECT_FALSE(m.Matches(ContainerType({1.0f, 2.0f})));
}

TYPED_TEST(FloatArrayEqContainerTypeTest, HandlesInfinities) {
  using ContainerType = TypeParam;
  auto reference_values = {1.0f, std::numeric_limits<float>::infinity(),
                           -std::numeric_limits<float>::infinity()};
  ContainerType reference_container(reference_values);
  const Matcher<ContainerType> m = FloatArrayEq(reference_container);
  EXPECT_TRUE(m.Matches(reference_container));
  EXPECT_FALSE(m.Matches(ContainerType({1.0f, 2.0f, 3.0f})));
}

static const double kEps = 1.0e-6;

TEST(EigenArrayNearTest, ArrayXd) {
  const Eigen::ArrayXd expected = Eigen::ArrayXd::Random(4);
  Eigen::ArrayXd actual = expected;
  EXPECT_THAT(actual, EigenArrayNear(expected, kEps));
  EXPECT_THAT(actual, EigenArrayNear(expected, 1.0e-100));

  actual += 100;
  EXPECT_THAT(actual, Not(EigenArrayNear(expected, kEps)));
  // Wrong shape.
  actual.resize(2);
  EXPECT_THAT(actual, Not(EigenArrayNear(expected, kEps)));
}

TEST(EigenArrayNearTest, ArrayXdInlinedValues) {
  Eigen::ArrayXd actual(3);
  actual << 1.0, 2.0, 3.0;
  EXPECT_THAT(actual, EigenArrayNear<double>({1.0, 2.0, 3.0}, kEps));
  EXPECT_THAT(actual,
              EigenArrayNear<double>({1.0, 2.0 + 0.5 * kEps, 3.0}, kEps));

  EXPECT_THAT(actual, Not(EigenArrayNear<double>({1.0, 2.0, 5.0}, kEps)));
  // Wrong shape.
  EXPECT_THAT(actual, Not(EigenArrayNear<double>({1.0, 2.0}, kEps)));
}

TEST(EigenArrayNearTest, EmptyArrayX) {
  const Eigen::ArrayXi empty;
  EXPECT_THAT(empty, EigenArrayNear(empty, kEps));
  // Can pass in an Eigen expression type.
  EXPECT_THAT(empty, EigenArrayNear(Eigen::ArrayXi(), kEps));

  EXPECT_THAT(empty, Not(EigenArrayNear<int>({1, 2}, kEps)));
  EXPECT_THAT(empty, Not(EigenArrayNear(Eigen::ArrayXi::Zero(3), kEps)));
}

TEST(EigenArrayNearTest, ArrayXXf) {
  const Eigen::ArrayXXf expected = Eigen::ArrayXXf::Random(4, 5);
  Eigen::ArrayXXf actual = expected;
  EXPECT_THAT(actual, EigenArrayNear(expected, kEps));
  EXPECT_THAT(actual, EigenArrayNear(expected, 1.0e-100));

  actual.row(2) += 100;
  EXPECT_THAT(actual, Not(EigenArrayNear(expected, kEps)));
  // Wrong shape.
  EXPECT_THAT(expected, Not(EigenArrayNear(expected.transpose(), kEps)));
  actual.resize(4, 3);
  EXPECT_THAT(actual, Not(EigenArrayNear(expected, kEps)));

  // Expression type.
  actual.resize(3, 2);
  actual.col(0) << 1.0, 2.0, 3.0;
  actual.col(1) << 4.0, 5.0, 6.0;
  std::vector<float> expected_vector({1.0, 2.0, 3.0, 4.0, 5.0, 6.0});
  EXPECT_THAT(actual, EigenArrayNear(Eigen::Map<Eigen::ArrayXXf>(
                                         &expected_vector[0], /*rows=*/3,
                                         /*cols=*/2),
                                     kEps));
  // Wrong shape.
  EXPECT_THAT(actual, Not(EigenArrayNear(Eigen::Map<Eigen::ArrayXXf>(
                                             &expected_vector[0], /*rows=*/3,
                                             /*cols=*/1),
                                         kEps)));
}

TEST(EigenArrayNearTest, DifferentMajor) {
  Eigen::Array<float, Dynamic, Dynamic, ColMajor> col_major(2, 3);
  col_major << 1.0, 2.0, 3.0, 4.0, 5.0, 6.0;
  Eigen::Array<float, Dynamic, Dynamic, RowMajor> row_major(2, 3);
  row_major << 1.0, 2.0, 3.0, 4.0, 5.0, 6.0;
  CHECK_EQ(col_major(1, 0), row_major(1, 0));

  EXPECT_THAT(row_major, EigenArrayNear(col_major, 0.0));
  EXPECT_THAT(row_major, EigenArrayNear<float>({{1.0, 2.0, 3.0},  //
                                                {4.0, 5.0, 6.0}},
                                               0.0));
  EXPECT_THAT(col_major, EigenArrayNear(row_major, 0.0));
  EXPECT_THAT(col_major, EigenArrayNear<float>({{1.0, 2.0, 3.0},  //
                                                {4.0, 5.0, 6.0}},
                                               0.0));
}

TEST(EigenArrayNearTest, ArrayXXfInlinedValues) {
  Eigen::ArrayXXf actual(2, 3);
  actual.row(0) << 1.0, 2.0, 3.0;
  actual.row(1) << 4.0, -5.0, -6.0;

  EXPECT_THAT(actual, EigenArrayNear<float>({{1.0, 2.0, 3.0},  //
                                             {4.0, -5.0, -6.0}},
                                            kEps));
  EXPECT_THAT(actual, EigenArrayNear<float>(
                          {{1.0, 2.0, 3.0},
                           {4.0, -5.0, static_cast<float>(-6.0 - 0.9 * kEps)}},
                          kEps));
  EXPECT_THAT(actual, Not(EigenArrayNear<float>({{1.0, 2.0, 3.0},  //
                                                 {4.0, -5.0, -8.0}},
                                                kEps)));
  // Wrong shape.
  EXPECT_THAT(actual, Not(EigenArrayNear<float>({{1.0, 2.0, 3.0}}, kEps)));
}

TEST(EigenArrayEqTest, ArrayXd) {
  const Eigen::ArrayXd expected = Eigen::ArrayXd::Random(4);
  Eigen::ArrayXd actual = expected;
  EXPECT_THAT(actual, EigenArrayEq(expected));

  actual += 100;
  EXPECT_THAT(actual, Not(EigenArrayEq(expected)));
  // Wrong shape.
  actual.resize(2);
  EXPECT_THAT(actual, Not(EigenArrayEq(expected)));
}

TEST(EigenArrayEqTest, ArrayXdInlinedValues) {
  Eigen::ArrayXd actual(3);
  actual << 1.0, 2.0, 3.0;
  EXPECT_THAT(actual, EigenArrayEq<double>({1.0, 2.0, 3.0}));
  EXPECT_THAT(actual, EigenArrayEq<double>({1.0, 2.0 + 5.0e-7, 3.0}));

  EXPECT_THAT(actual, Not(EigenArrayEq<double>({1.0, 2.0, 5.0})));
  // Wrong shape.
  EXPECT_THAT(actual, Not(EigenArrayEq<double>({1.0, 2.0})));
}

TEST(EigenArrayEqTest, EmptyArrayX) {
  const Eigen::ArrayXi empty;
  EXPECT_THAT(empty, EigenArrayEq(empty));
  // Can pass in an Eigen expression type.
  EXPECT_THAT(empty, EigenArrayEq(Eigen::ArrayXi()));

  EXPECT_THAT(empty, Not(EigenArrayEq<int>({1, 2})));
  EXPECT_THAT(empty, Not(EigenArrayEq(Eigen::ArrayXi::Zero(3))));
}

TEST(EigenArrayEqTest, ArrayXXf) {
  const Eigen::ArrayXXf expected = Eigen::ArrayXXf::Random(4, 5);
  Eigen::ArrayXXf actual = expected;
  EXPECT_THAT(actual, EigenArrayEq(expected));

  actual.row(2) += 100;
  EXPECT_THAT(actual, Not(EigenArrayEq(expected)));
  // Wrong shape.
  EXPECT_THAT(expected, Not(EigenArrayEq(expected.transpose())));
  actual.resize(4, 3);
  EXPECT_THAT(actual, Not(EigenArrayEq(expected)));

  // Expression type.
  actual.resize(3, 2);
  actual.col(0) << 1.0, 2.0, 3.0;
  actual.col(1) << 4.0, 5.0, 6.0;
  std::vector<float> expected_vector({1.0, 2.0, 3.0, 4.0, 5.0, 6.0});
  EXPECT_THAT(actual, EigenArrayEq(Eigen::Map<Eigen::ArrayXXf>(
                          &expected_vector[0], /*rows=*/3, /*cols=*/2)));
  // Wrong shape.
  EXPECT_THAT(actual, Not(EigenArrayEq(Eigen::Map<Eigen::ArrayXXf>(
                          &expected_vector[0], /*rows=*/3, /*cols=*/1))));
}

TEST(EigenArrayEqTest, ArrayXXfInlinedValues) {
  Eigen::ArrayXXf actual(2, 3);
  actual.row(0) << 1.0, 2.0, 3.0;
  actual.row(1) << 4.0, -5.0, -6.0;

  EXPECT_THAT(actual, EigenArrayEq<float>({{1.0, 2.0, 3.0},  //
                                           {4.0, -5.0, -6.0}}));
  EXPECT_THAT(actual, EigenArrayEq<float>({{1.0, 2.0, 3.0},  //
                                           {4.0, -5.0, -6.0 - 1.0e-6}}));
  EXPECT_THAT(actual, Not(EigenArrayEq<float>({{1.0, 2.0, 3.0},  //
                                               {4.0, -5.0, -8.0}})));
  // Wrong shape.
  EXPECT_THAT(actual, Not(EigenArrayEq<float>({{1.0, 2.0, 3.0}})));
}

}  // namespace
}  // namespace operations_research::pdlp
