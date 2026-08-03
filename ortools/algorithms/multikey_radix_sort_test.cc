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
#include <functional>
#include <limits>
#include <memory>
#include <numeric>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/log/absl_log.h"
#include "absl/numeric/bits.h"
#include "absl/random/random.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
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
  if constexpr (std::is_floating_point_v<T>) {
    // Avoid overflow in absl::Uniform by halving the max value. A some point
    // when generating random numbers, absl::Uniform will subtract the bounds.
    // Using the full range would result in a NaN, which would result in the
    // radix sort sorting a set of identical incorrect values.
    const T half_max_val = 0.5 * std::numeric_limits<T>::max();
    return {-half_max_val, half_max_val};
  } else {
    return {std::numeric_limits<T>::lowest(), std::numeric_limits<T>::max()};
  }
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

class AutoRadixSortStableTest : public testing::TestWithParam<int> {};

TEST_P(AutoRadixSortStableTest, IsStable) {
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
  absl::c_stable_sort(expected, [](const StableItem& a, const StableItem& b) {
    return a.key < b.key;
  });

  AutoHistogramRadixSort(items,
                         [](const StableItem& item) { return item.key; });

  ASSERT_EQ(items.size(), size);
  ASSERT_EQ(items.size(), expected.size());
  for (int i = 0; i < size; ++i) {
    EXPECT_EQ(items[i].key, expected[i].key) << "i = " << i;
    EXPECT_EQ(items[i].value, expected[i].value) << "i = " << i;
  }
}

INSTANTIATE_TEST_SUITE_P(StableTestInstantiation, AutoRadixSortStableTest,
                         testing::Values(0, 1, 3, 10, 30, 100, 300, 1000));

class RangeRadixSortStableTest : public testing::TestWithParam<int> {};

TEST_P(RangeRadixSortStableTest, IsStable) {
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
  absl::c_stable_sort(expected, [](const StableItem& a, const StableItem& b) {
    return a.key < b.key;
  });

  RangeHistogramRadixSort(0, 9, items,
                          [](const StableItem& item) { return item.key; });

  ASSERT_EQ(items.size(), size);
  ASSERT_EQ(items.size(), expected.size());
  for (int i = 0; i < size; ++i) {
    EXPECT_EQ(items[i].key, expected[i].key) << "i = " << i;
    EXPECT_EQ(items[i].value, expected[i].value) << "i = " << i;
  }
}

INSTANTIATE_TEST_SUITE_P(StableTestInstantiation, RangeRadixSortStableTest,
                         testing::Values(0, 1, 3, 10, 30, 100, 300, 1000));

TEST(MultikeyHistogramRadixSortTest, ThreeKeys) {
  struct MyStruct {
    uint32_t a;
    uint32_t b;
    uint32_t c;

    bool operator==(const MyStruct& other) const {
      return a == other.a && b == other.b && c == other.c;
    }
  };

  const std::vector<MyStruct> data = {
      {2, 1, 3}, {1, 2, 3}, {1, 1, 1}, {2, 2, 2}, {1, 1, 2},
  };
  std::vector<MyStruct> values_multi = data;
  MultikeyHistogramRadixSort(
      values_multi, [](const auto& p) { return p.c; },
      [](const auto& p) { return p.b; }, [](const auto& p) { return p.a; });

  EXPECT_THAT(values_multi, ElementsAre(MyStruct{1, 1, 1}, MyStruct{1, 1, 2},
                                        MyStruct{1, 2, 3}, MyStruct{2, 1, 3},
                                        MyStruct{2, 2, 2}));
  std::vector<MyStruct> values_seq = data;
  AutoHistogramRadixSort(values_seq, [](const auto& p) { return p.c; });
  AutoHistogramRadixSort(values_seq, [](const auto& p) { return p.b; });
  AutoHistogramRadixSort(values_seq, [](const auto& p) { return p.a; });
  EXPECT_EQ(values_multi, values_seq);
}

TEST(MultikeyRadixSortTest, ThreeKeys) {
  struct MyStruct {
    uint32_t a;
    uint32_t b;
    uint32_t c;

    bool operator==(const MyStruct& other) const {
      return a == other.a && b == other.b && c == other.c;
    }
  };

  const std::vector<MyStruct> data = {
      {2, 1, 3}, {1, 2, 3}, {1, 1, 1}, {2, 2, 2}, {1, 1, 2},
  };
  std::vector<MyStruct> values_multi = data;
  MultikeyRadixSort(
      values_multi, [](const auto& p) { return p.c; },
      [](const auto& p) { return p.b; }, [](const auto& p) { return p.a; });

  EXPECT_THAT(values_multi, ElementsAre(MyStruct{1, 1, 1}, MyStruct{1, 1, 2},
                                        MyStruct{1, 2, 3}, MyStruct{2, 1, 3},
                                        MyStruct{2, 2, 2}));
  std::vector<MyStruct> values_seq = data;
  AutoRadixSort(values_seq, [](const auto& p) { return p.c; });
  AutoRadixSort(values_seq, [](const auto& p) { return p.b; });
  AutoRadixSort(values_seq, [](const auto& p) { return p.a; });
  EXPECT_EQ(values_multi, values_seq);
}

TEST(MultikeyHistogramRadixSortTest, ThreeKeysDecreasing) {
  struct MyStruct {
    uint32_t a;
    uint32_t b;
    uint32_t c;

    bool operator==(const MyStruct& other) const {
      return a == other.a && b == other.b && c == other.c;
    }
  };

  const std::vector<MyStruct> data = {
      {2, 1, 3}, {1, 2, 3}, {1, 1, 1}, {2, 2, 2}, {1, 1, 2},
  };
  std::vector<MyStruct> values_multi = data;
  MultikeyHistogramRadixSort<RadixSortDirection::kDecreasing>(
      values_multi, [](const auto& p) { return p.c; },
      [](const auto& p) { return p.b; }, [](const auto& p) { return p.a; });

  EXPECT_THAT(values_multi, ElementsAre(MyStruct{2, 2, 2}, MyStruct{2, 1, 3},
                                        MyStruct{1, 2, 3}, MyStruct{1, 1, 2},
                                        MyStruct{1, 1, 1}));
  std::vector<MyStruct> values_seq = data;
  AutoHistogramRadixSort<RadixSortDirection::kDecreasing>(
      values_seq, [](const auto& p) { return p.c; });
  AutoHistogramRadixSort<RadixSortDirection::kDecreasing>(
      values_seq, [](const auto& p) { return p.b; });
  AutoHistogramRadixSort<RadixSortDirection::kDecreasing>(
      values_seq, [](const auto& p) { return p.a; });
  EXPECT_EQ(values_multi, values_seq);
}

TEST(MultikeyRadixSortTest, ThreeKeysDecreasing) {
  struct MyStruct {
    uint32_t a;
    uint32_t b;
    uint32_t c;

    bool operator==(const MyStruct& other) const {
      return a == other.a && b == other.b && c == other.c;
    }
  };

  const std::vector<MyStruct> data = {
      {2, 1, 3}, {1, 2, 3}, {1, 1, 1}, {2, 2, 2}, {1, 1, 2},
  };
  std::vector<MyStruct> values_multi = data;
  MultikeyRadixSort<RadixSortDirection::kDecreasing>(
      values_multi, [](const auto& p) { return p.c; },
      [](const auto& p) { return p.b; }, [](const auto& p) { return p.a; });

  EXPECT_THAT(values_multi, ElementsAre(MyStruct{2, 2, 2}, MyStruct{2, 1, 3},
                                        MyStruct{1, 2, 3}, MyStruct{1, 1, 2},
                                        MyStruct{1, 1, 1}));
  std::vector<MyStruct> values_seq = data;
  AutoRadixSort<RadixSortDirection::kDecreasing>(
      values_seq, [](const auto& p) { return p.c; });
  AutoRadixSort<RadixSortDirection::kDecreasing>(
      values_seq, [](const auto& p) { return p.b; });
  AutoRadixSort<RadixSortDirection::kDecreasing>(
      values_seq, [](const auto& p) { return p.a; });
  EXPECT_EQ(values_multi, values_seq);
}

std::vector<std::pair<int32_t, int32_t>> GeneratePairData(size_t length) {
  return GenerateRandomInt32PairVector(length, 10 * length);
}

std::vector<std::tuple<int32_t, int64_t, uint64_t, uint32_t>> GenerateTupleData(
    size_t length) {
  return GenerateRandomTupleVector(length);
}

std::vector<std::tuple<int32_t, int64_t, uint64_t, uint32_t>>
GenerateTupleDataSmallBounds(size_t length) {
  constexpr int kRange32 = 10'000;
  constexpr int kRange64 = 10'000'000;
  return GenerateRandomTupleVector(length, {-kRange32, kRange32},
                                   {-kRange64, kRange64}, {0, kRange64},
                                   {0, kRange32});
}

template <auto GeneratorFn, RadixSortDirection Direction,
          typename... Extractors>
void TestTupleSorting(Extractors... extractors) {
  constexpr int kLength = 1'000'000;
  auto data = GeneratorFn(kLength);

  auto values_multi = data;
  auto expected = values_multi;
  (absl::c_stable_sort(expected,
                       [&](const auto& lhs, const auto& rhs) {
                         if constexpr (Direction ==
                                       RadixSortDirection::kIncreasing) {
                           return extractors(lhs) < extractors(rhs);
                         } else {
                           return extractors(lhs) > extractors(rhs);
                         }
                       }),
   ...);

  MultikeyHistogramRadixSort<Direction>(values_multi, extractors...);

  EXPECT_EQ(values_multi, expected);
  auto values_seq = data;
  (AutoHistogramRadixSort<Direction>(values_seq, extractors), ...);
  EXPECT_EQ(values_multi, values_seq);
}

template <auto GeneratorFn, RadixSortDirection Direction,
          typename... Extractors>
void TestTupleSortingNonHistogram(Extractors... extractors) {
  constexpr int kLength = 1'000'000;
  auto data = GeneratorFn(kLength);

  auto values_multi = data;
  auto expected = values_multi;
  (absl::c_stable_sort(expected,
                       [&](const auto& lhs, const auto& rhs) {
                         if constexpr (Direction ==
                                       RadixSortDirection::kIncreasing) {
                           return extractors(lhs) < extractors(rhs);
                         } else {
                           return extractors(lhs) > extractors(rhs);
                         }
                       }),
   ...);

  MultikeyRadixSort<Direction>(values_multi, extractors...);

  EXPECT_EQ(values_multi, expected);
  auto values_seq = data;
  (AutoRadixSort<Direction>(values_seq, extractors), ...);
  EXPECT_EQ(values_multi, values_seq);
}

TEST(MultikeyHistogramRadixSortTest, SortsRandomVector) {
  TestTupleSorting<GeneratePairData, RadixSortDirection::kIncreasing>(
      [](const auto& p) { return p.second; },
      [](const auto& p) { return p.first; });
}
TEST(MultikeyRadixSortTest, SortsRandomVector) {
  TestTupleSortingNonHistogram<GeneratePairData,
                               RadixSortDirection::kIncreasing>(
      [](const auto& p) { return p.second; },
      [](const auto& p) { return p.first; });
}

TEST(MultikeyHistogramRadixSortTest, Tuple) {
  TestTupleSorting<GenerateTupleData, RadixSortDirection::kIncreasing>(
      [](const auto& t) { return std::get<3>(t); },
      [](const auto& t) { return std::get<2>(t); },
      [](const auto& t) { return std::get<1>(t); },
      [](const auto& t) { return std::get<0>(t); });
}
TEST(MultikeyRadixSortTest, Tuple) {
  TestTupleSortingNonHistogram<GenerateTupleData,
                               RadixSortDirection::kIncreasing>(
      [](const auto& t) { return std::get<3>(t); },
      [](const auto& t) { return std::get<2>(t); },
      [](const auto& t) { return std::get<1>(t); },
      [](const auto& t) { return std::get<0>(t); });
}

TEST(MultikeyHistogramRadixSortTest, TupleSmallBounds) {
  TestTupleSorting<GenerateTupleDataSmallBounds,
                   RadixSortDirection::kIncreasing>(
      [](const auto& t) { return std::get<3>(t); },
      [](const auto& t) { return std::get<2>(t); },
      [](const auto& t) { return std::get<1>(t); },
      [](const auto& t) { return std::get<0>(t); });
}
TEST(MultikeyRadixSortTest, TupleSmallBounds) {
  TestTupleSortingNonHistogram<GenerateTupleDataSmallBounds,
                               RadixSortDirection::kIncreasing>(
      [](const auto& t) { return std::get<3>(t); },
      [](const auto& t) { return std::get<2>(t); },
      [](const auto& t) { return std::get<1>(t); },
      [](const auto& t) { return std::get<0>(t); });
}

TEST(MultikeyHistogramRadixSortTest, DecreasingTuple) {
  TestTupleSorting<GenerateTupleData, RadixSortDirection::kDecreasing>(
      [](const auto& t) { return std::get<3>(t); },
      [](const auto& t) { return std::get<2>(t); },
      [](const auto& t) { return std::get<1>(t); },
      [](const auto& t) { return std::get<0>(t); });
}
TEST(MultikeyRadixSortTest, DecreasingTuple) {
  TestTupleSortingNonHistogram<GenerateTupleData,
                               RadixSortDirection::kDecreasing>(
      [](const auto& t) { return std::get<3>(t); },
      [](const auto& t) { return std::get<2>(t); },
      [](const auto& t) { return std::get<1>(t); },
      [](const auto& t) { return std::get<0>(t); });
}

TEST(MultikeyHistogramRadixSortTest, DecreasingTupleSmallBounds) {
  TestTupleSorting<GenerateTupleDataSmallBounds,
                   RadixSortDirection::kDecreasing>(
      [](const auto& t) { return std::get<3>(t); },
      [](const auto& t) { return std::get<2>(t); },
      [](const auto& t) { return std::get<1>(t); },
      [](const auto& t) { return std::get<0>(t); });
}
TEST(MultikeyRadixSortTest, DecreasingTupleSmallBounds) {
  TestTupleSortingNonHistogram<GenerateTupleDataSmallBounds,
                               RadixSortDirection::kDecreasing>(
      [](const auto& t) { return std::get<3>(t); },
      [](const auto& t) { return std::get<2>(t); },
      [](const auto& t) { return std::get<1>(t); },
      [](const auto& t) { return std::get<0>(t); });
}

template <auto SortFn>
void TestNegativeKeys() {
  std::vector<int32_t> values = {5, -2, 4, -1, 0, 3, -10};
  SortFn(values);
  EXPECT_THAT(values, ElementsAre(-10, -2, -1, 0, 3, 4, 5));
}

TEST(AutoRadixSortTest, AutoRadixSortNegativeKeys) {
  TestNegativeKeys<[](auto& arr) { AutoHistogramRadixSort(arr); }>();
}

TEST(RangeRadixSortTest, RangeRadixSortNegativeKeys) {
  TestNegativeKeys<[](auto& arr) {
    RangeHistogramRadixSort(-10, 5, arr, [](const auto& p) { return p; });
  }>();
}

template <auto SortFn,
          RadixSortDirection Direction = RadixSortDirection::kIncreasing>
void TestSortCArray() {
  constexpr int kLength = 10'000;
  const auto bounds = NonNegativeRange<int>(100'000);
  std::vector<int> values = GenerateRandomVector<int>(kLength, bounds);
  int c_array[kLength];
  for (int i = 0; i < kLength; ++i) {
    c_array[i] = values[i];
  }
  SortFn(c_array, bounds);
  if constexpr (Direction == RadixSortDirection::kIncreasing) {
    std::sort(values.begin(), values.end());
  } else {
    std::sort(values.begin(), values.end(), std::greater<>());
  }
  const std::vector<int> c_array_vec(std::begin(c_array), std::end(c_array));
  EXPECT_EQ(c_array_vec, values);
}

TEST(AutoRadixSortTest, SortCArray) {
  TestSortCArray<[](auto& arr, const auto&) { AutoHistogramRadixSort(arr); }>();
}

TEST(RangeRadixSortTest, SortCArray) {
  TestSortCArray<[](auto& arr, const auto& bounds) {
    RangeHistogramRadixSort(bounds.first, bounds.second, arr,
                            [](const auto& val) { return val; });
  }>();
}

TEST(AutoRadixSortTest, SortCArrayDecreasing) {
  TestSortCArray<[](auto& arr, const auto&) {
    AutoHistogramRadixSort<RadixSortDirection::kDecreasing>(arr);
  },
                 /*direction=*/RadixSortDirection::kDecreasing>();
}

TEST(RangeRadixSortTest, SortCArrayDecreasing) {
  TestSortCArray<[](auto& arr, const auto& bounds) {
    RangeHistogramRadixSort<RadixSortDirection::kDecreasing>(
        bounds.first, bounds.second, arr, [](const auto& val) { return val; });
  },
                 /*direction=*/RadixSortDirection::kDecreasing>();
}

template <auto SortFn>
void TestSortStrongVector() {
  constexpr int kLength = 10'000;
  const auto bounds = FullRange<int64_t>();
  std::vector<int64_t> std_values =
      GenerateRandomVector<int64_t>(kLength, bounds);
  ::util_intops::StrongVector<StrongIndex, int64_t> values;
  values.reserve(kLength);
  for (int64_t v : std_values) {
    values.push_back(v);
  }
  auto expected_values = values;
  std::sort(expected_values.begin(), expected_values.end());
  SortFn(values, bounds);
  EXPECT_EQ(values, expected_values);
}

TEST(AutoRadixSortTest, SortStrongVector) {
  TestSortStrongVector<[](auto& arr, const auto&) {
    AutoHistogramRadixSort(arr, [](const auto& val) { return val; });
  }>();
}

TEST(RangeRadixSortTest, SortStrongVector) {
  TestSortStrongVector<[](auto& arr, const auto& bounds) {
    RangeHistogramRadixSort(bounds.first, bounds.second, arr,
                            [](const auto& val) { return val; });
  }>();
}

template <auto SortFn>
void TestSortsEmptyVector() {
  std::vector<int64_t> values = {};
  const auto bounds = std::pair<int64_t, int64_t>(0, 0);
  SortFn(values, bounds);
  EXPECT_THAT(values, testing::IsEmpty());
}

TEST(RangeHistogramRadixSortTest, SortsEmptyVector) {
  std::vector<int64_t> values = {};
  RangeHistogramRadixSort(int64_t{0}, int64_t{0}, values,
                          [](const auto& val) { return val; });
  EXPECT_THAT(values, testing::IsEmpty());
}

template <RadixSortDirection Direction = RadixSortDirection::kIncreasing>
void TestSortPairsOfInts() {
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
    if constexpr (Direction == RadixSortDirection::kIncreasing) {
      std::stable_sort(
          expected.begin(), expected.end(),
          [](const auto& a, const auto& b) { return a.first < b.first; });
    } else {
      std::stable_sort(
          expected.begin(), expected.end(),
          [](const auto& a, const auto& b) { return a.first > b.first; });
    }
    auto sorted = values;
    RangeHistogramRadixSort<Direction>(0, kMax, sorted,
                                       [](const auto& p) { return p.first; });
    EXPECT_EQ(sorted, expected);
  }

  // Sort by second key.
  {
    auto expected = values;
    if constexpr (Direction == RadixSortDirection::kIncreasing) {
      std::stable_sort(
          expected.begin(), expected.end(),
          [](const auto& a, const auto& b) { return a.second < b.second; });
    } else {
      std::stable_sort(
          expected.begin(), expected.end(),
          [](const auto& a, const auto& b) { return a.second > b.second; });
    }
    auto sorted = values;
    RangeHistogramRadixSort<Direction>(0, kMax, sorted,
                                       [](const auto& p) { return p.second; });
    EXPECT_EQ(sorted, expected);
  }
}

TEST(RangeRadixSortTest, SortPairsOfInts) {
  TestSortPairsOfInts<RadixSortDirection::kIncreasing>();
}

TEST(RangeRadixSortTest, SortPairsOfIntsDecreasing) {
  TestSortPairsOfInts<RadixSortDirection::kDecreasing>();
}

TEST(RangeRadixSortTest, AllElementsIdentical) {
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
  RangeHistogramRadixSort(12345, 12345, values, [](MyStruct v) { return v.a; });
  EXPECT_EQ(values, expected);
  values = expected;
  AutoHistogramRadixSort(values, [](MyStruct v) { return v.a; });
  EXPECT_EQ(values, expected);
}
// AutoHistogramRadixSort requires copyable types, so it cannot sort containers
// of move-only types like std::unique_ptr.
TEST(AutoRadixSortTest, DoesNotSupportMoveOnlyTypes) {
  ASSERT_FALSE(std::is_copy_constructible_v<std::unique_ptr<int>>);
  ASSERT_FALSE(std::is_copy_assignable_v<std::unique_ptr<int>>);
}

struct Int64NonNeg {
  constexpr std::pair<int64_t, int64_t> bounds() const {
    return NonNegativeRange<int64_t>();
  }
  std::vector<int64_t> operator()(size_t n) const {
    return GenerateRandomVector<int64_t>(n, bounds());
  }
};

struct Int64Small {
  constexpr std::pair<int64_t, int64_t> bounds() const { return {0, 100000}; }
  std::vector<int64_t> operator()(size_t n) const {
    return GenerateRandomVector<int64_t>(n, bounds());
  }
};

struct Int64MaxMinusSmall {
  constexpr std::pair<int64_t, int64_t> bounds() const {
    return {std::numeric_limits<int64_t>::max() - 100000,
            std::numeric_limits<int64_t>::max()};
  }
  std::vector<int64_t> operator()(size_t n) const {
    return GenerateRandomVector<int64_t>(n, bounds());
  }
};

template <typename T>
struct FloatFull {
  constexpr std::pair<T, T> bounds() const { return FullRange<T>(); }
  std::vector<T> operator()(size_t n) const {
    return GenerateRandomVector<T>(n, bounds());
  }
};

template <typename T>
struct FloatSmall {
  constexpr std::pair<T, T> bounds() const { return {T{0.0}, T{100000.0}}; }
  std::vector<T> operator()(size_t n) const {
    return GenerateRandomVector<T>(n, bounds());
  }
};

template <typename T>
struct FloatMedium {
  constexpr std::pair<T, T> bounds() const {
    return {T{23500.0}, T{9500000.0}};
  }
  std::vector<T> operator()(size_t n) const {
    return GenerateRandomVector<T>(n, bounds());
  }
};

template <typename T>
class AutoRadixSortFloatTest : public ::testing::Test {};

using FloatingPointTypes = ::testing::Types<float, double>;
TYPED_TEST_SUITE(AutoRadixSortFloatTest, FloatingPointTypes);

template <typename T, RadixSortDirection Direction, typename SortFn>
void TestFloatSorting(const std::vector<T>& data, SortFn sort_fn) {
  auto values = data;
  auto expected = data;
  if constexpr (Direction == RadixSortDirection::kIncreasing) {
    std::sort(expected.begin(), expected.end());
  } else {
    std::sort(expected.begin(), expected.end(), std::greater<T>());
  }
  sort_fn(values);
  EXPECT_EQ(values, expected);
}

TYPED_TEST(AutoRadixSortFloatTest, ComprehensiveFloatSorting) {
  constexpr int kLength = 250'000;
  const auto full_bounds = FloatFull<TypeParam>{}.bounds();
  const auto full_data = FloatFull<TypeParam>{}(kLength);
  const auto small_bounds = FloatSmall<TypeParam>{}.bounds();
  const auto small_data = FloatSmall<TypeParam>{}(kLength);
  const auto medium_bounds = FloatMedium<TypeParam>{}.bounds();
  const auto medium_data = FloatMedium<TypeParam>{}(kLength);

  for (const auto& [data, bounds] :
       {std::make_pair(full_data, full_bounds),
        std::make_pair(small_data, small_bounds),
        std::make_pair(medium_data, medium_bounds)}) {
    const TypeParam key_min = bounds.first;
    const TypeParam key_max = bounds.second;

    // AutoHistogramRadixSort
    TestFloatSorting<TypeParam, RadixSortDirection::kIncreasing>(
        data, [](auto& v) {
          AutoHistogramRadixSort<RadixSortDirection::kIncreasing>(v);
        });
    TestFloatSorting<TypeParam, RadixSortDirection::kDecreasing>(
        data, [](auto& v) {
          AutoHistogramRadixSort<RadixSortDirection::kDecreasing>(v);
        });

    // AutoRadixSort
    TestFloatSorting<TypeParam, RadixSortDirection::kIncreasing>(
        data,
        [](auto& v) { AutoRadixSort<RadixSortDirection::kIncreasing>(v); });
    TestFloatSorting<TypeParam, RadixSortDirection::kDecreasing>(
        data,
        [](auto& v) { AutoRadixSort<RadixSortDirection::kDecreasing>(v); });

    // RangeHistogramRadixSort
    TestFloatSorting<TypeParam, RadixSortDirection::kIncreasing>(
        data, [key_min, key_max](auto& v) {
          RangeHistogramRadixSort<RadixSortDirection::kIncreasing>(key_min,
                                                                   key_max, v);
        });
    TestFloatSorting<TypeParam, RadixSortDirection::kDecreasing>(
        data, [key_min, key_max](auto& v) {
          RangeHistogramRadixSort<RadixSortDirection::kDecreasing>(key_min,
                                                                   key_max, v);
        });

    // RangeRadixSort
    TestFloatSorting<TypeParam, RadixSortDirection::kIncreasing>(
        data, [key_min, key_max](auto& v) {
          RangeRadixSort<RadixSortDirection::kIncreasing>(key_min, key_max, v);
        });
    TestFloatSorting<TypeParam, RadixSortDirection::kDecreasing>(
        data, [key_min, key_max](auto& v) {
          RangeRadixSort<RadixSortDirection::kDecreasing>(key_min, key_max, v);
        });
  }
}

TYPED_TEST(AutoRadixSortFloatTest, FloatToSortableUintPreservesOrder) {
  constexpr int kLength = 10'000;
  const auto values = FloatFull<TypeParam>{}(kLength);

  for (int i = 0; i < values.size(); ++i) {
    for (int j = 0; j < 10; ++j) {
      const TypeParam f1 = values[i];
      const TypeParam f2 = values[(i + j) % values.size()];
      const auto u1 = FloatToSortableUint(f1);
      const auto u2 = FloatToSortableUint(f2);
      EXPECT_EQ(u1 < u2, f1 < f2) << "f1 = " << f1 << ", f2 = " << f2;
      EXPECT_EQ(u1 > u2, f1 > f2) << "f1 = " << f1 << ", f2 = " << f2;
      EXPECT_EQ(u1 == u2, f1 == f2) << "f1 = " << f1 << ", f2 = " << f2;
    }
  }
}

TYPED_TEST(AutoRadixSortFloatTest, FloatToSortableUintIsBijection) {
  constexpr int kLength = 10'000;
  const auto values = FloatFull<TypeParam>{}(kLength);

  for (const TypeParam f : values) {
    const auto uint_val = FloatToSortableUint(f);
    const auto float_val = SortableUintToFloat(uint_val);
    EXPECT_EQ(float_val, f) << "f = " << f << ", float_val = " << float_val;
  }
}

struct StdSortWrapper {
  template <RadixSortDirection Direction, typename T>
  void operator()(std::vector<T>& v, int64_t, int64_t) const {
    if constexpr (Direction == RadixSortDirection::kIncreasing) {
      std::sort(v.begin(), v.end());
    } else {
      std::sort(v.begin(), v.end(), std::greater<T>());
    }
  }
};

struct StdStableSortWrapper {
  template <RadixSortDirection Direction, typename T>
  void operator()(std::vector<T>& v, int64_t, int64_t) const {
    if constexpr (Direction == RadixSortDirection::kIncreasing) {
      std::stable_sort(v.begin(), v.end());
    } else {
      std::stable_sort(v.begin(), v.end(), std::greater<T>());
    }
  }
};

struct AutoRadixSortWrapper {
  template <RadixSortDirection Direction, typename T>
  void operator()(std::vector<T>& v, int64_t, int64_t) const {
    AutoRadixSort<Direction>(v);
  }
};

struct AutoHistogramRadixSortWrapper {
  template <RadixSortDirection Direction, typename T>
  void operator()(std::vector<T>& v, int64_t, int64_t) const {
    AutoHistogramRadixSort<Direction>(v);
  }
};

struct RangeRadixSortWrapper {
  template <RadixSortDirection Direction, typename T>
  void operator()(std::vector<T>& v, int64_t min_val, int64_t max_val) const {
    RangeRadixSort<Direction>(min_val, max_val, v);
  }
};

struct RangeHistogramRadixSortWrapper {
  template <RadixSortDirection Direction, typename T>
  void operator()(std::vector<T>& v, int64_t min_val, int64_t max_val) const {
    RangeHistogramRadixSort<Direction>(min_val, max_val, v);
  }
};

template <typename SortFn, typename GeneratorFn>
void RunPerformanceComparisonTest(const std::string& sort_name,
                                  const std::string& gen_name, int size,
                                  RadixSortDirection Direction, int64_t min_val,
                                  int64_t max_val) {
  GeneratorFn generator;
  auto data = generator(size);

  auto copy1 = data;
  absl::Time start1 = absl::Now();
  if (Direction == RadixSortDirection::kIncreasing) {
    SortFn{}.template operator()<RadixSortDirection::kIncreasing>(
        copy1, min_val, max_val);
  } else {
    SortFn{}.template operator()<RadixSortDirection::kDecreasing>(
        copy1, min_val, max_val);
  }
  absl::Duration time1 = absl::Now() - start1;

  auto copy2 = data;
  absl::Time start2 = absl::Now();
  int num_bits = 64;
  if (min_val >= 0 && max_val > 0) {
    num_bits = 64 - absl::countl_zero(static_cast<uint64_t>(max_val));
  }
  ::operations_research::RadixSort(absl::MakeSpan(copy2), num_bits);
  absl::Duration time2 = absl::Now() - start2;

  double time1_us = absl::ToDoubleMicroseconds(time1);
  double time2_us = absl::ToDoubleMicroseconds(time2);
  double ratio = time1_us > 0 ? time2_us / time1_us : 0.0;

  ABSL_LOG(INFO) << ", " << sort_name << ", " << gen_name << ", " << size
                 << ", "
                 << (Direction == RadixSortDirection::kIncreasing ? "1" : "0")
                 << ", " << time1 << ", " << time2 << ", " << ratio;
}

TEST(AutoRadixSortTest, PerformanceComparison) {
  const std::vector<int> sizes = {100, 1000, 10000, 100000, 1000000};
  const std::vector<RadixSortDirection> kDirections = {
      RadixSortDirection::kIncreasing, RadixSortDirection::kDecreasing};
  const int64_t kInt64Max = std::numeric_limits<int64_t>::max();

  for (const int size : sizes) {
    for (const RadixSortDirection direction : kDirections) {
      RunPerformanceComparisonTest<AutoRadixSortWrapper, Int64NonNeg>(
          "AutoRadixSort", "Int64NonNeg", size, direction, 0, kInt64Max);
      RunPerformanceComparisonTest<AutoRadixSortWrapper, Int64Small>(
          "AutoRadixSort", "Int64Small", size, direction, 0, 100000);
      RunPerformanceComparisonTest<AutoRadixSortWrapper, Int64MaxMinusSmall>(
          "AutoRadixSort", "Int64MaxMinusSmall", size, direction,
          kInt64Max - 100000, kInt64Max);

      RunPerformanceComparisonTest<AutoHistogramRadixSortWrapper, Int64NonNeg>(
          "AutoHistogramRadixSort", "Int64NonNeg", size, direction, 0,
          kInt64Max);
      RunPerformanceComparisonTest<AutoHistogramRadixSortWrapper, Int64Small>(
          "AutoHistogramRadixSort", "Int64Small", size, direction, 0, 100000);
      RunPerformanceComparisonTest<AutoHistogramRadixSortWrapper,
                                   Int64MaxMinusSmall>(
          "AutoHistogramRadixSort", "Int64MaxMinusSmall", size, direction,
          kInt64Max - 100000, kInt64Max);
    }
  }
}

TEST(RangeRadixSortTest, PerformanceComparison) {
  const std::vector<int> sizes = {100, 1000, 10000, 100000, 1000000};
  const std::vector<RadixSortDirection> kDirections = {
      RadixSortDirection::kIncreasing, RadixSortDirection::kDecreasing};
  const int64_t kInt64Max = std::numeric_limits<int64_t>::max();

  for (const int size : sizes) {
    for (const RadixSortDirection direction : kDirections) {
      RunPerformanceComparisonTest<RangeRadixSortWrapper, Int64NonNeg>(
          "RangeRadixSort", "Int64NonNeg", size, direction, 0, kInt64Max);
      RunPerformanceComparisonTest<RangeRadixSortWrapper, Int64Small>(
          "RangeRadixSort", "Int64Small", size, direction, 0, 100000);
      RunPerformanceComparisonTest<RangeRadixSortWrapper, Int64MaxMinusSmall>(
          "RangeRadixSort", "Int64MaxMinusSmall", size, direction,
          kInt64Max - 100000, kInt64Max);

      RunPerformanceComparisonTest<RangeHistogramRadixSortWrapper, Int64NonNeg>(
          "RangeHistogramRadixSort", "Int64NonNeg", size, direction, 0,
          kInt64Max);
      RunPerformanceComparisonTest<RangeHistogramRadixSortWrapper, Int64Small>(
          "RangeHistogramRadixSort", "Int64Small", size, direction, 0, 100000);
      RunPerformanceComparisonTest<RangeHistogramRadixSortWrapper,
                                   Int64MaxMinusSmall>(
          "RangeHistogramRadixSort", "Int64MaxMinusSmall", size, direction,
          kInt64Max - 100000, kInt64Max);
    }
  }
}

template <typename SortFn1, typename SortFn2, typename GeneratorFn>
void RunAlgorithmComparisonTest(const std::string& sort1_name,
                                const std::string& sort2_name,
                                const std::string& gen_name, int size,
                                const RadixSortDirection Direction,
                                int64_t min_val, int64_t max_val) {
  GeneratorFn generator;
  auto data = generator(size);

  auto copy1 = data;
  absl::Time start1 = absl::Now();
  if (Direction == RadixSortDirection::kIncreasing) {
    SortFn1{}.template operator()<RadixSortDirection::kIncreasing>(
        copy1, min_val, max_val);
  } else {
    SortFn1{}.template operator()<RadixSortDirection::kDecreasing>(
        copy1, min_val, max_val);
  }
  absl::Duration time1 = absl::Now() - start1;

  auto copy2 = data;
  absl::Time start2 = absl::Now();
  if (Direction == RadixSortDirection::kIncreasing) {
    SortFn2{}.template operator()<RadixSortDirection::kIncreasing>(
        copy2, min_val, max_val);
  } else {
    SortFn2{}.template operator()<RadixSortDirection::kDecreasing>(
        copy2, min_val, max_val);
  }
  absl::Duration time2 = absl::Now() - start2;

  double time1_us = absl::ToDoubleMicroseconds(time1);
  double time2_us = absl::ToDoubleMicroseconds(time2);
  double ratio = time2_us > 0 ? time1_us / time2_us : 0.0;

  ABSL_LOG(INFO) << ", " << sort1_name << " vs " << sort2_name << ", "
                 << gen_name << ", " << size << ", "
                 << (Direction == RadixSortDirection::kIncreasing ? "1" : "0")
                 << ", " << time1 << ", " << time2 << ", ratio: " << ratio;
}

TEST(AutoRadixSortTest, AlgorithmComparison) {
  const std::vector<int> sizes = {100, 1000, 10000, 100000, 1000000};
  const std::vector<RadixSortDirection> kDirections = {
      RadixSortDirection::kIncreasing, RadixSortDirection::kDecreasing};
  const int64_t kInt64Max = std::numeric_limits<int64_t>::max();

  for (const int size : sizes) {
    for (const RadixSortDirection direction : kDirections) {
      RunAlgorithmComparisonTest<AutoRadixSortWrapper,
                                 AutoHistogramRadixSortWrapper, Int64NonNeg>(
          "AutoRadixSort", "AutoHistogramRadixSort", "Int64NonNeg", size,
          direction, 0, kInt64Max);
      RunAlgorithmComparisonTest<AutoRadixSortWrapper,
                                 AutoHistogramRadixSortWrapper, Int64Small>(
          "AutoRadixSort", "AutoHistogramRadixSort", "Int64Small", size,
          direction, 0, 100000);
      RunAlgorithmComparisonTest<AutoRadixSortWrapper,
                                 AutoHistogramRadixSortWrapper,
                                 Int64MaxMinusSmall>(
          "AutoRadixSort", "AutoHistogramRadixSort", "Int64MaxMinusSmall", size,
          direction, kInt64Max - 100000, kInt64Max);
    }
  }
}

TEST(RangeRadixSortTest, AlgorithmComparison) {
  const std::vector<int> sizes = {100, 1000, 10000, 100000, 1000000};
  const std::vector<RadixSortDirection> kDirections = {
      RadixSortDirection::kIncreasing, RadixSortDirection::kDecreasing};
  const int64_t kInt64Max = std::numeric_limits<int64_t>::max();

  for (const int size : sizes) {
    for (const RadixSortDirection direction : kDirections) {
      RunAlgorithmComparisonTest<RangeRadixSortWrapper,
                                 RangeHistogramRadixSortWrapper, Int64NonNeg>(
          "RangeRadixSort", "RangeHistogramRadixSort", "Int64NonNeg", size,
          direction, 0, kInt64Max);
      RunAlgorithmComparisonTest<RangeRadixSortWrapper,
                                 RangeHistogramRadixSortWrapper, Int64Small>(
          "RangeRadixSort", "RangeHistogramRadixSort", "Int64Small", size,
          direction, 0, 100000);
      RunAlgorithmComparisonTest<RangeRadixSortWrapper,
                                 RangeHistogramRadixSortWrapper,
                                 Int64MaxMinusSmall>(
          "RangeRadixSort", "RangeHistogramRadixSort", "Int64MaxMinusSmall",
          size, direction, kInt64Max - 100000, kInt64Max);
    }
  }
}

TEST(IndirectAutoRadixSortTest, SortMoveOnlyTypesWithIndependentKeys) {
  struct MoveOnlyStruct {
    std::unique_ptr<int> val;
    bool operator==(const MoveOnlyStruct& other) const {
      if (val == nullptr || other.val == nullptr) {
        return val == other.val;
      }
      return *val == *other.val;
    }
  };
  std::vector<MoveOnlyStruct> container;
  container.push_back({std::make_unique<int>(10)});
  container.push_back({std::make_unique<int>(20)});
  container.push_back({std::make_unique<int>(30)});

  std::vector<double> key = {3.0, 1.0, 2.0};
  ComputeAutoRadixSortPermutation(container, [&](const std::size_t i) {
    return key[i];
  }).ApplyTo(container);
  EXPECT_EQ(*container[0].val, 20);
  EXPECT_EQ(*container[1].val, 30);
  EXPECT_EQ(*container[2].val, 10);
}

struct MyValueStruct {
  std::string name;
  bool operator==(const MyValueStruct& other) const {
    return name == other.name;
  }
};

std::pair<std::vector<MyValueStruct>, std::vector<double>>
GenerateSmallSeparateArrays() {
  std::vector<MyValueStruct> container = {{"A"}, {"B"}, {"C"}};
  std::vector<double> key = {3.0, 1.0, 2.0};
  return {std::move(container), std::move(key)};
}

TEST(IndirectAutoRadixSortTest, SortStructsWithIndependentKeys) {
  auto [container, key] = GenerateSmallSeparateArrays();
  ComputeAutoRadixSortPermutation(container, [&](const std::size_t i) {
    return key[i];
  }).ApplyTo(container);
  EXPECT_THAT(container, ElementsAre(MyValueStruct{"B"}, MyValueStruct{"C"},
                                     MyValueStruct{"A"}));
}

TEST(IndirectRangeRadixSortTest, SortStructsWithIndependentKeysRange) {
  auto [container, key] = GenerateSmallSeparateArrays();
  ComputeRangeRadixSortPermutation(1.0, 3.0, container,
                                   [&](const std::size_t i) { return key[i]; })
      .ApplyTo(container);
  EXPECT_THAT(container, ElementsAre(MyValueStruct{"B"}, MyValueStruct{"C"},
                                     MyValueStruct{"A"}));
}

TEST(IndirectAutoHistogramRadixSortTest, SortStructsWithIndependentKeys) {
  auto [container, key] = GenerateSmallSeparateArrays();
  ComputeAutoHistogramRadixSortPermutation(container, [&](const std::size_t i) {
    return key[i];
  }).ApplyTo(container);
  EXPECT_THAT(container, ElementsAre(MyValueStruct{"B"}, MyValueStruct{"C"},
                                     MyValueStruct{"A"}));
}

TEST(IndirectRangeHistogramRadixSortTest, SortStructsWithIndependentKeysRange) {
  auto [container, key] = GenerateSmallSeparateArrays();
  ComputeRangeHistogramRadixSortPermutation(
      1.0, 3.0, container, [&](const std::size_t i) { return key[i]; })
      .ApplyTo(container);
  EXPECT_THAT(container, ElementsAre(MyValueStruct{"B"}, MyValueStruct{"C"},
                                     MyValueStruct{"A"}));
}

std::pair<std::vector<MyValueStruct>, std::vector<double>>
GenerateSeparateArrays(int n) {
  std::vector<MyValueStruct> container(n);
  std::vector<double> key(n);
  absl::BitGen bitgen;
  for (int i = 0; i < n; ++i) {
    container[i] = MyValueStruct{std::to_string(i)};
    key[i] = absl::Uniform<double>(bitgen, 0.0, 1000.0);
  }
  return {std::move(container), std::move(key)};
}

template <typename SortFn>
void VerifyIndirectSortLarge(SortFn sort_fn, RadixSortDirection direction) {
  constexpr std::size_t n = 10000;
  auto [container, key] = GenerateSeparateArrays(n);

  std::vector<MyValueStruct> expected = container;
  std::vector<std::size_t> permutation(n);
  std::iota(permutation.begin(), permutation.end(), 0);
  std::stable_sort(permutation.begin(), permutation.end(),
                   [&](const std::size_t a, const std::size_t b) {
                     if (direction == RadixSortDirection::kIncreasing) {
                       return key[a] < key[b];
                     } else {
                       return key[a] > key[b];
                     }
                   });
  std::vector<MyValueStruct> expected_sorted(n);
  for (std::size_t i = 0; i < n; ++i) {
    expected_sorted[i] = expected[permutation[i]];
  }

  RadixSortPermutation perm = sort_fn(container, key);
  perm.ApplyTo(container);
  EXPECT_EQ(container, expected_sorted);
}

TEST(IndirectAutoRadixSortTest, SortLargeIncreasing) {
  VerifyIndirectSortLarge(
      [](const auto& container, const auto& key) {
        return ComputeAutoRadixSortPermutation<RadixSortDirection::kIncreasing>(
            container, [&](const std::size_t i) { return key[i]; });
      },
      RadixSortDirection::kIncreasing);
}

TEST(IndirectAutoRadixSortTest, SortLargeDecreasing) {
  VerifyIndirectSortLarge(
      [](const auto& container, const auto& key) {
        return ComputeAutoRadixSortPermutation<RadixSortDirection::kDecreasing>(
            container, [&](const std::size_t i) { return key[i]; });
      },
      RadixSortDirection::kDecreasing);
}

TEST(IndirectRangeRadixSortTest, SortLargeIncreasing) {
  VerifyIndirectSortLarge(
      [](const auto& container, const auto& key) {
        return ComputeRangeRadixSortPermutation<
            RadixSortDirection::kIncreasing>(
            0.0, 1000.0, container,
            [&](const std::size_t i) { return key[i]; });
      },
      RadixSortDirection::kIncreasing);
}

TEST(IndirectRangeRadixSortTest, SortLargeDecreasing) {
  VerifyIndirectSortLarge(
      [](const auto& container, const auto& key) {
        return ComputeRangeRadixSortPermutation<
            RadixSortDirection::kDecreasing>(
            0.0, 1000.0, container,
            [&](const std::size_t i) { return key[i]; });
      },
      RadixSortDirection::kDecreasing);
}

TEST(IndirectAutoHistogramRadixSortTest, SortLargeIncreasing) {
  VerifyIndirectSortLarge(
      [](const auto& container, const auto& key) {
        return ComputeAutoHistogramRadixSortPermutation<
            RadixSortDirection::kIncreasing>(
            container, [&](const std::size_t i) { return key[i]; });
      },
      RadixSortDirection::kIncreasing);
}

TEST(IndirectAutoHistogramRadixSortTest, SortLargeDecreasing) {
  VerifyIndirectSortLarge(
      [](const auto& container, const auto& key) {
        return ComputeAutoHistogramRadixSortPermutation<
            RadixSortDirection::kDecreasing>(
            container, [&](const std::size_t i) { return key[i]; });
      },
      RadixSortDirection::kDecreasing);
}

TEST(IndirectRangeHistogramRadixSortTest, SortLargeRangeIncreasing) {
  VerifyIndirectSortLarge(
      [](const auto& container, const auto& key) {
        return ComputeRangeHistogramRadixSortPermutation<
            RadixSortDirection::kIncreasing>(
            0.0, 1000.0, container,
            [&](const std::size_t i) { return key[i]; });
      },
      RadixSortDirection::kIncreasing);
}

TEST(IndirectRangeHistogramRadixSortTest, SortLargeRangeDecreasing) {
  VerifyIndirectSortLarge(
      [](const auto& container, const auto& key) {
        return ComputeRangeHistogramRadixSortPermutation<
            RadixSortDirection::kDecreasing>(
            0.0, 1000.0, container,
            [&](const std::size_t i) { return key[i]; });
      },
      RadixSortDirection::kDecreasing);
}

TEST(RadixSortPermutationTest, ValidationChecks) {
  RadixSortPermutation perm1(5);
  EXPECT_TRUE(perm1.IsValid());

  RadixSortPermutation perm2(std::vector<std::size_t>{0, 2, 1, 4, 3});
  EXPECT_TRUE(perm2.IsValid());

  // Duplicate index
  RadixSortPermutation perm3(std::vector<std::size_t>{0, 1, 1, 3});
  EXPECT_FALSE(perm3.IsValid());

  // Out of bounds index
  RadixSortPermutation perm4(std::vector<std::size_t>{0, 1, 2, 4});
  EXPECT_FALSE(perm4.IsValid());
}

TEST(IndirectAutoRadixSortTest, SortCascadedStructs) {
  struct CascadedStruct {
    int primary;
    int secondary;
    bool operator==(const CascadedStruct& other) const {
      return primary == other.primary && secondary == other.secondary;
    }
  };
  std::vector<CascadedStruct> container = {{2, 10}, {1, 20}, {2, 5}, {1, 5}};
  // Sorting by secondary key first, then primary key.
  RadixSortPermutation perm(container.size());
  UpdateAutoRadixSortPermutation(perm, container, [&](const std::size_t i) {
    return container[i].secondary;
  });
  UpdateAutoRadixSortPermutation(perm, container, [&](const std::size_t i) {
    return container[i].primary;
  });

  perm.ApplyTo(container);

  EXPECT_THAT(container,
              ElementsAre(CascadedStruct{1, 5}, CascadedStruct{1, 20},
                          CascadedStruct{2, 5}, CascadedStruct{2, 10}));
}

}  // namespace
}  // namespace operations_research
