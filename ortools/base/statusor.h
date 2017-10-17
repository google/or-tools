// Copyright 2010-2017 Google
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

#ifndef OR_TOOLS_BASE_STATUSOR_H_
#define OR_TOOLS_BASE_STATUSOR_H_

#include "ortools/base/status.h"

namespace util {

// T should be a pointer type. Eg. StatusOr<Graph*>.
template <class T>
struct StatusOr {
  // Non-explicit constructors, by design.
  StatusOr(T value) : value_(value) {  // NOLINT
    CHECK(value != nullptr);  // This enforces that T is a pointer type.
  }
  StatusOr(const Status& status) : status_(status) {  // NOLINT
    CHECK(!status_.ok()) << status.ToString();
  }

  // Copy constructor.
  StatusOr(const StatusOr& other)
      : value_(other.value_), status_(other.status_) {}

  bool ok() const { return value_ != nullptr; }
  const T& ValueOrDie() const {
    CHECK(ok());
    return value_;
  }

  Status status() const {
    return value_ != nullptr ? /*OK*/ Status() : status_;
  }

 private:
  T value_;
  Status status_;
};

}  // namespace util

#endif  // OR_TOOLS_BASE_STATUSOR_H_
