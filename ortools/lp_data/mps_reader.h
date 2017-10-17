// Copyright 2010-2017 Google
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


// A reader for files in the MPS format.
// see http://lpsolve.sourceforge.net/5.5/mps-format.htm
// and http://www.ici.ro/camo/language/ml11.htm.
//
// MPS stands for Mathematical Programming System.
//
// The format was invented by IBM in the 60's, and has become the de facto
// standard. We developed this reader to be able to read benchmark data
// files. Using the MPS file format for new models is discouraged.

#ifndef OR_TOOLS_LP_DATA_MPS_READER_H_
#define OR_TOOLS_LP_DATA_MPS_READER_H_

#include <algorithm>  // for max
#include <unordered_map>
#include <string>  // for std::string
#include <vector>  // for vector

#include "ortools/base/macros.h"  // for DISALLOW_COPY_AND_ASSIGN, NULL
#include "ortools/base/stringprintf.h"
#include "ortools/base/int_type.h"
#include "ortools/base/int_type_indexed_vector.h"
#include "ortools/base/map_util.h"  // for FindOrNull, FindWithDefault
#include "ortools/base/hash.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_types.h"

namespace operations_research {
namespace glop {

// Different types of constraints for a given row.
typedef enum {
  UNKNOWN_ROW_TYPE,
  EQUALITY,
  LESS_THAN,
  GREATER_THAN,
  OBJECTIVE,
  NONE
} MPSRowType;

// Reads a linear program in the mps format.
//
// All Load() methods clear the previously loaded instance and store the result
// in the given LinearProgram. They return false in case of failure to read the
// instance.
class MPSReader {
 public:
  MPSReader();

  // Loads instance from a file.
  bool LoadFile(const std::string& file_name, LinearProgram* data);

  // Loads instance from a file, specifying if free or fixed format is used.
  bool LoadFileWithMode(const std::string& source, bool free_form,
                        LinearProgram* data);

  // Same as load file, but try free form mps on fail.
  bool LoadFileAndTryFreeFormOnFail(const std::string& file_name,
                                    LinearProgram* data);

  // Returns the name of the last loaded problem as defined in the NAME line.
  std::string GetProblemName() const;

  // See log_errors_ (the default is true).
  void set_log_errors(bool v) { log_errors_ = v; }

 private:
  // Number of fields in one line of MPS file
  static const int kNumFields;

  // Starting positions of each of the fields.
  static const int kFieldStartPos[];

  // Lengths of each of the fields.
  static const int kFieldLength[];

  // Resets the object to its initial value before reading a new file.
  void Reset();

  // Displays some information on the last loaded file.
  void DisplaySummary();

  // Get each field for a given line.
  void SplitLineIntoFields();

  // Get the first word in a line.
  std::string GetFirstWord() const;

  // Returns true if the line contains a comment (starting with '*') or
  // if it it is a blank line.
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
  void ProcessLine(char* line);

  // Process section NAME in the MPS file.
  void ProcessNameSection();

  // Process section ROWS in the MPS file.
  void ProcessRowsSection();

  // Process section COLUMNS in the MPS file.
  void ProcessColumnsSection();

  // Process section RHS in the MPS file.
  void ProcessRhsSection();

  // Process section RANGES in the MPS file.
  void ProcessRangesSection();

  // Process section BOUNDS in the MPS file.
  void ProcessBoundsSection();

  // Process section SOS in the MPS file.
  void ProcessSosSection();

  // Safely converts a std::string to a double. Possibly sets parse_success_ to
  // false if the std::string passed as parameter is ill-formed.
  double GetDoubleFromString(const std::string& param);

  // Different types of variables, as defined in the MPS file specification.
  // Note these are more precise than the ones in PrimalSimplex.
  typedef enum {
    UNKNOWN_BOUND_TYPE,
    LOWER_BOUND,
    UPPER_BOUND,
    FIXED_VARIABLE,
    FREE_VARIABLE,
    NEGATIVE,
    POSITIVE,
    BINARY
  } BoundTypeId;

  // Stores a bound value of a given type, for a given column name.
  void StoreBound(const std::string& bound_type_mnemonic, const std::string& column_name,
                  const std::string& bound_value);

  // Stores a coefficient value for a column number and a row name.
  void StoreCoefficient(ColIndex col, const std::string& row_name,
                        const std::string& row_value);

  // Stores a right-hand-side value for a row name.
  void StoreRightHandSide(const std::string& row_name, const std::string& row_value);

  // Stores a range constraint of value row_value for a row name.
  void StoreRange(const std::string& row_name, const std::string& row_value);

  // Boolean set to true if the reader expects a free-form MPS file.
  bool free_form_;

  LinearProgram* data_;

  // The name of the problem as defined on the NAME line in the MPS file.
  std::string problem_name_;

  // True if the parsing was successful.
  bool parse_success_;

  // Storage of the fields for a line of the MPS file.
  std::vector<std::string> fields_;

  // Stores the name of the objective row.
  std::string objective_name_;

  // Enum for section ids.
  typedef enum {
    UNKNOWN_SECTION,
    COMMENT,
    NAME,
    ROWS,
    LAZYCONS,
    COLUMNS,
    RHS,
    RANGES,
    BOUNDS,
    SOS,
    ENDATA
  } SectionId;

  // Id of the current section of MPS file.
  SectionId section_;

  // Maps section mnemonic --> section id.
  std::unordered_map<std::string, SectionId> section_name_to_id_map_;

  // Maps row type mnemonic --> row type id.
  std::unordered_map<std::string, MPSRowType> row_name_to_id_map_;

  // Maps bound type mnemonic --> bound type id.
  std::unordered_map<std::string, BoundTypeId> bound_name_to_id_map_;

  // Set of bound type mnemonics that constrain variables to be integer.
  std::unordered_set<std::string> integer_type_names_set_;

  // The current line number in the file being parsed.
  int64 line_num_;

  // The current line in the file being parsed.
  std::string line_;

  // A row of Booleans. is_binary_by_default_[col] is true if col
  // appeared within a scope started by INTORG and ended with INTEND markers.
  DenseBooleanRow is_binary_by_default_;

  // True if the problem contains lazy constraints (LAZYCONS).
  bool has_lazy_constraints_;

  // True if the next variable has to be interpreted as an integer variable.
  // This is used to support the marker INTORG that starts an integer section
  // and INTEND that ends it.
  bool in_integer_section_;

  // We keep track of the number of unconstrained rows so we can display it to
  // the user because other solvers usually ignore them and we don't (they will
  // be removed in the preprocessor).
  int num_unconstrained_rows_;

  // Whether to log errors to LOG(ERROR) or not. Sometimes we just want to use
  // this class to detect a valid .mps file, and encountering errors is
  // expected.
  bool log_errors_;

  DISALLOW_COPY_AND_ASSIGN(MPSReader);
};

}  // namespace glop
}  // namespace operations_research

#endif  // OR_TOOLS_LP_DATA_MPS_READER_H_
