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

#ifndef OR_TOOLS_MATH_OPT_CPP_ID_SET_H_
#define OR_TOOLS_MATH_OPT_CPP_ID_SET_H_

#include <initializer_list>
#include <iterator>
#include <utility>

#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "ortools/math_opt/core/arrow_operator_proxy.h"
#include "ortools/math_opt/cpp/key_types.h"
#include "ortools/math_opt/storage/model_storage.h"

namespace operations_research {
namespace math_opt {

// Similar to a absl::flat_hash_set<K> for K as Variable or LinearConstraint.
//
// Important differences:
//  * The storage is more efficient, as we store the underlying ids directly.
//  * The consequence of that is that the keys are usually returned by value in
//    situations where the flat_hash_set would return references.
//  * You cannot mix variables/constraints from multiple models in these maps.
//    Doing so results in a CHECK failure.
//
// Implementation notes:
//  * Emptying the set (with clear() or erase()) resets the underlying model to
//    nullptr, enabling reusing the same instance with a different model.
//  * Operator= and swap() support operating with different models by
//    respectively replacing or swapping it.
//  * For details requirements on K, see key_types.h.
//
// See also IdMap for the equivalent class for maps.
template <typename K>
class IdSet {
 public:
  using IdType = typename K::IdType;
  using StorageType = absl::flat_hash_set<IdType>;
  using key_type = K;
  using value_type = key_type;
  using size_type = typename StorageType::size_type;
  using difference_type = typename StorageType::difference_type;
  using reference = K;
  using const_reference = const K;
  using pointer = void;
  using const_pointer = void;

  class const_iterator {
   public:
    using value_type = IdSet::value_type;
    using reference = IdSet::const_reference;
    using pointer = IdSet::const_pointer;
    using difference_type = IdSet::difference_type;
    using iterator_category = std::forward_iterator_tag;

    const_iterator() = default;

    inline const_reference operator*() const;
    inline internal::ArrowOperatorProxy<reference> operator->() const;
    inline const_iterator& operator++();
    inline const_iterator operator++(int);

    friend bool operator==(const const_iterator& lhs,
                           const const_iterator& rhs) {
      return lhs.storage_iterator_ == rhs.storage_iterator_;
    }
    friend bool operator!=(const const_iterator& lhs,
                           const const_iterator& rhs) {
      return lhs.storage_iterator_ != rhs.storage_iterator_;
    }

   private:
    friend class IdSet;

    inline const_iterator(
        const IdSet* set,
        typename StorageType::const_iterator storage_iterator);

    const IdSet* set_ = nullptr;
    typename StorageType::const_iterator storage_iterator_;
  };

  // All iterators on sets are const; but STL still defines the `iterator`
  // type. The `flat_hash_set` defines two classes the but the policy makes both
  // constant. Here to simplify the code we use the same type.
  using iterator = const_iterator;

  IdSet() = default;
  template <typename InputIt>
  inline IdSet(InputIt first, InputIt last);
  inline IdSet(std::initializer_list<value_type> ilist);

  // Typically for internal use only.
  inline IdSet(const ModelStorage* storage, StorageType values);

  inline const_iterator cbegin() const;
  inline const_iterator begin() const;

  inline const_iterator cend() const;
  inline const_iterator end() const;

  bool empty() const { return set_.empty(); }
  size_type size() const { return set_.size(); }
  inline void clear();
  void reserve(size_type count) { set_.reserve(count); }

  inline std::pair<const_iterator, bool> insert(const K& k);
  template <typename InputIt>
  inline void insert(InputIt first, InputIt last);
  inline void insert(std::initializer_list<value_type> ilist);

  inline std::pair<const_iterator, bool> emplace(const K& k);

  // Returns the number of elements erased (zero or one).
  inline size_type erase(const K& k);
  // In STL erase(const_iterator) returns an iterator. But flat_hash_set instead
  // has void return types. So here we also use void.
  inline void erase(const_iterator pos);
  inline const_iterator erase(const_iterator first, const_iterator last);

  inline void swap(IdSet& other);

  inline size_type count(const K& k) const;
  inline bool contains(const K& k) const;
  inline const_iterator find(const K& k) const;
  inline std::pair<const_iterator, const_iterator> equal_range(
      const K& k) const;

  const StorageType& raw_set() const { return set_; }
  const ModelStorage* storage() const { return storage_; }

  friend bool operator==(const IdSet& lhs, const IdSet& rhs) {
    return lhs.storage_ == rhs.storage_ && lhs.set_ == rhs.set_;
  }
  friend bool operator!=(const IdSet& lhs, const IdSet& rhs) {
    return !(lhs == rhs);
  }

 private:
  // CHECKs that storage_ and k.storage() matches when this set is not empty
  // (i.e. its storage_ is not null). When it is empty, simply check that
  // k.storage() is not null.
  inline void CheckModel(const K& k) const;
  // Sets storage_ to k.storage() if this set is empty (i.e. its storage_ is
  // null). Else CHECK that it has the same storage. It also CHECK that
  // k.storage() is not null.
  inline void CheckOrSetModel(const K& k);

  // Invariant: storage == nullptr if and only if set_.empty().
  const ModelStorage* storage_ = nullptr;
  StorageType set_;
};

// Calls a.swap(b).
//
// This function is used for making IdSet "swappable".
// Ref: https://en.cppreference.com/w/cpp/named_req/Swappable.
template <typename K>
void swap(IdSet<K>& a, IdSet<K>& b) {
  a.swap(b);
}

////////////////////////////////////////////////////////////////////////////////
// Inline implementations
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// IdSet::const_iterator
////////////////////////////////////////////////////////////////////////////////

template <typename K>
typename IdSet<K>::const_iterator::reference
IdSet<K>::const_iterator::operator*() const {
  return K(set_->storage_, *storage_iterator_);
}

template <typename K>
internal::ArrowOperatorProxy<typename IdSet<K>::const_iterator::reference>
IdSet<K>::const_iterator::operator->() const {
  return internal::ArrowOperatorProxy<reference>(**this);
}

template <typename K>
typename IdSet<K>::const_iterator& IdSet<K>::const_iterator::operator++() {
  ++storage_iterator_;
  return *this;
}

template <typename K>
typename IdSet<K>::const_iterator IdSet<K>::const_iterator::operator++(int) {
  const_iterator ret = *this;
  ++(*this);
  return ret;
}

template <typename K>
IdSet<K>::const_iterator::const_iterator(
    const IdSet* set, typename StorageType::const_iterator storage_iterator)
    : set_(set), storage_iterator_(std::move(storage_iterator)) {}

////////////////////////////////////////////////////////////////////////////////
// IdSet
////////////////////////////////////////////////////////////////////////////////

template <typename K>
IdSet<K>::IdSet(const ModelStorage* storage, StorageType values)
    : storage_(values.empty() ? nullptr : storage), set_(std::move(values)) {
  if (!set_.empty()) {
    CHECK(storage_ != nullptr);
  }
}

template <typename K>
template <typename InputIt>
IdSet<K>::IdSet(InputIt first, InputIt last) {
  insert(first, last);
}

template <typename K>
IdSet<K>::IdSet(std::initializer_list<value_type> ilist) {
  insert(ilist);
}

template <typename K>
typename IdSet<K>::const_iterator IdSet<K>::cbegin() const {
  return const_iterator(this, set_.cbegin());
}

template <typename K>
typename IdSet<K>::const_iterator IdSet<K>::begin() const {
  return cbegin();
}

template <typename K>
typename IdSet<K>::const_iterator IdSet<K>::cend() const {
  return const_iterator(this, set_.cend());
}

template <typename K>
typename IdSet<K>::const_iterator IdSet<K>::end() const {
  return cend();
}

template <typename K>
void IdSet<K>::clear() {
  storage_ = nullptr;
  set_.clear();
}

template <typename K>
std::pair<typename IdSet<K>::const_iterator, bool> IdSet<K>::insert(
    const K& k) {
  return emplace(k);
}

template <typename K>
template <typename InputIt>
void IdSet<K>::insert(const InputIt first, const InputIt last) {
  for (InputIt it = first; it != last; ++it) {
    insert(*it);
  }
}

template <typename K>
void IdSet<K>::insert(std::initializer_list<value_type> ilist) {
  insert(ilist.begin(), ilist.end());
}

template <typename K>
std::pair<typename IdSet<K>::const_iterator, bool> IdSet<K>::emplace(
    const K& k) {
  CheckOrSetModel(k);
  auto initial_ret = set_.emplace(k.typed_id());
  return std::make_pair(const_iterator(this, std::move(initial_ret.first)),
                        initial_ret.second);
}

template <typename K>
typename IdSet<K>::size_type IdSet<K>::erase(const K& k) {
  CheckModel(k);
  const size_type ret = set_.erase(k.typed_id());
  if (set_.empty()) {
    storage_ = nullptr;
  }
  return ret;
}

template <typename K>
void IdSet<K>::erase(const const_iterator pos) {
  set_.erase(pos.storage_iterator_);
  if (set_.empty()) {
    storage_ = nullptr;
  }
}

template <typename K>
typename IdSet<K>::const_iterator IdSet<K>::erase(const const_iterator first,
                                                  const const_iterator last) {
  auto ret = set_.erase(first.storage_iterator_, last.storage_iterator_);
  if (set_.empty()) {
    storage_ = nullptr;
  }
  return const_iterator(this, std::move(ret));
}

template <typename K>
void IdSet<K>::swap(IdSet& other) {
  using std::swap;
  swap(storage_, other.storage_);
  swap(set_, other.set_);
}

template <typename K>
typename IdSet<K>::size_type IdSet<K>::count(const K& k) const {
  CheckModel(k);
  return set_.count(k.typed_id());
}

template <typename K>
bool IdSet<K>::contains(const K& k) const {
  CheckModel(k);
  return set_.contains(k.typed_id());
}

template <typename K>
typename IdSet<K>::const_iterator IdSet<K>::find(const K& k) const {
  CheckModel(k);
  return const_iterator(this, set_.find(k.typed_id()));
}

template <typename K>
std::pair<typename IdSet<K>::const_iterator, typename IdSet<K>::const_iterator>
IdSet<K>::equal_range(const K& k) const {
  const auto it = find(k);
  if (it == end()) {
    return {it, it};
  }
  return {it, std::next(it)};
}

template <typename K>
void IdSet<K>::CheckModel(const K& k) const {
  CHECK(k.storage() != nullptr) << internal::kKeyHasNullModelStorage;
  CHECK(storage_ == nullptr || storage_ == k.storage())
      << internal::kObjectsFromOtherModelStorage;
}

template <typename K>
void IdSet<K>::CheckOrSetModel(const K& k) {
  CHECK(k.storage() != nullptr) << internal::kKeyHasNullModelStorage;
  if (storage_ == nullptr) {
    storage_ = k.storage();
  } else {
    CHECK_EQ(storage_, k.storage()) << internal::kObjectsFromOtherModelStorage;
  }
}

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_CPP_ID_SET_H_
