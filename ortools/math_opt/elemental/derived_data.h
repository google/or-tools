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

#ifndef OR_TOOLS_MATH_OPT_ELEMENTAL_DERIVED_DATA_H_
#define OR_TOOLS_MATH_OPT_ELEMENTAL_DERIVED_DATA_H_

#include <array>
#include <string>
#include <tuple>
#include <type_traits>

#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "ortools/math_opt/elemental/arrays.h"
#include "ortools/math_opt/elemental/attr_key.h"
#include "ortools/math_opt/elemental/attributes.h"
#include "ortools/math_opt/elemental/elements.h"
#include "ortools/util/fp_roundtrip_conv.h"

namespace operations_research::math_opt {

// A helper to manipulate the list of attributes.
struct AllAttrs {
  // The number of available attribute types.
  static constexpr int kNumAttrTypes =
      std::tuple_size_v<AllAttrTypeDescriptors>;

  // Returns the descriptor of the `i-th` attribute type in the list.
  template <int i>
  using TypeDescriptor = std::tuple_element_t<i, AllAttrTypeDescriptors>;

  // Returns the `i-th` attribute type in the list.
  template <int i>
  using Type = typename TypeDescriptor<i>::AttrType;

  // Returns the index of attribute type `AttrT`.
  // Fails to compile if `AttrT` is not an attribute.
  template <typename AttrT>
  static constexpr int GetIndex() {
    constexpr int index = GetIndexIfAttr<AttrT>();
    // This weird construct is to show `AttrT` explicitly instead of letting
    // the user fish it out of the stack trace when the static_assert fails.
    static_assert(
        std::is_const_v<std::conditional_t<(index >= 0), const AttrT, AttrT>>,
        "no such attribute");
    return index;
  }

  // Applies `fn` on each value for each attribute type. `fn` must have a
  // overload set of `operator(AttrType)` that accepts a `AttrType` for
  // each attribute type.
  template <typename Fn>
  static void ForEachAttr(Fn&& fn) {
    ForEach(
        [&fn](const auto& descriptor) {
          for (auto attr : descriptor.Enumerate()) {
            fn(attr);
          }
        },
        AllAttrTypeDescriptors{});
  }
};

// Returns the descriptor for attribute `AttrT`.
template <typename AttrT>
using AttrTypeDescriptorT =
    AllAttrs::TypeDescriptor<AllAttrs::GetIndex<AttrT>()>;

// Returns the default value for the attribute type `attr`.
//
// For example GetAttrDefaultValue<DoubleAttr2::kLinConCoef>() returns 0.0.
template <auto attr>
constexpr typename AttrTypeDescriptorT<decltype(attr)>::ValueType
GetAttrDefaultValue() {
  return AttrTypeDescriptorT<decltype(attr)>::kAttrDescriptors[static_cast<int>(
                                                                   attr)]
      .default_value;
}

// Returns the number of elements in a key for the attribute type `AttrType`.
//
// For example `GetAttrKeySize<DoubleAttr2>()` returns 2.
template <typename AttrType>
constexpr int GetAttrKeySize() {
  return AttrTypeDescriptorT<AttrType>::kNumKeyElements;
}
template <auto attr>
constexpr int GetAttrKeySize() {
  return GetAttrKeySize<decltype(attr)>();
}

// The type of the `AttrKey` for attribute type `AttrType`.
template <typename AttrType>
using AttrKeyFor = AttrKey<AttrTypeDescriptorT<AttrType>::kNumKeyElements,
                           typename AttrTypeDescriptorT<AttrType>::Symmetry>;

// The value type for attribute type `AttrType`.
template <typename AttrType>
using ValueTypeFor = typename AttrTypeDescriptorT<AttrType>::ValueType;

// Returns the array of elements for the key for the attribute type `attr`.
//
// For example, GetElementTypes<DoubleAttr2>() returns the array
// {ElementType::kLinearConstraint, ElementType::kVariable}.
template <typename AttrType>
constexpr std::array<ElementType, GetAttrKeySize<AttrType>()> GetElementTypes(
    const AttrType attr) {
  return AttrTypeDescriptorT<AttrType>::kAttrDescriptors[static_cast<int>(attr)]
      .key_types;
}
template <auto attr>
constexpr std::array<ElementType, GetAttrKeySize<attr>()> GetElementTypes() {
  return GetElementTypes(attr);
}

// After C++20, this can be replaced by a lambda. C++17 does not allow lambdas
// in unevaluated contexts.
template <template <int i> typename ValueType>
struct EnumeratedTupleCpp17Helper {
  template <int... i>
  auto operator()() const {
    return std::make_tuple(ValueType<i>()...);
  }
};

// A tuple of `ValueType<i>` for `i` in `0..n`.
template <int n, template <int i> typename ValueType>
using EnumeratedTuple =
    decltype(ApplyOnIndexRange<n>(EnumeratedTupleCpp17Helper<ValueType>{}));

// A map of attribute to `ValueType<i>`, where `i` is the index of the attribute
// type.
// See `AttrMapTest` for example usage.
// NOTE: this is formally a map (it maps attributes to values), but internally
// uses dense storage.
template <template <int i> typename ValueType>
class AttrMap {
 public:
  template <typename AttrT>
  ValueType<AllAttrs::GetIndex<AttrT>()>& operator[](AttrT a) {
    // TODO(b/365997645): post C++ 23, prefer `std::to_underlying(a)`.
    return std::get<AllAttrs::GetIndex<AttrT>()>(
        array_tuple_)[static_cast<int>(a)];
  }

  template <typename AttrT>
  const ValueType<AllAttrs::GetIndex<AttrT>()>& operator[](AttrT a) const {
    // The `const_cast` is fine because non-const `operator[]` does not mutate
    // anything and we're casting the return type back to const.
    return (*const_cast<AttrMap*>(this))[a];
  }

  // Applies `fn` on each value for each attribute type. `fn` must have a
  // overload set of `operator()` that accepts a `ValueType<i>` for `i` in
  // `0..AllAttrs::kSize`.
  // This cannot be an iterator because value types are not homogeneous.
  template <typename Fn>
  void ForEachAttrValue(Fn&& fn) {
    ForEach(
        [&fn](auto& array) {
          for (auto& value : array) {
            fn(value);
          }
        },
        array_tuple_);
  }

 private:
  template <int i>
  using ArrayType =
      std::array<ValueType<i>, AllAttrs::TypeDescriptor<i>::NumAttrs()>;
  EnumeratedTuple<AllAttrs::kNumAttrTypes, ArrayType> array_tuple_;
};

// Calls `fn<attr>()`.
template <typename AttrType, typename Fn, int n = 0>
decltype(auto) CallForAttr(AttrType attr, Fn&& fn) {
  using Descriptor = AttrTypeDescriptorT<AttrType>;
  if constexpr (n < Descriptor::NumAttrs()) {
    constexpr AttrType a = static_cast<AttrType>(n);
    if (a == attr) {
      return fn.template operator()<a>();
    }
    return CallForAttr<AttrType, Fn, n + 1>(attr, std::forward<Fn>(fn));
  } else {
    LOG(FATAL) << "impossible";
    return decltype(fn.template operator()<AttrType{}>()) {};
  }
}

template <typename ValueType>
std::string FormatAttrValue(const ValueType v) {
  return absl::StrCat(v);
}

template <>
inline std::string FormatAttrValue(const double v) {
  return RoundTripDoubleFormat::ToString(v);
}

template <>
inline std::string FormatAttrValue(const bool v) {
  return v ? "true" : "false";
}

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_ELEMENTAL_DERIVED_DATA_H_
