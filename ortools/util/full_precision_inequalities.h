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

#ifndef ORTOOLS_UTIL_FULL_PRECISION_INEQUALITIES_H_
#define ORTOOLS_UTIL_FULL_PRECISION_INEQUALITIES_H_

#include <algorithm>
#include <bit>
#include <cmath>
#include <compare>
#include <cstdint>
#include <limits>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/casts.h"
#include "absl/container/inlined_vector.h"
#include "absl/log/check.h"
#include "absl/numeric/int128.h"
#include "absl/types/span.h"
#include "ortools/port/attributes.h"

namespace operations_research {

// TODO(user): Expose these functions in an appropriate place.

// Function that is equivalent to std::nextafter(v, kInf), but significantly
// faster since it doesn't need to set or check CPU flags.
constexpr double FastStepUp(double v);

// Function that is equivalent to std::nextafter(v, -kInf), but significantly
// faster since it doesn't need to set or check CPU flags.
constexpr double FastStepDown(double v);

// Returns `e` such that `2^e <= |v| < 2^(e + 1)`.
// For v = 0, returns `min_int`. For inf, returns `max_int`.
constexpr int FastILogB(double v);

// Returns `2^exponent` if representable, otherwise 0 or infinity (depending on
// the sign of the exponent).
constexpr double FastTwoExp(int exponent);

// Returns the closest double to `v * 2^exponent` (infinity if overflow).
// It is truncated if the result is subnormal.
constexpr double FastLdExp(double v, int exponent);

// Returns true iff `v` is a finite positive exact power of two `2^e`.
inline bool IsPowerOfTwo(double v);

// Returns the odd integer significand `q` in `[1, 2^53 - 1]` such that
// `v = q * 2^k` for some integer `k`. Requires finite `v > 0`.
inline uint64_t OddSignificand(double v);

// Returns the highest exponent `e` such that `2^e <= value` (`int::min()` for
// `value == 0`, and `int::max()` for `+infinity`). Requires `value >= 0`.
inline int HighestPowerOfTwoAtOrBelow(double value);

// Returns the lowest exponent `e` such that `value <= 2^e` (`int::min()` for
// `value == 0`, and `int::max()` for `+infinity`). Requires `value >= 0`.
inline int LowestPowerOfTwoAtOrAbove(double value);

// Returns the largest exponent `e` such that `magnitude * 2^e <= max_bound` in
// R (`int::min()` for `max_bound <= 0`, and `int::max()` for `max_bound ==
// +infinity`). Requires finite `magnitude > 0`.
inline int MaxPowerOfTwoExponentWithinBound(double magnitude, double max_bound);

// Returns `value` rounded to the nearest multiple of `2^precision_exponent`.
constexpr double RoundToPrecision(double v, int precision_exponent);

// Returns `value` rounded to the multiple of `2^precision_exponent` closest to
// 0.
constexpr double TruncToPrecision(double v, int precision_exponent);

// Returns `value` rounded to the multiple of `2^precision_exponent` furthest
// from 0.
constexpr double RoundAwayFromZeroToPrecision(double v, int precision_exponent);

// Returns `value` rounded to the previous multiple of `2^precision_exponent`.
constexpr double FloorToPrecision(double v, int precision_exponent);

// Returns `value` rounded to the next multiple of `2^precision_exponent`.
constexpr double CeilToPrecision(double v, int precision_exponent);

// Represents a power of two, i.e. 2^exponent. Used for the API of TightSum.
// Supports between 2^-15 and 2^15 (the reason is to avoid overflows in the
// exponent).
struct PowerOfTwo {
  PowerOfTwo(int exponent);
  int exponent;
};

namespace internal_full_precision_inequalities {
class InternalTightSum;
}  // namespace internal_full_precision_inequalities

// Represents a sum of terms with full precision. The sum is not necessarily
// representable as a double. This is not thread-safe, but is thread-compatible.
//
// * The amortized complexity of adding a term is:
//  O(1 + exp_spread / num_terms)
//  where exp_spread = log(abs(max_nonzero_term)) - log(abs(min_nonzero_term)).
// * The complexity of getting the bounds is O(1).
// So if one adds N terms and gets the bounds no more than O(N) times, the
// complexity is O(N + exp_spread).
class TightSum {
 public:
  // Explicit constructor. It is fundamental to have it as it prevents to
  // zero-initialize the object via memset, saving a huge time for very small
  // computations. It saves ~40% of CPU time in
  // `BM_ProdBoundsViaTightSum<double, double>`.
  // See e.g. go/totw/146#user-declared-vs-user-provided-constructors. See also
  //  cs/search?q=getMaxStoresPerMemset%20file:TargetLowering%5C.h
  // that explains, why, depending on the alignment of the objects (here
  // absl::int128), so its size, memset is not inlined anymore adding a
  // significant cost.
  TightSum() {};

  TightSum(const TightSum& other) = default;
  TightSum& operator=(const TightSum& other) = default;

  void Clear();

  TightSum& Negate();

  // Adds the product of the given arguments to the sum. arguments can be of
  // type double, int64_t, or PowerOfTwo. Returns itself for chaining.
  // Two versions to simplify the implementation.
  template <typename Arg>
    requires(!std::is_same_v<Arg, TightSum>)
  TightSum& Add(Arg arg);

  TightSum& Add(const TightSum& other) { return AddProd(other); }

  template <typename Arg1, typename Arg2, typename... OtherArgs>
    requires(!std::is_same_v<Arg1, TightSum>)
  TightSum& AddProd(Arg1 arg1, Arg2 arg2, OtherArgs... other_args);

  TightSum& AddProd(const TightSum& other, bool negate = false,
                    PowerOfTwo scaling = PowerOfTwo(0));

  friend std::strong_ordering operator<=>(const TightSum& a, const double x) {
    return a.CompareTo(x);
  }

  friend std::strong_ordering operator<=>(const double x, const TightSum& a) {
    // 0 <=> ? defined as opposite for std::strong_ordering.
    return 0 <=> a.CompareTo(x);
  }

  friend bool operator==(const TightSum& a, const double x) {
    return a.CompareTo(x) == std::strong_ordering::equal;
  }

  friend std::strong_ordering operator<=>(const TightSum& a, const int64_t x) {
    return a.CompareTo(x);
  }

  friend std::strong_ordering operator<=>(const int64_t x, const TightSum& a) {
    // 0 <=> ? defined as opposite for std::strong_ordering.
    return 0 <=> a.CompareTo(x);
  }

  friend bool operator==(const TightSum& a, const int64_t x) {
    return a.CompareTo(x) == std::strong_ordering::equal;
  }

  friend std::strong_ordering operator<=>(const TightSum& a, const int x) {
    return a.CompareTo(int64_t{x});
  }

  friend std::strong_ordering operator<=>(const int x, const TightSum& a) {
    return 0 <=> a.CompareTo(int64_t{x});
  }

  friend bool operator==(const TightSum& a, const int x) {
    return a.CompareTo(int64_t{x}) == std::strong_ordering::equal;
  }

  // Returns true if no infinite terms have been added to the sum.
  bool IsFinite() const { return inf_sign_ == std::strong_ordering::equal; }

  // Returns currently known bounds on the sum.
  ABSL_DEPRECATED("Use TightBounds() instead.")
  std::pair<double, double> LooseBounds() const { return TightBounds(); }

  ABSL_DEPRECATED("Use TightLowerBound() instead.")
  double LooseLowerBound() const { return TightLowerBound(); }

  ABSL_DEPRECATED("Use TightUpperBound() instead.")
  double LooseUpperBound() const { return TightUpperBound(); }

  // Returns the tight bounds of the sum. In other words, returns [lb, ub]
  // such that lb <= sum <= ub and
  //  * if sum is representable as a double, then lb == ub == sum;
  //  * otherwise, ub = nextafter(lb, inf).
  std::pair<double, double> TightBounds() const {
    if (!bounds_updated_) UpdateBounds();

    return {lb_, ub_};
  }

  double TightLowerBound() const { return TightBounds().first; }

  double TightUpperBound() const { return TightBounds().second; }

  // Returns {floor(sum), ceil(sum)}.
  // * If sum > std::numeric_limits<int64_t>::max(), returns
  //   {std::numeric_limits<int64_t>::max(), nullopt}.
  // * If sum < std::numeric_limits<int64_t>::min(), returns
  //   {nullopt, std::numeric_limits<int64_t>::min()}.
  std::pair<std::optional<int64_t>, std::optional<int64_t>> Int64Bounds()
      const {
    if (!bounds_updated_) UpdateBounds();

    return {floor_, ceil_};
  }

  std::optional<int64_t> Int64Floor() const { return Int64Bounds().first; }

  std::optional<int64_t> Int64Ceil() const { return Int64Bounds().second; }

 private:
  // Internal representation of a number.
  // Note that a term needs not to be canonical: it is valid to represent 4.0 as
  // 1.0 * 2^2 or 2.0 * 2^1 or 4.0 * 2^0.
  struct Term {
    absl::int128 mantissa ORTOOLS_REQUIRE_EXPLICIT_INIT;
    int exponent ABSL_REQUIRE_EXPLICIT_INIT;
  };

  static Term ToTerm(double x);
  static Term ToTerm(float x);
  static Term ToTerm(int64_t n);
  static Term ToTerm(int n);
  static Term ToTerm(PowerOfTwo p);

  // Exposes some internal details to the implementation.
  friend class internal_full_precision_inequalities::InternalTightSum;

  // Returns the comparison between the sum and x. Formally sum <=> x where <=>
  // has the semantics of the three way operator. NaN are not handled.
  std::strong_ordering CompareTo(double x) const;

  std::strong_ordering CompareTo(int64_t x) const;

  // Returns mantissa, exponent and exact such that
  //       mantissa * 2^exponent <= sum < (mantissa + 1) * 2^exponent and
  // mantissa < -2^62 or mantissa >= 2^62.
  // exact is true if and only if the left bound is an equality.
  std::tuple<int64_t, int, bool> ExtractSignificand() const;

  void UpdateBounds() const;

  // Adds the given term to the sum.
  void AddTerm(const Term& term);

  // Replaces TightSum by infinity if the incoming product is infinite. Crashes
  // if it would be NaN.
  template <typename Arg1, typename... OtherArgs>
  void SetInfIfNeeded(Arg1 arg1, OtherArgs... other_args);

  // Adds the product of term1 and the elements in factors and removes them from
  // factors_.
  void MultiplyTermsAndAdd(Term term1);

  // The three fields below are used to store the sum in base 2^64. We use two
  // complement encoding for negative numbers. More precisely, the sum is equal
  // to:
  //   sum_(i = 0..digits_.size() - 1) digits_[i] * 2^(exponent_ + 64 * i)
  //       - negative_ * 2^(exponent_ + 64 * digits_.size())
  mutable bool negative_ = false;

  mutable int exponent_ = 0;

  mutable absl::InlinedVector<uint64_t, 32> digits_;

  // Stores the factors to multiply the last term of the sum.
  absl::InlinedVector<Term, 6> factors_;

  // If the sum is infinite, this is the sign. Once the sum is infinite, it
  // cannot change (it crashes if signs are not consistent). equal means that
  // the sum is finite.
  std::strong_ordering inf_sign_ = std::strong_ordering::equal;

  // Known bounds on the sum.
  mutable double lb_ = 0.0;
  mutable double ub_ = 0.0;

  mutable std::optional<int64_t> floor_ = 0;
  mutable std::optional<int64_t> ceil_ = 0;

  // Whether the bounds are tight.
  mutable bool bounds_updated_ = true;
};

// Returns the sum of a vector of doubles or int64_t.
template <typename Number>
TightSum TightSumOf(absl::Span<const Number> v);

// Returns the dot product of two vectors of double or int64_t.
template <typename Number1, typename Number2>
TightSum TightDotProduct(absl::Span<const Number1> v,
                         absl::Span<const Number2> w);

// For any combination of int64_t, non-NaN double and PowerOfTwo (excluding 0
// with +-inf), ProdBounds(a, b, p) returns a tight pair of bounds [lb, ub] such
// that lb <= a * b * p <= ub in R (or {+-inf, +-inf} if an operand is
// infinite).
std::pair<double, double> ProdBounds(int64_t a, int64_t b,
                                     PowerOfTwo p = PowerOfTwo(0));
std::pair<double, double> ProdBounds(int64_t a, double b,
                                     PowerOfTwo p = PowerOfTwo(0));
std::pair<double, double> ProdBounds(double a, int64_t b,
                                     PowerOfTwo p = PowerOfTwo(0));
std::pair<double, double> ProdBounds(double a, double b,
                                     PowerOfTwo p = PowerOfTwo(0));

// For any combination of int64_t, finite double and PowerOfTwo,
// ProdFloorCeil(a, b, p) returns Floor(a * b * p) and Ceil(a * b * p) as
// int64_t. If the result is not representable as int64_t, returns {nullopt,
// int64min} or {int64max, nullopt} depending on the sign.
std::pair<std::optional<int64_t>, std::optional<int64_t>> ProdFloorCeil(
    int64_t a, int64_t b, PowerOfTwo p = PowerOfTwo(0));
std::pair<std::optional<int64_t>, std::optional<int64_t>> ProdFloorCeil(
    int64_t a, double b, PowerOfTwo p = PowerOfTwo(0));
std::pair<std::optional<int64_t>, std::optional<int64_t>> ProdFloorCeil(
    double a, int64_t b, PowerOfTwo p = PowerOfTwo(0));
std::pair<std::optional<int64_t>, std::optional<int64_t>> ProdFloorCeil(
    double a, double b, PowerOfTwo p = PowerOfTwo(0));

// For any combination of int64_t and non-NaN double (excluding +inf with -inf),
// SumBounds(a, b) returns a tight pair of bounds [lb, ub] such that
// lb <= a + b <= ub in R (or {+-inf, +-inf} if an operand is infinite).
std::pair<double, double> SumBounds(int64_t a, int64_t b);
std::pair<double, double> SumBounds(int64_t a, double b);
std::pair<double, double> SumBounds(double a, int64_t b);
std::pair<double, double> SumBounds(double a, double b);

// For any combination of int64_t, finite double, SumFloorCeil(a, b) returns
// Floor(a + b) and Ceil(a + b) as int64_t. If the result is not representable
// as int64_t, returns {nullopt, int64min} or {int64max, nullopt} depending on
// the sign.
std::pair<std::optional<int64_t>, std::optional<int64_t>> SumFloorCeil(
    int64_t a, int64_t b);
std::pair<std::optional<int64_t>, std::optional<int64_t>> SumFloorCeil(
    int64_t a, double b);
std::pair<std::optional<int64_t>, std::optional<int64_t>> SumFloorCeil(
    double a, int64_t b);
std::pair<std::optional<int64_t>, std::optional<int64_t>> SumFloorCeil(
    double a, double b);

// Returns a tight double interval containing n.
std::pair<double, double> Int64DoubleBounds(int64_t n);

// Tight bounds [lb, ub] containing the exact rational quotient `a / b` for a
// non-NaN `a` and a finite `b != 0` (`{q, q}` when `q = a / b` is exact, and a
// 1-ulp interval otherwise). A quotient that is too large to be representable
// gives `{max_double, inf}` (or `{-inf, -max_double}`). If `a` is infinite,
// returns `{a, a}` or `{-a, -a}` depending on the sign of `b`.
std::pair<double, double> QuotientBounds(double a, double b);

// Outward-rounded bounds for sums, products, and quotients where operands may
// be infinite.
//
// `ExactScaledSumLb` and `ExactScaledSumUb` compute the outward-rounded bound
// on `x` implied by `a + b <= divisor * x` and `divisor * x <= a + b`,
// respectively: when `divisor < 0.0`, dividing reverses the inequality so the
// two functions delegate to each other on `(-a, -b, -divisor)` (producing an
// upper bound on `(a + b) / divisor` for `ExactScaledSumLb`, and a lower bound
// for `ExactScaledSumUb`).
inline double ExactSumLb(double a, double b);
inline double ExactSumUb(double a, double b);
inline double ExactScaledSumLb(double a, double b, double divisor);
inline double ExactScaledSumUb(double a, double b, double divisor);
inline double ExactProdLb(double a, double b);
inline double ExactProdUb(double a, double b);
inline double ExactQuotientLb(double a, double b);
inline double ExactQuotientUb(double a, double b);
inline double MaxAbsScaledTightSum(const TightSum& sum,
                                   double pos_divisor = 1.0);

// Scales an interval `[lb, ub]` by multiplying by a finite `factor` or dividing
// by a finite non-zero `divisor`, flipping the endpoints when the scalar is
// negative and rounding finite bounds outward (`[0.0, 0.0]` when `factor == 0`,
// even for infinite endpoints) so the returned interval encloses every exact
// `x * factor` or `x / divisor` for `x` in `[lb, ub]`.
inline std::pair<double, double> RelaxedMulInterval(double lb, double ub,
                                                    double factor);
inline std::pair<double, double> RelaxedDivInterval(double lb, double ub,
                                                    double divisor);

// Tight outward-rounded bounds `[lb, ub]` on `a * b` for `a` in `[a_lb, a_ub]`
// and `b` in `[b_lb, b_ub]` (treating `0 * +-inf` as `0.0`, as in
// `RelaxedMulInterval`, so `[0.0, 0.0]` multiplied by an unbounded interval
// yields `[0.0, 0.0]`). Requires non-NaN bounds with `a_lb <= a_ub` and
// `b_lb <= b_ub`.
inline std::pair<double, double> IntervalProdBounds(double a_lb, double a_ub,
                                                    double b_lb, double b_ub);

constexpr double FastStepUp(const double v) {
  static_assert(std::numeric_limits<double>::is_iec559);
  static_assert(std::numeric_limits<double>::radix == 2);
  static_assert(sizeof(double) == sizeof(uint64_t), "double must be 64 bits.");

  DCHECK(!std::isnan(v));

  constexpr int64_t kInfBits =
      absl::bit_cast<int64_t>(std::numeric_limits<double>::infinity());
  constexpr int64_t kNegZeroBits = absl::bit_cast<int64_t>(-0.0);
  int64_t i = absl::bit_cast<int64_t>(v);
  if (i == kNegZeroBits) i = 0;
  i += (i != kInfBits) ? ((i >= 0) ? 1 : -1) : 0;
  return absl::bit_cast<double>(i);
}

constexpr double FastStepDown(const double v) {
  static_assert(std::numeric_limits<double>::is_iec559);
  static_assert(std::numeric_limits<double>::radix == 2);
  static_assert(sizeof(double) == sizeof(uint64_t), "double must be 64 bits.");

  DCHECK(!std::isnan(v));

  constexpr int64_t kNegInfBits =
      absl::bit_cast<int64_t>(-std::numeric_limits<double>::infinity());
  constexpr int64_t kNegZeroBits = absl::bit_cast<int64_t>(-0.0);
  int64_t i = absl::bit_cast<int64_t>(v);
  if (i == 0) i = kNegZeroBits;
  i += (i != kNegInfBits) ? ((i >= 0) ? -1 : 1) : 0;
  return absl::bit_cast<double>(i);
}

constexpr int FastILogB(const double v) {
  static_assert(std::numeric_limits<double>::is_iec559);
  static_assert(std::numeric_limits<double>::radix == 2);
  static_assert(sizeof(double) == sizeof(uint64_t), "double must be 64 bits.");
  DCHECK(!std::isnan(v));

  const uint64_t i = absl::bit_cast<uint64_t>(v);
  const int biased_exp = (i >> 52) & 0x7ff;
  // inf or NaN.
  if (biased_exp == 2047) return std::numeric_limits<int>::max();
  if (biased_exp > 0) return biased_exp - 1023;
  const uint64_t mantissa = i & ((int64_t{1} << 52) - 1);
  // 0.
  if (mantissa == 0) return std::numeric_limits<int>::min();
  return -1011 - std::countl_zero(mantissa);
}

constexpr double FastTwoExp(const int exponent) {
  static_assert(std::numeric_limits<double>::is_iec559);
  static_assert(std::numeric_limits<double>::radix == 2);
  static_assert(sizeof(double) == sizeof(uint64_t), "double must be 64 bits.");

  if (exponent > 1023) return std::numeric_limits<double>::infinity();
  if (exponent >= -1022) {
    return absl::bit_cast<double>(static_cast<uint64_t>(exponent + 1023) << 52);
  }
  if (exponent >= -1074) {
    return absl::bit_cast<double>(uint64_t{1} << (exponent + 1074));
  }
  return 0.0;
}

constexpr double FastLdExp(const double v, const int exponent) {
  static_assert(std::numeric_limits<double>::is_iec559);
  static_assert(std::numeric_limits<double>::radix == 2);
  static_assert(sizeof(double) == sizeof(uint64_t), "double must be 64 bits.");
  constexpr double kInf = std::numeric_limits<double>::infinity();
  constexpr uint64_t kMantissaMask = (int64_t{1} << 52) - 1;
  DCHECK(!std::isnan(v));

  if (v == 0.0) return 0.0;
  if (!std::isfinite(v)) return v;

  const uint64_t i = absl::bit_cast<uint64_t>(v);
  const int64_t biased_exp = (i >> 52) & 0x7ff;
  int64_t new_biased_exp = biased_exp + exponent;
  if (new_biased_exp <= -52) return 0.0;
  if (biased_exp > 0) {
    if (new_biased_exp >= 2047) return v < 0 ? -kInf : kInf;
    if (new_biased_exp > 0) {
      return absl::bit_cast<double>(i +
                                    (static_cast<uint64_t>(exponent) << 52));
    }
    const uint64_t mantissa = (i & kMantissaMask) | (uint64_t{1} << 52);
    return absl::bit_cast<double>((mantissa >> (1 - new_biased_exp)) |
                                  (i & (uint64_t{1} << 63)));
  }
  // biased_exp == 0.
  uint64_t mantissa = i & kMantissaMask;
  if (exponent < 0) {
    const double res = absl::bit_cast<double>((mantissa >> -exponent) |
                                              (i & (uint64_t{1} << 63)));
    return res == -0.0 ? 0.0 : res;
  }
  const int s = std::countl_zero(mantissa) - 11;
  if (exponent < s) {
    return absl::bit_cast<double>((mantissa << exponent) |
                                  (i & (uint64_t{1} << 63)));
  }
  mantissa <<= s;
  new_biased_exp -= s - 1;  // new_biased_exp = exponent - (s - 1) > 0.
  if (new_biased_exp >= 2047) return v < 0 ? -kInf : kInf;
  return absl::bit_cast<double>((mantissa & kMantissaMask) |
                                (new_biased_exp << 52) |
                                (i & (uint64_t{1} << 63)));
}

inline bool IsPowerOfTwo(const double v) {
  return v > 0.0 && std::isfinite(v) && FastTwoExp(FastILogB(v)) == v;
}

inline uint64_t OddSignificand(const double v) {
  static_assert(std::numeric_limits<double>::is_iec559);
  static_assert(std::numeric_limits<double>::radix == 2);
  static_assert(sizeof(double) == sizeof(uint64_t), "double must be 64 bits.");
  DCHECK_GT(v, 0.0);
  DCHECK(std::isfinite(v));

  const uint64_t bits = absl::bit_cast<uint64_t>(v);
  const uint64_t biased_exp = (bits >> 52) & 0x7ff;
  const uint64_t sig = (bits & ((uint64_t{1} << 52) - 1)) |
                       (biased_exp > 0 ? (uint64_t{1} << 52) : 0);
  return sig >> std::countr_zero(sig);
}

inline int HighestPowerOfTwoAtOrBelow(const double value) {
  CHECK_GE(value, 0.0);
  return FastILogB(value);
}

inline int LowestPowerOfTwoAtOrAbove(const double value) {
  CHECK_GE(value, 0.0);
  if (value == 0.0) return std::numeric_limits<int>::min();
  if (!std::isfinite(value)) return std::numeric_limits<int>::max();
  const int e = FastILogB(value);
  return FastTwoExp(e) == value ? e : e + 1;
}

inline int MaxPowerOfTwoExponentWithinBound(const double magnitude,
                                            const double max_bound) {
  DCHECK_GT(magnitude, 0.0);
  DCHECK(std::isfinite(magnitude));
  if (max_bound <= 0.0) return std::numeric_limits<int>::min();
  if (!std::isfinite(max_bound)) return std::numeric_limits<int>::max();
  const int e_mag = FastILogB(magnitude);
  const int e_bnd = FastILogB(max_bound);
  const int exp = e_bnd - e_mag;
  return FastLdExp(magnitude, -e_mag) <= FastLdExp(max_bound, -e_bnd) ? exp
                                                                      : exp - 1;
}

constexpr double RoundToPrecision(const double v,
                                  const int precision_exponent) {
  static_assert(std::numeric_limits<double>::is_iec559);
  static_assert(std::numeric_limits<double>::radix == 2);
  static_assert(sizeof(double) == sizeof(uint64_t), "double must be 64 bits.");
  DCHECK(!std::isnan(v));

  if (v == 0.0) return 0.0;
  if (!std::isfinite(v)) return v;

  const uint64_t i = absl::bit_cast<uint64_t>(v);
  int64_t exp = ((i >> 52) & 0x7ff) - 1023;
  const bool subnormal = exp == -1023;
  if (subnormal) exp = -1022;
  if (exp - 52 >= precision_exponent) return v;
  if (exp + 1 < precision_exponent) return 0.0;
  if (exp + 1 == precision_exponent) {
    return subnormal ? 0.0
           : v < 0   ? -FastTwoExp(precision_exponent)
                     : FastTwoExp(precision_exponent);
  }
  const int first_lost_position = 51 + precision_exponent - exp;
  const bool first_lost_bit = (i & (uint64_t{1} << first_lost_position)) != 0;
  const uint64_t truncated = i & (~uint64_t{0} << (first_lost_position + 1));
  // Note that the special case where we leak into the exponent works as well.
  const double res =
      first_lost_bit
          ? absl::bit_cast<double>(truncated +
                                   (uint64_t{1} << (first_lost_position + 1)))
          : absl::bit_cast<double>(truncated);
  return res == -0.0 ? 0.0 : res;
}

constexpr double TruncToPrecision(const double v,
                                  const int precision_exponent) {
  static_assert(std::numeric_limits<double>::is_iec559);
  static_assert(std::numeric_limits<double>::radix == 2);
  static_assert(sizeof(double) == sizeof(uint64_t), "double must be 64 bits.");
  DCHECK(!std::isnan(v));

  if (v == 0.0) return 0.0;
  if (!std::isfinite(v)) return v;

  const uint64_t i = absl::bit_cast<uint64_t>(v);
  int64_t exp = ((i >> 52) & 0x7ff) - 1023;
  if (exp == -1023) exp = -1022;
  if (exp - 52 >= precision_exponent) return v;
  if (exp + 1 <= precision_exponent) return 0.0;
  const int first_kept_position = 52 + precision_exponent - exp;
  const double res =
      absl::bit_cast<double>(i & (~uint64_t{0} << first_kept_position));
  return res == -0.0 ? 0.0 : res;
}

constexpr double RoundAwayFromZeroToPrecision(const double v,
                                              const int precision_exponent) {
  static_assert(std::numeric_limits<double>::is_iec559);
  static_assert(std::numeric_limits<double>::radix == 2);
  static_assert(sizeof(double) == sizeof(uint64_t), "double must be 64 bits.");
  DCHECK(!std::isnan(v));

  if (v == 0.0) return 0.0;
  if (!std::isfinite(v)) return v;

  const uint64_t i = absl::bit_cast<uint64_t>(v);
  int64_t exp = ((i >> 52) & 0x7ff) - 1023;
  if (exp == -1023) exp = -1022;
  if (exp - 52 >= precision_exponent) return v;
  if (exp + 1 <= precision_exponent) {
    return v < 0 ? -FastTwoExp(precision_exponent)
                 : FastTwoExp(precision_exponent);
  }
  const int first_kept_position = 52 + precision_exponent - exp;
  const uint64_t lost_bits = (i & ((uint64_t{1} << first_kept_position) - 1));
  const uint64_t truncated = i & (~uint64_t{0} << (first_kept_position));
  // Note that the special case where we leak into the exponent works as well.
  const double res =
      lost_bits != 0 ? absl::bit_cast<double>(
                           truncated + (uint64_t{1} << (first_kept_position)))
                     : absl::bit_cast<double>(truncated);
  return res == -0.0 ? 0.0 : res;
}

constexpr double FloorToPrecision(const double v,
                                  const int precision_exponent) {
  return v < 0 ? RoundAwayFromZeroToPrecision(v, precision_exponent)
               : TruncToPrecision(v, precision_exponent);
}

constexpr double CeilToPrecision(const double v, const int precision_exponent) {
  return v > 0 ? RoundAwayFromZeroToPrecision(v, precision_exponent)
               : TruncToPrecision(v, precision_exponent);
}

inline PowerOfTwo::PowerOfTwo(const int exponent) : exponent(exponent) {
  DCHECK_LE(exponent, 1 << 15);
  DCHECK_GE(exponent, -(1 << 15));
}

inline TightSum::Term TightSum::ToTerm(const double x) {
  static_assert(std::numeric_limits<double>::is_iec559);
  static_assert(std::numeric_limits<double>::radix == 2);
  static_assert(sizeof(double) == sizeof(uint64_t), "double must be 64 bits.");

  DCHECK(!std::isnan(x));
  const int64_t bits = absl::bit_cast<int64_t>(x);
  const bool negative = bits < 0;
  int biased_exp = (bits >> 52) & 0x7ff;
  int64_t mantissa = bits & ((int64_t{1} << 52) - 1);
  if (biased_exp > 0) {
    mantissa |= int64_t{1} << 52;
  } else {
    biased_exp = 1;
  }
  return {.mantissa = negative ? -mantissa : mantissa,
          .exponent = biased_exp - 1075};
}

inline TightSum::Term TightSum::ToTerm(const float x) {
  return ToTerm(static_cast<double>(x));
}

inline TightSum::Term TightSum::ToTerm(const int64_t n) {
  return {.mantissa = n, .exponent = 0};
}

inline TightSum::Term TightSum::ToTerm(const int n) {
  return ToTerm(static_cast<int64_t>(n));
}

inline TightSum::Term TightSum::ToTerm(const PowerOfTwo p) {
  return {.mantissa = 1, .exponent = p.exponent};
}

inline void TightSum::Clear() {
  negative_ = false;
  digits_.clear();
  inf_sign_ = std::strong_ordering::equal;
  lb_ = 0.0;
  ub_ = 0.0;
  floor_ = 0;
  ceil_ = 0;
  bounds_updated_ = true;
}

template <typename Arg>
  requires(!std::is_same_v<Arg, TightSum>)
TightSum& TightSum::Add(Arg arg) {
  SetInfIfNeeded(arg);
  if (inf_sign_ != std::strong_ordering::equal) return *this;

  AddTerm(ToTerm(arg));
  return *this;
}

template <typename Arg1, typename Arg2, typename... OtherArgs>
  requires(!std::is_same_v<Arg1, TightSum>)
TightSum& TightSum::AddProd(Arg1 arg1, Arg2 arg2, OtherArgs... other_args) {
  SetInfIfNeeded(arg1, arg2, other_args...);
  if (inf_sign_ != std::strong_ordering::equal) return *this;

  Term term1 = ToTerm(arg1);
  // No overflow possible here.
  const Term term2 = ToTerm(arg2);
  term1.mantissa *= term2.mantissa;
  term1.exponent += term2.exponent;
  if constexpr (sizeof...(other_args) == 0) {
    AddTerm(term1);
    return *this;
  }

  (factors_.push_back(ToTerm(other_args)), ...);
  MultiplyTermsAndAdd(term1);
  return *this;
}

template <typename Arg1, typename... OtherArgs>
void TightSum::SetInfIfNeeded(Arg1 arg1, OtherArgs... other_args) {
  auto isnan = [](auto arg) {
    if constexpr (std::is_floating_point_v<decltype(arg)>) {
      return std::isnan(arg);
    }
    return false;
  };
  CHECK(!isnan(arg1) && (!isnan(other_args) && ...)) << "NaN not supported";

  auto isfinite = [](auto arg) {
    if constexpr (std::is_floating_point_v<decltype(arg)>) {
      return std::isfinite(arg);
    }
    return true;
  };
  if (isfinite(arg1) && (isfinite(other_args) && ...)) {
    // No factor is infinite so the sum does not change.
    return;
  }
  auto neg = [](auto arg) {
    if constexpr (!std::is_same_v<decltype(arg), PowerOfTwo>) {
      CHECK_NE(arg, 0) << "inf * 0 would give NaN";
      return arg < 0;
    } else {
      return false;
    }
  };
  bool prod_neg;
  // Needs to separate "constexpr"-ly as the compiler does not support empty ^.
  if constexpr (sizeof...(other_args) == 0) {
    prod_neg = neg(arg1);
  } else {
    prod_neg = neg(arg1) ^ (neg(other_args) ^ ...);
  }
  const std::strong_ordering prod_sign =
      prod_neg ? std::strong_ordering::less : std::strong_ordering::greater;
  if (inf_sign_ == std::strong_ordering::equal) {
    inf_sign_ = prod_sign;
    // Clear the sum to represent it as inf.
    digits_.clear();
    // Fix once and for all the bounds.
    bounds_updated_ = true;
    constexpr double kInf = std::numeric_limits<double>::infinity();
    if (inf_sign_ == std::strong_ordering::less) {
      lb_ = -kInf;
      ub_ = -kInf;
      floor_ = std::nullopt;
      ceil_ = std::numeric_limits<int64_t>::min();
    } else {
      lb_ = kInf;
      ub_ = kInf;
      floor_ = std::numeric_limits<int64_t>::max();
      ceil_ = std::nullopt;
    }
  } else {
    CHECK_EQ(inf_sign_, prod_sign) << "Inconsistent infinite values in sum";
  }
}

inline std::strong_ordering TightSum::CompareTo(const double x) const {
  if (!bounds_updated_) UpdateBounds();

  if (ub_ == x && lb_ == x) return std::strong_ordering::equal;
  if (ub_ <= x) return std::strong_ordering::less;
  return std::strong_ordering::greater;
}

inline std::strong_ordering TightSum::CompareTo(const int64_t x) const {
  if (!bounds_updated_) UpdateBounds();

  if (ceil_ == x && floor_ == x) return std::strong_ordering::equal;
  if (floor_ < x) return std::strong_ordering::less;
  return std::strong_ordering::greater;
}

template <typename Number>
TightSum TightSumOf(const absl::Span<const Number> v) {
  TightSum res;
  for (const Number& n : v) {
    res.Add(n);
  }
  return res;
}

// Returns the dot product of two vectors of double or int64_t.
template <typename Number1, typename Number2>
TightSum TightDotProduct(absl::Span<const Number1> v,
                         absl::Span<const Number2> w) {
  CHECK_EQ(v.size(), w.size());
  TightSum res;
  for (int i = 0; i < v.size(); ++i) {
    res.AddProd(v[i], w[i]);
  }
  return res;
}

inline double ExactSumLb(const double a, const double b) {
  return SumBounds(a, b).first;
}

inline double ExactSumUb(const double a, const double b) {
  return SumBounds(a, b).second;
}

inline double ExactProdLb(const double a, const double b) {
  return ProdBounds(a, b).first;
}

inline double ExactProdUb(const double a, const double b) {
  return ProdBounds(a, b).second;
}

inline double ExactQuotientLb(const double a, const double b) {
  return QuotientBounds(a, b).first;
}

inline double ExactQuotientUb(const double a, const double b) {
  return QuotientBounds(a, b).second;
}

inline double ExactScaledSumLb(const double a, const double b,
                               const double divisor) {
  if (divisor < 0.0) return ExactScaledSumUb(-a, -b, -divisor);
  return ExactQuotientLb(ExactSumLb(a, b), divisor);
}

inline double ExactScaledSumUb(const double a, const double b,
                               const double divisor) {
  if (divisor < 0.0) return ExactScaledSumLb(-a, -b, -divisor);
  return ExactQuotientUb(ExactSumUb(a, b), divisor);
}

inline double MaxAbsScaledTightSum(const TightSum& sum,
                                   const double pos_divisor) {
  const auto [lb, ub] = sum.TightBounds();
  const auto [dlb, dub] = RelaxedDivInterval(lb, ub, pos_divisor);
  return std::max(std::abs(dlb), std::abs(dub));
}

inline std::pair<double, double> RelaxedMulInterval(const double lb,
                                                    const double ub,
                                                    const double factor) {
  DCHECK(std::isfinite(factor));
  if (factor == 0.0) return {0.0, 0.0};
  if (factor < 0.0) return RelaxedMulInterval(-ub, -lb, -factor);
  return {ExactProdLb(lb, factor), ExactProdUb(ub, factor)};
}

inline std::pair<double, double> RelaxedDivInterval(const double lb,
                                                    const double ub,
                                                    const double divisor) {
  if (divisor < 0.0) return RelaxedDivInterval(-ub, -lb, -divisor);
  return {ExactQuotientLb(lb, divisor), ExactQuotientUb(ub, divisor)};
}

inline std::pair<double, double> IntervalProdBounds(const double a_lb,
                                                    const double a_ub,
                                                    const double b_lb,
                                                    const double b_ub) {
  DCHECK(!std::isnan(a_lb) && !std::isnan(a_ub));
  DCHECK(!std::isnan(b_lb) && !std::isnan(b_ub));
  DCHECK_LE(a_lb, a_ub);
  DCHECK_LE(b_lb, b_ub);
  const auto prod_lb = [](double a, double b) {
    return (a == 0.0 || b == 0.0) ? 0.0 : ExactProdLb(a, b);
  };
  const auto prod_ub = [](double a, double b) {
    return (a == 0.0 || b == 0.0) ? 0.0 : ExactProdUb(a, b);
  };
  if (a_lb >= 0.0) {
    return {prod_lb(b_lb < 0.0 ? a_ub : a_lb, b_lb),
            prod_ub(b_ub > 0.0 ? a_ub : a_lb, b_ub)};
  }
  if (a_ub <= 0.0) {
    return {prod_lb(b_ub > 0.0 ? a_lb : a_ub, b_ub),
            prod_ub(b_lb < 0.0 ? a_lb : a_ub, b_lb)};
  }
  if (b_lb >= 0.0) {
    return {prod_lb(a_lb, b_ub), prod_ub(a_ub, b_ub)};
  }
  if (b_ub <= 0.0) {
    return {prod_lb(a_ub, b_lb), prod_ub(a_lb, b_lb)};
  }
  return {std::min(ExactProdLb(a_lb, b_ub), ExactProdLb(a_ub, b_lb)),
          std::max(ExactProdUb(a_lb, b_lb), ExactProdUb(a_ub, b_ub))};
}

}  // namespace operations_research

#endif  // ORTOOLS_UTIL_FULL_PRECISION_INEQUALITIES_H_
