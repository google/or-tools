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

#ifndef OR_TOOLS_MATH_OPT_VALIDATORS_SPARSE_MATRIX_VALIDATOR_H_
#define OR_TOOLS_MATH_OPT_VALIDATORS_SPARSE_MATRIX_VALIDATOR_H_

#include <cstdint>

#include "absl/status/status.h"
#include "absl/types/span.h"
#include "ortools/math_opt/core/model_summary.h"
#include "ortools/math_opt/model.pb.h"

namespace operations_research::math_opt {

// Validates that the input satisfies the following invariants:
//   1. matrix.row_ids, matrix.column_ids, and matrix.coefficients are all the
//      same length.
//   2. matrix.row_ids and matrix.column_ids are nonnegative.
//   3. The matrix is in row major ordering with no repeats.
//   4. Each entry in matrix.coefficients is finite and not NaN.
//   5. If enforce_upper_triangular=true, then matrix must be upper triangular.
absl::Status SparseMatrixValid(const SparseDoubleMatrixProto& matrix,
                               bool enforce_upper_triangular = false);

// Verifies that:
//   1. matrix.row_ids is a subset of row_ids.
//   2. matrix.column_ids is a subset of column_ids.
absl::Status SparseMatrixIdsAreKnown(const SparseDoubleMatrixProto& matrix,
                                     const IdNameBiMap& row_ids,
                                     const IdNameBiMap& column_ids);

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_VALIDATORS_SPARSE_MATRIX_VALIDATOR_H_
