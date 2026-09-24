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

#include "absl/types/span.h"

namespace operations_research {

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

}  // namespace operations_research

#endif  // ORTOOLS_UTIL_FULL_PRECISION_INEQUALITIES_H_
