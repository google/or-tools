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

#include "ortools/algorithms/multikey_radix_sort.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/random/random.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/algorithms/radix_sort.h"
#include "ortools/base/gmock.h"
#include "ortools/base/strong_int.h"
#include "ortools/base/strong_vector.h"

namespace operations_research {
namespace {
DEFINE_STRONG_INT_TYPE(StrongIndex, int32_t);

using ::testing::ElementsAre;

template <typename T>
constexpr std::pair<T, T> FullRange() {
  return {std::numeric_limits<T>::lowest(), std::numeric_limits<T>::max()};
}

template <typename T>
constexpr std::pair<T, T> NonNegativeRange(
    const T max_value = std::numeric_limits<T>::max()) {
  return {0, max_value};
}

template <typename T>
T GenerateRandomValue(absl::BitGen& bitgen, std::pair<T, T> bounds) {
  return absl::Uniform<T>(absl::IntervalClosedClosed, bitgen, bounds.first,
                          bounds.second);
}

template <typename T>
std::vector<T> GenerateRandomVector(size_t length, std::pair<T, T> bounds) {
  absl::BitGen bitgen;
  std::vector<T> values(length);
  for (size_t i = 0; i < length; ++i) {
    values[i] = GenerateRandomValue<T>(bitgen, bounds);
  }
  return values;
}

std::vector<std::pair<int32_t, int32_t>> GenerateRandomInt32PairVector(
    size_t length, int max_value) {
  absl::BitGen bitgen;
  std::vector<std::pair<int32_t, int32_t>> values(length);
  const auto bounds = NonNegativeRange<int32_t>(max_value);
  for (size_t i = 0; i < length; ++i) {
    values[i].first = GenerateRandomValue<int32_t>(bitgen, bounds);
    values[i].second = GenerateRandomValue<int32_t>(bitgen, bounds);
  }
  return values;
}

std::vector<std::tuple<int32_t, int64_t, uint64_t, uint32_t>>
GenerateRandomTupleVector(
    size_t length,
    std::pair<int32_t, int32_t> int32_bounds = FullRange<int32_t>(),
    std::pair<int64_t, int64_t> int64_bounds = FullRange<int64_t>(),
    std::pair<uint64_t, uint64_t> uint64_bounds = FullRange<uint64_t>(),
    std::pair<uint32_t, uint32_t> uint32_bounds = FullRange<uint32_t>()) {
  absl::BitGen bitgen;
  std::vector<std::tuple<int32_t, int64_t, uint64_t, uint32_t>> values(length);
  for (size_t i = 0; i < length; ++i) {
    // Note: intervals are closed closed, so the max values can be included.
    values[i] = {GenerateRandomValue<int32_t>(bitgen, int32_bounds),
                 GenerateRandomValue<int64_t>(bitgen, int64_bounds),
                 GenerateRandomValue<uint64_t>(bitgen, uint64_bounds),
                 GenerateRandomValue<uint32_t>(bitgen, uint32_bounds)};
  }
  return values;
}

class MultikeyRadixSortStableTest : public testing::TestWithParam<int> {};

TEST_P(MultikeyRadixSortStableTest, IsStable) {
  struct StableItem {
    int key;
    int value;

    bool operator<(const StableItem& other) const { return key < other.key; }
  };

  const int size = GetParam();
  std::vector<StableItem> items;
  items.reserve(size);
  for (int i = 0; i < size; ++i) {
    items.push_back({i % 10, i});
  }
  std::vector<StableItem> expected = items;
  absl::c_stable_sort(expected);

  MultikeyRadixSort(items, [](const StableItem& item) { return item.key; });

  ASSERT_EQ(items.size(), size);
  ASSERT_EQ(items.size(), expected.size());
  for (int i = 0; i < size; ++i) {
    EXPECT_EQ(items[i].key, expected[i].key) << "i = " << i;
    EXPECT_EQ(items[i].value, expected[i].value) << "i = " << i;
  }
}

INSTANTIATE_TEST_SUITE_P(StableTestInstantiation, MultikeyRadixSortStableTest,
                         testing::Values(0, 1, 3, 10, 30, 100, 300, 1000));

class RadixSortMinMaxStableTest : public testing::TestWithParam<int> {};

TEST_P(RadixSortMinMaxStableTest, IsStable) {
  struct StableItem {
    int key;
    int value;

    bool operator<(const StableItem& other) const { return key < other.key; }
  };

  const int size = GetParam();
  std::vector<StableItem> items;
  items.reserve(size);
  for (int i = 0; i < size; ++i) {
    items.push_back({i % 10, i});
  }
  std::vector<StableItem> expected = items;
  absl::c_stable_sort(expected);

  RadixSortMinMax(0, 9, items, [](const StableItem& item) { return item.key; });

  ASSERT_EQ(items.size(), size);
  ASSERT_EQ(items.size(), expected.size());
  for (int i = 0; i < size; ++i) {
    EXPECT_EQ(items[i].key, expected[i].key) << "i = " << i;
    EXPECT_EQ(items[i].value, expected[i].value) << "i = " << i;
  }
}

INSTANTIATE_TEST_SUITE_P(StableTestInstantiation, RadixSortMinMaxStableTest,
                         testing::Values(0, 1, 3, 10, 30, 100, 300, 1000));

TEST(MultikeyRadixSortTest, MultikeyRadixSortThreeKeys) {
  struct MyStruct {
    uint32_t a;
    uint32_t b;
    uint32_t c;

    bool operator==(const MyStruct& other) const {
      return a == other.a && b == other.b && c == other.c;
    }
  };

  std::vector<MyStruct> values = {
      {2, 1, 3}, {1, 2, 3}, {1, 1, 1}, {2, 2, 2}, {1, 1, 2},
  };

  MultikeyRadixSort(
      values, [](const auto& p) { return p.c; },
      [](const auto& p) { return p.b; }, [](const auto& p) { return p.a; });

  EXPECT_THAT(values, ElementsAre(MyStruct{1, 1, 1}, MyStruct{1, 1, 2},
                                  MyStruct{1, 2, 3}, MyStruct{2, 1, 3},
                                  MyStruct{2, 2, 2}));
}

TEST(MultikeyRadixSortTest, MultikeyRadixSortOneKey) {
  std::vector<int32_t> values = {5, 2, 4, 1, 3};

  MultikeyRadixSort(values, [](const auto& p) { return p; });

  EXPECT_THAT(values, ElementsAre(1, 2, 3, 4, 5));
}

TEST(RadixSortMinMaxTest, RadixSortMinMaxOneKey) {
  std::vector<int32_t> values = {5, 2, 4, 1, 3};

  RadixSortMinMax(1, 5, values, [](const auto& p) { return p; });

  EXPECT_THAT(values, ElementsAre(1, 2, 3, 4, 5));
}

void TestRandomVectorSortWithParams(int radix_bits, size_t length,
                                    int max_value) {
  auto values = GenerateRandomInt32PairVector(length, max_value);
  auto expected = values;
  std::sort(expected.begin(), expected.end());

  MultikeyRadixSort(
      radix_bits, values, [](const auto& p) { return p.second; },
      [](const auto& p) { return p.first; });

  EXPECT_EQ(values, expected);
}

TEST(MultikeyRadixSortTest, SortsRandomVector_DifferentRadixBits) {
  constexpr int kLength = 100'000;
  constexpr int kRange = 10 * kLength;
  for (int radix_bits = 6; radix_bits <= 12; ++radix_bits) {
    SCOPED_TRACE(absl::StrCat("radix_bits = ", radix_bits));
    TestRandomVectorSortWithParams(radix_bits, kLength, kRange);
  }
}

TEST(MultikeyRadixSortTest, SortsRandomVectorMultikeyRadixSort) {
  constexpr int kLength = 1'000'000;
  constexpr int kRange = 10 * kLength;
  auto values = GenerateRandomInt32PairVector(kLength, kRange);
  auto expected = values;
  std::sort(expected.begin(), expected.end());

  MultikeyRadixSort(
      values, [](const auto& p) { return p.second; },
      [](const auto& p) { return p.first; });

  EXPECT_EQ(values, expected);
}

TEST(MultikeyRadixSortTest, MultikeyRadixSortTuple) {
  constexpr int kLength = 1'000'000;
  auto values = GenerateRandomTupleVector(kLength);

  auto expected = values;
  std::sort(expected.begin(), expected.end());

  MultikeyRadixSort(
      values, [](const auto& t) { return std::get<3>(t); },
      [](const auto& t) { return std::get<2>(t); },
      [](const auto& t) { return std::get<1>(t); },
      [](const auto& t) { return std::get<0>(t); });

  EXPECT_EQ(values, expected);
}

TEST(MultikeyRadixSortTest, MultikeyRadixSortTupleSmallBounds) {
  constexpr int kLength = 1'000'000;
  constexpr int kRange32 = 10'000;
  constexpr int kRange64 = 10'000'000;
  auto values = GenerateRandomTupleVector(kLength, {-kRange32, kRange32},
                                          {-kRange64, kRange64}, {0, kRange64},
                                          {0, kRange32});

  auto expected = values;
  std::sort(expected.begin(), expected.end());

  MultikeyRadixSort(
      values, [](const auto& t) { return std::get<3>(t); },
      [](const auto& t) { return std::get<2>(t); },
      [](const auto& t) { return std::get<1>(t); },
      [](const auto& t) { return std::get<0>(t); });

  EXPECT_EQ(values, expected);
}

TEST(MultikeyRadixSortTest, MultikeyRadixSortNegativeKeys) {
  std::vector<int32_t> values = {5, -2, 4, -1, 0, 3, -10};

  MultikeyRadixSort(values);

  EXPECT_THAT(values, ElementsAre(-10, -2, -1, 0, 3, 4, 5));
}

TEST(RadixSortMinMaxTest, RadixSortMinMaxNegativeKeys) {
  std::vector<int32_t> values = {5, -2, 4, -1, 0, 3, -10};

  RadixSortMinMax(-10, 5, values, [](const auto& p) { return p; });

  EXPECT_THAT(values, ElementsAre(-10, -2, -1, 0, 3, 4, 5));
}

TEST(MultikeyRadixSortTest, SortCArray) {
  constexpr int kLength = 10'000;
  absl::BitGen bitgen;
  const auto bounds = NonNegativeRange<int>(100'000);
  std::vector<int> values = GenerateRandomVector<int>(kLength, bounds);
  int c_array[kLength];
  for (int i = 0; i < kLength; ++i) {
    c_array[i] = values[i];
  }
  MultikeyRadixSort(c_array);
  MultikeyRadixSort(values);
  const std::vector<int> c_array_vec(std::begin(c_array), std::end(c_array));
  EXPECT_EQ(c_array_vec, values);
}

TEST(RadixSortMinMaxTest, SortCArray) {
  constexpr int kLength = 10'000;
  absl::BitGen bitgen;
  const auto bounds = NonNegativeRange<int>(100'000);
  std::vector<int> values = GenerateRandomVector<int>(kLength, bounds);
  int c_array[kLength];
  for (int i = 0; i < kLength; ++i) {
    c_array[i] = values[i];
  }
  RadixSortMinMax(bounds.first, bounds.second, c_array,
                  [](const auto& val) { return val; });
  MultikeyRadixSort(values);
  const std::vector<int> c_array_vec(std::begin(c_array), std::end(c_array));
  EXPECT_EQ(c_array_vec, values);
}

TEST(MultikeyRadixSortTest, SortStrongVector) {
  constexpr int kLength = 10'000;
  ::util_intops::StrongVector<StrongIndex, int64_t> values;
  values.reserve(kLength);
  absl::BitGen bitgen;
  for (int i = 0; i < kLength; ++i) {
    values.push_back(absl::Uniform<int64_t>(bitgen, kint64min, kint64max));
  }
  auto expected_values = values;
  std::sort(expected_values.begin(), expected_values.end());
  MultikeyRadixSort(values, [](const auto& val) { return val; });
  EXPECT_EQ(values, expected_values);
}

TEST(RadixSortMinMaxTest, SortStrongVector) {
  constexpr int kLength = 10'000;
  ::util_intops::StrongVector<StrongIndex, int64_t> values;
  values.reserve(kLength);
  absl::BitGen bitgen;
  for (int i = 0; i < kLength; ++i) {
    values.push_back(
        absl::Uniform<int64_t>(bitgen, std::numeric_limits<int64_t>::lowest(),
                               std::numeric_limits<int64_t>::max()));
  }
  auto expected_values = values;
  std::sort(expected_values.begin(), expected_values.end());
  RadixSortMinMax(std::numeric_limits<int64_t>::lowest(),
                  std::numeric_limits<int64_t>::max(), values,
                  [](const auto& val) { return val; });
  EXPECT_EQ(values, expected_values);
}

TEST(MultikeyRadixSortTest, SortsEmptyVector) {
  std::vector<int64_t> values = {};
  MultikeyRadixSort(values);
  EXPECT_THAT(values, testing::IsEmpty());
}

TEST(RadixSortMinMaxTest, SortsEmptyVector) {
  std::vector<int64_t> values = {};
  RadixSortMinMax(0L, 0L, values, [](const auto& val) { return val; });
  EXPECT_THAT(values, testing::IsEmpty());
}

TEST(RadixSortMinMaxTest, SortPairsOfInts) {
  constexpr int kMax = 30;
  std::vector<std::pair<int, int>> values;
  for (int i = 0; i <= kMax; ++i) {
    for (int j = 0; j <= kMax; ++j) {
      values.push_back({i, j});
    }
  }

  absl::BitGen bitgen;
  std::shuffle(values.begin(), values.end(), bitgen);

  // Sort by first key.
  {
    auto expected = values;
    std::stable_sort(
        expected.begin(), expected.end(),
        [](const auto& a, const auto& b) { return a.first < b.first; });
    auto sorted = values;
    RadixSortMinMax(0, kMax, sorted, [](const auto& p) { return p.first; });
    EXPECT_EQ(sorted, expected);
  }

  // Sort by second key.
  {
    auto expected = values;
    std::stable_sort(
        expected.begin(), expected.end(),
        [](const auto& a, const auto& b) { return a.second < b.second; });
    auto sorted = values;
    RadixSortMinMax(0, kMax, sorted, [](const auto& p) { return p.second; });
    EXPECT_EQ(sorted, expected);
  }
}

TEST(RadixSortMinMaxTest, AllElementsIdentical) {
  struct MyStruct {
    int a;
    int b;
    bool operator==(const MyStruct& other) const {
      return a == other.a && b == other.b;
    }
  };
  constexpr int kLength = 100'000;
  std::vector<MyStruct> values(kLength);
  for (int i = 0; i < kLength; ++i) {
    values[i].a = 12345;
    values[i].b = i;
  }
  auto expected = values;
  RadixSortMinMax(12345, 12345, values, [](MyStruct v) { return v.a; });
  EXPECT_EQ(values, expected);
  values = expected;
  MultikeyRadixSort(values, [](MyStruct v) { return v.a; });
  EXPECT_EQ(values, expected);
}

// MultikeyRadixSort requires copyable types, so it cannot sort containers of
// move-only types like std::unique_ptr.
TEST(MultikeyRadixSortTest, DoesNotSupportMoveOnlyTypes) {
  ASSERT_FALSE(std::is_copy_constructible_v<std::unique_ptr<int>>);
  ASSERT_FALSE(std::is_copy_assignable_v<std::unique_ptr<int>>);
}

}  // namespace
}  // namespace operations_research
