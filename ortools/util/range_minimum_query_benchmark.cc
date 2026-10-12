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

#include <utility>
#include <vector>

#include "absl/container/fixed_array.h"
#include "benchmark/benchmark.h"
#include "ortools/util/bitset.h"
#include "ortools/util/range_minimum_query.h"

namespace operations_research {
namespace {

void BM_Constructor(benchmark::State& state) {
  int size = state.range(0);
  for (auto _ : state) {
    std::vector<int> array(size, 0);
    const RangeMinimumQuery<int> rmq(std::move(array));
    rmq.RangeMinimum(0, size);
  }
}

BENCHMARK(BM_Constructor)->Range(1, 1 << 20);

// Since a call to GetMinimumFromRange is around 3 ns, this benchmark reports
// the time for 100 calls. There is a significant overhead from the additional
// operations performed in the tight loop. If the line
//    sum += rmq.GetMinimumFromRange(from, to);
// is substituted with
//    sum += rmq.GetMinimumFromRange(0, i & size_mask);
// the time for 100 queries drops from 330 to 267. The actual time (without the
// bit and) is probably around 250 ns / 100 Queries.
void BM_100Queries(benchmark::State& state) {
  int size = state.range(0);
  std::vector<int> array(size, 0);
  const RangeMinimumQuery<int> rmq(std::move(array));

  const int msb_size = MostSignificantBitPosition32(size);
  const int size_mask = (1 << msb_size) - 1;
  int i = 0;
  int sum = 0;
  for (auto _ : state) {
    for (int j = 0; j < 100; j++) {
      const int to = (i++ & size_mask) | 1;
      const int from = to >> 1;
      sum += rmq.RangeMinimum(from, to);
    }
  }
  benchmark::DoNotOptimize(sum);
}

BENCHMARK(BM_100Queries)->Range(1, 1 << 20);

}  // namespace
}  // namespace operations_research
