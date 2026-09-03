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
#include <limits>

#include "absl/numeric/int128.h"
#include "benchmark/benchmark.h"
#include "ortools/algorithms/binary_search.h"

namespace operations_research {
namespace {

template <typename T>
void BM_BinarySearch(benchmark::State& state) {
  auto functor = [](T x) { return x > std::numeric_limits<T>::max() / 2; };
  for (const auto s : state) {
    benchmark::DoNotOptimize(functor);
    auto result = BinarySearch<T>(std::numeric_limits<T>::max(),
                                  std::numeric_limits<T>::min(), functor);
    benchmark::DoNotOptimize(result);
  }
}
BENCHMARK(BM_BinarySearch<float>);
BENCHMARK(BM_BinarySearch<double>);
BENCHMARK(BM_BinarySearch<int>);
BENCHMARK(BM_BinarySearch<unsigned>);
BENCHMARK(BM_BinarySearch<int64_t>);
BENCHMARK(BM_BinarySearch<uint64_t>);
BENCHMARK(BM_BinarySearch<absl::int128>);

}  // namespace
}  // namespace operations_research
