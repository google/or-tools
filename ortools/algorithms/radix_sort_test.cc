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

#include "ortools/algorithms/radix_sort.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <random>
#include <type_traits>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/log/log.h"
#include "absl/numeric/bits.h"
#include "absl/numeric/int128.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/random/distributions.h"
#include "absl/random/random.h"
#include "absl/types/span.h"
#include "benchmark/benchmark.h"
#include "gtest/gtest.h"
#include "ortools/base/dump_vars.h"
#include "ortools/base/gmock.h"
#include "ortools/base/mathutil.h"

namespace operations_research {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

template <typename T>
class NumBitsForZeroToTest : public ::testing::Test {};

TYPED_TEST_SUITE_P(NumBitsForZeroToTest);

TYPED_TEST_P(NumBitsForZeroToTest, CorrectnessStressTest) {
  absl::BitGen rng;
  constexpr int kNumTests = 1'000'000;
  for (int test = 0; test < kNumTests; ++test) {
    const TypeParam max_val = absl::LogUniform<TypeParam>(
        rng, 0, std::numeric_limits<TypeParam>::max());
    const int num_bits = NumBitsForZeroTo(max_val);
    EXPECT_LE(absl::int128{max_val}, absl::int128{1} << num_bits);
  }
}

REGISTER_TYPED_TEST_SUITE_P(NumBitsForZeroToTest, CorrectnessStressTest);
using IntTypes = ::testing::Types<int, uint32_t, int64_t, uint64_t, int16_t,
                                  uint16_t, int8_t, uint8_t>;

INSTANTIATE_TYPED_TEST_SUITE_P(My, NumBitsForZeroToTest, IntTypes);

// If T is a floating-point type, ignores min_val / max_val.
template <typename T>
std::vector<T> RandomValues(absl::BitGenRef rng, size_t size,
                            bool allow_negative, std::optional<T> max_abs_val) {
  std::vector<T> values(size);
  for (T& v : values) {
    if constexpr (std::is_integral_v<T>) {
      v = absl::Uniform<T>(absl::IntervalClosedClosed, rng, 0,
                           max_abs_val.value_or(std::numeric_limits<T>::max()));
    } else {
      v = absl::Uniform<double>(rng, 1.0, 2.0) *
          std::exp2(absl::Uniform(rng, std::numeric_limits<T>::min_exponent,
                                  std::numeric_limits<T>::max_exponent));
    }
    if (allow_negative && absl::Bernoulli(rng, 0.5)) v = -v;
  }
  return values;
}

// To facilitate the testing of the RadixSortTpl<> template with various
// radix_width and num_passes, we use this wrapper, non-template function
// (except for the value type T) in the unit test below.
template <typename T>
void RadixSortForTest(T* values, size_t size, int radix_width, int num_passes);

// We don't test all radix widths, primarily because it would lead to absurd
// compilation times (> 5min, problematic with blaze) because of the
// "template explosion" of RadixSortForTest.
// These tested widths must of course cover the widths used by the main
// RadixSort() in the .h, and a few more to stress-test the logic.
constexpr int kTestedRadixWidths[] = {8, 10, 11, 13, 16};

int RandomRadixWidth(absl::BitGenRef rng) {
  return kTestedRadixWidths[absl::Uniform<int>(rng, 0,
                                               std::size(kTestedRadixWidths))];
}

constexpr size_t kMaxSizeForSmallStressTests = 300;

template <typename T>
class RadixSortTest : public ::testing::Test {};

TYPED_TEST_SUITE_P(RadixSortTest);

TYPED_TEST_P(RadixSortTest, RandomizedCorrectnessTestAgainstStdSortSmallSizes) {
  constexpr int kNumTests = 100'000;
  absl::BitGen rng;
  // Generate the test sizes and sort them, so that we debug the smallest
  // possible array when there's a bug.
  std::vector<size_t> sizes(kNumTests);
  for (size_t& size : sizes) {
    size = absl::LogUniform<size_t>(rng, 2, kMaxSizeForSmallStressTests);
  }
  absl::c_sort(sizes);
  for (int test = 0; test < kNumTests; ++test) {
    // Draw the size of the test array, and whether we'll allow negative values.
    const size_t size = sizes[test];
    const bool allow_negative =
        std::is_signed_v<TypeParam> && absl::Bernoulli(rng, 0.5);

    // Will we use the standard RadixSort() or the RadixSortTpl<>() variant?
    const bool use_main_radix_sort = absl::Bernoulli(rng, 0.5);
    const bool use_num_bits = std::is_integral_v<TypeParam> &&
                              use_main_radix_sort && !allow_negative &&
                              absl::Bernoulli(rng, 0.5);

    // We potentially test the "power usage" of calling RadixSortTpl<> with
    // radix_width * num_passes < num_bits(TypeParam), when the actual values
    // in the array don't use all the bits of the value type.
    std::optional<TypeParam> max_abs_val;  // If set, the max absolute value.
    int val_bits = sizeof(TypeParam) * 8;  // Number of bits used by values.
    if constexpr (std::is_integral_v<TypeParam>) {
      max_abs_val = absl::LogUniform<TypeParam>(
          rng, 0, std::numeric_limits<TypeParam>::max());
      if (!allow_negative) {
        typedef std::make_unsigned_t<TypeParam> U;
        val_bits = std::numeric_limits<U>::digits -
                   absl::countl_zero(static_cast<U>(max_abs_val.value()));
      }
    }

    // Generate the data.
    std::vector<TypeParam> unsorted_values =
        RandomValues(rng, size, allow_negative, max_abs_val);
    std::vector<TypeParam> sorted_values = unsorted_values;

    // Sort.
    int radix_width = -1;
    int num_passes = -1;
    if (use_main_radix_sort) {
      if (use_num_bits) {
        RadixSort(absl::MakeSpan(sorted_values),
                  NumBitsForZeroTo(max_abs_val.value()));
      } else {
        RadixSort(absl::MakeSpan(sorted_values));
      }
    } else {
      // Draw random (radix_width, num_passes) pairs until we get a valid one.
      constexpr int kMaxNumPasses = 8;
      for (int i = 0;; ++i) {
        radix_width = RandomRadixWidth(rng);
        num_passes = MathUtil::CeilOfRatio(val_bits, radix_width);
        if (num_passes <= kMaxNumPasses) break;  // valid pair!
        ASSERT_LT(i, 1000)
            << "Couldn't generate a valid (radix_width, num_passes) pair. ";
      }
      RadixSortForTest(sorted_values.data(), sorted_values.size(), radix_width,
                       num_passes);
    }

    std::vector<TypeParam> expected_values = unsorted_values;
    absl::c_sort(expected_values);
    ASSERT_TRUE(sorted_values == expected_values)
        << DUMP_VARS(test, use_main_radix_sort, radix_width, num_passes, size,
                     allow_negative, use_num_bits, val_bits, max_abs_val,
                     unsorted_values, sorted_values, expected_values);
  }
}

TYPED_TEST_P(RadixSortTest, SizeZeroAndOne) {
  absl::BitGen rng;
  constexpr int kNumTests = 1'000;
  for (int test = 0; test < kNumTests; ++test) {
    std::vector<TypeParam> values;

    RadixSort(absl::MakeSpan(values));
    ASSERT_THAT(values, IsEmpty());

    // Now test RadixSortTpl<radix_width, num_passes>.
    // Draw random (radix_width, num_passes) pairs until we get a valid one.
    constexpr int val_bits = sizeof(TypeParam) * 8;
    constexpr int kMaxNumPasses = 8;
    int radix_width = -1;
    int num_passes = -1;
    for (int i = 0;; ++i) {
      radix_width = RandomRadixWidth(rng);
      num_passes = MathUtil::CeilOfRatio(val_bits, radix_width);
      if (num_passes <= kMaxNumPasses) break;  // valid pair!
      ASSERT_LT(i, 1000)
          << "Couldn't generate a valid (radix_width, num_passes) pair. ";
    }
    SCOPED_TRACE(DUMP_VARS(radix_width, num_passes));
    RadixSortForTest(values.data(), 0, radix_width, num_passes);
    ASSERT_THAT(values, IsEmpty());

    // Add a single value, and test again.
    const TypeParam value = absl::Uniform<TypeParam>(
        absl::IntervalClosedClosed, rng, std::numeric_limits<TypeParam>::min(),
        std::numeric_limits<TypeParam>::max());
    SCOPED_TRACE(DUMP_VARS(value));
    values.push_back(value);
    RadixSort(absl::MakeSpan(values));
    ASSERT_THAT(values, ElementsAre(value));
    RadixSortForTest(values.data(), 1, radix_width, num_passes);
    ASSERT_THAT(values, ElementsAre(value));
  }
}

constexpr size_t kMaxSizeForLargeStressTests = 32 << 20;

TYPED_TEST_P(RadixSortTest, RandomizedCorrectnessTestAgainstStdSortLargeSizes) {
  // Similar to the stress test above, but for larger sizes.
  constexpr int kNumTests = 10;
  absl::BitGen rng;
  for (int test = 0; test < kNumTests; ++test) {
    const size_t size = static_cast<size_t>(
        std::exp(absl::Uniform(rng, std::log(kMaxSizeForSmallStressTests),
                               std::log(kMaxSizeForLargeStressTests))));
    const bool allow_negative = absl::Bernoulli(rng, 0.5);
    std::vector<TypeParam> values =
        RandomValues<TypeParam>(rng, size, allow_negative, /*max_abs_val=*/{});
    const bool use_main_radix_sort = absl::Bernoulli(rng, 0.5);
    const bool use_num_bits = std::is_integral_v<TypeParam> &&
                              use_main_radix_sort && !allow_negative &&
                              absl::Bernoulli(rng, 0.5);

    int radix_width = -1;
    int num_passes = -1;
    if (use_main_radix_sort) {
      if (use_num_bits) {
        RadixSort(
            absl::MakeSpan(values),
            NumBitsForZeroTo(size == 0 ? 1 : *absl::c_max_element(values)));
      } else {
        RadixSort(absl::MakeSpan(values));
      }
    } else {
      radix_width = RandomRadixWidth(rng);
      num_passes =
          MathUtil::CeilOfRatio<int>(sizeof(TypeParam) * 8, radix_width);
      RadixSortForTest(values.data(), values.size(), radix_width, num_passes);
    }
    // Contrary to the 'small' stress test, we don't log the data upon failure.
    ASSERT_TRUE(absl::c_is_sorted(values))
        << DUMP_VARS(test, use_main_radix_sort, radix_width, num_passes, size,
                     allow_negative, use_num_bits);
  }
}

REGISTER_TYPED_TEST_SUITE_P(RadixSortTest, SizeZeroAndOne,
                            RandomizedCorrectnessTestAgainstStdSortSmallSizes,
                            RandomizedCorrectnessTestAgainstStdSortLargeSizes);
using MyTypes = ::testing::Types<int, uint32_t, int64_t, uint64_t, int16_t,
                                 uint16_t, int8_t, uint8_t, double, float>;

INSTANTIATE_TYPED_TEST_SUITE_P(My, RadixSortTest, MyTypes);

// _____________________________________________________________________________
// Benchmarks.

template <typename T>
std::vector<T> SortedValues(size_t size) {
  const T offset = std::is_signed_v<T> ? -static_cast<T>(size) / 2 : T{0};
  std::vector<T> values(size);
  for (size_t i = 0; i < size; ++i) values[i] = i + offset;
  return values;
}

enum Algo {
  kStdSort,
  kRadixSortTpl,
  kRadixSortKnownMax,
  kRadixSortComputeMax,
  kRadixSortWorst,
};

enum InputOrder {
  kRandom,
  kSorted,
  kAlmostSorted,
  kReverseSorted,
};

template <Algo algo, typename T, InputOrder input_order, int radix_width,
          int num_passes>
void BM_Sort(benchmark::State& state) {
  const size_t size = state.range(0);
  std::optional<T> max_abs_val;
  if constexpr (!std::is_signed_v<T> &&
                radix_width * num_passes < std::numeric_limits<T>::digits) {
    max_abs_val = (T{1} << (radix_width * num_passes)) - 1;
  }
  std::mt19937 rng(1234);
  std::vector<T> values =
      input_order == kRandom
          ? RandomValues(rng, size, std::is_signed_v<T>, max_abs_val)
          : SortedValues<T>(size);
  if (input_order == kReverseSorted) {
    absl::c_reverse(values);
  }
  if (input_order == kAlmostSorted) {
    const int p1 = absl::Uniform<int>(rng, 0, size);
    const int p2 = absl::Uniform<int>(rng, 0, size);
    std::swap(values[p1], values[p2]);
  }
  std::vector<T> to_sort = values;
  for (auto _ : state) {
    to_sort = values;
    if constexpr (algo == kStdSort) {
      absl::c_sort(to_sort);
    } else if constexpr (algo == kRadixSortTpl) {
      absl::Span<T> span{to_sort.data(), to_sort.size()};
      RadixSortTpl<T, radix_width, num_passes>(span);
    } else if constexpr (algo == kRadixSortKnownMax) {
      absl::Span<T> span = absl::MakeSpan(to_sort);
      RadixSort(span, NumBitsForZeroTo(
                          max_abs_val.value_or(std::numeric_limits<T>::max())));
    } else if constexpr (algo == kRadixSortComputeMax) {
      absl::Span<T> span{to_sort.data(), to_sort.size()};
      RadixSort(span, NumBitsForZeroTo(
                          size == 0 ? 1 : *absl::c_max_element(to_sort)));
    } else if constexpr (algo == kRadixSortWorst) {
      absl::Span<T> span{to_sort.data(), to_sort.size()};
      RadixSort(span);
    } else {
      LOG(DFATAL) << "Unsupported algo: " << algo;
    }
    benchmark::DoNotOptimize(to_sort);
  }
  state.SetItemsProcessed(state.iterations() * size);
}

// For std::sort(), we don't benchmark with much detail past 32k, because it
// reaches its "very" slow part where the item/s throuput is almost horizontal.
BENCHMARK(BM_Sort<kStdSort, int, kRandom, 1, 1>)
    ->RangeMultiplier(2)
    ->Range(1, 32 << 10)
    ->Arg(128 << 10)
    ->Arg(1 << 20)
    ->Arg(8 << 20);

BENCHMARK(BM_Sort<kStdSort, int64_t, kRandom, 1, 1>)
    ->RangeMultiplier(2)
    ->Range(1, 32 << 10)
    ->Arg(128 << 10)
    ->Arg(1 << 20)
    ->Arg(8 << 20);

BENCHMARK(BM_Sort<kStdSort, int, kSorted, 1, 1>)
    ->RangeMultiplier(2)
    ->Range(1, 128 << 10);

BENCHMARK(BM_Sort<kStdSort, int, kReverseSorted, 1, 1>)
    ->RangeMultiplier(2)
    ->Range(1, 128 << 10);

BENCHMARK(BM_Sort<kStdSort, int, kAlmostSorted, 1, 1>)
    ->RangeMultiplier(2)
    ->Range(1, 128 << 10);

BENCHMARK(BM_Sort<kRadixSortTpl, uint32_t, kRandom, /*radix_width=*/8,
                  /*num_passes=*/4>)
    ->RangeMultiplier(2)
    ->Range(16, 2048);
BENCHMARK(BM_Sort<kRadixSortTpl, uint32_t, kRandom, /*radix_width=*/11,
                  /*num_passes=*/3>)
    ->RangeMultiplier(2)
    ->Range(256, 32 << 20);
BENCHMARK(BM_Sort<kRadixSortTpl, uint32_t, kRandom, /*radix_width=*/16,
                  /*num_passes=*/2>)
    ->RangeMultiplier(2)
    ->Range(128 << 10, 32 << 20);

BENCHMARK(BM_Sort<kRadixSortTpl, int, kRandom, /*radix_width=*/8,
                  /*num_passes=*/4>)
    ->RangeMultiplier(2)
    ->Range(16, 2048);
BENCHMARK(BM_Sort<kRadixSortTpl, int, kRandom, /*radix_width=*/11,
                  /*num_passes=*/3>)
    ->RangeMultiplier(2)
    ->Range(256, 32 << 20);
BENCHMARK(BM_Sort<kRadixSortTpl, int, kRandom, /*radix_width=*/16,
                  /*num_passes=*/2>)
    ->RangeMultiplier(2)
    ->Range(128 << 10, 32 << 20);

BENCHMARK(BM_Sort<kRadixSortKnownMax, int, kRandom, /*radix_width=*/16,
                  /*num_passes=*/2>)
    ->RangeMultiplier(2)
    ->Range(128 << 10, 32 << 20);
BENCHMARK(BM_Sort<kRadixSortComputeMax, int, kRandom, /*radix_width=*/16,
                  /*num_passes=*/2>)
    ->RangeMultiplier(2)
    ->Range(128 << 10, 32 << 20);
BENCHMARK(BM_Sort<kRadixSortWorst, int, kRandom, /*radix_width=*/16,
                  /*num_passes=*/2>)
    ->RangeMultiplier(2)
    ->Range(128 << 10, 32 << 20);

BENCHMARK(BM_Sort<kRadixSortTpl, float, kRandom, /*radix_width=*/8,
                  /*num_passes=*/4>)
    ->RangeMultiplier(2)
    ->Range(16, 2048);
BENCHMARK(BM_Sort<kRadixSortTpl, float, kRandom, /*radix_width=*/11,
                  /*num_passes=*/3>)
    ->RangeMultiplier(2)
    ->Range(256, 32 << 20);
BENCHMARK(BM_Sort<kRadixSortTpl, float, kRandom, /*radix_width=*/16,
                  /*num_passes=*/2>)
    ->RangeMultiplier(2)
    ->Range(128 << 10, 32 << 20);

BENCHMARK(BM_Sort<kRadixSortTpl, uint64_t, kRandom, /*radix_width=*/11,
                  /*num_passes=*/6>)
    ->RangeMultiplier(2)
    ->Range(2048, 8 << 20)
    ->Arg(32 << 20)
    ->Arg(128 << 20);
BENCHMARK(BM_Sort<kRadixSortTpl, uint64_t, kRandom, /*radix_width=*/13,
                  /*num_passes=*/5>)
    ->RangeMultiplier(2)
    ->Range(2048, 8 << 20)
    ->Arg(32 << 20)
    ->Arg(128 << 20);
BENCHMARK(BM_Sort<kRadixSortTpl, uint64_t, kRandom, /*radix_width=*/16,
                  /*num_passes=*/4>)
    ->RangeMultiplier(2)
    ->Range(128 << 10, 8 << 20)
    ->Arg(32 << 20)
    ->Arg(128 << 20);
BENCHMARK(BM_Sort<kRadixSortTpl, uint64_t, kRandom, /*radix_width=*/22,
                  /*num_passes=*/3>)
    ->RangeMultiplier(2)
    ->Range(128 << 10, 8 << 20)
    ->Arg(32 << 20)
    ->Arg(128 << 20);

BENCHMARK(BM_Sort<kRadixSortTpl, int64_t, kRandom, /*radix_width=*/11,
                  /*num_passes=*/6>)
    ->RangeMultiplier(2)
    ->Range(2048, 8 << 20)
    ->Arg(32 << 20)
    ->Arg(128 << 20);
BENCHMARK(BM_Sort<kRadixSortTpl, int64_t, kRandom, /*radix_width=*/13,
                  /*num_passes=*/5>)
    ->RangeMultiplier(2)
    ->Range(2048, 8 << 20)
    ->Arg(32 << 20)
    ->Arg(128 << 20);
BENCHMARK(BM_Sort<kRadixSortTpl, int64_t, kRandom, /*radix_width=*/16,
                  /*num_passes=*/4>)
    ->RangeMultiplier(2)
    ->Range(128 << 10, 8 << 20)
    ->Arg(32 << 20)
    ->Arg(128 << 20);
BENCHMARK(BM_Sort<kRadixSortTpl, int64_t, kRandom, /*radix_width=*/22,
                  /*num_passes=*/3>)
    ->RangeMultiplier(2)
    ->Range(128 << 10, 8 << 20)
    ->Arg(32 << 20)
    ->Arg(128 << 20);

BENCHMARK(BM_Sort<kRadixSortTpl, double, kRandom, /*radix_width=*/11,
                  /*num_passes=*/6>)
    ->RangeMultiplier(2)
    ->Range(2048, 8 << 20)
    ->Arg(32 << 20)
    ->Arg(128 << 20);
BENCHMARK(BM_Sort<kRadixSortTpl, double, kRandom, /*radix_width=*/13,
                  /*num_passes=*/5>)
    ->RangeMultiplier(2)
    ->Range(2048, 8 << 20)
    ->Arg(32 << 20)
    ->Arg(128 << 20);
BENCHMARK(BM_Sort<kRadixSortTpl, double, kRandom, /*radix_width=*/16,
                  /*num_passes=*/4>)
    ->RangeMultiplier(2)
    ->Range(128 << 10, 8 << 20)
    ->Arg(32 << 20)
    ->Arg(128 << 20);
BENCHMARK(BM_Sort<kRadixSortTpl, double, kRandom, /*radix_width=*/22,
                  /*num_passes=*/3>)
    ->RangeMultiplier(2)
    ->Range(128 << 10, 8 << 20)
    ->Arg(32 << 20)
    ->Arg(128 << 20);

// This is the template expansion cruft needed by RadixSortForTest. See usage.
template <typename T, int radix_width>
void RadixSortTpl2(absl::Span<T> values, int num_passes) {
  switch (num_passes) {
    case 0:
      return;
    case 1:
      RadixSortTpl<T, radix_width, 1>(values);
      return;
    case 2:
      RadixSortTpl<T, radix_width, 2>(values);
      return;
    case 3:
      RadixSortTpl<T, radix_width, 3>(values);
      return;
    case 4:
      RadixSortTpl<T, radix_width, 4>(values);
      return;
    case 5:
      RadixSortTpl<T, radix_width, 5>(values);
      return;
    case 6:
      RadixSortTpl<T, radix_width, 6>(values);
      return;
    case 7:
      RadixSortTpl<T, radix_width, 7>(values);
      return;
    case 8:
      RadixSortTpl<T, radix_width, 8>(values);
      return;
  }
  LOG(FATAL) << "Unsupported num_passes: " << num_passes;
}

template <typename T>
void RadixSortForTest(T* values, size_t size, int radix_width, int num_passes) {
  switch (radix_width) {
    case 8:
      RadixSortTpl2<T, 8>({values, size}, num_passes);
      return;
    case 10:
      RadixSortTpl2<T, 10>({values, size}, num_passes);
      return;
    case 11:
      RadixSortTpl2<T, 11>({values, size}, num_passes);
      return;
    case 13:
      RadixSortTpl2<T, 13>({values, size}, num_passes);
      return;
    case 16:
      RadixSortTpl2<T, 16>({values, size}, num_passes);
      return;
  }
  LOG(FATAL) << "Unsupported radix_width: " << radix_width;
}

}  // namespace
}  // namespace operations_research
