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

#ifndef OR_TOOLS_LP_DATA_MPS_READER_TEMPLATE_H_
#define OR_TOOLS_LP_DATA_MPS_READER_TEMPLATE_H_

// A templated-reader for MPS (Mathematical Programming System) format.
//
// From Wikipedia https://en.wikipedia.org/wiki/MPS_(format):
// ```
// The format was named after an early IBM LP product[1] and has emerged as a de
// facto standard ASCII medium among most of the commercial LP solvers.
//
// MPS is column-oriented (as opposed to entering the model as equations), and
// all model components (variables, rows, etc.) receive names. MPS is an old
// format, so it is set up for punch cards: Fields start in column 2, 5, 15, 25,
// 40 and 50. Sections of an MPS file are marked by so-called header cards,
// which are distinguished by their starting in column 1. Although it is typical
// to use upper-case throughout the file for historical reasons, many
// MPS-readers will accept mixed-case for anything except the header cards, and
// some allow mixed-case anywhere. The names that you choose for the individual
// entities (constraints or variables) are not important to the solver; one
// should pick meaningful names, or easy names for a post-processing code to
// read.
// ```
//
// For example:
// ```
// NAME          TESTPROB
// ROWS
//  N  COST
//  L  LIM1
//  G  LIM2
//  E  MYEQN
// COLUMNS
//     XONE      COST      1              LIM1      1
//     XONE      LIM2      1
//     YTWO      COST      4              LIM1      1
//     YTWO      MYEQN     -1
//     ZTHREE    COST      9              LIM2      1
//     ZTHREE    MYEQN     1
// RHS
//     RHS1      LIM1      5              LIM2      10
//     RHS1      MYEQN     7
// BOUNDS
//  UP BND1      XONE      4
//  LO BND1      YTWO      -1
//  UP BND1      YTWO      1
// ENDATA
// ```
//
// Note that the example, and the previous paragraph, mention that data must
// start at given columns in the text. This is commonly referred to as 'fixed'
// (width) format. In this version of the format, variable and constraint names
// can contain white space, but they are limited to a maximum width of eight
// characters, and each `section` marker must start at column 1.
//
// A common alternative is the so-called `free` format; where names
// can have (in principle) arbitrary length, but no white space, and where each
// data or section line can start with or without white space.
// In both cases the number of fields in each line remain unchanged.
// This implementation supports both `fixed` and `free` (width) format.
//
// TODO(b/284163180): The current behavior is that in free format header lines
// do not start with white space, and data lines must start with at least one
// white space.
//
// Although there is no `one` format (as many solvers have expanded
// it over time to support their own generalizations to MIP; i.e. Mixed Integer
// (Linear) Programming; most support the sections shown in the previous
// example.
//
// In what follows, we describe the format and requirements for each of the
// supported sections. Note that sections must appear in the order in
// this description, and that optional sections can be skipped altogether, but
// if they do appear, the must do so in the order in this description.
//
// Note that variables and constraints are declared in the order in which they
// appear in the file.
// Lines whose first character is '*' are considered comments and ignored;
// empty lines are also ignored.
//
// Section order and data within each section:
//
// - NAME
//
// This optional section has the format:
// 'NAME         <optional_name>`
// In fixed format, <optional_name> must start at column 15.
//
// - OBJSENSE
//
// This optional section specifies the objective direction of the problem (min
// or max), by a single line containing either 'MIN' or 'MAX'. In fixed format,
// this field must start at column 2. If no OBJSENSE section is present, the
// problem should be treated as a minimization problem (this is the most widely
// used convention, but the actual behavior is implementation defined).
//
// - ROWS
//
// This is mandatory section, and each following data line is composed of
// lines with two fields:
// ' T RowName'
// where T is one of:
// - N: for no constraint type, usually used to describe objective
//      coefficients. The first row of type N is used as objective function. If
//      no row is of type N, then the objective function is zero, and the
//      problem can be seen a feasibility problem.
// - L: for less than or equal,
// - G: for greater than or equal,
// - E: for equality constraints.
// Right hand side of constraints are zero by default (these can be overridden
// in sections RHS and RANGES). Repeating a 'RowName' is undefined behavior. In
// fixed format, the type appears in column 2 and the row name starts in
// column 5.
//
// - LAZYCONS
//
// This section is optional, and has the same format (and meaning) as the ROWS
// section, i.e. each constraint mentioned here must be new, and each one of
// them defines a constraint of the problem. The only difference is that
// constraints defined in this section are marked as 'lazy', meaning that there
// might be an advantage, when solving the problem, to dynamically add them to
// the solving process on the fly.
//
// - COLUMNS
//
// This is a mandatory section, and each of the following data lines is
// composed of three or five fields with the format:
// ' <ColName> <RowName> <Value> <RowName2> <Value2>'
// where 'RowName' and 'RowName2' are constraints defined in the ROWS or
// LAZYCONS section; 'Value' and 'Value2' are finite values; 'RowName2' and
// 'Value2' are optional. The triplet <RowName,ColName,Value> is added to
// constraint matrix; and, if present, the triplet <RowName2,ColName,Value2> is
// added to the constraint matrix. Note that there is no explicit requirement
// that triplets are unique (and how to treat duplicates is
// implementation-defined) nor sorted.
//
// In fixed format, the column name starts in column 5, the row name for the
// first non-zero starts in column 15, and the value for the first non-zero
// starts in column 25. If a second non-zero is present, the row name starts in
// column 40 and the value starts in column 50.
//
// The COLUMNS section can optionally include (possibly multiple) integrality
// markers. Variables mentioned between a pair of markers are assigned type
// 'Integer' with binary bounds by default (even if the variable appears for the
// first time outside a pair of integrality markers, thus changing its default
// bounds). Refer to the BOUNDS section for how to change these bounds.
//
// The format of these markers is (excluding double quotes):
// - " <IgnoredField> 'MARKER' 'INTORG'",
// - " <ColName> <RowName> <Value> <RowName2> <Value2>"
//   ...
// - " <ColName> <RowName> <Value> <RowName2> <Value2>"
// - " <IgnoredField> 'MARKER' 'INTEND'",
// Where the first field is ignored. In fixed format, the fields start in
// columns 5, 15 and 40, respectively. Note that the second field must exactly
// match 'MARKER', and the third field must be 'INTORG' for opening an integer
// section, and 'INTEND' for closing an integer section.
//
// - RHS
//
// This is a mandatory section, and each of the following data lines is
// composed of three or five fields with the format:
// ' <Ignored_Field> <RowName1> <Value1> <OptionalRow2> <OptionalValue2>',
// where the first field is ignored, and <RowName> must have been defined in
// sections ROWS or LAZYCONS with type E, L or G, and where <Value1> is the
// right hand side of <RowName>, and must be a finite value. If <OptionalRow2>
// and <OptionalValue2> are present, the same constraints and behavior applies.
// In fixed format fields start at columns 2, 5, 15, 40 and 50.
//
// You can specify an objective 'Offset' by adding the pair <Objective_Name>
// <Minus_Offset> in one of the data lines of the RHS section.
//
// - RANGES
//
// This is an optional section, and each of the following data lines is
// composed of three or five fields:
// ' <IgnoredField> <RowName> <Range1> <OptionalRowName2> <OptionalRange2>',
// where the first field is ignored, and <RowName> must have been defined in
// sections ROWS or LAZYCONS with type E, L or G, and <Range1> must be a finite
// value. In fixed format fields must start in columns 2, 5, 15, 40 and 50.
//
// The effect of specifying a range depends on the sense of the
// specified row and whether the range has a positive or negative <Range1>:
//
//    Row_type   Range_value_sign   rhs_lower_limit  rhs_upper_limit
//    G          + or -             rhs              rhs + |range|
//    L          + or -             rhs - |range|    rhs
//    E          +                  rhs              rhs + range
//    E          -                  rhs + range      rhs
//
// If <OptionalRowName2> and <OptionalRange2> are present, the same constraints
// and behavior applies.
//
// - BOUNDS
//
// Each variable has by default a lower bound of zero, and an upper bound of
// infinity, except if the variable is mentioned between integrality markers and
// is not mentioned in this section, in which case its lower bound is zero, and
// its upper bound is one.
//
// This is a mandatory section, and each of the following data lines is composed
// of three or four fields with the format:
// ' <BoundType> <IgnoredField> <ColName> <Value>',
// - LO: lower bound for variable, <Value> is mandatory, and
//       the data line has the effect of setting <Value> <= <ColName>,
// - UP: upper bound for variable, <Value> is mandatory, and the
//       data line has the effect of setting <ColName> <= <Value>,
// - FX: for fixed variable, <Value> is mandatory, and the data line has the
//       effect of setting <Value> <= <ColName> <= <Value>,
// - FR: for `free` variable, <Value> is optional and ignored if present, and
//       the data line has the effect of setting −∞ <= <ColName> <= ∞,
// - MI: infinity lower bound, <Value> is optional and ignored if present, and
//       the data line has the effect of setting −∞ <= <ColName>,
// - PL: infinity upper bound, <Value> is optional and ignored if present, and
//       the data line has the effect of setting <ColName> <= ∞,
// - BV: binary variable, <Value> is optional and ignored if present, and the
//       data line has the effect of setting 0 <= <ColName> <= 1,
// - LI: lower bound for integer variables, same constraints and effect as LO.
// - UI: upper bound for integer variables, same constraints and effect as UP.
// - SC: stands for semi-continuous and indicates that the variable may be zero,
//      but if not must be equal to at least the value given (this is not a
//      common type of variable, and can easily be described in terms of a
//      binary plus a continuous variable and a constraint linking the two, an
//      implementation may choose not to support this kind of variable);
//      <Value> is mandatory, and is only meaningful if it is strictly
//      positive.
// No extra constraints or assumptions are imposed on the order, or the number
// of bound constraints on a variable. Each data line is processed sequentially
// and its effects enforced; regardless of previously set bounds, explicitly or
// by default.
//
// In fixed format, fields start in columns 2, 5, 15 and 25.
//
// - INDICATORS
//
// This is an optional section, and each of the following data lines is
// composed of four fields with the format:
// ' IF <RowName> <ColName> <BinaryValue>',
// where <RowName> is a row defined either in the ROWS or LAZYCONS sections,
// <ColName> is forced to be a binary variable (intersecting previously set
// bounds with the interval [0,1], and requiring it to be integer); the effect
// of the data line is to remove <RowName> from the set of common linear
// constraints (which must be satisfied for all feasible solutions), and
// instead require the constraint to be satisfied only if the binary variable
// <ColName> holds the value <BinaryValue>.
// Note that integer/primal tolerances on variables have surprising effects: if
// a binary variable has the value (1+-tolerance), it is considered to be at
// value 1 for the purposes of indicator constraints.
//
// - ENDDATA
//
// This is a mandatory section, and it should be the last line in a MPS
// file/string. What happens with lines after this section is undefined
// behavior.
//
// Some extended versions (often times incompatible between themselves) of the
// format can be seen here:
//
//  https://www.gurobi.com/documentation/10.0/refman/mps_format.html
//  https://www.ibm.com/docs/en/icos/22.1.0?topic=standard-records-in-mps-format
//  https://lpsolve.sourceforge.net/5.0/mps-format.htm

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"
#include "ortools/base/logging.h"
#include "ortools/base/map_util.h"
#include "ortools/base/protobuf_util.h"
#include "ortools/base/status_macros.h"
#include "ortools/util/filelineiter.h"

namespace operations_research {

// Forms of MPS format supported, either detected automatically, or free
// format, or fixed (width) format.
enum class MPSReaderFormat { kAutoDetect, kFree, kFixed };

// Templated `MPS` reader. The template class `DataWrapper` must provide:
// type:
// - `IndexType`: for indices of rows and columns.
// functions:
// - `void SetUp()`: Called before parsing. After this function call, the
//              internal state of the object should be the same as in creation.
//              Note that this function can be called more than once if using
//              `MPSReaderFormat::kAutoDetect` format.
// - `void CleanUp()`: Called once, after parsing has been successful, to
//              perform any internal clean up if needed.
// - `double ConstraintLowerBound(IndexType index)`: Returns the (currently
//              stored) lower bound for 'Constraint[index]'; where 'index' is
//              a value previously returned by `FindOrCreateConstraint`.
// - `double ConstraintUpperBound(IndexType index)`: Returns the (currently
//              stored) upper bound for 'Constraint[index]'; where 'index' is
//              a value previously returned by `FindOrCreateConstraint`.
// - `IndexType FindOrCreateConstraint(const std::string& row_name)`: Returns
// the
//              (internally assigned) index to the constraint of the given
//              name. If `row_name` is new, the constraint must be created with
//              a zero lower bound and a zero upper bound.
// - `IndexType FindOrCreateVariable(const std::string& col_name)`: Returns the
//              (internally assigned) index to the variable of the given name.
//              Newly created variables should have a zero objective, zero
//              lower bound, infinity upper bound, and be considered as
//              'continuous'. If `col_name` is new, the variable must be
//              created with a zero objective coefficient, a zero lower bound,
//              and an infinity upper bound.
// - `void SetConstraintBounds(IndexType index,double lower_bound, double
//              upper_bound)`: Stores lower and upper bounds for
//              'Constraint[index]'. `index` is a value previously returned by
//              `FindOrCreateConstraint`.
// - `void SetConstraintCoefficient(IndexType row_index, IndexType col_index,
//              double coefficient)`: Stores/Adds a new coefficient for the
//              constraint matrix entry (row_index,col_index); where
//              `row_index` is a value previously returned by
//              `FindOrCreateConstraint`, and `col_index` is a value
//              previously returned by `FindOrCreateVariable`.
// - `void SetIsLazy(IndexType row_index)`: Marks 'Constraint[row_index]' as a
//              `lazy constraint`, meaning that the constraint is part of the
//              problem definition, but it might be advantageous to add them
//              as 'cuts' when solving the problem; where `row_index` is a
//              value previously returned by `FindOrCreateConstraint`.
// - `void SetName(const std::string&)`: Stores the model's name.
// - 'void SetObjectiveCoefficient(IndexType index, double coefficient)`:
//              Stores `coefficient` as the new objective coefficient for
//              'Variable[index]'; where `index` is a value previously
//              returned by `FindOrCreateVariable`.
// - `void SetObjectiveDirection(bool maximize)`: If `maximize==true` the
//              parsed model represents a maximization problem, otherwise, or
//              if the function is never called, the model is a minimization
//              problem.
// - `void SetObjectiveOffset(double)`: Stores the objective offset of the
//              model.
// - `void SetVariableTypeToInteger(IndexType index)`: Marks 'Variable[index]'
//              as 'integer'; where `index` is a value previously returned by
//              `FindOrCreateVariable`.
// - `void SetVariableTypeToSemiContinuous(IndexType index)`: Marks
//              'Variable[index]' as 'semi continuous'; where `index` is a
//              value previously returned by `FindOrCreateVariable`.
// - `void SetVariableBounds(IndexType index, double lower_bound, double
//              upper_bound)`: Stores the lower and upper bounds for
//              'Variable[index]'; where `index` is a value previously
//              returned by `FindOrCreateVariable`.
// - `double VariableLowerBound(IndexType index)`: Returns the (currently)
//              stored lower bound for 'Variable[index'; where `index` is a
//              value previously returned by `FindOrCreateVariable`.
// - `double VariableUpperBound(IndexType index)`: Returns the (currently)
//              stored upper bound for 'Variable[index'; where `index` is a
//              value previously returned by `FindOrCreateVariable`.
// - `absl::Status CreateIndicatorConstraint(const std::string& row_name,
//              IndexType col_index, bool var_value)`: Marks constraint named
//              `row_name` to be an 'indicator constraint', that should hold
//              if 'Variable[col_index]' holds value 0 if `var_value`==false,
//              or when 'Variable[col_index]' holds value 1 if
//              `var_value`==true. Where `row_name` must have been an argument
//              in a previous call to `FindOrCreateConstraint`, and
//              `col_index` is a value previously returned by
//              `FindOrCreateVariable`. Note that 'Variable[col_index]' should
//              be marked as integer and have bounds in {0,1}.
template <class DataWrapper>
class MPSReaderTemplate {
 public:
  // Type for row and column indices, as provided by `DataWrapper`.
  using IndexType = typename DataWrapper::IndexType;

  MPSReaderTemplate();

  // Parses a file in MPS format; if successful, returns the type of MPS
  // format detected (one of `kFree` or `kFixed`). If `form` is either `kFixed`
  // or `kFree`, the function will either return `kFixed` (or `kFree`
  // respectivelly) if the input data satisfies the format, or an
  // `absl::InvalidArgumentError` otherwise.
  absl::StatusOr<MPSReaderFormat> ParseFile(
      absl::string_view file_name, DataWrapper* data,
      MPSReaderFormat form = MPSReaderFormat::kAutoDetect);

  // Parses a string in MPS format; if successful, returns the type of MPS
  // format detected (one of `kFree` or `kFixed`). If `form` is either `kFixed`
  // or `kFree`, the function will either return `kFixed` (or `kFree`
  // respectivelly) if the input data satisfies the format, or an
  // `absl::InvalidArgumentError` otherwise.
  absl::StatusOr<MPSReaderFormat> ParseString(
      absl::string_view source, DataWrapper* data,
      MPSReaderFormat form = MPSReaderFormat::kAutoDetect);

 private:
  static constexpr double kInfinity = std::numeric_limits<double>::infinity();

  // Number of fields in one line of MPS file.
  static constexpr int kNumFields = 6;

  // Starting positions of each of the fields for fixed format.
  static constexpr int kFieldStartPos[kNumFields] = {1, 4, 14, 24, 39, 49};

  // Lengths of each of the fields for fixed format.
  static constexpr int kFieldLength[kNumFields] = {2, 8, 8, 12, 8, 12};

  // Positions where there should be spaces for fixed format.
  static constexpr int kSpacePos[12] = {12, 13, 22, 23, 36, 37,
                                        38, 47, 48, 61, 62, 63};

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
  //  If in fixed form, the offset is 0.
  //  If in fixed form and the number of fields is odd, it is 1,
  //  otherwise it is 0.
  // This is useful when processing RANGES and RHS sections.
  int GetFieldOffset() const { return free_form_ ? fields_.size() & 1 : 0; }

  // Line processor.
  absl::Status ProcessLine(absl::string_view line, DataWrapper* data);

  // Process section OBJSENSE in MPS file.
  absl::Status ProcessObjectiveSenseSection(DataWrapper* data);

  // Process section ROWS in the MPS file.
  absl::Status ProcessRowsSection(bool is_lazy, DataWrapper* data);

  // Process section COLUMNS in the MPS file.
  absl::Status ProcessColumnsSection(DataWrapper* data);

  // Process section RHS in the MPS file.
  absl::Status ProcessRhsSection(DataWrapper* data);

  // Process section RANGES in the MPS file.
  absl::Status ProcessRangesSection(DataWrapper* data);

  // Process section BOUNDS in the MPS file.
  absl::Status ProcessBoundsSection(DataWrapper* data);

  // Process section INDICATORS in the MPS file.
  absl::Status ProcessIndicatorsSection(DataWrapper* data);

  // Safely converts a string to a numerical type. Returns an error if the
  // string passed as parameter is ill-formed.
  absl::StatusOr<double> GetDoubleFromString(const std::string& str);
  absl::StatusOr<bool> GetBoolFromString(const std::string& str);

  // Different types of variables, as defined in the MPS file specification.
  // Note these are more precise than the ones in PrimalSimplex.
  enum class BoundTypeId {
    kUnknownBoundType,
    kLowerBound,
    kUpperBound,
    kFixedVariable,
    kFreeVariable,
    kInfiniteLowerBound,
    kInfiniteUpperBound,
    kBinary,
    kSemiContinuous
  };

  // Different types of constraints for a given row.
  enum class RowTypeId {
    kUnknownRowType,
    kEquality,
    kLessThan,
    kGreaterThan,
    kObjective,
    kNone
  };

  // Stores a bound value of a given type, for a given column name.
  absl::Status StoreBound(const std::string& bound_type_mnemonic,
                          const std::string& column_name,
                          const std::string& bound_value, DataWrapper* data);

  // Stores a coefficient value for a column number and a row name.
  absl::Status StoreCoefficient(IndexType col, const std::string& row_name,
                                const std::string& row_value,
                                DataWrapper* data);

  // Stores a right-hand-side value for a row name.
  absl::Status StoreRightHandSide(const std::string& row_name,
                                  const std::string& row_value,
                                  DataWrapper* data);

  // Stores a range constraint of value row_value for a row name.
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
  enum class SectionId {
    kUnknownSection,
    kComment,
    kName,
    kObjsense,
    kRows,
    kLazycons,
    kColumns,
    kRhs,
    kRanges,
    kBounds,
    kIndicators,
    kEndData
  };

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
  IndexType num_unconstrained_rows_;
};

template <class DataWrapper>
absl::StatusOr<MPSReaderFormat> MPSReaderTemplate<DataWrapper>::ParseFile(
    const absl::string_view file_name, DataWrapper* const data,
    const MPSReaderFormat form) {
  if (data == nullptr) {
    return absl::InvalidArgumentError("NULL pointer passed as argument.");
  }

  if (form != MPSReaderFormat::kFree && form != MPSReaderFormat::kFixed) {
    if (ParseFile(file_name, data, MPSReaderFormat::kFixed).ok()) {
      return MPSReaderFormat::kFixed;
    }
    return ParseFile(file_name, data, MPSReaderFormat::kFree);
  }

  DCHECK(form == MPSReaderFormat::kFree || form == MPSReaderFormat::kFixed);
  free_form_ = form == MPSReaderFormat::kFree;
  Reset();
  data->SetUp();
  File* file = nullptr;
  RETURN_IF_ERROR(file::Open(file_name, "r", &file, file::Defaults()));
  for (const absl::string_view line :
       FileLines(file_name, file, FileLineIterator::REMOVE_INLINE_CR)) {
    RETURN_IF_ERROR(ProcessLine(line, data));
  }
  data->CleanUp();
  DisplaySummary();
  return form;
}

template <class DataWrapper>
absl::StatusOr<MPSReaderFormat> MPSReaderTemplate<DataWrapper>::ParseString(
    absl::string_view source, DataWrapper* const data,
    const MPSReaderFormat form) {
  if (form != MPSReaderFormat::kFree && form != MPSReaderFormat::kFixed) {
    if (ParseString(source, data, MPSReaderFormat::kFixed).ok()) {
      return MPSReaderFormat::kFixed;
    }
    return ParseString(source, data, MPSReaderFormat::kFree);
  }

  DCHECK(form == MPSReaderFormat::kFree || form == MPSReaderFormat::kFixed);
  free_form_ = form == MPSReaderFormat::kFree;
  Reset();
  data->SetUp();
  for (absl::string_view line : absl::StrSplit(source, '\n')) {
    RETURN_IF_ERROR(ProcessLine(line, data));
  }
  data->CleanUp();
  DisplaySummary();
  return form;
}

template <class DataWrapper>
absl::Status MPSReaderTemplate<DataWrapper>::ProcessLine(absl::string_view line,
                                                         DataWrapper* data) {
  ++line_num_;
  // Deal with windows end of line characters.
  absl::ConsumeSuffix(&line, "\r");
  line_ = std::string(line);
  if (IsCommentOrBlank()) {
    return absl::OkStatus();  // Skip blank lines and comments.
  }
  if (!free_form_ && absl::StrContains(line_, '\t')) {
    return InvalidArgumentError("File contains tabs.");
  }

  // TODO(b/284163180): Fix handling of sections and data in `free_form`.
  std::string section;
  if (line_[0] != '\0' && line_[0] != ' ') {
    section = GetFirstWord();
    section_ = gtl::FindWithDefault(section_name_to_id_map_, section,
                                    SectionId::kUnknownSection);
    if (section_ == SectionId::kUnknownSection) {
      return InvalidArgumentError("Unknown section.");
    }
    if (section_ == SectionId::kComment) {
      return absl::OkStatus();
    }
    if (section_ == SectionId::kObjsense) {
      return absl::OkStatus();
    }
    if (section_ == SectionId::kName) {
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
    case SectionId::kName:
      return InvalidArgumentError("Second NAME field.");
    case SectionId::kObjsense:
      return ProcessObjectiveSenseSection(data);
    case SectionId::kRows:
      return ProcessRowsSection(/*is_lazy=*/false, data);
    case SectionId::kLazycons:
      return ProcessRowsSection(/*is_lazy=*/true, data);
    case SectionId::kColumns:
      return ProcessColumnsSection(data);
    case SectionId::kRhs:
      return ProcessRhsSection(data);
    case SectionId::kRanges:
      return ProcessRangesSection(data);
    case SectionId::kBounds:
      return ProcessBoundsSection(data);
    case SectionId::kIndicators:
      return ProcessIndicatorsSection(data);
    case SectionId::kEndData:  // Do nothing.
      break;
    default:
      return InvalidArgumentError("Unknown section.");
  }
  return absl::OkStatus();
}

template <class DataWrapper>
absl::Status MPSReaderTemplate<DataWrapper>::ProcessObjectiveSenseSection(
    DataWrapper* data) {
  if (fields_.size() != 1 && fields_[0] != "MIN" && fields_[0] != "MAX") {
    return InvalidArgumentError("Expected objective sense (MAX or MIN).");
  }
  data->SetObjectiveDirection(/*maximize=*/fields_[0] == "MAX");
  return absl::OkStatus();
}

template <class DataWrapper>
absl::Status MPSReaderTemplate<DataWrapper>::ProcessRowsSection(
    bool is_lazy, DataWrapper* data) {
  if (fields_.size() < 2) {
    return InvalidArgumentError("Not enough fields in ROWS section.");
  }
  const std::string row_type_name = fields_[0];
  const std::string row_name = fields_[1];
  RowTypeId row_type = gtl::FindWithDefault(row_name_to_id_map_, row_type_name,
                                            RowTypeId::kUnknownRowType);
  if (row_type == RowTypeId::kUnknownRowType) {
    return InvalidArgumentError("Unknown row type.");
  }

  // The first NONE constraint is used as the objective.
  if (objective_name_.empty() && row_type == RowTypeId::kNone) {
    row_type = RowTypeId::kObjective;
    objective_name_ = row_name;
  } else {
    if (row_type == RowTypeId::kNone) {
      ++num_unconstrained_rows_;
    }
    const IndexType row = data->FindOrCreateConstraint(row_name);
    if (is_lazy) data->SetIsLazy(row);

    // The initial row range is [0, 0]. We encode the type in the range by
    // setting one of the bounds to +/- infinity.
    switch (row_type) {
      case RowTypeId::kLessThan:
        data->SetConstraintBounds(row, -kInfinity,
                                  data->ConstraintUpperBound(row));
        break;
      case RowTypeId::kGreaterThan:
        data->SetConstraintBounds(row, data->ConstraintLowerBound(row),
                                  kInfinity);
        break;
      case RowTypeId::kNone:
        data->SetConstraintBounds(row, -kInfinity, kInfinity);
        break;
      case RowTypeId::kEquality:
      default:
        break;
    }
  }
  return absl::OkStatus();
}

template <class DataWrapper>
absl::Status MPSReaderTemplate<DataWrapper>::ProcessColumnsSection(
    DataWrapper* data) {
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
  const IndexType col = data->FindOrCreateVariable(column_name);
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
absl::Status MPSReaderTemplate<DataWrapper>::ProcessRhsSection(
    DataWrapper* data) {
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
absl::Status MPSReaderTemplate<DataWrapper>::ProcessRangesSection(
    DataWrapper* data) {
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
absl::Status MPSReaderTemplate<DataWrapper>::ProcessBoundsSection(
    DataWrapper* data) {
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
absl::Status MPSReaderTemplate<DataWrapper>::ProcessIndicatorsSection(
    DataWrapper* data) {
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

  const IndexType col = data->FindOrCreateVariable(column_name);
  // Variables used in indicator constraints become Boolean by default.
  data->SetVariableTypeToInteger(col);
  data->SetVariableBounds(col, std::max(0.0, data->VariableLowerBound(col)),
                          std::min(1.0, data->VariableUpperBound(col)));

  RETURN_IF_ERROR(
      AppendLineToError(data->CreateIndicatorConstraint(row_name, col, value)));

  return absl::OkStatus();
}

template <class DataWrapper>
absl::Status MPSReaderTemplate<DataWrapper>::StoreCoefficient(
    IndexType col, const std::string& row_name, const std::string& row_value,
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
    const IndexType row = data->FindOrCreateConstraint(row_name);
    data->SetConstraintCoefficient(row, col, value);
  }
  return absl::OkStatus();
}

template <class DataWrapper>
absl::Status MPSReaderTemplate<DataWrapper>::StoreRightHandSide(
    const std::string& row_name, const std::string& row_value,
    DataWrapper* data) {
  if (row_name.empty()) return absl::OkStatus();

  if (row_name != objective_name_) {
    const IndexType row = data->FindOrCreateConstraint(row_name);
    double value;
    ASSIGN_OR_RETURN(value, GetDoubleFromString(row_value));

    // The row type is encoded in the bounds, so at this point we have either
    // (-kInfinity, 0.0], [0.0, 0.0] or [0.0, kInfinity). We use the right
    // hand side to change any finite bound.
    const double lower_bound =
        (data->ConstraintLowerBound(row) == -kInfinity) ? -kInfinity : value;
    const double upper_bound =
        (data->ConstraintUpperBound(row) == kInfinity) ? kInfinity : value;
    data->SetConstraintBounds(row, lower_bound, upper_bound);
  } else {
    // We treat minus the right hand side of COST as the objective offset, in
    // line with what the MPS writer does and what Gurobi's MPS format
    // expects.
    double value;
    ASSIGN_OR_RETURN(value, GetDoubleFromString(row_value));
    data->SetObjectiveOffset(-value);
  }
  return absl::OkStatus();
}

template <class DataWrapper>
absl::Status MPSReaderTemplate<DataWrapper>::StoreRange(
    const std::string& row_name, const std::string& range_value,
    DataWrapper* data) {
  if (row_name.empty()) return absl::OkStatus();

  const IndexType row = data->FindOrCreateConstraint(row_name);
  double range;
  ASSIGN_OR_RETURN(range, GetDoubleFromString(range_value));

  double lower_bound = data->ConstraintLowerBound(row);
  double upper_bound = data->ConstraintUpperBound(row);
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
absl::Status MPSReaderTemplate<DataWrapper>::StoreBound(
    const std::string& bound_type_mnemonic, const std::string& column_name,
    const std::string& bound_value, DataWrapper* data) {
  const BoundTypeId bound_type_id =
      gtl::FindWithDefault(bound_name_to_id_map_, bound_type_mnemonic,
                           BoundTypeId::kUnknownBoundType);
  if (bound_type_id == BoundTypeId::kUnknownBoundType) {
    return InvalidArgumentError("Unknown bound type.");
  }
  const IndexType col = data->FindOrCreateVariable(column_name);
  if (integer_type_names_set_.count(bound_type_mnemonic) != 0) {
    data->SetVariableTypeToInteger(col);
  }
  if (is_binary_by_default_.size() <= col) {
    // This is the first time that this column has been encountered.
    is_binary_by_default_.resize(col + 1, false);
  }
  double lower_bound = data->VariableLowerBound(col);
  double upper_bound = data->VariableUpperBound(col);
  // If a variable is binary by default, its status is reset if any bound
  // is set on it. We take care to restore the default bounds for general
  // integer variables.
  if (is_binary_by_default_[col]) {
    lower_bound = 0.0;
    upper_bound = kInfinity;
  }
  switch (bound_type_id) {
    case BoundTypeId::kLowerBound: {
      ASSIGN_OR_RETURN(lower_bound, GetDoubleFromString(bound_value));
      // TODO(b/285121446): Decide to keep or remove this corner case behavior.
      // LI with the value 0.0 specifies general integers with no upper bound.
      if (bound_type_mnemonic == "LI" && lower_bound == 0.0) {
        upper_bound = kInfinity;
      }
      break;
    }
    case BoundTypeId::kUpperBound: {
      ASSIGN_OR_RETURN(upper_bound, GetDoubleFromString(bound_value));
      break;
    }
    case BoundTypeId::kSemiContinuous: {
      ASSIGN_OR_RETURN(upper_bound, GetDoubleFromString(bound_value));
      data->SetVariableTypeToSemiContinuous(col);
      break;
    }
    case BoundTypeId::kFixedVariable: {
      ASSIGN_OR_RETURN(lower_bound, GetDoubleFromString(bound_value));
      upper_bound = lower_bound;
      break;
    }
    case BoundTypeId::kFreeVariable:
      lower_bound = -kInfinity;
      upper_bound = +kInfinity;
      break;
    case BoundTypeId::kInfiniteLowerBound:
      lower_bound = -kInfinity;
      break;
    case BoundTypeId::kInfiniteUpperBound:
      upper_bound = +kInfinity;
      break;
    case BoundTypeId::kBinary:
      lower_bound = 0.0;
      upper_bound = 1.0;
      break;
    case BoundTypeId::kUnknownBoundType:
    default:
      return InvalidArgumentError("Unknown bound type.");
  }
  is_binary_by_default_[col] = false;
  data->SetVariableBounds(col, lower_bound, upper_bound);
  return absl::OkStatus();
}

template <class DataWrapper>
MPSReaderTemplate<DataWrapper>::MPSReaderTemplate()
    : free_form_(true),
      fields_(kNumFields),
      section_(SectionId::kUnknownSection),
      section_name_to_id_map_(),
      row_name_to_id_map_(),
      bound_name_to_id_map_(),
      integer_type_names_set_(),
      line_num_(0),
      line_(),
      in_integer_section_(false),
      num_unconstrained_rows_(0) {
  section_name_to_id_map_["NAME"] = SectionId::kName;
  section_name_to_id_map_["OBJSENSE"] = SectionId::kObjsense;
  section_name_to_id_map_["ROWS"] = SectionId::kRows;
  section_name_to_id_map_["LAZYCONS"] = SectionId::kLazycons;
  section_name_to_id_map_["COLUMNS"] = SectionId::kColumns;
  section_name_to_id_map_["RHS"] = SectionId::kRhs;
  section_name_to_id_map_["RANGES"] = SectionId::kRanges;
  section_name_to_id_map_["BOUNDS"] = SectionId::kBounds;
  section_name_to_id_map_["INDICATORS"] = SectionId::kIndicators;
  section_name_to_id_map_["ENDATA"] = SectionId::kEndData;
  row_name_to_id_map_["E"] = RowTypeId::kEquality;
  row_name_to_id_map_["L"] = RowTypeId::kLessThan;
  row_name_to_id_map_["G"] = RowTypeId::kGreaterThan;
  row_name_to_id_map_["N"] = RowTypeId::kNone;
  bound_name_to_id_map_["LO"] = BoundTypeId::kLowerBound;
  bound_name_to_id_map_["UP"] = BoundTypeId::kUpperBound;
  bound_name_to_id_map_["FX"] = BoundTypeId::kFixedVariable;
  bound_name_to_id_map_["FR"] = BoundTypeId::kFreeVariable;
  bound_name_to_id_map_["MI"] = BoundTypeId::kInfiniteLowerBound;
  bound_name_to_id_map_["PL"] = BoundTypeId::kInfiniteUpperBound;
  bound_name_to_id_map_["BV"] = BoundTypeId::kBinary;
  bound_name_to_id_map_["LI"] = BoundTypeId::kLowerBound;
  bound_name_to_id_map_["UI"] = BoundTypeId::kUpperBound;
  bound_name_to_id_map_["SC"] = BoundTypeId::kSemiContinuous;
  // TODO(user): Support 'SI' (semi integer).
  integer_type_names_set_.insert("BV");
  integer_type_names_set_.insert("LI");
  integer_type_names_set_.insert("UI");
}

template <class DataWrapper>
void MPSReaderTemplate<DataWrapper>::Reset() {
  fields_.resize(kNumFields);
  line_num_ = 0;
  in_integer_section_ = false;
  num_unconstrained_rows_ = 0;
  objective_name_.clear();
}

template <class DataWrapper>
void MPSReaderTemplate<DataWrapper>::DisplaySummary() {
  if (num_unconstrained_rows_ > 0) {
    VLOG(1) << "There are " << num_unconstrained_rows_ + 1
            << " unconstrained rows. The first of them (" << objective_name_
            << ") was used as the objective.";
  }
}

template <class DataWrapper>
bool MPSReaderTemplate<DataWrapper>::IsFixedFormat() {
  for (const int i : kSpacePos) {
    if (i >= line_.length()) break;
    if (line_[i] != ' ') return false;
  }
  return true;
}

template <class DataWrapper>
absl::Status MPSReaderTemplate<DataWrapper>::SplitLineIntoFields() {
  if (free_form_) {
    fields_ = absl::StrSplit(line_, absl::ByAnyChar(" \t"), absl::SkipEmpty());
    if (fields_.size() > kNumFields) {
      return InvalidArgumentError("Found too many fields.");
    }
  } else {
    // Note: the name should also comply with the fixed format guidelines
    // (maximum 8 characters) but in practice there are many problem files in
    // the netlib archive that are in fixed format and have a long name. We
    // choose to ignore these cases and treat them as fixed format anyway.
    if (section_ != SectionId::kName && !IsFixedFormat()) {
      return InvalidArgumentError("Line is not in fixed format.");
    }
    const int length = line_.length();
    // Several parts of the code assume that `fields_` has the right number of
    // fields detected.
    fields_.clear();
    for (int i = 0; i < kNumFields; ++i) {
      if (kFieldStartPos[i] < length) {
        fields_.push_back(line_.substr(kFieldStartPos[i], kFieldLength[i]));
        fields_.back().erase(fields_.back().find_last_not_of(" ") + 1);
      }
    }
  }
  return absl::OkStatus();
}

template <class DataWrapper>
std::string MPSReaderTemplate<DataWrapper>::GetFirstWord() const {
  if (line_[0] == ' ') {
    return std::string("");
  }
  const int first_space_pos = line_.find(' ');
  const std::string first_word = line_.substr(0, first_space_pos);
  return first_word;
}

template <class DataWrapper>
bool MPSReaderTemplate<DataWrapper>::IsCommentOrBlank() const {
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

template <class DataWrapper>
absl::StatusOr<double> MPSReaderTemplate<DataWrapper>::GetDoubleFromString(
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

template <class DataWrapper>
absl::StatusOr<bool> MPSReaderTemplate<DataWrapper>::GetBoolFromString(
    const std::string& str) {
  int result;
  if (!absl::SimpleAtoi(str, &result) || result < 0 || result > 1) {
    return InvalidArgumentError(
        absl::StrCat("Failed to convert \"", str, "\" to bool."));
  }
  return result;
}

template <class DataWrapper>
absl::Status MPSReaderTemplate<DataWrapper>::InvalidArgumentError(
    const std::string& error_message) {
  return AppendLineToError(absl::InvalidArgumentError(error_message));
}

template <class DataWrapper>
absl::Status MPSReaderTemplate<DataWrapper>::AppendLineToError(
    const absl::Status& status) {
  return util::StatusBuilder(status).SetAppend()
         << " Line " << line_num_ << ": \"" << line_ << "\".";
}

}  //  namespace operations_research
#endif  // OR_TOOLS_LP_DATA_MPS_READER_TEMPLATE_H_
