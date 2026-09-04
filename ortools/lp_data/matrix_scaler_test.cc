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

#include "ortools/lp_data/matrix_scaler.h"

#include <cmath>
#include <cstdint>
#include <random>

#include "absl/log/check.h"
#include "absl/random/random.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/strong_vector.h"
#include "ortools/glop/parameters.pb.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/sparse.h"
#include "ortools/lp_data/sparse_column.h"
#include "ortools/util/fp_utils_testing.h"

namespace operations_research {
namespace glop {
namespace {

template <class I, class T>
void ExpectEqualVectors(const util_intops::StrongVector<I, T>& v1,
                        const util_intops::StrongVector<I, T>& v2) {
  EXPECT_EQ(v1.size(), v2.size());
  int size = v1.size();
  for (I i(0); i < size; ++i) {
    EXPECT_THAT(v2[i], WithinSameAbsoluteOrRelativeTolerance(v1[i], 1e-9));
  }
}

class MatrixScalerTest
    : public ::testing::TestWithParam<GlopParameters::ScalingAlgorithm> {};

TEST_P(MatrixScalerTest, Test1) {
  std::mt19937 randomizer(0);
  bool using_lp = (GetParam() == GlopParameters::LINEAR_PROGRAM);
  // Avoid timeout that occurs with a large LP.
  const ColIndex kMaxCol(using_lp ? 20 : 100);
  const RowIndex kMaxRow(using_lp ? 100 : 500);
  const int kRange = 100000000;

  SparseMatrix matrix;
  matrix.PopulateFromZero(kMaxRow, kMaxCol);

  for (ColIndex col(0); col < kMaxCol; ++col) {
    SparseColumn* const column = matrix.mutable_column(col);

    for (RowIndex row(0); row < kMaxRow; ++row) {
      Fractional v(-kRange + 2 * absl::Uniform<int32_t>(randomizer, 0, kRange));
      if (v != 0.0) {
        column->SetCoefficient(row, v);
      }
    }
  }
  matrix.CleanUp();

  SparseMatrix copy;
  copy.PopulateFromSparseMatrix(matrix);
  SparseMatrixScaler scaler;
  scaler.Init(&matrix);
  scaler.Scale(GetParam());

  // Recover the scaling factors for columns.
  DenseColumn col_vector(kMaxRow, 1.0);
  DenseColumn col_copy = col_vector;

  // Get them in the up direction.
  scaler.ScaleColumnVector(true, &col_vector);

  // Recover the scaling factors for rows.
  DenseRow row_vector(kMaxCol, 1.0);
  DenseRow row_copy = row_vector;

  // Get them in the down direction.
  scaler.ScaleRowVector(false, &row_vector);

  // Check that the scaling is done appropriately.
  for (ColIndex col(0); col < kMaxCol; ++col) {
    const SparseColumn& column = matrix.column(col);
    const SparseColumn& column_copy = copy.column(col);
    EXPECT_EQ(column.num_entries(), column_copy.num_entries());
    for (const EntryIndex i : column.AllEntryIndices()) {
      EXPECT_EQ(column.EntryRow(i), column_copy.EntryRow(i));
      const Fractional value = column.EntryCoefficient(i);
      const Fractional copy_value = column_copy.EntryCoefficient(i) /
                                    col_vector[column.EntryRow(i)] *
                                    row_vector[col];
      EXPECT_THAT(copy_value,
                  WithinSameAbsoluteOrRelativeTolerance(value, 1e-9));
    }
  }

  // Unscale the matrix.
  {
    const ColIndex num_cols = matrix.num_cols();
    for (ColIndex col(0); col < num_cols; ++col) {
      const Fractional column_scale = scaler.col_scales()[col];
      DCHECK_NE(0.0, column_scale);
      SparseColumn* const column = matrix.mutable_column(col);
      if (column != nullptr) {
        column->MultiplyByConstant(column_scale);
        column->ComponentWiseMultiply(scaler.row_scales());
      }
    }
  }

  for (ColIndex col(0); col < kMaxCol; ++col) {
    const SparseColumn& column = matrix.column(col);
    const SparseColumn& column_copy = copy.column(col);
    EXPECT_EQ(column.num_entries(), column_copy.num_entries());
    for (const EntryIndex i : column.AllEntryIndices()) {
      EXPECT_EQ(column.EntryRow(i), column_copy.EntryRow(i));
      EXPECT_THAT(column_copy.EntryCoefficient(i),
                  WithinSameAbsoluteOrRelativeTolerance(
                      column.EntryCoefficient(i), 1e-9));
    }
  }

  // Unscale the vectors and check that everything is back.
  scaler.ScaleColumnVector(false, &col_vector);
  ExpectEqualVectors(col_copy, col_vector);
  scaler.ScaleRowVector(true, &row_vector);
  ExpectEqualVectors(row_copy, row_vector);
}

TEST_P(MatrixScalerTest, EmptyRow) {
  SparseMatrix matrix;
  matrix.SetNumRows(RowIndex{2});
  matrix.AppendUnitVector(RowIndex{0}, 100.0);

  SparseMatrixScaler scaler;
  scaler.Init(&matrix);
  scaler.Scale(GetParam());

  SparseMatrix expected_matrix;
  expected_matrix.SetNumRows(RowIndex{2});
  expected_matrix.AppendUnitVector(RowIndex{0}, 1.0);

  // We expect fairly good precision with a simple matrix.
  EXPECT_TRUE(matrix.Equals(expected_matrix, /*tolerance=*/1e-9))
      << "matrix: " << matrix.Dump()
      << " expected_matrix: " << expected_matrix.Dump();
}

TEST_P(MatrixScalerTest, EmptyColumn) {
  SparseMatrix matrix;
  matrix.SetNumRows(RowIndex{2});
  matrix.AppendEmptyColumn();
  matrix.AppendUnitVector(RowIndex{0}, 100.0);

  SparseMatrixScaler scaler;
  scaler.Init(&matrix);
  scaler.Scale(GetParam());

  SparseMatrix expected_matrix;
  expected_matrix.SetNumRows(RowIndex{2});
  expected_matrix.AppendEmptyColumn();
  expected_matrix.AppendUnitVector(RowIndex{0}, 1.0);

  // We expect fairly good precision with a simple matrix.
  EXPECT_TRUE(matrix.Equals(expected_matrix, /*tolerance=*/1e-9))
      << "matrix: " << matrix.Dump()
      << " expected_matrix: " << expected_matrix.Dump();
}

TEST_P(MatrixScalerTest, EmptyMatrices) {
  SparseMatrix matrix;
  // We try different kinds of "empty" matrices.
  for (int i = 0; i < 3; ++i) {
    if (i == 1) matrix.PopulateFromZero(RowIndex(0), ColIndex(10));
    if (i == 2) matrix.PopulateFromZero(RowIndex(10), ColIndex(0));

    SparseMatrixScaler scaler;
    scaler.Init(&matrix);
    scaler.Scale(GetParam());
    const int kArbitraryLength = 100;
    for (int j = 0; j < kArbitraryLength; ++j) {
      EXPECT_EQ(1.0, scaler.RowUnscalingFactor(RowIndex(j)));
      EXPECT_EQ(1.0, scaler.ColUnscalingFactor(ColIndex(j)));
    }
  }
}

TEST_P(MatrixScalerTest, NullMatrix) {
  const ColIndex kMaxCol(10);
  const RowIndex kMaxRow(10);
  SparseMatrix matrix;
  matrix.PopulateFromZero(kMaxRow, kMaxCol);
  SparseMatrixScaler scaler;
  scaler.Init(&matrix);
  scaler.Scale(GetParam());
  for (ColIndex col(0); col < kMaxCol; ++col) {
    for (const SparseColumn::Entry e : matrix.column(col)) {
      EXPECT_EQ(0.0, e.coefficient());
    }
  }
}

TEST(SparseMatrixTest, HighDynamicRange) {
  SparseMatrix matrix{
      {1, -1e-44, 0}, {0, -1, 1e-50}, {0.1, -1e-64, 1}, {1, -1e-40, 0}};
  SparseMatrixScaler scaler;
  scaler.Init(&matrix);
  // LP scaler can handle any dynamic range, so there's no need to
  // parameterize this test like the others.
  scaler.Scale(GlopParameters::EQUILIBRATION);

  // In this case, the scaler should do nothing.
  for (RowIndex row(0); row < matrix.num_rows(); ++row) {
    EXPECT_EQ(1.0, scaler.RowUnscalingFactor(row));
  }
  for (ColIndex col(0); col < matrix.num_cols(); ++col) {
    EXPECT_EQ(1.0, scaler.ColUnscalingFactor(col));
  }
}

INSTANTIATE_TEST_SUITE_P(
    AllScalerTests, MatrixScalerTest,
    ::testing::Values(GlopParameters::LINEAR_PROGRAM,
                      GlopParameters::EQUILIBRATION, GlopParameters::DEFAULT),
    [](const testing::TestParamInfo<MatrixScalerTest::ParamType>& info) {
      return GlopParameters::ScalingAlgorithm_Name(info.param);
    });

// Scales the given symmetric matrix with Ruiz
// equilibration and asserts that symmetry is
// preserved: A'[i,j] == A'[j,i] and row scales
// equal column scales.
//
// For a symmetric matrix A, the scaled matrix
// A' = R * A * C preserves symmetry only when
// R == C. Ruiz achieves this when both rows and
// columns use the same sqrt-of-infinity-norm
// scaling factor.
void ExpectSymmetricAfterRuiz(SparseMatrix* matrix) {
  const int n = matrix->num_rows().value();
  ASSERT_EQ(n, matrix->num_cols().value()) << "Matrix must be square";

  SparseMatrixScaler scaler;
  scaler.Init(matrix);
  scaler.Scale(GlopParameters::RUIZ);

  // The scaled matrix must remain symmetric:
  // A'[i,j] == A'[j,i] for all i < j.
  const double kTolerance = 1e-9;
  for (int i = 0; i < n; ++i) {
    for (int j = i + 1; j < n; ++j) {
      const Fractional a_ij = matrix->LookUpValue(RowIndex(i), ColIndex(j));
      const Fractional a_ji = matrix->LookUpValue(RowIndex(j), ColIndex(i));
      EXPECT_NEAR(a_ij, a_ji, kTolerance)
          << "Symmetry broken at (" << i << ", " << j << "): "
          << "A'[" << i << "," << j << "]=" << a_ij << " vs A'[" << j << ","
          << i << "]=" << a_ji;
    }
  }

  // Row and column scales must match for a
  // symmetric matrix (R == C).
  for (int k = 0; k < n; ++k) {
    const Fractional row_scale = scaler.RowUnscalingFactor(RowIndex(k));
    const Fractional col_scale = scaler.ColUnscalingFactor(ColIndex(k));
    EXPECT_NEAR(row_scale, col_scale, kTolerance)
        << "Scale mismatch at index " << k << ": row_scale=" << row_scale
        << " col_scale=" << col_scale;
  }
}

// Hand-crafted 4x4 symmetric matrix with varied
// magnitudes (0.5 to 100) and structural zeros.
TEST(SparseMatrixScalerTest, RuizPreservesSymmetrySmall) {
  //   [  2    0.5    0   100 ]
  //   [  0.5  8      3     0 ]
  //   [  0    3     50     7 ]
  //   [100    0      7     1 ]
  const int kSize = 4;
  const double kEntries[kSize][kSize] = {
      {2.0, 0.5, 0.0, 100.0},
      {0.5, 8.0, 3.0, 0.0},
      {0.0, 3.0, 50.0, 7.0},
      {100.0, 0.0, 7.0, 1.0},
  };

  SparseMatrix matrix;
  matrix.PopulateFromZero(RowIndex(kSize), ColIndex(kSize));
  for (int j = 0; j < kSize; ++j) {
    SparseColumn* const col = matrix.mutable_column(ColIndex(j));
    for (int i = 0; i < kSize; ++i) {
      if (kEntries[i][j] != 0.0) {
        col->SetCoefficient(RowIndex(i), kEntries[i][j]);
      }
    }
  }
  matrix.CleanUp();
  ExpectSymmetricAfterRuiz(&matrix);
}

// Random sparse symmetric matrix of dimension 50.
// Each off-diagonal pair (i,j) has a 30% chance of
// being nonzero, with magnitude drawn from
// [1e-3, 1e3]. Diagonal entries are always nonzero.
TEST(SparseMatrixScalerTest, RuizPreservesSymmetryRandom) {
  const int kSize = 50;
  const double kSparsity = 0.3;
  std::mt19937 rng(42);
  std::uniform_real_distribution<double> sparsity_dist(0.0, 1.0);
  // Log-uniform distribution over [1e-3, 1e3]
  // to exercise a wide dynamic range.
  std::uniform_real_distribution<double> log_mag_dist(-3.0, 3.0);
  std::uniform_real_distribution<double> sign_dist(-1.0, 1.0);

  // Build the lower triangle, then mirror to
  // upper triangle.
  SparseMatrix matrix;
  matrix.PopulateFromZero(RowIndex(kSize), ColIndex(kSize));
  for (int i = 0; i < kSize; ++i) {
    // Diagonal entry (always nonzero).
    const double diag = std::pow(10.0, log_mag_dist(rng));
    matrix.mutable_column(ColIndex(i))->SetCoefficient(RowIndex(i), diag);

    for (int j = 0; j < i; ++j) {
      if (sparsity_dist(rng) >= kSparsity) {
        continue;
      }
      const double magnitude = std::pow(10.0, log_mag_dist(rng));
      const double sign = sign_dist(rng) < 0.0 ? -1.0 : 1.0;
      const double value = sign * magnitude;
      // Set both A[i,j] and A[j,i].
      matrix.mutable_column(ColIndex(j))->SetCoefficient(RowIndex(i), value);
      matrix.mutable_column(ColIndex(i))->SetCoefficient(RowIndex(j), value);
    }
  }
  matrix.CleanUp();
  ExpectSymmetricAfterRuiz(&matrix);
}

}  // namespace
}  // namespace glop
}  // namespace operations_research
