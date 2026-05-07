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

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include "absl/random/random.h"
#include "absl/types/span.h"
#include "benchmark/benchmark.h"
#include "ortools/algorithms/multikey_radix_sort.h"
#include "ortools/algorithms/radix_sort.h"

namespace operations_research {
namespace {

template <typename T>
constexpr std::pair<T, T> NonNegativeRange(
    const T max_value = std::numeric_limits<T>::max()) {
  return {0, max_value};
}

template <typename T>
T GenerateRandomValue(absl::BitGen& bitgen, std::pair<T, T> bounds) {
  return absl::Uniform<T>(absl::IntervalClosedClosed, bitgen, bounds.first,
                          bounds.second);
}

template <typename T>
std::vector<T> GenerateRandomVector(size_t length, std::pair<T, T> bounds) {
  absl::BitGen bitgen;
  std::vector<T> values(length);
  for (size_t i = 0; i < length; ++i) {
    values[i] = GenerateRandomValue<T>(bitgen, bounds);
  }
  return values;
}

std::vector<std::pair<int32_t, int32_t>> GenerateRandomInt32PairVector(
    size_t length, int max_value) {
  absl::BitGen bitgen;
  std::vector<std::pair<int32_t, int32_t>> values(length);
  const auto bounds = NonNegativeRange<int32_t>(max_value);
  for (size_t i = 0; i < length; ++i) {
    values[i].first = GenerateRandomValue<int32_t>(bitgen, bounds);
    values[i].second = GenerateRandomValue<int32_t>(bitgen, bounds);
  }
  return values;
}

void BM_StdSort(benchmark::State& state) {
  const size_t length = state.range(0);
  const int bounds = 10 * length;
  auto values = GenerateRandomInt32PairVector(length, bounds);
  for (auto _ : state) {
    state.PauseTiming();
    auto copy = values;
    state.ResumeTiming();
    std::sort(copy.begin(), copy.end());
    benchmark::DoNotOptimize(copy);
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * length);
}
BENCHMARK(BM_StdSort)->RangeMultiplier(2)->Range(8, 16 << 20);

void BM_StdSortInt64Max(benchmark::State& state) {
  const size_t length = state.range(0);
  auto values = GenerateRandomVector(length, NonNegativeRange<int64_t>());
  for (auto _ : state) {
    state.PauseTiming();
    auto copy = values;
    state.ResumeTiming();
    std::sort(copy.begin(), copy.end());
    benchmark::DoNotOptimize(copy);
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * length);
}
BENCHMARK(BM_StdSortInt64Max)->RangeMultiplier(10)->Range(100, 1'000'000);

void BM_StdSortInt64Small(benchmark::State& state) {
  const size_t length = state.range(0);
  auto values = GenerateRandomVector<int64_t>(length, {0, 100'000});
  for (auto _ : state) {
    state.PauseTiming();
    auto copy = values;
    state.ResumeTiming();
    std::sort(copy.begin(), copy.end());
    benchmark::DoNotOptimize(copy);
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * length);
}
BENCHMARK(BM_StdSortInt64Small)->RangeMultiplier(10)->Range(100, 1'000'000);

void BM_StdSortInt64MaxMinusSmall(benchmark::State& state) {
  const size_t length = state.range(0);
  constexpr int64_t kRangeMax = std::numeric_limits<int64_t>::max();
  constexpr int64_t kRangeSize = 100'000;
  auto values = GenerateRandomVector<int64_t>(
      length, {kRangeMax - kRangeSize, kRangeMax});
  for (auto _ : state) {
    state.PauseTiming();
    auto copy = values;
    state.ResumeTiming();
    std::sort(copy.begin(), copy.end());
    benchmark::DoNotOptimize(copy);
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * length);
}
BENCHMARK(BM_StdSortInt64MaxMinusSmall)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);

void BM_StdStableSortInt64Max(benchmark::State& state) {
  const size_t length = state.range(0);
  auto values = GenerateRandomVector(length, NonNegativeRange<int64_t>());
  for (auto _ : state) {
    state.PauseTiming();
    auto copy = values;
    state.ResumeTiming();
    std::stable_sort(copy.begin(), copy.end());
    benchmark::DoNotOptimize(copy);
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * length);
}
BENCHMARK(BM_StdStableSortInt64Max)->RangeMultiplier(10)->Range(100, 1'000'000);

void BM_StdStableSortInt64Small(benchmark::State& state) {
  const size_t length = state.range(0);
  auto values = GenerateRandomVector<int64_t>(length, {0, 100'000});
  for (auto _ : state) {
    state.PauseTiming();
    auto copy = values;
    state.ResumeTiming();
    std::stable_sort(copy.begin(), copy.end());
    benchmark::DoNotOptimize(copy);
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * length);
}
BENCHMARK(BM_StdStableSortInt64Small)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);

void BM_StdStableSortInt64MaxMinusSmall(benchmark::State& state) {
  const size_t length = state.range(0);
  constexpr int64_t kRangeMax = std::numeric_limits<int64_t>::max();
  constexpr int64_t kRangeSize = 100'000;
  auto values = GenerateRandomVector<int64_t>(
      length, {kRangeMax - kRangeSize, kRangeMax});
  for (auto _ : state) {
    state.PauseTiming();
    auto copy = values;
    state.ResumeTiming();
    std::stable_sort(copy.begin(), copy.end());
    benchmark::DoNotOptimize(copy);
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * length);
}
BENCHMARK(BM_StdStableSortInt64MaxMinusSmall)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);

void BM_MultikeyRadixSort(benchmark::State& state) {
  const size_t length = state.range(0);
  const int bounds = 10 * length;
  auto values = GenerateRandomInt32PairVector(length, bounds);
  for (auto _ : state) {
    state.PauseTiming();
    auto copy = values;
    state.ResumeTiming();
    MultikeyRadixSort(
        copy, [](const auto& p) { return p.second; },
        [](const auto& p) { return p.first; });
    benchmark::DoNotOptimize(copy);
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * length);
}
BENCHMARK(BM_MultikeyRadixSort)->RangeMultiplier(2)->Range(8, 16 << 20);

void BM_MultikeyRadixSortInt64Max(benchmark::State& state) {
  const size_t length = state.range(0);
  auto values = GenerateRandomVector(length, NonNegativeRange<int64_t>());
  for (auto _ : state) {
    state.PauseTiming();
    auto copy = values;
    state.ResumeTiming();
    MultikeyRadixSort(copy);
    benchmark::DoNotOptimize(copy);
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * length);
}
BENCHMARK(BM_MultikeyRadixSortInt64Max)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);

void BM_MultikeyRadixSortInt64Small(benchmark::State& state) {
  const size_t length = state.range(0);
  auto values = GenerateRandomVector<int64_t>(length, {0, 100'000});
  for (auto _ : state) {
    state.PauseTiming();
    auto copy = values;
    state.ResumeTiming();
    MultikeyRadixSort(copy);
    benchmark::DoNotOptimize(copy);
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * length);
}
BENCHMARK(BM_MultikeyRadixSortInt64Small)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);

void BM_MultikeyRadixSortInt64MaxMinusSmall(benchmark::State& state) {
  const size_t length = state.range(0);
  constexpr int64_t kRangeMax = std::numeric_limits<int64_t>::max();
  constexpr int64_t kRangeSize = 100'000;
  auto values = GenerateRandomVector<int64_t>(
      length, {kRangeMax - kRangeSize, kRangeMax});
  for (auto _ : state) {
    state.PauseTiming();
    auto copy = values;
    state.ResumeTiming();
    MultikeyRadixSort(copy);
    benchmark::DoNotOptimize(copy);
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * length);
}
BENCHMARK(BM_MultikeyRadixSortInt64MaxMinusSmall)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);

void BM_RadixSortInt64Max(benchmark::State& state) {
  const size_t length = state.range(0);
  auto values = GenerateRandomVector(length, NonNegativeRange<int64_t>());
  for (auto _ : state) {
    state.PauseTiming();
    auto copy = values;
    state.ResumeTiming();
    ::operations_research::RadixSort(absl::MakeSpan(copy), 64);
    benchmark::DoNotOptimize(copy);
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * length);
}
BENCHMARK(BM_RadixSortInt64Max)->RangeMultiplier(10)->Range(100, 1'000'000);

void BM_RadixSortInt64Small(benchmark::State& state) {
  const size_t length = state.range(0);
  auto values = GenerateRandomVector<int64_t>(length, {0, 100'000});
  for (auto _ : state) {
    state.PauseTiming();
    auto copy = values;
    state.ResumeTiming();
    ::operations_research::RadixSort(absl::MakeSpan(copy),
                                     17);  // 100'000 fits in 17 bits
    benchmark::DoNotOptimize(copy);
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * length);
}
BENCHMARK(BM_RadixSortInt64Small)->RangeMultiplier(10)->Range(100, 1'000'000);

void BM_RadixSortInt64SmallNoHint(benchmark::State& state) {
  const size_t length = state.range(0);
  auto values = GenerateRandomVector<int64_t>(length, {0, 100'000});
  for (auto _ : state) {
    state.PauseTiming();
    auto copy = values;
    state.ResumeTiming();
    ::operations_research::RadixSort(absl::MakeSpan(copy));
    benchmark::DoNotOptimize(copy);
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * length);
}
BENCHMARK(BM_RadixSortInt64SmallNoHint)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);

void BM_RadixSortInt64MaxMinusSmall(benchmark::State& state) {
  const size_t length = state.range(0);
  constexpr int64_t kRangeMax = std::numeric_limits<int64_t>::max();
  constexpr int64_t kRangeSize = 100'000;
  auto values = GenerateRandomVector<int64_t>(
      length, {kRangeMax - kRangeSize, kRangeMax});
  for (auto _ : state) {
    state.PauseTiming();
    auto copy = values;
    state.ResumeTiming();
    ::operations_research::RadixSort(absl::MakeSpan(copy), 64);
    benchmark::DoNotOptimize(copy);
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * length);
}
BENCHMARK(BM_RadixSortInt64MaxMinusSmall)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);

void BM_RadixSortMinMaxInt64Max(benchmark::State& state) {
  const size_t length = state.range(0);
  auto values = GenerateRandomVector(length, NonNegativeRange<int64_t>());
  for (auto _ : state) {
    state.PauseTiming();
    auto copy = values;
    state.ResumeTiming();
    RadixSortMinMax(int64_t{0}, std::numeric_limits<int64_t>::max(), copy,
                    [](auto v) { return v; });
    benchmark::DoNotOptimize(copy);
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * length);
}
BENCHMARK(BM_RadixSortMinMaxInt64Max)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);

void BM_RadixSortMinMaxInt64Small(benchmark::State& state) {
  const size_t length = state.range(0);
  auto values = GenerateRandomVector<int64_t>(length, {0, 100'000});
  for (auto _ : state) {
    state.PauseTiming();
    auto copy = values;
    state.ResumeTiming();
    RadixSortMinMax(9, int64_t{0}, int64_t{100'000}, copy,
                    [](auto v) { return v; });
    benchmark::DoNotOptimize(copy);
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * length);
}
BENCHMARK(BM_RadixSortMinMaxInt64Small)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);

void BM_RadixSortMinMaxInt64MaxMinusSmall(benchmark::State& state) {
  const size_t length = state.range(0);
  constexpr int64_t kRangeMax = std::numeric_limits<int64_t>::max();
  constexpr int64_t kRangeSize = 100'000;
  auto values = GenerateRandomVector<int64_t>(
      length, {kRangeMax - kRangeSize, kRangeMax});
  for (auto _ : state) {
    state.PauseTiming();
    auto copy = values;
    state.ResumeTiming();
    RadixSortMinMax(kRangeMax - kRangeSize, kRangeMax, copy,
                    [](auto v) { return v; });
    benchmark::DoNotOptimize(copy);
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * length);
}
BENCHMARK(BM_RadixSortMinMaxInt64MaxMinusSmall)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);

}  // namespace
}  // namespace operations_research
