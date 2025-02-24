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

#ifndef OR_TOOLS_BASE_ARRAY_INTERNAL_H_
#define OR_TOOLS_BASE_ARRAY_INTERNAL_H_

#include <array>
#include <cstddef>
#include <type_traits>
#include <utility>

#include "absl/utility/utility.h"

namespace gtl::internal_array {

template <typename T, std::size_t N, std::size_t... Idx>
constexpr std::array<std::remove_cv_t<T>, N> to_array_internal(
    T (&ts)[N], absl::index_sequence<Idx...>) {
  return {{ts[Idx]...}};
}

template <typename T, std::size_t N, std::size_t... Idx>
constexpr std::array<std::remove_cv_t<T>, N> to_array_internal(
    T (&&ts)[N], absl::index_sequence<Idx...>) {
  return {{std::move(ts[Idx])...}};
}

}  // namespace gtl::internal_array

#endif  // OR_TOOLS_BASE_ARRAY_INTERNAL_H_
