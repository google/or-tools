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
#include <array>
#include <bit>
#include <cstdint>
#include <random>
#include <vector>

#include "absl/log/check.h"
#include "absl/random/random.h"
#include "benchmark/benchmark.h"
#include "ortools/util/bitset.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace {

////////////////////////////////////////////////////////////////////////////////
// Least / Most Significant Bit Position
////////////////////////////////////////////////////////////////////////////////

namespace bitset_index {

// These implementations are currently not exposed in `bitset.h` but we want to
// test their performances.
int LSB32Std(uint32_t n) { return std::countr_zero(n); }
int LSB64Std(uint64_t n) { return std::countr_zero(n); }
int MSB32Std(uint32_t n) { return 31 - std::countl_zero(n); }
int MSB64Std(uint64_t n) { return 63 - std::countl_zero(n); }

// An enum to select between LSBIndex and MSBIndex.
enum class Function { kLSB, kMSB };

// An enum to select the implementation.
enum class Impl { kBasic, kBuiltin, kDeBruijn, kStd };

// A type to describe a complete implementation.
template <Function function, typename T, Impl implementation, auto FunctionPtr>
struct TestCase {
  using UIntT = T;
  static constexpr Function kFunction = function;
  static constexpr Impl kImpl = implementation;
  static int Compute(UIntT value) { return FunctionPtr(value); }
};

// Shorthand for all available types.
using LSB_32_Basic = TestCase<Function::kLSB, uint32_t, Impl::kBasic,
                              LeastSignificantBitPosition32Default>;
#ifdef ORTOOLS_BITSET_USE_FAST_LEAST_SIGNIFICANT_BIT
using LSB_32_Builtin = TestCase<Function::kLSB, uint32_t, Impl::kBuiltin,
                                LeastSignificantBitPosition32Fast>;
#endif
using LSB_32_DeBruijn = TestCase<Function::kLSB, uint32_t, Impl::kDeBruijn,
                                 LeastSignificantBitPosition32DeBruijn>;
using LSB_32_Std = TestCase<Function::kLSB, uint32_t, Impl::kStd, LSB32Std>;
using LSB_64_Basic = TestCase<Function::kLSB, uint64_t, Impl::kBasic,
                              LeastSignificantBitPosition64Default>;
#ifdef ORTOOLS_BITSET_USE_FAST_LEAST_SIGNIFICANT_BIT
using LSB_64_Builtin = TestCase<Function::kLSB, uint64_t, Impl::kBuiltin,
                                LeastSignificantBitPosition64Fast>;
#endif
using LSB_64_DeBruijn = TestCase<Function::kLSB, uint64_t, Impl::kDeBruijn,
                                 LeastSignificantBitPosition64DeBruijn>;
using LSB_64_Std = TestCase<Function::kLSB, uint64_t, Impl::kStd, LSB64Std>;
using MSB_32_Basic = TestCase<Function::kMSB, uint32_t, Impl::kBasic,
                              MostSignificantBitPosition32Default>;
#ifdef ORTOOLS_BITSET_USE_FAST_LEAST_SIGNIFICANT_BIT
using MSB_32_Builtin = TestCase<Function::kMSB, uint32_t, Impl::kBuiltin,
                                MostSignificantBitPosition32Fast>;
#endif
using MSB_32_Std = TestCase<Function::kMSB, uint32_t, Impl::kStd, MSB32Std>;
using MSB_64_Basic = TestCase<Function::kMSB, uint64_t, Impl::kBasic,
                              MostSignificantBitPosition64Default>;
#ifdef ORTOOLS_BITSET_USE_FAST_LEAST_SIGNIFICANT_BIT
using MSB_64_Builtin = TestCase<Function::kMSB, uint64_t, Impl::kBuiltin,
                                MostSignificantBitPosition64Fast>;
#endif
using MSB_64_Std = TestCase<Function::kMSB, uint64_t, Impl::kStd, MSB64Std>;

enum class LoopVectorize { kYes, kNo };

template <LoopVectorize loop_vectorize, typename TestCase>
void BM_BitIndex(benchmark::State& state) {
  using UIntT = typename TestCase::UIntT;
  std::array<UIntT, 2048> data;
  // Seeding an absl::BitGen for process stability.
  const std::seed_seq seed_seq{1, 2, 3};
  absl::BitGen gen(seed_seq);
  std::generate(data.begin(), data.end(),
                [&]() { return absl::Uniform<UIntT>(gen); });
  for (auto _ : state) {
    if constexpr (loop_vectorize == LoopVectorize::kYes) {
      for (UIntT value : data) {
        int result = TestCase::Compute(value);
        benchmark::DoNotOptimize(result);
      }
    } else {
      int dependency = 0;  // prevent loop vectorization.
      for (UIntT value : data) {
        dependency = TestCase::Compute(value + dependency);
      }
      benchmark::DoNotOptimize(dependency);
    }
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                          data.size());
}

BENCHMARK(BM_BitIndex<LoopVectorize::kNo, LSB_32_Basic>);
#ifdef ORTOOLS_BITSET_USE_FAST_LEAST_SIGNIFICANT_BIT
BENCHMARK(BM_BitIndex<LoopVectorize::kNo, LSB_32_Builtin>);
#endif
BENCHMARK(BM_BitIndex<LoopVectorize::kNo, LSB_32_DeBruijn>);
BENCHMARK(BM_BitIndex<LoopVectorize::kNo, LSB_32_Std>);
BENCHMARK(BM_BitIndex<LoopVectorize::kNo, LSB_64_Basic>);
#ifdef ORTOOLS_BITSET_USE_FAST_LEAST_SIGNIFICANT_BIT
BENCHMARK(BM_BitIndex<LoopVectorize::kNo, LSB_64_Builtin>);
#endif
BENCHMARK(BM_BitIndex<LoopVectorize::kNo, LSB_64_DeBruijn>);
BENCHMARK(BM_BitIndex<LoopVectorize::kNo, LSB_64_Std>);
BENCHMARK(BM_BitIndex<LoopVectorize::kNo, MSB_32_Basic>);
#ifdef ORTOOLS_BITSET_USE_FAST_LEAST_SIGNIFICANT_BIT
BENCHMARK(BM_BitIndex<LoopVectorize::kNo, MSB_32_Builtin>);
#endif
BENCHMARK(BM_BitIndex<LoopVectorize::kNo, MSB_32_Std>);
BENCHMARK(BM_BitIndex<LoopVectorize::kNo, MSB_64_Basic>);
#ifdef ORTOOLS_BITSET_USE_FAST_LEAST_SIGNIFICANT_BIT
BENCHMARK(BM_BitIndex<LoopVectorize::kNo, MSB_64_Builtin>);
#endif
BENCHMARK(BM_BitIndex<LoopVectorize::kNo, MSB_64_Std>);
BENCHMARK(BM_BitIndex<LoopVectorize::kYes, LSB_32_Basic>);
#ifdef ORTOOLS_BITSET_USE_FAST_LEAST_SIGNIFICANT_BIT
BENCHMARK(BM_BitIndex<LoopVectorize::kYes, LSB_32_Builtin>);
#endif
BENCHMARK(BM_BitIndex<LoopVectorize::kYes, LSB_32_DeBruijn>);
BENCHMARK(BM_BitIndex<LoopVectorize::kYes, LSB_32_Std>);
BENCHMARK(BM_BitIndex<LoopVectorize::kYes, LSB_64_Basic>);
#ifdef ORTOOLS_BITSET_USE_FAST_LEAST_SIGNIFICANT_BIT
BENCHMARK(BM_BitIndex<LoopVectorize::kYes, LSB_64_Builtin>);
#endif
BENCHMARK(BM_BitIndex<LoopVectorize::kYes, LSB_64_DeBruijn>);
BENCHMARK(BM_BitIndex<LoopVectorize::kYes, LSB_64_Std>);
BENCHMARK(BM_BitIndex<LoopVectorize::kYes, MSB_32_Basic>);
#ifdef ORTOOLS_BITSET_USE_FAST_LEAST_SIGNIFICANT_BIT
BENCHMARK(BM_BitIndex<LoopVectorize::kYes, MSB_32_Builtin>);
#endif
BENCHMARK(BM_BitIndex<LoopVectorize::kYes, MSB_32_Std>);
BENCHMARK(BM_BitIndex<LoopVectorize::kYes, MSB_64_Basic>);
#ifdef ORTOOLS_BITSET_USE_FAST_LEAST_SIGNIFICANT_BIT
BENCHMARK(BM_BitIndex<LoopVectorize::kYes, MSB_64_Builtin>);
#endif
BENCHMARK(BM_BitIndex<LoopVectorize::kYes, MSB_64_Std>);

}  // namespace bitset_index

DEFINE_STRONG_INT64_TYPE(TestIndex);

void BM_BitQueue64DenseSet(benchmark::State& state) {
  int size = state.range(0);
  BitQueue64 queue(size);
  for (auto _ : state) {
    for (int s = 0; s < size; ++s) queue.Set(s);
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.max_iterations) * size /
                          8);
}

void BM_BitQueue64DenseSetAndDeque(benchmark::State& state) {
  int size = state.range(0);
  BitQueue64 queue(size);
  for (auto _ : state) {
    for (int s = 0; s < size; ++s) queue.Set(s);
    while (queue.Top() != -1) queue.ClearTop();
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.max_iterations) * size /
                          8);
}

void BM_BitQueue64SparseSetAndDeque(benchmark::State& state) {
  int size = state.range(0);
  BitQueue64 queue(size * 10);

  const int kRandomSeed = 1;
  std::mt19937 randomizer(kRandomSeed);
  std::vector<int> positions;
  for (int s = 0; s < size; ++s) {
    positions.push_back(absl::Uniform<int32_t>(randomizer, 0, size * 10));
  }

  for (auto _ : state) {
    for (const int s : positions) queue.Set(s);
    while (queue.Top() != -1) queue.ClearTop();
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.max_iterations) * size /
                          8);
}

void BM_VectorBoolAssign(benchmark::State& state) {
  int size = state.range(0);
  std::vector<bool> v(size);
  for (auto _ : state) {
    v.assign(size, false);
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.max_iterations) *
                          v.size() / 8);
}

void BM_Bitset64Assign(benchmark::State& state) {
  int size = state.range(0);
  TestIndex kSize(size);
  Bitset64<TestIndex> bitset(kSize);
  for (auto _ : state) {
    bitset.ClearAndResize(kSize);
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.max_iterations) * size /
                          8);
}

// Test the speed of modifying a vector<bool>.
void BM_VectorBoolChange(benchmark::State& state) {
  const int kBenchmarkSetSize = 1000 * 1000;
  std::vector<bool> v(kBenchmarkSetSize, false);
  for (auto _ : state) {
    for (int index = 0; index < kBenchmarkSetSize; ++index) {
      v[index] = true;
    }
    for (int index = 0; index < kBenchmarkSetSize; ++index) {
      v[index] = false;
    }
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.max_iterations) * 2 *
                          kBenchmarkSetSize / 8);
}

// Test the speed of modifying a Bitset64.
void BM_Bitset64Change(benchmark::State& state) {
  const int kBenchmarkSetSize = 1000 * 1000;
  TestIndex kSize(kBenchmarkSetSize);
  Bitset64<TestIndex> bitset(kSize);
  for (auto _ : state) {
    for (TestIndex index(0); index < kBenchmarkSetSize; ++index) {
      bitset.Set(index);
    }
    for (TestIndex index(0); index < kBenchmarkSetSize; ++index) {
      bitset.Clear(index);
    }
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.max_iterations) * 2 *
                          kBenchmarkSetSize / 8);
}

// Test the speed of modifying a SparseBitset.
void BM_SparseBitsetChange(benchmark::State& state) {
  const int kBenchmarkSetSize = 1000 * 1000;
  SparseBitset<> bitset(kBenchmarkSetSize);
  for (auto _ : state) {
    for (int index = 0; index < kBenchmarkSetSize; ++index) {
      bitset.Set(index);
    }
    for (int index = 0; index < kBenchmarkSetSize; ++index) {
      bitset.Clear(index);
    }
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.max_iterations) * 2 *
                          kBenchmarkSetSize / 8);
}

static std::vector<bool> GetBenchmarkIterationData(float density_in_percent) {
  const int kBenchmarkSetSize = 1000 * 1000;
  const int kBenchmarkRandomSeed = 1;
  std::vector<bool> data(kBenchmarkSetSize, false);
  std::mt19937 randomizer(kBenchmarkRandomSeed);
  const float kDensity = static_cast<float>(density_in_percent) / 100.0;
  for (int index = 0; index < kBenchmarkSetSize; ++index) {
    if (absl::Uniform<float>(absl::IntervalOpen, randomizer, 0, 1) < kDensity) {
      data[index] = true;
    }
  }
  return data;
}

// Test the speed of iterating over a vector<bool> with a given density of
// true.
void BM_VectorBoolIteration(benchmark::State& state) {
  int density_in_percent = state.range(0);
  std::vector<bool> v = GetBenchmarkIterationData(density_in_percent);

  int64_t sum_indices(0.0);
  for (auto _ : state) {
    for (int index = 0; index < v.size(); ++index) {
      if (v[index]) {
        sum_indices += index;
      }
    }
  }
  CHECK_GE(sum_indices, 0.0);
  state.SetBytesProcessed(static_cast<int64_t>(state.max_iterations) *
                          v.size() / 8);
}

// Test the speed of iterating over a Bitset64 with a given density of true.
void BM_Bitset64Iteration(benchmark::State& state) {
  int density_in_percent = state.range(0);
  std::vector<bool> v = GetBenchmarkIterationData(density_in_percent);
  TestIndex kBenchmarkSetSize(v.size());
  Bitset64<TestIndex> bitset(kBenchmarkSetSize);
  for (TestIndex index(0); index < kBenchmarkSetSize; ++index) {
    if (v[index.value()]) {
      bitset.Set(index);
    }
  }

  int64_t sum_indices(0.0);
  for (auto _ : state) {
    for (const TestIndex index : bitset) {
      sum_indices += index.value();
    }
  }
  CHECK_GE(sum_indices, 0.0);
  state.SetBytesProcessed(static_cast<int64_t>(state.max_iterations) *
                          v.size() / 8);
}

// Test the speed of clearing a SparseBitset with the sparse algorithm.
// This is useful to determine a good kSparseThreshold in ClearAndResize().
void BM_SparseBitSetClear(benchmark::State& state) {
  int sparsity = state.range(0);
  std::vector<bool> v = GetBenchmarkIterationData(100.0 / sparsity);
  SparseBitset<> bitset(v.size());
  for (int index = 0; index < v.size(); ++index) {
    if (v[index]) bitset.Set(index);
  }

  for (auto _ : state) {
    // Note that we just emulate the sparse clear.
    // TODO(user): because of the cache behavior this is somewhat not a really
    // good benchmark. But then this is the same for the Assign() benchmarks.
    for (int i : bitset.PositionsSetAtLeastOnce()) bitset.Clear(i);
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.max_iterations) *
                          v.size() / 8);
}

/* Run with:
blaze run -c opt ortools/util:bitset_benchmark --
--benchmarks=all
*/
BENCHMARK(BM_BitQueue64DenseSet)->Arg(100)->Arg(1000)->Arg(10000);
BENCHMARK(BM_BitQueue64DenseSetAndDeque)->Arg(100)->Arg(1000)->Arg(10000);
BENCHMARK(BM_BitQueue64SparseSetAndDeque)->Arg(100)->Arg(1000)->Arg(10000);
BENCHMARK(BM_SparseBitSetClear)->Arg(100)->Arg(200)->Arg(300)->Arg(400);
BENCHMARK(BM_VectorBoolAssign)->Arg(10)->Arg(100)->Arg(1000)->Arg(10000);
BENCHMARK(BM_Bitset64Assign)->Arg(10)->Arg(100)->Arg(1000)->Arg(10000);
BENCHMARK(BM_VectorBoolChange);
BENCHMARK(BM_Bitset64Change);
BENCHMARK(BM_SparseBitsetChange);
BENCHMARK(BM_VectorBoolIteration)->Arg(0)->Arg(25)->Arg(50)->Arg(75)->Arg(100);
BENCHMARK(BM_Bitset64Iteration)
    ->Arg(0)
    ->Arg(1)
    ->Arg(25)
    ->Arg(50)
    ->Arg(75)
    ->Arg(100);

#undef BM_SB

}  // namespace
}  // namespace operations_research
