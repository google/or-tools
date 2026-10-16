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
#include <cstdint>
#include <random>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/random/distributions.h"
#include "benchmark/benchmark.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace {

void BM_DomainIntersection(benchmark::State& state) {
  // Use a fully deterministic pseudo-random sequence for consistent results.
  std::mt19937 random(12345);
  const int kNumIntervalsPerDomain = state.range(0);
  // Pre-generate a few domains at random: we want enough to be able to generate
  // a very large number of pairs of domains, while not taking too much space
  // (ideally, should fit in L3 cache).
  const int kNumDomains =
      static_cast<int>(10000 / sqrt(kNumIntervalsPerDomain));
  std::vector<Domain> domains(kNumDomains);
  std::vector<int64_t> flat_intervals;
  for (int i = 0; i < kNumDomains; ++i) {
    int64_t x = absl::Uniform(random, 0, 1000);
    flat_intervals.clear();
    flat_intervals.push_back(x);
    for (int k = 1; k < 2 * kNumIntervalsPerDomain; ++k) {
      x += absl::LogUniform(random, 1, 1000);
      flat_intervals.push_back(x);
    }
    domains[i] = Domain::FromFlatSpanOfIntervals(flat_intervals);
  }

  int64_t witness = 0;  // To prevent compiler optimization. See usage.
  int i = 0;
  int j = 0;
  for (auto _ : state) {
    witness += domains[i].IntersectionWith(domains[j]).NumIntervals();
    if (++j == kNumDomains) {
      j = 0;
      if (++i == kNumDomains) i = 0;
    }
  }
  if (state.iterations() > 1000000) {
    CHECK_GT(witness, state.iterations() * kNumIntervalsPerDomain / 6);
    CHECK_LT(witness, state.iterations() * kNumIntervalsPerDomain);
    VLOG(1) << "Average intersection size: "
            << static_cast<double>(witness) / state.iterations();
  }
  state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_DomainIntersection)->RangeMultiplier(2)->Range(1, 64);

void BM_SingleIntervalDomainConstructor(benchmark::State& state) {
  constexpr int kArraySize = 10000;
  std::vector<Domain> domains;
  domains.reserve(kArraySize);
  int witness = 0;  // To prevent the compiler from optimizing our code away.
  int i = 0;
  for (auto _ : state) {
    domains.emplace_back(i, i + 43);
    if (domains.size() == kArraySize) {
      witness += domains[i].Max() - domains[i].Min();
      domains.clear();
    }
    if ((i += 57) >= kArraySize) i -= kArraySize;
  }
  CHECK_NE(witness, 42);
  state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_SingleIntervalDomainConstructor);

SortedDisjointIntervalList MakeSortedDisjointIntervalList(int count) {
  SortedDisjointIntervalList intervals;
  std::vector<int> starts, ends;
  for (int i = 0; i < count; ++i) {
    starts.push_back(i << 1);
    ends.push_back((i << 1) + 1);
  }
  intervals.InsertIntervals(starts, ends);
  return intervals;
}

void BM_ShortStructCopyIteration(benchmark::State& state) {
  const int count = state.range(0);
  auto intervals = MakeSortedDisjointIntervalList(count);
  for (auto _ : state) {
    for (ClosedInterval interval : intervals) {
      benchmark::DoNotOptimize(interval);
    }
  }
}

BENCHMARK(BM_ShortStructCopyIteration)->Range(1 << 3, 1 << 18);

void BM_ShortStructReferenceIteration(benchmark::State& state) {
  const int count = state.range(0);
  auto intervals = MakeSortedDisjointIntervalList(count);
  for (auto _ : state) {
    for (const ClosedInterval& interval : intervals) {
      benchmark::DoNotOptimize(const_cast<ClosedInterval&>(interval));
    }
  }
}

BENCHMARK(BM_ShortStructReferenceIteration)->Range(1 << 3, 1 << 18);
std::vector<ClosedInterval> MakeSortedAndDisjointIntervals(int count) {
  std::vector<ClosedInterval> intervals;
  for (int i = 0; i < count; ++i) {
    intervals.push_back(ClosedInterval(i << 1, (i << 1) + 1));
  }
  return intervals;
}

void BM_IntervalsAreSortedAndNonAdjacent(benchmark::State& state) {
  const int count = state.range(0);
  // Worst case intervals for IntervalsAreSortedAndNonAdjacent().
  auto intervals = MakeSortedAndDisjointIntervals(count);
  for (auto _ : state) {
    benchmark::DoNotOptimize(IntervalsAreSortedAndNonAdjacent(intervals));
  }
}

BENCHMARK(BM_IntervalsAreSortedAndNonAdjacent)
    ->DenseRange(1, 7)
    ->Range(1 << 3, 1 << 18);
}  // namespace
}  // namespace operations_research
