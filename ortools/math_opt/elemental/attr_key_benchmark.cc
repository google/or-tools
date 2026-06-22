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
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "benchmark/benchmark.h"
#include "ortools/math_opt/elemental/attr_key.h"
#include "ortools/math_opt/elemental/symmetry.h"
#include "ortools/math_opt/elemental/testing.h"

namespace operations_research::math_opt {
namespace {

constexpr int kBenchmarkSize = 30;

template <typename SetT>
void BM_HashSet0(benchmark::State& state) {
  SetT set;
  for (const auto s : state) {
    auto it = set.find(AttrKey());
    benchmark::DoNotOptimize(it);
  }
}
BENCHMARK(BM_HashSet0<AttrKeyHashSet<AttrKey<0>>>);
BENCHMARK(BM_HashSet0<absl::flat_hash_set<AttrKey<0>>>);

template <typename T>
void BM_HashMap1(benchmark::State& state) {
  absl::flat_hash_map<T, int> map;
  for (int i = 0; i < kBenchmarkSize * kBenchmarkSize; ++i) {
    if (i % 2 > 0) {  // Half of the lookups are hits.
      map[T(i)] = i;
    }
  }
  for (const auto s : state) {
    for (int i = 0; i < kBenchmarkSize * kBenchmarkSize; ++i) {
      auto it = map.find(T(i));
      benchmark::DoNotOptimize(it);
    }
  }
}
BENCHMARK(BM_HashMap1<AttrKey<1>>);
BENCHMARK(BM_HashMap1<int64_t>);

template <typename T>
void BM_HashMap2(benchmark::State& state) {
  absl::flat_hash_map<T, int> map;
  for (int i = 0; i < kBenchmarkSize; ++i) {
    for (int j = 0; j < kBenchmarkSize; ++j) {
      if ((i * kBenchmarkSize + j) % 2 > 0) {  // Half of the lookups are hits.
        map[T(i, j)] = i;
      }
    }
  }
  for (const auto s : state) {
    for (int i = 0; i < kBenchmarkSize; ++i) {
      for (int j = 0; j < kBenchmarkSize; ++j) {
        auto it = map.find(T(i, j));
        benchmark::DoNotOptimize(it);
      }
    }
  }
}
BENCHMARK(BM_HashMap2<AttrKey<2>>);
BENCHMARK(BM_HashMap2<std::pair<int64_t, int64_t>>);

template <int n>
void BM_SortAttrKeys(benchmark::State& state) {
  const std::vector<AttrKey<n>> keys =
      MakeRandomAttrKeys<n, NoSymmetry>(state.range(0), state.range(0));

  for (const auto s : state) {
    auto copy = keys;
    absl::c_sort(copy);
    benchmark::DoNotOptimize(copy);
  }
}
BENCHMARK(BM_SortAttrKeys<1>)->Arg(100)->Arg(10000);
BENCHMARK(BM_SortAttrKeys<2>)->Arg(100)->Arg(10000);

}  // namespace
}  // namespace operations_research::math_opt
