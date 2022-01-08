// Copyright 2010-2021 Google LLC
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

#ifndef OR_TOOLS_BASE_STATUS_BUILDER_H_
#define OR_TOOLS_BASE_STATUS_BUILDER_H_

#include <sstream>
#include <string>

#include "absl/status/status.h"

namespace util {

class StatusBuilder {
 public:
  explicit StatusBuilder(const absl::StatusCode code) : code_(code) {}

  explicit StatusBuilder(const absl::Status& status) : code_(status.code()) {
    ss_ << std::string(status.message());
  }

  operator absl::Status() const {  // NOLINT
    return absl::Status(code_, ss_.str());
  }

  template <class T>
  StatusBuilder& operator<<(const T& t) {
    ss_ << t;
    return *this;
  }

  StatusBuilder& SetAppend() { return *this; }

 private:
  const absl::StatusCode code_;
  std::ostringstream ss_;
};

inline StatusBuilder AbortedErrorBuilder() {
  return StatusBuilder(absl::StatusCode::kAborted);
}

inline StatusBuilder AlreadyExistsErrorBuilder() {
  return StatusBuilder(absl::StatusCode::kAlreadyExists);
}

inline StatusBuilder CancelledErrorBuilder() {
  return StatusBuilder(absl::StatusCode::kCancelled);
}

inline StatusBuilder DataLossErrorBuilder() {
  return StatusBuilder(absl::StatusCode::kDataLoss);
}

inline StatusBuilder DeadlineExceededErrorBuilder() {
  return StatusBuilder(absl::StatusCode::kDeadlineExceeded);
}

inline StatusBuilder FailedPreconditionErrorBuilder() {
  return StatusBuilder(absl::StatusCode::kFailedPrecondition);
}

inline StatusBuilder InternalErrorBuilder() {
  return StatusBuilder(absl::StatusCode::kInternal);
}

inline StatusBuilder InvalidArgumentErrorBuilder() {
  return StatusBuilder(absl::StatusCode::kInvalidArgument);
}

inline StatusBuilder NotFoundErrorBuilder() {
  return StatusBuilder(absl::StatusCode::kNotFound);
}

inline StatusBuilder OutOfRangeErrorBuilder() {
  return StatusBuilder(absl::StatusCode::kOutOfRange);
}

inline StatusBuilder PermissionDeniedErrorBuilder() {
  return StatusBuilder(absl::StatusCode::kPermissionDenied);
}

inline StatusBuilder UnauthenticatedErrorBuilder() {
  return StatusBuilder(absl::StatusCode::kUnauthenticated);
}

inline StatusBuilder ResourceExhaustedErrorBuilder() {
  return StatusBuilder(absl::StatusCode::kResourceExhausted);
}

inline StatusBuilder UnavailableErrorBuilder() {
  return StatusBuilder(absl::StatusCode::kUnavailable);
}

inline StatusBuilder UnimplementedErrorBuilder() {
  return StatusBuilder(absl::StatusCode::kUnimplemented);
}

inline StatusBuilder UnknownErrorBuilder() {
  return StatusBuilder(absl::StatusCode::kUnknown);
}

}  // namespace util

#endif  // OR_TOOLS_BASE_STATUS_BUILDER_H_
