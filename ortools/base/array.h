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

#ifndef OR_TOOLS_BASE_ARRAY_H_
#define OR_TOOLS_BASE_ARRAY_H_

//! @todo(corentinl) std::to_array available in C++20.

#include <array>
#include <cstddef>
#include <type_traits>

#include "absl/utility/utility.h"
#include "ortools/base/array_internal.h"

namespace gtl {

/// A utility function to build `std::array` objects from built-in arrays
/// without specifying their size.
///
/// Example:
/// @code{.cpp}
/// auto b = gtl::to_array<std::pair<int, int>>({
///     {1, 2},
///     {3, 4},
/// });
/// @endcode
template <typename T, std::size_t N>
constexpr std::array<std::remove_cv_t<T>, N> to_array(T (&ts)[N]) {
  return internal_array::to_array_internal(ts, absl::make_index_sequence<N>{});
}

template <typename T, std::size_t N>
constexpr std::array<std::remove_cv_t<T>, N> to_array(T (&&ts)[N]) {
  return internal_array::to_array_internal(std::move(ts),
                                           absl::make_index_sequence<N>{});
}

}  // namespace gtl

#endif  // OR_TOOLS_BASE_ARRAY_H_
