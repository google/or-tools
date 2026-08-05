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

#include "ortools/glop/lu_factorization.h"

#include <algorithm>
#include <cstdlib>
#include <random>

#include "absl/random/bit_gen_ref.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/log_severity.h"
#include "ortools/graph_base/iterators.h"
#include "ortools/lp_data/lp_test_utils.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/lp_types_testing.h"
#include "ortools/lp_data/lp_utils.h"
#include "ortools/lp_data/permutation.h"
#include "ortools/lp_data/sparse.h"
#include "ortools/lp_data/sparse_column.h"
#include "ortools/util/fp_utils.h"

namespace operations_research {
namespace glop {
namespace {

using ::testing::ContainerEq;

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

void TestLuFactorization(const SparseMatrix& matrix, Fractional det,
                         Fractional one_norm, Fractional inv_one_norm,
                         Fractional inf_norm, Fractional inv_inf_norm) {
  const Fractional kEps(1e-8);
  GlopParameters parameters;
  parameters.set_lu_factorization_pivot_threshold(1.0);
  auto wrapped_matrix = SparseMatrixWrapperForTest(&matrix);

  LuFactorization lu;
  lu.SetParameters(parameters);
  EXPECT_THAT(lu.ComputeFactorization(wrapped_matrix.AsView()),
              AbnormalityStatusIsOK());
  EXPECT_FALSE(lu.IsIdentityFactorization());

  const RowPermutation& row_perm = lu.row_perm();
  const ColumnPermutation& inverse_col_perm = lu.inverse_col_perm();
  SparseMatrix paq;
  paq.PopulateFromPermutedMatrix(matrix, row_perm, inverse_col_perm);

  SparseMatrix product;
  lu.ComputeLowerTimesUpper(&product);

  EXPECT_TRUE(paq.Equals(product, kEps));
  EXPECT_COMPARABLE(det, lu.ComputeDeterminant(), kEps);
  EXPECT_COMPARABLE(one_norm, matrix.ComputeOneNorm(), kEps);
  EXPECT_COMPARABLE(inv_one_norm, lu.ComputeInverseOneNorm(), kEps);
  EXPECT_COMPARABLE(one_norm * inv_one_norm,
                    lu.ComputeOneNormConditionNumber(wrapped_matrix.AsView()),
                    kEps);
  EXPECT_COMPARABLE(inf_norm, matrix.ComputeInfinityNorm(), kEps);
  EXPECT_COMPARABLE(inv_inf_norm, lu.ComputeInverseInfinityNorm(), kEps);
  EXPECT_COMPARABLE(
      inf_norm * inv_inf_norm,
      lu.ComputeInfinityNormConditionNumber(wrapped_matrix.AsView()), kEps);
}

TEST(LuFactorizationTest, Identity) {
  LuFactorization lu;
  const SparseMatrix matrix{{1}};
  const auto not_used = SparseMatrixWrapperForTest(&matrix);
  EXPECT_TRUE(lu.IsIdentityFactorization());
  EXPECT_EQ(1.0, lu.ComputeDeterminant());
  EXPECT_EQ(1.0, lu.ComputeInverseOneNorm());
  EXPECT_EQ(1.0, lu.ComputeOneNormConditionNumber(not_used.AsView()));
  EXPECT_EQ(1.0, lu.ComputeInverseInfinityNorm());
  EXPECT_EQ(1.0, lu.ComputeInfinityNormConditionNumber(not_used.AsView()));
  EXPECT_EQ(1.0, lu.GetFillInPercentage(not_used.AsView()));

  DenseColumn dense_column(RowIndex(16), Fractional(8.0));
  SparseColumn sparse_column;
  sparse_column.PopulateFromDenseVector(dense_column);
  EXPECT_EQ(32.0 * 32.0, lu.RightSolveSquaredNorm(ColumnView(sparse_column)));

  DenseColumn result = dense_column;
  lu.RightSolve(&result);
  EXPECT_THAT(result, ContainerEq(dense_column));

  DenseRow row_result = Transpose(dense_column);
  lu.LeftSolve(&row_result);
  EXPECT_THAT(row_result, ContainerEq(Transpose(dense_column)));
}

TEST(LuFactorizationTest, NotFactorizable1) {
  const SparseMatrix matrix{{1, 25, 1}, {1, 64, 1}, {1, 144, 1}};
  const auto wrapped_matrix = SparseMatrixWrapperForTest(&matrix);
  LuFactorization lu;
  EXPECT_THAT(lu.ComputeFactorization(wrapped_matrix.AsView()),
              AbnormalityStatusIs(
                  AbnormalityCause::kLuFactorizationMarkowitzPivotTooSmall));
  EXPECT_TRUE(lu.IsIdentityFactorization());
}

TEST(LuFactorizationTest, NotFactorizable2) {
  const SparseMatrix matrix{
      {1, 2, 3}, {5, 4, 0}, {6, 6, 3}  // line 1 + line 2
  };
  const auto wrapped_matrix = SparseMatrixWrapperForTest(&matrix);
  LuFactorization lu;
  EXPECT_THAT(lu.ComputeFactorization(wrapped_matrix.AsView()),
              AbnormalityStatusIs(
                  AbnormalityCause::kLuFactorizationMarkowitzPivotTooSmall));
  EXPECT_TRUE(lu.IsIdentityFactorization());
}

TEST(LuFactorizationTest, NotFactorizable3) {
  // The middle column is column 1 - column 3.
  const SparseMatrix matrix{{1, -2, 3}, {5, -3, 8}, {6, 3, 3}};
  const auto wrapped_matrix = SparseMatrixWrapperForTest(&matrix);
  LuFactorization lu;
  EXPECT_THAT(lu.ComputeFactorization(wrapped_matrix.AsView()),
              AbnormalityStatusIs(
                  AbnormalityCause::kLuFactorizationMarkowitzPivotTooSmall));
  EXPECT_TRUE(lu.IsIdentityFactorization());
}

TEST(LuFactorizationTest, NotFactorizable4) {
  // The last column is zero.
  const SparseMatrix matrix{{1, -2, 0}, {5, -3, 0}, {6, 3, 0}};
  const auto wrapped_matrix = SparseMatrixWrapperForTest(&matrix);
  LuFactorization lu;
  EXPECT_THAT(lu.ComputeFactorization(wrapped_matrix.AsView()),
              AbnormalityStatusIs(
                  AbnormalityCause::kLuFactorizationMarkowitzPivotTooSmall));
  EXPECT_TRUE(lu.IsIdentityFactorization());
}

TEST(LuFactorizationTest, NotFactorizable5) {
  // The first row is zero.
  const SparseMatrix matrix{{0, 0, 0}, {5, -3, 8}, {6, 3, 3}};
  const auto wrapped_matrix = SparseMatrixWrapperForTest(&matrix);
  LuFactorization lu;
  EXPECT_THAT(lu.ComputeFactorization(wrapped_matrix.AsView()),
              AbnormalityStatusIs(
                  AbnormalityCause::kLuFactorizationMarkowitzPivotTooSmall));
  EXPECT_TRUE(lu.IsIdentityFactorization());
}

// Golub and Van Loan p. 109
TEST(LuFactorizationTest, LuGolubVanLoanp109) {
  const SparseMatrix kMatrix{{.0001, 1}, {1, 1}};
  TestLuFactorization(kMatrix, -0.9999, 2, 2.0002, 2, 2.0002);
}

// In the following five tests, we check that the permutation on columns
// does not influence the norms of the matrix and its inverse, but that only
// the sign of the determinant may change.
TEST(LuFactorizationTest, MatrixPermutation1) {
  const SparseMatrix kMatrix{{1, 25, 5}, {1, 64, 8}, {1, 144, 12}};
  TestLuFactorization(kMatrix, -84.0, 233, 13.0 / 2, 157, 11);
}

TEST(LuFactorizationTest, MatrixPermutation2) {
  const SparseMatrix kMatrix{{1, 5, 25}, {1, 8, 64}, {1, 12, 144}};
  TestLuFactorization(kMatrix, 84.0, 233, 13.0 / 2, 157, 11);
}

TEST(LuFactorizationTest, MatrixPermutation3) {
  const SparseMatrix kMatrix{{25, 5, 1}, {64, 8, 1}, {144, 12, 1}};
  TestLuFactorization(kMatrix, -84.0, 233, 13.0 / 2, 157, 11);
}

// Cormen et al. p. 750
TEST(LuFactorizationTest, LuPermutedCormenp750) {
  const SparseMatrix kMatrix{
      {2, 5, 3, 1}, {6, 19, 13, 5}, {2, 23, 19, 10}, {4, 31, 10, 11}};
  TestLuFactorization(kMatrix, 24.0, 78, 1085.0 / 8, 56, 90);
}

// Cormen et al. p. 750
TEST(LuFactorizationTest, LuCormenp750) {
  const SparseMatrix kMatrix{
      {2, 3, 1, 5}, {6, 13, 5, 19}, {2, 19, 10, 23}, {4, 10, 11, 31}};
  TestLuFactorization(kMatrix, 24.0, 78, 1085.0 / 8, 56, 90);
}

// Cormen et al. p. 753
TEST(LuFactorizationTest, LuCormenp753) {
  const SparseMatrix kMatrix{
      {2, 0, 2, 0.6}, {3, 3, 4, -2}, {5, 5, 4, 2}, {-1, -2, 3.4, -1}};
  TestLuFactorization(kMatrix, -120.0, 67.0 / 5, 103.0 / 75, 16, 491.0 / 375);
}

// An ill-conditioned matrix, the norm of the inverse is large,
// and so is the condition number.
TEST(LuFactorizationTest, IllConditioned1) {
  const Fractional kEps = 0.0001;
  const SparseMatrix kMatrix{{1, 1}, {1, 1 + kEps}};
  const Fractional e = std::abs(kEps);
  const Fractional f = std::abs(kEps + 1);
  const Fractional norm = std::max(Fractional(2), f + 1);
  const Fractional inv_norm = std::max(2 / e, (f + 1) / e);
  TestLuFactorization(kMatrix, kEps, norm, inv_norm, norm, inv_norm);
}

// An ill-conditioned matrix, the norm of the inverse is large,
// and so is the condition number.
TEST(LuFactorizationTest, IllConditioned2) {
  const Fractional kEps = 0.0001;
  const SparseMatrix kMatrix{{1, 2}, {2, 4 - kEps}};
  const Fractional e = std::abs(kEps);
  const Fractional f = std::abs(4 - kEps);
  const Fractional norm = std::max(Fractional(3), f + 2);
  const Fractional inv_norm = std::max(3 / e, (f + 2) / e);
  TestLuFactorization(kMatrix, -kEps, norm, inv_norm, norm, inv_norm);
}

//------------------------------------------------------
// Random tests
//------------------------------------------------------

void CheckLuFactorizationUsingRandomMatrices(RowIndex num_rows,
                                             Fractional tolerance,
                                             absl::BitGenRef randomizer) {
  SparseMatrix lower_matrix;
  FillSparseMatrixWithRandomLowerTriangularMatrix(num_rows, /*density=*/0.3,
                                                  randomizer, &lower_matrix);

  SparseMatrix upper_matrix;
  FillSparseMatrixWithRandomUpperTriangularMatrix(num_rows, /*density=*/0.3,
                                                  randomizer, &upper_matrix);

  SparseMatrix matrix;
  matrix.PopulateFromProduct(lower_matrix, upper_matrix);

  const auto wrapped_matrix = SparseMatrixWrapperForTest(&matrix);

  LuFactorization lu;
  EXPECT_THAT(lu.ComputeFactorization(wrapped_matrix.AsView()),
              AbnormalityStatusIsOK());
  EXPECT_FALSE(lu.IsIdentityFactorization());

  const RowPermutation& row_perm = lu.row_perm();
  const ColumnPermutation& inverse_col_perm = lu.inverse_col_perm();
  SparseMatrix paq;
  paq.PopulateFromPermutedMatrix(matrix, row_perm, inverse_col_perm);

  SparseMatrix product;
  lu.ComputeLowerTimesUpper(&product);
  EXPECT_TRUE(paq.Equals(product, tolerance));
}

class LuFactorizationRandomTest : public testing::TestWithParam<int> {
 protected:
  LuFactorizationRandomTest() : randomizer_(/*seed=*/GetParam()) {}
  std::mt19937 randomizer_;
};
INSTANTIATE_TEST_SUITE_P(ShardedInvocation, LuFactorizationRandomTest,
                         testing::Range(0, 10));

TEST_P(LuFactorizationRandomTest, CheckLuFactorizationOnRandomMatrices) {
  const RowIndex kNumRows(100);
  const Fractional kTolerance(1e-7);
  const int kNumAttempts = DEBUG_MODE ? 10 : 300;
  for (int i = 0; i < kNumAttempts; ++i) {
    CheckLuFactorizationUsingRandomMatrices(kNumRows, kTolerance, randomizer_);
  }
}

}  // namespace
}  // namespace glop
}  // namespace operations_research
