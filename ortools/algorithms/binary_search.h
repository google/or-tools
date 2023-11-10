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
#include <cstdint>
#include <functional>
#include <utility>

#include "absl/functional/function_ref.h"
#include "absl/log/check.h"
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
// TODO(user): replace std::function by absl::FunctionRef here and in
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

// Searches in the range [begin, end), where Point supports basic arithmetic.
template <class Point, class Value>
std::pair<Point, Value> RangeConvexMinimum(Point begin, Point end,
                                           absl::FunctionRef<Value(Point)> f);
template <class Point, class Value>
std::pair<Point, Value> RangeConvexMinimum(std::pair<Point, Value> current_min,
                                           Point begin, Point end,
                                           absl::FunctionRef<Value(Point)> f);

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
std::pair<Point, Value> RangeConvexMinimum(Point begin, Point end,
                                           absl::FunctionRef<Value(Point)> f) {
  DCHECK_LT(begin, end);
  const Value size = end - begin;
  if (size == 1) {
    return {begin, f(begin)};
  }

  // Starts by splitting interval in two with two queries and getting some info.
  // Note the current min will be outside the interval.
  std::pair<Point, Value> current_min;
  {
    DCHECK_GE(size, 2);
    const Point mid = begin + (end - begin) / 2;
    DCHECK_GT(mid, begin);
    const Value v = f(mid);
    const Point before_mid = mid - 1;
    const Value before_v = f(before_mid);
    if (before_v == v) return {before_mid, before_v};
    if (before_v < v) {
      // Note that we exclude before_mid from the range.
      current_min = {before_mid, before_v};
      end = before_mid;
    } else {
      current_min = {mid, v};
      begin = mid + 1;
    }
  }
  if (begin >= end) return current_min;
  return RangeConvexMinimum<Point, Value>(current_min, begin, end, f);
}

template <class Point, class Value>
std::pair<Point, Value> RangeConvexMinimum(std::pair<Point, Value> current_min,
                                           Point begin, Point end,
                                           absl::FunctionRef<Value(Point)> f) {
  DCHECK_LT(begin, end);
  while ((end - begin) > 1) {
    DCHECK(current_min.first < begin || current_min.first >= end);
    bool current_is_after_end = current_min.first >= end;
    const Point mid = begin + (end - begin) / 2;
    const Value v = f(mid);
    if (v >= current_min.second) {
      // If the midpoint is no better than our current minimum, then the
      // global min must lie between our midpoint and our current min.
      if (current_is_after_end) {
        begin = mid + 1;
      } else {
        end = mid;
      }
    } else {
      // v < current_min.second, we cannot decide, so we use a second value
      // close to v like in the initial step.
      DCHECK_GT(mid, begin);
      const Point before_mid = mid - 1;
      const Value before_v = f(before_mid);
      if (before_v == v) return {before_mid, before_v};
      if (before_v < v) {
        current_min = {before_mid, before_v};
        current_is_after_end = true;
        end = before_mid;
      } else {
        current_is_after_end = false;
        current_min = {mid, v};
        begin = mid + 1;
      }
    }
  }

  if (end - begin == 1) {
    const Value v = f(begin);
    if (v <= current_min.second) return {begin, v};
  }
  return current_min;
}

template <class Point, class Value>
std::pair<Point, Value> ConvexMinimum(absl::Span<const Point> sorted_points,
                                      std::function<Value(Point)> f) {
  auto index_f = [&](int index) -> Value { return f(sorted_points[index]); };
  const auto& [index, v] =
      RangeConvexMinimum<int64_t, Value>(0, sorted_points.size(), index_f);
  return {sorted_points[index], v};
}

template <class Point, class Value>
std::pair<Point, Value> ConvexMinimum(bool is_to_the_right,
                                      std::pair<Point, Value> current_min,
                                      absl::Span<const Point> sorted_points,
                                      std::function<Value(Point)> f) {
  auto index_f = [&](int index) -> Value { return f(sorted_points[index]); };
  std::pair<int, Value> index_current_min = std::make_pair(
      is_to_the_right ? sorted_points.size() : -1, current_min.second);
  const auto& [index, v] = RangeConvexMinimum<int64_t, Value>(
      index_current_min, 0, sorted_points.size(), index_f);
  if (index == index_current_min.first) return current_min;
  return {sorted_points[index], v};
}
}  // namespace operations_research

#endif  // OR_TOOLS_ALGORITHMS_BINARY_SEARCH_H_
