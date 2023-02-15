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

#ifndef OR_TOOLS_UTIL_STRING_UTIL_H_
#define OR_TOOLS_UTIL_STRING_UTIL_H_

#include <string>

#include "absl/strings/str_cat.h"

namespace operations_research {

// Crops a multi-line string horizontally and vertically, as needed. Skipped
// lines (to spare vertical space) are replaced by "... [42 LINES CROPPED] ..."
// and non-skipped but cropped lines (to spare horizontal space) are replaced
// by "%prefix% ..[35 CHARS CROPPED].. %suffix%" where %prefix% and %suffix% are
// equally-sized (possibly off-by-one) substrings of the line, to fit in the
// required width.
//
// WARNING: The code is intended to be used for debugging and visual aid.
// While it shouldn't crash, it makes a few shortcuts and can violate the
// requirements (eg. some lines may be longer than max_line_length).
std::string CropMultiLineString(const std::string& s, int max_line_length,
                                int max_num_lines);

// Helper to display a object with a DebugString method in a absl::StrJoin.
struct DebugStringFormatter {
  template <typename T>
  void operator()(std::string* out, const T& t) const {
    absl::StrAppend(out, t.DebugString());
  }
};

}  // namespace operations_research

#endif  // OR_TOOLS_UTIL_STRING_UTIL_H_
