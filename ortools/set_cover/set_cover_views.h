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

#include "ortools/set_cover/base_types.h"

namespace operations_research::scp {

// To keep it simple, views iterators do not belong STL iterator category.
// Thus, some range operations are here duplicated.
template <typename ContainerT, typename Op>
void for_each(const ContainerT& container, Op op) {
  for (const auto& elem : container) {
    op(elem);
  }
}
template <typename ContainerT, typename Op>
bool any_of(const ContainerT& container, Op op) {
  for (const auto& elem : container) {
    if (op(elem)) {
      return true;
    }
  }
  return false;
}
template <typename ContainerT, typename Op>
bool none_of(const ContainerT& container, Op op) {
  return !any_of(container, op);
}
template <typename ContainerT, typename Op>
bool all_of(const ContainerT& container, Op op) {
  return !any_of(container, [&](const auto& elem) { return !op(elem); });
}

// View exposing only the elements of a container that are indexed by a list of
// indices. The list pointer can be null, in which case all elements are
// returned.
template <typename ContainerT, typename IndexListT>
class IndexListView {
 public:
  using container_iterator = decltype(std::declval<const ContainerT>().begin());
  using index_iterator = decltype(std::declval<const IndexListT>().begin());
  using index_type = std::remove_cv_t<
      std::remove_reference_t<decltype(*std::declval<index_iterator>())>>;

  struct IndexListViewIterator {
    IndexListViewIterator(const ContainerT* container, index_iterator iter)
        : container_(container), index_iter_(iter) {}

    bool operator==(const IndexListViewIterator& other) const {
      DCHECK_EQ(container_, other.container_);
      return index_iter_ == other.index_iter_;
    }
    bool operator!=(const IndexListViewIterator& other) const {
      return !(*this == other);
    }
    IndexListViewIterator& operator++() {
      ++index_iter_;
      return *this;
    }
    decltype(auto) operator*() const {
      DCHECK(index_type(0) <= *index_iter_ &&
             *index_iter_ < index_type(container_->size()));
      return (*container_)[*index_iter_];
    }

   private:
    const ContainerT* container_;
    index_iterator index_iter_;
  };

  IndexListView(const ContainerT* container, const IndexListT* indices)
      : container_(container), indices_(indices) {}
  BaseInt size() const { return indices_->size(); }
  bool empty() const { return size() == 0; }
  decltype(auto) AtRelativeIndex(BaseInt index) const {
    DCHECK(0 <= index && index < size());
    return (*container_)[(*indices_)[index]];
  }
  decltype(auto) operator[](index_type index) const {
    DCHECK(index_type(0) <= index && index < index_type(container_->size()));
    return (*container_)[index];
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
// are used as indices to a boolean vector. The view filters and returns only
// those elements for which the corresponding boolean vector entry is set to
// true. If the boolean vector pointer is null, the view includes all elements
// from the container without applying any filtering.
template <typename ContainerT, typename EnableVectorT>
class IndexListFilter {
 public:
  using container_iterator = decltype(std::declval<const ContainerT>().begin());

  class IndexListFilterIterator {
   public:
    IndexListFilterIterator(container_iterator iterator, container_iterator end,
                            const EnableVectorT* is_active)
        : iterator_(iterator), end_(end), is_active_(is_active) {
      AdjustToValidValue();
    }
    bool operator==(const IndexListFilterIterator& other) const {
      return iterator_ == other.iterator_;
    }
    bool operator!=(const IndexListFilterIterator& other) const {
      return iterator_ != other.iterator_;
    }
    IndexListFilterIterator& operator++() {
      ++iterator_;
      AdjustToValidValue();
      return *this;
    }
    decltype(auto) operator*() const { return *iterator_; }

   private:
    void AdjustToValidValue() {
      while (iterator_ != end_ && !(*is_active_)[*iterator_]) {
        ++iterator_;
      }
    }

    container_iterator iterator_;
    container_iterator end_;
    const EnableVectorT* is_active_;
  };

  IndexListFilter(const ContainerT* container, const EnableVectorT* is_active_,
                  BaseInt size)
      : container_(container), is_active_(is_active_), size_(size) {}
  IndexListFilterIterator begin() const {
    return IndexListFilterIterator(container_->begin(), container_->end(),
                                   is_active_);
  }
  IndexListFilterIterator end() const {
    return IndexListFilterIterator(container_->end(), container_->end(),
                                   is_active_);
  }
  BaseInt size() const { return is_active_ ? size_ : container_->size(); }
  bool empty() const { return size() == 0; }

 private:
  const ContainerT* container_;
  const EnableVectorT* is_active_;
  BaseInt size_;
};

// This view provides a mechanism to access and filter elements in a 2D sparse
// container. The filtering is applied in two stages:
// 1. The first dimension is filtered based on a provided list of indices,
//    allowing selective access to specific sub-containers.
// 2. The second dimension (elements of each sub-container) is further filtered
//    using a boolean vector, which determines which elements within the
//    sub-container are included in the view.
template <typename SparseContainer2D, typename IndexListT,
          typename EnableVectorT, typename SizeVectorT>
class SparseFilteredView : public IndexListView<SparseContainer2D, IndexListT> {
 public:
  using base = IndexListView<SparseContainer2D, IndexListT>;
  using dim2_container_type = typename SparseContainer2D::value_type;
  using dim2_view_type = IndexListFilter<dim2_container_type, EnableVectorT>;
  using dim1_view_iterator = decltype(std::declval<base>().begin());
  using sizes_view_type = IndexListView<SizeVectorT, IndexListT>;
  using sizes_iterator = decltype(std::declval<sizes_view_type>().begin());
  using index_type = std::remove_reference_t<
      std::remove_cv_t<decltype(*std::declval<IndexListT>().begin())>>;

  class SparseFocus2DViewIterator {
   public:
    SparseFocus2DViewIterator(dim1_view_iterator iter,
                              const EnableVectorT* active_elements,
                              sizes_iterator size_iter)
        : iter_(iter),
          active_elements_(active_elements),
          sizes_iter_(size_iter) {}
    bool operator==(const SparseFocus2DViewIterator& other) const {
      return iter_ == other.iter_;
    }
    bool operator!=(const SparseFocus2DViewIterator& other) const {
      return iter_ != other.iter_;
    }
    SparseFocus2DViewIterator& operator++() {
      ++iter_;
      ++sizes_iter_;
      return *this;
    }
    dim2_view_type operator*() const {
      return dim2_view_type(&(*iter_), active_elements_, *sizes_iter_);
    }

   private:
    dim1_view_iterator iter_;
    const EnableVectorT* active_elements_;
    sizes_iterator sizes_iter_;
  };

  SparseFilteredView() = default;
  SparseFilteredView(const SparseContainer2D* container,
                     const IndexListT* focus_indices,
                     const EnableVectorT* active_elements,
                     const SizeVectorT* sizes)
      : base(container, focus_indices),
        active_elements_(active_elements),
        sizes_(sizes, focus_indices) {}

  SparseFocus2DViewIterator begin() const {
    return SparseFocus2DViewIterator(base::begin(), active_elements_,
                                     sizes_.begin());
  }
  SparseFocus2DViewIterator end() const {
    return SparseFocus2DViewIterator(base::end(), active_elements_,
                                     sizes_.end());
  }
  dim2_view_type operator[](index_type i) const {
    return dim2_view_type(&base::operator[](i), active_elements_, sizes_[i]);
  }

 private:
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
