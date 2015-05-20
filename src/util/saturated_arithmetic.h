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

#ifndef OR_TOOLS_UTIL_SATURATED_ARITHMETIC_H_
#define OR_TOOLS_UTIL_SATURATED_ARITHMETIC_H_

#include <cmath>
#include <limits>

#include "base/integral_types.h"
#include "util/bitset.h"

namespace operations_research {
// ---------- Overflow utility functions ----------

// Implement two's complement addition and subtraction on int64s.
//
// The C and C++ standards specify that the overflow of signed integers is
// undefined. This is because of the different possible representations that may
// be used for signed integers (one's complement, two's complement, sign and
// magnitude). Such overflows are detected by Address Sanitizer with
// -fsanitize=signed-integer-overflow.
//
// Simple, portable overflow detection on current machines relies on
// these two functions. For example, if the sign of the sum of two positive
// integers is negative, there has been an overflow.
//
// Note that the static assert will break if the code is compiled on machines
// which do not use two's complement.
inline int64 TwosComplementAddition(int64 x, int64 y) {
  static_assert(static_cast<uint64>(-1LL) == ~0ULL,
                "The target architecture does not use two's complement.");
  return static_cast<int64>(static_cast<uint64>(x) + static_cast<uint64>(y));
}

inline int64 TwosComplementSubtraction(int64 x, int64 y) {
  static_assert(static_cast<uint64>(-1LL) == ~0ULL,
                "The target architecture does not use two's complement.");
  return static_cast<int64>(static_cast<uint64>(x) - static_cast<uint64>(y));
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
  return right > 0 && left > TwosComplementSubtraction(kint64max, right);
}

inline bool AddUnderflows(int64 left, int64 right) {
  return right < 0 && left < TwosComplementSubtraction(kint64min, right);
}

// Performs *b += a and returns false iff the addition overflow or underflow.
// This function only works for typed integer type (IntType<>).
template <typename IntegerType>
bool SafeAddInto(IntegerType a, IntegerType* b) {
  const int64 x = a.value();
  const int64 y = b->value();
  const int64 result = TwosComplementAddition(x, y);
  // Overflow cannot occur if operands have different signs.
  // It can only occur if sign(x) == sign(y) and sign(result) != sign(x),
  // which is equivalent to: sign(x) != sign(result) && sign(y) != sign(result).
  // This is captured when 'negative_if_overflow' below is negative.
  const int64 negative_if_overflow = (x ^ result) & (y ^ result);
  if (negative_if_overflow < 0) {
    return false;
  }
  *b += a;
  return true;
}

// Returns kint64max if x >= 0 and kint64min if x < 0.
inline int64 CapWithSignOf(int64 x) {
  // sign_bit is 1 when x < 0, 0 otherwise.
  const int64 sign_bit = static_cast<uint64>(x) >> 63;
  // return kint64max if x >= 0 or kint64max + 1 (== kint64min) if x < 0.
  return TwosComplementAddition(kint64max, sign_bit);
}

inline int64 CapAddGeneric(int64 x, int64 y) {
  const int64 result = TwosComplementAddition(x, y);
  // Overflow cannot occur if operands have different signs.
  // It can only occur if sign(x) == sign(y) and sign(result) != sign(x),
  // which is equivalent to: sign(x) != sign(result) && sign(y) != sign(result).
  // This is captured when 'negative_if_overflow' below is negative.
  const int64 negative_if_overflow = (x ^ result) & (y ^ result);
  return negative_if_overflow < 0 ? CapWithSignOf(x) : result;
}

#if defined(__GNUC__) && defined(ARCH_K8) && !defined(__APPLE__)
// TODO(user): port this to other architectures.
inline int64 CapAddFast(int64 x, int64 y) {
  const int64 cap = CapWithSignOf(x);
  int64 result;
  asm volatile(  // 'volatile': ask compiler optimizer "keep as is".
      "\t"   "movq %[x],%[result]"
      "\n\t" "addq %[y],%[result]"
      "\n\t" "cmovoq %[cap],%[result]"  // Conditional move if overflow.
      : [result] "=r&"(result)  // Output: don't use an input register ('&').
      : [x] "r"(x), [y] "r"(y), [cap] "r"(cap)  // Input.
      : "cc"  /* Clobbered registers */  );
  return result;
}
#endif

inline int64 CapAdd(int64 x, int64 y) {
#if !defined(MEMORY_SANITIZER) && !defined(ADDRESS_SANITIZER) && \
    defined(__GNUC__) && defined(ARCH_K8) && !defined(__APPLE__)
  return CapAddFast(x, y);
#else
  return CapAddGeneric(x, y);
#endif
}

inline bool SubOverflows(int64 left, int64 right) {
  return right < 0 && left > TwosComplementAddition(kint64max, right);
}

inline bool SubUnderflows(int64 left, int64 right) {
  return right > 0 && left < TwosComplementAddition(kint64min, right);
}

inline int64 CapSubGeneric(int64 x, int64 y) {
  const int64 result = TwosComplementSubtraction(x, y);
  // This is the same reasoning as for CapAdd. We have x = result + y.
  // The formula is the same, with 'x' and result exchanged.
  const int64 negative_if_overflow = (x ^ result) & (x ^ y);
  return negative_if_overflow < 0 ? CapWithSignOf(x) : result;
}

#if defined(__GNUC__) && defined(ARCH_K8) && !defined(__APPLE__)
// TODO(user): port this to other architectures.
inline int64 CapSubFast(int64 x, int64 y) {
  const int64 cap = CapWithSignOf(x);
  int64 result;
  asm volatile(  // 'volatile': ask compiler optimizer "keep as is".
      "\t"   "movq %[x],%[result]"
      "\n\t" "subq %[y],%[result]"
      "\n\t" "cmovoq %[cap],%[result]"  // Conditional move if overflow.
      : [result] "=r&"(result)  // Output: don't use an input register ('&').
      : [x] "r"(x), [y] "r"(y), [cap] "r"(cap)  // Input.
      : "cc"  /* Clobbered registers */  );
  return result;
}
#endif

inline int64 CapSub(int64 x, int64 y) {
#if !defined(MEMORY_SANITIZER) && !defined(ADDRESS_SANITIZER) && \
    defined(__GNUC__) && defined(ARCH_K8) && !defined(__APPLE__)
  return CapSubFast(x, y);
#else
  return CapSubGeneric(x, y);
#endif
}

inline int64 CapOpp(int64 v) {
  // Note: -kint64min != kint64max.
  return v == kint64min ? kint64max : -v;
}

// For an inspiration of the function below, see Henry S. Warren, Hacker's
// Delight second edition, Addison-Wesley Professional, 2012, p.32,
// section 2-13.
// http://www.amazon.com/Hackers-Delight-Edition-Henry-Warren/dp/0321842685
inline int64 CapProdGeneric(int64 x, int64 y) {
  // Early return. This would have to be tested later on to avoid division
  // by zero.
  if (y == 0) return 0;
  const int64 z = x * y;
  // Note: the following test on the most significant bit position brings a 20%
  // improvement on a Nehalem machine using g++. Your mileage may vary.
  if (MostSignificantBitPosition64(std::abs(x)) +
      MostSignificantBitPosition64(std::abs(y)) < 62) return z;
  // Catch x = kint64min separately. When y = -1, z = kint64min, and z / -1
  // produces a Division Exception (on x86-64) as the positive result does not
  // fit in a signed 64-bit integer. (This is the only problematic case.)
  if (x == kint64min) return CapWithSignOf(x ^ y);
  // We know that at this point y != 0 and x != kint64min, so it is safe to
  // divide z by y.
  if (z / y == x) return z;
  return CapWithSignOf(x ^ y);
}

inline int64 CapProdUsingDoubles(int64 a, int64 b) {
  // This is the same algorithm as used in the Python intobject.c file.
  const double double_prod = static_cast<double>(a) * static_cast<double>(b);
  const int64 int_prod = a * b;
  // It's faster to compute this before now as it can be done in parallel with
  // the computations on doubles (product, conversion from int), and the integer
  // product.
  const int64 cap = CapWithSignOf(a ^ b);
  const double int_prod_as_double = static_cast<double>(int_prod);
  if (int_prod_as_double == double_prod) {
    return int_prod;
  }
  const double diff = std::abs(int_prod_as_double - double_prod);
  if (32.0 * diff <= std::abs(double_prod)) {
    return int_prod;
  }
  return cap;
}

#if defined(__GNUC__) && defined(ARCH_K8) && !defined(__APPLE__)
// TODO(user): port this to other architectures.
inline int64 CapProdFast(int64 x, int64 y) {
  // cap = kint64max if x and y have the same sign, cap = kint64min
  // otherwise.
  const int64 cap = CapWithSignOf(x ^ y);
  int64 result;
  // Here, we use the fact that imul of two signed 64-integers returns a 128-bit
  // result -- we care about the lower 64 bits. More importantly, imul also sets
  // the carry flag if 64 bits were not enough.
  // We then use cmovc to return cap if the carry was set.
  // TODO(user): remove the two 'movq' by setting the right constraints on
  // input and output registers.
  asm volatile(  // 'volatile': ask compiler optimizer "keep as is".
      "\t"   "movq %[x],%%rax"
      "\n\t" "imulq %[y]"
      "\n\t" "cmovc %[cap],%%rax"  // Conditional move if carry.
      "\n\t" "movq %%rax,%[result]"
      : [result] "=r&"(result)  // Output: don't use an input register ('&').
      : [x] "r"(x), [y] "r"(y), [cap] "r"(cap)  // Input.
      : "cc", "%rax", "%rdx" /* Clobbered registers */);
  return result;
}
#endif

inline int64 CapProd(int64 x, int64 y) {
#if !defined(MEMORY_SANITIZER) && !defined(ADDRESS_SANITIZER) && \
    defined(__GNUC__) && defined(ARCH_K8) && !defined(__APPLE__)
  return CapProdFast(x, y);
#else
  return CapProdUsingDoubles(x, y);
#endif
}
}  // namespace operations_research

#endif  // OR_TOOLS_UTIL_SATURATED_ARITHMETIC_H_
