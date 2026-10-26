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

#include <queue>
#include <random>
#include <vector>

#include "absl/random/random.h"
#include "benchmark/benchmark.h"
#include "ortools/base/adjustable_priority_queue.h"
#include "ortools/util/integer_pq.h"

namespace operations_research {
namespace {

// Element for IntegerPriorityQueue and std::priority_queue<>.
struct IntegerPqElement {
  IntegerPqElement() {}
  IntegerPqElement(int index, int priority) : i(index), p(priority) {}
  int Index() const { return i; }
  bool operator<(const IntegerPqElement& o) const { return p < o.p; }

  int i;
  int p;
};

// Element used for AdjustablePriorityQueue.
struct AdjustablePqElement {
  void SetHeapIndex(int h) { heap_index = h; }
  int GetHeapIndex() const { return heap_index; }
  bool operator<(const AdjustablePqElement& other) const {
    return priority < other.priority;
  }
  int heap_index = -1;
  int priority = 0;
};

static void BM_IntegerPQ_Updates(benchmark::State& state) {
  const int n = state.range(0);
  std::mt19937 random(12345);
  IntegerPriorityQueue<IntegerPqElement> int_pq(n);
  for (int e = 0; e < n; ++e) int_pq.Add(IntegerPqElement(e, 0));

  for (auto _ : state) {
    const int e = absl::Uniform(random, 0, n);
    const int priority = absl::Uniform(random, 0, 1 << 30);
    int_pq.ChangePriority(IntegerPqElement(e, priority));
  }
  state.SetBytesProcessed(state.iterations());
}

static void BM_AdjustablePQ_Updates(benchmark::State& state) {
  const int n = state.range(0);
  std::mt19937 random(12345);
  std::vector<AdjustablePqElement> queue_elements(n);
  AdjustablePriorityQueue<AdjustablePqElement> pq;
  for (int e = 0; e < n; ++e) {
    queue_elements[e].priority = 0;
    pq.Add(&queue_elements[e]);
  }

  for (auto _ : state) {
    const int e = absl::Uniform(random, 0, n);
    const int priority = absl::Uniform(random, 0, 1 << 30);
    queue_elements[e].priority = priority;
    pq.NoteChangedPriority(&queue_elements[e]);
  }
  state.SetBytesProcessed(state.iterations());
}

static void BM_STL_AddPop(benchmark::State& state) {
  const int n = state.range(0);
  std::mt19937 random(12345);
  std::priority_queue<IntegerPqElement> pq;
  for (auto _ : state) {
    for (int e = 0; e < n; ++e) {
      pq.push(IntegerPqElement(e, absl::Uniform(random, 0, 1 << 30)));
    }
    for (int e = 0; e < n; ++e) pq.pop();
  }
  state.SetBytesProcessed(state.iterations() * n);
}

static void BM_IntegerPQ_AddPop(benchmark::State& state) {
  const int n = state.range(0);
  std::mt19937 random(12345);
  IntegerPriorityQueue<IntegerPqElement> int_pq(n);

  for (auto _ : state) {
    for (int e = 0; e < n; ++e) {
      int_pq.Add(IntegerPqElement(e, absl::Uniform(random, 0, 1 << 30)));
    }
    for (int e = 0; e < n; ++e) int_pq.Pop();
  }
  state.SetBytesProcessed(state.iterations() * n);
}

static void BM_AdjustablePQ_AddPop(benchmark::State& state) {
  const int n = state.range(0);
  std::mt19937 random(12345);
  std::vector<AdjustablePqElement> queue_elements(n);
  AdjustablePriorityQueue<AdjustablePqElement> pq;

  for (auto _ : state) {
    for (int e = 0; e < n; ++e) {
      queue_elements[e].priority = absl::Uniform(random, 0, 1 << 30);
      pq.Add(&queue_elements[e]);
    }
    for (int e = 0; e < n; ++e) pq.Pop();
  }
  state.SetBytesProcessed(state.iterations() * n);
}

BENCHMARK(BM_IntegerPQ_Updates)->Range(1, 1 << 20);
BENCHMARK(BM_AdjustablePQ_Updates)->Range(1, 1 << 20);

BENCHMARK(BM_STL_AddPop)->Range(1, 1 << 20);
BENCHMARK(BM_IntegerPQ_AddPop)->Range(1, 1 << 20);
BENCHMARK(BM_AdjustablePQ_AddPop)->Range(1, 1 << 20);

}  // namespace
}  // namespace operations_research
