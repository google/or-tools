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

#ifndef OR_TOOLS_ALGORITHMS_SET_COVER_READER_H_
#define OR_TOOLS_ALGORITHMS_SET_COVER_READER_H_

#include "absl/strings/string_view.h"
#include "ortools/algorithms/set_cover_model.h"

namespace operations_research {

// Readers for set covering problems at
// http://people.brunel.ac.uk/~mastjjb/jeb/orlib/scpinfo.html
// All the instances have either the Beasley or the rail format.

// There is currently NO error handling, as the files are in a limited number.
// TODO(user): add proper error handling.

// Also, note that the indices in the files, when mentioned, start from 1, while
// SetCoverModel starts from 0, The translation is done at read time.

// Reads a rail set cover problem create by Beasley and returns a SetCoverModel.
// The format of all of these 80 data files is:
// number of rows (m), number of columns (n)
// for each column j, (j=1,...,n): the cost of the column c(j)
// for each row i (i=1,...,m): the number of columns which cover
// row i followed by a list of the columns which cover row i
SetCoverModel ReadBeasleySetCoverProblem(absl::string_view filename);

// Reads a rail set cover problem and returns a SetCoverModel.
// The format of these test problems is:
// number of rows (m), number of columns (n)
// for each column j (j=1,...,n): the cost of the column, the
// number of rows that it covers followed by a list of the rows
// that it covers.
SetCoverModel ReadRailSetCoverProblem(absl::string_view filename);

}  // namespace operations_research

#endif  // OR_TOOLS_ALGORITHMS_SET_COVER_READER_H_
