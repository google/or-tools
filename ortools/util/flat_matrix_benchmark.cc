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

#include <cstdint>

#include "absl/base/casts.h"
#include "absl/log/check.h"
#include "benchmark/benchmark.h"
#include "ortools/util/flat_matrix.h"
#include "ortools/util/random_engine.h"

namespace operations_research {
namespace {

template <bool do_work>
void BM_FlatMatrixRandomAccess(benchmark::State& state) {
  const int size = state.range(0);
  const uint64_t mask = size - 1;
  CHECK(!(mask & size)) << "size must be a power of 2: " << size;
  FlatMatrix<double> m(size, size);
  random_engine_t random;
  double sum = 0;
  for (const auto& _ : state) {
    const uint64_t x = random();
    if (do_work) {
      sum += m[x & mask][(x >> 32) & mask];
    } else {
      // The 'baseline' timing does similar operations, without the matrix
      // lookup.
      sum += absl::bit_cast<double>(x & ((mask << 32) | mask));
    }
  }
  benchmark::DoNotOptimize(sum);
}

BENCHMARK(BM_FlatMatrixRandomAccess<false>)->Range(1, 1 << 12);
BENCHMARK(BM_FlatMatrixRandomAccess<true>)->Range(1, 1 << 12);

}  // namespace
}  // namespace operations_research
