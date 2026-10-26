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
#include <optional>
#include <tuple>
#include <utility>

#include "absl/base/casts.h"
#include "absl/container/inlined_vector.h"
#include "absl/log/check.h"
#include "absl/numeric/int128.h"
#include "ortools/base/types.h"

namespace operations_research {

namespace internal_full_precision_inequalities {

class InternalTightSum {
 public:
  using Term = TightSum::Term;
  template <typename T>
  static Term ToTerm(T x) {
    return TightSum::ToTerm(x);
  }
};

}  // namespace internal_full_precision_inequalities

namespace {

using Term = internal_full_precision_inequalities::InternalTightSum::Term;
template <typename T>
Term ToTerm(T x) {
  return internal_full_precision_inequalities::InternalTightSum::ToTerm(x);
}

constexpr double kInf = std::numeric_limits<double>::infinity();
constexpr double kDoubleMin = std::numeric_limits<double>::denorm_min();
constexpr double kDoubleMax = std::numeric_limits<double>::max();

inline int CountLZero(const absl::int128 n) {
  const uint64_t hi = static_cast<uint64_t>(absl::Int128High64(n));
  return hi == 0 ? 64 + std::countl_zero(absl::Int128Low64(n))
                 : std::countl_zero(hi);
}

inline int CountLOne(const absl::int128 n) {
  const uint64_t hi = static_cast<uint64_t>(absl::Int128High64(n));
  return hi == kuint64max ? 64 + std::countl_one(absl::Int128Low64(n))
                          : std::countl_one(hi);
}

// Returns the number of leading insignificant bits in n.
// Assumes n != 0.
inline int CountLeadingInsignificant(const absl::int128 n) {
  return n < 0 ? CountLOne(n - 1) : CountLZero(n);
}

// Returns a * b + c as a hi / low pair such that a * b + c = hi * 2^128 + low.
inline std::pair<absl::int128, absl::int128> ProdSum(const absl::int128 a,
                                                     const absl::int128 b,
                                                     const absl::int128 c) {
  // Returns two int128 such that x = hi * 2^64 + lo, hi in [-2^63, 2^63] and lo
  // in [-2^63, 2^63 - 1].
  auto split = [](absl::int128 x) {
    absl::int128 hi = absl::Int128High64(x);
    int64_t lo = static_cast<int64_t>(absl::Int128Low64(x));
    if (lo < 0) ++hi;
    return std::make_pair(hi, static_cast<absl::int128>(lo));
  };
  const auto [a_hi, a_lo] = split(a);
  const auto [b_hi, b_lo] = split(b);
  const auto [c_hi, c_lo] = split(c);

  const absl::int128 lo0 = a_lo * b_lo + c_lo;  // in [-2^126, 2^126 + 2^63 - 1]
  const absl::int128 lo0_hi = absl::Int128High64(lo0);  // in [-2^62, 2^62]
  const absl::int128 lo0_lo = absl::Int128Low64(lo0);   // in [0, 2^64 - 1]

  // Both arguments to split() are in [-2^126 - 2^64, 2^126 + 2^64], so neither
  // can overflow int128.
  const auto [mid1_hi, mid1_lo] = split(a_hi * b_lo + lo0_hi);
  const auto [mid2_hi, mid2_lo] = split(a_lo * b_hi + c_hi + mid1_lo);

  const absl::int128 hi = a_hi * b_hi + mid1_hi + mid2_hi;
  // mid2_lo is in [-2^63, 2^63 - 1] and lo0_lo is in [0, 2^64 - 1], so lo is
  // guaranteed to be in [-2^127, 2^127 - 1] without overflow.
  const absl::int128 lo = (mid2_lo << 64) + lo0_lo;
  return {hi, lo};
}

}  // namespace

TightSum& TightSum::Negate() {
  if (!IsFinite()) {
    const bool neg = inf_sign_ == std::strong_ordering::greater;
    Clear();
    SetInfIfNeeded(neg ? -kInf : kInf);
    return *this;
  }
  if (digits_.empty() && !negative_) return *this;

  bounds_updated_ = false;
  bool carry = true;
  for (uint64_t& d : digits_) {
    d = ~d + (carry ? 1 : 0);
    carry = carry && (d == 0);
  }
  if (!carry) {
    negative_ = !negative_;
  } else if (negative_) {
    digits_.push_back(1);
    negative_ = false;
  }
  return *this;
}

TightSum& TightSum::AddProd(const TightSum& other, const bool negate,
                            const PowerOfTwo scaling) {
  // TODO(user): This is not super efficient and can be improved.
  if (!other.IsFinite()) {
    const bool other_neg =
        (other.inf_sign_ == std::strong_ordering::less) ^ negate;
    SetInfIfNeeded(other_neg ? -kInf : kInf);
    return *this;
  }
  if (!IsFinite()) return *this;

  if (this == &other) {
    if (scaling.exponent == 0) {
      if (negate) {
        Clear();
      } else {
        ++exponent_;
        bounds_updated_ = false;
      }
      return *this;
    }

    const TightSum copy = other;
    return AddProd(copy, negate, scaling);
  }

  if (digits_.empty() && !negative_) {
    *this = other;
    exponent_ += scaling.exponent;
    if (scaling.exponent != 0) bounds_updated_ = false;
    if (negate) Negate();
    return *this;
  }

  const int num_digits = other.digits_.size();
  for (int i = 0; i < num_digits; ++i) {
    const absl::int128 m = other.digits_[i];
    AddTerm({.mantissa = negate ? -m : m,
             .exponent = other.exponent_ + scaling.exponent + 64 * i});
  }

  if (other.negative_) {
    AddTerm({.mantissa = negate ? 1 : -1,
             .exponent = other.exponent_ + scaling.exponent + 64 * num_digits});
  }
  return *this;
}

// Note that this is slow. However, it is called only when a product of three or
// more terms is added to the sum.
void TightSum::MultiplyTermsAndAdd(Term term1) {
  // Fast algorithm when there is no overflow.
  int k = 0;
  for (; k < factors_.size(); ++k) {
    if (CountLeadingInsignificant(factors_[k].mantissa) +
            CountLeadingInsignificant(term1.mantissa) <
        129) {
      // Overflow detected, we fall back to the school style multiplication.
      break;
    }
    term1.mantissa *= factors_[k].mantissa;
    term1.exponent += factors_[k].exponent;
  }
  if (k == factors_.size()) {
    AddTerm(term1);
    factors_.clear();
    return;
  }

  // Slow algorithm when there is overflow. Should be relatively rare, but
  // cannot be avoided.

  // At the beginning of each iteration,
  //  sum(product_terms) = term1 * product(v = 0 .. k-1, factors_[v])
  // at the end of each iteration,
  //  sum(product_terms) = term1 * product(v = 0 .. k, factors_[v])
  // At any point, exponents of terms_[i] .. are following an arithmetic
  // progression with a common difference of 128.
  absl::InlinedVector<Term, 16> product_terms = {term1};
  for (; k < factors_.size(); ++k) {
    // School style multiplication.
    absl::int128 rem_mantissa = 0;
    for (int u = 0; u < product_terms.size(); ++u) {
      std::tie(rem_mantissa, product_terms[u].mantissa) = ProdSum(
          product_terms[u].mantissa, factors_[k].mantissa, rem_mantissa);
      product_terms[u].exponent += factors_[k].exponent;
    }
    if (rem_mantissa != 0) {
      product_terms.push_back(
          {.mantissa = rem_mantissa,
           .exponent = 128 + product_terms.back().exponent});
    }
  }
  for (const Term& term : product_terms) {
    AddTerm(term);
  }
  factors_.clear();
}

namespace {

double ReconstructDouble(uint64_t mantissa, int64_t biased_exp,
                         bool negative = false) {
  static_assert(std::numeric_limits<double>::is_iec559);
  static_assert(std::numeric_limits<double>::radix == 2);
  static_assert(sizeof(double) == sizeof(uint64_t), "double must be 64 bits.");
  return absl::bit_cast<double>((negative ? uint64_t{1} << 63 : 0) |
                                (biased_exp << 52) |
                                (mantissa & ~(uint64_t{1} << 52)));
}

// Returns tight bounds for t, where t is a term.
std::pair<double, double> TermDoubleBounds(const Term& t) {
  if (t.mantissa == 0) return {0.0, 0.0};

  const bool negative = t.mantissa < 0;
  const int sig_bits = 128 - CountLeadingInsignificant(t.mantissa);
  // result is u * 2^result_normalized_exp where u is in [1, 2).
  const int result_normalized_exp = sig_bits + t.exponent - 1;
  if (result_normalized_exp > 1023) {
    // Overflow.
    return negative ? std::make_pair(-kInf, -kDoubleMax)
                    : std::make_pair(kDoubleMax, kInf);
  }
  if (result_normalized_exp <= -1075) {
    // Underflow.
    return negative ? std::make_pair(FastStepDown(0.0), 0.0)
                    : std::make_pair(0.0, FastStepUp(0.0));
  }
  // Number of bits in the significand of the result.
  const int sig_bits_double = std::min(53, 1075 + result_normalized_exp);
  const int biased_exp = std::max(result_normalized_exp + 1023, 0);
  if (sig_bits <= sig_bits_double) {
    // No loss of precision.
    const uint64_t double_mantissa = absl::Int128Low64(t.mantissa)
                                     << (sig_bits_double - sig_bits);
    const double res = ReconstructDouble(
        negative ? -double_mantissa : double_mantissa, biased_exp, negative);
    return {res, res};
  }
  const int lost_digits = sig_bits - sig_bits_double;
  const uint64_t double_mantissa = absl::Int128Low64(t.mantissa >> lost_digits);
  const bool exact = (t.mantissa & ((absl::int128{1} << lost_digits) - 1)) == 0;
  const double lb_abs = ReconstructDouble(
      negative ? ~double_mantissa + exact : double_mantissa, biased_exp);
  const double ub_abs = exact ? lb_abs : FastStepUp(lb_abs);
  return negative ? std::make_pair(-ub_abs, -lb_abs)
                  : std::make_pair(lb_abs, ub_abs);
}

// Returns the floor and ceil of t as int64_t. If the result is not
// representable as int64_t, returns {int64max, nullopt} or {nullopt, int64min}
// depending on the sign.
std::pair<std::optional<int64_t>, std::optional<int64_t>> TermFloorCeil(
    const Term& t) {
  if (t.mantissa == 0) return {0, 0};

  const bool negative = t.mantissa < 0;
  // Significand.
  const int sig_bits = 128 - CountLeadingInsignificant(t.mantissa);
  if (sig_bits + t.exponent <= 0) {
    return negative ? std::make_pair(-1, 0) : std::make_pair(0, 1);
  }
  // Special case for min int64_t.
  if (negative && sig_bits + t.exponent == 64 &&
      t.mantissa == (absl::int128{-1} << (sig_bits - 1))) {
    return {kint64min, kint64min};
  }
  if (sig_bits + t.exponent >= 64) {
    if (negative) {
      return {std::nullopt, kint64min};
    } else {
      return {kint64max, std::nullopt};
    }
  }
  if (t.exponent >= 0) {
    const int64_t val = static_cast<int64_t>(t.mantissa << t.exponent);
    return {val, val};
  }
  const absl::int128 lost_digits =
      t.mantissa & ((absl::int128{1} << -t.exponent) - 1);
  const int64_t lb = static_cast<int64_t>(t.mantissa >> -t.exponent);
  if (lost_digits == 0) return {lb, lb};
  if (lb == kint64max) return {kint64max, std::nullopt};
  return {lb, lb + 1};
}

template <typename Number1, typename Number2>
Term ProdAsTerm(const Number1 a, const Number2 b, const PowerOfTwo p) {
  if constexpr (std::is_same_v<Number1, double>) CHECK(std::isfinite(a));
  if constexpr (std::is_same_v<Number2, double>) CHECK(std::isfinite(b));
  const Term a_term = ToTerm(a);
  Term b_term = ToTerm(b);
  b_term.mantissa *= a_term.mantissa;
  b_term.exponent += a_term.exponent + p.exponent;
  return b_term;
}

}  // namespace

// Returns mantissa, exponent and exact such that
//       mantissa * 2^exponent <= sum < (mantissa + 1) * 2^exponent and
// mantissa < -2^62 or mantissa >= 2^62.
// exact is true if and only if the left bound is an equality. The only
// exception is when the sum is 0, where we return {0, 0, true}.
std::tuple<int64_t, int, bool> TightSum::ExtractSignificand() const {
  // Recall that the sum is:
  //   sum_(i = 0..digits_.size() - 1) digits_[i] * 2^(exponent_ + 64 * i)
  //       - negative_ * 2^(exponent_ + 64 * digits_.size())
  // If the sum is negative, leading ones are insignificant while if the sum is
  // positive, leading zeros are insignificant.
  if (negative_) {
    while (!digits_.empty() && digits_.back() == kuint64max) {
      digits_.pop_back();
    }
  } else {
    while (!digits_.empty() && digits_.back() == 0) {
      digits_.pop_back();
    }
  }
  if (digits_.empty()) {
    return negative_ ? std::make_tuple(int64_t{-1} << 63, exponent_ - 63, true)
                     : std::make_tuple(int64_t{0}, 0, true);
  }
  const uint64_t sign = negative_ ? uint64_t{1} << 63 : 0;
  const int leading_insignificant = negative_
                                        ? std::countl_one(digits_.back())
                                        : std::countl_zero(digits_.back());

  if (leading_insignificant <= 1) {
    // 63 significant bits are enough.
    const int lost_digits = 1 - leading_insignificant;
    const uint64_t lost_digits_mask = (uint64_t{1} << lost_digits) - 1;
    const int64_t mantissa =
        std::bit_cast<int64_t>(sign | (digits_.back() >> lost_digits));
    const int exponent = exponent_ + 64 * (digits_.size() - 1) + lost_digits;
    const bool exact = (digits_.back() & lost_digits_mask) == 0 &&
                       std::all_of(digits_.begin(), digits_.end() - 1,
                                   [](uint64_t digit) { return digit == 0; });
    return {mantissa, exponent, exact};
  }

  if (digits_.size() == 1) {
    const int64_t mantissa = std::bit_cast<int64_t>(
        sign | (digits_.back() << (leading_insignificant - 1)));
    const int exponent = exponent_ + 1 - leading_insignificant;
    return {mantissa, exponent, true};
  }

  const int lost_digits = 65 - leading_insignificant;
  const uint64_t lost_digits_mask = (uint64_t{1} << lost_digits) - 1;
  const uint64_t comp2mantissa =
      (digits_.back() << (leading_insignificant - 1)) |
      (digits_[digits_.size() - 2] >> lost_digits);
  const int64_t mantissa = std::bit_cast<int64_t>(sign | comp2mantissa);
  const int exponent = exponent_ + 64 * (digits_.size() - 2) + lost_digits;
  const bool exact = (digits_[digits_.size() - 2] & lost_digits_mask) == 0 &&
                     std::all_of(digits_.begin(), digits_.end() - 2,
                                 [](uint64_t digit) { return digit == 0; });
  return {mantissa, exponent, exact};
}

namespace {

// Returns the double bounds of a real number with the given mantissa and
// exponent. Assumes that
//       mantissa * 2^exponent <= N < (mantissa + 1) * 2^exponent and
// mantissa < -2^62 or mantissa >= 2^62 or mantissa == 0. exact is true if and
// only if the left bound is an equality. This is the output contract of
// ExtractSignificand.
std::pair<double, double> ToDoubleBounds(const int64_t mantissa,
                                         const int exponent, const bool exact) {
  if (mantissa == 0) return {0.0, 0.0};

  DCHECK(mantissa >= int64_t{1} << 62 || mantissa < int64_t{-1} << 62);

  // Compute abs_mantissa and abs_exponent such that
  //       abs_mantissa * 2^abs_exponent
  //    <= abs(represented_number)
  //    <  (abs_mantissa + 1) * 2^abs_exponent and
  // 2^62 <= abs_mantissa < 2^63.
  uint64_t abs_mantissa;
  int abs_exponent = exponent;
  if (mantissa >= 0) {
    abs_mantissa = static_cast<uint64_t>(mantissa);
  } else if (exact) {
    if (mantissa == kint64min) {
      abs_mantissa = uint64_t{1} << 62;
      ++abs_exponent;
    } else {
      abs_mantissa = static_cast<uint64_t>(-mantissa);
    }
  } else {
    abs_mantissa = ~static_cast<uint64_t>(mantissa);
  }

  const int binary_exp = abs_exponent + 62;
  if (binary_exp > 1023) {
    return mantissa < 0 ? std::make_pair(-kInf, -kDoubleMax)
                        : std::make_pair(kDoubleMax, kInf);
  }
  if (binary_exp < -1074) {
    return mantissa < 0 ? std::make_pair(-kDoubleMin, 0.0)
                        : std::make_pair(0.0, kDoubleMin);
  }
  const int sig_bits = std::min(53, binary_exp + 1075);
  const int lost_bits = 63 - sig_bits;
  const uint64_t dbl_mantissa = abs_mantissa >> lost_bits;
  const uint64_t biased_exponent = std::max(binary_exp + 1023, 0);
  const bool dbl_exact =
      exact && (abs_mantissa & ((uint64_t{1} << lost_bits) - 1)) == 0;
  const double mag_lb = ReconstructDouble(dbl_mantissa, biased_exponent);
  const double mag_ub = dbl_exact ? mag_lb : FastStepUp(mag_lb);
  return mantissa < 0 ? std::make_pair(-mag_ub, -mag_lb)
                      : std::make_pair(mag_lb, mag_ub);
}

// Returns the floor and ceil of a real number with the given mantissa and
// exponent. Assumes that
//       mantissa * 2^exponent <= N < (mantissa + 1) * 2^exponent and
// mantissa < -2^62 or mantissa >= 2^62 or mantissa == 0. exact is true if and
// only if the left bound is an equality. This is the output contract of
// ExtractSignificand.
std::pair<std::optional<int64_t>, std::optional<int64_t>> ToFloorCeil(
    const int64_t mantissa, const int exponent, const bool exact) {
  DCHECK(mantissa == 0 || mantissa >= int64_t{1} << 62 ||
         mantissa < int64_t{-1} << 62);
  if (exponent > 0) {
    return mantissa < 0 ? std::make_pair(std::optional<int64_t>{},
                                         std::make_optional(kint64min))
                        : std::make_pair(std::make_optional(kint64max),
                                         std::optional<int64_t>{});
  }
  if (exponent <= -64) {
    return mantissa < 0 ? std::make_pair(-1, 0) : std::make_pair(0, 1);
  }
  const int lost_digits = -exponent;
  const int64_t fl = mantissa >> lost_digits;
  const bool int_exact =
      exact && (mantissa & ((uint64_t{1} << lost_digits) - 1)) == 0;
  return {fl, int_exact         ? fl
              : fl == kint64max ? std::nullopt
                                : std::make_optional(fl + 1)};
}

}  // namespace

void TightSum::UpdateBounds() const {
  // Mantissa and exponent such that
  //       mantissa * 2^exponent <= sum < (mantissa + 1) * 2^exponent and
  // mantissa < -2^62 or mantissa >= 2^62.
  // exact is true if and only if the left bound is an equality.
  bounds_updated_ = true;
  const auto [mantissa, exponent, exact] = ExtractSignificand();
  std::tie(lb_, ub_) = ToDoubleBounds(mantissa, exponent, exact);
  std::tie(floor_, ceil_) = ToFloorCeil(mantissa, exponent, exact);
}

void TightSum::AddTerm(const Term& term) {
  if (term.mantissa == 0) return;

  bounds_updated_ = false;
  if (digits_.empty() && !negative_) {
    // Adds four digits_ to avoid resizing.
    digits_.resize(6, 0);
    exponent_ = term.exponent - 4 * 64;
    // Note that we use two's complement for negative numbers so that we can
    // transfer directly from int128.
    uint64_t* const data = digits_.data();
    data[4] = absl::Int128Low64(term.mantissa);
    data[5] = absl::Int128High64(term.mantissa);
    negative_ = term.mantissa < 0;
    return;
  }

  // Decompose term.exponent = 64 * hi_exp + lo_exp + exponent_. (Do not use
  // div and mod as we do not want the usual C++ rounding, but lo_exp in [0,
  // 63]).
  const int shifted_exp = term.exponent - exponent_;
  const int lo_exp = shifted_exp & 63;
  // Actual floor of the division by 64. See
  // https://en.cppreference.com/cpp/language/operator_arithmetic for negative
  // case.
  int hi_exp = shifted_exp >> 6;
  const uint64_t first_digit = absl::Int128Low64(term.mantissa) << lo_exp;
  absl::int128 next_digits = term.mantissa >> (64 - lo_exp);
  if (hi_exp < 0) {
    // Add the necessary number of digits + 4 to avoid making this too often.
    const int num_digits_to_add = 4 - hi_exp;
    digits_.insert(digits_.begin(), num_digits_to_add, 0);
    hi_exp = 4;
    exponent_ -= num_digits_to_add << 6;
  }
  if (hi_exp + 1 >= digits_.size()) {
    digits_.resize(hi_exp + 2, negative_ ? kuint64max : 0);
  }

  uint64_t* const data = digits_.data();

  data[hi_exp] += first_digit;
  if (data[hi_exp] < first_digit) ++next_digits;
  if (next_digits == 0) return;

  ++hi_exp;
  const uint64_t second_digit = absl::Int128Low64(next_digits);
  int64_t rem = absl::Int128High64(next_digits);
  data[hi_exp] += second_digit;
  if (data[hi_exp] < second_digit) ++rem;
  if (rem == 0) return;

  ++hi_exp;
  const int size = digits_.size();
  const bool neg_added = term.mantissa < 0;
  if (hi_exp < size) {
    data[hi_exp] += rem;
    if (neg_added) {
      if (data[hi_exp] < static_cast<uint64_t>(rem)) return;

      rem = -1;
      while (++hi_exp < size) {
        --data[hi_exp];
        if (data[hi_exp] != kuint64max) return;
      }
    } else {
      if (data[hi_exp] >= rem) return;

      rem = 1;
      while (++hi_exp < size) {
        ++data[hi_exp];
        if (data[hi_exp] > 0) return;
      }
    }
  }
  // Now, we are at the end of digits_ with a non-zero carry.
  if (negative_) {
    // Takes into account the term -2^(64 * digits_.size()).
    --rem;
  }
  if (rem != 0) digits_.push_back(rem);
  negative_ = neg_added;
}

std::pair<double, double> ProdBounds(const int64_t a, const int64_t b,
                                     const PowerOfTwo p) {
  return TermDoubleBounds(ProdAsTerm(a, b, p));
}

std::pair<double, double> ProdBounds(const int64_t a, const double b,
                                     const PowerOfTwo p) {
  return TermDoubleBounds(ProdAsTerm(a, b, p));
}

std::pair<double, double> ProdBounds(const double a, const int64_t b,
                                     const PowerOfTwo p) {
  return ProdBounds(b, a, p);
}

std::pair<double, double> ProdBounds(const double a, const double b,
                                     const PowerOfTwo p) {
  return TermDoubleBounds(ProdAsTerm(a, b, p));
}

std::pair<std::optional<int64_t>, std::optional<int64_t>> ProdFloorCeil(
    int64_t a, int64_t b, PowerOfTwo p) {
  return TermFloorCeil(ProdAsTerm(a, b, p));
}

std::pair<std::optional<int64_t>, std::optional<int64_t>> ProdFloorCeil(
    int64_t a, double b, PowerOfTwo p) {
  return TermFloorCeil(ProdAsTerm(a, b, p));
}

std::pair<std::optional<int64_t>, std::optional<int64_t>> ProdFloorCeil(
    double a, int64_t b, PowerOfTwo p) {
  return TermFloorCeil(ProdAsTerm(a, b, p));
}

std::pair<std::optional<int64_t>, std::optional<int64_t>> ProdFloorCeil(
    double a, double b, PowerOfTwo p) {
  return TermFloorCeil(ProdAsTerm(a, b, p));
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
  const double lb = ReconstructDouble(abs_n >> lost_digits, 1075 + lost_digits);
  const double ub =
      (abs_n & ((uint64_t{1} << lost_digits) - 1)) == 0 ? lb : FastStepUp(lb);
  if (n < 0) {
    return {-ub, -lb};
  } else {
    return {lb, ub};
  }
}

}  // namespace operations_research
