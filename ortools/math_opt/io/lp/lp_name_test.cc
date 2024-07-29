// Copyright 2010-2024 Google LLC
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

#include "ortools/math_opt/io/lp/lp_name.h"

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"

namespace operations_research::lp_format {
namespace {

using ::testing::AllOf;
using ::testing::HasSubstr;
using ::testing::status::StatusIs;

TEST(ValidateCharInNameTest, BasicUse) {
  for (bool is_leading : {false, true}) {
    SCOPED_TRACE(absl::StrCat("is leading: ", is_leading));
    for (unsigned char c : {'a', 'A', 'b', 'B', 'z', 'Z', '_', '{', '}'}) {
      SCOPED_TRACE(absl::StrCat("testing character: ", c));
      EXPECT_TRUE(ValidateCharInName(c, is_leading));
    }
    for (unsigned char c : {'+', '-', '*', '/', ':', '\0'}) {
      SCOPED_TRACE(absl::StrCat("testing character: ", c));
      EXPECT_FALSE(ValidateCharInName(c, is_leading));
    }
  }
}

TEST(ValidateCharInNameTest, LeadingChars) {
  for (bool is_leading : {false, true}) {
    SCOPED_TRACE(absl::StrCat("is leading: ", is_leading));
    for (unsigned char c : {'.', '0', '1', '9'}) {
      SCOPED_TRACE(absl::StrCat("testing character: ", c));
      const bool should_be_allowed = !is_leading;
      EXPECT_EQ(ValidateCharInName(c, is_leading), should_be_allowed);
    }
  }
}

TEST(ValidateNameTest, BasicUse) {
  EXPECT_OK(ValidateName("x8"));
  EXPECT_OK(ValidateName("A_b_C"));
  EXPECT_THAT(ValidateName(""),
              StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("empty")));
  EXPECT_THAT(ValidateName("8x"), StatusIs(absl::StatusCode::kInvalidArgument,
                                           AllOf(HasSubstr("index: 0"),
                                                 HasSubstr("character: 8"))));
  EXPECT_THAT(ValidateName("x-8"), StatusIs(absl::StatusCode::kInvalidArgument,
                                            AllOf(HasSubstr("index: 1"),
                                                  HasSubstr("character: -"))));
}

}  // namespace
}  // namespace operations_research::lp_format
