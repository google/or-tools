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

  int LookupOrAdd(T object) { return TryInsert(std::move(object)).first; }
  std::optional<int> Lookup(const T& object) const {
    return impl_->Lookup(object);
  }

  // DCHECKs that the index is valid.
  const T& operator[](int index) const { return impl_->operator[](index); }

  absl::Span<const T> span() const { return impl_->objects_; }
  int size() const { return span().size(); }

  // Like LookupOrAdd(), but also returns whether the object was newly inserted.
  std::pair<int, bool> TryInsert(T object) {
    return impl_->TryInsert(std::move(object));
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

template <typename T, typename CompareOrHashT, typename Eq>
class Index<T, CompareOrHashT, Eq>::Impl {
 public:
  // We need to wrap T object when looking them up, to differentiate from
  // looking up an int, when T is also of type int.
  struct LookupKey {
    const T& object;
  };

  // To support heterogeneous lookup, we need to wrap the functors. See usage.
  struct CompareOrHashTWrapper;
  struct EqWrapper;

  using Container =
      typename HashOrTreeContainer<int, CompareOrHashTWrapper, EqWrapper>::Set;

  static Container MakeContainer(Impl* impl, int reserve_num_objects,
                                 CompareOrHashT compare_or_hash, Eq eq) {
    if constexpr (internal::LooksLikeHasher<CompareOrHashT, T>) {
      return Container(reserve_num_objects,
                       CompareOrHashTWrapper{impl, std::move(compare_or_hash)},
                       EqWrapper{impl, std::move(eq)});
    } else {
      return Container(CompareOrHashTWrapper{impl, std::move(compare_or_hash)});
    }
  }

  explicit Impl(int reserve_num_objects, CompareOrHashT compare_or_hash, Eq eq)
      : index_(MakeContainer(this, reserve_num_objects,
                             std::move(compare_or_hash), std::move(eq))) {
    objects_.reserve(reserve_num_objects);
  }

  // Replication of the public API.
  std::pair<int, bool> TryInsert(T object) {
    const int i = objects_.size();
    objects_.push_back(std::move(object));
    auto result = index_.insert(i);
    if (result.second) return std::make_pair(i, true);
    // The object was already present. Remove it from the vector and return the
    // existing index.
    objects_.pop_back();
    return std::make_pair(*result.first, false);
  }

  std::optional<int> Lookup(const T& object) const {
    auto it = index_.find(LookupKey{object});
    if (it == index_.end()) return std::nullopt;
    return *it;
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
struct Index<T, CompareOrHashT, Eq>::Impl::CompareOrHashTWrapper {
  using is_transparent = void;

  // If CompareOrHashT is a hasher, here's its heterogeneous lookup API.
  size_t operator()(int index) const
    requires internal::LooksLikeHasher<CompareOrHashT, T>
  {
    return compare_or_hash(impl->objects_[index]);
  }
  size_t operator()(const LookupKey& key) const
    requires internal::LooksLikeHasher<CompareOrHashT, T>
  {
    return compare_or_hash(key.object);
  }

  // If CompareOrHashT is a comparator, here's its heterogeneous lookup API.
  bool operator()(int a, int b) const
    requires(!internal::LooksLikeHasher<CompareOrHashT, T>)
  {
    return compare_or_hash(impl->objects_[a], impl->objects_[b]);
  }
  bool operator()(int a, const LookupKey& b) const
    requires(!internal::LooksLikeHasher<CompareOrHashT, T>)
  {
    return compare_or_hash(impl->objects_[a], b.object);
  }
  bool operator()(const LookupKey& a, int b) const
    requires(!internal::LooksLikeHasher<CompareOrHashT, T>)
  {
    return compare_or_hash(a.object, impl->objects_[b]);
  }

  const Index::Impl* impl = nullptr;
  [[no_unique_address]] CompareOrHashT compare_or_hash;
};

template <typename T, typename CompareOrHashT, typename Eq>
struct Index<T, CompareOrHashT, Eq>::Impl::EqWrapper {
  using is_transparent = void;

  bool operator()(int a, int b) const {
    return eq(impl->objects_[a], impl->objects_[b]);
  }
  bool operator()(int a, const LookupKey& b) const {
    return eq(impl->objects_[a], b.object);
  }
  bool operator()(const LookupKey& a, int b) const {
    return eq(a.object, impl->objects_[b]);
  }

  const Index::Impl* impl = nullptr;
  [[no_unique_address]] Eq eq;
};

}  // namespace graph
}  // namespace util

#endif  // UTIL_GRAPH_INDEX_H_
