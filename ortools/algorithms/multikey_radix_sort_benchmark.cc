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
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/random/random.h"
#include "absl/types/span.h"
#include "benchmark/benchmark.h"
#include "ortools/algorithms/multikey_radix_sort.h"
#include "ortools/algorithms/radix_sort.h"

namespace benchmark {
extern std::string FLAGS_benchmark_filter;
}  // namespace benchmark

namespace operations_research {
namespace {

template <typename T>
constexpr std::pair<T, T> FullRange() {
  constexpr T max_value = std::is_floating_point_v<T>
                              ? std::numeric_limits<T>::max() / 2
                              : std::numeric_limits<T>::max();
  constexpr T min_value = std::is_floating_point_v<T>
                              ? std::numeric_limits<T>::min() / 2
                              : std::numeric_limits<T>::lowest();
  return {min_value, max_value};
}

template <typename T>
constexpr std::pair<T, T> NonNegativeRange() {
  constexpr T max_value = std::is_floating_point_v<T>
                              ? std::numeric_limits<T>::max() / 2
                              : std::numeric_limits<T>::max();
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

template <auto SortFn, typename GeneratorFn>
void BM_Sort(benchmark::State& state) {
  GeneratorFn generator;
  const size_t length = state.range(0);
  const auto values = generator(length);
  CHECK_EQ(values.size(), length);
  auto sorted_values = values;
  std::sort(sorted_values.begin(), sorted_values.end());
  auto copy = values;
  SortFn(copy, generator);
  for (size_t i = 1; i < length; ++i) {
    CHECK_EQ(copy[i], sorted_values[i]);
    CHECK_LE(copy[i - 1], copy[i]);
  }
  CHECK_EQ(copy[0], sorted_values[0]);
  for (const auto _ : state) {
    state.PauseTiming();
    auto copy = values;
    state.ResumeTiming();
    SortFn(copy, generator);
    benchmark::ClobberMemory();
    benchmark::DoNotOptimize(copy);
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * length);
}

struct Int64NonNeg {
  constexpr std::pair<int64_t, int64_t> bounds() const {
    return NonNegativeRange<int64_t>();
  }
  std::vector<int64_t> operator()(size_t n) const {
    return GenerateRandomVector<int64_t>(n, bounds());
  }
};

struct Int64Small {
  constexpr std::pair<int64_t, int64_t> bounds() const { return {0, 100000}; }
  std::vector<int64_t> operator()(size_t n) const {
    return GenerateRandomVector<int64_t>(n, bounds());
  }
};

struct Int64MaxMinusSmall {
  constexpr std::pair<int64_t, int64_t> bounds() const {
    return {std::numeric_limits<int64_t>::max() - 100000,
            std::numeric_limits<int64_t>::max()};
  }
  std::vector<int64_t> operator()(size_t n) const {
    return GenerateRandomVector<int64_t>(n, bounds());
  }
};

struct DoubleFull {
  constexpr std::pair<double, double> bounds() const {
    return FullRange<double>();
  }
  std::vector<double> operator()(size_t n) const {
    return GenerateRandomVector<double>(n, bounds());
  }
};

struct DoubleSmall {
  constexpr std::pair<double, double> bounds() const { return {0.0, 100000.0}; }
  std::vector<double> operator()(size_t n) const {
    return GenerateRandomVector<double>(n, bounds());
  }
};

struct DoubleMedium {
  constexpr std::pair<double, double> bounds() const {
    return {23500.0, 9500000.0};
  }
  std::vector<double> operator()(size_t n) const {
    return GenerateRandomVector<double>(n, bounds());
  }
};

struct FloatFull {
  constexpr std::pair<float, float> bounds() const {
    return FullRange<float>();
  }
  std::vector<float> operator()(size_t n) const {
    return GenerateRandomVector<float>(n, bounds());
  }
};

struct FloatSmall {
  constexpr std::pair<float, float> bounds() const { return {0.0f, 100000.0f}; }
  std::vector<float> operator()(size_t n) const {
    return GenerateRandomVector<float>(n, bounds());
  }
};

struct FloatMedium {
  constexpr std::pair<float, float> bounds() const {
    return {23500.0f, 9500000.0f};
  }
  std::vector<float> operator()(size_t n) const {
    return GenerateRandomVector<float>(n, bounds());
  }
};

constexpr auto StdSortFn = [](auto& copy, const auto&) {
  std::sort(copy.begin(), copy.end());
};
BENCHMARK(BM_Sort<StdSortFn, Int64NonNeg>)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);
BENCHMARK(BM_Sort<StdSortFn, Int64Small>)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);
BENCHMARK(BM_Sort<StdSortFn, Int64MaxMinusSmall>)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);
BENCHMARK(BM_Sort<StdSortFn, DoubleFull>)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);
BENCHMARK(BM_Sort<StdSortFn, DoubleSmall>)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);
BENCHMARK(BM_Sort<StdSortFn, DoubleMedium>)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);
BENCHMARK(BM_Sort<StdSortFn, FloatFull>)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);
BENCHMARK(BM_Sort<StdSortFn, FloatSmall>)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);
BENCHMARK(BM_Sort<StdSortFn, FloatMedium>)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);

constexpr auto StdStableSortFn = [](auto& copy, const auto&) {
  std::stable_sort(copy.begin(), copy.end());
};
BENCHMARK(BM_Sort<StdStableSortFn, Int64NonNeg>)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);
BENCHMARK(BM_Sort<StdStableSortFn, Int64Small>)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);
BENCHMARK(BM_Sort<StdStableSortFn, Int64MaxMinusSmall>)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);

template <int kBits>
constexpr auto RadixSortFn = [](auto& copy, const auto&) {
  ::operations_research::RadixSort(absl::MakeSpan(copy), kBits);
};
BENCHMARK(BM_Sort<RadixSortFn<64>, Int64NonNeg>)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);
BENCHMARK(BM_Sort<RadixSortFn<17>, Int64Small>)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);
BENCHMARK(BM_Sort<RadixSortFn<64>, Int64MaxMinusSmall>)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);

constexpr auto RadixSortNoHintFn = [](auto& copy, const auto&) {
  ::operations_research::RadixSort(absl::MakeSpan(copy));
};
BENCHMARK(BM_Sort<RadixSortNoHintFn, Int64Small>)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);

constexpr auto AutoRadixSortFn = [](auto& copy, const auto&) {
  AutoRadixSort(copy);
};
BENCHMARK(BM_Sort<AutoRadixSortFn, Int64NonNeg>)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);
BENCHMARK(BM_Sort<AutoRadixSortFn, Int64Small>)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);
BENCHMARK(BM_Sort<AutoRadixSortFn, Int64MaxMinusSmall>)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);
BENCHMARK(BM_Sort<AutoRadixSortFn, DoubleFull>)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);
BENCHMARK(BM_Sort<AutoRadixSortFn, DoubleSmall>)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);
BENCHMARK(BM_Sort<AutoRadixSortFn, DoubleMedium>)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);
BENCHMARK(BM_Sort<AutoRadixSortFn, FloatFull>)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);
BENCHMARK(BM_Sort<AutoRadixSortFn, FloatSmall>)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);
BENCHMARK(BM_Sort<AutoRadixSortFn, FloatMedium>)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);

constexpr auto RangeRadixSortFn = [](auto& copy, const auto& generator) {
  const auto [min_val, max_val] = generator.bounds();
  RangeRadixSort(min_val, max_val, copy);
};

BENCHMARK(BM_Sort<RangeRadixSortFn, Int64NonNeg>)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);
BENCHMARK(BM_Sort<RangeRadixSortFn, Int64Small>)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);
BENCHMARK(BM_Sort<RangeRadixSortFn, Int64MaxMinusSmall>)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);
BENCHMARK(BM_Sort<RangeRadixSortFn, DoubleFull>)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);
BENCHMARK(BM_Sort<RangeRadixSortFn, DoubleSmall>)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);
BENCHMARK(BM_Sort<RangeRadixSortFn, DoubleMedium>)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);
BENCHMARK(BM_Sort<RangeRadixSortFn, FloatFull>)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);
BENCHMARK(BM_Sort<RangeRadixSortFn, FloatSmall>)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);
BENCHMARK(BM_Sort<RangeRadixSortFn, FloatMedium>)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);

constexpr auto AutoHistogramRadixSortFn = [](auto& copy, const auto&) {
  AutoHistogramRadixSort(copy);
};
BENCHMARK(BM_Sort<AutoHistogramRadixSortFn, Int64NonNeg>)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);
BENCHMARK(BM_Sort<AutoHistogramRadixSortFn, Int64Small>)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);
BENCHMARK(BM_Sort<AutoHistogramRadixSortFn, Int64MaxMinusSmall>)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);
BENCHMARK(BM_Sort<AutoHistogramRadixSortFn, DoubleFull>)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);
BENCHMARK(BM_Sort<AutoHistogramRadixSortFn, DoubleSmall>)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);
BENCHMARK(BM_Sort<AutoHistogramRadixSortFn, DoubleMedium>)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);
BENCHMARK(BM_Sort<AutoHistogramRadixSortFn, FloatFull>)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);
BENCHMARK(BM_Sort<AutoHistogramRadixSortFn, FloatSmall>)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);
BENCHMARK(BM_Sort<AutoHistogramRadixSortFn, FloatMedium>)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);

constexpr auto RangeHistogramRadixSortFn = [](auto& copy,
                                              const auto& generator) {
  const auto [min_val, max_val] = generator.bounds();
  RangeHistogramRadixSort(min_val, max_val, copy);
};
BENCHMARK(BM_Sort<RangeHistogramRadixSortFn, Int64NonNeg>)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);
BENCHMARK(BM_Sort<RangeHistogramRadixSortFn, Int64Small>)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);
BENCHMARK(BM_Sort<RangeHistogramRadixSortFn, Int64MaxMinusSmall>)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);
BENCHMARK(BM_Sort<RangeHistogramRadixSortFn, DoubleFull>)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);
BENCHMARK(BM_Sort<RangeHistogramRadixSortFn, DoubleSmall>)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);
BENCHMARK(BM_Sort<RangeHistogramRadixSortFn, DoubleMedium>)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);
BENCHMARK(BM_Sort<RangeHistogramRadixSortFn, FloatFull>)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);
BENCHMARK(BM_Sort<RangeHistogramRadixSortFn, FloatSmall>)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);
BENCHMARK(BM_Sort<RangeHistogramRadixSortFn, FloatMedium>)
    ->RangeMultiplier(10)
    ->Range(100, 1'000'000);

}  // namespace
}  // namespace operations_research

int main(int argc, char** argv) {
  ::benchmark::Initialize(&argc, argv);
  if (::benchmark::FLAGS_benchmark_filter.empty()) {
    ::benchmark::FLAGS_benchmark_filter = "all";
  }
  ::benchmark::RunSpecifiedBenchmarks();
  ::benchmark::Shutdown();
  return 0;
}
