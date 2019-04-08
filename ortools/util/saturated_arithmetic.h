// Copyright 2010-2018 Google LLC
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

#include "absl/base/casts.h"
#include "ortools/base/integral_types.h"
#include "ortools/util/bitset.h"

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
  return absl::bit_cast<int64>(static_cast<uint64>(x) + static_cast<uint64>(y));
}

inline int64 TwosComplementSubtraction(int64 x, int64 y) {
  static_assert(static_cast<uint64>(-1LL) == ~0ULL,
                "The target architecture does not use two's complement.");
  return absl::bit_cast<int64>(static_cast<uint64>(x) - static_cast<uint64>(y));
}

// Helper function that returns true if an overflow has occured in computing
// sum = x + y. sum is expected to be computed elsewhere.
inline bool AddHadOverflow(int64 x, int64 y, int64 sum) {
  // Overflow cannot occur if operands have different signs.
  // It can only occur if sign(x) == sign(y) and sign(sum) != sign(x),
  // which is equivalent to: sign(x) != sign(sum) && sign(y) != sign(sum).
  // This is captured when the expression below is negative.
  DCHECK_EQ(sum, TwosComplementAddition(x, y));
  return ((x ^ sum) & (y ^ sum)) < 0;
}

inline bool SubHadOverflow(int64 x, int64 y, int64 diff) {
  // This is the same reasoning as for AddHadOverflow. We have x = diff + y.
  // The formula is the same, with 'x' and diff exchanged.
  DCHECK_EQ(diff, TwosComplementSubtraction(x, y));
  return AddHadOverflow(diff, y, x);
}

// A note on overflow treatment.
// kint64min and kint64max are treated as infinity.
// Thus if the computation overflows, the result is always kint64m(ax/in).
//
// Note(user): this is actually wrong: when computing A-B, if A is kint64max
// and B is finite, then A-B won't be kint64max: overflows aren't sticky.
// TODO(user): consider making some operations overflow-sticky, some others
// not, but make an explicit choice throughout.
inline bool AddOverflows(int64 x, int64 y) {
  return AddHadOverflow(x, y, TwosComplementAddition(x, y));
}

inline int64 SubOverflows(int64 x, int64 y) {
  return SubHadOverflow(x, y, TwosComplementSubtraction(x, y));
}

// Performs *b += a and returns false iff the addition overflow or underflow.
// This function only works for typed integer type (IntType<>).
template <typename IntegerType>
bool SafeAddInto(IntegerType a, IntegerType* b) {
  const int64 x = a.value();
  const int64 y = b->value();
  const int64 sum = TwosComplementAddition(x, y);
  if (AddHadOverflow(x, y, sum)) return false;
  *b = sum;
  return true;
}

// Returns kint64max if x >= 0 and kint64min if x < 0.
inline int64 CapWithSignOf(int64 x) {
  // return kint64max if x >= 0 or kint64max + 1 (== kint64min) if x < 0.
  return TwosComplementAddition(kint64max, static_cast<int64>(x < 0));
}

inline int64 CapAddGeneric(int64 x, int64 y) {
  const int64 result = TwosComplementAddition(x, y);
  return AddHadOverflow(x, y, result) ? CapWithSignOf(x) : result;
}

#if defined(__GNUC__) && defined(ARCH_K8)
// TODO(user): port this to other architectures.
inline int64 CapAddFast(int64 x, int64 y) {
  const int64 cap = CapWithSignOf(x);
  int64 result = x;
  // clang-format off
  asm volatile(  // 'volatile': ask compiler optimizer "keep as is".
      "\t" "addq %[y],%[result]"
      "\n\t" "cmovoq %[cap],%[result]"  // Conditional move if overflow.
      : [result] "=r"(result)  // Output
      : "[result]" (result), [y] "r"(y), [cap] "r"(cap)  // Input.
      : "cc"  /* Clobbered registers */  );
  // clang-format on
  return result;
}
#endif

inline int64 CapAdd(int64 x, int64 y) {
#if defined(__GNUC__) && defined(ARCH_K8)
  return CapAddFast(x, y);
#else
  return CapAddGeneric(x, y);
#endif
}

inline int64 CapSubGeneric(int64 x, int64 y) {
  const int64 result = TwosComplementSubtraction(x, y);
  return SubHadOverflow(x, y, result) ? CapWithSignOf(x) : result;
}

#if defined(__GNUC__) && defined(ARCH_K8)
// TODO(user): port this to other architectures.
inline int64 CapSubFast(int64 x, int64 y) {
  const int64 cap = CapWithSignOf(x);
  int64 result = x;
  // clang-format off
  asm volatile(  // 'volatile': ask compiler optimizer "keep as is".
      "\t" "subq %[y],%[result]"
      "\n\t" "cmovoq %[cap],%[result]"  // Conditional move if overflow.
      : [result] "=r"(result)  // Output
      : "[result]" (result), [y] "r"(y), [cap] "r"(cap)  // Input.
      : "cc"  /* Clobbered registers */  );
  // clang-format on
  return result;
}
#endif

inline int64 CapSub(int64 x, int64 y) {
#if defined(__GNUC__) && defined(ARCH_K8)
  return CapSubFast(x, y);
#else
  return CapSubGeneric(x, y);
#endif
}

// Note(user): -kint64min != kint64max, but kint64max == ~kint64min.
inline int64 CapOpp(int64 v) { return v == kint64min ? ~v : -v; }

namespace cap_prod_util {
// Returns an unsigned int equal to the absolute value of n, in a way that
// will not produce overflows.
inline uint64 uint_abs(int64 n) {
  return n < 0 ? ~static_cast<uint64>(n) + 1 : static_cast<uint64>(n);
}
}  // namespace cap_prod_util

// The generic algorithm computes a bound on the number of bits necessary to
// store the result. For this it uses the position of the most significant bits
// of each of the arguments.
// If the result needs at least 64 bits, then return a capped value.
// If the result needs at most 63 bits, then return the product.
// Otherwise, the result may use 63 or 64 bits: compute the product
// as a uint64, and cap it if necessary.
inline int64 CapProdGeneric(int64 x, int64 y) {
  const uint64 a = cap_prod_util::uint_abs(x);
  const uint64 b = cap_prod_util::uint_abs(y);
  // Let MSB(x) denote the most significant bit of x. We have:
  // MSB(x) + MSB(y) <= MSB(x * y) <= MSB(x) + MSB(y) + 1
  const int msb_sum =
      MostSignificantBitPosition64(a) + MostSignificantBitPosition64(b);
  const int kMaxBitIndexInInt64 = 63;
  if (msb_sum <= kMaxBitIndexInInt64 - 2) return x * y;
  // Catch a == 0 or b == 0 now, as MostSignificantBitPosition64(0) == 0.
  // TODO(user): avoid this by writing function Log2(a) with Log2(0) == -1.
  if (a == 0 || b == 0) return 0;
  const int64 cap = CapWithSignOf(x ^ y);
  if (msb_sum >= kMaxBitIndexInInt64) return cap;
  // The corner case is when msb_sum == 62, i.e. at least 63 bits will be
  // needed to store the product. The following product will never overflow
  // on uint64, since msb_sum == 62.
  const uint64 u_prod = a * b;
  // The overflow cases are captured by one of the following conditions:
  // (cap >= 0 && u_prod >= static_cast<uint64>(kint64max) or
  // (cap < 0 && u_prod >= static_cast<uint64>(kint64min)).
  // These can be optimized as follows (and if the condition is false, it is
  // safe to compute x * y.
  if (u_prod >= static_cast<uint64>(cap)) return cap;
  const int64 abs_result = absl::bit_cast<int64>(u_prod);
  return cap < 0 ? -abs_result : abs_result;
}

#if defined(__GNUC__) && defined(ARCH_K8)
// TODO(user): port this to other architectures.
inline int64 CapProdFast(int64 x, int64 y) {
  // cap = kint64max if x and y have the same sign, cap = kint64min
  // otherwise.
  const int64 cap = CapWithSignOf(x ^ y);
  int64 result = x;
  // Here, we use the fact that imul of two signed 64-integers returns a 128-bit
  // result -- we care about the lower 64 bits. More importantly, imul also sets
  // the carry flag if 64 bits were not enough.
  // We therefore use cmovc to return cap if the carry was set.
  // clang-format off
  asm volatile(  // 'volatile': ask compiler optimizer "keep as is".
      "\n\t" "imulq %[y],%[result]"
      "\n\t" "cmovcq %[cap],%[result]"  // Conditional move if carry.
      : [result] "=r"(result)  // Output
      : "[result]" (result), [y] "r"(y), [cap] "r"(cap)  // Input.
      : "cc" /* Clobbered registers */);
  // clang-format on
  return result;
}
#endif

inline int64 CapProd(int64 x, int64 y) {
#if defined(__GNUC__) && defined(ARCH_K8)
  return CapProdFast(x, y);
#else
  return CapProdGeneric(x, y);
#endif
}
}  // namespace operations_research

#endif  // OR_TOOLS_UTIL_SATURATED_ARITHMETIC_H_
