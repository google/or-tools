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

#include <string>

#include "benchmark/benchmark.h"

// We need to do this *before* including util/stats.h, so that the
// SCOPED_TIME_STAT() macro does the actual tracking of stats.
#ifndef OR_STATS
#define OR_STATS
#endif  // OR_STATS
#include "ortools/util/stats.h"

namespace operations_research {
namespace {

// This provides a point of comparison for the cost of the benchmark loop. This
// is the best `BM_ScopedOverhead<DisabledScopedTimeStats>` can do.
void BM_EmptyBenchmarkLoop(benchmark::State& state) {
  for (auto _ : state) {
  }
}
BENCHMARK(BM_EmptyBenchmarkLoop);

// This increments stats directly without using RAII. This is the best
// `BM_ScopedOverhead<EnabledScopedTimeStats>` can do.
void BM_DirectOverhead(benchmark::State& state) {
  TimeDistribution stat(__FUNCTION__);
  for (auto _ : state) {
    stat.StartTimer();
    stat.StopTimerAndAddElapsedTime();
  }
}
BENCHMARK(BM_DirectOverhead);

template <typename ScopedStatsT>
void BM_ScopedOverhead(benchmark::State& state) {
  StatsGroup stats("scoped interface with map<>");
  // We fill the map with similar names to test the overhead.
  for (int i = 0; i < 10; ++i) {
    std::string str(i, '_');
    stats.LookupOrCreateTimeDistribution(__FUNCTION__ + str);
  }
  for (auto _ : state) {
    ScopedStatsT scoped_stats(&stats, __FUNCTION__);
  }
  benchmark::DoNotOptimize(stats);
}

BENCHMARK(BM_ScopedOverhead<DisabledScopedTimeStats>);
BENCHMARK(BM_ScopedOverhead<EnabledScopedTimeStats>);

#undef OR_STATS
}  // namespace
}  // namespace operations_research
