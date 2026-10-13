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

#include <bit>
#include <cmath>
#include <compare>
#include <cstdint>
#include <limits>
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
double FastStepUp(double v);

// Function that is equivalent to std::nextafter(v, -kInf), but significantly
// faster since it doesn't need to set or check CPU flags.
double FastStepDown(double v);

// Represents a power of two, i.e. 2^exponent. Used for the API of Sum. Supports
// between 2^-30 and 2^30 (the reason is to avoid overflows in the exponent).
struct PowerOfTwo {
  PowerOfTwo(int exponent);
  int exponent;
};

// Represents a sum of terms with full precision. The sum is not necessarily
// representable as a double. This is not thread-safe.
class Sum {
 public:
  // Adds the product of the given arguments to the sum. arguments can be of
  // type double, int64_t, or PowerOfTwo. Returns itself for chaining.
  // Two versions to simplify the implementation.
  template <typename Arg>
  Sum& Add(Arg arg);
  template <typename Arg1, typename Arg2, typename... OtherArgs>
  Sum& AddProd(Arg1 arg1, Arg2 arg2, OtherArgs... other_args);

  friend std::strong_ordering operator<=>(const Sum& a, const double x) {
    return a.CompareTo(x);
  }

  friend std::strong_ordering operator<=>(const double x, const Sum& a) {
    // 0 <=> ? defined as opposite for std::strong_ordering.
    return 0 <=> a.CompareTo(x);
  }

  friend std::strong_ordering operator<=>(const Sum& a, const int64_t x) {
    return a.CompareTo(x);
  }

  friend std::strong_ordering operator<=>(const int64_t x, const Sum& a) {
    // 0 <=> ? defined as opposite for std::strong_ordering.
    return 0 <=> a.CompareTo(x);
  }

  // Returns currently known bounds on the sum.
  std::pair<double, double> LooseBounds() const { return {lb_, ub_}; }

  double LooseLb() const { return lb_; }

  double LooseUb() const { return ub_; }

  // Returns the tight bounds of the sum. In other words, returns [lb, ub]
  // such that lb <= sum <= ub and
  //  * if sum is representable as a double, then lb == ub == sum;
  //  * otherwise, ub = nextafter(lb, inf).
  std::pair<double, double> TightBounds() const {
    if (tight_bounds_) return {lb_, ub_};

    return SlowTightBounds();
  }

  double TightLb() const { return TightBounds().first; }

  double TightUb() const { return TightBounds().second; }

  // Returns {floor(sum), ceil(sum)}, clamped to
  // [std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max()].
  std::pair<int64_t, int64_t> SaturatedInt64Bounds() const;

  int64_t SaturatedFloor() const { return SaturatedInt64Bounds().first; }

  int64_t SaturatedCeil() const { return SaturatedInt64Bounds().second; }

  // A term of the sum. This is public to be used in implementation functions.
  // However, it is not part of the public API (it could be used only as a
  // struct from outside as no member of Sum give access to terms).
  struct Term {
    // These are used as sentinels for infinite values exponents. A value with
    // such an exponent is infinite with sign given by the negative field.
    // mantissa must be 1.
    constexpr static int kExpInf = std::numeric_limits<int>::max();

    bool negative ABSL_REQUIRE_EXPLICIT_INIT;
    absl::uint128 mantissa ORTOOLS_REQUIRE_EXPLICIT_INIT;
    int exponent ABSL_REQUIRE_EXPLICIT_INIT;
  };

 private:
  static Term ToTerm(double x);
  static Term ToTerm(int64_t n);
  static Term ToTerm(PowerOfTwo p);

  // Returns the comparison between the sum and x. Formally sum <=> x where <=>
  // has the semantics of the three way operator. NaN are not handled.
  std::strong_ordering CompareTo(double x) const;

  std::strong_ordering CompareTo(int64_t x) const;

  // Specialized for int64_t and double.
  template <typename Number>
  std::strong_ordering SlowCompareTo(Number x) const;

  std::pair<double, double> SlowTightBounds() const;

  // Adds the given term to the sum and updates the bounds.
  void AddTerm(const Term& term);

  // Adds the given term to the bounds in a safe guaranteed way. Untight the
  // bounds.
  void AddTermToBounds(const Term& term) const;

  void SortIfNeeded() const;

  // Replaces Sum by infinity if the incoming product is infinite. Crashes if it
  // would be NaN.
  template <typename Arg1, typename... OtherArgs>
  void SetInfIfNeeded(Arg1 arg1, OtherArgs... other_args);

  // Multiplies the last term of the sum by the factors in factors_ and removes
  // them from factors_.
  void MultiplyLastTerm();

  mutable absl::InlinedVector<Term, 16> terms_;

  // Stores the factors to multiply the last term of the sum.
  absl::InlinedVector<Term, 6> factors_;

  // If the sum is infinite, this is the sign. Once the sum is infinite, it
  // cannot change (it crashes if signs are not consistent). equal means that
  // the sum is finite.
  std::strong_ordering inf_sign_ = std::strong_ordering::equal;

  // Terms are sorted in increasing order of exponent, then mantissa, up to one
  // before this index.
  mutable int terms_sorted_before_ = 0;

  // Known bounds on the sum.
  mutable double lb_ = 0.0;
  mutable double ub_ = 0.0;

  // Whether the bounds are tight.
  mutable bool tight_bounds_ = true;
};

// Returns the sum of a vector of doubles or int64_t.
template <typename Number>
Sum SumAll(absl::Span<const Number> v);

// Returns the DotProduct of two vectors of double or int64_t.
template <typename Number1, typename Number2>
Sum DotProduct(absl::Span<const Number1> v, absl::Span<const Number2> w);

// TODO(user): All these function should be removed. However, keep them here
// during the transition to the new API.

// Here for computations in R, we use the standard injection from double to R: a
// double is mapped to (-1)^sign * mantissa * 2^exponent (see
// https://en.wikipedia.org/wiki/Double-precision_floating-point_format).
// Crashes if a NaN is encountered or if a NaN would be produced (e.g. inf -
// inf). infinite values are handled if they are sign consistent.

// Returns the comparison between the sum of v and 0. Formally sum(v) <=> 0.0
// where <=> has the semantics of the three way operator. NaN are not handled.
std::strong_ordering ComputeSumSign(absl::Span<const double> v);

// Returns true if the sum of v is strictly positive in R (full precision).
// Crashes if a NaN is encountered. Infinite values are handled if they are sign
// consistent.
bool SumIsPositive(absl::Span<const double> v);

// Returns true if the sum of v is strictly negative in R (full precision).
// Crashes if a NaN is encountered. Infinite values are handled if they are sign
// consistent.
bool SumIsNegative(absl::Span<const double> v);

// Returns a pair of lb, ub such that lb <= dot_product(a, b) <= ub provably,
// for the actual dot product of a and b (in R). These bounds are not
// necessarily tight and can be infinite.
std::pair<double, double> GetLooseDotProductBounds(absl::Span<const double> a,
                                                   absl::Span<const double> b);

// Returns a pair of lb, ub such that lb <= dot_product(a, b) <= ub provably,
// for the actual dot product of a and b (in R). These bounds are tight:
//  * if dot_product(a, b) is representable as a double, then lb == ub ==
//    dot_product(a, b);
//  * otherwise, ub = nextafter(lb, inf).
// Note that the algorithm is in O(n * log n * err) where n is the size of the
// vectors and err is the number of loose bits in GetLooseDotProductBounds,
// hence is better than full precision arithmetic which would be O(n^2).
//
// tl;dr: it is tighter but (much) slower than GetLooseDotProductBounds.
std::pair<double, double> GetTightDotProductBounds(absl::Span<const double> a,
                                                   absl::Span<const double> b);

// Returns the comparison between dot product and bound. Formally a.b <=> bound
// where <=> has the semantics of the three way operator. NaN are not handled.
std::strong_ordering CmpDotProduct(absl::Span<const double> a,
                                   absl::Span<const double> b, double bound);

// Returns true if the dot product of a and b is smaller or equal to ub in R
// (full precision).
bool DotProductIsSmallerOrEqual(absl::Span<const double> a,
                                absl::Span<const double> b, double ub);

// Returns true if the dot product of a and b is greater or equal to lb in R
// (full precision).
bool DotProductIsGreaterOrEqual(absl::Span<const double> a,
                                absl::Span<const double> b, double lb);

// Returns true if the dot product of a and b is equal to res in R (full
// precision).
bool DotProductIsEqual(absl::Span<const double> a, absl::Span<const double> b,
                       double res);

// For any combination of int64_t and finite double, ProdBounds(a, b) returns a
// tight pair of bounds [lb, ub] such that lb <= a * b <= ub in R.
std::pair<double, double> ProdBounds(int64_t a, int64_t b);
std::pair<double, double> ProdBounds(int64_t a, double b);
std::pair<double, double> ProdBounds(double a, int64_t b);
std::pair<double, double> ProdBounds(double a, double b);

// Returns a tight double interval containing n.
std::pair<double, double> Int64DoubleBounds(int64_t n);

// Returns tight bounds for scale * (n + pre_shift) + shift.
// TODO(user): improve the API. In particular, it is very close to
// `GetTightDotProductBounds`. We should deduplicate the code.
// Preconditions: scale, shift, and pre_shift are finite.
std::pair<double, double> FuseAddMultiplyAdd(double scale, double shift,
                                             double pre_shift, int64_t n);

inline double FastStepUp(const double v) {
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

inline double FastStepDown(const double v) {
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

// Inline implementation of class Sum for cases where speed matters (short
// functions).

inline PowerOfTwo::PowerOfTwo(const int exponent) : exponent(exponent) {
  DCHECK_LE(exponent, 1 << 30);
  DCHECK_GE(exponent, -(1 << 30));
}

template <typename Arg>
Sum& Sum::Add(Arg arg) {
  SetInfIfNeeded(arg);
  if (inf_sign_ != std::strong_ordering::equal) return *this;

  AddTerm(ToTerm(arg));
  return *this;
}

template <typename Arg1, typename Arg2, typename... OtherArgs>
Sum& Sum::AddProd(Arg1 arg1, Arg2 arg2, OtherArgs... other_args) {
  SetInfIfNeeded(arg1, arg2, other_args...);
  if (inf_sign_ != std::strong_ordering::equal) return *this;

  Term term1 = ToTerm(arg1);
  // No overflow possible here.
  const Term term2 = ToTerm(arg2);
  term1.mantissa *= term2.mantissa;
  term1.exponent += term2.exponent;
  term1.negative ^= term2.negative;
  if constexpr (sizeof...(other_args) == 0) {
    AddTerm(term1);
    return *this;
  }

  terms_.push_back(term1);
  (factors_.push_back(ToTerm(other_args)), ...);
  const int end_before = terms_.size() - 1;
  MultiplyLastTerm();
  for (int i = end_before; i < terms_.size(); ++i) {
    AddTermToBounds(terms_[i]);
  }
  return *this;
}

inline Sum::Term Sum::ToTerm(const double x) {
  CHECK(!std::isnan(x));
  if (!std::isfinite(x)) {
    return {.negative = x < 0, .mantissa = 1, .exponent = Sum::Term::kExpInf};
  }
  int exp;
  const double frac = std::frexp(std::abs(x), &exp);
  uint64_t mantissa = static_cast<uint64_t>(std::ldexp(frac, 53));
  if (mantissa != 0) {
    const int trailing_z = std::countr_zero(mantissa);
    mantissa >>= trailing_z;
    exp += trailing_z;
  }
  return {.negative = x < 0, .mantissa = mantissa, .exponent = exp - 53};
}

inline Sum::Term Sum::ToTerm(const int64_t n) {
  uint64_t mantissa =
      n == std::numeric_limits<int64_t>::min()  // Avoid overflow of std::abs.
          ? uint64_t{1} << 63
          : static_cast<uint64_t>(std::abs(n));
  int exp = 0;
  if (mantissa != 0) {
    const int trailing_z = std::countr_zero(mantissa);
    mantissa >>= trailing_z;
    exp = trailing_z;
  }
  return {.negative = n < 0, .mantissa = mantissa, .exponent = exp};
}

inline Sum::Term Sum::ToTerm(const PowerOfTwo p) {
  return {.negative = false, .mantissa = 1, .exponent = p.exponent};
}

template <typename Arg1, typename... OtherArgs>
void Sum::SetInfIfNeeded(Arg1 arg1, OtherArgs... other_args) {
  auto isinf = [](auto arg) {
    if constexpr (std::is_same_v<decltype(arg), double>) {
      CHECK(!std::isnan(arg)) << "NaN not supported";
      return !std::isfinite(arg);
    } else {
      return false;
    }
  };
  if (!isinf(arg1) && (!isinf(other_args) && ...)) {
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
    terms_.clear();
    // Fix once and for all the bounds.
    terms_sorted_before_ = 0;
    tight_bounds_ = true;
    constexpr double kInf = std::numeric_limits<double>::infinity();
    lb_ = inf_sign_ == std::strong_ordering::less ? -kInf : kInf;
    ub_ = lb_;
  } else {
    CHECK_EQ(inf_sign_, prod_sign) << "Inconsistent infinite values in sum";
  }
}

inline std::strong_ordering Sum::CompareTo(const double x) const {
  if (ub_ < x) return std::strong_ordering::less;
  if (lb_ > x) return std::strong_ordering::greater;
  if (ub_ == x && lb_ == x) return std::strong_ordering::equal;
  if (tight_bounds_) {
    if (ub_ == x) return std::strong_ordering::less;
    if (lb_ == x) return std::strong_ordering::greater;
  }
  return SlowCompareTo(x);
}

inline std::strong_ordering Sum::CompareTo(const int64_t x) const {
  const auto [x_lb, x_ub] = Int64DoubleBounds(x);
  if (ub_ < x_lb) return std::strong_ordering::less;
  if (lb_ > x_ub) return std::strong_ordering::greater;
  if (ub_ == x_lb && lb_ == x_ub) return std::strong_ordering::equal;
  return SlowCompareTo(x);
}

template <typename Number>
Sum SumAll(const absl::Span<const Number> v) {
  Sum res;
  for (const Number& n : v) {
    res.Add(n);
  }
  return res;
}

// Returns the DotProduct of two vectors of double or int64_t.
template <typename Number1, typename Number2>
Sum DotProduct(absl::Span<const Number1> v, absl::Span<const Number2> w) {
  CHECK_EQ(v.size(), w.size());
  Sum res;
  for (int i = 0; i < v.size(); ++i) {
    res.AddProd(v[i], w[i]);
  }
  return res;
}

}  // namespace operations_research

#endif  // ORTOOLS_UTIL_FULL_PRECISION_INEQUALITIES_H_
