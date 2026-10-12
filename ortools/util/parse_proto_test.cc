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

#include "ortools/util/parse_proto.h"

#include <string>

#include "absl/strings/str_cat.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/util/status.pb.h"

namespace operations_research {

using ::testing::AllOf;
using ::testing::EquivToProto;
using ::testing::HasSubstr;

TEST(ParseTextProtoForFlagTest, Success) {
  StatusProto result;
  std::string error;
  EXPECT_TRUE(ParseTextProtoForFlag("code: 12", &result, &error));
  StatusProto expected;
  expected.set_code(12);
  EXPECT_THAT(result, EquivToProto(expected));
  EXPECT_EQ(error, std::string{});
}

TEST(ParseTextProtoForFlagTest, Failure) {
  StatusProto result;
  std::string error;
  EXPECT_FALSE(ParseTextProtoForFlag("cat: 12", &result, &error));
  EXPECT_THAT(error, HasSubstr("cat"));
}

TEST(ParseTextProtoForFlagTest, AllFailures) {
  StatusProto result;
  std::string error;
  EXPECT_FALSE(ParseTextProtoForFlag("cat: 12 dog: 4", &result, &error));
  EXPECT_THAT(error, AllOf(HasSubstr("cat"), HasSubstr("dog")));
}

TEST(ParseTextProtoForFlagTest, ErrorInFirstLineAtPosition) {
  StatusProto result;
  std::string error;
  EXPECT_FALSE(ParseTextProtoForFlag("code: 12 cat: 3", &result, &error));
  const std::string error_caret =
      absl::StrCat(std::string(std::string("code: 12 cat").size(), ' '), "^");
  EXPECT_THAT(error, HasSubstr(absl::StrCat("code: 12 cat: 3\n", error_caret)));
}

TEST(ParseTextProtoForFlagTest, ErrorInSecondLineAtPosition) {
  StatusProto result;
  std::string error;
  EXPECT_FALSE(ParseTextProtoForFlag("code: 12\ncat: 3", &result, &error));
  const std::string error_caret =
      absl::StrCat(std::string(std::string("cat").size(), ' '), "^");
  EXPECT_THAT(error, HasSubstr(absl::StrCat("cat: 3\n", error_caret)));
}

}  // namespace operations_research
