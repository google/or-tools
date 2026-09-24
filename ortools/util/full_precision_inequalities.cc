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

#include "ortools/util/full_precision_inequalities.h"

#include <cmath>
#include <compare>
#include <cstdint>
#include <limits>
#include <numeric>
#include <optional>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/base/attributes.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/numeric/int128.h"
#include "absl/types/span.h"
#include "ortools/algorithms/multikey_radix_sort.h"
#include "ortools/port/attributes.h"

namespace operations_research {

namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();

}

std::pair<double, double> GetLooseDotProductBounds(
    const absl::Span<const double> a, const absl::Span<const double> b) {
  // We could use DCHECK, but this is fast compared to the algorithm itself.
  CHECK_EQ(a.size(), b.size());

  double dp_ub = 0.0;
  double dp_lb = 0.0;
  for (int i = 0; i < a.size(); ++i) {
    const double prod = a[i] * b[i];
    dp_ub = std::nextafter(dp_ub + std::nextafter(prod, kInf), kInf);
    dp_lb = std::nextafter(dp_lb + std::nextafter(prod, -kInf), -kInf);
  }
  return {dp_lb, dp_ub};
}

namespace {

// Represents a real number with an int128 mantissa. Represents mantissa * 2^
// exponent up to the negative sign.
struct RealNumber {
  bool negative ABSL_REQUIRE_EXPLICIT_INIT;
  absl::uint128 mantissa ORTOOLS_REQUIRE_EXPLICIT_INIT;
  int exponent ABSL_REQUIRE_EXPLICIT_INIT;
};

RealNumber FromDouble(const double d) {
  CHECK(std::isfinite(d));
  int exp;
  const double frac = std::frexp(std::abs(d), &exp);
  return RealNumber{
      .negative = d < 0,
      .mantissa = static_cast<absl::uint128>(std::ldexp(frac, 53)),
      .exponent = exp - 53};
}

// Returns the product of a and b. It is guaranteed that mantissa >> 106 == 0.
RealNumber Prod(const double a, const double b) {
  const RealNumber a_real = FromDouble(a);
  DCHECK_EQ(a_real.mantissa >> 53, 0);
  const RealNumber b_real = FromDouble(b);
  DCHECK_EQ(b_real.mantissa >> 53, 0);
  return RealNumber{
      .negative = a_real.negative != b_real.negative,
      .mantissa = a_real.mantissa * b_real.mantissa,
      .exponent = a_real.exponent + b_real.exponent,
  };
}

// Shift the given real number with its overflow to the right by the given
// shift and adapt the exponent. Returns the comparison of the lost part with 0.
// The overflow is meant to be added to r mantissa with a factor (1 << 128).
// Note that overflow cannot increase.
std::strong_ordering ShiftRight(RealNumber& r, uint64_t& overflow,
                                const int shift) {
  if (shift == 0) return std::strong_ordering::equal;

  if (shift >= 128 + 64) {
    // Drops everything.
    std::strong_ordering dropped_part_cmp_zero =
        r.mantissa == 0 && overflow == 0
            ? std::strong_ordering::equal
            : (r.negative ? std::strong_ordering::less
                          : std::strong_ordering::greater);
    overflow = 0;
    r.mantissa = 0;
    r.exponent += shift;
    return dropped_part_cmp_zero;
  }

  if (shift >= 128) {
    const int shift_128 = shift - 128;
    const uint64_t to_drop = overflow & ((uint64_t{1} << shift_128) - 1);
    const std::strong_ordering dropped_part_cmp_zero =
        to_drop == 0 && r.mantissa == 0
            ? std::strong_ordering::equal
            : (r.negative ? std::strong_ordering::less
                          : std::strong_ordering::greater);
    r.mantissa = overflow >> shift_128;
    overflow = 0;
    r.exponent += shift;
    return dropped_part_cmp_zero;
  }

  const absl::uint128 to_drop = r.mantissa & ((absl::uint128{1} << shift) - 1);
  r.mantissa >>= shift;
  r.mantissa |= (static_cast<absl::uint128>(overflow) << (128 - shift));
  if (shift < 64) {
    overflow >>= shift;
  } else {
    overflow = 0;
  }
  r.exponent += shift;
  return to_drop == 0 ? std::strong_ordering::equal
                      : (r.negative ? std::strong_ordering::less
                                    : std::strong_ordering::greater);
}

// Adds b to a updating its overflow. Precondition: a.exponent == b.exponent.
// Note that overflow change by at most 1 unit.
void AddWithOverflow(RealNumber& a, uint64_t& overflow, const RealNumber& b) {
  DCHECK_EQ(a.exponent, b.exponent);
  if (a.negative == b.negative) {
    a.mantissa += b.mantissa;
    // Detect overflow.
    if (a.mantissa < b.mantissa) ++overflow;
  } else if (overflow == 0 && a.mantissa < b.mantissa) {
    a.mantissa = b.mantissa - a.mantissa;
    a.negative = b.negative;
  } else if (a.mantissa < b.mantissa) {
    a.mantissa -= b.mantissa;
    --overflow;
  } else {
    a.mantissa -= b.mantissa;
  }
}

// Returns the comparison of the sum of the given real numbers with 0.
// The algorithm avoids use of floating point arithmetic. Note that it reorder
// the input vector.
std::strong_ordering SumSign(std::vector<RealNumber>& terms) {
  if (terms.empty()) return std::strong_ordering::equal;

  // Sort by increasing exponent.
  absl::c_sort(terms, [](const RealNumber& a, const RealNumber& b) {
    return a.exponent < b.exponent;
  });
  RealNumber remainder = terms.front();
  // Stores the part of the remainder that has been dropped due to overflow. It
  // cannot overflow itself as it would need (1 << 64) overflows in remainder,
  // see below.
  uint64_t rem_overflow = 0;
  std::strong_ordering dropped_part_cmp_zero = std::strong_ordering::equal;
  // For invariants, set S = sum(terms) and initialize dropped_part = 0 (these
  // not being actually stored).
  // TODO(user): In case the speed is an issue, we could accumulate terms
  // with equal exponents together as long as there is no overflow. No overflow
  // is guaranteed for less than 1 << (128 - 2 * 53) = 1 << 22 terms if we
  // assume that the initial numbers are at most product of two doubles.
  for (int i = 1; i < terms.size(); ++i) {
    // Invariant:
    // S = sum(j >= i, terms[j]) + remainder + dropped_part holds with:
    //  * remainder.exponent <= terms[i].exponent,
    //  * abs(dropped_part) < 2^{remainder.exponent}
    //  * dropped_part_cmp_zero is the comparison of the dropped part with 0.

    // Shift right the remainder to match the exponent of terms[i].
    const std::strong_ordering cur_dropped_part_cmp_zero = ShiftRight(
        remainder, rem_overflow, terms[i].exponent - remainder.exponent);
    if (cur_dropped_part_cmp_zero != std::strong_ordering::equal) {
      dropped_part_cmp_zero = cur_dropped_part_cmp_zero;
    }
    // Invariant:
    // S = sum(j >= i, terms[j]) + remainder + dropped_part holds with:
    //  * remainder.exponent = terms[i].exponent,
    //  * abs(dropped_part) < 2^{remainder.exponent},
    //  * dropped_part_cmp_zero is the comparison of the dropped part with 0.

    // Compute remainder += terms[i].
    AddWithOverflow(remainder, rem_overflow, terms[i]);
    // Invariant:
    // S = sum(j > i, terms[j]) + remainder + dropped_part holds with:
    //  * remainder.exponent = terms[i].exponent,
    //  * abs(dropped_part) < 2^{remainder.exponent},
    //  * dropped_part_cmp_zero is the comparison of the dropped part with 0.
  }
  // We want to return true if S <=> 0.
  // S = remainder + dropped_part.
  if (remainder.mantissa != 0) {
    return remainder.negative ? std::strong_ordering::less
                              : std::strong_ordering::greater;
  }
  return dropped_part_cmp_zero;
}

}  // namespace

std::strong_ordering ComputeSumSign(const absl::Span<const double> v) {
  std::vector<RealNumber> terms;
  terms.reserve(v.size());
  // Store the sign of infinite values if any. If they are inconsistent, fail.
  std::optional<std::strong_ordering> inf_sign = std::nullopt;
  for (int i = 0; i < v.size(); ++i) {
    const double val = v[i];
    if (std::isfinite(val)) {
      terms.push_back(FromDouble(val));
    } else {
      CHECK(!std::isnan(val)) << "NaN values not supported.";
      const std::strong_ordering current_inf_sign =
          val < 0.0 ? std::strong_ordering::less
                    : std::strong_ordering::greater;
      if (!inf_sign.has_value()) {
        inf_sign = current_inf_sign;
      } else {
        CHECK_EQ(inf_sign.value(), current_inf_sign)
            << "Inconsistent infinite values found.";
      }
    }
  }
  if (inf_sign.has_value()) return inf_sign.value();

  return SumSign(terms);
}

bool SumIsPositive(const absl::Span<const double> v) {
  return ComputeSumSign(v) == std::strong_ordering::greater;
}

bool SumIsNegative(const absl::Span<const double> v) {
  return ComputeSumSign(v) == std::strong_ordering::less;
}

std::strong_ordering CmpDotProduct(const absl::Span<const double> a,
                                   const absl::Span<const double> b,
                                   const double bound) {
  // We could use DCHECK, but this is fast compared to the algorithm itself.
  CHECK_EQ(a.size(), b.size());

  // Fast path.
  // Compute a proven upper and lower bound for the dot product.
  const auto [dp_lb, dp_ub] = GetLooseDotProductBounds(a, b);

  // Note that if size == 0, we have dp_ub == 0.0 and dp_lb == 0.0 so that
  // we do not enter the slow path.
  if (dp_lb > bound) return std::strong_ordering::greater;
  if (dp_ub < bound) return std::strong_ordering::less;
  if (dp_ub == bound && dp_lb == bound) return std::strong_ordering::equal;

  // Slow path. We have size > 0.
  // Computes terms such that sum(terms) == dot_product - bound.
  std::vector<RealNumber> terms;
  terms.reserve(a.size() + 1);
  // Store the sign of infinite values if any. If they are inconsistent, fail.
  std::optional<std::strong_ordering> inf_sign = std::nullopt;
  for (int i = 0; i < a.size(); ++i) {
    const double a_i = a[i];
    const double b_i = b[i];
    if (std::isfinite(a_i) && std::isfinite(b_i)) {
      terms.push_back(Prod(a_i, b_i));
    } else {
      CHECK(!std::isnan(a_i) && !std::isnan(b_i))
          << "NaN values not supported.";
      CHECK(a_i != 0.0 && b_i != 0.0) << "NaN value obtained from 0 * inf";
      const std::strong_ordering current_inf_sign =
          (a_i < 0.0) == (b_i < 0.0) ? std::strong_ordering::greater
                                     : std::strong_ordering::less;
      if (!inf_sign.has_value()) {
        inf_sign = current_inf_sign;
      } else {
        CHECK_EQ(*inf_sign, current_inf_sign)
            << "Inconsistent infinite values found.";
      }
    }
  }
  if (std::isfinite(bound)) {
    terms.push_back(FromDouble(bound));
    terms.back().negative = !terms.back().negative;
  } else {
    CHECK(!std::isnan(bound)) << "NaN values not supported.";
    const std::strong_ordering current_inf_sign =
        bound < 0.0 ? std::strong_ordering::greater
                    : std::strong_ordering::less;
    if (!inf_sign.has_value()) {
      inf_sign = current_inf_sign;
    } else {
      CHECK_EQ(*inf_sign, current_inf_sign)
          << "Inconsistent infinite values found.";
    }
  }
  if (inf_sign.has_value()) return *inf_sign;

  return SumSign(terms);
}

namespace {

// Returns a value strictly between a and b, which is as close as possible to
// the middle cardinal-wise, with the aim to make dichotomy converge faster when
// double are 64 bits IEC 559. In this case, the convergence takes at most 64
// steps, rather than at most 2045 steps with usual dichotomy (e.g. if we search
// next_after(0) from the initial interval [0, 2^1022]).
double CardinalMidpoint(const double a, const double b) {
  CHECK(!std::isnan(a));
  CHECK(!std::isnan(b));
  // All static asserts of FloatToSortableUint.
  constexpr bool kCanCallFloatToSortableUint =
      std::numeric_limits<double>::is_iec559 &&
      std::numeric_limits<double>::radix == 2 &&
      sizeof(double) == sizeof(uint64_t) && sizeof(float) == sizeof(uint32_t);
  if constexpr (!kCanCallFloatToSortableUint) {
    // Fallback for incompatible types.
    return std::midpoint(a, b);
  } else {
    // a_index / b_index indexes the position of a / b within ordered doubles.
    const uint64_t a_index = FloatToSortableUint(a);
    const uint64_t b_index = FloatToSortableUint(b);
    return SortableUintToFloat(std::midpoint(a_index, b_index));
  }
}

}  // namespace

std::pair<double, double> GetTightDotProductBounds(
    const absl::Span<const double> a, const absl::Span<const double> b) {
  // We could use DCHECK, but this is fast compared to the algorithm itself.
  CHECK_EQ(a.size(), b.size());

  auto [lb, ub] = GetLooseDotProductBounds(a, b);
  if (lb == ub) return {lb, ub};

  if (std::isfinite(lb) &&
      CmpDotProduct(a, b, lb) == std::strong_ordering::equal) {
    return {lb, lb};
  }
  if (std::isfinite(ub) &&
      CmpDotProduct(a, b, ub) == std::strong_ordering::equal) {
    return {ub, ub};
  }
  // Perform a dichotomy.
  while (std::nextafter(lb, ub) != ub) {
    const double mid = CardinalMidpoint(lb, ub);
    // Note that this could be optimized as a part of the call of CmpDotProduct
    // is common to all calls.
    const auto cmp = CmpDotProduct(a, b, mid);
    if (cmp == std::strong_ordering::equal) return {mid, mid};

    if (cmp == std::strong_ordering::less) {
      ub = mid;
    } else {
      lb = mid;
    }
  }
  return {lb, ub};
}

bool DotProductIsSmallerOrEqual(const absl::Span<const double> a,
                                const absl::Span<const double> b,
                                const double ub) {
  // We could use DCHECK, but this is fast compared to the algorithm itself.
  CHECK_EQ(a.size(), b.size());

  return CmpDotProduct(a, b, ub) != std::strong_ordering::greater;
}

bool DotProductIsGreaterOrEqual(const absl::Span<const double> a,
                                const absl::Span<const double> b,
                                const double lb) {
  // We could use DCHECK, but this is fast compared to the algorithm itself.
  CHECK_EQ(a.size(), b.size());

  return CmpDotProduct(a, b, lb) != std::strong_ordering::less;
}

bool DotProductIsEqual(const absl::Span<const double> a,
                       const absl::Span<const double> b, const double res) {
  // We could use DCHECK, but this is fast compared to the algorithm itself.
  CHECK_EQ(a.size(), b.size());

  return CmpDotProduct(a, b, res) == std::strong_ordering::equal;
}

}  // namespace operations_research
