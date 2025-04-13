// Copyright 2025 Francesco Cavaliere
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

#ifndef OR_TOOLS_ORTOOLS_SET_COVER_VIEWS_H
#define OR_TOOLS_ORTOOLS_SET_COVER_VIEWS_H

#include <absl/meta/type_traits.h>
#include <absl/types/span.h>

#include <cstddef>

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
//    the position the have in the new container and not in the original one.
//    This also need the old indices stored in rows/columns to be mapped into
//    the new ones.

namespace util_intops {

namespace util {

template <typename RangeT>
using range_const_iterator_type =
    absl::remove_cvref_t<decltype(std::declval<const RangeT>().begin())>;
template <typename RangeT>
using range_value_type = absl::remove_cvref_t<
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
class FilterIndicesView {
 public:
  struct FilterIndicesViewIterator
      : util::IteratorCRTP<FilterIndicesViewIterator, IntT> {
    FilterIndicesViewIterator(IntT index, const EnableVectorT* is_active)
        : index_(index), is_active_(is_active) {
      AdjustToValidValue();
      std::vector<bool> vb;
    }
    bool operator!=(FilterIndicesViewIterator other) const {
      return index_ != other.index_;
    }
    FilterIndicesViewIterator& operator++() {
      ++index_;
      AdjustToValidValue();
      return *this;
    }
    IntT operator*() const { return index_; }

   private:
    void AdjustToValidValue() {
      while (index_ < IntT(is_active_->size()) &&
             !util::at(is_active_, index_)) {
        ++index_;
      }
    }

    IntT index_;
    const EnableVectorT* is_active_;
  };

  FilterIndicesView(const EnableVectorT* is_active) : is_active_(is_active) {}
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
    IndexListViewIterator(absl::Span<value_type> values, index_iterator iter)
        : values_(values), index_iter_(iter) {}
    bool operator!=(const IndexListViewIterator& other) const {
      DCHECK_EQ(values_.data(), other.values_.data());
      return index_iter_ != other.index_iter_;
    }
    IndexListViewIterator& operator++() {
      ++index_iter_;
      return *this;
    }
    decltype(auto) operator*() const {
      return values_[static_cast<size_t>(*index_iter_)];
    }

   private:
    absl::Span<value_type> values_;
    index_iterator index_iter_;
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
    // TODO(user): we could check that index is in indices_, but that's is O(n).
    return values_[static_cast<size_t>(index)];
  }
  IndexListViewIterator begin() const {
    return IndexListViewIterator(values_, indices_.begin());
  }
  IndexListViewIterator end() const {
    return IndexListViewIterator(values_, indices_.end());
  }

 private:
  absl::Span<value_type> values_;
  absl::Span<index_type> indices_;
};

// This view provides access to elements of a container of integral types, which
// are used as indices to a filter (in) vector.
// The view filters and returns only those elements that, when used a subscript
// to the filter vector, are evaluated to a true value.
// Looping over this view is equivalent to:
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
    ValueFilterViewIterator(value_iterator iterator, value_iterator end,
                            const EnableVectorT* is_active)
        : iterator_(iterator), end_(end), is_active_(is_active) {
      AdjustToValidValue();
    }
    bool operator!=(const ValueFilterViewIterator& other) const {
      DCHECK_EQ(is_active_, other.is_active_);
      return iterator_ != other.iterator_;
    }
    ValueFilterViewIterator& operator++() {
      ++iterator_;
      AdjustToValidValue();
      return *this;
    }
    decltype(auto) operator*() const { return *iterator_; }

   private:
    void AdjustToValidValue() {
      while (iterator_ != end_ && !util::at(is_active_, *iterator_)) {
        ++iterator_;
      }
    }

    value_iterator iterator_;
    value_iterator end_;
    const EnableVectorT* is_active_;
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
    IndexFilterViewIterator(value_iterator iterator,
                            enable_iterator is_active_begin,
                            enable_iterator is_active_end)
        : iterator_(iterator),
          is_active_iter_(is_active_begin),
          is_active_end_(is_active_end) {
      AdjustToValidValue();
    }
    bool operator!=(const IndexFilterViewIterator& other) const {
      DCHECK(is_active_end_ == other.is_active_end_);
      return iterator_ != other.iterator_;
    }
    IndexFilterViewIterator& operator++() {
      ++is_active_iter_;
      ++iterator_;
      AdjustToValidValue();
      return *this;
    }
    decltype(auto) operator*() const { return *iterator_; }

   private:
    void AdjustToValidValue() {
      while (is_active_iter_ != is_active_end_ && !*is_active_iter_) {
        ++is_active_iter_;
        ++iterator_;
      }
    }

    value_iterator iterator_;
    enable_iterator is_active_iter_;
    enable_iterator is_active_end_;
  };

  template <typename ValueRangeT>
  IndexFilterView(const ValueRangeT* values, const EnableVectorT* is_active_)
      : values_(absl::MakeConstSpan(*values)), is_active_(is_active_) {
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
// 1. The first dimension is generic and can be both and index-list or
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
    TwoLevelsViewIterator(level1_iterator iter,
                          const EnableVectorT* active_items)
        : iter_(iter), active_items_(active_items) {}
    bool operator!=(const TwoLevelsViewIterator& other) const {
      DCHECK_EQ(active_items_, other.active_items_);
      return iter_ != other.iter_;
    }
    TwoLevelsViewIterator& operator++() {
      ++iter_;
      return *this;
    }
    level2_type operator*() const {
      return level2_type(&(*iter_), active_items_);
    }

   private:
    level1_iterator iter_;
    const EnableVectorT* active_items_;
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

}  // namespace util_intops

#endif /* OR_TOOLS_ORTOOLS_SET_COVER_VIEWS_H */
