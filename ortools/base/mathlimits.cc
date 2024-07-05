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

#include "ortools/base/mathlimits.h"

namespace operations_research {

#define DEF_COMMON_LIMITS(Type)            \
  const bool MathLimits<Type>::kIsSigned;  \
  const bool MathLimits<Type>::kIsInteger; \
  const int MathLimits<Type>::kMin10Exp;   \
  const int MathLimits<Type>::kMax10Exp;

#define DEF_UNSIGNED_INT_LIMITS(Type)    \
  DEF_COMMON_LIMITS(Type)                \
  const Type MathLimits<Type>::kPosMin;  \
  const Type MathLimits<Type>::kPosMax;  \
  const Type MathLimits<Type>::kMin;     \
  const Type MathLimits<Type>::kMax;     \
  const Type MathLimits<Type>::kEpsilon; \
  const Type MathLimits<Type>::kStdError;

#define DEF_SIGNED_INT_LIMITS(Type)     \
  DEF_UNSIGNED_INT_LIMITS(Type)         \
  const Type MathLimits<Type>::kNegMin; \
  const Type MathLimits<Type>::kNegMax;

#define DEF_PRECISION_LIMITS(Type) const int MathLimits<Type>::kPrecisionDigits;

// http://en.wikipedia.org/wiki/Quadruple_precision_floating-point_format#Double-double_arithmetic
// With some compilers (gcc 4.6.x) on some platforms (powerpc64),
// "long double" is implemented as a pair of double: "double double" format.
// This causes a problem with epsilon (eps).
// eps is the smallest positive number such that 1.0 + eps > 1.0
//
// Normal format:  1.0 + e = 1.0...01      // N-1 zeros for N fraction bits
// D-D format:     1.0 + e = 1.000...0001  // epsilon can be very small
//
// In the normal format, 1.0 + e has to fit in one stretch of bits.
// The maximum rounding error is half of eps.
//
// In the double-double format, 1.0 + e splits across two doubles:
// 1.0 in the high double, e in the low double, and they do not have to
// be contiguous.  The maximum rounding error on a value close to 1.0 is
// much larger than eps.
//
// Some code checks for errors by comparing a computed value to a golden
// value +/- some multiple of the maximum rounding error.  The maximum
// rounding error is not available so we use eps as an approximation
// instead.  That fails when long double is in the double-double format.
// Therefore, we define kStdError as a multiple of
// max(DBL_EPSILON * DBL_EPSILON, kEpsilon) rather than a multiple of kEpsilon.

#define DEF_FP_LIMITS(Type, min_val, max_val, eps_val, inf_val)              \
  DEF_COMMON_LIMITS(Type)                                                    \
  const Type MathLimits<Type>::kPosMin = min_val;                            \
  const Type MathLimits<Type>::kPosMax = max_val;                            \
  const Type MathLimits<Type>::kMin = -max_val;                              \
  const Type MathLimits<Type>::kMax = max_val;                               \
  const Type MathLimits<Type>::kNegMin = -min_val;                           \
  const Type MathLimits<Type>::kNegMax = -max_val;                           \
  const Type MathLimits<Type>::kEpsilon = eps_val;                           \
  /* 32 is 5 bits of mantissa error; should be adequate for common errors */ \
  const Type MathLimits<Type>::kStdError =                                   \
      32 * (static_cast<Type>(DBL_EPSILON * DBL_EPSILON) >                   \
                    MathLimits<Type>::kEpsilon                               \
                ? static_cast<Type>(DBL_EPSILON * DBL_EPSILON)               \
                : MathLimits<Type>::kEpsilon);                               \
  DEF_PRECISION_LIMITS(Type)                                                 \
  const Type MathLimits<Type>::kNaN = inf_val - inf_val;                     \
  const Type MathLimits<Type>::kPosInf = inf_val;                            \
  const Type MathLimits<Type>::kNegInf = -inf_val;

// The following are *not* casts!
DEF_SIGNED_INT_LIMITS(signed char)
DEF_SIGNED_INT_LIMITS(short)      // NOLINT(runtime/int)
DEF_SIGNED_INT_LIMITS(int)        // NOLINT(runtime/int)
DEF_SIGNED_INT_LIMITS(long)       // NOLINT(runtime/int)
DEF_SIGNED_INT_LIMITS(long long)  // NOLINT(runtime/int)

DEF_UNSIGNED_INT_LIMITS(unsigned char)
DEF_UNSIGNED_INT_LIMITS(unsigned short)      // NOLINT(runtime/int)
DEF_UNSIGNED_INT_LIMITS(unsigned)            // NOLINT(runtime/int)
DEF_UNSIGNED_INT_LIMITS(unsigned long)       // NOLINT(runtime/int)
DEF_UNSIGNED_INT_LIMITS(unsigned long long)  // NOLINT(runtime/int)

DEF_FP_LIMITS(float, FLT_MIN, FLT_MAX, FLT_EPSILON, HUGE_VALF)
DEF_FP_LIMITS(double, DBL_MIN, DBL_MAX, DBL_EPSILON, HUGE_VAL)
DEF_FP_LIMITS(long double, LDBL_MIN, LDBL_MAX, LDBL_EPSILON, HUGE_VALL)

#undef DEF_COMMON_LIMITS
#undef DEF_SIGNED_INT_LIMITS
#undef DEF_UNSIGNED_INT_LIMITS
#undef DEF_FP_LIMITS
#undef DEF_PRECISION_LIMITS

}  // namespace operations_research
