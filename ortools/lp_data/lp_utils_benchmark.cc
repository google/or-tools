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

#include "absl/log/check.h"
#include "benchmark/benchmark.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/lp_utils.h"

namespace operations_research {
namespace glop {
namespace {

template <bool precise>
static void BM_SquaredNorm(benchmark::State& state) {
  const RowIndex num_rows(1 << 20);
  DenseColumn col(num_rows, 0.0);
  for (RowIndex i(0); i < num_rows; ++i) {
    col[i] = Fractional(i.value());
  }
  for (auto _ : state) {
    CHECK_GE(precise ? PreciseSquaredNorm(col) : SquaredNorm(col), 0.0);
  }
}

#define PRECISE true
BENCHMARK_TEMPLATE(BM_SquaredNorm, !PRECISE);
BENCHMARK_TEMPLATE(BM_SquaredNorm, PRECISE);
#undef PRECISE

}  // namespace
}  // namespace glop
}  // namespace operations_research
