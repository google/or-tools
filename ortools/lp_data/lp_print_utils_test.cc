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

#include "ortools/lp_data/lp_print_utils.h"

#include "gtest/gtest.h"
#include "ortools/lp_data/lp_types.h"

namespace operations_research {
namespace glop {
namespace {

TEST(LpPrintUtilsTest, DisplayNumbers) {
  EXPECT_EQ(Stringify(3.0 / 4), "0.75");
  EXPECT_EQ(Stringify(-3.0 / 4), "-0.75");
  EXPECT_EQ(Stringify(kInfinity), "inf");
  EXPECT_EQ(Stringify(-kInfinity), "-inf");
}

TEST(LpPrintUtilsTest, DisplayNumbersAsFraction) {
  EXPECT_EQ(Stringify(Fractional(2.0 / 3), true), "2/3");
  EXPECT_EQ(Stringify(Fractional(-2.0 / 3), true), "-2/3");
  EXPECT_EQ(Stringify(Fractional(kInfinity), true), "inf");
  EXPECT_EQ(Stringify(Fractional(-kInfinity), true), "-inf");
  // Expects fraction display for approximations computed within a given
  // precision.
  EXPECT_EQ(StringifyRational(0.66667, 1.0e-5), "2/3");
  EXPECT_EQ(StringifyRational(-0.1428572, 1.0e-6), "-1/7");
}

TEST(LpPrintUtilsTest, DisplayNumbersAsDecimals) {
  EXPECT_EQ(Stringify(Fractional(3.0 / 4), false), "0.75");
  EXPECT_EQ(Stringify(Fractional(-3.0 / 4), false), "-0.75");
  EXPECT_EQ(Stringify(Fractional(kInfinity), false), "inf");
  EXPECT_EQ(Stringify(Fractional(-kInfinity), false), "-inf");
}

TEST(LpPrintUtilsTest, DisplayMonomialsWithFractions) {
  EXPECT_EQ(StringifyMonomial(Fractional(0), "x", true), "");
  EXPECT_EQ(StringifyMonomial(Fractional(1), "x", true), " + x");
  EXPECT_EQ(StringifyMonomial(Fractional(-1), "x", true), " - x");
  EXPECT_EQ(StringifyMonomial(Fractional(2), "x", true), " + 2 x");
  EXPECT_EQ(StringifyMonomial(Fractional(-2), "x", true), " - 2 x");
  EXPECT_EQ(StringifyMonomial(Fractional(3.0 / 4), "x", true), " + 3/4 x");
  EXPECT_EQ(StringifyMonomial(Fractional(-3.0 / 4), "x", true), " - 3/4 x");
  EXPECT_EQ(StringifyMonomial(Fractional(3.0 / 4), "x", true), " + 3/4 x");
  EXPECT_EQ(StringifyMonomial(Fractional(-3.0 / 4), "x", true), " - 3/4 x");
  EXPECT_EQ(StringifyMonomial(Fractional(kInfinity), "x", true), " + inf x");
  EXPECT_EQ(StringifyMonomial(Fractional(-kInfinity), "x", true), " - inf x");
}

TEST(LpPrintUtilsTest, DisplayMonomialsWithDecimals) {
  EXPECT_EQ(StringifyMonomial(Fractional(0), "x", false), "");
  EXPECT_EQ(StringifyMonomial(Fractional(1), "x", false), " + x");
  EXPECT_EQ(StringifyMonomial(Fractional(-1), "x", false), " - x");
  EXPECT_EQ(StringifyMonomial(Fractional(2), "x", false), " + 2 x");
  EXPECT_EQ(StringifyMonomial(Fractional(-2), "x", false), " - 2 x");
  EXPECT_EQ(StringifyMonomial(Fractional(kInfinity), "x", false), " + inf x");
  EXPECT_EQ(StringifyMonomial(Fractional(-kInfinity), "x", false), " - inf x");
}

}  // namespace
}  // namespace glop
}  // namespace operations_research
