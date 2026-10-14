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
#include <cstdint>
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
            .expected_bounds = {std::nextafter(3.0, 2.0), 3.0}},
        GetTightDotProductBoundsTestCase{.a = {+kInf, -1.0},
                                         .b = {1.0, -kInf},
                                         .expected_bounds = {+kInf, +kInf}}));

struct ProdBoundsIntIntTestCase {
  int64_t a;
  int64_t b;
  std::pair<double, double> expected_bounds;
};

using ProdBoundsIntIntTest = testing::TestWithParam<ProdBoundsIntIntTestCase>;

TEST_P(ProdBoundsIntIntTest, PosPos) {
  const auto param = GetParam();
  EXPECT_EQ(ProdBounds(param.a, param.b), param.expected_bounds);
  EXPECT_EQ(ProdBounds(param.b, param.a), param.expected_bounds);
}

TEST_P(ProdBoundsIntIntTest, PosNeg) {
  const auto param = GetParam();
  const auto neg_bounds = std::make_pair(-param.expected_bounds.second,
                                         -param.expected_bounds.first);
  EXPECT_EQ(ProdBounds(param.a, -param.b), neg_bounds);
  EXPECT_EQ(ProdBounds(-param.b, param.a), neg_bounds);
}

TEST_P(ProdBoundsIntIntTest, NegPos) {
  const auto param = GetParam();
  const auto neg_bounds = std::make_pair(-param.expected_bounds.second,
                                         -param.expected_bounds.first);
  EXPECT_EQ(ProdBounds(-param.a, param.b), neg_bounds);
  EXPECT_EQ(ProdBounds(param.b, -param.a), neg_bounds);
}

TEST_P(ProdBoundsIntIntTest, NegNeg) {
  const auto param = GetParam();
  EXPECT_EQ(ProdBounds(-param.a, -param.b), param.expected_bounds);
  EXPECT_EQ(ProdBounds(-param.b, -param.a), param.expected_bounds);
}

INSTANTIATE_TEST_SUITE_P(
    Values, ProdBoundsIntIntTest,
    testing::Values(
        ProdBoundsIntIntTestCase{.a = 0, .b = 0, .expected_bounds = {0.0, 0.0}},
        ProdBoundsIntIntTestCase{.a = 0, .b = 7, .expected_bounds = {0.0, 0.0}},
        ProdBoundsIntIntTestCase{.a = 1, .b = 1, .expected_bounds = {1.0, 1.0}},
        ProdBoundsIntIntTestCase{
            .a = 3, .b = 4, .expected_bounds = {12.0, 12.0}},
        ProdBoundsIntIntTestCase{
            .a = (int64_t{1} << 53) - 1,
            .b = 1,
            .expected_bounds = {static_cast<double>((int64_t{1} << 53) - 1),
                                static_cast<double>((int64_t{1} << 53) - 1)}},
        ProdBoundsIntIntTestCase{
            .a = (int64_t{1} << 53) + 1,
            .b = 1,
            .expected_bounds = {std::ldexp(1.0, 53),
                                std::nextafter(std::ldexp(1.0, 53), kInf)}},
        ProdBoundsIntIntTestCase{
            .a = int64_t{1} << 30,
            .b = int64_t{1} << 30,
            .expected_bounds = {std::ldexp(1.0, 60), std::ldexp(1.0, 60)}},
        ProdBoundsIntIntTestCase{
            .a = (int64_t{1} << 30) + 1,
            .b = int64_t{1} << 30,
            .expected_bounds = {std::ldexp(1.0, 60) + std::ldexp(1.0, 30),
                                std::ldexp(1.0, 60) + std::ldexp(1.0, 30)}},
        ProdBoundsIntIntTestCase{
            .a = (int64_t{1} << 53) + 1,
            .b = int64_t{1} << 7,
            .expected_bounds = {std::ldexp(1.0, 60),
                                std::nextafter(std::ldexp(1.0, 60), kInf)}},
        ProdBoundsIntIntTestCase{
            .a = (int64_t{1} << 50) + 1,
            .b = (int64_t{1} << 50) + 1,
            .expected_bounds = {std::ldexp(1.0, 100) + std::ldexp(1.0, 51),
                                std::nextafter(std::ldexp(1.0, 100) +
                                                   std::ldexp(1.0, 51),
                                               kInf)}},
        ProdBoundsIntIntTestCase{
            .a = std::numeric_limits<int64_t>::max(),
            .b = 1,
            .expected_bounds = {std::nextafter(std::ldexp(1.0, 63), 0.0),
                                std::ldexp(1.0, 63)}},
        ProdBoundsIntIntTestCase{
            .a = std::numeric_limits<int64_t>::max(),
            .b = std::numeric_limits<int64_t>::max(),
            .expected_bounds = {std::nextafter(std::ldexp(1.0, 126), 0.0),
                                std::ldexp(1.0, 126)}}));

struct ProdBoundsIntDoubleTestCase {
  int64_t a;
  double b;
  std::pair<double, double> expected_bounds;
};

using ProdBoundsIntDoubleTest =
    testing::TestWithParam<ProdBoundsIntDoubleTestCase>;

TEST_P(ProdBoundsIntDoubleTest, PosPos) {
  const auto param = GetParam();
  EXPECT_EQ(ProdBounds(param.a, param.b), param.expected_bounds);
  EXPECT_EQ(ProdBounds(param.b, param.a), param.expected_bounds);
}

TEST_P(ProdBoundsIntDoubleTest, PosNeg) {
  const auto param = GetParam();
  const auto neg_bounds = std::make_pair(-param.expected_bounds.second,
                                         -param.expected_bounds.first);
  EXPECT_EQ(ProdBounds(param.a, -param.b), neg_bounds);
  EXPECT_EQ(ProdBounds(-param.b, param.a), neg_bounds);
}

TEST_P(ProdBoundsIntDoubleTest, NegPos) {
  const auto param = GetParam();
  const auto neg_bounds = std::make_pair(-param.expected_bounds.second,
                                         -param.expected_bounds.first);
  EXPECT_EQ(ProdBounds(-param.a, param.b), neg_bounds);
  EXPECT_EQ(ProdBounds(param.b, -param.a), neg_bounds);
}

TEST_P(ProdBoundsIntDoubleTest, NegNeg) {
  const auto param = GetParam();
  EXPECT_EQ(ProdBounds(-param.a, -param.b), param.expected_bounds);
  EXPECT_EQ(ProdBounds(-param.b, -param.a), param.expected_bounds);
}

INSTANTIATE_TEST_SUITE_P(
    Values, ProdBoundsIntDoubleTest,
    testing::Values(
        ProdBoundsIntDoubleTestCase{
            .a = 0, .b = 0.0, .expected_bounds = {0.0, 0.0}},
        ProdBoundsIntDoubleTestCase{
            .a = 0, .b = 3.5, .expected_bounds = {0.0, 0.0}},
        ProdBoundsIntDoubleTestCase{
            .a = 5, .b = 0.0, .expected_bounds = {0.0, 0.0}},
        ProdBoundsIntDoubleTestCase{
            .a = 3, .b = 1.5, .expected_bounds = {4.5, 4.5}},
        ProdBoundsIntDoubleTestCase{
            .a = (int64_t{1} << 53) + 1,
            .b = 0.5,
            .expected_bounds = {std::ldexp(1.0, 52),
                                std::nextafter(std::ldexp(1.0, 52), kInf)}},
        ProdBoundsIntDoubleTestCase{
            .a = 3,
            .b = std::nextafter(1.0, 2.0),
            .expected_bounds = {std::nextafter(3.0, 4.0),
                                std::nextafter(std::nextafter(3.0, 4.0),
                                               kInf)}},
        ProdBoundsIntDoubleTestCase{
            .a = (int64_t{1} << 50) + 1,
            .b = 1.5,
            .expected_bounds = {std::ldexp(3.0, 49) + 1.5,
                                std::ldexp(3.0, 49) + 1.5}},
        ProdBoundsIntDoubleTestCase{
            .a = (int64_t{1} << 52) + 1,
            .b = 1.5,
            .expected_bounds = {std::ldexp(3.0, 51) + 1.0,
                                std::ldexp(3.0, 51) + 2.0}}));

struct ProdBoundsDoubleDoubleTestCase {
  double a;
  double b;
  std::pair<double, double> expected_bounds;
};

using ProdBoundsDoubleDoubleTest =
    testing::TestWithParam<ProdBoundsDoubleDoubleTestCase>;

TEST_P(ProdBoundsDoubleDoubleTest, PosPos) {
  const auto param = GetParam();
  EXPECT_EQ(ProdBounds(param.a, param.b), param.expected_bounds);
  EXPECT_EQ(ProdBounds(param.b, param.a), param.expected_bounds);
}

TEST_P(ProdBoundsDoubleDoubleTest, PosNeg) {
  const auto param = GetParam();
  const auto neg_bounds = std::make_pair(-param.expected_bounds.second,
                                         -param.expected_bounds.first);
  EXPECT_EQ(ProdBounds(param.a, -param.b), neg_bounds);
  EXPECT_EQ(ProdBounds(-param.b, param.a), neg_bounds);
}

TEST_P(ProdBoundsDoubleDoubleTest, NegPos) {
  const auto param = GetParam();
  const auto neg_bounds = std::make_pair(-param.expected_bounds.second,
                                         -param.expected_bounds.first);
  EXPECT_EQ(ProdBounds(-param.a, param.b), neg_bounds);
  EXPECT_EQ(ProdBounds(param.b, -param.a), neg_bounds);
}

TEST_P(ProdBoundsDoubleDoubleTest, NegNeg) {
  const auto param = GetParam();
  EXPECT_EQ(ProdBounds(-param.a, -param.b), param.expected_bounds);
  EXPECT_EQ(ProdBounds(-param.b, -param.a), param.expected_bounds);
}

INSTANTIATE_TEST_SUITE_P(
    Values, ProdBoundsDoubleDoubleTest,
    testing::Values(
        ProdBoundsDoubleDoubleTestCase{
            .a = 0.0, .b = 0.0, .expected_bounds = {0.0, 0.0}},
        ProdBoundsDoubleDoubleTestCase{
            .a = 0.0, .b = 3.5, .expected_bounds = {0.0, 0.0}},
        ProdBoundsDoubleDoubleTestCase{
            .a = 1.5, .b = 2.5, .expected_bounds = {3.75, 3.75}},
        ProdBoundsDoubleDoubleTestCase{
            .a = 1.0 + std::ldexp(1.0, -25),
            .b = 1.0 + std::ldexp(1.0, -25),
            .expected_bounds =
                {1.0 + std::ldexp(1.0, -24) + std::ldexp(1.0, -50),
                 1.0 + std::ldexp(1.0, -24) + std::ldexp(1.0, -50)}},
        ProdBoundsDoubleDoubleTestCase{
            .a = 1.0 + std::ldexp(1.0, -30),
            .b = 1.0 + std::ldexp(1.0, -30),
            .expected_bounds = {1.0 + std::ldexp(1.0, -29),
                                std::nextafter(1.0 + std::ldexp(1.0, -29),
                                               kInf)}},
        ProdBoundsDoubleDoubleTestCase{
            .a = std::nextafter(1.0, 2.0),
            .b = std::nextafter(1.0, 2.0),
            .expected_bounds = {1.0 + std::ldexp(1.0, -51),
                                std::nextafter(1.0 + std::ldexp(1.0, -51),
                                               kInf)}},
        ProdBoundsDoubleDoubleTestCase{
            .a = std::nextafter(1.0, 2.0),
            .b = std::nextafter(1.0, 0.0),
            .expected_bounds = {1.0, std::nextafter(1.0, 2.0)}},
        ProdBoundsDoubleDoubleTestCase{
            .a = std::ldexp(1.5, 500),
            .b = std::ldexp(2.5, -400),
            .expected_bounds = {std::ldexp(3.75, 100), std::ldexp(3.75, 100)}},
        ProdBoundsDoubleDoubleTestCase{
            .a = std::numeric_limits<double>::max(),
            .b = 2,
            .expected_bounds =
                {
                    std::numeric_limits<double>::max(),
                    std::numeric_limits<double>::infinity(),
                }},
        ProdBoundsDoubleDoubleTestCase{.a = 0.232366,
                                       .b = 4.94066e-324,
                                       .expected_bounds = {
                                           0.0,
                                           std::nextafter(0.0, 1.0),
                                       }}));

struct Int64DoubleBoundsTestCase {
  int64_t n;
  std::pair<double, double> expected_bounds;
};

using Int64DoubleBoundsTest = testing::TestWithParam<Int64DoubleBoundsTestCase>;

TEST_P(Int64DoubleBoundsTest, Pos) {
  const auto param = GetParam();
  EXPECT_EQ(Int64DoubleBounds(param.n), param.expected_bounds);
}

TEST_P(Int64DoubleBoundsTest, Neg) {
  const auto param = GetParam();
  const auto neg_bounds = std::make_pair(-param.expected_bounds.second,
                                         -param.expected_bounds.first);
  EXPECT_EQ(Int64DoubleBounds(-param.n), neg_bounds);
}

INSTANTIATE_TEST_SUITE_P(
    Values, Int64DoubleBoundsTest,
    testing::Values(
        Int64DoubleBoundsTestCase{.n = 0, .expected_bounds = {0.0, 0.0}},
        Int64DoubleBoundsTestCase{.n = 1, .expected_bounds = {1.0, 1.0}},
        Int64DoubleBoundsTestCase{.n = 42, .expected_bounds = {42.0, 42.0}},
        Int64DoubleBoundsTestCase{
            .n = (int64_t{1} << 53) - 1,
            .expected_bounds = {static_cast<double>((int64_t{1} << 53) - 1),
                                static_cast<double>((int64_t{1} << 53) - 1)}},
        Int64DoubleBoundsTestCase{
            .n = int64_t{1} << 53,
            .expected_bounds = {std::ldexp(1.0, 53), std::ldexp(1.0, 53)}},
        Int64DoubleBoundsTestCase{
            .n = (int64_t{1} << 53) + 1,
            .expected_bounds = {std::ldexp(1.0, 53),
                                std::nextafter(std::ldexp(1.0, 53), kInf)}},
        Int64DoubleBoundsTestCase{
            .n = int64_t{1} << 60,
            .expected_bounds = {std::ldexp(1.0, 60), std::ldexp(1.0, 60)}},
        Int64DoubleBoundsTestCase{
            .n = (int64_t{1} << 60) + (int64_t{1} << 30),
            .expected_bounds = {std::ldexp(1.0, 60) + std::ldexp(1.0, 30),
                                std::ldexp(1.0, 60) + std::ldexp(1.0, 30)}},
        Int64DoubleBoundsTestCase{
            .n = (int64_t{1} << 60) + 1,
            .expected_bounds = {std::ldexp(1.0, 60),
                                std::nextafter(std::ldexp(1.0, 60), kInf)}},
        Int64DoubleBoundsTestCase{
            .n = std::numeric_limits<int64_t>::max(),
            .expected_bounds = {std::nextafter(std::ldexp(1.0, 63), 0.0),
                                std::ldexp(1.0, 63)}}));

struct FuseAddMultiplyAddTestCase {
  double scale;
  double shift;
  double pre_shift;
  int64_t n;
  std::pair<double, double> expected_bounds;
};

using FuseAddMultiplyAddTest =
    testing::TestWithParam<FuseAddMultiplyAddTestCase>;

TEST_P(FuseAddMultiplyAddTest, PosScalePosN) {
  const auto param = GetParam();
  EXPECT_EQ(
      FuseAddMultiplyAdd(param.scale, param.shift, param.pre_shift, param.n),
      param.expected_bounds);
}

TEST_P(FuseAddMultiplyAddTest, NegScalePosN) {
  const auto param = GetParam();
  const auto neg_bounds = std::make_pair(-param.expected_bounds.second,
                                         -param.expected_bounds.first);
  EXPECT_EQ(
      FuseAddMultiplyAdd(-param.scale, -param.shift, param.pre_shift, param.n),
      neg_bounds);
}

TEST_P(FuseAddMultiplyAddTest, PosScaleNegN) {
  const auto param = GetParam();
  if (param.n == std::numeric_limits<int64_t>::min()) return;
  const auto neg_bounds = std::make_pair(-param.expected_bounds.second,
                                         -param.expected_bounds.first);
  EXPECT_EQ(
      FuseAddMultiplyAdd(param.scale, -param.shift, -param.pre_shift, -param.n),
      neg_bounds);
}

TEST_P(FuseAddMultiplyAddTest, NegScaleNegN) {
  const auto param = GetParam();
  if (param.n == std::numeric_limits<int64_t>::min()) return;
  EXPECT_EQ(
      FuseAddMultiplyAdd(-param.scale, param.shift, -param.pre_shift, -param.n),
      param.expected_bounds);
}

INSTANTIATE_TEST_SUITE_P(
    Values, FuseAddMultiplyAddTest,
    testing::Values(
        FuseAddMultiplyAddTestCase{.scale = 0.0,
                                   .shift = 0.0,
                                   .pre_shift = 0.0,
                                   .n = 0,
                                   .expected_bounds = {0.0, 0.0}},
        FuseAddMultiplyAddTestCase{.scale = 0.0,
                                   .shift = 2.5,
                                   .pre_shift = 0.5,
                                   .n = 10,
                                   .expected_bounds = {2.5, 2.5}},
        FuseAddMultiplyAddTestCase{.scale = 1.0,
                                   .shift = 0.0,
                                   .pre_shift = 0.0,
                                   .n = 5,
                                   .expected_bounds = {5.0, 5.0}},
        FuseAddMultiplyAddTestCase{.scale = 2.0,
                                   .shift = 1.0,
                                   .pre_shift = 0.5,
                                   .n = 3,
                                   .expected_bounds = {8.0, 8.0}},
        FuseAddMultiplyAddTestCase{.scale = 0.5,
                                   .shift = -1.25,
                                   .pre_shift = -0.5,
                                   .n = 7,
                                   .expected_bounds = {2.0, 2.0}},
        FuseAddMultiplyAddTestCase{
            .scale = 1.0,
            .shift = 0.0,
            .pre_shift = 0.0,
            .n = (int64_t{1} << 53) + 1,
            .expected_bounds = {std::ldexp(1.0, 53),
                                std::nextafter(std::ldexp(1.0, 53), kInf)}},
        FuseAddMultiplyAddTestCase{
            .scale = 1.0,
            .shift = 0.0,
            .pre_shift = -1.0,
            .n = (int64_t{1} << 53) + 1,
            .expected_bounds = {std::ldexp(1.0, 53), std::ldexp(1.0, 53)}},
        FuseAddMultiplyAddTestCase{.scale = 1.0,
                                   .shift = -std::ldexp(1.0, 60),
                                   .pre_shift = 0.5,
                                   .n = (int64_t{1} << 60) + 1,
                                   .expected_bounds = {1.5, 1.5}},
        FuseAddMultiplyAddTestCase{
            .scale = std::numeric_limits<double>::denorm_min(),
            .shift = 1.0,
            .pre_shift = 0.5,
            .n = 0,
            .expected_bounds = {1.0, std::nextafter(1.0, kInf)}},
        FuseAddMultiplyAddTestCase{
            .scale = std::numeric_limits<double>::denorm_min(),
            .shift = 1.0,
            .pre_shift = 0.5,
            .n = -1,
            .expected_bounds = {std::nextafter(1.0, 0.0), 1.0}},
        FuseAddMultiplyAddTestCase{
            .scale = 1.0,
            .shift = 0.0,
            .pre_shift = 0.0,
            .n = std::numeric_limits<int64_t>::min(),
            .expected_bounds = {-std::ldexp(1.0, 63), -std::ldexp(1.0, 63)}},
        FuseAddMultiplyAddTestCase{.scale = 1.0,
                                   .shift = std::ldexp(1.0, 63),
                                   .pre_shift = 0.5,
                                   .n = std::numeric_limits<int64_t>::min(),
                                   .expected_bounds = {0.5, 0.5}},
        FuseAddMultiplyAddTestCase{
            .scale = 1.0,
            .shift = 0.0,
            .pre_shift = 0.0,
            .n = std::numeric_limits<int64_t>::max(),
            .expected_bounds = {std::nextafter(std::ldexp(1.0, 63), 0.0),
                                std::ldexp(1.0, 63)}},
        FuseAddMultiplyAddTestCase{.scale = 1.0,
                                   .shift = -std::ldexp(1.0, 63),
                                   .pre_shift = 0.5,
                                   .n = std::numeric_limits<int64_t>::max(),
                                   .expected_bounds = {-0.5, -0.5}},
        FuseAddMultiplyAddTestCase{
            .scale = std::numeric_limits<double>::max(),
            .shift = 0.0,
            .pre_shift = 0.5,
            .n = 1,
            .expected_bounds = {std::numeric_limits<double>::max(), kInf}}));

TEST(SumTest, DefaultConstructedIsZero) {
  Sum sum;

  EXPECT_EQ(sum.LooseBounds(), std::make_pair(0.0, 0.0));
  EXPECT_EQ(sum.LooseLb(), 0.0);
  EXPECT_EQ(sum.LooseUb(), 0.0);
  EXPECT_EQ(sum.TightBounds(), std::make_pair(0.0, 0.0));
  EXPECT_EQ(sum.TightLb(), 0.0);
  EXPECT_EQ(sum.TightUb(), 0.0);
  EXPECT_EQ(sum <=> 0.0, std::strong_ordering::equal);
  EXPECT_EQ(0.0 <=> sum, std::strong_ordering::equal);
  EXPECT_EQ(sum <=> -0.0, std::strong_ordering::equal);
  EXPECT_EQ(sum <=> 1.0, std::strong_ordering::less);
  EXPECT_EQ(1.0 <=> sum, std::strong_ordering::greater);
  EXPECT_EQ(sum <=> -1.0, std::strong_ordering::greater);
  EXPECT_EQ(-1.0 <=> sum, std::strong_ordering::less);
  EXPECT_EQ(sum <=> int64_t{0}, std::strong_ordering::equal);
  EXPECT_EQ(int64_t{0} <=> sum, std::strong_ordering::equal);
  EXPECT_EQ(sum <=> int64_t{1}, std::strong_ordering::less);
  EXPECT_EQ(int64_t{1} <=> sum, std::strong_ordering::greater);
  EXPECT_EQ(sum <=> int64_t{-1}, std::strong_ordering::greater);
  EXPECT_EQ(int64_t{-1} <=> sum, std::strong_ordering::less);
}

TEST(SumTest, AddProdSingleArgumentTypesAndChaining) {
  EXPECT_EQ(Sum().Add(3.25).TightBounds(), std::make_pair(3.25, 3.25));
  EXPECT_EQ(Sum().Add(int64_t{-42}).TightBounds(),
            std::make_pair(-42.0, -42.0));
  EXPECT_EQ(Sum().Add(PowerOfTwo(0)).TightBounds(), std::make_pair(1.0, 1.0));
  EXPECT_EQ(Sum().Add(PowerOfTwo(30)).TightBounds(),
            std::make_pair(std::ldexp(1.0, 30), std::ldexp(1.0, 30)));
  EXPECT_EQ(Sum().Add(PowerOfTwo(-30)).TightBounds(),
            std::make_pair(std::ldexp(1.0, -30), std::ldexp(1.0, -30)));

  Sum chained;
  chained.Add(1.5).Add(int64_t{-4}).Add(PowerOfTwo(3));
  EXPECT_EQ(chained.TightBounds(), std::make_pair(5.5, 5.5));
}

TEST(SumTest, AddProdTwoArgumentCombinations) {
  EXPECT_EQ(Sum().AddProd(1.5, -2.5).TightBounds(),
            std::make_pair(-3.75, -3.75));

  Sum mixed_int_double;
  mixed_int_double.AddProd(0.25, int64_t{12}).AddProd(int64_t{-2}, 1.5);
  EXPECT_EQ(mixed_int_double.TightBounds(), std::make_pair(0.0, 0.0));

  Sum mixed_pow2;
  mixed_pow2.AddProd(3.0, PowerOfTwo(-2))
      .AddProd(int64_t{5}, PowerOfTwo(-2))
      .AddProd(PowerOfTwo(3), PowerOfTwo(-2));
  EXPECT_EQ(mixed_pow2.TightBounds(), std::make_pair(4.0, 4.0));

  // (2^50 + 1)^2 = 2^100 + 2^51 + 1, which requires >53 bits before
  // cancellation.
  const int64_t a = (int64_t{1} << 50) + 1;
  Sum exact_int_prod;
  exact_int_prod.Add(-std::ldexp(1.0, 100))
      .AddProd(a, a)
      .Add(-std::ldexp(1.0, 51));
  EXPECT_LE(exact_int_prod.LooseLb(), 1.0);
  EXPECT_GE(exact_int_prod.LooseUb(), 1.0);
  EXPECT_EQ(exact_int_prod.TightBounds(), std::make_pair(1.0, 1.0));
}

TEST(SumTest, AddProdThreeOrMoreArgumentsWithoutOverflow) {
  Sum sum;
  // 1.5 * (-3) * 16 = -72; 2.0 * 3.0 * 2 * 8 = 96; -72 + 96 = 24.
  sum.AddProd(1.5, int64_t{-3}, PowerOfTwo(4))
      .AddProd(2.0, 3.0, int64_t{2}, PowerOfTwo(3));

  EXPECT_EQ(sum.TightBounds(), std::make_pair(24.0, 24.0));
}

TEST(SumTest, AddProdThreeOrMoreArgumentsWithMantissaOverflow) {
  // (2^62 + 1)^3 = 2^186 + 3 * 2^124 + 3 * 2^62 + 1.
  // The product mantissa has 187 bits (> 128), exercising multi-limb
  // multiplication in MultiplyLastTerms.
  const int64_t a = (int64_t{1} << 62) + 1;

  Sum pos_cube;
  pos_cube.AddProd(a, a, a)
      .Add(-std::ldexp(1.0, 186))
      .AddProd(int64_t{-3}, std::ldexp(1.0, 124))
      .AddProd(int64_t{-3}, std::ldexp(1.0, 62));
  EXPECT_EQ(pos_cube.TightBounds(), std::make_pair(1.0, 1.0));

  Sum neg_cube;
  neg_cube.AddProd(a, -a, a)
      .Add(std::ldexp(1.0, 186))
      .AddProd(int64_t{3}, std::ldexp(1.0, 124))
      .AddProd(int64_t{3}, std::ldexp(1.0, 62));
  EXPECT_EQ(neg_cube.TightBounds(), std::make_pair(-1.0, -1.0));

  // (2^62 + 1)^5 = 2^310 + 5 * 2^248 + 10 * 2^186 + 10 * 2^124 + 5 * 2^62 + 1.
  // The product mantissa has 311 bits (> 256), exercising >= 3 limbs.
  Sum fifth_power;
  fifth_power.AddProd(a, a, a, a, a)
      .Add(-std::ldexp(1.0, 310))
      .AddProd(int64_t{-5}, std::ldexp(1.0, 248))
      .AddProd(int64_t{-10}, std::ldexp(1.0, 186))
      .AddProd(int64_t{-10}, std::ldexp(1.0, 124))
      .AddProd(int64_t{-5}, std::ldexp(1.0, 62));
  EXPECT_EQ(fifth_power.TightBounds(), std::make_pair(1.0, 1.0));
}

TEST(SumTest, AddProdTenArgumentsWithDelayedMantissaOverflow) {
  // First 5 terms:
  //   (2^7 - 1)(2^7 + 1)(2^14 + 1)(2^28 + 1)(2^56 + 1) = 2^112 - 1,
  // whose 112-bit mantissa fits in 128 bits (handled by the fast path in
  // MultiplyLastTerm).
  // Remaining 5 terms:
  //   (2^31 - 1)(2^31 + 1)(2^62 + 1) * (2^62 - 1)(2^62 + 1) = (2^124 - 1)^2.
  // Whole product:
  //   (2^112 - 1)(2^124 - 1)^2
  //     = 2^360 - 2^248 - 2^237 + 2^125 + 2^112 - 1,
  // whose mantissa has 360 bits (> 300), exercising the transition from the
  // fast loop to multi-limb multiplication.
  Sum sum;
  sum.AddProd((int64_t{1} << 7) - 1, (int64_t{1} << 7) + 1,
              (int64_t{1} << 14) + 1, (int64_t{1} << 28) + 1,
              (int64_t{1} << 56) + 1, (int64_t{1} << 31) - 1,
              (int64_t{1} << 31) + 1, (int64_t{1} << 62) + 1,
              (int64_t{1} << 62) - 1, (int64_t{1} << 62) + 1)
      .Add(-std::ldexp(1.0, 360))
      .Add(std::ldexp(1.0, 248))
      .Add(std::ldexp(1.0, 237))
      .Add(-std::ldexp(1.0, 125))
      .Add(-std::ldexp(1.0, 112));
  EXPECT_EQ(sum.TightBounds(), std::make_pair(-1.0, -1.0));
}

struct SumComparisonTestCase {
  std::vector<double> terms;
  double rhs;
  std::strong_ordering expected_ordering;
};

using SumComparisonTest = testing::TestWithParam<SumComparisonTestCase>;

TEST_P(SumComparisonTest, LValueAndRValueComparisons) {
  const auto& param = GetParam();
  Sum sum;
  for (const double x : param.terms) {
    sum.Add(x);
  }
  const std::strong_ordering reversed_ordering = 0 <=> param.expected_ordering;

  // L-value comparisons on both sides.
  EXPECT_EQ(sum <=> param.rhs, param.expected_ordering);
  EXPECT_EQ(param.rhs <=> sum, reversed_ordering);

  // R-value comparisons on both sides.
  EXPECT_EQ(SumAll<double>(param.terms) <=> param.rhs, param.expected_ordering);
  EXPECT_EQ(param.rhs <=> SumAll<double>(param.terms), reversed_ordering);

  // Synthesized relational operators.
  EXPECT_EQ(sum < param.rhs,
            param.expected_ordering == std::strong_ordering::less);
  EXPECT_EQ(sum <= param.rhs,
            param.expected_ordering != std::strong_ordering::greater);
  EXPECT_EQ(sum > param.rhs,
            param.expected_ordering == std::strong_ordering::greater);
  EXPECT_EQ(sum >= param.rhs,
            param.expected_ordering != std::strong_ordering::less);
  EXPECT_EQ(param.rhs < sum, reversed_ordering == std::strong_ordering::less);
  EXPECT_EQ(param.rhs <= sum,
            reversed_ordering != std::strong_ordering::greater);
  EXPECT_EQ(param.rhs > sum,
            reversed_ordering == std::strong_ordering::greater);
  EXPECT_EQ(param.rhs >= sum, reversed_ordering != std::strong_ordering::less);
}

TEST_P(SumComparisonTest, NegatedComparisons) {
  const auto& param = GetParam();
  const std::vector<double> negated_terms = Negate(param.terms);
  const std::strong_ordering negated_ordering = 0 <=> param.expected_ordering;

  EXPECT_EQ(SumAll<double>(negated_terms) <=> -param.rhs, negated_ordering);
  EXPECT_EQ(-param.rhs <=> SumAll<double>(negated_terms),
            param.expected_ordering);
}

INSTANTIATE_TEST_SUITE_P(
    Values, SumComparisonTest,
    testing::Values(
        SumComparisonTestCase{.terms = {},
                              .rhs = 0.0,
                              .expected_ordering = std::strong_ordering::equal},
        SumComparisonTestCase{.terms = {},
                              .rhs = 1.0,
                              .expected_ordering = std::strong_ordering::less},
        SumComparisonTestCase{.terms = {1.0, 2.0},
                              .rhs = 3.0,
                              .expected_ordering = std::strong_ordering::equal},
        SumComparisonTestCase{
            .terms = {1.0, 2.0},
            .rhs = std::nextafter(3.0, 0.0),
            .expected_ordering = std::strong_ordering::greater},
        SumComparisonTestCase{.terms = {1.0, 2.0},
                              .rhs = std::nextafter(3.0, kInf),
                              .expected_ordering = std::strong_ordering::less},
        // Inexact sum strictly between 1.0 and nextafter(1.0, kInf).
        SumComparisonTestCase{
            .terms = {1.0, std::numeric_limits<double>::denorm_min()},
            .rhs = 1.0,
            .expected_ordering = std::strong_ordering::greater},
        SumComparisonTestCase{
            .terms = {1.0, std::numeric_limits<double>::denorm_min()},
            .rhs = std::nextafter(1.0, kInf),
            .expected_ordering = std::strong_ordering::less},
        // Massive cancellation.
        SumComparisonTestCase{
            .terms = {1e100, 1e-100, -1e100},
            .rhs = 0.0,
            .expected_ordering = std::strong_ordering::greater},
        SumComparisonTestCase{.terms = {1e100, 1e-100, -1e100},
                              .rhs = 1e-100,
                              .expected_ordering = std::strong_ordering::equal},
        SumComparisonTestCase{
            .terms = {1e100, 1e-100, -1e100},
            .rhs = std::nextafter(1e-100, 0.0),
            .expected_ordering = std::strong_ordering::greater},
        SumComparisonTestCase{.terms = {1e100, 1e-100, -1e100},
                              .rhs = std::nextafter(1e-100, kInf),
                              .expected_ordering = std::strong_ordering::less},
        SumComparisonTestCase{.terms = {1e100, 1.0, -1.0, -1e100},
                              .rhs = 0.0,
                              .expected_ordering = std::strong_ordering::equal},
        // Finite sum exceeding double::max().
        SumComparisonTestCase{
            .terms = {std::numeric_limits<double>::max(), 1.0},
            .rhs = std::numeric_limits<double>::max(),
            .expected_ordering = std::strong_ordering::greater},
        SumComparisonTestCase{
            .terms = {std::numeric_limits<double>::max(), 1.0},
            .rhs = kInf,
            .expected_ordering = std::strong_ordering::less},
        // Infinite sum.
        SumComparisonTestCase{.terms = {1e100, kInf, -1e100},
                              .rhs = kInf,
                              .expected_ordering = std::strong_ordering::equal},
        SumComparisonTestCase{
            .terms = {1e100, kInf, -1e100},
            .rhs = std::numeric_limits<double>::max(),
            .expected_ordering = std::strong_ordering::greater}));

struct SumInt64ComparisonTestCase {
  std::vector<double> terms;
  int64_t rhs;
  std::strong_ordering expected_ordering;
};

using SumInt64ComparisonTest =
    testing::TestWithParam<SumInt64ComparisonTestCase>;

TEST_P(SumInt64ComparisonTest, LValueAndRValueComparisons) {
  const auto& param = GetParam();
  Sum sum;
  for (const double x : param.terms) {
    sum.Add(x);
  }
  const std::strong_ordering reversed_ordering = 0 <=> param.expected_ordering;

  // L-value comparisons on both sides.
  EXPECT_EQ(sum <=> param.rhs, param.expected_ordering);
  EXPECT_EQ(param.rhs <=> sum, reversed_ordering);

  // R-value comparisons on both sides.
  EXPECT_EQ(SumAll<double>(param.terms) <=> param.rhs, param.expected_ordering);
  EXPECT_EQ(param.rhs <=> SumAll<double>(param.terms), reversed_ordering);

  // Synthesized relational operators.
  EXPECT_EQ(sum < param.rhs,
            param.expected_ordering == std::strong_ordering::less);
  EXPECT_EQ(sum <= param.rhs,
            param.expected_ordering != std::strong_ordering::greater);
  EXPECT_EQ(sum > param.rhs,
            param.expected_ordering == std::strong_ordering::greater);
  EXPECT_EQ(sum >= param.rhs,
            param.expected_ordering != std::strong_ordering::less);
  EXPECT_EQ(param.rhs < sum, reversed_ordering == std::strong_ordering::less);
  EXPECT_EQ(param.rhs <= sum,
            reversed_ordering != std::strong_ordering::greater);
  EXPECT_EQ(param.rhs > sum,
            reversed_ordering == std::strong_ordering::greater);
  EXPECT_EQ(param.rhs >= sum, reversed_ordering != std::strong_ordering::less);

  // Subsequent queries on the same Sum must remain consistent after CompareTo
  // updates cached bounds.
  const auto [loose_lb, loose_ub] = sum.LooseBounds();
  const auto [tight_lb, tight_ub] = sum.TightBounds();
  EXPECT_LE(loose_lb, tight_lb);
  EXPECT_GE(loose_ub, tight_ub);
  if (param.expected_ordering == std::strong_ordering::equal) {
    EXPECT_EQ(std::make_pair(tight_lb, tight_ub), Int64DoubleBounds(param.rhs));
    if (param.rhs > std::numeric_limits<int64_t>::min()) {
      EXPECT_EQ(sum <=> param.rhs - 1, std::strong_ordering::greater);
    }
    if (param.rhs < std::numeric_limits<int64_t>::max()) {
      EXPECT_EQ(sum <=> param.rhs + 1, std::strong_ordering::less);
    }
  }
}

TEST_P(SumInt64ComparisonTest, NegatedComparisons) {
  const auto& param = GetParam();
  if (param.rhs == std::numeric_limits<int64_t>::min()) return;
  const std::vector<double> negated_terms = Negate(param.terms);
  const std::strong_ordering negated_ordering = 0 <=> param.expected_ordering;

  EXPECT_EQ(SumAll<double>(negated_terms) <=> -param.rhs, negated_ordering);
  EXPECT_EQ(-param.rhs <=> SumAll<double>(negated_terms),
            param.expected_ordering);
}

INSTANTIATE_TEST_SUITE_P(
    Values, SumInt64ComparisonTest,
    testing::Values(
        SumInt64ComparisonTestCase{
            .terms = {},
            .rhs = 0,
            .expected_ordering = std::strong_ordering::equal},
        SumInt64ComparisonTestCase{
            .terms = {},
            .rhs = 1,
            .expected_ordering = std::strong_ordering::less},
        SumInt64ComparisonTestCase{
            .terms = {1.0, 2.0},
            .rhs = 3,
            .expected_ordering = std::strong_ordering::equal},
        SumInt64ComparisonTestCase{
            .terms = {1.0, 2.0},
            .rhs = 2,
            .expected_ordering = std::strong_ordering::greater},
        SumInt64ComparisonTestCase{
            .terms = {1.0, 2.0},
            .rhs = 4,
            .expected_ordering = std::strong_ordering::less},
        // Non-integer and subnormal offsets around an integer.
        SumInt64ComparisonTestCase{
            .terms = {3.0, 0.25},
            .rhs = 3,
            .expected_ordering = std::strong_ordering::greater},
        SumInt64ComparisonTestCase{
            .terms = {3.0, 0.25},
            .rhs = 4,
            .expected_ordering = std::strong_ordering::less},
        SumInt64ComparisonTestCase{
            .terms = {1.0, std::numeric_limits<double>::denorm_min()},
            .rhs = 1,
            .expected_ordering = std::strong_ordering::greater},
        SumInt64ComparisonTestCase{
            .terms = {1.0, -std::numeric_limits<double>::denorm_min()},
            .rhs = 1,
            .expected_ordering = std::strong_ordering::less},
        SumInt64ComparisonTestCase{
            .terms = {1.0, -std::numeric_limits<double>::denorm_min()},
            .rhs = 0,
            .expected_ordering = std::strong_ordering::greater},
        // Massive cancellation.
        SumInt64ComparisonTestCase{
            .terms = {1e100, 1e-100, -1e100},
            .rhs = 0,
            .expected_ordering = std::strong_ordering::greater},
        SumInt64ComparisonTestCase{
            .terms = {1e100, 1e-100, -1e100},
            .rhs = 1,
            .expected_ordering = std::strong_ordering::less},
        SumInt64ComparisonTestCase{
            .terms = {1e100, 5.0, -2.0, -1e100},
            .rhs = 3,
            .expected_ordering = std::strong_ordering::equal},
        SumInt64ComparisonTestCase{
            .terms = {1e100, 10.0, -1e100},
            .rhs = (int64_t{1} << 60) + 127,
            .expected_ordering = std::strong_ordering::less},
        // Integers requiring > 53 bits of precision (not exact as double).
        SumInt64ComparisonTestCase{
            .terms = {std::ldexp(1.0, 53), 1.0},
            .rhs = int64_t{1} << 53,
            .expected_ordering = std::strong_ordering::greater},
        SumInt64ComparisonTestCase{
            .terms = {std::ldexp(1.0, 53), 1.0},
            .rhs = (int64_t{1} << 53) + 1,
            .expected_ordering = std::strong_ordering::equal},
        SumInt64ComparisonTestCase{
            .terms = {std::ldexp(1.0, 53), 1.0},
            .rhs = (int64_t{1} << 53) + 2,
            .expected_ordering = std::strong_ordering::less},
        SumInt64ComparisonTestCase{
            .terms = {1e100, std::ldexp(1.0, 60), 1.0, -1e100},
            .rhs = int64_t{1} << 60,
            .expected_ordering = std::strong_ordering::greater},
        SumInt64ComparisonTestCase{
            .terms = {1e100, std::ldexp(1.0, 60), 1.0, -1e100},
            .rhs = (int64_t{1} << 60) + 1,
            .expected_ordering = std::strong_ordering::equal},
        SumInt64ComparisonTestCase{
            .terms = {1e100, std::ldexp(1.0, 60), 1.0, -1e100},
            .rhs = (int64_t{1} << 60) + 2,
            .expected_ordering = std::strong_ordering::less},
        SumInt64ComparisonTestCase{
            .terms = {std::ldexp(1.0, 60), 0.5},
            .rhs = int64_t{1} << 60,
            .expected_ordering = std::strong_ordering::greater},
        SumInt64ComparisonTestCase{
            .terms = {std::ldexp(1.0, 60), 0.5},
            .rhs = (int64_t{1} << 60) + 1,
            .expected_ordering = std::strong_ordering::less},
        // Extreme int64_t values (2^63 - 1 and -2^63).
        SumInt64ComparisonTestCase{
            .terms = {std::ldexp(1.0, 63), -1.0},
            .rhs = std::numeric_limits<int64_t>::max(),
            .expected_ordering = std::strong_ordering::equal},
        SumInt64ComparisonTestCase{
            .terms = {std::ldexp(1.0, 63), -1.0},
            .rhs = std::numeric_limits<int64_t>::max() - 1,
            .expected_ordering = std::strong_ordering::greater},
        SumInt64ComparisonTestCase{
            .terms = {std::ldexp(1.0, 63)},
            .rhs = std::numeric_limits<int64_t>::max(),
            .expected_ordering = std::strong_ordering::greater},
        SumInt64ComparisonTestCase{
            .terms = {-std::ldexp(1.0, 63)},
            .rhs = std::numeric_limits<int64_t>::min(),
            .expected_ordering = std::strong_ordering::equal},
        SumInt64ComparisonTestCase{
            .terms = {-std::ldexp(1.0, 63)},
            .rhs = std::numeric_limits<int64_t>::min() + 1,
            .expected_ordering = std::strong_ordering::less},
        SumInt64ComparisonTestCase{
            .terms = {-std::ldexp(1.0, 63), 1.0},
            .rhs = std::numeric_limits<int64_t>::min(),
            .expected_ordering = std::strong_ordering::greater},
        SumInt64ComparisonTestCase{
            .terms = {-std::ldexp(1.0, 63), 1.0},
            .rhs = std::numeric_limits<int64_t>::min() + 1,
            .expected_ordering = std::strong_ordering::equal},
        SumInt64ComparisonTestCase{
            .terms = {-std::ldexp(1.0, 63), -1.0},
            .rhs = std::numeric_limits<int64_t>::min(),
            .expected_ordering = std::strong_ordering::less},
        // Finite and infinite sums outside int64_t range.
        SumInt64ComparisonTestCase{
            .terms = {1e100},
            .rhs = std::numeric_limits<int64_t>::max(),
            .expected_ordering = std::strong_ordering::greater},
        SumInt64ComparisonTestCase{
            .terms = {1e100, kInf, -1e100},
            .rhs = std::numeric_limits<int64_t>::max(),
            .expected_ordering = std::strong_ordering::greater},
        SumInt64ComparisonTestCase{
            .terms = {-1e100, -kInf, 1e100},
            .rhs = std::numeric_limits<int64_t>::min(),
            .expected_ordering = std::strong_ordering::less}));

struct SumTightBoundsTestCase {
  std::vector<double> terms;
  std::pair<double, double> expected_bounds;
};

using SumTightBoundsTest = testing::TestWithParam<SumTightBoundsTestCase>;

TEST_P(SumTightBoundsTest, BoundsAndSpecificationInvariants) {
  const auto& param = GetParam();
  Sum sum;
  for (const double x : param.terms) {
    sum.Add(x);
  }

  // LooseBounds must be consistent with LooseLb / LooseUb and enclose
  // TightBounds.
  const auto [loose_lb, loose_ub] = sum.LooseBounds();
  EXPECT_EQ(sum.LooseLb(), loose_lb);
  EXPECT_EQ(sum.LooseUb(), loose_ub);
  EXPECT_LE(loose_lb, param.expected_bounds.first);
  EXPECT_GE(loose_ub, param.expected_bounds.second);

  // TightBounds must match TightLb / TightUb and expected_bounds.
  EXPECT_EQ(sum.TightBounds(), param.expected_bounds);
  EXPECT_EQ(sum.TightLb(), param.expected_bounds.first);
  EXPECT_EQ(sum.TightUb(), param.expected_bounds.second);

  // Specification invariant: either exact (lb == ub) or adjacent doubles
  // strictly enclosing the sum.
  if (param.expected_bounds.first == param.expected_bounds.second) {
    EXPECT_EQ(sum <=> param.expected_bounds.first, std::strong_ordering::equal);
  } else {
    EXPECT_EQ(param.expected_bounds.second,
              std::nextafter(param.expected_bounds.first, kInf));
    EXPECT_EQ(sum <=> param.expected_bounds.first,
              std::strong_ordering::greater);
    EXPECT_EQ(sum <=> param.expected_bounds.second, std::strong_ordering::less);
  }
}

TEST_P(SumTightBoundsTest, NegatedBounds) {
  const auto& param = GetParam();
  Sum sum = SumAll<double>(Negate(param.terms));
  const auto expected_neg_bounds = std::make_pair(
      param.expected_bounds.second == 0.0 ? 0.0 : -param.expected_bounds.second,
      param.expected_bounds.first == 0.0 ? 0.0 : -param.expected_bounds.first);

  EXPECT_EQ(sum.TightBounds(), expected_neg_bounds);
}

INSTANTIATE_TEST_SUITE_P(
    Values, SumTightBoundsTest,
    testing::Values(
        SumTightBoundsTestCase{.terms = {}, .expected_bounds = {0.0, 0.0}},
        SumTightBoundsTestCase{.terms = {0.0}, .expected_bounds = {0.0, 0.0}},
        SumTightBoundsTestCase{.terms = {-0.0}, .expected_bounds = {0.0, 0.0}},
        SumTightBoundsTestCase{.terms = {1.5, 2.25},
                               .expected_bounds = {3.75, 3.75}},
        SumTightBoundsTestCase{.terms = {1e100, 3.5, -1e100},
                               .expected_bounds = {3.5, 3.5}},
        SumTightBoundsTestCase{
            .terms = {std::numeric_limits<double>::max(),
                      std::numeric_limits<double>::denorm_min(),
                      -std::numeric_limits<double>::max()},
            .expected_bounds = {std::numeric_limits<double>::denorm_min(),
                                std::numeric_limits<double>::denorm_min()}},
        SumTightBoundsTestCase{
            .terms = {1.0, std::numeric_limits<double>::denorm_min()},
            .expected_bounds = {1.0, std::nextafter(1.0, kInf)}},
        SumTightBoundsTestCase{
            .terms = {1.0, -std::numeric_limits<double>::denorm_min()},
            .expected_bounds = {std::nextafter(1.0, 0.0), 1.0}},
        // Complete cancellation at largest exponent with non-zero dropped part
        // (exercises dichotomy in SlowTightBounds).
        SumTightBoundsTestCase{
            .terms = {std::ldexp(1.0, -500), std::ldexp(1.0, 500),
                      -std::ldexp(1.0, 500)},
            .expected_bounds = {std::ldexp(1.0, -500), std::ldexp(1.0, -500)}},
        SumTightBoundsTestCase{
            .terms = {std::ldexp(1.0, -500), std::ldexp(1.0, -600),
                      std::ldexp(1.0, 500), -std::ldexp(1.0, 500)},
            .expected_bounds = {std::ldexp(1.0, -500),
                                std::nextafter(std::ldexp(1.0, -500), kInf)}},
        // Finite overflow above double::max().
        SumTightBoundsTestCase{
            .terms = {std::numeric_limits<double>::max(), 1.0},
            .expected_bounds = {std::numeric_limits<double>::max(), kInf}},
        // Infinite sum.
        SumTightBoundsTestCase{.terms = {kInf, -1e100},
                               .expected_bounds = {kInf, kInf}}));

TEST(SumTest, UnderflowSubnormalBounds) {
  const double kDenormMin = std::numeric_limits<double>::denorm_min();

  Sum pos_underflow;
  pos_underflow.AddProd(kDenormMin, PowerOfTwo(-1));
  EXPECT_EQ(pos_underflow.TightBounds(), std::make_pair(0.0, kDenormMin));
  EXPECT_EQ(pos_underflow <=> 0.0, std::strong_ordering::greater);
  EXPECT_EQ(pos_underflow <=> kDenormMin, std::strong_ordering::less);
  EXPECT_EQ(pos_underflow <=> int64_t{0}, std::strong_ordering::greater);

  Sum neg_underflow;
  neg_underflow.AddProd(-kDenormMin, PowerOfTwo(-1));
  EXPECT_EQ(neg_underflow.TightBounds(), std::make_pair(-kDenormMin, 0.0));
  EXPECT_EQ(neg_underflow <=> 0.0, std::strong_ordering::less);
  EXPECT_EQ(neg_underflow <=> -kDenormMin, std::strong_ordering::greater);
  EXPECT_EQ(neg_underflow <=> int64_t{0}, std::strong_ordering::less);
}

TEST(SumTest, Int64ExtremeValues) {
  const int64_t kMinInt = std::numeric_limits<int64_t>::min();
  const int64_t kMaxInt = std::numeric_limits<int64_t>::max();

  EXPECT_EQ(Sum().Add(kMinInt).TightBounds(),
            std::make_pair(-std::ldexp(1.0, 63), -std::ldexp(1.0, 63)));
  EXPECT_EQ(Sum().Add(kMinInt) <=> kMinInt, std::strong_ordering::equal);
  EXPECT_EQ(Sum().Add(kMinInt) <=> kMinInt + 1, std::strong_ordering::less);
  EXPECT_EQ(Sum().Add(kMaxInt).TightBounds(),
            std::make_pair(std::nextafter(std::ldexp(1.0, 63), 0.0),
                           std::ldexp(1.0, 63)));
  EXPECT_EQ(Sum().Add(kMaxInt) <=> kMaxInt, std::strong_ordering::equal);
  EXPECT_EQ(Sum().Add(kMaxInt) <=> kMaxInt - 1, std::strong_ordering::greater);
  EXPECT_EQ(Sum().Add(kMaxInt).Add(kMinInt).TightBounds(),
            std::make_pair(-1.0, -1.0));
  EXPECT_EQ(Sum().Add(kMaxInt).Add(kMinInt) <=> int64_t{-1},
            std::strong_ordering::equal);
}

TEST(SumTest, AccumulatorUint128Overflow) {
  // 8 * (2^63 - 1)^2 = 2^129 - 2^67 + 8, which overflows a 128-bit unsigned
  // accumulator at exponent 0 in ExponentOrderedSumBounds.
  const int64_t kMaxInt = std::numeric_limits<int64_t>::max();
  Sum sum;
  for (int i = 0; i < 8; ++i) {
    sum.AddProd(kMaxInt, kMaxInt);
  }
  EXPECT_EQ(sum.TightBounds(),
            std::make_pair(std::nextafter(std::ldexp(1.0, 129), 0.0),
                           std::ldexp(1.0, 129)));

  sum.Add(-std::ldexp(1.0, 129)).Add(std::ldexp(1.0, 67));
  EXPECT_EQ(sum.TightBounds(), std::make_pair(8.0, 8.0));
  EXPECT_EQ(sum <=> int64_t{8}, std::strong_ordering::equal);
}

TEST(SumTest, IncrementalAdditionsAndQueries) {
  Sum sum;
  sum.Add(1e100).Add(1.0);

  // First query sorts current terms and tightens bounds.
  EXPECT_EQ(sum <=> 1e100, std::strong_ordering::greater);
  EXPECT_EQ(sum.TightBounds(),
            std::make_pair(1e100, std::nextafter(1e100, kInf)));
  // Repeated TightBounds() returns cached result.
  EXPECT_EQ(sum.TightBounds(),
            std::make_pair(1e100, std::nextafter(1e100, kInf)));

  // Adding a new term invalidates tight_bounds_ and exercises inplace_merge on
  // the next query.
  sum.Add(-1e100);
  EXPECT_LE(sum.LooseLb(), 1.0);
  EXPECT_GE(sum.LooseUb(), 1.0);
  EXPECT_EQ(sum <=> 1.0, std::strong_ordering::equal);
  EXPECT_EQ(sum <=> int64_t{1}, std::strong_ordering::equal);
  EXPECT_EQ(sum.TightBounds(), std::make_pair(1.0, 1.0));
  EXPECT_EQ(sum.LooseBounds(), std::make_pair(1.0, 1.0));

  // Adding another term after exact tightening untightens bounds again.
  sum.Add(std::numeric_limits<double>::denorm_min());
  EXPECT_EQ(sum <=> 1.0, std::strong_ordering::greater);
  EXPECT_EQ(sum <=> int64_t{1}, std::strong_ordering::greater);
  EXPECT_EQ(sum.TightBounds(), std::make_pair(1.0, std::nextafter(1.0, kInf)));
}

TEST(SumTest, InfiniteProductsAndConsistentSigns) {
  Sum pos_inf;
  pos_inf.Add(1e100)
      .AddProd(-kInf, -2.0, int64_t{3}, PowerOfTwo(2))
      .Add(-1e200)
      .Add(kInf);
  EXPECT_EQ(pos_inf.LooseBounds(), std::make_pair(kInf, kInf));
  EXPECT_EQ(pos_inf.TightBounds(), std::make_pair(kInf, kInf));
  EXPECT_EQ(pos_inf <=> kInf, std::strong_ordering::equal);
  EXPECT_EQ(pos_inf <=> std::numeric_limits<double>::max(),
            std::strong_ordering::greater);
  EXPECT_EQ(pos_inf <=> std::numeric_limits<int64_t>::max(),
            std::strong_ordering::greater);

  Sum neg_inf;
  neg_inf.Add(1e100).AddProd(kInf, int64_t{-3}).Add(-kInf);
  EXPECT_EQ(neg_inf.LooseBounds(), std::make_pair(-kInf, -kInf));
  EXPECT_EQ(neg_inf.TightBounds(), std::make_pair(-kInf, -kInf));
  EXPECT_EQ(neg_inf <=> -kInf, std::strong_ordering::equal);
  EXPECT_EQ(neg_inf <=> -std::numeric_limits<double>::max(),
            std::strong_ordering::less);
  EXPECT_EQ(neg_inf <=> std::numeric_limits<int64_t>::min(),
            std::strong_ordering::less);
}

TEST(SumTest, SumAllAndDotProductHelpers) {
  EXPECT_EQ(SumAll<double>({}).TightBounds(), std::make_pair(0.0, 0.0));
  EXPECT_EQ(SumAll<double>({1e100, 2.5, -1e100}).TightBounds(),
            std::make_pair(2.5, 2.5));
  EXPECT_EQ(SumAll<int64_t>({(int64_t{1} << 60) + 1, -(int64_t{1} << 60)})
                .TightBounds(),
            std::make_pair(1.0, 1.0));

  EXPECT_EQ((DotProduct<double, double>({}, {}).TightBounds()),
            std::make_pair(0.0, 0.0));
  EXPECT_EQ((DotProduct<double, double>({1e50, 1.5, -1e50}, {1e50, 2.0, 1e50})
                 .TightBounds()),
            std::make_pair(3.0, 3.0));
  EXPECT_EQ((DotProduct<int64_t, int64_t>(
                 {(int64_t{1} << 50) + 1, -(int64_t{1} << 50)},
                 {int64_t{1} << 50, (int64_t{1} << 50) + 1})
                 .TightBounds()),
            std::make_pair(0.0, 0.0));
  EXPECT_EQ(
      (DotProduct<double, int64_t>({0.5, -0.25}, {int64_t{10}, int64_t{4}})
           .TightBounds()),
      std::make_pair(4.0, 4.0));
  EXPECT_EQ(
      (DotProduct<int64_t, double>({int64_t{10}, int64_t{4}}, {0.5, -0.25})
           .TightBounds()),
      std::make_pair(4.0, 4.0));
}

TEST(SumDeathTest, InvalidInputsCrash) {
  const double kNaN = std::numeric_limits<double>::quiet_NaN();
  EXPECT_DEATH(Sum().Add(kNaN), "NaN not supported");
  EXPECT_DEATH(Sum().AddProd(1.0, kNaN), "NaN not supported");
  EXPECT_DEATH(Sum().AddProd(kInf, 0.0), "inf \\* 0 would give NaN");
  EXPECT_DEATH(Sum().AddProd(int64_t{0}, -kInf), "inf \\* 0 would give NaN");
  EXPECT_DEATH(Sum().Add(kInf).Add(-kInf),
               "Inconsistent infinite values in sum");
  EXPECT_DEATH(Sum().Add(-kInf).AddProd(kInf, 2.0),
               "Inconsistent infinite values in sum");
  EXPECT_DEATH((DotProduct<double, double>({1.0}, {1.0, 2.0})), "");
}

struct SumSaturatedFloorCeilTestCase {
  std::vector<double> terms;
  int64_t expected_floor;
  int64_t expected_ceil;
};

using SumSaturatedFloorCeilTest =
    testing::TestWithParam<SumSaturatedFloorCeilTestCase>;

TEST_P(SumSaturatedFloorCeilTest, SaturatedFloorAndCeilMatchExpected) {
  constexpr int64_t kMinInt = std::numeric_limits<int64_t>::min();
  constexpr int64_t kMaxInt = std::numeric_limits<int64_t>::max();
  const auto& param = GetParam();

  // Test on fresh Sum instances (exercising both loose-bounds and tight-bounds
  // paths) as well as after TightBounds() is cached.
  EXPECT_EQ(SumAll<double>(param.terms).SaturatedInt64Bounds(),
            std::make_pair(param.expected_floor, param.expected_ceil));
  EXPECT_EQ(SumAll<double>(param.terms).SaturatedFloor(), param.expected_floor);
  EXPECT_EQ(SumAll<double>(param.terms).SaturatedCeil(), param.expected_ceil);

  Sum sum = SumAll<double>(param.terms);
  sum.TightBounds();
  EXPECT_EQ(sum.SaturatedInt64Bounds(),
            std::make_pair(param.expected_floor, param.expected_ceil));
  EXPECT_EQ(sum.SaturatedFloor(), param.expected_floor);
  EXPECT_EQ(sum.SaturatedCeil(), param.expected_ceil);

  // Verify mathematical floor/ceil order invariants against CompareTo(int64_t).
  if (param.expected_floor > kMinInt) {
    EXPECT_GE(sum, param.expected_floor);
  }
  if (param.expected_floor < kMaxInt) {
    EXPECT_LT(sum, param.expected_floor + 1);
  }
  if (param.expected_ceil < kMaxInt) {
    EXPECT_LE(sum, param.expected_ceil);
  }
  if (param.expected_ceil > kMinInt) {
    EXPECT_GT(sum, param.expected_ceil - 1);
  }
}

INSTANTIATE_TEST_SUITE_P(
    Values, SumSaturatedFloorCeilTest,
    testing::Values(
        SumSaturatedFloorCeilTestCase{
            .terms = {}, .expected_floor = 0, .expected_ceil = 0},
        SumSaturatedFloorCeilTestCase{
            .terms = {3.0}, .expected_floor = 3, .expected_ceil = 3},
        SumSaturatedFloorCeilTestCase{
            .terms = {-3.0}, .expected_floor = -3, .expected_ceil = -3},
        SumSaturatedFloorCeilTestCase{
            .terms = {3.25}, .expected_floor = 3, .expected_ceil = 4},
        SumSaturatedFloorCeilTestCase{
            .terms = {-3.25}, .expected_floor = -4, .expected_ceil = -3},
        // Subnormal perturbations around integers.
        SumSaturatedFloorCeilTestCase{
            .terms = {1.0, std::numeric_limits<double>::denorm_min()},
            .expected_floor = 1,
            .expected_ceil = 2},
        SumSaturatedFloorCeilTestCase{
            .terms = {1.0, -std::numeric_limits<double>::denorm_min()},
            .expected_floor = 0,
            .expected_ceil = 1},
        SumSaturatedFloorCeilTestCase{
            .terms = {-1.0, std::numeric_limits<double>::denorm_min()},
            .expected_floor = -1,
            .expected_ceil = 0},
        SumSaturatedFloorCeilTestCase{
            .terms = {-1.0, -std::numeric_limits<double>::denorm_min()},
            .expected_floor = -2,
            .expected_ceil = -1},
        // Massive cancellation.
        SumSaturatedFloorCeilTestCase{.terms = {1e100, 3.25, -1e100},
                                      .expected_floor = 3,
                                      .expected_ceil = 4},
        SumSaturatedFloorCeilTestCase{.terms = {1e100, 1e-100, -1e100},
                                      .expected_floor = 0,
                                      .expected_ceil = 1},
        SumSaturatedFloorCeilTestCase{.terms = {1e100, -1e-100, -1e100},
                                      .expected_floor = -1,
                                      .expected_ceil = 0},
        // High-precision values with >53 significant bits (requiring binary
        // search between adjacent doubles).
        SumSaturatedFloorCeilTestCase{.terms = {std::ldexp(1.0, 53), 0.5},
                                      .expected_floor = int64_t{1} << 53,
                                      .expected_ceil = (int64_t{1} << 53) + 1},
        SumSaturatedFloorCeilTestCase{.terms = {std::ldexp(1.0, 53), 1.0},
                                      .expected_floor = (int64_t{1} << 53) + 1,
                                      .expected_ceil = (int64_t{1} << 53) + 1},
        SumSaturatedFloorCeilTestCase{.terms = {std::ldexp(1.0, 53), 1.5},
                                      .expected_floor = (int64_t{1} << 53) + 1,
                                      .expected_ceil = (int64_t{1} << 53) + 2},
        SumSaturatedFloorCeilTestCase{
            .terms = {1e100, std::ldexp(1.0, 60), 123.25, -1e100},
            .expected_floor = (int64_t{1} << 60) + 123,
            .expected_ceil = (int64_t{1} << 60) + 124},
        SumSaturatedFloorCeilTestCase{
            .terms = {-1e100, -std::ldexp(1.0, 60), -123.25, 1e100},
            .expected_floor = -(int64_t{1} << 60) - 124,
            .expected_ceil = -(int64_t{1} << 60) - 123},
        // Boundaries and saturation near INT64_MAX (2^63 - 1).
        SumSaturatedFloorCeilTestCase{
            .terms = {std::ldexp(1.0, 63), -1.5},
            .expected_floor = std::numeric_limits<int64_t>::max() - 1,
            .expected_ceil = std::numeric_limits<int64_t>::max()},
        SumSaturatedFloorCeilTestCase{
            .terms = {std::ldexp(1.0, 63), -1.0},
            .expected_floor = std::numeric_limits<int64_t>::max(),
            .expected_ceil = std::numeric_limits<int64_t>::max()},
        SumSaturatedFloorCeilTestCase{
            .terms = {std::ldexp(1.0, 63), -0.5},
            .expected_floor = std::numeric_limits<int64_t>::max(),
            .expected_ceil = std::numeric_limits<int64_t>::max()},
        SumSaturatedFloorCeilTestCase{
            .terms = {std::ldexp(1.0, 63)},
            .expected_floor = std::numeric_limits<int64_t>::max(),
            .expected_ceil = std::numeric_limits<int64_t>::max()},
        SumSaturatedFloorCeilTestCase{
            .terms = {1e100},
            .expected_floor = std::numeric_limits<int64_t>::max(),
            .expected_ceil = std::numeric_limits<int64_t>::max()},
        SumSaturatedFloorCeilTestCase{
            .terms = {kInf, -1e100},
            .expected_floor = std::numeric_limits<int64_t>::max(),
            .expected_ceil = std::numeric_limits<int64_t>::max()},
        // Boundaries and saturation near INT64_MIN (-2^63).
        SumSaturatedFloorCeilTestCase{
            .terms = {-std::ldexp(1.0, 63), 1.0},
            .expected_floor = std::numeric_limits<int64_t>::min() + 1,
            .expected_ceil = std::numeric_limits<int64_t>::min() + 1},
        SumSaturatedFloorCeilTestCase{
            .terms = {-std::ldexp(1.0, 63), 0.5},
            .expected_floor = std::numeric_limits<int64_t>::min(),
            .expected_ceil = std::numeric_limits<int64_t>::min() + 1},
        SumSaturatedFloorCeilTestCase{
            .terms = {-std::ldexp(1.0, 63)},
            .expected_floor = std::numeric_limits<int64_t>::min(),
            .expected_ceil = std::numeric_limits<int64_t>::min()},
        SumSaturatedFloorCeilTestCase{
            .terms = {-std::ldexp(1.0, 63), -0.5},
            .expected_floor = std::numeric_limits<int64_t>::min(),
            .expected_ceil = std::numeric_limits<int64_t>::min()},
        SumSaturatedFloorCeilTestCase{
            .terms = {-std::ldexp(1.0, 63), -1.0},
            .expected_floor = std::numeric_limits<int64_t>::min(),
            .expected_ceil = std::numeric_limits<int64_t>::min()},
        SumSaturatedFloorCeilTestCase{
            .terms = {-1e100},
            .expected_floor = std::numeric_limits<int64_t>::min(),
            .expected_ceil = std::numeric_limits<int64_t>::min()},
        SumSaturatedFloorCeilTestCase{
            .terms = {-kInf, 1e100},
            .expected_floor = std::numeric_limits<int64_t>::min(),
            .expected_ceil = std::numeric_limits<int64_t>::min()}));

TEST(SumTest, SaturatedFloorAndCeilUnderflowSubnormals) {
  const double kDenormMin = std::numeric_limits<double>::denorm_min();

  Sum pos_underflow;
  pos_underflow.AddProd(kDenormMin, PowerOfTwo(-1));
  EXPECT_EQ(pos_underflow.SaturatedFloor(), 0);
  EXPECT_EQ(pos_underflow.SaturatedCeil(), 1);

  Sum neg_underflow;
  neg_underflow.AddProd(-kDenormMin, PowerOfTwo(-1));
  EXPECT_EQ(neg_underflow.SaturatedFloor(), -1);
  EXPECT_EQ(neg_underflow.SaturatedCeil(), 0);
}

}  // namespace
}  // namespace operations_research
