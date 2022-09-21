// Copyright 2010-2022 Google LLC
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

#ifndef OR_TOOLS_UTIL_STRING_ARRAY_H_
#define OR_TOOLS_UTIL_STRING_ARRAY_H_

#include <string>
#include <vector>

namespace operations_research {
// ---------- Pretty Print Helpers ----------

// See the straightforward (and unique) usage of this macro below.
#define RETURN_STRINGIFIED_VECTOR(vector, separator, method) \
  std::string out;                                           \
  for (int i = 0; i < vector.size(); ++i) {                  \
    if (i > 0) out += separator;                             \
    out += vector[i] method;                                 \
  }                                                          \
  return out

// Converts a vector into a string by calling the given method (or simply
// getting the given string member), on all elements, and concatenating
// the obtained strings with the given separator.

// Join v[i].DebugString().
template <class T>
std::string JoinDebugString(const std::vector<T>& v,
                            const std::string& separator) {
  RETURN_STRINGIFIED_VECTOR(v, separator, .DebugString());
}

// Join v[i]->DebugString().
template <class T>
std::string JoinDebugStringPtr(const std::vector<T>& v,
                               const std::string& separator) {
  RETURN_STRINGIFIED_VECTOR(v, separator, ->DebugString());
}

// Join v[i]->name().
template <class T>
std::string JoinNamePtr(const std::vector<T>& v, const std::string& separator) {
  RETURN_STRINGIFIED_VECTOR(v, separator, ->name());
}

// Join v[i]->name.
template <class T>
std::string JoinNameFieldPtr(const std::vector<T>& v,
                             const std::string& separator) {
  RETURN_STRINGIFIED_VECTOR(v, separator, ->name);
}

#undef RETURN_STRINGIFIED_VECTOR

}  // namespace operations_research
#endif  // OR_TOOLS_UTIL_STRING_ARRAY_H_
