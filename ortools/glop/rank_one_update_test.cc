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

#include "ortools/glop/rank_one_update.h"

#include <random>

#include "gtest/gtest.h"
#include "ortools/lp_data/lp_test_utils.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/lp_utils.h"
#include "ortools/lp_data/scattered_vector.h"
#include "ortools/lp_data/sparse.h"
#include "ortools/lp_data/sparse_column.h"
#include "ortools/util/fp_utils.h"

namespace operations_research {
namespace glop {
namespace {

TEST(RankOneUpdateElementatyMatrixTest, RandomSolves) {
  const RowIndex kNumRows(1000);
  const ColIndex kNumCols(100);
  SparseMatrix matrix;
  std::mt19937 randomizer(12345);
  FillSparseMatrixRandomly(kNumRows, kNumCols, /*density=*/0.2, randomizer,
                           &matrix);
  CompactSparseMatrix storage(matrix);

  DenseColumn scratchpad;
  DenseColumn expected;
  DenseRow row_scratchpad;

  // Test all pairs of columns as u and v.
  for (ColIndex u_index(0); u_index < kNumCols; ++u_index) {
    for (ColIndex v_index(0); v_index < kNumCols; ++v_index) {
      storage.ColumnCopyToDenseColumn(u_index, &scratchpad);
      RankOneUpdateElementaryMatrix update_matrix(
          &storage, u_index, v_index,
          storage.ColumnScalarProduct(v_index, Transpose(scratchpad)));
      EXPECT_FALSE(update_matrix.IsSingular());

      SparseColumn rhs;
      FillSparseColumnRandomly(kNumRows, /*density=*/0.05, randomizer, &rhs);
      rhs.CopyToDenseVector(kNumRows, &expected);

      rhs.CopyToDenseVector(kNumRows, &scratchpad);
      update_matrix.RightSolve(&scratchpad);
      update_matrix.RightMultiply(&scratchpad);
      for (RowIndex row(0); row < kNumRows; ++row) {
        EXPECT_COMPARABLE(scratchpad[row], expected[row], kComparableEpsilon);
      }

      rhs.CopyToDenseVector(kNumRows, &scratchpad);
      row_scratchpad = Transpose(scratchpad);
      update_matrix.LeftSolve(&row_scratchpad);
      update_matrix.LeftMultiply(&row_scratchpad);
      for (RowIndex row(0); row < kNumRows; ++row) {
        EXPECT_COMPARABLE(row_scratchpad[RowToColIndex(row)], expected[row],
                          kComparableEpsilon);
      }
    }
  }
}

TEST(RankOneUpdateFactorizationTest, RandomSolvesWithNonZeros) {
  const RowIndex kNumRows(1000);
  const ColIndex kNumCols(100);
  SparseMatrix matrix;
  std::mt19937 randomizer(12345);
  FillSparseMatrixRandomly(kNumRows, kNumCols, /*density=*/0.2, randomizer,
                           &matrix);
  CompactSparseMatrix storage(matrix);
  ScatteredColumn col_scratchpad;

  // Creates a RankOneUpdateFactorization with the elementary matrices formed
  // by all the pair of vectors.
  RankOneUpdateFactorization factorization;
  factorization.set_hypersparse_ratio(2.0);  // Disable hypersparse solving.
  for (ColIndex u_index(0); u_index < kNumCols; ++u_index) {
    storage.ColumnCopyToDenseColumn(u_index, &col_scratchpad.values);
    for (ColIndex v_index(0); v_index < kNumCols; ++v_index) {
      RankOneUpdateElementaryMatrix update_matrix(
          &storage, u_index, v_index,
          storage.ColumnScalarProduct(v_index,
                                      Transpose(col_scratchpad.values)));
      EXPECT_FALSE(update_matrix.IsSingular());
      factorization.Update(update_matrix);
    }
  }

  SparseColumn rhs;
  FillSparseColumnRandomly(kNumRows, /*density=*/0.05, randomizer, &rhs);

  // Test RightSolveWithNonZeros() by comparing it to RightSolve() and verifying
  // the non-zero positions.
  rhs.CopyToDenseVector(kNumRows, &col_scratchpad.values);
  factorization.RightSolve(&col_scratchpad.values);
  const DenseColumn expected_col = col_scratchpad.values;
  rhs.CopyToDenseVector(kNumRows, &col_scratchpad.values);
  ComputeNonZeros(col_scratchpad.values, &col_scratchpad.non_zeros);
  factorization.RightSolveWithNonZeros(&col_scratchpad);
  for (const RowIndex row : col_scratchpad.non_zeros) {
    EXPECT_COMPARABLE(col_scratchpad[row], expected_col[row], Fractional(1e-7));
    col_scratchpad[row] = 0.0;
  }
  for (RowIndex row(0); row < kNumRows; ++row) {
    EXPECT_EQ(0.0, col_scratchpad[row]);
  }

  // Test LeftSolveWithNonZeros() by comparing it to LeftSolve() and verifying
  // the non-zero positions.
  rhs.CopyToDenseVector(kNumRows, &col_scratchpad.values);
  ScatteredRow row_scratchpad = TransposedView(col_scratchpad);
  factorization.LeftSolve(&row_scratchpad.values);
  const DenseRow expected_row = row_scratchpad.values;
  row_scratchpad.values = Transpose(col_scratchpad.values);
  ComputeNonZeros(row_scratchpad.values, &row_scratchpad.non_zeros);
  factorization.LeftSolveWithNonZeros(&row_scratchpad);
  for (const ColIndex col : row_scratchpad.non_zeros) {
    EXPECT_COMPARABLE(row_scratchpad[col], expected_row[col], Fractional(1e-7));
    row_scratchpad[col] = 0.0;
  }
  for (ColIndex col(0); col < RowToColIndex(kNumRows); ++col) {
    EXPECT_EQ(0.0, row_scratchpad[col]);
  }
}

}  // namespace
}  // namespace glop
}  // namespace operations_research
