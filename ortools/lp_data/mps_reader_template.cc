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

#include "ortools/lp_data/mps_reader_template.h"

#include "absl/container/inlined_vector.h"
#include "absl/status/status.h"
#include "absl/strings/ascii.h"
#include "absl/strings/string_view.h"
#include "ortools/base/status_macros.h"

namespace operations_research::internal {
namespace {

// Starting positions of each of the fields for fixed format.
static constexpr int kFieldStartPos[kNumMpsFields] = {1, 4, 14, 24, 39, 49};

// Lengths of each of the fields for fixed format.
static constexpr int kFieldLength[kNumMpsFields] = {2, 8, 8, 12, 8, 12};

// Positions where there should be spaces for fixed format.
static constexpr int kSpacePos[12] = {12, 13, 22, 23, 36, 37,
                                      38, 47, 48, 61, 62, 63};

}  // namespace

bool MPSLineInfo::IsFixedFormat() const {
  constexpr int kMaxLineSize =
      kFieldStartPos[kNumMpsFields - 1] + kFieldLength[kNumMpsFields - 1];
  // Note that `line_` already has been stripped of trailing white spaces.
  const int line_size = line_.size();
  if (line_size > kMaxLineSize) return false;
  for (const int i : kSpacePos) {
    if (i >= line_size) break;
    if (line_[i] != ' ') return false;
  }
  return true;
}

absl::Status MPSLineInfo::SplitLineIntoFields() {
  if (free_form_) {
    absl::string_view remaining_line = absl::StripLeadingAsciiWhitespace(line_);
    // Although using `fields_ = StrSplit(line, ByAnyChar(" \t))` is shorter to
    // write, this explicit loop, and checking the `kNumMpsFields` is
    // significantly faster.
    while (!remaining_line.empty()) {
      if (fields_.size() == kNumMpsFields) {
        return InvalidArgumentError("Found too many fields.");
      }
      // find_first_of() returns npos for "not found". substr() interprets
      // len==npos as end of string.
      const int pos = remaining_line.find_first_of(" \t");
      fields_.push_back(remaining_line.substr(0, pos));
      if (pos == absl::string_view::npos) {
        // substr() will throw an exception if the start is npos.
        break;
      }
      remaining_line =
          absl::StripLeadingAsciiWhitespace(remaining_line.substr(pos));
    }
  } else {
    const int line_size = line_.size();
    for (int i = 0; i < kNumMpsFields; ++i) {
      if (kFieldStartPos[i] >= line_size) break;
      fields_.push_back(absl::StripTrailingAsciiWhitespace(
          line_.substr(kFieldStartPos[i], kFieldLength[i])));
    }
  }
  return absl::OkStatus();
}

absl::string_view MPSLineInfo::GetFirstWord() const {
  return line_.substr(0, line_.find(' '));
}

bool MPSLineInfo::IsCommentOrBlank() const {
  // Trailing whitespace has already been trimmed, so a blank line has become
  // empty.
  return (line_.empty() || line_[0] == '*');
}

absl::Status MPSLineInfo::InvalidArgumentError(
    absl::string_view error_message) const {
  return AppendLineToError(absl::InvalidArgumentError(error_message));
}

absl::Status MPSLineInfo::AppendLineToError(const absl::Status& status) const {
  return util::StatusBuilder(status).SetAppend()
         << " Line " << line_num_ << ": \"" << line_ << "\".";
}

}  //  namespace operations_research::internal
