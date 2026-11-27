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

#include "ortools/math_opt/validators/sparse_matrix_validator.h"

#include <cmath>
#include <cstdint>

#include "absl/status/status.h"
#include "absl/status/status_macros.h"
#include "absl/types/span.h"
#include "ortools/base/status_builder.h"
#include "ortools/math_opt/core/model_summary.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/validators/ids_validator.h"

namespace operations_research::math_opt {

absl::Status SparseMatrixValid(const SparseDoubleMatrixProto& matrix,
                               const bool enforce_upper_triangular) {
  return SparseMatrixValid(matrix.row_ids(), matrix.column_ids(),
                           matrix.coefficients(), enforce_upper_triangular);
}

absl::Status SparseMatrixValid(const absl::Span<const int64_t> row_ids,
                               const absl::Span<const int64_t> column_ids,
                               const absl::Span<const double> coefficients,
                               const bool enforce_upper_triangular) {
  const int64_t nnz = row_ids.size();
  if (nnz != column_ids.size()) {
    return ortools::InvalidArgumentErrorBuilder()
           << "Expected row_id.size=" << nnz
           << " equal to column_ids.size=" << column_ids.size();
  }
  if (nnz != coefficients.size()) {
    return ortools::InvalidArgumentErrorBuilder()
           << "Expected row_id.size=" << nnz
           << " equal to coefficients.size=" << coefficients.size();
  }
  int64_t previous_row = -1;
  int64_t previous_col = -1;
  for (int64_t i = 0; i < nnz; ++i) {
    const int64_t row = row_ids[i];
    if (row < 0) {
      return ortools::InvalidArgumentErrorBuilder()
             << "row_ids should be nonnegative, but found id: " << row
             << " (at index: " << i << ")";
    }
    const int64_t col = column_ids[i];
    if (col < 0) {
      return ortools::InvalidArgumentErrorBuilder()
             << "column_ids should be nonnegative, but found id: " << col
             << " (at index: " << i << ")";
    }
    if (enforce_upper_triangular && row > col) {
      return ortools::InvalidArgumentErrorBuilder()
             << "lower triangular entry at [" << row << ", " << col
             << "] (at index: " << i << ")";
    }
    if (row < previous_row) {
      return ortools::InvalidArgumentErrorBuilder()
             << "row_ids should be nondecreasing, but found ids ["
             << previous_row << ", " << row << "] at indices [" << i - 1 << ", "
             << i << "]";
    } else if (row == previous_row) {
      if (previous_col >= col) {
        return ortools::InvalidArgumentErrorBuilder()
               << "column_ids should be strictly increasing within a row, but "
                  "for row_id: "
               << row << " found [" << previous_col << ", " << col
               << "] at indices, [" << i - 1 << ", " << i << "]";
      }
    }
    // When row > previous_row, we have a new row, nothing to check.
    if (!std::isfinite(coefficients[i])) {
      return ortools::InvalidArgumentErrorBuilder()
             << "Expected finite coefficients without NaN, but at row_id: "
             << row << ", column_id: " << col
             << " found coefficient: " << coefficients[i] << " (at index: " << i
             << ")";
    }
    previous_row = row;
    previous_col = col;
  }
  return absl::OkStatus();
}

absl::Status SparseMatrixIdsAreKnown(const SparseDoubleMatrixProto& matrix,
                                     const IdNameBiMap& row_ids,
                                     const IdNameBiMap& column_ids) {
  ABSL_RETURN_IF_ERROR(CheckIdsSubset(matrix.row_ids(), row_ids))
      << "Unknown row_id";
  ABSL_RETURN_IF_ERROR(CheckIdsSubset(matrix.column_ids(), column_ids))
      << "Unknown column_id";
  return absl::OkStatus();
}

}  // namespace operations_research::math_opt
