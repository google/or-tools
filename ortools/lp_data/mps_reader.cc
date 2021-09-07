// Copyright 2010-2021 Google LLC
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

#include <cstdint>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_split.h"
#include "ortools/base/status_builder.h"
#include "ortools/lp_data/lp_types.h"

namespace operations_research {
namespace glop {

class MPSReaderImpl {
 public:
  MPSReaderImpl();

  // Parses instance from a file. We currently support LinearProgram and
  // MpModelProto for the Data type, but it should be easy to add more.
  template <class Data>
  absl::Status ParseFile(const std::string& file_name, Data* data,
                         MPSReader::Form form);

 private:
  // Number of fields in one line of MPS file.
  static const int kNumFields;

  // Starting positions of each of the fields for fixed format.
  static const int kFieldStartPos[];

  // Lengths of each of the fields for fixed format.
  static const int kFieldLength[];

  // Positions where there should be spaces for fixed format.
  static const int kSpacePos[];

  // Resets the object to its initial value before reading a new file.
  void Reset();

  // Displays some information on the last loaded file.
  void DisplaySummary();

  // Get each field for a given line.
  absl::Status SplitLineIntoFields();

  // Returns true if the line matches the fixed format.
  bool IsFixedFormat();

  // Get the first word in a line.
  std::string GetFirstWord() const;

  // Returns true if the line contains a comment (starting with '*') or
  // if it is a blank line.
  bool IsCommentOrBlank() const;

  // Helper function that returns fields_[offset + index].
  const std::string& GetField(int offset, int index) const {
    return fields_[offset + index];
  }

  // Returns the offset at which to start the parsing of fields_.
  //   If in fixed form, the offset is 0.
  //   If in fixed form and the number of fields is odd, it is 1,
  //   otherwise it is 0.
  // This is useful when processing RANGES and RHS sections.
  int GetFieldOffset() const { return free_form_ ? fields_.size() & 1 : 0; }

  // Line processor.
  template <class DataWrapper>
  absl::Status ProcessLine(const std::string& line, DataWrapper* data);

  // Process section OBJSENSE in MPS file.
  template <class DataWrapper>
  absl::Status ProcessObjectiveSenseSection(DataWrapper* data);

  // Process section ROWS in the MPS file.
  template <class DataWrapper>
  absl::Status ProcessRowsSection(bool is_lazy, DataWrapper* data);

  // Process section COLUMNS in the MPS file.
  template <class DataWrapper>
  absl::Status ProcessColumnsSection(DataWrapper* data);

  // Process section RHS in the MPS file.
  template <class DataWrapper>
  absl::Status ProcessRhsSection(DataWrapper* data);

  // Process section RANGES in the MPS file.
  template <class DataWrapper>
  absl::Status ProcessRangesSection(DataWrapper* data);

  // Process section BOUNDS in the MPS file.
  template <class DataWrapper>
  absl::Status ProcessBoundsSection(DataWrapper* data);

  // Process section INDICATORS in the MPS file.
  template <class DataWrapper>
  absl::Status ProcessIndicatorsSection(DataWrapper* data);

  // Process section SOS in the MPS file.
  absl::Status ProcessSosSection();

  // Safely converts a string to a numerical type. Returns an error if the
  // string passed as parameter is ill-formed.
  absl::StatusOr<double> GetDoubleFromString(const std::string& str);
  absl::StatusOr<bool> GetBoolFromString(const std::string& str);

  // Different types of variables, as defined in the MPS file specification.
  // Note these are more precise than the ones in PrimalSimplex.
  enum BoundTypeId {
    UNKNOWN_BOUND_TYPE,
    LOWER_BOUND,
    UPPER_BOUND,
    FIXED_VARIABLE,
    FREE_VARIABLE,
    INFINITE_LOWER_BOUND,
    INFINITE_UPPER_BOUND,
    BINARY
  };

  // Different types of constraints for a given row.
  enum RowTypeId {
    UNKNOWN_ROW_TYPE,
    EQUALITY,
    LESS_THAN,
    GREATER_THAN,
    OBJECTIVE,
    NONE
  };

  // Stores a bound value of a given type, for a given column name.
  template <class DataWrapper>
  absl::Status StoreBound(const std::string& bound_type_mnemonic,
                          const std::string& column_name,
                          const std::string& bound_value, DataWrapper* data);

  // Stores a coefficient value for a column number and a row name.
  template <class DataWrapper>
  absl::Status StoreCoefficient(int col, const std::string& row_name,
                                const std::string& row_value,
                                DataWrapper* data);

  // Stores a right-hand-side value for a row name.
  template <class DataWrapper>
  absl::Status StoreRightHandSide(const std::string& row_name,
                                  const std::string& row_value,
                                  DataWrapper* data);

  // Stores a range constraint of value row_value for a row name.
  template <class DataWrapper>
  absl::Status StoreRange(const std::string& row_name,
                          const std::string& range_value, DataWrapper* data);

  // Returns an InvalidArgumentError with the given error message, postfixed by
  // the current line of the .mps file (number and contents).
  absl::Status InvalidArgumentError(const std::string& error_message);

  // Appends the current line of the .mps file (number and contents) to the
  // status if it's an error message.
  absl::Status AppendLineToError(const absl::Status& status);

  // Boolean set to true if the reader expects a free-form MPS file.
  bool free_form_;

  // Storage of the fields for a line of the MPS file.
  std::vector<std::string> fields_;

  // Stores the name of the objective row.
  std::string objective_name_;

  // Enum for section ids.
  typedef enum {
    UNKNOWN_SECTION,
    COMMENT,
    NAME,
    OBJSENSE,
    ROWS,
    LAZYCONS,
    COLUMNS,
    RHS,
    RANGES,
    BOUNDS,
    INDICATORS,
    SOS,
    ENDATA
  } SectionId;

  // Id of the current section of MPS file.
  SectionId section_;

  // Maps section mnemonic --> section id.
  absl::flat_hash_map<std::string, SectionId> section_name_to_id_map_;

  // Maps row type mnemonic --> row type id.
  absl::flat_hash_map<std::string, RowTypeId> row_name_to_id_map_;

  // Maps bound type mnemonic --> bound type id.
  absl::flat_hash_map<std::string, BoundTypeId> bound_name_to_id_map_;

  // Set of bound type mnemonics that constrain variables to be integer.
  absl::flat_hash_set<std::string> integer_type_names_set_;

  // The current line number in the file being parsed.
  int64_t line_num_;

  // The current line in the file being parsed.
  std::string line_;

  // A row of Booleans. is_binary_by_default_[col] is true if col
  // appeared within a scope started by INTORG and ended with INTEND markers.
  std::vector<bool> is_binary_by_default_;

  // True if the next variable has to be interpreted as an integer variable.
  // This is used to support the marker INTORG that starts an integer section
  // and INTEND that ends it.
  bool in_integer_section_;

  // We keep track of the number of unconstrained rows so we can display it to
  // the user because other solvers usually ignore them and we don't (they will
  // be removed in the preprocessor).
  int num_unconstrained_rows_;

  DISALLOW_COPY_AND_ASSIGN(MPSReaderImpl);
};

// Data templates.

template <class Data>
class DataWrapper {};

template <>
class DataWrapper<LinearProgram> {
 public:
  explicit DataWrapper(LinearProgram* data) { data_ = data; }

  void SetUp() {
    data_->SetDcheckBounds(false);
    data_->Clear();
  }

  void SetName(const std::string& name) { data_->SetName(name); }

  void SetObjectiveDirection(bool maximize) {
    data_->SetMaximizationProblem(maximize);
  }

  int FindOrCreateConstraint(const std::string& name) {
    return data_->FindOrCreateConstraint(name).value();
  }
  void SetConstraintBounds(int index, double lower_bound, double upper_bound) {
    data_->SetConstraintBounds(RowIndex(index), lower_bound, upper_bound);
  }
  void SetConstraintCoefficient(int row_index, int col_index,
                                double coefficient) {
    data_->SetCoefficient(RowIndex(row_index), ColIndex(col_index),
                          coefficient);
  }
  void SetIsLazy(int row_index) {
    LOG_FIRST_N(WARNING, 1)
        << "LAZYCONS section detected. It will be handled as an extension of "
           "the ROWS section.";
  }
  double ConstraintLowerBound(int row_index) {
    return data_->constraint_lower_bounds()[RowIndex(row_index)];
  }
  double ConstraintUpperBound(int row_index) {
    return data_->constraint_upper_bounds()[RowIndex(row_index)];
  }

  int FindOrCreateVariable(const std::string& name) {
    return data_->FindOrCreateVariable(name).value();
  }
  void SetVariableTypeToInteger(int index) {
    data_->SetVariableType(ColIndex(index),
                           LinearProgram::VariableType::INTEGER);
  }
  void SetVariableBounds(int index, double lower_bound, double upper_bound) {
    data_->SetVariableBounds(ColIndex(index), lower_bound, upper_bound);
  }
  void SetObjectiveCoefficient(int index, double coefficient) {
    data_->SetObjectiveCoefficient(ColIndex(index), coefficient);
  }
  bool VariableIsInteger(int index) {
    return data_->IsVariableInteger(ColIndex(index));
  }
  double VariableLowerBound(int index) {
    return data_->variable_lower_bounds()[ColIndex(index)];
  }
  double VariableUpperBound(int index) {
    return data_->variable_upper_bounds()[ColIndex(index)];
  }

  absl::Status CreateIndicatorConstraint(std::string row_name, int col_index,
                                         bool col_value) {
    return absl::UnimplementedError(
        "LinearProgram does not support indicator constraints.");
  }

  void CleanUp() { data_->CleanUp(); }

 private:
  LinearProgram* data_;
};

template <>
class DataWrapper<MPModelProto> {
 public:
  explicit DataWrapper(MPModelProto* data) { data_ = data; }

  void SetUp() { data_->Clear(); }

  void SetName(const std::string& name) { data_->set_name(name); }

  void SetObjectiveDirection(bool maximize) { data_->set_maximize(maximize); }

  int FindOrCreateConstraint(const std::string& name) {
    const auto it = constraint_indices_by_name_.find(name);
    if (it != constraint_indices_by_name_.end()) return it->second;

    const int index = data_->constraint_size();
    MPConstraintProto* const constraint = data_->add_constraint();
    constraint->set_lower_bound(0.0);
    constraint->set_upper_bound(0.0);
    constraint->set_name(name);
    constraint_indices_by_name_[name] = index;
    return index;
  }
  void SetConstraintBounds(int index, double lower_bound, double upper_bound) {
    data_->mutable_constraint(index)->set_lower_bound(lower_bound);
    data_->mutable_constraint(index)->set_upper_bound(upper_bound);
  }
  void SetConstraintCoefficient(int row_index, int col_index,
                                double coefficient) {
    // Note that we assume that there is no duplicate in the mps file format. If
    // there is, we will just add more than one entry from the same variable in
    // a constraint, and we let any program that ingests an MPModelProto handle
    // it.
    MPConstraintProto* const constraint = data_->mutable_constraint(row_index);
    constraint->add_var_index(col_index);
    constraint->add_coefficient(coefficient);
  }
  void SetIsLazy(int row_index) {
    data_->mutable_constraint(row_index)->set_is_lazy(true);
  }
  double ConstraintLowerBound(int row_index) {
    return data_->constraint(row_index).lower_bound();
  }
  double ConstraintUpperBound(int row_index) {
    return data_->constraint(row_index).upper_bound();
  }

  int FindOrCreateVariable(const std::string& name) {
    const auto it = variable_indices_by_name_.find(name);
    if (it != variable_indices_by_name_.end()) return it->second;

    const int index = data_->variable_size();
    MPVariableProto* const variable = data_->add_variable();
    variable->set_lower_bound(0.0);
    variable->set_name(name);
    variable_indices_by_name_[name] = index;
    return index;
  }
  void SetVariableTypeToInteger(int index) {
    data_->mutable_variable(index)->set_is_integer(true);
  }
  void SetVariableBounds(int index, double lower_bound, double upper_bound) {
    data_->mutable_variable(index)->set_lower_bound(lower_bound);
    data_->mutable_variable(index)->set_upper_bound(upper_bound);
  }
  void SetObjectiveCoefficient(int index, double coefficient) {
    data_->mutable_variable(index)->set_objective_coefficient(coefficient);
  }
  bool VariableIsInteger(int index) {
    return data_->variable(index).is_integer();
  }
  double VariableLowerBound(int index) {
    return data_->variable(index).lower_bound();
  }
  double VariableUpperBound(int index) {
    return data_->variable(index).upper_bound();
  }

  absl::Status CreateIndicatorConstraint(std::string cst_name, int var_index,
                                         bool var_value) {
    const auto it = constraint_indices_by_name_.find(cst_name);
    if (it == constraint_indices_by_name_.end()) {
      return absl::InvalidArgumentError(
          absl::StrCat("Constraint \"", cst_name, "\" doesn't exist."));
    }
    const int cst_index = it->second;

    MPGeneralConstraintProto* const constraint =
        data_->add_general_constraint();
    constraint->set_name(
        absl::StrCat("ind_", data_->constraint(cst_index).name()));
    MPIndicatorConstraint* const indicator =
        constraint->mutable_indicator_constraint();
    *indicator->mutable_constraint() = data_->constraint(cst_index);
    indicator->set_var_index(var_index);
    indicator->set_var_value(var_value);
    constraints_to_delete_.insert(cst_index);

    return absl::OkStatus();
  }

  void CleanUp() {
    google::protobuf::util::RemoveAt(data_->mutable_constraint(),
                                     constraints_to_delete_);
  }

 private:
  MPModelProto* data_;

  absl::flat_hash_map<std::string, int> variable_indices_by_name_;
  absl::flat_hash_map<std::string, int> constraint_indices_by_name_;
  absl::node_hash_set<int> constraints_to_delete_;
};

template <class Data>
absl::Status MPSReaderImpl::ParseFile(const std::string& file_name, Data* data,
                                      MPSReader::Form form) {
  if (data == nullptr) {
    return absl::InvalidArgumentError("NULL pointer passed as argument.");
  }

  if (form == MPSReader::AUTO_DETECT) {
    if (ParseFile(file_name, data, MPSReader::FIXED).ok()) {
      return absl::OkStatus();
    }
    return ParseFile(file_name, data, MPSReader::FREE);
  }

  // TODO(user): Use the form directly.
  free_form_ = form == MPSReader::FREE;
  Reset();
  DataWrapper<Data> data_wrapper(data);
  data_wrapper.SetUp();
  for (const std::string& line :
       FileLines(file_name, FileLineIterator::REMOVE_INLINE_CR)) {
    RETURN_IF_ERROR(ProcessLine(line, &data_wrapper));
  }
  data_wrapper.CleanUp();
  DisplaySummary();
  return absl::OkStatus();
}

template <class DataWrapper>
absl::Status MPSReaderImpl::ProcessLine(const std::string& line,
                                        DataWrapper* data) {
  ++line_num_;
  line_ = line;
  if (IsCommentOrBlank()) {
    return absl::OkStatus();  // Skip blank lines and comments.
  }
  if (!free_form_ && absl::StrContains(line_, '\t')) {
    return InvalidArgumentError("File contains tabs.");
  }
  std::string section;
  if (line[0] != '\0' && line[0] != ' ') {
    section = GetFirstWord();
    section_ =
        gtl::FindWithDefault(section_name_to_id_map_, section, UNKNOWN_SECTION);
    if (section_ == UNKNOWN_SECTION) {
      return InvalidArgumentError("Unknown section.");
    }
    if (section_ == COMMENT) {
      return absl::OkStatus();
    }
    if (section_ == OBJSENSE) {
      return absl::OkStatus();
    }
    if (section_ == NAME) {
      RETURN_IF_ERROR(SplitLineIntoFields());
      // NOTE(user): The name may differ between fixed and free forms. In
      // fixed form, the name has at most 8 characters, and starts at a specific
      // position in the NAME line. For MIPLIB2010 problems (eg, air04, glass4),
      // the name in fixed form ends up being preceded with a whitespace.
      // TODO(user): Return an error for fixed form if the problem name
      // does not fit.
      if (free_form_) {
        if (fields_.size() >= 2) {
          data->SetName(fields_[1]);
        }
      } else {
        const std::vector<std::string> free_fields =
            absl::StrSplit(line_, absl::ByAnyChar(" \t"), absl::SkipEmpty());
        const std::string free_name =
            free_fields.size() >= 2 ? free_fields[1] : "";
        const std::string fixed_name = fields_.size() >= 3 ? fields_[2] : "";
        if (free_name != fixed_name) {
          return InvalidArgumentError(
              "Fixed form invalid: name differs between free and fixed "
              "forms.");
        }
        data->SetName(fixed_name);
      }
    }
    return absl::OkStatus();
  }
  RETURN_IF_ERROR(SplitLineIntoFields());
  switch (section_) {
    case NAME:
      return InvalidArgumentError("Second NAME field.");
    case OBJSENSE:
      return ProcessObjectiveSenseSection(data);
    case ROWS:
      return ProcessRowsSection(/*is_lazy=*/false, data);
    case LAZYCONS:
      return ProcessRowsSection(/*is_lazy=*/true, data);
    case COLUMNS:
      return ProcessColumnsSection(data);
    case RHS:
      return ProcessRhsSection(data);
    case RANGES:
      return ProcessRangesSection(data);
    case BOUNDS:
      return ProcessBoundsSection(data);
    case INDICATORS:
      return ProcessIndicatorsSection(data);
    case SOS:
      return ProcessSosSection();
    case ENDATA:  // Do nothing.
      break;
    default:
      return InvalidArgumentError("Unknown section.");
  }
  return absl::OkStatus();
}

template <class DataWrapper>
absl::Status MPSReaderImpl::ProcessObjectiveSenseSection(DataWrapper* data) {
  if (fields_.size() != 1 && fields_[0] != "MIN" && fields_[0] != "MAX") {
    return InvalidArgumentError("Expected objective sense (MAX or MIN).");
  }
  data->SetObjectiveDirection(/*maximize=*/fields_[0] == "MAX");
  return absl::OkStatus();
}

template <class DataWrapper>
absl::Status MPSReaderImpl::ProcessRowsSection(bool is_lazy,
                                               DataWrapper* data) {
  if (fields_.size() < 2) {
    return InvalidArgumentError("Not enough fields in ROWS section.");
  }
  const std::string row_type_name = fields_[0];
  const std::string row_name = fields_[1];
  RowTypeId row_type = gtl::FindWithDefault(row_name_to_id_map_, row_type_name,
                                            UNKNOWN_ROW_TYPE);
  if (row_type == UNKNOWN_ROW_TYPE) {
    return InvalidArgumentError("Unknown row type.");
  }

  // The first NONE constraint is used as the objective.
  if (objective_name_.empty() && row_type == NONE) {
    row_type = OBJECTIVE;
    objective_name_ = row_name;
  } else {
    if (row_type == NONE) {
      ++num_unconstrained_rows_;
    }
    const int row = data->FindOrCreateConstraint(row_name);
    if (is_lazy) data->SetIsLazy(row);

    // The initial row range is [0, 0]. We encode the type in the range by
    // setting one of the bounds to +/- infinity.
    switch (row_type) {
      case LESS_THAN:
        data->SetConstraintBounds(row, -kInfinity,
                                  data->ConstraintUpperBound(row));
        break;
      case GREATER_THAN:
        data->SetConstraintBounds(row, data->ConstraintLowerBound(row),
                                  kInfinity);
        break;
      case NONE:
        data->SetConstraintBounds(row, -kInfinity, kInfinity);
        break;
      case EQUALITY:
      default:
        break;
    }
  }
  return absl::OkStatus();
}

template <class DataWrapper>
absl::Status MPSReaderImpl::ProcessColumnsSection(DataWrapper* data) {
  // Take into account the INTORG and INTEND markers.
  if (absl::StrContains(line_, "'MARKER'")) {
    if (absl::StrContains(line_, "'INTORG'")) {
      VLOG(2) << "Entering integer marker.\n" << line_;
      if (in_integer_section_) {
        return InvalidArgumentError("Found INTORG inside the integer section.");
      }
      in_integer_section_ = true;
    } else if (absl::StrContains(line_, "'INTEND'")) {
      VLOG(2) << "Leaving integer marker.\n" << line_;
      if (!in_integer_section_) {
        return InvalidArgumentError(
            "Found INTEND without corresponding INTORG.");
      }
      in_integer_section_ = false;
    }
    return absl::OkStatus();
  }
  const int start_index = free_form_ ? 0 : 1;
  if (fields_.size() < start_index + 3) {
    return InvalidArgumentError("Not enough fields in COLUMNS section.");
  }
  const std::string& column_name = GetField(start_index, 0);
  const std::string& row1_name = GetField(start_index, 1);
  const std::string& row1_value = GetField(start_index, 2);
  const int col = data->FindOrCreateVariable(column_name);
  is_binary_by_default_.resize(col + 1, false);
  if (in_integer_section_) {
    data->SetVariableTypeToInteger(col);
    // The default bounds for integer variables are [0, 1].
    data->SetVariableBounds(col, 0.0, 1.0);
    is_binary_by_default_[col] = true;
  } else {
    data->SetVariableBounds(col, 0.0, kInfinity);
  }
  RETURN_IF_ERROR(StoreCoefficient(col, row1_name, row1_value, data));
  if (fields_.size() == start_index + 4) {
    return InvalidArgumentError("Unexpected number of fields.");
  }
  if (fields_.size() - start_index > 4) {
    const std::string& row2_name = GetField(start_index, 3);
    const std::string& row2_value = GetField(start_index, 4);
    RETURN_IF_ERROR(StoreCoefficient(col, row2_name, row2_value, data));
  }
  return absl::OkStatus();
}

template <class DataWrapper>
absl::Status MPSReaderImpl::ProcessRhsSection(DataWrapper* data) {
  const int start_index = free_form_ ? 0 : 2;
  const int offset = start_index + GetFieldOffset();
  if (fields_.size() < offset + 2) {
    return InvalidArgumentError("Not enough fields in RHS section.");
  }
  // const std::string& rhs_name = fields_[0]; is not used
  const std::string& row1_name = GetField(offset, 0);
  const std::string& row1_value = GetField(offset, 1);
  RETURN_IF_ERROR(StoreRightHandSide(row1_name, row1_value, data));
  if (fields_.size() - start_index >= 4) {
    const std::string& row2_name = GetField(offset, 2);
    const std::string& row2_value = GetField(offset, 3);
    RETURN_IF_ERROR(StoreRightHandSide(row2_name, row2_value, data));
  }
  return absl::OkStatus();
}

template <class DataWrapper>
absl::Status MPSReaderImpl::ProcessRangesSection(DataWrapper* data) {
  const int start_index = free_form_ ? 0 : 2;
  const int offset = start_index + GetFieldOffset();
  if (fields_.size() < offset + 2) {
    return InvalidArgumentError("Not enough fields in RHS section.");
  }
  // const std::string& range_name = fields_[0]; is not used
  const std::string& row1_name = GetField(offset, 0);
  const std::string& row1_value = GetField(offset, 1);
  RETURN_IF_ERROR(StoreRange(row1_name, row1_value, data));
  if (fields_.size() - start_index >= 4) {
    const std::string& row2_name = GetField(offset, 2);
    const std::string& row2_value = GetField(offset, 3);
    RETURN_IF_ERROR(StoreRange(row2_name, row2_value, data));
  }
  return absl::OkStatus();
}

template <class DataWrapper>
absl::Status MPSReaderImpl::ProcessBoundsSection(DataWrapper* data) {
  if (fields_.size() < 3) {
    return InvalidArgumentError("Not enough fields in BOUNDS section.");
  }
  const std::string bound_type_mnemonic = fields_[0];
  const std::string bound_row_name = fields_[1];
  const std::string column_name = fields_[2];
  std::string bound_value;
  if (fields_.size() >= 4) {
    bound_value = fields_[3];
  }
  return StoreBound(bound_type_mnemonic, column_name, bound_value, data);
}

template <class DataWrapper>
absl::Status MPSReaderImpl::ProcessIndicatorsSection(DataWrapper* data) {
  // TODO(user): Enforce section order. This section must come after
  // anything related to constraints, or we'll have partial data inside the
  // indicator constraints.
  if (fields_.size() < 4) {
    return InvalidArgumentError("Not enough fields in INDICATORS section.");
  }

  const std::string type = fields_[0];
  if (type != "IF") {
    return InvalidArgumentError(
        "Indicator constraints must start with \"IF\".");
  }
  const std::string row_name = fields_[1];
  const std::string column_name = fields_[2];
  const std::string column_value = fields_[3];

  bool value;
  ASSIGN_OR_RETURN(value, GetBoolFromString(column_value));

  const int col = data->FindOrCreateVariable(column_name);
  // Variables used in indicator constraints become Boolean by default.
  data->SetVariableTypeToInteger(col);
  data->SetVariableBounds(col, std::max(0.0, data->VariableLowerBound(col)),
                          std::min(1.0, data->VariableUpperBound(col)));

  RETURN_IF_ERROR(
      AppendLineToError(data->CreateIndicatorConstraint(row_name, col, value)));

  return absl::OkStatus();
}

template <class DataWrapper>
absl::Status MPSReaderImpl::StoreCoefficient(int col,
                                             const std::string& row_name,
                                             const std::string& row_value,
                                             DataWrapper* data) {
  if (row_name.empty() || row_name == "$") {
    return absl::OkStatus();
  }

  double value;
  ASSIGN_OR_RETURN(value, GetDoubleFromString(row_value));
  if (value == kInfinity || value == -kInfinity) {
    return InvalidArgumentError("Constraint coefficients cannot be infinity.");
  }
  if (value == 0.0) return absl::OkStatus();
  if (row_name == objective_name_) {
    data->SetObjectiveCoefficient(col, value);
  } else {
    const int row = data->FindOrCreateConstraint(row_name);
    data->SetConstraintCoefficient(row, col, value);
  }
  return absl::OkStatus();
}

template <class DataWrapper>
absl::Status MPSReaderImpl::StoreRightHandSide(const std::string& row_name,
                                               const std::string& row_value,
                                               DataWrapper* data) {
  if (row_name.empty()) return absl::OkStatus();

  if (row_name != objective_name_) {
    const int row = data->FindOrCreateConstraint(row_name);
    Fractional value;
    ASSIGN_OR_RETURN(value, GetDoubleFromString(row_value));

    // The row type is encoded in the bounds, so at this point we have either
    // (-kInfinity, 0.0], [0.0, 0.0] or [0.0, kInfinity). We use the right
    // hand side to change any finite bound.
    const Fractional lower_bound =
        (data->ConstraintLowerBound(row) == -kInfinity) ? -kInfinity : value;
    const Fractional upper_bound =
        (data->ConstraintUpperBound(row) == kInfinity) ? kInfinity : value;
    data->SetConstraintBounds(row, lower_bound, upper_bound);
  }
  return absl::OkStatus();
}

template <class DataWrapper>
absl::Status MPSReaderImpl::StoreRange(const std::string& row_name,
                                       const std::string& range_value,
                                       DataWrapper* data) {
  if (row_name.empty()) return absl::OkStatus();

  const int row = data->FindOrCreateConstraint(row_name);
  Fractional range;
  ASSIGN_OR_RETURN(range, GetDoubleFromString(range_value));

  Fractional lower_bound = data->ConstraintLowerBound(row);
  Fractional upper_bound = data->ConstraintUpperBound(row);
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
  data->SetConstraintBounds(row, lower_bound, upper_bound);
  return absl::OkStatus();
}

template <class DataWrapper>
absl::Status MPSReaderImpl::StoreBound(const std::string& bound_type_mnemonic,
                                       const std::string& column_name,
                                       const std::string& bound_value,
                                       DataWrapper* data) {
  const BoundTypeId bound_type_id = gtl::FindWithDefault(
      bound_name_to_id_map_, bound_type_mnemonic, UNKNOWN_BOUND_TYPE);
  if (bound_type_id == UNKNOWN_BOUND_TYPE) {
    return InvalidArgumentError("Unknown bound type.");
  }
  const int col = data->FindOrCreateVariable(column_name);
  if (integer_type_names_set_.count(bound_type_mnemonic) != 0) {
    data->SetVariableTypeToInteger(col);
  }
  if (is_binary_by_default_.size() <= col) {
    // This is the first time that this column has been encountered.
    is_binary_by_default_.resize(col + 1, false);
  }
  // Check that "binary by default" implies "integer".
  DCHECK(!is_binary_by_default_[col] || data->VariableIsInteger(col));
  Fractional lower_bound = data->VariableLowerBound(col);
  Fractional upper_bound = data->VariableUpperBound(col);
  // If a variable is binary by default, its status is reset if any bound
  // is set on it. We take care to restore the default bounds for general
  // integer variables.
  if (is_binary_by_default_[col]) {
    lower_bound = Fractional(0.0);
    upper_bound = kInfinity;
  }
  switch (bound_type_id) {
    case LOWER_BOUND: {
      ASSIGN_OR_RETURN(lower_bound, GetDoubleFromString(bound_value));
      // LI with the value 0.0 specifies general integers with no upper bound.
      if (bound_type_mnemonic == "LI" && lower_bound == 0.0) {
        upper_bound = kInfinity;
      }
      break;
    }
    case UPPER_BOUND: {
      ASSIGN_OR_RETURN(upper_bound, GetDoubleFromString(bound_value));
      break;
    }
    case FIXED_VARIABLE: {
      ASSIGN_OR_RETURN(lower_bound, GetDoubleFromString(bound_value));
      upper_bound = lower_bound;
      break;
    }
    case FREE_VARIABLE:
      lower_bound = -kInfinity;
      upper_bound = +kInfinity;
      break;
    case INFINITE_LOWER_BOUND:
      lower_bound = -kInfinity;
      break;
    case INFINITE_UPPER_BOUND:
      upper_bound = +kInfinity;
      break;
    case BINARY:
      lower_bound = Fractional(0.0);
      upper_bound = Fractional(1.0);
      break;
    case UNKNOWN_BOUND_TYPE:
    default:
      return InvalidArgumentError("Unknown bound type.");
  }
  is_binary_by_default_[col] = false;
  data->SetVariableBounds(col, lower_bound, upper_bound);
  return absl::OkStatus();
}

const int MPSReaderImpl::kNumFields = 6;
const int MPSReaderImpl::kFieldStartPos[kNumFields] = {1, 4, 14, 24, 39, 49};
const int MPSReaderImpl::kFieldLength[kNumFields] = {2, 8, 8, 12, 8, 12};
const int MPSReaderImpl::kSpacePos[12] = {12, 13, 22, 23, 36, 37,
                                          38, 47, 48, 61, 62, 63};

MPSReaderImpl::MPSReaderImpl()
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
  section_name_to_id_map_["OBJSENSE"] = OBJSENSE;
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
  bound_name_to_id_map_["MI"] = INFINITE_LOWER_BOUND;
  bound_name_to_id_map_["PL"] = INFINITE_UPPER_BOUND;
  bound_name_to_id_map_["BV"] = BINARY;
  bound_name_to_id_map_["LI"] = LOWER_BOUND;
  bound_name_to_id_map_["UI"] = UPPER_BOUND;
  integer_type_names_set_.insert("BV");
  integer_type_names_set_.insert("LI");
  integer_type_names_set_.insert("UI");
}

void MPSReaderImpl::Reset() {
  fields_.resize(kNumFields);
  line_num_ = 0;
  in_integer_section_ = false;
  num_unconstrained_rows_ = 0;
  objective_name_.clear();
}

void MPSReaderImpl::DisplaySummary() {
  if (num_unconstrained_rows_ > 0) {
    VLOG(1) << "There are " << num_unconstrained_rows_ + 1
            << " unconstrained rows. The first of them (" << objective_name_
            << ") was used as the objective.";
  }
}

bool MPSReaderImpl::IsFixedFormat() {
  for (const int i : kSpacePos) {
    if (i >= line_.length()) break;
    if (line_[i] != ' ') return false;
  }
  return true;
}

absl::Status MPSReaderImpl::SplitLineIntoFields() {
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
  return absl::OkStatus();
}

std::string MPSReaderImpl::GetFirstWord() const {
  if (line_[0] == ' ') {
    return std::string("");
  }
  const int first_space_pos = line_.find(' ');
  const std::string first_word = line_.substr(0, first_space_pos);
  return first_word;
}

bool MPSReaderImpl::IsCommentOrBlank() const {
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

absl::StatusOr<double> MPSReaderImpl::GetDoubleFromString(
    const std::string& str) {
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

absl::StatusOr<bool> MPSReaderImpl::GetBoolFromString(const std::string& str) {
  int result;
  if (!absl::SimpleAtoi(str, &result) || result < 0 || result > 1) {
    return InvalidArgumentError(
        absl::StrCat("Failed to convert \"", str, "\" to bool."));
  }
  return result;
}

absl::Status MPSReaderImpl::ProcessSosSection() {
  return InvalidArgumentError("Section SOS currently not supported.");
}

absl::Status MPSReaderImpl::InvalidArgumentError(
    const std::string& error_message) {
  return AppendLineToError(absl::InvalidArgumentError(error_message));
}

absl::Status MPSReaderImpl::AppendLineToError(const absl::Status& status) {
  return util::StatusBuilder(status).SetAppend()
         << " Line " << line_num_ << ": \"" << line_ << "\".";
}

// Parses instance from a file.
absl::Status MPSReader::ParseFile(const std::string& file_name,
                                  LinearProgram* data, Form form) {
  return MPSReaderImpl().ParseFile(file_name, data, form);
}

absl::Status MPSReader::ParseFile(const std::string& file_name,
                                  MPModelProto* data, Form form) {
  return MPSReaderImpl().ParseFile(file_name, data, form);
}

}  // namespace glop
}  // namespace operations_research
