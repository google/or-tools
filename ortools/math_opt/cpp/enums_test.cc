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

#include "ortools/math_opt/cpp/enums.h"

#include <optional>
#include <sstream>
#include <string>
#include <type_traits>

#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/cpp/enums_test.pb.h"
#include "ortools/math_opt/cpp/enums_testing.h"
#include "ortools/math_opt/testing/stream.h"

namespace operations_research::math_opt {
namespace {

enum class TestEnum {
  kFirstValue = TEST_ENUM_FIRST_VALUE,
  kSecondValue = TEST_ENUM_SECOND_VALUE
};

}  // namespace

// Template specialization must be in the same namespace. It can't be in the
// anonymous namespace.
MATH_OPT_DEFINE_ENUM(TestEnum, TEST_ENUM_UNSPECIFIED);

std::optional<absl::string_view> Enum<TestEnum>::ToOptString(TestEnum value) {
  switch (value) {
    case TestEnum::kFirstValue:
      return "first_value";
    case TestEnum::kSecondValue:
      return "second_value";
  }
  return std::nullopt;
}

absl::Span<const TestEnum> Enum<TestEnum>::AllValues() {
  static constexpr TestEnum kTestEnumValues[] = {TestEnum::kFirstValue,
                                                 TestEnum::kSecondValue};

  return absl::MakeConstSpan(kTestEnumValues);
}

namespace {

using ::testing::ElementsAre;
using ::testing::Optional;

TEST(EnumsTest, EnumToProtoWithOptional) {
  std::optional<TestEnum> opt_value = TestEnum::kFirstValue;
  EXPECT_EQ(EnumToProto(opt_value), TEST_ENUM_FIRST_VALUE);
  EXPECT_EQ(EnumToProto<TestEnum>(std::nullopt), TEST_ENUM_UNSPECIFIED);
}

TEST(EnumsTest, EnumToProtoWithValue) {
  EXPECT_EQ(EnumToProto(TestEnum::kFirstValue), TEST_ENUM_FIRST_VALUE);
  EXPECT_EQ(EnumToProto(TestEnum::kSecondValue), TEST_ENUM_SECOND_VALUE);
}

TEST(EnumsTest, FromProto) {
  EXPECT_THAT(EnumFromProto(TEST_ENUM_FIRST_VALUE),
              Optional(TestEnum::kFirstValue));
  EXPECT_THAT(EnumFromProto(TEST_ENUM_SECOND_VALUE),
              Optional(TestEnum::kSecondValue));
  EXPECT_EQ(EnumFromProto(TEST_ENUM_UNSPECIFIED), std::nullopt);
}

TEST(EnumsTest, EnumToOptString) {
  EXPECT_THAT(EnumToOptString(TestEnum::kFirstValue),
              Optional(absl::string_view("first_value")));
  EXPECT_THAT(EnumToOptString(TestEnum::kSecondValue),
              Optional(absl::string_view("second_value")));
  EXPECT_EQ(EnumToOptString(static_cast<TestEnum>(-15)), std::nullopt);
}

TEST(EnumsTest, EnumToString) {
  EXPECT_THAT(EnumToString(TestEnum::kFirstValue), "first_value");
  EXPECT_THAT(EnumToString(TestEnum::kSecondValue), "second_value");
}

TEST(EnumsDeathTest, EnumToString) {
  EXPECT_DEATH_IF_SUPPORTED(EnumToString(static_cast<TestEnum>(-15)),
                            "invalid value: -15");
}

TEST(EnumsTest, EnumFromString) {
  EXPECT_THAT(EnumFromString<TestEnum>("first_value"),
              Optional(TestEnum::kFirstValue));
  EXPECT_THAT(EnumFromString<TestEnum>("second_value"),
              Optional(TestEnum::kSecondValue));
  EXPECT_EQ(EnumFromString<TestEnum>("unknown"), std::nullopt);
}

TEST(EnumsTest, AllValues) {
  EXPECT_THAT(Enum<TestEnum>::AllValues(),
              ElementsAre(TestEnum::kFirstValue, TestEnum::kSecondValue));
}

TEST(EnumsTest, Stream) {
  EXPECT_EQ(StreamToString(TestEnum::kFirstValue), "first_value");
  EXPECT_EQ(StreamToString(static_cast<TestEnum>(-15)), "<invalid enum (-15)>");
}

TEST(EnumsTest, OptStream) {
  EXPECT_EQ(StreamToString(std::make_optional(TestEnum::kFirstValue)),
            "first_value");
  EXPECT_EQ(StreamToString(std::make_optional(static_cast<TestEnum>(-15))),
            "<invalid enum (-15)>");
  EXPECT_EQ(StreamToString(std::optional<TestEnum>()), "<unspecified>");
}

INSTANTIATE_TYPED_TEST_SUITE_P(TestEnum, EnumTest, TestEnum);

}  // namespace
}  // namespace operations_research::math_opt
