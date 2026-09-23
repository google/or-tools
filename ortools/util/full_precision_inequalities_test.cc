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
#include <utility>
#include <vector>

#include "absl/types/span.h"
#include "gtest/gtest.h"

namespace operations_research {
namespace {

struct IsDotProductCmpOrEqualTestCase {
  std::vector<double> a;
  std::vector<double> b;
  double bound;
  bool expect_smaller_or_equal;
};

using IsDotProductCmpOrEqualTest =
    testing::TestWithParam<IsDotProductCmpOrEqualTestCase>;

std::vector<double> Negate(const std::vector<double>& v) {
  std::vector<double> negated;
  negated.reserve(v.size());
  for (const double x : v) {
    negated.push_back(-x);
  }
  return negated;
}

TEST_P(IsDotProductCmpOrEqualTest, PosPos) {
  auto param = GetParam();
  EXPECT_EQ(IsDotProductSmallerOrEqual(param.a, param.b, param.bound),
            param.expect_smaller_or_equal);
}

TEST_P(IsDotProductCmpOrEqualTest, PosNeg) {
  auto param = GetParam();
  EXPECT_EQ(IsDotProductGreaterOrEqual(param.a, Negate(param.b), -param.bound),
            param.expect_smaller_or_equal);
}

TEST_P(IsDotProductCmpOrEqualTest, NegPos) {
  auto param = GetParam();
  EXPECT_EQ(IsDotProductGreaterOrEqual(Negate(param.a), param.b, -param.bound),
            param.expect_smaller_or_equal);
}

TEST_P(IsDotProductCmpOrEqualTest, NegNeg) {
  auto param = GetParam();
  EXPECT_EQ(
      IsDotProductSmallerOrEqual(Negate(param.a), Negate(param.b), param.bound),
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
                                       .expect_smaller_or_equal = true}));

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
