// Copyright 2010-2022 Google LLC
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

#include "ortools/algorithms/duplicate_remover.h"

#include <random>
#include <vector>

#include "benchmark/benchmark.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "ortools/base/linked_hash_set.h"
#include "ortools/util/random_engine.h"
#include "util/tuple/dump_vars.h"

namespace operations_research {
namespace {

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;

TEST(DenseIntDuplicateRemoverTest, RemoveDuplicatesEmpty) {
  std::vector<int> v;
  DenseIntDuplicateRemover deduper(10);
  deduper.RemoveDuplicates(&v);
  EXPECT_THAT(v, IsEmpty());
}

TEST(DenseIntDuplicateRemoverTest, RemoveDuplicatesNZeroAndEmpty) {
  std::vector<int> v;
  DenseIntDuplicateRemover deduper(0);
  deduper.RemoveDuplicates(&v);
  EXPECT_THAT(v, IsEmpty());
}

TEST(DenseIntDuplicateRemoverTest, RemoveDuplicatesSimpleCaseWithDuplicates) {
  std::vector<int> v = {1, 8, 2, 2, 8, 4, 1, 2, 7, 0, 2};
  DenseIntDuplicateRemover deduper(9);
  deduper.RemoveDuplicates(&v);
  EXPECT_THAT(v, ElementsAre(1, 8, 2, 4, 7, 0));
}

TEST(DenseIntDuplicateRemoverTest, RemoveDuplicatesSimpleCaseWithNoDuplicates) {
  std::vector<int> v = {3, 2, 0, 5, 4, 1};
  const std::vector<int> v_copy = v;
  DenseIntDuplicateRemover deduper(6);
  deduper.RemoveDuplicates(&v);
  EXPECT_THAT(v, ElementsAreArray(v_copy));
}

TEST(DenseIntDuplicateRemoverTest, RemoveDuplicatesWithRepeatedField) {
  const std::vector<int> v = {1, 0, 1, 2, 1};
  google::protobuf::RepeatedField<int> r(v.begin(), v.end());
  DenseIntDuplicateRemover deduper(3);
  deduper.RemoveDuplicates(&r);
  EXPECT_THAT(r, ElementsAre(1, 0, 2));
}

std::vector<int> UniqueValues(absl::Span<const int> span) {
  absl::flat_hash_set<int> set;
  std::vector<int> out;
  for (int x : span)
    if (set.insert(x).second) out.push_back(x);
  return out;
}

TEST(DenseIntDuplicateRemoverTest, RemoveDuplicatesRandomizedStressTest) {
  constexpr int kNumValues = 1003;
  DenseIntDuplicateRemover deduper(kNumValues);
  constexpr int kNumTests = 1'000'000;
  absl::BitGen random;
  for (int t = 0; t < kNumTests; ++t) {
    const int size = absl::LogUniform(random, 0, 16);
    const int domain_size =
        absl::Uniform(absl::IntervalClosed, random, 1, kNumValues);
    std::vector<int> v(size);
    for (int& x : v) x = absl::Uniform(random, 0, domain_size);
    const std::vector<int> v_initial = v;
    const std::vector<int> unique_values = UniqueValues(v);
    deduper.RemoveDuplicates(&v);
    ASSERT_THAT(v, ElementsAreArray(unique_values)) << DUMP_VARS(t, v_initial);
  }
}

TEST(DenseIntDuplicateRemoverTest,
     AppendAndLazilyRemoveDuplicatesRandomizedStressTest) {
  constexpr int kNumValues = 103;
  constexpr int kNumTests = 1'000;
  std::mt19937 random;
  gtl::linked_hash_set<int> reference;
  std::vector<int> v;
  int64_t num_extra_elements = 0;
  int64_t num_unique_elements = 0;
  for (int t = 0; t < kNumTests; ++t) {
    const int num_inserts = absl::LogUniform(random, 2, 1 << 16);
    const int domain_size =
        absl::Uniform(absl::IntervalClosed, random, 1, kNumValues);
    v.clear();
    reference.clear();
    DenseIntDuplicateRemover deduper(domain_size);
    for (int i = 0; i < num_inserts; ++i) {
      const int x = absl::Uniform(random, 0, domain_size);
      deduper.AppendAndLazilyRemoveDuplicates(x, &v);
      reference.insert(x);
    }
    ASSERT_LE(v.size(), domain_size * 2 + 15);
    const int old_size = v.size();
    deduper.RemoveDuplicates(&v);
    num_unique_elements += v.size();
    num_extra_elements += old_size - v.size();
    ASSERT_THAT(v, ElementsAreArray(reference))
        << DUMP_VARS(t, num_inserts, domain_size, old_size, v.size());
  }
  EXPECT_LE(static_cast<double>(num_extra_elements) / num_unique_elements, 0.5);
}

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
    testing::DoNotOptimize(v);
    testing::DoNotOptimize(set);
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
