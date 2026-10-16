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

#include "ortools/algorithms/float_ordinal_bijection.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numeric>
#include <type_traits>
#include <vector>

#include "absl/random/distributions.h"
#include "absl/random/random.h"
#include "gtest/gtest.h"

namespace operations_research {
namespace {

// Generates a vector of `length` random floating-point numbers of type
// `Float` across `[-0.5 * max(), 0.5 * max()]` (halved to avoid overflow in
// `absl::Uniform`).
template <typename Float>
std::vector<Float> GenerateRandomFloatVector(const std::size_t length) {
  static_assert(std::is_floating_point_v<Float>);
  const Float half_max_val = Float{0.5} * std::numeric_limits<Float>::max();
  absl::BitGen bitgen;
  std::vector<Float> values(length);
  for (std::size_t i = 0; i < length; ++i) {
    values[i] = absl::Uniform<Float>(absl::IntervalClosedClosed, bitgen,
                                     -half_max_val, half_max_val);
  }
  return values;
}

// Test fixture for order-preserving floating-point to unsigned integer ordinal
// bijections across `float` and `double`.
template <typename Float>
class FloatOrdinalBijectionTest : public ::testing::Test {};

using FloatingPointTypes = ::testing::Types<float, double>;
TYPED_TEST_SUITE(FloatOrdinalBijectionTest, FloatingPointTypes);

TYPED_TEST(FloatOrdinalBijectionTest, FloatToOrdinalPreservesOrder) {
  constexpr std::size_t kLength = 10'000;
  const std::vector<TypeParam> values =
      GenerateRandomFloatVector<TypeParam>(kLength);
  using UintType = decltype(FloatToOrdinal(TypeParam{0}));

  for (std::size_t i = 0; i < values.size(); ++i) {
    for (std::size_t j = 0; j < 10; ++j) {
      const TypeParam f1 = values[i];
      const TypeParam f2 = values[(i + j) % values.size()];
      const UintType u1 = FloatToOrdinal(f1);
      const UintType u2 = FloatToOrdinal(f2);
      EXPECT_EQ(u1 < u2, f1 < f2) << "f1 = " << f1 << ", f2 = " << f2;
      EXPECT_EQ(u1 > u2, f1 > f2) << "f1 = " << f1 << ", f2 = " << f2;
      EXPECT_EQ(u1 == u2, f1 == f2) << "f1 = " << f1 << ", f2 = " << f2;
    }
  }
}

TYPED_TEST(FloatOrdinalBijectionTest, FloatToOrdinalIsBijection) {
  constexpr std::size_t kLength = 10'000;
  const std::vector<TypeParam> values =
      GenerateRandomFloatVector<TypeParam>(kLength);
  using UintType = decltype(FloatToOrdinal(TypeParam{0}));

  for (const TypeParam f : values) {
    const UintType uint_val = FloatToOrdinal(f);
    const TypeParam float_val = OrdinalToFloat(uint_val);
    EXPECT_EQ(float_val, f) << "f = " << f << ", float_val = " << float_val;
  }
}

TYPED_TEST(FloatOrdinalBijectionTest,
           OrdinalMidpointIsWithinBoundsAndOrdinalMiddle) {
  constexpr std::size_t kLength = 10'000;
  const std::vector<TypeParam> values =
      GenerateRandomFloatVector<TypeParam>(kLength);
  using UintType = decltype(FloatToOrdinal(TypeParam{0}));

  EXPECT_EQ(OrdinalMidpoint(TypeParam{-0.0}, TypeParam{0.0}), TypeParam{0.0});
  EXPECT_EQ(OrdinalMidpoint(TypeParam{0.0}, TypeParam{-0.0}), TypeParam{0.0});
  EXPECT_EQ(OrdinalMidpoint(std::numeric_limits<TypeParam>::lowest(),
                            std::numeric_limits<TypeParam>::max()),
            TypeParam{0.0});

  for (std::size_t i = 0; i < values.size(); ++i) {
    EXPECT_EQ(OrdinalMidpoint(values[i], values[i]), values[i]);
    for (std::size_t j = 1; j < 10; ++j) {
      const TypeParam f1 = values[i];
      const TypeParam f2 = values[(i + j) % values.size()];
      const TypeParam mid = OrdinalMidpoint(f1, f2);
      EXPECT_GE(mid, std::min(f1, f2)) << "f1 = " << f1 << ", f2 = " << f2;
      EXPECT_LE(mid, std::max(f1, f2)) << "f1 = " << f1 << ", f2 = " << f2;
      const UintType expected_uint =
          std::midpoint(FloatToOrdinal(f1), FloatToOrdinal(f2));
      EXPECT_EQ(FloatToOrdinal(mid), expected_uint);
    }
  }
}

TYPED_TEST(FloatOrdinalBijectionTest,
           OrdinalMidpointBisectionConvergesInBitWidthSteps) {
  constexpr int kMaxSteps = sizeof(TypeParam) * 8;
  using UintType = decltype(FloatToOrdinal(TypeParam{0}));
  const std::vector<TypeParam> targets = {
      std::numeric_limits<TypeParam>::lowest(),
      TypeParam{-1e10},
      TypeParam{-1.0},
      -std::numeric_limits<TypeParam>::min(),
      TypeParam{0.0},
      std::numeric_limits<TypeParam>::min(),
      TypeParam{1.0},
      TypeParam{1e10},
      std::numeric_limits<TypeParam>::max(),
  };

  for (const TypeParam target : targets) {
    TypeParam low = std::numeric_limits<TypeParam>::lowest();
    TypeParam high = std::numeric_limits<TypeParam>::max();
    int steps = 0;
    while (low < high && steps < kMaxSteps) {
      const TypeParam mid = OrdinalMidpoint(low, high);
      if (mid == low || mid == high) {
        break;
      }
      if (target <= mid) {
        high = mid;
      } else {
        low = mid;
      }
      ++steps;
    }
    EXPECT_LE(steps, kMaxSteps);
    EXPECT_LE(FloatToOrdinal(high) - FloatToOrdinal(low), UintType{1});
    EXPECT_TRUE(low == target || high == target);
  }
}

// Holds the final bracketing interval `[low, high]` and the number of steps
// taken by `BisectMonotonic`.
template <typename Float>
struct BisectionResult {
  Float low;
  Float high;
  int num_steps;
};

// Bisects a monotonically increasing function `fn` on `[low, high]` using
// `midpoint_fn` until `mid == low` or `mid == high`, counting each step.
template <typename Float, typename Function, typename MidpointFn>
BisectionResult<Float> BisectMonotonic(Float low, Float high, Function fn,
                                       MidpointFn midpoint_fn) {
  int num_steps = 0;
  while (low < high) {
    const Float mid = midpoint_fn(low, high);
    if (mid == low || mid == high) {
      break;
    }
    ++num_steps;
    if (fn(mid) <= Float{0}) {
      low = mid;
    } else {
      high = mid;
    }
  }
  return {.low = low, .high = high, .num_steps = num_steps};
}

// Bisects a monotonically increasing function `fn` on `[low, high]` by
// transforming `low` and `high` to ordinals once and applying only the inverse
// transform `OrdinalToFloat` to evaluate `fn` at each step.
template <typename Float, typename Function>
BisectionResult<Float> OrdinalBisectMonotonic(const Float low, const Float high,
                                              Function fn) {
  using Ordinal = Ordinal<Float>;
  Ordinal low_ordinal = FloatToOrdinal(low);
  Ordinal high_ordinal = FloatToOrdinal(high);
  int num_steps = 0;
  while (low_ordinal < high_ordinal) {
    const Ordinal mid_ordinal = std::midpoint(low_ordinal, high_ordinal);
    if (mid_ordinal == low_ordinal || mid_ordinal == high_ordinal) {
      break;
    }
    ++num_steps;
    if (fn(OrdinalToFloat(mid_ordinal)) <= Float{0}) {
      low_ordinal = mid_ordinal;
    } else {
      high_ordinal = mid_ordinal;
    }
  }
  return {.low = OrdinalToFloat(low_ordinal),
          .high = OrdinalToFloat(high_ordinal),
          .num_steps = num_steps};
}

// Evaluates `exp(x) - kOffset`, using `std::expm1(x)` when `kOffset == 1.0` to
// avoid catastrophic cancellation when `|x| <= 2^-digits` and preserve sign
// accuracy down to `kSubnormalMin`.
template <auto kOffset, typename Float = decltype(kOffset)>
Float ExpM(const Float x) {
  if constexpr (kOffset == Float{1.0}) {
    return std::expm1(x);
  } else {
    return std::exp(x) - kOffset;
  }
}

TYPED_TEST(FloatOrdinalBijectionTest, ExpMBisection) {
  using Float = TypeParam;
  constexpr Float kMax = std::numeric_limits<Float>::max();
  constexpr Float kSubnormalMin = std::numeric_limits<Float>::denorm_min();
  constexpr Float kInf = std::numeric_limits<Float>::infinity();
  constexpr int kNumBits = sizeof(Float) * 8;
  constexpr int kMantissaBits = std::numeric_limits<Float>::digits - 1;
  const auto FloatMidPoint = [](const Float a, const Float b) -> Float {
    return std::midpoint(a, b);
  };

  // 1. Bisect `exp(x) - kBigValue = 0`, where `kBigValue = 1e38`. Its root
  // `38 * ln(10) ~= 87.498` lies in `[64, 128) = [2^6, 2^7)` (unbiased exponent
  // `e* = 6`), while staying below `FLT_MAX ~= 3.4e38` for `float`.
  constexpr Float kBigValue = Float{1e38};
  constexpr int kRootExponent = 6;
  constexpr int kExponentHalvings =
      std::numeric_limits<Float>::max_exponent - kRootExponent;

  const BisectionResult<Float> std_midpoint_res =
      BisectMonotonic(Float{0}, kMax, ExpM<kBigValue>, FloatMidPoint);
  const BisectionResult<Float> wide_std_midpoint_res =
      BisectMonotonic(-kMax, kMax, ExpM<kBigValue>, FloatMidPoint);
  const BisectionResult<Float> ordinal_midpoint_res =
      BisectMonotonic(Float{0}, kMax, ExpM<kBigValue>, OrdinalMidpoint<Float>);
  const BisectionResult<Float> inf_midpoint_res =
      BisectMonotonic(-kInf, kInf, ExpM<kBigValue>, OrdinalMidpoint<Float>);

  // Verify that all runs converge to the exact same 1-ULP bracket around the
  // root.
  EXPECT_EQ(std::nextafter(ordinal_midpoint_res.low, ordinal_midpoint_res.high),
            ordinal_midpoint_res.high);
  EXPECT_LE(ExpM<kBigValue>(ordinal_midpoint_res.low), Float{0});
  EXPECT_GE(ExpM<kBigValue>(ordinal_midpoint_res.high), Float{0});

  EXPECT_EQ(std_midpoint_res.low, ordinal_midpoint_res.low);
  EXPECT_EQ(std_midpoint_res.high, ordinal_midpoint_res.high);
  EXPECT_EQ(wide_std_midpoint_res.low, ordinal_midpoint_res.low);
  EXPECT_EQ(wide_std_midpoint_res.high, ordinal_midpoint_res.high);
  EXPECT_EQ(inf_midpoint_res.low, ordinal_midpoint_res.low);
  EXPECT_EQ(inf_midpoint_res.high, ordinal_midpoint_res.high);

  // `std::midpoint` requires `kExponentHalvings` steps (`122` for `float`,
  // `1,018` for `double`) on `[0, kMax]` plus `kMantissaBits` steps (`23` for
  // `float`, `52` for `double`), totaling `145` steps for `float` and `1,070`
  // steps for `double` (`146` and `1,071` on `[-kMax, kMax]`).
  // `OrdinalMidpoint` takes `kNumBits - 1` steps (`31` / `63`) on `[0, kMax]`
  // and `kNumBits` steps (`32` / `64`) on `[-inf, +inf]`.
  EXPECT_EQ(std_midpoint_res.num_steps, kExponentHalvings + kMantissaBits);
  EXPECT_EQ(wide_std_midpoint_res.num_steps,
            kExponentHalvings + 1 + kMantissaBits);
  EXPECT_EQ(ordinal_midpoint_res.num_steps, kNumBits - 1);
  EXPECT_EQ(inf_midpoint_res.num_steps, kNumBits);

  // 2. Bisect `exp(x) - 1 = 0` (`x* = 0`), which reaches the maximum number of
  // exponent-halving steps across IEEE 754 floating-point numbers.
  constexpr int kMaxNormalExponentHalvings =
      std::numeric_limits<Float>::max_exponent -
      std::numeric_limits<Float>::min_exponent + 1;

  // On `[0, kMax]`, `std::midpoint` performs `kMaxNormalExponentHalvings`
  // normal exponent halvings (`254` for `float`, `2,046` for `double`) plus
  // `kMantissaBits` subnormal halvings (`23` for `float`, `52` for `double`),
  // totaling `277` steps for `float` and `2,098` steps for `double`.
  const BisectionResult<Float> nonneg_std_res =
      BisectMonotonic(Float{0}, kMax, ExpM<Float{1.0}>, FloatMidPoint);
  const BisectionResult<Float> nonneg_ordinal_res =
      BisectMonotonic(Float{0}, kMax, ExpM<Float{1.0}>, OrdinalMidpoint<Float>);

  EXPECT_EQ(nonneg_std_res.low, Float{0});
  EXPECT_EQ(nonneg_std_res.high, kSubnormalMin);
  EXPECT_EQ(nonneg_ordinal_res.low, Float{0});
  EXPECT_EQ(nonneg_ordinal_res.high, kSubnormalMin);
  EXPECT_EQ(nonneg_std_res.num_steps,
            kMaxNormalExponentHalvings + kMantissaBits);
  EXPECT_EQ(nonneg_ordinal_res.num_steps, kNumBits - 2);

  // On `[-kMax, kMax]`, `std::midpoint` requires 1 additional halving to reach
  // the global maximum of `278` steps for `float` and `2,099` steps for
  // `double`.
  const BisectionResult<Float> max_std_res =
      BisectMonotonic(-kMax, kMax, ExpM<Float{1.0}>, FloatMidPoint);
  const BisectionResult<Float> wide_ordinal_res = BisectMonotonic(
      Float{-0.5} * kMax, kMax, ExpM<Float{1.0}>, OrdinalMidpoint<Float>);

  EXPECT_EQ(max_std_res.low, Float{0});
  EXPECT_EQ(max_std_res.high, kSubnormalMin);
  EXPECT_EQ(wide_ordinal_res.low, Float{0});
  EXPECT_EQ(wide_ordinal_res.high, kSubnormalMin);
  EXPECT_EQ(max_std_res.num_steps,
            kMaxNormalExponentHalvings + 1 + kMantissaBits);
  EXPECT_EQ(wide_ordinal_res.num_steps, kNumBits - 1);
}

// Evaluates `x * x - 2` to bisect `sqrt(2)`.
template <typename Float>
Float Sqrt2Residual(const Float x) {
  return x * x - Float{2};
}

TYPED_TEST(FloatOrdinalBijectionTest, SqrtBisection) {
  using Float = TypeParam;
  constexpr int kMantissaBits = std::numeric_limits<Float>::digits - 1;
  const auto FloatMidPoint = [](const Float a, const Float b) -> Float {
    return std::midpoint(a, b);
  };

  // Within the single binade `[1, 2]`, `FloatToOrdinal` is affine in `x`
  // (`FloatToOrdinal(2) - FloatToOrdinal(1) == 2^M`), so `OrdinalMidpoint` and
  // `std::midpoint` follow the exact same trajectory and both converge in
  // `kMantissaBits` steps (`23` for `float`, `52` for `double`).
  const BisectionResult<Float> std_midpoint_res =
      BisectMonotonic(Float{1}, Float{2}, Sqrt2Residual<Float>, FloatMidPoint);
  const BisectionResult<Float> ordinal_midpoint_res = BisectMonotonic(
      Float{1}, Float{2}, Sqrt2Residual<Float>, OrdinalMidpoint<Float>);

  EXPECT_EQ(std::nextafter(ordinal_midpoint_res.low, ordinal_midpoint_res.high),
            ordinal_midpoint_res.high);
  EXPECT_LE(Sqrt2Residual(ordinal_midpoint_res.low), Float{0});
  EXPECT_GE(Sqrt2Residual(ordinal_midpoint_res.high), Float{0});

  EXPECT_EQ(std_midpoint_res.low, ordinal_midpoint_res.low);
  EXPECT_EQ(std_midpoint_res.high, ordinal_midpoint_res.high);
  EXPECT_EQ(std_midpoint_res.num_steps, kMantissaBits);
  EXPECT_EQ(ordinal_midpoint_res.num_steps, kMantissaBits);
}

TYPED_TEST(FloatOrdinalBijectionTest, OrdinalBisectMonotonicExpM) {
  using Float = TypeParam;
  constexpr Float kMax = std::numeric_limits<Float>::max();
  constexpr Float kSubnormalMin = std::numeric_limits<Float>::denorm_min();
  constexpr Float kInf = std::numeric_limits<Float>::infinity();
  constexpr int kNumBits = sizeof(Float) * 8;

  // 1. Bisect `exp(x) - kBigValue = 0`, where `kBigValue = 1e38`.
  constexpr Float kBigValue = Float{1e38};
  const BisectionResult<Float> expected_big_res =
      BisectMonotonic(Float{0}, kMax, ExpM<kBigValue>, OrdinalMidpoint<Float>);
  const BisectionResult<Float> nonneg_big_res =
      OrdinalBisectMonotonic(Float{0}, kMax, ExpM<kBigValue>);
  const BisectionResult<Float> wide_big_res =
      OrdinalBisectMonotonic(-kMax, kMax, ExpM<kBigValue>);
  const BisectionResult<Float> inf_big_res =
      OrdinalBisectMonotonic(-kInf, kInf, ExpM<kBigValue>);

  EXPECT_EQ(nonneg_big_res.low, expected_big_res.low);
  EXPECT_EQ(nonneg_big_res.high, expected_big_res.high);
  EXPECT_EQ(wide_big_res.low, expected_big_res.low);
  EXPECT_EQ(wide_big_res.high, expected_big_res.high);
  EXPECT_EQ(inf_big_res.low, expected_big_res.low);
  EXPECT_EQ(inf_big_res.high, expected_big_res.high);
  EXPECT_EQ(nonneg_big_res.num_steps, kNumBits - 1);
  EXPECT_EQ(wide_big_res.num_steps, kNumBits);
  EXPECT_EQ(inf_big_res.num_steps, kNumBits);

  // 2. Bisect `exp(x) - 1 = 0` (`x* = 0`). Because `OrdinalBisectMonotonic`
  // checks termination on integer ordinals rather than floating-point equality,
  // it distinguishes `-0.0` and `+0.0` even on symmetric intervals `[-kMax,
  // kMax]` and `[-kInf, kInf]`.
  const BisectionResult<Float> nonneg_one_res =
      OrdinalBisectMonotonic(Float{0}, kMax, ExpM<Float{1.0}>);
  const BisectionResult<Float> wide_one_res =
      OrdinalBisectMonotonic(-kMax, kMax, ExpM<Float{1.0}>);
  const BisectionResult<Float> inf_one_res =
      OrdinalBisectMonotonic(-kInf, kInf, ExpM<Float{1.0}>);

  EXPECT_EQ(nonneg_one_res.low, Float{0});
  EXPECT_EQ(nonneg_one_res.high, kSubnormalMin);
  EXPECT_EQ(wide_one_res.low, Float{0});
  EXPECT_EQ(wide_one_res.high, kSubnormalMin);
  EXPECT_EQ(inf_one_res.low, Float{0});
  EXPECT_EQ(inf_one_res.high, kSubnormalMin);
  EXPECT_EQ(nonneg_one_res.num_steps, kNumBits - 2);
  EXPECT_EQ(wide_one_res.num_steps, kNumBits);
  EXPECT_EQ(inf_one_res.num_steps, kNumBits);
}

TYPED_TEST(FloatOrdinalBijectionTest, OrdinalBisectMonotonicSqrt) {
  using Float = TypeParam;
  constexpr int kMantissaBits = std::numeric_limits<Float>::digits - 1;

  const BisectionResult<Float> expected_res = BisectMonotonic(
      Float{1}, Float{2}, Sqrt2Residual<Float>, OrdinalMidpoint<Float>);
  const BisectionResult<Float> ordinal_bisect_res =
      OrdinalBisectMonotonic(Float{1}, Float{2}, Sqrt2Residual<Float>);

  EXPECT_EQ(ordinal_bisect_res.low, expected_res.low);
  EXPECT_EQ(ordinal_bisect_res.high, expected_res.high);
  EXPECT_EQ(ordinal_bisect_res.num_steps, kMantissaBits);
}

}  // namespace
}  // namespace operations_research
