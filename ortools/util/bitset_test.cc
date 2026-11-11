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

#include "ortools/util/bitset.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <random>
#include <string>
#include <type_traits>
#include <vector>

#include "absl/base/optimization.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/random/random.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/log_severity.h"
#include "ortools/base/stl_util.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace {

using ::testing::ElementsAre;

////////////////////////////////////////////////////////////////////////////////
// Least / Most Significant Bit Position
////////////////////////////////////////////////////////////////////////////////

namespace bitset_index {

// An enum to select between LSBIndex and MSBIndex.
enum class Function { kLSB, kMSB };

// A type to describe a complete implementation.
template <Function function, typename T, internal::BitScanAlgorithm algo>
struct TestCase {
  using UIntT = T;
  static constexpr Function kFunction = function;
  static constexpr internal::BitScanAlgorithm kAlgo = algo;
  static int Compute(UIntT value) {
    if constexpr (function == Function::kLSB) {
      return internal::LeastSignificantBitPosition<algo>(value);
    } else {
      return internal::MostSignificantBitPosition<algo>(value);
    }
  }
};

// Shorthand for all available types.
using LSB_32_Basic =
    TestCase<Function::kLSB, uint32_t, internal::BitScanAlgorithm::kBasic>;
using LSB_32_DeBruijn =
    TestCase<Function::kLSB, uint32_t, internal::BitScanAlgorithm::kDeBruijn>;
using LSB_32_Std =
    TestCase<Function::kLSB, uint32_t, internal::BitScanAlgorithm::kStd>;
using LSB_64_Basic =
    TestCase<Function::kLSB, uint64_t, internal::BitScanAlgorithm::kBasic>;
using LSB_64_DeBruijn =
    TestCase<Function::kLSB, uint64_t, internal::BitScanAlgorithm::kDeBruijn>;
using LSB_64_Std =
    TestCase<Function::kLSB, uint64_t, internal::BitScanAlgorithm::kStd>;
using MSB_32_Basic =
    TestCase<Function::kMSB, uint32_t, internal::BitScanAlgorithm::kBasic>;
using MSB_32_Std =
    TestCase<Function::kMSB, uint32_t, internal::BitScanAlgorithm::kStd>;
using MSB_64_Basic =
    TestCase<Function::kMSB, uint64_t, internal::BitScanAlgorithm::kBasic>;
using MSB_64_Std =
    TestCase<Function::kMSB, uint64_t, internal::BitScanAlgorithm::kStd>;

// This help produce readable names for the typed tests.
class NameGenerator {
 public:
  static absl::string_view GetString(Function function) {
    switch (function) {
      case Function::kLSB:
        return "LSB";
      case Function::kMSB:
        return "MSB";
    }
    ABSL_UNREACHABLE();
  }
  static absl::string_view GetString(internal::BitScanAlgorithm algo) {
    switch (algo) {
      case internal::BitScanAlgorithm::kBasic:
        return "Basic";
      case internal::BitScanAlgorithm::kDeBruijn:
        return "DeBruijn";
      case internal::BitScanAlgorithm::kStd:
        return "Std";
    }
    ABSL_UNREACHABLE();
  }
  template <typename T>
  static std::string GetName(int) {
    using UIntT = typename T::UIntT;
    static_assert(
        std::is_same_v<UIntT, uint32_t> || std::is_same_v<UIntT, uint64_t>,
        "Unsupported type");
    return absl::StrFormat("%s(uint%d)_%s", GetString(T::kFunction),
                           sizeof(UIntT) * 8, GetString(T::kAlgo));
  }
};

template <typename TestCase>
class BitIndex : public testing::Test {};

using BitIndexTypes =
    ::testing::Types<LSB_32_Basic, LSB_32_DeBruijn, LSB_32_Std, LSB_64_Basic,
                     LSB_64_DeBruijn, LSB_64_Std, MSB_32_Basic, MSB_32_Std,
                     MSB_64_Basic, MSB_64_Std>;
TYPED_TEST_SUITE(BitIndex, BitIndexTypes, NameGenerator);

TYPED_TEST(BitIndex, SingleBit) {
  using TestCase = TypeParam;
  using UIntT = typename TestCase::UIntT;
  constexpr Function kFunction = TestCase::kFunction;
  static_assert(kFunction == Function::kLSB || kFunction == Function::kMSB);
  constexpr size_t kTypeBits = sizeof(UIntT) * 8;
  for (int i = 0; i < kTypeBits; ++i) {
    const UIntT bit_at_i = UIntT(1) << i;
    EXPECT_EQ(i, TestCase::Compute(bit_at_i));
  }
  // NOTE: `LeastSignificantBitPosition64` and `LeastSignificantBitPosition32`
  // are currently unspecified for the 0 value so we don't test it.
  if constexpr (kFunction == Function::kMSB) {
    EXPECT_EQ(0, TestCase::Compute(0));
  }
}

TYPED_TEST(BitIndex, TwoBits) {
  using TestCase = TypeParam;
  using UIntT = typename TestCase::UIntT;
  constexpr Function kFunction = TestCase::kFunction;
  static_assert(kFunction == Function::kLSB || kFunction == Function::kMSB);
  constexpr size_t kTypeBits = sizeof(UIntT) * 8;
  for (int i = 0; i < kTypeBits; ++i) {
    for (int j = 0; j < kTypeBits; ++j) {
      const UIntT bit_at_i = UIntT(1) << i;
      const UIntT bit_at_j = UIntT(1) << j;
      const UIntT combined = bit_at_i | bit_at_j;
      const int expected =
          kFunction == Function::kLSB ? std::min(i, j) : std::max(i, j);
      EXPECT_EQ(expected, TestCase::Compute(combined));
    }
  }
}

}  // namespace bitset_index

TEST(BitsetTest, BitPos64) {
  for (int i = 0; i < 128; ++i) {
    EXPECT_EQ(BitPos64(i), i % 64);
  }
}

TEST(BitsetTest, BitOffset) {
  for (int i = 0; i < 128; ++i) {
    EXPECT_EQ(BitOffset64(i), i / 64);
  }
}

TEST(BitsetTest, BitLength) {
  EXPECT_EQ(0, BitLength64(0));
  EXPECT_EQ(1, BitLength64(33));
  EXPECT_EQ(1, BitLength64(64));
  EXPECT_EQ(2, BitLength64(65));
}

TEST(BitsetTest, OneBit) {
  const uint32_t kBits32 = 0;
  EXPECT_EQ(sizeof(kBits32), sizeof(OneBit32(0)));
  const uint64_t kBits64 = 0;
  EXPECT_EQ(sizeof(kBits64), sizeof(OneBit64(0)));
}

TEST(BitsetTest, IsBitSet) {
  const uint64_t kBits64[2] = {0UL, 0xffffffffUL};
  EXPECT_FALSE(IsBitSet64(kBits64, 0));
  EXPECT_TRUE(IsBitSet64(kBits64, 64));
}

TEST(BitsetTest, BitShift) { EXPECT_EQ(1 << 6, BitShift64(1)); }

TEST(BitsetTest, SetBit64) {
  constexpr int kNumBuckets = 10;
  constexpr int kNumBits = kNumBuckets * 64;
  std::array<uint64_t, kNumBuckets> bits;
  for (int i = 0; i < kNumBits; ++i) {
    bits.fill(0);
    SetBit64(bits.data(), i);
    EXPECT_TRUE(IsBitSet64(bits.data(), i));
  }
}

TEST(BitsetTest, ClearBit64) {
  constexpr int kNumBuckets = 10;
  constexpr int kNumBits = kNumBuckets * 64;
  std::array<uint64_t, kNumBuckets> bits;
  for (int i = 0; i < kNumBits; ++i) {
    bits.fill(kAllBits64);
    ClearBit64(bits.data(), i);
    EXPECT_FALSE(IsBitSet64(bits.data(), i));
  }
}

TEST(BitsetTest, OneRange64) {
  for (int s = 0; s < 64; ++s) {
    for (int e = s; e < 64; ++e) {
      const uint64_t bits = OneRange64(s, e);
      for (int i = 0; i < 64; ++i) {
        EXPECT_EQ(i >= s && i <= e, IsBitSet64(&bits, i));
      }
    }
  }
}

TEST(BitsetTest, IntervalUp64) {
  for (int i = 0; i < 64; ++i) {
    const uint64_t bits = IntervalUp64(i);
    EXPECT_EQ(i, LeastSignificantBitPosition64(bits));
    EXPECT_EQ(64 - i, std::popcount(bits));
  }
}

TEST(BitsetTest, IntervalDown64) {
  for (int i = 0; i < 64; ++i) {
    const uint64_t bits = IntervalDown64(i);
    EXPECT_EQ(i, MostSignificantBitPosition64(bits));
    EXPECT_EQ(i + 1, std::popcount(bits));
  }
}

TEST(BitsetTest, BitCountRange64) {
  absl::BitGen randomizer;
  constexpr int kTests = 1000;
  constexpr int kNumBuckets = 10;
  constexpr int kNumBits = kNumBuckets * 64;
  std::array<uint64_t, kNumBuckets> bits;
  for (int i = 0; i < kTests; ++i) {
    bits.fill(0);
    const int num_to_set = absl::LogUniform(randomizer, 0, kNumBits);
    for (int j = 0; j < num_to_set; ++j) {
      SetBit64(bits.data(), absl::Uniform(randomizer, 0, kNumBits));
    }
    int s = absl::Uniform(randomizer, 0, kNumBits);
    int e = (i % 2 == 0)
                ? absl::Uniform(randomizer, 0, kNumBits)
                : std::min(kNumBits - 1, s + absl::Uniform(randomizer, 0, 64));
    if (s > e) {
      std::swap(s, e);
    }
    int count = 0;
    for (int bit = s; bit <= e; ++bit) {
      count += IsBitSet64(bits.data(), bit);
    }
    EXPECT_EQ(count, BitCountRange64(bits.data(), s, e));
  }
}

TEST(BitsetTest, IsEmptyRange64) {
  absl::BitGen randomizer;
  constexpr int kTests = 1000;
  constexpr int kNumBuckets = 10;
  constexpr int kNumBits = kNumBuckets * 64;
  std::array<uint64_t, kNumBuckets> bits;
  for (int i = 0; i < kTests; ++i) {
    bits.fill(0);
    const int num_to_set = absl::LogUniform(randomizer, 0, kNumBits);
    for (int j = 0; j < num_to_set; ++j) {
      SetBit64(bits.data(), absl::Uniform(randomizer, 0, kNumBits));
    }
    int s = absl::Uniform(randomizer, 0, kNumBits);
    int e = (i % 2 == 0)
                ? absl::Uniform(randomizer, 0, kNumBits)
                : std::min(kNumBits - 1, s + absl::Uniform(randomizer, 0, 64));
    if (s > e) {
      std::swap(s, e);
    }
    bool empty = true;
    for (int bit = s; bit <= e; ++bit) {
      if (IsBitSet64(bits.data(), bit)) {
        empty = false;
        break;
      }
    }
    EXPECT_EQ(empty, IsEmptyRange64(bits.data(), s, e));
  }
}

TEST(BitsetTest, FindFirstBit64) {
  absl::BitGen randomizer;
  constexpr int kTests = 1000;
  constexpr int kNumBuckets = 10;
  constexpr int kNumBits = kNumBuckets * 64;
  std::array<uint64_t, kNumBuckets> bits;
  for (int i = 0; i < kTests; ++i) {
    bits.fill(0);
    const int num_to_set = absl::LogUniform(randomizer, 0, kNumBits);
    for (int j = 0; j < num_to_set; ++j) {
      SetBit64(bits.data(), absl::Uniform(randomizer, 0, kNumBits));
    }
    int start = absl::Uniform(randomizer, 0, kNumBits);
    int end = (i % 2 == 0) ? absl::Uniform(randomizer, 0, kNumBits)
                           : std::min(kNumBits - 1,
                                      start + absl::Uniform(randomizer, 0, 64));
    if (start > end) {
      std::swap(start, end);
    }
    int bitup = -1;
    int bitdown = -1;
    for (int bit = start; bit <= end; ++bit) {
      if (IsBitSet64(bits.data(), bit)) {
        bitdown = bit;
        if (bitup == -1) {
          bitup = bit;
        }
      }
    }
    EXPECT_EQ(bitup, LeastSignificantBitPosition64(bits.data(), start, end));
    EXPECT_EQ(bitdown, MostSignificantBitPosition64(bits.data(), start, end));
    if (IsBitSet64(bits.data(), end)) {
      EXPECT_EQ(bitup,
                UnsafeLeastSignificantBitPosition64(bits.data(), start, end));
    }
    if (IsBitSet64(bits.data(), start)) {
      EXPECT_EQ(bitdown,
                UnsafeMostSignificantBitPosition64(bits.data(), start, end));
    }
    if (bitup == -1) {
      SetBit64(bits.data(), end);
      EXPECT_EQ(end,
                UnsafeLeastSignificantBitPosition64(bits.data(), start, end));
      ClearBit64(bits.data(), end);

      SetBit64(bits.data(), start);
      EXPECT_EQ(start,
                UnsafeMostSignificantBitPosition64(bits.data(), start, end));
      ClearBit64(bits.data(), start);
    }
  }
}

DEFINE_STRONG_INT64_TYPE(TestIndex);

TEST(SparseBitsetTest, Empty) {
  SparseBitset<TestIndex> bitset;
  EXPECT_EQ(0, bitset.NumberOfSetCallsWithDifferentArguments());
}

TEST(SparseBitsetTest, BasicBehavior) {
  SparseBitset<TestIndex> bitset;
  bitset.ClearAndResize(TestIndex(10));
  EXPECT_FALSE(bitset[TestIndex(1)]);
  EXPECT_FALSE(bitset[TestIndex(4)]);
  EXPECT_FALSE(bitset[TestIndex(6)]);
  bitset.Set(TestIndex(1));
  bitset.Set(TestIndex(1));
  bitset.Set(TestIndex(4));
  EXPECT_TRUE(bitset[TestIndex(1)]);
  EXPECT_TRUE(bitset[TestIndex(4)]);
  EXPECT_FALSE(bitset[TestIndex(6)]);
  EXPECT_EQ(2, bitset.NumberOfSetCallsWithDifferentArguments());
  bitset.Clear(TestIndex(1));
  EXPECT_EQ(2, bitset.NumberOfSetCallsWithDifferentArguments());
  EXPECT_FALSE(bitset[TestIndex(1)]);
}

TEST(SparseBitsetTest, ClearAndResize) {
  const int kSize = 10000;
  SparseBitset<> bitset(kSize);
  for (int i = 0; i < kSize; ++i) EXPECT_EQ(bitset[i], false);
  bitset.Set(10);
  bitset.Set(100);
  bitset.Set(9000);

  bitset.ClearAndResize(kSize / 3);
  EXPECT_EQ(bitset.size(), kSize / 3);
  for (int i = 0; i < kSize / 3; ++i) {
    EXPECT_EQ(bitset[i], false);
    bitset.Set(i);
  }

  bitset.ClearAndResize(2 * kSize);
  for (int i = 0; i < 2 * kSize; ++i) EXPECT_EQ(bitset[i], false);
}

TEST(SparseBitsetTest, Resize) {
  const int kSize = 10000;
  SparseBitset<> bitset(kSize);
  bitset.Set(10);
  bitset.Set(9000);
  bitset.Set(100);

  bitset.Resize(20000);
  EXPECT_EQ(20000, bitset.size());
  EXPECT_THAT(bitset.PositionsSetAtLeastOnce(), ElementsAre(10, 9000, 100));

  bitset.Resize(150);
  EXPECT_EQ(150, bitset.size());
  EXPECT_THAT(bitset.PositionsSetAtLeastOnce(), ElementsAre(10, 100));

  bitset.Resize(0);
  EXPECT_EQ(0, bitset.size());
  EXPECT_TRUE(bitset.PositionsSetAtLeastOnce().empty());
}

TEST(SparseBitsetTest, NotifyAllClear) {
  const int kSize = 10000;
  SparseBitset<> bitset(kSize);
  bitset.Set(10);
  bitset.Set(9000);
  bitset.Set(100);

  for (int index : bitset.PositionsSetAtLeastOnce()) {
    bitset.Clear(index);
  }
  bitset.NotifyAllClear();
  EXPECT_TRUE(bitset.PositionsSetAtLeastOnce().empty());
}

TEST(SparseBitset, ResetAllToFalse) {
  const int kSize = 10000;
  SparseBitset<> bitset(kSize);
  bitset.Set(10);
  bitset.Set(100);
  bitset.Set(9000);
  int bit_count = 0;
  for (int i = 0; i < kSize; ++i) {
    if (bitset[i]) ++bit_count;
  }
  EXPECT_EQ(3, bit_count);
  bitset.ResetAllToFalse();
  for (int i = 0; i < kSize; ++i) EXPECT_EQ(bitset[i], false);
}

TEST(SparseBitsetTest, CopyFrom) {
  SparseBitset<TestIndex> src(TestIndex(100));
  src.Set(TestIndex(7));
  src.Set(TestIndex(70));

  SparseBitset<TestIndex> dst(TestIndex(50));
  dst.Set(TestIndex(3));
  dst.CopyFrom(src);

  EXPECT_EQ(dst.size(), TestIndex(100));
  EXPECT_THAT(dst.PositionsSetAtLeastOnce(),
              ElementsAre(TestIndex(7), TestIndex(70)));
  for (TestIndex i(0); i < 100; ++i) {
    EXPECT_EQ(dst[i], i == TestIndex(7) || i == TestIndex(70));
  }
}

TEST(Bitset64Test, NegativeSize) {
  if (!DEBUG_MODE) {
    Bitset64<TestIndex> bitset(TestIndex(-10));
    EXPECT_EQ(0, bitset.size());
  }
}

TEST(Bitset64Test, BasicFunctions) {
  const TestIndex kSize(100);
  Bitset64<TestIndex> bitset(kSize);

  EXPECT_EQ(kSize, bitset.size());
  for (TestIndex i(0); i < kSize; ++i) {
    EXPECT_FALSE(bitset.IsSet(i));
  }

  for (TestIndex i(0); i < kSize; ++i) {
    bitset.Set(i);
    EXPECT_TRUE(bitset.IsSet(i));
    EXPECT_TRUE(bitset.IsSet(i));
    bitset.Clear(i);
    EXPECT_FALSE(bitset.IsSet(i));
    EXPECT_FALSE(bitset.IsSet(i));
  }

  for (TestIndex i(0); i < kSize; ++i) {
    bitset.Set(i);
  }
  for (TestIndex i(0); i < kSize; ++i) {
    EXPECT_TRUE(bitset.IsSet(i));
  }
  bitset.ClearAndResize(kSize);
  for (TestIndex i(0); i < kSize; ++i) {
    EXPECT_FALSE(bitset.IsSet(i));
  }
  for (TestIndex i(0); i < kSize; ++i) {
    bitset.Set(i);
  }
  bitset.ClearAll();
  for (TestIndex i(0); i < kSize; ++i) {
    EXPECT_FALSE(bitset.IsSet(i));
  }
  for (TestIndex i(0); i < kSize; ++i) {
    bitset.Set(i);
  }
  EXPECT_EQ(*std::max_element(bitset.begin(), bitset.end()), kSize - 1);
  bitset.ClearBucket(TestIndex(65));
  for (TestIndex i(0); i < TestIndex(64); ++i) {
    EXPECT_TRUE(bitset.IsSet(i));
  }
  for (TestIndex i(64); i < kSize; ++i) {
    EXPECT_FALSE(bitset.IsSet(i));
  }
}

TEST(Bitset64Test, IntegerSpecialization) {
  const int kSize = 1000;
  Bitset64<> bitset(kSize);
  EXPECT_EQ(kSize, bitset.size());
  for (int i = 0; i < kSize; ++i) EXPECT_FALSE(bitset.IsSet(i));
  for (int i = 0; i < kSize; ++i) {
    bitset.Set(i);
    EXPECT_TRUE(bitset.IsSet(i));
    EXPECT_TRUE(bitset.IsSet(i));
    bitset.Clear(i);
    EXPECT_FALSE(bitset.IsSet(i));
    EXPECT_FALSE(bitset.IsSet(i));
  }

  for (int i = 0; i < kSize; ++i) bitset.Set(i);
  for (int i = 0; i < kSize; ++i) EXPECT_TRUE(bitset.IsSet(i));

  bitset.ClearAndResize(kSize);
  for (int i = 0; i < kSize; ++i) EXPECT_FALSE(bitset.IsSet(i));

  for (int i = 0; i < kSize; ++i) bitset.Set(i);
  bitset.ClearAndResize(kSize / 3);
  EXPECT_EQ(bitset.size(), kSize / 3);
  for (int i = 0; i < kSize / 3; ++i) {
    EXPECT_FALSE(bitset.IsSet(i));
    bitset.Set(i);
  }

  bitset.ClearAndResize(3 * kSize);
  for (int i = 0; i < 3 * kSize; ++i) EXPECT_FALSE(bitset.IsSet(i));
}

TEST(Bitset64Test, Resize) {
  Bitset64<TestIndex> bitset;
  EXPECT_EQ(TestIndex(0), bitset.size());
  const TestIndex kLarge(100000);
  for (TestIndex i(0); i < kLarge; ++i) {
    bitset.Resize(i + 1);
    bitset.Set(i, i.value() % 2 ? true : false);
    EXPECT_EQ(i + 1, bitset.size());
  }
  EXPECT_EQ(kLarge, bitset.size());
  for (int i = 2; i < 10; i++) {
    bitset.Resize(kLarge / i);
    bitset.Resize(kLarge);
    absl::flat_hash_set<TestIndex> copy_from_iterator;
    for (const TestIndex j : bitset) {
      copy_from_iterator.insert(j);
    }
    for (TestIndex j(0); j < kLarge; ++j) {
      const bool expected = (j.value() % 2 == 1) && j < kLarge / i;
      EXPECT_EQ(expected, bitset.IsSet(j));
      EXPECT_EQ(copy_from_iterator.contains(j), bitset.IsSet(j));
    }
  }
}

TEST(Bitset64Test, Iterator) {
  static_assert(std::forward_iterator<Bitset64<int>::Iterator>);
  Bitset64<int> bitset(5);
  bitset.Set(0);
  bitset.Set(2);
  bitset.Set(4);
  auto it = bitset.begin();
  auto next = it;
  ++next;
  EXPECT_FALSE(it == next);
  EXPECT_TRUE(it != next);
  EXPECT_TRUE(it == it);
  EXPECT_FALSE(it != it);
  ++it;
  EXPECT_TRUE(it == next);
  ++it;
  next++;
  EXPECT_TRUE(it == next);
}

TEST(Bitset64Test, CopyBucket) {
  Bitset64<TestIndex> bitset(TestIndex(100));
  Bitset64<TestIndex> bitset2(TestIndex(50));
  for (TestIndex i(0); i < 100; ++i) bitset.Set(i, (i % 7) == 0);
  for (TestIndex i(0); i < 50; ++i) bitset2.Set(i, (i % 3) == 0);
  bitset.CopyBucket(bitset2, TestIndex(0));
  for (TestIndex i(0); i < 50; ++i) {
    EXPECT_EQ(bitset[i], (i % 3) == 0);
  }
  for (TestIndex i(50); i < 64; ++i) {
    EXPECT_FALSE(bitset[i]);
  }
  for (TestIndex i(64); i < 100; ++i) {
    EXPECT_EQ(bitset[i], (i % 7) == 0);
  }
}

TEST(Bitset64Test, Intersection) {
  Bitset64<TestIndex> bitset(TestIndex(100));
  Bitset64<TestIndex> bitset2(TestIndex(50));
  for (TestIndex i(0); i < 100; ++i) bitset.Set(i, (i % 7) == 0);
  for (TestIndex i(0); i < 50; ++i) bitset2.Set(i, (i % 3) == 0);
  bitset.Intersection(bitset2);
  for (TestIndex i(0); i < 100; ++i) {
    EXPECT_EQ((i < 50) && ((i % 7) == 0) && ((i % 3) == 0), bitset.IsSet(i));
  }
}

TEST(Bitset64Test, UnionSameSize) {
  Bitset64<TestIndex> bitset(TestIndex(200));
  Bitset64<TestIndex> bitset2(TestIndex(200));
  bitset.Set(TestIndex(30));
  bitset.Set(TestIndex(99));
  bitset.Set(TestIndex(100));
  bitset2.Set(TestIndex(17));
  bitset2.Set(TestIndex(100));
  bitset2.Set(TestIndex(102));
  bitset2.Set(TestIndex(199));
  bitset.Union(bitset2);
  for (TestIndex i(0); i < 200; ++i) {
    int64_t v = i.value<int64_t>();
    SCOPED_TRACE(absl::StrCat("i=", v));
    if (v == 30 || v == 99 || v == 100 || v == 17 || v == 102 || v == 199) {
      EXPECT_TRUE(bitset.IsSet(i));
    } else {
      EXPECT_FALSE(bitset.IsSet(i));
    }
  }
}

TEST(Bitset64Test, UnionSmaller) {
  Bitset64<TestIndex> bitset(TestIndex(100));
  Bitset64<TestIndex> bitset2(TestIndex(200));
  bitset.Set(TestIndex(30));
  bitset.Set(TestIndex(99));
  bitset2.Set(TestIndex(17));
  bitset2.Set(TestIndex(30));
  bitset2.Set(TestIndex(100));
  bitset2.Set(TestIndex(102));
  bitset2.Set(TestIndex(199));
  bitset.Union(bitset2);
  for (TestIndex i(0); i < 100; ++i) {
    int64_t v = i.value<int64_t>();
    SCOPED_TRACE(absl::StrCat("i=", v));
    if (v == 30 || v == 99 || v == 17) {
      EXPECT_TRUE(bitset.IsSet(i));
    } else {
      EXPECT_FALSE(bitset.IsSet(i));
    }
  }
}

TEST(Bitset64Test, UnionLarger) {
  Bitset64<TestIndex> bitset(TestIndex(100));
  Bitset64<TestIndex> bitset2(TestIndex(200));
  bitset.Set(TestIndex(30));
  bitset.Set(TestIndex(99));
  bitset.Set(TestIndex(17));
  bitset2.Set(TestIndex(17));
  bitset2.Set(TestIndex(100));
  bitset2.Set(TestIndex(102));
  bitset2.Set(TestIndex(199));
  bitset2.Union(bitset);
  for (TestIndex i(0); i < 200; ++i) {
    int64_t v = i.value<int64_t>();
    SCOPED_TRACE(absl::StrCat("i=", v));
    if (v == 30 || v == 99 || v == 100 || v == 17 || v == 102 || v == 199) {
      EXPECT_TRUE(bitset2.IsSet(i));
    } else {
      EXPECT_FALSE(bitset2.IsSet(i));
    }
  }
}

TEST(Bitset64Test, IteratorOverEmptyBitSet) {
  Bitset64<TestIndex> bitset;
  int num_bits = 0;
  for (const TestIndex index : bitset) {
    CHECK_EQ(index, TestIndex(-1));  // Never executed.
    ++num_bits;
  }
  EXPECT_EQ(num_bits, 0);
}

TEST(Bitset64Test, IteratorOverRandomBitSet) {
  const TestIndex kSize(100 * 1000);
  Bitset64<TestIndex> bitset(kSize);
  EXPECT_EQ(kSize, bitset.size());

  const float kDensity = 0.5;
  int num_set_positions = 0;

  const int kRandomSeed = 1;
  std::mt19937 randomizer(kRandomSeed);
  for (TestIndex i(0); i < kSize; ++i) {
    if (absl::Uniform<float>(absl::IntervalOpen, randomizer, 0, 1) < kDensity) {
      bitset.Set(i);
      EXPECT_TRUE(bitset.IsSet(i));
      ++num_set_positions;
    } else {
      EXPECT_FALSE(bitset.IsSet(i));
    }
  }

  int num_iterated_positions = 0;
  TestIndex old = TestIndex(-1);
  for (const TestIndex index : bitset) {
    EXPECT_TRUE(bitset.IsSet(index));
    EXPECT_GT(index, old);
    num_iterated_positions++;
    old = index;
  }
  EXPECT_EQ(num_set_positions, num_iterated_positions);
}

static_assert(std::is_trivially_copyable_v<Bitset64<int>::View>);
static_assert(std::is_trivially_copyable_v<Bitset64<int>::ConstView>);
static_assert(std::is_trivially_copyable_v<Bitset64<TestIndex>::View>);
static_assert(std::is_trivially_copyable_v<Bitset64<TestIndex>::ConstView>);
#ifdef NDEBUG
static_assert(sizeof(Bitset64<int>::View) == sizeof(uint64_t*));
static_assert(sizeof(Bitset64<int>::ConstView) == sizeof(const uint64_t*));
static_assert(sizeof(Bitset64<TestIndex>::View) == sizeof(uint64_t*));
static_assert(sizeof(Bitset64<TestIndex>::ConstView) ==
              sizeof(const uint64_t*));
#endif

TEST(Bitset64Test, Views) {
  Bitset64<TestIndex> bitset(TestIndex(100));
  Bitset64<TestIndex>::View view = bitset.view();
  const Bitset64<TestIndex>::ConstView const_view = bitset.const_view();

  view.Set(TestIndex(0));
  view.Set(TestIndex(63));
  view.Set(TestIndex(64));
  view.Set(TestIndex(99));

  EXPECT_TRUE(view[TestIndex(0)]);
  EXPECT_TRUE(const_view[TestIndex(63)]);
  EXPECT_TRUE(const_view[TestIndex(64)]);
  EXPECT_TRUE(const_view[TestIndex(99)]);
  EXPECT_FALSE(const_view[TestIndex(1)]);
  EXPECT_FALSE(const_view[TestIndex(65)]);

  view.Clear(TestIndex(63));
  EXPECT_FALSE(view[TestIndex(63)]);
  EXPECT_FALSE(const_view[TestIndex(63)]);
  EXPECT_FALSE(bitset.IsSet(TestIndex(63)));

  EXPECT_EQ(const_view.GetBucket(TestIndex(64)), OneBit64(0) | OneBit64(35));
  view.GetBucket(TestIndex(64)) = 0;
  EXPECT_FALSE(const_view[TestIndex(64)]);
  EXPECT_FALSE(const_view[TestIndex(99)]);
}

#ifndef NDEBUG
TEST(Bitset64DeathTest, ViewBoundsChecking) {
  Bitset64<TestIndex> empty;
  EXPECT_DEATH((void)empty.const_view()[TestIndex(0)], "");
  EXPECT_DEATH((void)empty.view()[TestIndex(0)], "");
  EXPECT_DEATH(empty.view().Set(TestIndex(0)), "");
  EXPECT_DEATH(empty.view().Clear(TestIndex(0)), "");

  // Size 10 allocates a single 64-bit bucket, so indices 10 and 63 lie within
  // the allocated word but outside [0, size).
  Bitset64<TestIndex> bitset(TestIndex(10));
  Bitset64<TestIndex>::View view = bitset.view();
  const Bitset64<TestIndex>::ConstView const_view = bitset.const_view();

  EXPECT_DEATH((void)view[TestIndex(-1)], "");
  EXPECT_DEATH((void)view[TestIndex(10)], "");
  EXPECT_DEATH((void)view[TestIndex(63)], "");
  EXPECT_DEATH((void)const_view[TestIndex(-1)], "");
  EXPECT_DEATH((void)const_view[TestIndex(10)], "");
  EXPECT_DEATH((void)const_view[TestIndex(63)], "");

  EXPECT_DEATH(view.Set(TestIndex(-1)), "");
  EXPECT_DEATH(view.Set(TestIndex(10)), "");
  EXPECT_DEATH(view.Clear(TestIndex(-1)), "");
  EXPECT_DEATH(view.Clear(TestIndex(10)), "");
  EXPECT_DEATH((void)view.GetBucket(TestIndex(-1)), "");
  EXPECT_DEATH((void)view.GetBucket(TestIndex(10)), "");
  EXPECT_DEATH((void)const_view.GetBucket(TestIndex(-1)), "");
  EXPECT_DEATH((void)const_view.GetBucket(TestIndex(10)), "");
}

TEST(Bitset64DeathTest, MemberBoundsChecking) {
  Bitset64<TestIndex> bitset(TestIndex(10));
  EXPECT_DEATH((void)bitset[TestIndex(-1)], "");
  EXPECT_DEATH((void)bitset[TestIndex(10)], "");
  EXPECT_DEATH(bitset.Set(TestIndex(-1)), "");
  EXPECT_DEATH(bitset.Set(TestIndex(10)), "");
  EXPECT_DEATH(bitset.Clear(TestIndex(-1)), "");
  EXPECT_DEATH(bitset.Clear(TestIndex(10)), "");
  EXPECT_DEATH(bitset.ClearBucket(TestIndex(-1)), "");
  EXPECT_DEATH(bitset.ClearBucket(TestIndex(10)), "");

  Bitset64<TestIndex> smaller(TestIndex(5));
  EXPECT_DEATH(bitset.CopyBucket(smaller, TestIndex(7)), "");
}

TEST(Bitset64DeathTest, NegativeSize) {
  EXPECT_DEATH((Bitset64<TestIndex>(TestIndex(-1))), "");
  Bitset64<TestIndex> bitset(TestIndex(10));
  EXPECT_DEATH(bitset.Resize(TestIndex(-1)), "");
  EXPECT_DEATH(bitset.ClearAndResize(TestIndex(-1)), "");
}
#endif  // NDEBUG

TEST(BitQueue64Test, BasicTest) {
  const int kSize = 100000;
  BitQueue64 queue(kSize);

  const int kRandomSeed = 1;
  std::mt19937 randomizer(kRandomSeed);
  std::vector<int> positions;
  for (TestIndex i(0); i < kSize; ++i) {
    const int pos = absl::Uniform<int32_t>(randomizer, 0, kSize);
    queue.Set(pos);
    positions.push_back(pos);
  }
  gtl::STLSortAndRemoveDuplicates(&positions);
  while (!positions.empty()) {
    ASSERT_EQ(queue.Top(), positions.back());
    positions.pop_back();
    queue.ClearTop();
  }
  EXPECT_EQ(queue.Top(), -1);
}

TEST(BitQueue64Test, SetAllUpTo) {
  BitQueue64 queue(100);
  queue.SetAllBefore(71);
  for (int pos = 70; pos >= 0; --pos) {
    EXPECT_EQ(queue.Top(), pos);
    queue.ClearTop();
  }
  EXPECT_EQ(queue.Top(), -1);
}

}  // namespace
}  // namespace operations_research
