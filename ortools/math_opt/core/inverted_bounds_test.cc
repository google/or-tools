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

#include "ortools/math_opt/core/inverted_bounds.h"

#include <cstddef>
#include <cstdint>
#include <sstream>
#include <vector>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"

namespace operations_research::math_opt {
namespace {

using ::testing::status::StatusIs;

TEST(InvertedBoundsTest, Empty) {
  const InvertedBounds empty;
  EXPECT_TRUE(empty.empty());
  EXPECT_OK(empty.ToStatus());
}

TEST(InvertedBoundsTest, SomeVariables) {
  const InvertedBounds inverted_bounds = {.variables = {2, 4, 6}};
  EXPECT_FALSE(inverted_bounds.empty());
  EXPECT_THAT(
      inverted_bounds.ToStatus(),
      StatusIs(absl::StatusCode::kInvalidArgument,
               "variables with ids 2,4,6 have lower_bound > upper_bound"));
}

TEST(InvertedBoundsTest, TooManyVariables) {
  InvertedBounds inverted_bounds = {};
  for (std::size_t i = 0; i < kMaxInvertedBounds + 1; ++i) {
    inverted_bounds.variables.push_back(i);
  }

  EXPECT_FALSE(inverted_bounds.empty());

  std::ostringstream expected;
  expected << "variables with ids ";
  for (std::size_t i = 0; i < kMaxInvertedBounds; ++i) {
    if (i != 0) {
      expected << ',';
    }
    expected << i;
  }
  expected << "... have lower_bound > upper_bound";
  EXPECT_THAT(inverted_bounds.ToStatus(),
              StatusIs(absl::StatusCode::kInvalidArgument, expected.str()));
}

TEST(InvertedBoundsTest, SomeLinearConstraints) {
  const InvertedBounds inverted_bounds = {.linear_constraints = {2, 4, 6}};
  EXPECT_FALSE(inverted_bounds.empty());
  EXPECT_THAT(
      inverted_bounds.ToStatus(),
      StatusIs(
          absl::StatusCode::kInvalidArgument,
          "linear constraints with ids 2,4,6 have lower_bound > upper_bound"));
}

TEST(InvertedBoundsTest, TooManyLinearConstraints) {
  InvertedBounds inverted_bounds = {};
  for (std::size_t i = 0; i < kMaxInvertedBounds + 1; ++i) {
    inverted_bounds.linear_constraints.push_back(i);
  }

  EXPECT_FALSE(inverted_bounds.empty());

  std::ostringstream expected;
  expected << "linear constraints with ids ";
  for (std::size_t i = 0; i < kMaxInvertedBounds; ++i) {
    if (i != 0) {
      expected << ',';
    }
    expected << i;
  }
  expected << "... have lower_bound > upper_bound";
  EXPECT_THAT(inverted_bounds.ToStatus(),
              StatusIs(absl::StatusCode::kInvalidArgument, expected.str()));
}

TEST(InvertedBoundsTest, SomeVariablesAndLinearConstraints) {
  const InvertedBounds inverted_bounds = {.variables = {2, 4, 6},
                                          .linear_constraints = {3, 7, 8}};
  EXPECT_FALSE(inverted_bounds.empty());
  EXPECT_THAT(
      inverted_bounds.ToStatus(),
      StatusIs(absl::StatusCode::kInvalidArgument,
               "variables with ids 2,4,6 and linear constraints with ids 3,7,8 "
               "have lower_bound > upper_bound"));
}

}  // namespace
}  // namespace operations_research::math_opt
