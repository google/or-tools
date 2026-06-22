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

#include "ortools/math_opt/testing/stream.h"

#include <ostream>

#include "gtest/gtest.h"

namespace operations_research::math_opt {
namespace {

struct TestStruct {
  int integer = 0;
};

std::ostream& operator<<(std::ostream& ostr, const TestStruct& value) {
  ostr << "{integer: " << value.integer << "}";
  return ostr;
}

TEST(StreamToStringTest, StreamToString) {
  EXPECT_EQ(StreamToString(TestStruct{.integer = 1}), "{integer: 1}");
}

}  // namespace
}  // namespace operations_research::math_opt
