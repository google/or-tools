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
#include <utility>

#include "absl/container/flat_hash_set.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/random/random.h"
#include "benchmark/benchmark.h"
#include "ortools/util/dense_set.h"

namespace operations_research {
namespace {

template <typename Set>
int64_t DoOneBMIteration(Set& set, absl::BitGenRef gen) {
  int64_t sum = 0;
  // Don't use generate random numbers inside the innermost loop to avoid
  // benchmarking absl::Uniform.
  int64_t base = absl::Uniform<int64_t>(gen, 0, 1024 * 1024);
  for (int i = 1; i <= 100; ++i) {
    set.insert((i * base) % (1024 * 1024));
  }
  for (int v : set) {
    sum += v;
  }
  while (set.size() > 1000) {
    set.erase(set.begin());
  }
  return sum;
}

template <typename Set>
void BM_SetIteration(benchmark::State& state, Set set = Set()) {
  absl::BitGen gen;
  int64_t sum = 0;
  int base = absl::Uniform<int64_t>(gen, 1, 1024 * 1024);
  for (int i = 0; i < 1000; ++i) {
    set.insert(i * base % (1024 * 1024));
  }
  for (auto s : state) {
    sum += DoOneBMIteration(set, gen);
  }
}

void BM_HashSetIteration(benchmark::State& state) {
  BM_SetIteration<absl::flat_hash_set<int>>(state);
}
void BM_DenseSetIteration(benchmark::State& state) {
  BM_SetIteration<DenseSet<int>>(state);
}
void BM_HashSetIterationPreReserved(benchmark::State& state) {
  absl::flat_hash_set<int> set;
  set.reserve(1500);
  BM_SetIteration(state, std::move(set));
}
void BM_DenseSetIterationPreReserved(benchmark::State& state) {
  DenseSet<int> set;
  set.reserve(1024 * 1024);
  BM_SetIteration(state, std::move(set));
}
void BM_UnsafeDenseSetIterationPreReserved(benchmark::State& state) {
  UnsafeDenseSet<int> set;
  set.reserve(1024 * 1024);
  BM_SetIteration(state, std::move(set));
}
void BM_HashSetIterationPreReservedRehash(benchmark::State& state) {
  absl::flat_hash_set<int> set;
  set.reserve(1500);
  absl::BitGen gen;
  int64_t sum = 0;
  for (int i = 0; i < 1000; ++i) {
    set.insert(absl::Uniform(gen, 0, 1024 * 1024));
  }
  for (auto s : state) {
    set.rehash(1500);
    sum += DoOneBMIteration(set, gen);
  }
}

BENCHMARK(BM_HashSetIteration);
BENCHMARK(BM_DenseSetIteration);
BENCHMARK(BM_HashSetIterationPreReserved);
BENCHMARK(BM_DenseSetIterationPreReserved);
BENCHMARK(BM_UnsafeDenseSetIterationPreReserved);
BENCHMARK(BM_HashSetIterationPreReservedRehash);

}  // namespace
}  // namespace operations_research
