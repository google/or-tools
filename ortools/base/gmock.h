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

#ifndef OR_TOOLS_BASE_GMOCK_H_
#define OR_TOOLS_BASE_GMOCK_H_

#include "absl/status/status_matchers.h"
#include "ortools/base/gmock.h"
#include "ortools/base/protocol-buffer-matchers.h"  // IWYU pragma: export

namespace testing::status {
using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
}  // namespace testing::status

// Macros for testing the results of functions that return absl::Status or
// absl::StatusOr<T> (for any type T).
#define EXPECT_OK(expression) EXPECT_THAT(expression, ::testing::status::IsOk())
#define ASSERT_OK(expression) ASSERT_THAT(expression, ::testing::status::IsOk())

#define STATUS_MATCHERS_IMPL_CONCAT_INNER_(x, y) x##y
#define STATUS_MATCHERS_IMPL_CONCAT_(x, y) \
  STATUS_MATCHERS_IMPL_CONCAT_INNER_(x, y)

#undef ASSERT_OK_AND_ASSIGN
#define ASSERT_OK_AND_ASSIGN(lhs, rexpr) \
  ASSERT_OK_AND_ASSIGN_IMPL_(            \
      STATUS_MATCHERS_IMPL_CONCAT_(_status_or_value, __COUNTER__), lhs, rexpr)

#define ASSERT_OK_AND_ASSIGN_IMPL_(statusor, lhs, rexpr) \
  auto statusor = (rexpr);                               \
  ASSERT_TRUE(statusor.ok()) << statusor.status();       \
  lhs = std::move(statusor.value())

#endif  // OR_TOOLS_BASE_GMOCK_H_
