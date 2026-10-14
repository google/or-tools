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
#include <cstdint>
#include <functional>
#include <random>
#include <vector>

#include "absl/random/bit_gen_ref.h"
#include "absl/random/distributions.h"
#include "benchmark/benchmark.h"
#include "ortools/util/sort.h"

namespace operations_research {
namespace {

std::vector<int> RandomVector(int size, int min_value, int max_value,
                              absl::BitGenRef randomizer) {
  std::vector<int> to_sort;
  for (int i = 0; i < size; ++i) {
    to_sort.push_back(
        absl::Uniform(absl::IntervalClosed, randomizer, min_value, max_value));
  }
  return to_sort;
}

// Test the performances of the algorithm.
// ---------------------------------------

std::vector<int> RandomAlmostSortedVector(int size, int min_value,
                                          int max_value, int num_swaps,
                                          absl::BitGenRef randomizer) {
  // Build a random sorted vector.
  std::vector<int> to_sort =
      RandomVector(size, min_value, max_value, randomizer);
  std::sort(to_sort.begin(), to_sort.end());

  // Apply num_swaps random swaps.
  for (int i = 0; i < num_swaps; ++i) {
    const int position1 = absl::Uniform(randomizer, 0, size);
    const int position2 = absl::Uniform(randomizer, 0, size);
    const int tmp = to_sort[position1];
    to_sort[position1] = to_sort[position2];
    to_sort[position2] = tmp;
  }
  return to_sort;
}

static void BM_Incr_Sort_Almost_Sorted(benchmark::State& state) {
  const int size = state.range(0);
  const int num_swaps = state.range(1);
  std::mt19937 randomizer(12345);
  for (auto _ : state) {
    state.PauseTiming();
    std::vector<int> to_sort =
        RandomAlmostSortedVector(size, -1024, 1024, num_swaps, randomizer);
    state.ResumeTiming();
    IncrementalSort(to_sort.begin(), to_sort.end());
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) * size);
}

static void BM_Std_Sort_Almost_Sorted(benchmark::State& state) {
  const int size = state.range(0);
  const int num_swaps = state.range(1);
  std::mt19937 randomizer(12345);
  for (auto _ : state) {
    state.PauseTiming();
    std::vector<int> to_sort =
        RandomAlmostSortedVector(size, -1024, 1024, num_swaps, randomizer);
    state.ResumeTiming();
    if (!std::is_sorted(to_sort.begin(), to_sort.end())) {
      std::sort(to_sort.begin(), to_sort.end());
    }
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) * size);
}

static void BM_Std_Stable_Sort_Almost_Sorted(benchmark::State& state) {
  const int size = state.range(0);
  const int num_swaps = state.range(1);
  std::mt19937 randomizer(12345);
  for (auto _ : state) {
    state.PauseTiming();
    std::vector<int> to_sort =
        RandomAlmostSortedVector(size, -1024, 1024, num_swaps, randomizer);
    state.ResumeTiming();
    if (!std::is_sorted(to_sort.begin(), to_sort.end())) {
      std::stable_sort(to_sort.begin(), to_sort.end());
    }
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) * size);
}

namespace {
int LogN(int n) {
  int log = n;
  log |= log >> 1;
  log |= log >> 2;
  log |= log >> 4;
  log |= log >> 8;
  log |= log >> 16;
  return log + 1;
}
}  // namespace

static void BM_Incr_Sort_LogN(benchmark::State& state) {
  const int size = state.range(0);
  std::mt19937 randomizer(12345);
  for (auto _ : state) {
    state.PauseTiming();
    std::vector<int> to_sort =
        RandomAlmostSortedVector(size, -1024, 1024, LogN(size), randomizer);
    state.ResumeTiming();
    IncrementalSort(to_sort.begin(), to_sort.end());
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) * size);
}

static void BM_Std_Sort_LogN(benchmark::State& state) {
  const int size = state.range(0);
  std::mt19937 randomizer(12345);
  for (auto _ : state) {
    state.PauseTiming();
    std::vector<int> to_sort =
        RandomAlmostSortedVector(size, -1024, 1024, LogN(size), randomizer);
    state.ResumeTiming();
    if (!std::is_sorted(to_sort.begin(), to_sort.end())) {
      std::sort(to_sort.begin(), to_sort.end());
    }
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) * size);
}

static void BM_Std_Stable_Sort_LogN(benchmark::State& state) {
  const int size = state.range(0);
  std::mt19937 randomizer(12345);
  for (auto _ : state) {
    state.PauseTiming();
    std::vector<int> to_sort =
        RandomAlmostSortedVector(size, -1024, 1024, LogN(size), randomizer);
    state.ResumeTiming();
    if (!std::is_sorted(to_sort.begin(), to_sort.end())) {
      std::stable_sort(to_sort.begin(), to_sort.end());
    }
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) * size);
}

static void BM_Incr_Sort_Sorted(benchmark::State& state) {
  const int size = state.range(0);
  std::mt19937 randomizer(12345);
  for (auto _ : state) {
    state.PauseTiming();
    std::vector<int> to_sort =
        RandomAlmostSortedVector(size, -1024, 1024, 0, randomizer);
    state.ResumeTiming();
    IncrementalSort(to_sort.begin(), to_sort.end());
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) * size);
}

static void BM_Std_Sort_Sorted(benchmark::State& state) {
  const int size = state.range(0);
  std::mt19937 randomizer(12345);
  for (auto _ : state) {
    state.PauseTiming();
    std::vector<int> to_sort =
        RandomAlmostSortedVector(size, -1024, 1024, 0, randomizer);
    state.ResumeTiming();
    if (!std::is_sorted(to_sort.begin(), to_sort.end())) {
      std::sort(to_sort.begin(), to_sort.end());
    }
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) * size);
}

static void BM_Std_Stable_Sort_Sorted(benchmark::State& state) {
  const int size = state.range(0);
  std::mt19937 randomizer(12345);
  for (auto _ : state) {
    state.PauseTiming();
    std::vector<int> to_sort =
        RandomAlmostSortedVector(size, -1024, 1024, 0, randomizer);
    state.ResumeTiming();
    if (!std::is_sorted(to_sort.begin(), to_sort.end())) {
      std::stable_sort(to_sort.begin(), to_sort.end());
    }
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) * size);
}

static void BM_Incr_Sort_Random(benchmark::State& state) {
  const int size = state.range(0);
  std::mt19937 randomizer(12345);
  for (auto _ : state) {
    state.PauseTiming();
    std::vector<int> to_sort = RandomVector(size, -1024, 1024, randomizer);
    state.ResumeTiming();
    IncrementalSort(to_sort.begin(), to_sort.end());
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) * size);
}

static void BM_Std_Sort_Random(benchmark::State& state) {
  const int size = state.range(0);
  std::mt19937 randomizer(12345);
  for (auto _ : state) {
    state.PauseTiming();
    std::vector<int> to_sort = RandomVector(size, -1024, 1024, randomizer);
    state.ResumeTiming();
    if (!std::is_sorted(to_sort.begin(), to_sort.end())) {
      std::sort(to_sort.begin(), to_sort.end());
    }
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) * size);
}

static void BM_Std_Stable_Sort_Random(benchmark::State& state) {
  const int size = state.range(0);
  std::mt19937 randomizer(12345);
  for (auto _ : state) {
    state.PauseTiming();
    std::vector<int> to_sort = RandomVector(size, -1024, 1024, randomizer);
    state.ResumeTiming();
    if (!std::is_sorted(to_sort.begin(), to_sort.end())) {
      std::stable_sort(to_sort.begin(), to_sort.end());
    }
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) * size);
}

static void BM_Incr_Sort_Desc(benchmark::State& state) {
  const int size = state.range(0);
  std::mt19937 randomizer(12345);
  for (auto _ : state) {
    state.PauseTiming();
    std::vector<int> to_sort = RandomVector(size, -1024, 1024, randomizer);
    std::sort(to_sort.begin(), to_sort.end(), std::greater<int>());
    state.ResumeTiming();
    IncrementalSort(to_sort.begin(), to_sort.end());
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) * size);
}

static void BM_Std_Sort_Desc(benchmark::State& state) {
  const int size = state.range(0);
  std::mt19937 randomizer(12345);
  for (auto _ : state) {
    state.PauseTiming();
    std::vector<int> to_sort = RandomVector(size, -1024, 1024, randomizer);
    std::sort(to_sort.begin(), to_sort.end(), std::greater<int>());
    state.ResumeTiming();
    if (!std::is_sorted(to_sort.begin(), to_sort.end())) {
      std::sort(to_sort.begin(), to_sort.end());
    }
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) * size);
}

static void BM_Std_Stable_Sort_Desc(benchmark::State& state) {
  const int size = state.range(0);
  std::mt19937 randomizer(12345);
  for (auto _ : state) {
    state.PauseTiming();
    std::vector<int> to_sort = RandomVector(size, -1024, 1024, randomizer);
    std::sort(to_sort.begin(), to_sort.end(), std::greater<int>());
    state.ResumeTiming();
    if (!std::is_sorted(to_sort.begin(), to_sort.end())) {
      std::stable_sort(to_sort.begin(), to_sort.end());
    }
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) * size);
}

// Already sorted vectors.
// -----------------------

BENCHMARK(BM_Incr_Sort_Sorted)
    ->Arg(1)
    ->Arg(8)
    ->Arg(16)
    ->Arg(32)
    ->Arg(64)
    ->Arg(128)
    ->Arg(512)
    ->Arg(4048)
    ->Arg(32768)
    ->Arg(262144)
    ->Arg(1048576);

BENCHMARK(BM_Std_Sort_Sorted)
    ->Arg(1)
    ->Arg(8)
    ->Arg(16)
    ->Arg(32)
    ->Arg(64)
    ->Arg(128)
    ->Arg(512)
    ->Arg(4048)
    ->Arg(32768)
    ->Arg(262144)
    ->Arg(1048576);

BENCHMARK(BM_Std_Stable_Sort_Sorted)
    ->Arg(1)
    ->Arg(8)
    ->Arg(16)
    ->Arg(32)
    ->Arg(64)
    ->Arg(128)
    ->Arg(512)
    ->Arg(4048)
    ->Arg(32768)
    ->Arg(262144)
    ->Arg(1048576);

// Almost sorted vectors.
// ----------------------

BENCHMARK(BM_Incr_Sort_Almost_Sorted)
    ->ArgPair(8, 1)
    ->ArgPair(8, 2)
    ->ArgPair(8, 4)
    ->ArgPair(16, 1)
    ->ArgPair(16, 2)
    ->ArgPair(16, 4)
    ->ArgPair(16, 8)
    ->ArgPair(32, 1)
    ->ArgPair(32, 2)
    ->ArgPair(32, 4)
    ->ArgPair(32, 8)
    ->ArgPair(64, 1)
    ->ArgPair(64, 2)
    ->ArgPair(64, 4)
    ->ArgPair(64, 8)
    ->ArgPair(128, 1)
    ->ArgPair(128, 2)
    ->ArgPair(128, 4)
    ->ArgPair(128, 8)
    ->ArgPair(512, 1)
    ->ArgPair(512, 2)
    ->ArgPair(512, 4)
    ->ArgPair(512, 8)
    ->ArgPair(4048, 1)
    ->ArgPair(4048, 2)
    ->ArgPair(4048, 4)
    ->ArgPair(4048, 8)
    ->ArgPair(32768, 1)
    ->ArgPair(32768, 2)
    ->ArgPair(32768, 4)
    ->ArgPair(32768, 8)
    ->ArgPair(262144, 1)
    ->ArgPair(262144, 2)
    ->ArgPair(262144, 4)
    ->ArgPair(262144, 8)
    ->ArgPair(1048576, 1)
    ->ArgPair(1048576, 2)
    ->ArgPair(1048576, 4)
    ->ArgPair(1048576, 8);

BENCHMARK(BM_Std_Sort_Almost_Sorted)
    ->ArgPair(8, 1)
    ->ArgPair(8, 2)
    ->ArgPair(8, 4)
    ->ArgPair(16, 1)
    ->ArgPair(16, 2)
    ->ArgPair(16, 4)
    ->ArgPair(16, 8)
    ->ArgPair(32, 1)
    ->ArgPair(32, 2)
    ->ArgPair(32, 4)
    ->ArgPair(32, 8)
    ->ArgPair(64, 1)
    ->ArgPair(64, 2)
    ->ArgPair(64, 4)
    ->ArgPair(64, 8)
    ->ArgPair(128, 1)
    ->ArgPair(128, 2)
    ->ArgPair(128, 4)
    ->ArgPair(128, 8)
    ->ArgPair(512, 1)
    ->ArgPair(512, 2)
    ->ArgPair(512, 4)
    ->ArgPair(512, 8)
    ->ArgPair(4048, 1)
    ->ArgPair(4048, 2)
    ->ArgPair(4048, 4)
    ->ArgPair(4048, 8)
    ->ArgPair(32768, 1)
    ->ArgPair(32768, 2)
    ->ArgPair(32768, 4)
    ->ArgPair(32768, 8)
    ->ArgPair(262144, 1)
    ->ArgPair(262144, 2)
    ->ArgPair(262144, 4)
    ->ArgPair(262144, 8)
    ->ArgPair(1048576, 1)
    ->ArgPair(1048576, 2)
    ->ArgPair(1048576, 4)
    ->ArgPair(1048576, 8);

BENCHMARK(BM_Std_Stable_Sort_Almost_Sorted)
    ->ArgPair(8, 1)
    ->ArgPair(8, 2)
    ->ArgPair(8, 4)
    ->ArgPair(16, 1)
    ->ArgPair(16, 2)
    ->ArgPair(16, 4)
    ->ArgPair(16, 8)
    ->ArgPair(32, 1)
    ->ArgPair(32, 2)
    ->ArgPair(32, 4)
    ->ArgPair(32, 8)
    ->ArgPair(64, 1)
    ->ArgPair(64, 2)
    ->ArgPair(64, 4)
    ->ArgPair(64, 8)
    ->ArgPair(128, 1)
    ->ArgPair(128, 2)
    ->ArgPair(128, 4)
    ->ArgPair(128, 8)
    ->ArgPair(512, 1)
    ->ArgPair(512, 2)
    ->ArgPair(512, 4)
    ->ArgPair(512, 8)
    ->ArgPair(4048, 1)
    ->ArgPair(4048, 2)
    ->ArgPair(4048, 4)
    ->ArgPair(4048, 8)
    ->ArgPair(32768, 1)
    ->ArgPair(32768, 2)
    ->ArgPair(32768, 4)
    ->ArgPair(32768, 8)
    ->ArgPair(262144, 1)
    ->ArgPair(262144, 2)
    ->ArgPair(262144, 4)
    ->ArgPair(262144, 8)
    ->ArgPair(1048576, 1)
    ->ArgPair(1048576, 2)
    ->ArgPair(1048576, 4)
    ->ArgPair(1048576, 8);

// Log N misplaced elements.
// -------------------------

BENCHMARK(BM_Incr_Sort_LogN)
    ->Arg(8)
    ->Arg(16)
    ->Arg(32)
    ->Arg(128)
    ->Arg(512)
    ->Arg(4048)
    ->Arg(32768)
    ->Arg(262144)
    ->Arg(1048576);

BENCHMARK(BM_Std_Sort_LogN)
    ->Arg(8)
    ->Arg(16)
    ->Arg(32)
    ->Arg(128)
    ->Arg(512)
    ->Arg(4048)
    ->Arg(32768)
    ->Arg(262144)
    ->Arg(1048576);

BENCHMARK(BM_Std_Stable_Sort_LogN)
    ->Arg(8)
    ->Arg(16)
    ->Arg(32)
    ->Arg(128)
    ->Arg(512)
    ->Arg(4048)
    ->Arg(32768)
    ->Arg(262144)
    ->Arg(1048576);

// Random vectors.
// ---------------

BENCHMARK(BM_Incr_Sort_Random)
    ->Arg(1)
    ->Arg(8)
    ->Arg(16)
    ->Arg(32)
    ->Arg(64)
    ->Arg(128)
    ->Arg(512)
    ->Arg(4048)
    ->Arg(32768)
    ->Arg(262144)
    ->Arg(1048576);

BENCHMARK(BM_Std_Sort_Random)
    ->Arg(1)
    ->Arg(8)
    ->Arg(16)
    ->Arg(32)
    ->Arg(64)
    ->Arg(128)
    ->Arg(512)
    ->Arg(4048)
    ->Arg(32768)
    ->Arg(262144)
    ->Arg(1048576);

BENCHMARK(BM_Std_Stable_Sort_Random)
    ->Arg(1)
    ->Arg(8)
    ->Arg(16)
    ->Arg(32)
    ->Arg(64)
    ->Arg(128)
    ->Arg(512)
    ->Arg(4048)
    ->Arg(32768)
    ->Arg(262144)
    ->Arg(1048576);

// Inversely sorted vectors.
// -------------------------

BENCHMARK(BM_Incr_Sort_Desc)
    ->Arg(1)
    ->Arg(8)
    ->Arg(16)
    ->Arg(32)
    ->Arg(64)
    ->Arg(128)
    ->Arg(512)
    ->Arg(4048)
    ->Arg(32768)
    ->Arg(262144)
    ->Arg(1048576);

BENCHMARK(BM_Std_Sort_Desc)
    ->Arg(1)
    ->Arg(8)
    ->Arg(16)
    ->Arg(32)
    ->Arg(64)
    ->Arg(128)
    ->Arg(512)
    ->Arg(4048)
    ->Arg(32768)
    ->Arg(262144)
    ->Arg(1048576);

BENCHMARK(BM_Std_Stable_Sort_Desc)
    ->Arg(1)
    ->Arg(8)
    ->Arg(16)
    ->Arg(32)
    ->Arg(64)
    ->Arg(128)
    ->Arg(512)
    ->Arg(4048)
    ->Arg(32768)
    ->Arg(262144)
    ->Arg(1048576);

}  // namespace
}  // namespace operations_research
