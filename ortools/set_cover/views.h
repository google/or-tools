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
  struct MaskedIndicesViewIterator
      : util::IteratorCRTP<MaskedIndicesViewIterator, IntegerT> {
    MaskedIndicesViewIterator(IntegerT index, const FilterMaskT& mask)
        : index_(index), mask_(mask) {
      AdjustToValidValue();
    }

    bool operator!=(MaskedIndicesViewIterator other) const {
      return index_ != other.index_;
    }

    MaskedIndicesViewIterator& operator++() {
      ++index_;
      AdjustToValidValue();
      return *this;
    }

    IntegerT operator*() const { return index_; }

   private:
    void AdjustToValidValue() {
      while (index_ < IntegerT(mask_.get().size()) &&
             !util::at(&mask_.get(), index_)) {
        ++index_;
      }
    }

    IntegerT index_;
    std::reference_wrapper<const FilterMaskT> mask_;
  };

  explicit MaskedIndicesView(const FilterMaskT& mask) : mask_(mask) {}

  MaskedIndicesViewIterator begin() const {
    return MaskedIndicesViewIterator(IntegerT(0), mask_.get());
  }

  MaskedIndicesViewIterator end() const {
    return MaskedIndicesViewIterator(IntegerT(mask_.get().size()), mask_.get());
  }

 private:
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
  using value_type = const ValueT;
  using index_type = const IndexT;
  using value_iterator = typename absl::Span<value_type>::iterator;
  using index_iterator = typename absl::Span<index_type>::iterator;

  struct IndexListViewIterator
      : util::IteratorCRTP<IndexListViewIterator, value_type> {
    IndexListViewIterator(absl::Span<value_type> values,
                          index_iterator index_iterator)
        : values_(values), index_iterator_(index_iterator) {}

    bool operator!=(const IndexListViewIterator& other) const {
      DCHECK_EQ(values_.data(), other.values_.data());
      return index_iterator_ != other.index_iterator_;
    }

    IndexListViewIterator& operator++() {
      ++index_iterator_;
      return *this;
    }

    decltype(auto) operator*() const {
      return values_[static_cast<size_t>(*index_iterator_)];
    }

   private:
    absl::Span<value_type> values_;
    index_iterator index_iterator_;
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
  using value_type = const ValueT;
  using value_iterator = typename absl::Span<value_type>::iterator;

  struct IndirectMaskedViewIterator
      : util::IteratorCRTP<IndirectMaskedViewIterator, value_type> {
    IndirectMaskedViewIterator(value_iterator iterator_begin,
                               value_iterator iterator_end,
                               const FilterMaskT& mask)
        : current_iter_(iterator_begin), end_iter_(iterator_end), mask_(mask) {
      AdjustToValidValue();
    }

    bool operator!=(const IndirectMaskedViewIterator& other) const {
      DCHECK_EQ(&mask_.get(), &other.mask_.get());
      return current_iter_ != other.current_iter_;
    }

    IndirectMaskedViewIterator& operator++() {
      ++current_iter_;
      AdjustToValidValue();
      return *this;
    }

    decltype(auto) operator*() const { return *current_iter_; }

   private:
    void AdjustToValidValue() {
      while (current_iter_ != end_iter_ &&
             !util::at(&mask_.get(), *current_iter_)) {
        ++current_iter_;
      }
    }

    value_iterator current_iter_;
    value_iterator end_iter_;
    std::reference_wrapper<const FilterMaskT> mask_;
  };

  template <typename ValueRangeT>
  IndirectMaskedView(const ValueRangeT* values, const FilterMaskT& mask)
      : values_(absl::MakeConstSpan(*values)), mask_(mask) {
    DCHECK(values != nullptr);
  }

  IndirectMaskedViewIterator begin() const {
    return IndirectMaskedViewIterator(values_.begin(), values_.end(),
                                      mask_.get());
  }

  IndirectMaskedViewIterator end() const {
    return IndirectMaskedViewIterator(values_.end(), values_.end(),
                                      mask_.get());
  }

  // NOTE: uses indices of the original container, not the filtered one
  template <typename IndexT>
  decltype(auto) operator[](IndexT index) const {
    decltype(auto) value = values_[static_cast<size_t>(index)];
    DCHECK(util::at(&mask_.get(), value))
        << "Inactive value. Are you using relative indices?";
    return value;
  }

  absl::Span<value_type> base() const { return values_; }

 private:
  absl::Span<value_type> values_;
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
  using value_type = const ValueT;
  using value_iterator = typename absl::Span<value_type>::iterator;
  using enable_iterator = util::range_const_iterator_type<FilterMaskT>;

  struct MaskedValuesViewIterator
      : util::IteratorCRTP<MaskedValuesViewIterator, value_type> {
    MaskedValuesViewIterator(value_iterator iterator,
                             enable_iterator mask_begin,
                             enable_iterator mask_end)
        : value_iter_(iterator), mask_iter_(mask_begin), mask_end_(mask_end) {
      AdjustToValidValue();
    }

    bool operator!=(const MaskedValuesViewIterator& other) const {
      DCHECK(mask_end_ == other.mask_end_);
      return value_iter_ != other.value_iter_;
    }

    MaskedValuesViewIterator& operator++() {
      ++mask_iter_;
      ++value_iter_;
      AdjustToValidValue();
      return *this;
    }

    decltype(auto) operator*() const { return *value_iter_; }

   private:
    void AdjustToValidValue() {
      while (mask_iter_ != mask_end_ && !*mask_iter_) {
        ++mask_iter_;
        ++value_iter_;
      }
    }

    value_iterator value_iter_;
    enable_iterator mask_iter_;
    enable_iterator mask_end_;
  };

  template <typename ValueRangeT>
  MaskedValuesView(const ValueRangeT* values, const FilterMaskT& mask)
      : values_(absl::MakeConstSpan(*values)), mask_(mask) {
    DCHECK(values != nullptr);
    DCHECK_EQ(values->size(), mask_.get().size());
  }

  MaskedValuesViewIterator begin() const {
    return MaskedValuesViewIterator(values_.begin(), mask_.get().begin(),
                                    mask_.get().end());
  }

  MaskedValuesViewIterator end() const {
    return MaskedValuesViewIterator(values_.end(), mask_.get().end(),
                                    mask_.get().end());
  }

  absl::Span<value_type> base() const { return values_; }

  // NOTE: uses indices of the original container, not the filtered one
  template <typename IndexT>
  decltype(auto) operator[](IndexT index) const {
    DCHECK(util::at(&mask_.get(), index))
        << "Inactive value. Are you using relative indices?";
    return values_[static_cast<size_t>(index)];
  }

 private:
  absl::Span<value_type> values_;
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
  using outer_iterator = util::range_const_iterator_type<OuterViewT>;
  using outer_value = util::range_value_type<OuterViewT>;
  using inner_value = util::range_value_type<outer_value>;
  using inner_view_type = IndirectMaskedView<inner_value, FilterMaskT>;

  struct NestedMaskedViewIterator
      : util::IteratorCRTP<NestedMaskedViewIterator, inner_view_type> {
    NestedMaskedViewIterator(outer_iterator iterator,
                             const FilterMaskT& inner_mask)
        : outer_iter_(iterator), inner_mask_(inner_mask) {}

    bool operator!=(const NestedMaskedViewIterator& other) const {
      DCHECK_EQ(&inner_mask_.get(), &other.inner_mask_.get());
      return outer_iter_ != other.outer_iter_;
    }

    NestedMaskedViewIterator& operator++() {
      ++outer_iter_;
      return *this;
    }

    inner_view_type operator*() const {
      return inner_view_type(&(*outer_iter_), inner_mask_.get());
    }

    const OuterViewT& base() const { return *this; }

   private:
    outer_iterator outer_iter_;
    std::reference_wrapper<const FilterMaskT> inner_mask_;
  };

  NestedMaskedView(OuterViewT outer_view, const FilterMaskT& inner_mask)
      : OuterViewT(outer_view), inner_mask_(inner_mask) {}

  NestedMaskedViewIterator begin() const {
    return NestedMaskedViewIterator(OuterViewT::begin(), inner_mask_.get());
  }

  NestedMaskedViewIterator end() const {
    return NestedMaskedViewIterator(OuterViewT::end(), inner_mask_.get());
  }

  template <typename IndexT>
  inner_view_type operator[](IndexT i) const {
    auto& inner_container = OuterViewT::operator[](i);
    return inner_view_type(&inner_container, inner_mask_.get());
  }

 private:
  std::reference_wrapper<const FilterMaskT> inner_mask_;
};

struct IdentityTransform {
  template <typename T>
  T&& operator()(T&& value) const {
    return std::forward<T>(value);
  }
};

template <typename FromT, typename ToT>
struct StaticCastTransform {
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
  using value_type = const ValueT;
  using value_iterator = typename absl::Span<value_type>::iterator;

  struct TransformViewIterator
      : util::IteratorCRTP<TransformViewIterator, value_type> {
    explicit TransformViewIterator(value_iterator iterator)
        : current_iter_(iterator) {}

    bool operator!=(const TransformViewIterator& other) const {
      return current_iter_ != other.current_iter_;
    }

    TransformViewIterator& operator++() {
      ++current_iter_;
      return *this;
    }

    decltype(auto) operator*() const {
      return ValueTransformT()(*current_iter_);
    }

   private:
    value_iterator current_iter_;
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
