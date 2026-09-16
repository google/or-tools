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

#include "ortools/glop/initial_basis.h"

#include <random>
#include <vector>

#include "absl/random/random.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/glop/parameters.pb.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/sparse.h"

namespace operations_research {
namespace glop {
namespace {

using ::testing::ContainerEq;

void TakeAllPossibleColumnsTest(bool bixby_or_triangular) {
  const int kSize = 100;
  const ColIndex num_cols(kSize);
  const RowIndex num_rows(kSize);
  SparseMatrix matrix;
  matrix.PopulateFromIdentity(num_cols);
  DenseRow objective(num_cols, 0.0);
  DenseRow lower_bound(num_cols, 0.0);
  DenseRow upper_bound(num_cols, 100.0);
  VariableTypeRow variable_type(num_cols,
                                VariableType::UPPER_AND_LOWER_BOUNDED);
  CompactSparseMatrix compact_matrix(matrix);
  InitialBasis initial_basis(compact_matrix, objective, lower_bound,
                             upper_bound, variable_type);

  // No replacement possible.
  RowToColMapping basis(num_rows, ColIndex(1));
  if (bixby_or_triangular) {
    initial_basis.CompleteBixbyBasis(num_cols, &basis);
  } else {
    initial_basis.CompleteTriangularPrimalBasis(num_cols, &basis);
  }
  for (RowIndex row(0); row < num_rows; ++row) {
    EXPECT_EQ(ColIndex(1), basis[row]);
  }

  // All replacements possible
  basis.assign(num_rows, kInvalidCol);
  if (bixby_or_triangular) {
    initial_basis.CompleteBixbyBasis(num_cols, &basis);
  } else {
    initial_basis.CompleteTriangularPrimalBasis(num_cols, &basis);
  }
  for (RowIndex row(0); row < num_rows; ++row) {
    EXPECT_EQ(RowToColIndex(row), basis[row]);
  }

  // Random replacements.
  const int kRandomSeed = 1;
  const int kOneIn = 3;
  std::mt19937 randomizer(kRandomSeed);
  basis.assign(num_rows, kInvalidCol);
  for (RowIndex row(0); row < num_rows; ++row) {
    if (absl::Bernoulli(randomizer, 1.0 / kOneIn)) {
      basis[row] = RowToColIndex(row);
    }
  }
  if (bixby_or_triangular) {
    initial_basis.CompleteBixbyBasis(num_cols, &basis);
  } else {
    initial_basis.CompleteTriangularPrimalBasis(num_cols, &basis);
  }
  for (RowIndex row(0); row < num_rows; ++row) {
    EXPECT_EQ(RowToColIndex(row), basis[row]);
  }
}

class InitialBasisTest
    : public ::testing::TestWithParam<GlopParameters::InitialBasisHeuristic> {};

TEST_P(InitialBasisTest, ExampleMatrix) {
  const SparseMatrix matrix{
      // Commenting each row so that clang-format doesn't collapse them.
      // Only the position of the nonzeros is taken into account so the values
      // are arbitrary.
      {1, 0, 2, 3, 0, 1, 0, 0},  // 0
      {4, 5, 0, 6, 7, 0, 1, 0},  // 1
      {8, 9, 0, 0, 3, 0, 0, 1}   // 2
  };

  const ColIndex num_cols = matrix.num_cols();
  const RowIndex num_rows = matrix.num_rows();
  DenseRow objective(num_cols, 0.0);

  // Columns with lower bounds are 1, 2, 3, 5, 6, 7.
  DenseRow lower_bound(num_cols, 0.0);
  lower_bound[ColIndex(0)] = -kInfinity;
  lower_bound[ColIndex(4)] = -kInfinity;

  // Columns with upper bounds are 2, 4, 5, 7.
  DenseRow upper_bound(num_cols, kInfinity);
  upper_bound[ColIndex(2)] = 1.0;
  upper_bound[ColIndex(4)] = 0.0;
  upper_bound[ColIndex(5)] = 0.0;
  upper_bound[ColIndex(7)] = 1.0;

  VariableTypeRow variable_type(num_cols);
  variable_type[ColIndex(0)] = VariableType::UNCONSTRAINED;
  variable_type[ColIndex(1)] = VariableType::LOWER_BOUNDED;
  variable_type[ColIndex(2)] = VariableType::UPPER_AND_LOWER_BOUNDED;
  variable_type[ColIndex(3)] = VariableType::LOWER_BOUNDED;
  variable_type[ColIndex(4)] = VariableType::UPPER_BOUNDED;
  variable_type[ColIndex(5)] = VariableType::FIXED_VARIABLE;
  variable_type[ColIndex(6)] = VariableType::LOWER_BOUNDED;
  variable_type[ColIndex(7)] = VariableType::UPPER_AND_LOWER_BOUNDED;

  CompactSparseMatrix compact_matrix(matrix);
  InitialBasis initial_basis(compact_matrix, objective, lower_bound,
                             upper_bound, variable_type);

  RowToColMapping basis(num_rows);

  if (GetParam() == GlopParameters::TRIANGULAR) {
    basis[RowIndex(0)] = ColIndex(5);
    basis[RowIndex(1)] = ColIndex(6);
    basis[RowIndex(2)] = ColIndex(7);
    initial_basis.CompleteTriangularPrimalBasis(num_cols, &basis);
    EXPECT_EQ(ColIndex(5), basis[RowIndex(0)]);
    EXPECT_EQ(ColIndex(6), basis[RowIndex(1)]);
    EXPECT_EQ(ColIndex(7), basis[RowIndex(2)]);
  }
  if (GetParam() == GlopParameters::MAROS) {
    initial_basis.GetPrimalMarosBasis(num_cols, &basis);
    EXPECT_EQ(ColIndex(0), basis[RowIndex(0)]);
    EXPECT_EQ(ColIndex(6), basis[RowIndex(1)]);
    EXPECT_EQ(ColIndex(1), basis[RowIndex(2)]);
  }
}

INSTANTIATE_TEST_SUITE_P(All, InitialBasisTest,
                         ::testing::Values(GlopParameters::TRIANGULAR,
                                           GlopParameters::MAROS));

TEST(BixbyBasisTest, TakeAllPossibleColumnsBixby) {
  TakeAllPossibleColumnsTest(true);
}

TEST(BixbyBasisTest, TakeAllPossibleColumnsTriangular) {
  TakeAllPossibleColumnsTest(false);
}

TEST(BixbyBasisTest, CorrectColumnOrder) {
  const ColIndex kSize(10);
  SparseMatrix matrix;
  matrix.PopulateFromIdentity(kSize);
  DenseRow objective({0.0, 0.0, 0.0, 1.0, -2.0, 3.0, 8.0, 2.0, 3.0, 4.0});
  DenseRow lower_bound(
      {0.0, -1.0, -kInfinity, -kInfinity, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
  DenseRow upper_bound(
      {10.0, kInfinity, 0.0, kInfinity, 10.0, 10.0, 0.0, 2.0, 3.0, 4.0});
  VariableTypeRow variable_type(
      {VariableType::UPPER_AND_LOWER_BOUNDED, VariableType::LOWER_BOUNDED,
       VariableType::UPPER_BOUNDED, VariableType::UNCONSTRAINED,
       VariableType::UPPER_AND_LOWER_BOUNDED,
       VariableType::UPPER_AND_LOWER_BOUNDED, VariableType::FIXED_VARIABLE,
       VariableType::UPPER_AND_LOWER_BOUNDED,
       VariableType::UPPER_AND_LOWER_BOUNDED,
       VariableType::UPPER_AND_LOWER_BOUNDED});
  CompactSparseMatrix compact_matrix(matrix);
  InitialBasis initial_basis(compact_matrix, objective, lower_bound,
                             upper_bound, variable_type);

  std::vector<ColIndex> candidates;
  initial_basis.ComputeCandidates(kSize, &candidates);
  std::vector<ColIndex> expected = {ColIndex(3), ColIndex(1), ColIndex(2),
                                    ColIndex(0), ColIndex(4), ColIndex(5),
                                    ColIndex(9), ColIndex(8), ColIndex(7)};
  EXPECT_THAT(candidates, ContainerEq(expected));
}

}  // namespace
}  // namespace glop
}  // namespace operations_research
