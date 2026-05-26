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

#include "ortools/util/zvector.h"

#include <array>
#include <cstdint>
#include <limits>
#include <random>

#include "absl/random/random.h"
#include "gtest/gtest.h"

namespace operations_research {

template <typename T>
void TestZVectorZeroIndex() {
  constexpr int64_t kZeroIndex = 0;
  constexpr int64_t kNonZeroIndex = 10;
  constexpr std::array kArrayBounds = {std::array{kZeroIndex, kZeroIndex},
                                       std::array{kZeroIndex, kNonZeroIndex},
                                       std::array{-kNonZeroIndex, kZeroIndex}};
  const T kValue = 1;
  for (int64_t i = 0; i < kArrayBounds.size(); ++i) {
    ZVector<T> array(kArrayBounds[i][0], kArrayBounds[i][1]);
    array.Set(kZeroIndex, kValue);
    EXPECT_EQ(kValue, array[kZeroIndex]);
  }
}

TEST(ZVectorTest, SingleZeroIndex) {
  TestZVectorZeroIndex<int8_t>();
  TestZVectorZeroIndex<int16_t>();
  TestZVectorZeroIndex<int32_t>();
  TestZVectorZeroIndex<int64_t>();
  TestZVectorZeroIndex<uint8_t>();
  TestZVectorZeroIndex<uint16_t>();
  TestZVectorZeroIndex<uint32_t>();
  TestZVectorZeroIndex<uint64_t>();
}

template <typename T>
void TestZeroLengthZVector() {
  ZVector<T> a;
  a.SetAll(1);
}

TEST(ZVectorTest, ZeroLength) {
  TestZeroLengthZVector<int8_t>();
  TestZeroLengthZVector<int16_t>();
  TestZeroLengthZVector<int32_t>();
  TestZeroLengthZVector<int64_t>();
  TestZeroLengthZVector<uint8_t>();
  TestZeroLengthZVector<uint16_t>();
  TestZeroLengthZVector<uint32_t>();
  TestZeroLengthZVector<uint64_t>();
}

template <typename T>
struct make_unsigned;
template <>
struct make_unsigned<int8_t> {
  typedef uint8_t type;
};
template <>
struct make_unsigned<int16_t> {
  typedef uint16_t type;
};
template <>
struct make_unsigned<int32_t> {
  typedef uint32_t type;
};
template <>
struct make_unsigned<int64_t> {
  typedef uint64_t type;
};
template <>
struct make_unsigned<uint8_t> {
  typedef uint8_t type;
};
template <>
struct make_unsigned<uint16_t> {
  typedef uint16_t type;
};
template <>
struct make_unsigned<uint32_t> {
  typedef uint32_t type;
};
template <>
struct make_unsigned<uint64_t> {
  typedef uint64_t type;
};

template <typename T>
T Random(std::mt19937* const randomizer) {
  // Using the unsigned version of the type guarantees modular arithmetic,
  // and avoids overflow on things such as INT_MAX - INT_MIN.
  typedef typename make_unsigned<T>::type U;
  const U kMin = std::numeric_limits<T>::min();
  const U kMax = std::numeric_limits<T>::max();
  return static_cast<T>(kMin +
                        absl::Uniform<uint64_t>(*randomizer, 0, kMax - kMin));
}

template <typename T>
void TestZVectorRangeValue() {
  const int64_t kMinIndex = -100000;
  const int64_t kMaxIndex = 100000;

  ZVector<T> array(kMinIndex, kMaxIndex);
  std::mt19937 randomizer(0);
  for (int64_t i = kMinIndex; i <= kMaxIndex; ++i) {
    T value = Random<T>(&randomizer);
    array.Set(i, value);
  }
  randomizer.seed(0);
  for (int64_t i = kMinIndex; i <= kMaxIndex; ++i) {
    T value = Random<T>(&randomizer);
    EXPECT_EQ(value, array[i]);
    EXPECT_EQ(value, array.Value(i));
  }
  array.SetAll(0);
  for (int64_t i = kMinIndex; i <= kMaxIndex; ++i) {
    EXPECT_EQ(0, array[i]);
    EXPECT_EQ(0, array.Value(i));
  }
}

TEST(ZVectorTest, Random) {
  TestZVectorRangeValue<int8_t>();
  TestZVectorRangeValue<int16_t>();
  TestZVectorRangeValue<int32_t>();
  TestZVectorRangeValue<int64_t>();
  TestZVectorRangeValue<uint8_t>();
  TestZVectorRangeValue<uint16_t>();
  TestZVectorRangeValue<uint32_t>();
  TestZVectorRangeValue<uint64_t>();
}

#ifndef NDEBUG
// Death depends on DCHECKs, so we do not check for death in opt mode.
TEST(ZVectorDeathTest, ReverseBounds) {
  const int64_t kMinIndex = 1000;
  const int64_t kMaxIndex = -1000;
  EXPECT_DEATH({ ZVector<int32_t>(kMinIndex, kMaxIndex); }, "");
}

TEST(ZVectorDeathTest, BoundOverflow) {
  const int64_t kMinIndex = -1000;
  const int64_t kMaxIndex = 1000;
  ZVector<int32_t> array = ZVector<int32_t>(kMinIndex, kMaxIndex);
  EXPECT_DEATH({ array.Set(kMaxIndex + 1, 12); }, "");
  EXPECT_DEATH({ array.Value(kMaxIndex + 1); }, "");
  EXPECT_DEATH({ array[kMaxIndex + 1]; }, "");
  EXPECT_DEATH({ array.Set(kMinIndex - 1, 12); }, "");
  EXPECT_DEATH({ array.Value(kMinIndex - 1); }, "");
  EXPECT_DEATH({ array[kMinIndex - 1]; }, "");
}
#endif

}  // namespace operations_research
