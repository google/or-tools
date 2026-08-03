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

#ifndef UTIL_GRAPH_INDEX_H_
#define UTIL_GRAPH_INDEX_H_

#include <concepts>
#include <cstddef>
#include <cstring>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/graph_base/hash_or_tree_container.h"

namespace util {
namespace graph {

// Stores a set of unique objects in a vector and provides O(1) lookup by value,
// if T is hashable (else O(log n)).
// In other words, this maps arbitrary objects to dense integers, fast.
template <typename T, typename CompareOrHashT = PreferHashOrCompare<T>,
          typename Eq = std::equal_to<>>
class Index {
 public:
  typedef T value_type;
  explicit Index(int reserve_num_objects = 0,
                 CompareOrHashT compare_or_hash = CompareOrHashT(),
                 Eq eq = Eq());

  int LookupOrAdd(T object) { return TryEmplace(std::move(object)).first; }
  std::optional<int> Lookup(const T& object) const {
    return impl_->Lookup(object);
  }

  // DCHECKs that the index is valid.
  const T& operator[](int index) const { return impl_->operator[](index); }

  absl::Span<const T> span() const { return impl_->objects_; }
  int size() const { return span().size(); }

  // Like LookupOrAdd(), but also returns whether the object was newly inserted.
  template <typename U>
  std::pair<int, bool> TryEmplace(U&& object) {
    return impl_->TryEmplace(std::forward<U>(object));
  }

  // For direct range iteration.
  absl::Span<const T>::iterator begin() const { return span().begin(); }
  absl::Span<const T>::iterator end() const { return span().end(); }

  void clear() { impl_->clear(); }

  // Move out the underlying vector<T>. *this must be being moved.
  // EXAMPLE: std::vector<T> v = std::move(index).Extract();
  std::vector<T> Extract() && { return std::move(impl_->objects_); }

 private:
  // We use an indirection via unique_ptr<Impl> to make *this movable, because
  // Impl requires pointer stability of `this` across moves.
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

namespace internal {
template <typename H, typename ValueT>
concept LooksLikeHasher = requires(const H& h, const ValueT& v) {
  { h(v) } -> std::integral;
};
}  // namespace internal

// Implementation of `Index`. Conceptually this is a `vector<T>` of objects, and
// a set of `T*` (represented has a hash set or ordered set depending on
// `CompareOrHashT`). A subtlety is that instead of storing pointers as `T*`, we
// store indices into the vector of objects, from which we can retrieve the
// actual object pointers. Adding a new object to the vector invalidates
// pointers, but not indices. Storing indices therefore allows adding new
// objects to the vector without having to touch other entries in the set.
//
// The indices are represented in a type-safe way as `ObjectIndex`.
// We rely on heterogeneous lookup (https://abseil.io/tips/144) to allow lookups
// into the set by `const T& t`, while the actual set keys are `ObjectIndex`:
// we simply need to compare the object `t` with the object at corresponding
// index in the vector.
// More generally, when the functors `CompareOrHashT` and `Eq` are transparent,
// lookups are allowed using any key type `U&&` that can be transparently
// compared with `T`.
//
// For insertions (see `TryEmplace`), the situation is a a bit more complex.
// We want to be able to do the lookup and insertion in one step using
// `try_emplace`, without constructing unnecessary temporaries. So we need both
// an index (in case the object is not present and needs to be inserted) and a
// `U&&` lookup key. This is represented by `EmplaceableObject`, whis is a
// temporary value-typed object that acts as a lookup key but converts to an
// `ObjectIndex`, inserting a new `T` in the vector when the set decides that it
// needs to be inserted.
template <typename T, typename CompareOrHashT, typename Eq>
class Index<T, CompareOrHashT, Eq>::Impl {
 public:
  // A temporary object to represent a potential new object in the index.
  template <typename U>
  struct EmplaceableObject {
    static constexpr bool kIsEmplaceableObject = true;

    const U& key_ref() const { return key; }

    U&& key;  // The lookup key (might be different from T if transparent).
    int index_if_inserted;  // The index to give the newly emplaced object.
    Impl* const impl;
  };

  class ObjectIndex {
   public:
    using ValueType = int;

    template <typename U>
    explicit ObjectIndex(EmplaceableObject<U> emplaceable)
        : ObjectIndex(emplaceable.index_if_inserted) {
      // We need to emplace the object here instead of in `TryInsert`, because
      // in debug mode absl containers immediately rehash/compare the keys to
      // check invariants, and looking up by `ObjectIndex` will try to look up
      // by index, so the object needs to already be inserted.
      emplaceable.impl->objects_.emplace_back(std::forward<U>(emplaceable.key));
    }

    explicit ObjectIndex(ValueType index) {
      memcpy(index_, &index, sizeof(ValueType));
    }

    ValueType index() const {
      ValueType index;
      memcpy(&index, index_, sizeof(ValueType));
      return index;
    }

   private:
    char index_[sizeof(ValueType)];
  };

  // To support heterogeneous lookup, we need to wrap the functors. See usage.
  struct FunctorWrapper;
  struct CompareOrHashTWrapper;
  struct EqWrapper;
  struct Empty {};

  // TODO(user): Ideally this would be a `Set`, but
  // guaranteed-not-to-construct emplace is only supported for maps through
  // `try_emplace`.
  using Container =
      typename HashOrTreeContainer<ObjectIndex, CompareOrHashTWrapper,
                                   EqWrapper>::template Map<Empty>;

  static_assert(sizeof(typename Container::value_type) ==
                sizeof(typename ObjectIndex::ValueType) + 1);
  static_assert(alignof(typename Container::value_type) == 1);

  static Container MakeContainer(Impl* impl, int reserve_num_objects,
                                 CompareOrHashT compare_or_hash, Eq eq) {
    if constexpr (internal::LooksLikeHasher<CompareOrHashT, T>) {
      return Container(
          reserve_num_objects,
          CompareOrHashTWrapper{{impl}, std::move(compare_or_hash)},
          EqWrapper{{impl}, std::move(eq)});
    } else {
      return Container(
          CompareOrHashTWrapper{{impl}, std::move(compare_or_hash)});
    }
  }

  explicit Impl(int reserve_num_objects, CompareOrHashT compare_or_hash, Eq eq)
      : index_(MakeContainer(this, reserve_num_objects,
                             std::move(compare_or_hash), std::move(eq))) {
    objects_.reserve(reserve_num_objects);
  }

  // Replication of the public API.
  template <typename U>
  std::pair<int, bool> TryEmplace(U&& u) {
    const auto [it, inserted] = index_.try_emplace(EmplaceableObject<U>{
        .key = std::forward<U>(u),
        .index_if_inserted = static_cast<int>(objects_.size()),
        .impl = this});
    return std::make_pair(it->first.index(), inserted);
  }

  std::optional<int> Lookup(const T& object) const {
    auto it = index_.find(object);
    if (it == index_.end()) return std::nullopt;
    return it->first.index();
  }

  const T& operator[](int index) const {
    DCHECK_GE(index, 0);
    DCHECK_LT(index, objects_.size());
    return objects_[index];
  }

  void clear() {
    index_.clear();
    objects_.clear();
  }

  // Contains all inserted objects. Natural map from dense int to object.
  std::vector<T> objects_;

  // Behaves like a map<T, int>, but without storing the object T again, which
  // are already stored in 'objects_'. Instead, we store the dense index of
  // each object, and use custom hash/comparison/eq functors that uses
  // objects_[i] to convert int i before hashing/comparing it.
  Container index_;
};

template <typename T, typename CompareOrHashT, typename Eq>
Index<T, CompareOrHashT, Eq>::Index(int reserve_num_objects,
                                    CompareOrHashT compare_or_hash, Eq eq)
    : impl_(std::make_unique<Impl>(
          reserve_num_objects, std::move(compare_or_hash), std::move(eq))) {}

template <typename T, typename CompareOrHashT, typename Eq>
struct Index<T, CompareOrHashT, Eq>::Impl::FunctorWrapper {
  using is_transparent = void;

  template <typename KeyT>
  decltype(auto) GetKey(const KeyT& k) const {
    if constexpr (std::is_same_v<KeyT, ObjectIndex>) {
      // This is the index of an object in our vector of objects.
      return impl->objects_[k.index()];
    } else if constexpr (requires { k.kIsEmplaceableObject; }) {
      // This is an EmplaceableObject, use the transparent lookup key.
      return k.key_ref();
    } else {
      // This is a transparent lookup key.
      return k;
    }
  }
  const Index::Impl* impl = nullptr;
};

template <typename T, typename CompareOrHashT, typename Eq>
struct Index<T, CompareOrHashT, Eq>::Impl::CompareOrHashTWrapper
    : public FunctorWrapper {
  // If CompareOrHashT is a hasher, here's its heterogeneous lookup API.
  template <typename KeyT>
  size_t operator()(const KeyT& k) const
    requires internal::LooksLikeHasher<CompareOrHashT, T>
  {
    return compare_or_hash(FunctorWrapper::GetKey(k));
  }

  // If CompareOrHashT is a comparator, here's its heterogeneous lookup API.
  template <typename KeyT1, typename KeyT2>
  bool operator()(const KeyT1& a, const KeyT2& b) const
    requires(!internal::LooksLikeHasher<CompareOrHashT, T>)
  {
    return compare_or_hash(FunctorWrapper::GetKey(a),
                           FunctorWrapper::GetKey(b));
  }

  [[no_unique_address]] CompareOrHashT compare_or_hash;
};

template <typename T, typename CompareOrHashT, typename Eq>
struct Index<T, CompareOrHashT, Eq>::Impl::EqWrapper : public FunctorWrapper {
  template <typename KeyT1, typename KeyT2>
  bool operator()(const KeyT1& a, const KeyT2& b) const {
    return eq(FunctorWrapper::GetKey(a), FunctorWrapper::GetKey(b));
  }

  [[no_unique_address]] Eq eq;
};

}  // namespace graph
}  // namespace util

#endif  // UTIL_GRAPH_INDEX_H_
