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
    IndexListViewIterator(const ContainerT* container, size_t index)
        : container_(container), pos_(index), use_index_(true) {}
    IndexListViewIterator(const ContainerT* container, index_iterator iter)
        : container_(container),
          pos_(idx_or_iter{.iter = iter}),
          use_index_(false) {}

    bool operator!=(const IndexListViewIterator& other) const {
      DCHECK_EQ(container_, other.container_);
      return use_index_ ? pos_.index != other.pos_.index
                        : pos_.iter != other.pos_.iter;
    }
    IndexListViewIterator& operator++() {
      if (use_index_) {
        ++pos_.index;
      } else {
        ++pos_.iter;
      }
      return *this;
    }
    decltype(auto) operator*() const {
      return (*container_)[use_index_ ? index_type(pos_.index) : *pos_.iter];
    }

   private:
    const ContainerT* container_;
    union idx_or_iter {
      size_t index;
      index_iterator iter;
    } pos_;
    bool use_index_;
  };

  IndexListView(const ContainerT& container, const IndexListT* element_list)
      : container_(&container), indices_(element_list) {}
  BaseInt size() const {
    return indices_ ? indices_->end() - indices_->begin() : container_->size();
  }
  bool empty() const { return size() == 0; }
  decltype(auto) AtRelativeIndex(BaseInt index) const {
    DCHECK(0 <= index && index < size());
    return (*container_)[indices_ ? (*indices_)[index] : index_type(index)];
  }
  decltype(auto) operator[](index_type index) const {
    DCHECK(0 <= index && index < container_->size());
    return (*container_)[index];
  }

  IndexListViewIterator begin() const {
    if (indices_ == nullptr) {
      return IndexListViewIterator(container_, 0ULL);
    }
    return IndexListViewIterator(container_, indices_->begin());
  }
  IndexListViewIterator end() const {
    if (indices_ == nullptr) {
      return IndexListViewIterator(container_, container_->size());
    }
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
template <typename ContainerT, typename BoolVectorT>
class IndexListFilter {
 public:
  using container_iterator = decltype(std::declval<const ContainerT>().begin());

  struct IndexListFilterIterator {
    mutable container_iterator iterator_;
    container_iterator end_;
    const BoolVectorT* is_active_;
    bool operator!=(const IndexListFilterIterator& other) const {
      return iterator_ != other.iterator_;
    }
    IndexListFilterIterator& operator++() {
      ++iterator_;
      return *this;
    }
    decltype(auto) operator*() const {
      while (is_active_ && iterator_ != end_ && !(*is_active_)[*iterator_]) {
        ++iterator_;
      }
      return *iterator_;
    }
  };

  IndexListFilter(const ContainerT& container, const BoolVectorT* is_active_)
      : begin_(container.begin()),
        end_(container.end()),
        is_active_(is_active_) {}
  IndexListFilterIterator begin() const {
    return IndexListFilterIterator(begin_, end_, is_active_);
  }
  IndexListFilterIterator end() const {
    return IndexListFilterIterator(end_, end_, is_active_);
  }

 private:
  container_iterator begin_;
  container_iterator end_;
  const BoolVectorT* is_active_;
};

// This view provides a mechanism to access and filter elements in a 2D sparse
// container. The filtering is applied in two stages:
// 1. The first dimension is filtered based on a provided list of indices,
//    allowing selective access to specific sub-containers.
// 2. The second dimension (elements of each sub-container) is further filtered
//    using a boolean vector, which determines which elements within the
//    sub-container are included in the view.
template <typename SparseContainer2D, typename IndexListT, typename BoolVectorT>
class SparseFilteredView : public IndexListView<SparseContainer2D, IndexListT> {
 public:
  using base = IndexListView<SparseContainer2D, IndexListT>;
  using dim2_container_type = typename SparseContainer2D::value_type;
  using dim2_view_type = IndexListFilter<dim2_container_type, BoolVectorT>;
  using dim1_view_iterator = decltype(std::declval<base>().begin());

  class SparseFocus2DViewIterator {
   public:
    SparseFocus2DViewIterator(dim1_view_iterator iter,
                              const BoolVectorT* active_elements)
        : iter_(iter), active_elements_(active_elements) {}
    bool operator!=(const SparseFocus2DViewIterator& other) const {
      return iter_ != other.iter_;
    }
    SparseFocus2DViewIterator& operator++() {
      ++iter_;
      return *this;
    }
    const dim2_view_type& operator*() const {
      return IndexListFilter(*iter_, active_elements_);
    }

   private:
    dim1_view_iterator iter_;
    const BoolVectorT* active_elements_;
  };

  SparseFilteredView() = default;
  SparseFilteredView(const SparseContainer2D& container,
                     const IndexListT* focus_indices,
                     const BoolVectorT* active_elements)
      : base(container, focus_indices), active_elements_(active_elements) {}

  SparseFocus2DViewIterator begin() const {
    return SparseFocus2DViewIterator(base::begin(), active_elements_);
  }
  SparseFocus2DViewIterator end() const {
    return SparseFocus2DViewIterator(base::end(), active_elements_);
  }
  template <typename T>
  dim2_view_type operator[](T j) const {
    return dim2_view_type(base::operator[](j), active_elements_);
  }

 private:
  const BoolVectorT* active_elements_;
};

// Represents a range of integral values, typically used to convert a boolean
// vector into a sequence of indices. This allows iteration over the indices
// corresponding to `true` values in the boolean vector, enabling  filtering and
// traversal of elements without allocation needed for the index list.
template <typename IntType>
struct IntRange {
  struct IntRangeIterator {
    IntType index_;
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
  IntRange(IntType end) : begin_(), end_(end) {}
  IntRange(IntType begin, IntType end) : begin_(begin), end_(end) {}

  IntType size() const { return end_ - begin_; }
  bool empty() const { return size() == 0; }
  IntRangeIterator begin() const { return IntRangeIterator{begin_}; }
  IntRangeIterator end() const { return IntRangeIterator{end_}; }
  IntType operator[](IntType index) const {
    DCHECK(IntType{} <= index && index < end_ - begin_);
    return begin_ + index;
  }

 private:
  IntType begin_;
  IntType end_;
};

}  // namespace operations_research::scp

#endif /* OR_TOOLS_ORTOOLS_SET_COVER_SET_COVER_VIEWS_H */
