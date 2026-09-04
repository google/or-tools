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

#include "ortools/port/proto_utils.h"

#include <algorithm>
#include <string>

#include "absl/algorithm/container.h"
#include "absl/log/log.h"
#include "absl/strings/escaping.h"
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"
#include "gtest/gtest.h"
#include "ortools/base/dump_vars.h"
#include "ortools/base/gmock.h"
#include "ortools/base/macros/os_support.h"
#include "ortools/base/types.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/util/test.pb.h"

namespace operations_research {
namespace {

using ::testing::AllOf;
using ::testing::EndsWith;
using ::testing::EqualsProto;
using ::testing::HasSubstr;
using ::testing::StartsWith;

TEST(ProtobufDebugStringTest, EmptyProto) {
  EXPECT_EQ(ProtobufDebugString(TestProto()),
            "# [operations_research.TestProto of 0 bytes]\n");
}

TEST(ProtobufDebugStringTest, TextFormatWhenFittingLimits) {
  TestProto proto;
  proto.set_bool_type(true);
  proto.set_int32_type(123);
  EXPECT_EQ(ProtobufDebugString(proto),
            "# [operations_research.TestProto of 4 bytes]\nbool_type: "
            "true\nint32_type: 123\n");
}

TEST(ProtobufDebugStringTest,
     BinaryFormatWhenTextFormatExceedsMaxLinesOrMaxChars) {
  TestProto proto;
  for (int i = 0; i < 10; ++i) {
    proto.add_repeated_int64_type(1'000'000'000'000'000'000 + i);
  }

  // The text format takes ≥10 lines. With max_lines = 9, it should fallback to
  // the binary base64.
  const std::string debug_str =
      ProtobufDebugString(proto, /*max_chars=*/1000, /*max_lines=*/9);
  constexpr absl::string_view kExpectedPrefix =
      "[operations_research.TestProto of 110 bytes] ";
  EXPECT_THAT(debug_str, StartsWith(kExpectedPrefix));
  absl::string_view b64_payload = debug_str;
  ASSERT_TRUE(absl::ConsumePrefix(&b64_payload, kExpectedPrefix));
  std::string decoded_serialized;
  ASSERT_TRUE(absl::Base64Unescape(b64_payload, &decoded_serialized));
  TestProto deserialized;
  ASSERT_TRUE(deserialized.ParseFromString(decoded_serialized));
  EXPECT_THAT(deserialized, EqualsProto(proto));

  // With max_chars the limiting factor, it should also fall back to binary.
  EXPECT_EQ(ProtobufDebugString(proto, /*max_chars=*/400), debug_str);
}

TEST(ProtobufDebugStringTest, GenericFallbackWhenBinaryExceedsMaxChars) {
  TestProto proto;
  proto.set_string_type("a_somewhat_longer_string_for_testing");

  // With max_chars too small for the binary, it should only print the header.
  EXPECT_EQ(ProtobufDebugString(proto, /*max_chars=*/30),
            absl::StrFormat("[operations_research.TestProto of %d bytes]",
                            proto.ByteSizeLong()));
}

TEST(ProtobufDebugStringTest, NegativeLimits) {
  TestProto proto;
  proto.set_int32_type(42);

  EXPECT_EQ(ProtobufDebugString(proto, /*max_chars=*/-1, /*max_lines=*/-1),
            absl::StrFormat("[operations_research.TestProto of %d bytes]",
                            proto.ByteSizeLong()));
}

#if defined(ORTOOLS_TARGET_OS_SUPPORTS_PROTO_DESCRIPTOR)
static_assert(operations_research::kTargetOsSupportsProtoDescriptor);

TEST(ProtobufTextFormatPrintToStringForFlagTest, OneLine) {
  MPModelProto proto;
  proto.set_name("some name");
  proto.set_objective_offset(2.0);
  EXPECT_EQ(ProtobufTextFormatPrintToStringForFlag(proto),
            "objective_offset: 2 name: \"some name\"");
}

#else
static_assert(!operations_research::kTargetOsSupportsProtoDescriptor);
// Note(user): we are intentionally testing this on the enum
// MPModelRequest::SolverType, because the values of the enums are
// nonconsecutive.
TEST(ProtoEnumToStringTest, GoodValues) {
  EXPECT_EQ(ProtoEnumToString<MPModelRequest::SolverType>(
                MPModelRequest::CLP_LINEAR_PROGRAMMING),
            "CLP_LINEAR_PROGRAMMING");
  EXPECT_EQ(ProtoEnumToString<MPModelRequest::SolverType>(
                MPModelRequest::GLOP_LINEAR_PROGRAMMING),
            "GLOP_LINEAR_PROGRAMMING");
  EXPECT_EQ(ProtoEnumToString<MPModelRequest::SolverType>(
                MPModelRequest::GLIP_MIXED_INTEGER_PROGRAMMING),
            "GLIP_MIXED_INTEGER_PROGRAMMING");
  EXPECT_EQ(ProtoEnumToString<MPModelRequest::SolverType>(
                MPModelRequest::SAT_INTEGER_PROGRAMMING),
            "SAT_INTEGER_PROGRAMMING");
}

TEST(ProtoEnumToStringTest, BadValues) {
  EXPECT_EQ(ProtoEnumToString<MPModelRequest::SolverType>(
                static_cast<MPModelRequest::SolverType>(1000)),
            "Invalid enum value of: 1000 for enum type: SolverType");
}

TEST(ProtobufParseTextProtoForFlagTest, ParseEmpty) {
  MPModelProto result;
  std::string error;
  EXPECT_TRUE(ProtobufParseTextProtoForFlag("", &result, &error));
  // Check that result is empty
  EXPECT_EQ(result.ByteSizeLong(), 0);
  EXPECT_EQ(error, std::string{});
}

TEST(ProtobufParseTextProtoForFlagTest, ParseNonEmptyFails) {
  MPModelProto result;
  std::string error;
  EXPECT_FALSE(
      ProtobufParseTextProtoForFlag("objective_offset: 2.0", &result, &error));
  EXPECT_THAT(error,
              testing::HasSubstr("cannot parse text protos on this platform"));
}

TEST(ProtobufTextFormatPrintToStringForFlagTest, SimpleProto) {
  MPModelProto proto;
  proto.set_objective_offset(2.0);

  EXPECT_THAT(ProtobufTextFormatPrintToStringForFlag(proto),
              testing::AllOf(testing::HasSubstr("MPModelProto"),
                             testing::HasSubstr("not supported")));
}
#endif  // defined(ORTOOLS_TARGET_OS_SUPPORTS_PROTO_DESCRIPTOR)

}  // namespace
}  // namespace operations_research
