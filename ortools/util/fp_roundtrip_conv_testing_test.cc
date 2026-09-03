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

#include "ortools/util/fp_roundtrip_conv_testing.h"

#include <sstream>

#include "gtest/gtest.h"
#include "ortools/util/fp_roundtrip_conv.h"

namespace operations_research {
namespace {

TEST(TestNumberTest, Test) {
  {
    std::ostringstream oss;
    oss << kRoundTripTestNumber;
    EXPECT_NE(oss.str(), kRoundTripTestNumberStr);
    EXPECT_EQ(oss.str(), "0.1");
  }
  {
    std::ostringstream oss;
    oss << RoundTripDoubleFormat(kRoundTripTestNumber);
    EXPECT_EQ(oss.str(), kRoundTripTestNumberStr);
  }
}

}  // namespace
}  // namespace operations_research
