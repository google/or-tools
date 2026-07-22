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

#include "ortools/util/rational_approximation.h"

#include <limits>

#include "gtest/gtest.h"
#include "ortools/util/fp_utils.h"

namespace operations_research {
namespace {
static const double kEpsilon = std::numeric_limits<double>::epsilon();

TEST(RationalApproximation, ContinuedFraction) {
  Fraction fraction = RationalApproximation(2.0 / 3, kEpsilon);
  EXPECT_EQ(fraction.first, 2);
  EXPECT_EQ(fraction.second, 3);
  const double kTestedNumber = 17.373281721478865;
  fraction = RationalApproximation(kTestedNumber, kEpsilon);
  // The result for fraction.first and fraction.second varies depending on the
  // type for Fractional (kTestedNumber cannot be represented accurately
  // in a float), so we just test that the fraction is close enough.
  EXPECT_COMPARABLE(kTestedNumber,
                    static_cast<double>(fraction.first) /
                        static_cast<double>(fraction.second),
                    kEpsilon);
  fraction = RationalApproximation(0.4214, kEpsilon);
  EXPECT_EQ(fraction.first, 2107);
  EXPECT_EQ(fraction.second, 5000);
  fraction = RationalApproximation(0.42139999999999994, kEpsilon);
  EXPECT_EQ(fraction.first, 2107);
  EXPECT_EQ(fraction.second, 5000);
  // Expects rational approximations within a given precision.
  fraction = RationalApproximation(0.66667, 1e-5);
  EXPECT_EQ(fraction.first, 2);
  EXPECT_EQ(fraction.second, 3);
  fraction = RationalApproximation(0.1428572, 1e-6);
  EXPECT_EQ(fraction.first, 1);
  EXPECT_EQ(fraction.second, 7);
}

}  // anonymous namespace
}  // namespace operations_research
