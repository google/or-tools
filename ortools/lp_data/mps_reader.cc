// Copyright 2010-2018 Google LLC
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

#include "ortools/lp_data/mps_reader.h"

#include "absl/strings/match.h"
#include "absl/strings/str_split.h"
#include "ortools/base/status.h"
//#include "ortools/base/status_builder.h"

namespace operations_research {
namespace glop {

const int MPSReader::kNumFields = 6;
const int MPSReader::kFieldStartPos[kNumFields] = {1, 4, 14, 24, 39, 49};
const int MPSReader::kFieldLength[kNumFields] = {2, 8, 8, 12, 8, 12};
const int MPSReader::kSpacePos[12] = {12, 13, 22, 23, 36, 37,
                                      38, 47, 48, 61, 62, 63};

MPSReader::MPSReader()
    : free_form_(true),
      fields_(kNumFields),
      section_(UNKNOWN_SECTION),
      section_name_to_id_map_(),
      row_name_to_id_map_(),
      bound_name_to_id_map_(),
      integer_type_names_set_(),
      line_num_(0),
      line_(),
      in_integer_section_(false),
      num_unconstrained_rows_(0) {
  section_name_to_id_map_["*"] = COMMENT;
  section_name_to_id_map_["NAME"] = NAME;
  section_name_to_id_map_["ROWS"] = ROWS;
  section_name_to_id_map_["LAZYCONS"] = LAZYCONS;
  section_name_to_id_map_["COLUMNS"] = COLUMNS;
  section_name_to_id_map_["RHS"] = RHS;
  section_name_to_id_map_["RANGES"] = RANGES;
  section_name_to_id_map_["BOUNDS"] = BOUNDS;
  section_name_to_id_map_["INDICATORS"] = INDICATORS;
  section_name_to_id_map_["ENDATA"] = ENDATA;
  row_name_to_id_map_["E"] = EQUALITY;
  row_name_to_id_map_["L"] = LESS_THAN;
  row_name_to_id_map_["G"] = GREATER_THAN;
  row_name_to_id_map_["N"] = NONE;
  bound_name_to_id_map_["LO"] = LOWER_BOUND;
  bound_name_to_id_map_["UP"] = UPPER_BOUND;
  bound_name_to_id_map_["FX"] = FIXED_VARIABLE;
  bound_name_to_id_map_["FR"] = FREE_VARIABLE;
  bound_name_to_id_map_["MI"] = NEGATIVE;
  bound_name_to_id_map_["PL"] = POSITIVE;
  bound_name_to_id_map_["BV"] = BINARY;
  bound_name_to_id_map_["LI"] = LOWER_BOUND;
  bound_name_to_id_map_["UI"] = UPPER_BOUND;
  integer_type_names_set_.insert("BV");
  integer_type_names_set_.insert("LI");
  integer_type_names_set_.insert("UI");
}

void MPSReader::Reset() {
  fields_.resize(kNumFields);
  line_num_ = 0;
  in_integer_section_ = false;
  num_unconstrained_rows_ = 0;
  objective_name_.clear();
}

void MPSReader::DisplaySummary() {
  if (num_unconstrained_rows_ > 0) {
    VLOG(1) << "There are " << num_unconstrained_rows_ + 1
            << " unconstrained rows. The first of them (" << objective_name_
            << ") was used as the objective.";
  }
}

bool MPSReader::IsFixedFormat() {
  for (const int i : kSpacePos) {
    if (i >= line_.length()) break;
    if (line_[i] != ' ') return false;
  }
  return true;
}

util::Status MPSReader::SplitLineIntoFields() {
  if (free_form_) {
    fields_ = absl::StrSplit(line_, absl::ByAnyChar(" \t"), absl::SkipEmpty());
    if (fields_.size() > kNumFields) {
      return InvalidArgumentError("Found too many fields.");
    }
  } else {
    // Note: the name should also comply with the fixed format guidelines
    // (maximum 8 characters) but in practice there are many problem files in
    // our netlib archive that are in fixed format and have a long name. We
    // choose to ignore these cases and treat them as fixed format anyway.
    if (section_ != NAME && !IsFixedFormat()) {
      return InvalidArgumentError("Line is not in fixed format.");
    }
    const int length = line_.length();
    for (int i = 0; i < kNumFields; ++i) {
      if (kFieldStartPos[i] < length) {
        fields_[i] = line_.substr(kFieldStartPos[i], kFieldLength[i]);
        fields_[i].erase(fields_[i].find_last_not_of(" ") + 1);
      } else {
        fields_[i] = "";
      }
    }
  }
  return util::OkStatus();
}

std::string MPSReader::GetFirstWord() const {
  if (line_[0] == ' ') {
    return std::string("");
  }
  const int first_space_pos = line_.find(' ');
  const std::string first_word = line_.substr(0, first_space_pos);
  return first_word;
}

bool MPSReader::IsCommentOrBlank() const {
  const char* line = line_.c_str();
  if (*line == '*') {
    return true;
  }
  for (; *line != '\0'; ++line) {
    if (*line != ' ' && *line != '\t') {
      return false;
    }
  }
  return true;
}

util::StatusOr<double> MPSReader::GetDoubleFromString(const std::string& str) {
  double result;
  if (!absl::SimpleAtod(str, &result)) {
    return InvalidArgumentError(
        absl::StrCat("Failed to convert \"", str, "\" to double."));
  }
  if (std::isnan(result)) {
    return InvalidArgumentError("Found NaN value.");
  }
  return result;
}

util::StatusOr<bool> MPSReader::GetBoolFromString(const std::string& str) {
  int result;
  if (!absl::SimpleAtoi(str, &result) || result < 0 || result > 1) {
    return InvalidArgumentError(
        absl::StrCat("Failed to convert \"", str, "\" to bool."));
  }
  if (std::isnan(result)) {
    return InvalidArgumentError("Found NaN value.");
  }
  return result;
}

util::Status MPSReader::ProcessSosSection() {
  return InvalidArgumentError("Section SOS currently not supported.");
}

util::Status MPSReader::InvalidArgumentError(const std::string& error_message) {
  return util::InvalidArgumentError(error_message);
}

// util::Status MPSReader::AppendLineToError(const util::Status& status) {
//   return util::StatusBuilder(status, GTL_LOC).SetAppend()
//          << " Line " << line_num_ << ": \"" << line_ << "\".";
// }

}  // namespace glop
}  // namespace operations_research
