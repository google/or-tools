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
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/base/attributes.h"
#include "absl/functional/any_invocable.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/numeric/int128.h"
#include "absl/types/span.h"
#include "ortools/algorithms/multikey_radix_sort.h"

namespace operations_research {

namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();

}

std::pair<double, double> GetLooseDotProductBounds(
    const absl::AnyInvocable<double(int) const>& get_a,
    const absl::AnyInvocable<double(int) const>& get_b, const int size) {
  double dp_ub = 0.0;
  double dp_lb = 0.0;
  for (int i = 0; i < size; ++i) {
    const double prod = get_a(i) * get_b(i);
    dp_ub = std::nextafter(dp_ub + std::nextafter(prod, kInf), kInf);
    dp_lb = std::nextafter(dp_lb + std::nextafter(prod, -kInf), -kInf);
  }
  return {dp_lb, dp_ub};
}

std::pair<double, double> GetLooseDotProductBounds(
    const absl::Span<const double> a, const absl::Span<const double> b) {
  // We could use DCHECK, but this is fast compared to the algorithm itself.
  CHECK_EQ(a.size(), b.size());
  return GetLooseDotProductBounds([&a](int i) { return a[i]; },
                                  [&b](int i) { return b[i]; }, a.size());
}

namespace {

// Represents a real number with an int128 mantissa. Represents mantissa * 2^
// exponent up to the negative sign.
struct RealNumber {
  bool negative ABSL_REQUIRE_EXPLICIT_INIT;
  absl::uint128 mantissa ABSL_REQUIRE_EXPLICIT_INIT;
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

}  // namespace

std::strong_ordering CmpDotProduct(
    const absl::AnyInvocable<double(int) const>& get_a,
    const absl::AnyInvocable<double(int) const>& get_b, const int size,
    const double bound) {
  // All comments assume kGreater is false except where noted otherwise. If
  // kGreater is true the logic is reversed.

  // Fast path.
  // Compute a proven upper and lower bound for the dot product.
  const auto [dp_lb, dp_ub] = GetLooseDotProductBounds(get_a, get_b, size);

  // Note that if size == 0, we have dp_ub == 0.0 and dp_lb == 0.0 so that
  // we do not enter the slow path.
  if (dp_lb > bound) return std::strong_ordering::greater;
  if (dp_ub < bound) return std::strong_ordering::less;
  if (dp_ub == bound && dp_lb == bound) return std::strong_ordering::equal;

  // Slow path. We have size > 0.
  // Computes terms such that sum(terms) == dot_product - bound.
  std::vector<RealNumber> terms;
  terms.reserve(size + 1);
  for (int i = 0; i < size; ++i) {
    terms.push_back(Prod(get_a(i), get_b(i)));
  }
  terms.push_back(FromDouble(bound));
  terms.back().negative = !terms.back().negative;

  // Sort by increasing exponent.
  absl::c_sort(terms, [](const RealNumber& a, const RealNumber& b) {
    return a.exponent < b.exponent;
  });
  RealNumber remainder = terms.front();
  std::strong_ordering dropped_part_cmp_zero = std::strong_ordering::equal;
  // For invariants, set S = sum(terms) and initialize dropped_part = 0 (these
  // not being actually stored).
  for (int i = 1; i < terms.size(); ++i) {
    // Invariant:
    // S = sum(j >= i, terms[j]) + remainder + dropped_part holds with:
    //  * remainder.exponent <= terms[i].exponent,
    //  * abs(dropped_part) < 2^{remainder.exponent}
    //  * dropped_part_cmp_zero is the comparison of the dropped part with 0.
    const int exponent_diff = terms[i].exponent - remainder.exponent;
    if (exponent_diff >= 128) {
      if (remainder.mantissa != 0) {
        dropped_part_cmp_zero = remainder.negative
                                    ? std::strong_ordering::less
                                    : std::strong_ordering::greater;
      }
      // Implicit: dropped_part += remainder
      remainder.mantissa = 0;
      remainder.exponent = terms[i].exponent;
    } else if (exponent_diff > 0) {
      // Shift the remainder to align it with the terms[i].
      const absl::uint128 to_drop =
          remainder.mantissa & ((absl::uint128{1} << exponent_diff) - 1);
      if (to_drop != 0) {
        dropped_part_cmp_zero = remainder.negative
                                    ? std::strong_ordering::less
                                    : std::strong_ordering::greater;
      }
      // Implicit: dropped_part += to_drop * 2^{remainder.exponent}
      remainder.mantissa >>= exponent_diff;
      remainder.exponent = terms[i].exponent;
    }
    // Invariant:
    // S = sum(j >= i, terms[j]) + remainder + dropped_part holds with:
    //  * remainder.exponent = terms[i].exponent,
    //  * abs(dropped_part) < 2^{remainder.exponent},
    //  * dropped_part_cmp_zero is the comparison of the dropped part with 0.

    // Compute remainder += terms[i].
    if (remainder.negative == terms[i].negative) {
      remainder.mantissa += terms[i].mantissa;
      // Check that we did not overflow. As all terms have a mantissa < 2^106,
      // we would need 2^22 terms with same exponent and same sign to
      // risk an overflow. Knowing that the fast path failed, it is very
      // unlikely.
      CHECK_GE(remainder.mantissa, terms[i].mantissa);
    } else if (remainder.mantissa < terms[i].mantissa) {
      remainder.mantissa = terms[i].mantissa - remainder.mantissa;
      remainder.negative = terms[i].negative;
    } else {
      remainder.mantissa -= terms[i].mantissa;
    }
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

std::strong_ordering CmpDotProduct(const absl::Span<const double> a,
                                   const absl::Span<const double> b,
                                   const double bound) {
  // We could use DCHECK, but this is fast compared to the algorithm itself.
  CHECK_EQ(a.size(), b.size());
  return CmpDotProduct([&a](int i) { return a[i]; },
                       [&b](int i) { return b[i]; }, a.size(), bound);
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
    const absl::AnyInvocable<double(int) const>& get_a,
    const absl::AnyInvocable<double(int) const>& get_b, const int size) {
  auto [lb, ub] = GetLooseDotProductBounds(get_a, get_b, size);
  if (lb == ub) return {lb, ub};

  if (std::isfinite(lb) &&
      CmpDotProduct(get_a, get_b, size, lb) == std::strong_ordering::equal) {
    return {lb, lb};
  }
  if (std::isfinite(ub) &&
      CmpDotProduct(get_a, get_b, size, ub) == std::strong_ordering::equal) {
    return {ub, ub};
  }
  // Perform a dichotomy.
  while (std::nextafter(lb, ub) != ub) {
    const double mid = CardinalMidpoint(lb, ub);
    // Note that this could be optimized as a part of the call of CmpDotProduct
    // is common to all calls.
    const auto cmp = CmpDotProduct(get_a, get_b, size, mid);
    if (cmp == std::strong_ordering::equal) return {mid, mid};

    if (cmp == std::strong_ordering::less) {
      ub = mid;
    } else {
      lb = mid;
    }
  }
  return {lb, ub};
}

std::pair<double, double> GetTightDotProductBounds(
    const absl::Span<const double> a, const absl::Span<const double> b) {
  // We could use DCHECK, but this is fast compared to the algorithm itself.
  CHECK_EQ(a.size(), b.size());
  return GetTightDotProductBounds([&a](int i) { return a[i]; },
                                  [&b](int i) { return b[i]; }, a.size());
}

bool IsDotProductSmallerOrEqual(
    const absl::AnyInvocable<double(int) const>& get_a,
    const absl::AnyInvocable<double(int) const>& get_b, const int size,
    const double ub) {
  return CmpDotProduct(get_a, get_b, size, ub) != std::strong_ordering::greater;
}

bool IsDotProductSmallerOrEqual(const absl::Span<const double> a,
                                const absl::Span<const double> b,
                                const double ub) {
  // We could use DCHECK, but this is fast compared to the algorithm itself.
  CHECK_EQ(a.size(), b.size());
  return IsDotProductSmallerOrEqual([&a](int i) { return a[i]; },
                                    [&b](int i) { return b[i]; }, a.size(), ub);
}

bool IsDotProductGreaterOrEqual(
    const absl::AnyInvocable<double(int) const>& get_a,
    const absl::AnyInvocable<double(int) const>& get_b, const int size,
    const double lb) {
  return CmpDotProduct(get_a, get_b, size, lb) != std::strong_ordering::less;
}

bool IsDotProductGreaterOrEqual(const absl::Span<const double> a,
                                const absl::Span<const double> b,
                                const double lb) {
  // We could use DCHECK, but this is fast compared to the algorithm itself.
  CHECK_EQ(a.size(), b.size());
  return IsDotProductGreaterOrEqual([&a](int i) { return a[i]; },
                                    [&b](int i) { return b[i]; }, a.size(), lb);
}

bool IsDotProductEqual(const absl::AnyInvocable<double(int) const>& get_a,
                       const absl::AnyInvocable<double(int) const>& get_b,
                       const int size, const double res) {
  return CmpDotProduct(get_a, get_b, size, res) == std::strong_ordering::equal;
}

bool IsDotProductEqual(const absl::Span<const double> a,
                       const absl::Span<const double> b, const double res) {
  // We could use DCHECK, but this is fast compared to the algorithm itself.
  CHECK_EQ(a.size(), b.size());
  return IsDotProductEqual([&a](int i) { return a[i]; },
                           [&b](int i) { return b[i]; }, a.size(), res);
}

}  // namespace operations_research
