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

#include "ortools/util/status_macros.h"

#include <utility>

#include "absl/status/status.h"
#include "absl/status/status_macros.h"
#include "absl/status/statusor.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"

namespace operations_research {
namespace {

using ::testing::HasSubstr;
using ::testing::Pair;
using ::testing::status::IsOkAndHolds;
using ::testing::status::StatusIs;

TEST(OrAssignOrReturn3Test, CostlyExpression) {
  // We use a function in the test to validate that OR_ASSIGN_OR_RETURN3 does
  // not evaluate the third parameters unless the status is failing.
  int costly_function_calls = 0;
  const auto costly_function = [&]() {
    ++costly_function_calls;
    return "costly result";
  };

  const auto macro_invoker =
      [&](const absl::StatusOr<std::pair<int, float>> input)
      -> absl::StatusOr<std::pair<int, float>> {
    // Here we also test that we can use destructured assignment, even if we
    // want a pair in the end.
    OR_ASSIGN_OR_RETURN3((const auto [x, y]), input, _ << costly_function());
    return std::make_pair(x, y);
  };

  EXPECT_THAT(macro_invoker(std::make_pair(3, -1.5)),
              IsOkAndHolds(Pair(3, -1.5)));
  EXPECT_EQ(costly_function_calls, 0);

  EXPECT_THAT(macro_invoker(absl::InternalError("oops")),
              StatusIs(absl::StatusCode::kInternal,
                       AllOf(HasSubstr("oops"), HasSubstr("costly result"))));
  EXPECT_EQ(costly_function_calls, 1);
}

}  // namespace
}  // namespace operations_research
