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

#include "ortools/util/full_precision_inequalities.h"

#include <cmath>
#include <compare>
#include <limits>
#include <utility>
#include <vector>

#include "absl/types/span.h"
#include "gtest/gtest.h"

namespace operations_research {
namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();

std::vector<double> Negate(const std::vector<double>& v) {
  std::vector<double> negated;
  negated.reserve(v.size());
  for (const double x : v) {
    negated.push_back(-x);
  }
  return negated;
}

struct ComputeSumSignTestCase {
  std::vector<double> v;
  std::strong_ordering expected_sign;
};

using ComputeSumSignTest = testing::TestWithParam<ComputeSumSignTestCase>;

TEST_P(ComputeSumSignTest, Pos) {
  const auto& param = GetParam();
  EXPECT_EQ(ComputeSumSign(param.v), param.expected_sign);
}

TEST_P(ComputeSumSignTest, Neg) {
  const auto& param = GetParam();
  // See https://en.cppreference.com/cpp/utility/compare/strong_ordering for the
  // definition of <=>.
  const std::strong_ordering opposite_sign = 0 <=> param.expected_sign;
  EXPECT_EQ(ComputeSumSign(Negate(param.v)), opposite_sign);
}

INSTANTIATE_TEST_SUITE_P(
    Values, ComputeSumSignTest,
    testing::Values(
        ComputeSumSignTestCase{.v = {},
                               .expected_sign = std::strong_ordering::equal},
        ComputeSumSignTestCase{.v = {0.0},
                               .expected_sign = std::strong_ordering::equal},
        ComputeSumSignTestCase{.v = {-0.0},
                               .expected_sign = std::strong_ordering::equal},
        ComputeSumSignTestCase{.v = {1.0},
                               .expected_sign = std::strong_ordering::greater},
        ComputeSumSignTestCase{.v = {1e-308},
                               .expected_sign = std::strong_ordering::greater},
        ComputeSumSignTestCase{.v = {1.0, 2.0, -3.0},
                               .expected_sign = std::strong_ordering::equal},
        ComputeSumSignTestCase{.v = {1.0, 2.0, -2.9},
                               .expected_sign = std::strong_ordering::greater},
        ComputeSumSignTestCase{.v = {1.0, 2.0, -3.1},
                               .expected_sign = std::strong_ordering::less},
        ComputeSumSignTestCase{.v = {1e16, 1.0, -1e16},
                               .expected_sign = std::strong_ordering::greater},
        ComputeSumSignTestCase{.v = {1e16, -1.0, -1e16},
                               .expected_sign = std::strong_ordering::less},
        ComputeSumSignTestCase{.v = {1e16, 1e-16, -1e16},
                               .expected_sign = std::strong_ordering::greater},
        ComputeSumSignTestCase{.v = {1e16, -1e-16, -1e16},
                               .expected_sign = std::strong_ordering::less},
        ComputeSumSignTestCase{.v = {1e100, 1e-100, -1e100},
                               .expected_sign = std::strong_ordering::greater},
        ComputeSumSignTestCase{.v = {1e100, -1e-100, -1e100},
                               .expected_sign = std::strong_ordering::less},
        ComputeSumSignTestCase{.v = {1e100, 1.0, -1.0, -1e100},
                               .expected_sign = std::strong_ordering::equal},
        ComputeSumSignTestCase{.v = {std::numeric_limits<double>::max(),
                                     std::numeric_limits<double>::denorm_min(),
                                     -std::numeric_limits<double>::max()},
                               .expected_sign = std::strong_ordering::greater},
        ComputeSumSignTestCase{
            .v = {1e100, kInf, -1.0, kInf},
            .expected_sign = std::strong_ordering::greater}));

struct IsDotProductCmpOrEqualTestCase {
  std::vector<double> a;
  std::vector<double> b;
  double bound;
  bool expect_smaller_or_equal;
};

using IsDotProductCmpOrEqualTest =
    testing::TestWithParam<IsDotProductCmpOrEqualTestCase>;

TEST_P(IsDotProductCmpOrEqualTest, PosPos) {
  auto param = GetParam();
  EXPECT_EQ(DotProductIsSmallerOrEqual(param.a, param.b, param.bound),
            param.expect_smaller_or_equal);
}

TEST_P(IsDotProductCmpOrEqualTest, PosNeg) {
  auto param = GetParam();
  EXPECT_EQ(DotProductIsGreaterOrEqual(param.a, Negate(param.b), -param.bound),
            param.expect_smaller_or_equal);
}

TEST_P(IsDotProductCmpOrEqualTest, NegPos) {
  auto param = GetParam();
  EXPECT_EQ(DotProductIsGreaterOrEqual(Negate(param.a), param.b, -param.bound),
            param.expect_smaller_or_equal);
}

TEST_P(IsDotProductCmpOrEqualTest, NegNeg) {
  auto param = GetParam();
  EXPECT_EQ(
      DotProductIsSmallerOrEqual(Negate(param.a), Negate(param.b), param.bound),
      param.expect_smaller_or_equal);
}

INSTANTIATE_TEST_SUITE_P(
    Values, IsDotProductCmpOrEqualTest,
    testing::Values(
        IsDotProductCmpOrEqualTestCase{
            .a = {}, .b = {}, .bound = 0.0, .expect_smaller_or_equal = true},
        IsDotProductCmpOrEqualTestCase{
            .a = {}, .b = {}, .bound = -1.0, .expect_smaller_or_equal = false},
        IsDotProductCmpOrEqualTestCase{.a = {1.0, 2.0},
                                       .b = {3.0, 4.0},
                                       .bound = 11.0,
                                       .expect_smaller_or_equal = true},
        IsDotProductCmpOrEqualTestCase{.a = {1.0, 2.0},
                                       .b = {3.0, 4.0},
                                       .bound = 10.9,
                                       .expect_smaller_or_equal = false},
        IsDotProductCmpOrEqualTestCase{.a = {1e-308},
                                       .b = {1e-308},
                                       .bound = 0.0,
                                       .expect_smaller_or_equal = false},
        IsDotProductCmpOrEqualTestCase{.a = {1, 1e-308},
                                       .b = {1, 1e-308},
                                       .bound = 1.0,
                                       .expect_smaller_or_equal = false},
        IsDotProductCmpOrEqualTestCase{.a = {1e16, 1.0, -1e16},
                                       .b = {1.0, 1.0, 1.0},
                                       .bound = 1.5,
                                       .expect_smaller_or_equal = true},
        IsDotProductCmpOrEqualTestCase{.a = {1e16, 1.0, -1e16},
                                       .b = {1.0, 1.0, 1.0},
                                       .bound = 0.5,
                                       .expect_smaller_or_equal = false},
        IsDotProductCmpOrEqualTestCase{.a = {1e16, 1e-16, -1e16},
                                       .b = {1.0, 1.0, 1.0},
                                       .bound = 0.0,
                                       .expect_smaller_or_equal = false},
        IsDotProductCmpOrEqualTestCase{.a = {1e16, -1e-16, -1e16},
                                       .b = {1.0, 1.0, 1.0},
                                       .bound = 0.0,
                                       .expect_smaller_or_equal = true},
        IsDotProductCmpOrEqualTestCase{.a = {kInf, -1e-16, 1e16},
                                       .b = {1.0, 1.0, kInf},
                                       .bound = 0.0,
                                       .expect_smaller_or_equal = false},
        IsDotProductCmpOrEqualTestCase{.a = {-kInf, -1e-16, -1e16},
                                       .b = {1.0, 1.0, kInf},
                                       .bound = 0.0,
                                       .expect_smaller_or_equal = true}));

// Test that overflows are handled correctly. This test was failing because of
// overflowing int128.
TEST(IsDotProductCmpOrEqualTest, VeryLongSum) {
  // Compensated sum of (1 << 23) + (1 << 24) = 3 << 23 terms.
  // (1 << 23) * 2 - (1 << 24) * 1
  std::vector<double> v((3 << 23) - 1, 1.0);
  std::vector<double> w(1 << 23, 2.0);
  w.insert(w.end(), (1 << 24) - 1, -1.0);

  EXPECT_EQ(CmpDotProduct(v, w, 0.0), std::strong_ordering::greater);
  v.push_back(1.0);
  w.push_back(-1.0);
  EXPECT_EQ(CmpDotProduct(v, w, 0.0), std::strong_ordering::equal);
  v.push_back(1.0);
  w.push_back(-1.0);
  EXPECT_EQ(CmpDotProduct(v, w, 0.0), std::strong_ordering::less);
}

struct GetTightDotProductBoundsTestCase {
  std::vector<double> a;
  std::vector<double> b;
  std::pair<double, double> expected_bounds;
};

using GetTightDotProductBoundsTest =
    testing::TestWithParam<GetTightDotProductBoundsTestCase>;

TEST_P(GetTightDotProductBoundsTest, PosPos) {
  auto param = GetParam();
  EXPECT_EQ(GetTightDotProductBounds(param.a, param.b), param.expected_bounds);
}

TEST_P(GetTightDotProductBoundsTest, PosNeg) {
  auto param = GetParam();
  const auto neg_bounds = std::make_pair(-param.expected_bounds.second,
                                         -param.expected_bounds.first);
  EXPECT_EQ(GetTightDotProductBounds(param.a, Negate(param.b)), neg_bounds);
}

TEST_P(GetTightDotProductBoundsTest, NegPos) {
  auto param = GetParam();
  const auto neg_bounds = std::make_pair(-param.expected_bounds.second,
                                         -param.expected_bounds.first);
  EXPECT_EQ(GetTightDotProductBounds(Negate(param.a), param.b), neg_bounds);
}

TEST_P(GetTightDotProductBoundsTest, NegNeg) {
  auto param = GetParam();
  EXPECT_EQ(GetTightDotProductBounds(Negate(param.a), Negate(param.b)),
            param.expected_bounds);
}

INSTANTIATE_TEST_SUITE_P(
    Values, GetTightDotProductBoundsTest,
    testing::Values(
        GetTightDotProductBoundsTestCase{
            .a = {}, .b = {}, .expected_bounds = {0.0, 0.0}},
        GetTightDotProductBoundsTestCase{
            .a = {1.0, 2.0}, .b = {3.0, 4.0}, .expected_bounds = {11.0, 11.0}},
        GetTightDotProductBoundsTestCase{.a = {1e16, 1.0, -1e16},
                                         .b = {1.0, 1.0, 1.0},
                                         .expected_bounds = {1.0, 1.0}},
        GetTightDotProductBoundsTestCase{.a = {1e16, 1e-16, -1e16},
                                         .b = {1.0, 1.0, 1.0},
                                         .expected_bounds = {1e-16, 1e-16}},
        GetTightDotProductBoundsTestCase{.a = {1e16, -1e-16, -1e16},
                                         .b = {1.0, 1.0, 1.0},
                                         .expected_bounds = {-1e-16, -1e-16}},
        GetTightDotProductBoundsTestCase{
            .a = {3.0, std::nextafter(0.0, 1.0)},
            .b = {1.0, std::nextafter(0.0, 1.0)},
            .expected_bounds = {3.0, std::nextafter(3.0, 4.0)}},
        GetTightDotProductBoundsTestCase{
            .a = {3.0, std::nextafter(0.0, 1.0)},
            .b = {1.0, std::nextafter(0.0, -1.0)},
            .expected_bounds = {std::nextafter(3.0, 2.0), 3.0}}));

}  // namespace
}  // namespace operations_research
