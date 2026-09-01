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

#include "ortools/util/fp_utils.h"

#include <math.h>

#include <cfenv>  // NOLINT(build/c++11)
#include <cmath>
#include <cstdint>
#include <limits>
#include <random>
#include <vector>

#include "absl/base/casts.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/random/random.h"
#include "absl/strings/str_format.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/types.h"

namespace operations_research {
namespace {

// To have more readable one-liners below.
const double kInfinity = std::numeric_limits<double>::infinity();

// An intricate way to set 'result' to zero, to avoid compiler optimization.
double GenerateZero() {
  const int n = 100;
  double result = n * (n - 1) / 2;
  for (int i = 0; i < n; ++i) {
    result -= i;
  }
  CHECK_EQ(0.0, result);
  return result;
}

double GenerateNaN() { return GenerateZero() / GenerateZero(); }

double GenerateDivisionByZero() { return 1.0 / GenerateZero(); }

void RunFloatingPointExceptionTest(int exception_flags, bool detect_nan,
                                   bool detect_div_by_zero) {
  ScopedFloatingPointEnv scoped_fenv;
  if (!scoped_fenv.EnableExceptions(exception_flags)) {
    GTEST_SKIP() << "Floating point exceptions are not supported.";
  }
  if (detect_nan) {
    EXPECT_DEATH(
        {
          const double x = GenerateNaN();
          EXPECT_NE(x, x);  // Use the value to prevent removal by optimization.
        },
        "");
  } else {
    const double x = GenerateNaN();
    EXPECT_NE(x, x);
  }
  if (detect_div_by_zero) {
    EXPECT_DEATH(
        {
          // Use the value to prevent removal by optimization.
          EXPECT_EQ(kInfinity, GenerateDivisionByZero());
        },
        "");
  } else {
    EXPECT_EQ(kInfinity, GenerateDivisionByZero());
  }
}

TEST(ScopedFpEnv, NoDetection) {
  RunFloatingPointExceptionTest(0, false, false);
}

TEST(ScopedFpEnv, NanDetection) {
  RunFloatingPointExceptionTest(FE_INVALID, true, false);
}

TEST(ScopedFpEnv, DivisionByZeroDetection) {
  RunFloatingPointExceptionTest(FE_DIVBYZERO, false, true);
}

TEST(ScopedFpEnv, NanDetectionDivisionByZeroDetection) {
  RunFloatingPointExceptionTest(FE_INVALID | FE_DIVBYZERO, true, true);
}

TEST(Comparable, ComparisonsWithinTolerances) {
  EXPECT_COMPARABLE(-kInfinity, -kInfinity, 1e-6);
  EXPECT_COMPARABLE(kInfinity, kInfinity, 1e-6);
  EXPECT_NOTCOMPARABLE(-kInfinity, kInfinity, 1e-6);
  EXPECT_NOTCOMPARABLE(kInfinity, -kInfinity, 1e-6);
  EXPECT_NOTCOMPARABLE(1.0, kInfinity, 1e-6);
  EXPECT_NOTCOMPARABLE(-kInfinity, 1.0, 1e-6);
  EXPECT_COMPARABLE(1.0, 1.0 + 1e-7, 1e-6);
  EXPECT_NOTCOMPARABLE(1.0, 1.0 + 1e-5, 1e-6);
  EXPECT_COMPARABLE(0.0, 0.0 + 1e-7, 1e-6);
  EXPECT_NOTCOMPARABLE(0.0, 0.0 + 1e-5, 1e-6);
  EXPECT_NOTCOMPARABLE(1.0, std::numeric_limits<double>::quiet_NaN(), 1e-6);
}

TEST(Comparable, ComparisonsWithinAbolusteTolerance) {
  EXPECT_TRUE(AreWithinAbsoluteTolerance(-kInfinity, -kInfinity, 1e-6));
  EXPECT_TRUE(AreWithinAbsoluteTolerance(kInfinity, kInfinity, 1e-6));
  EXPECT_FALSE(AreWithinAbsoluteTolerance(-kInfinity, kInfinity, 1e-6));
  EXPECT_FALSE(AreWithinAbsoluteTolerance(kInfinity, -kInfinity, 1e-6));
  EXPECT_FALSE(AreWithinAbsoluteTolerance(1.0, kInfinity, 1e-6));
  EXPECT_FALSE(AreWithinAbsoluteTolerance(-kInfinity, 1.0, 1e-6));
  EXPECT_TRUE(AreWithinAbsoluteTolerance(1.0, 1.0 + 1e-7, 1e-6));
  EXPECT_FALSE(AreWithinAbsoluteTolerance(1.0, 1.0 + 1e-5, 1e-6));
  EXPECT_TRUE(AreWithinAbsoluteTolerance(0.0, 0.0 + 1e-7, 1e-6));
  EXPECT_FALSE(AreWithinAbsoluteTolerance(0.0, 0.0 + 1e-5, 1e-6));
  EXPECT_FALSE(AreWithinAbsoluteTolerance(
      1.0, std::numeric_limits<double>::quiet_NaN(), 1e-6));
}

TEST(IsSmallerWithinToleranceTest, BasicTest) {
  EXPECT_TRUE(IsSmallerWithinTolerance(kInfinity, kInfinity, 1e-6));
  EXPECT_TRUE(IsSmallerWithinTolerance(-kInfinity, -kInfinity, 1e-6));
  EXPECT_TRUE(IsSmallerWithinTolerance(-kInfinity, kInfinity, 1e-6));
  EXPECT_FALSE(IsSmallerWithinTolerance(kInfinity, -kInfinity, 1e-6));

  EXPECT_TRUE(IsSmallerWithinTolerance(123.456, kInfinity, 1e-6));
  EXPECT_FALSE(IsSmallerWithinTolerance(kInfinity, 4567.0, 1e-6));

  const double kNaN = std::numeric_limits<double>::quiet_NaN();
  EXPECT_FALSE(IsSmallerWithinTolerance(kNaN, kNaN, 1e-6));
  EXPECT_FALSE(IsSmallerWithinTolerance(kNaN, kInfinity, 1e-6));
  EXPECT_FALSE(IsSmallerWithinTolerance(kNaN, -kInfinity, 1e-6));
  EXPECT_FALSE(IsSmallerWithinTolerance(kNaN, 0.0, 1e-6));
  EXPECT_FALSE(IsSmallerWithinTolerance(kInfinity, kNaN, 1e-6));
  EXPECT_FALSE(IsSmallerWithinTolerance(-kInfinity, kNaN, 1e-6));
  EXPECT_FALSE(IsSmallerWithinTolerance(0.0, kNaN, 1e-6));

  EXPECT_TRUE(IsSmallerWithinTolerance(-5.0 + 1e-6, -5.0, 1e-6));
  EXPECT_FALSE(IsSmallerWithinTolerance(-5.0 + 1e-6, -5.0, 1e-8));
  EXPECT_TRUE(IsSmallerWithinTolerance(1.0, 1.0, 0.0));
  EXPECT_FALSE(IsSmallerWithinTolerance(
      1.0 + std::numeric_limits<double>::epsilon(), 1.0, 0.0));
}

TEST(IsIntegerWithinToleranceTest, BasicTest) {
  EXPECT_TRUE(IsIntegerWithinTolerance(4.9, .2));
  EXPECT_FALSE(IsIntegerWithinTolerance(4.9, .05));

  EXPECT_TRUE(IsIntegerWithinTolerance(5.1, .2));
  EXPECT_FALSE(IsIntegerWithinTolerance(5.1, .05));
}

TEST(IsIntegerWithinToleranceTest, InfTest) {
  EXPECT_FALSE(IsIntegerWithinTolerance(kInfinity, 100.0));
  EXPECT_FALSE(IsIntegerWithinTolerance(-kInfinity, 100.0));
  EXPECT_FALSE(IsIntegerWithinTolerance(kInfinity, kInfinity));
  EXPECT_FALSE(IsIntegerWithinTolerance(-kInfinity, kInfinity));
}

TEST(GetBestScalingOfDoublesToInt64Test, ErrorCases) {
  const double kInfinity = std::numeric_limits<double>::infinity();
  std::vector<double> input{0, 1, 2, 3};
  double scale;
  double error;

  input[1] = kInfinity;
  GetBestScalingOfDoublesToInt64(input, uint64_t{1} << 60, &scale, &error);
  EXPECT_EQ(error, kInfinity);

  input[1] = -kInfinity;
  GetBestScalingOfDoublesToInt64(input, uint64_t{1} << 60, &scale, &error);
  EXPECT_EQ(error, kInfinity);

  input[1] = std::numeric_limits<double>::quiet_NaN();
  GetBestScalingOfDoublesToInt64(input, uint64_t{1} << 60, &scale, &error);
  EXPECT_EQ(error, kInfinity);

  input[1] = std::numeric_limits<double>::signaling_NaN();
  GetBestScalingOfDoublesToInt64(input, uint64_t{1} << 60, &scale, &error);
  EXPECT_EQ(error, kInfinity);

  // Just to show that it works with 0.
  input[1] = 0;
  GetBestScalingOfDoublesToInt64(input, uint64_t{1} << 60, &scale, &error);
  EXPECT_EQ(error, 0);

  // And note that the max_absolute_sum negative also lead to an error.
  GetBestScalingOfDoublesToInt64(input, -1, &scale, &error);
  EXPECT_EQ(error, kInfinity);
}

TEST(GetBestScalingOfDoublesToInt64Test, AllZeroVector) {
  std::vector<double> input(1000, 0.0);
  double scale;
  double error;
  GetBestScalingOfDoublesToInt64(input, uint64_t{1} << 60, &scale, &error);
  EXPECT_EQ(error, 0.0);
  EXPECT_EQ(scale, 1.0);
}

TEST(GetBestScalingOfDoublesToInt64Test, ZeroBounds) {
  std::vector<double> input(1000, 50.0);
  std::vector<double> lb(1000, 0.0);
  std::vector<double> ub(1000, 0.0);
  const double scale =
      GetBestScalingOfDoublesToInt64(input, lb, ub, uint64_t{1} << 60);
  EXPECT_EQ(scale, 1.0);

  double error;
  double abs_error;
  ComputeScalingErrors(input, lb, ub, scale, &error, &abs_error);
  EXPECT_EQ(error, 0.0);
  EXPECT_EQ(abs_error, 0.0);
}

TEST(GetBestScalingOfDoublesToInt64Test, PowerOfTwo) {
  std::vector<double> input{ldexp(1.0, 100), ldexp(-1.0, 10), ldexp(1.0, -10)};
  double scale;
  double error;
  GetBestScalingOfDoublesToInt64(input, int64_t{1} << 60, &scale, &error);
  EXPECT_EQ(scale, ldexp(1.0, 60 - 100));
  EXPECT_EQ(error, 1.0);  // The last value just disappeared...
}

TEST(GetBestScalingOfDoublesToInt64Test, MaxSum) {
  std::vector<double> input{3, -3, 5, 0, 8};
  double scale;
  double error;

  // The sum of the input absolute values is 19.
  GetBestScalingOfDoublesToInt64(input, 19, &scale, &error);
  EXPECT_EQ(scale, 1.0);
  EXPECT_EQ(error, 0.0);

  // If we use a lower value, then the number needs to be scaled down.
  GetBestScalingOfDoublesToInt64(input, 18, &scale, &error);
  EXPECT_EQ(scale, 0.5);

  // We loose the bit at 1 from the 3s and 5.
  EXPECT_COMPARABLE(error, 1.0 / 3.0, 1e-10);
}

void CheckNoError(absl::Span<const double> input, double scale) {
  for (double x : input) {
    EXPECT_EQ(round(x * scale) / scale, x);
  }
}

TEST(GetBestScalingOfDoublesToInt64Test, NoErrorForIntegers) {
  const int64_t max_mantissa = (1LL << std::numeric_limits<double>::digits) - 1;
  std::vector<double> input{max_mantissa, 1, 123456789, 0, -max_mantissa, -10};
  double scale;
  double error;
  GetBestScalingOfDoublesToInt64(input, int64_t{1} << 60, &scale, &error);
  EXPECT_EQ(scale, 32.0);  // This depends on the limit 1LL << 60.
  EXPECT_EQ(error, 0.0);
  CheckNoError(input, scale);

  // Note that if the exponent of all number are shifted, then this still work.
  for (int i = 0; i < input.size(); ++i) {
    input[i] = ldexp(input[i], -123);
  }
  GetBestScalingOfDoublesToInt64(input, int64_t{1} << 60, &scale, &error);
  EXPECT_EQ(scale, ldexp(32.0, 123));
  EXPECT_EQ(error, 0.0);
  CheckNoError(input, scale);
}

TEST(GetBestScalingOfDoublesToInt64Test, NoErrorForCloseDoubles) {
  std::mt19937 random(12345);
  std::vector<double> input;
  // Because std::numeric_limits<double>::digits is 53, we can represent
  // them exactly with a scale of 2^52, and their sum will still stay below
  // the specified limit (because we just sum 2^10 of them).
  for (int i = 0; i < 1024; ++i) {
    input.push_back(absl::Uniform<double>(random, 1.0, 2.0));
  }
  double scale;
  double error;
  GetBestScalingOfDoublesToInt64(input, kint64max, &scale, &error);
  EXPECT_EQ(error, 0.0);
  EXPECT_EQ(scale, int64_t{1} << 52);
  CheckNoError(input, scale);

  // Now, if we sum some more, then we will need a lower scale and we will loose
  // one bit of precision.
  for (int i = 0; i < 1024; ++i) {
    input.push_back(-absl::Uniform<double>(random, 1.0, 2.0));
  }
  GetBestScalingOfDoublesToInt64(input, kint64max, &scale, &error);
  EXPECT_EQ(error, std::numeric_limits<double>::epsilon());
  EXPECT_EQ(scale, int64_t{1} << 51);
}

TEST(GetBestScalingOfDoublesToInt64Test, BoundOnSumIsCorrect) {
  std::mt19937 random(12345);
  const double x = static_cast<double>((1LL << 32) - 1);
  std::vector<double> input{1.0, x - 1, ldexp(x, 32)};
  double scale;
  double error;
  GetBestScalingOfDoublesToInt64(input, kint64max, &scale, &error);

  // Scaling by 0.5 is not enough, because the sum will be kint64max + 1
  // as shown by the following 3 lines:
  EXPECT_EQ(1, static_cast<int64_t>(round(input[0] * 0.5)));
  EXPECT_EQ((int64_t{1} << 31) - 1,
            static_cast<int64_t>(round(input[1] * 0.5)));
  EXPECT_EQ(((int64_t{1} << 32) - 1) << 31,
            static_cast<int64_t>(round(input[2] * 0.5)));

  EXPECT_GT(error, 0.0);
  EXPECT_EQ(scale, 1.0 / 4.0);

  // If x[0] is zero, then we can scale everyting by 0.5 without error.
  input[0] = 0;
  GetBestScalingOfDoublesToInt64(input, kint64max, &scale, &error);
  EXPECT_EQ(error, 0.0);
  CheckNoError(input, scale);
  EXPECT_EQ(scale, 0.5);
}

TEST(GetBestScalingOfDoublesToInt64Test, Infinity) {
  std::vector<double> input{1e-313};
  double scale;
  double error;
  GetBestScalingOfDoublesToInt64(input, kint64max, &scale, &error);
  EXPECT_LT(scale, std::numeric_limits<double>::infinity());
  EXPECT_EQ(scale, ldexp(1.0, std::numeric_limits<double>::max_exponent - 1));
}

TEST(GetBestScalingOfDoublesToInt64Test, Robustness) {
  std::mt19937 random(12345);
  std::vector<double> input;
  double scale;
  double error;
  for (int i = 0; i < 1024; ++i) {
    input.clear();

    // Generate i "garbage" doubles.
    for (int j = 0; j < i; ++j) {
      input.push_back(absl::bit_cast<double>(absl::Uniform<uint64_t>(random)));
    }

    // Verify that nothing crash and that the value seems reasonable.
    GetBestScalingOfDoublesToInt64(input, kint64max, &scale, &error);
    if (error != std::numeric_limits<double>::infinity()) {
      CHECK_GT(scale, 0);
      CHECK_GE(error, 0);
      CHECK_LE(error, 1);
    }
  }
}

TEST(ComputeScalingErrorTest, BasicTest) {
  std::vector<double> coeffs = {1.1, 0.7, -1.3};
  std::vector<double> lbs = {0, 0, 0};
  std::vector<double> ubs = {10, 10, 10};
  double coeff_relative_error;
  double sum_max_error;
  ComputeScalingErrors(coeffs, lbs, ubs, /*scaling_factor=*/1,
                       &coeff_relative_error, &sum_max_error);

  // The biggest relative error is on the coeff 0.7 that is rounded to 1.
  EXPECT_NEAR(coeff_relative_error, 0.3 / 0.7, 1e-10);

  // The maximum sum difference is obtained on {0, 10, 10} where the exact sum
  // is (0.7 - 1.3) * 10 and the rounded one is (1 - 1) * 10.
  EXPECT_NEAR(sum_max_error, 6.0, 1e-10);
}

TEST(ComputeGcdOfRoundedDoublesTest, BasicTest) {
  EXPECT_EQ(1, ComputeGcdOfRoundedDoubles({}, 1.0));
  EXPECT_EQ(1, ComputeGcdOfRoundedDoubles({0.1, -0.2, 0.3}, 1.0));
  EXPECT_EQ(10, ComputeGcdOfRoundedDoubles({50, 0, 40, 60}, 1.0));
  EXPECT_EQ(1, ComputeGcdOfRoundedDoubles({7, 5, 25, 14}, 1.0));
  EXPECT_EQ(1, ComputeGcdOfRoundedDoubles({-7, -5, 25, -14}, 1.0));
  EXPECT_EQ(7, ComputeGcdOfRoundedDoubles({7, 0}, 1.0));
  EXPECT_EQ(7, ComputeGcdOfRoundedDoubles({0, 7}, 1.0));
  EXPECT_EQ(7, ComputeGcdOfRoundedDoubles({-7, 0}, 1.0));
  EXPECT_EQ(7, ComputeGcdOfRoundedDoubles({0, -7}, 1.0));
  EXPECT_EQ(18, ComputeGcdOfRoundedDoubles({1, 0, 2, 3}, 18.0));
  EXPECT_EQ(18, ComputeGcdOfRoundedDoubles({1, 0, 2, 3}, -18.0));
}

TEST(InterpolateTest, BasicTest) {
  EXPECT_DOUBLE_EQ(1.8, Interpolate<double>(2, 1, .8));
}

constexpr int kDoubleMinExponent = std::numeric_limits<double>::min_exponent;
constexpr int kDoubleMaxExponent = std::numeric_limits<double>::max_exponent;
TEST(FastIlogbTest, Correctness) {
  absl::BitGen bitgen;
  for (int j = 0; j < 1024; ++j) {
    for (int i = kDoubleMinExponent; i < kDoubleMaxExponent; ++i) {
      for (int sign : {-1, 1}) {
        const double input =
            sign * absl::Uniform(absl::IntervalClosedOpen, bitgen, 1.0, 2.0);
        ASSERT_EQ(fast_ilogb(scalbn(input, i)), i);
      }
    }
  }
}

TEST(FastScalbnTest, Correctness) {
  std::mt19937 bitgen(19560618);
  for (int j = 0; j < 1024; ++j) {
    for (int i = kDoubleMinExponent; i < kDoubleMaxExponent; ++i) {
      for (int sign : {-1, 1}) {
        const double input = sign * absl::Uniform(bitgen, 1.0, 2.0);
        ASSERT_EQ(fast_scalbn(input, i), scalbn(input, i));
      }
    }
  }
}

template <class T>
T RandomElementOf(std::mt19937& random, const std::vector<T>& array) {
  return array[absl::Uniform<int>(random, 0, array.size())];
}

// Test on random input, ensure that for finite numbers whose result is
// representable, we get the correct result.
TEST(FastScalbnTest, DontFailOnRubbish) {
  constexpr int kNumTests = (1 << 25);
  std::mt19937 bitgen(19540820);
  int64_t count_ok = 0;
  int64_t count_nan = 0;
  int64_t count_subnormal = 0;
  int64_t count_zero = 0;
  int64_t count_infinity = 0;
  // We inject some special values that are worth testing, but that are too
  // unlikely to be produced by a purely bit-uniform random double.
  const std::vector<double> kSpecialValues = {kInfinity, -kInfinity, 0.0, -0.0};
  for (int i = 0; i < kNumTests; ++i) {
    const double input = (i & 0xff)
                             ? absl::bit_cast<double>(absl::Uniform<uint64_t>(
                                   bitgen, 0, kuint64max))
                             : RandomElementOf(bitgen, kSpecialValues);
    const int add_to_exponent =
        (i % 97 != 0) ? absl::Uniform<int>(bitgen, kDoubleMinExponent - 1,
                                           kDoubleMaxExponent + 2)
                      : absl::Uniform<int>(bitgen, kint32min, kint32max);
    // Note that no matter the input, the function will not crash, although
    // the result may be undefined behavior.
    const double result = fast_scalbn(input, add_to_exponent);
    if (std::isnan(input)) {
      ++count_nan;
    } else if (std::isinf(input)) {
      ++count_infinity;
    } else if (input == 0.0) {
      ++count_zero;
      ASSERT_EQ(result, input);
    } else if (!std::isnormal(input)) {
      ++count_subnormal;
    } else {
      const int input_exponent = fast_ilogb(input);
      const int result_exponent = input_exponent + add_to_exponent;
      if (result_exponent < kDoubleMaxExponent &&
          result_exponent >= kDoubleMinExponent) {
        // If input and output is representable, the result must be equal to
        // the value returned by the standard function.
        const double posix_result = scalbn(input, add_to_exponent);
        ASSERT_EQ(result, posix_result) << absl::StrFormat(
            "\nBit representations:\n\t%10s %016llx %.16g\n\t%10s %016llx "
            "%.16g\n\t%10s %016llx %.16g\nExponents:\n\t%10s %10d\n\t%10s "
            "%10d\n\t%10s %10d\nIterations: %d",
            "input", absl::bit_cast<uint64_t>(input), input, "fast",
            absl::bit_cast<uint64_t>(result), result, "posix",
            absl::bit_cast<uint64_t>(posix_result), posix_result, "input",
            input_exponent, "result", result_exponent, "add_to",
            add_to_exponent, i);
        ++count_ok;
      }
    }
  }
  LOG(INFO) << absl::StrFormat(
      "Input tested:\n\t%20s %.4f%% (count %g)\n\t%20s %.4f%% (count "
      "%g)\n\t%20s %.4f%% (count %g)\n\t%20s %.4f%% (count %g)\n\t%20s %.4f%% "
      "(count %g)",
      "Valid", count_ok * 100.0 / kNumTests, count_ok, "Subnormal",
      count_subnormal * 100.0 / kNumTests, count_subnormal, "NaN",
      count_nan * 100.0 / kNumTests, count_nan, "Infinity",
      count_infinity * 100.0 / kNumTests, count_infinity, "Zero",
      count_zero * 100.0 / kNumTests, count_zero);
  EXPECT_LT(0.55 * kNumTests, count_ok);
  EXPECT_LT(4e-4 * kNumTests, count_subnormal);
  EXPECT_LT(4e-4 * kNumTests, count_nan);
  EXPECT_LT(4e-4 * kNumTests, count_infinity);
  EXPECT_LT(4e-4 * kNumTests, count_zero);
}

}  // namespace
}  // namespace operations_research
