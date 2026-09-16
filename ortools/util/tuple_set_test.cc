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

#include "ortools/util/tuple_set.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "gtest/gtest.h"

namespace operations_research {
namespace {

TEST(IntTupleSetTest, SimpleFlow) {
  IntTupleSet set(3);
  set.Insert3(1, 3, 5);
  set.Insert3(1, 3, 5);
  set.Insert3(1, 3, 6);
  set.Insert3(2, 4, 5);
  set.Insert3(2, 4, 7);
  EXPECT_EQ(4, set.NumTuples());
  EXPECT_EQ(3, set.Arity());
  std::vector<int64_t> tuple1 = {1, 3, 6};
  std::vector<int> tuple2 = {1, 3, 6};
  std::vector<int64_t> tuple3 = {1, 3, 16};
  std::vector<int> tuple4 = {1, 3, 16};
  EXPECT_TRUE(set.Contains(tuple1));
  EXPECT_TRUE(set.Contains(tuple2));
  EXPECT_FALSE(set.Contains(tuple3));
  EXPECT_FALSE(set.Contains(tuple4));
}

TEST(IntTupleSetTest, OneCopy) {
  IntTupleSet set(3);
  set.Insert3(1, 3, 5);
  set.Insert3(1, 3, 5);
  set.Insert3(1, 3, 6);
  // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
  IntTupleSet set2(set);
  EXPECT_EQ(2, set2.NumTuples());
  EXPECT_EQ(3, set2.Arity());
  std::vector<int64_t> tuple1 = {1, 3, 6};
  std::vector<int64_t> tuple2 = {1, 3, 16};
  EXPECT_TRUE(set2.Contains(tuple1));
  EXPECT_FALSE(set2.Contains(tuple2));
}

TEST(IntTupleSetTest, OneCopyWithEqual) {
  IntTupleSet set(3);
  set.Insert3(1, 3, 5);
  set.Insert3(1, 3, 5);
  set.Insert3(1, 3, 6);
  // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
  IntTupleSet set2 = set;
  EXPECT_EQ(2, set2.NumTuples());
  EXPECT_EQ(3, set2.Arity());
  std::vector<int64_t> tuple1 = {1, 3, 6};
  std::vector<int64_t> tuple2 = {1, 3, 16};
  EXPECT_TRUE(set2.Contains(tuple1));
  EXPECT_FALSE(set2.Contains(tuple2));
}

TEST(IntTupleSetTest, CopyAndBranchOnOriginal) {
  IntTupleSet set(3);
  set.Insert3(1, 3, 5);
  set.Insert3(1, 3, 5);
  set.Insert3(1, 3, 6);
  // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
  IntTupleSet set2(set);
  set.Insert3(3, 3, 3);
  EXPECT_EQ(3, set.NumTuples());
  EXPECT_EQ(2, set2.NumTuples());
  std::vector<int64_t> tuple1 = {3, 3, 3};
  EXPECT_TRUE(set.Contains(tuple1));
  EXPECT_FALSE(set2.Contains(tuple1));
}

TEST(IntTupleSetTest, CopyAndBranchOnCopy) {
  IntTupleSet set(3);
  set.Insert3(1, 3, 5);
  set.Insert3(1, 3, 5);
  set.Insert3(1, 3, 6);
  // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
  IntTupleSet set2(set);
  set2.Insert3(3, 3, 3);
  EXPECT_EQ(2, set.NumTuples());
  EXPECT_EQ(3, set2.NumTuples());
  std::vector<int64_t> tuple1 = {3, 3, 3};
  EXPECT_FALSE(set.Contains(tuple1));
  EXPECT_TRUE(set2.Contains(tuple1));
}

TEST(IntTupleSetTest, MultipleCopiesAndDeletions) {
  std::unique_ptr<IntTupleSet> set(new IntTupleSet(3));
  set->Insert3(1, 3, 5);
  set->Insert3(1, 3, 5);
  set->Insert3(2, 4, 6);
  std::unique_ptr<IntTupleSet> set2(new IntTupleSet(*set));
  std::unique_ptr<IntTupleSet> set3(new IntTupleSet(*set2));
  set2->Insert3(3, 3, 3);

  std::vector<int64_t> tuple1 = {3, 3, 3};
  EXPECT_EQ(2, set->NumTuples());
  EXPECT_EQ(3, set2->NumTuples());
  EXPECT_EQ(2, set3->NumTuples());
  EXPECT_FALSE(set->Contains(tuple1));
  EXPECT_TRUE(set2->Contains(tuple1));
  EXPECT_FALSE(set3->Contains(tuple1));

  // Delete 'set'
  set.reset(nullptr);
  EXPECT_EQ(3, set2->NumTuples());
  EXPECT_EQ(2, set3->NumTuples());
  EXPECT_TRUE(set2->Contains(tuple1));
  EXPECT_FALSE(set3->Contains(tuple1));

  // Delete 'set3'
  set3.reset(nullptr);
  EXPECT_EQ(3, set2->NumTuples());
  EXPECT_TRUE(set2->Contains(tuple1));
}

TEST(IntTupleSetTest, ZeroArity) {
  IntTupleSet set(0);
  EXPECT_EQ(0, set.Arity());
  EXPECT_EQ(0, set.NumTuples());
  std::vector<int> empty_tuple;
  EXPECT_FALSE(set.Contains(empty_tuple));
  EXPECT_EQ(0, set.Insert(empty_tuple));
  EXPECT_EQ(1, set.NumTuples());
  EXPECT_TRUE(set.Contains(empty_tuple));
  EXPECT_EQ(-1, set.Insert(empty_tuple));
}

TEST(IntTupleSetTest, NumDifferentValuesInColumn) {
  IntTupleSet set(3);
  set.Insert3(1, 3, 5);
  set.Insert3(1, 3, 5);
  set.Insert3(1, 3, 6);
  set.Insert3(2, 4, 5);
  set.Insert3(2, 4, 7);
  EXPECT_EQ(2, set.NumDifferentValuesInColumn(0));
  EXPECT_EQ(2, set.NumDifferentValuesInColumn(1));
  EXPECT_EQ(3, set.NumDifferentValuesInColumn(2));
  EXPECT_EQ(0, set.NumDifferentValuesInColumn(-1));
  EXPECT_EQ(0, set.NumDifferentValuesInColumn(3));
}

TEST(IntTupleSetTest, SortedTupleSet) {
  IntTupleSet set(3);
  set.Insert3(1, 2, 5);
  set.Insert3(1, 4, 5);
  set.Insert3(1, 5, 6);
  set.Insert3(2, 3, 5);
  set.Insert3(2, 0, 7);
  EXPECT_EQ(5, set.NumDifferentValuesInColumn(1));
  IntTupleSet sorted = set.SortedByColumn(1);
  EXPECT_EQ(0, sorted.Value(0, 1));
  EXPECT_EQ(2, sorted.Value(1, 1));
  EXPECT_EQ(3, sorted.Value(2, 1));
  EXPECT_EQ(4, sorted.Value(3, 1));
  EXPECT_EQ(5, sorted.Value(4, 1));
  EXPECT_EQ(2, set.Value(0, 1));
  EXPECT_EQ(4, set.Value(1, 1));
  EXPECT_EQ(5, set.Value(2, 1));
  EXPECT_EQ(3, set.Value(3, 1));
  EXPECT_EQ(0, set.Value(4, 1));
}

TEST(IntTupleSetTest, SortedTupleSetStability) {
  IntTupleSet set(3);
  set.Insert3(1, 5, 6);
  set.Insert3(2, 3, 5);
  set.Insert3(2, 0, 7);
  set.Insert3(1, 4, 5);
  set.Insert3(1, 2, 5);
  EXPECT_EQ(5, set.NumDifferentValuesInColumn(1));
  IntTupleSet sorted = set.SortedByColumn(0);
  EXPECT_EQ(5, sorted.Value(0, 1));
  EXPECT_EQ(4, sorted.Value(1, 1));
  EXPECT_EQ(2, sorted.Value(2, 1));
  EXPECT_EQ(3, sorted.Value(3, 1));
  EXPECT_EQ(0, sorted.Value(4, 1));
}

void Expect3(const IntTupleSet& set, int index, int64_t v0, int64_t v1,
             int64_t v2) {
  EXPECT_EQ(v0, set.Value(index, 0));
  EXPECT_EQ(v1, set.Value(index, 1));
  EXPECT_EQ(v2, set.Value(index, 2));
}

TEST(IntTupleSetTest, LexicographicallySortedTupleSet) {
  IntTupleSet set(3);
  set.Insert3(1, 2, 5);
  set.Insert3(1, 4, 6);
  set.Insert3(1, 5, 6);
  set.Insert3(1, 4, 5);
  set.Insert3(2, 3, 5);
  set.Insert3(2, 0, 7);
  IntTupleSet sorted = set.SortedLexicographically();
  Expect3(sorted, 0, 1, 2, 5);
  Expect3(sorted, 1, 1, 4, 5);
  Expect3(sorted, 2, 1, 4, 6);
  Expect3(sorted, 3, 1, 5, 6);
  Expect3(sorted, 4, 2, 0, 7);
  Expect3(sorted, 5, 2, 3, 5);
}

TEST(IntTupleSetTest, InsertReturnValue) {
  IntTupleSet set(2);
  EXPECT_EQ(0, set.Insert2(0, 1));
  EXPECT_EQ(1, set.Insert2(1, 0));
  EXPECT_EQ(-1, set.Insert2(0, 1));
  EXPECT_EQ(2, set.Insert2(1, 1));
  EXPECT_EQ(-1, set.Insert2(1, 0));
}

TEST(IntTupleSetDeathTest, WrongArity) {
  IntTupleSet set(3);
  EXPECT_DEATH(set.Insert2(2, 4), "");
  EXPECT_DEATH(set.Insert4(2, 4, 4, 0), "");
}
}  // namespace
}  // namespace operations_research
