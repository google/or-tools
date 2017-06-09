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

#ifndef OR_TOOLS_GLOP_STATUS_H_
#define OR_TOOLS_GLOP_STATUS_H_

#include <string>
#include "ortools/base/port.h"

namespace operations_research {
namespace glop {

// Return type for the solver functions that return "Did that work?".
// It should only be used for unrecoverable errors.
class Status {
 public:
  // Possible kinds of errors.
  enum ErrorCode {
    // Not an error. Returned on success.
    NO_ERROR = 0,

    // The LU factorization of the current basis couldn't be computed.
    ERROR_LU = 1,

    // The current variable values are out of their bound modulo the tolerance.
    ERROR_BOUND = 2,

    // A pointer argument was NULL when it shouldn't be.
    ERROR_NULL = 3,

    // The linear program is invalid or it does not have the required format.
    ERROR_INVALID_PROBLEM = 4,

  };

  // Creates a "successful" status.
  Status();

  // Creates a status with the specified error code and error message.
  // If "code == 0", error_message is ignored and a Status object identical
  // to Status::OK is constructed.
  Status(ErrorCode error_code, std::string error_message);

  // Improves readability but identical to 0-arg constructor.
  static const Status OK() { return Status(); }

  // Accessors.
  ErrorCode error_code() const { return error_code_; }
  const std::string& error_message() const { return error_message_; }
  bool ok() const { return error_code_ == NO_ERROR; }

 private:
  ErrorCode error_code_;
  std::string error_message_;
};

// Returns the std::string representation of the ErrorCode enum.
std::string GetErrorCodeString(Status::ErrorCode error_code);

// Macro to simplify error propagation between function returning Status.
#define GLOP_RETURN_IF_ERROR(function_call)        \
  do {                                             \
    const Status return_status = function_call;    \
    if (!return_status.ok()) return return_status; \
  } while (false)

// Macro to simplify the creation of an error.
#define GLOP_RETURN_AND_LOG_ERROR(error_code, message)                     \
  do {                                                                     \
    const std::string error_message = message;                                  \
    LOG(ERROR) << GetErrorCodeString(error_code) << ": " << error_message; \
    return Status(error_code, error_message);                              \
  } while (false)

// Macro to check that a pointer argument is not null.
#define GLOP_RETURN_ERROR_IF_NULL(arg)                                 \
  if (arg == nullptr) {                                                \
    const std::string variable_name = #arg;                                 \
    const std::string error_message = variable_name + " must not be null."; \
    LOG(DFATAL) << error_message;                                      \
    return Status(Status::ERROR_NULL, error_message);                  \
  }

}  // namespace glop
}  // namespace operations_research

#endif  // OR_TOOLS_GLOP_STATUS_H_
