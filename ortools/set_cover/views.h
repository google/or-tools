// Copyright 2025 Francesco Cavaliere
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

#ifndef ORTOOLS_SET_COVER_VIEWS_H_
#define ORTOOLS_SET_COVER_VIEWS_H_

// NOTE: It may be unexpected, but views provide a subscript operator that
// directly accesses the underlying original container using the original
// indices. This behavior is particularly relevant in the context of the Set
// Cover problem, where we often work with subsets of columns or rows. Despite
// this, each column or row still contains the original indices, which are
// used for referring to other columns/rows.
//
// This design abstracts access to the underlying container, ensuring consistent
// behavior across the following scenarios:
// 1. Directly using the original container.
// 2. Accessing a subset of the original items via a view.
// 3. Using a new container with a compacted subset of items, indexing them with
//    the position they have in the new container and not in the original one.
//    This also needs the old indices stored in rows/columns to be mapped into
//    the new ones.

#include <cstddef>
#include <iterator>
#include <type_traits>
#include <vector>

#include "absl/types/span.h"

namespace operations_research {

namespace util {

template <typename RangeT>
using range_const_iterator_type =
    std::remove_cvref_t<decltype(std::declval<const RangeT>().begin())>;
template <typename RangeT>
using range_value_type = std::remove_cvref_t<
    decltype(*std::declval<range_const_iterator_type<RangeT>>())>;

// Enable compatibility with STL algorithms.
template <typename IterT, typename ValueT>
struct IteratorCRTP {
  using iterator_category = std::input_iterator_tag;
  using value_type = ValueT;
  using difference_type = std::ptrdiff_t;
  using pointer = IterT;
  using reference = value_type&;
  bool operator==(const IterT& other) const {
    return !(*static_cast<const IterT*>(this) != other);
  }
  pointer operator->() const { return static_cast<const IterT*>(this); }
};

template <typename ValueRangeT, typename IndexT>
decltype(auto) at(const ValueRangeT* container, IndexT index) {
  DCHECK(IndexT(0) <= index && index < IndexT(container->size()));
  return (*container)[index];
}

}  // namespace util

// View exposing only the indices of a container that are marked as active.
// Looping over this view is equivalent to:
//
// for (IntT index; index < IntT<is_active_->size()); ++index) {
//   if (is_active[index]) {
//     your_code(index);
//   }
// }
//
template <typename IntT, typename EnableVectorT>
class FilterIndexRangeView {
 public:
  struct FilterIndicesViewIterator
      : util::IteratorCRTP<FilterIndicesViewIterator, IntT> {
    FilterIndicesViewIterator(IntT index_, const EnableVectorT* is_active_)
        : index(index_), is_active(is_active_) {
      AdjustToValidValue();
      std::vector<bool> vb;
    }
    bool operator!=(FilterIndicesViewIterator other) const {
      return index != other.index;
    }
    FilterIndicesViewIterator& operator++() {
      ++index;
      AdjustToValidValue();
      return *this;
    }
    IntT operator*() const { return index; }

   private:
    void AdjustToValidValue() {
      while (index < IntT(is_active->size()) && !util::at(is_active, index)) {
        ++index;
      }
    }

    IntT index;
    const EnableVectorT* is_active;
  };

  explicit FilterIndexRangeView(const EnableVectorT* is_active)
      : is_active_(is_active) {}
  FilterIndicesViewIterator begin() const {
    return FilterIndicesViewIterator(IntT(0), is_active_);
  }
  FilterIndicesViewIterator end() const {
    return FilterIndicesViewIterator(IntT(is_active_->size()), is_active_);
  }

 private:
  const EnableVectorT* is_active_;
};

// View exposing only the elements of a container that are indexed by a list of
// indices.
// Looping over this view is equivalent to:
//
// for (decltype(auto) index : indices) {
//   your_code(container[index]);
// }
//
template <typename ValueT, typename IndexT>
class IndexListView {
 public:
  using value_type = const ValueT;
  using index_type = const IndexT;
  using value_iterator = typename absl::Span<value_type>::iterator;
  using index_iterator = typename absl::Span<index_type>::iterator;

  struct IndexListViewIterator
      : util::IteratorCRTP<IndexListViewIterator, value_type> {
    IndexListViewIterator(absl::Span<value_type> values_,
                          index_iterator idx_iterator_)
        : values(values_), idx_iterator(idx_iterator_) {}
    bool operator!=(const IndexListViewIterator& other) const {
      DCHECK_EQ(values.data(), other.values.data());
      return idx_iterator != other.idx_iterator;
    }
    IndexListViewIterator& operator++() {
      ++idx_iterator;
      return *this;
    }
    decltype(auto) operator*() const {
      return values[static_cast<size_t>(*idx_iterator)];
    }

   private:
    absl::Span<value_type> values;
    index_iterator idx_iterator;
  };

  IndexListView() = default;

  template <typename ValueRangeT, typename IndexRangeT>
  IndexListView(const ValueRangeT* values, const IndexRangeT* indices)
      : values_(absl::MakeConstSpan(*values)),
        indices_(absl::MakeConstSpan(*indices)) {}

  auto size() const { return indices_.size(); }
  bool empty() const { return indices_.empty(); }

  // NOTE: uses indices of the original container, not the filtered one
  decltype(auto) operator[](index_type index) const {
    // TODO(user): we could check that index is in indices_, but that's O(n).
    return values_[static_cast<size_t>(index)];
  }
  IndexListViewIterator begin() const {
    return IndexListViewIterator(values_, indices_.begin());
  }
  IndexListViewIterator end() const {
    return IndexListViewIterator(values_, indices_.end());
  }
  absl::Span<value_type> base() const { return values_; }

 private:
  absl::Span<value_type> values_;
  absl::Span<index_type> indices_;
};

// This view provides access to elements of a container of integral types, which
// are used as indices to a filter (in) vector.
// The view filters and returns only those elements that, when used as a
// subscript to the filter vector, are evaluated to a true value. Looping over
// this view is equivalent to:
//
// for (decltype(auto) item : container) {
//   if (is_active[item]) {
//     your_code(iter);
//   }
// }
//
template <typename ValueT, typename EnableVectorT>
class ValueFilterView {
 public:
  using value_type = const ValueT;
  using value_iterator = typename absl::Span<value_type>::iterator;

  struct ValueFilterViewIterator
      : util::IteratorCRTP<ValueFilterViewIterator, value_type> {
    ValueFilterViewIterator(value_iterator iterator_begin,
                            value_iterator iterator_end,
                            const EnableVectorT* is_active_)
        : iterator(iterator_begin), end(iterator_end), is_active(is_active_) {
      AdjustToValidValue();
    }
    bool operator!=(const ValueFilterViewIterator& other) const {
      DCHECK_EQ(is_active, other.is_active);
      return iterator != other.iterator;
    }
    ValueFilterViewIterator& operator++() {
      ++iterator;
      AdjustToValidValue();
      return *this;
    }
    decltype(auto) operator*() const { return *iterator; }

   private:
    void AdjustToValidValue() {
      while (iterator != end && !util::at(is_active, *iterator)) {
        ++iterator;
      }
    }

    value_iterator iterator;
    value_iterator end;
    const EnableVectorT* is_active;
  };

  template <typename ValueRangeT>
  ValueFilterView(const ValueRangeT* values, const EnableVectorT* is_active)
      : values_(absl::MakeConstSpan(*values)), is_active_(is_active) {
    DCHECK(values != nullptr);
    DCHECK(is_active != nullptr);
  }
  ValueFilterViewIterator begin() const {
    return ValueFilterViewIterator(values_.begin(), values_.end(), is_active_);
  }
  ValueFilterViewIterator end() const {
    return ValueFilterViewIterator(values_.end(), values_.end(), is_active_);
  }

  // NOTE: uses indices of the original container, not the filtered one
  template <typename IndexT>
  decltype(auto) operator[](IndexT index) const {
    decltype(auto) value = values_[static_cast<size_t>(index)];
    DCHECK(util::at(is_active_, value))
        << "Inactive value. Are you using relative indices?";
    return value;
  }
  absl::Span<value_type> base() const { return values_; }

 private:
  absl::Span<value_type> values_;
  const EnableVectorT* is_active_;
};

// Somewhat equivalent to ValueFilterView<StrongIntRange, EnableVectorT>
// Looping over this view is equivalent to:
//
// auto c_it = container.begin();
// auto active_it = is_active.begin();
// for (; active_it != is_active.end(); ++active_it, ++c_it) {
//   if (*active_it) {
//     your_code(*c_it);
//   }
// }
//
template <typename ValueT, typename EnableVectorT>
class IndexFilterView {
 public:
  using value_type = const ValueT;
  using value_iterator = typename absl::Span<value_type>::iterator;
  using enable_iterator = util::range_const_iterator_type<EnableVectorT>;

  struct IndexFilterViewIterator
      : util::IteratorCRTP<IndexFilterViewIterator, value_type> {
    IndexFilterViewIterator(value_iterator iterator_,
                            enable_iterator is_active_begin_,
                            enable_iterator is_active_end_)
        : iterator(iterator_),
          is_active_iter(is_active_begin_),
          is_active_end(is_active_end_) {
      AdjustToValidValue();
    }
    bool operator!=(const IndexFilterViewIterator& other) const {
      DCHECK(is_active_end == other.is_active_end);
      return iterator != other.iterator;
    }
    IndexFilterViewIterator& operator++() {
      ++is_active_iter;
      ++iterator;
      AdjustToValidValue();
      return *this;
    }
    decltype(auto) operator*() const { return *iterator; }

   private:
    void AdjustToValidValue() {
      while (is_active_iter != is_active_end && !*is_active_iter) {
        ++is_active_iter;
        ++iterator;
      }
    }

    value_iterator iterator;
    enable_iterator is_active_iter;
    enable_iterator is_active_end;
  };

  template <typename ValueRangeT>
  IndexFilterView(const ValueRangeT* values, const EnableVectorT* is_active)
      : values_(absl::MakeConstSpan(*values)), is_active_(is_active) {
    DCHECK(values != nullptr);
    DCHECK(is_active_ != nullptr);
    DCHECK_EQ(values->size(), is_active_->size());
  }
  IndexFilterViewIterator begin() const {
    return IndexFilterViewIterator(values_.begin(), is_active_->begin(),
                                   is_active_->end());
  }
  IndexFilterViewIterator end() const {
    return IndexFilterViewIterator(values_.end(), is_active_->end(),
                                   is_active_->end());
  }
  absl::Span<value_type> base() const { return values_; }

  // NOTE: uses indices of the original container, not the filtered one
  template <typename IndexT>
  decltype(auto) operator[](IndexT index) const {
    DCHECK(util::at(is_active_, index))
        << "Inactive value. Are you using relative indices?";
    return values_[static_cast<size_t>(index)];
  }

 private:
  absl::Span<value_type> values_;
  const EnableVectorT* is_active_;
};

// This view provides a mechanism to access and filter elements in a 2D
// container. The filtering is applied in two stages:
// 1. The first dimension is generic and can be either an index-list or
//    bool-vector based view.
// 2. The second dimension (items of each sub-container) is further filtered
//    using a boolean-vector-like object, which determines which elements within
//    the sub-container are included in the view.
template <typename Lvl1ViewT, typename EnableVectorT>
class TwoLevelsView : public Lvl1ViewT {
 public:
  using level1_iterator = util::range_const_iterator_type<Lvl1ViewT>;
  using level1_value = util::range_value_type<Lvl1ViewT>;
  using level2_value = util::range_value_type<level1_value>;
  using level2_type = ValueFilterView<level2_value, EnableVectorT>;

  struct TwoLevelsViewIterator
      : util::IteratorCRTP<TwoLevelsViewIterator, level2_type> {
    TwoLevelsViewIterator(level1_iterator iterator_,
                          const EnableVectorT* active_items_)
        : iterator(iterator_), active_items(active_items_) {}
    bool operator!=(const TwoLevelsViewIterator& other) const {
      DCHECK_EQ(active_items, other.active_items);
      return iterator != other.iterator;
    }
    TwoLevelsViewIterator& operator++() {
      ++iterator;
      return *this;
    }
    level2_type operator*() const {
      return level2_type(&(*iterator), active_items);
    }
    const Lvl1ViewT& base() const { return *this; }

   private:
    level1_iterator iterator;
    const EnableVectorT* active_items;
  };

  TwoLevelsView() = default;
  TwoLevelsView(Lvl1ViewT lvl1_view, const EnableVectorT* active_items)
      : Lvl1ViewT(lvl1_view), active_items_(active_items) {}
  TwoLevelsViewIterator begin() const {
    return TwoLevelsViewIterator(Lvl1ViewT::begin(), active_items_);
  }
  TwoLevelsViewIterator end() const {
    return TwoLevelsViewIterator(Lvl1ViewT::end(), active_items_);
  }

  template <typename indexT>
  level2_type operator[](indexT i) const {
    auto& level2_container = Lvl1ViewT::operator[](i);
    return level2_type(&level2_container, active_items_);
  }

 private:
  const EnableVectorT* active_items_;
};

struct NoTransform {
  template <typename T>
  T&& operator()(T&& value) const {
    return std::forward<T>(value);
  }
};

template <typename FromT, typename ToT>
struct TypeCastTransform {
  ToT operator()(FromT value) const { return static_cast<ToT>(value); }
};

// View applying stateless transformation to the values of a contiguous
// container. Looping over this view is equivalent to:
//
// for (IndexT i(0); i < IndexT(container.size()); ++i) {
//   your_code(ValueTransformT()(values_[static_cast<size_t>(i)]));
// }
//
template <typename ValueT, typename IndexT,
          typename ValueTransformT = NoTransform>
class TransformView {
 public:
  using value_type = const ValueT;
  using value_iterator = typename absl::Span<value_type>::iterator;

  struct TransformViewIterator
      : util::IteratorCRTP<TransformViewIterator, value_type> {
    explicit TransformViewIterator(value_iterator iterator_)
        : iterator(iterator_) {}
    bool operator!=(const TransformViewIterator& other) const {
      return iterator != other.iterator;
    }
    TransformViewIterator& operator++() {
      ++iterator;
      return *this;
    }
    decltype(auto) operator*() const { return ValueTransformT()(*iterator); }

   private:
    value_iterator iterator;
  };

  TransformView() = default;

  template <typename ValueRangeT>
  explicit TransformView(const ValueRangeT* values)
      : values_(absl::MakeConstSpan(*values)) {}

  auto size() const { return values_.size(); }
  bool empty() const { return values_.empty(); }

  decltype(auto) operator[](IndexT index) const {
    return ValueTransformT()(values_[static_cast<size_t>(index)]);
  }
  TransformViewIterator begin() const {
    return TransformViewIterator(values_.begin());
  }
  TransformViewIterator end() const {
    return TransformViewIterator(values_.end());
  }
  absl::Span<value_type> base() const { return values_; }

 private:
  absl::Span<value_type> values_;
};

}  // namespace operations_research

#endif  // ORTOOLS_SET_COVER_VIEWS_H_
