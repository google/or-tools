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

#include <stdio.h>

#include <random>

#include "absl/random/bit_gen_ref.h"
#include "benchmark/benchmark.h"
#include "ortools/lp_data/lp_test_utils.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/lp_utils.h"
#include "ortools/lp_data/sparse.h"

namespace operations_research {
namespace glop {
namespace {

void BM_ColumnScalarProduct(benchmark::State& state) {
  constexpr int kAspectRatio = 5;
  const ColIndex num_cols(state.range(0));
  const RowIndex num_rows(kAspectRatio * state.range(0));
  SparseMatrix matrix;
  std::mt19937 randomizer(12345);
  FillSparseMatrixRandomly(num_rows, num_cols, /*density=*/0.2, randomizer,
                           &matrix);
  CompactSparseMatrix compact_matrix(matrix);

  DenseColumn dense_column;
  compact_matrix.ColumnCopyToDenseColumn(ColIndex(num_cols.value() / 2),
                                         &dense_column);
  const auto view = compact_matrix.view();

  // Check scalar product with all the other columns.
  for (const auto s : state) {
    for (ColIndex col(0); col < matrix.num_cols(); ++col) {
      auto product =
          view.ColumnScalarProduct(col, Transpose(dense_column).const_view());
      benchmark::DoNotOptimize(product);
    }
  }
}

BENCHMARK(BM_ColumnScalarProduct)->Range(256, 4096);

}  // namespace
}  // namespace glop
}  // namespace operations_research
