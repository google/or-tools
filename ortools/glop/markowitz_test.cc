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

#include "ortools/glop/markowitz.h"

#include <cstdint>
#include <random>
#include <vector>

#include "absl/random/distributions.h"
#include "gtest/gtest.h"
#include "ortools/graph_base/iterators.h"
#include "ortools/lp_data/lp_test_utils.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/permutation.h"
#include "ortools/lp_data/sparse.h"
#include "ortools/lp_data/sparse_column.h"

namespace operations_research {
namespace glop {
namespace {

TEST(ColumnPriorityQueueTest, BasicTest) {
  ColumnPriorityQueue pq;
  pq.Reset(/*max_degree=*/10, /*num_cols=*/ColIndex(10));
  pq.PushOrAdjust(ColIndex(2), 3);
  pq.PushOrAdjust(ColIndex(3), 2);
  pq.PushOrAdjust(ColIndex(4), 1);
  pq.PushOrAdjust(ColIndex(4), 4);

  EXPECT_EQ(pq.Pop(), ColIndex(3));
  EXPECT_EQ(pq.Pop(), ColIndex(2));
  EXPECT_EQ(pq.Pop(), ColIndex(4));
  EXPECT_EQ(pq.Pop(), kInvalidCol);
}

// SparseMatrixWrapperForTest provides a simple interface for converting from
// SparseMatrix to CompactSparseMatrix and CompactSparseMatrixView in a way that
// otherwise is not used in production.
class SparseMatrixWrapperForTest {
 public:
  explicit SparseMatrixWrapperForTest(const SparseMatrix* matrix)
      : compact_matrix_(*matrix) {
    const auto identity_mapping = ::util::IntegerRange<ColIndex>(
        ColIndex(0), ColIndex(matrix->num_cols()));
    basis_ = RowToColMapping(identity_mapping.begin(), identity_mapping.end());
  }

  CompactSparseMatrixView AsView() const {
    return CompactSparseMatrixView(&compact_matrix_, &basis_);
  }

 private:
  CompactSparseMatrix compact_matrix_;
  RowToColMapping basis_;
};

TEST(MarkowitzTest, EmptyMatrix) {
  SparseMatrix matrix;
  const auto wrapped_matrix = SparseMatrixWrapperForTest(&matrix);
  Markowitz markowitz;
  RowPermutation row_perm(RowIndex(10));
  ColumnPermutation col_perm(ColIndex(100));
  EXPECT_TRUE(markowitz
                  .ComputeRowAndColumnPermutation(wrapped_matrix.AsView(),
                                                  &row_perm, &col_perm)
                  .ok());
  EXPECT_TRUE(row_perm.empty());
  EXPECT_TRUE(col_perm.empty());
}

TEST(MarkowitzTest, IdentityMatrix) {
  const ColIndex num_cols(100);
  const RowIndex num_rows(ColToRowIndex(num_cols));
  SparseMatrix matrix;
  matrix.PopulateFromIdentity(num_cols);
  const auto wrapped_matrix = SparseMatrixWrapperForTest(&matrix);

  Markowitz markowitz;
  RowPermutation row_perm;
  ColumnPermutation col_perm;
  EXPECT_TRUE(markowitz
                  .ComputeRowAndColumnPermutation(wrapped_matrix.AsView(),
                                                  &row_perm, &col_perm)
                  .ok());
  EXPECT_EQ(num_cols.value(), col_perm.size());
  for (ColIndex col(0); col < num_cols; ++col) {
    EXPECT_EQ(col, col_perm[col]);
  }
  EXPECT_EQ(num_rows.value(), row_perm.size());
  for (RowIndex row(0); row < num_rows; ++row) {
    EXPECT_EQ(row, row_perm[row]);
  }
}

TEST(MarkowitzTest, SimpleSingularMatrix) {
  const ColIndex num_cols(100);
  SparseMatrix matrix;
  matrix.PopulateFromIdentity(num_cols);
  matrix.mutable_column(ColIndex(40))->Clear();
  const auto wrapped_matrix = SparseMatrixWrapperForTest(&matrix);
  Markowitz markowitz;
  RowPermutation row_perm;
  ColumnPermutation col_perm;
  EXPECT_FALSE(markowitz
                   .ComputeRowAndColumnPermutation(wrapped_matrix.AsView(),
                                                   &row_perm, &col_perm)
                   .ok());

  // We still get a maximum non-singular set of columns:
  int num_columns_used = 0;
  for (ColIndex col(0); col < num_cols; ++col) {
    if (col_perm[col] != kInvalidCol) ++num_columns_used;
  }
  EXPECT_EQ(kInvalidCol, col_perm[ColIndex(40)]);
  EXPECT_EQ(num_cols - 1, num_columns_used);
}

TEST(MarkowitzTest, SingletonSingularMatrix) {
  const ColIndex num_cols(100);
  SparseMatrix matrix;
  matrix.PopulateFromIdentity(num_cols);
  matrix.mutable_column(ColIndex(50))->Clear();
  matrix.mutable_column(ColIndex(50))->SetCoefficient(RowIndex(10), 1.0);
  const auto wrapped_matrix = SparseMatrixWrapperForTest(&matrix);

  Markowitz markowitz;
  RowPermutation row_perm;
  ColumnPermutation col_perm;
  EXPECT_FALSE(markowitz
                   .ComputeRowAndColumnPermutation(wrapped_matrix.AsView(),
                                                   &row_perm, &col_perm)
                   .ok());

  // We still get a maximum non-singular set of columns:
  int num_columns_used = 0;
  for (ColIndex col(0); col < num_cols; ++col) {
    if (col_perm[col] != kInvalidCol) ++num_columns_used;
  }
  EXPECT_EQ(kInvalidCol, col_perm[ColIndex(50)]);
  EXPECT_EQ(num_cols - 1, num_columns_used);
}

TEST(MarkowitzTest, SingularMatrixWithProportionalColumns) {
  const ColIndex num_cols(100);
  const RowIndex num_rows(ColToRowIndex(num_cols));
  SparseMatrix matrix;
  matrix.PopulateFromIdentity(num_cols);
  DenseColumn duplicate(num_rows, 1.0);
  matrix.mutable_column(ColIndex(50))->PopulateFromDenseVector(duplicate);
  matrix.mutable_column(ColIndex(80))->PopulateFromDenseVector(duplicate);
  matrix.mutable_column(ColIndex(88))->PopulateFromDenseVector(duplicate);
  matrix.mutable_column(ColIndex(88))->MultiplyByConstant(3.14);
  const auto wrapped_matrix = SparseMatrixWrapperForTest(&matrix);

  Markowitz markowitz;
  RowPermutation row_perm;
  ColumnPermutation col_perm;
  EXPECT_FALSE(markowitz
                   .ComputeRowAndColumnPermutation(wrapped_matrix.AsView(),
                                                   &row_perm, &col_perm)
                   .ok());

  // We still get a maximum non-singular set of columns:
  int num_columns_used = 0;
  for (ColIndex col(0); col < num_cols; ++col) {
    if (col_perm[col] != kInvalidCol) ++num_columns_used;
  }
  // 50, 80 and 88 are all proportional, so only one of them will be chosen.
  EXPECT_EQ(kInvalidCol, col_perm[ColIndex(80)]);
  EXPECT_EQ(kInvalidCol, col_perm[ColIndex(50)]);
  EXPECT_EQ(num_cols - 2, num_columns_used);
}

TEST(MarkowitzTest, SingularMatrixWithProportionalRows) {
  const ColIndex num_cols(100);
  SparseMatrix matrix;
  matrix.PopulateFromIdentity(num_cols);
  matrix.mutable_column(ColIndex(10))->Clear();
  matrix.mutable_column(ColIndex(20))->Clear();
  for (ColIndex col(0); col < num_cols; ++col) {
    matrix.mutable_column(col)->SetCoefficient(RowIndex(10), 2.0);
    matrix.mutable_column(col)->SetCoefficient(RowIndex(20), -1.0);
  }
  matrix.CleanUp();
  const auto wrapped_matrix = SparseMatrixWrapperForTest(&matrix);

  Markowitz markowitz;
  RowPermutation row_perm;
  ColumnPermutation col_perm;
  EXPECT_FALSE(markowitz
                   .ComputeRowAndColumnPermutation(wrapped_matrix.AsView(),
                                                   &row_perm, &col_perm)
                   .ok());

  // We still get a maximum non-singular set of columns:
  int num_columns_used = 0;
  for (ColIndex col(0); col < num_cols; ++col) {
    if (col_perm[col] != kInvalidCol) ++num_columns_used;
  }
  EXPECT_EQ(kInvalidRow, row_perm[RowIndex(20)]);
  EXPECT_EQ(num_cols - 1, num_columns_used);
}

TEST(MarkowitzTest, PermutedDenseUpperTriangularMatrix) {
  const ColIndex num_cols(100);
  const RowIndex num_rows(ColToRowIndex(num_cols));
  SparseMatrix matrix;
  matrix.PopulateFromIdentity(num_cols);
  for (ColIndex col(0); col < num_cols; ++col) {
    for (RowIndex row(0); row < col.value(); ++row) {
      matrix.mutable_column(col)->SetCoefficient(row, 1.0);
    }
    matrix.mutable_column(col)->CheckNoDuplicates();
  }

  RowPermutation initial_row_perm(num_rows);
  ColumnPermutation initial_col_perm(num_cols);
  initial_row_perm.PopulateRandomly();
  initial_col_perm.PopulateRandomly();
  RowPermutation initial_inverse_row_perm;
  ColumnPermutation initial_inverse_col_perm;
  initial_inverse_row_perm.PopulateFromInverse(initial_row_perm);
  initial_inverse_col_perm.PopulateFromInverse(initial_col_perm);

  // Note that there is just one way to put the matrix back into upper
  // triangular form.
  SparseMatrix permuted_matrix;
  permuted_matrix.PopulateFromPermutedMatrix(matrix, initial_row_perm,
                                             initial_inverse_col_perm);
  permuted_matrix.CleanUp();
  const auto wrapped_matrix = SparseMatrixWrapperForTest(&permuted_matrix);

  Markowitz markowitz;
  SparseMatrix lower, upper;
  TriangularMatrix compact_lower, compact_upper;
  RowPermutation row_perm;
  ColumnPermutation col_perm;
  EXPECT_TRUE(markowitz
                  .ComputeLU(wrapped_matrix.AsView(), &row_perm, &col_perm,
                             &compact_lower, &compact_upper)
                  .ok());
  compact_lower.CopyToSparseMatrix(&lower);
  compact_upper.CopyToSparseMatrix(&upper);

  for (ColIndex col(0); col < num_cols; ++col) {
    const RowIndex row = ColToRowIndex(col);
    EXPECT_EQ(row_perm[row], initial_inverse_row_perm[row]);
    EXPECT_EQ(col_perm[col], initial_inverse_col_perm[col]);
    EXPECT_EQ(1, lower.column(col).num_entries());
  }

  SparseMatrix paq, product;
  ColumnPermutation inverse_col_perm;
  inverse_col_perm.PopulateFromInverse(col_perm);
  paq.PopulateFromPermutedMatrix(permuted_matrix, row_perm, inverse_col_perm);
  product.PopulateFromProduct(lower, upper);
  EXPECT_TRUE(paq.Equals(product, 1e-10));
}

TEST(MarkowitzTest, RandomSparseMatrix) {
  SparseMatrix matrix;
  const ColIndex num_cols(200);
  const RowIndex num_rows(ColToRowIndex(num_cols));
  std::mt19937 randomizer(12345);
  FillSparseMatrixRandomly(num_rows, num_cols, /*density=*/0.05, randomizer,
                           &matrix);
  const auto wrapped_matrix = SparseMatrixWrapperForTest(&matrix);

  Markowitz markowitz;
  SparseMatrix lower, upper;
  TriangularMatrix compact_lower, compact_upper;
  RowPermutation row_perm;
  ColumnPermutation col_perm;
  EXPECT_TRUE(markowitz
                  .ComputeLU(wrapped_matrix.AsView(), &row_perm, &col_perm,
                             &compact_lower, &compact_upper)
                  .ok());
  compact_lower.CopyToSparseMatrix(&lower);
  compact_upper.CopyToSparseMatrix(&upper);

  SparseMatrix paq, product;
  ColumnPermutation inverse_col_perm;
  inverse_col_perm.PopulateFromInverse(col_perm);
  paq.PopulateFromPermutedMatrix(matrix, row_perm, inverse_col_perm);
  product.PopulateFromProduct(lower, upper);
  EXPECT_TRUE(paq.Equals(product, 1e-10));

  // So we know when we do a change that impacts the decomposition.
  EXPECT_EQ(1981, matrix.num_entries());
  EXPECT_NEAR(5627, lower.num_entries().value(), 10);
  EXPECT_NEAR(5697, upper.num_entries().value(), 10);
}

TEST(MarkowitzTest, RandomWideNonSquareMatrix) {
  SparseMatrix matrix;
  const ColIndex num_cols(1000);
  const RowIndex num_rows(200);
  std::mt19937 randomizer(12345);
  FillSparseMatrixRandomly(num_rows, num_cols, /*density=*/0.5, randomizer,
                           &matrix);

  // Clear some columns.
  for (ColIndex col(0); col < num_cols; ++col) {
    if (absl::Bernoulli(randomizer, 0.1)) {
      matrix.mutable_column(col)->Clear();
    }
  }
  const auto wrapped_matrix = SparseMatrixWrapperForTest(&matrix);

  // Check that we get the factorization of a square sumatrix.
  Markowitz markowitz;
  RowPermutation row_perm;
  ColumnPermutation col_perm;

  EXPECT_TRUE(markowitz
                  .ComputeRowAndColumnPermutation(wrapped_matrix.AsView(),
                                                  &row_perm, &col_perm)
                  .ok());
  EXPECT_EQ(row_perm.size(), num_rows);
  EXPECT_EQ(col_perm.size(), num_cols);

  int num_columns_used = 0;
  for (ColIndex col(0); col < num_cols; ++col) {
    if (col_perm[col] != kInvalidCol) ++num_columns_used;
  }
  EXPECT_EQ(RowToColIndex(num_rows), num_columns_used);
}

TEST(MarkowitzTest, RandomTallNonSquareMatrix) {
  SparseMatrix matrix;
  const ColIndex num_cols(150);
  const RowIndex num_rows(200);
  std::mt19937 randomizer(12345);
  FillSparseMatrixRandomly(num_rows, num_cols, /*density=*/0.5, randomizer,
                           &matrix);
  const auto wrapped_matrix = SparseMatrixWrapperForTest(&matrix);

  // Check that we get the factorization of all 150 columns.
  Markowitz markowitz;
  RowPermutation row_perm;
  ColumnPermutation col_perm;
  EXPECT_TRUE(markowitz
                  .ComputeRowAndColumnPermutation(wrapped_matrix.AsView(),
                                                  &row_perm, &col_perm)
                  .ok());
  EXPECT_EQ(row_perm.size(), num_rows);
  EXPECT_EQ(col_perm.size(), num_cols);

  // Check that all 150 columns are assigned.
  int num_columns_used = 0;
  for (ColIndex col(0); col < num_cols; ++col) {
    if (col_perm[col] != kInvalidCol) ++num_columns_used;
  }
  EXPECT_EQ(num_cols, num_columns_used);

  // Check that if we add 50 singleton columns with their ones at the position
  // given by the unassigned elements of row_perm, then we get a non-singular
  // square matrix.
  for (RowIndex row(0); row < num_rows; ++row) {
    if (row_perm[row] == kInvalidRow) {
      matrix.AppendUnitVector(row, 1.0);
    }
  }
  const auto other_wrapped_matrix = SparseMatrixWrapperForTest(&matrix);
  EXPECT_TRUE(markowitz
                  .ComputeRowAndColumnPermutation(other_wrapped_matrix.AsView(),
                                                  &row_perm, &col_perm)
                  .ok());
  EXPECT_EQ(row_perm.size(), num_rows);
  EXPECT_EQ(col_perm.size(), RowToColIndex(num_rows));
}

TEST(MatrixNonZeroPatternTest, AddEntryAndDegreeComputation) {
  MatrixNonZeroPattern tested_class;
  const int kSize = 1000;
  tested_class.Reset(RowIndex(kSize), ColIndex(kSize));
  for (int i = 0; i < kSize; ++i) {
    for (int j = 0; j < kSize; ++j) {
      if ((i + j) % 2 == 0) {
        tested_class.AddEntry(RowIndex(i), ColIndex(j));
      }
    }
  }
  for (int i = 0; i < kSize; ++i) {
    EXPECT_EQ(kSize / 2, tested_class.ColDegree(ColIndex(i)));
    EXPECT_EQ(kSize / 2, tested_class.RowDegree(RowIndex(i)));
  }
}

TEST(MatrixNonZeroPatternTest, RemoveDeletedColumnsFromRow) {
  MatrixNonZeroPattern tested_class;
  const RowIndex kRow(0);
  const int kSize = 1000;
  tested_class.Reset(RowIndex(kSize), ColIndex(kSize));
  std::vector<bool> row_entries(kSize, true);
  for (int i = 0; i < kSize; ++i) {
    tested_class.AddEntry(kRow, ColIndex(i));
  }

  // Remove kSize entries at random (removing an already removed entry does
  // nothing).
  int num_present = kSize;
  std::mt19937 randomizer(12345);
  for (int i = 0; i < kSize; ++i) {
    const ColIndex col_to_remove =
        ColIndex(absl::Uniform(randomizer, 0, kSize));
    if (!tested_class.IsColumnDeleted(col_to_remove)) {
      tested_class.DeleteRowAndColumn(RowIndex(1), col_to_remove);
    }
    if (absl::Bernoulli(randomizer, 0.1)) {
      tested_class.RemoveDeletedColumnsFromRow(kRow);
    }
    if (row_entries[col_to_remove.value()] == true) {
      --num_present;
      row_entries[col_to_remove.value()] = false;
    }
  }
  tested_class.RemoveDeletedColumnsFromRow(kRow);

  EXPECT_EQ(num_present, tested_class.RowNonZero(kRow).size());
  for (const ColIndex col : tested_class.RowNonZero(kRow)) {
    EXPECT_TRUE(row_entries[col.value()]);
  }
}

TEST(MatrixNonZeroPatternTest, GetFirstNonDeletedColumnFromRow) {
  MatrixNonZeroPattern tested_class;
  const RowIndex kRow(0);
  const int kSize = 1000;
  tested_class.Reset(RowIndex(kSize), ColIndex(kSize));

  EXPECT_EQ(kInvalidCol, tested_class.GetFirstNonDeletedColumnFromRow(kRow));

  tested_class.AddEntry(kRow, ColIndex(1));
  tested_class.AddEntry(kRow, ColIndex(2));
  tested_class.AddEntry(kRow, ColIndex(4));

  EXPECT_EQ(ColIndex(1), tested_class.GetFirstNonDeletedColumnFromRow(kRow));
  tested_class.DeleteRowAndColumn(RowIndex(1), ColIndex(1));
  EXPECT_EQ(ColIndex(2), tested_class.GetFirstNonDeletedColumnFromRow(kRow));
  tested_class.DeleteRowAndColumn(RowIndex(1), ColIndex(2));
  EXPECT_EQ(ColIndex(4), tested_class.GetFirstNonDeletedColumnFromRow(kRow));
  tested_class.DeleteRowAndColumn(RowIndex(1), ColIndex(4));
  EXPECT_EQ(kInvalidCol, tested_class.GetFirstNonDeletedColumnFromRow(kRow));
}

TEST(MatrixNonZeroPatternTest, SimpleUpdate) {
  MatrixNonZeroPattern tested_class;
  const int kSize = 1000;
  tested_class.Reset(RowIndex(kSize), ColIndex(kSize));

  // Create a matrix with the first column and the first row containing only
  // 1.0 and the other coefficients set to 0.0.
  SparseColumn first_column;
  for (int i = 0; i < kSize; ++i) {
    tested_class.AddEntry(RowIndex(0), ColIndex(i));
    if (i != 0) {
      first_column.SetCoefficient(RowIndex(i), 1.0);
      tested_class.AddEntry(RowIndex(i), ColIndex(0));
    }
  }
  EXPECT_TRUE(first_column.CheckNoDuplicates());

  // Update it using (0, 0) as a pivot.
  tested_class.DeleteRowAndColumn(RowIndex(0), ColIndex(0));
  tested_class.Update(RowIndex(0), ColIndex(0), first_column);
  for (int i = 1; i < kSize; ++i) {
    const RowIndex row(i);
    tested_class.DecreaseRowDegree(row);
  }

  // Test that we have a full 1 matrix on the lower right.
  for (int i = 1; i < kSize; ++i) {
    EXPECT_EQ(kSize - 1, tested_class.ColDegree(ColIndex(i)));
    EXPECT_EQ(kSize - 1, tested_class.RowDegree(RowIndex(i)));
  }
}

TEST(ColumnPriorityQueueTest, RandomUpdates) {
  const int kNumCols = 1000;
  const int kNumUpdates = 10000;
  std::mt19937 randomizer(12345);
  ColumnPriorityQueue queue;
  queue.Reset(kNumCols, ColIndex(kNumCols));
  StrictITIVector<ColIndex, int32_t> col_degree(ColIndex(kNumCols), 0);
  int reference_size = 0;
  for (int i = 0; i < kNumUpdates; ++i) {
    const ColIndex col = ColIndex(absl::Uniform(randomizer, 0, kNumCols));
    const int degree = absl::Uniform(randomizer, 0, kNumCols - 1) + 1;
    queue.PushOrAdjust(col, degree);
    if (col_degree[col] == 0) {
      ++reference_size;
    }
    col_degree[col] = degree;
  }

  int size = 0;
  int old_degree = 0;
  ColIndex old_col(0);
  while (true) {
    const ColIndex col = queue.Pop();
    if (col == kInvalidCol) break;
    const int degree = col_degree[col];
    EXPECT_GE(degree, old_degree);
    old_col = col;
    old_degree = degree;
    ++size;
  }
  EXPECT_EQ(reference_size, size);
}

TEST(SparseMatrixWithReusableColumnMemoryTest, RandomUpdates) {
  const int kNumCols = 1000;
  const int kNumUpdates = 100000;
  std::mt19937 randomizer(12345);

  SparseMatrixWithReusableColumnMemory matrix;
  matrix.Reset(ColIndex(kNumCols));

  int num_empty = 0;
  for (int i = 0; i < kNumUpdates; ++i) {
    const ColIndex col = ColIndex(absl::Uniform(randomizer, 0, kNumCols));
    if (matrix.column(col).IsEmpty()) {
      num_empty++;
      matrix.mutable_column(col)->SetCoefficient(RowIndex(col.value()), 1.0);
    } else {
      EXPECT_EQ(RowIndex(col.value()), matrix.column(col).GetFirstRow());
      if (absl::Bernoulli(randomizer, 0.5)) {
        matrix.ClearAndReleaseColumn(col);
      }
    }
  }

  // Because we have a chance to release the column, we see quite a lot of
  // empty columns, but not all of them...
  EXPECT_EQ(34005, num_empty);
}

TEST(ColumnPriorityQueueTest, PopOnEmptyQueue) {
  ColumnPriorityQueue queue;
  queue.Reset(/*max_degree=*/10, /*num_cols=*/ColIndex(10));
  EXPECT_EQ(queue.Pop(), kInvalidCol);
  EXPECT_EQ(queue.Pop(), kInvalidCol);

  queue.PushOrAdjust(ColIndex(1), 2);
  queue.PushOrAdjust(ColIndex(1), 5);
  queue.PushOrAdjust(ColIndex(2), 2);
  queue.PushOrAdjust(ColIndex(3), 3);
  EXPECT_EQ(queue.Pop(), ColIndex(2));
  EXPECT_EQ(queue.Pop(), ColIndex(3));
  EXPECT_EQ(queue.Pop(), ColIndex(1));
  EXPECT_EQ(queue.Pop(), kInvalidCol);
  EXPECT_EQ(queue.Pop(), kInvalidCol);
}

}  // namespace
}  // namespace glop
}  // namespace operations_research
