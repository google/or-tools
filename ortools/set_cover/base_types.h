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

#ifndef ORTOOLS_SET_COVER_BASE_TYPES_H_
#define ORTOOLS_SET_COVER_BASE_TYPES_H_

#include <sys/types.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <tuple>
#include <type_traits>
#include <vector>

#include "absl/log/check.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ortools/base/strong_int.h"
#include "ortools/base/strong_vector.h"
#include "ortools/base/timer.h"

namespace operations_research {

// Basic non-strict type for cost. The speed penalty for using double is ~2%.
using Cost = double;

// Base non-strict integer type for counting elements and subsets.
// Using ints makes it possible to represent problems with more than 2 billion
// (2e9) elements and subsets. If need arises one day, BaseInt can be split
// into SubsetBaseInt and ElementBaseInt.
// Quick testing has shown a slowdown of about 20-25% when using int64_t.
using BaseInt = int32_t;

// We make heavy use of strong typing to avoid obvious mistakes.
// Subset index.
DEFINE_STRONG_INT_TYPE(SubsetIndex, BaseInt);

// Element index.
DEFINE_STRONG_INT_TYPE(ElementIndex, BaseInt);

// Position in a vector. The vector may either represent a column, i.e. a
// subset with all its elements, or a row, i,e. the list of subsets which
// contain a given element.
DEFINE_STRONG_INT_TYPE(ColumnEntryIndex, BaseInt);
DEFINE_STRONG_INT_TYPE(RowEntryIndex, BaseInt);

using SubsetRange = util_intops::StrongIntRange<SubsetIndex>;
using ElementRange = util_intops::StrongIntRange<ElementIndex>;
using ColumnEntryRange = util_intops::StrongIntRange<ColumnEntryIndex>;
using RowEntryRange = util_intops::StrongIntRange<RowEntryIndex>;

using SubsetCostVector = util_intops::StrongVector<SubsetIndex, Cost>;
using ElementCostVector = util_intops::StrongVector<ElementIndex, Cost>;

using SparseColumn = util_intops::StrongVector<ColumnEntryIndex, ElementIndex>;
using SparseRow = util_intops::StrongVector<RowEntryIndex, SubsetIndex>;

using ElementToIntVector = util_intops::StrongVector<ElementIndex, BaseInt>;
using SubsetToIntVector = util_intops::StrongVector<SubsetIndex, BaseInt>;

// Views of the sparse vectors.
using SparseColumnView = util_intops::StrongVector<SubsetIndex, SparseColumn>;
using SparseRowView = util_intops::StrongVector<ElementIndex, SparseRow>;

using SubsetBoolVector = util_intops::StrongVector<SubsetIndex, bool>;
using ElementBoolVector = util_intops::StrongVector<ElementIndex, bool>;

using SubsetWeightVector = util_intops::StrongVector<SubsetIndex, double>;
using ElementWeightVector = util_intops::StrongVector<ElementIndex, double>;

// Maps from element to subset. Useful to compress the sparse row view.
using ElementToSubsetVector =
    util_intops::StrongVector<ElementIndex, SubsetIndex>;

using SparseColumnIterator = SparseColumn::iterator;
using SparseRowIterator = SparseRow::iterator;
using SparseColumnConstIterator = SparseColumn::const_iterator;
using SparseRowConstIterator = SparseRow::const_iterator;

template <typename Index>
class IndexRangeIterator;

// A range of indices, that can be iterated over. Useful if used in a
// range-for loop or as an IterableContainer.
template <typename Index>
class IndexRange {
 public:
  using iterator = IndexRangeIterator<Index>;
  using const_iterator = IndexRangeIterator<Index>;
  using value_type = Index;

  IndexRange(Index start, Index end) : start_(start), end_(end) {}

  explicit IndexRange(Index end) : start_(Index(0)), end_(end) {}

  Index get_start() const { return start_; }
  Index get_end() const { return end_; }

  // For range-for loops.
  iterator begin() { return iterator(*this); }
  iterator end() { return iterator(*this, true); }

  // For const range-for loops.
  const_iterator begin() const { return const_iterator(*this); }
  const_iterator end() const { return const_iterator(*this, true); }

 private:
  Index start_;
  Index end_;
};

// Additional deduction guide
template <class Index>
IndexRange(Index a, Index b) -> IndexRange<Index>;

// The iterator for an IndexRange.
template <typename Index>
class IndexRangeIterator {
 public:
  explicit IndexRangeIterator(const IndexRange<Index>& range)
      : range_(range), current_(range.get_start()) {}

  IndexRangeIterator(const IndexRange<Index>& range, bool at_end)
      : range_(range), current_(at_end ? range.get_end() : range.get_start()) {}

  bool at_end() const { return current_ == range_.get_end(); }

  Index operator*() { return current_; }

  bool operator==(const IndexRangeIterator& other) const {
    DCHECK_EQ(range_.get_start(), other.range_.get_start());
    DCHECK_EQ(range_.get_end(), other.range_.get_end());
    return current_ == other.current_;
  }

  bool operator!=(const IndexRangeIterator& other) const {
    return !(*this == other);
  }

  IndexRangeIterator& operator++() {
    ++current_;
    return *this;
  }

 private:
  IndexRange<Index> range_;
  Index current_;
};

// A container that can be iterated over, but does not own the data.
// The container can be either const or non-const.
// The container can be either a std::vector, a absl::Span, an IndexRange, a
// StrongVector or a CompressedStrongList.
// We use the Curiously-Recurring Template Pattern (CRTP) to avoid having to
// specify the type of the container in the derived class, and to not lose
// runtime performance because of virtual functions.
template <typename T, typename Derived>
class IterableContainerBase {
 public:
  using value_type = typename std::decay_t<T>::value_type;
  using iterator_type = decltype(std::begin(std::declval<T>()));

  auto begin() { return static_cast<Derived*>(this)->begin_impl(); }
  auto end() { return static_cast<Derived*>(this)->end_impl(); }
  auto begin() const { return static_cast<const Derived*>(this)->begin_impl(); }
  auto end() const { return static_cast<const Derived*>(this)->end_impl(); }
};

template <typename T>
class IterableContainer
    : public IterableContainerBase<T, IterableContainer<T>> {
 public:
  explicit IterableContainer(const T& data_source) : data_(data_source) {}

  auto begin_impl() { return data_.begin(); }
  auto end_impl() { return data_.end(); }
  auto begin_impl() const { return data_.begin(); }
  auto end_impl() const { return data_.end(); }

 private:
  T data_;
};

// Additional deduction guide.
template <typename T>
IterableContainer(const T& data_source) -> IterableContainer<T>;

// Simple stopwatch class that enables recording the time spent in various
// functions in the code.
// It uses RAII to automatically record the time spent in the constructor and
// destructor, independently of the path taken by the code.
class StopWatch {
 public:
  explicit StopWatch(absl::Duration* duration) : duration_(duration), timer_() {
    timer_.Start();
  }
  ~StopWatch() {
    timer_.Stop();
    *duration_ = timer_.GetDuration();
  }
  // Returns the elapsed time in seconds at a given moment. To be used to
  // implement time limits in the derived classes.
  double elapsed_time_in_seconds() const { return timer_.Get(); }

  // Returns the elapsed time as an absl::Duration.
  absl::Duration GetElapsedDuration() const { return timer_.GetDuration(); }

 private:
  absl::Duration* duration_;
  WallTimer timer_;
};

}  // namespace operations_research

#endif  // ORTOOLS_SET_COVER_BASE_TYPES_H_
