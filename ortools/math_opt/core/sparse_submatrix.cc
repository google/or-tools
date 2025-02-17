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

#include "ortools/math_opt/core/sparse_submatrix.h"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <optional>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/math_opt/core/sparse_vector.h"
#include "ortools/math_opt/core/sparse_vector_view.h"
#include "ortools/math_opt/sparse_containers.pb.h"

namespace operations_research::math_opt {
namespace {

// A semi-open range [start, end). If end is nullopt, all indices >= start are
// included.
struct IndexRange {
  int64_t start;
  std::optional<int64_t> end;

  // Returns true if the input value is in the [start, end) range.
  bool Contains(const int64_t id) const {
    return id >= start && (!end.has_value() || id < *end);
  }
};

}  // namespace

SparseSubmatrixRowsView SparseSubmatrixByRows(
    const SparseDoubleMatrixProto& matrix, const int64_t start_row_id,
    const std::optional<int64_t> end_row_id, const int64_t start_col_id,
    const std::optional<int64_t> end_col_id) {
  const int matrix_size = matrix.row_ids_size();
  CHECK_EQ(matrix_size, matrix.column_ids_size());
  CHECK_EQ(matrix_size, matrix.coefficients_size());
  const IndexRange row_range = {.start = start_row_id, .end = end_row_id};
  const IndexRange col_range = {.start = start_col_id, .end = end_col_id};

  SparseSubmatrixRowsView filtered_rows;

  // row_start, next_row_start and row_end are indices into the matrix data.
  for (int row_start = 0, next_row_start; row_start < matrix_size;
       // next_row_start is set from row_end once found at the start of the loop
       // below.
       row_start = next_row_start) {
    // Find the end of the current row such that all index in [start, end) are
    // for the same row.
    const int64_t row_id = matrix.row_ids(row_start);
    int row_end = row_start + 1;
    while (row_end < matrix_size && matrix.row_ids(row_end) == row_id) {
      ++row_end;
    }

    // Prepare the next iteration.
    next_row_start = row_end;

    // Ignore rows not in the expected range.
    if (!row_range.Contains(row_id)) {
      continue;
    }

    // Finds the first column or the row in the col_range.
    int row_cols_start = row_start;
    while (row_cols_start < row_end &&
           !col_range.Contains(matrix.column_ids(row_cols_start))) {
      ++row_cols_start;
    }

    // Finds the first column greater of equal to row_cols_start that is not in
    // the col_range.
    int row_cols_end = row_cols_start;
    while (row_cols_end < row_end &&
           col_range.Contains(matrix.column_ids(row_cols_end))) {
      ++row_cols_end;
    }
    const int row_cols_len = row_cols_end - row_cols_start;

    if (row_cols_len != 0) {
      filtered_rows.emplace_back(
          row_id, MakeView(absl::MakeConstSpan(matrix.column_ids())
                               .subspan(row_cols_start, row_cols_len),
                           absl::MakeConstSpan(matrix.coefficients())
                               .subspan(row_cols_start, row_cols_len)));
    }
  }

  return filtered_rows;
}

std::vector<std::pair<int64_t, SparseVector<double>>> TransposeSparseSubmatrix(
    const SparseSubmatrixRowsView& submatrix_by_rows) {
  // Extract the columns by iterating on the filtered views of the rows (the
  // matrix is row major).
  absl::flat_hash_map<int64_t, SparseVector<double>> filtered_columns;
  for (const auto& [row_id, column_values] : submatrix_by_rows) {
    for (const auto [column_id, value] : column_values) {
      SparseVector<double>& row_values = filtered_columns[column_id];
      row_values.ids.push_back(row_id);
      row_values.values.push_back(value);
    }
  }

  // The output should be sorted by column id.
  std::vector<std::pair<int64_t, SparseVector<double>>> sorted_filtered_columns(
      std::make_move_iterator(filtered_columns.begin()),
      std::make_move_iterator(filtered_columns.end()));
  std::sort(sorted_filtered_columns.begin(), sorted_filtered_columns.end(),
            [](const std::pair<int64_t, SparseVector<double>>& lhs,
               const std::pair<int64_t, SparseVector<double>>& rhs) {
              return lhs.first < rhs.first;
            });

  return sorted_filtered_columns;
}

}  // namespace operations_research::math_opt
