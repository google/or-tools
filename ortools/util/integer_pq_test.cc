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

#include "ortools/util/integer_pq.h"

#include <algorithm>
#include <random>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/random/random.h"
#include "gtest/gtest.h"

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

TEST(IntegerPriorityQueue, RandomTest) {
  std::mt19937 random(12345);
  for (int n = 1; n < 1000; ++n) {
    IntegerPriorityQueue<IntegerPqElement> int_pq(n);
    std::vector<std::pair<int, int>> to_be_sorted(n);
    for (int e = 0; e < n; ++e) {
      // This test requires that there is no identical priority, otherwise,
      // the order may not be the same with the sort().
      const int priority = absl::Uniform(random, 0, 1 << 30);
      to_be_sorted[e].first = priority;
      to_be_sorted[e].second = e;
      int_pq.Add(IntegerPqElement(e, priority));
    }

    // More changes.
    for (int i = 0; i < n; ++i) {
      const int e = absl::Uniform(random, 0, n);
      const int priority = absl::Uniform(random, 0, 1 << 30);
      to_be_sorted[e].first = priority;
      int_pq.ChangePriority(IntegerPqElement(e, priority));
    }

    int i = 0;
    std::sort(to_be_sorted.begin(), to_be_sorted.end());
    std::reverse(to_be_sorted.begin(), to_be_sorted.end());
    while (!int_pq.IsEmpty()) {
      CHECK_LT(i, n);
      const int top = int_pq.Top().Index();
      ASSERT_EQ(to_be_sorted[i].first, int_pq.Top().p);
      ASSERT_EQ(to_be_sorted[i].second, top);
      int_pq.Pop();
      ++i;
    }
    ASSERT_EQ(i, n);
  }
}

}  // namespace
}  // namespace operations_research
