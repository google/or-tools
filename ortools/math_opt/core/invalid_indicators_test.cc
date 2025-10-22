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

#include "ortools/math_opt/core/invalid_indicators.h"

#include <cstdint>
#include <sstream>
#include <vector>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"

namespace operations_research::math_opt {
namespace {

using ::testing::status::StatusIs;

TEST(InvalidIndicatorsTest, Empty) {
  const InvalidIndicators empty;
  EXPECT_TRUE(empty.invalid_indicators.empty());
  EXPECT_OK(empty.ToStatus());
}

TEST(InvalidIndicatorsTest, SomeEntries) {
  const InvalidIndicators invalid_indicators = {
      .invalid_indicators = {{.variable = 1, .constraint = 2},
                             {.variable = 3, .constraint = 4}}};
  EXPECT_FALSE(invalid_indicators.invalid_indicators.empty());
  EXPECT_THAT(invalid_indicators.ToStatus(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "the following (indicator constraint ID, indicator "
                       "variable ID) pairs are invalid as the indicator "
                       "variable is not binary: (2, 1), (4, 3)"));
}

TEST(InvalidIndicatorsTest, TooManyEntries) {
  InvalidIndicators invalid_indicators = {};
  for (int64_t i = 0; i < kMaxNonBinaryIndicatorVariables + 1; ++i) {
    invalid_indicators.invalid_indicators.push_back(
        {.variable = 10 + i, .constraint = i});
  }

  EXPECT_FALSE(invalid_indicators.invalid_indicators.empty());

  std::ostringstream expected;
  expected << "the following (indicator constraint ID, indicator variable ID) "
              "pairs are invalid as the indicator variable is not binary: ";
  for (int64_t i = 0; i < kMaxNonBinaryIndicatorVariables; ++i) {
    expected << "(" << i << ", " << 10 + i << "), ";
  }
  expected << "...";
  EXPECT_THAT(invalid_indicators.ToStatus(),
              StatusIs(absl::StatusCode::kInvalidArgument, expected.str()));
}

TEST(InvalidIndicatorsTest, Sort) {
  InvalidIndicators invalid_indicators = {
      .invalid_indicators = {{.variable = 1, .constraint = 3},
                             {.variable = 2, .constraint = 2},
                             {.variable = 3, .constraint = 1}}};
  invalid_indicators.Sort();
  ASSERT_EQ(invalid_indicators.invalid_indicators.size(), 3);
  EXPECT_THAT(invalid_indicators.invalid_indicators[0].constraint, 1);
  EXPECT_THAT(invalid_indicators.invalid_indicators[1].constraint, 2);
  EXPECT_THAT(invalid_indicators.invalid_indicators[2].constraint, 3);
}

}  // namespace
}  // namespace operations_research::math_opt
