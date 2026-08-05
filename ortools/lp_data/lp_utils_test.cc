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

#include "ortools/lp_data/lp_utils.h"

#include <cmath>
#include <random>
#include <vector>

#include "absl/random/distributions.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/permutation.h"
#include "ortools/lp_data/scattered_vector.h"
#include "ortools/lp_data/sparse_column.h"
#include "ortools/util/fp_utils.h"

namespace operations_research {
namespace glop {
namespace {

using ::testing::ContainerEq;
static const Fractional kTolerance = 1e-10;

TEST(FractionalityTest, PositiveNumbers) {
  EXPECT_COMPARABLE(0.3, Fractionality(1.7), kTolerance);
  EXPECT_COMPARABLE(0.3, Fractionality(1.3), kTolerance);
  EXPECT_COMPARABLE(0.5, Fractionality(0.5), kTolerance);
  EXPECT_COMPARABLE(0.0, Fractionality(10), kTolerance);
}

TEST(FractionalityTest, NegativeNumbers) {
  EXPECT_COMPARABLE(0.3, Fractionality(-1.7), kTolerance);
  EXPECT_COMPARABLE(0.3, Fractionality(-1.3), kTolerance);
  EXPECT_COMPARABLE(0.5, Fractionality(-0.5), kTolerance);
  EXPECT_COMPARABLE(0.0, Fractionality(-10), kTolerance);
}

TEST(SquaredNormTest, SimpleExample) {
  std::vector<Fractional> data = {0, 1, 2, 3, 4};
  EXPECT_COMPARABLE(30.0, SquaredNorm(data), kTolerance);
}

TEST(SquaredNormAndResetToZeroTest, SimpleExample) {
  std::vector<Fractional> data = {0, 1, 2, 3, 4};
  EXPECT_COMPARABLE(30.0, SquaredNormAndResetToZero(absl::MakeSpan(data)),
                    kTolerance);
  EXPECT_EQ(0.0, SquaredNorm(data));
}

TEST(DenseVectorFunctionsTest, MathCorrectness) {
  const RowIndex num_rows(1 << 20);
  DenseColumn col(num_rows, 0.0);
  for (RowIndex i(0); i < num_rows; ++i) {
    col[i] = Fractional(i.value());
  }

  const double n = num_rows.value();
  const double exact_squared_norm = (n - 1) * n * (2 * n - 1) / 6;
  EXPECT_COMPARABLE(ScalarProduct(Transpose(col), col), SquaredNorm(col),
                    kTolerance);
  EXPECT_EQ(PreciseScalarProduct(Transpose(col), col), PreciseSquaredNorm(col));
  EXPECT_COMPARABLE(exact_squared_norm, SquaredNorm(col), 1e-11);
  EXPECT_EQ(exact_squared_norm, PreciseSquaredNorm(col));
}

TEST(SparseVectorFunctionsTest, MathCorrectness) {
  std::mt19937 randomizer(12345);
  const ColIndex num_cols(1 << 20);
  const ColIndex reduced_num_cols(num_cols - 20);
  DenseRow row(num_cols, 0.0);
  SparseColumn column;
  for (ColIndex i(0); i < num_cols; ++i) {
    row[i] = absl::Uniform<float>(randomizer, -0.5, 0.5);
    if ((i.value() % 4) == 0) {
      column.SetCoefficient(ColToRowIndex(i),
                            absl::Uniform<float>(randomizer, -0.5, 0.5));
    }
  }
  EXPECT_TRUE(column.CheckNoDuplicates());

  const Fractional snorm_row = SquaredNorm(Transpose(row));
  const Fractional precise_snorm_row = PreciseSquaredNorm(Transpose(row));
  const Fractional snorm_col = SquaredNorm(column);
  const Fractional precise_snorm_col = PreciseSquaredNorm(column);
  const Fractional scalar_product = ScalarProduct(row, column);
  const Fractional precise_scalar_product = PreciseScalarProduct(row, column);
  const Fractional partial_scalar_product =
      PartialScalarProduct(row, column, reduced_num_cols.value());

  EXPECT_COMPARABLE(-44.931725, precise_scalar_product, 1e-5);
  EXPECT_COMPARABLE(21886.315, precise_snorm_col, 1e-5);
  EXPECT_COMPARABLE(87317.665, precise_snorm_row, 1e-5);
  EXPECT_NE(snorm_row, precise_snorm_row);
  EXPECT_NE(snorm_col, precise_snorm_col);
  EXPECT_COMPARABLE(scalar_product, precise_scalar_product, 1e-11);
  EXPECT_COMPARABLE(snorm_row, precise_snorm_row, 1e-11);
  EXPECT_COMPARABLE(snorm_col, precise_snorm_col, 1e-11);
  EXPECT_COMPARABLE(-44.784831, partial_scalar_product, 1e-5);

  // We compute in row, row += Transpose(col) and check the formula:
  // ||a + b||^2 = ||a||^2 + ||b||^2 + 2 * ScalarProduct(a, b);
  for (const SparseColumn::Entry e : column) {
    row[RowToColIndex(e.row())] += e.coefficient();
  }
  EXPECT_LE(fabs(SquaredNorm(Transpose(row)) - snorm_row - snorm_col -
                 2 * scalar_product),
            1e-7);
  EXPECT_LE(fabs(PreciseSquaredNorm(Transpose(row)) - precise_snorm_row -
                 precise_snorm_col - 2 * precise_scalar_product),
            1e-10);
}

TEST(InfinityNormTest, Correctness) {
  const RowIndex num_rows(1 << 20);
  DenseColumn col(num_rows, 0.0);
  EXPECT_EQ(0.0, InfinityNorm(col));

  col[RowIndex(10)] = 0.4;
  col[RowIndex(11)] = 0.5;
  col[RowIndex(12)] = 0.3;
  col[RowIndex(13)] = -0.3;
  EXPECT_EQ(0.5, InfinityNorm(col));

  col[RowIndex(15)] = -1.1;
  EXPECT_EQ(1.1, InfinityNorm(col));
}

TEST(DensityTest, Correctness) {
  const ColIndex num_cols(1 << 20);
  DenseRow row(num_cols, 0.0);
  for (ColIndex i(0); i < num_cols; ++i) {
    row[i] = (i.value() % 4 == 0) ? 0.0 : 1.0;
  }
  EXPECT_EQ(0.75, Density(row));
}

TEST(RemoveNearZeroEntriesTest, Correctness) {
  const int n = 1 << 20;
  DenseRow row(ColIndex(n), 0.0);
  DenseColumn col(RowIndex(n), 0.0);
  for (ColIndex i(0); i < n; ++i) {
    row[i] = ((i.value() % 4) == 0) ? -1.0 : 1.0;
    col[ColToRowIndex(i)] = row[i];
  }
  RemoveNearZeroEntries(0.99, &row);
  RemoveNearZeroEntries(0.99, &col);
  for (ColIndex i(0); i < n; ++i) {
    EXPECT_EQ(((i.value() % 4) == 0) ? -1.0 : 1.0, row[i]);
    EXPECT_EQ(row[i], col[ColToRowIndex(i)]);
  }
  RemoveNearZeroEntries(1.01, &row);
  RemoveNearZeroEntries(1.01, &col);
  for (ColIndex i(0); i < n; ++i) {
    EXPECT_EQ(0.0, row[i]);
    EXPECT_EQ(row[i], col[ColToRowIndex(i)]);
  }
}

TEST(TransposeTest, EmptyVector) {
  DenseColumn col;
  const DenseRow& row = Transpose(col);
  EXPECT_EQ(row.size(), 0);
  const DenseColumn& double_transposed_col = Transpose(Transpose(col));
  EXPECT_TRUE(double_transposed_col.empty());
}

TEST(TransposeTest, Correctness) {
  const int n = 1 << 20;
  DenseRow row(ColIndex(n), 0.0);
  DenseColumn col(RowIndex(n), 0.0);
  for (ColIndex i(0); i < n; ++i) {
    row[i] = (i.value() % 4 == 0) ? 0.0 : 1.0;
    col[ColToRowIndex(i)] = row[i];
  }
  for (ColIndex i(0); i < n; ++i) {
    EXPECT_EQ(Transpose(row)[ColToRowIndex(i)], col[ColToRowIndex(i)]);
    EXPECT_EQ(row[i], Transpose(col)[i]);
  }
}

SparseColumn CreateSimpleSparseColumn() {
  SparseColumn column;
  column.SetCoefficient(RowIndex(2), -4.0);
  column.SetCoefficient(RowIndex(3), 7.0);
  column.SetCoefficient(RowIndex(4), -8.0);
  column.SetCoefficient(RowIndex(5), -10.0);
  column.SetCoefficient(RowIndex(6), 8.0);
  column.SetCoefficient(RowIndex(7), 0.0);
  EXPECT_TRUE(column.CheckNoDuplicates());
  return column;
}

TEST(InfinityNormTest, SparseCorrectness) {
  SparseColumn column;
  EXPECT_EQ(0.0, InfinityNorm(column));
  column = CreateSimpleSparseColumn();
  EXPECT_EQ(10.0, InfinityNorm(column));
}

TEST(RestrictedInfinityNormTest, Correctness) {
  SparseColumn column = CreateSimpleSparseColumn();

  const RowIndex kOriginalIndex(80);
  RowIndex row_index(kOriginalIndex);
  DenseBooleanColumn rows_to_consider(RowIndex(10), false);

  // No rows to consider.
  EXPECT_EQ(0.0, RestrictedInfinityNorm(ColumnView(column), rows_to_consider,
                                        &row_index));
  EXPECT_EQ(kOriginalIndex, row_index);

  // 0.0 leaves row_index untouched.
  rows_to_consider[RowIndex(7)] = true;
  EXPECT_EQ(0.0, RestrictedInfinityNorm(ColumnView(column), rows_to_consider,
                                        &row_index));
  EXPECT_EQ(kOriginalIndex, row_index);

  // Some rows to consider.
  rows_to_consider[RowIndex(2)] = true;
  rows_to_consider[RowIndex(4)] = true;
  rows_to_consider[RowIndex(6)] = true;
  EXPECT_EQ(8.0, RestrictedInfinityNorm(ColumnView(column), rows_to_consider,
                                        &row_index));
  EXPECT_EQ(RowIndex(4), row_index);
}

TEST(SetSupportToFalseTest, Correctness) {
  SparseColumn column = CreateSimpleSparseColumn();

  DenseBooleanColumn bool_column(RowIndex(8), true);
  SetSupportToFalse(ColumnView(column), &bool_column);
  EXPECT_TRUE(bool_column[RowIndex(0)]);
  EXPECT_TRUE(bool_column[RowIndex(1)]);
  for (RowIndex row(2); row <= 6; ++row) {
    EXPECT_FALSE(bool_column[row]);
  }

  // Note that a 0.0 is not in the support even though the entry is in the
  // SparseColumn.
  EXPECT_TRUE(bool_column[RowIndex(7)]);
}

TEST(IsDominatedTest, Correctness) {
  const RowIndex num_rows(8);
  SparseColumn column;
  DenseColumn radius(num_rows, 0.0);
  EXPECT_TRUE(IsDominated(ColumnView(column), radius));

  column = CreateSimpleSparseColumn();
  EXPECT_FALSE(IsDominated(ColumnView(column), radius));

  radius = DenseColumn(num_rows, 9.9999);
  EXPECT_FALSE(IsDominated(ColumnView(column), radius));

  radius = DenseColumn(num_rows, 10.0);
  EXPECT_TRUE(IsDominated(ColumnView(column), radius));

  radius = DenseColumn(num_rows, kInfinity);
  EXPECT_TRUE(IsDominated(ColumnView(column), radius));

  column.CopyToDenseVector(num_rows, &radius);
  for (RowIndex row(0); row < num_rows; ++row) {
    radius[row] = fabs(radius[row]);
  }
  EXPECT_TRUE(IsDominated(ColumnView(column), radius));
}

TEST(ComputeNonZerosTest, Correctness) {
  const RowIndex kNumRows(10000);
  DenseColumn dense_column(kNumRows, 0);
  std::vector<RowIndex> non_zeros;
  ComputeNonZeros(dense_column, &non_zeros);
  EXPECT_TRUE(non_zeros.empty());
  dense_column[RowIndex(12)] = 1.0;
  dense_column[RowIndex(6)] = -1.0;
  dense_column[RowIndex(600)] = 4.0;
  dense_column[RowIndex(432)] = 1e-10;
  ComputeNonZeros(dense_column, &non_zeros);
  std::vector<RowIndex> expected_non_zeros(
      {RowIndex(6), RowIndex(12), RowIndex(432), RowIndex(600)});
  EXPECT_THAT(expected_non_zeros, ContainerEq(non_zeros));
}

TEST(PermuteWithScratchpadTest, Correctness) {
  const RowIndex kNumRows(10000);
  DenseColumn dense_column(kNumRows, 0);
  DenseColumn zero_scrathpad;
  RowPermutation row_perm(kNumRows);
  row_perm.PopulateFromIdentity();

  // Test behavior on empty input.
  DenseColumn output = dense_column;
  PermuteWithScratchpad(row_perm, &zero_scrathpad, &output);
  EXPECT_THAT(dense_column, ContainerEq(output));

  // Modify output (these changes should be discarded).
  output[RowIndex(123)] = 100;
  output[RowIndex(0)] = -1.0;
  output[kNumRows - 1] = 1.0;

  // Test behavior for identity permutation.
  dense_column[RowIndex(12)] = 1.0;
  dense_column[RowIndex(6)] = -1.0;
  dense_column[RowIndex(600)] = 4.0;
  dense_column[RowIndex(432)] = 1e-10;
  output = dense_column;
  PermuteWithScratchpad(row_perm, &zero_scrathpad, &output);
  EXPECT_THAT(dense_column, ContainerEq(output));

  // Test behavior with a random permutation.
  row_perm.PopulateRandomly();
  output = dense_column;
  PermuteWithScratchpad(row_perm, &zero_scrathpad, &output);
  DenseColumn expected_output;
  ApplyPermutation(row_perm, dense_column, &expected_output);
  EXPECT_THAT(expected_output, ContainerEq(output));

  // Test the PermuteWithKnownNonZeros() version.
  std::vector<RowIndex> known_non_zeros{RowIndex(6), RowIndex(12),
                                        RowIndex(432), RowIndex(600)};
  output = dense_column;
  PermuteWithKnownNonZeros(row_perm, &zero_scrathpad, &output,
                           &known_non_zeros);
  EXPECT_EQ(output, expected_output);
}

TEST(ClearAndResizeVectorWithNonZerosTest, Correctness) {
  const RowIndex kNumRows(1000);

  // Empty.
  ScatteredColumn column;
  ClearAndResizeVectorWithNonZeros(kNumRows, &column);
  EXPECT_EQ(column.values.size(), kNumRows);
  EXPECT_TRUE(IsAllZero(column.values));
  EXPECT_TRUE(column.non_zeros.empty());

  // Sparse.
  column[RowIndex(12)] = 1.0;
  column[RowIndex(6)] = -1.0;
  column[RowIndex(600)] = 4.0;
  column[RowIndex(432)] = 1e-10;
  ComputeNonZeros(column.values, &column.non_zeros);
  ClearAndResizeVectorWithNonZeros(kNumRows, &column);
  EXPECT_EQ(column.values.size(), kNumRows);
  EXPECT_TRUE(IsAllZero(column.values));
  EXPECT_TRUE(column.non_zeros.empty());

  // Dense.
  for (RowIndex row(0); row < kNumRows; ++row) {
    column[row] = 3.14;
  }
  ComputeNonZeros(column.values, &column.non_zeros);
  ClearAndResizeVectorWithNonZeros(kNumRows, &column);
  EXPECT_EQ(column.values.size(), kNumRows);
  EXPECT_TRUE(IsAllZero(column.values));
  EXPECT_TRUE(column.non_zeros.empty());
}

TEST(ClearAndResizeVectorWithNonZerosTest, SmallerSizeWhenCleaning) {
  ScatteredColumn column;
  column.values.assign(RowIndex(1000), 0.0);
  column[RowIndex(12)] = 1.0;
  column[RowIndex(6)] = -1.0;
  column[RowIndex(600)] = 4.0;
  column[RowIndex(432)] = 1e-10;
  ComputeNonZeros(column.values, &column.non_zeros);

  // We now want to clear it and shrink its size.
  ClearAndResizeVectorWithNonZeros(RowIndex(20), &column);
  EXPECT_EQ(column.values.size(), RowIndex(20));
  EXPECT_TRUE(IsAllZero(column.values));
  EXPECT_TRUE(column.non_zeros.empty());
}

TEST(ChangeSignTest, Correctness) {
  // TODO(user): ITIVector and our version that derive from it
  // (StrictITIVector) can't be "brace initialized". This should be fixable.
  std::vector<Fractional> v = {1.0, -2.0, -4.0, 10.0, 0.0};
  std::vector<Fractional> inverse = {-1.0, 2.0, 4.0, -10.0, 0.0};
  DenseRow row(v.begin(), v.end());
  DenseRow inverse_row(inverse.begin(), inverse.end());
  ChangeSign(&row);
  EXPECT_THAT(row, ContainerEq(inverse_row));

  ChangeSign(&row);
  ChangeSign(&row);
  EXPECT_THAT(row, ContainerEq(inverse_row));
}

TEST(SumWithOneMissingTest, NoInfinity) {
  SumWithPositiveInfiniteAndOneMissing sum;
  sum.Add(1.0);
  EXPECT_EQ(sum.SumWithout(1.0), 0.0);
  sum.Add(0.0);
  EXPECT_EQ(sum.SumWithout(1.0), 0.0);
  EXPECT_EQ(sum.SumWithout(0.0), 1.0);
  sum.Add(-2.0);
  EXPECT_EQ(sum.SumWithout(0.0), -1.0);
  EXPECT_EQ(sum.SumWithout(1.0), -2.0);
  EXPECT_EQ(sum.SumWithout(-2.0), 1.0);
}

TEST(SumWithOneMissingTest, OneInfinity) {
  SumWithPositiveInfiniteAndOneMissing sum;
  sum.Add(1.0);
  sum.Add(kInfinity);
  sum.Add(-2.0);
  EXPECT_EQ(sum.SumWithout(1.0), kInfinity);
  EXPECT_EQ(sum.SumWithout(-2.0), kInfinity);
  EXPECT_EQ(sum.SumWithout(kInfinity), -1.0);
}

TEST(SumWithOneMissingTest, OnlyOneInfinity) {
  SumWithPositiveInfiniteAndOneMissing sum;
  sum.Add(kInfinity);
  EXPECT_EQ(sum.SumWithout(kInfinity), 0.0);
}

TEST(SumWithOneMissingTest, TwoInfinities) {
  SumWithPositiveInfiniteAndOneMissing sum;
  sum.Add(1.0);
  sum.Add(kInfinity);
  sum.Add(kInfinity);
  sum.Add(-2.0);
  EXPECT_EQ(sum.SumWithout(1.0), kInfinity);
  EXPECT_EQ(sum.SumWithout(-2.0), kInfinity);
  EXPECT_EQ(sum.SumWithout(kInfinity), kInfinity);
}

TEST(SumWithOneMissingTest, AddAndRemoveInfinities) {
  SumWithPositiveInfiniteAndOneMissing sum;
  sum.Add(1.0);
  sum.Add(kInfinity);
  sum.Add(kInfinity);
  sum.Add(-2.0);
  sum.RemoveOneInfinity();
  EXPECT_EQ(sum.Sum(), kInfinity);
  EXPECT_EQ(sum.SumWithout(1.0), kInfinity);
  EXPECT_EQ(sum.SumWithout(-2.0), kInfinity);
  EXPECT_EQ(sum.SumWithout(kInfinity), -1.0);
  sum.RemoveOneInfinity();
  EXPECT_EQ(sum.Sum(), -1.0);
  EXPECT_EQ(sum.SumWithout(1.0), -2.0);
  EXPECT_EQ(sum.SumWithout(-2.0), 1.0);
}

TEST(SumWithOneMissingTest, NumericalAccuracy) {
  SumWithPositiveInfiniteAndOneMissing sum;
  sum.Add(1.0);  // Inherent imprecision ~1e-15.
  for (int i = 0; i < 1000000; ++i) {
    sum.Add(1e-16);
  }
  EXPECT_NEAR(1e-10, sum.SumWithout(1), 1e-15);
}

TEST(SumWithOneMissingTest, NegativeInfinite) {
  SumWithNegativeInfiniteAndOneMissing sum;
  sum.Add(1.0);
  sum.Add(-2.0);
  EXPECT_EQ(sum.SumWithout(-2.0), 1.0);
  sum.Add(-kInfinity);
  EXPECT_EQ(sum.SumWithout(1.0), -kInfinity);
  EXPECT_EQ(sum.SumWithout(-kInfinity), -1.0);
  sum.Add(-kInfinity);
  EXPECT_EQ(sum.SumWithout(1.0), -kInfinity);
  EXPECT_EQ(sum.SumWithout(-kInfinity), -kInfinity);
}

TEST(SumWithOneMissingTest, Overflow) {
  SumWithPositiveInfiniteAndOneMissing sum;
  sum.Add(1e308);
  EXPECT_EQ(sum.Sum(), 1e308);
  sum.Add(1e308);
  EXPECT_EQ(sum.Sum(), kInfinity);

  // We cannot do much in this case.
  EXPECT_EQ(sum.SumWithout(1e308), kInfinity);

  // The underlying KahanSum can return nan if we add stuff to an infinite sum.
  // We took care of this in SumWithOneMissing, but this test used to fail when
  // it was introduced.
  sum.Add(1e308);
  EXPECT_EQ(sum.SumWithout(1e308), kInfinity);
}

}  // namespace
}  // namespace glop
}  // namespace operations_research
