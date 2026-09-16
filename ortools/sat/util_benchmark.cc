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
#include <random>
#include <vector>

#include "benchmark/benchmark.h"
#include "ortools/sat/util.h"
#include "ortools/util/random_engine.h"

namespace operations_research {
namespace sat {
namespace {

static void BM_bounded_subset_sum(benchmark::State& state) {
  random_engine_t random_;
  const int num_items = state.range(0);
  const int num_choices = state.range(1);
  const int max_capacity = state.range(2);
  const int max_size = state.range(3);

  const int num_updates = num_items * num_choices;
  const int capacity = std::uniform_int_distribution<int>(
      max_capacity / 2, max_capacity)(random_);
  MaxBoundedSubsetSum subset_sum(capacity);
  std::uniform_int_distribution<int> size_dist(0, max_size);
  std::vector<int64_t> choices(num_choices);
  for (auto _ : state) {
    subset_sum.Reset(capacity);
    for (int i = 0; i < num_items; ++i) {
      for (int j = 0; j < num_choices; ++j) {
        choices[j] = size_dist(random_);
      }
      subset_sum.AddChoices(choices);
    }
  }
  // Number of updates.
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                          num_updates);
}

BENCHMARK(BM_bounded_subset_sum)
    ->Args({10, 3, 30, 5})
    ->Args({10, 4, 50, 10})
    ->Args({10, 4, 30, 20})
    ->Args({25, 3, 30, 5})
    ->Args({25, 4, 50, 10})
    ->Args({25, 4, 30, 20})
    ->Args({60, 3, 30, 5})
    ->Args({60, 4, 50, 10})
    ->Args({60, 4, 30, 20})
    ->Args({100, 3, 30, 5})
    ->Args({100, 4, 50, 10})
    ->Args({100, 4, 30, 20});

}  // namespace
}  // namespace sat
}  // namespace operations_research
