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

#include "ortools/base/status_builder.h"

#include <string>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"

namespace ortools {
namespace {

using ::testing::status::StatusIs;

TEST(StatusBuilderTest, Factories) {
  constexpr char kMsg[] = "message";
  EXPECT_THAT(absl::Status(AbortedErrorBuilder() << kMsg),
              StatusIs(absl::StatusCode::kAborted, kMsg));
  EXPECT_THAT(absl::Status(AlreadyExistsErrorBuilder() << kMsg),
              StatusIs(absl::StatusCode::kAlreadyExists, kMsg));
  EXPECT_THAT(absl::Status(CancelledErrorBuilder() << kMsg),
              StatusIs(absl::StatusCode::kCancelled, kMsg));
  EXPECT_THAT(absl::Status(DataLossErrorBuilder() << kMsg),
              StatusIs(absl::StatusCode::kDataLoss, kMsg));
  EXPECT_THAT(absl::Status(DeadlineExceededErrorBuilder() << kMsg),
              StatusIs(absl::StatusCode::kDeadlineExceeded, kMsg));
  EXPECT_THAT(absl::Status(FailedPreconditionErrorBuilder() << kMsg),
              StatusIs(absl::StatusCode::kFailedPrecondition, kMsg));
  EXPECT_THAT(absl::Status(InternalErrorBuilder() << kMsg),
              StatusIs(absl::StatusCode::kInternal, kMsg));
  EXPECT_THAT(absl::Status(InvalidArgumentErrorBuilder() << kMsg),
              StatusIs(absl::StatusCode::kInvalidArgument, kMsg));
  EXPECT_THAT(absl::Status(NotFoundErrorBuilder() << kMsg),
              StatusIs(absl::StatusCode::kNotFound, kMsg));
  EXPECT_THAT(absl::Status(OutOfRangeErrorBuilder() << kMsg),
              StatusIs(absl::StatusCode::kOutOfRange, kMsg));
  EXPECT_THAT(absl::Status(PermissionDeniedErrorBuilder() << kMsg),
              StatusIs(absl::StatusCode::kPermissionDenied, kMsg));
  EXPECT_THAT(absl::Status(UnauthenticatedErrorBuilder() << kMsg),
              StatusIs(absl::StatusCode::kUnauthenticated, kMsg));
  EXPECT_THAT(absl::Status(ResourceExhaustedErrorBuilder() << kMsg),
              StatusIs(absl::StatusCode::kResourceExhausted, kMsg));
  EXPECT_THAT(absl::Status(UnavailableErrorBuilder() << kMsg),
              StatusIs(absl::StatusCode::kUnavailable, kMsg));
  EXPECT_THAT(absl::Status(UnimplementedErrorBuilder() << kMsg),
              StatusIs(absl::StatusCode::kUnimplemented, kMsg));
  EXPECT_THAT(absl::Status(UnknownErrorBuilder() << kMsg),
              StatusIs(absl::StatusCode::kUnknown, kMsg));
}

}  // namespace
}  // namespace ortools
