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

// A faster version of flat_hash_map for Variable and LinearConstraint keys.
#ifndef OR_TOOLS_MATH_OPT_CPP_ID_MAP_H_
#define OR_TOOLS_MATH_OPT_CPP_ID_MAP_H_

#include <algorithm>
#include <initializer_list>
#include <iterator>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/types/span.h"
#include "absl/log/check.h"
#include "ortools/base/strong_int.h"
#include "ortools/math_opt/core/arrow_operator_proxy.h"  // IWYU pragma: export
#include "ortools/math_opt/cpp/key_types.h"
#include "ortools/math_opt/storage/model_storage.h"

namespace operations_research {
namespace math_opt {

// Similar to a absl::flat_hash_map<K, V> for K as Variable or LinearConstraint.
//
// Important differences:
//  * The storage is more efficient, as we store the underlying ids directly.
//  * The consequence of that is that the keys are usually returned by value in
//    situations where the flat_hash_map would return references.
//  * You cannot mix variables/constraints from multiple models in these maps.
//    Doing so results in a CHECK failure.
//
// Implementation notes:
//  * Emptying the map (with clear() or erase()) resets the underlying model to
//    nullptr, enabling reusing the same instance with a different model.
//  * Operator= and swap() support operating with different models by
//    respectively replacing or swapping it.
//  * For details requirements on K, see key_types.h.
//
// See also IdSet for the equivalent class for sets.
template <typename K, typename V>
class IdMap {
 public:
  using IdType = typename K::IdType;
  using StorageType = absl::flat_hash_map<IdType, V>;
  using key_type = K;
  using mapped_type = V;
  using value_type = std::pair<const K, V>;
  using size_type = typename StorageType::size_type;
  using difference_type = typename StorageType::difference_type;
  using reference = std::pair<const K, V&>;
  using const_reference = std::pair<const K, const V&>;
  using pointer = void;
  using const_pointer = void;

  class iterator {
   public:
    using value_type = IdMap::value_type;
    using reference = IdMap::reference;
    using pointer = IdMap::pointer;
    using difference_type = IdMap::difference_type;
    using iterator_category = std::forward_iterator_tag;

    iterator() = default;

    inline reference operator*() const;
    inline internal::ArrowOperatorProxy<reference> operator->() const;
    inline iterator& operator++();
    inline iterator operator++(int);

    friend bool operator==(const iterator& lhs, const iterator& rhs) {
      return lhs.storage_iterator_ == rhs.storage_iterator_;
    }
    friend bool operator!=(const iterator& lhs, const iterator& rhs) {
      return lhs.storage_iterator_ != rhs.storage_iterator_;
    }

   private:
    friend class IdMap;

    inline iterator(const IdMap* map,
                    typename StorageType::iterator storage_iterator);

    const IdMap* map_ = nullptr;
    typename StorageType::iterator storage_iterator_;
  };

  class const_iterator {
   public:
    using value_type = IdMap::value_type;
    using reference = IdMap::const_reference;
    using pointer = IdMap::const_pointer;
    using difference_type = IdMap::difference_type;
    using iterator_category = std::forward_iterator_tag;

    const_iterator() = default;
    inline const_iterator(const iterator& non_const_iterator);  // NOLINT

    inline reference operator*() const;
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
    friend class IdMap;

    inline const_iterator(
        const IdMap* map,
        typename StorageType::const_iterator storage_iterator);

    const IdMap* map_ = nullptr;
    typename StorageType::const_iterator storage_iterator_;
  };

  IdMap() = default;
  template <typename InputIt>
  inline IdMap(InputIt first, InputIt last);
  inline IdMap(std::initializer_list<value_type> ilist);

  // Typically for internal use only.
  inline IdMap(const ModelStorage* storage, StorageType values);

  inline const_iterator cbegin() const;
  inline const_iterator begin() const;
  inline iterator begin();

  inline const_iterator cend() const;
  inline const_iterator end() const;
  inline iterator end();

  bool empty() const { return map_.empty(); }
  size_type size() const { return map_.size(); }
  inline void clear();
  void reserve(size_type count) { map_.reserve(count); }

  inline std::pair<iterator, bool> insert(std::pair<K, V> k_v);
  template <typename InputIt>
  inline void insert(InputIt first, InputIt last);
  inline void insert(std::initializer_list<value_type> ilist);

  template <typename M>
  inline std::pair<iterator, bool> insert_or_assign(const K& k, M&& v);

  inline std::pair<iterator, bool> emplace(const K& k, V v);
  template <typename... Args>
  inline std::pair<iterator, bool> try_emplace(const K& k, Args&&... args);

  // Returns the number of elements erased (zero or one).
  inline size_type erase(const K& k);
  // In STL erase(const_iterator) and erase(iterator) both return an
  // iterator. But flat_hash_map instead has void return types. So here we also
  // use void.
  //
  // In flat_hash_map, both erase(const_iterator) and erase(iterator) are
  // defined since there is also the erase<K>(const K&) that exists and that
  // would be used. Since we don't have this overload, we can rely on the
  // automatic cast of the iterator in const_iterator.
  inline void erase(const_iterator pos);
  inline iterator erase(const_iterator first, const_iterator last);

  inline void swap(IdMap& other);

  inline const V& at(const K& k) const;
  inline V& at(const K& k);
  inline V& operator[](const K& k);
  inline size_type count(const K& k) const;
  inline bool contains(const K& k) const;
  inline iterator find(const K& k);
  inline const_iterator find(const K& k) const;
  inline std::pair<iterator, iterator> equal_range(const K& k);
  inline std::pair<const_iterator, const_iterator> equal_range(
      const K& k) const;

  // Updates the values in this map by adding the value of the corresponding
  // keys in the other map. For keys only in the other map, insert their value.
  //
  // This function is only available when type V supports operator+=.
  //
  // This is equivalent to (but is more efficient than):
  //   for (const auto pair : other) {
  //     (*this)[pair.first] += pair.second;
  //   }
  //
  // This function CHECK that all the keys in the two maps have the same model.
  inline void Add(const IdMap& other);

  // Updates the values in this map by subtracting the value of the
  // corresponding keys in the other map. For keys only in the other map, insert
  // the opposite of their value.
  //
  // This function is only available when type V supports operator-=.
  //
  // This is equivalent to (but is more efficient than):
  //   for (const auto pair : other) {
  //     (*this)[pair.first] -= pair.second;
  //   }
  //
  // This function CHECK that all the keys in the two maps have the same model.
  inline void Subtract(const IdMap& other);

  inline std::vector<V> Values(absl::Span<const K> keys) const;
  inline absl::flat_hash_map<K, V> Values(
      const absl::flat_hash_set<K>& keys) const;

  inline std::vector<K> SortedKeys() const;

  // Returns the values in sorted KEY order.
  inline std::vector<V> SortedValues() const;

  const StorageType& raw_map() const { return map_; }
  const ModelStorage* storage() const { return storage_; }

  friend bool operator==(const IdMap& lhs, const IdMap& rhs) {
    return lhs.storage_ == rhs.storage_ && lhs.map_ == rhs.map_;
  }
  friend bool operator!=(const IdMap& lhs, const IdMap& rhs) {
    return !(lhs == rhs);
  }

 private:
  inline std::vector<IdType> SortedIds() const;
  // CHECKs that storage_ and k.storage() matches when this map is not empty
  // (i.e. its storage_ is not null). When it is empty, simply check that
  // k.storage() is not null.
  inline void CheckModel(const K& k) const;
  // Sets storage_ to k.storage() if this map is empty (i.e. its storage_ is
  // null). Else CHECK that it has the same model. It also CHECK that
  // k.storage() is not null.
  inline void CheckOrSetModel(const K& k);
  // Sets storage_ to other.storage_ if this map is empty (i.e. its storage_ is
  // null). Else if the other map is not empty, CHECK that it has the same
  // model.
  inline void CheckOrSetModel(const IdMap& other);

  // Invariant: storage == nullptr if and only if map_.empty().
  const ModelStorage* storage_ = nullptr;
  StorageType map_;
};

// Calls a.swap(b).
//
// This function is used for making MapId "swappable".
// Ref: https://en.cppreference.com/w/cpp/named_req/Swappable.
template <typename K, typename V>
void swap(IdMap<K, V>& a, IdMap<K, V>& b) {
  a.swap(b);
}

////////////////////////////////////////////////////////////////////////////////
// Inline implementations
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// IdMap::iterator
////////////////////////////////////////////////////////////////////////////////

template <typename K, typename V>
typename IdMap<K, V>::reference IdMap<K, V>::iterator::operator*() const {
  return reference(K(map_->storage_, storage_iterator_->first),
                   storage_iterator_->second);
}

template <typename K, typename V>
internal::ArrowOperatorProxy<typename IdMap<K, V>::iterator::reference>
IdMap<K, V>::iterator::operator->() const {
  return internal::ArrowOperatorProxy<reference>(**this);
}

template <typename K, typename V>
typename IdMap<K, V>::iterator& IdMap<K, V>::iterator::operator++() {
  ++storage_iterator_;
  return *this;
}

template <typename K, typename V>
typename IdMap<K, V>::iterator IdMap<K, V>::iterator::operator++(int) {
  iterator ret = *this;
  ++(*this);
  return ret;
}

template <typename K, typename V>
IdMap<K, V>::iterator::iterator(const IdMap* map,
                                typename StorageType::iterator storage_iterator)
    : map_(map), storage_iterator_(std::move(storage_iterator)) {}

////////////////////////////////////////////////////////////////////////////////
// IdMap::const_iterator
////////////////////////////////////////////////////////////////////////////////

template <typename K, typename V>
IdMap<K, V>::const_iterator::const_iterator(const iterator& non_const_iterator)
    : map_(non_const_iterator.map_),
      storage_iterator_(non_const_iterator.storage_iterator_) {}

template <typename K, typename V>
typename IdMap<K, V>::const_iterator::reference
IdMap<K, V>::const_iterator::operator*() const {
  return reference(K(map_->storage_, storage_iterator_->first),
                   storage_iterator_->second);
}

template <typename K, typename V>
internal::ArrowOperatorProxy<typename IdMap<K, V>::const_iterator::reference>
IdMap<K, V>::const_iterator::operator->() const {
  return internal::ArrowOperatorProxy<reference>(**this);
}

template <typename K, typename V>
typename IdMap<K, V>::const_iterator&
IdMap<K, V>::const_iterator::operator++() {
  ++storage_iterator_;
  return *this;
}

template <typename K, typename V>
typename IdMap<K, V>::const_iterator IdMap<K, V>::const_iterator::operator++(
    int) {
  const_iterator ret = *this;
  ++(*this);
  return ret;
}

template <typename K, typename V>
IdMap<K, V>::const_iterator::const_iterator(
    const IdMap* map, typename StorageType::const_iterator storage_iterator)
    : map_(map), storage_iterator_(std::move(storage_iterator)) {}

////////////////////////////////////////////////////////////////////////////////
// IdMap
////////////////////////////////////////////////////////////////////////////////

template <typename K, typename V>
IdMap<K, V>::IdMap(const ModelStorage* storage, StorageType values)
    : storage_(values.empty() ? nullptr : storage), map_(std::move(values)) {
  if (!map_.empty()) {
    CHECK(storage_ != nullptr);
  }
}

template <typename K, typename V>
template <typename InputIt>
IdMap<K, V>::IdMap(InputIt first, InputIt last) {
  insert(first, last);
}

template <typename K, typename V>
IdMap<K, V>::IdMap(std::initializer_list<value_type> ilist) {
  insert(ilist);
}

template <typename K, typename V>
typename IdMap<K, V>::const_iterator IdMap<K, V>::cbegin() const {
  return const_iterator(this, map_.cbegin());
}

template <typename K, typename V>
typename IdMap<K, V>::const_iterator IdMap<K, V>::begin() const {
  return cbegin();
}

template <typename K, typename V>
typename IdMap<K, V>::iterator IdMap<K, V>::begin() {
  return iterator(this, map_.begin());
}

template <typename K, typename V>
typename IdMap<K, V>::const_iterator IdMap<K, V>::cend() const {
  return const_iterator(this, map_.cend());
}

template <typename K, typename V>
typename IdMap<K, V>::const_iterator IdMap<K, V>::end() const {
  return cend();
}

template <typename K, typename V>
typename IdMap<K, V>::iterator IdMap<K, V>::end() {
  return iterator(this, map_.end());
}

template <typename K, typename V>
void IdMap<K, V>::clear() {
  storage_ = nullptr;
  map_.clear();
}

template <typename K, typename V>
std::pair<typename IdMap<K, V>::iterator, bool> IdMap<K, V>::insert(
    std::pair<K, V> k_v) {
  return emplace(k_v.first, std::move(k_v.second));
}

template <typename K, typename V>
template <typename InputIt>
void IdMap<K, V>::insert(const InputIt first, const InputIt last) {
  for (InputIt it = first; it != last; ++it) {
    insert(*it);
  }
}

template <typename K, typename V>
void IdMap<K, V>::insert(std::initializer_list<value_type> ilist) {
  insert(ilist.begin(), ilist.end());
}

template <typename K, typename V>
template <typename M>
std::pair<typename IdMap<K, V>::iterator, bool> IdMap<K, V>::insert_or_assign(
    const K& k, M&& v) {
  CheckOrSetModel(k);
  auto initial_ret = map_.insert_or_assign(k.typed_id(), std::forward<M>(v));
  return std::make_pair(iterator(this, std::move(initial_ret.first)),
                        initial_ret.second);
}

template <typename K, typename V>
std::pair<typename IdMap<K, V>::iterator, bool> IdMap<K, V>::emplace(const K& k,
                                                                     V v) {
  CheckOrSetModel(k);
  auto initial_ret = map_.emplace(k.typed_id(), std::move(v));
  return std::make_pair(iterator(this, std::move(initial_ret.first)),
                        initial_ret.second);
}

template <typename K, typename V>
template <typename... Args>
std::pair<typename IdMap<K, V>::iterator, bool> IdMap<K, V>::try_emplace(
    const K& k, Args&&... args) {
  CheckOrSetModel(k);
  auto initial_ret =
      map_.try_emplace(k.typed_id(), std::forward<Args>(args)...);
  return std::make_pair(iterator(this, std::move(initial_ret.first)),
                        initial_ret.second);
}

template <typename K, typename V>
typename IdMap<K, V>::size_type IdMap<K, V>::erase(const K& k) {
  CheckModel(k);
  const size_type ret = map_.erase(k.typed_id());
  if (map_.empty()) {
    storage_ = nullptr;
  }
  return ret;
}

template <typename K, typename V>
void IdMap<K, V>::erase(const const_iterator pos) {
  map_.erase(pos.storage_iterator_);
  if (map_.empty()) {
    storage_ = nullptr;
  }
}

template <typename K, typename V>
typename IdMap<K, V>::iterator IdMap<K, V>::erase(const const_iterator first,
                                                  const const_iterator last) {
  auto ret = map_.erase(first.storage_iterator_, last.storage_iterator_);
  if (map_.empty()) {
    storage_ = nullptr;
  }
  return iterator(this, std::move(ret));
}

template <typename K, typename V>
void IdMap<K, V>::swap(IdMap& other) {
  using std::swap;
  swap(storage_, other.storage_);
  swap(map_, other.map_);
}

template <typename K, typename V>
const V& IdMap<K, V>::at(const K& k) const {
  CheckModel(k);
  return map_.at(k.typed_id());
}

template <typename K, typename V>
V& IdMap<K, V>::at(const K& k) {
  CheckModel(k);
  return map_.at(k.typed_id());
}

template <typename K, typename V>
V& IdMap<K, V>::operator[](const K& k) {
  CheckOrSetModel(k);
  return map_[k.typed_id()];
}

template <typename K, typename V>
typename IdMap<K, V>::size_type IdMap<K, V>::count(const K& k) const {
  CheckModel(k);
  return map_.count(k.typed_id());
}

template <typename K, typename V>
bool IdMap<K, V>::contains(const K& k) const {
  CheckModel(k);
  return map_.contains(k.typed_id());
}

template <typename K, typename V>
typename IdMap<K, V>::iterator IdMap<K, V>::find(const K& k) {
  CheckModel(k);
  return iterator(this, map_.find(k.typed_id()));
}

template <typename K, typename V>
typename IdMap<K, V>::const_iterator IdMap<K, V>::find(const K& k) const {
  CheckModel(k);
  return const_iterator(this, map_.find(k.typed_id()));
}

template <typename K, typename V>
std::pair<typename IdMap<K, V>::iterator, typename IdMap<K, V>::iterator>
IdMap<K, V>::equal_range(const K& k) {
  const auto it = find(k);
  if (it == end()) {
    return {it, it};
  }
  return {it, std::next(it)};
}

template <typename K, typename V>
std::pair<typename IdMap<K, V>::const_iterator,
          typename IdMap<K, V>::const_iterator>
IdMap<K, V>::equal_range(const K& k) const {
  const auto it = find(k);
  if (it == end()) {
    return {it, it};
  }
  return {it, std::next(it)};
}

template <typename K, typename V>
void IdMap<K, V>::Add(const IdMap& other) {
  CheckOrSetModel(other);
  for (const auto& pair : other.map_) {
    map_[pair.first] += pair.second;
  }
}

template <typename K, typename V>
void IdMap<K, V>::Subtract(const IdMap& other) {
  CheckOrSetModel(other);
  for (const auto& pair : other.map_) {
    map_[pair.first] -= pair.second;
  }
}

template <typename K, typename V>
std::vector<V> IdMap<K, V>::Values(const absl::Span<const K> keys) const {
  std::vector<V> result;
  result.reserve(keys.size());
  for (const K key : keys) {
    result.push_back(at(key));
  }
  return result;
}

template <typename K, typename V>
absl::flat_hash_map<K, V> IdMap<K, V>::Values(
    const absl::flat_hash_set<K>& keys) const {
  absl::flat_hash_map<K, V> result;
  for (const K key : keys) {
    result[key] = at(key);
  }
  return result;
}

template <typename K, typename V>
std::vector<K> IdMap<K, V>::SortedKeys() const {
  std::vector<K> result;
  result.reserve(map_.size());
  for (const IdType id : SortedIds()) {
    result.push_back(K(storage_, id));
  }
  return result;
}

template <typename K, typename V>
std::vector<V> IdMap<K, V>::SortedValues() const {
  std::vector<V> result;
  result.reserve(map_.size());
  for (const IdType id : SortedIds()) {
    result.push_back(map_.at(id));
  }
  return result;
}

template <typename K, typename V>
std::vector<typename K::IdType> IdMap<K, V>::SortedIds() const {
  std::vector<IdType> result;
  result.reserve(map_.size());
  for (const auto& [id, _] : map_) {
    result.push_back(id);
  }
  std::sort(result.begin(), result.end());
  return result;
}

template <typename K, typename V>
void IdMap<K, V>::CheckModel(const K& k) const {
  CHECK(k.storage() != nullptr) << internal::kKeyHasNullModelStorage;
  CHECK(storage_ == nullptr || storage_ == k.storage())
      << internal::kObjectsFromOtherModelStorage;
}

template <typename K, typename V>
void IdMap<K, V>::CheckOrSetModel(const K& k) {
  CHECK(k.storage() != nullptr) << internal::kKeyHasNullModelStorage;
  if (storage_ == nullptr) {
    storage_ = k.storage();
  } else {
    CHECK_EQ(storage_, k.storage()) << internal::kObjectsFromOtherModelStorage;
  }
}

template <typename K, typename V>
void IdMap<K, V>::CheckOrSetModel(const IdMap& other) {
  if (storage_ == nullptr) {
    storage_ = other.storage_;
  } else if (other.storage_ != nullptr) {
    CHECK_EQ(storage_, other.storage_)
        << internal::kObjectsFromOtherModelStorage;
  } else {
    // By construction when other is not empty, it has a non null `storage_`.
    DCHECK(other.empty());
  }
}

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_CPP_ID_MAP_H_
