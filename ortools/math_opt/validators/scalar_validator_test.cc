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

#include "ortools/math_opt/validators/scalar_validator.h"

#include <cmath>
#include <limits>
#include <string>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"

namespace operations_research {
namespace math_opt {
namespace {

using ::absl::StatusCode;
using ::testing::HasSubstr;
using ::testing::status::StatusIs;

constexpr double kInf = std::numeric_limits<double>::infinity();

TEST(CheckScalarNoNanNoInfTests, ScalarIsNotNanOrInf) {
  EXPECT_OK(CheckScalarNoNanNoInf(2.0));
}

TEST(CheckScalarNoNanNoInfTests, ScalarIsNan) {
  EXPECT_THAT(
      CheckScalarNoNanNoInf(std::nan("")),
      StatusIs(StatusCode::kInvalidArgument,
               HasSubstr("Expected no NaN or inf but found value: nan")));
}

TEST(CheckScalarNoNanNoInfTests, ScalarIsInf) {
  EXPECT_THAT(
      CheckScalarNoNanNoInf(kInf),
      StatusIs(StatusCode::kInvalidArgument,
               HasSubstr("Expected no NaN or inf but found value: inf")));
}

TEST(CheckScalarTests, NanCkeck) {
  const DoubleOptions options;
  EXPECT_OK(CheckScalar(2.0, options));
  EXPECT_THAT(
      CheckScalar(std::nan(""), options),
      StatusIs(StatusCode::kInvalidArgument, HasSubstr("Invalid NaN value")));
}

TEST(CheckScalarTests, InfinityCheck) {
  const DoubleOptions options = {.allow_positive_infinity = false,
                                 .allow_negative_infinity = false};
  EXPECT_OK(CheckScalar(2.0, options));
  EXPECT_THAT(CheckScalar(kInf, options),
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("Invalid positive infinite value")));
  EXPECT_THAT(CheckScalar(-kInf, options),
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("Invalid negative infinite value")));
}

TEST(CheckScalarTests, PositiveCheck) {
  const DoubleOptions options = {.allow_positive = false};
  EXPECT_OK(CheckScalar(-2.0, options));
  EXPECT_OK(CheckScalar(0.0, options));
  EXPECT_THAT(CheckScalar(2.0, options),
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("Invalid positive value = 2")));
}

TEST(CheckScalarTests, NegativeCheck) {
  const DoubleOptions options = {.allow_negative = false};
  EXPECT_OK(CheckScalar(2.0, options));
  EXPECT_OK(CheckScalar(0.0, options));
  EXPECT_THAT(CheckScalar(-2.0, options),
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("Invalid negative value = -2")));
}

}  // namespace
}  // namespace math_opt
}  // namespace operations_research
