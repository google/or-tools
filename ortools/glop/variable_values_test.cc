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

#include "ortools/glop/variable_values.h"

#include <limits>
#include <vector>

#include "absl/random/random.h"
#include "gtest/gtest.h"
#include "ortools/glop/basis_representation.h"
#include "ortools/glop/dual_edge_norms.h"
#include "ortools/glop/parameters.pb.h"
#include "ortools/glop/pricing.h"
#include "ortools/glop/variables_info.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/sparse.h"

namespace operations_research {
namespace glop {
namespace {

TEST(VariableValuesTest, PrimalInfeasibility) {
  // Wire everything for a 4x4 identity LP matrix with 4 variables fixed at 1.
  const ColIndex kNumCols(4);
  SparseMatrix matrix;
  matrix.PopulateFromIdentity(kNumCols);
  CompactSparseMatrix compact_matrix(matrix);
  DenseRow lower_bound({1.0, 1.0, 1.0, 1.0});
  DenseRow upper_bound({1.0, 1.0, 1.0, 1.0});
  VariablesInfo variable_info(compact_matrix);
  variable_info.LoadBoundsAndReturnTrueIfUnchanged(lower_bound, upper_bound);
  variable_info.InitializeToDefaultStatus();
  MatrixView matrix_view(matrix);
  RowToColMapping basis({ColIndex(0), ColIndex(1), ColIndex(2), ColIndex(3)});
  BasisFactorization factorization(&compact_matrix, &basis);
  GlopParameters parameters;
  DualEdgeNorms dual_edge_norms(factorization);
  absl::BitGen random;
  DynamicMaximum<RowIndex> dual_prices(random);
  VariableValues variable_values(parameters, compact_matrix, basis,
                                 variable_info, factorization, &dual_edge_norms,
                                 &dual_prices);

  // Test corner case infeasibility.
  // The infeasibility should always be computed by (value - upper_bound) or
  // (lower_bound - value);
  DenseRow unused;
  variable_values.ResetAllNonBasicVariableValues(unused);
  parameters.set_primal_feasibility_tolerance(1e-6);
  const double epsilon = std::numeric_limits<double>::epsilon();
  variable_values.Set(ColIndex(0), 1.0 - 1e-6);
  variable_values.Set(ColIndex(1), (1.0 - 1e-6) + epsilon);  // feasible.
  variable_values.Set(ColIndex(2), 1.0 + 1e-6);              // feasible.
  variable_values.Set(ColIndex(3), (1.0 + 1e-6) + epsilon);

  DenseRow objective(kNumCols, 0.0);
  std::vector<RowIndex> all_rows{RowIndex(0), RowIndex(1), RowIndex(2),
                                 RowIndex(3)};
  variable_values.UpdatePrimalPhaseICosts(all_rows, &objective);
  EXPECT_EQ(objective, (DenseRow{-1.0, 0.0, 0.0, 1.0}));
  EXPECT_EQ(variable_values.ComputeMaximumPrimalInfeasibility(),
            ((1.0 + 1e-6) + epsilon) - 1.0);
}

}  // namespace
}  // namespace glop
}  // namespace operations_research
