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

#ifndef ORTOOLS_SET_COVER_SET_COVER_READER_H_
#define ORTOOLS_SET_COVER_SET_COVER_READER_H_

#include "absl/strings/string_view.h"
#include "ortools/set_cover/base_types.h"
#include "ortools/set_cover/set_cover_model.h"

namespace operations_research {

// Readers for set covering problems at
// http://people.brunel.ac.uk/~mastjjb/jeb/orlib/scpinfo.html
// All the instances have either the Beasley or the rail format.

// There is currently NO error handling, as the files are in a limited number.
// TODO(user): add proper error handling.

// Also, note that the indices in the files, when mentioned, start from 1, while
// SetCoverModel starts from 0, The translation is done at read time.

enum class SetCoverFormat {
  EMPTY,      // returned by ParseFileFormat when the format is not recognized.
  ORLIB,      // when "orlib" is passed to ParseFileFormat.
  RAIL,       // when "rail" is passed to ParseFileFormat.
  FIMI,       // when "fimi" is passed to ParseFileFormat,
  PROTO,      // when "proto" is passed to ParseFileFormat.proto.
  PROTO_BIN,  // when "proto_bin" is passed to ParseFileFormat.
  TXT,        // when "txt" is passed to ParseFileFormat (for solutions only).
};

// Parses a string into a SetCoverFormat. Returns EMPTY if the format is not
// recognized. The string is case insensitive.
SetCoverFormat ParseFileFormat(absl::string_view format_name);

// Reads a set cover problem from a file. The format is specified by the
// SetCoverFormat. The valid formats are ORLIB, RAIL, FIMI, PROTO, PROTO_BIN.
SetCoverModel ReadModel(absl::string_view filename, SetCoverFormat format);

// Shortcut passing a string instead of a SetCoverFormat.
inline SetCoverModel ReadModel(absl::string_view filename,
                               absl::string_view format) {
  return ReadModel(filename, ParseFileFormat(format));
}

// Reads a set cover solution from a file. The format is specified by the
// SetCoverFormat. The valid formats are TXT, PROTO, PROTO_BIN.
SubsetBoolVector ReadSolution(absl::string_view filename,
                              SetCoverFormat format);

// Shortcut passing a string instead of a SetCoverFormat.
inline SubsetBoolVector ReadSolution(absl::string_view filename,
                                     absl::string_view format) {
  return ReadSolution(filename, ParseFileFormat(format));
}

// Writes a set cover problem to a file. The format is specified by the
// SetCoverFormat. The valid formats are ORLIB, RAIL, FIMI, PROTO, PROTO_BIN.
void WriteModel(const SetCoverModel& model, absl::string_view filename,
                SetCoverFormat format);

// Shortcut passing a string instead of a SetCoverFormat.
inline void WriteModel(const SetCoverModel& model, absl::string_view filename,
                       absl::string_view format) {
  WriteModel(model, filename, ParseFileFormat(format));
}

// Writes a set cover solution to a file. The format is specified by the
// SetCoverFormat. The valid formats are TXT, PROTO, PROTO_BIN.
void WriteSolution(const SetCoverModel& model, const SubsetBoolVector& solution,
                   absl::string_view filename, SetCoverFormat format);

// Shortcut passing a string instead of a SetCoverFormat.
inline void WriteSolution(const SetCoverModel& model,
                          const SubsetBoolVector& solution,
                          absl::string_view filename,
                          absl::string_view format) {
  WriteSolution(model, solution, filename, ParseFileFormat(format));
}

// Reads a rail set cover problem create by Beasley and returns a SetCoverModel.
// The format of all of these 80 data files is:
// number of rows (m), number of columns (n)
// for each column j, (j=1,...,n): the cost of the column c(j)
// for each row i (i=1,...,m): the number of columns which cover
// row i followed by a list of the columns which cover row i
// The columns and rows are 1-indexed with this file format.
// The translation to 0-indexing is done at read time.
SetCoverModel ReadOrlibScp(absl::string_view filename);

// Reads a rail set cover problem and returns a SetCoverModel.
// The format of these test problems is:
// number of rows (m), number of columns (n)
// for each column j (j=1,...,n): the cost of the column, the
// number of rows that it covers followed by a list of the rows
// that it covers.
// The columns and rows are 1-indexed with this file format.
// The translation to 0-indexing is done at read time.
SetCoverModel ReadOrlibRail(absl::string_view filename);

// Reads a file in the FIMI / .dat file format. FIMI stands for "Frequent
// Itemset Mining Implementations".
// The file is given column-by-column, with each column containing a space-
// separated list of elements terminating with a newline. The elements are
// 0-indexed.
// The cost of each subset is 1.
SetCoverModel ReadFimiDat(absl::string_view filename);

// Reads a set cover problem from a SetCoverProto.
// The proto is either read from a binary (if binary is true) or a text file.
SetCoverModel ReadSetCoverProto(absl::string_view filename, bool binary);

// Writers for the Beasley and Rail formats.
// The translation of indices from 0 to 1-indexing is done at write time.
void WriteOrlibScp(const SetCoverModel& model, absl::string_view filename);
void WriteOrlibRail(const SetCoverModel& model, absl::string_view filename);

// Writes a set cover problem to a SetCoverProto.
// The proto is either written to a binary (if binary is true) or a text file.
// The model is modified (its columns are sorted) in-place when the proto is
// generated.
void WriteSetCoverProto(const SetCoverModel& model, absl::string_view filename,
                        bool binary);

// Reads a set cover solution from a text file.
// The format of the file is:
// number of columns (n)
// number of selected columns (k)
// for each i (j=1,...,k): 1 if column[i] is selected, 0 otherwise.
// The solution is 0-indexed.
SubsetBoolVector ReadSetCoverSolutionText(absl::string_view filename);

// Reads a set cover solution from a SetCoverSolutionResponse proto.
// The proto is either read from a binary (if binary is true) or a text file.
// The solution is 0-indexed.
SubsetBoolVector ReadSetCoverSolutionProto(absl::string_view filename,
                                           bool binary);

// Writes a set cover solution to a text file.
// The format of the file is:
// number of columns (n)
// number of selected columns (k)
// for each i (j=1,...,k): 1 if column[i] is selected, 0 otherwise.
// The solution is 0-indexed.
void WriteSetCoverSolutionText(const SetCoverModel& model,
                               const SubsetBoolVector& solution,
                               absl::string_view filename);

// Writes a set cover solution to a SetCoverSolutionResponse proto.
// The proto is either written to a binary (if binary is true) or a text file.
// The solution is 0-indexed.
void WriteSetCoverSolutionProto(const SetCoverModel& model,
                                const SubsetBoolVector& solution,
                                absl::string_view filename, bool binary);

}  // namespace operations_research

#endif  // ORTOOLS_SET_COVER_SET_COVER_READER_H_
