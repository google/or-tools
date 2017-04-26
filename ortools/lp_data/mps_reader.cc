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


#include "ortools/lp_data/mps_reader.h"

#include <math.h>
#include <algorithm>
#include <memory>
#include <utility>
#include <fstream>

#include "ortools/base/callback.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/logging.h"
#include "ortools/base/stringprintf.h"
#include "ortools/base/file.h"
#include "ortools/base/filelinereader.h"
#include "ortools/base/numbers.h"  // for safe_strtod
#include "ortools/base/split.h"
#include "ortools/base/strutil.h"
#include "ortools/base/map_util.h"  // for FindOrNull, FindWithDefault
#include "ortools/lp_data/lp_print_utils.h"
#include "ortools/base/status.h"

DEFINE_bool(mps_free_form, false, "Read MPS files in free form.");
DEFINE_bool(mps_stop_after_first_error, true, "Stop after the first error.");

namespace operations_research {
namespace glop {

const int MPSReader::kNumFields = 6;
const int MPSReader::kFieldStartPos[kNumFields] = {1, 4, 14, 24, 39, 49};
const int MPSReader::kFieldLength[kNumFields] = {2, 8, 8, 12, 8, 12};

MPSReader::MPSReader()
    : free_form_(FLAGS_mps_free_form),
      data_(nullptr),
      problem_name_(""),
      parse_success_(true),
      fields_(kNumFields),
      section_(UNKNOWN_SECTION),
      section_name_to_id_map_(),
      row_name_to_id_map_(),
      bound_name_to_id_map_(),
      integer_type_names_set_(),
      line_num_(0),
      line_(),
      has_lazy_constraints_(false),
      in_integer_section_(false),
      num_unconstrained_rows_(0),
      log_errors_(true) {
  section_name_to_id_map_["*"] = COMMENT;
  section_name_to_id_map_["NAME"] = NAME;
  section_name_to_id_map_["ROWS"] = ROWS;
  section_name_to_id_map_["LAZYCONS"] = LAZYCONS;
  section_name_to_id_map_["COLUMNS"] = COLUMNS;
  section_name_to_id_map_["RHS"] = RHS;
  section_name_to_id_map_["RANGES"] = RANGES;
  section_name_to_id_map_["BOUNDS"] = BOUNDS;
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
  parse_success_ = true;
  problem_name_.clear();
  line_num_ = 0;
  has_lazy_constraints_ = false;
  in_integer_section_ = false;
  num_unconstrained_rows_ = 0;
  objective_name_.clear();
}

void MPSReader::DisplaySummary() {
  if (num_unconstrained_rows_ > 0) {
    LOG(INFO) << "There are " << num_unconstrained_rows_ + 1
              << " unconstrained rows. The first of them (" << objective_name_
              << ") was used as the objective.";
  }
}

void MPSReader::SplitLineIntoFields() {
  if (free_form_) {
    fields_ = strings::Split(line_, ' ', strings::SkipEmpty());
    CHECK_GE(kNumFields, fields_.size());
  } else {
    int length = line_.length();
    for (int i = 0; i < kNumFields; ++i) {
      if (kFieldStartPos[i] < length) {
        fields_[i] = line_.substr(kFieldStartPos[i], kFieldLength[i]);
        fields_[i].erase(fields_[i].find_last_not_of(" ") + 1);
      } else {
        fields_[i] = "";
      }
    }
  }
}

std::string MPSReader::GetFirstWord() const {
  if (line_[0] == ' ') {
    return std::string("");
  }
  const int first_space_pos = line_.find(' ');
  std::string first_word = line_.substr(0, first_space_pos);
  return first_word;
}

bool MPSReader::LoadFile(const std::string& file_name, LinearProgram* data) {
  if (data == nullptr) {
    LOG(ERROR) << "Serious programming error: NULL LinearProgram pointer "
               << "passed as argument.";
    return false;
  }
  Reset();
  data_ = data;
  data_->Clear();
  std::ifstream tmp_file(file_name.c_str());
        const bool file_exists = tmp_file.good();
        tmp_file.close();
        if (file_exists) {
          FileLineReader reader(file_name.c_str());
          reader.set_line_callback(
              NewPermanentCallback(this, &MPSReader::ProcessLine));
          reader.Reload();
    data->CleanUp();
    DisplaySummary();
    return reader.loaded_successfully() && parse_success_;
  } else {
    LOG(DFATAL) << "File not found: " << file_name;
    return false;
  }
}

// TODO(user): Ideally have a method to compare instances of LinearProgram
// and have method which reads in both modes, compares the programs and checks
// that either both modes succeeded and led to the same program, or one mode
// failed or both modes failed (cf. what is done in linear_solver/solve.cc
// using protos).
bool MPSReader::LoadFileWithMode(const std::string& file_name, bool free_form,
                                 LinearProgram* data) {
  free_form_ = free_form;
  if (LoadFile(file_name, data)) {
    free_form_ = FLAGS_mps_free_form;
    return true;
  }
  free_form_ = FLAGS_mps_free_form;
  return false;
}

bool MPSReader::LoadFileAndTryFreeFormOnFail(const std::string& file_name,
                                             LinearProgram* data) {
  if (!LoadFileWithMode(file_name, false, data)) {
    LOG(INFO) << "Trying to read as an MPS free-format file.";
    return LoadFileWithMode(file_name, true, data);
  }
  return true;
}


std::string MPSReader::GetProblemName() const { return problem_name_; }

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

void MPSReader::ProcessLine(char* line) {
  ++line_num_;
  if (!parse_success_ && FLAGS_mps_stop_after_first_error) return;
  line_ = line;
  if (IsCommentOrBlank()) {
    return;  // Skip blank lines and comments.
  }
  std::string section;
  if (*line != '\0' && *line != ' ') {
    section = GetFirstWord();
    section_ =
        FindWithDefault(section_name_to_id_map_, section, UNKNOWN_SECTION);
    if (section_ == UNKNOWN_SECTION) {
      if (log_errors_) {
        LOG(ERROR) << "At line " << line_num_
                   << ": Unknown section: " << section
                   << ". (Line contents: " << line_ << ").";
      }
      parse_success_ = false;
      return;
    }
    if (section_ == COMMENT) {
      return;
    }
    if (section_ == NAME) {
      SplitLineIntoFields();
      if (free_form_) {
        if (fields_.size() >= 2) {
          problem_name_ = fields_[1];
        }
      } else {
        if (fields_.size() >= 3) {
          problem_name_ = fields_[2];
        }
      }
      // NOTE(user): The name may differ between fixed and free forms. In
      // fixed form, the name has at most 8 characters, and starts at a specific
      // position in the NAME line. For MIPLIB2010 problems (eg, air04, glass4),
      // the name in fixed form ends up being preceeded with a whitespace.
      // TODO(user, bdb): Return an error for fixed form if the problem name
      // does not fit.
      data_->SetName(problem_name_);
    }
    return;
  }
  SplitLineIntoFields();
  switch (section_) {
    case NAME:
      if (log_errors_) {
        LOG(ERROR) << "At line " << line_num_ << ": Second NAME field"
                   << ". (Line contents: " << line_ << ").";
      }
      parse_success_ = false;
      break;
    case ROWS:
      ProcessRowsSection();
      break;
    case LAZYCONS:
      if (!has_lazy_constraints_) {
        LOG(WARNING) << "LAZYCONS section detected. It will be handled as an "
                        "extension of the ROWS section.";
        has_lazy_constraints_ = true;
      }
      ProcessRowsSection();
      break;
    case COLUMNS:
      ProcessColumnsSection();
      break;
    case RHS:
      ProcessRhsSection();
      break;
    case RANGES:
      ProcessRangesSection();
      break;
    case BOUNDS:
      ProcessBoundsSection();
      break;
    case SOS:
      ProcessSosSection();
      break;
    case ENDATA:  // Do nothing.
      break;
    default:
      if (log_errors_) {
        LOG(ERROR) << "At line " << line_num_
                   << ": Unknown section: " << section
                   << ". (Line contents: " << line_ << ").";
      }
      parse_success_ = false;
      break;
  }
}

double MPSReader::GetDoubleFromString(const std::string& param) {
  double result;
  if (!safe_strtod(param, &result)) {
    if (log_errors_) {
      LOG(ERROR) << "At line " << line_num_
                 << ": Failed to convert std::string to double. String = " << param
                 << ". (Line contents = '" << line_ << "')."
                 << " free_form_ = " << free_form_;
    }
    parse_success_ = false;
  }
  return result;
}

void MPSReader::ProcessRowsSection() {
  std::string row_type_name = fields_[0];
  std::string row_name = fields_[1];
  MPSRowType row_type =
      FindWithDefault(row_name_to_id_map_, row_type_name, UNKNOWN_ROW_TYPE);
  if (row_type == UNKNOWN_ROW_TYPE) {
    if (log_errors_) {
      LOG(ERROR) << "At line " << line_num_ << ": Unknown row type "
                 << row_type_name << ". (Line contents = " << line_ << ").";
    }
    parse_success_ = false;
    return;
  }

  // The first NONE constraint is used as the objective.
  if (objective_name_.empty() && row_type == NONE) {
    row_type = OBJECTIVE;
    objective_name_ = row_name;
  } else {
    if (row_type == NONE) {
      ++num_unconstrained_rows_;
    }
    RowIndex row = data_->FindOrCreateConstraint(row_name);

    // The initial row range is [0, 0]. We encode the type in the range by
    // setting one of the bound to +/- infinity.
    switch (row_type) {
      case LESS_THAN:
        data_->SetConstraintBounds(row, -kInfinity,
                                   data_->constraint_upper_bounds()[row]);
        break;
      case GREATER_THAN:
        data_->SetConstraintBounds(row, data_->constraint_lower_bounds()[row],
                                   kInfinity);
        break;
      case NONE:
        data_->SetConstraintBounds(row, -kInfinity, kInfinity);
        break;
      case EQUALITY:
      default:
        break;
    }
  }
}

void MPSReader::ProcessColumnsSection() {
  // Take into account the INTORG and INTEND markers.
  if (line_.find("'MARKER'") != std::string::npos) {
    if (line_.find("'INTORG'") != std::string::npos) {
      in_integer_section_ = true;
    } else if (line_.find("'INTEND'") != std::string::npos) {
      in_integer_section_ = false;
    }
    return;
  }
  const int start_index = free_form_ ? 0 : 1;
  const std::string& column_name = GetField(start_index, 0);
  const std::string& row1_name = GetField(start_index, 1);
  const std::string& row1_value = GetField(start_index, 2);
  const ColIndex col = data_->FindOrCreateVariable(column_name);
  is_binary_by_default_.resize(col + 1, false);
  if (in_integer_section_) {
    data_->SetVariableType(col, LinearProgram::VariableType::INTEGER);
    // The default bounds for integer variables are [0, 1].
    data_->SetVariableBounds(col, 0.0, 1.0);
    is_binary_by_default_[col] = true;
  } else {
    data_->SetVariableBounds(col, 0.0, kInfinity);
  }
  StoreCoefficient(col, row1_name, row1_value);
  if (fields_.size() - start_index >= 4) {
    const std::string& row2_name = GetField(start_index, 3);
    const std::string& row2_value = GetField(start_index, 4);
    StoreCoefficient(col, row2_name, row2_value);
  }
}

void MPSReader::ProcessRhsSection() {
  const int start_index = free_form_ ? 0 : 2;
  const int offset = start_index + GetFieldOffset();
  // const std::string& rhs_name = fields_[0]; is not used
  const std::string& row1_name = GetField(offset, 0);
  const std::string& row1_value = GetField(offset, 1);
  StoreRightHandSide(row1_name, row1_value);
  if (fields_.size() - start_index >= 4) {
    const std::string& row2_name = GetField(offset, 2);
    const std::string& row2_value = GetField(offset, 3);
    StoreRightHandSide(row2_name, row2_value);
  }
}

void MPSReader::ProcessRangesSection() {
  const int start_index = free_form_ ? 0 : 2;
  const int offset = start_index + GetFieldOffset();
  // const std::string& range_name = fields_[0]; is not used
  const std::string& row1_name = GetField(offset, 0);
  const std::string& row1_value = GetField(offset, 1);
  StoreRange(row1_name, row1_value);
  if (fields_.size() - start_index >= 4) {
    const std::string& row2_name = GetField(offset, 2);
    const std::string& row2_value = GetField(offset, 3);
    StoreRange(row2_name, row2_value);
  }
}

void MPSReader::ProcessBoundsSection() {
  std::string bound_type_mnemonic = fields_[0];
  std::string bound_row_name = fields_[1];
  std::string column_name = fields_[2];
  std::string bound_value;
  if (fields_.size() >= 4) {
    bound_value = fields_[3];
  }
  StoreBound(bound_type_mnemonic, column_name, bound_value);
}

void MPSReader::ProcessSosSection() {
  LOG(ERROR) << "At line " << line_num_
             << "Section SOS currently not supported."
             << ". (Line contents: " << line_ << ").";
  parse_success_ = false;
}

void MPSReader::StoreCoefficient(ColIndex col, const std::string& row_name,
                                 const std::string& row_value) {
  if (row_name.empty() || row_name == "$") {
    return;
  }
  const Fractional value(GetDoubleFromString(row_value));
  if (value == 0.0) return;
  if (row_name == objective_name_) {
    data_->SetObjectiveCoefficient(col, value);
  } else {
    const RowIndex row = data_->FindOrCreateConstraint(row_name);
    data_->SetCoefficient(row, col, value);
  }
}

void MPSReader::StoreRightHandSide(const std::string& row_name,
                                   const std::string& row_value) {
  if (row_name.empty()) {
    return;
  }
  if (row_name != objective_name_) {
    RowIndex row = data_->FindOrCreateConstraint(row_name);
    const Fractional value = GetDoubleFromString(row_value);

    // The row type is encoded in the bounds, so at this point we have either
    // (-kInfinity, 0.0], [0.0, 0.0] or [0.0, kInfinity). We use the right
    // hand side to change any finite bound.
    const Fractional lower_bound =
        (data_->constraint_lower_bounds()[row] == -kInfinity) ? -kInfinity
                                                              : value;
    const Fractional upper_bound =
        (data_->constraint_upper_bounds()[row] == kInfinity) ? kInfinity
                                                             : value;
    data_->SetConstraintBounds(row, lower_bound, upper_bound);
  }
}

void MPSReader::StoreRange(const std::string& row_name, const std::string& range_value) {
  if (row_name.empty()) {
    return;
  }
  const RowIndex row = data_->FindOrCreateConstraint(row_name);
  const Fractional range(GetDoubleFromString(range_value));

  Fractional lower_bound = data_->constraint_lower_bounds()[row];
  Fractional upper_bound = data_->constraint_upper_bounds()[row];
  if (lower_bound == upper_bound) {
    if (range < 0.0) {
      lower_bound += range;
    } else {
      upper_bound += range;
    }
  }
  if (lower_bound == -kInfinity) {
    lower_bound = upper_bound - fabs(range);
  }
  if (upper_bound == kInfinity) {
    upper_bound = lower_bound + fabs(range);
  }
  data_->SetConstraintBounds(row, lower_bound, upper_bound);
}

void MPSReader::StoreBound(const std::string& bound_type_mnemonic,
                           const std::string& column_name,
                           const std::string& bound_value) {
  const BoundTypeId bound_type_id = FindWithDefault(
      bound_name_to_id_map_, bound_type_mnemonic, UNKNOWN_BOUND_TYPE);
  if (bound_type_id == UNKNOWN_BOUND_TYPE) {
    parse_success_ = false;
    if (log_errors_) {
      LOG(ERROR) << "At line " << line_num_ << ": Unknown bound type "
                 << bound_type_mnemonic << ". (Line contents = " << line_
                 << ").";
    }
    return;
  }
  const ColIndex col = data_->FindOrCreateVariable(column_name);
  if (integer_type_names_set_.count(bound_type_mnemonic) != 0) {
    data_->SetVariableType(col, LinearProgram::VariableType::INTEGER);
  }
  // Resize the is_binary_by_default_ in case it is the first time this column
  // is encountered.
  is_binary_by_default_.resize(col + 1, false);
  // Check that "binary by default" implies "integer".
  DCHECK(!is_binary_by_default_[col] || data_->IsVariableInteger(col));
  Fractional lower_bound = data_->variable_lower_bounds()[col];
  Fractional upper_bound = data_->variable_upper_bounds()[col];
  // If a variable is binary by default, its status is reset if any bound
  // is set on it. We take care to restore the default bounds for general
  // integer variables.
  if (is_binary_by_default_[col]) {
    lower_bound = Fractional(0.0);
    upper_bound = kInfinity;
  }
  switch (bound_type_id) {
    case LOWER_BOUND:
      lower_bound = Fractional(GetDoubleFromString(bound_value));
      break;
    case UPPER_BOUND:
      upper_bound = Fractional(GetDoubleFromString(bound_value));
      break;
    case FIXED_VARIABLE: {
      const Fractional value(GetDoubleFromString(bound_value));
      lower_bound = value;
      upper_bound = value;
      break;
    }
    case FREE_VARIABLE:
      lower_bound = -kInfinity;
      upper_bound = +kInfinity;
      break;
    case NEGATIVE:
      lower_bound = -kInfinity;
      upper_bound = Fractional(0.0);
      break;
    case POSITIVE:
      lower_bound = Fractional(0.0);
      upper_bound = +kInfinity;
      break;
    case BINARY:
      lower_bound = Fractional(0.0);
      upper_bound = Fractional(1.0);
      break;
    case UNKNOWN_BOUND_TYPE:
    default:
      if (log_errors_) {
        LOG(ERROR) << "At line " << line_num_
                   << "Serious error: unknown bound type " << column_name << " "
                   << bound_type_mnemonic << " " << bound_value
                   << ". (Line contents: " << line_ << ").";
      }
      parse_success_ = false;
  }
  is_binary_by_default_[col] = false;
  data_->SetVariableBounds(col, lower_bound, upper_bound);
}

}  // namespace glop
}  // namespace operations_research
