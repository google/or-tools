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

#include "ortools/math_opt/core/concurrent_calls_guard.h"

#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/status_macros.h"

namespace operations_research::math_opt {
namespace {

using ::testing::HasSubstr;
using ::testing::status::StatusIs;

const absl::string_view kConcurrentCallsErrorSubstring = "concurrent calls";

TEST(ConcurrentCallsGuard, ConcurrentCalls) {
  ConcurrentCallsGuard::Tracker tracker;
  {  // Limit the scope of `guard`.
    ASSERT_OK_AND_ASSIGN(const ConcurrentCallsGuard guard,
                         ConcurrentCallsGuard::TryAcquire(tracker));

    // Make two "concurrent calls" (from the point of view of
    // ConcurrentCallsGuard, we would be in another call because there exists a
    // ConcurrentCallsGuard) and test that it fails.
    ASSERT_THAT(ConcurrentCallsGuard::TryAcquire(tracker),
                StatusIs(absl::StatusCode::kInvalidArgument,
                         HasSubstr(kConcurrentCallsErrorSubstring)));
    ASSERT_THAT(ConcurrentCallsGuard::TryAcquire(tracker),
                StatusIs(absl::StatusCode::kInvalidArgument,
                         HasSubstr(kConcurrentCallsErrorSubstring)));
  }

  // After the terminated the previous call (by destroying the
  // ConcurrentCallsGuard at the end of the scope) we should be able to make
  // another call.
  ASSERT_OK(ConcurrentCallsGuard::TryAcquire(tracker));
}

TEST(ConcurrentCallsGuard, MoveConstructor) {
  ConcurrentCallsGuard::Tracker tracker;
  {  // Limit the scope of guard_from_move.
    ASSERT_OK_AND_ASSIGN(ConcurrentCallsGuard guard_from_move,
                         [&]() -> absl::StatusOr<ConcurrentCallsGuard> {
                           ASSIGN_OR_RETURN(
                               ConcurrentCallsGuard guard,
                               ConcurrentCallsGuard::TryAcquire(tracker));
                           return ConcurrentCallsGuard(std::move(guard));
                           // Exiting this scope will destroy the guard which
                           // will have been moved out (there is no copy
                           // constructor). This will test that the code of
                           // ~ConcurrentCallsGuard() properly deals with
                           // moved-out instances.
                         }());

    // The ConcurrentCallsGuard created from the move should prevent any
    // "concurrent call".
    ASSERT_THAT(ConcurrentCallsGuard::TryAcquire(tracker),
                StatusIs(absl::StatusCode::kInvalidArgument,
                         HasSubstr(kConcurrentCallsErrorSubstring)));
  }

  // After the terminated the previous call (by destroying the
  // ConcurrentCallsGuard at the end of the scope) we should be able to make
  // another call.
  ASSERT_OK(ConcurrentCallsGuard::TryAcquire(tracker));
}

}  // namespace
}  // namespace operations_research::math_opt
