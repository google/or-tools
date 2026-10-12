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

#include <algorithm>
#include <bit>
#include <cmath>
#include <compare>
#include <cstdint>
#include <limits>
#include <numeric>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>

#include "absl/algorithm/container.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/numeric/int128.h"
#include "absl/types/span.h"
#include "ortools/algorithms/multikey_radix_sort.h"

namespace operations_research {

namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();
constexpr double kDoubleMax = std::numeric_limits<double>::max();

inline int CountLZero(const absl::uint128 n) {
  const uint64_t hi = absl::Uint128High64(n);
  return hi == 0 ? 64 + std::countl_zero(absl::Uint128Low64(n))
                 : std::countl_zero(hi);
}

inline int CountRZero(const absl::uint128 n) {
  const uint64_t lo = absl::Uint128Low64(n);
  return lo == 0 ? 64 + std::countr_zero(absl::Uint128High64(n))
                 : std::countr_zero(lo);
}

// Returns a * b +/- c, with the sign of c given by c_negative. The result is
// given as a hi / low / is_negative tuple.
// Note that (2^128 - 1) * (2^128 - 1) + 2^128 = 2^256 - 2^128 + 1 so that there
// are no overflows.
inline std::tuple<absl::uint128, absl::uint128, bool> ProdSum(
    const absl::uint128 a, const absl::uint128 b, const absl::uint128 c,
    const bool c_negative) {
  // Product.
  const absl::uint128 a_hi = absl::Uint128High64(a);
  const absl::uint128 a_lo = absl::Uint128Low64(a);
  const absl::uint128 b_hi = absl::Uint128High64(b);
  const absl::uint128 b_lo = absl::Uint128Low64(b);
  absl::uint128 low = a_lo * b_lo;
  absl::uint128 hi = a_hi * b_hi;
  const absl::uint128 mid1 = a_hi * b_lo;
  const absl::uint128 mid = mid1 + a_lo * b_hi;
  if (mid < mid1) hi += absl::uint128{1} << 64;
  hi += absl::Uint128High64(mid);
  const absl::uint128 to_add_low = mid << 64;
  low += to_add_low;
  if (low < to_add_low) ++hi;
  // Sum.
  if (!c_negative) {
    low += c;
    if (low < c) ++hi;
    return {hi, low, false};
  }
  if (hi == 0 && low < c) return {0, c - low, true};
  if (low < c) return {hi - 1, low - c, false};
  return {hi, low - c, false};
}

}  // namespace

// Note that this is slow. However, it is called only when a product of three or
// more terms is added to the sum.
void Sum::MultiplyLastTerm() {
  int i = terms_.size() - 1;
  // Fast algorithm when there is no overflow.
  int k = 0;
  for (; k < factors_.size(); ++k) {
    if (CountLZero(factors_[k].mantissa) + CountLZero(terms_[i].mantissa) <
        128) {
      // Overflow detected, we fall back to the school style multiplication.
      break;
    }
    terms_[i].negative ^= factors_[k].negative;
    terms_[i].mantissa *= factors_[k].mantissa;
    terms_[i].exponent += factors_[k].exponent;
  }

  // Slow algorithm when there is overflow. Should be relatively rare, but
  // cannot be avoided.

  // At the beginning of each iteration,
  //  sum(u = i .., terms_[u]) = product(v = i .. k-1, initial_terms[v])
  // at the end of each iteration,
  //  sum(u = i .., terms_[u]) = product(v = i .. k, initial_terms[v])
  // At any point, exponents of terms_[i] .. are following an arithmetic
  // progression with a common difference of 128.
  for (; k < factors_.size(); ++k) {
    // School style multiplication.
    absl::uint128 rem_mantissa = 0;
    bool rem_negative = false;
    for (int u = i; u < terms_.size(); ++u) {
      const bool expected_sign = factors_[k].negative ^ terms_[u].negative;
      const auto [hi, lo, opposite] =
          ProdSum(terms_[u].mantissa, factors_[k].mantissa, rem_mantissa,
                  rem_negative != expected_sign);
      rem_mantissa = hi;
      rem_negative = expected_sign != opposite;
      terms_[u].exponent += factors_[k].exponent;
      terms_[u].mantissa = lo;
      terms_[u].negative = rem_negative;
    }
    if (rem_mantissa != 0) {
      terms_.push_back({.negative = rem_negative,
                        .mantissa = rem_mantissa,
                        .exponent = 128 + terms_.back().exponent});
    }
  }
  factors_.clear();
}

namespace {

std::pair<double, double> GetBoundsWhenOneIsNotFinite(
    const absl::Span<const double> a, const absl::Span<const double> b) {
  DCHECK(!std::isfinite(a[0]) || !std::isfinite(b[0]));
  std::optional<std::strong_ordering> inf_sign = std::nullopt;
  for (int i = 0; i < a.size(); ++i) {
    if (!std::isfinite(a[i]) || !std::isfinite(b[i])) {
      CHECK(!std::isnan(a[i]) && !std::isnan(b[i]))
          << "NaN value not supported.";
      CHECK(a[i] != 0.0 && b[i] != 0.0) << "NaN value obtained from 0 * inf";
      const std::strong_ordering current_inf_sign =
          (a[i] < 0.0) == (b[i] < 0.0) ? std::strong_ordering::greater
                                       : std::strong_ordering::less;
      if (!inf_sign.has_value()) {
        inf_sign = current_inf_sign;
      } else {
        CHECK_EQ(*inf_sign, current_inf_sign)
            << "Inconsistent infinite values found.";
      }
    }
  }
  return (*inf_sign == std::strong_ordering::greater)
             ? std::make_pair(kInf, kInf)
             : std::make_pair(-kInf, -kInf);
}

}  // namespace

std::pair<double, double> GetLooseDotProductBounds(
    const absl::Span<const double> a, const absl::Span<const double> b) {
  // We could use DCHECK, but this is fast compared to the algorithm itself.
  CHECK_EQ(a.size(), b.size());

  double dp_ub = 0.0;
  double dp_lb = 0.0;
  for (int i = 0; i < a.size(); ++i) {
    // Handle infinite values. This is a rare case, but it should be checked to
    // avoid absurd NaN results. Also some downstream code assumes that infinite
    // values have been filtered out here.
    if (!std::isfinite(a[i]) || !std::isfinite(b[i])) {
      return GetBoundsWhenOneIsNotFinite(a.subspan(i), b.subspan(i));
    }
    const double prod = a[i] * b[i];
    dp_ub = FastStepUp(dp_ub + FastStepUp(prod));
    dp_lb = FastStepDown(dp_lb + FastStepDown(prod));
  }
  return {dp_lb, dp_ub};
}

namespace {

// Shift the given real number with its overflow to the right by the given
// shift and adapt the exponent. Returns the comparison of the lost part with 0.
// The overflow is meant to be added to r mantissa with a factor (1 << 128).
// Note that overflow cannot increase.
std::strong_ordering ShiftRight(Sum::Term& r, uint64_t& overflow,
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
void AddWithOverflow(Sum::Term& a, uint64_t& overflow, const Sum::Term& b) {
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

// Adds b to a if the exact sum fits in a without overflow; otherwise leaves a
// unchanged and returns false.
bool AddIfExact(Sum::Term& a, const Sum::Term& b) {
  if (b.mantissa == 0) return true;
  if (a.mantissa == 0) {
    a = b;
    return true;
  }

  absl::uint128 a_mantissa = a.mantissa;
  absl::uint128 b_mantissa = b.mantissa;
  int exponent = a.exponent;
  if (a.exponent > b.exponent) {
    const int shift = a.exponent - b.exponent;
    if (shift >= 128 || a_mantissa >= (absl::uint128{1} << (128 - shift))) {
      return false;
    }
    a_mantissa <<= shift;
    exponent = b.exponent;
  } else if (b.exponent > a.exponent) {
    const int shift = b.exponent - a.exponent;
    if (shift >= 128 || b_mantissa >= (absl::uint128{1} << (128 - shift))) {
      return false;
    }
    b_mantissa <<= shift;
  }

  if (a.negative == b.negative) {
    const absl::uint128 sum = a_mantissa + b_mantissa;
    if (sum < a_mantissa) return false;
    a.mantissa = sum;
    a.exponent = exponent;
    return true;
  }

  if (a_mantissa >= b_mantissa) {
    a.mantissa = a_mantissa - b_mantissa;
  } else {
    a.mantissa = b_mantissa - a_mantissa;
    a.negative = b.negative;
  }
  a.exponent = exponent;
  return true;
}

// Returns {lb, ub} where lb <= (hi * 2^128 + lo) * 2^exp <= ub and the bounds
// are tight. In other terms, posing x = (hi * 2^128 + lo) * 2^exp:
//  * lb == ub == x if x is representable as a double, and
//  * lb < x < ub == FastStepUp(lb) otherwise.
std::pair<double, double> HiLowBounds(uint64_t hi, absl::uint128 lo, int exp) {
  if (hi == 0 && lo == 0) return {0.0, 0.0};

  int bit_width;
  if (hi != 0) {
    bit_width = 128 + std::bit_width(hi);
  } else if (absl::Uint128High64(lo) != 0) {
    bit_width = 64 + std::bit_width(absl::Uint128High64(lo));
  } else {
    bit_width = std::bit_width(absl::Uint128Low64(lo));
  }

  // (hi * 2^128 + lo) * 2^exp lies in [2^binary_exp, 2^(binary_exp + 1)).
  const int binary_exp = (bit_width - 1) + exp;
  if (binary_exp > 1023) {
    return {std::numeric_limits<double>::max(), kInf};
  }
  if (binary_exp < -1074) {
    return {0.0, FastStepUp(0.0)};
  }

  // Number of mantissa bits available in double at this exponent:
  // 53 for normal numbers (binary_exp >= -1022), or 1..52 for subnormals.
  const int sig_bits = std::min(53, binary_exp + 1075);

  // Mantissa aligned in such a way that the result is:
  //  * [mantissa * 2^(-52)] * 2^(binary_exp - sig_bits + 53)
  // or, in other terms,
  //  * [mantissa * 2^(-52)] * 2^binary_exp if binary_exp >= -1022 [normal case]
  //  * [mantissa * 2^(-52)] * 2^-1022      if binary_exp <= -1023 [subnormal
  //  case]
  uint64_t mantissa;
  bool is_exact;
  if (bit_width <= sig_bits) {
    mantissa = static_cast<uint64_t>(lo) << (sig_bits - bit_width);
    is_exact = true;
  } else {
    const int shift = bit_width - sig_bits;
    if (shift >= 128) {
      const int s = shift - 128;
      mantissa = hi >> s;
      const uint64_t dropped_hi = (hi & ((uint64_t{1} << s) - 1));
      is_exact = (dropped_hi == 0) && (lo == 0);
    } else {
      const absl::uint128 shifted =
          (lo >> shift) | ((static_cast<absl::uint128>(hi) << (128 - shift)));
      mantissa = static_cast<uint64_t>(shifted);
      const absl::uint128 dropped_lo = lo & ((absl::uint128{1} << shift) - 1);
      is_exact = (dropped_lo == 0);
    }
  }

  const double lower =
      std::ldexp(static_cast<double>(mantissa), binary_exp + 1 - sig_bits);
  if (is_exact) {
    return {lower, lower};
  } else {
    return {lower, FastStepUp(lower)};
  }
}

// Returns provable lower and upper bounds [lb, ub] on the sum S of the given
// real numbers, sharing the exact sign of S:
//  * S == 0  <=>  lb == 0.0 && ub == 0.0
//  * S > 0   <=>  0.0 <= lb <= ub && ub > 0.0
//  * S < 0   <=>  lb <= ub <= 0.0 && lb < 0.0
// Whenever lb != ub, S lies strictly inside the open interval (lb, ub).
// Precondition: the input vector is sorted by increasing exponent.
// If additional_term is present, it is added to the sum.
std::pair<double, double> ExponentOrderedSumBounds(
    const absl::Span<const Sum::Term> terms,
    std::optional<Sum::Term> additional_term) {
  DCHECK(absl::c_is_sorted(terms, [](const Sum::Term& a, const Sum::Term& b) {
    return a.exponent < b.exponent;
  }));
  DCHECK(!terms.empty());
  const int min_exponent =
      additional_term.has_value()
          ? std::min(terms.front().exponent, additional_term->exponent)
          : terms.front().exponent;
  Sum::Term remainder = {
      .negative = false, .mantissa = 0, .exponent = min_exponent};
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
  for (int i = 0; i < terms.size() || additional_term.has_value(); ++i) {
    const bool use_additional_term =
        additional_term.has_value() &&
        (i == terms.size() || additional_term->exponent <= terms[i].exponent);
    const Sum::Term& current_term =
        use_additional_term ? *additional_term : terms[i];
    // Invariant:
    // S = sum(j >= i, terms[j]) + remainder + dropped_part holds with:
    //  * remainder.exponent <= terms[i].exponent,
    //  * abs(dropped_part) < 2^{remainder.exponent}
    //  * dropped_part_cmp_zero is the comparison of the dropped part with 0.

    // Shift right the remainder to match the exponent of terms[i].
    const std::strong_ordering cur_dropped_part_cmp_zero = ShiftRight(
        remainder, rem_overflow, current_term.exponent - remainder.exponent);
    if (cur_dropped_part_cmp_zero != std::strong_ordering::equal) {
      dropped_part_cmp_zero = cur_dropped_part_cmp_zero;
    }
    // Invariant:
    // S = sum(j >= i, terms[j]) + remainder + dropped_part holds with:
    //  * remainder.exponent = terms[i].exponent,
    //  * abs(dropped_part) < 2^{remainder.exponent},
    //  * dropped_part_cmp_zero is the comparison of the dropped part with 0.

    // Compute remainder += terms[i].
    AddWithOverflow(remainder, rem_overflow, current_term);
    // Invariant:
    // S = sum(j > i, terms[j]) + remainder + dropped_part holds with:
    //  * remainder.exponent = terms[i].exponent,
    //  * abs(dropped_part) < 2^{remainder.exponent},
    //  * dropped_part_cmp_zero is the comparison of the dropped part with 0.
    if (use_additional_term) {
      additional_term.reset();
      --i;
    }
  }

  if (rem_overflow == 0 && remainder.mantissa == 0) {
    if (dropped_part_cmp_zero == std::strong_ordering::equal) {
      return {0.0, 0.0};
    }
    const double bound =
        std::max(FastStepUp(0.0), std::ldexp(1.0, remainder.exponent));
    if (dropped_part_cmp_zero == std::strong_ordering::less) {
      return {-bound, 0.0};
    }
    return {0.0, bound};
  }

  // Let M = rem_overflow * 2^128 + remainder.mantissa and
  // E = remainder.exponent. Then |S| lies strictly in
  // (M' * 2^E, (M' + 1) * 2^E) where:
  //     M' = M if dropped part has the same sign as the remainder:
  //          M - 1 else.
  double mag_lb;
  double mag_ub;
  if (dropped_part_cmp_zero == std::strong_ordering::equal) {
    // |S| = M * 2^{remainder.exponent} exactly.
    std::tie(mag_lb, mag_ub) =
        HiLowBounds(rem_overflow, remainder.mantissa, remainder.exponent);
  } else {
    uint64_t hi = rem_overflow;
    absl::uint128 lo = remainder.mantissa;
    const bool dropped_is_neg =
        (dropped_part_cmp_zero == std::strong_ordering::less);
    if (dropped_is_neg != remainder.negative) {
      // dropped_part has opposite sign to remainder: M' = M - 1.
      if (lo == 0) --hi;
      --lo;
    }
    mag_lb = HiLowBounds(hi, lo, remainder.exponent).first;

    const absl::uint128 lo_rem_plus_1 = lo + 1;
    const uint64_t hi_rem_plus_1 = hi + (lo_rem_plus_1 == 0 ? 1 : 0);
    mag_ub =
        HiLowBounds(hi_rem_plus_1, lo_rem_plus_1, remainder.exponent).second;
  }

  return remainder.negative
             ? std::make_pair(-mag_ub, mag_lb == 0.0 ? 0.0 : -mag_lb)
             : std::make_pair(mag_lb, mag_ub);
}

// Returns tight bounds around x * 2^exponent.
std::pair<double, double> LdExpBounds(const double x, const int exponent) {
  static_assert(std::numeric_limits<double>::is_iec559);
  static_assert(std::numeric_limits<double>::radix == 2);
  static_assert(sizeof(double) == sizeof(uint64_t), "double must be 64 bits.");

  if (x == 0.0) return {0.0, 0.0};

  int result_exp;
  const double normalized_x = std::frexp(x, &result_exp);
  result_exp += exponent;

  if (result_exp > 1024) {
    // Overflow.
    return x > 0.0 ? std::make_pair(kDoubleMax, kInf)
                   : std::make_pair(-kInf, -kDoubleMax);
  }
  const double candidate = std::ldexp(normalized_x, result_exp);
  if (result_exp >= -1021) {
    // The result is not subnormal and not overflowing.
    return {candidate, candidate};
  }
  const double candidate_scaled = std::ldexp(candidate, -result_exp);
  const double lb =
      candidate_scaled > normalized_x ? FastStepDown(candidate) : candidate;
  const double ub =
      candidate_scaled < normalized_x ? FastStepUp(candidate) : candidate;
  return {lb, ub};
}

// Returns tight bounds for t, where t is a term.
std::pair<double, double> TermDoubleBounds(const Sum::Term& t) {
  static_assert(std::numeric_limits<double>::is_iec559);
  static_assert(std::numeric_limits<double>::radix == 2);
  static_assert(sizeof(double) == sizeof(uint64_t), "double must be 64 bits.");
  if (t.mantissa == 0) return {0.0, 0.0};

  const int init_rz = CountRZero(t.mantissa);
  const absl::uint128 mantissa = t.mantissa >> init_rz;
  const int exponent = t.exponent + init_rz;

  // Original significand.
  const int sig_bits = 128 - CountLZero(mantissa);
  // result is u * 2^result_normalized_exp where u is in [1, 2).
  const int result_normalized_exp = sig_bits + exponent - 1;
  if (result_normalized_exp > 1023) {
    // Overflow.
    return t.negative ? std::make_pair(-kInf, -kDoubleMax)
                      : std::make_pair(kDoubleMax, kInf);
  }
  if (result_normalized_exp <= -1075) {
    // Underflow.
    return t.negative ? std::make_pair(FastStepDown(0.0), 0.0)
                      : std::make_pair(0.0, FastStepUp(0.0));
  }
  // Number of bits in the significand of the result.
  const int sig_bits_double = std::min(53, 1075 + result_normalized_exp);
  if (sig_bits <= sig_bits_double) {
    // No loss of precision.
    const uint64_t double_mantissa =
        (absl::Uint128Low64(mantissa) << (sig_bits_double - sig_bits)) &
        ~(uint64_t{1} << 52);
    const int double_exponent = std::max(result_normalized_exp + 1023, 0);
    const double res = std::bit_cast<double>(
        (t.negative ? int64_t{1} << 63 : 0) |
        (static_cast<uint64_t>(double_exponent) << 52) | double_mantissa);
    return {res, res};
  }
  const int lost_digits = sig_bits - sig_bits_double;
  const uint64_t double_mantissa =
      absl::Uint128Low64(mantissa >> lost_digits) & ~(uint64_t{1} << 52);
  const int double_exponent = std::max(result_normalized_exp + 1023, 0);
  const double lb_abs = std::bit_cast<double>(
      (static_cast<uint64_t>(double_exponent) << 52) | double_mantissa);
  const double ub_abs =
      (mantissa & ((absl::uint128{1} << lost_digits) - 1)) == 0
          ? lb_abs
          : FastStepUp(lb_abs);
  return t.negative ? std::make_pair(-ub_abs, -lb_abs)
                    : std::make_pair(lb_abs, ub_abs);
}

// Returns a value strictly between a and b, which is as close as possible to
// the middle cardinal-wise, with the aim to make dichotomy converge faster when
// double are 64 bits IEC 559. In this case, the convergence takes at most 64
// steps, rather than at most 2045 steps with usual dichotomy (e.g. if we search
// nextafter(0) from the initial interval [0, 2^1022]).
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

void Sum::SortIfNeeded() const {
  if (terms_sorted_before_ == terms_.size()) return;

  const auto cmp = [](const Term& a, const Term& b) {
    return a.exponent < b.exponent;
  };
  std::sort(terms_.begin() + terms_sorted_before_, terms_.end(), cmp);
  if (terms_sorted_before_ > 0) {
    std::inplace_merge(terms_.begin(), terms_.begin() + terms_sorted_before_,
                       terms_.end(), cmp);
  }
  terms_sorted_before_ = terms_.size();
}

template <typename Number>
std::strong_ordering Sum::SlowCompareTo(const Number x) const {
  constexpr bool kIsDouble = std::is_same_v<Number, double>;
  if constexpr (kIsDouble) {
    CHECK(!std::isnan(x));
    if (!std::isfinite(x)) {
      DCHECK_EQ(inf_sign_, std::strong_ordering::equal);
      return x > 0.0 ? std::strong_ordering::less
                     : std::strong_ordering::greater;
    }
  }

  SortIfNeeded();
  // Avoid overflow by negating x on int64_t.
  Term extra_term = ToTerm(x);
  extra_term.negative = !extra_term.negative;
  const auto [lb, ub] = ExponentOrderedSumBounds(terms_, extra_term);
  if (lb != 0.0) {
    if constexpr (kIsDouble) {
      lb_ = std::max(lb_, x == 0 ? lb : FastStepDown(lb + x));
      ub_ = std::min(ub_, x == 0 ? ub : FastStepUp(ub + x));
    } else {
      const auto [x_lb, x_ub] = Int64DoubleBounds(x);
      lb_ = std::max(lb_, FastStepDown(lb + x_lb));
      ub_ = std::min(ub_, FastStepUp(ub + x_ub));
    }
  }
  if (lb < 0.0) return std::strong_ordering::less;
  if (ub > 0.0) return std::strong_ordering::greater;

  if constexpr (kIsDouble) {
    lb_ = x;
    ub_ = x;
    tight_bounds_ = true;
  } else {
    std::tie(lb_, ub_) = Int64DoubleBounds(x);
    if (lb_ == ub_) tight_bounds_ = true;
  }
  return std::strong_ordering::equal;
}
template std::strong_ordering Sum::SlowCompareTo<double>(double x) const;
template std::strong_ordering Sum::SlowCompareTo<int64_t>(int64_t x) const;

std::pair<double, double> Sum::SlowTightBounds() const {
  SortIfNeeded();

  tight_bounds_ = true;
  {
    auto [lb, ub] = ExponentOrderedSumBounds(terms_, std::nullopt);
    lb_ = std::max(lb_, lb);
    ub_ = std::min(ub_, ub);
  }
  if (lb_ == ub_) return {lb_, ub_};

  // Perform a dichotomy (only needed if >52 leading bits cancelled out at the
  // largest exponent).
  while (FastStepUp(lb_) != ub_) {
    const double mid = CardinalMidpoint(lb_, ub_);
    const auto cmp = *this <=> mid;
    if (cmp == std::strong_ordering::equal) {
      lb_ = mid;
      ub_ = mid;
      return {lb_, ub_};
    }

    if (cmp == std::strong_ordering::less) {
      ub_ = mid;
    } else {
      lb_ = mid;
    }
  }
  return {lb_, ub_};
}

void Sum::AddTerm(const Term& term) {
  if (term.mantissa == 0) return;

  AddTermToBounds(term);
  if (terms_.size() == terms_sorted_before_ ||
      !AddIfExact(terms_.back(), term)) {
    terms_.push_back(term);
  }
}

void Sum::AddTermToBounds(const Term& term) const {
  if (term.mantissa == 0) return;

  const auto [lb, ub] = TermDoubleBounds(term);
  // TODO(user): We could makes this tighter. Should we?
  lb_ = FastStepDown(lb_ + lb);
  ub_ = FastStepUp(ub_ + ub);
  tight_bounds_ = false;
}

std::strong_ordering ComputeSumSign(const absl::Span<const double> v) {
  return SumAll(v) <=> 0.0;
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

  return DotProduct(a, b) <=> bound;
}

std::pair<double, double> GetTightDotProductBounds(
    const absl::Span<const double> a, const absl::Span<const double> b) {
  // We could use DCHECK, but this is fast compared to the algorithm itself.
  CHECK_EQ(a.size(), b.size());

  return DotProduct(a, b).TightBounds();
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

std::pair<double, double> ProdBounds(const int64_t a, const int64_t b) {
  const absl::uint128 abs_prod = static_cast<absl::uint128>(std::abs(a)) *
                                 static_cast<absl::uint128>(std::abs(b));
  const int leading_zeros =
      absl::Uint128High64(abs_prod) == 0
          ? 64 + std::countl_zero(absl::Uint128Low64(abs_prod))
          : std::countl_zero(absl::Uint128High64(abs_prod));
  const int lost_digits = 128 - leading_zeros - 53;
  if (lost_digits <= 0) {
    const double res = a > 0 == b > 0 ? static_cast<double>(abs_prod)
                                      : -static_cast<double>(abs_prod);
    return {res, res};
  }
  const double lb =
      std::ldexp(static_cast<double>(abs_prod >> lost_digits), lost_digits);
  const double ub = (abs_prod & ((absl::uint128{1} << lost_digits) - 1)) == 0
                        ? lb
                        : FastStepUp(lb);
  if (a < 0 == b < 0) {
    return {lb, ub};
  } else {
    return {-ub, -lb};
  }
}

std::pair<double, double> ProdBounds(const int64_t a, const double b) {
  // Only deal with finite values. Could do infinite, but the case 0 x inf would
  // be invalid anyway.
  CHECK(std::isfinite(b));
  int e;
  const int64_t b_mantissa =
      static_cast<int64_t>(std::ldexp(std::frexp(b, &e), 53));
  const auto [lb_m, ub_m] = ProdBounds(a, b_mantissa);
  return {LdExpBounds(lb_m, e - 53).first, LdExpBounds(ub_m, e - 53).second};
}

std::pair<double, double> ProdBounds(const double a, const int64_t b) {
  return ProdBounds(b, a);
}

std::pair<double, double> ProdBounds(const double a, const double b) {
  int e_a;
  const int64_t a_mantissa =
      static_cast<int64_t>(std::ldexp(std::frexp(a, &e_a), 53));
  int e_b;
  const int64_t b_mantissa =
      static_cast<int64_t>(std::ldexp(std::frexp(b, &e_b), 53));
  const auto [lb_m, ub_m] = ProdBounds(a_mantissa, b_mantissa);
  double lb = LdExpBounds(lb_m, e_a + e_b - 106).first;
  double ub = LdExpBounds(ub_m, e_a + e_b - 106).second;
  return {lb, ub};
}

std::pair<double, double> Int64DoubleBounds(const int64_t n) {
  // Avoid the overflow of std::abs when n ==
  // std::numeric_limits<int64_t>::min().
  const uint64_t abs_n = n == std::numeric_limits<int64_t>::min()
                             ? uint64_t{1} << 63
                             : static_cast<uint64_t>(std::abs(n));
  const int lost_digits = 64 - std::countl_zero(abs_n) - 53;
  if (lost_digits <= 0) {
    const double res = static_cast<double>(n);
    return {res, res};
  }
  const double lb =
      std::ldexp(static_cast<double>(abs_n >> lost_digits), lost_digits);
  const double ub =
      (abs_n & ((uint64_t{1} << lost_digits) - 1)) == 0 ? lb : FastStepUp(lb);
  if (n < 0) {
    return {-ub, -lb};
  } else {
    return {lb, ub};
  }
}

std::pair<double, double> FuseAddMultiplyAdd(const double scale,
                                             const double shift,
                                             const double pre_shift,
                                             const int64_t n) {
  DCHECK(std::isfinite(scale));
  DCHECK(std::isfinite(shift));
  DCHECK(std::isfinite(pre_shift));
  return Sum()
      .AddProd(scale, n)
      .AddProd(scale, pre_shift)
      .Add(shift)
      .TightBounds();
}

}  // namespace operations_research
