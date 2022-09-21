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

// Tools to extract some sub-components of sparse matrices.
#ifndef OR_TOOLS_MATH_OPT_CORE_SPARSE_SUBMATRIX_H_
#define OR_TOOLS_MATH_OPT_CORE_SPARSE_SUBMATRIX_H_

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "ortools/math_opt/core/sparse_vector.h"
#include "ortools/math_opt/core/sparse_vector_view.h"
#include "ortools/math_opt/sparse_containers.pb.h"

namespace operations_research::math_opt {

// A vector that contains one pair (row_id, columns_coefficients) per row,
// sorted by row_id. The columns_coefficients are views.
using SparseSubmatrixRowsView =
    std::vector<std::pair<int64_t, SparseVectorView<double>>>;

// Returns the coefficients of columns in the range [start_col_id, end_col_id)
// for each row in the range [start_row_id, end_row_id).
//
// Returns a vector that contains one pair (row_id, columns_coefficients) per
// row. It CHECKs that the input matrix is valid. The coefficients are returned
// in a views that points to the input matrix's data. Therefore they should not
// be used after the proto is modified/deleted.
//
// When end_(col|row)_id is nullopt, includes all indices greater or equal to
// start_(col|row)_id.
//
// This functions runs in O(size of matrix).
//
// Use TransposeSparseSubmatrix() to transpose the submatrix and get the
// columns instead of the rows.
//
// Usage example:
//
//   // With this input sparse matrix:
//   //  |0 1 2 3 4 5 6
//   // -+-------------
//   // 0|2 - - - 3 4 -
//   // 1|- - - - - - -
//   // 2|- 5 - 1 - - 3
//   // 3|9 - - 8 - - 7
//   const SparseDoubleMatrixProto matrix = ...;
//
//   // Keeping coefficients of lines >= 1 and columns in [1, 6).
//   const auto rows = SparseSubmatrixByRows(
//       matrix,
//       /*start_row_id=*/1, /*end_row_id=*/std::nullopt,
//       /*start_col_id=*/1, /*end_col_id=*/6);
//
//   // The returned rows and coefficients will be:
//   //   {2, {{1, 5.0}, {3, 1.0}}}
//   //   {3, {          {3, 8.0}}}
//
SparseSubmatrixRowsView SparseSubmatrixByRows(
    const SparseDoubleMatrixProto& matrix, int64_t start_row_id,
    std::optional<int64_t> end_row_id, int64_t start_col_id,
    std::optional<int64_t> end_col_id);

// Returns a vector that contains one pair (row_id, rows_coefficients) per
// column.
//
// The coefficients are returned as copies of the input views.
//
// This functions runs in:
//   O(num_non_zeros + num_non_empty_cols * lg(num_non_empty_cols)).
std::vector<std::pair<int64_t, SparseVector<double>>> TransposeSparseSubmatrix(
    const SparseSubmatrixRowsView& submatrix_by_rows);

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_CORE_SPARSE_SUBMATRIX_H_
