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

#include "ortools/util/sort.h"

#include <algorithm>
#include <random>
#include <vector>

#include "absl/random/bit_gen_ref.h"
#include "absl/random/distributions.h"
#include "gtest/gtest.h"

namespace operations_research {
namespace {

// Test the correctness of the algorithm.
// --------------------------------------

std::vector<int> RandomVector(int size, int min_value, int max_value,
                              absl::BitGenRef randomizer) {
  std::vector<int> to_sort;
  for (int i = 0; i < size; ++i) {
    to_sort.push_back(
        absl::Uniform(absl::IntervalClosed, randomizer, min_value, max_value));
  }
  return to_sort;
}

bool SortRandomIntVector(int max_comparisons, int size, int min_value,
                         int max_value, absl::BitGenRef randomizer) {
  // Build two identical random vectors.
  std::vector<int> to_std_sort =
      RandomVector(size, min_value, max_value, randomizer);
  std::vector<int> to_incr_sort = to_std_sort;

  // Sort the first vector with a well tested algorithm.
  std::sort(to_std_sort.begin(), to_std_sort.end());
  if (!std::is_sorted(to_std_sort.begin(), to_std_sort.end())) return false;

  // Sort the second vector with the algorithm to test.
  IncrementalSort(max_comparisons, to_incr_sort.begin(), to_incr_sort.end());
  if (!std::is_sorted(to_incr_sort.begin(), to_incr_sort.end())) return false;

  // Make sure that both sorted vectors are identical
  for (int i = 0; i < size; ++i) {
    if (to_incr_sort[i] != to_std_sort[i]) return false;
  }
  return true;
}

TEST(IncrementalSort, Empty) {
  std::mt19937 randomizer(12345);
  EXPECT_TRUE(SortRandomIntVector(0, 0, -1024, 1024, randomizer));
}

TEST(IncrementalSort, OneElement) {
  std::mt19937 randomizer(12345);
  EXPECT_TRUE(SortRandomIntVector(1, 0, -1024, 1024, randomizer));
}

TEST(IncrementalSort, NoInsertionSort) {
  std::mt19937 randomizer(12345);
  for (int i = 0; i < 10000; ++i) {
    ASSERT_TRUE(SortRandomIntVector(0, 1024, -1024, 1024, randomizer));
  }
}

TEST(IncrementalSort, InsertionSortOnly) {
  std::mt19937 randomizer(12345);
  for (int i = 0; i < 10000; ++i) {
    ASSERT_TRUE(SortRandomIntVector(10000000, 1024, -1024, 1024, randomizer));
  }
}

TEST(IncrementalSort, MixedSort) {
  std::mt19937 randomizer(12345);
  for (int i = 0; i < 10000; ++i) {
    ASSERT_TRUE(SortRandomIntVector(2048, 1024, -1024, 1024, randomizer));
  }
}

}  // namespace
}  // namespace operations_research
