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

#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

#include "absl/random/random.h"
#include "benchmark/benchmark.h"
#include "ortools/set_cover/base_types.h"

namespace operations_research {
namespace {

SparseRow GenerateRandomSparseRow(size_t size, int64_t max_value) {
  SparseRow sparse_row;
  sparse_row.reserve(size);
  absl::BitGen gen;
  std::uniform_int_distribution<int64_t> dist(0, max_value);
  SubsetIndex current_value(0);
  for (size_t i = 0; i < size; ++i) {
    current_value += SubsetIndex(dist(gen));
    sparse_row.push_back(current_value);
  }
  return sparse_row;
}

static void BM_StrongVectorIteration(benchmark::State& state) {
  const size_t size = state.range(0);
  const int64_t delta_range = state.range(1);
  SparseRow strong_vector = GenerateRandomSparseRow(size, delta_range);
  for (auto _ : state) {
    int64_t sum = 0;
    for (const auto& x : strong_vector) {
      sum += x.value();
    }
    benchmark::DoNotOptimize(sum);  // Prevent optimization
  }
}
BENCHMARK(BM_StrongVectorIteration)
    ->ArgsProduct({{100'000, 100'000'000}, {1 << 8, 1 << 16}});

}  // namespace
}  // namespace operations_research
