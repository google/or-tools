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

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/macros/os_support.h"
#include "ortools/linear_solver/linear_solver.pb.h"

namespace operations_research {
namespace {

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
