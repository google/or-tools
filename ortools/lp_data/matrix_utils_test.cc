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

#include "ortools/lp_data/matrix_utils.h"

#include <random>

#include "absl/random/distributions.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/lp_data/lp_test_utils.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/sparse.h"
#include "ortools/lp_data/sparse_column.h"

namespace operations_research {
namespace glop {
namespace {

using ::testing::ContainerEq;
using ::testing::Each;
using ::testing::Eq;

const Fractional kTestTolerance = 1e-10;

TEST(FindProportionalColumnsTest, EmptyMatrix) {
  SparseMatrix matrix;
  ColMapping mapping = FindProportionalColumns(matrix, kTestTolerance);
  EXPECT_TRUE(mapping.empty());
  mapping = FindProportionalColumnsUsingSimpleAlgorithm(matrix, kTestTolerance);
  EXPECT_TRUE(mapping.empty());
}

TEST(FindProportionalColumnsTest, NoProportionalColumnsForIdentity) {
  const ColIndex kNumCols(100);
  SparseMatrix matrix;
  matrix.PopulateFromIdentity(kNumCols);

  const ColMapping mapping =
      FindProportionalColumnsUsingSimpleAlgorithm(matrix, kTestTolerance);
  EXPECT_EQ(mapping.size(), kNumCols);
  EXPECT_THAT(mapping, Each(Eq(kInvalidCol)));

  const ColMapping other_mapping =
      FindProportionalColumns(matrix, kTestTolerance);
  EXPECT_THAT(other_mapping, ContainerEq(mapping));
}

TEST(FindProportionalColumnsTest, NoProportionalColumnsOnRandomDenseMatrix) {
  const ColIndex kNumCols(100);
  const RowIndex kNumRows(100);
  SparseMatrix matrix;
  matrix.PopulateFromZero(kNumRows, kNumCols);
  std::mt19937 randomizer(12345);
  for (ColIndex col(0); col < kNumCols; ++col) {
    for (RowIndex row(0); row < kNumRows; ++row) {
      matrix.mutable_column(col)->SetCoefficient(
          row, absl::Uniform<float>(randomizer, -10.0, 10.0));
    }
  }

  const ColMapping mapping =
      FindProportionalColumnsUsingSimpleAlgorithm(matrix, kTestTolerance);
  EXPECT_EQ(mapping.size(), kNumCols);
  EXPECT_THAT(mapping, Each(Eq(kInvalidCol)));

  const ColMapping other_mapping =
      FindProportionalColumns(matrix, kTestTolerance);
  EXPECT_THAT(other_mapping, ContainerEq(mapping));
}

TEST(FindProportionalColumnsTest, SingletonAreAlwaysProportional) {
  const ColIndex kNumCols(1000);
  SparseMatrix matrix;
  std::mt19937 randomizer(12345);
  matrix.PopulateFromZero(RowIndex(1), kNumCols);
  for (ColIndex col(0); col < kNumCols; ++col) {
    matrix.mutable_column(col)->SetCoefficient(
        RowIndex(0), absl::Uniform<float>(randomizer, -10.0, 10.0));
  }

  const ColMapping mapping =
      FindProportionalColumnsUsingSimpleAlgorithm(matrix, kTestTolerance);
  EXPECT_EQ(mapping.size(), kNumCols);
  EXPECT_EQ(kInvalidCol, mapping[ColIndex(0)]);
  for (ColIndex col(1); col < kNumCols; ++col) {
    EXPECT_EQ(ColIndex(0), mapping[col]);
  }

  const ColMapping other_mapping =
      FindProportionalColumns(matrix, kTestTolerance);
  EXPECT_THAT(other_mapping, ContainerEq(mapping));
}

TEST(FindProportionalColumnsTest, ComplexCase) {
  const ColIndex kNumCols(100);
  const RowIndex kNumRows(20);
  const ColIndex kEquivClass(10);

  SparseMatrix reference_columns;
  std::mt19937 randomizer(12345);
  FillSparseMatrixRandomly(kNumRows, kEquivClass, /*density=*/0.2, randomizer,
                           &reference_columns);
  reference_columns.CleanUp();

  SparseMatrix matrix;
  matrix.PopulateFromZero(kNumRows, kNumCols);
  for (ColIndex col(0); col < kNumCols; ++col) {
    matrix.mutable_column(col)->PopulateFromSparseVector(
        reference_columns.column(col % kEquivClass));
    matrix.mutable_column(col)->MultiplyByConstant(
        absl::Uniform<float>(randomizer, -10.0, 10.0));
  }

  const ColMapping mapping =
      FindProportionalColumnsUsingSimpleAlgorithm(matrix, kTestTolerance);
  EXPECT_EQ(mapping.size(), kNumCols);
  for (ColIndex col(0); col < kNumCols; ++col) {
    if (matrix.column(col).IsEmpty()) continue;
    if (col < kEquivClass) {
      EXPECT_EQ(kInvalidCol, mapping[col]);
    } else {
      EXPECT_EQ(col % kEquivClass, mapping[col]);
    }
  }

  const ColMapping other_mapping =
      FindProportionalColumns(matrix, kTestTolerance);
  EXPECT_THAT(other_mapping, ContainerEq(mapping));
}

TEST(AreFirstColumnsAndRowsExactlyEqualsTest, EmptyMatrix) {
  SparseMatrix a;
  CompactSparseMatrix b;
  EXPECT_TRUE(
      AreFirstColumnsAndRowsExactlyEquals(RowIndex(0), ColIndex(0), a, b));
  EXPECT_FALSE(
      AreFirstColumnsAndRowsExactlyEquals(RowIndex(1), ColIndex(0), a, b));
  EXPECT_FALSE(
      AreFirstColumnsAndRowsExactlyEquals(RowIndex(0), ColIndex(1), a, b));
}

TEST(AreFirstColumnsAndRowsExactlyEqualsTest, EmptyColumns) {
  const ColIndex kNumCols(100);
  const RowIndex kNumRows(100);
  SparseMatrix a;
  a.PopulateFromZero(kNumRows, kNumCols);
  std::mt19937 randomizer(12345);
  for (ColIndex col(0); col < kNumCols; ++col) {
    for (RowIndex row(0); row < kNumRows; ++row) {
      a.mutable_column(col)->SetCoefficient(
          row, absl::Uniform<float>(randomizer, -10.0, 10.0));
    }
  }
  a.CleanUp();
  CompactSparseMatrix compact_a(a);

  // Same matrix is equal.
  EXPECT_TRUE(
      AreFirstColumnsAndRowsExactlyEquals(kNumRows, kNumCols, a, compact_a));

  // Lets clear a column.
  a.mutable_column(ColIndex(50))->Clear();
  EXPECT_FALSE(
      AreFirstColumnsAndRowsExactlyEquals(kNumRows, kNumCols, a, compact_a));
}

TEST(AreFirstColumnsAndRowsExactlyEqualsTest, ComplexCase) {
  const ColIndex kNumCols(100);
  const RowIndex kNumRows(100);
  SparseMatrix a;
  a.PopulateFromZero(kNumRows, kNumCols);
  std::mt19937 randomizer(12345);
  for (ColIndex col(0); col < kNumCols; ++col) {
    for (RowIndex row(0); row < kNumRows; ++row) {
      a.mutable_column(col)->SetCoefficient(
          row, absl::Uniform<float>(randomizer, -10.0, 10.0));
    }
  }
  a.CleanUp();
  CompactSparseMatrix compact_a(a);

  // Same matrix is equal.
  EXPECT_TRUE(
      AreFirstColumnsAndRowsExactlyEquals(kNumRows, kNumCols, a, compact_a));

  // matrix B is an extension of A.
  SparseMatrix b;
  const ColIndex kExtendedNumCols(kNumCols + 10);
  const RowIndex kExtendedNumRows(kNumRows + 100);
  b.PopulateFromZero(kExtendedNumRows, kExtendedNumCols);
  for (ColIndex col(0); col < kNumCols; ++col) {
    RowIndex start(0);
    if (col < kNumCols) {
      b.mutable_column(col)->PopulateFromSparseVector(a.column(col));
      start = kNumRows;
    }
    for (RowIndex row(start); row < kExtendedNumRows; ++row) {
      b.mutable_column(col)->SetCoefficient(
          row, absl::Uniform<float>(randomizer, -10.0, 10.0));
    }
  }
  b.CleanUp();
  CompactSparseMatrix compact_b(b);

  EXPECT_TRUE(
      AreFirstColumnsAndRowsExactlyEquals(kNumRows, kNumCols, a, compact_b));
  EXPECT_FALSE(AreFirstColumnsAndRowsExactlyEquals(kExtendedNumRows, kNumCols,
                                                   a, compact_b));
  EXPECT_FALSE(AreFirstColumnsAndRowsExactlyEquals(kNumRows, kExtendedNumCols,
                                                   a, compact_b));
}

TEST(IsRightMostSquareMatrixIdentityTest, EmptyMatrix) {
  const ColIndex kNumCols(100);
  const RowIndex kNumRows(100);
  SparseMatrix a;
  EXPECT_TRUE(IsRightMostSquareMatrixIdentity(a));

  a.PopulateFromZero(kNumRows, kNumCols);
  EXPECT_FALSE(IsRightMostSquareMatrixIdentity(a));
}

TEST(IsRightMostSquareMatrixIdentityTest, LessColumnsThanRows) {
  const ColIndex kNumCols(10);
  const RowIndex kNumRows(100);
  SparseMatrix a;
  a.PopulateFromZero(kNumRows, kNumCols);
  for (ColIndex col(0); col < kNumCols; ++col) {
    a.mutable_column(col)->SetCoefficient(ColToRowIndex(col), 1.0);
  }
  EXPECT_FALSE(IsRightMostSquareMatrixIdentity(a));
}

TEST(IsRightMostSquareMatrixIdentityTest, IdentityMatrix) {
  const ColIndex kNumCols(100);
  SparseMatrix a;
  a.PopulateFromIdentity(kNumCols);
  EXPECT_TRUE(IsRightMostSquareMatrixIdentity(a));
}

TEST(IsRightMostSquareMatrixIdentityTest, HasIdentityMatrix) {
  const ColIndex kNumCols(1000);
  const RowIndex kNumRows(100);
  const ColIndex kFirstIdentityColumn(900);
  SparseMatrix a;
  a.PopulateFromZero(kNumRows, kNumCols);

  for (RowIndex row(0); row < kNumRows; ++row) {
    SparseColumn* const column =
        a.mutable_column(kFirstIdentityColumn + RowToColIndex(row));
    column->SetCoefficient(row, 1.0);
  }
  EXPECT_TRUE(IsRightMostSquareMatrixIdentity(a));

  // Destroy the identity matrix by adding one more value to one of its columns.
  a.mutable_column(ColIndex(901))->SetCoefficient(RowIndex(99), 1.0);
  EXPECT_FALSE(IsRightMostSquareMatrixIdentity(a));
}

}  // namespace
}  // namespace glop
}  // namespace operations_research
