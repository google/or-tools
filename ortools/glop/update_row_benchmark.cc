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

#include <random>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/random/distributions.h"
#include "benchmark/benchmark.h"
#include "ortools/glop/basis_representation.h"
#include "ortools/glop/update_row.h"
#include "ortools/glop/variables_info.h"
#include "ortools/lp_data/lp_test_utils.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/sparse.h"

namespace operations_research {
namespace glop {
namespace {

void BM_ComputeUpdateRow(benchmark::State& state) {
  int num_lhs_entries = state.range(0);
  int algo_number = state.range(1);
  const ColIndex num_cols(10000);
  const RowIndex num_rows(100);
  const Fractional matrix_density(0.1);
  SparseMatrix matrix;
  std::mt19937 randomizer(12345);
  FillSparseMatrixRandomly(num_rows, num_cols, matrix_density, randomizer,
                           &matrix);
  CompactSparseMatrix compact_matrix(matrix);
  CompactSparseMatrix transposed_matrix;
  transposed_matrix.PopulateFromTranspose(compact_matrix);

  // Bounds do not matter as long as the variables are not fixed.
  // We assume the first columns are BASIC, so they will be ignored.
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

  // We want a random vector with *exactly* num_lhs_entries. The CHECK() is here
  // to prevent an infinite loop when num_lhs_entries becomes too big.
  CHECK_LE(num_lhs_entries, 90);
  std::mt19937 random(12345);
  DenseRow lhs(RowToColIndex(num_rows), 0.0);
  for (int i = 0; i < num_lhs_entries; ++i) {
    ColIndex col(absl::Uniform<int>(random, 0, num_rows.value()));
    while (lhs[col] != 0.0) {
      col = ColIndex(absl::Uniform<int>(random, 0, num_rows.value()));
    }
    lhs[col] = absl::Uniform<double>(random, -1.0, 1.0);
  }

  std::vector<std::string> algorithms{"column", "row", "row_hypersparse"};
  for (auto _ : state) {
    update_row.ComputeUpdateRowForBenchmark(lhs, algorithms[algo_number]);
  }
  // The destruction of the objects may take a while.
}

// To interpret the result, on my computer, the transition between "row" and
// "row_hypersparse" happens at about 12, which means just after there are more
// entries than the number of columns in the matrix.
BENCHMARK(BM_ComputeUpdateRow)->ArgPair(2, 1);
BENCHMARK(BM_ComputeUpdateRow)->ArgPair(3, 1);
BENCHMARK(BM_ComputeUpdateRow)->ArgPair(4, 1);
BENCHMARK(BM_ComputeUpdateRow)->ArgPair(5, 1);
BENCHMARK(BM_ComputeUpdateRow)->ArgPair(6, 1);
BENCHMARK(BM_ComputeUpdateRow)->ArgPair(7, 1);
BENCHMARK(BM_ComputeUpdateRow)->ArgPair(8, 1);
BENCHMARK(BM_ComputeUpdateRow)->ArgPair(9, 1);
BENCHMARK(BM_ComputeUpdateRow)->ArgPair(10, 1);
BENCHMARK(BM_ComputeUpdateRow)->ArgPair(11, 1);
BENCHMARK(BM_ComputeUpdateRow)->ArgPair(12, 1);
BENCHMARK(BM_ComputeUpdateRow)->ArgPair(13, 1);
BENCHMARK(BM_ComputeUpdateRow)->ArgPair(14, 1);
BENCHMARK(BM_ComputeUpdateRow)->ArgPair(15, 1);
BENCHMARK(BM_ComputeUpdateRow)->ArgPair(16, 1);

BENCHMARK(BM_ComputeUpdateRow)->ArgPair(2, 2);
BENCHMARK(BM_ComputeUpdateRow)->ArgPair(3, 2);
BENCHMARK(BM_ComputeUpdateRow)->ArgPair(4, 2);
BENCHMARK(BM_ComputeUpdateRow)->ArgPair(5, 2);
BENCHMARK(BM_ComputeUpdateRow)->ArgPair(6, 2);
BENCHMARK(BM_ComputeUpdateRow)->ArgPair(7, 2);
BENCHMARK(BM_ComputeUpdateRow)->ArgPair(8, 2);
BENCHMARK(BM_ComputeUpdateRow)->ArgPair(9, 2);
BENCHMARK(BM_ComputeUpdateRow)->ArgPair(10, 2);
BENCHMARK(BM_ComputeUpdateRow)->ArgPair(11, 2);
BENCHMARK(BM_ComputeUpdateRow)->ArgPair(12, 2);
BENCHMARK(BM_ComputeUpdateRow)->ArgPair(13, 2);
BENCHMARK(BM_ComputeUpdateRow)->ArgPair(14, 2);
BENCHMARK(BM_ComputeUpdateRow)->ArgPair(15, 2);
BENCHMARK(BM_ComputeUpdateRow)->ArgPair(16, 2);

// The "column" algorithm performance shouldn't depend on the density.
// There are 9900 columns (the first 100 BASIC ones are ignored).
BENCHMARK(BM_ComputeUpdateRow)->ArgPair(5, 0);
BENCHMARK(BM_ComputeUpdateRow)->ArgPair(10, 0);
BENCHMARK(BM_ComputeUpdateRow)->ArgPair(25, 0);
BENCHMARK(BM_ComputeUpdateRow)->ArgPair(50, 0);
BENCHMARK(BM_ComputeUpdateRow)->ArgPair(75, 0);

// The transition between column and row. It happens at around 60 on my machine
// in September 2013. That means that the number of row-wise entries is about
// 60% of the number of column-wise entries.
BENCHMARK(BM_ComputeUpdateRow)->ArgPair(20, 1);
BENCHMARK(BM_ComputeUpdateRow)->ArgPair(30, 1);
BENCHMARK(BM_ComputeUpdateRow)->ArgPair(40, 1);
BENCHMARK(BM_ComputeUpdateRow)->ArgPair(50, 1);
BENCHMARK(BM_ComputeUpdateRow)->ArgPair(60, 1);
BENCHMARK(BM_ComputeUpdateRow)->ArgPair(70, 1);
BENCHMARK(BM_ComputeUpdateRow)->ArgPair(80, 1);

}  // namespace
}  // namespace glop
}  // namespace operations_research
