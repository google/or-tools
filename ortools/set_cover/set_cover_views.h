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

#ifndef OR_TOOLS_ORTOOLS_SET_COVER_SET_COVER_VIEWS_H
#define OR_TOOLS_ORTOOLS_SET_COVER_SET_COVER_VIEWS_H

#include <absl/meta/type_traits.h>

#include "ortools/set_cover/base_types.h"
#include "ortools/set_cover/set_cover_model.h"

namespace operations_research::scp {

namespace util {
template <typename RangeT>
struct range_traits {
  using range_type = RangeT;
  using iterator_type = decltype(std::declval<RangeT>().begin());
  using const_iterator_type = decltype(std::declval<const RangeT>().begin());
  using value_type =
      absl::remove_cvref_t<decltype(*std::declval<iterator_type>())>;
};

template <typename RangeT>
using range_iterator_type = typename range_traits<RangeT>::iterator_type;
template <typename RangeT>
using range_const_iterator_type =
    typename range_traits<RangeT>::const_iterator_type;
template <typename RangeT>
using range_value_type = typename range_traits<RangeT>::value_type;

// Enable compatibility with STL algorithms.
template <typename IterT, typename ValueT>
struct IteratorCRTP {
  using iterator_category = std::input_iterator_tag;
  using value_type = ValueT;
  using difference_type = BaseInt;
  using pointer = IterT;
  using reference = value_type&;
  bool operator==(const IterT& other) const {
    return !(*static_cast<const IterT*>(this) != other);
  }
  pointer operator->() const { return static_cast<const IterT*>(this); }
};

template <typename ContainerT, typename IndexT>
decltype(auto) at(const ContainerT* container, IndexT index) {
  DCHECK(IndexT(0) <= index && index < IndexT(container->size()));
  return (*container)[index];
}

}  // namespace util

// View exposing only the elements of a container that are indexed by a list of
// indices.
// Looping over this view is equivalent to:
//
// for (decltype(auto) index : indices) {
//   your_code(container[index]);
// }
//
template <typename ContainerT, typename IndexListT>
class IndexListView {
 public:
  using container_iterator = util::range_const_iterator_type<ContainerT>;
  using value_type = util::range_value_type<ContainerT>;
  using index_iterator = util::range_const_iterator_type<IndexListT>;
  using index_type = util::range_value_type<IndexListT>;

  struct IndexListViewIterator
      : util::IteratorCRTP<IndexListViewIterator, value_type> {
    IndexListViewIterator(const ContainerT* container, index_iterator iter)
        : container_(container), index_iter_(iter) {}
    bool operator!=(const IndexListViewIterator& other) const {
      DCHECK_EQ(container_, other.container_);
      return index_iter_ != other.index_iter_;
    }
    IndexListViewIterator& operator++() {
      ++index_iter_;
      return *this;
    }
    decltype(auto) operator*() const {
      return util::at(container_, *index_iter_);
    }

   private:
    const ContainerT* container_;
    index_iterator index_iter_;
  };

  IndexListView(const ContainerT* container, const IndexListT* indices)
      : container_(container), indices_(indices) {}
  BaseInt size() const { return indices_->size(); }
  bool empty() const { return size() == 0; }
  decltype(auto) operator[](index_type index) const {
    return util::at(container_, index);
  }

  IndexListViewIterator begin() const {
    return IndexListViewIterator(container_, indices_->begin());
  }
  IndexListViewIterator end() const {
    return IndexListViewIterator(container_, indices_->end());
  }

 private:
  const ContainerT* container_;
  const IndexListT* indices_;
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
template <typename ContainerT, typename EnableVectorT>
class ValueFilterView {
 public:
  using value_type = util::range_value_type<ContainerT>;
  using container_iterator = util::range_const_iterator_type<ContainerT>;

  struct ValueFilterViewIterator
      : util::IteratorCRTP<ValueFilterViewIterator, value_type> {
    ValueFilterViewIterator(container_iterator iterator, container_iterator end,
                            const EnableVectorT* is_active)
        : iterator_(iterator), end_(end), is_active_(is_active) {
      AdjustToValidValue();
    }
    bool operator!=(const ValueFilterViewIterator& other) const {
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

    container_iterator iterator_;
    container_iterator end_;
    const EnableVectorT* is_active_;
  };

  ValueFilterView(const ContainerT* container, const EnableVectorT* is_active,
                  BaseInt size)
      : container_(container), is_active_(is_active), size_(size) {
    DCHECK(container != nullptr);
    DCHECK(is_active != nullptr);
  }
  ValueFilterViewIterator begin() const {
    return ValueFilterViewIterator(container_->begin(), container_->end(),
                                   is_active_);
  }
  ValueFilterViewIterator end() const {
    return ValueFilterViewIterator(container_->end(), container_->end(),
                                   is_active_);
  }
  BaseInt size() const { return size_; }
  bool empty() const { return size() == 0; }

  template <typename IndexT>
  decltype(auto) operator[](IndexT index) const {
    return util::at(container_, index);
  }

  BaseInt size() const { return is_active_ ? size_ : container_->size(); }
  bool empty() const { return size() == 0; }

 private:
  const ContainerT* container_;
  const EnableVectorT* is_active_;
  BaseInt size_;
};

// This view provides a mechanism to access and filter elements in a 2D
// container. The filtering is applied in two stages:
// 1. The first dimension is generic and can be both and index-list or
//    bool-vector based view.
// 2. The second dimension (items of each sub-container) is further filtered
//    using a boolean vector, which determines which elements within the
//    sub-container are included in the view.
template <typename Lvl1ViewT, typename SizeViewT, typename EnableVectorT>
class TwoLevelsView : public Lvl1ViewT {
 public:
  using level1_iterator = util::range_const_iterator_type<Lvl1ViewT>;
  using level1_value = util::range_value_type<Lvl1ViewT>;
  using level2_type = ValueFilterView<level1_value, EnableVectorT>;
  using sizes_iterator = util::range_const_iterator_type<SizeViewT>;

  struct SparseFocus2DViewIterator
      : util::IteratorCRTP<SparseFocus2DViewIterator, level2_type> {
    SparseFocus2DViewIterator(level1_iterator iter,
                              const EnableVectorT* active_items,
                              sizes_iterator size_iter)
        : iter_(iter), active_items_(active_items), sizes_iter_(size_iter) {}
    bool operator!=(const SparseFocus2DViewIterator& other) const {
      return iter_ != other.iter_;
    }
    SparseFocus2DViewIterator& operator++() {
      ++iter_;
      ++sizes_iter_;
      return *this;
    }
    level2_type operator*() const {
      return level2_type(&(*iter_), active_items_, *sizes_iter_);
    }

   private:
    level1_iterator iter_;
    const EnableVectorT* active_items_;
    sizes_iterator sizes_iter_;
  };

  TwoLevelsView() = default;
  TwoLevelsView(Lvl1ViewT lvl1_view, SizeViewT sizes_view,
                const EnableVectorT* active_items)
      : Lvl1ViewT(lvl1_view),
        sizes_view_(sizes_view),
        active_items_(active_items) {}
  SparseFocus2DViewIterator begin() const {
    return SparseFocus2DViewIterator(Lvl1ViewT::begin(), active_items_,
                                     sizes_view_.begin());
  }
  SparseFocus2DViewIterator end() const {
    return SparseFocus2DViewIterator(Lvl1ViewT::end(), active_items_,
                                     sizes_view_.end());
  }

  template <typename indexT>
  level2_type operator[](indexT i) const {
    auto& level2_container = Lvl1ViewT::operator[](i);
    return level2_type(&level2_container, active_items_, sizes_view_[i]);
  }

 private:
  SizeViewT sizes_view_;
  const EnableVectorT* active_items_;
};

  const EnableVectorT* active_elements_;
  sizes_view_type sizes_;
};

// Represents a range of integral values, typically used to convert a boolean
// vector into a sequence of indices. This allows iteration over the indices
// corresponding to `true` values in the boolean vector, enabling  filtering and
// traversal of elements without allocation needed for the index list.
template <typename IntType>
struct IntRange {
  struct IntRangeIterator {
    IntType index_;
    bool operator==(const IntRangeIterator& other) const {
      return index_ == other.index_;
  }
    bool operator!=(const IntRangeIterator& other) const {
      return index_ != other.index_;
  }
    IntRangeIterator& operator++() {
      ++index_;
      return *this;
    }
    IntType operator*() const { return index_; }
  };

  IntRange() = default;
  IntRange(BaseInt size_) : begin_(), size_(size_) {}
  IntRange(IntType begin, BaseInt size_t) : begin_(begin), size_(size) {}

  BaseInt size() const { return size_; }
  bool empty() const { return size() == 0; }
  IntRangeIterator begin() const { return IntRangeIterator{begin_}; }
  IntRangeIterator end() const { return IntRangeIterator{begin_ + size_}; }
  IntType operator[](IntType index) const {
    DCHECK(IntType{} <= index && index < size_);
    return begin_ + index;
  }

 private:
  IntType begin_;
  BaseInt size_;  // Storing end_ does not work with StrongInt
};

}  // namespace operations_research::scp

#endif /* OR_TOOLS_ORTOOLS_SET_COVER_SET_COVER_VIEWS_H */
