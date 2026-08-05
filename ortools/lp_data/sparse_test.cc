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

#include "ortools/lp_data/sparse.h"

#include <stdio.h>

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

#include "absl/log/log.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/random/random.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/lp_data/lp_test_utils.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/lp_utils.h"
#include "ortools/lp_data/permutation.h"
#include "ortools/lp_data/sparse_column.h"

namespace operations_research {
namespace glop {
namespace {

using ::testing::ContainerEq;

// --------------------------------------------------------
// SparseMatrix
// --------------------------------------------------------

TEST(SparseMatrixTest, BraceInitializationAndDump) {
  SparseMatrix matrix{
      // Commenting each row so that clang-format doesn't collapse them.
      {0, 4, 0},  // 0
      {5, 0, 0},  // 1
      {5, 2, 0},  // 2
      {7, 0, 0}   // 3
  };
  EXPECT_EQ(matrix.num_rows(), RowIndex(4));
  EXPECT_EQ(matrix.num_cols(), ColIndex(3));
  EXPECT_EQ(matrix.num_entries(), EntryIndex(5));
  EXPECT_EQ(matrix.Dump(),
            "{ 0 4 0 }\n"
            "{ 5 0 0 }\n"
            "{ 5 2 0 }\n"
            "{ 7 0 0 }\n");
}

TEST(SparseMatrixTest, PopulateFromTranspose) {
  SparseMatrix matrix{
      {0, 4, 0},  // 0
      {5, 0, 0},  // 1
      {5, 2, 0},  // 2
      {7, 0, 0}   // 3
  };
  SparseMatrix transpose{
      {0, 5, 5, 7},  // 0
      {4, 0, 2, 0},  // 1
      {0, 0, 0, 0},  // 2
  };

  SparseMatrix result;
  result.PopulateFromTranspose(matrix);
  EXPECT_TRUE(result.Equals(transpose, 0));

  result.PopulateFromTranspose(transpose);
  EXPECT_TRUE(result.Equals(matrix, 0));
}

TEST(SparseMatrixTest, Clear) {
  SparseMatrix matrix{
      {0, 4, 0},  // 0
      {5, 0, 0}   // 1
  };
  matrix.Clear();
  EXPECT_EQ(0, matrix.num_cols());
  EXPECT_EQ(0, matrix.num_rows());
  EXPECT_TRUE(matrix.IsEmpty());
}

TEST(SparseMatrixTest, AppendUnitVector) {
  SparseMatrix matrix{
      {0, 4, 0},  // 0
      {5, 0, 0},  // 1
      {5, 2, 0},  // 2
      {7, 0, 0}   // 3
  };
  const RowIndex kNumRows = matrix.num_rows();
  const ColIndex kNumCols = matrix.num_cols();
  const EntryIndex kNumEntries = matrix.num_entries();

  matrix.AppendUnitVector(RowIndex(2), 5.2);
  EXPECT_EQ(kNumRows, matrix.num_rows());
  EXPECT_EQ(kNumCols + 1, matrix.num_cols());
  EXPECT_EQ(kNumEntries + 1, matrix.num_entries());
  EXPECT_EQ(5.2, matrix.LookUpValue(RowIndex(2), kNumCols));

  // Empty unit vector still get added, but disappear on clean-up.
  matrix.AppendUnitVector(RowIndex(2), 0.0);
  EXPECT_EQ(kNumRows, matrix.num_rows());
  EXPECT_EQ(kNumCols + 2, matrix.num_cols());
  EXPECT_EQ(kNumEntries + 2, matrix.num_entries());
  EXPECT_EQ(0.0, matrix.LookUpValue(RowIndex(2), kNumCols + 1));

  matrix.CleanUp();
  EXPECT_EQ(kNumRows, matrix.num_rows());
  EXPECT_EQ(kNumCols + 2, matrix.num_cols());
  EXPECT_EQ(kNumEntries + 1, matrix.num_entries());
}

TEST(SparseMatrixTest, PopulateFromSum) {
  const Fractional kAlpha(1);
  SparseMatrix matrix_a{
      {0, 4, 0},  // 0
      {5, 0, 0},  // 1
      {5, 2, 0},  // 2
      {7, 0, 0}   // 3
  };
  const Fractional kBeta(1);
  SparseMatrix matrix_b{
      {1, 2, 3},  // 0
      {1, 3, 2},  // 1
      {2, 1, 3},  // 2
      {2, 3, 1}   // 3
  };
  SparseMatrix expected_result{
      {1, 6, 3},  // 0
      {6, 3, 2},  // 1
      {7, 3, 3},  // 2
      {9, 3, 1}   // 3
  };
  SparseMatrix result;
  result.PopulateFromLinearCombination(kAlpha, matrix_a, kBeta, matrix_b);
  EXPECT_TRUE(result.Equals(expected_result, 0.0));
}

TEST(SparseMatrixTest, PopulateFromLinearCombination) {
  const Fractional kAlpha(2.0);
  SparseMatrix matrix_a{
      {0, 4, 0},  // 0
      {0, 0, 0},  // 1
      {5, 0, 0},  // 2
      {7, 0, 0}   // 3
  };
  const Fractional kBeta(7.0);
  SparseMatrix matrix_b{
      {1, 0, 0},  // 0
      {1, 0, 0},  // 1
      {2, 1, 0},  // 2
      {2, 0, 1}   // 3
  };
  SparseMatrix expected_result{
      {7, 8, 0},   // 0
      {7, 0, 0},   // 1
      {24, 7, 0},  // 2
      {28, 0, 7}   // 3
  };
  SparseMatrix result;
  result.PopulateFromLinearCombination(kAlpha, matrix_a, kBeta, matrix_b);
  EXPECT_TRUE(result.Equals(expected_result, 0.0));
}

TEST(SparseMatrixTest, PopulateFromZeroLinearCombination) {
  const Fractional kAlpha(0.0);
  SparseMatrix matrix_a{
      {0, 4, 0},  // 0
      {0, 0, 0},  // 1
      {5, 0, 0},  // 2
      {7, 0, 0}   // 3
  };
  const Fractional kBeta(0.0);
  SparseMatrix matrix_b{
      {1, 0, 0},  // 0
      {1, 0, 0},  // 1
      {2, 1, 0},  // 2
      {2, 0, 1}   // 3
  };
  SparseMatrix result;
  result.PopulateFromLinearCombination(kAlpha, matrix_a, kBeta, matrix_b);
  EXPECT_EQ(0, result.num_entries());

  // Note that the size stays the same.
  EXPECT_EQ(matrix_a.num_rows(), result.num_rows());
  EXPECT_EQ(matrix_a.num_cols(), result.num_cols());
}

TEST(SparseMatrixTest, PopulateFromProduct) {
  SparseMatrix matrix_a{
      {0, 4},  // 0
      {0, 0},  // 1
      {5, 3},  // 2
      {7, 0}   // 3
  };
  SparseMatrix matrix_b{
      {1, 0, 0, 1, 2},  // 0
      {1, 0, 3, 0, 0}   // 1
  };
  SparseMatrix expected_result{
      {4, 0, 12, 0, 0},  // 0
      {0, 0, 0, 0, 0},   // 1
      {8, 0, 9, 5, 10},  // 2
      {7, 0, 0, 7, 14}   // 3
  };
  SparseMatrix result;
  result.PopulateFromProduct(matrix_a, matrix_b);
  EXPECT_TRUE(result.Equals(expected_result, 0.0));
}

// Used as a witness to detect new/updated fields of the "SparseMatrix" class,
// so that we can remind authors to update the swap and copy methods.
//
// IMPORTANT:
// - When updating this, respect the ordering of the fields of the original
//   class! It matters for the value of sizeof(SparseMatrixClone).
// - Update the Swap() and PopulateFromSparseMatrix() methods in SparseMatrix.
struct SparseMatrixClone {
  StrictITIVector<ColIndex, SparseColumn> columns_;
  RowIndex num_rows_;
};

TEST(SparseMatrixTest, PopulateFromSparseMatrix) {
  SparseMatrix sparse_matrix;
  sparse_matrix.PopulateFromIdentity(ColIndex(10));
  SparseMatrix copy;
  copy.PopulateFromSparseMatrix(sparse_matrix);
  EXPECT_EQ(ColIndex(10), copy.num_cols());
  EXPECT_EQ(RowIndex(10), copy.num_rows());
  EXPECT_EQ(EntryIndex(10), copy.num_entries());

  EXPECT_EQ(sizeof(SparseMatrixClone), sizeof(SparseMatrix));
}

TEST(SparseMatrixTest, Swap) {
  SparseMatrix sparse_matrix;
  sparse_matrix.PopulateFromIdentity(ColIndex(10));
  SparseMatrix other_matrix;

  EXPECT_FALSE(sparse_matrix.IsEmpty());
  other_matrix.Swap(&sparse_matrix);
  EXPECT_EQ(ColIndex(10), other_matrix.num_cols());
  EXPECT_EQ(RowIndex(10), other_matrix.num_rows());
  EXPECT_EQ(EntryIndex(10), other_matrix.num_entries());
  EXPECT_TRUE(sparse_matrix.IsEmpty());

  EXPECT_EQ(sizeof(SparseMatrixClone), sizeof(SparseMatrix));
}

TEST(SparseMatrixTest, DeleteColumns) {
  const ColIndex kNumcols(10);
  SparseMatrix sparse_matrix;
  sparse_matrix.PopulateFromIdentity(kNumcols);

  // Note that the Boolean vector is shorter on purpose.
  DenseBooleanRow to_delete(ColIndex(5), false);
  to_delete[ColIndex(1)] = true;
  to_delete[ColIndex(3)] = true;
  to_delete[ColIndex(4)] = true;

  sparse_matrix.DeleteColumns(to_delete);
  const ColIndex kExpectedSize(7);
  EXPECT_EQ(kExpectedSize, sparse_matrix.num_cols());
  ColIndex new_col(0);
  for (ColIndex col(0); col < kNumcols; ++col) {
    if (col < to_delete.size() && to_delete[col]) continue;
    EXPECT_EQ(1.0, sparse_matrix.LookUpValue(ColToRowIndex(col), new_col));
    ++new_col;
  }
}

// Example from wikipedia http://en.wikipedia.org/wiki/Matrix_norm
TEST(SparseMatrixTest, ComputeOneNormAndInfinityNorm3x3) {
  SparseMatrix matrix{
      {3, 5, 7},  // 0
      {2, 6, 4},  // 1
      {0, 2, 8}   // 2
  };
  EXPECT_EQ(19, matrix.ComputeOneNorm());
  EXPECT_EQ(15, matrix.ComputeInfinityNorm());
}

// Example from wikipedia http://en.wikipedia.org/wiki/Matrix_norm
TEST(SparseMatrixTest, ComputeOneNormAndInfinityNorm4x4) {
  SparseMatrix matrix{
      {2, 4, 2, 1},  // 0
      {3, 1, 5, 2},  // 1
      {1, 2, 3, 3},  // 2
      {0, 6, 1, 2}   // 3
  };
  EXPECT_EQ(13, matrix.ComputeOneNorm());
  EXPECT_EQ(11, matrix.ComputeInfinityNorm());
}

TEST(SparseMatrixTest, HighDynamicRange) {
  SparseMatrix matrix{
      {1, -1e-44, 0},    // 0
      {0, -1, 1e-50},    // 1
      {0.1, -1e-64, 1},  // 2
      {1, -1e-40, 0}     // 3
  };
  Fractional min_magnitude;
  Fractional max_magnitude;
  matrix.ComputeMinAndMaxMagnitudes(&min_magnitude, &max_magnitude);
  EXPECT_EQ(1e-64, min_magnitude);
  EXPECT_EQ(1.0, max_magnitude);
  const Fractional dynamic_range = max_magnitude / min_magnitude;
  EXPECT_EQ(1e64, dynamic_range);
}

TEST(SparseMatrixTest, NullDynamicRange) {
  const ColIndex kMaxCol(10);
  const RowIndex kMaxRow(10);

  SparseMatrix matrix;
  matrix.PopulateFromZero(kMaxRow, kMaxCol);

  Fractional min_magnitude;
  Fractional max_magnitude;
  matrix.ComputeMinAndMaxMagnitudes(&min_magnitude, &max_magnitude);
  EXPECT_EQ(0.0, min_magnitude);
  EXPECT_EQ(0.0, max_magnitude);
}

TEST(SparseMatrixTest, AppendRowsFromSparseMatrix) {
  SparseMatrix matrix_a{
      {0, 1, 0},  // 0
      {2, 0, 3}   // 1
  };
  const SparseMatrix matrix_b{
      {0, 4, 5},  // 0
      {6, 0, 7}   // 1
  };
  const SparseMatrix expected_matrix_a{
      {0, 1, 0},  // 0
      {2, 0, 3},  // 1
      {0, 4, 5},  // 2
      {6, 0, 7}   // 3
  };

  EXPECT_TRUE(matrix_a.AppendRowsFromSparseMatrix(matrix_b));
  ASSERT_EQ(4, matrix_a.num_rows());
  ASSERT_EQ(3, matrix_a.num_cols());
  EXPECT_TRUE(expected_matrix_a.Equals(matrix_a, 1e-64));
}

TEST(SparseMatrixTest, AppendRowsFromSparseMatrixFailsIfNumColsDiffer) {
  SparseMatrix matrix_a{
      {0, 1, 0, 0},  // 0
      {2, 0, 3, 0}   // 1
  };
  SparseMatrix matrix_b{
      {0, 4, 5},  // 0
      {6, 0, 7}   // 1
  };

  EXPECT_FALSE(matrix_a.AppendRowsFromSparseMatrix(matrix_b));
  EXPECT_FALSE(matrix_b.AppendRowsFromSparseMatrix(matrix_a));
}

TEST(CompactMatrixTest, EmptyMatrices) {
  SparseMatrix matrix;
  CompactSparseMatrix compact_matrix(matrix);
  EXPECT_EQ(EntryIndex(0), compact_matrix.num_entries());
  EXPECT_EQ(RowIndex(0), compact_matrix.num_rows());
  EXPECT_EQ(ColIndex(0), compact_matrix.num_cols());
  EXPECT_TRUE(compact_matrix.IsEmpty());

  CompactSparseMatrix transpose;
  transpose.PopulateFromTranspose(compact_matrix);
  EXPECT_EQ(EntryIndex(0), transpose.num_entries());
  EXPECT_EQ(RowIndex(0), transpose.num_rows());
  EXPECT_EQ(ColIndex(0), transpose.num_cols());
  EXPECT_TRUE(transpose.IsEmpty());
}

TEST(CompactSparseMatrixTest, EmptyColumnView) {
  CompactSparseMatrix matrix;
  matrix.Reset(RowIndex(10));
  DenseColumn column(RowIndex(10), 0.0);
  matrix.AddDenseColumn(column);
  EXPECT_EQ(matrix.column(ColIndex(0)).num_entries(), 0);
}

// Returns true if the two arguments encode the same matrix.
void ExpectMatrixAndCompactMatrixAreExactlyTheSame(
    const SparseMatrix& matrix, const CompactSparseMatrix& compact_matrix) {
  EXPECT_EQ(matrix.num_cols(), compact_matrix.num_cols());
  EXPECT_EQ(matrix.num_rows(), compact_matrix.num_rows());
  EXPECT_EQ(matrix.num_entries(), compact_matrix.num_entries());
  for (ColIndex col(0); col < matrix.num_cols(); ++col) {
    EntryIndex j(0);
    const SparseColumn& sparse_column = matrix.column(col);
    const auto view = compact_matrix.view();
    for (const EntryIndex i : view.Column(col)) {
      EXPECT_EQ(view.EntryRow(i), sparse_column.EntryRow(j));
      EXPECT_EQ(view.EntryCoefficient(i), sparse_column.EntryCoefficient(j));
      ++j;
    }
    EXPECT_EQ(j, sparse_column.num_entries());
    EXPECT_EQ(compact_matrix.ColumnNumEntries(col),
              sparse_column.num_entries());
  }
}

TEST(CompactSparseMatrixTest, PopulateFromSparseMatrixAndAddSlacks) {
  const SparseMatrix matrix{
      {0, 4, 0},  // 0
      {5, 0, 0},  // 1
      {5, 2, 0},  // 2
      {7, 0, 0}   // 3
  };
  const SparseMatrix expected{
      {0, 4, 0, 1, 0, 0, 0},  // 0
      {5, 0, 0, 0, 1, 0, 0},  // 1
      {5, 2, 0, 0, 0, 1, 0},  // 2
      {7, 0, 0, 0, 0, 0, 1}   // 3
  };

  CompactSparseMatrix compact_matrix;
  compact_matrix.PopulateFromSparseMatrixAndAddSlacks(matrix);
  ExpectMatrixAndCompactMatrixAreExactlyTheSame(expected, compact_matrix);
}

TEST(CompactMatrixTest, PopulateFromSparseMatrixAndIterations) {
  const RowIndex kNumRows(1000);
  const ColIndex kNumCols(2000);
  SparseMatrix matrix;
  std::mt19937 randomizer(12345);
  FillSparseMatrixRandomly(kNumRows, kNumCols, /*density=*/0.2, randomizer,
                           &matrix);
  CompactSparseMatrix compact_matrix(matrix);
  ExpectMatrixAndCompactMatrixAreExactlyTheSame(matrix, compact_matrix);
}

TEST(CompactMatrixTest, AddDenseColumn) {
  const RowIndex kNumRows(1000);
  const ColIndex kNumCols(2000);
  SparseMatrix matrix;
  std::mt19937 randomizer(12345);
  FillSparseMatrixRandomly(kNumRows, kNumCols, /*density=*/0.2, randomizer,
                           &matrix);
  CompactSparseMatrix compact_matrix;
  compact_matrix.Reset(kNumRows);
  DenseColumn scratchpad;
  for (ColIndex col(0); col < kNumCols; ++col) {
    matrix.column(col).CopyToDenseVector(kNumRows, &scratchpad);
    compact_matrix.AddDenseColumn(scratchpad);
  }
  ExpectMatrixAndCompactMatrixAreExactlyTheSame(matrix, compact_matrix);
}

TEST(CompactMatrixTest, AddAndClearColumnWithNonZeros) {
  const RowIndex kNumRows(1000);
  const ColIndex kNumCols(2000);
  SparseMatrix matrix;
  std::mt19937 randomizer(12345);
  FillSparseMatrixRandomly(kNumRows, kNumCols, /*density=*/0.2, randomizer,
                           &matrix);

  CompactSparseMatrix compact_matrix;
  compact_matrix.Reset(kNumRows);
  DenseColumn scratchpad;
  std::vector<RowIndex> non_zeros;
  for (ColIndex col(0); col < kNumCols; ++col) {
    matrix.column(col).CopyToDenseVector(kNumRows, &scratchpad);
    for (const SparseColumn::Entry e : matrix.column(col)) {
      non_zeros.push_back(e.row());
      // Add some duplicates.
      if (e.row().value() % 2 == 0) {
        non_zeros.push_back(e.row());
      }
    }
    compact_matrix.AddAndClearColumnWithNonZeros(&scratchpad, &non_zeros);
    EXPECT_TRUE(non_zeros.empty());
    for (RowIndex row(0); row < matrix.num_rows(); ++row) {
      EXPECT_EQ(0.0, scratchpad[row]);
    }
  }
  ExpectMatrixAndCompactMatrixAreExactlyTheSame(matrix, compact_matrix);
}

TEST(CompactMatrixTest, ColumnUtilityFunctions) {
  const RowIndex kNumRows(1000);
  const ColIndex kNumCols(200);
  SparseMatrix matrix;
  std::mt19937 randomizer(12345);
  FillSparseMatrixRandomly(kNumRows, kNumCols, /*density=*/0.2, randomizer,
                           &matrix);
  CompactSparseMatrix compact_matrix(matrix);

  DenseColumn scratchpad;
  for (ColIndex i(0); i < matrix.num_cols(); ++i) {
    compact_matrix.ColumnCopyToDenseColumn(i, &scratchpad);

    // Check copy is ok (only on the non-zeros).
    for (const SparseColumn::Entry e : matrix.column(i)) {
      EXPECT_EQ(scratchpad[e.row()], e.coefficient());
    }

    // Check that subtracting the column makes scratchpad all zero.
    compact_matrix.ColumnAddMultipleToDenseColumn(i, -1.0, &scratchpad);
    for (RowIndex row(0); row < matrix.num_rows(); ++row) {
      EXPECT_EQ(0.0, scratchpad[row]);
    }

    // Add back the column and check again.
    compact_matrix.ColumnAddMultipleToDenseColumn(i, 1.0, &scratchpad);
    for (const SparseColumn::Entry e : matrix.column(i)) {
      EXPECT_EQ(scratchpad[e.row()], e.coefficient());
    }

    // Check scalar product with all the other columns.
    for (ColIndex col(0); col < matrix.num_cols(); ++col) {
      EXPECT_NEAR(
          ScalarProduct(scratchpad, matrix.column(col)),
          compact_matrix.ColumnScalarProduct(col, Transpose(scratchpad)),
          1e-10);
    }
  }
}

TEST(CompactMatrixTest, PopulateFromTransposeSparseMatrix) {
  const RowIndex kNumRows(1000);
  const ColIndex kNumCols(2000);
  SparseMatrix matrix;
  std::mt19937 randomizer(12345);
  FillSparseMatrixRandomly(kNumRows, kNumCols, /*density=*/0.2, randomizer,
                           &matrix);

  CompactSparseMatrix compact_matrix(matrix);
  CompactSparseMatrix compact_transpose_matrix;
  compact_transpose_matrix.PopulateFromTranspose(compact_matrix);
  SparseMatrix transpose_matrix;
  transpose_matrix.PopulateFromTranspose(matrix);
  ExpectMatrixAndCompactMatrixAreExactlyTheSame(transpose_matrix,
                                                compact_transpose_matrix);

  // Note that this only work because FillSparseMatrixRandomly() do generate
  // a SparseMatrix whith its column entries ordered by row.
  CompactSparseMatrix double_compact_transpose_matrix;
  double_compact_transpose_matrix.PopulateFromTranspose(
      compact_transpose_matrix);
  ExpectMatrixAndCompactMatrixAreExactlyTheSame(
      matrix, double_compact_transpose_matrix);
}

TEST(TriangularMatrixTest, AddAndNormalizeTriangularColumn) {
  SparseColumn column;
  column.SetCoefficient(RowIndex(2), 12.0);
  column.SetCoefficient(RowIndex(3), 9.0);
  column.SetCoefficient(RowIndex(4), 3.0);
  column.SetCoefficient(RowIndex(5), 6.0);
  TriangularMatrix matrix;
  matrix.Reset(RowIndex(10), ColIndex(1));
  matrix.AddAndNormalizeTriangularColumn(column, RowIndex(4), 3.0);

  // Note that the diagonal elements always end up in the diagonal row for
  // the given column.
  SparseColumn output;
  matrix.CopyColumnToSparseColumn(ColIndex(0), &output);
  EXPECT_EQ("[0]=1, [2]=4, [3]=3, [5]=2", output.DebugString());
}

// ----------------------------------------------------------------------------
// Triangular solves test.
// ----------------------------------------------------------------------------

TEST(TriangularMatrixTest, AllSolveIdentity) {
  DenseColumn kDenseColumn{3, 7, 13, 17};

  SparseMatrix matrix;
  matrix.PopulateFromIdentity(ColIndex(4));
  TriangularMatrix compact_matrix;
  compact_matrix.PopulateFromTriangularSparseMatrix(matrix);

  DenseColumn column = kDenseColumn;
  compact_matrix.LowerSolve(&column);
  EXPECT_THAT(kDenseColumn, ContainerEq(column));
  compact_matrix.TransposeUpperSolve(&column);
  EXPECT_THAT(kDenseColumn, ContainerEq(column));
  compact_matrix.UpperSolve(&column);
  EXPECT_THAT(kDenseColumn, ContainerEq(column));
  compact_matrix.TransposeLowerSolve(&column);
  EXPECT_THAT(kDenseColumn, ContainerEq(column));
}

TEST(TriangularMatrixTest, ExploitFirstIdentityColumns) {
  SparseMatrix matrix{
      {1, 0, 0, 0},   // 0
      {0, 1, 0, 0},   // 1
      {0, 0, -1, 0},  // 2
      {0, 0, 0, 1}    // 3
  };
  TriangularMatrix triangular_matrix;
  triangular_matrix.PopulateFromTriangularSparseMatrix(matrix);
  EXPECT_EQ(ColIndex(2), triangular_matrix.GetFirstNonIdentityColumn());
}

const SparseMatrix kLowerMatrix{
    // 0   1    2    3    4   5   6   7   8    9   10   11  12 13 14
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},      //  0
    {0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},      //  1
    {0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},      //  2
    {0, -5, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},     //  3
    {0, 0, 13, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},     //  4
    {0, 0, 0, 9, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},      //  5
    {0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0},      //  6
    {0, 0, -11, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0},    //  7
    {0, 0, 0, -13, 0, 14, 0, 0, 1, 0, 0, 0, 0, 0, 0},   //  8
    {0, 0, 6, 0, -15, 0, 9, 0, 0, 1, 0, 0, 0, 0, 0},    //  9
    {0, 0, 0, 0, 0, 0, -5, 8, -3, 0, 1, 0, 0, 0, 0},    // 10
    {0, 0, 0, 0, 0, 0, 0, -9, 0, 0, -10, 1, 0, 0, 0},   // 11
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 9, 0, -14, 1, 0, 0},    // 12
    {0, 0, 0, 0, 0, 0, 0, 0, 0, -12, -4, 1, -8, 1, 0},  // 13
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 7, 1}       // 14
};

const SparseMatrix kUpperMatrix{
    // 0  1   2   3   4   5   6    7    8    9   10   11   12   13 14
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},     //  0
    {0, 1, 0, -5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},    //  1
    {0, 0, 1, 0, 13, 0, 0, -11, 0, 6, 0, 0, 0, 0, 0},  //  2
    {0, 0, 0, 1, 0, 9, 0, 0, -13, 0, 0, 0, 0, 0, 0},   //  3
    {0, 0, 0, 0, 1, 0, 0, 0, 0, -15, 0, 0, 0, 0, 0},   //  4
    {0, 0, 0, 0, 0, 1, 0, 0, 14, 0, 0, 0, 0, 0, 0},    //  5
    {0, 0, 0, 0, 0, 0, 1, 0, 0, 9, -5, 0, 0, 0, 0},    //  6
    {0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 8, -9, 0, 0, 0},    //  7
    {0, 0, 0, 0, 0, 0, 0, 0, 1, 0, -3, 0, 0, 0, 0},    //  8
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 9, -12, 0},   //  9
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, -10, 0, -4, 1},  // 10
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, -14, 1, 0},   // 11
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, -8, 0},    // 12
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 7},     // 13
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}      // 14
};

TEST(TriangularMatrixTest, TriangularLowerSolve) {
  const DenseColumn kDenseColumn{1, 2,  3,  4,  5,  6,  7, 8,
                                 9, 10, 11, 12, 13, 14, 15};
  const DenseColumn kExpectedResult{1,    2,     3,      14,      -34,
                                    -120, 7,     41,     1871,    -581,
                                    5331, 53691, 756916, 6016003, -42117337};

  TriangularMatrix compact_matrix;
  TriangularMatrix transposed_compact_matrix;
  compact_matrix.PopulateFromTriangularSparseMatrix(kLowerMatrix);
  transposed_compact_matrix.PopulateFromTranspose(compact_matrix);

  DenseColumn column = kDenseColumn;
  compact_matrix.LowerSolve(&column);
  EXPECT_THAT(kExpectedResult, ContainerEq(column));

  column = kDenseColumn;
  transposed_compact_matrix.TransposeUpperSolve(&column);
  EXPECT_THAT(kExpectedResult, ContainerEq(column));

  // Test the hyper-sparse version.
  // Since column is dense, this function should return an empty vector.
  column = kDenseColumn;
  RowIndexVector non_zeros;
  ComputeNonZeros(column, &non_zeros);
  compact_matrix.ComputeRowsToConsiderWithDfs(&non_zeros);
  EXPECT_TRUE(non_zeros.empty());
}

TEST(TriangularMatrixTest, TransposeLowerSolve) {
  const DenseColumn kDenseColumn{1, 2,  3,  4,  5,  6,  7, 8,
                                 9, 10, 11, 12, 13, 14, 15};
  const DenseColumn kExpectedResult{
      1,       -207322223, 6693824, -41464445, 80300, 4176276, -545360, 706349,
      -298305, 5353,       -99438,  -9907,     -715,  -91,     15};

  TriangularMatrix triangular_matrix;
  triangular_matrix.PopulateFromTriangularSparseMatrix(kLowerMatrix);

  DenseColumn column = kDenseColumn;
  triangular_matrix.TransposeLowerSolve(&column);
  EXPECT_THAT(kExpectedResult, ContainerEq(column));
}

TEST(TriangularMatrixTest, LowerSparseSolve) {
  const SparseMatrix matrix{
      // 1  2  3  4  5  6  7  8  9 10 11 12 13 14
      {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},  //  1
      {0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},  //  2
      {2, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},  //  3
      {0, 2, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},  //  4
      {0, 0, 2, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},  //  5
      {0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0},  //  6
      {0, 2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0},  //  7
      {0, 0, 2, 0, 2, 0, 0, 1, 0, 0, 0, 0, 0, 0},  //  8
      {0, 2, 0, 2, 0, 2, 0, 0, 1, 0, 0, 0, 0, 0},  //  9
      {0, 0, 0, 0, 0, 2, 2, 2, 0, 1, 0, 0, 0, 0},  // 10
      {0, 0, 0, 0, 0, 0, 2, 0, 0, 2, 1, 0, 0, 0},  // 11
      {0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 2, 1, 0, 0},  // 12
      {0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 1, 0},  // 13
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 2, 1},  // 14
  };
  DenseColumn column{0, 0, 0, 4, 0, 6, 0, 0, 0, 0, 0, 0, 0, 0};
  const DenseColumn kExpectedResult{0, 0,   0,   4,  0,  6,  0,
                                    0, -20, -12, 24, -8, 32, -40};

  TriangularMatrix triangular_matrix;
  triangular_matrix.PopulateFromTriangularSparseMatrix(matrix);
  triangular_matrix.LowerSolve(&column);
  EXPECT_THAT(kExpectedResult, ContainerEq(column));
}

TEST(TriangularMatrixTest, UpperSolve) {
  DenseColumn column{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
  const DenseColumn kExpectedResult{
      1,       -207322223, 6693824, -41464445, 80300, 4176276, -545360, 706349,
      -298305, 5353,       -99438,  -9907,     -715,  -91,     15};
  TriangularMatrix triangular_matrix;
  triangular_matrix.PopulateFromTriangularSparseMatrix(kUpperMatrix);
  triangular_matrix.UpperSolve(&column);
  EXPECT_THAT(kExpectedResult, ContainerEq(column));
}

TEST(TriangularMatrixTest, TransposeUpperSolve) {
  DenseColumn column{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
  const DenseColumn kExpectedResult{1,    2,     3,      14,      -34,
                                    -120, 7,     41,     1871,    -581,
                                    5331, 53691, 756916, 6016003, -42117337};
  TriangularMatrix triangular_matrix;
  triangular_matrix.PopulateFromTriangularSparseMatrix(kUpperMatrix);
  triangular_matrix.TransposeUpperSolve(&column);
  EXPECT_THAT(kExpectedResult, ContainerEq(column));
}

void CheckLowerPermutedSolves(RowIndex num_rows, Fractional tolerance,
                              absl::BitGenRef randomizer) {
  SparseMatrix lower_matrix;
  FillSparseMatrixWithRandomLowerTriangularMatrix(num_rows, /*density=*/0.5,
                                                  randomizer, &lower_matrix);

  SparseMatrix sparse_column_matrix;
  sparse_column_matrix.AppendEmptyColumn();
  FillSparseColumnRandomly(num_rows, /*density=*/0.5, randomizer,
                           sparse_column_matrix.mutable_column(ColIndex(0)));

  SparseMatrix rhs_column_matrix;
  rhs_column_matrix.PopulateFromProduct(lower_matrix, sparse_column_matrix);
  const SparseColumn& rhs_column = rhs_column_matrix.column(ColIndex(0));

  RowPermutation row_permutation(num_rows);
  row_permutation.PopulateRandomly();
  RowPermutation inverse_row_permutation;
  inverse_row_permutation.PopulateFromInverse(row_permutation);

  const ColIndex num_cols(num_rows.value());
  ColumnPermutation inverse_col_perm(num_cols);
  inverse_col_perm.PopulateFromIdentity();

  SparseMatrix permuted_lower_matrix;
  permuted_lower_matrix.PopulateFromPermutedMatrix(
      lower_matrix, inverse_row_permutation, inverse_col_perm);

  RowMapping row_mapping(num_rows, kInvalidRow);
  for (RowIndex row(0); row < num_rows; ++row) {
    row_mapping[row] = inverse_row_permutation[row];
  }

  TriangularMatrix triangular_matrix;
  triangular_matrix.PopulateFromTriangularSparseMatrix(lower_matrix);
  triangular_matrix.ApplyRowPermutationToNonDiagonalEntries(
      inverse_row_permutation);
  SparseColumn sparse_result_a;
  triangular_matrix.PermutedLowerSolve(rhs_column, row_permutation, row_mapping,
                                       &sparse_result_a, &sparse_result_a);

  SparseColumn sparse_result_b;
  triangular_matrix.PermutedLowerSparseSolve(ColumnView(rhs_column),
                                             row_permutation, &sparse_result_b,
                                             &sparse_result_b);

  for (RowIndex row(0); row < num_rows; ++row) {
    EXPECT_LE(fabs(sparse_result_a.LookUpCoefficient(row) -
                   sparse_result_b.LookUpCoefficient(row)),
              tolerance);
  }
}

void CheckLowerSolves(RowIndex num_rows, Fractional tolerance,
                      absl::BitGenRef randomizer) {
  SparseMatrix lower_matrix;
  FillSparseMatrixWithRandomLowerTriangularMatrix(num_rows, /*density=*/0.5,
                                                  randomizer, &lower_matrix);

  SparseMatrix sparse_column_matrix;
  sparse_column_matrix.AppendEmptyColumn();
  FillSparseColumnRandomly(num_rows, /*density=*/0.1, randomizer,
                           sparse_column_matrix.mutable_column(ColIndex(0)));

  DenseColumn expected;
  sparse_column_matrix.column(ColIndex(0))
      .CopyToDenseVector(num_rows, &expected);

  SparseMatrix rhs_column_matrix;
  rhs_column_matrix.PopulateFromProduct(lower_matrix, sparse_column_matrix);
  DenseColumn rhs_column;
  rhs_column_matrix.column(ColIndex(0))
      .CopyToDenseVector(num_rows, &rhs_column);

  TriangularMatrix compact_lower;
  compact_lower.PopulateFromTriangularSparseMatrix(lower_matrix);

  // Dense lower solve.
  DenseColumn temp = rhs_column;
  compact_lower.LowerSolve(&temp);
  ExpectFractionalVectorComparable(expected, temp);

  // Sparse lower solve.
  temp = rhs_column;
  std::vector<RowIndex> non_zeros;
  ComputeNonZeros(temp, &non_zeros);
  compact_lower.ComputeRowsToConsiderWithDfs(&non_zeros);
  if (non_zeros.empty()) {
    compact_lower.LowerSolve(&temp);
  } else {
    compact_lower.HyperSparseSolveWithReversedNonZeros(&temp, &non_zeros);
  }
  ExpectFractionalVectorComparable(expected, temp);
}

TEST(TriangularMatrixTest, RandomTests) {
  const RowIndex kNumRows(30);
  const Fractional kTolerance(1e-10);
  const int kNumSolves = 100;

  std::mt19937 randomizer(12345);
  for (int i = 0; i < kNumSolves; ++i) {
    CheckLowerPermutedSolves(kNumRows, kTolerance, randomizer);
    CheckLowerSolves(kNumRows, kTolerance, randomizer);
  }
}

TEST(TriangularMatrixTest, PermutedTriangularLowerSolve) {
  const DenseColumn kRhs{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
  const DenseColumn kExpectedResult{1,    2,     3,      14,      -34,
                                    -120, 7,     41,     1871,    -581,
                                    5331, 53691, 756916, 6016003, -42117337};

  DenseColumn dense_column;
  SparseColumn sparse_column;
  TriangularMatrix compact_matrix;
  compact_matrix.PopulateFromTriangularSparseMatrix(kLowerMatrix);

  // Test first with the identity permutation.
  const RowIndex num_rows(kLowerMatrix.num_rows());
  RowPermutation row_perm(num_rows);
  row_perm.PopulateFromIdentity();
  sparse_column.PopulateFromDenseVector(kRhs);
  compact_matrix.PermutedLowerSparseSolve(ColumnView(sparse_column), row_perm,
                                          &sparse_column, &sparse_column);
  sparse_column.CopyToDenseVector(num_rows, &dense_column);
  ExpectFractionalVectorComparable(kExpectedResult, dense_column);

  // Test with a random permutation.
  row_perm.PopulateRandomly();
  compact_matrix.ApplyRowPermutationToNonDiagonalEntries(row_perm);
  sparse_column.PopulateFromDenseVector(kRhs);
  sparse_column.ApplyRowPermutation(row_perm);

  RowPermutation inverse_row_perm;
  inverse_row_perm.PopulateFromInverse(row_perm);
  compact_matrix.PermutedLowerSparseSolve(ColumnView(sparse_column),
                                          inverse_row_perm, &sparse_column,
                                          &sparse_column);
  sparse_column.ApplyRowPermutation(inverse_row_perm);
  sparse_column.CopyToDenseVector(num_rows, &dense_column);
  ExpectFractionalVectorComparable(kExpectedResult, dense_column);
}

TEST(TriangularMatrixTest, TriangularUpperSolve) {
  const DenseColumn kDenseColumn{1, 2,  3,  4,  5,  6,  7, 8,
                                 9, 10, 11, 12, 13, 14, 15};
  const DenseColumn kExpectedResult{
      1,       -207322223, 6693824, -41464445, 80300, 4176276, -545360, 706349,
      -298305, 5353,       -99438,  -9907,     -715,  -91,     15};

  TriangularMatrix compact_matrix;
  TriangularMatrix transposed_compact_matrix;
  compact_matrix.PopulateFromTriangularSparseMatrix(kLowerMatrix);
  transposed_compact_matrix.PopulateFromTranspose(compact_matrix);

  DenseColumn column = kDenseColumn;
  compact_matrix.TransposeLowerSolve(&column);
  EXPECT_THAT(kExpectedResult, ContainerEq(column));

  column = kDenseColumn;
  transposed_compact_matrix.UpperSolve(&column);
  EXPECT_THAT(kExpectedResult, ContainerEq(column));

  // Test the hyper-sparse version.
  // Since column is dense, this function should return an empty vector.
  RowIndexVector non_zeros;
  column = kDenseColumn;
  ComputeNonZeros(column, &non_zeros);
  compact_matrix.ComputeRowsToConsiderWithDfs(&non_zeros);
  EXPECT_TRUE(non_zeros.empty());
}

// CheckHyperSparseSolves is a utility function that explicitly tests the
// hyper-sparse solve routines for matrix and transpose_matrix with right-hand
// side rhs.  The true result of a left (right) solve with matrix is
// result_left (result_right).
void CheckHyperSparseSolves(const TriangularMatrix& matrix,
                            const TriangularMatrix& transpose_matrix,
                            const DenseColumn& rhs,
                            const DenseColumn& result_left,
                            const DenseColumn& result_right) {
  // Compute the non-zero position for the hyper-sparse solves.
  RowIndexVector non_zero_rows;
  RowIndexVector sorted_non_zeros;
  ComputeNonZeros(rhs, &non_zero_rows);
  ASSERT_FALSE(non_zero_rows.empty());
  matrix.ComputeRowsToConsiderWithDfs(&non_zero_rows);
  ASSERT_FALSE(non_zero_rows.empty());

  // It is exactly the same as the one computed by
  // ComputeRowsToConsiderInSortedOrder() up to the order.
  ComputeNonZeros(rhs, &sorted_non_zeros);
  matrix.ComputeRowsToConsiderInSortedOrder(&sorted_non_zeros);
  {
    RowIndexVector copy = non_zero_rows;
    std::sort(copy.begin(), copy.end());
    EXPECT_EQ(copy, sorted_non_zeros);
  }

  // This non-zero pattern can be used for a normal solve ...
  DenseColumn other_result = rhs;
  matrix.HyperSparseSolveWithReversedNonZeros(&other_result, &non_zero_rows);
  ExpectFractionalVectorComparable(result_left, other_result);

  // ... or a transpose one.
  other_result = rhs;
  transpose_matrix.TransposeHyperSparseSolveWithReversedNonZeros(
      &other_result, &non_zero_rows);
  ExpectFractionalVectorComparable(result_left, other_result);

  // And if we use the sorted version, we get EXACTLY the same result.
  other_result = rhs;
  matrix.HyperSparseSolve(&other_result, &sorted_non_zeros);
  EXPECT_EQ(result_left, other_result);

  // Now do the same for the right solve.
  ComputeNonZeros(rhs, &non_zero_rows);
  transpose_matrix.ComputeRowsToConsiderWithDfs(&non_zero_rows);
  ASSERT_FALSE(non_zero_rows.empty());

  other_result = rhs;
  transpose_matrix.HyperSparseSolveWithReversedNonZeros(&other_result,
                                                        &non_zero_rows);
  ExpectFractionalVectorComparable(result_right, other_result);

  other_result = rhs;
  matrix.TransposeHyperSparseSolveWithReversedNonZeros(&other_result,
                                                       &non_zero_rows);
  ExpectFractionalVectorComparable(result_right, other_result);
}

TEST(TriangularMatrixTest, RandomTriangularSolvesAreAllConsistent) {
  const ColIndex kNumCols(10000);
  const RowIndex kNumRows(kNumCols.value());
  std::mt19937 randomizer(12345);

  // We generate upper first, because we don't need the diagonal of ones
  SparseMatrix upper_matrix;
  FillSparseMatrixWithRandomUpperTriangularMatrix(kNumRows, /*density=*/0.0002,
                                                  randomizer, &upper_matrix);

  TriangularMatrix matrix;
  TriangularMatrix transpose_matrix;
  transpose_matrix.PopulateFromTriangularSparseMatrix(upper_matrix);
  matrix.PopulateFromTranspose(transpose_matrix);

  for (int num_solves = 0; num_solves < 10; ++num_solves) {
    // Generate a random rhs.
    DenseColumn rhs(kNumRows, 0.0);
    for (RowIndex row(0); row < kNumRows; ++row) {
      if (absl::Bernoulli(randomizer, 1.0 / 100)) {
        rhs[row] = absl::Uniform<double>(randomizer, -10.0, 10.0);
      }
    }

    // First do a normal lower solve.
    DenseColumn result_left = rhs;
    matrix.LowerSolve(&result_left);

    // It should be the same as a transpose upper solve.
    DenseColumn other_result = rhs;
    transpose_matrix.TransposeUpperSolve(&other_result);
    ExpectFractionalVectorComparable(result_left, other_result);

    // Now do the same for a right solve.
    DenseColumn result_right = rhs;
    transpose_matrix.UpperSolve(&result_right);

    other_result = rhs;
    matrix.TransposeLowerSolve(&other_result);
    ExpectFractionalVectorComparable(result_right, other_result);

    CheckHyperSparseSolves(matrix, transpose_matrix, rhs, result_left,
                           result_right);
  }
}

TEST(TriangularMatrixTest, RandomTriangularSolvesWithDiagonalOfOne) {
  const ColIndex kNumCols(10000);
  const RowIndex kNumRows(kNumCols.value());
  std::mt19937 randomizer(12345);

  // Generate a random lower sparse triangular matrix with a diagonal of ones.
  // Note that the order of the entries is not sorted to test that the functions
  // handle this case properly (even if it will be slower).
  TriangularMatrix matrix;
  matrix.Reset(kNumRows, kNumCols);
  for (ColIndex col(0); col < kNumCols; ++col) {
    SparseColumn sparse_column;
    const RowIndex diagonal_row(col.value());
    int num_under_diagonal_entries =
        kNumRows.value() - diagonal_row.value() - 1;
    for (int i = 0; i < num_under_diagonal_entries; ++i) {
      if (absl::Bernoulli(randomizer, 1.0 / 5000)) {
        int shifted_index = (col.value() + i) % num_under_diagonal_entries;
        sparse_column.SetCoefficient(
            diagonal_row + 1 + RowIndex(shifted_index),
            absl::Uniform<double>(randomizer, -10.0, 10.0));
      }
    }
    matrix.AddTriangularColumnWithGivenDiagonalEntry(sparse_column,
                                                     diagonal_row, 1.0);
  }
  TriangularMatrix transpose_matrix;
  transpose_matrix.PopulateFromTranspose(matrix);

  // First do non-permuted solves and store the input/output vectors.
  std::vector<DenseColumn> inputs;
  std::vector<DenseColumn> outputs;
  for (int num_solves = 0; num_solves < 10; ++num_solves) {
    // Generate a random rhs.
    DenseColumn rhs(kNumRows, 0.0);
    for (RowIndex row(0); row < kNumRows; ++row) {
      if (absl::Bernoulli(randomizer, 1.0 / 100)) {
        rhs[row] = absl::Uniform<double>(randomizer, -10.0, 10.0);
      }
    }
    inputs.push_back(rhs);

    // First do a normal lower solve.
    DenseColumn result_left = rhs;
    matrix.LowerSolve(&result_left);
    outputs.push_back(result_left);

    // Then do a right lower solve, by doing an upper solve with the transpose.
    DenseColumn result_right = rhs;
    transpose_matrix.UpperSolve(&result_right);

    CheckHyperSparseSolves(matrix, transpose_matrix, rhs, result_left,
                           result_right);
  }

  // Do the permuted sparse lower solve version.
  RowPermutation row_perm(kNumRows);
  row_perm.PopulateRandomly();
  RowPermutation inverse_row_perm;
  inverse_row_perm.PopulateFromInverse(row_perm);

  // The function supports a partial permutation:
  // if perm[i] < 0 then column perm[i] is assumed to be an identity column.
  RowPermutation partial_inverse_row_perm;
  partial_inverse_row_perm.PopulateFromInverse(row_perm);
  for (RowIndex row(0); row < kNumRows; ++row) {
    if (matrix.ColumnIsDiagonalOnly(
            RowToColIndex(partial_inverse_row_perm[row]))) {
      partial_inverse_row_perm[row] = kInvalidRow;
    }
  }

  matrix.ApplyRowPermutationToNonDiagonalEntries(row_perm);
  for (int i = 0; i < inputs.size(); ++i) {
    SparseColumn sparse_vector;
    sparse_vector.PopulateFromDenseVector(inputs[i]);
    sparse_vector.ApplyRowPermutation(row_perm);
    matrix.PermutedLowerSparseSolve(ColumnView(sparse_vector),
                                    partial_inverse_row_perm, &sparse_vector,
                                    &sparse_vector);
    sparse_vector.ApplyRowPermutation(inverse_row_perm);

    DenseColumn result;
    sparse_vector.CopyToDenseVector(kNumRows, &result);
    ExpectFractionalVectorComparable(result, outputs[i]);
  }
}

#ifndef NDEBUG
TEST(TriangularMatrixDeathTest, NonTriangularMatrix) {
  // The matrix is the 2x2 all ones matrix.
  TriangularMatrix matrix;
  SparseColumn column;
  column.SetCoefficient(RowIndex(0), 1.0);
  column.SetCoefficient(RowIndex(1), 1.0);

  matrix.Reset(RowIndex(2), ColIndex(2));
  matrix.AddTriangularColumn(ColumnView(column), RowIndex(0));
  matrix.AddTriangularColumn(ColumnView(column), RowIndex(1));

  // This tests the anti-cycle detection in debug mode.
  RowPermutation perm(RowIndex(2));
  perm.PopulateFromIdentity();

  RowIndexVector rows;
  ASSERT_DEATH(matrix.PermutedComputeRowsToConsider(ColumnView(column), perm,
                                                    &rows, &rows),
               "");
}
#endif

TEST(TriangularMatrixTest, InverseInfinityNormOfIdentity) {
  const ColIndex kNumCols(10);
  SparseMatrix matrix;
  matrix.PopulateFromIdentity(kNumCols);

  TriangularMatrix triangular_matrix;
  triangular_matrix.PopulateFromTriangularSparseMatrix(matrix);

  const Fractional norm = triangular_matrix.ComputeInverseInfinityNorm();
  EXPECT_EQ(norm, 1.0);

  const Fractional norm_bound =
      triangular_matrix.ComputeInverseInfinityNormUpperBound();
  EXPECT_EQ(norm_bound, 1.0);
}

TEST(TriangularMatrixTest, InverseInfinityNormOfUpperMatrix) {
  const SparseMatrix kUpperMatrix{{1, 0, 0, 0, 8, 0, 0}, {0, 2, 0, 7, 0, 0, 0},
                                  {0, 0, 3, 0, 0, 9, 0}, {0, 0, 0, 1, 0, 0, 10},
                                  {0, 0, 0, 0, 1, 0, 0}, {0, 0, 0, 0, 0, 4, 0},
                                  {0, 0, 0, 0, 0, 0, 1}};
  /*
  Inverse of kUpperMatrix.
  {{ 1, 0, 0, 0, -8, 0, 0 },
  {  0, 0.5, 0, -3.5, 0, 0, 35 },
  {  0, 0, 0.333, 0, 0, -0.75, 0 },
  {  0, 0, 0, 1, 0, 0, -10},
  {  0, 0, 0, 0, 1, 0, 0},
  {  0, 0, 0, 0, 0, 0.25, 0 },
  {  0, 0, 0, 0, 0, 0, 1 }}
  */
  TriangularMatrix triangular_matrix;
  triangular_matrix.PopulateFromTriangularSparseMatrix(kUpperMatrix);

  Fractional norm = triangular_matrix.ComputeInverseInfinityNorm();
  EXPECT_EQ(norm, 39);  // Corresponds to the second row of the inverse.

  Fractional norm_bound =
      triangular_matrix.ComputeInverseInfinityNormUpperBound();
  EXPECT_EQ(norm_bound, 39);
}

TEST(TriangularMatrixTest, InverseInfinityNormOfLowerMatrix) {
  const SparseMatrix kLowerMatrix{
      {1, 0, 0, 0, 0, 0, 0},  {0, 1, 0, 0, 0, 0, 0},  {0, 0, 1, 0, 0, 0, 0},
      {0, -5, 0, 1, 0, 0, 0}, {0, 0, 13, 0, 1, 0, 0}, {0, 0, 0, 9, 0, 1, 0},
      {0, 0, 0, 0, 0, 0, 1}};
  /*
  Inverse of kLowerMatrix.
  {{ 1, 0, 0, 0, 0, 0, 0},
  { 0, 1, 0, 0, 0, 0, 0 },
  { 0, 0, 1, 0, 0, 0, 0 },
  { 0, 5, 0, 1, 0, 0, 0},
  { 0, 0, -13, 0, 1, 0, 0},
  { 0, -45, 0, -9, 0, 1, 0 },
  { 0, 0, 0, 0, 0, 0, 1 }}
  */

  TriangularMatrix triangular_matrix;
  triangular_matrix.PopulateFromTriangularSparseMatrix(kLowerMatrix);

  Fractional norm = triangular_matrix.ComputeInverseInfinityNorm();
  EXPECT_EQ(norm, 55);  // Corresponds to the sixth row of the inverse.

  Fractional norm_bound =
      triangular_matrix.ComputeInverseInfinityNormUpperBound();
  EXPECT_EQ(norm_bound, 55);
}

void TestInverseInfinityNormOnRandomMatrix(RowIndex num_rows,
                                           absl::BitGenRef randomizer,
                                           bool is_lower) {
  SparseMatrix matrix;

  for (int i = 0; i < 10; ++i) {
    if (is_lower) {
      FillSparseMatrixWithRandomLowerTriangularMatrix(num_rows, /*density=*/0.5,
                                                      randomizer, &matrix);
    } else {
      FillSparseMatrixWithRandomUpperTriangularMatrix(num_rows, /*density=*/0.5,
                                                      randomizer, &matrix);
    }

    TriangularMatrix triangular_matrix;
    triangular_matrix.PopulateFromTriangularSparseMatrix(matrix);

    const Fractional norm = triangular_matrix.ComputeInverseInfinityNorm();
    const Fractional norm_ub =
        triangular_matrix.ComputeInverseInfinityNormUpperBound();

    EXPECT_LT(norm, norm_ub);
    LOG(INFO) << "exact norm: " << norm << " upper_bound: " << norm_ub;
  }
}

TEST(TriangularMatrixTest, InverseInfinityNormOfRandomMatrixUpper) {
  const RowIndex kNumRows(100);
  std::mt19937 randomizer(12345);
  TestInverseInfinityNormOnRandomMatrix(kNumRows, randomizer,
                                        /*is_lower=*/false);
}

TEST(TriangularMatrixTest, InverseInfinityNormOfRandomMatrixLower) {
  const RowIndex kNumRows(100);
  std::mt19937 randomizer(12345);
  TestInverseInfinityNormOnRandomMatrix(kNumRows, randomizer,
                                        /*is_lower=*/true);
}

}  // namespace
}  // namespace glop
}  // namespace operations_research
