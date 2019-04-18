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

#ifndef OR_TOOLS_BASE_INT128_H_
#define OR_TOOLS_BASE_INT128_H_

#include "absl/numeric/int128.h"

namespace absl {

constexpr int64 BitCastToSigned(uint64 v) {
  // Casting an unsigned integer to a signed integer of the same
  // width is implementation defined behavior if the source value would not fit
  // in the destination type. We step around it with a roundtrip bitwise not
  // operation to make sure this function remains constexpr. Clang, GCC, and
  // MSVC optimize this to a no-op on x86-64.
  return v & (uint64{1} << 63) ? ~static_cast<int64>(~v)
                               : static_cast<int64>(v);
}

class int128 {
 public:
  int128() = default;

  constexpr int128(int64 v)  // NOLINT(runtime/explicit)
      : lo_{static_cast<uint64>(v)}, hi_{v < 0 ? ~int64{0} : 0} {}
  int128& operator=(int64 v) { return *this = int128(v); }
  constexpr explicit operator int64() const { return BitCastToSigned(lo_); }

  friend int128 operator+(int128 lhs, int128 rhs);

  int128& operator+=(int128 other) {
    *this = *this + other;
    return *this;
  }

  friend constexpr int128 MakeInt128(int64 high, uint64 low);
  friend constexpr uint64 Int128Low64(int128 v);
  friend constexpr int64 Int128High64(int128 v);

 private:
  constexpr int128(int64 high, uint64 low) : hi_(high), lo_(low) {}
#if defined(ABSL_IS_LITTLE_ENDIAN)
  uint64 lo_;
  int64 hi_;
#elif defined(ABSL_IS_BIG_ENDIAN)
  int64 hi_;
  uint64 lo_;
#else  // byte order
#error "Unsupported byte order: must be little-endian or big-endian."
#endif  // byte order
};

inline uint128 MakeUint128(int128 v) {
  return MakeUint128(static_cast<uint64>(Int128High64(v)), Int128Low64(v));
}

constexpr int128 MakeInt128(int64 high, uint64 low) {
  return int128(high, low);
}
constexpr uint64 Int128Low64(int128 v) { return v.lo_; }
constexpr int64 Int128High64(int128 v) { return v.hi_; }

inline int128 operator+(int128 lhs, int128 rhs) {
  int128 result = MakeInt128(Int128High64(lhs) + Int128High64(rhs),
                             Int128Low64(lhs) + Int128Low64(rhs));
  if (Int128Low64(result) < Int128Low64(lhs)) {  // check for carry
    return MakeInt128(Int128High64(result) + 1, Int128Low64(result));
  }
  return result;
}

inline int128 operator-(int128 lhs, int128 rhs) {
  int128 result = MakeInt128(Int128High64(lhs) - Int128High64(rhs),
                             Int128Low64(lhs) - Int128Low64(rhs));
  if (Int128Low64(lhs) < Int128Low64(rhs)) {  // check for carry
    return MakeInt128(Int128High64(result) - 1, Int128Low64(result));
  }
  return result;
}

inline int128 operator*(int128 lhs, int128 rhs) {
  uint128 result = MakeUint128(lhs) * MakeUint128(rhs);
  return MakeInt128(BitCastToSigned(Uint128High64(result)),
                    Uint128Low64(result));
}

inline uint128 UnsignedAbsoluteValue(int128 v) {
  // Cast to uint128 before possibly negating because -Int128Min() is undefined.
  return Int128High64(v) < 0 ? -MakeUint128(v) : MakeUint128(v);
}

#define STEP(T, n, pos, sh)                   \
  do {                                        \
    if ((n) >= (static_cast<T>(1) << (sh))) { \
      (n) = (n) >> (sh);                      \
      (pos) |= (sh);                          \
    }                                         \
  } while (0)
static inline int Fls64(uint64 n) {
  assert(n != 0);
  int pos = 0;
  STEP(uint64, n, pos, 0x20);
  uint32_t n32 = static_cast<uint32_t>(n);
  STEP(uint32_t, n32, pos, 0x10);
  STEP(uint32_t, n32, pos, 0x08);
  STEP(uint32_t, n32, pos, 0x04);
  return pos + ((uint64{0x3333333322221100} >> (n32 << 2)) & 0x3);
}
#undef STEP

static inline int Fls128(uint128 n) {
  if (uint64 hi = Uint128High64(n)) {
    return Fls64(hi) + 64;
  }
  return Fls64(Uint128Low64(n));
}

// Long division/modulo for uint128 implemented using the shift-subtract
// division algorithm adapted from:
// https://stackoverflow.com/questions/5386377/division-without-using
inline void DivModImpl(uint128 dividend, uint128 divisor, uint128* quotient_ret,
                       uint128* remainder_ret) {
  assert(divisor != 0);

  if (divisor > dividend) {
    *quotient_ret = 0;
    *remainder_ret = dividend;
    return;
  }

  if (divisor == dividend) {
    *quotient_ret = 1;
    *remainder_ret = 0;
    return;
  }

  uint128 denominator = divisor;
  uint128 quotient = 0;

  // Left aligns the MSB of the denominator and the dividend.
  const int shift = Fls128(dividend) - Fls128(denominator);
  denominator <<= shift;

  // Uses shift-subtract algorithm to divide dividend by denominator. The
  // remainder will be left in dividend.
  for (int i = 0; i <= shift; ++i) {
    quotient <<= 1;
    if (dividend >= denominator) {
      dividend -= denominator;
      quotient |= 1;
    }
    denominator >>= 1;
  }

  *quotient_ret = quotient;
  *remainder_ret = dividend;
}

inline int128 operator/(int128 lhs, int128 rhs) {
  // assert(lhs != Int128Min() || rhs != -1);  // UB on two's complement.

  uint128 quotient = 0;
  uint128 remainder = 0;
  DivModImpl(UnsignedAbsoluteValue(lhs), UnsignedAbsoluteValue(rhs), &quotient,
             &remainder);
  if ((Int128High64(lhs) < 0) != (Int128High64(rhs) < 0)) quotient = -quotient;
  return MakeInt128(BitCastToSigned(Uint128High64(quotient)),
                    ::absl::Uint128Low64(quotient));
}

inline bool operator>(int128 lhs, int128 rhs) {
  return (Int128High64(lhs) == Int128High64(rhs))
             ? (Int128Low64(lhs) > Int128Low64(rhs))
             : (Int128High64(lhs) > Int128High64(rhs));
}

}  // namespace absl

#endif  // OR_TOOLS_BASE_INT128_H_
