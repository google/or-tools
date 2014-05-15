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
#ifndef OR_TOOLS_UTIL_SATURATED_ARITHMETIC_H_
#define OR_TOOLS_UTIL_SATURATED_ARITHMETIC_H_

#include <limits>

#include "base/integral_types.h"

namespace operations_research {
// ---------- Overflow utility functions ----------

// Performs *b += a and returns false iff the addition overflow or underflow.
// This function only works for typed integer type (IntType<>).
template <typename IntegerType>
bool SafeAddInto(IntegerType a, IntegerType* b) {
  if (a > 0) {
    if (*b > std::numeric_limits<typename IntegerType::ValueType>::max() - a)
      return false;
  } else {
    if (*b < std::numeric_limits<typename IntegerType::ValueType>::min() - a)
      return false;
  }
  *b += a;
  return true;
}

// A note on overflow treatment.
// kint64min and kint64max are treated as infinity.
// Thus if the computation overflows, the result is always kint64m(ax/in).
//
// Note(user): this is actually wrong: when computing A-B, if A is kint64max
// and B is finite, then A-B won't be kint64max: overflows aren't sticky.
// TODO(user): consider making some operations overflow-sticky, some others
// not, but make an explicit choice throughout.

inline bool AddOverflows(int64 left, int64 right) {
  return right > 0 && left > kint64max - right;
}

inline bool AddUnderflows(int64 left, int64 right) {
  return right < 0 && left < kint64min - right;
}

inline int64 CapAdd(int64 x, int64 y) {
  const int64 result = x + y;
  // Overflow cannot occur if operands have different signs.
  // It can only occur if sign(x) == sign(y) and sign(result) != sign(x),
  // which is equivalent to: sign(x) != sign(result) && sign(y) != sign(result).
  // This is captured when 'negative_if_overflow' below is negative.
  const int64 negative_if_overflow = (x ^ result) & (y ^ result);
  // sign_bit is 1 when x < 0, 0 otherwise.
  const int64 sign_bit = static_cast<uint64>(x) >> 63;
  // cap = kint64max if x >= 0 and cap = kint64max + 1 (== kint64min) if x < 0.
  const int64 cap = sign_bit + kint64max;
  return negative_if_overflow < 0 ? cap : result;
}

inline bool SubOverflows(int64 left, int64 right) {
  return right < 0 && left > kint64max + right;
}

inline bool SubUnderflows(int64 left, int64 right) {
  return right > 0 && left < kint64min + right;
}

inline int64 CapSub(int64 x, int64 y) {
  const int64 result = x - y;
  // This is the same reasoning as for CapAdd. We have x = result + y.
  // The formula is the same, with 'x' and result exchanged.
  const int64 negative_if_overflow = (x ^ result) & (x ^ y);
  const int64 sign_bit = static_cast<uint64>(x) >> 63;
  const int64 cap = sign_bit + kint64max;
  return negative_if_overflow < 0 ? cap : result;
}

inline int64 CapOpp(int64 v) {
  // Note: -kint64min != kint64max.
  return v == kint64min ? kint64max : -v;
}

inline int64 CapProd(int64 left, int64 right) {
#if defined(__GNUC__) && defined(ARCH_K8)
  // sign_bit will be 1 iff left and right have opposite signs.
  const int64 sign_bit = static_cast<uint64>(left ^ right) >> 63;
  // cap is thus kint64max if left and right have the same sign, and kint64min
  // if left and right have opposite signs.
  const int64 cap = sign_bit + kint64max;
  int64 result;
  // Here, we use the fact that imul of two signed 64-integers returns a 128-bit
  // result -- we care about the lower 64 bits. More importantly, imul also sets
  // the carry flag if 64 bits were not enough.
  // We then use cmovc to return cap if the carry was set.
  asm(       "movq %1,%%rax"
      "\n\t" "imulq %2"
      "\n\t" "cmovc %3,%%rax"
      "\n\t" "movq %%rax,%0"
      : "=r"(result)
      : "r"(left), "r"(right), "r"(cap)
      : "%rax", "%rdx" );
  return result;
#else
  if (left == 0 || right == 0) {
    return 0;
  }
  if (left > 0) {   /* left is positive */
    if (right > 0) {/* left and right are positive */
      if (left > (kint64max / right)) {
        return kint64max;
      }
    } else {/* left positive, right non-positive */
      if (right < (kint64min / left)) {
        return kint64min;
      }
    }               /* left positive, right non-positive */
  } else {          /* left is non-positive */
    if (right > 0) {/* left is non-positive, right is positive */
      if (left < (kint64min / right)) {
        return kint64min;
      }
    } else {/* left and right are non-positive */
      if (right < (kint64max / left)) {
        return kint64max;
      }
    } /* end if left and right are non-positive */
  }   /* end if left is non-positive */
  return left * right;
#endif
}

}  // namespace operations_research

#endif  // OR_TOOLS_UTIL_SATURATED_ARITHMETIC_H_
