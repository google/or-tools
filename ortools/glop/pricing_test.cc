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

#include "ortools/glop/pricing.h"

#include <limits>
#include <set>
#include <vector>

#include "absl/random/distributions.h"
#include "absl/random/random.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace glop {
namespace {

DEFINE_STRONG_INDEX_TYPE(TestIndex);

TEST(DynamicMaximumTest, Empty) {
  absl::BitGen random;
  DynamicMaximum<TestIndex> dynamic_max(random);
  EXPECT_EQ(dynamic_max.GetMaximum(), -1);
}

TEST(DynamicMaximumTest, SeemsToWorkAsExpected) {
  absl::BitGen random;
  DynamicMaximum<TestIndex> dynamic_max(random);
  const int n = 100;
  dynamic_max.ClearAndResize(TestIndex(n));
  EXPECT_EQ(dynamic_max.GetMaximum(), TestIndex(-1));

  std::vector<bool> in_set(n, false);
  std::vector<double> values(n, 0.0);
  for (int iter = 0; iter < 1000; ++iter) {
    const int num_updates = absl::Uniform(random, 0, n);
    for (int i = 0; i < num_updates; ++i) {
      const TestIndex index(absl::Uniform(random, 0, n));
      const double value = absl::Uniform<double>(random, 0.0, 1.0);

      if (absl::Bernoulli(random, 0.5)) {
        in_set[index.value()] = true;
        values[index.value()] = value;
        dynamic_max.AddOrUpdate(index, value);
      } else {
        in_set[index.value()] = false;
        dynamic_max.Remove(index);
      }
    }

    double expected_max = -1;
    for (int i = 0; i < n; ++i) {
      if (!in_set[i]) continue;
      if (values[i] > expected_max) {
        expected_max = values[i];
      }
    }
    if (expected_max == -1) {
      EXPECT_EQ(dynamic_max.GetMaximum(), TestIndex(-1));
    } else {
      EXPECT_EQ(values[dynamic_max.GetMaximum().value()], expected_max);
    }
  }
}

// At least for a small number (<32 currently), we get random behavior.
TEST(DynamicMaximumTest, Randomized) {
  absl::BitGen random;
  DynamicMaximum<TestIndex> dynamic_max(random);
  dynamic_max.ClearAndResize(TestIndex(100));
  EXPECT_EQ(dynamic_max.GetMaximum(), TestIndex(-1));

  const int num_added = 30;
  for (int i = 0; i < num_added; ++i) {
    dynamic_max.AddOrUpdate(TestIndex(i), 42.0);
  }

  std::set<TestIndex> seen;
  for (int i = 0; i < 1000; ++i) {
    seen.insert(dynamic_max.GetMaximum());
  }
  EXPECT_EQ(seen.size(), num_added);
}

// This cover a bug fixed at the same time as this was added.
TEST(DynamicMaximumTest, EquivalentChoiceIsProperlyCleared) {
  absl::BitGen random;
  DynamicMaximum<TestIndex> dynamic_max(random);
  dynamic_max.ClearAndResize(TestIndex(100));

  dynamic_max.AddOrUpdate(TestIndex(1), 0.0);
  dynamic_max.AddOrUpdate(TestIndex(2), 0.0);
  dynamic_max.GetMaximum();  // Fill equivalent_choices_.

  // Now there is no longer any valid choices.
  dynamic_max.Remove(TestIndex(1));
  dynamic_max.Remove(TestIndex(2));
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(dynamic_max.GetMaximum(), TestIndex(-1));
  }
}

// In some corner case, we insert a value like Square(reduced_cost) / norm
// that overflow a double. The class work fine with infinity, so we don't want
// to have issue there.
TEST(DynamicMaximumTest, WorkWithInfinity) {
  absl::BitGen random;
  DynamicMaximum<TestIndex> dynamic_max(random);
  dynamic_max.ClearAndResize(TestIndex(100));

  const double kInfinity = std::numeric_limits<double>::infinity();
  dynamic_max.AddOrUpdate(TestIndex(1), kInfinity);
  dynamic_max.AddOrUpdate(TestIndex(2), kInfinity);
  dynamic_max.AddOrUpdate(TestIndex(3), 0.0);

  const TestIndex first = dynamic_max.GetMaximum();
  dynamic_max.Remove(first);
  const TestIndex second = dynamic_max.GetMaximum();
  dynamic_max.Remove(second);

  const std::vector<TestIndex> seen = {first, second};
  EXPECT_THAT(seen,
              ::testing::UnorderedElementsAre(TestIndex(1), TestIndex(2)));

  EXPECT_EQ(dynamic_max.GetMaximum(), TestIndex(3));
}

}  // namespace
}  // namespace glop
}  // namespace operations_research
