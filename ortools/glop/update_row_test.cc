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

#include "ortools/glop/update_row.h"

#include <random>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/glop/basis_representation.h"
#include "ortools/glop/variables_info.h"
#include "ortools/lp_data/lp_test_utils.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/sparse.h"

namespace operations_research {
namespace glop {
namespace {

using ::testing::ContainerEq;

TEST(UpdateRowTest, RecomputationTest) {
  SparseMatrix matrix{
      {1, 0, 0, 0, 0, 1},  // 0
      {0, 1, 0, 0, 1, 0},  // 1
      {0, 0, 1, 1, 0, 0},  // 2
  };
  CompactSparseMatrix compact_matrix(matrix);
  CompactSparseMatrix transposed_matrix;
  transposed_matrix.PopulateFromTranspose(compact_matrix);
  DenseRow lower_bound(matrix.num_cols(), -10);
  DenseRow upper_bound(matrix.num_cols(), 10);
  VariablesInfo variables_info(compact_matrix);
  variables_info.LoadBoundsAndReturnTrueIfUnchanged(lower_bound, upper_bound);
  variables_info.InitializeToDefaultStatus();
  RowToColMapping basis(matrix.num_rows(), kInvalidCol);
  for (ColIndex col(0); col < matrix.num_cols(); ++col) {
    if (col < RowToColIndex(matrix.num_rows())) {
      variables_info.UpdateToBasicStatus(col);
      basis[ColToRowIndex(col)] = col;
    } else {
      variables_info.UpdateToNonBasicStatus(col,
                                            VariableStatus::AT_LOWER_BOUND);
    }
  }

  MatrixView matrix_view(matrix);
  BasisFactorization basis_factorization(&compact_matrix, &basis);
  CHECK(basis_factorization.Initialize().ok());
  UpdateRow update_row(compact_matrix, transposed_matrix, variables_info, basis,
                       basis_factorization);

  // Changing the row causes an update.
  update_row.ComputeUpdateRow(RowIndex(2));
  EXPECT_EQ(update_row.GetNonZeroPositions(), (ColIndexVector{ColIndex(3)}));
  update_row.ComputeUpdateRow(RowIndex(1));
  EXPECT_EQ(update_row.GetNonZeroPositions(), (ColIndexVector{ColIndex(4)}));
  update_row.ComputeUpdateRow(RowIndex(0));
  EXPECT_EQ(update_row.GetNonZeroPositions(), (ColIndexVector{ColIndex(5)}));

  // But not changing it does not cause any update.
  variables_info.UpdateToBasicStatus(ColIndex(5));
  variables_info.UpdateToNonBasicStatus(ColIndex(0),
                                        VariableStatus::AT_LOWER_BOUND);
  basis[RowIndex(0)] = ColIndex(5);

  update_row.ComputeUpdateRow(RowIndex(0));
  EXPECT_EQ(update_row.GetNonZeroPositions(), (ColIndexVector{ColIndex(5)}));

  // Invalidate will cause a recomputation though.
  update_row.Invalidate();
  update_row.ComputeUpdateRow(RowIndex(0));
  EXPECT_EQ(update_row.GetNonZeroPositions(), (ColIndexVector{ColIndex(0)}));
}

TEST(UpdateRowTest, ResultIsTheSameWithAllAlgorithmVersions) {
  const RowIndex num_rows(100);
  const Fractional matrix_density(0.1);
  SparseMatrix matrix;
  std::mt19937 randomizer(12345);
  FillSparseMatrixRandomly(num_rows, /*num_cols=*/ColIndex(10000),
                           matrix_density, randomizer, &matrix);
  CompactSparseMatrix compact_matrix;
  compact_matrix.PopulateFromSparseMatrixAndAddSlacks(matrix);
  CompactSparseMatrix transposed_matrix;
  transposed_matrix.PopulateFromTranspose(compact_matrix);

  // The bounds do not matter as long as the variables are not fixed.
  // We assume the first columns are BASIC, so they will be ignored.
  const ColIndex num_cols = compact_matrix.num_cols();
  DenseRow lower_bound(num_cols, -10);
  DenseRow upper_bound(num_cols, 10);
  VariablesInfo variables_info(compact_matrix);
  variables_info.LoadBoundsAndReturnTrueIfUnchanged(lower_bound, upper_bound);
  variables_info.InitializeToDefaultStatus();
  RowToColMapping basis(num_rows, kInvalidCol);
  for (ColIndex col(0); col < num_cols; ++col) {
    if (col < RowToColIndex(num_rows)) {
      variables_info.UpdateToBasicStatus(col);
      basis[ColToRowIndex(col)] = col;
    } else {
      variables_info.UpdateToNonBasicStatus(col,
                                            VariableStatus::AT_LOWER_BOUND);
    }
  }

  MatrixView matrix_view(matrix);
  BasisFactorization basis_factorization(&compact_matrix, &basis);
  CHECK(basis_factorization.Initialize().ok());
  UpdateRow update_row(compact_matrix, transposed_matrix, variables_info, basis,
                       basis_factorization);

  // The actual test:
  // We use the function actually used by the solver as a reference.
  const RowIndex kLeavingRow(42);
  update_row.ComputeUpdateRow(kLeavingRow);
  DenseRow base_coefficients = update_row.GetCoefficients();
  ColIndexVector base_non_zero_positions(
      update_row.GetNonZeroPositions().begin(),
      update_row.GetNonZeroPositions().end());
  DenseRow lhs = update_row.GetUnitRowLeftInverse().values;

  // And we test it against the 3 algorithm variants.
  // Note that the coefficients outside the non zeros position can be garbage.
  std::vector<std::string> algorithms{"column", "row", "row_hypersparse"};
  for (const std::string& algorithm : algorithms) {
    update_row.ComputeUpdateRowForBenchmark(lhs, algorithm);
    EXPECT_THAT(update_row.GetNonZeroPositions(),
                ::testing::Eq(base_non_zero_positions));
    const DenseRow& coefficients = update_row.GetCoefficients();
    for (const ColIndex col : base_non_zero_positions) {
      // Note that the result is exactly the same, because the computations were
      // done in the same order, this is not true in the real code, because
      // the non-zeros of lhs are not necessarily in order.
      //
      // Note(user): This is no longer true because of the loop expansion used
      // in the column-wise algo. So we use double_eq here.
      EXPECT_NEAR(base_coefficients[col], coefficients[col], 1e-10);
    }
  }
}

}  // namespace
}  // namespace glop
}  // namespace operations_research
