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

#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/random/distributions.h"
#include "benchmark/benchmark.h"
#include "ortools/algorithms/duplicate_remover.h"
#include "ortools/util/random_engine.h"

namespace operations_research {
namespace {

template <bool use_flat_hash_set>
void BM_AppendAndLazilyRemoveDuplicates(benchmark::State& state) {
  const int num_inserts = state.range(0);
  const int domain_size = state.range(1);
  std::vector<int> to_insert(num_inserts);
  random_engine_t random;
  for (int& x : to_insert) x = absl::Uniform(random, 0, domain_size);
  DenseIntDuplicateRemover deduper(domain_size);
  std::vector<int> v;
  absl::flat_hash_set<int> set;
  for (auto _ : state) {
    v.clear();
    set.clear();
    for (int x : to_insert) {
      if (use_flat_hash_set) {
        set.insert(x);
      } else {
        deduper.AppendAndLazilyRemoveDuplicates(x, &v);
      }
    }
    if (!use_flat_hash_set) deduper.RemoveDuplicates(&v);
    benchmark::DoNotOptimize(v);
    benchmark::DoNotOptimize(set);
  }
  state.SetItemsProcessed(state.iterations() * num_inserts);
}

BENCHMARK(BM_AppendAndLazilyRemoveDuplicates<true>)
    ->ArgPair(1, 10)
    ->ArgPair(10, 2)
    ->ArgPair(10, 10)
    ->ArgPair(100, 100)
    ->ArgPair(100, 10)
    ->ArgPair(10'000, 10'000)
    ->ArgPair(10'000, 1'000)
    ->ArgPair(10'000, 100)
    ->ArgPair(10'000, 10)
    ->ArgPair(1'000'000, 1'000'000)
    ->ArgPair(1'000'000, 10'000)
    ->ArgPair(1'000'000, 100);

BENCHMARK(BM_AppendAndLazilyRemoveDuplicates<false>)
    ->ArgPair(1, 10)
    ->ArgPair(10, 2)
    ->ArgPair(10, 10)
    ->ArgPair(100, 100)
    ->ArgPair(100, 10)
    ->ArgPair(10'000, 10'000)
    ->ArgPair(10'000, 1'000)
    ->ArgPair(10'000, 100)
    ->ArgPair(10'000, 10)
    ->ArgPair(1'000'000, 1'000'000)
    ->ArgPair(1'000'000, 10'000)
    ->ArgPair(1'000'000, 100);

}  // namespace
}  // namespace operations_research
