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

#ifndef OR_TOOLS_SET_COVER_BASE_TYPES_H_
#define OR_TOOLS_SET_COVER_BASE_TYPES_H_

#include <cstddef>
#include <cstdint>
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

// Maps from element to subset. Useful to compress the sparse row view.
using ElementToSubsetVector =
    util_intops::StrongVector<ElementIndex, SubsetIndex>;

template <typename EntryIndex, typename Index>
class CompressedStrongVectorIterator;

// A compressed vector of strong indices (e.g. SubsetIndex, ElementIndex), with
// EntryIndex indicating the position in the vector (e.g. RowEntryIndex or
// ColumnEntryIndex).
// The vector is compressed in a variable-length encoding, where each element
// is encoded as a delta from the previous element.
// The vector is stored in a byte stream, which is addressed byte-by-byte, which
// is necessary to store variable-length integers.
// The variable-length encoding is little-endian base128 (LEB128) as used by
// protobufs. Since we only use non-negative integers, there is no need to
// encode the sign bit (so non-zigzag encoding or similar techniques).
//
// TODO(user):
// There is a lot of room for optimization in this class.
// - Use a bit-twiddling approach to encode and decode integers.
// - Use another simpler variable encoding (base on vu128) using a single 64-bit
// word and limited to storing 8 bytes with 7 bits per byte. A 64-bit word would
// contain 8*7 = 56 bits. This is enough for an index in an array with
// 2^56 = 7.2e16 elements, and more that the address space of current (2025)
// machines.
// - Access memory by 64-bit words instead of bytes.
// - Make Codecs a template parameter of CompressedStrongVector (?)
template <typename EntryIndex, typename Index>
class CompressedStrongVector {
 public:
  using iterator = CompressedStrongVectorIterator<EntryIndex, Index>;
  using const_iterator = CompressedStrongVectorIterator<EntryIndex, Index>;
  using value_type = Index;

  explicit CompressedStrongVector() : memorized_value_(0), byte_stream_() {}

  // Initializes the compressed vector from a strong vector.
  // TODO(user): try to guess the size of the compressed vector and pre-allocate
  // it. Experience shows it consumes between 1 and 2 bytes per Index on
  // average.
  explicit CompressedStrongVector(
      const util_intops::StrongVector<EntryIndex, Index>& strong_vector)
      : memorized_value_(0), byte_stream_() {
    for (const Index x : strong_vector) {
      push_back(x);
    }
  }

  explicit CompressedStrongVector(absl::Span<const Index> vec)
      : memorized_value_(0), byte_stream_() {
    for (const Index x : vec) {
      push_back(x);
    }
  }

  // Appends an x to the vector in a compressed form.
  void push_back(Index x) { EncodeInteger(x.value()); }

  // Returns a reference to the underlying byte stream.
  const std::vector<uint8_t>& byte_stream() const { return byte_stream_; }

  // Returns the number of bytes needed to store the vector.
  size_t size_in_bytes() const { return byte_stream_.size(); }

  bool empty() const { return byte_stream_.empty(); }

  // Reserves space for n bytes.
  void Reserve(size_t n) { byte_stream_.reserve(n); }

  // For range-for loops.
  iterator begin() { return iterator(*this); }
  iterator end() { return iterator(*this, true); }

  // For const range-for loops.
  const_iterator begin() const { return const_iterator(*this); }
  const_iterator end() const { return const_iterator(*this, true); }

 private:
  void EncodeInteger(BaseInt x) {
    BaseInt delta = x - memorized_value_;  // Delta from previous value.
    DCHECK_GE(delta, 0);

    // Push the delta as a variable-length integer.
    while (delta >= 128) {
      // Keep the lower 7 bits of the delta and set the higher bit to 1 to mark
      // the continuation of the variable-length encoding.
      byte_stream_.push_back(static_cast<uint8_t>(delta | 0x80));
      // Shift the delta by 7 bits to get the next value.
      delta >>= 7;
    }
    // The final byte is less than 128, so its higher bit is zero, which marks
    // the end of the variable-length encoding.
    byte_stream_.push_back(static_cast<uint8_t>(delta));

    // Do not forget to remember the last value.
    memorized_value_ = x + kMinDelta;
  }
  // The last value memorized in the vector. It is defined as the last value
  // appended to the vector plus a kMinDelta. It starts at zero.
  BaseInt memorized_value_;

  // The minimum difference between two consecutive elements in the vector.
  static constexpr int64_t kMinDelta = 1;

  // The byte stream.
  std::vector<uint8_t> byte_stream_;
};

// Iterator for a compressed strong vector. There is no tool for decompressing
// a compressed strong vector, so this iterator is the only way to access the
// elements, always in order.
// The iterator is not thread-safe.
template <typename EntryIndex, typename Index>
class CompressedStrongVectorIterator {
 public:
  explicit CompressedStrongVectorIterator(
      const CompressedStrongVector<EntryIndex, Index>& compressed_vector)
      : compressed_vector_(compressed_vector),
        memorized_value_(0),
        pos_(0),
        last_pos_(0) {}

  explicit CompressedStrongVectorIterator(
      const CompressedStrongVector<EntryIndex, Index>& compressed_vector,
      bool at_end)
      : compressed_vector_(compressed_vector),
        memorized_value_(0),
        pos_(0),
        last_pos_(at_end ? compressed_vector.size_in_bytes() : 0) {}

  bool at_end() const {
    DCHECK_LE(last_pos_, compressed_vector_.size_in_bytes());
    return last_pos_ == compressed_vector_.size_in_bytes();
  }

  Index operator*() { return DecodeInteger(); }

  bool operator==(const CompressedStrongVectorIterator& other) const {
    DCHECK_EQ(&compressed_vector_, &(other.compressed_vector_));
    return last_pos_ == other.last_pos_;
  }

  bool operator!=(const CompressedStrongVectorIterator& other) const {
    return !(*this == other);
  }

  CompressedStrongVectorIterator& operator++() {
    last_pos_ = pos_;
    return *this;
  }

 private:
  Index DecodeInteger() {
    // This can be made much faster by using a bit-twiddling approach.
    const std::vector<uint8_t>& encoded = compressed_vector_.byte_stream();
    BaseInt delta = 0;
    int shift = 0;
    for (pos_ = last_pos_, shift = 0;
         encoded[pos_] >= 128 && pos_ < compressed_vector_.size_in_bytes();
         shift += 7, ++pos_) {
      delta |= static_cast<BaseInt>(encoded[pos_] & 0x7F) << shift;
    }
    delta |= static_cast<BaseInt>(encoded[pos_]) << shift;
    ++pos_;
    memorized_value_ += Index(delta + kMinDelta);
    return memorized_value_ - Index(kMinDelta);
  }

  // The compressed vector.
  const CompressedStrongVector<EntryIndex, Index>& compressed_vector_;

  // The last value memorized in the decoder. It is defined as the last value
  // appended to the vector plus a kMinDelta. It starts at zero.
  Index memorized_value_;

  // The current position in the byte stream.
  int64_t pos_;

  // The last position in the byte stream.
  int64_t last_pos_;

  static constexpr int64_t kMinDelta = 1;
};

using CompressedColumn = CompressedStrongVector<ColumnEntryIndex, ElementIndex>;
using CompressedRow = CompressedStrongVector<RowEntryIndex, SubsetIndex>;

using CompressedColumnView =
    util_intops::StrongVector<SubsetIndex, CompressedColumn>;
using CompressedRowView =
    util_intops::StrongVector<ElementIndex, CompressedRow>;

using CompressedColumnIterator =
    CompressedStrongVectorIterator<ColumnEntryIndex, ElementIndex>;
using CompressedRowIterator =
    CompressedStrongVectorIterator<RowEntryIndex, SubsetIndex>;

template <typename Index>
class IndexRangeIterator;

// A range of indices, that can be iterated over. Useful if used in a range-for
// loop or as an IterableContainer.
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
// StrongVector or a CompressedStrongVector.
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
  // Returns the elapsed time in seconds at a given moment. To be use to
  // implement time limits in the derived classes.
  double elapsed_time_in_seconds() const { return timer_.Get(); }

  absl::Duration GetElapsedDuration() const { return timer_.GetDuration(); }

 private:
  absl::Duration* duration_;
  WallTimer timer_;
};

}  // namespace operations_research

#endif  // OR_TOOLS_SET_COVER_BASE_TYPES_H_
