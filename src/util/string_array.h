// Copyright 2010-2013 Google
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

#include "base/integral_types.h"
#include "base/stringprintf.h"

namespace operations_research {
// ---------- Pretty Print Helpers ----------

// See the straightforward (and unique) usage of this macro below.
#define RETURN_STRINGIFIED_VECTOR(vector, separator, method) \
  std::string out;                                                \
  for (int i = 0; i < vector.size(); ++i) {                  \
    if (i > 0) out += separator;                             \
    out += vector[i] method;                                 \
  }                                                          \
  return out

// Converts a vector into a std::string by calling the given method (or simply
// getting the given std::string member), on all elements, and concatenating
// the obtained strings with the given separator.

// Join v[i].DebugString().
template <class T>
std::string JoinDebugString(const std::vector<T>& v, const std::string& separator) {
  RETURN_STRINGIFIED_VECTOR(v, separator, .DebugString());
}

// Join v[i]->DebugString().
template <class T>
std::string JoinDebugStringPtr(const std::vector<T>& v, const std::string& separator) {
  RETURN_STRINGIFIED_VECTOR(v, separator, ->DebugString());
}

// Join v[i]->name().
template <class T>
std::string JoinNamePtr(const std::vector<T>& v, const std::string& separator) {
  RETURN_STRINGIFIED_VECTOR(v, separator, ->name());
}

// Join v[i]->name.
template <class T>
std::string JoinNameFieldPtr(const std::vector<T>& v, const std::string& separator) {
  RETURN_STRINGIFIED_VECTOR(v, separator, ->name);
}

#undef RETURN_STRINGIFIED_VECTOR

// TODO(user): use strings::Join instead of the methods below.

// Creates a std::string from an array of int64, and a separator.
inline std::string Int64ArrayToString(const int64* const array, int size,
                                 const std::string& separator) {
  std::string out;
  for (int i = 0; i < size; ++i) {
    if (i > 0) {
      out.append(separator);
    }
    StringAppendF(&out, "%" GG_LL_FORMAT "d", array[i]);
  }
  return out;
}

// Creates a std::string from an array of int, and a separator.
inline std::string IntArrayToString(const int* const array, int size,
                               const std::string& separator) {
  std::string out;
  for (int i = 0; i < size; ++i) {
    if (i > 0) {
      out.append(separator);
    }
    StringAppendF(&out, "%d", array[i]);
  }
  return out;
}

// Creates a std::string from an vector of int64, and a separator.
inline std::string IntVectorToString(const std::vector<int64>& array,
                                const std::string& separator) {
  return Int64ArrayToString(array.data(), array.size(), separator);
}

// Creates a std::string from an vector of int, and a separator.
inline std::string IntVectorToString(const std::vector<int>& array,
                                const std::string& separator) {
  return IntArrayToString(array.data(), array.size(), separator);
}

}  // namespace operations_research
#endif  // OR_TOOLS_UTIL_STRING_ARRAY_H_
