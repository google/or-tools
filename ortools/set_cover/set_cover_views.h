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

#ifndef CAV_OR_TOOLS_ORTOOLS_SET_COVER_SET_COVER_VIEWS_H
#define CAV_OR_TOOLS_ORTOOLS_SET_COVER_SET_COVER_VIEWS_H

#include <vector>

#include "ortools/set_cover/base_types.h"

namespace operations_research::scp {

// View exposing only the elements of a container that are indexed by a list of
// indices. The list pointer can be null, in which case all elements are
// returned.
template <typename ContainerT, typename IndexListT = std::vector<int>>
class IndexListView {
 public:
  using container_iterator = decltype(std::declval<const ContainerT>().begin());
  using index_iterator = decltype(std::declval<const IndexListT>().begin());
  using index_type = decltype(*std::declval<index_iterator>());

  struct IndexListViewIterator {
    container_iterator elements_iter_;
    index_iterator index_iter_;
    bool no_indices_;

    bool operator!=(const IndexListViewIterator& other) const {
      return elements_iter_ != other.elements_iter_ |
             index_iter_ != other.index_iter_;
    }
    IndexListViewIterator& operator++() {
      elements_iter_ += no_indices_ ? 1 : 0;
      index_iter_ += !no_indices_ ? 1 : 0;
      return *this;
    }
    decltype(auto) operator*() const {
      return elements_iter_[no_indices_ ? 0 : *index_iter_];
    }
  };

  IndexListView(const ContainerT& container, const IndexListT* element_list)
      : begin_(container.begin()),
        end_(container.end()),
        indices_(element_list) {}
  IndexListView(const ContainerT& container, std::nullptr_t)
      : begin_(container.begin()), end_(container.end()), indices_(nullptr) {}
  BaseInt size() const {
    return indices_ ? indices_->end() - indices_->begin() : end_ - begin_;
  }
  bool empty() const { return size() == 0; }
  decltype(auto) operator[](BaseInt index) const {
    DCHECK(0 <= index && index < size());
    return begin_[indices_ ? (*indices_->begin())[index] : index_type(index)];
  }
  decltype(auto) AtOriginalIndex(index_type index) const {
    DCHECK(0 <= index && index < end_ - begin_);
    return begin_[index];
  }

  IndexListViewIterator begin() const {
    if (indices_ == nullptr) return IndexListViewIterator(begin_, {}, true);
    return IndexListViewIterator(begin_, indices_->begin(), false);
  }
  IndexListViewIterator end() const {
    if (indices_ == nullptr) return IndexListViewIterator(end_, {}, true);
    return IndexListViewIterator(begin_, indices_->end(), false);
  }

 private:
  container_iterator begin_;
  container_iterator end_;
  const IndexListT* indices_;
};

// This view provides access to elements of a container of integral types, which
// are used as indices to a boolean vector. The view filters and returns only
// those elements for which the corresponding boolean vector entry is set to
// true. If the boolean vector pointer is null, the view includes all elements
// from the container without applying any filtering.
template <typename ContainerT, typename BoolVectorT = std::vector<bool>>
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
  IndexListFilter(const ContainerT& container, std::nullptr_t)
      : begin_(container.begin()), end_(container.end()), is_active_(nullptr) {}
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
class SparseFilteredView : IndexListView<SparseContainer2D, IndexListT> {
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
    const IndexListFilter<dim2_container_type>& operator*() const {
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
  IndexListFilter<dim2_container_type> operator[](BaseInt focus_j) const {
    return IndexListFilter<dim2_container_type>(base::operator[](focus_j),
                                                active_elements_);
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
  IntType operator[](BaseInt index) const {
    DCHECK(0 <= index && index < size());
    return begin_ + index;
  }

 private:
  IntType begin_;
  IntType end_;
};

}  // namespace operations_research::scp

#endif /* CAV_OR_TOOLS_ORTOOLS_SET_COVER_SET_COVER_VIEWS_H */
