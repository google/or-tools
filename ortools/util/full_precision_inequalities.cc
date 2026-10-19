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
#include "absl/base/casts.h"
#include "absl/container/inlined_vector.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/numeric/int128.h"
#include "absl/types/span.h"
#include "ortools/algorithms/float_ordinal_bijection.h"
#include "ortools/base/types.h"

namespace operations_research {

namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();
constexpr double kDoubleMax = std::numeric_limits<double>::max();

inline int CountLZero(const absl::uint128 n) {
  const uint64_t hi = absl::Uint128High64(n);
  return hi == 0 ? 64 + std::countl_zero(absl::Uint128Low64(n))
                 : std::countl_zero(hi);
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
void TightSum::MultiplyLastTerm() {
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
      terms_.push_back({.mantissa = rem_mantissa,
                        .negative = rem_negative,
                        .exponent = 128 + terms_.back().exponent});
    }
  }
  factors_.clear();
}

namespace {

// Shift the given real number with its overflow to the right by the given
// shift and adapt the exponent. Returns the comparison of the lost part with 0.
// The overflow is meant to be added to r mantissa with a factor (1 << 128).
// Note that overflow cannot increase.
std::strong_ordering ShiftRight(TightSum::Term& r, uint64_t& overflow,
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
void AddWithOverflow(TightSum::Term& a, uint64_t& overflow,
                     const TightSum::Term& b) {
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
bool AddIfExact(TightSum::Term& a, const TightSum::Term& b) {
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
    if (shift > CountLZero(a_mantissa)) {
      return false;
    }
    a_mantissa <<= shift;
    exponent = b.exponent;
  } else if (b.exponent > a.exponent) {
    const int shift = b.exponent - a.exponent;
    if (shift > CountLZero(b_mantissa)) {
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

double ReconstructDouble(uint64_t mantissa, int64_t biased_exp,
                         bool negative = false) {
  static_assert(std::numeric_limits<double>::is_iec559);
  static_assert(std::numeric_limits<double>::radix == 2);
  static_assert(sizeof(double) == sizeof(uint64_t), "double must be 64 bits.");
  return absl::bit_cast<double>((negative ? uint64_t{1} << 63 : 0) |
                                (biased_exp << 52) |
                                (mantissa & ~(uint64_t{1} << 52)));
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

  const uint64_t biased_exp = std::max(binary_exp + 1023, 0);
  const double lower = ReconstructDouble(mantissa, biased_exp);
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
    const absl::Span<const TightSum::Term> terms,
    std::optional<TightSum::Term> additional_term) {
  DCHECK(absl::c_is_sorted(
      terms, [](const TightSum::Term& a, const TightSum::Term& b) {
        return a.exponent < b.exponent;
      }));
  DCHECK(!terms.empty());
  const int min_exponent =
      additional_term.has_value()
          ? std::min(terms.front().exponent, additional_term->exponent)
          : terms.front().exponent;
  TightSum::Term remainder = {
      .mantissa = 0, .negative = false, .exponent = min_exponent};
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
    const TightSum::Term& current_term =
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

// Returns tight bounds for t, where t is a term.
std::pair<double, double> TermDoubleBounds(const TightSum::Term& t) {
  if (t.mantissa == 0) return {0.0, 0.0};

  // Original significand.
  const int sig_bits = 128 - CountLZero(t.mantissa);
  // result is u * 2^result_normalized_exp where u is in [1, 2).
  const int result_normalized_exp = sig_bits + t.exponent - 1;
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
  const int biased_exp = std::max(result_normalized_exp + 1023, 0);
  if (sig_bits <= sig_bits_double) {
    // No loss of precision.
    const uint64_t double_mantissa = absl::Uint128Low64(t.mantissa)
                                     << (sig_bits_double - sig_bits);
    const double res =
        ReconstructDouble(double_mantissa, biased_exp, t.negative);
    return {res, res};
  }
  const int lost_digits = sig_bits - sig_bits_double;
  const uint64_t double_mantissa =
      absl::Uint128Low64(t.mantissa >> lost_digits);
  const double lb_abs = ReconstructDouble(double_mantissa, biased_exp);
  const double ub_abs =
      (t.mantissa & ((absl::uint128{1} << lost_digits) - 1)) == 0
          ? lb_abs
          : FastStepUp(lb_abs);
  return t.negative ? std::make_pair(-ub_abs, -lb_abs)
                    : std::make_pair(lb_abs, ub_abs);
}

// Returns a value strictly between a and b, which is as close as possible to
// the middle ordinal-wise, with the aim to make dichotomy converge faster when
// double are 64 bits IEC 559. In this case, the convergence takes at most 64
// steps, rather than at most 2099 steps with usual dichotomy (e.g. if we search
// nextafter(0) from the initial interval [0, 2^1022]).
double OrdinalOrStandardMidpoint(const double a, const double b) {
  CHECK(!std::isnan(a));
  CHECK(!std::isnan(b));
  if constexpr (kOrdinalBijectionCompatible<double>) {
    return OrdinalMidpoint(a, b);
  } else {
    // Fallback for incompatible types.
    return std::midpoint(a, b);
  }
}

}  // namespace

void TightSum::SortAndCompressIfNeeded() const {
  if (terms_sorted_before_ == terms_.size()) return;
  DCHECK_GT(terms_.size(), 0);

  const auto cmp = [](const Term& a, const Term& b) {
    return a.exponent < b.exponent;
  };
  // Sort and compress the newly added terms in [terms_sorted_before_, end).
  std::sort(terms_.begin() + terms_sorted_before_, terms_.end(), cmp);
  int new_end = terms_sorted_before_;
  for (int j = terms_sorted_before_; j < terms_.size(); ++j) {
    if (new_end == terms_sorted_before_ ||
        !AddIfExact(terms_[new_end - 1], terms_[j])) {
      terms_[new_end++] = terms_[j];
    }
  }
  terms_.resize(new_end, ToTerm(0));
  if (terms_sorted_before_ == 0) {
    terms_sorted_before_ = terms_.size();
    return;
  }

  // Merge [0, terms_sorted_before_) with [terms_sorted_before_, terms_.size()),
  // calling AddIfExact only on pairs involving a newly added term.
  const absl::InlinedVector<Term, 16> old_terms(
      terms_.begin(), terms_.begin() + terms_sorted_before_);
  int i_old = 0;
  int i_new = terms_sorted_before_;
  int out = -1;
  bool out_is_from_new = false;

  auto append = [&](const Term& t, const bool is_from_new) {
    if (out >= 0 &&
        (is_from_new || out_is_from_new || terms_[out].mantissa == 0)) {
      if (AddIfExact(terms_[out], t)) {
        out_is_from_new = false;
        return;
      }
    }
    terms_[++out] = t;
    out_is_from_new = is_from_new;
  };

  while (i_old < old_terms.size() && i_new < terms_.size()) {
    if (old_terms[i_old].exponent <= terms_[i_new].exponent) {
      append(old_terms[i_old++], /*is_from_new=*/false);
    } else {
      append(terms_[i_new++], /*is_from_new=*/true);
    }
  }
  while (i_new < terms_.size()) {
    append(terms_[i_new++], /*is_from_new=*/true);
  }
  while (i_old < old_terms.size()) {
    append(old_terms[i_old++], /*is_from_new=*/false);
  }
  terms_.resize(out + 1, ToTerm(0));
  terms_sorted_before_ = terms_.size();
}

template <typename Number>
std::strong_ordering TightSum::SlowCompareTo(const Number x) const {
  constexpr bool kIsDouble = std::is_same_v<Number, double>;
  if constexpr (kIsDouble) {
    CHECK(!std::isnan(x));
    if (!std::isfinite(x)) {
      DCHECK_EQ(inf_sign_, std::strong_ordering::equal);
      return x > 0.0 ? std::strong_ordering::less
                     : std::strong_ordering::greater;
    }
  }

  SortAndCompressIfNeeded();
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
template std::strong_ordering TightSum::SlowCompareTo<double>(double x) const;
template std::strong_ordering TightSum::SlowCompareTo<int64_t>(int64_t x) const;

std::pair<double, double> TightSum::SlowTightBounds() const {
  SortAndCompressIfNeeded();

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
    const double mid = OrdinalOrStandardMidpoint(lb_, ub_);
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

void TightSum::AddTerm(const Term& term) {
  if (term.mantissa == 0) return;

  AddTermToBounds(term);
  if (terms_.size() == terms_sorted_before_ ||
      !AddIfExact(terms_.back(), term)) {
    terms_.push_back(term);
  }
}

namespace {

constexpr double kSmallestDoubleAboveInt64Max = 1.0 * (1ULL << 63);

int64_t SaturatedDoubleToInt64(const double x) {
  if (x <= -kSmallestDoubleAboveInt64Max) {
    return kint64min;
  }
  if (x >= kSmallestDoubleAboveInt64Max) {
    return kint64max;
  }
  return static_cast<int64_t>(x);
}

}  // namespace

std::pair<std::optional<int64_t>, std::optional<int64_t>>
TightSum::Int64Bounds() const {
  // TODO(user): if this turns out to be performance-critical, we can
  // write an implementation similar to ExponentOrderedSumBounds() that would
  // compute those bounds without needing a binary search.
  int64_t low = SaturatedDoubleToInt64(lb_);
  int64_t high = SaturatedDoubleToInt64(ub_);
  while (true) {
    // Invariant: low - 1 < sum < high + 1
    DCHECK(low == kint64min || low - 1 < *this);
    DCHECK(high == kint64max || *this < high + 1);

    const int64_t mid = std::midpoint(low, high);
    const std::strong_ordering cmp = *this <=> mid;
    if (cmp == std::strong_ordering::equal) {
      return {mid, mid};
    }
    if (cmp == std::strong_ordering::less) {
      if (mid == kint64min) return {std::nullopt, kint64min};
      if (mid == low) return {low - 1, low};
      high = mid - 1;
    } else {
      if (mid == kint64max) return {kint64max, std::nullopt};
      if (mid == high) return {high, high + 1};
      low = mid + 1;
    }
  }
}

void TightSum::AddTermToBounds(const Term& term) const {
  if (term.mantissa == 0) return;

  const auto [lb, ub] = TermDoubleBounds(term);
  // TODO(user): We could makes this tighter. Should we?
  lb_ = FastStepDown(lb_ + lb);
  ub_ = FastStepUp(ub_ + ub);
  tight_bounds_ = false;
}

std::pair<double, double> ProdBounds(const int64_t a, const int64_t b,
                                     const PowerOfTwo p) {
  return TermDoubleBounds({.mantissa = static_cast<absl::uint128>(std::abs(a)) *
                                       static_cast<absl::uint128>(std::abs(b)),
                           .negative = (a < 0) != (b < 0),
                           .exponent = p.exponent});
}

std::pair<double, double> ProdBounds(const int64_t a, const double b,
                                     const PowerOfTwo p) {
  // Only deal with finite values. Could do infinite, but the case 0 x inf would
  // be invalid anyway.
  CHECK(std::isfinite(b));
  int e;
  const int64_t b_mantissa =
      static_cast<int64_t>(std::ldexp(std::frexp(b, &e), 53));
  return TermDoubleBounds(
      {.mantissa = static_cast<absl::uint128>(std::abs(a)) *
                   static_cast<absl::uint128>(std::abs(b_mantissa)),
       .negative = (a < 0) != (b < 0.0),
       .exponent = p.exponent + e - 53});
}

std::pair<double, double> ProdBounds(const double a, const int64_t b,
                                     const PowerOfTwo p) {
  return ProdBounds(b, a, p);
}

std::pair<double, double> ProdBounds(const double a, const double b,
                                     const PowerOfTwo p) {
  int e_a;
  const int64_t a_mantissa =
      static_cast<int64_t>(std::ldexp(std::frexp(a, &e_a), 53));
  int e_b;
  const int64_t b_mantissa =
      static_cast<int64_t>(std::ldexp(std::frexp(b, &e_b), 53));
  return TermDoubleBounds(
      {.mantissa = static_cast<absl::uint128>(std::abs(a_mantissa)) *
                   static_cast<absl::uint128>(std::abs(b_mantissa)),
       .negative = (a < 0.0) != (b < 0.0),
       .exponent = p.exponent + e_a + e_b - 106});
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

}  // namespace operations_research
