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

#ifndef ORTOOLS_MATH_OPT_ELEMENTAL_ATTR_STORAGE_H_
#define ORTOOLS_MATH_OPT_ELEMENTAL_ATTR_STORAGE_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>
#include <variant>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "ortools/base/map_util.h"
#include "ortools/math_opt/elemental/attr_key.h"
#include "ortools/math_opt/elemental/symmetry.h"

namespace operations_research::math_opt {

namespace detail {

// A non-default key set based on a vector. This is very efficient for
// insertions, reads, and slicing, but does not support deletions.
template <int n>
class DenseKeySet {
 public:
  // {Dense,Sparse}KeySet stores symmetric keys, symmetry is handled by
  // `SlicingStorage`.
  using Key = AttrKey<n, NoSymmetry>;

  DenseKeySet() = default;

  size_t size() const { return key_set_.size(); }

  template <typename F>
  // requires std::invocable<F, const Key&>
  void ForEach(F f) const {
    for (const Key& key : key_set_) {
      f(key);
    }
  }

  // Note: this does not check for duplicates. This is fine because inserting
  // into this map is gated on inserting into the AttrStorage, which does check
  // for duplicates.
  void Insert(const Key& key) { key_set_.push_back(key); }

  auto begin() const { return key_set_.begin(); }
  auto end() const { return key_set_.end(); }

 private:
  std::vector<Key> key_set_;
};

// A non-default key set based on a hash set. Simple, but requires a hash lookup
// for each insertion and deletion.
template <int n>
class SparseKeySet {
 public:
  // {Dense,Sparse}KeySet stores symmetric keys, symmetry is handled by
  // `SlicingStorage`.
  using Key = AttrKey<n, NoSymmetry>;

  explicit SparseKeySet(const DenseKeySet<n>& dense_set)
      : key_set_(dense_set.begin(), dense_set.end()) {}

  size_t size() const { return key_set_.size(); }

  template <typename F>
  // requires std::invocable<F, const Key&>
  void ForEach(F f) const {
    for (const Key& key : key_set_) {
      f(key);
    }
  }

  void Erase(const Key& key) { key_set_.erase(key); }
  void Insert(const Key& key) { key_set_.insert(key); }

 private:
  absl::flat_hash_set<Key> key_set_;
};

// A non-default key set that switches between implementations
// opportunistically: It starts dense, and switches to sparse if there are
// deletions.
template <int n>
class KeySet {
 public:
  using Key = AttrKey<n, NoSymmetry>;

  size_t size() const {
    return std::visit([](const auto& impl) { return impl.size(); }, impl_);
  }

  // We can't do begin/end because the iterator types are not the same.
  template <typename F>
  // requires std::invocable<F, const Key&>
  void ForEach(F f) const {
    return std::visit(
        [f = std::move(f)](const auto& impl) {
          return impl.ForEach(std::move(f));
        },
        impl_);
  }

  auto Erase(const Key& key) { return AsSparse().Erase(key); }

  void Insert(const Key& key) {
    std::visit([&](auto& impl) { impl.Insert(key); }, impl_);
  }

 private:
  SparseKeySet<n>& AsSparse() {
    if (auto* sparse = std::get_if<SparseKeySet<n>>(&impl_)) {
      return *sparse;
    }
    // Switch to a sparse representation.
    impl_ = SparseKeySet<n>(std::get<DenseKeySet<n>>(impl_));
    return std::get<SparseKeySet<n>>(impl_);
  }

  std::variant<DenseKeySet<n>, SparseKeySet<n>> impl_;
};

// When we have two or more dimensions, we need to store the nondefaults for
// each dimension to support slicing.
template <int n, typename Symmetry, typename = void>
class SlicingSupport {
 public:
  using Key = AttrKey<n, Symmetry>;

  void AddRowsAndColumns(const Key key) {
    // NOLINTNEXTLINE(clang-diagnostic-pre-c++20-compat)
    ForEachDimension([this, key]<int i>() {
      if (MustInsertNondefault<i>(key, Symmetry{})) {
        key_nondefaults_[i][key[i]].Insert(key.template RemoveElement<i>());
      }
    });
  }

  // Requires key is currently stored with a non-default value.
  void ClearRowsAndColumns(Key key) {
    // NOLINTNEXTLINE(clang-diagnostic-pre-c++20-compat)
    ForEachDimension([this, key]<int i>() {
      const auto& key_elem = key[i];
      auto& nondefaults = key_nondefaults_[i];
      if (nondefaults[key_elem].size() == 1) {
        nondefaults.erase(key_elem);
      } else {
        nondefaults[key_elem].Erase(key.template RemoveElement<i>());
      }
    });
  }

  void Clear() {
    for (auto& key_nondefaults : key_nondefaults_) {
      key_nondefaults.clear();
    }
  }

  template <int i>
  std::vector<Key> Slice(const int64_t key_elem) const {
    return SliceImpl<i>(
        key_elem, Symmetry{},
        // NOLINTNEXTLINE(clang-diagnostic-pre-c++20-compat)
        [key_elem]<int... is>(KeySetExpansion<is>... expansions) {
          std::vector<Key> slice((expansions.key_set.size() + ...));
          Key* out = slice.data();
          // NOLINTNEXTLINE(clang-diagnostic-pre-c++20-compat)
          const auto append = [key_elem, &out]<int j>(
                                  const KeySetExpansion<j>& expansion) {
            expansion.key_set.ForEach(
                [key_elem, &out](const AttrKey<n - 1> other_elems) {
                  *out = other_elems.template AddElement<j, Symmetry>(key_elem);
                  ++out;
                });
          };
          (append(expansions), ...);
          return slice;
        });
  }

  template <int i>
  int64_t GetSliceSize(const int64_t key_elem) const {
    return SliceImpl<i>(key_elem, Symmetry{}, [](const auto... expansions) {
      return (expansions.key_set.size() + ...);
    });
  }

 private:
  // We store the nondefaults for a given id along a given dimension as a set of
  // `AttrKey<n-1, NoSymmetry>` (the current dimension is not stored).
  using NonDefaultKeySet = KeySet<n - 1>;
  // For each dimension, we store the nondefaults for each id.
  using NonDefaultKeySetById = absl::flat_hash_map<int64_t, NonDefaultKeySet>;
  // We need one such set per dimension.
  using NonDefaultsPerDimension = std::array<NonDefaultKeySetById, n>;

  // Represents a NonDefaultKeySet to be expanded by inserting an element on
  // dimension `i`.
  template <int i>
  struct KeySetExpansion {
    KeySetExpansion(const NonDefaultsPerDimension& key_nondefaults,
                    int64_t key_elem)
        : key_set(gtl::FindWithDefault(key_nondefaults[i], key_elem)) {}
    const NonDefaultKeySet& key_set;
  };

  // A helper function that calls `F` `i` times with template arguments `n-1` to
  // `0`.
  template <typename F, int i = n - 1>
  static void ForEachDimension(const F& f) {
    f.template operator()<i>();
    if constexpr (i > 0) {
      ForEachDimension<F, i - 1>(f);
    }
  }

  template <int i>
  static bool MustInsertNondefault(const Key&, NoSymmetry) {
    return true;
  }

  template <int i, int k, int l>
  static bool MustInsertNondefault(const Key& key, ElementSymmetry<k, l>) {
    // For attributes that are symmetric on `k` and `l`, elements on the
    // diagonal need to be in only one of the nondefaults for `k` or `l`
    // (otherwise they would be counted twice in `Slice()`). We arbitrarily pick
    // `k`.
    if constexpr (i == l) {
      const bool is_diagonal = key[k] == key[l];
      return !is_diagonal;
    }
    return true;
  }

  // `Fn` should be a functor that takes any number of `KeySetExpansion`
  // arguments.
  template <int i, typename Fn>
  auto SliceImpl(const int64_t key_elem, NoSymmetry, const Fn& fn) const {
    static_assert(n > 1);
    return fn(KeySetExpansion<i>(key_nondefaults_, key_elem));
  }

  template <int i, int k, int l, typename Fn>
  auto SliceImpl(const int64_t key_elem, ElementSymmetry<k, l>,
                 const Fn& fn) const {
    static_assert(n > 1);
    if constexpr (i != k && i != l) {
      // This is a normal dimension, not a symmetric one.
      return SliceImpl<i>(key_elem, NoSymmetry(), fn);
    } else {
      // For symmetric dimensions, we need to look up the keys on both
      // dimensions `l` and `k`.
      return fn(KeySetExpansion<k>(key_nondefaults_, key_elem),
                KeySetExpansion<l>(key_nondefaults_, key_elem));
    }
  }

  NonDefaultsPerDimension key_nondefaults_;
};

// Without slicing we don't need to track anything.
template <int n, typename Symmetry>
struct SlicingSupport<n, Symmetry, std::enable_if_t<(n < 2), std::void_t<>>> {
  using Key = AttrKey<n, Symmetry>;

  void AddRowsAndColumns(Key) {}
  void ClearRowsAndColumns(Key) {}
  void Clear() {}
};

}  // namespace detail

// Stores the value of an attribute keyed on n elements (e.g.
// linear_constraint_coefficient is a double valued attribute keyed first on
// LinearConstraint and then on Variable).
//
// Memory usage:
//   Storing `k` elements with non-default values in a `AttrStorage<V, n>` uses
//   `sizeof(V) * (n^2 + 1) * k / load_factor` (where load_factor is the absl
//   hash map load factor, typically 0.8), plus a small allocation overhead of
//   `O(k)`.
template <typename V, int n, typename Symmetry>
class AttrStorage {
 public:
  using Key = AttrKey<n, Symmetry>;
  // If this no longer holds, we should sprinkle the code with `move`s and
  // return `V`s by ref.
  static_assert(std::is_trivially_copyable_v<V>);

  // Generally avoid, provided to make working with std::array easier.
  explicit AttrStorage() : AttrStorage(V{}) {}

  // The default value of the attribute is its value when the model is created
  // (e.g. for linear_constraint_coefficient, 0.0).
  explicit AttrStorage(const V default_value) : default_value_(default_value) {}

  AttrStorage(const AttrStorage&) = default;
  AttrStorage& operator=(const AttrStorage&) = default;
  AttrStorage(AttrStorage&&) = default;
  AttrStorage& operator=(AttrStorage&&) = default;

  // Returns true if the attribute for `key` has a value different from its
  // default.
  bool IsNonDefault(const Key key) const {
    return non_default_values_.contains(key);
  }

  // Returns the previous value if value has changed, otherwise returns
  // `std::nullopt`.
  std::optional<V> Set(const Key key, const V value) {
    bool is_default = value == default_value_;
    if (is_default) {
      const auto it = non_default_values_.find(key);
      if (it == non_default_values_.end()) {
        return std::nullopt;
      }
      const V prev_value = it->second;
      non_default_values_.erase(it);
      slicing_support_.ClearRowsAndColumns(key);
      return prev_value;
    }
    const auto [it, inserted] = non_default_values_.try_emplace(key, value);
    if (inserted) {
      slicing_support_.AddRowsAndColumns(key);
      return default_value_;
    }
    // !is_default and !inserted
    if (value == it->second) {
      return std::nullopt;
    }
    return std::exchange(it->second, value);
  }

  // Returns the value of the attribute for `key` (return the default value if
  // the attribute value for `key` is unset).
  V Get(const Key key) const {
    return GetIfNonDefault(key).value_or(default_value_);
  }

  // Returns the value of the attribute for `key`, or nullopt.
  std::optional<V> GetIfNonDefault(const Key key) const {
    auto it = non_default_values_.find(key);
    if (it == non_default_values_.end()) {
      return std::nullopt;
    }
    return it->second;
  }

  // Sets the value of the attribute for `key` to the default value.
  void Erase(const Key key) {
    if (non_default_values_.erase(key)) {
      slicing_support_.ClearRowsAndColumns(key);
    }
  }

  // Returns the keys (ids pairs) the of the elements with a non-default value
  // for this attribute.
  std::vector<Key> NonDefaults() const {
    std::vector<Key> result;
    result.reserve(non_default_values_.size());
    for (const auto& [key, unused] : non_default_values_) {
      result.push_back(key);
    }
    return result;
  }

  // Returns the set of all keys `K` such that:
  // - There exists `k_{0}..k_{n-1}` such that
  //   `K == AttrKey(k_{0}, ..., k_{i-1}, key_elem, k_{i+1}, ..., k_{n-1})`, and
  // - `K` has a non-default value for this attribute.
  template <int i>
  std::vector<Key> Slice(const int64_t key_elem) const {
    static_assert(n >= 1);
    if constexpr (n == 1) {
      return non_default_values_.contains(Key(key_elem))
                 ? std::vector<Key>({Key(key_elem)})
                 : std::vector<Key>();
    } else {
      return slicing_support_.template Slice<i>(key_elem);
    }
  }

  // Returns the size of the given slice: This is equivalent to
  // `Slice(key_elem).size()`, but `O(1)`.
  template <int i>
  int64_t GetSliceSize(const int64_t key_elem) const {
    static_assert(n >= 1);
    if constexpr (n == 1) {
      return non_default_values_.count(Key(key_elem));
    } else {
      return slicing_support_.template GetSliceSize<i>(key_elem);
    }
  }

  // Returns the number of keys (element pairs) with non-default values for this
  // attribute.
  int64_t num_non_defaults() const { return non_default_values_.size(); }

  // Restore all elements to their default value for this attribute.
  void Clear() {
    non_default_values_.clear();
    slicing_support_.Clear();
  }

 private:
  V default_value_;
  AttrKeyHashMap<Key, V> non_default_values_;
  detail::SlicingSupport<n, Symmetry> slicing_support_;
};

}  // namespace operations_research::math_opt

#endif  // ORTOOLS_MATH_OPT_ELEMENTAL_ATTR_STORAGE_H_
