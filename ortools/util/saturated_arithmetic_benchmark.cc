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
#include <random>
#include <vector>

#include "absl/log/log.h"
#include "benchmark/benchmark.h"
#include "ortools/base/types.h"
#include "ortools/util/saturated_arithmetic.h"

namespace operations_research {
namespace {

std::vector<int64_t> GenerateInt64ValuesForTesting() {
  std::vector<int64_t> v;
  // Generate a bunch of special values...
  for (int64_t base :
       {int64_t{0}, kint64min, kint64max, static_cast<int64_t>(kint32max),
        static_cast<int64_t>(kint32min)}) {
    // ...scaled by various factor...
    for (int64_t scaled : {base, base / 2, base / 3 * 2}) {
      // ...with small offsets added.
      for (int64_t offset : {-1000, -2, -1, 0, 1, 2, 1000}) {
        // Avoid generating an overflow here. Do not want saturated as we
        // want corner-case numbers, not saturated arithmetic.
        v.push_back(TwosComplementAddition(scaled, offset));
      }
    }
  }
  return v;
}

std::vector<int64_t> NonOverflowCases() {
  std::vector<int64_t> v;
  for (int64_t i = -50; i < 50; ++i) {
    v.push_back(i);
  }
  return v;
}

// A renaming of GenerateInt64ValuesForTesting() to make the display of
// the micro-benchmarks look nicer.
inline std::vector<int64_t> CornerCases() {
  return GenerateInt64ValuesForTesting();
}

// In the benchmark, we can use either the "special" testing values, or
// some "safe" values to measure peak performance.
#define BENCH_INT64_OPERATOR(Operator, GenerationFunction)             \
  void BM_##Operator##_##GenerationFunction(benchmark::State& state) { \
    std::vector<int64_t> values1 = GenerationFunction();               \
    std::vector<int64_t> values2 = values1;                            \
    std::mt19937 random(12345);                                        \
    std::shuffle(values2.begin(), values2.end(), random);              \
    int64_t ret = 0;                                                   \
    for (auto _ : state) {                                             \
      for (const int64_t a : values1) {                                \
        for (const int64_t b : values2) {                              \
          ret |= Operator(a, b);                                       \
        }                                                              \
      }                                                                \
    }                                                                  \
    state.SetItemsProcessed(state.max_iterations * values1.size() *    \
                            values2.size());                           \
    VLOG(1) << "Result: " << ret;                                      \
  }                                                                    \
  BENCHMARK(BM_##Operator##_##GenerationFunction)

// To be used only for speed benchmarking, on non-overflowing test cases.
inline int64_t SimpleMultiplication(int64_t x, int64_t y) { return x * y; }

BENCH_INT64_OPERATOR(CapAdd, CornerCases);
BENCH_INT64_OPERATOR(CapSub, CornerCases);
BENCH_INT64_OPERATOR(CapProd, CornerCases);
#if defined(__GNUC__) && defined(__x86_64__)
BENCH_INT64_OPERATOR(CapAddAsm, CornerCases);
BENCH_INT64_OPERATOR(CapSubAsm, CornerCases);
BENCH_INT64_OPERATOR(CapProdAsm, CornerCases);
#endif
#if defined(__clang__)
BENCH_INT64_OPERATOR(CapAddBuiltIn, CornerCases);
BENCH_INT64_OPERATOR(CapSubBuiltIn, CornerCases);
BENCH_INT64_OPERATOR(CapProdBuiltIn, CornerCases);
#endif

BENCH_INT64_OPERATOR(CapAddGeneric, CornerCases);
BENCH_INT64_OPERATOR(CapSubGeneric, CornerCases);
BENCH_INT64_OPERATOR(CapProdGeneric, CornerCases);

BENCH_INT64_OPERATOR(CapAdd, NonOverflowCases);
BENCH_INT64_OPERATOR(CapSub, NonOverflowCases);
BENCH_INT64_OPERATOR(CapProd, NonOverflowCases);
#if defined(__GNUC__) && defined(__x86_64__)
BENCH_INT64_OPERATOR(CapAddAsm, NonOverflowCases);
BENCH_INT64_OPERATOR(CapSubAsm, NonOverflowCases);
BENCH_INT64_OPERATOR(CapProdAsm, NonOverflowCases);
#endif
#if defined(__clang__)
BENCH_INT64_OPERATOR(CapAddBuiltIn, NonOverflowCases);
BENCH_INT64_OPERATOR(CapSubBuiltIn, NonOverflowCases);
BENCH_INT64_OPERATOR(CapProdBuiltIn, NonOverflowCases);
#endif

BENCH_INT64_OPERATOR(CapAddGeneric, NonOverflowCases);
BENCH_INT64_OPERATOR(CapSubGeneric, NonOverflowCases);
BENCH_INT64_OPERATOR(CapProdGeneric, NonOverflowCases);
BENCH_INT64_OPERATOR(TwosComplementAddition, NonOverflowCases);
BENCH_INT64_OPERATOR(TwosComplementSubtraction, NonOverflowCases);
BENCH_INT64_OPERATOR(SimpleMultiplication, NonOverflowCases);

// TODO(user): do the same for CapOpp.

}  // namespace
}  // namespace operations_research
