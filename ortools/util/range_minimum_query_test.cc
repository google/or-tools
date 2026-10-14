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

#include "ortools/util/range_minimum_query.h"

#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "ortools/base/log_severity.h"
#include "ortools/base/types.h"

namespace operations_research {
namespace {

// Class with the same interface as RangeMinimumQuery, except it computes
// answers to queries with a straightforward linear scan,
// and does not die on queries with length <= 0 or are cross-sequence.
template <typename T, typename Compare = std::less<T>>
class LinearRangeMinQuery {
 public:
  LinearRangeMinQuery() : cmp_() {}

  void Clear() {
    values_.clear();
    num_values_in_tables_ = 0;
  }

  int TableSize() const { return num_values_in_tables_; }

  void MakeTableFromNewElements() { num_values_in_tables_ = values_.size(); }

  void PushBack(const T& value) { values_.push_back(value); }

  T RangeMinimum(int range_begin_index, int range_end_index) const {
    T min = values_[range_begin_index];
    for (int i = range_begin_index + 1; i < range_end_index; ++i) {
      min = std::min(min, values_[i], cmp_);
    }
    return min;
  }

 private:
  std::vector<T> values_;
  const Compare cmp_;
  int num_values_in_tables_ = 0;
};

// Test both LinearRangeMinQuery and RangeMinimumQuery with
// hard values, further tests will compare the results of RangeMinimumQuery
// against the simpler LinearRangeMinQuery.
TEST(FixedValuesRangeMinimumQueryTest, SimpleUseCaseGreater) {
  LinearRangeMinQuery<int64_t, std::greater<int64_t>> lrmq;
  RangeMinimumQuery<int64_t, std::greater<int64_t>> rmq;

  EXPECT_EQ(lrmq.TableSize(), 0);
  EXPECT_EQ(rmq.TableSize(), 0);
  // Making a rmq with no elements must not crash.
  rmq.MakeTableFromNewElements();

  const std::vector<int64_t> values1 = {4, 6, 1, 3};
  for (int i = 0; i < values1.size(); ++i) {
    lrmq.PushBack(values1[i]);
    rmq.PushBack(values1[i]);
  }
  EXPECT_EQ(lrmq.TableSize(), 0);
  EXPECT_EQ(rmq.TableSize(), 0);
  lrmq.MakeTableFromNewElements();
  rmq.MakeTableFromNewElements();
  // Making an rmq with no elements must not crash.
  rmq.MakeTableFromNewElements();
  EXPECT_EQ(lrmq.TableSize(), values1.size());
  EXPECT_EQ(rmq.TableSize(), values1.size());
  // Largest element of all is 6.
  EXPECT_EQ(lrmq.RangeMinimum(0, values1.size()), 6);
  EXPECT_EQ(rmq.RangeMinimum(0, values1.size()), 6);
  // Largest of {1, 3} is 3.
  EXPECT_EQ(lrmq.RangeMinimum(2, 4), 3);
  EXPECT_EQ(rmq.RangeMinimum(2, 4), 3);
  // 1-sized ranges.
  for (int i = 0; i < values1.size(); ++i) {
    EXPECT_EQ(lrmq.RangeMinimum(i, i + 1), values1[i]);
    EXPECT_EQ(rmq.RangeMinimum(i, i + 1), values1[i]);
  }

  // Add another range, check that both ranges can be queried.
  // Then remove previous range and recheck.
  for (int iteration = 0; iteration < 2; ++iteration) {
    const int size1 = lrmq.TableSize();
    const std::vector<int64_t> values2 = {-4, 2, -7, -9};
    for (int i = 0; i < values2.size(); ++i) {
      lrmq.PushBack(values2[i]);
      rmq.PushBack(values2[i]);
    }
    EXPECT_EQ(lrmq.TableSize(), iteration == 0 ? values1.size() : 0);
    EXPECT_EQ(rmq.TableSize(), iteration == 0 ? values1.size() : 0);
    lrmq.MakeTableFromNewElements();
    rmq.MakeTableFromNewElements();
    EXPECT_EQ(lrmq.TableSize(), size1 + values2.size());
    EXPECT_EQ(rmq.TableSize(), size1 + values2.size());

    EXPECT_EQ(lrmq.RangeMinimum(size1 + 1, size1 + 3), 2);
    EXPECT_EQ(rmq.RangeMinimum(size1 + 1, size1 + 3), 2);
    // Clear data structures, next iteration will re-add second range.
    lrmq.Clear();
    rmq.Clear();
  }
}

using ValueSequences = std::vector<std::vector<int64_t>>;

class LinearRangeMinQueryTest
    : public ::testing::TestWithParam<ValueSequences> {
 public:
  void SetUp() override {
    for (const auto& sequence : GetParam()) {
      const int begin_index = rmq_.TableSize();
      for (const int64_t value : sequence) {
        lrmq_.PushBack(value);
        rmq_.PushBack(value);
      }
      lrmq_.MakeTableFromNewElements();
      rmq_.MakeTableFromNewElements();

      queries_.push_back({
          .begin_index = begin_index,
          .end_index = rmq_.TableSize(),
      });
      CheckAllRangeQueries(lrmq_, rmq_, queries_.back());
    }
  }

  void CheckAllSequences() {
    for (const SequenceQueries& query : queries_) {
      CheckAllRangeQueries(lrmq_, rmq_, query);
    }
  }

 private:
  struct SequenceQueries {
    int begin_index;
    int end_index;
  };
  std::vector<SequenceQueries> queries_;

  void CheckAllRangeQueries(const LinearRangeMinQuery<int64_t>& lrmq,
                            const RangeMinimumQuery<int64_t>& rmq,
                            const SequenceQueries& queries) {
    for (int i = queries.begin_index; i < queries.end_index; ++i) {
      for (int j = i + 1; j <= queries.end_index; ++j) {
        EXPECT_EQ(lrmq.RangeMinimum(i, j), rmq.RangeMinimum(i, j));
      }
    }
  }

  LinearRangeMinQuery<int64_t> lrmq_;
  RangeMinimumQuery<int64_t> rmq_;
};

TEST_P(LinearRangeMinQueryTest, CheckAll) { CheckAllSequences(); }

INSTANTIATE_TEST_SUITE_P(
    ComparingWithStraightForwardImplementation, LinearRangeMinQueryTest,
    ::testing::Values(
        ValueSequences{{0, 7, 2}, {1, 7, 4, 2, 6, 1}, {2, 7, 3, 6, 6, 3}},
        ValueSequences{{2, 4, 1, 6, 3, 5, 7, 4, 2, 2, 1, 5, 7, 2, 6}},
        ValueSequences{{6, 4, 1, 3, 6, 1, 7, 4, 2}},
        ValueSequences{{0, 7, 2, 1, 7, 4, 2, 6, 1, 2, 7, 3, 6, 6, 3}},
        ValueSequences{{kint64min, kint64min + 1}},
        ValueSequences{{kint64max, kint64max - 1}}));

TEST(RangeMinimumQueryDeathTest, EmptyRange) {
  if constexpr (!DEBUG_MODE) GTEST_SKIP() << "Skipped in opt mode";
  RangeMinimumQuery<int64_t> rmq;
  rmq.PushBack(1);
  rmq.PushBack(2);
  rmq.MakeTableFromNewElements();
  EXPECT_DEBUG_DEATH(rmq.RangeMinimum(1, 1), ".*");
}

TEST(RangeMinimumQueryDeathTest, ReversedRange) {
  if constexpr (!DEBUG_MODE) GTEST_SKIP() << "Skipped in opt mode";
  RangeMinimumQuery<int64_t> rmq;
  rmq.PushBack(1);
  rmq.PushBack(2);
  rmq.MakeTableFromNewElements();
  EXPECT_DEBUG_DEATH(rmq.RangeMinimum(1, 0), ".*");
}

TEST(RangeMinimumQueryDeathTest, EndIndexWithinSequence) {
  if constexpr (!DEBUG_MODE) GTEST_SKIP() << "Skipped in opt mode";
  RangeMinimumQuery<int64_t> rmq;
  rmq.PushBack(1);
  rmq.PushBack(2);
  rmq.MakeTableFromNewElements();
  EXPECT_EQ(rmq.RangeMinimum(1, 2), 2);
  EXPECT_DEBUG_DEATH(rmq.RangeMinimum(1, 3), ".*");
}

TEST(RangeMinimumQueryDeathTest, NoPendingNewElementOnQuery) {
  if constexpr (!DEBUG_MODE) GTEST_SKIP() << "Skipped in opt mode";
  RangeMinimumQuery<int64_t> rmq;
  rmq.PushBack(1);
  rmq.PushBack(2);
  rmq.MakeTableFromNewElements();
  rmq.PushBack(3);
  rmq.PushBack(4);
  EXPECT_DEBUG_DEATH(rmq.RangeMinimum(2, 4), ".*");
}

class RangeMinimumQueryTest : public testing::Test {
 protected:
  // Builds an rmq from the array and tests all {n choose 2} possible queries
  // against a trivial linear scan. Since the complexity is O(n^3), do not
  // use this function with arrays of length more than 5.
  template <typename T, typename Compare = std::less<T>>
  void ConstructAndLinearlyTestAllRanges(std::vector<T> array);

  // Same as above, but creates a table to compute all queries, and then
  // compares the table with the results from an rmq. The complexity is
  // O(n^2), so this method can be used for arrays of length 10.
  template <typename T, typename Compare = std::less<T>>
  void ConstructAndTestAllRangesWithDP(std::vector<T> array);
};

template <typename T, typename Compare>
void RangeMinimumQueryTest::ConstructAndLinearlyTestAllRanges(
    std::vector<T> array) {
  Compare cmp;
  const RangeMinimumQuery<T, Compare> rmq(std::move(array));
  for (int from = 0; from < rmq.array().size() - 1; ++from) {
    for (int to = from + 1; to < rmq.array().size(); ++to) {
      T cumul = rmq.array()[from];
      for (int i = from + 1; i < to; ++i) {
        cumul = std::min(cumul, rmq.array()[i], cmp);
      }
      EXPECT_EQ(cumul, rmq.RangeMinimum(from, to));
    }
  }
}

template <typename T, typename Compare>
void RangeMinimumQueryTest::ConstructAndTestAllRangesWithDP(
    std::vector<T> array) {
  Compare cmp;
  const RangeMinimumQuery<T, Compare> rmq(std::move(array));
  absl::flat_hash_map<std::pair<int, int>, T> table;

  for (int i = 0; i < rmq.array().size(); ++i) {
    table[{i, i + 1}] = rmq.array()[i];
  }
  for (int delta = 2; delta < rmq.array().size(); ++delta) {
    for (int from = 0; from + delta <= rmq.array().size(); ++from) {
      table[{from, from + delta}] = std::min(
          table[{from, from + delta - 1}], rmq.array()[from + delta - 1], cmp);
      T expected = table[{from, from + delta}];
      T actual = rmq.RangeMinimum(from, from + delta);
      EXPECT_EQ(expected, actual);
    }
  }
}

TEST_F(RangeMinimumQueryTest, GeneralBasicTest) {
  const std::vector<int> array = {-21, 53,  -58, -90, 48,  -56, -100,
                                  33,  -96, 86,  37,  -91, -57, 43,
                                  6,   32,  -90, 21,  -31, 61};
  const RangeMinimumQuery<int> rmq(array);
  EXPECT_EQ(37, rmq.array()[10]);
  EXPECT_EQ(43, rmq.array()[13]);
  EXPECT_EQ(-100, rmq.RangeMinimum(0, 20));
  EXPECT_EQ(-90, rmq.RangeMinimum(2, 6));
  EXPECT_EQ(33, rmq.RangeMinimum(7, 8));
  EXPECT_EQ(-96, rmq.RangeMinimum(8, 12));
}

TEST_F(RangeMinimumQueryTest, TestUnderlyingArray) {
  const std::vector<int> array = {-1, 1, 3, 14};
  const RangeMinimumQuery<int> rmq(array);
  EXPECT_EQ(array, rmq.array());
}

struct GreaterInt {
  bool operator()(int lhs, int rhs) const { return lhs > rhs; }
};

TEST_F(RangeMinimumQueryTest, SmallInputTest) {
  const std::vector<int> array1 = {10, 20, -100, 20, 10};
  ConstructAndLinearlyTestAllRanges(array1);
  ConstructAndLinearlyTestAllRanges<int, GreaterInt>(array1);

  const std::vector<int> array2 = {1, 2, 3, 4, 0};
  ConstructAndLinearlyTestAllRanges(array2);
  ConstructAndLinearlyTestAllRanges<int, GreaterInt>(array2);

  const std::vector<int> array3 = {0, 1, 2, 3, 4};
  ConstructAndLinearlyTestAllRanges(array3);
  ConstructAndLinearlyTestAllRanges<int, GreaterInt>(array3);
}

TEST_F(RangeMinimumQueryTest, LargeInputTest) {
  const std::vector<int> array1 = {66, -27, 77, 47, -2, -1, 35, 17, -31, -72};
  ConstructAndTestAllRangesWithDP(array1);
  ConstructAndTestAllRangesWithDP<int, GreaterInt>(array1);

  const std::vector<int> array2 = {-69, -40, 41,  -46, -48,
                                   -84, 54,  -98, 84,  69};
  ConstructAndLinearlyTestAllRanges(array2);
  ConstructAndLinearlyTestAllRanges<int, GreaterInt>(array2);
}

struct StringCompare {
  bool operator()(absl::string_view lhs, absl::string_view rhs) const {
    return lhs.compare(rhs) < 0;
  }
};

TEST_F(RangeMinimumQueryTest, TypeString) {
  const std::vector<std::string> test_array = {
      "abracadabra",
      "abraham",
      "aubergine",
      "arnold",
  };
  ConstructAndLinearlyTestAllRanges<std::string, StringCompare>(test_array);
}

class RangeMinimumIndexQueryTest : public testing::Test {
 protected:
  // Builds an index rmq from the array and tests all {n choose 2} possible
  // queries against a trivial linear scan. Since the complexity is O(n^3), do
  // not use this function with arrays of length more than 5.
  template <typename T, typename Compare = std::less<T>>
  void ConstructAndLinearlyTestAllRanges(std::vector<T> array,
                                         Compare cmp = Compare()) {
    const RangeMinimumIndexQuery<T, Compare> rmq(array, cmp);
    for (int from = 0; from < array.size(); ++from) {
      for (int to = from + 1; to <= array.size(); ++to) {
        T expected_min = array[rmq.GetMinimumIndexFromRange(from, to)];
        for (int scan = from; scan < to; ++scan) {
          // cmp corresponds to <, so to express expected_min <= array[scan] we
          // say !(array[scan] < expected_min)
          EXPECT_FALSE(cmp(array[scan], expected_min));
        }
      }
    }
  }
};

TEST_F(RangeMinimumIndexQueryTest, RangeMinimumIndexQueryTest) {
  ConstructAndLinearlyTestAllRanges<int, std::greater<int>>({0, 0, 0, 0, 0});
  ConstructAndLinearlyTestAllRanges<int>({0, 1, 2, 3, 4});
  ConstructAndLinearlyTestAllRanges<double, std::greater<int>>(
      {0., -1., -2., -3., -4.});
  ConstructAndLinearlyTestAllRanges<int>({3, 2, 1, 2, 3});
  ConstructAndLinearlyTestAllRanges<std::string, StringCompare>(
      {"magic", "magician", "mask"});
}
}  // namespace
}  // namespace operations_research
