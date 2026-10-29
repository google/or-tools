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
#include <cstdint>
#include <cstring>
#include <random>
#include <string>
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

// These implementations are currently not exposed in `bitset.h` but we want to
// test their performances.
int LSB32Std(uint32_t n) { return std::countr_zero(n); }
int LSB64Std(uint64_t n) { return std::countr_zero(n); }
int MSB32Std(uint32_t n) { return 31 - std::countl_zero(n); }
int MSB64Std(uint64_t n) { return 63 - std::countl_zero(n); }

// An enum to select between LSBIndex and MSBIndex.
enum class Function { kLSB, kMSB };

// An enum to select the implementation.
enum class Impl { kBasic, kBuiltin, kDeBruijn, kStd };

// A type to describe a complete implementation.
template <Function function, typename T, Impl implementation, auto FunctionPtr>
struct TestCase {
  using UIntT = T;
  static constexpr Function kFunction = function;
  static constexpr Impl kImpl = implementation;
  static int Compute(UIntT value) { return FunctionPtr(value); }
};

// Shorthand for all available types.
using LSB_32_Basic = TestCase<Function::kLSB, uint32_t, Impl::kBasic,
                              LeastSignificantBitPosition32Default>;
#ifdef ORTOOLS_BITSET_USE_FAST_LEAST_SIGNIFICANT_BIT
using LSB_32_Builtin = TestCase<Function::kLSB, uint32_t, Impl::kBuiltin,
                                LeastSignificantBitPosition32Fast>;
#endif
using LSB_32_DeBruijn = TestCase<Function::kLSB, uint32_t, Impl::kDeBruijn,
                                 LeastSignificantBitPosition32DeBruijn>;
using LSB_32_Std = TestCase<Function::kLSB, uint32_t, Impl::kStd, LSB32Std>;
using LSB_64_Basic = TestCase<Function::kLSB, uint64_t, Impl::kBasic,
                              LeastSignificantBitPosition64Default>;
#ifdef ORTOOLS_BITSET_USE_FAST_LEAST_SIGNIFICANT_BIT
using LSB_64_Builtin = TestCase<Function::kLSB, uint64_t, Impl::kBuiltin,
                                LeastSignificantBitPosition64Fast>;
#endif
using LSB_64_DeBruijn = TestCase<Function::kLSB, uint64_t, Impl::kDeBruijn,
                                 LeastSignificantBitPosition64DeBruijn>;
using LSB_64_Std = TestCase<Function::kLSB, uint64_t, Impl::kStd, LSB64Std>;
using MSB_32_Basic = TestCase<Function::kMSB, uint32_t, Impl::kBasic,
                              MostSignificantBitPosition32Default>;
#ifdef ORTOOLS_BITSET_USE_FAST_LEAST_SIGNIFICANT_BIT
using MSB_32_Builtin = TestCase<Function::kMSB, uint32_t, Impl::kBuiltin,
                                MostSignificantBitPosition32Fast>;
#endif
using MSB_32_Std = TestCase<Function::kMSB, uint32_t, Impl::kStd, MSB32Std>;
using MSB_64_Basic = TestCase<Function::kMSB, uint64_t, Impl::kBasic,
                              MostSignificantBitPosition64Default>;
#ifdef ORTOOLS_BITSET_USE_FAST_LEAST_SIGNIFICANT_BIT
using MSB_64_Builtin = TestCase<Function::kMSB, uint64_t, Impl::kBuiltin,
                                MostSignificantBitPosition64Fast>;
#endif
using MSB_64_Std = TestCase<Function::kMSB, uint64_t, Impl::kStd, MSB64Std>;

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
  static absl::string_view GetString(Impl implementation) {
    switch (implementation) {
      case Impl::kBasic:
        return "Basic";
      case Impl::kBuiltin:
        return "Builtin";
      case Impl::kDeBruijn:
        return "DeBruijn";
      case Impl::kStd:
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
                           sizeof(UIntT) * 8, GetString(T::kImpl));
  }
};

template <typename TestCase>
class BitIndex : public testing::Test {};

using BitIndexTypes =
    ::testing::Types<LSB_32_Basic,
#ifdef ORTOOLS_BITSET_USE_FAST_LEAST_SIGNIFICANT_BIT
                     LSB_32_Builtin,
#endif
                     LSB_32_DeBruijn, LSB_32_Std, LSB_64_Basic,
#ifdef ORTOOLS_BITSET_USE_FAST_LEAST_SIGNIFICANT_BIT
                     LSB_64_Builtin,
#endif
                     LSB_64_DeBruijn, LSB_64_Std, MSB_32_Basic,
#ifdef ORTOOLS_BITSET_USE_FAST_LEAST_SIGNIFICANT_BIT
                     MSB_32_Builtin,
#endif
                     MSB_32_Std, MSB_64_Basic,
#ifdef ORTOOLS_BITSET_USE_FAST_LEAST_SIGNIFICANT_BIT
                     MSB_64_Builtin,
#endif
                     MSB_64_Std>;
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

TEST(BitsetTest, BitPos32) {
  for (int i = 0; i < 128; ++i) {
    EXPECT_EQ(BitPos32(i), i % 32);
    EXPECT_EQ(BitPos64(i), i % 64);
  }
}

TEST(BitsetTest, BitOffset) {
  for (int i = 0; i < 128; ++i) {
    EXPECT_EQ(BitOffset32(i), i / 32);
    EXPECT_EQ(BitOffset64(i), i / 64);
  }
}

TEST(BitsetTest, BitLength) {
  EXPECT_EQ(0, BitLength32(0));
  EXPECT_EQ(1, BitLength32(32));
  EXPECT_EQ(2, BitLength32(33));
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
  const uint32_t kBits32[2] = {0UL, 0xffffUL};
  EXPECT_FALSE(IsBitSet32(kBits32, 0));
  EXPECT_TRUE(IsBitSet32(kBits32, 32));
  const uint64_t kBits64[2] = {0UL, 0xffffffffUL};
  EXPECT_FALSE(IsBitSet64(kBits64, 0));
  EXPECT_TRUE(IsBitSet64(kBits64, 64));
}

TEST(BitsetTest, BitShift) {
  EXPECT_EQ(1 << 5, BitShift32(1));
  EXPECT_EQ(1 << 6, BitShift64(1));
}

#define TEST_SET_BIT(size)                                       \
  TEST(BitsetTest, SetBit##size) {                               \
    const int kSize = 10;                                        \
    std::array<uint##size##_t, kSize> bits;                      \
    for (int i = 0; i < kSize * sizeof(*bits.data() * 8); ++i) { \
      memset(bits.data(), 0UL, kSize * sizeof(*bits.data()));    \
      SetBit##size(bits.data(), i);                              \
      EXPECT_TRUE(IsBitSet##size(bits.data(), i));               \
    }                                                            \
  }

TEST_SET_BIT(32)
TEST_SET_BIT(64)

#undef TEST_SET_BIT

#define TEST_CLEAR_BIT(size)                                     \
  TEST(BitsetTest, ClearBit##size) {                             \
    const int kSize = 10;                                        \
    std::array<uint##size##_t, kSize> bits;                      \
    for (int i = 0; i < kSize * sizeof(*bits.data() * 8); ++i) { \
      memset(bits.data(), 0xff, kSize * sizeof(*bits.data()));   \
      ClearBit##size(bits.data(), i);                            \
      EXPECT_FALSE(IsBitSet##size(bits.data(), i));              \
    }                                                            \
  }

TEST_CLEAR_BIT(32);
TEST_CLEAR_BIT(64);

#undef TEST_CLEAR_BIT

#define TEST_BIT_COUNT(size, zero)                  \
  TEST(BitsetTest, BitCount##size) {                \
    const int kTests = 1000;                        \
    absl::BitGen randomizer;                        \
    uint##size##_t bits = zero;                     \
    int count = 0;                                  \
    for (int i = 0; i < kTests; ++i) {              \
      int bit = absl::Uniform(randomizer, 0, size); \
      if (!IsBitSet##size(&bits, bit)) {            \
        SetBit##size(&bits, bit);                   \
        ++count;                                    \
      }                                             \
      EXPECT_EQ(count, BitCount##size(bits));       \
    }                                               \
  }

TEST_BIT_COUNT(32, 0UL);
TEST_BIT_COUNT(64, 0ULL);

#undef TEST_BIT_COUNT

#define TEST_ONE_RANGE(size)                                   \
  TEST(BitsetTest, OneRange##size) {                           \
    absl::BitGen randomizer;                                   \
    const int kTests = 1000;                                   \
    for (int i = 0; i < kTests; ++i) {                         \
      int s = absl::Uniform(randomizer, 0, size);              \
      int e = absl::Uniform(randomizer, 0, size);              \
      if (s > e) {                                             \
        std::swap(s, e);                                       \
      }                                                        \
      uint##size##_t bits = OneRange##size(s, e);              \
      for (int i = 0; i < size; ++i) {                         \
        EXPECT_EQ(i >= s && i <= e, IsBitSet##size(&bits, i)); \
      }                                                        \
    }                                                          \
  }

TEST_ONE_RANGE(32);
TEST_ONE_RANGE(64);

#undef TEST_ONE_RANGE

#define TEST_INTERVAL_UP(size)                               \
  TEST(BitsetTest, IntervalUp##size) {                       \
    for (int i = 0; i < size; ++i) {                         \
      uint##size##_t bits = IntervalUp##size(i);             \
      EXPECT_EQ(i, LeastSignificantBitPosition##size(bits)); \
      EXPECT_EQ(size - i, BitCount##size(bits));             \
    }                                                        \
  }

TEST_INTERVAL_UP(32)
TEST_INTERVAL_UP(64)

#undef TEST_INTERVAL_UP

#define TEST_INTERVAL_DOWN(size)                            \
  TEST(BitsetTest, IntervalDown##size) {                    \
    for (int i = 0; i < size; ++i) {                        \
      uint##size##_t bits = IntervalDown##size(i);          \
      EXPECT_EQ(i, MostSignificantBitPosition##size(bits)); \
      EXPECT_EQ(i + 1, BitCount##size(bits));               \
    }                                                       \
  }

TEST_INTERVAL_DOWN(32)
TEST_INTERVAL_DOWN(64)

#undef TEST_INTERVAL_DOWN

#define TEST_BIT_COUNT_RANGE(size)                              \
  TEST(BitsetTest, BitCountRange##size) {                       \
    absl::BitGen randomizer;                                    \
    const int kTests = 1000;                                    \
    const int kSize = 10;                                       \
    std::array<uint##size##_t, kSize> bits;                     \
    for (int i = 0; i < kTests; ++i) {                          \
      int s = absl::Uniform(randomizer, 0, size);               \
      int e = absl::Uniform(randomizer, 0, size);               \
      if (s > e) {                                              \
        std::swap(s, e);                                        \
      }                                                         \
      int count = 0;                                            \
      for (int bit = 0; bit < size; ++bit) {                    \
        if (absl::Uniform(randomizer, 0, 2) == 0) {             \
          SetBit##size(bits.data(), bit);                       \
          if (s <= bit && bit <= e) {                           \
            ++count;                                            \
          }                                                     \
        } else {                                                \
          ClearBit##size(bits.data(), bit);                     \
        }                                                       \
      }                                                         \
      EXPECT_EQ(count, BitCountRange##size(bits.data(), s, e)); \
    }                                                           \
  }

TEST_BIT_COUNT_RANGE(32)
TEST_BIT_COUNT_RANGE(64)

#undef TEST_BIT_COUNT_RANGE

#define TEST_IS_EMPTY_RANGE(size)                              \
  TEST(BitsetTest, IsEmptyRange##size) {                       \
    absl::BitGen randomizer;                                   \
    const int kTests = 1000;                                   \
    const int kSize = 10;                                      \
    std::array<uint##size##_t, kSize> bits;                    \
    for (int i = 0; i < kTests; ++i) {                         \
      int s = absl::Uniform(randomizer, 0, size);              \
      int e = absl::Uniform(randomizer, 0, size);              \
      if (s > e) {                                             \
        std::swap(s, e);                                       \
      }                                                        \
      bool empty = true;                                       \
      for (int bit = 0; bit < size; ++bit) {                   \
        if (absl::Uniform(randomizer, 0, 2) == 0) {            \
          SetBit##size(bits.data(), bit);                      \
          if (s <= bit && bit <= e) {                          \
            empty = false;                                     \
          }                                                    \
        } else {                                               \
          ClearBit##size(bits.data(), bit);                    \
        }                                                      \
      }                                                        \
      EXPECT_EQ(empty, IsEmptyRange##size(bits.data(), s, e)); \
    }                                                          \
  }

TEST_IS_EMPTY_RANGE(32)
TEST_IS_EMPTY_RANGE(64)

#undef TEST_IS_EMPTY_RANGE

#define TEST_SIGNIFICANT_BIT_POSITION(size)                                    \
  TEST(BitsetTest, FindFirstBit##size) {                                       \
    absl::BitGen randomizer;                                                   \
    const int kTests = 1000;                                                   \
    const int kSize = 10;                                                      \
    std::array<uint##size##_t, kSize> bits;                                    \
    for (int i = 0; i < kTests; ++i) {                                         \
      int start = absl::Uniform(randomizer, 0, size);                          \
      int end = absl::Uniform(randomizer, 0, size);                            \
      if (start > end) {                                                       \
        std::swap(start, end);                                                 \
      }                                                                        \
      int bitup = -1;                                                          \
      int bitdown = -1;                                                        \
      for (int bit = 0; bit < size; ++bit) {                                   \
        if (absl::Uniform(randomizer, 0, 2) == 0) {                            \
          SetBit##size(bits.data(), bit);                                      \
          if (bit <= end && bit >= start) {                                    \
            bitdown = bit;                                                     \
            if (bitup == -1) {                                                 \
              bitup = bit;                                                     \
            }                                                                  \
          }                                                                    \
        } else {                                                               \
          ClearBit##size(bits.data(), bit);                                    \
        }                                                                      \
      }                                                                        \
      EXPECT_EQ(bitup,                                                         \
                LeastSignificantBitPosition##size(bits.data(), start, end));   \
      if (IsBitSet##size(bits.data(), end)) {                                  \
        EXPECT_EQ(bitup, UnsafeLeastSignificantBitPosition##size(bits.data(),  \
                                                                 start, end)); \
      }                                                                        \
      EXPECT_EQ(bitdown,                                                       \
                MostSignificantBitPosition##size(bits.data(), start, end));    \
      if (IsBitSet##size(bits.data(), start)) {                                \
        EXPECT_EQ(bitdown, UnsafeMostSignificantBitPosition##size(             \
                               bits.data(), start, end));                      \
      }                                                                        \
    }                                                                          \
  }

TEST_SIGNIFICANT_BIT_POSITION(32)
TEST_SIGNIFICANT_BIT_POSITION(64)

#undef TEST_SIGNIFICANT_BIT_POSITION

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

TEST(Bitset64Test, PushBack) {
  Bitset64<TestIndex> bitset;
  EXPECT_EQ(TestIndex(0), bitset.size());
  const TestIndex kLarge(100000);
  for (TestIndex i(0); i < kLarge; ++i) {
    bitset.PushBack(i.value() % 2 ? true : false);
    EXPECT_EQ(i + 1, bitset.size());
  }
  EXPECT_EQ(kLarge, bitset.size());
  for (TestIndex i(0); i < kLarge; ++i) {
    EXPECT_EQ(i.value() % 2 ? true : false, bitset.IsSet(i));
  }
}

TEST(Bitset64Test, Resize) {
  Bitset64<TestIndex> bitset;
  EXPECT_EQ(TestIndex(0), bitset.size());
  const TestIndex kLarge(100000);
  for (TestIndex i(0); i < kLarge; ++i) {
    bitset.PushBack(i.value() % 2 ? true : false);
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

TEST(Bitset64Test, DebugString) {
  const char* kExpectedBitset1 =
      "100000010000001000000100000010000001000000100000010000001000000100000010"
      "0000010000001000000100000010";
  const char* kExpectedBitset2 =
      "10010010010010010010010010010010010010010010010010";
  const char* kExpectedBitset3 =
      "10000000000000010000000000000010000000000000010000";
  Bitset64<TestIndex> bitset(TestIndex(100));
  Bitset64<TestIndex> bitset2(TestIndex(50));
  Bitset64<TestIndex> bitset3(TestIndex(50));
  for (TestIndex i(0); i < 100; ++i) bitset.Set(i, (i % 7) == 0);
  for (TestIndex i(0); i < 50; ++i) bitset2.Set(i, (i % 3) == 0);
  for (TestIndex i(0); i < 50; ++i) bitset3.Set(i, (i % 15) == 0);
  EXPECT_EQ(kExpectedBitset1, bitset.DebugString());
  EXPECT_EQ(kExpectedBitset2, bitset2.DebugString());
  EXPECT_EQ(kExpectedBitset3, bitset3.DebugString());
}

TEST(Bitset64Test, SetContentFromBitsetAcrossTypes) {
  Bitset64<TestIndex> bitset(TestIndex(100));
  {
    Bitset64<int64_t> bitset2(50);
    bitset2.Set(7);
    bitset.SetContentFromBitset<int64_t>(bitset2);
  }
  for (TestIndex i(0); i < TestIndex(100); i++) {
    SCOPED_TRACE(i.value());
    if (i == TestIndex(7)) {
      EXPECT_TRUE(bitset[i]);
    } else {
      EXPECT_FALSE(bitset[i]);
    }
  }
}

TEST(Bitset64Test, SetContentFromBitsetSameSizeAcrossTypes) {
  Bitset64<TestIndex> bitset(TestIndex(100));
  {
    Bitset64<int64_t> bitset2(100);
    bitset2.Set(7);
    bitset.SetContentFromBitset<int64_t>(bitset2);
  }
  for (TestIndex i(0); i < TestIndex(100); i++) {
    SCOPED_TRACE(i.value());
    if (i == TestIndex(7)) {
      EXPECT_TRUE(bitset[i]);
    } else {
      EXPECT_FALSE(bitset[i]);
    }
  }
}

TEST(Bitset64Test, SetContentFromBitset) {
  Bitset64<TestIndex> bitset(TestIndex(100));
  Bitset64<TestIndex> bitset2(TestIndex(50));
  Bitset64<TestIndex> bitset3(TestIndex(50));
  for (TestIndex i(0); i < 100; ++i) bitset.Set(i, (i % 7) == 0);
  for (TestIndex i(0); i < 50; ++i) bitset2.Set(i, (i % 3) == 0);
  for (TestIndex i(0); i < 50; ++i) bitset3.Set(i, (i % 15) == 0);
  bitset.SetContentFromBitset(bitset2);
  for (TestIndex i(0); i < 50; ++i) {
    EXPECT_EQ(bitset[i], (i % 3) == 0);
  }
  for (TestIndex i(50); i < 100; ++i) {
    EXPECT_EQ(bitset[i], (i % 7) == 0);
  }
  bitset3.SetContentFromBitset(bitset2);
  for (TestIndex i(0); i < 50; ++i) {
    EXPECT_EQ(bitset3[i], (i % 3) == 0);
  }
  for (TestIndex i(0); i < 50; ++i) bitset3.Set(i, (i % 15) == 0);
  bitset3.SetContentFromBitsetOfSameSize(bitset2);
  for (TestIndex i(0); i < 50; ++i) {
    EXPECT_EQ(bitset3[i], (i % 3) == 0);
  }
  for (TestIndex i(0); i < 100; ++i) bitset.Set(i, (i % 7) == 0);
  bitset2.SetContentFromBitset(bitset);
  for (TestIndex i(0); i < 50; ++i) {
    EXPECT_EQ(bitset2[i], (i % 7) == 0);
  }
  EXPECT_EQ(100, bitset.size());
  EXPECT_EQ(50, bitset2.size());
  EXPECT_EQ(50, bitset3.size());
  for (TestIndex i(0); i < 50; ++i) bitset2.Set(i, (i % 3) == 0);
  bitset.CopyBucket(bitset2, TestIndex(0));
  for (TestIndex i(0); i < 50; ++i) {
    EXPECT_EQ(bitset[i], (i % 3) == 0);
  }
  for (TestIndex i(64); i < 100; ++i) {
    EXPECT_EQ(bitset[i], (i % 7) == 0);
  }
  for (TestIndex i(0); i < 100; ++i) {
    for (TestIndex j(0); j < 100; ++j) bitset.Set(j, (j % 7) == 0);
    Bitset64<TestIndex> other_bitset(i);
    for (TestIndex j(0); j < i; ++j) other_bitset.Set(j, (j % 3) == 0);
    bitset.SetContentFromBitset(other_bitset);
    for (TestIndex j(0); j < i; ++j) {
      EXPECT_EQ(bitset[j], (j % 3) == 0);
    }
    for (TestIndex j(i); j < 100; ++j) {
      EXPECT_EQ(bitset[j], (j % 7) == 0);
    }
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
