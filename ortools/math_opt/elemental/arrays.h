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

// Utilities to apply template functors on index ranges.
// See tests for examples.
#ifndef OR_TOOLS_MATH_OPT_ELEMENTAL_ARRAYS_H_
#define OR_TOOLS_MATH_OPT_ELEMENTAL_ARRAYS_H_

#include <tuple>
#include <utility>

namespace operations_research::math_opt {

// Calls `fn<0, ..., n-1>()`, and returns the result. Typically used for
// simple reduce operations that can be expressed as a fold.
//
// Examples:
//  - Sum of elements from 0 to 5 (result is 15):
//      `ApplyOnIndexRange<6>([]<int... i>() { return (i + ... + 0); });`
//
//  - Sum of elements of array `a`:
//      ```
//      ApplyOnIndexRange<a.size()>([&a]<int... i>() {
//        return (a[i] + ... + 0);
//      });
//      ```
template <int n, typename Fn>
constexpr decltype(auto) ApplyOnIndexRange(Fn&& fn) {
  // NOLINTNEXTLINE(clang-diagnostic-pre-c++20-compat)
  return [&fn]<int... is>(std::integer_sequence<int, is...>) mutable {
    return fn.template operator()<is...>();
  }(std::make_integer_sequence<int, n>());
}

// Calls (fn<0>(), ..., fn<n-1>()).
// Typically used for independent operations on elements, or more complex reduce
// operations that cannot be expressed with a fold.
//
// Example (independent operations): Log each array element for some array `a`:
//   `ForEachIndex<a.size()>([&a]<int i>() { LOG(ERROR) << a[i]; });`
//
// NOTE: this returns the result of the last call, which allows returning some
// internal state (and avoids capturing an external variable by reference) for
// complex fold operations. See `CollectTest` for an example.
template <int n, typename Fn>
constexpr decltype(auto) ForEachIndex(Fn&& fn) {
  return ApplyOnIndexRange<n>(
      // NOLINTNEXTLINE(clang-diagnostic-pre-c++20-compat)
      [&fn]<int... is>() { return (fn.template operator()<is>(), ...); });
}

// Calls `fn` of each element of `tuple`, and returns the result of the
// last invocation.
template <typename Fn, typename Tuple>
constexpr decltype(auto) ForEach(Fn&& fn, Tuple&& tuple) {
  // NOLINTNEXTLINE(clang-diagnostic-pre-c++20-compat)
  return std::apply([&fn]<typename... Ts>(
                        Ts&&... ts) { return (fn(std::forward<Ts>(ts)), ...); },
                    std::forward<Tuple>(tuple));
}

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_ELEMENTAL_ARRAYS_H_
