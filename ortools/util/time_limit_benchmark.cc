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

#include <limits>

#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "benchmark/benchmark.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace {

const double kInfinity = std::numeric_limits<double>::infinity();

template <bool with_time_limit>
static void BM_TimeLimitReached(benchmark::State& state) {
  for (auto _ : state) {
    TimeLimit time_limit(1.0);
    double sum = 0.0;
    for (int i = 0; i < 10000; ++i) {
      if (with_time_limit) {
        CHECK(!time_limit.LimitReached());
      }
      // Something that takes some time.
      for (int j = 0; j < 50; ++j) {
        sum += i * j;
      }
    }
    CHECK(!time_limit.LimitReached());
    CHECK_GE(sum, 0.0);
  }
  state.SetLabel(absl::StrCat(with_time_limit ? "With" : "Without",
                              " TimeLimitReached()"));
}

// This just gives an idea of how slow it is to call LimitReached().
// blaze run -c opt --linkopt=-static --dynamic_mode=off
//   ortools/util:time_limit_test -- --benchmarks=all
//
// Here are perflab benchmark results:
// Run on lpl35 (24 X 2800 MHz CPUs); 2014/05/21-04:12:59
// CPU: Intel Westmere with HyperThreading (12 cores) dL1:32KB dL2:256KB
// Benchmark                    Time(ns)    CPU(ns) Iterations
// -----------------------------------------------------------
// BM_TimeLimitReached<false>     716538     713331        981  Without
// BM_TimeLimitReached<true>      742439     739093        947  With TimeLimit
BENCHMARK_TEMPLATE(BM_TimeLimitReached, /*with_time_limit=*/false);
BENCHMARK_TEMPLATE(BM_TimeLimitReached, /*with_time_limit=*/true);

// This is to verify that the messages do not have any effect on performance
// of the optimized code (that the compiler in opt mode removes the messages).
// blaze run -c opt --linkopt=static --dynamic_mode=off
//   //ortools/util:time_limit_test -- --benchmarks=all
//
// Here are perflab benchmark results:
// Run on lpab10 (32 X 2600 MHz CPUs); 2015/07/27-02:59:39
// CPU: Intel Sandybridge with HyperThreading (16 cores) dL1:32KB dL2:256KB
// Benchmark                            Time(ns)    CPU(ns) Iterations
// -------------------------------------------------------------------
// BM_AdvanceDeterministicTime<false>     514067     513431       1000  Without
// BM_AdvanceDeterministicTime<true>      440777     440644       1596  With
//
// Note that the difference between the two benchmarks is most likely because of
// memory cache behavior. The same difference (but with opposite values) is
// observed when the order of the two benchmarks is reversed.
template <bool with_messages>
static void BM_AdvanceDeterministicTime(benchmark::State& state) {
  for (auto _ : state) {
    TimeLimit time_limit(kInfinity, 1000.0);
    double sum = 0.0;
    while (!time_limit.LimitReached()) {
      // Something that takes time.
      for (int i = 0; i < 500; ++i) {
        sum += i * i;
      }
      if (with_messages) {
        time_limit.AdvanceDeterministicTime(1.0, "AdvanceTime");
      } else {
        time_limit.AdvanceDeterministicTime(1.0);
      }
    }
    CHECK_GE(sum, 0.0);
  }
  state.SetLabel(absl::StrCat("AdvanceDeterministicTime",
                              with_messages ? "With" : "Without",
                              "NamedCounters"));
}

BENCHMARK_TEMPLATE(BM_AdvanceDeterministicTime, /*with_messages=*/false);
BENCHMARK_TEMPLATE(BM_AdvanceDeterministicTime, /*with_messages=*/true);

}  // namespace
}  // namespace operations_research
