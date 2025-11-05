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

// Typed tests for enums that use enums.h.
#ifndef ORTOOLS_MATH_OPT_CPP_ENUMS_TESTING_H_
#define ORTOOLS_MATH_OPT_CPP_ENUMS_TESTING_H_

#include <limits>
#include <optional>
#include <type_traits>
#include <vector>

#include "absl/numeric/int128.h"
#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/logging.h"
#include "ortools/math_opt/cpp/enums.h"

namespace operations_research::math_opt {

// A type parameterized test suite for testing the correct implementation of
// Enum<E> for a given enum.
//
// Usage:
//
//  INSTANTIATE_TYPED_TEST_SUITE_P(<E>, EnumTest, <E>);
//
template <typename E>
class EnumTest : public testing::Test {
 public:
};

TYPED_TEST_SUITE_P(EnumTest);

TYPED_TEST_P(EnumTest, UnderlyingTypeFits) {
  // We could static_assert that but using a gUnit test works too.
  using underlying_type_limits =
      std::numeric_limits<std::underlying_type_t<TypeParam>>;
  using Proto = typename Enum<TypeParam>::Proto;
  CHECK_GE(EnumProto<Proto>::kMin, underlying_type_limits::min());
  CHECK_LE(EnumProto<Proto>::kMax, underlying_type_limits::max());
}

TYPED_TEST_P(EnumTest, AllProtoValues) {
  using Proto = typename Enum<TypeParam>::Proto;

  bool found_unspecified = false;
  std::vector<TypeParam> found_values;
  for (int i = EnumProto<Proto>::kMin; i <= EnumProto<Proto>::kMax; ++i) {
    if (!EnumProto<Proto>::kIsValid(i)) {
      continue;
    }

    const auto proto_value = static_cast<Proto>(i);
    SCOPED_TRACE(proto_value);

    const std::optional<TypeParam> cpp_enum = EnumFromProto(proto_value);
    if (proto_value == Enum<TypeParam>::kProtoUnspecifiedValue) {
      found_unspecified = true;
      ASSERT_FALSE(cpp_enum.has_value());
    } else {
      ASSERT_TRUE(cpp_enum.has_value());
      found_values.push_back(*cpp_enum);
    }

    const Proto reconverted_proto_value = EnumToProto(cpp_enum);
    EXPECT_EQ(proto_value, reconverted_proto_value);
  }

  // We should have found all C++ enum + nullopt when traversing all possible
  // Proto enums. And the AllValues() of C++ should not contain any additional
  // value.
  ASSERT_TRUE(found_unspecified);
  ASSERT_THAT(found_values, ::testing::UnorderedElementsAreArray(
                                Enum<TypeParam>::AllValues()));
}

TYPED_TEST_P(EnumTest, AllCppValuesString) {
  for (const TypeParam cpp_value : Enum<TypeParam>::AllValues()) {
    const auto cpp_underlying_value =
        static_cast<std::underlying_type_t<TypeParam>>(cpp_value);
    const std::optional<absl::string_view> opt_str_value =
        EnumToOptString(cpp_value);
    ASSERT_TRUE(opt_str_value.has_value())
        << "cpp_value: " << cpp_underlying_value;
    EXPECT_THAT(EnumFromString<TypeParam>(*opt_str_value),
                ::testing::Optional(cpp_value))
        << "cpp_value: " << cpp_underlying_value;
  }
}

REGISTER_TYPED_TEST_SUITE_P(EnumTest, UnderlyingTypeFits, AllProtoValues,
                            AllCppValuesString);

}  // namespace operations_research::math_opt

#endif  // ORTOOLS_MATH_OPT_CPP_ENUMS_TESTING_H_
