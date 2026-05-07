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

// A high level benchmark to quickly evaluate if a change in radix_sort.h causes
// a performance regression.
//
// Note that the benchmarks in radix_sort_benchmark.cc are for tuning the
// actual algorithm, specifically, the radix width as a function of the input
// size. Performance regressions on radix widths we do not actually use do not
// matter. Further, that file defines so many different benchmarks that it is
// slow to run them all with replication. In this file, we try to come up with
// a minimal representative set.

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <random>
#include <type_traits>
#include <vector>

#include "absl/log/check.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/random/distributions.h"
#include "absl/types/span.h"
#include "benchmark/benchmark.h"
#include "ortools/algorithms/radix_sort.h"

namespace operations_research {
namespace {

template <typename T>
T RandomValue(absl::BitGenRef bit_gen) {
  if constexpr (std::is_integral_v<T>) {
    return absl::Uniform<T>(bit_gen, std::numeric_limits<T>::min(),
                            std::numeric_limits<T>::max());
  } else {
    T v = absl::Uniform<double>(bit_gen, 1.0, 2.0) *
          std::exp2(absl::Uniform(bit_gen, std::numeric_limits<T>::min_exponent,
                                  std::numeric_limits<T>::max_exponent));
    if (absl::Bernoulli(bit_gen, 0.5)) v = -v;
    return v;
  }
}

constexpr int kSmallDomainMaxInt = 10'000;

template <typename T>
T RandomValueSmallDomain(absl::BitGenRef bit_gen) {
  CHECK(std::is_integral_v<T>);
  return absl::Uniform<T>(bit_gen, 0, static_cast<T>(kSmallDomainMaxInt));
}

template <typename T>
void BM_RadixSort(benchmark::State& state) {
  std::mt19937_64 bit_gen;
  const int size = state.range(0);
  const bool small_domain = static_cast<bool>(state.range(1));
  std::vector<T> values;
  values.reserve(size);
  for (int i = 0; i < size; ++i) {
    if (small_domain) {
      values.push_back(RandomValueSmallDomain<T>(bit_gen));
    } else {
      values.push_back(RandomValue<T>(bit_gen));
    }
  }
  for (auto _ : state) {
    std::vector<T> to_sort = values;
    if (small_domain) {
      RadixSort(absl::MakeSpan(to_sort),
                NumBitsForZeroTo(static_cast<T>(kSmallDomainMaxInt)));
    } else {
      RadixSort(absl::MakeSpan(to_sort));
    }

    benchmark::DoNotOptimize(to_sort);
  }
  state.SetItemsProcessed(state.iterations() * size);
}

// Pick problem sizes to stress crossing various cache sizes.
//
// num_items | width bytes | memory
// 10K       | 4           | 40KB
// 500K      | 4           | 2MB
// 10M       | 4           | 40MB
// 10K       | 8           | 80KB
// 500K      | 8           | 4MB
// 10M       | 8           | 80MB
//
// (Note that radix sort actually needs a second copy of the vector and a
// little extra memory for bucket counts.)
//
// Cache size on a typical work station:
//
// $ getconf -a | grep CACHE
// L1d cache:                               32 KB
// L1i cache:                               64 KB
// L2 cache:                                2 MB
// L3 cache:                                25 MB
constexpr auto kProblemSizes = std::to_array({10'000, 500'000, 10'000'000});

void BenchmarkSortArgs(benchmark::Benchmark* bench) {
  for (const int num_items : kProblemSizes) {
    bench->Args({num_items, /*small_domain=*/0});
  }
}

void BenchmarkSortArgsWithSmallDomain(benchmark::Benchmark* bench) {
  for (const int num_items : kProblemSizes) {
    for (const bool small_domain : {false, true}) {
      bench->Args({num_items, static_cast<int>(small_domain)});
    }
  }
}

BENCHMARK_TEMPLATE(BM_RadixSort, int)->Apply(BenchmarkSortArgsWithSmallDomain);
BENCHMARK_TEMPLATE(BM_RadixSort, int64_t)
    ->Apply(BenchmarkSortArgsWithSmallDomain);
BENCHMARK_TEMPLATE(BM_RadixSort, uint32_t)
    ->Apply(BenchmarkSortArgsWithSmallDomain);
BENCHMARK_TEMPLATE(BM_RadixSort, uint64_t)
    ->Apply(BenchmarkSortArgsWithSmallDomain);
BENCHMARK_TEMPLATE(BM_RadixSort, float)->Apply(BenchmarkSortArgs);
BENCHMARK_TEMPLATE(BM_RadixSort, double)->Apply(BenchmarkSortArgs);

}  // namespace
}  // namespace operations_research
