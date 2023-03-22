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

}  // namespace operations_research

#endif  // OR_TOOLS_ALGORITHMS_BINARY_SEARCH_H_
