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

#include <vector>

#include "absl/random/random.h"
#include "benchmark/benchmark.h"
#include "ortools/util/random_subset.h"

namespace operations_research {
namespace {

void BM_RandomSubset(benchmark::State& state) {
  absl::BitGen random;
  const int N = state.range(0);
  const int K = state.range(1);
  for (auto _ : state) {
    std::vector<int> subset = RandomSubset(N, K, random);
    benchmark::DoNotOptimize(subset);
  }
}

void BenchmarkArgs(benchmark::Benchmark* bm) {
  for (int n : {100, 1024, 10240, 102400, 1048576, 10485760, 104857600}) {
    std::vector<int> k_values = {0, 1, 2, 3, 4};
    while (k_values.back() < n / 10) {
      k_values.push_back(k_values.back() * 2);
    }
    for (int k : k_values) {
      bm->Args({n, k});
    }
  }
}
BENCHMARK(BM_RandomSubset)->Apply(BenchmarkArgs);

}  // namespace
}  // namespace operations_research
