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

#include "ortools/algorithms/adjustable_k_ary_heap.h"

#include <cstdint>
#include <limits>
#include <queue>
#include <random>
#include <utility>
#include <vector>

#include "absl/random/random.h"
#include "gtest/gtest.h"
#include "ortools/base/strong_int.h"
#include "ortools/base/strong_vector.h"

namespace operations_research {

TEST(AdjustableKAryHeapTest, RandomDataStrongCheck) {
  const int kSize = 10'000;
  const double priority_range = kSize / 100;
  absl::BitGen generator;
  std::uniform_real_distribution<float> priority_dist(0, priority_range);
  std::vector<std::pair<float, int>> subsets_and_values(kSize);
  for (int i = 0; i < kSize; ++i) {
    subsets_and_values[i] = {priority_dist(generator), i};
  }

  AdjustableKAryHeap<float, int, 5, true> heap(subsets_and_values, kSize);
  EXPECT_TRUE(heap.CheckHeapProperty());
  float last = std::numeric_limits<float>::max();
  while (!heap.IsEmpty()) {
    const auto prio = heap.TopPriority();
    heap.Pop();
    EXPECT_LE(prio, last);
    last = prio;
  }
  EXPECT_TRUE(heap.IsEmpty());
  EXPECT_TRUE(heap.CheckHeapProperty());
}

TEST(AdjustableKAryHeapTest, RandomDataSpeed) {
  const int kSize = 1'000'000;
  const double priority_range = kSize / 100;
  absl::BitGen generator;
  std::uniform_real_distribution<float> priority_dist(0, priority_range);
  std::vector<std::pair<float, int>> subsets_and_values(kSize);
  for (int i = 0; i < kSize; ++i) {
    subsets_and_values[i] = {priority_dist(generator), i};
  }

  AdjustableKAryHeap<float, int, 4, true> heap(subsets_and_values, kSize);
  EXPECT_TRUE(heap.CheckHeapProperty());
  while (!heap.IsEmpty()) {
    heap.Pop();
  }
  EXPECT_TRUE(heap.CheckHeapProperty());
  EXPECT_TRUE(heap.IsEmpty());
}

TEST(AdjustableKAryHeapTest, UpdateStrongCheck) {
  const int kSize = 10'000;
  const int kNumUpdates = kSize / 100;
  const double priority_range = kSize / 100;
  absl::BitGen generator;
  std::uniform_real_distribution<float> priority_dist(0, priority_range);
  std::uniform_int_distribution<int> index_dist(0, kSize - 1);
  std::vector<std::pair<float, int>> subsets_and_values(kSize);
  for (int i = 0; i < kSize; ++i) {
    subsets_and_values[i] = {priority_dist(generator), i};
  }
  AdjustableKAryHeap<float, int, 4, true> heap(subsets_and_values, kSize);
  EXPECT_TRUE(heap.CheckHeapProperty());
  for (int iter = 0; iter < kNumUpdates; ++iter) {
    heap.Update({priority_dist(generator), index_dist(generator)});
    EXPECT_TRUE(heap.CheckHeapProperty());
  }
}

TEST(AdjustableKAryHeapTest, RemoveStrongCheck) {
  const int kSize = 10'000;
  const int kNumRemovals = kSize;
  const double priority_range = kSize / 10;
  absl::BitGen generator;
  std::uniform_real_distribution<float> priority_dist(0, priority_range);
  std::uniform_int_distribution<int> index_dist(0, kSize - 1);
  std::vector<std::pair<float, int>> subsets_and_values(kSize);
  for (int i = 0; i < kSize; ++i) {
    subsets_and_values[i] = {priority_dist(generator), i};
  }
  AdjustableKAryHeap<float, int, 4, true> heap(subsets_and_values, kSize);
  EXPECT_TRUE(heap.CheckHeapProperty());
  for (int iter = 0; iter < kNumRemovals; ++iter) {
    heap.Remove(iter);
    EXPECT_TRUE(heap.CheckHeapProperty());
  }
}

TEST(AdjustableKAryHeapTest, OneByOneStrongCheck) {
  const int kSize = 10'000;
  const int kNumInsertions = kSize;
  const double priority_range = kSize / 10;
  absl::BitGen generator;
  std::uniform_real_distribution<float> priority_dist(0, priority_range);
  std::uniform_int_distribution<int> index_dist(0, kSize - 1);
  std::vector<std::pair<float, int>> subsets_and_values;
  AdjustableKAryHeap<float, int, 4, true> heap;
  EXPECT_TRUE(heap.CheckHeapProperty());
  for (int iter = 0; iter < kNumInsertions; ++iter) {
    heap.Insert({priority_dist(generator), index_dist(generator)});
    EXPECT_TRUE(heap.CheckHeapProperty());
  }
}

TEST(AdjustableKAryHeapTest, OneByOneStrongSpeed) {
  const int kSize = 1'000'000;
  const int kNumInsertions = kSize;
  const double priority_range = kSize / 10;
  absl::BitGen generator;
  std::uniform_real_distribution<float> priority_dist(0, priority_range);
  std::uniform_int_distribution<int> index_dist(0, kSize - 1);
  std::vector<std::pair<float, int>> subsets_and_values;
  AdjustableKAryHeap<float, int, 4, true> heap;
  EXPECT_TRUE(heap.CheckHeapProperty());
  for (int iter = 0; iter < kNumInsertions; ++iter) {
    heap.Insert({priority_dist(generator), index_dist(generator)});
  }
  EXPECT_TRUE(heap.CheckHeapProperty());
}

TEST(StandardHeapTest, RandomDataSpeed) {
  const int kSize = 1'000'000;
  const double priority_range = kSize / 100;
  absl::BitGen generator;
  std::uniform_real_distribution<float> priority_dist(0, priority_range);

  std::vector<float> values(kSize);
  for (int i = 0; i < kSize; ++i) {
    values[i] = priority_dist(generator);
  }
  std::priority_queue<float> heap(values.begin(), values.end());
  while (!heap.empty()) {
    heap.pop();
  }
}

TEST(AdjustableKAryHeapTest, DoubleInsertionOneRemoval) {
  const int kSize = 10'000;
  AdjustableKAryHeap<float, int, 4, true> heap;

  for (int i = 0; i < kSize; ++i) {
    heap.Insert({static_cast<float>(i), i});
    heap.Insert({static_cast<float>(i + 1), i});
    heap.Remove(i);

    EXPECT_FALSE(heap.Contains(i));
  }
  EXPECT_TRUE(heap.CheckHeapProperty());
}

TEST(AdjustableKAryHeapTest, TopBottomIndexPrioritySimple) {
  // In this test, the element with the lowest priority is the last one.
  AdjustableKAryHeap<int, int, 4, /*IsMaxHeap=*/true> heap;

  heap.Insert({10, 0});
  EXPECT_EQ(heap.TopIndex(), 0);
  EXPECT_EQ(heap.TopPriority(), 10);
  EXPECT_EQ(heap.BottomIndex(), 0);
  EXPECT_EQ(heap.BottomPriority(), 10);

  heap.Insert({20, 1});
  EXPECT_EQ(heap.TopIndex(), 1);
  EXPECT_EQ(heap.TopPriority(), 20);
  EXPECT_EQ(heap.BottomIndex(), 0);
  EXPECT_EQ(heap.BottomPriority(), 10);

  EXPECT_TRUE(heap.Remove(0));
  EXPECT_EQ(heap.TopIndex(), 1);
  EXPECT_EQ(heap.TopPriority(), 20);
  EXPECT_EQ(heap.BottomIndex(), 1);
  EXPECT_EQ(heap.BottomPriority(), 20);
}

TEST(AdjustableKAryHeapTest, BottomIndexPriority) {
  // In this test, the element with the lowest priority is not the last one in
  // the heap representation. A wrong implementation of the
  // `GetLowestPriorityChild()` could lead to a wrong result here (and did so).
  std::vector<std::pair<int, int>> elements = {
      {10, 0}, {5, 1}, {8, 2}, {1, 3}, {2, 4}};
  AdjustableKAryHeap<int, int, 4, /*IsMaxHeap=*/true> heap;
  heap.Load(elements, /*universe_size=*/5);

  EXPECT_EQ(heap.BottomIndex(), 3);
  EXPECT_EQ(heap.BottomPriority(), 1);
}

DEFINE_STRONG_INT_TYPE(NodeIndex, int32_t);

TEST(AdjustableKAryHeapTest, StrongIntIndex) {
  // As most of the implementation is based on templates, check that it compiles
  // with StrongInt. Hence, this test should use all the public methods at least
  // once.
  AdjustableKAryHeap<int, NodeIndex, 4, true> heap;

  heap.Load(/*indices=*/std::vector{NodeIndex(0)},
            /*priorities=*/std::vector{1},
            /*universe_size=*/NodeIndex(1));
  EXPECT_FALSE(heap.IsEmpty());
  heap.Clear();
  EXPECT_TRUE(heap.IsEmpty());

  heap.Insert({1, NodeIndex(1)});
  heap.Update({2, NodeIndex(1)});
  EXPECT_TRUE(heap.CheckHeapProperty());
  EXPECT_FALSE(heap.IsEmpty());

  EXPECT_TRUE(heap.Remove(NodeIndex(1)));
  EXPECT_FALSE(heap.Contains(NodeIndex(1)));
  EXPECT_EQ(heap.heap_size(), NodeIndex(0));
  heap.Clear();

  heap.Insert({1, NodeIndex(1)});
  EXPECT_EQ(heap.TopPriority(), 1);
  EXPECT_EQ(heap.TopIndex(), NodeIndex(1));
  EXPECT_EQ(heap.BottomPriority(), 1);
  EXPECT_EQ(heap.BottomIndex(), NodeIndex(1));
  heap.Pop();
}
}  // namespace operations_research
