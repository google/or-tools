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

#include "ortools/math_opt/cpp/formatters.h"

#include <cmath>
#include <string>

#include "gtest/gtest.h"
#include "ortools/math_opt/testing/stream.h"
#include "ortools/util/fp_roundtrip_conv_testing.h"
#include "util/gtl/extend/debug_printing.h"
#include "util/gtl/extend/equality.h"
#include "util/gtl/extend/extend.h"

namespace operations_research::math_opt {

namespace {

struct FormattersTestCase
    : gtl::Extend<FormattersTestCase>::With<gtl::EqualityExtension,
                                            gtl::DebugPrintingExtension> {
  const std::string test_name;
  const double x;
  const bool is_first;
  const std::string expected_leading_coefficient;
  const std::string expected_constant_term;
};

using FormattersTest = testing::TestWithParam<FormattersTestCase>;

TEST_P(FormattersTest, LeadingCoefficientFormatter) {
  const FormattersTestCase& test_case = GetParam();
  EXPECT_EQ(StreamToString(
                LeadingCoefficientFormatter(test_case.x, test_case.is_first)),
            test_case.expected_leading_coefficient);
}

TEST_P(FormattersTest, ConstantFormatter) {
  const FormattersTestCase& test_case = GetParam();
  EXPECT_EQ(StreamToString(ConstantFormatter(test_case.x, test_case.is_first)),
            test_case.expected_constant_term);
}

INSTANTIATE_TEST_SUITE_P(
    FormattersTests, FormattersTest,
    testing::ValuesIn<FormattersTestCase>(
        {{.test_name = "positive",
          .x = 1.2,
          .is_first = false,
          .expected_leading_coefficient = " + 1.2*",
          .expected_constant_term = " + 1.2"},
         {.test_name = "positive_first",
          .x = 1.2,
          .is_first = true,
          .expected_leading_coefficient = "1.2*",
          .expected_constant_term = "1.2"},
         {.test_name = "negative",
          .x = -4,
          .is_first = false,
          .expected_leading_coefficient = " - 4*",
          .expected_constant_term = " - 4"},
         {.test_name = "negative_first",
          .x = -4.35,
          .is_first = true,
          .expected_leading_coefficient = "-4.35*",
          .expected_constant_term = "-4.35"},
         {.test_name = "one",
          .x = 1,
          .is_first = false,
          .expected_leading_coefficient = " + ",
          .expected_constant_term = " + 1"},
         {.test_name = "one_first",
          .x = 1,
          .is_first = true,
          .expected_leading_coefficient = "",
          .expected_constant_term = "1"},
         {.test_name = "minus_one",
          .x = -1,
          .is_first = false,
          .expected_leading_coefficient = " - ",
          .expected_constant_term = " - 1"},
         {.test_name = "minus_one_first",
          .x = -1,
          .is_first = true,
          .expected_leading_coefficient = "-",
          .expected_constant_term = "-1"},
         {.test_name = "zero",
          .x = 0,
          .is_first = false,
          .expected_leading_coefficient = " + 0*",
          .expected_constant_term = ""},
         {.test_name = "zero_first",
          .x = 0,
          .is_first = true,
          .expected_leading_coefficient = "0*",
          .expected_constant_term = "0"},
         {.test_name = "nan",
          .x = std::nan(""),
          .is_first = false,
          .expected_leading_coefficient = " + nan*",
          .expected_constant_term = " + nan"},
         {.test_name = "nan_first",
          .x = std::nan(""),
          .is_first = true,
          .expected_leading_coefficient = "nan*",
          .expected_constant_term = "nan"},
         {.test_name = "fp_roundtrip",
          .x = kRoundTripTestNumber,
          .is_first = false,
          .expected_leading_coefficient =
              absl::StrCat(" + ", kRoundTripTestNumberStr, "*"),
          .expected_constant_term = absl::StrCat(" + ",
                                                 kRoundTripTestNumberStr)},
         {.test_name = "fp_roundtrip_true",
          .x = kRoundTripTestNumber,
          .is_first = true,
          .expected_leading_coefficient = absl::StrCat(kRoundTripTestNumberStr,
                                                       "*"),
          .expected_constant_term = std::string(kRoundTripTestNumberStr)}}),
    [](const testing::TestParamInfo<FormattersTest::ParamType>& info) {
      return info.param.test_name;
    });

}  // namespace

}  // namespace operations_research::math_opt
