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

#include "ortools/math_opt/solvers/glpk/gap.h"

#include <cmath>
#include <limits>
#include <vector>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/util/fp_roundtrip_conv.h"

namespace operations_research::math_opt {
namespace {

using ::testing::IsNan;

constexpr double kInf = std::numeric_limits<double>::infinity();
constexpr double kEpsilon = std::numeric_limits<double>::epsilon();
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

// Returns some non-negative values (and thus non-NaN) used to tests cases where
// WorstGLPKDualBound() should either return them verbatim or return the same
// constant.
std::vector<double> SomeNonNegativeValues() {
  return {0.0,
          kInf,
          std::numeric_limits<double>::max(),
          std::numeric_limits<double>::min(),
          std::numeric_limits<double>::denorm_min(),
          std::nextafter(0.0, kInf),
          12.345};
}

TEST(WorstGLPKDualBoundTest, ZeroGapLimit) {
  for (const double positive_objective_value : SomeNonNegativeValues()) {
    // First test with maximization.
    EXPECT_EQ(
        WorstGLPKDualBound(
            /*is_maximize=*/true, /*objective_value=*/positive_objective_value,
            /*relative_gap_limit=*/0.0),
        positive_objective_value)
        << "objective_value: "
        << RoundTripDoubleFormat(positive_objective_value);

    // Test with the corresponding negative value.
    EXPECT_EQ(
        WorstGLPKDualBound(
            /*is_maximize=*/true, /*objective_value=*/-positive_objective_value,
            /*relative_gap_limit=*/0.0),
        -positive_objective_value)
        << "objective_value: "
        << RoundTripDoubleFormat(-positive_objective_value);

    // Repeat tests with minimization.
    EXPECT_EQ(
        WorstGLPKDualBound(
            /*is_maximize=*/false, /*objective_value=*/positive_objective_value,
            /*relative_gap_limit=*/0.0),
        positive_objective_value)
        << "objective_value: "
        << RoundTripDoubleFormat(positive_objective_value);
    EXPECT_EQ(WorstGLPKDualBound(
                  /*is_maximize=*/false,
                  /*objective_value=*/-positive_objective_value,
                  /*relative_gap_limit=*/0.0),
              -positive_objective_value)
        << "objective_value: "
        << RoundTripDoubleFormat(-positive_objective_value);
  }
  EXPECT_THAT(WorstGLPKDualBound(/*is_maximize=*/true, /*objective_value=*/kNaN,
                                 /*relative_gap_limit=*/0.0),
              IsNan());
  EXPECT_THAT(
      WorstGLPKDualBound(/*is_maximize=*/false,
                         /*objective_value=*/kNaN, /*relative_gap_limit=*/0.0),
      IsNan());
}

TEST(WorstGLPKDualBoundTest, NaNGapLimit) {
  for (const double positive_objective_value : SomeNonNegativeValues()) {
    // First test with maximization.
    EXPECT_EQ(
        WorstGLPKDualBound(
            /*is_maximize=*/true, /*objective_value=*/positive_objective_value,
            /*relative_gap_limit=*/kNaN),
        positive_objective_value)
        << "objective_value: "
        << RoundTripDoubleFormat(positive_objective_value);

    // Test with the corresponding negative value.
    EXPECT_EQ(
        WorstGLPKDualBound(
            /*is_maximize=*/true, /*objective_value=*/-positive_objective_value,
            /*relative_gap_limit=*/kNaN),
        -positive_objective_value)
        << "objective_value: "
        << RoundTripDoubleFormat(-positive_objective_value);

    // Repeat tests with minimization.
    EXPECT_EQ(
        WorstGLPKDualBound(
            /*is_maximize=*/false, /*objective_value=*/positive_objective_value,
            /*relative_gap_limit=*/kNaN),
        positive_objective_value)
        << "objective_value: "
        << RoundTripDoubleFormat(positive_objective_value);
    EXPECT_EQ(WorstGLPKDualBound(
                  /*is_maximize=*/false,
                  /*objective_value=*/-positive_objective_value,
                  /*relative_gap_limit=*/kNaN),
              -positive_objective_value)
        << "objective_value: "
        << RoundTripDoubleFormat(-positive_objective_value);
  }
  EXPECT_THAT(WorstGLPKDualBound(/*is_maximize=*/true, /*objective_value=*/kNaN,
                                 /*relative_gap_limit=*/kNaN),
              IsNan());
  EXPECT_THAT(
      WorstGLPKDualBound(/*is_maximize=*/false,
                         /*objective_value=*/kNaN, /*relative_gap_limit=*/kNaN),
      IsNan());
}

TEST(WorstGLPKDualBoundTest, InfiniteGapLimit) {
  for (const double positive_objective_value : SomeNonNegativeValues()) {
    // First test with maximization.
    EXPECT_EQ(
        WorstGLPKDualBound(
            /*is_maximize=*/true, /*objective_value=*/positive_objective_value,
            /*relative_gap_limit=*/kInf),
        kInf)
        << "objective_value: "
        << RoundTripDoubleFormat(positive_objective_value);

    // Test with the corresponding negative value.
    EXPECT_EQ(
        WorstGLPKDualBound(
            /*is_maximize=*/true, /*objective_value=*/-positive_objective_value,
            /*relative_gap_limit=*/kInf),
        kInf)
        << "objective_value: "
        << RoundTripDoubleFormat(-positive_objective_value);

    // Repeat tests with minimization.
    EXPECT_EQ(
        WorstGLPKDualBound(
            /*is_maximize=*/false, /*objective_value=*/positive_objective_value,
            /*relative_gap_limit=*/kInf),
        -kInf)
        << "objective_value: "
        << RoundTripDoubleFormat(positive_objective_value);
    EXPECT_EQ(WorstGLPKDualBound(
                  /*is_maximize=*/false,
                  /*objective_value=*/-positive_objective_value,
                  /*relative_gap_limit=*/kInf),
              -kInf)
        << "objective_value: "
        << RoundTripDoubleFormat(-positive_objective_value);
  }
  EXPECT_THAT(WorstGLPKDualBound(/*is_maximize=*/true, /*objective_value=*/kNaN,
                                 /*relative_gap_limit=*/kInf),
              IsNan());
  EXPECT_THAT(
      WorstGLPKDualBound(/*is_maximize=*/false,
                         /*objective_value=*/kNaN, /*relative_gap_limit=*/kInf),
      IsNan());
}

TEST(WorstGLPKDualBoundTest, FiniteGapLimit) {
  EXPECT_EQ(
      WorstGLPKDualBound(/*is_maximize=*/false,
                         /*objective_value=*/3.0, /*relative_gap_limit=*/0.5),
      3.0 - 0.5 * (3.0 + kEpsilon));
  EXPECT_EQ(
      WorstGLPKDualBound(/*is_maximize=*/true,
                         /*objective_value=*/3.0, /*relative_gap_limit=*/0.5),
      3.0 + 0.5 * (3.0 + kEpsilon));
  EXPECT_EQ(
      WorstGLPKDualBound(/*is_maximize=*/false,
                         /*objective_value=*/-3.0, /*relative_gap_limit=*/0.5),
      -3.0 - 0.5 * (3.0 + kEpsilon));
  EXPECT_EQ(
      WorstGLPKDualBound(/*is_maximize=*/true,
                         /*objective_value=*/-3.0, /*relative_gap_limit=*/0.5),
      -3.0 + 0.5 * (3.0 + kEpsilon));

  EXPECT_EQ(
      WorstGLPKDualBound(/*is_maximize=*/false,
                         /*objective_value=*/3.0, /*relative_gap_limit=*/5.0),
      3.0 - 5.0 * (3.0 + kEpsilon));
  EXPECT_EQ(
      WorstGLPKDualBound(/*is_maximize=*/true,
                         /*objective_value=*/3.0, /*relative_gap_limit=*/5.0),
      3.0 + 5.0 * (3.0 + kEpsilon));
  EXPECT_EQ(
      WorstGLPKDualBound(/*is_maximize=*/false,
                         /*objective_value=*/-3.0, /*relative_gap_limit=*/5.0),
      -3.0 - 5.0 * (3.0 + kEpsilon));
  EXPECT_EQ(
      WorstGLPKDualBound(/*is_maximize=*/true,
                         /*objective_value=*/-3.0, /*relative_gap_limit=*/5.0),
      -3.0 + 5.0 * (3.0 + kEpsilon));
}

}  // namespace
}  // namespace operations_research::math_opt
