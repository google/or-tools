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

#ifndef ORTOOLS_BASE_STATUS_BUILDER_H_
#define ORTOOLS_BASE_STATUS_BUILDER_H_

#include "absl/status/status.h"
#include "absl/status/status_builder.h"

namespace ortools {

inline absl::StatusBuilder AbortedErrorBuilder() {
  return absl::StatusBuilder(absl::StatusCode::kAborted);
}

inline absl::StatusBuilder AlreadyExistsErrorBuilder() {
  return absl::StatusBuilder(absl::StatusCode::kAlreadyExists);
}

inline absl::StatusBuilder CancelledErrorBuilder() {
  return absl::StatusBuilder(absl::StatusCode::kCancelled);
}

inline absl::StatusBuilder DataLossErrorBuilder() {
  return absl::StatusBuilder(absl::StatusCode::kDataLoss);
}

inline absl::StatusBuilder DeadlineExceededErrorBuilder() {
  return absl::StatusBuilder(absl::StatusCode::kDeadlineExceeded);
}

inline absl::StatusBuilder FailedPreconditionErrorBuilder() {
  return absl::StatusBuilder(absl::StatusCode::kFailedPrecondition);
}

inline absl::StatusBuilder InternalErrorBuilder() {
  return absl::StatusBuilder(absl::StatusCode::kInternal);
}

inline absl::StatusBuilder InvalidArgumentErrorBuilder() {
  return absl::StatusBuilder(absl::StatusCode::kInvalidArgument);
}

inline absl::StatusBuilder NotFoundErrorBuilder() {
  return absl::StatusBuilder(absl::StatusCode::kNotFound);
}

inline absl::StatusBuilder OutOfRangeErrorBuilder() {
  return absl::StatusBuilder(absl::StatusCode::kOutOfRange);
}

inline absl::StatusBuilder PermissionDeniedErrorBuilder() {
  return absl::StatusBuilder(absl::StatusCode::kPermissionDenied);
}

inline absl::StatusBuilder UnauthenticatedErrorBuilder() {
  return absl::StatusBuilder(absl::StatusCode::kUnauthenticated);
}

inline absl::StatusBuilder ResourceExhaustedErrorBuilder() {
  return absl::StatusBuilder(absl::StatusCode::kResourceExhausted);
}

inline absl::StatusBuilder UnavailableErrorBuilder() {
  return absl::StatusBuilder(absl::StatusCode::kUnavailable);
}

inline absl::StatusBuilder UnimplementedErrorBuilder() {
  return absl::StatusBuilder(absl::StatusCode::kUnimplemented);
}

inline absl::StatusBuilder UnknownErrorBuilder() {
  return absl::StatusBuilder(absl::StatusCode::kUnknown);
}

}  // namespace ortools

#endif  // ORTOOLS_BASE_STATUS_BUILDER_H_
