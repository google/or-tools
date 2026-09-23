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

#include <compare>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "absl/types/span.h"

namespace operations_research {

// Here for computations in R, we use the standard injection from double to R: a
// double is mapped to (-1)^sign * mantissa * 2^exponent (see
// https://en.wikipedia.org/wiki/Double-precision_floating-point_format).
// All functions here assume all values are finite.

// Returns a pair of lb, ub such that lb <= dot_product(a, b) <= ub provably,
// for the actual dot product of a and b (in R). These bounds are not
// necessarily tight and can be infinite.
std::pair<double, double> GetLooseDotProductBounds(absl::Span<const double> a,
                                                   absl::Span<const double> b);

// Returns a pair of lb, ub such that lb <= dot_product(a, b) <= ub provably,
// for the actual dot product of a and b (in R). These bounds are not
// necessarily tight and can be infinite. Variant of the
// GetLooseDotProductBounds function above which takes getters instead of spans.
std::pair<double, double> GetLooseDotProductBounds(
    const absl::AnyInvocable<double(int) const>& get_a,
    const absl::AnyInvocable<double(int) const>& get_b, int size);

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

// Returns a pair of lb, ub such that lb <= dot_product(a, b) <= ub provably,
// for the actual dot product of a and b (in R). These bounds are tight:
//  * if dot_product(a, b) is representable as a double, then lb == ub ==
//    dot_product(a, b);
//  * otherwise, ub = nextafter(lb, inf).
// Note that the algorithm is in O(n * log n * err) where n is the size of the
// vectors and err is the number of loose bits in GetLooseDotProductBounds,
// hense is better than full precision arithmetic which would be O(n^2). Variant
// of the GetTightDotProductBounds function above which takes getters instead of
// spans.
std::pair<double, double> GetTightDotProductBounds(
    const absl::AnyInvocable<double(int) const>& get_a,
    const absl::AnyInvocable<double(int) const>& get_b, int size);

// Returns the comparison between dot product and bound. Formally a.b <=> bound
// where <=> has the semantics of the three way operator. NaN are not handled.
std::strong_ordering CmpDotProduct(absl::Span<const double> a,
                                   absl::Span<const double> b, double bound);

// Returns the comparison between dot product and bound. Formally a.b <=> bound
// where <=> has the semantics of the three way operator. NaN are not handled.
// Variant of the CmpDotProduct function above which takes getters instead of
// spans.
std::strong_ordering CmpDotProduct(
    const absl::AnyInvocable<double(int) const>& get_a,
    const absl::AnyInvocable<double(int) const>& get_b, int size, double bound);

// Returns true if the dot product of a and b is smaller or equal to ub in R
// (full precision).
bool IsDotProductSmallerOrEqual(absl::Span<const double> a,
                                absl::Span<const double> b, double ub);

// Returns true if the dot product of a and b is smaller or equal to ub in R
// (full precision).
bool IsDotProductSmallerOrEqual(
    const absl::AnyInvocable<double(int) const>& get_a,
    const absl::AnyInvocable<double(int) const>& get_b, int size, double ub);

// Returns true if the dot product of a and b is greater or equal to lb in R
// (full precision).
bool IsDotProductGreaterOrEqual(absl::Span<const double> a,
                                absl::Span<const double> b, double lb);

// Returns true if the dot product of a and b is equal to lb in R (full
// precision).
bool IsDotProductGreaterOrEqual(
    const absl::AnyInvocable<double(int) const>& get_a,
    const absl::AnyInvocable<double(int) const>& get_b, int size, double lb);

// Returns true if the dot product of a and b is equal to res in R (full
// precision).
bool IsDotProductEqual(absl::Span<const double> a, absl::Span<const double> b,
                       double res);

// Returns true if the dot product of a and b is equal to res in R (full
// precision).
bool IsDotProductEqual(const absl::AnyInvocable<double(int) const>& get_a,
                       const absl::AnyInvocable<double(int) const>& get_b,
                       int size, double res);

}  // namespace operations_research

#endif  // ORTOOLS_UTIL_FULL_PRECISION_INEQUALITIES_H_
