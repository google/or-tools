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

#include "ortools/base/strtoint.h"

#include <limits>
#include <string>

#include "gtest/gtest.h"
#include "ortools/base/types.h"

namespace operations_research {

TEST(StrutilTest, StrtoFunctions) {
  // 64-bit conversions are pass-through on all current platforms
  EXPECT_EQ(0, strtoint64("0"));

  EXPECT_EQ(std::numeric_limits<int64_t>::max(),
            strtoint64("9223372036854775807"));

  EXPECT_EQ(std::numeric_limits<int64_t>::min(),
            strtoint64("-9223372036854775808"));

  // safe signed 32-bit conversions within 32-bit range
  EXPECT_EQ(0, strtoint32("0"));

  EXPECT_EQ(std::numeric_limits<int32_t>::max(), strtoint32("2147483647"));

  EXPECT_EQ(std::numeric_limits<int32_t>::min(), strtoint32("-2147483648"));
}

TEST(StrutilTest, AtoiFunctions) {
  // basic atoi32/64 functions, including checks for overflow equivalency
  // even in invalid conversions
  EXPECT_EQ(0, atoi64("0"));
  EXPECT_EQ(12345, atoi64("12345"));
  EXPECT_EQ(-12345, atoi64("-12345"));
  EXPECT_EQ(std::numeric_limits<int64_t>::max(), atoi64("9223372036854775807"));
  EXPECT_EQ(std::numeric_limits<int64_t>::min(),
            atoi64("-9223372036854775808"));

  EXPECT_EQ(0, atoi32("0"));
  EXPECT_EQ(12345, atoi32("12345"));
  EXPECT_EQ(-12345, atoi32("-12345"));
  EXPECT_EQ(std::numeric_limits<int32_t>::max(), atoi32("2147483647"));
  EXPECT_EQ(std::numeric_limits<int32_t>::min(), atoi32("-2147483648"));
}

}  // namespace operations_research
