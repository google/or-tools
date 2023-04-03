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

// IWYU pragma: private, include "ortools/math_opt/cpp/math_opt.h"
// IWYU pragma: friend "ortools/math_opt/cpp/.*"

// This header defines the common properties of "key types" and some related
// constants.
//
// Key types are types that are used as identifiers in the C++ interface where
// the ModelStorage is using typed integers. They are pairs of (storage,
// typed_index) where `storage` is a pointer on an ModelStorage and
// `typed_index` is the typed integer type used in ModelStorage (or a pair of
// typed integers for QuadraticTermKey).
//
// A key type K must match the following requirements:
//   - K::IdType is a value type used for indices.
//   - K has a constructor K(const ModelStorage*, K::IdType).
//   - K is a value-semantic type.
//   - K has a function with signature `K::IdType K::typed_id() const`.
//   - K has a function with signature `const ModelStorage* K::storage() const`.
//     It must return a non-null pointer.
//   - K::IdType is a valid key for absl::flat_hash_map or absl::flat_hash_set
//     (supports hash and ==).
//   - the is_key_type_v<> below should include them.
#ifndef OR_TOOLS_MATH_OPT_CPP_KEY_TYPES_H_
#define OR_TOOLS_MATH_OPT_CPP_KEY_TYPES_H_

#include <type_traits>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/math_opt/storage/model_storage.h"

namespace operations_research::math_opt {

// Forward declarations of types implementing the keys interface defined at the
// top of this file.
class Variable;
class LinearConstraint;
class QuadraticConstraint;
class SecondOrderConeConstraint;
class Sos1Constraint;
class Sos2Constraint;
class IndicatorConstraint;
class QuadraticTermKey;
class Objective;

// True for types in MathOpt that implements the keys interface defined at the
// top of this file.
//
// This is used in conjunction with std::enable_if_t<> to prevent Argument
// Dependent Lookup (ADL) from selecting some overload defined in MathOpt
// because one of the template type is in MathOpt. For example the SortedKeys()
// function below could be selected as a valid overload in another namespace if
// the values in the hash map are in the math_opt namespace.
template <typename T>
constexpr inline bool is_key_type_v =
    (std::is_same_v<T, Variable> || std::is_same_v<T, LinearConstraint> ||
     std::is_same_v<T, QuadraticConstraint> ||
     std::is_same_v<T, SecondOrderConeConstraint> ||
     std::is_same_v<T, Sos1Constraint> || std::is_same_v<T, Sos2Constraint> ||
     std::is_same_v<T, IndicatorConstraint> ||
     std::is_same_v<T, QuadraticTermKey> || std::is_same_v<T, Objective>);

// Returns the keys of the map sorted by their (storage(), type_id()).
//
// Implementation note: here we must use std::enable_if to prevent Argument
// Dependent Lookup (ADL) from selecting this overload for maps which values are
// in MathOpt but keys are not.
template <typename Map,
          typename = std::enable_if_t<is_key_type_v<typename Map::key_type>>>
std::vector<typename Map::key_type> SortedKeys(const Map& map) {
  using K = typename Map::key_type;
  std::vector<K> result;
  result.reserve(map.size());
  for (const typename Map::const_reference item : map) {
    result.push_back(item.first);
  }
  absl::c_sort(result, [](const K& lhs, const K& rhs) {
    if (lhs.storage() != rhs.storage()) {
      return lhs.storage() < rhs.storage();
    }
    return lhs.typed_id() < rhs.typed_id();
  });
  return result;
}

// Returns the elements of the set sorted by their (storage(), type_id()).
//
// Implementation note: here we must use std::enable_if to prevent Argument
// Dependent Lookup (ADL) from selecting this overload for maps which values are
// in MathOpt but keys are not.
template <typename Set,
          typename = std::enable_if_t<is_key_type_v<typename Set::key_type>>>
std::vector<typename Set::key_type> SortedElements(const Set& set) {
  using K = typename Set::key_type;
  std::vector<K> result;
  result.reserve(set.size());
  for (const typename Set::const_reference item : set) {
    result.push_back(item);
  }
  absl::c_sort(result, [](const K& lhs, const K& rhs) {
    if (lhs.storage() != rhs.storage()) {
      return lhs.storage() < rhs.storage();
    }
    return lhs.typed_id() < rhs.typed_id();
  });
  return result;
}

// Returns the values corresponding to the keys. Keys must be present in the
// input map.
//
// The keys must be in a type convertible to absl::Span<const K>.
//
// Implementation note: here we must use std::enable_if to prevent Argument
// Dependent Lookup (ADL) from selecting this overload for maps which values are
// in MathOpt but keys are not.
template <typename Map, typename Keys,
          typename = std::enable_if_t<is_key_type_v<typename Map::key_type>>>
std::vector<typename Map::mapped_type> Values(const Map& map,
                                              const Keys& keys) {
  using K = typename Map::key_type;
  const absl::Span<const K> keys_span = keys;
  std::vector<typename Map::mapped_type> result;
  result.reserve(keys_span.size());
  for (const K& key : keys_span) {
    result.push_back(map.at(key));
  }
  return result;
}

namespace internal {

// The CHECK message to use when a KeyType::storage() is nullptr.
inline constexpr absl::string_view kKeyHasNullModelStorage =
    "The input key has null .storage().";

// The CHECK message to use when two KeyType with different storage() are used
// in the same collection.
inline constexpr absl::string_view kObjectsFromOtherModelStorage =
    "The input objects belongs to another model.";

// The Status message to use when an input KeyType is from an unexpected
// storage().
inline constexpr absl::string_view kInputFromInvalidModelStorage =
    "the input does not belong to the same model";

// Returns a failure when the input pointer is not nullptr and points to a
// different model storage than expected_storage (which must not be nullptr).
//
// Failure message is kInputFromInvalidModelStorage.
inline absl::Status CheckModelStorage(
    const ModelStorage* const storage,
    const ModelStorage* const expected_storage) {
  if (expected_storage == nullptr) {
    return absl::InternalError("expected_storage is nullptr");
  }
  if (storage != nullptr && storage != expected_storage) {
    return absl::InvalidArgumentError(internal::kInputFromInvalidModelStorage);
  }
  return absl::OkStatus();
}

}  // namespace internal
}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_CPP_KEY_TYPES_H_
