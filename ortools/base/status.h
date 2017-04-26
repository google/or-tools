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

#ifndef OR_TOOLS_BASE_STATUS_H_
#define OR_TOOLS_BASE_STATUS_H_

#include <string>
#include "ortools/base/logging.h"
#include "ortools/base/join.h"

namespace util {

namespace error {
enum Error {
  INTERNAL = 1,
  INVALID_ARGUMENT = 2,
  DEADLINE_EXCEEDED = 3,
};
}  // namespace error

struct Status {
  enum { OK = 0 };
  Status() : error_code_(OK) {}
  Status(int error_code) : error_code_(error_code) {}  // NOLINT
  Status(int error_code, const std::string& error_message)
      : error_code_(error_code), error_message_(error_message) {}
  Status(const Status& other)
      : error_code_(other.error_code_), error_message_(other.error_message_) {}

  bool ok() const { return error_code_ == OK; }

  std::string ToString() const {
    if (ok()) return "OK";
    return StrCat("ERROR #", error_code_, ": '", error_message_, "'");
  }

  std::string error_message() const { return error_message_; }

  void IgnoreError() const {}

 private:
  int error_code_;
  std::string error_message_;
};

inline std::ostream& operator<<(std::ostream& out, const Status& status) {
  out << status.ToString();
  return out;
}

}  // namespace util

#define CHECK_OK(status) CHECK_EQ("OK", (status).ToString())

#endif  // OR_TOOLS_BASE_STATUS_H_
