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

#ifndef ORTOOLS_UTIL_STRING_ARRAY_H_
#define ORTOOLS_UTIL_STRING_ARRAY_H_

#include <string>
#include <vector>

#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"

namespace operations_research {
// ---------- Pretty Print Helpers ----------

// Converts a vector into a string by calling the given method (or simply
// getting the given string member), on all elements, and concatenating
// the obtained strings with the given separator.

// Join v[i].DebugString().
template <class T>
std::string JoinDebugString(const std::vector<T>& v,
                            absl::string_view separator = ", ") {
  return absl::StrJoin(v, separator, [](std::string* out, const T& t) {
    out->append(t.DebugString());
  });
}

// Join v[i]->DebugString().
template <class T>
std::string JoinDebugStringPtr(const std::vector<T>& v,
                               absl::string_view separator = ", ") {
  return absl::StrJoin(v, separator, [](std::string* out, const T& t) {
    out->append(t->DebugString());
  });
}

// Join v[i]->name().
template <class T>
std::string JoinNamePtr(const std::vector<T>& v,
                        absl::string_view separator = ", ") {
  return absl::StrJoin(v, separator, [](std::string* out, const T& t) {
    out->append(t->name());
  });
}

// Join v[i]->name.
template <class T>
std::string JoinNameFieldPtr(const std::vector<T>& v,
                             absl::string_view separator = ", ") {
  return absl::StrJoin(
      v, separator, [](std::string* out, const T& t) { out->append(t->name); });
}

}  // namespace operations_research
#endif  // ORTOOLS_UTIL_STRING_ARRAY_H_
