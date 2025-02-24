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
#include <limits>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/random/random.h"
#include "gtest/gtest.h"
#include "ortools/algorithms/adjustable_k_ary_heap.h"

// Stress test for AdjustableKaryHeap.
// The test generates a random heap of size num_elements. Then, it randomly
// changes the priority of a fraction of the elements (fraction_to_change),
// removes a fraction of the elements (fraction_to_remove), and reinserts a
// fraction of the elements (fraction_to_reinsert). After all of these
// operations, the test verifies that the heap property is satisfied. Then, it
// pops all of the elements from the heap and verifies that the elements are
// popped in order.

ABSL_FLAG(int32_t, num_elements, 100000000,
          "The number of elements for the stress test.");
ABSL_FLAG(double, fraction_to_change_priority, 0.01,
          "The fraction of elements that will get a changed priority after "
          "initial population.");
ABSL_FLAG(double, fraction_to_reinsert, 0.001,
          "The fraction of elements that will get reinserted after "
          "initial population.");
ABSL_FLAG(double, fraction_to_remove, 0.001,
          "The fraction of elements that will get removed after "
          "initial population.");

namespace operations_research {

template <typename Priority, typename Index, int Arity, bool IsMaxHeap>
void StressTest() {
  AdjustableKAryHeap<Priority, Index, Arity, IsMaxHeap> heap;
  std::mt19937 rnd(/*seed=*/301);
  const int32_t num_elements = absl::GetFlag(FLAGS_num_elements);
  const double fraction_to_change =
      absl::GetFlag(FLAGS_fraction_to_change_priority);
  const double fraction_to_reinsert = absl::GetFlag(FLAGS_fraction_to_reinsert);
  const double fraction_to_remove = absl::GetFlag(FLAGS_fraction_to_remove);

  LOG(INFO) << "Populating AdjustableKaryHeap with num_elements = "
            << num_elements;

  std::vector<int32_t> elts_to_change, elts_to_reinsert, elts_to_remove;
  for (int32_t i = 0; i < num_elements; ++i) {
    const double priority = absl::Uniform<double>(rnd, 0, 1000000000.0);

    if (absl::Uniform<double>(rnd, 0, 1.0) < fraction_to_change) {
      elts_to_change.push_back(i);
    }
    if (absl::Uniform<double>(rnd, 0, 1.0) < fraction_to_reinsert) {
      elts_to_reinsert.push_back(i);
    }
    if (absl::Uniform<double>(rnd, 0, 1.0) < fraction_to_remove) {
      elts_to_remove.push_back(i);
    }
    heap.Insert({priority, i});
    LOG_EVERY_POW_2(INFO) << "heap.Insert, i = " << i;
  }
  LOG(INFO) << "AdjustableKaryHeap filled with heap_size = "
            << heap.heap_size();
  LOG(INFO) << "elts_to_change.size() = " << elts_to_change.size();
  for (const auto elem : elts_to_change) {
    const double updated_priority = absl::Uniform<double>(rnd, 0, 1000000000.0);
    heap.Update({updated_priority, elem});
  }

  LOG(INFO) << "AdjustableKaryHeap filled with heap_size = "
            << heap.heap_size();
  LOG(INFO) << "elts_to_remove.size() = " << elts_to_remove.size();
  for (const auto elem : elts_to_remove) {
    heap.Remove(elem);
  }

  LOG(INFO) << "AdjustableKaryHeap filled with heap_size = "
            << heap.heap_size();
  LOG(INFO) << "elts_to_reinsert.size() = " << elts_to_reinsert.size();
  for (const auto elem : elts_to_reinsert) {
    const double updated_priority = absl::Uniform<double>(rnd, 0, 1000000000.0);
    heap.Insert({updated_priority, elem});
  }

  LOG(INFO) << "Running AdjustableKaryHeap::CheckHeapProperty()";
  CHECK(heap.CheckHeapProperty());
  LOG(INFO) << "heap.CheckHeapProperty() complete";
  if (IsMaxHeap) {
    double largest = std::numeric_limits<double>::infinity();
    while (!heap.IsEmpty()) {
      const auto prio = heap.TopPriority();
      const auto idx = heap.TopIndex();
      heap.Pop();
      CHECK_LE(prio, largest);
      largest = prio;
      heap.Remove(idx);
      LOG_EVERY_POW_2(INFO)
          << "heap.Remove, heap.heap_size() = " << heap.heap_size();
    }
  } else {
    double smallest = -std::numeric_limits<double>::infinity();
    while (!heap.IsEmpty()) {
      const auto prio = heap.TopPriority();
      const auto idx = heap.TopIndex();
      heap.Pop();
      CHECK_LE(smallest, prio);
      smallest = prio;
      heap.Remove(idx);
      LOG_EVERY_POW_2(INFO)
          << "heap.Remove, heap.heap_size() = " << heap.heap_size();
    }
  }
  LOG(INFO) << "AdjustableKaryHeap is now Empty. Stress64BitClean Complete";
}

#define ADJUSTABLE_KARY_HEAP_STRESS_TEST(arity)           \
  TEST(AdjustableKAryHeapTest, Stress32Bit##arity##Max) { \
    StressTest<double, int32_t, arity, true>();           \
  }                                                       \
  TEST(AdjustableKAryHeapTest, Stress32Bit##arity##Min) { \
    StressTest<double, int32_t, arity, false>();          \
  }

ADJUSTABLE_KARY_HEAP_STRESS_TEST(2);
ADJUSTABLE_KARY_HEAP_STRESS_TEST(3);
ADJUSTABLE_KARY_HEAP_STRESS_TEST(4);
ADJUSTABLE_KARY_HEAP_STRESS_TEST(5);
ADJUSTABLE_KARY_HEAP_STRESS_TEST(6);
ADJUSTABLE_KARY_HEAP_STRESS_TEST(7);
ADJUSTABLE_KARY_HEAP_STRESS_TEST(8);
ADJUSTABLE_KARY_HEAP_STRESS_TEST(9);
ADJUSTABLE_KARY_HEAP_STRESS_TEST(10);
ADJUSTABLE_KARY_HEAP_STRESS_TEST(11);
ADJUSTABLE_KARY_HEAP_STRESS_TEST(12);
ADJUSTABLE_KARY_HEAP_STRESS_TEST(13);
ADJUSTABLE_KARY_HEAP_STRESS_TEST(14);
ADJUSTABLE_KARY_HEAP_STRESS_TEST(15);
ADJUSTABLE_KARY_HEAP_STRESS_TEST(16);

#undef ADJUSTABLE_KARY_HEAP_STRESS_TEST

}  // namespace operations_research
