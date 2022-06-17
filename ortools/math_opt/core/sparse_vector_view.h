// Copyright 2010-2022 Google LLC
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

// A read only view for sparse vectors that implements various utilities.
//
// This header defines:
//   * SparseVectorView<T>: a sparse vector as a span of int64_t ids and another
//     span of values of type T. The underlying data is not owned and the user
//     must ensure the data outlives the view.
//   * Overloaded MakeView factories to avoid explicit template arguments.
//
// The utilities implemented by SparseVectorView<T> include:
//   * const range iterations over the (id, value) pairs.
//   * .as_map<IdT>() member function that returns the view as a
//     absl::flat_hash_map<IdT, T>

// Example:
//   const std::vector<int64_t> ids = {2, 5, 7};
//   const std::vector<double> values= {1.0, 3.0, 1.0};
//   const SparseVectorView<double> view = MakeView(ids, values);
//
// Now view.ids => [2, 5, 7] and view.values => [1.0, 3.0, 1.0]. To iterate over
// the (id, value) pairs:
//
//   for(const auto [id, value] : view) {
//     ...
//   }
//
//  To get a map that casts the ids to the strong int type VariableId:
//
//  auto map = view.as_map<VariableId>();
//
// For more information, see the class comments below.

#ifndef OR_TOOLS_MATH_OPT_CORE_SPARSE_VECTOR_VIEW_H_
#define OR_TOOLS_MATH_OPT_CORE_SPARSE_VECTOR_VIEW_H_

#include <stdint.h>

#include <cstdint>
#include <iterator>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/types/span.h"
#include "google/protobuf/message.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/map_util.h"
#include "ortools/math_opt/core/arrow_operator_proxy.h"  // IWYU pragma: export
#include "ortools/math_opt/core/sparse_vector.h"
#include "ortools/math_opt/sparse_containers.pb.h"

namespace operations_research {
namespace math_opt {

// Recovers the values-type of a SparseVector type like SparseDoubleVector or
// SparseBoolVector.
template <typename SparseVector>
using sparse_value_type = typename std::remove_reference<
    decltype(SparseVector().values())>::type::value_type;

// Abstracts sparse ids-values structures like SparseDoubleVector and mimics
// its .ids()/.values() API. It additionally provides const range iterations
// over the (id, value) pairs and conversion to a map.
//
// The returned iterators are proper STL forward iterators that can be used with
// STL containers. For example to build a vector of pairs of values, one can
// simply use the iterators:
//
//   const auto view = MakeView(arg);
//   const std::vector v(view.begin(), view.end());
//
// Constructor SparseVectorView<T>(ids, values) will not check if ids and values
// have the same length. However, iterator functions and .as_map() will CHECK
// fail if ids and values do not have the same length.
template <typename T>
class SparseVectorView {
 public:
  using value_type = std::pair<int64_t, T>;

  class const_iterator {
   public:
    using value_type = SparseVectorView::value_type;
    using reference = value_type;
    using pointer = void;
    using const_pointer = void;
    using difference_type = int;
    using iterator_category = std::forward_iterator_tag;

    reference operator*() const;
    inline internal::ArrowOperatorProxy<reference> operator->() const;
    const_iterator& operator++();
    bool operator==(const const_iterator& other) const;
    bool operator!=(const const_iterator& other) const;

   private:
    friend class SparseVectorView;

    inline explicit const_iterator(const SparseVectorView* view,
                                   bool at_end = false);

    const SparseVectorView* view_ = nullptr;
    int index_ = 0;
  };

  SparseVectorView(absl::Span<const int64_t> ids, absl::Span<const T> values)
      : ids_(std::move(ids)), values_(std::move(values)) {}
  SparseVectorView() {}

  inline const_iterator begin() const;
  inline const_iterator end() const;

  absl::Span<const int64_t> ids() const { return ids_; }
  int64_t ids(int index) const { return ids_[index]; }
  int ids_size() const { return ids_.size(); }
  absl::Span<const T> values() const { return values_; }
  int values_size() const { return values_.size(); }
  const T& values(int index) const { return values_[index]; }

  // It should be possible to construct an IndexType from an integer
  template <typename IndexType>
  absl::flat_hash_map<IndexType, T> as_map();

 private:
  absl::Span<const int64_t> ids_;
  absl::Span<const T> values_;
};

// Returns a view for values that are  vector-like collection like
// std::vector<T> or google::protobuf::RepeatedField<T>. See other overloads for
// other values-types.
template <typename Collection, typename T = typename Collection::value_type>
SparseVectorView<T> MakeView(absl::Span<const int64_t> ids,
                             const Collection& values) {
  return SparseVectorView<T>(ids, values);
}

// Returns a view for values that are google::protobuf::RepeatedPtrField<T>.
// Common use for this overload is when T = std::string. See other overloads for
// other values-types.
template <typename T>
SparseVectorView<const T*> MakeView(
    const google::protobuf::RepeatedField<int64_t>& ids,
    const google::protobuf::RepeatedPtrField<T>& values) {
  return SparseVectorView<const T*>(ids, values);
}

// Returns a view for values in a SparseDoubleVectorProto, SparseBoolVectorProto
// or similar structure. For such cases, it is preferred over the two-argument
// overloads. See other overloads for other values-types.
template <typename SparseVectorProto,
          typename T = sparse_value_type<SparseVectorProto>>
SparseVectorView<T> MakeView(const SparseVectorProto& sparse_vector) {
  return SparseVectorView<T>(sparse_vector.ids(), sparse_vector.values());
}

// Returns a view for values in a SparseVector. For this case it is preferred
// over the two-argument overloads. See other overloads for other values-types.
template <typename T>
SparseVectorView<T> MakeView(const SparseVector<T>& sparse_vector) {
  return SparseVectorView<T>(sparse_vector.ids, sparse_vector.values);
}

////////////////////////////////////////////////////////////////////////////////
// Inline implementations
////////////////////////////////////////////////////////////////////////////////

template <typename T>
SparseVectorView<T>::const_iterator::const_iterator(
    const SparseVectorView<T>* view, bool at_end)
    : view_(view) {
  if (at_end) {
    index_ = view_->ids_size();
  }
}

template <typename T>
typename SparseVectorView<T>::const_iterator::reference
SparseVectorView<T>::const_iterator::operator*() const {
  return {view_->ids(index_), view_->values(index_)};
}

template <typename T>
internal::ArrowOperatorProxy<
    typename SparseVectorView<T>::const_iterator::reference>
SparseVectorView<T>::const_iterator::operator->() const {
  return internal::ArrowOperatorProxy<reference>(**this);
}

template <typename T>
typename SparseVectorView<T>::const_iterator&
SparseVectorView<T>::const_iterator::operator++() {
  DCHECK_LT(index_, view_->ids_size());
  ++index_;
  return *this;
}

template <typename T>
bool SparseVectorView<T>::const_iterator::operator==(
    const const_iterator& other) const {
  DCHECK_EQ(view_, other.view_);
  return index_ == other.index_;
}

template <typename T>
bool SparseVectorView<T>::const_iterator::operator!=(
    const const_iterator& other) const {
  return !(*this == other);
}

template <typename T>
typename SparseVectorView<T>::const_iterator SparseVectorView<T>::begin()
    const {
  DCHECK_EQ(ids_size(), values_size());
  return const_iterator(this, /*at_end=*/false);
}

template <typename T>
typename SparseVectorView<T>::const_iterator SparseVectorView<T>::end() const {
  DCHECK_EQ(ids_size(), values_size());
  return const_iterator(this, /*at_end=*/true);
}

template <typename T>
template <typename IndexType>
absl::flat_hash_map<IndexType, T> SparseVectorView<T>::as_map() {
  absl::flat_hash_map<IndexType, T> result;
  CHECK_EQ(ids_size(), values_size());
  result.reserve(ids_size());
  for (const auto& [id, value] : *this) {
    gtl::InsertOrDie(&result, IndexType(id), value);
  }
  return result;
}

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_CORE_SPARSE_VECTOR_VIEW_H_
