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

#ifndef ORTOOLS_MATH_OPT_ELEMENTAL_ATTR_KEY_H_
#define ORTOOLS_MATH_OPT_ELEMENTAL_ATTR_KEY_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <ostream>
#include <type_traits>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/types/span.h"
#include "ortools/base/status_builder.h"
#include "ortools/math_opt/elemental/elements.h"
#include "ortools/math_opt/elemental/symmetry.h"

namespace operations_research::math_opt {

// An attribute key for an attribute keyed on `n` elements.
// `AttrKey` is a value type.
template <int n, typename Symmetry = NoSymmetry>
class AttrKey {
 public:
  using value_type = int64_t;
  using SymmetryT = Symmetry;

  // Default constructor: values are uninitialized.
  constexpr AttrKey() {}  // NOLINT: uninitialized on purpose.

  template <typename... Ints,
            typename = std::enable_if_t<(sizeof...(Ints) == n &&
                                         (std::is_integral_v<Ints> && ...))>>
  explicit constexpr AttrKey(const Ints... ids) {
    auto push_back = [this, i = 0](auto e) mutable { element_ids_[i++] = e; };
    (push_back(ids), ...);
    Symmetry::Enforce(element_ids_);
  }

  template <ElementType... element_types,
            typename = std::enable_if_t<(sizeof...(element_types) == n)>>
  explicit constexpr AttrKey(const ElementId<element_types>... ids)
      : AttrKey(ids.value()...) {}

  constexpr AttrKey(std::array<value_type, n> ids)  // NOLINT: pybind11.
      : element_ids_(ids) {
    Symmetry::Enforce(element_ids_);
  }

  // Canonicalizes a non-canonical key, i.e., enforces the symmetries
  static constexpr AttrKey Canonicalize(AttrKey<n, NoSymmetry> key) {
    return AttrKey(key.element_ids_);
  }

  // Creates a key from a range of `n` elements.
  static absl::StatusOr<AttrKey> FromRange(absl::Span<const int64_t> range) {
    if (range.size() != n) {
      return ::util::InvalidArgumentErrorBuilder()
             << "cannot build AttrKey<" << n << "> from a range of size "
             << range.size();
    }
    AttrKey result;
    std::copy(range.begin(), range.end(), result.element_ids_.begin());
    Symmetry::Enforce(result.element_ids_);
    return result;
  }

  constexpr AttrKey(const AttrKey&) = default;
  constexpr AttrKey& operator=(const AttrKey&) = default;

  static constexpr int size() { return n; }

  // Element access.
  constexpr value_type operator[](const int dim) const {
    DCHECK_LT(dim, n);
    DCHECK_GE(dim, 0);
    return element_ids_[dim];
  }
  constexpr value_type& operator[](const int dim) {
    DCHECK_LT(dim, n);
    DCHECK_GE(dim, 0);
    return element_ids_[dim];
  }

  // Element iteration.
  constexpr const value_type* begin() const { return element_ids_.data(); }
  constexpr const value_type* end() const {
    return element_ids_.data() + element_ids_.size();
  }

  // `AttrKey` is comparable (ordering is lexicographic) and hashable.
  //
  // TODO(b/365998156): post C++ 20, replace by spaceship operator (with all
  // comparison operators below). Do NOT use the default generated operator (see
  // below).
  friend constexpr bool operator==(const AttrKey& l, const AttrKey& r) {
    // This is much faster than using the default generated `operator==`.
    for (int i = 0; i < n; ++i) {
      if (l.element_ids_[i] != r.element_ids_[i]) {
        return false;
      }
    }
    return true;
  }

  friend constexpr bool operator<(const AttrKey& l, const AttrKey& r) {
    // This is much faster than using the default generated `operator<`.
    for (int i = 0; i < n; ++i) {
      if (l.element_ids_[i] < r.element_ids_[i]) {
        return true;
      }
      if (l.element_ids_[i] > r.element_ids_[i]) {
        return false;
      }
    }
    return false;
  }

  friend constexpr bool operator<=(const AttrKey& l, const AttrKey& r) {
    // This is much faster than using the default generated `operator<`.
    for (int i = 0; i < n; ++i) {
      if (l.element_ids_[i] < r.element_ids_[i]) {
        return true;
      }
      if (l.element_ids_[i] > r.element_ids_[i]) {
        return false;
      }
    }
    return true;
  }

  friend constexpr bool operator>(const AttrKey& l, const AttrKey& r) {
    return r < l;
  }

  friend constexpr bool operator>=(const AttrKey& l, const AttrKey& r) {
    return r <= l;
  }

  template <typename H>
  friend H AbslHashValue(H h, const AttrKey& a) {
    return H::combine_contiguous(std::move(h), a.element_ids_.data(), n);
  }

  // `AttrKey` is printable for logging and tests.
  template <typename Sink>
  friend void AbslStringify(Sink& sink, const AttrKey& key) {
    sink.Append(absl::StrCat(
        "AttrKey(", absl::StrJoin(absl::MakeSpan(key.element_ids_), ", "),
        ")"));
  }

  // Removes the element at dimension `dim` from the key and returns a key with
  // only remaining dimensions.
  template <int dim>
  AttrKey<n - 1, NoSymmetry> RemoveElement() const {
    static_assert(dim >= 0);
    static_assert(dim < n);
    AttrKey<n - 1, NoSymmetry> result;
    for (int i = 0; i < dim; ++i) {
      result.element_ids_[i] = element_ids_[i];
    }
    for (int i = dim + 1; i < n; ++i) {
      result.element_ids_[i - 1] = element_ids_[i];
    }
    return result;
  }

  // Adds element `elem` at dimension `dim` and returns the result.
  // The result must respect `NewSymmetry` (we `DCHECK` this).
  template <int dim, typename NewSymmetry>
  AttrKey<n + 1, NewSymmetry> AddElement(const value_type elem) const {
    static_assert(dim >= 0);
    static_assert(dim < n + 1);
    AttrKey<n + 1, NewSymmetry> result;
    for (int i = 0; i < dim; ++i) {
      result.element_ids_[i] = element_ids_[i];
    }
    result.element_ids_[dim] = elem;
    for (int i = dim + 1; i < n + 1; ++i) {
      result.element_ids_[i] = element_ids_[i - 1];
    }
    DCHECK(NewSymmetry::Validate(result.element_ids_))
        << result << " does not have `" << NewSymmetry::GetName()
        << "` symmetry";
    return result;
  }

 private:
  template <int other_n, typename OtherSymmetry>
  friend class AttrKey;
  std::array<value_type, n> element_ids_;
};

// CTAD for `AttrKey(1,2)`.
template <typename... Ints>
AttrKey(Ints... dims) -> AttrKey<sizeof...(Ints), NoSymmetry>;

// Traits to detect whether `T` is an `AttrKey`.
template <typename T>
struct is_attr_key : public std::false_type {};

template <int n, typename Symmetry>
struct is_attr_key<AttrKey<n, Symmetry>> : public std::true_type {};

template <typename T>
static constexpr inline bool is_attr_key_v = is_attr_key<T>::value;

// Required for open-source `StatusBuilder` support.
template <int n, typename Symmetry>
std::ostream& operator<<(std::ostream& ostr, const AttrKey<n, Symmetry>& key) {
  ostr << absl::StrCat(key);
  return ostr;
}

namespace detail {
// A set of zero or one `AttrKey<0, Symmetry>, V`. This is used to make
// implementations of `AttrDiff` and `AttrStorage` uniform.
// `V` must by default constructible, trivially destructible and copyable
// (we'll fail to compile otherwise).
// After c++26, optional is a sequence container, so this can pretty much become
// `std::optional<AttrKey<0, Symmetry>>` + `find()`.
template <typename Symmetry, typename V>
class AttrKey0RawSet {
 public:
  using value_type = V;
  using Key = AttrKey<0, Symmetry>;

  template <typename ValueT>
  class IteratorImpl {
   public:
    IteratorImpl() = default;
    // `iterator` converts to `const_iterator`.
    IteratorImpl(const IteratorImpl<std::remove_cv_t<ValueT>>& other)  // NOLINT
        : value_(other.value_) {}

    // Dereference.
    ValueT& operator*() const {
      DCHECK_NE(value_, nullptr);
      return *value_;
    }
    ValueT* operator->() const {
      DCHECK_NE(value_, nullptr);
      return value_;
    }

    // Increment.
    IteratorImpl& operator++() {
      DCHECK_NE(value_, nullptr);
      value_ = nullptr;
      return *this;
    }

    // Equality.
    friend bool operator==(const IteratorImpl& l, const IteratorImpl& r) {
      return l.value_ == r.value_;
    }
    friend bool operator!=(const IteratorImpl& l, const IteratorImpl& r) {
      return !(l == r);
    }

   private:
    friend class AttrKey0RawSet;
    explicit IteratorImpl(ValueT& value) : value_(&value) {}

    ValueT* value_ = nullptr;
  };

  using iterator = IteratorImpl<value_type>;
  using const_iterator = IteratorImpl<const value_type>;

  AttrKey0RawSet() = default;

  bool empty() const { return !engaged_; }
  size_t size() const { return engaged_ ? 1 : 0; }

  const_iterator begin() const {
    return engaged_ ? const_iterator(value_) : const_iterator();
  }
  const_iterator end() const { return const_iterator(); }
  iterator begin() { return engaged_ ? iterator(value_) : iterator(); }
  iterator end() { return iterator(); }

  bool contains(Key) const { return engaged_; }
  const_iterator find(Key) const { return begin(); }
  iterator find(Key) { return begin(); }

  void clear() { engaged_ = false; }
  size_t erase(Key) {
    if (engaged_) {
      engaged_ = false;
      return 1;
    }
    return 0;
  }
  size_t erase(const_iterator) { return erase(Key()); }

  template <typename... Args>
  std::pair<iterator, bool> try_emplace(Key, Args&&... args) {
    if (engaged_) {
      return std::make_pair(iterator(value_), false);
    }
    value_ = value_type(Key(), std::forward<Args>(args)...);
    engaged_ = true;
    return std::make_pair(iterator(value_), true);
  }

  std::pair<iterator, bool> insert(const value_type& v) {
    if (engaged_) {
      return std::make_pair(iterator(value_), false);
    }
    value_ = v;
    engaged_ = true;
    return std::make_pair(iterator(value_), true);
  }

 private:
  // The following greatly simplifies the implementation because we don't have
  // to worry about side effects of the dtor (see e.g. `clear()`).
  static_assert(std::is_trivially_destructible_v<value_type>);

  bool engaged_ = false;
  value_type value_;
};

}  // namespace detail

// A hash set of `AttrKeyT`, where `AttrKeyT` is an `AttrKey<n, Symmetry>`.
template <typename AttrKeyT,
          typename = std::enable_if_t<is_attr_key_v<AttrKeyT>>>
using AttrKeyHashSet = std::conditional_t<
    (AttrKeyT::size() > 0), absl::flat_hash_set<AttrKeyT>,
    detail::AttrKey0RawSet<typename AttrKeyT::SymmetryT, AttrKeyT>>;

// A hash map of `AttrKeyT` to `V`, where `AttrKeyT` is an
// `AttrKey<n, Symmetry>`.
template <typename AttrKeyT, typename V,
          typename = std::enable_if_t<is_attr_key_v<AttrKeyT>>>
using AttrKeyHashMap =
    std::conditional_t<(AttrKeyT::size() > 0), absl::flat_hash_map<AttrKeyT, V>,
                       detail::AttrKey0RawSet<typename AttrKeyT::SymmetryT,
                                              std::pair<AttrKeyT, V>>>;

}  // namespace operations_research::math_opt

#endif  // ORTOOLS_MATH_OPT_ELEMENTAL_ATTR_KEY_H_
