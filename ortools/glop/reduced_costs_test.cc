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

#include "ortools/glop/reduced_costs.h"

#include <cstdlib>
#include <random>
#include <vector>

#include "absl/random/random.h"
#include "gtest/gtest.h"
#include "ortools/glop/basis_representation.h"
#include "ortools/glop/variables_info.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/sparse.h"

namespace operations_research {
namespace glop {
namespace {

TEST(ReducedCostsTest, CostPerturbation) {
  const ColIndex kNumCols(200);
  const RowIndex kNumRows(100);

  SparseMatrix matrix;
  // We don't care about the actual content, so it is okay to have "out of
  // bound" RowIndex for this test.
  matrix.PopulateFromIdentity(kNumCols);
  matrix.SetNumRows(kNumRows);
  CompactSparseMatrix compact_matrix(matrix);
  DenseRow objective(kNumCols, 0.0);
  DenseRow lower_bound(kNumCols, 0.0);
  DenseRow upper_bound(kNumCols, 0.0);
  DenseRow variable_values;
  RowToColMapping basis;
  VariablesInfo variables_info(compact_matrix);
  BasisFactorization basis_factorization(&compact_matrix, &basis);
  absl::BitGen random;
  ReducedCosts reduced_costs(compact_matrix, objective, basis, variables_info,
                             basis_factorization, random);

  // Consider a variety of type and cost.
  std::vector<Fractional> costs = {-1, -100, 0, 1, 100};

  auto flip_coin = [&]() -> bool {
    return std::uniform_int_distribution<int>(0, 1)(random);
  };
  for (ColIndex col(0); col < kNumCols; ++col) {
    lower_bound[col] = flip_coin() ? -kInfinity : 0.0;
    upper_bound[col] = flip_coin() ? +kInfinity : 1.0;
    objective[col] =
        costs[std::uniform_int_distribution<int>(0, costs.size() - 1)(random)];
  }

  // Only the types are used here.
  variables_info.LoadBoundsAndReturnTrueIfUnchanged(lower_bound, upper_bound);

  // Test that the perturbation is not too big and that it keeps
  // dual-feasibility.
  reduced_costs.PerturbCosts();
  const DenseRow& perturbations = reduced_costs.GetCostPerturbations();
  for (ColIndex col(0); col < kNumCols; ++col) {
    if (col >= kNumCols - RowToColIndex(kNumRows)) {
      // Only the "structural" variables are perturbed. Note that normally the
      // non-structural ones have a cost of zero, but not in this test.
      ASSERT_EQ(0.0, perturbations[col]);
      continue;
    }

    const double max_perturbation = 2e-5 * (std::abs(objective[col]) + 1);
    if (upper_bound[col] == kInfinity && lower_bound[col] != -kInfinity) {
      // Increasing the cost means increasing the reduced cost, so if it is
      // positive (aka dual-feasible for a lower-bounded variable) it will stay
      // positive.
      ASSERT_GT(perturbations[col], 0.0);
      ASSERT_LT(perturbations[col], max_perturbation);
    } else if (upper_bound[col] != kInfinity &&
               lower_bound[col] == -kInfinity) {
      ASSERT_GT(perturbations[col], -max_perturbation);
      ASSERT_LT(perturbations[col], 0.0);
    } else if (upper_bound[col] == kInfinity &&
               lower_bound[col] == -kInfinity) {
      ASSERT_EQ(perturbations[col], 0.0);
    } else {
      // Boxed variable. Such a variable can always be made dual-feasible by
      // setting it to the appropriate bound. In this case the perturbation keep
      // the sign of the cost.
      if (objective[col] == 0.0) {
        ASSERT_EQ(perturbations[col], 0.0);
      } else if (objective[col] > 0.0) {
        ASSERT_GT(perturbations[col], 0.0);
        ASSERT_LT(perturbations[col], max_perturbation);
      } else {
        ASSERT_GT(perturbations[col], -max_perturbation);
        ASSERT_LT(perturbations[col], 0.0);
      }
    }
  }
}

}  // namespace
}  // namespace glop
}  // namespace operations_research
