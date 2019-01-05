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

#ifndef OR_TOOLS_DATA_SET_COVERING_PARSER_H_
#define OR_TOOLS_DATA_SET_COVERING_PARSER_H_

#include <string>
#include <vector>

#include "ortools/base/integral_types.h"
#include "ortools/data/set_covering_data.h"

namespace operations_research {
namespace scp {

// Set covering problem.
//
// We have a list of subsets of a set. Each subset has a cost.  The
// goal is to select of solution set of subsets such that (1) all elements
// of the set belongs to at least one subset of the solution set, and (2)
// the sum of the cost of each subset in the solution set is minimal.
//
// To follow the standard literature, each element is called a row, and each
// subset is called a column.

class ScpParser {
 public:
  enum Section {
    INIT,
    COSTS,
    COLUMN,
    NUM_COLUMNS_IN_ROW,
    ROW,
    NUM_NON_ZEROS,
    END,
    ERROR,
  };

  enum Format {
    // The original scp format of these problem is:
    //
    // number of rows (m), number of columns (n)
    //
    // the cost of each column c(j),j=1,...,n
    //
    // for each row i (i=1,...,m): the number of columns which cover row
    // i followed by a list of the columns which cover row i.
    //
    // The original problems (scp*) from the OR-LIB follow this format.
    SCP_FORMAT,
    // The railroad format is:
    //   number of rows (m), number of columns (n)
    //
    //   for each column j (j=1,...,n): the cost of the column, the number
    //   of rows that it covers followed by a list of the rows that it
    //   covers.
    //
    // The railroad problems follow this format.
    RAILROAD_FORMAT,
    // The triplet format is:
    //
    // number of rows (m), number of columns (n)
    //
    // for each column, the 3 rows it contains.  Note that the cost of
    // each column is 1.
    //
    // The Steiner triple covering problems follow this format.
    TRIPLET_FORMAT,
    // The spp format is:
    //   number of rows (m), number of columns (n)
    //
    //   for each column j (j=1,...,n): the cost of the column, the number
    //   of rows that it covers followed by a list of the rows that it
    //   covers.
    //
    //   number of non_zeros
    //
    // The set partitioning problems follow this format.
    SPP_FORMAT
  };

  ScpParser();

  // This will clear the data before importing the file.
  bool LoadProblem(const std::string& filename, Format format, ScpData* data);

 private:
  void ProcessLine(const std::string& line, Format format, ScpData* data);
  void LogError(const std::string& line, const std::string& error_message);
  int strtoint32(const std::string& word);
  int64 strtoint64(const std::string& word);

  Section section_;
  int line_;
  int remaining_;
  int current_;
};

}  // namespace scp
}  // namespace operations_research

#endif  // OR_TOOLS_DATA_SET_COVERING_PARSER_H_
