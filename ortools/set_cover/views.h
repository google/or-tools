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
#include <functional>
#include <iterator>
#include <type_traits>

#include "absl/log/check.h"
#include "absl/types/span.h"

namespace operations_research {

namespace views_util {

// Helper to determine the const iterator type for a given range type.
template <typename RangeT>
using range_const_iterator_type =
    std::remove_cvref_t<decltype(std::declval<const RangeT>().begin())>;

// Helper to determine the value type for a given range type.
template <typename RangeT>
using range_value_type = std::remove_cvref_t<
    decltype(*std::declval<range_const_iterator_type<RangeT>>())>;

// Enable compatibility with STL algorithms.
template <typename IterT, typename ValueT>
struct IteratorCRTP {
  // Category of the iterator.
  using iterator_category = std::input_iterator_tag;

  // Type of values returned by the iterator.
  using value_type = ValueT;

  // Signed integer type for representing differences.
  using difference_type = std::ptrdiff_t;

  // Type that represents a pointer to the iterator itself.
  using pointer = IterT;

  // Reference type to the value.
  using reference = value_type&;

  // Comparison operator to check for equality.
  bool operator==(const IterT& other) const {
    return !(*static_cast<const IterT*>(this) != other);
  }

  // Member access operator returning the pointer.
  pointer operator->() const {
    const pointer result = static_cast<const IterT*>(this);
    DCHECK(result != nullptr);
    return result;
  }
};

// Accesses a element in a container at the specified index, with bounds check.
template <typename ValueRangeT, typename IndexT>
decltype(auto) at(const ValueRangeT* container, IndexT index) {
  DCHECK(container != nullptr);
  DCHECK(IndexT(0) <= index && index < IndexT(container->size()));
  return (*container)[index];
}

}  // namespace views_util

// View exposing only the integer indices [0, mask.size()) where the mask
// evaluates to true. Looping over this view is equivalent to:
//
// for (IntegerT index(0); index < IntegerT(mask.size()); ++index) {
//   if (mask[index]) {
//     your_code(index);
//   }
// }
//
template <typename IntegerT, typename FilterMaskT>
class MaskedIndicesView {
 public:
  // Iterator over the active indices.
  struct MaskedIndicesViewIterator
      : views_util::IteratorCRTP<MaskedIndicesViewIterator, IntegerT> {
    // Constructor.
    MaskedIndicesViewIterator(IntegerT index, const FilterMaskT& mask)
        : index_(index), mask_(mask) {
      DCHECK_GE(index, IntegerT(0));
      AdjustToValidValue();
    }

    // Inequality operator.
    bool operator!=(MaskedIndicesViewIterator other) const {
      return index_ != other.index_;
    }

    // Pre-increment operator.
    MaskedIndicesViewIterator& operator++() {
      DCHECK_LT(index_, IntegerT(mask_.get().size()));
      ++index_;
      AdjustToValidValue();
      return *this;
    }

    // Dereference operator.
    IntegerT operator*() const {
      DCHECK_GE(index_, IntegerT(0));
      DCHECK_LT(index_, IntegerT(mask_.get().size()));
      DCHECK(views_util::at(&mask_.get(), index_));
      return index_;
    }

   private:
    // Moves the index forward to the next index where the mask
    // evaluates to true.
    void AdjustToValidValue() {
      while (index_ < IntegerT(mask_.get().size()) &&
             !views_util::at(&mask_.get(), index_)) {
        ++index_;
      }
    }

    // The current index.
    IntegerT index_;

    // Reference wrapper for the underlying mask.
    std::reference_wrapper<const FilterMaskT> mask_;
  };

  // Constructor with a reference to the filter mask.
  explicit MaskedIndicesView(const FilterMaskT& mask) : mask_(mask) {}

  // Returns the iterator to the beginning of the active indices.
  MaskedIndicesViewIterator begin() const {
    return MaskedIndicesViewIterator(IntegerT(0), mask_.get());
  }

  // Returns the iterator to the end of the active indices.
  MaskedIndicesViewIterator end() const {
    return MaskedIndicesViewIterator(IntegerT(mask_.get().size()), mask_.get());
  }

 private:
  // Reference wrapper for the underlying mask.
  std::reference_wrapper<const FilterMaskT> mask_;
};

// View exposing only the elements of a value container that are selected by a
// list/span of indices. Looping over this view is equivalent to:
//
// for (const auto index : indices) {
//   your_code(values[index]);
// }
//
template <typename ValueT, typename IndexT>
class IndexListView {
 public:
  // The type of values in the view.
  using value_type = const ValueT;

  // The type of index.
  using index_type = const IndexT;

  // The value iterator type.
  using value_iterator = typename absl::Span<value_type>::iterator;

  // The index iterator type.
  using index_iterator = typename absl::Span<index_type>::iterator;

  // Iterator for index list views.
  struct IndexListViewIterator
      : views_util::IteratorCRTP<IndexListViewIterator, value_type> {
    // Constructor.
    IndexListViewIterator(absl::Span<value_type> values,
                          index_iterator index_iterator)
        : values_(values), index_iterator_(index_iterator) {}

    // Inequality operator.
    bool operator!=(const IndexListViewIterator& other) const {
      DCHECK_EQ(values_.data(), other.values_.data());
      return index_iterator_ != other.index_iterator_;
    }

    // Pre-increment operator.
    IndexListViewIterator& operator++() {
      ++index_iterator_;
      return *this;
    }

    // Dereference operator.
    decltype(auto) operator*() const {
      DCHECK(index_iterator_ != nullptr);
      const size_t idx = static_cast<size_t>(*index_iterator_);
      DCHECK_LT(idx, values_.size());
      return values_[idx];
    }

   private:
    // Span containing the underlying values.
    absl::Span<value_type> values_;

    // Iterator to the current index.
    index_iterator index_iterator_;
  };

  // Default constructor.
  IndexListView() = default;

  // Constructor taking reference arrays for both values and indices.
  template <typename ValueRangeT, typename IndexRangeT>
  IndexListView(const ValueRangeT* values, const IndexRangeT* indices)
      : values_(absl::MakeConstSpan(*values)),
        indices_(absl::MakeConstSpan(*indices)) {
    DCHECK(values != nullptr);
    DCHECK(indices != nullptr);
  }

  // Returns the size of the view (number of indices).
  auto size() const { return indices_.size(); }

  // Returns true if the view is empty.
  bool empty() const { return indices_.empty(); }

  // Accesses the element by index in the underlying original container.
  // NOTE: uses indices of the original container, not the filtered one
  decltype(auto) operator[](index_type index) const {
    DCHECK(IsIndexInIndices(index));
    return values_[static_cast<size_t>(index)];
  }

  // Returns the iterator to the beginning of the view.
  IndexListViewIterator begin() const {
    return IndexListViewIterator(values_, indices_.begin());
  }

  // Returns the iterator to the end of the view.
  IndexListViewIterator end() const {
    return IndexListViewIterator(values_, indices_.end());
  }

  // Returns the underlying base container as a span.
  absl::Span<value_type> base() const { return values_; }

 private:
  // Returns true if the given index is present in indices_.
  bool IsIndexInIndices(index_type index) const {
    for (const index_type idx : indices_) {
      if (idx == index) {
        return true;
      }
    }
    return false;
  }

  // Span containing the underlying values.
  absl::Span<value_type> values_;

  // Span containing the list of indices.
  absl::Span<index_type> indices_;
};

// View exposing only the elements of a container (which are integral
// indices/IDs) whose corresponding entry in a boolean filter mask evaluates to
// true. Looping over this view is equivalent to:
//
// for (const auto item : values) {
//   if (mask[item]) {
//     your_code(item);
//   }
// }
//
template <typename ValueT, typename FilterMaskT>
class IndirectMaskedView {
 public:
  // The type of values in the view.
  using value_type = const ValueT;

  // The value iterator type.
  using value_iterator = typename absl::Span<value_type>::iterator;

  // Iterator for indirect masked views.
  struct IndirectMaskedViewIterator
      : views_util::IteratorCRTP<IndirectMaskedViewIterator, value_type> {
    // Constructor.
    IndirectMaskedViewIterator(value_iterator iterator_begin,
                               value_iterator iterator_end,
                               const FilterMaskT& mask)
        : current_iter_(iterator_begin), end_iter_(iterator_end), mask_(mask) {
      AdjustToValidValue();
    }

    // Inequality operator.
    bool operator!=(const IndirectMaskedViewIterator& other) const {
      DCHECK_EQ(&mask_.get(), &other.mask_.get());
      return current_iter_ != other.current_iter_;
    }

    // Pre-increment operator.
    IndirectMaskedViewIterator& operator++() {
      DCHECK(current_iter_ != end_iter_);
      ++current_iter_;
      AdjustToValidValue();
      return *this;
    }

    // Dereference operator.
    decltype(auto) operator*() const {
      DCHECK(current_iter_ != end_iter_);
      DCHECK(views_util::at(&mask_.get(), *current_iter_));
      return *current_iter_;
    }

   private:
    // Moves the iterator forward to the next element where the mask
    // evaluates to true.
    void AdjustToValidValue() {
      while (current_iter_ != end_iter_ &&
             !views_util::at(&mask_.get(), *current_iter_)) {
        ++current_iter_;
      }
    }

    // The current iterator position.
    value_iterator current_iter_;

    // The end of the underlying range.
    value_iterator end_iter_;

    // Reference wrapper for the underlying mask.
    std::reference_wrapper<const FilterMaskT> mask_;
  };

  // Constructor taking values container and the filter mask.
  template <typename ValueRangeT>
  IndirectMaskedView(const ValueRangeT* values, const FilterMaskT& mask)
      : values_(absl::MakeConstSpan(*values)), mask_(mask) {
    DCHECK(values != nullptr);
  }

  // Returns the iterator to the beginning of the view.
  IndirectMaskedViewIterator begin() const {
    return IndirectMaskedViewIterator(values_.begin(), values_.end(),
                                      mask_.get());
  }

  // Returns the iterator to the end of the view.
  IndirectMaskedViewIterator end() const {
    return IndirectMaskedViewIterator(values_.end(), values_.end(),
                                      mask_.get());
  }

  // Accesses the element in the view using the index of the original container.
  // NOTE: uses indices of the original container, not the filtered one.
  template <typename IndexT>
  decltype(auto) operator[](IndexT index) const {
    decltype(auto) value = values_[static_cast<size_t>(index)];
    DCHECK(views_util::at(&mask_.get(), value))
        << "Inactive value. Are you using relative indices?";
    return value;
  }

  // Returns the underlying base container as a span.
  absl::Span<value_type> base() const { return values_; }

 private:
  // Span containing the underlying values.
  absl::Span<value_type> values_;

  // Reference wrapper for the underlying mask.
  std::reference_wrapper<const FilterMaskT> mask_;
};

// View exposing only the elements of a container whose parallel entry in a
// boolean filter mask evaluates to true. Looping over this view is equivalent
// to:
//
// auto val_it = values.begin();
// auto mask_it = mask.begin();
// for (; mask_it != mask.end(); ++mask_it, ++val_it) {
//   if (*mask_it) {
//     your_code(*val_it);
//   }
// }
//
template <typename ValueT, typename FilterMaskT>
class MaskedValuesView {
 public:
  // The type of values in the view.
  using value_type = const ValueT;

  // The value iterator type.
  using value_iterator = typename absl::Span<value_type>::iterator;

  // The mask iterator type.
  using enable_iterator = views_util::range_const_iterator_type<FilterMaskT>;

  // Iterator for masked values views.
  struct MaskedValuesViewIterator
      : views_util::IteratorCRTP<MaskedValuesViewIterator, value_type> {
    // Constructor.
    MaskedValuesViewIterator(value_iterator iterator,
                             enable_iterator mask_begin,
                             enable_iterator mask_end)
        : value_iter_(iterator), mask_iter_(mask_begin), mask_end_(mask_end) {
      AdjustToValidValue();
    }

    // Inequality operator.
    bool operator!=(const MaskedValuesViewIterator& other) const {
      DCHECK(mask_end_ == other.mask_end_);
      return value_iter_ != other.value_iter_;
    }

    // Pre-increment operator.
    MaskedValuesViewIterator& operator++() {
      DCHECK(mask_iter_ != mask_end_);
      ++mask_iter_;
      ++value_iter_;
      AdjustToValidValue();
      return *this;
    }

    // Dereference operator.
    decltype(auto) operator*() const {
      DCHECK(mask_iter_ != mask_end_);
      DCHECK(*mask_iter_);
      return *value_iter_;
    }

   private:
    // Moves the iterator forward to the next element where the mask
    // evaluates to true.
    void AdjustToValidValue() {
      while (mask_iter_ != mask_end_ && !*mask_iter_) {
        ++mask_iter_;
        ++value_iter_;
      }
    }

    // The current iterator position in the values container.
    value_iterator value_iter_;

    // The current iterator position in the mask structure.
    enable_iterator mask_iter_;

    // The end of the mask structure.
    enable_iterator mask_end_;
  };

  // Constructor taking values container and the filter mask.
  template <typename ValueRangeT>
  MaskedValuesView(const ValueRangeT* values, const FilterMaskT& mask)
      : values_(absl::MakeConstSpan(*values)), mask_(mask) {
    DCHECK(values != nullptr);
    DCHECK_EQ(values->size(), mask_.get().size());
  }

  // Returns the iterator to the beginning of the view.
  MaskedValuesViewIterator begin() const {
    return MaskedValuesViewIterator(values_.begin(), mask_.get().begin(),
                                    mask_.get().end());
  }

  // Returns the iterator to the end of the view.
  MaskedValuesViewIterator end() const {
    return MaskedValuesViewIterator(values_.end(), mask_.get().end(),
                                    mask_.get().end());
  }

  // Returns the underlying base container as a span.
  absl::Span<value_type> base() const { return values_; }

  // Accesses the element in the view using the index of the original container.
  // NOTE: uses indices of the original container, not the filtered one
  template <typename IndexT>
  decltype(auto) operator[](IndexT index) const {
    DCHECK(views_util::at(&mask_.get(), index))
        << "Inactive value. Are you using relative indices?";
    return values_[static_cast<size_t>(index)];
  }

 private:
  // Span containing the underlying values.
  absl::Span<value_type> values_;

  // Reference wrapper for the underlying mask.
  std::reference_wrapper<const FilterMaskT> mask_;
};

// This view provides a mechanism to access and filter elements in a 2D
// container. The filtering is applied in two stages:
// 1. The outer dimension is provided by an outer view (e.g., subset of
// columns).
// 2. The inner dimension (items of each sub-container) is dynamically wrapped
//    in an IndirectMaskedView using a shared inner filter mask.
template <typename OuterViewT, typename FilterMaskT>
class NestedMaskedView : public OuterViewT {
 public:
  // Iterator type for the outer container.
  using outer_iterator = views_util::range_const_iterator_type<OuterViewT>;

  // Value type for the outer container.
  using outer_value = views_util::range_value_type<OuterViewT>;

  // Value type for the inner container.
  using inner_value = views_util::range_value_type<outer_value>;

  // Indirect masked view type of the inner elements.
  using inner_view_type = IndirectMaskedView<inner_value, FilterMaskT>;

  // Iterator for the nested masked view.
  struct NestedMaskedViewIterator
      : views_util::IteratorCRTP<NestedMaskedViewIterator, inner_view_type> {
    // Constructor.
    NestedMaskedViewIterator(outer_iterator iterator,
                             const FilterMaskT& inner_mask)
        : outer_iter_(iterator), inner_mask_(inner_mask) {}

    // Inequality operator.
    bool operator!=(const NestedMaskedViewIterator& other) const {
      DCHECK_EQ(&inner_mask_.get(), &other.inner_mask_.get());
      return outer_iter_ != other.outer_iter_;
    }

    // Pre-increment operator.
    NestedMaskedViewIterator& operator++() {
      ++outer_iter_;
      return *this;
    }

    // Dereference operator.
    inner_view_type operator*() const {
      return inner_view_type(&(*outer_iter_), inner_mask_.get());
    }

    // Accesses the underlying outer view.
    const OuterViewT& base() const { return *this; }

   private:
    // Iterator position in the outer view.
    outer_iterator outer_iter_;

    // Reference wrapper for the underlying inner mask.
    std::reference_wrapper<const FilterMaskT> inner_mask_;
  };

  // Constructor taking the outer view and a reference to the inner mask.
  NestedMaskedView(OuterViewT outer_view, const FilterMaskT& inner_mask)
      : OuterViewT(outer_view), inner_mask_(inner_mask) {}

  // Returns the iterator to the beginning of the view.
  NestedMaskedViewIterator begin() const {
    return NestedMaskedViewIterator(OuterViewT::begin(), inner_mask_.get());
  }

  // Returns the iterator to the end of the view.
  NestedMaskedViewIterator end() const {
    return NestedMaskedViewIterator(OuterViewT::end(), inner_mask_.get());
  }

  // Subscript operator returning the inner view mapping at specified index i.
  template <typename IndexT>
  inner_view_type operator[](IndexT i) const {
    const auto& inner_container = OuterViewT::operator[](i);
    return inner_view_type(&inner_container, inner_mask_.get());
  }

 private:
  // Reference wrapper for the underlying inner mask.
  std::reference_wrapper<const FilterMaskT> inner_mask_;
};

// Functor applying identity transformation.
struct IdentityTransform {
  // Returns the forwarded value without transformation.
  template <typename T>
  T&& operator()(T&& value) const {
    return std::forward<T>(value);
  }
};

// Functor casting a value of one type to another.
template <typename FromT, typename ToT>
struct StaticCastTransform {
  // Returns value cast to type ToT.
  ToT operator()(FromT value) const { return static_cast<ToT>(value); }
};

// View applying a stateless transformation to the values of a contiguous
// container. Looping over this view is equivalent to:
//
// for (IndexT i(0); i < IndexT(values.size()); ++i) {
//   your_code(ValueTransformT()(values[static_cast<size_t>(i)]));
// }
//
template <typename ValueT, typename IndexT,
          typename ValueTransformT = IdentityTransform>
class TransformView {
 public:
  // The type of values in the view.
  using value_type = const ValueT;

  // The value iterator type.
  using value_iterator = typename absl::Span<value_type>::iterator;

  // Iterator for transformed views.
  struct TransformViewIterator
      : views_util::IteratorCRTP<TransformViewIterator, value_type> {
    // Constructor.
    explicit TransformViewIterator(value_iterator iterator)
        : current_iter_(iterator) {}

    // Inequality operator.
    bool operator!=(const TransformViewIterator& other) const {
      return current_iter_ != other.current_iter_;
    }

    // Pre-increment operator.
    TransformViewIterator& operator++() {
      ++current_iter_;
      return *this;
    }

    // Dereference operator applying transformation.
    decltype(auto) operator*() const {
      return ValueTransformT()(*current_iter_);
    }

   private:
    // Current position of iterator.
    value_iterator current_iter_;
  };

  // Default constructor.
  TransformView() = default;

  // Constructor taking a container of values.
  template <typename ValueRangeT>
  explicit TransformView(const ValueRangeT* values)
      : values_(absl::MakeConstSpan(*values)) {
    DCHECK(values != nullptr);
  }

  // Returns the size of the view.
  auto size() const { return values_.size(); }

  // Returns true if the view is empty.
  bool empty() const { return values_.empty(); }

  // Subscript operator returning transformed element at specified index.
  decltype(auto) operator[](IndexT index) const {
    const size_t idx = static_cast<size_t>(index);
    DCHECK_LT(idx, values_.size());
    return ValueTransformT()(values_[idx]);
  }

  // Returns the iterator to the beginning of the view.
  TransformViewIterator begin() const {
    return TransformViewIterator(values_.begin());
  }

  // Returns the iterator to the end of the view.
  TransformViewIterator end() const {
    return TransformViewIterator(values_.end());
  }

  // Returns the underlying base container as a span.
  absl::Span<value_type> base() const { return values_; }

 private:
  // Span containing the underlying values.
  absl::Span<value_type> values_;
};

}  // namespace operations_research

#endif  // ORTOOLS_SET_COVER_VIEWS_H_
