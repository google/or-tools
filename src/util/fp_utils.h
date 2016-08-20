// Copyright 2010-2014 Google
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

#if defined(_MSC_VER)
#pragma fenv_access (on) // NOLINT
#else
#include <fenv.h>  // NOLINT
#endif
#if !defined(__ANDROID__) && !defined(__APPLE__) && !defined(_MSC_VER)
#include <fpu_control.h>
#endif

#ifdef __SSE__
#include <xmmintrin.h>
#endif

#include <algorithm>
#include <cmath>
#include <limits>

#include "base/logging.h"

#if defined(_MSC_VER)
static inline double isnan(double value) { return _isnan(value); }
static inline double round(double value) { return floor(value + 0.5); }
#elif defined(__APPLE__) || __GNUC__ >= 5
using std::isnan;
#endif

namespace operations_research {

// ScopedFloatingPointEnv is used to easily enable Floating-point exceptions.
// The initial state is automatically restored when the object is deleted.
//
// Note(user): For some reason, this causes an FPE exception to be triggered for
// unknown reasons when compiled in 32 bits. Because of this, we do not turn
// on FPE exception if ARCH_K8 is not defined.
//
// TODO(user): Make it work on 32 bits.
// TODO(user): Make it work on msvc, currently calls to _controlfp crash.

class ScopedFloatingPointEnv {
 public:
  ScopedFloatingPointEnv()  {
#if defined(_MSC_VER)
    // saved_control_ = _controlfp(0, 0);
#elif defined(ARCH_K8)
    CHECK_EQ(0, fegetenv(&saved_fenv_));
#endif
  }

  ~ScopedFloatingPointEnv() {
#if defined(_MSC_VER)
    // CHECK_EQ(saved_control_, _controlfp(saved_control_, 0xFFFFFFFF));
#elif defined(ARCH_K8)
    CHECK_EQ(0, fesetenv(&saved_fenv_));
#endif
  }

  void EnableExceptions(int excepts) {
#if defined(_MSC_VER)
    // _controlfp(static_cast<unsigned int>(excepts), _MCW_EM);
#elif defined(ARCH_K8)
    CHECK_EQ(0, fegetenv(&fenv_));
    excepts &= FE_ALL_EXCEPT;
#ifdef __APPLE__
    fenv_.__control &= ~excepts;
#else  // Linux
    fenv_.__control_word &= ~excepts;
#endif
    fenv_.__mxcsr &= ~(excepts << 7);
    CHECK_EQ(0, fesetenv(&fenv_));
#endif
  }

 private:
#if defined(_MSC_VER)
  // unsigned int saved_control_;
#elif defined(ARCH_K8)
  fenv_t fenv_;
  mutable fenv_t saved_fenv_;
#endif
};

// The following macro does not change "var", but forces gcc to consider it
// being modified. This can be used to avoid wrong over-optimizations by gcc.
// See http://gcc.gnu.org/bugzilla/show_bug.cgi?id=47617 for an explanation.
#ifdef NDEBUG
#define TOUCH(var) asm volatile("" : "+X"(var))
#else
#define TOUCH(var)
#endif

#if (defined(__i386__) || defined(__x86_64__)) && defined(__linux__) && \
    !defined(__ANDROID__)
inline fpu_control_t GetFPPrecision() {
  fpu_control_t status = 0;  // Initialized to zero to please memory sanitizer.
  _FPU_GETCW(status);
  return status & (_FPU_EXTENDED | _FPU_DOUBLE | _FPU_SINGLE);
}


// CPU precision control. Parameters can be:
// _FPU_EXTENDED, _FPU_DOUBLE or _FPU_SINGLE.
inline void SetFPPrecision(fpu_control_t precision) {
  fpu_control_t status = 0;  // Initialized to zero to please memory sanitizer.
  _FPU_GETCW(status);
  TOUCH(status);
  status &= ~(_FPU_EXTENDED | _FPU_DOUBLE | _FPU_SINGLE);
  status |= precision;
  _FPU_SETCW(status);
  DCHECK_EQ(precision, GetFPPrecision());
}
#endif  // (defined(__i386__) || defined(__x86_64__)) && defined(__linux__) && \
        // !defined(__ANDROID__)

#undef TOUCH

template <typename FloatType>
inline bool IsPositiveOrNegativeInfinity(FloatType x) {
  return x == std::numeric_limits<FloatType>::infinity() ||
         x == -std::numeric_limits<FloatType>::infinity();
}

// Tests whether x and y are close to one another using absolute and relative
// tolerances.
// Returns true if |x - y| <= a (with a being the absolute_tolerance).
// The above case is useful for values that are close to zero.
// Returns true if |x - y| <= std::max(|x|, |y|) * r. (with r being the relative
//                                                tolerance.)
// The cases for infinities are treated separately to avoid generating NaNs.
template <typename FloatType>
bool AreWithinAbsoluteOrRelativeTolerances(FloatType x, FloatType y,
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
template <typename FloatType>
bool AreWithinAbsoluteTolerance(FloatType x, FloatType y,
                                FloatType absolute_tolerance) {
  DCHECK_LE(0.0, absolute_tolerance);
  if (IsPositiveOrNegativeInfinity(x) || IsPositiveOrNegativeInfinity(y)) {
    return x == y;
  }
  return fabs(x - y) <= absolute_tolerance;
}

// Returns true if x is less than y or slighlty greater than y with the given
// absolute or relative tolerance.
template <typename FloatType>
bool IsSmallerWithinTolerance(FloatType x, FloatType y, FloatType tolerance) {
  if (IsPositiveOrNegativeInfinity(y)) return x <= y;
  return x <= y + tolerance * std::max(1.0, std::min(std::abs(x), std::abs(y)));
}

// Returns true if x is within tolerance of any integer.  Always returns
// false for x equal to +/- infinity.
template <typename FloatType>
inline bool IsIntegerWithinTolerance(FloatType x, FloatType tolerance) {
  DCHECK_LE(0.0, tolerance);
  if (IsPositiveOrNegativeInfinity(x)) return false;
  return std::abs(x - std::round(x)) <= tolerance;
}

// Handy alternatives to EXPECT_NEAR(), using relative and absolute tolerance
// instead of relative tolerance only, and with a proper support for infinity.
// TODO(user): investigate moving this to base/ or some other place.
#define EXPECT_COMPARABLE(expected, obtained, epsilon)                    \
  EXPECT_TRUE(operations_research::AreWithinAbsoluteOrRelativeTolerances( \
      expected, obtained, epsilon, epsilon))                              \
      << obtained << " != expected value " << expected                    \
      << " within epsilon = " << epsilon;

#define EXPECT_NOTCOMPARABLE(expected, obtained, epsilon)                  \
  EXPECT_FALSE(operations_research::AreWithinAbsoluteOrRelativeTolerances( \
      expected, obtained, epsilon, epsilon))                               \
      << obtained << " == expected value " << expected                     \
      << " within epsilon = " << epsilon;

// Given an array of doubles, this computes a positive scaling factor such that
// the scaled doubles can then be rounded to integers with little or no loss of
// precision, and so that the L1 norm of these integers is <= max_sum. More
// precisely, the following formulas will hold:
// - For all i, |round(factor * x[i]) / factor  - x[i]| <= error * |x[i]|
// - The sum over i of |round(factor * x[i])| <= max_sum.
//
// The algorithm tries to minimize "error" (which is the relative error). Note
// however than in really broken cases, the error might be infinity and the
// factor zero.
//
// Note on the algorithm:
// - It only uses factors of the form 2^n (i.e. ldexp(1.0, n)) for simplicity.
// - The error will be zero in many practical instances. For example, if x
//   contains only integers with low magnitude; or if x contains doubles whose
//   exponents cover a small range.
// - It chooses the factor as high as possible under the given constraints, as
//   a result the numbers produced may be large. To balance this, we recommend
//   to divide the scaled integers by their gcd() which will result in no loss
//   of precision and will help in many practical cases.
//
// TODO(user): incorporate the gcd computation here? The issue is that I am
// not sure if I just do factor /= gcd that round(x * factor) will be the same.
void GetBestScalingOfDoublesToInt64(const std::vector<double>& x,
                                    int64 max_absolute_sum,
                                    double* scaling_factor,
                                    double* relative_error);

// Returns the Greatest Common Divisor of the numbers
// round(fabs(x[i] * scaling_factor)). The numbers 0 are ignored and if they are
// all zero then the result is 1. Note that round(fabs()) is the same as
// fabs(round()) since the numbers are rounded away from zero.
int64 ComputeGcdOfRoundedDoubles(const std::vector<double>& x,
                                 double scaling_factor);

// Returns alpha * x + (1 - alpha) * y.
template <typename FloatType>
inline FloatType Interpolate(FloatType x, FloatType y, FloatType alpha) {
  return alpha * x + (1 - alpha) * y;
}

}  // namespace operations_research

#endif  // OR_TOOLS_UTIL_FP_UTILS_H_
