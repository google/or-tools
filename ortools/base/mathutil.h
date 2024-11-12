// Copyright 2010-2024 Google LLC
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

#ifndef OR_TOOLS_BASE_MATHUTIL_H_
#define OR_TOOLS_BASE_MATHUTIL_H_

#include <math.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

#include "absl/base/casts.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"

namespace operations_research {
class MathUtil {
 public:
  // CeilOfRatio<IntegralType>
  // FloorOfRatio<IntegralType>
  //   Returns the ceil (resp. floor) of the ratio of two integers.
  //
  //   IntegralType: any integral type, whether signed or not.
  //   numerator: any integer: positive, negative, or zero.
  //   denominator: a non-zero integer, positive or negative.
  template <typename IntegralType>
  static IntegralType CeilOfRatio(IntegralType numerator,
                                  IntegralType denominator) {
    DCHECK_NE(0, denominator);
    const IntegralType rounded_toward_zero = numerator / denominator;
    const IntegralType intermediate_product = rounded_toward_zero * denominator;
    const bool needs_adjustment =
        (rounded_toward_zero >= 0) &&
        ((denominator > 0 && numerator > intermediate_product) ||
         (denominator < 0 && numerator < intermediate_product));
    const IntegralType adjustment = static_cast<IntegralType>(needs_adjustment);
    const IntegralType ceil_of_ratio = rounded_toward_zero + adjustment;
    return ceil_of_ratio;
  }
  template <typename IntegralType>
  static IntegralType FloorOfRatio(IntegralType numerator,
                                   IntegralType denominator) {
    DCHECK_NE(0, denominator);
    const IntegralType rounded_toward_zero = numerator / denominator;
    const IntegralType intermediate_product = rounded_toward_zero * denominator;
    const bool needs_adjustment =
        (rounded_toward_zero <= 0) &&
        ((denominator > 0 && numerator < intermediate_product) ||
         (denominator < 0 && numerator > intermediate_product));
    const IntegralType adjustment = static_cast<IntegralType>(needs_adjustment);
    const IntegralType floor_of_ratio = rounded_toward_zero - adjustment;
    return floor_of_ratio;
  }

  // Returns the greatest common divisor of two unsigned integers x and y.
  static unsigned int GCD(unsigned int x, unsigned int y) {
    while (y != 0) {
      unsigned int r = x % y;
      x = y;
      y = r;
    }
    return x;
  }

  // Returns the least common multiple of two unsigned integers.  Returns zero
  // if either is zero.
  static unsigned int LeastCommonMultiple(unsigned int a, unsigned int b) {
    if (a > b) {
      return (a / MathUtil::GCD(a, b)) * b;
    } else if (a < b) {
      return (b / MathUtil::GCD(b, a)) * a;
    } else {
      return a;
    }
  }

  // Absolute value of x.
  // Works correctly for unsigned types and
  // for special floating point values.
  // Note: 0.0 and -0.0 are not differentiated by Abs (Abs(0.0) is -0.0),
  // which should be OK: see the comment for Max above.
  template <typename T>
  static T Abs(const T x) {
    return x > 0 ? x : -x;
  }

  // Returns the square of x.
  template <typename T>
  static T Square(const T x) {
    return x * x;
  }

  // Euclid's Algorithm.
  // Returns: the greatest common divisor of two unsigned integers x and y.
  static int64_t GCD64(int64_t x, int64_t y) {
    DCHECK_GE(x, 0);
    DCHECK_GE(y, 0);
    while (y != 0) {
      int64_t r = x % y;
      x = y;
      y = r;
    }
    return x;
  }

  template <typename T>
  static T IPow(T base, int exp) {
    return pow(base, exp);
  }

  template <class IntOut, class FloatIn>
  static IntOut Round(FloatIn x) {
    // We don't use sgn(x) below because there is no need to distinguish the
    // (x == 0) case.  Also note that there are specialized faster versions
    // of this function for Intel, ARM and PPC processors at the bottom
    // of this file.
    if (x > -0.5 && x < 0.5) {
      // This case is special, because for largest floating point number
      // below 0.5, the addition of 0.5 yields 1 and this would lead
      // to incorrect result.
      return static_cast<IntOut>(0);
    }
    return static_cast<IntOut>(x < 0 ? (x - 0.5) : (x + 0.5));
  }

  // Returns the minimum integer value which is a multiple of rounding_value,
  // and greater than or equal to input_value.
  // The input_value must be greater than or equal to zero, and the
  // rounding_value must be greater than zero.
  template <typename IntType>
  static IntType RoundUpTo(IntType input_value, IntType rounding_value) {
    static_assert(std::numeric_limits<IntType>::is_integer,
                  "RoundUpTo() operation type is not integer");
    DCHECK_GE(input_value, 0);
    DCHECK_GT(rounding_value, 0);
    const IntType remainder = input_value % rounding_value;
    return (remainder == 0) ? input_value
                            : (input_value - remainder + rounding_value);
  }

  // Convert a floating-point number to an integer. For all inputs x where
  // static_cast<IntOut>(x) is legal according to the C++ standard, the result
  // is identical to that cast (i.e. the result is x with its fractional part
  // truncated whenever that is representable as IntOut).
  //
  // static_cast would cause undefined behavior for the following cases, which
  // have well-defined behavior for this function:
  //
  //  1. If x is NaN, the result is zero.
  //
  //  2. If the truncated form of x is above the representable range of IntOut,
  //     the result is std::numeric_limits<IntOut>::max().
  //
  //  3. If the truncated form of x is below the representable range of IntOut,
  //     the result is std::numeric_limits<IntOut>::lowest().
  //
  // Note that cases #2 and #3 cover infinities as well as finite numbers.
  //
  // The range of FloatIn must include the range of IntOut, otherwise
  // the results are undefined.
  template <class IntOut, class FloatIn>
  static IntOut SafeCast(FloatIn x) {
    COMPILE_ASSERT(!std::numeric_limits<FloatIn>::is_integer,
                   FloatIn_is_integer);
    COMPILE_ASSERT(std::numeric_limits<IntOut>::is_integer,
                   IntOut_is_not_integer);
    COMPILE_ASSERT(std::numeric_limits<IntOut>::radix == 2, IntOut_is_base_2);

    // Special case NaN, for which the logic below doesn't work.
    if (std::isnan(x)) {
      return 0;
    }

    // Negative values all clip to zero for unsigned results.
    if (!std::numeric_limits<IntOut>::is_signed && x < 0) {
      return 0;
    }

    // Handle infinities.
    if (std::isinf(x)) {
      return x < 0 ? std::numeric_limits<IntOut>::lowest()
                   : std::numeric_limits<IntOut>::max();
    }

    // Set exp such that x == f * 2^exp for some f with |f| in [0.5, 1.0),
    // unless x is zero in which case exp == 0. Note that this implies that the
    // magnitude of x is strictly less than 2^exp.
    int exp = 0;
    std::frexp(x, &exp);

    // Let N be the number of non-sign bits in the representation of IntOut. If
    // the magnitude of x is strictly less than 2^N, the truncated version of x
    // is representable as IntOut. The only representable integer for which this
    // is not the case is kMin for signed types (i.e. -2^N), but that is covered
    // by the fall-through below.
    if (exp <= std::numeric_limits<IntOut>::digits) {
      return x;
    }

    // Handle numbers with magnitude >= 2^N.
    return x < 0 ? std::numeric_limits<IntOut>::lowest()
                 : std::numeric_limits<IntOut>::max();
  }

  // --------------------------------------------------------------------
  // SafeRound
  //   These functions round a floating-point number to an integer.
  //   Results are identical to Round, except in cases where
  //   the argument is NaN, or when the rounded value would overflow the
  //   return type. In those cases, Round has undefined
  //   behavior. SafeRound returns 0 when the argument is
  //   NaN, and returns the closest possible integer value otherwise (i.e.
  //   std::numeric_limits<IntOut>::max() for large positive values, and
  //   std::numeric_limits<IntOut>::lowest() for large negative values).
  //   The range of FloatIn must include the range of IntOut, otherwise
  //   the results are undefined.
  // --------------------------------------------------------------------
  template <class IntOut, class FloatIn>
  static IntOut SafeRound(FloatIn x) {
    COMPILE_ASSERT(!std::numeric_limits<FloatIn>::is_integer,
                   FloatIn_is_integer);
    COMPILE_ASSERT(std::numeric_limits<IntOut>::is_integer,
                   IntOut_is_not_integer);

    if (std::isnan(x)) {
      return 0;
    } else {
      return SafeCast<IntOut>((x < 0.) ? (x - 0.5) : (x + 0.5));
    }
  }

  // --------------------------------------------------------------------
  // FastInt64Round
  //   Fast routines for converting floating-point numbers to integers.
  //
  //   These routines are approximately 6 times faster than the default
  //   implementation of Round<int> on Intel processors (12 times faster on
  //   the Pentium 3).  They are also more than 5 times faster than simply
  //   casting a "double" to an "int" using static_cast<int>.  This is
  //   because casts are defined to truncate towards zero, which on Intel
  //   processors requires changing the rounding mode and flushing the
  //   floating-point pipeline (unless programs are compiled specifically
  //   for the Pentium 4, which has a new instruction to avoid this).
  //
  //   Numbers that are halfway between two integers may be rounded up or
  //   down.  This is because the conversion is done using the default
  //   rounding mode, which rounds towards the closest even number in case
  //   of ties.  So for example, FastIntRound(0.5) == 0, but
  //   FastIntRound(1.5) == 2.  These functions should only be used with
  //   applications that don't care about which way such half-integers are
  //   rounded.
  //
  //   There are template specializations of Round() which call these
  //   functions (for "int" and "int64" only), but it's safer to call them
  //   directly.
  static int64_t FastInt64Round(double x) { return Round<int64_t>(x); }

  // Returns Stirling's Approximation for log(n!) which has an error
  // of at worst 1/(1260*n^5).
  static double Stirling(double n);

  // Returns the log of the binomial coefficient C(n, k), known in the
  // vernacular as "N choose K".  Why log?  Because the integer number
  // for non-trivial N and K would overflow.
  // Note that if k > 15, this uses Stirling's approximation of log(n!).
  // The relative error is about 1/(1260*k^5) (which is 7.6e-10 when k=16).
  static double LogCombinations(int n, int k);
};
}  // namespace operations_research

#endif  // OR_TOOLS_BASE_MATHUTIL_H_
