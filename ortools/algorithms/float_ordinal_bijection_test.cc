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
#include <bit>
#include <bitset>
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

// Generates a vector of `length` uniformly distributed random unsigned integer
// ordinals of type `Ordinal<Float>` across the full bit-width range.
template <typename Float>
std::vector<Ordinal<Float>> GenerateRandomOrdinalVector(
    const std::size_t length) {
  static_assert(std::is_floating_point_v<Float>);
  absl::BitGen bitgen;
  std::vector<Ordinal<Float>> values(length);
  for (std::size_t i = 0; i < length; ++i) {
    values[i] = absl::Uniform<Ordinal<Float>>(
        absl::IntervalClosedClosed, bitgen,
        std::numeric_limits<Ordinal<Float>>::lowest(),
        std::numeric_limits<Ordinal<Float>>::max());
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
  using Ordinal = Ordinal<TypeParam>;

  for (std::size_t i = 0; i < values.size(); ++i) {
    for (std::size_t j = 0; j < 10; ++j) {
      const TypeParam f1 = values[i];
      const TypeParam f2 = values[(i + j) % values.size()];
      const Ordinal u1 = FloatToOrdinal(f1);
      const Ordinal u2 = FloatToOrdinal(f2);
      EXPECT_EQ(u1 < u2, f1 < f2) << "f1 = " << f1 << ", f2 = " << f2;
      EXPECT_EQ(u1 > u2, f1 > f2) << "f1 = " << f1 << ", f2 = " << f2;
      EXPECT_EQ(u1 == u2, f1 == f2) << "f1 = " << f1 << ", f2 = " << f2;
    }
  }
}

TYPED_TEST(FloatOrdinalBijectionTest,
           OrdinalToFloatIsLeftInverseOfFloatToOrdinal) {
  constexpr std::size_t kLength = 10'000;
  using Ordinal = Ordinal<TypeParam>;
  const std::vector<Ordinal> values =
      GenerateRandomOrdinalVector<TypeParam>(kLength);

  for (const Ordinal f_bits : values) {
    const TypeParam f = std::bit_cast<TypeParam>(f_bits);
    const Ordinal uint_val = FloatToOrdinal(f);
    const TypeParam float_val = OrdinalToFloat(uint_val);
    if (std::isnan(f)) {
      EXPECT_TRUE(std::isnan(float_val))
          << "f = " << f << ", float_val = " << float_val << " ("
          << std::bitset<8 * sizeof(TypeParam)>(f_bits) << ")";
    } else {
      EXPECT_EQ(float_val, f)
          << "f = " << f << ", float_val = " << float_val << " ("
          << std::bitset<8 * sizeof(TypeParam)>(f_bits) << ")";
    }
  }
}

TYPED_TEST(FloatOrdinalBijectionTest,
           FloatToOrdinalIsLeftInverseOfOrdinalToFloat) {
  constexpr std::size_t kLength = 10'000;
  using Ordinal = Ordinal<TypeParam>;
  const std::vector<Ordinal> values =
      GenerateRandomOrdinalVector<TypeParam>(kLength);

  for (const Ordinal u : values) {
    const TypeParam float_val = OrdinalToFloat(u);
    const Ordinal uint_val = FloatToOrdinal(float_val);
    EXPECT_EQ(uint_val, u) << "u = " << u << ", uint_val = " << uint_val << " ("
                           << std::bitset<8 * sizeof(TypeParam)>(u) << ")";
  }
}

TYPED_TEST(FloatOrdinalBijectionTest,
           OrdinalMidpointIsWithinBoundsAndOrdinalMiddle) {
  constexpr std::size_t kLength = 10'000;
  const std::vector<TypeParam> values =
      GenerateRandomFloatVector<TypeParam>(kLength);

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
    }
  }
}

TYPED_TEST(FloatOrdinalBijectionTest,
           OrdinalMidpointBisectionConvergesInBitWidthSteps) {
  constexpr int kMaxSteps = sizeof(TypeParam) * 8;
  using Ordinal = Ordinal<TypeParam>;
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
    TypeParam low = -std::numeric_limits<TypeParam>::infinity();
    TypeParam high = std::numeric_limits<TypeParam>::infinity();
    int steps = 0;
    while (low < high) {
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
    EXPECT_LE(FloatToOrdinal(high) - FloatToOrdinal(low), Ordinal{1});
    EXPECT_EQ(high, target);
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

// Evaluates `std::midpoint(a, b)`. Taking the address of a non-member standard
// library function such as `std::midpoint<Float>` has unspecified behavior in
// C++20 (`[namespace.std]` §16.4.5.2.1), and `<numeric>` also overloads
// `std::midpoint` for both arithmetic types `T(T, T)` and pointers `T*(T*,
// T*)`, making `std::midpoint<Float>` an ambiguous overload set when passed to
// a deduced template parameter `MidpointFn`.
template <typename Float>
Float StdMidpoint(const Float a, const Float b) {
  return std::midpoint(a, b);
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

// Evaluates `x * x - 2` to bisect `sqrt(2)`.
template <typename Float>
Float Sqrt2Residual(const Float x) {
  return x * x - Float{2};
}

// Evaluates Isaac Newton's historical cubic `y^3 - 2 * y - 5` from *De analysi
// per aequationes numero terminorum infinitas* (1669, published 1711) and
// *Methodus fluxionum et serierum infinitarum* (1671, published 1736), which
// has a unique real root at `y* ~= 2.09455148154232659148`.
template <typename Float>
Float NewtonCubicResidual(const Float y) {
  return y * y * y - Float{2} * y - Float{5};
}

// Runs `BisectMonotonic` with `OrdinalMidpoint` and `OrdinalBisectMonotonic`
// on `[low, high]` for `fn`, verifying 1-ULP root bracketing and that both
// take `expected_ordinal_steps`. When `expected_std_steps >= 0`, also runs
// `BisectMonotonic` with `std::midpoint` and verifies that it converges to
// the same bracket in `expected_std_steps`. Returns the converged bracket.
template <typename Float, typename Function>
BisectionResult<Float> VerifyBisectionBracket(
    const Float low, const Float high, Function fn,
    const int expected_std_steps, const int expected_ordinal_steps) {
  const BisectionResult<Float> ordinal_res =
      BisectMonotonic(low, high, fn, OrdinalMidpoint<Float>);
  const BisectionResult<Float> direct_res =
      OrdinalBisectMonotonic(low, high, fn);

  EXPECT_EQ(std::nextafter(ordinal_res.low, ordinal_res.high),
            ordinal_res.high);
  EXPECT_LE(fn(ordinal_res.low), Float{0});
  EXPECT_GE(fn(ordinal_res.high), Float{0});
  EXPECT_EQ(ordinal_res.num_steps, expected_ordinal_steps);

  EXPECT_EQ(direct_res.low, ordinal_res.low);
  EXPECT_EQ(direct_res.high, ordinal_res.high);
  EXPECT_EQ(direct_res.num_steps, expected_ordinal_steps);

  if (expected_std_steps >= 0) {
    const BisectionResult<Float> std_res =
        BisectMonotonic(low, high, fn, StdMidpoint<Float>);
    EXPECT_EQ(std_res.low, ordinal_res.low);
    EXPECT_EQ(std_res.high, ordinal_res.high);
    EXPECT_EQ(std_res.num_steps, expected_std_steps);
  }
  return ordinal_res;
}

// Verifies bisection of `fn` (whose unique root has unbiased IEEE 754
// exponent `kRootExponent`) across `[0, max]`, `[-max, max]`, and
// `[-inf, inf]`, asserting that all brackets converge to the same 1-ULP
// interval.
template <typename Float, int kRootExponent, typename Function>
BisectionResult<Float> VerifyWidePositiveRootBisection(Function fn) {
  constexpr Float kMax = std::numeric_limits<Float>::max();
  constexpr Float kInf = std::numeric_limits<Float>::infinity();
  constexpr int kNumBits = sizeof(Float) * 8;
  constexpr int kMantissaBits = std::numeric_limits<Float>::digits - 1;
  constexpr int kExponentHalvings =
      std::numeric_limits<Float>::max_exponent - kRootExponent;

  // `std::midpoint` requires `kExponentHalvings + kMantissaBits` steps on
  // `[0, kMax]` (`+1` on `[-kMax, kMax]`), whereas `OrdinalMidpoint` and
  // `OrdinalBisectMonotonic` take `kNumBits - 1` steps on `[0, kMax]` and
  // `kNumBits` steps on `[-kMax, kMax]` and `[-kInf, kInf]`.
  const BisectionResult<Float> nonneg_res = VerifyBisectionBracket(
      Float{0}, kMax, fn, kExponentHalvings + kMantissaBits, kNumBits - 1);
  const BisectionResult<Float> sym_res = VerifyBisectionBracket(
      -kMax, kMax, fn, kExponentHalvings + 1 + kMantissaBits, kNumBits);
  const BisectionResult<Float> inf_res =
      VerifyBisectionBracket(-kInf, kInf, fn, -1, kNumBits);

  EXPECT_EQ(sym_res.low, nonneg_res.low);
  EXPECT_EQ(sym_res.high, nonneg_res.high);
  EXPECT_EQ(inf_res.low, nonneg_res.low);
  EXPECT_EQ(inf_res.high, nonneg_res.high);
  return nonneg_res;
}

TYPED_TEST(FloatOrdinalBijectionTest, ExpMBisection) {
  using Float = TypeParam;
  constexpr Float kMax = std::numeric_limits<Float>::max();
  constexpr Float kSubnormalMin = std::numeric_limits<Float>::denorm_min();
  constexpr Float kInf = std::numeric_limits<Float>::infinity();
  constexpr int kNumBits = sizeof(Float) * 8;
  constexpr int kMantissaBits = std::numeric_limits<Float>::digits - 1;

  // 1. Bisect `exp(x) - 1e38 = 0`, whose root `38 * ln(10) ~= 87.498` lies in
  // `[64, 128) = [2^6, 2^7)` (unbiased exponent `e* = 6`).
  VerifyWidePositiveRootBisection<Float, 6>(ExpM<Float{1e38}>);

  // 2. Bisect `exp(x) - 1 = 0` (`x* = 0`), which reaches the maximum number of
  // exponent-halving steps across IEEE 754 floating-point numbers.
  constexpr int kMaxNormalExponentHalvings =
      std::numeric_limits<Float>::max_exponent -
      std::numeric_limits<Float>::min_exponent + 1;

  const BisectionResult<Float> nonneg_res = VerifyBisectionBracket(
      Float{0}, kMax, ExpM<Float{1.0}>,
      kMaxNormalExponentHalvings + kMantissaBits, kNumBits - 2);
  EXPECT_EQ(nonneg_res.low, Float{0});
  EXPECT_EQ(nonneg_res.high, kSubnormalMin);

  // On `[-kMax, kMax]`, `std::midpoint` requires 1 additional halving to reach
  // the global maximum of `278` steps (`float`) / `2,099` steps (`double`).
  const BisectionResult<Float> max_std_res =
      BisectMonotonic(-kMax, kMax, ExpM<Float{1.0}>, StdMidpoint<Float>);
  const BisectionResult<Float> asym_ordinal_res = BisectMonotonic(
      Float{-0.5} * kMax, kMax, ExpM<Float{1.0}>, OrdinalMidpoint<Float>);
  EXPECT_EQ(max_std_res.low, Float{0});
  EXPECT_EQ(max_std_res.high, kSubnormalMin);
  EXPECT_EQ(max_std_res.num_steps,
            kMaxNormalExponentHalvings + 1 + kMantissaBits);
  EXPECT_EQ(asym_ordinal_res.low, Float{0});
  EXPECT_EQ(asym_ordinal_res.high, kSubnormalMin);
  EXPECT_EQ(asym_ordinal_res.num_steps, kNumBits - 1);

  // Because `OrdinalBisectMonotonic` checks termination on integer ordinals
  // rather than floating-point equality, it distinguishes `-0.0` and `+0.0`
  // even on symmetric intervals `[-kMax, kMax]` and `[-kInf, kInf]`.
  const BisectionResult<Float> sym_direct_res =
      OrdinalBisectMonotonic(-kMax, kMax, ExpM<Float{1.0}>);
  const BisectionResult<Float> inf_direct_res =
      OrdinalBisectMonotonic(-kInf, kInf, ExpM<Float{1.0}>);
  EXPECT_EQ(sym_direct_res.low, Float{0});
  EXPECT_EQ(sym_direct_res.high, kSubnormalMin);
  EXPECT_EQ(sym_direct_res.num_steps, kNumBits);
  EXPECT_EQ(inf_direct_res.low, Float{0});
  EXPECT_EQ(inf_direct_res.high, kSubnormalMin);
  EXPECT_EQ(inf_direct_res.num_steps, kNumBits);
}

TYPED_TEST(FloatOrdinalBijectionTest, SqrtBisection) {
  using Float = TypeParam;
  constexpr int kMantissaBits = std::numeric_limits<Float>::digits - 1;

  // Within the single binade `[1, 2]`, `FloatToOrdinal` is affine in `x`
  // (`FloatToOrdinal(2) - FloatToOrdinal(1) == 2^M`), so `OrdinalMidpoint`,
  // `OrdinalBisectMonotonic`, and `std::midpoint` follow the exact same
  // trajectory in `kMantissaBits` steps (`23` for `float`, `52` for `double`).
  VerifyBisectionBracket(Float{1}, Float{2}, Sqrt2Residual<Float>,
                         kMantissaBits, kMantissaBits);
}

TYPED_TEST(FloatOrdinalBijectionTest, NewtonCubicBisection) {
  using Float = TypeParam;
  constexpr int kMantissaBits = std::numeric_limits<Float>::digits - 1;

  // On Newton's historical bracket `[2, 3]` (half of binade `[2, 4]`), all
  // three methods take `kMantissaBits - 1` steps (`22` for `float`, `51` for
  // `double`) and converge to the same bracket as wide-range bisection (`e* =
  // 1`).
  const BisectionResult<Float> bracket_23_res =
      VerifyBisectionBracket(Float{2}, Float{3}, NewtonCubicResidual<Float>,
                             kMantissaBits - 1, kMantissaBits - 1);
  const BisectionResult<Float> wide_res =
      VerifyWidePositiveRootBisection<Float, 1>(NewtonCubicResidual<Float>);

  EXPECT_EQ(wide_res.low, bracket_23_res.low);
  EXPECT_EQ(wide_res.high, bracket_23_res.high);
}

}  // namespace
}  // namespace operations_research
