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

#include "ortools/math_opt/cpp/remote_streaming_mode.h"

#include <sstream>
#include <string>

#include "absl/flags/marshalling.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"

namespace operations_research::math_opt {
namespace {

using ::testing::HasSubstr;

// Tests that the AbslParseFlag() and AbslUnparseFlag() functions properly
// roundtrips.
class RemoteStreamingSolveModeRoundtripTest
    : public testing::TestWithParam<RemoteStreamingSolveMode> {};

TEST_P(RemoteStreamingSolveModeRoundtripTest, Roundtrip) {
  RemoteStreamingSolveMode output = RemoteStreamingSolveMode::kDefault;
  std::string error;
  EXPECT_TRUE(absl::ParseFlag(absl::UnparseFlag(GetParam()), &output, &error))
      << "error: '" << absl::CEscape(error) << "'";
  EXPECT_EQ(output, GetParam());
}

INSTANTIATE_TEST_SUITE_P(
    All, RemoteStreamingSolveModeRoundtripTest,
    testing::Values(RemoteStreamingSolveMode::kDefault,
                    RemoteStreamingSolveMode::kSubprocess,
                    RemoteStreamingSolveMode::kInProcess),
    [](const testing::TestParamInfo<
        RemoteStreamingSolveModeRoundtripTest::ParamType>& info) {
      return AbslUnparseFlag(info.param);
    });

TEST(RemoteStreamingSolveModeTest, InvalidValue) {
  RemoteStreamingSolveMode output = RemoteStreamingSolveMode::kDefault;
  std::string error;
  EXPECT_FALSE(absl::ParseFlag("unknown", &output, &error))
      << "output: " << output;
  EXPECT_THAT(error, HasSubstr("unknown value"));
}

TEST(RemoteStreamingSolveModeTest, PrintToStdStream) {
  std::ostringstream oss;
  oss << RemoteStreamingSolveMode::kDefault;
  EXPECT_EQ(oss.str(), "default");
}

TEST(RemoteStreamingSolveModeTest, PrintToAbsl) {
  EXPECT_EQ(absl::StrCat(RemoteStreamingSolveMode::kDefault), "default");
}

}  // namespace
}  // namespace operations_research::math_opt
