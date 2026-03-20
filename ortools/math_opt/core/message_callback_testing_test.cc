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

#include "ortools/math_opt/core/message_callback_testing.h"

#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/core/base_solver.h"

namespace operations_research::math_opt {
namespace {

using ::testing::ElementsAre;

TEST(TestingMessageCallbackTest, Empty) {
  std::vector<std::vector<std::string>> calls;

  const BaseSolver::MessageCallback message_callback =
      TestingMessageCallback(&calls);
  message_callback({"first", "second"});
  message_callback({"third"});

  EXPECT_THAT(
      calls, ElementsAre(ElementsAre("first", "second"), ElementsAre("third")));
}

TEST(TestingMessageCallbackTest, NonEmpty) {
  std::vector<std::vector<std::string>> calls = {
      {"first"},
      {"second"},
  };

  const BaseSolver::MessageCallback message_callback =
      TestingMessageCallback(&calls);
  message_callback({"third"});

  EXPECT_THAT(calls, ElementsAre(ElementsAre("first"), ElementsAre("second"),
                                 ElementsAre("third")));
}

}  // namespace
}  // namespace operations_research::math_opt
