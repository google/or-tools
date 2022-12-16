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

#ifndef OR_TOOLS_MATH_OPT_ELEMENTAL_ATTRIBUTES_H_
#define OR_TOOLS_MATH_OPT_ELEMENTAL_ATTRIBUTES_H_

#include <array>
#include <cstdint>
#include <limits>
#include <ostream>
#include <tuple>
#include <type_traits>

#include "absl/strings/string_view.h"
#include "ortools/base/array.h"
#include "ortools/math_opt/elemental/arrays.h"
#include "ortools/math_opt/elemental/elements.h"
#include "ortools/math_opt/elemental/symmetry.h"

namespace operations_research::math_opt {

// A base class for all attribute type descriptors.
// `ValueTypeT` is the attribute value type, and `n` is the number of key
// elements (e.g. `Double2` attribute has `ValueType` == `double` and `n` == 2).
// This uses
// [CRTP](https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern) in
// `Impl` to deduce common descriptor properties from `Impl`. `Impl` must
// inherit from `AttrTypeDescriptor` and define the following entities:
//   - `static constexpr absl::string_view kName`: The name of the attribute
//     type.
//   - `enum class AttrType`: The attribute type, with `k` enumerators
//     corresponding to attributes for this type. Enumerators must be numbered
//     `0..(k-1)` (a good way to do this is to leave them unnumbered).
//   - `std::array<AttrDescriptor, k> kAttrDescriptors`: A descriptor for each
//     of the `k` attributes for this type.
template <typename ValueTypeT, int n, typename SymmetryT, typename Impl>
struct AttrTypeDescriptor {
  // The type of attribute values (e.g. `bool`, `int64_t`, `double`).
  using ValueType = ValueTypeT;

  // The number of key elements.
  static constexpr int kNumKeyElements = n;

  // The key symmetry. For example, this can be used to enforce that
  // quadratic objective coefficients are the same for `(i, j)` and `(j, i)`
  // (see `kObjQuadCoef` below).
  using Symmetry = SymmetryT;

  // A descriptor of an attribute of this attribute type.
  // E.g., this could describe the attribute `DoubleAttr1::kVarLb`.
  struct AttrDescriptor {
    // The name of the attribute value.
    absl::string_view name;
    // The default value.
    ValueType default_value;
    // The types of the `n` key elements.
    std::array<ElementType, n> key_types;
  };

  // Returns the number of attributes of this attribute type.
  static constexpr int NumAttrs() { return Impl::kAttrDescriptors.size(); }

  // Returns an array with all attributes of this attribute type.
  static constexpr auto Enumerate() {
    std::array<typename Impl::AttrType, NumAttrs()> result;
    for (int i = 0; i < NumAttrs(); ++i) {
      result[i] = {static_cast<typename Impl::AttrType>(i)};
    }
    return result;
  }
};

struct BoolAttr0TypeDescriptor
    : public AttrTypeDescriptor<bool, 0, NoSymmetry, BoolAttr0TypeDescriptor> {
  static constexpr absl::string_view kName = "BoolAttr0";

  enum class AttrType { kMaximize };

  static constexpr auto kAttrDescriptors = gtl::to_array<AttrDescriptor>(
      {{.name = "maximize", .default_value = false, .key_types = {}}});
};

struct BoolAttr1TypeDescriptor
    : public AttrTypeDescriptor<bool, 1, NoSymmetry, BoolAttr1TypeDescriptor> {
  static constexpr absl::string_view kName = "BoolAttr1";

  enum class AttrType {
    kVarInteger,
    kAuxObjMaximize,
    kIndConActivateOnZero,
  };

  static constexpr auto kAttrDescriptors = gtl::to_array<AttrDescriptor>(
      {{.name = "variable_integer",
        .default_value = false,
        .key_types = {ElementType::kVariable}},
       {.name = "auxiliary_objective_maximize",
        .default_value = false,
        .key_types = {ElementType::kAuxiliaryObjective}},
       {.name = "indicator_constraint_activate_on_zero",
        .default_value = false,
        .key_types = {ElementType::kIndicatorConstraint}}});
};

struct IntAttr0TypeDescriptor
    : public AttrTypeDescriptor<int64_t, 0, NoSymmetry,
                                IntAttr0TypeDescriptor> {
  static constexpr absl::string_view kName = "IntAttr0";

  enum class AttrType {
    kObjPriority,
  };

  static constexpr auto kAttrDescriptors = gtl::to_array<AttrDescriptor>({
      {.name = "objective_priority", .default_value = 0, .key_types = {}},
  });
};

struct IntAttr1TypeDescriptor
    : public AttrTypeDescriptor<int64_t, 1, NoSymmetry,
                                IntAttr1TypeDescriptor> {
  static constexpr absl::string_view kName = "IntAttr1";

  enum class AttrType {
    kAuxObjPriority,
  };

  static constexpr auto kAttrDescriptors = gtl::to_array<AttrDescriptor>({
      {.name = "auxiliary_objective_priority",
       .default_value = 0,
       .key_types = {ElementType::kAuxiliaryObjective}},
  });
};

struct DoubleAttr0TypeDescriptor
    : public AttrTypeDescriptor<double, 0, NoSymmetry,
                                DoubleAttr0TypeDescriptor> {
  static constexpr absl::string_view kName = "DoubleAttr0";

  enum class AttrType { kObjOffset };

  static constexpr auto kAttrDescriptors = gtl::to_array<AttrDescriptor>(
      {{.name = "objective_offset", .default_value = 0.0, .key_types = {}}});
};

struct DoubleAttr1TypeDescriptor
    : public AttrTypeDescriptor<double, 1, NoSymmetry,
                                DoubleAttr1TypeDescriptor> {
  static constexpr absl::string_view kName = "DoubleAttr1";

  enum class AttrType {
    kVarLb,
    kVarUb,
    kObjLinCoef,
    kLinConLb,
    kLinConUb,
    kAuxObjOffset,
    kQuadConLb,
    kQuadConUb,
    kIndConLb,
    kIndConUb,
  };

  static constexpr auto kAttrDescriptors = gtl::to_array<AttrDescriptor>({
      {.name = "variable_lower_bound",
       .default_value = -std::numeric_limits<double>::infinity(),
       .key_types = {ElementType::kVariable}},
      {.name = "variable_upper_bound",
       .default_value = std::numeric_limits<double>::infinity(),
       .key_types = {ElementType::kVariable}},
      {.name = "objective_linear_coefficient",
       .default_value = 0.0,
       .key_types = {ElementType::kVariable}},
      {.name = "linear_constraint_lower_bound",
       .default_value = -std::numeric_limits<double>::infinity(),
       .key_types = {ElementType::kLinearConstraint}},
      {.name = "linear_constraint_upper_bound",
       .default_value = std::numeric_limits<double>::infinity(),
       .key_types = {ElementType::kLinearConstraint}},
      {.name = "auxiliary_objective_offset",
       .default_value = 0.0,
       .key_types = {ElementType::kAuxiliaryObjective}},
      {.name = "quadratic_constraint_lower_bound",
       .default_value = -std::numeric_limits<double>::infinity(),
       .key_types = {ElementType::kQuadraticConstraint}},
      {.name = "quadratic_constraint_upper_bound",
       .default_value = std::numeric_limits<double>::infinity(),
       .key_types = {ElementType::kQuadraticConstraint}},
      {.name = "indicator_constraint_lower_bound",
       .default_value = -std::numeric_limits<double>::infinity(),
       .key_types = {ElementType::kIndicatorConstraint}},
      {.name = "indicator_constraint_upper_bound",
       .default_value = std::numeric_limits<double>::infinity(),
       .key_types = {ElementType::kIndicatorConstraint}},
  });
};

struct DoubleAttr2TypeDescriptor
    : public AttrTypeDescriptor<double, 2, NoSymmetry,
                                DoubleAttr2TypeDescriptor> {
  static constexpr absl::string_view kName = "DoubleAttr2";

  enum class AttrType {
    kLinConCoef,
    kAuxObjLinCoef,
    kQuadConLinCoef,
    kIndConLinCoef
  };

  static constexpr auto kAttrDescriptors = gtl::to_array<AttrDescriptor>({
      {.name = "linear_constraint_coefficient",
       .default_value = 0.0,
       .key_types = {ElementType::kLinearConstraint, ElementType::kVariable}},
      {.name = "auxiliary_objective_linear_coefficient",
       .default_value = 0.0,
       .key_types = {ElementType::kAuxiliaryObjective, ElementType::kVariable}},
      {.name = "quadratic_constraint_linear_coefficient",
       .default_value = 0.0,
       .key_types = {ElementType::kQuadraticConstraint,
                     ElementType::kVariable}},
      {.name = "indicator_constraint_linear_coefficient",
       .default_value = 0.0,
       .key_types = {ElementType::kIndicatorConstraint,
                     ElementType::kVariable}},
  });
};

struct SymmetricDoubleAttr2TypeDescriptor
    : public AttrTypeDescriptor<double, 2, ElementSymmetry<0, 1>,
                                SymmetricDoubleAttr2TypeDescriptor> {
  static constexpr absl::string_view kName = "SymmetricDoubleAttr2";

  enum class AttrType {
    kObjQuadCoef,
  };

  static constexpr auto kAttrDescriptors = gtl::to_array<AttrDescriptor>({
      {.name = "objective_quadratic_coefficient",
       .default_value = 0.0,
       .key_types = {ElementType::kVariable, ElementType::kVariable}},
  });
};

// Note: For this type, we pick the symmetric elements to be the last 2 elements
// of the key (index 1 and 2).
struct SymmetricDoubleAttr3TypeDescriptor
    : public AttrTypeDescriptor<double, 3, ElementSymmetry<1, 2>,
                                SymmetricDoubleAttr3TypeDescriptor> {
  static constexpr absl::string_view kName = "SymmetricDoubleAttr3";

  enum class AttrType {
    kQuadConQuadCoef,
  };

  static constexpr auto kAttrDescriptors = gtl::to_array<AttrDescriptor>({
      {.name = "quadratic_constraint_quadratic_coefficient",
       .default_value = 0.0,
       .key_types = {ElementType::kQuadraticConstraint, ElementType::kVariable,
                     ElementType::kVariable}},
  });
};

struct VariableAttr1TypeDescriptor
    : public AttrTypeDescriptor<VariableId, 1, NoSymmetry,
                                VariableAttr1TypeDescriptor> {
  static constexpr absl::string_view kName = "VariableAttr1";

  enum class AttrType {
    kIndConIndicator,
  };

  static constexpr auto kAttrDescriptors = gtl::to_array<AttrDescriptor>({
      {.name = "indicator_constraint_indicator",
       .default_value = VariableId(),
       .key_types = {ElementType::kIndicatorConstraint}},
  });
};

// The list of all available attribute descriptors. This is typically
// manipulated using the `AllAttrs` helper in `derived_data.h`.
using AllAttrTypeDescriptors =
    std::tuple<BoolAttr0TypeDescriptor, BoolAttr1TypeDescriptor,
               IntAttr0TypeDescriptor, IntAttr1TypeDescriptor,
               DoubleAttr0TypeDescriptor, DoubleAttr1TypeDescriptor,
               DoubleAttr2TypeDescriptor, SymmetricDoubleAttr2TypeDescriptor,
               SymmetricDoubleAttr3TypeDescriptor, VariableAttr1TypeDescriptor>;

// Aliases for types.
using BoolAttr0 = BoolAttr0TypeDescriptor::AttrType;
using BoolAttr1 = BoolAttr1TypeDescriptor::AttrType;
using IntAttr0 = IntAttr0TypeDescriptor::AttrType;
using IntAttr1 = IntAttr1TypeDescriptor::AttrType;
using DoubleAttr0 = DoubleAttr0TypeDescriptor::AttrType;
using DoubleAttr1 = DoubleAttr1TypeDescriptor::AttrType;
using DoubleAttr2 = DoubleAttr2TypeDescriptor::AttrType;
using SymmetricDoubleAttr2 = SymmetricDoubleAttr2TypeDescriptor::AttrType;
using SymmetricDoubleAttr3 = SymmetricDoubleAttr3TypeDescriptor::AttrType;
using VariableAttr1 = VariableAttr1TypeDescriptor::AttrType;

// Returns the index of `AttrT` in `AllAttrTypes` if `AttrT` is an attribute
// type, -1 otherwise.
template <typename AttrT>
static constexpr int GetIndexIfAttr() {
  using Tuple = AllAttrTypeDescriptors;
  // NOLINTNEXTLINE(clang-diagnostic-pre-c++20-compat)
  return ApplyOnIndexRange<std::tuple_size_v<Tuple>>([]<int... i>() {
    return ((std::is_same_v<std::remove_cv_t<std::remove_reference_t<AttrT>>,
                            typename std::tuple_element_t<i, Tuple>::AttrType>
                 ? (i + 1)
                 : 0) +
            ... + -1);
  });
}

template <typename AttrT,
          typename = std::enable_if_t<(GetIndexIfAttr<AttrT>() >= 0)>>
absl::string_view ToString(const AttrT attr) {
  using Descriptor =
      std::tuple_element_t<GetIndexIfAttr<AttrT>(), AllAttrTypeDescriptors>;
  const int attr_index = static_cast<int>(attr);
  return Descriptor::kAttrDescriptors[attr_index].name;
}

template <typename Sink, typename AttrT,
          typename = std::enable_if_t<(GetIndexIfAttr<AttrT>() >= 0)>>
void AbslStringify(Sink& sink, const AttrT attr_type) {
  sink.Append(ToString(attr_type));
}

template <typename AttrT>
std::enable_if_t<(GetIndexIfAttr<AttrT>() >= 0), std::ostream&> operator<<(
    std::ostream& ostr, AttrT attr) {
  ostr << ToString(attr);
  return ostr;
}

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_ELEMENTAL_ATTRIBUTES_H_
