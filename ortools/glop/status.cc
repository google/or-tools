// Copyright 2010-2014 Google
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

#include "ortools/glop/status.h"

#include <utility>

#include "ortools/base/logging.h"

namespace operations_research {
namespace glop {

Status::Status() : error_code_(NO_ERROR), error_message_() {}

Status::Status(ErrorCode error_code, std::string error_message)
    : error_code_(error_code),
      error_message_(error_code == NO_ERROR ? "" : std::move(error_message)) {}

std::string GetErrorCodeString(Status::ErrorCode error_code) {
  switch (error_code) {
    case Status::NO_ERROR:
      return "NO_ERROR";
    case Status::ERROR_LU:
      return "ERROR_LU";
    case Status::ERROR_BOUND:
      return "ERROR_BOUND";
    case Status::ERROR_NULL:
      return "ERROR_NULL";
    case Status::ERROR_INVALID_PROBLEM:
      return "INVALID_PROBLEM";
  }
  // Fallback. We don't use "default:" so the compiler will return an error
  // if we forgot one enum case above.
  LOG(DFATAL) << "Invalid Status::ErrorCode " << error_code;
  return "UNKNOWN Status::ErrorCode";
}

}  // namespace glop
}  // namespace operations_research
