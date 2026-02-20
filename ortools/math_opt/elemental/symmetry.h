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

#ifndef ORTOOLS_MATH_OPT_ELEMENTAL_SYMMETRY_H_
#define ORTOOLS_MATH_OPT_ELEMENTAL_SYMMETRY_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "absl/log/check.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"

namespace operations_research::math_opt {

// A tag for no symmetry between two elements.
struct NoSymmetry {
  static constexpr absl::string_view GetName() { return "NoSymmetry"; };

  template <size_t n>
  static constexpr bool Validate(std::array<int64_t, n>&) {
    // All keys are valid.
    return true;
  }

  template <size_t n>
  static constexpr void Enforce(std::array<int64_t, n>&) {
    // No symmetry to enforce.
  }

  template <typename ElementTypeT, size_t n>
  static constexpr void CheckElementTypes(const std::array<ElementTypeT, n>&) {
    // No type constraints.
  }
};

// A tag to represent a symmetry between two elements `i` and `j`, i.e. the fact
// that the attribute value for `(key[i],key[j])` and `(key[j],key[i])` are the
// same. We internally represent such attribute keys with `key[i] <= key[j]`.
template <int i, int j>
struct ElementSymmetry {
  static_assert(0 <= i && i < j);

  static std::string GetName() {
    return absl::StrFormat("ElementSymmetry<%i, %i>", i, j);
  };

  template <size_t n>
  static constexpr bool Validate(std::array<int64_t, n>& ids) {
    static_assert(n > 1, "one-dimensional keys cannot have symmetries");
    static_assert(j < n);
    return ids[i] <= ids[j];
  }

  template <size_t n>
  static constexpr void Enforce(std::array<int64_t, n>& ids) {
    static_assert(n > 1, "one-dimensional keys cannot have symmetries");
    if (ids[i] > ids[j]) {
      std::swap(ids[i], ids[j]);
    }
  }

  template <typename ElementTypeT, size_t n>
  static constexpr void CheckElementTypes(
      const std::array<ElementTypeT, n>& element_types) {
    static_assert(n > 1, "one-dimensional keys cannot have symmetries");
    static_assert(j < n);
    CHECK_EQ(element_types[i], element_types[j])
        << "symmetric elements must be of the same type";
  }
};

}  // namespace operations_research::math_opt

#endif  // ORTOOLS_MATH_OPT_ELEMENTAL_SYMMETRY_H_
