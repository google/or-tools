// Copyright 2010-2022 Google LLC
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
#include <vector>

#include "absl/base/casts.h"
#include "ortools/base/basictypes.h"
#include "ortools/base/integral_types.h"
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

  static int64_t FastInt64Round(double x) { return Round<int64_t>(x); }
};
}  // namespace operations_research

#endif  // OR_TOOLS_BASE_MATHUTIL_H_
