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

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <random>
#include <type_traits>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/log/log.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/random/distributions.h"
#include "absl/random/random.h"
#include "absl/types/span.h"
#include "benchmark/benchmark.h"
#include "ortools/algorithms/radix_sort.h"

namespace operations_research {
namespace {

template <typename T>
std::vector<T> RandomValues(absl::BitGenRef rng, size_t size,
                            bool allow_negative, std::optional<T> max_abs_val) {
  std::vector<T> values(size);
  for (T& v : values) {
    if constexpr (std::is_integral_v<T>) {
      v = absl::Uniform<T>(absl::IntervalClosedClosed, rng, 0,
                           max_abs_val.value_or(std::numeric_limits<T>::max()));
    } else {
      v = absl::Uniform<double>(rng, 1.0, 2.0) *
          std::exp2(absl::Uniform(rng, std::numeric_limits<T>::min_exponent,
                                  std::numeric_limits<T>::max_exponent));
    }
    if (allow_negative && absl::Bernoulli(rng, 0.5)) v = -v;
  }
  return values;
}

template <typename T>
std::vector<T> SortedValues(size_t size) {
  const T offset = std::is_signed_v<T> ? -static_cast<T>(size) / 2 : T{0};
  std::vector<T> values(size);
  for (size_t i = 0; i < size; ++i) values[i] = i + offset;
  return values;
}

enum Algo {
  kStdSort,
  kRadixSortTpl,
  kRadixSortKnownMax,
  kRadixSortComputeMax,
  kRadixSortWorst,
};

enum InputOrder {
  kRandom,
  kSorted,
  kAlmostSorted,
  kReverseSorted,
};

template <Algo algo, typename T, InputOrder input_order, int radix_width,
          int num_passes>
void BM_Sort(benchmark::State& state) {
  const size_t size = state.range(0);
  std::optional<T> max_abs_val;
  if constexpr (!std::is_signed_v<T> &&
                radix_width * num_passes < std::numeric_limits<T>::digits) {
    max_abs_val = (T{1} << (radix_width * num_passes)) - 1;
  }
  std::mt19937 rng(1234);
  std::vector<T> values =
      input_order == kRandom
          ? RandomValues(rng, size, std::is_signed_v<T>, max_abs_val)
          : SortedValues<T>(size);
  if (input_order == kReverseSorted) {
    absl::c_reverse(values);
  }
  if (input_order == kAlmostSorted) {
    const int p1 = absl::Uniform<int>(rng, 0, size);
    const int p2 = absl::Uniform<int>(rng, 0, size);
    std::swap(values[p1], values[p2]);
  }
  std::vector<T> to_sort = values;
  for (auto _ : state) {
    to_sort = values;
    if constexpr (algo == kStdSort) {
      absl::c_sort(to_sort);
    } else if constexpr (algo == kRadixSortTpl) {
      absl::Span<T> span{to_sort.data(), to_sort.size()};
      RadixSortTpl<T, radix_width, num_passes>(span);
    } else if constexpr (algo == kRadixSortKnownMax) {
      absl::Span<T> span = absl::MakeSpan(to_sort);
      RadixSort(span, NumBitsForZeroTo(
                          max_abs_val.value_or(std::numeric_limits<T>::max())));
    } else if constexpr (algo == kRadixSortComputeMax) {
      absl::Span<T> span{to_sort.data(), to_sort.size()};
      RadixSort(span, NumBitsForZeroTo(
                          size == 0 ? 1 : *absl::c_max_element(to_sort)));
    } else if constexpr (algo == kRadixSortWorst) {
      absl::Span<T> span{to_sort.data(), to_sort.size()};
      RadixSort(span);
    } else {
      LOG(DFATAL) << "Unsupported algo: " << algo;
    }
    benchmark::DoNotOptimize(to_sort);
  }
  state.SetItemsProcessed(state.iterations() * size);
}

// For std::sort(), we don't benchmark with much detail past 32k, because it
// reaches its "very" slow part where the item/s throughput is almost
// horizontal.
BENCHMARK(BM_Sort<kStdSort, int, kRandom, 1, 1>)
    ->RangeMultiplier(2)
    ->Range(1, 32 << 10)
    ->Arg(128 << 10)
    ->Arg(1 << 20)
    ->Arg(8 << 20);

BENCHMARK(BM_Sort<kStdSort, int64_t, kRandom, 1, 1>)
    ->RangeMultiplier(2)
    ->Range(1, 32 << 10)
    ->Arg(128 << 10)
    ->Arg(1 << 20)
    ->Arg(8 << 20);

BENCHMARK(BM_Sort<kStdSort, int, kSorted, 1, 1>)
    ->RangeMultiplier(2)
    ->Range(1, 128 << 10);

BENCHMARK(BM_Sort<kStdSort, int, kReverseSorted, 1, 1>)
    ->RangeMultiplier(2)
    ->Range(1, 128 << 10);

BENCHMARK(BM_Sort<kStdSort, int, kAlmostSorted, 1, 1>)
    ->RangeMultiplier(2)
    ->Range(1, 128 << 10);

BENCHMARK(BM_Sort<kRadixSortTpl, uint32_t, kRandom, 8, 4>)
    ->RangeMultiplier(2)
    ->Range(16, 2048);
BENCHMARK(BM_Sort<kRadixSortTpl, uint32_t, kRandom, 11, 3>)
    ->RangeMultiplier(2)
    ->Range(256, 32 << 20);
BENCHMARK(BM_Sort<kRadixSortTpl, uint32_t, kRandom, 16, 2>)
    ->RangeMultiplier(2)
    ->Range(128 << 10, 32 << 20);

BENCHMARK(BM_Sort<kRadixSortTpl, int, kRandom, 8, 4>)
    ->RangeMultiplier(2)
    ->Range(16, 2048);
BENCHMARK(BM_Sort<kRadixSortTpl, int, kRandom, 11, 3>)
    ->RangeMultiplier(2)
    ->Range(256, 32 << 20);
BENCHMARK(BM_Sort<kRadixSortTpl, int, kRandom, 16, 2>)
    ->RangeMultiplier(2)
    ->Range(128 << 10, 32 << 20);

BENCHMARK(BM_Sort<kRadixSortKnownMax, int, kRandom, 16, 2>)
    ->RangeMultiplier(2)
    ->Range(128 << 10, 32 << 20);
BENCHMARK(BM_Sort<kRadixSortComputeMax, int, kRandom, 16, 2>)
    ->RangeMultiplier(2)
    ->Range(128 << 10, 32 << 20);
BENCHMARK(BM_Sort<kRadixSortWorst, int, kRandom, 16, 2>)
    ->RangeMultiplier(2)
    ->Range(128 << 10, 32 << 20);

BENCHMARK(BM_Sort<kRadixSortTpl, float, kRandom, 8, 4>)
    ->RangeMultiplier(2)
    ->Range(16, 2048);
BENCHMARK(BM_Sort<kRadixSortTpl, float, kRandom, 11, 3>)
    ->RangeMultiplier(2)
    ->Range(256, 32 << 20);
BENCHMARK(BM_Sort<kRadixSortTpl, float, kRandom, 16, 2>)
    ->RangeMultiplier(2)
    ->Range(128 << 10, 32 << 20);

BENCHMARK(BM_Sort<kRadixSortTpl, uint64_t, kRandom, 11, 6>)
    ->RangeMultiplier(2)
    ->Range(2048, 8 << 20)
    ->Arg(32 << 20)
    ->Arg(128 << 20);
BENCHMARK(BM_Sort<kRadixSortTpl, uint64_t, kRandom, 13, 5>)
    ->RangeMultiplier(2)
    ->Range(2048, 8 << 20)
    ->Arg(32 << 20)
    ->Arg(128 << 20);
BENCHMARK(BM_Sort<kRadixSortTpl, uint64_t, kRandom, 16, 4>)
    ->RangeMultiplier(2)
    ->Range(128 << 10, 8 << 20)
    ->Arg(32 << 20)
    ->Arg(128 << 20);
BENCHMARK(BM_Sort<kRadixSortTpl, uint64_t, kRandom, 22, 3>)
    ->RangeMultiplier(2)
    ->Range(128 << 10, 8 << 20)
    ->Arg(32 << 20)
    ->Arg(128 << 20);

BENCHMARK(BM_Sort<kRadixSortTpl, int64_t, kRandom, 11, 6>)
    ->RangeMultiplier(2)
    ->Range(2048, 8 << 20)
    ->Arg(32 << 20)
    ->Arg(128 << 20);
BENCHMARK(BM_Sort<kRadixSortTpl, int64_t, kRandom, 13, 5>)
    ->RangeMultiplier(2)
    ->Range(2048, 8 << 20)
    ->Arg(32 << 20)
    ->Arg(128 << 20);
BENCHMARK(BM_Sort<kRadixSortTpl, int64_t, kRandom, 16, 4>)
    ->RangeMultiplier(2)
    ->Range(128 << 10, 8 << 20)
    ->Arg(32 << 20)
    ->Arg(128 << 20);
BENCHMARK(BM_Sort<kRadixSortTpl, int64_t, kRandom, 22, 3>)
    ->RangeMultiplier(2)
    ->Range(128 << 10, 8 << 20)
    ->Arg(32 << 20)
    ->Arg(128 << 20);

BENCHMARK(BM_Sort<kRadixSortTpl, double, kRandom, 11, 6>)
    ->RangeMultiplier(2)
    ->Range(2048, 8 << 20)
    ->Arg(32 << 20)
    ->Arg(128 << 20);
BENCHMARK(BM_Sort<kRadixSortTpl, double, kRandom, 13, 5>)
    ->RangeMultiplier(2)
    ->Range(2048, 8 << 20)
    ->Arg(32 << 20)
    ->Arg(128 << 20);
BENCHMARK(BM_Sort<kRadixSortTpl, double, kRandom, 16, 4>)
    ->RangeMultiplier(2)
    ->Range(128 << 10, 8 << 20)
    ->Arg(32 << 20)
    ->Arg(128 << 20);
BENCHMARK(BM_Sort<kRadixSortTpl, double, kRandom, 22, 3>)
    ->RangeMultiplier(2)
    ->Range(128 << 10, 8 << 20)
    ->Arg(32 << 20)
    ->Arg(128 << 20);

}  // namespace
}  // namespace operations_research
