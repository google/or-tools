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

#include "ortools/glop/dual_edge_norms.h"

#include <random>

#include "gtest/gtest.h"
#include "ortools/glop/basis_representation.h"
#include "ortools/glop/parameters.pb.h"
#include "ortools/lp_data/lp_test_utils.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/lp_utils.h"
#include "ortools/lp_data/permutation.h"
#include "ortools/lp_data/scattered_vector.h"
#include "ortools/lp_data/sparse.h"
#include "ortools/util/fp_utils.h"

namespace operations_research {
namespace glop {
namespace {

TEST(DualEdgeNormsTest, NormUpdateSeemsReasonable) {
  // Take a random matrix.
  SparseMatrix matrix;
  const ColIndex kNumCols(100);
  const RowIndex kNumRows(50);
  std::mt19937 randomizer(12345);
  FillSparseMatrixRandomly(kNumRows, kNumCols, /*density=*/0.5, randomizer,
                           &matrix);

  // Take first columns as basis + factorize.
  RowToColMapping basis(kNumRows, ColIndex(0));
  for (RowIndex row(0); row < kNumRows; ++row) {
    basis[row] = RowToColIndex(row);
  }
  CompactSparseMatrix compact_matrix(matrix);
  BasisFactorization basis_factorization(&compact_matrix, &basis);
  DualEdgeNorms dual_pricing(basis_factorization);

  auto permute_basis_if_needed = [&]() {
    const ColumnPermutation& col_perm =
        basis_factorization.GetColumnPermutation();
    if (col_perm.empty()) return;
    RowToColMapping tmp;
    ApplyColumnPermutationToRowIndexedVector(col_perm.const_view(), &basis,
                                             &tmp);
    dual_pricing.UpdateDataOnBasisPermutation(col_perm);
    basis_factorization.SetColumnPermutationToIdentity();
  };

  ASSERT_TRUE(basis_factorization.Initialize().ok());
  permute_basis_if_needed();

  // The basis refactorization period needs to be set to zero.
  GlopParameters parameters;
  parameters.set_basis_refactorization_period(0);
  parameters.set_dynamically_adjust_refactorization_period(false);
  basis_factorization.SetParameters(parameters);

  // Basic initial norm checking.
  const DenseColumn::ConstView norms = dual_pricing.GetEdgeSquaredNorms();
  for (RowIndex row(0); row < kNumRows; ++row) {
    // The values seem reasonable.
    EXPECT_GE(norms[row], 0.1);
    EXPECT_LE(norms[row], 1e8);

    // A proved lower bound with Cauchy-Shwartz (see .cc).
    EXPECT_GE(norms[row], 1.0 / SquaredNorm(matrix.column(basis[row])));
  }

  // Perform a pivot (with a random matrix, we shouldn't run into a singular
  // basis).
  ColIndex entering_col(70);
  RowIndex leaving_row(10);
  ScatteredColumn direction;
  basis_factorization.RightSolveForProblemColumn(entering_col, &direction);

  // We always need the non_zeros to be filled for now.
  ComputeNonZeros(direction.values, &direction.non_zeros);

  ScatteredRow unit_row_left_inverse;
  basis_factorization.LeftSolveForUnitRow(RowToColIndex(leaving_row),
                                          &unit_row_left_inverse);
  dual_pricing.UpdateBeforeBasisPivot(entering_col, leaving_row, direction,
                                      unit_row_left_inverse);

  basis[leaving_row] = entering_col;
  EXPECT_TRUE(
      basis_factorization.Update(entering_col, leaving_row, direction).ok());
  permute_basis_if_needed();

  // Test the update.
  EXPECT_FALSE(dual_pricing.NeedsBasisRefactorization());
  DenseColumn updated_norm(dual_pricing.GetEdgeSquaredNorms().begin(),
                           dual_pricing.GetEdgeSquaredNorms().end());
  dual_pricing.Clear();
  EXPECT_TRUE(dual_pricing.NeedsBasisRefactorization());
  const DenseColumn::ConstView precise_norm =
      dual_pricing.GetEdgeSquaredNorms();
  int num_exactly_equals_norm = 0;
  for (RowIndex row(0); row < kNumRows; ++row) {
    // The update is actually quite precise for squared norm!! however, on real
    // life linear problems, the matrix are not as well conditioned as in our
    // example.
    EXPECT_COMPARABLE(updated_norm[row], precise_norm[row], Fractional(1e-7));
    if (updated_norm[row] == precise_norm[row]) {
      ++num_exactly_equals_norm;
    }
  }
  // It is unlikely, but a few unprecise norms may be exactly the same as the
  // precise ones. We allow a few equalities like this so the test is not too
  // flaky.
  EXPECT_LT(num_exactly_equals_norm, 5);
}

}  // namespace
}  // namespace glop
}  // namespace operations_research
