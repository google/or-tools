// Copyright 2010-2022 Google LLC
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

#ifndef OR_TOOLS_ALGORITHMS_BINARY_SEARCH_H_
#define OR_TOOLS_ALGORITHMS_BINARY_SEARCH_H_

#include <algorithm>
#include <cmath>
#include <functional>

#include "absl/numeric/int128.h"
#include "ortools/base/dump_vars.h"
#include "ortools/base/logging.h"

namespace operations_research {

// EXAMPLE:
//   // Finds the value x in [0,Pi/2] such that cos(x)=2sin(x):
//   BinarySearch<double>(/*x_true=*/0.0, /*x_false=*/M_PI/2,
//                        [](double x) { return cos(x) >= 2*sin(x); });
//
// Note that x_true > x_false is supported: it works either way.
//
// Ideally, f is a monotonic boolean function, such that:
// - f(x_true) = true
// - f(x_false) = false
// - there exists X such that f(x)=true for all x between x_true and X, and
//   f(x)=false for all x between X and x_false.
//
// In those conditions, this returns that value X (note that f(X) is true).
// See below for the NON-MONOTONIC case.
//
// Also note that 'Point' may be floating-point types: the function will still
// converge when the midpoint can't be distinguished from one of the limits,
// which will always happen.
// You can use other types than numerical types, too. absl::Duration is
// naturally supported.
//
// OVERFLOWS and NON-NUMERICAL TYPES: If your points may be subject to overflow
// (e.g. ((kint32max-1) + (kint32max))/2 will overflow an int32_t), or they
// don't support doing (x+y)/2, you can specialize BinarySearchMidpoint() to
// suit your needs. See the examples in the unit test.
//
// NON-MONOTONIC FUNCTIONS: If f isn't monotonic, the binary search will still
// run with it typical complexity, and finish. The Point X that it returns
// will be a "local" inflexion point, meaning that the smallest possible move
// of that point X to a point X' (in the x_true->x_false direction deduced from
// the arguments) would make f(X') return false. EXAMPLES:
// - If Point=int32_t, then the returned X verifies f(X) = true and f(X') =
// false
//   with X' = X + (x_true < x_false ? 1 : -1).
// - If Point=double, ditto with X' =  X * (1 + (x_true < x_false ? ε : -ε)),
//   where ε = std::numeric_limits<double>::epsilon().
//
// Note also that even if f() is non-deterministic, i.e. f(X) can sometimes
// return true and sometimes false for the same X, then the binary search will
// still finish, but it's hard to say anything about the returned X.
template <class Point>
Point BinarySearch(Point x_true, Point x_false, std::function<bool(Point)> f);

// Used by BinarySearch(). This is just (x+y)/2, with some DCHECKs to catch
// overflows. You should override (i.e. specialize) it if you risk getting
// overflows or need something more complicated than (x+y)/2.
// See examples in the unit test.
template <class Point>
Point BinarySearchMidpoint(Point x, Point y);

// Returns the minimum of a convex function on a discrete set of sorted points.
// It is an error to call this with an empty set of points.
//
// We assume the function is "unimodal" with potentially more than one minimum.
// That is strictly decreasing, then have a minimum that can have many points,
// and then is strictly increasing. In this case if we have two points with
// exactly the same value, one of the minimum is always between the two. We will
// only return one of the minimum.
//
// Note that if we allow for non strictly decreasing/increasing, then you have
// corner cases where one need to check all points to find the minimum. For
// instance, if the function is constant except at one point where it is lower.
//
// The usual algorithm to optimize such a function is a ternary search. However
// here we assume calls to f() are expensive, and we try to minimize those. So
// we use a slightly different algorithm than:
// https://en.wikipedia.org/wiki/Ternary_search
//
// TODO(user): Some relevant optimizations:
// - Abort early if we know a lower bound on the min.
// - Seed with a starting point if we know one.
// - We technically do not need the points to be sorted and can use
//   linear-time median computation to speed this up.
//
// TODO(user): replace std::function by absl::AnyInvocable here and in
// BinarySearch().
template <class Point, class Value>
std::pair<Point, Value> ConvexMinimum(absl::Span<const Point> sorted_points,
                                      std::function<Value(Point)> f);

// Internal part of ConvexMinimum() that can also be used directly in some
// situation when we already know some value of f(). This assumes that we
// already have a current_min candidate that is either before or after all the
// points in sorted_points.
template <class Point, class Value>
std::pair<Point, Value> ConvexMinimum(bool is_to_the_right,
                                      std::pair<Point, Value> current_min,
                                      absl::Span<const Point> sorted_points,
                                      std::function<Value(Point)> f);

// _____________________________________________________________________________
// Implementation.

namespace internal {
template <class T>
bool IsNanGeneric(const T&) {
  return false;
}

template <>
inline bool IsNanGeneric(const float& x) {
  return std::isnan(x);
}
template <>
inline bool IsNanGeneric(const double& x) {
  return std::isnan(x);
}
template <>
inline bool IsNanGeneric(const long double& x) {
  return std::isnan(x);
}

template <typename T>
bool AreNumbersOfSameSign(const T& x, const T& y) {
  if constexpr (std::is_same_v<T, absl::int128>) {
    return (x < 0) == (y < 0);
  } else if constexpr (std::is_same_v<T, absl::uint128> ||
                       std::is_unsigned_v<T>) {
    return true;
  } else if constexpr (std::is_signed_v<T>) {
    return std::signbit(x) == std::signbit(y);
  }
  return false;
}

template <typename IntT>
constexpr bool UsesTwosComplement() {
  if constexpr (std::is_integral_v<IntT>) {
    if constexpr (std::is_unsigned_v<IntT>) {
      return true;
    } else {
      return (IntT{-1} & IntT{3}) == IntT{3};
    }
  }
  return false;
}

}  // namespace internal

template <class Point>
Point BinarySearchMidpoint(Point x, Point y) {
  Point midpoint;
  if constexpr (internal::UsesTwosComplement<Point>()) {
    // For integers using two's complement (all compilers in practice up to
    // c++17, and all compilers in theory starting from c++20), we can use a
    // trick from Hacker's delight. See e.g.
    // https://lemire.me/blog/2022/12/06/fast-midpoint-between-two-integers-without-overflow/
    midpoint = (x | y) - ((x ^ y) >> 1);
  } else {
    midpoint = !internal::AreNumbersOfSameSign(x, y)
                   ? (x + y) / 2
                   :
                   // For integers of the same sign, avoid overflows with a
                   // simple trick.
                   x < y ? x + (y - x) / 2
                         : y + (x - y) / 2;
  }
  if (DEBUG_MODE &&
      !internal::IsNanGeneric(midpoint)) {  // Check for overflows.
    if (x < y) {
      DCHECK_LE(midpoint, y) << "Overflow?";
      DCHECK_GE(midpoint, x) << "Overflow?";
    } else {
      DCHECK_GE(midpoint, y) << "Overflow?";
      DCHECK_LE(midpoint, x) << "Overflow?";
    }
  }
  return midpoint;
}

template <class Point>
Point BinarySearch(Point x_true, Point x_false, std::function<bool(Point)> f) {
  DCHECK(f(x_true)) << x_true;
  DCHECK(!f(x_false)) << x_false;
  int num_iterations = 0;
  constexpr int kMaxNumIterations = 1000000;
  while (true) {
    // NOTE(user): If your "Point" type doesn't support the + or the /2
    // operations, we could imagine using the same trick as IsNanGeneric() to
    // make BinarySearch() work for your use case.
    const Point x = BinarySearchMidpoint(x_true, x_false);
    if (internal::IsNanGeneric(x) || x == x_true || x == x_false) return x_true;
    if (++num_iterations > kMaxNumIterations) {
      LOG(DFATAL)
          << "The binary search seems to loop forever! This indicates that your"
             " input types don't converge when repeatedly taking the midpoint";
      return x_true;
    }
    if (f(x)) {
      x_true = x;
    } else {
      x_false = x;
    }
  }
}

template <class Point, class Value>
std::pair<Point, Value> ConvexMinimum(absl::Span<const Point> sorted_points,
                                      std::function<Value(Point)> f) {
  DCHECK(!sorted_points.empty());
  if (sorted_points.size() == 1) {
    return {sorted_points[0], f(sorted_points[0])};
  }

  // Starts by splitting interval in two with two queries and getting some info.
  // Note the current min will be outside the interval.
  bool is_to_the_right;
  std::pair<Point, Value> current_min;
  {
    DCHECK_GE(sorted_points.size(), 2);
    const int i = sorted_points.size() / 2;
    const Value v = f(sorted_points[i]);
    const int before_i = i - 1;
    const Value before_v = f(sorted_points[before_i]);
    if (before_v == v) return {sorted_points[before_i], before_v};
    if (before_v < v) {
      // Note that we exclude before_i from the span.
      current_min = {sorted_points[before_i], before_v};
      is_to_the_right = true;
      sorted_points = sorted_points.subspan(0, std::max(0, before_i));
    } else {
      is_to_the_right = false;
      current_min = {sorted_points[i], v};
      sorted_points = sorted_points.subspan(i + 1);
    }
  }
  if (sorted_points.empty()) return current_min;
  return ConvexMinimum<Point, Value>(is_to_the_right, current_min,
                                     sorted_points, std::move(f));
}

template <class Point, class Value>
std::pair<Point, Value> ConvexMinimum(bool is_to_the_right,
                                      std::pair<Point, Value> current_min,
                                      absl::Span<const Point> sorted_points,
                                      std::function<Value(Point)> f) {
  DCHECK(!sorted_points.empty());
  while (sorted_points.size() > 1) {
    const int i = sorted_points.size() / 2;
    const Value v = f(sorted_points[i]);
    if (v >= current_min.second) {
      // If the midpoint is no better than our current minimum, then the
      // global min must lie between our midpoint and our current min.
      if (is_to_the_right) {
        sorted_points = sorted_points.subspan(i + 1);
      } else {
        sorted_points = sorted_points.subspan(0, i);
      }
    } else {
      // v < current_min.second, we cannot decide, so we use a second value
      // close to v like in the initial step.
      DCHECK_GT(i, 0);
      const int before_i = i - 1;
      const Value before_v = f(sorted_points[before_i]);
      if (before_v == v) return {sorted_points[before_i], before_v};
      if (before_v < v) {
        current_min = {sorted_points[before_i], before_v};
        is_to_the_right = true;
        sorted_points = sorted_points.subspan(0, std::max(0, before_i));
      } else {
        is_to_the_right = false;
        current_min = {sorted_points[i], v};
        sorted_points = sorted_points.subspan(i + 1);
      }
    }
  }

  if (!sorted_points.empty()) {
    const Value v = f(sorted_points[0]);
    if (v <= current_min.second) return {sorted_points[0], v};
  }
  return current_min;
}

}  // namespace operations_research

#endif  // OR_TOOLS_ALGORITHMS_BINARY_SEARCH_H_
