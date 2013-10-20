// Copyright 2010-2013 Google
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

// Utility functions on IEEE floating-point numbers.
// Implemented on float, double, and long double.
//
// Also a placeholder for tools controlling and checking FPU rounding modes.
//
// IMPORTANT NOTICE: you need to compile your binary with -frounding-math if
// you want to use rounding modes.

#ifndef OR_TOOLS_UTIL_FP_UTILS_H_
#define OR_TOOLS_UTIL_FP_UTILS_H_

#if !defined(_MSC_VER)
#include <fenv.h>  // NOLINT
#endif
#if !defined(__ANDROID__) && !defined(__APPLE__) && !defined(_MSC_VER)
#include <fpu_control.h>
#endif
#ifdef __SSE__
#include <xmmintrin.h>
#endif

#include <cmath>
#include <limits>

#include "base/logging.h"

#if defined(_MSC_VER)
static inline double isnan(double value) { return _isnan(value); }
static inline double round(double value) { return floor(value + 0.5); }
#elif defined(__APPLE__)
using std::isnan;
#endif

namespace operations_research {

// The following macro does not change "var", but forces gcc to consider it
// being modified. This can be used to avoid wrong over-optimizations by gcc.
// See http://gcc.gnu.org/bugzilla/show_bug.cgi?id=47617 for an explanation.
#ifdef NDEBUG
#define TOUCH(var) asm("":"+X"(var))
#else
#define TOUCH(var)
#endif

#if (defined(__i386__) || defined(__x86_64__)) && defined(__linux__)
inline fpu_control_t GetFPPrecision() {
  fpu_control_t status;
  _FPU_GETCW(status);
  return status & (_FPU_EXTENDED | _FPU_DOUBLE | _FPU_SINGLE);
}

// CPU precision control. Parameters can be:
// _FPU_EXTENDED, _FPU_DOUBLE or _FPU_SINGLE.
inline void SetFPPrecision(fpu_control_t precision) {
  fpu_control_t status;
  _FPU_GETCW(status);
  TOUCH(status);
  status &= ~(_FPU_EXTENDED | _FPU_DOUBLE | _FPU_SINGLE);
  status |= precision;
  _FPU_SETCW(status);
  DCHECK_EQ(precision, GetFPPrecision());
}
#endif  // (defined(__i386__) || defined(__x86_64__)) && defined(__linux__)

#undef TOUCH

template<typename FloatType>
inline bool IsPositiveOrNegativeInfinity(FloatType x) {
  return x == std::numeric_limits<FloatType>::infinity()
      || x == -std::numeric_limits<FloatType>::infinity();
}

// Tests whether x and y are close to one another using absolute and relative
// tolerances.
// Returns true if |x - y| <= a (with a being the absolute_tolerance).
// The above case is useful for values that are close to zero.
// Returns true if |x - y| <= max(|x|, |y|) * r. (with r being the relative
//                                                tolerance.)
// The cases for infinities are treated separately to avoid generating NaNs.
template<typename FloatType> bool AreWithinAbsoluteOrRelativeTolerances(
    FloatType x, FloatType y,
    FloatType relative_tolerance,
    FloatType absolute_tolerance) {
  DCHECK_LE(0.0, relative_tolerance);
  DCHECK_LE(0.0, absolute_tolerance);
  DCHECK_GT(1.0, relative_tolerance);
  if (IsPositiveOrNegativeInfinity(x) || IsPositiveOrNegativeInfinity(y)) {
    return x == y;
  }
  const FloatType difference = fabs(x - y);
  if (difference <= absolute_tolerance) {
    return true;
  }
  const FloatType largest_magnitude = std::max(fabs(x), fabs(y));
  return difference <= largest_magnitude * relative_tolerance;
}

// Tests whether x and y are close to one another using an absolute tolerance.
// Returns true if |x - y| <= a (with a being the absolute_tolerance).
// The cases for infinities are treated separately to avoid generating NaNs.
template<typename FloatType> bool AreWithinAbsoluteTolerance(
    FloatType x, FloatType y,
    FloatType absolute_tolerance) {
  DCHECK_LE(0.0, absolute_tolerance);
  if (IsPositiveOrNegativeInfinity(x) || IsPositiveOrNegativeInfinity(y)) {
    return x == y;
  }
  return fabs(x - y) <= absolute_tolerance;
}

// Handy alternatives to EXPECT_NEAR(), using relative and absolute tolerance
// instead of relative tolerance only, and with a proper support for infinity.
// TODO(user): investigate moving this to util/math/ or some other place.
#define EXPECT_COMPARABLE(expected, obtained, epsilon)                      \
  EXPECT_TRUE(operations_research::AreWithinAbsoluteOrRelativeTolerances(   \
      expected, obtained, epsilon, epsilon))                                \
      << obtained << " != expected value " << expected                      \
      << " within epsilon = " << epsilon;

#define EXPECT_NOTCOMPARABLE(expected, obtained, epsilon)                   \
  EXPECT_FALSE(operations_research::AreWithinAbsoluteOrRelativeTolerances(  \
      expected, obtained, epsilon, epsilon))                                \
      << obtained << " == expected value " << expected                      \
      << " within epsilon = " << epsilon;


}  // namespace operations_research

#endif  // OR_TOOLS_UTIL_FP_UTILS_H_
