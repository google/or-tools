// Copyright 2010-2021 Google LLC
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

// This header defines the common properties of "key types" and some related
// constants.
//
// MathOpt provides optimized custom collections for variables and
// constraints. This file contains implementation details for these custom
// collections and should not be needed by users.
//
// Key types are types that are used as identifiers in the C++ interface where
// the ModelStorage is using typed integers. They are pairs of (storage,
// typed_index) where `storage` is a pointer on an ModelStorage and
// `typed_index` is the typed integer type used in ModelStorage.
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
//
// These requirements are met by Variable and LinearConstraint.
#ifndef OR_TOOLS_MATH_OPT_CPP_KEY_TYPES_H_
#define OR_TOOLS_MATH_OPT_CPP_KEY_TYPES_H_

#include <initializer_list>

#include "absl/strings/string_view.h"
#include "ortools/base/logging.h"
#include "ortools/math_opt/core/model_storage.h"

namespace operations_research {
namespace math_opt {
namespace internal {

// The CHECK message to use when a KeyType::storage() is nullptr.
inline constexpr absl::string_view kKeyHasNullModelStorage =
    "The input key has null .storage().";

// The CHECK message to use when two KeyType with different storage() are used
// in the same collection.
inline constexpr absl::string_view kObjectsFromOtherModelStorage =
    "The input objects belongs to another model.";

// CHECKs that the non-null models the same, and returns the unique non-null
// model storage if it exists, otherwise null.
inline const ModelStorage* ConsistentModelStorage(
    std::initializer_list<const ModelStorage*> storages) {
  const ModelStorage* result = nullptr;
  for (const ModelStorage* const storage : storages) {
    if (storage != nullptr) {
      if (result == nullptr) {
        result = storage;
      } else {
        CHECK_EQ(storage, result) << internal::kObjectsFromOtherModelStorage;
      }
    }
  }
  return result;
}

}  // namespace internal
}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_CPP_KEY_TYPES_H_
