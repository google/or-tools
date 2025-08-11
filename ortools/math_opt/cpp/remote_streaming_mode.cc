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

#include "ortools/math_opt/cpp/remote_streaming_mode.h"

#include <ostream>
#include <string>

#include "absl/strings/string_view.h"

namespace operations_research::math_opt {

std::string AbslUnparseFlag(const RemoteStreamingSolveMode value) {
  switch (value) {
    case RemoteStreamingSolveMode::kDefault:
      return "default";
    case RemoteStreamingSolveMode::kSubprocess:
      return "subprocess";
    case RemoteStreamingSolveMode::kInProcess:
      return "inprocess";
  }
}

bool AbslParseFlag(const absl::string_view text,
                   RemoteStreamingSolveMode* const value,
                   std::string* const error) {
  // We don't need this function to be efficient, hence we reuse
  // AbslUnparseFlag().
  for (const RemoteStreamingSolveMode mode : {
           RemoteStreamingSolveMode::kDefault,
           RemoteStreamingSolveMode::kSubprocess,
           RemoteStreamingSolveMode::kInProcess,
       }) {
    if (text == AbslUnparseFlag(mode)) {
      *value = mode;
      return true;
    }
  }
  *error = "unknown value for enumeration";
  return false;
}

std::ostream& operator<<(std::ostream& out,
                         const RemoteStreamingSolveMode mode) {
  out << AbslUnparseFlag(mode);
  return out;
}

}  // namespace operations_research::math_opt
