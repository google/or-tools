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

#include "ortools/util/rev.h"

#include <cstdint>
#include <random>
#include <vector>

#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/random/distributions.h"
#include "gtest/gtest.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace {

TEST(RevRepositoryTest, BasicNonStampedBehavior) {
  RevRepository<int> repo;
  int a = 1;
  int b = 2;
  int c = 3;
  repo.SaveState(&a);  // no-op.
  a = 2;
  repo.SetLevel(0);  // no-op.
  EXPECT_EQ(a, 2);
  repo.SetLevel(1);
  repo.SaveState(&a);
  repo.SaveState(&b);
  b = 13;
  repo.SaveState(&b);
  repo.SaveState(&c);
  a = 12;
  c = 14;
  repo.SetLevel(10);
  repo.SaveState(&a);
  a = 20;
  repo.SetLevel(11);
  EXPECT_EQ(a, 20);
  repo.SetLevel(10);
  EXPECT_EQ(a, 20);
  repo.SetLevel(9);
  EXPECT_EQ(a, 12);
  repo.SetLevel(1);
  EXPECT_EQ(a, 12);
  EXPECT_EQ(b, 13);
  EXPECT_EQ(c, 14);
  repo.SetLevel(0);
  EXPECT_EQ(a, 2);
  EXPECT_EQ(b, 2);
  EXPECT_EQ(c, 3);
}

TEST(RevRepositoryTest, NoDiffWithStampedBehavior) {
  RevRepository<int> repo1;
  RevRepository<int> repo2;
  std::vector<int> vars1(10);
  std::vector<int> vars2(10);
  std::vector<int64_t> stamps2(10);
  std::mt19937 random(12345);
  for (int num_level_changes = 0; num_level_changes < 1000;
       ++num_level_changes) {
    const int level = absl::Uniform(random, 0, 100);
    repo1.SetLevel(level);
    repo2.SetLevel(level);
    for (int i = 0; i < vars1.size(); ++i) {
      ASSERT_EQ(vars1[i], vars2[i]);
      repo1.SaveState(&vars1[i]);
    }
    for (int num_random_updates = 0; num_random_updates < 100;
         ++num_random_updates) {
      const int i = absl::Uniform<int>(random, 0, vars1.size());
      const int value = absl::Uniform(random, 0, 1000);
      vars1[i] = value;
      repo2.SaveStateWithStamp(&vars2[i], &stamps2[i]);
      vars2[i] = value;
    }
  }
}

DEFINE_STRONG_INDEX_TYPE(TestType);

TEST(RevVectorTest, BasicBehavior) {
  RevVector<TestType, int> v;
  v.Grow(100);

  EXPECT_EQ(v[TestType(0)], 0);
  EXPECT_EQ(v[TestType(10)], 0);

  v.SetLevel(3);
  v.MutableRef(TestType(10)) = 4;
  EXPECT_EQ(v[TestType(10)], 4);

  v.SetLevel(6);
  v.MutableRef(TestType(10)) = 6;
  EXPECT_EQ(v[TestType(10)], 6);

  v.SetLevel(7);
  EXPECT_EQ(v[TestType(10)], 6);

  v.SetLevel(5);
  EXPECT_EQ(v[TestType(10)], 4);

  v.SetLevel(0);
  EXPECT_EQ(v[TestType(10)], 0);
}

TEST(RevMapTest, LikeMapWhenNoBacktracking) {
  RevMap<absl::flat_hash_map<int, int>> m;
  EXPECT_FALSE(m.contains(4));
  m.Set(4, 5);
  EXPECT_TRUE(m.contains(4));
  EXPECT_EQ(5, m.at(4));
  m.Set(4, 6);
  EXPECT_TRUE(m.contains(4));
  EXPECT_EQ(6, m.at(4));
  m.EraseOrDie(4);
  EXPECT_FALSE(m.contains(4));
}

TEST(RevMapDeathTest, EraseOrDie) {
  RevMap<absl::flat_hash_map<int, int>> m;
  EXPECT_FALSE(m.contains(4));
  m.Set(4, 5);
  m.EraseOrDie(4);
  EXPECT_DEATH(m.EraseOrDie(4), "key not present: '4'.");
}

TEST(RevMapTest, InsertDeleteInsertAndBacktrack) {
  RevMap<absl::btree_map<int, int>> m;
  EXPECT_FALSE(m.contains(4));

  m.SetLevel(1);
  EXPECT_FALSE(m.contains(4));
  m.Set(4, 5);
  EXPECT_TRUE(m.contains(4));

  m.SetLevel(4);
  EXPECT_TRUE(m.contains(4));
  m.EraseOrDie(4);
  EXPECT_FALSE(m.contains(4));

  // The level do not need to be contiguous, but the implementation assumes
  // they are mostly dense from an efficienty point of view.
  m.SetLevel(10);
  EXPECT_FALSE(m.contains(4));
  m.Set(4, 6);
  EXPECT_TRUE(m.contains(4));
  EXPECT_EQ(6, m.at(4));
  m.Set(4, 7);
  EXPECT_TRUE(m.contains(4));
  EXPECT_EQ(7, m.at(4));

  // Backtrack
  m.SetLevel(4);
  EXPECT_FALSE(m.contains(4));

  m.SetLevel(1);
  EXPECT_TRUE(m.contains(4));
  EXPECT_EQ(5, m.at(4));

  m.SetLevel(0);
  EXPECT_FALSE(m.contains(4));

  // Note that the higher level info is lost:
  m.SetLevel(10);
  EXPECT_FALSE(m.contains(4));
}

TEST(RevMapTest, ManySetsOperations) {
  std::mt19937 random(12345);
  const int kRange = 10000;

  RevMap<absl::flat_hash_map<int, int>> m;
  absl::flat_hash_map<int, int> reference;

  m.SetLevel(1);
  for (int i = 0; i < 10000; ++i) {
    const int key = absl::Uniform(random, 0, kRange);
    const int value = absl::Uniform(random, 0, kRange);
    m.Set(key, value);
    reference[key] = value;
  }
  EXPECT_EQ(m.size(), reference.size());

  // More insert at level 2.
  m.SetLevel(2);
  for (int i = 0; i < 10000; ++i) {
    m.Set(absl::Uniform(random, 0, kRange), absl::Uniform(random, 0, kRange));
  }
  EXPECT_GT(m.size(), reference.size());

  // Backtrack to level 1, m and reference should be the same.
  m.SetLevel(1);
  EXPECT_EQ(m.size(), reference.size());
  for (const auto entry : reference) {
    EXPECT_EQ(entry.second, m.at(entry.first));
  }

  m.SetLevel(0);
  EXPECT_TRUE(m.empty());
}

TEST(RevGrowingMultiMapTest, BasicTest) {
  RevGrowingMultiMap<int, int> map;
  map.Add(0, 1);
  map.Add(0, 2);
  map.SetLevel(1);
  map.Add(0, 2);
  map.Add(1, 3);
  EXPECT_EQ(map.Values(0), std::vector<int>({1, 2, 2}));
  EXPECT_EQ(map.Values(1), std::vector<int>({3}));
  map.SetLevel(0);
  EXPECT_EQ(map.Values(0), std::vector<int>({1, 2}));
  EXPECT_EQ(map.Values(1), std::vector<int>({}));
}

}  // namespace
}  // namespace operations_research
