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

#include "ortools/math_opt/io/lp/lp_name.h"

#include "absl/status/status.h"
#include "absl/strings/ascii.h"
#include "absl/strings/string_view.h"
#include "ortools/base/status_builder.h"

namespace operations_research::lp_format {

bool ValidateCharInName(unsigned char c, bool leading) {
  if (absl::ascii_isalpha(c)) {
    return true;
  }
  if (!leading) {
    if (c == '.' || absl::ascii_isdigit(c)) {
      return true;
    }
  }
  switch (c) {
    case '!':
    case '"':
    case '#':
    case '$':
    case '%':
    case '&':
    case '(':
    case ')':
    case ',':
    case ';':
    case '?':
    case '@':
    case '_':
    case '`':
    case '\'':
    case '{':
    case '}':
    case '~':
      return true;
    default:
      return false;
  }
}

absl::Status ValidateName(absl::string_view name) {
  if (name.empty()) {
    return absl::InvalidArgumentError("empty name invalid");
  }
  for (int i = 0; i < name.size(); ++i) {
    if (!ValidateCharInName(name[i], i == 0)) {
      return util::InvalidArgumentErrorBuilder()
             << "invalid character: " << name[i] << " at index: " << i
             << " in: " << name;
    }
  }
  return absl::OkStatus();
}

}  // namespace operations_research::lp_format
