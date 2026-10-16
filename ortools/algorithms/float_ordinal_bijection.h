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

#ifndef ORTOOLS_ALGORITHMS_FLOAT_ORDINAL_BIJECTION_H_
#define ORTOOLS_ALGORITHMS_FLOAT_ORDINAL_BIJECTION_H_

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <type_traits>

#include "absl/base/casts.h"
#include "absl/log/check.h"

namespace operations_research {

// Transformation Functions. These are bijections from a floating point number
// to an unsigned integer number (its ordinal in the representation ordering) of
// the same size. They preserve the order of the numbers. This makes it possible
// to sort floating point numbers using integer-based sorting algorithms like
// radix sort, while keeping the stability of the sort.
//
// Although it was rediscovered here, the idea is not new, but still quite
// recent considering the history of sorting algorithms. The following
// references, mostly from the graphics and game development world, describe the
// transformation in detail.

// Pierre Terdiman, 2000, "Radix Sort Revisited",
// https://codercorner.com/RadixSortRevisited.htm
// Michael Herf, 2001, "Radix Tricks", https://stereopsis.com/radix.html
// Pierre Terdiman, 2018, "Radix Redux",
// https://github.com/Pierre-Terdiman/RadixRedux/blob/master/RadixRedux.pdf

// Mapping from float to sortable uint (ordinal).
// 1. IEEE 754 floats are sign-magnitude. For positive values, the raw bits
// already sort correctly. For negative values, larger magnitudes have
// larger raw integer values, which is the inverse of the desired order.
//
// 2. This places all negative numbers below all positive numbers and
//    ensures `-0.0` and `0.0` are adjacent (`0x7F...FF` and `0x80...00`).
//
// 3. If the float is negative (MSB is 1):
// - We flip all the bits (one's complement).
// - This flips the magnitude bits (reversing the order) AND flips the
//   sign bit to 0.
// - Result: Negative NaNs are at the bottom of the set of ordinals:
//   `[0x00000000, 0x007FFFFE]` for `float` (with `-inf` at `0x007FFFFF`) and
//   `[0x0000000000000000, 0x000FFFFFFFFFFFFE]` for `double` (with `-inf` at
//   `0x000FFFFFFFFFFFFF`). Large negative magnitudes become small integers,
//   and `-0.0` becomes the largest value in the lower half (`0x7FF...FF`).
//
// 4. If the float is positive (MSB is 0):
// - We only flip the MSB to 1 (i.e. add `0x80...00` to shift the range).
// - Result: `0.0` becomes `0x80...00`, positive values fill the range up to
//   `+inf` (`0xFF800000` for `float`, `0xFFF0000000000000` for `double`), and
//   positive NaNs are at the top of the set of ordinals:
//   `[0xFF800001, 0xFFFFFFFF]` for `float` and
//   `[0xFFF0000000000001, 0xFFFFFFFFFFFFFFFF]` for `double`.

// Returns the unsigned integer type corresponding to the given
// floating-point type, with the same number of bits.
template <typename Float>
using Ordinal = std::conditional_t<
    std::is_same_v<Float, float>, uint32_t,
    std::conditional_t<std::is_same_v<Float, double>, uint64_t, void>>;

// True if the given type is a supported floating-point type for ordinal
// bijection.
// TODO(user): Support other floating-point types, like `float16`, `bfloat16`.
template <typename Float>
constexpr bool kOrdinalBijectionCompatible =
    std::numeric_limits<Float>::is_iec559 &&
    std::numeric_limits<Float>::radix == 2 &&
    sizeof(Float) == sizeof(Ordinal<Float>) &&
    (std::is_same_v<Float, float> || std::is_same_v<Float, double>);

template <typename Ordinal>
constexpr Ordinal kSignBitMask =
    Ordinal{1} << (std::numeric_limits<Ordinal>::digits - 1);

template <typename Float>
  requires kOrdinalBijectionCompatible<Float>
Ordinal<Float> FloatToOrdinal(const Float f) {
  using Ordinal = Ordinal<Float>;
  const Ordinal u = absl::bit_cast<Ordinal>(f);
  const bool sign_bit_set = u & kSignBitMask<Ordinal>;
  // Take one's complement if float is negative, and just flip the sign bit if
  // positive.
  return sign_bit_set ? ~u : u ^ kSignBitMask<Ordinal>;
}

template <typename Ordinal>
using Float = std::conditional_t<
    std::is_same_v<Ordinal, uint32_t>, float,
    std::conditional_t<std::is_same_v<Ordinal, uint64_t>, double, void>>;

// Inverse mapping from ordinal to float.
// 1. In the sortable space, we check the MSB.
// 2. If MSB is 1:
// - The value was originally positive: only flip the MSB back to 0.
// 3. If MSB is 0:
// - The value was originally negative: flip all bits (one's complement).
// Note that the function is very similar to `FloatToOrdinal`.
template <typename Ordinal>
  requires kOrdinalBijectionCompatible<Float<Ordinal>>
Float<Ordinal> OrdinalToFloat(const Ordinal u) {
  using Float = Float<Ordinal>;
  const bool sign_bit_set = u & kSignBitMask<Ordinal>;
  // Take one's complement if negative, and just flip the sign bit if positive.
  return absl::bit_cast<Float>(sign_bit_set ? u ^ kSignBitMask<Ordinal> : ~u);
}  // namespace operations_research

// This function is unrelated to sorting. It shows that the ordinal space can be
// used for bisection with a significant speedup compared to the original
// floating-point value space.
//
// Returns the floating-point value whose sortable unsigned integer ordinal is
// the midpoint (via `std::midpoint`) between `FloatToOrdinal(a)` and
// `FloatToOrdinal(b)`. Bisecting an interval `[a, b]` with `OrdinalMidpoint`
// halves the number of representable IEEE 754 values at each step, converging
// in at most 32 steps for `float` and 64 steps for `double`.
//
// By contrast using `std::midpoint` directly on `a` and `b` can take many more
// steps, up to 2^E - 1 + M steps, where E is the number of bits in the exponent
// and M is the number of bits in the mantissa. This translates to 255+24=279
// steps for `float` and 2047 + 52 = 2099 steps for `double`.
//
// Using this function can therefore bring up to 8x speedup for bisecting
// operations on floats, and up to 32x speedup for doubles.
//
// If used carefully using `OrdinalBisectMonotonic`, this can result in a
// significantly more important speedup for bisection-based root-finding
// algorithms. See the accompanying benchmark for details.
template <typename Float>
  requires std::is_same_v<Float, float> || std::is_same_v<Float, double>
Float OrdinalMidpoint(const Float a, const Float b) {
  // In theory, `a` and `b` could be NaN. But we require non-NaN values for
  // this type of computation.
  DCHECK(!std::isnan(a));
  DCHECK(!std::isnan(b));
  const Float result =
      OrdinalToFloat(std::midpoint(FloatToOrdinal(a), FloatToOrdinal(b)));
  DCHECK_GE(result, std::min(a, b));
  DCHECK_LE(result, std::max(a, b));
  return result;
}

}  // namespace operations_research

#endif  // ORTOOLS_ALGORITHMS_FLOAT_ORDINAL_BIJECTION_H_
