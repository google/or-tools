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

#include "benchmark/benchmark.h"
#include "ortools/sat/integer_base.h"

namespace operations_research {
namespace sat {
namespace {

static void BM_FloorRatio(benchmark::State& state) {
  IntegerValue divisor(654676436498);
  IntegerValue dividend(45454655155444);
  IntegerValue test(0);
  for (auto _ : state) {
    dividend++;
    divisor++;
    benchmark::DoNotOptimize(test += FloorRatio(dividend, divisor));
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()));
}

static void BM_PositiveRemainder(benchmark::State& state) {
  IntegerValue divisor(654676436498);
  IntegerValue dividend(45454655155444);
  IntegerValue test(0);
  for (auto _ : state) {
    dividend++;
    divisor++;
    benchmark::DoNotOptimize(test += PositiveRemainder(dividend, divisor));
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()));
}

static void BM_PositiveRemainderAlternative(benchmark::State& state) {
  IntegerValue divisor(654676436498);
  IntegerValue dividend(45454655155444);
  IntegerValue test(0);
  for (auto _ : state) {
    dividend++;
    divisor++;
    benchmark::DoNotOptimize(test += dividend -
                                     divisor * FloorRatio(dividend, divisor));
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()));
}

// What we use in the code. This is safe from integer overflow. The compiler
// should also do a single integer division to get the quotient and remainder.
static void BM_DivisionAndRemainder(benchmark::State& state) {
  IntegerValue divisor(654676436498);
  IntegerValue dividend(45454655155444);
  IntegerValue test(0);
  for (auto _ : state) {
    dividend++;
    divisor++;
    benchmark::DoNotOptimize(test += FloorRatio(dividend, divisor));
    benchmark::DoNotOptimize(test += PositiveRemainder(dividend, divisor));
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()));
}

// An alternative version, note however that divisor * f might overflow!
static void BM_DivisionAndRemainderAlternative(benchmark::State& state) {
  IntegerValue divisor(654676436498);
  IntegerValue dividend(45454655155444);
  IntegerValue test(0);
  for (auto _ : state) {
    dividend++;
    divisor++;
    const IntegerValue f = FloorRatio(dividend, divisor);
    benchmark::DoNotOptimize(test += f);
    benchmark::DoNotOptimize(test += dividend - divisor * f);
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()));
}

// The best we can hope for ?
static void BM_DivisionAndRemainderBaseline(benchmark::State& state) {
  IntegerValue divisor(654676436498);
  IntegerValue dividend(45454655155444);
  IntegerValue test(0);
  for (auto _ : state) {
    dividend++;
    divisor++;
    benchmark::DoNotOptimize(test += dividend / divisor);
    benchmark::DoNotOptimize(test += dividend % divisor);
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()));
}

BENCHMARK(BM_FloorRatio);
BENCHMARK(BM_PositiveRemainder);
BENCHMARK(BM_PositiveRemainderAlternative);
BENCHMARK(BM_DivisionAndRemainder);
BENCHMARK(BM_DivisionAndRemainderAlternative);
BENCHMARK(BM_DivisionAndRemainderBaseline);

}  // namespace
}  // namespace sat
}  // namespace operations_research
