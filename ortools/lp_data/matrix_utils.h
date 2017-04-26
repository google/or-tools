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

#ifndef OR_TOOLS_LP_DATA_MATRIX_UTILS_H_
#define OR_TOOLS_LP_DATA_MATRIX_UTILS_H_

#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/sparse.h"

namespace operations_research {
namespace glop {

// Finds the proportional columns of the given matrix: all the pairs of columns
// such that one is a non-zero scalar multiple of the other. The returned
// mapping for a given column will either be:
//  - The index of the first column which is proportional with this one.
//  - Or kInvalidCol if this column is not proportional to any other.
//
// Because of precision errors, two columns are declared proportional if the
// multiples (>= 1.0) defined by all the couple of coefficients of the same row
// are equal up to the given absolute tolerance.
//
// The complexity is in most cases O(num entries of the matrix). However,
// compared to the less efficient algorithm below, it is highly unlikely but
// possible that some pairs of proportional columns are not detected.
ColMapping FindProportionalColumns(const SparseMatrix& matrix,
                                   Fractional tolerance);

// A simple version of FindProportionalColumns() that compares all the columns
// pairs one by one. This is slow, but here for reference. The complexity is
// O(num_cols * num_entries).
ColMapping FindProportionalColumnsUsingSimpleAlgorithm(
    const SparseMatrix& matrix, Fractional tolerance);

// Returns true iff the two given matrices have exactly the same first num_rows
// entries on the first num_cols columns. The two given matrices must be ordered
// by rows (this is DCHECKed, but only for the first one at this point).
bool AreFirstColumnsAndRowsExactlyEquals(RowIndex num_rows, ColIndex num_cols,
                                         const SparseMatrix& matrix_a,
                                         const CompactSparseMatrix& matrix_b);

// Returns true iff the rightmost square matrix is an identity matrix.
bool IsRightMostSquareMatrixIdentity(const SparseMatrix& matrix);

}  // namespace glop
}  // namespace operations_research

#endif  // OR_TOOLS_LP_DATA_MATRIX_UTILS_H_
