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

#include "ortools/graph_base/iterators.h"

#include <cstdint>
#include <iterator>
#include <vector>

#include "gtest/gtest.h"
#include "ortools/base/strong_int.h"

namespace util {
namespace {

DEFINE_STRONG_INT_TYPE(TestIndex, int64_t);

#if __cplusplus >= 202002L
static_assert(std::random_access_iterator<IntegerRangeIterator<int>>);
static_assert(std::random_access_iterator<IntegerRangeIterator<TestIndex>>);
#endif  // __cplusplus >= 202002L

TEST(IntegerRangeTest, VariousEmptyRanges) {
  bool went_inside = false;
  for ([[maybe_unused]] const int i : IntegerRange<int>(0, 0)) {
    went_inside = true;
  }
  for ([[maybe_unused]] const int i : IntegerRange<int>(10, 10)) {
    went_inside = true;
  }
  for ([[maybe_unused]] const int i : IntegerRange<int>(-10, -10)) {
    went_inside = true;
  }
  EXPECT_FALSE(went_inside);
}

TEST(IntegerRangeTest, NormalBehavior) {
  int reference_index = 0;
  for (const int i : IntegerRange<int>(0, 100)) {
    EXPECT_EQ(i, reference_index);
    ++reference_index;
  }
  EXPECT_EQ(reference_index, 100);
}

TEST(IntegerRangeTest, NormalBehaviorWithIntType) {
  TestIndex reference_index(0);
  for (const TestIndex i :
       IntegerRange<TestIndex>(TestIndex(0), TestIndex(100))) {
    EXPECT_EQ(i, reference_index);
    ++reference_index;
  }
  EXPECT_EQ(reference_index, TestIndex(100));
}

TEST(IntegerRangeTest, AssignToVector) {
  static const int kRangeSize = 100;
  IntegerRange<int> range(0, kRangeSize);
  ASSERT_EQ(kRangeSize, range.size());
  std::vector<int> vector_from_range(range.begin(), range.end());
  ASSERT_EQ(kRangeSize, vector_from_range.size());
  for (int i = 0; i < kRangeSize; ++i) {
    EXPECT_EQ(i, vector_from_range[i]);
  }
}

TEST(ChasingIteratorTest, ChasingIterator) {
  static constexpr int kSentinel = 42;
  struct Tag {};
  using Iterator = ChasingIterator<int, kSentinel, Tag>;
  const auto end = Iterator{};
#if __cplusplus >= 202002L
  static_assert(std::forward_iterator<Iterator>);
#endif
  // There are two chains: 0->1->3 and 4->2.
  const int next[] = {1, 3, kSentinel, kSentinel, 2};
  {
    Iterator it(0, next);
    ASSERT_FALSE(it == end);
    EXPECT_EQ(*it, 0);
    ++it;
    ASSERT_FALSE(it == end);
    EXPECT_EQ(*it, 1);
    ++it;
    ASSERT_FALSE(it == end);
    EXPECT_EQ(*it, 3);
    ++it;
    ASSERT_TRUE(it == end);
  }
  {
    Iterator it(1, next);
    ASSERT_FALSE(it == end);
    EXPECT_EQ(*it, 1);
    ++it;
    ASSERT_FALSE(it == end);
    EXPECT_EQ(*it, 3);
    ++it;
    ASSERT_TRUE(it == end);
  }
  {
    Iterator it(2, next);
    ASSERT_FALSE(it == end);
    EXPECT_EQ(*it, 2);
    ++it;
    ASSERT_TRUE(it == end);
  }
  {
    Iterator it(3, next);
    ASSERT_FALSE(it == end);
    EXPECT_EQ(*it, 3);
    ++it;
    ASSERT_TRUE(it == end);
  }
  {
    Iterator it(4, next);
    ASSERT_FALSE(it == end);
    EXPECT_EQ(*it, 4);
    ++it;
    ASSERT_FALSE(it == end);
    EXPECT_EQ(*it, 2);
    ++it;
    ASSERT_TRUE(it == end);
  }
}

TEST(IntegerRangeTest, AssignToVectorOfIntType) {
  static const int kRangeSize = 100;
  IntegerRange<TestIndex> range(TestIndex(0), TestIndex(kRangeSize));
  std::vector<TestIndex> vector_from_range(range.begin(), range.end());
  ASSERT_EQ(kRangeSize, vector_from_range.size());
  for (int i = 0; i < kRangeSize; ++i) {
    EXPECT_EQ(TestIndex(i), vector_from_range[i]);
  }
}

TEST(ReverseTest, EmptyVector) {
  std::vector<int> test_vector;
  bool went_inside = false;
  for ([[maybe_unused]] const int value : Reverse(test_vector)) {
    went_inside = true;
  }
  EXPECT_FALSE(went_inside);
}

TEST(ReverseTest, ReverseOfAVector) {
  const int kSize = 10000;
  std::vector<int> test_vector;
  for (int i = 0; i < kSize; ++i) {
    test_vector.push_back(5 * i + 5);
  }
  int index = 0;
  for (int value : Reverse(test_vector)) {
    EXPECT_EQ(test_vector[kSize - 1 - index], value);
    index++;
  }
  // Same with references.
  index = 0;
  for (const int& value : Reverse(test_vector)) {
    EXPECT_EQ(test_vector[kSize - 1 - index], value);
    index++;
  }
}

}  // namespace
}  // namespace util
