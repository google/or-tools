// Copyright 2010-2018 Google LLC
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

#ifndef OR_TOOLS_BASE_CANONICAL_ERRORS_H_
#define OR_TOOLS_BASE_CANONICAL_ERRORS_H_

#include "ortools/base/status.h"

namespace util {

inline Status InternalError(const std::string& message) {
  return Status(error::INTERNAL, message);
}

inline Status InvalidArgumentError(const std::string& message) {
  return Status(error::INVALID_ARGUMENT, message);
}

inline Status UnimplementedError(const std::string& message) {
  return Status(error::NOT_IMPLEMENTED, message);
}

}  // namespace util

#endif  // OR_TOOLS_BASE_CANONICAL_ERRORS_H_
