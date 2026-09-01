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

#include "ortools/util/saturated_arithmetic.h"

#include <cstdint>
#include <limits>
#include <random>
#include <vector>

#include "absl/numeric/int128.h"
#include "absl/random/random.h"
#include "gtest/gtest.h"
#include "ortools/base/types.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace {

DEFINE_STRONG_INT64_TYPE(TestType);

void TestSafeAdd64(int64_t a, int64_t b, bool error_expected) {
  TestType result_b(b);
  TestType result_a(a);
  EXPECT_EQ(SafeAddInto(TestType(a), &result_b), !error_expected)
      << a << " + " << b;
  EXPECT_EQ(SafeAddInto(TestType(b), &result_a), !error_expected)
      << a << " + " << b;
  if (!error_expected) {
    EXPECT_EQ(result_a.value(), a + b) << a << " + " << b;
    EXPECT_EQ(result_b.value(), a + b) << a << " + " << b;
  }
}

TEST(SafeAddTest, BasicCases) {
  TestSafeAdd64(5, 42, /*error_expected*/ false);
  TestSafeAdd64(kint64min / 2, kint64min / 2, /*error_expected=*/false);
  TestSafeAdd64(kint64max / 2, kint64max / 2, /*error_expected=*/false);
  TestSafeAdd64(kint64min / 2, kint64min / 2 - 2,
                /*error_expected=*/true);
  TestSafeAdd64(kint64max / 2, kint64max / 2 + 2,
                /*error_expected=*/true);
  TestSafeAdd64(kint64min / 2, kint64min / 2 - 1,
                /*error_expected=*/true);
  TestSafeAdd64(kint64max / 2, kint64max / 2 + 1,
                /*error_expected=*/false);
  TestSafeAdd64(kint64min, -1, /*error_expected=*/true);
  TestSafeAdd64(kint64max, +1, /*error_expected=*/true);
  TestSafeAdd64(kint64min, +1, /*error_expected=*/false);
  TestSafeAdd64(kint64max, -1, /*error_expected=*/false);
}

std::vector<int64_t> GenerateInt64ValuesForTesting() {
  std::vector<int64_t> v;
  // Generate a bunch of special values...
  for (int64_t base :
       {int64_t{0}, kint64min, kint64max, static_cast<int64_t>(kint32max),
        static_cast<int64_t>(kint32min)}) {
    // ...scaled by various factor...
    for (int64_t scaled : {base, base / 2, base / 3 * 2}) {
      // ...with small offsets added.
      for (int64_t offset : {-1000, -2, -1, 0, 1, 2, 1000}) {
        // Avoid generating an overflow here. Do not want saturated as we
        // want corner-case numbers, not saturated arithmetic.
        v.push_back(TwosComplementAddition(scaled, offset));
      }
    }
  }
  return v;
}

class SaturatedArithmeticTest : public ::testing::Test {
 protected:
  // Reference (safe, but slow) versions of the functions whose behavior
  // we are testing. We'd like to use int128, but base/int128.h only provides
  // uint128, so we use bit tricks, and define our own conversions
  // uint128 <-> int64_t (see the "private" section).
  int64_t ReferenceCapAdd(int64_t a, int64_t b) {
    return Int128ToCapped64(Int64To128(a) + Int64To128(b));
  }
  int64_t ReferenceCapSub(int64_t a, int64_t b) {
    return Int128ToCapped64(Int64To128(a) - Int64To128(b));
  }
  int64_t ReferenceCapOpp(int64_t v) {
    return Int128ToCapped64(-Int64To128(v));
  }
  int64_t ReferenceCapProd(int64_t a, int64_t b) {
    return Int128ToCapped64(Int64To128(a) * Int64To128(b));
  }

  void SetUp() override {
    int64_values_ = GenerateInt64ValuesForTesting();
    // Now, add a few random values.
    const int kNumRandomValues = 1000;
    std::mt19937 random(12345);
    for (int i = 0; i < kNumRandomValues; ++i) {
      int64_values_.push_back(
          static_cast<int64_t>(absl::Uniform<uint64_t>(random)));
    }
  }

  std::vector<int64_t> int64_values_;

 private:
  bool IsNegative(absl::uint128 x) {
    return static_cast<int64_t>(absl::Uint128High64(x)) < 0;
  }

  absl::uint128 Int64To128(int64_t x) {
    return absl::MakeUint128(static_cast<uint64_t>(x < 0 ? -1 : 0),
                             static_cast<uint64_t>(x));
  }

  int64_t Int128ToCapped64(absl::uint128 x) {
    const int64_t low = static_cast<int64_t>(absl::Uint128Low64(x));
    if (IsNegative(x)) {
      if (absl::Uint128High64(x) != static_cast<uint64_t>(-1)) return kint64min;
      if (low >= 0) return kint64min;
      return low;
    }
    if (absl::Uint128High64(x) != 0) return kint64max;
    if (low < 0) return kint64max;
    return low;
  }

  uint64_t Uint128ToCappedU64(absl::uint128 x) {
    return x > absl::uint128(kuint64max) ? kuint64max : absl::Uint128Low64(x);
  }
};

// Don't use EXPECT to avoid gunit issues -- we test ~1M pairs of
// values, and generating that much output can freeze your test.
#define TEST_BINARY_OPERATOR(name, symbol, reference) \
  TEST_F(SaturatedArithmeticTest, name) {             \
    for (int64_t a : int64_values_) {                 \
      for (int64_t b : int64_values_) {               \
        ASSERT_EQ(reference(a, b), name(a, b))        \
            << a << " " << symbol << " " << b;        \
      }                                               \
    }                                                 \
  }

TEST_BINARY_OPERATOR(CapAdd, "+", ReferenceCapAdd);
TEST_BINARY_OPERATOR(CapAddGeneric, "+", ReferenceCapAdd);
TEST_BINARY_OPERATOR(CapSub, "-", ReferenceCapSub);
TEST_BINARY_OPERATOR(CapSubGeneric, "-", ReferenceCapSub);
TEST_BINARY_OPERATOR(CapProd, "*", ReferenceCapProd);
TEST_BINARY_OPERATOR(CapProdGeneric, "*", ReferenceCapProd);

#if defined(__GNUC__) && defined(__x86_64__)
TEST_BINARY_OPERATOR(CapAddAsm, "+", ReferenceCapAdd);
TEST_BINARY_OPERATOR(CapSubAsm, "-", ReferenceCapSub);
TEST_BINARY_OPERATOR(CapProdAsm, "*", ReferenceCapProd);
#endif

TEST_F(SaturatedArithmeticTest, CapOpp) {
  for (int64_t a : int64_values_) {
    ASSERT_EQ(ReferenceCapOpp(a), CapOpp(a)) << a;
  }
}

TEST_F(SaturatedArithmeticTest, CapAbs) {
  ASSERT_EQ(CapAbs(kint64min), kint64max);
  ASSERT_EQ(CapAbs(kint64max), kint64max);
  ASSERT_EQ(CapAbs(-100), 100);
  ASSERT_EQ(CapAbs(0), 0);
  ASSERT_EQ(CapAbs(200), 200);
}

TEST_F(SaturatedArithmeticTest, CapAddTo) {
  for (int64_t a : int64_values_) {
    for (int64_t b : int64_values_) {
      int64_t result = a;
      CapAddTo(b, &result);
      ASSERT_EQ(result, CapAdd(a, b)) << a << " + " << b;
    }
  }
}

TEST_F(SaturatedArithmeticTest, AddIntoOverflow) {
  for (int64_t a : int64_values_) {
    for (int64_t b : int64_values_) {
      int64_t result = b;
      if (AddIntoOverflow(a, &result)) {
        EXPECT_TRUE(AddOverflows(a, b));
      } else {
        EXPECT_FALSE(AddOverflows(a, b));
        EXPECT_EQ(result, a + b);
      }
    }
  }
}

TEST_F(SaturatedArithmeticTest, CapSubFrom) {
  for (int64_t a : int64_values_) {
    for (int64_t b : int64_values_) {
      int64_t result = a;
      CapSubFrom(/*amount=*/b, /*target=*/&result);
      ASSERT_EQ(result, CapSub(a, b)) << a << " + " << b;
    }
  }
}

TEST(CapOrFloatAdd, VariousTypes) {
  constexpr double kInf = std::numeric_limits<float>::infinity();
  EXPECT_EQ(CapOrFloatAdd(0.5, 0.25), 0.75);
  EXPECT_EQ(CapOrFloatAdd(kInf, 0.25), kInf);
  EXPECT_EQ(CapOrFloatAdd(int64_t{1}, kint64max), kint64max);
  EXPECT_EQ(CapOrFloatAdd(4, kint32max - 2), kint32max);

  // A corner case: float isn't accurate enough to represent 1+1e-10, which we
  // use to verify that the return type is the same as the argument type.
  EXPECT_EQ(static_cast<double>(CapOrFloatAdd(float{1}, float{1e-10})), 1.0);
}

// TODO(user): do the same for CapOpp.

}  // namespace
}  // namespace operations_research
