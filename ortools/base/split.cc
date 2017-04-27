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

#include "ortools/base/split.h"

#if defined(_MSC_VER)
#include <iterator>
#endif  // _MSC_VER
#include "ortools/base/logging.h"

namespace strings {
namespace {

// ----------------------------------------------------------------------
// InternalSplitStringUsing()
//    Split a std::string using a character delimiter. Append the components
//    to 'result'.
//
// Note: For multi-character delimiters, this routine will split on *ANY* of
// the characters in the std::string, not the entire std::string as a single delimiter.
// ----------------------------------------------------------------------
template <typename ITR>
static inline void InternalSplitStringUsingChar(const std::string& full, char c,
                                                ITR* result) {
  const char* p = full.data();
  const char* end = p + full.size();
  while (p != end) {
    if (*p == c) {
      ++p;
    } else {
      const char* start = p;
      while (++p != end && *p != c) {
      }
      result->emplace_back(start, p - start);
    }
  }
  return;
}

template <typename ITR>
static inline void InternalSplitStringUsing(const std::string& full,
                                            const char* delim, ITR* result) {
  // Optimize the common case where delim is a single character.
  if (delim[0] != '\0' && delim[1] == '\0') {
    char c = delim[0];
    InternalSplitStringUsingChar(full, c, result);
    return;
  }

  std::string::size_type begin_index, end_index;
  begin_index = full.find_first_not_of(delim);
  while (begin_index != std::string::npos) {
    end_index = full.find_first_of(delim, begin_index);
    if (end_index == std::string::npos) {
      end_index = full.size();
      result->emplace_back(full.c_str() + begin_index, end_index - begin_index);
      return;
    }
    result->emplace_back(full.c_str() + begin_index, end_index - begin_index);
    begin_index = full.find_first_not_of(delim, end_index);
  }
}

}  // namespace

std::vector<std::string> Split(const std::string& full, char delim, int flags) {
  CHECK_EQ(SkipEmpty(), flags);
  std::vector<std::string> out;
  InternalSplitStringUsingChar(full, delim, &out);
  return out;
}

std::vector<std::string> Split(const std::string& full, const char* delim, int flags) {
  CHECK_EQ(SkipEmpty(), flags);
  std::vector<std::string> out;
  InternalSplitStringUsing(full, delim, &out);
  return out;
}

std::vector<StringPiece> Split(const std::string& full, const char* delim,
                               int64 flags) {
  CHECK_EQ(SkipEmpty(), flags);
  std::vector<StringPiece> out;
  InternalSplitStringUsing(full, delim, &out);
  return out;
}

}  // namespace strings
