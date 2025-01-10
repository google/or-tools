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

#include <algorithm>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "ortools/base/logging.h"

namespace operations_research {

std::string CropMultiLineString(const std::string& s, int max_line_length,
                                int max_num_lines) {
  if (s.empty()) return "";
  std::vector<std::string> lines = absl::StrSplit(s, '\n');
  DCHECK_GE(lines.size(), 1);  // Even an empty string yields one (empty) line.
  // We ignore the terminating newline for line accounting, but we do output it
  // back in the end.
  const bool has_terminating_newline = lines.back().empty();
  if (has_terminating_newline) lines.pop_back();

  constexpr char kMultiLineCropTemplate[] = "###% 4d LINES SKIPPED ###";
  constexpr char kSingleLineCropTemplate[] = " ..[% 4d CHARS CROPPED ].. ";
  const int single_line_crop_template_length =
      absl::StrFormat(kSingleLineCropTemplate, 0).size();
  int num_lines_before = -1;
  int num_lines_after = -1;
  int num_lines_cropped = -1;
  if (lines.size() > max_num_lines) {
    num_lines_after = (max_num_lines - 1) / 2;
    // There's a corner case for max_num_lines=0. See the unit test.
    num_lines_before =
        max_num_lines == 0 ? 0 : max_num_lines - 1 - num_lines_after;
    num_lines_cropped = lines.size() - num_lines_before - num_lines_after;
    lines.erase(lines.begin() + num_lines_before + 1,
                lines.end() - num_lines_after);
    // We only fill the special line mentioning the skipped lines after we're
    // done with the horizontal crops, in the loop below.
    lines[num_lines_before].clear();
  }
  for (std::string& line : lines) {
    if (line.size() <= max_line_length) continue;
    const int num_chars_after = std::max(
        0, static_cast<int>(
               (max_line_length - single_line_crop_template_length) / 2));
    const int num_chars_before = std::max(
        0, static_cast<int>(max_line_length - single_line_crop_template_length -
                            num_chars_after));
    line = absl::StrCat(
        line.substr(0, num_chars_before),
        absl::StrFormat(
            kSingleLineCropTemplate,
            static_cast<int>(line.size() - num_chars_after - num_chars_before)),
        line.substr(line.size() - num_chars_after));
  }
  if (num_lines_cropped >= 0) {
    lines[num_lines_before] =
        absl::StrFormat(kMultiLineCropTemplate, num_lines_cropped);
  }
  if (has_terminating_newline) lines.push_back("");
  return absl::StrJoin(lines, "\n");
}

}  // namespace operations_research
