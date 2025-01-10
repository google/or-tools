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
#include <string>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "ortools/base/status_builder.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/validators/ids_validator.h"

namespace operations_research::math_opt {

absl::Status SparseMatrixValid(const SparseDoubleMatrixProto& matrix,
                               const bool enforce_upper_triangular) {
  const int nnz = matrix.row_ids_size();
  if (nnz != matrix.column_ids_size()) {
    return util::InvalidArgumentErrorBuilder()
           << "Expected row_id.size=" << nnz
           << " equal to column_ids.size=" << matrix.column_ids_size();
  }
  if (nnz != matrix.coefficients_size()) {
    return util::InvalidArgumentErrorBuilder()
           << "Expected row_id.size=" << nnz
           << " equal to coefficients.size=" << matrix.coefficients_size();
  }
  int64_t previous_row = -1;
  int64_t previous_col = -1;
  for (int i = 0; i < nnz; ++i) {
    const int64_t row = matrix.row_ids(i);
    if (row < 0) {
      return util::InvalidArgumentErrorBuilder()
             << "row_ids should be nonnegative, but found id: " << row
             << " (at index: " << i << ")";
    }
    const int64_t col = matrix.column_ids(i);
    if (col < 0) {
      return util::InvalidArgumentErrorBuilder()
             << "column_ids should be nonnegative, but found id: " << col
             << " (at index: " << i << ")";
    }
    if (enforce_upper_triangular && row > col) {
      return util::InvalidArgumentErrorBuilder()
             << "lower triangular entry at [" << row << ", " << col
             << "] (at index: " << i << ")";
    }
    if (row < previous_row) {
      return util::InvalidArgumentErrorBuilder()
             << "row_ids should be nondecreasing, but found ids ["
             << previous_row << ", " << row << "] at indices [" << i - 1 << ", "
             << i << "]";
    } else if (row == previous_row) {
      if (previous_col >= col) {
        return util::InvalidArgumentErrorBuilder()
               << "column_ids should be strictly increasing within a row, but "
                  "for row_id: "
               << row << " found [" << previous_col << ", " << col
               << "] at indices, [" << i - 1 << ", " << i << "]";
      }
    }
    // When row > previous_row, we have a new row, nothing to check.
    if (!std::isfinite(matrix.coefficients(i))) {
      return util::InvalidArgumentErrorBuilder()
             << "Expected finite coefficients without NaN, but at row_id: "
             << row << ", column_id: " << col
             << " found coefficient: " << matrix.coefficients(i)
             << " (at index: " << i << ")";
    }
    previous_row = row;
    previous_col = col;
  }
  return absl::OkStatus();
}

absl::Status SparseMatrixIdsAreKnown(const SparseDoubleMatrixProto& matrix,
                                     const IdNameBiMap& row_ids,
                                     const IdNameBiMap& column_ids) {
  RETURN_IF_ERROR(CheckIdsSubset(matrix.row_ids(), row_ids))
      << "Unknown row_id";
  RETURN_IF_ERROR(CheckIdsSubset(matrix.column_ids(), column_ids))
      << "Unknown column_id";
  return absl::OkStatus();
}

}  // namespace operations_research::math_opt
