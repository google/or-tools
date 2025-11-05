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

#ifndef ORTOOLS_MATH_OPT_STORAGE_MODEL_STORAGE_ITEM_H_
#define ORTOOLS_MATH_OPT_STORAGE_MODEL_STORAGE_ITEM_H_

#include <cstdint>
#include <ostream>
#include <type_traits>
#include <utility>

#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "ortools/math_opt/elemental/elements.h"
#include "ortools/math_opt/storage/model_storage.h"

namespace operations_research::math_opt {

// Represents an item that is stored in the ModelStorage.
// ModelStorageItem is a value type.
class ModelStorageItem {
 public:
  ModelStorageCPtr storage() const { return storage_; }

 protected:
  explicit ModelStorageItem(ModelStorageCPtr storage) : storage_(storage) {}

  // Disallow slicing
  // (https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rc-copy-virtual):
  ModelStorageItem(const ModelStorageItem&) = default;
  ModelStorageItem& operator=(const ModelStorageItem&) = default;
  ModelStorageItem(ModelStorageItem&&) = default;
  ModelStorageItem& operator=(ModelStorageItem&&) = default;

  ~ModelStorageItem() = default;

 private:
  ModelStorageCPtr storage_;
};

// Whether `ModelStorageElement` should define equality operators.
enum class ModelStorageElementEquality {
  kWithEquality,
  kWithoutEquality,
};

namespace internal {
void FormatModelStorageElement(std::ostream& ostr, ElementType element_type,
                               absl::string_view name, int64_t id);
}  // namespace internal

// Typed model storage item. All elemental types (Variables, LinearConstraint,
// ...) in the model derived from this. Some additional concepts are currently
// not typed (and derive from `ModelStorageItem` instead):
//  - SOS constraints: we'll migrate them to `ModelStorageElement` once they
//    are implemented in Elemental.
//  - `QuadraticTermKey` is only used transactionally to build the model, it's
//    not stored in the model.
//  - `Objective` is conceptually typed, but exposes a specific, optional-based
//    API to discriminate between the primary and secondaries objectives.
// `ModelStorageElement` is a value type and implements
// https://abseil.io/docs/cpp/guides/hash.
template <ElementType element_type, typename Derived,
          ModelStorageElementEquality generate_equality =
              ModelStorageElementEquality::kWithEquality>
class ModelStorageElement : public ModelStorageItem {
 public:
  // The typed integer used for ids.
  using IdType = ElementId<element_type>;

  ModelStorageElement(ModelStorageCPtr storage, IdType id)
      : ModelStorageItem(storage), id_(id) {}

  // TODO(b/395843100): Remove this, use `typed_id()` or `typed_id().value()`
  // instead.
  int64_t id() const { return id_.value(); }

  IdType typed_id() const { return id_; }

  template <typename H>
  friend H AbslHashValue(H h, const Derived& element) {
    return H::combine(std::move(h), element.id_.value(), element.storage());
  }

  template <ModelStorageElementEquality equality = generate_equality>
  friend std::enable_if_t<
      equality == ModelStorageElementEquality::kWithEquality, bool>
  operator==(const Derived& lhs, const Derived& rhs) {
    return lhs.id_ == rhs.id_ && lhs.storage() == rhs.storage();
  }

  template <ModelStorageElementEquality equality = generate_equality>
  friend std::enable_if_t<
      equality == ModelStorageElementEquality::kWithEquality, bool>
  operator!=(const Derived& lhs, const Derived& rhs) {
    return !(lhs == rhs);
  }

  // Note: for unnamed elements, we print the element type and id, but we don't
  // commit to the exect format.
  friend std::ostream& operator<<(std::ostream& ostr, const Derived& element) {
    internal::FormatModelStorageElement(ostr, element_type, element.name(),
                                        element.typed_id().value());
    return ostr;
  }

 protected:
  // Disallow slicing
  // (https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rc-copy-virtual):
  ModelStorageElement(const ModelStorageElement&) = default;
  ModelStorageElement& operator=(const ModelStorageElement&) = default;
  ModelStorageElement(ModelStorageElement&&) = default;
  ModelStorageElement& operator=(ModelStorageElement&&) = default;

  ~ModelStorageElement() = default;

 private:
  IdType id_;
};

// A trait that detects whether a type derives from `ModelStorageElement<e>` for
// some `e`.
template <typename T>
struct is_model_storage_element {
 private:
  template <ElementType element_type, typename Derived,
            ModelStorageElementEquality eq>
  static constexpr bool IsModelStorageElement(
      const ModelStorageElement<element_type, Derived, eq>*) {
    return true;
  }

  static constexpr bool IsModelStorageElement(...) { return false; }

 public:
  static constexpr bool value =
      IsModelStorageElement(static_cast<const T*>(nullptr));
};

// Represents an item that contains a bunch of items that live in the same model
// storage. The container is considered to be associated to a given model iff it
// has at least one item. Derived classes should maintain this invariant.
// In particular, they should call `SetOrCheckStorage` when an item is added and
// they should clear the storage when becoming empty (this includes being moved
// from if that clears the items in the container, see comments on the move
// constructor).
class ModelStorageItemContainer {
 public:
  // Returns `nullptr` if the container is not associated with any model
  // (`SetOrCheckStorage` has never been called, or the container was
  // emptied/moved from).
  NullableModelStorageCPtr storage() const { return storage_; }

 protected:
  explicit ModelStorageItemContainer(
      const NullableModelStorageCPtr storage = nullptr)
      : storage_(storage) {}

  // Disallow slicing
  // (https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rc-copy-virtual):
  ModelStorageItemContainer(const ModelStorageItemContainer& other) = default;
  ModelStorageItemContainer& operator=(const ModelStorageItemContainer& other) =
      default;

  // When moving we're leaving the moved-from object unassociated with any
  // model. Derived classes should hold no items after being moved from.
  ModelStorageItemContainer(ModelStorageItemContainer&& other) noexcept
      : storage_(std::exchange(other.storage_, nullptr)) {}
  ModelStorageItemContainer& operator=(
      ModelStorageItemContainer&& other) noexcept {
    storage_ = std::exchange(other.storage_, nullptr);
    return *this;
  }

  // Sets the storage_ to the input value if nullptr, else CHECKs that `item`
  // is associated with the same storage.
  void SetOrCheckStorage(const ModelStorageItem& item) {
    SetOrCheckStorageImpl(item.storage());
  }

  // Same as above, but additionally checks that the input container is already
  // associated with a storage.
  void SetOrCheckStorage(const ModelStorageItemContainer& container) {
    CHECK(container.storage() != nullptr) << "appending an empty container";
    SetOrCheckStorageImpl(container.storage());
  }

 private:
  void SetOrCheckStorageImpl(const ModelStorageCPtr storage) {
    if (storage_ != nullptr) {
      CHECK_EQ(storage, storage_)
          << "The input objects belongs to another model.";
      return;
    }
    storage_ = storage;
  }

  // We start not associated with any model storage.
  NullableModelStorageCPtr storage_;
};

}  // namespace operations_research::math_opt

#endif  // ORTOOLS_MATH_OPT_STORAGE_MODEL_STORAGE_ITEM_H_
