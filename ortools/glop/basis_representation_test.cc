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

#include "ortools/glop/basis_representation.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <random>
#include <vector>

#include "absl/random/bit_gen_ref.h"
#include "absl/random/distributions.h"
#include "absl/random/random.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/lp_types_testing.h"
#include "ortools/lp_data/lp_utils.h"
#include "ortools/lp_data/permutation.h"
#include "ortools/lp_data/scattered_vector.h"
#include "ortools/lp_data/sparse.h"
#include "ortools/util/fp_utils.h"

namespace operations_research {
namespace glop {
namespace {

using ::testing::ContainerEq;

// Parameterized test to test different densities of eta vectors.
class EtaMatrixTest : public ::testing::TestWithParam<double> {
 protected:
  double GetEtaDensity() { return GetParam(); }
  double GetRhsDensity() { return 0.6; }
};

// Generates a random dense column with given non-zero density.
DenseColumn GenerateDenseColumn(absl::BitGenRef randomizer,
                                double target_density) {
  const RowIndex kSize(1 << 16);
  DenseColumn result(kSize, 0.0);
  for (RowIndex i(0); i < kSize; ++i) {
    const bool is_not_zero = absl::Bernoulli(randomizer, target_density);
    if (is_not_zero) {
      result[i] = absl::Uniform<double>(randomizer, -10.0, 10.0);
    }
  }

  // Make sure it looks like expected.
  Fractional expected_square_of_coeff = 100.0 / 3.0;
  EXPECT_GE(SquaredNorm(result),
            target_density * kSize.value() * expected_square_of_coeff * 0.90);
  EXPECT_LE(SquaredNorm(result),
            target_density * kSize.value() * expected_square_of_coeff * 1.10);
  return result;
}

void CopyAndFillNonZero(const DenseColumn& input,
                        std::vector<RowIndex>* non_zeros, DenseColumn* output) {
  *output = input;
  for (RowIndex row(0); row < input.size(); ++row) {
    if (input[row] != 0.0) {
      non_zeros->push_back(row);
    }
  }
}

TEST_P(EtaMatrixTest, LeftSolveMathCorrectness) {
  const int kRandomSeed = 1;
  std::mt19937 randomizer(kRandomSeed);

  DenseColumn eta_col = GenerateDenseColumn(randomizer, GetEtaDensity());
  const ColIndex eta_index(
      absl::Uniform<int32_t>(randomizer, 0, eta_col.size().value()));
  if (eta_col[ColToRowIndex(eta_index)] == 0.0) {
    eta_col[ColToRowIndex(eta_index)] = 1.0;
  }

  ScatteredColumn dense_eta;
  CopyAndFillNonZero(eta_col, &dense_eta.non_zeros, &dense_eta.values);
  EtaMatrix matrix(eta_index, dense_eta);

  std::vector<ColIndex> temp_non_zero_positions;
  for (int num_solve = 0; num_solve < 3; ++num_solve) {
    // Test that y.E = c by computing the matrix product.
    const DenseRow c =
        Transpose(GenerateDenseColumn(randomizer, GetRhsDensity()));
    DenseRow y = c;
    matrix.LeftSolve(&y);
    for (ColIndex j(0); j < c.size(); ++j) {
      if (j == eta_index) {
        EXPECT_COMPARABLE(c[j], ScalarProduct(y, eta_col), Fractional(1e-8));
      } else {
        EXPECT_EQ(c[j], y[j]);
      }
    }

    // Test the sparse version.
    DenseRow y_for_sparse_solve = c;
    temp_non_zero_positions.clear();
    bool eta_index_is_present = false;
    for (ColIndex j(0); j < c.size(); ++j) {
      if (c[j] != 0.0) {
        temp_non_zero_positions.push_back(j);
        if (j == eta_index) eta_index_is_present = true;
      }
    }
    matrix.SparseLeftSolve(&y_for_sparse_solve, &temp_non_zero_positions);
    if (!eta_index_is_present) {
      EXPECT_EQ(eta_index, temp_non_zero_positions.back());
    }
    EXPECT_THAT(y_for_sparse_solve, ContainerEq(y));
  }
}

TEST_P(EtaMatrixTest, RightSolveMathCorrectness) {
  const int kRandomSeed = 1;
  std::mt19937 randomizer(kRandomSeed);

  DenseColumn eta_col = GenerateDenseColumn(randomizer, GetEtaDensity());
  const ColIndex eta_index(
      absl::Uniform<int32_t>(randomizer, 0, eta_col.size().value()));
  if (eta_col[ColToRowIndex(eta_index)] == 0.0) {
    eta_col[ColToRowIndex(eta_index)] = 1.0;
  }
  ScatteredColumn dense_eta;
  CopyAndFillNonZero(eta_col, &dense_eta.non_zeros, &dense_eta.values);
  EtaMatrix matrix(eta_index, dense_eta);

  for (int num_solve = 0; num_solve < 3; ++num_solve) {
    // Test that E.d = a by computing the matrix product.
    const DenseColumn a = GenerateDenseColumn(randomizer, GetRhsDensity());
    DenseColumn d = a;
    matrix.RightSolve(&d);
    for (RowIndex i(0); i < d.size(); ++i) {
      if (i == ColToRowIndex(eta_index)) {
        EXPECT_COMPARABLE(a[i], eta_col[i] * d[i], Fractional(1e-12));
      } else {
        EXPECT_COMPARABLE(a[i], d[i] + eta_col[i] * d[ColToRowIndex(eta_index)],
                          Fractional(1e-12));
      }
    }
  }
}

INSTANTIATE_TEST_SUITE_P(All, EtaMatrixTest,
                         ::testing::Values(0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6,
                                           0.7, 0.8, 0.9, 1.0));

// TODO(user): add unit tests on BasisFactorization.

TEST(BasisFactorizationTest, ConditionNumber) {
  const Fractional kEps = 0.0001;
  const SparseMatrix kMatrix{{1, 3}, {2, 6 - kEps}};
  RowToColMapping basis(kMatrix.num_rows(), kInvalidCol);
  for (RowIndex row(0); row < kMatrix.num_rows(); ++row) {
    basis[row] = RowToColIndex(row);
  }
  CompactSparseMatrix compact_matrix(kMatrix);
  BasisFactorization factorization(&compact_matrix, &basis);
  AbnormalityStatus refactorization_status =
      factorization.ForceRefactorization();
  if (!factorization.GetColumnPermutation().empty()) {
    RowToColMapping tmp;
    ApplyColumnPermutationToRowIndexedVector(
        factorization.GetColumnPermutation().const_view(), &basis, &tmp);
    factorization.SetColumnPermutationToIdentity();
  }

  // TODO(user): add a unit test that updates the eta product of the matrix,
  // and does not use the refactorized form.
  // The results were formally computed using Maxima:
  // a:matrix([1,3],[2,6-eps]);
  // [mat_norm(a,1), mat_norm(invert(a),1)];
  // [mat_norm(a,inf), mat_norm(invert(a),inf)];
  EXPECT_THAT(refactorization_status, AbnormalityStatusIsOK());
  const Fractional e = std::abs(kEps);
  const Fractional f = std::abs(6 - kEps);

  const Fractional one_norm = 3 + f;
  const Fractional inv_one_norm = std::max(4 / e, (f + 2) / e);
  const Fractional one_cond_number = one_norm * inv_one_norm;
  EXPECT_COMPARABLE(one_norm, factorization.ComputeOneNorm(), Fractional(1e-9));
  EXPECT_COMPARABLE(inv_one_norm, factorization.ComputeInverseOneNorm(),
                    Fractional(1e-9));
  EXPECT_COMPARABLE(one_cond_number,
                    factorization.ComputeOneNormConditionNumber(),
                    Fractional(1e-9));

  const Fractional inf_norm = std::max(Fractional(4), f + 2);
  const Fractional inv_inf_norm = (3 + f) / e;
  const Fractional inf_cond_number = one_norm * inv_one_norm;
  EXPECT_COMPARABLE(inf_norm, factorization.ComputeInfinityNorm(),
                    Fractional(1e-9));
  EXPECT_COMPARABLE(inv_inf_norm, factorization.ComputeInverseInfinityNorm(),
                    Fractional(1e-9));
  EXPECT_COMPARABLE(inf_cond_number,
                    factorization.ComputeInfinityNormConditionNumber(),
                    Fractional(1e-9));
}

}  // namespace
}  // namespace glop
}  // namespace operations_research
