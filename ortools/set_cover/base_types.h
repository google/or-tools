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
#include "ortools/set_cover/fast_varint.h"

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

// A compressed list of strong indices (e.g. SubsetIndex, ElementIndex), with
// EntryIndex indicating the position in the vector (e.g. RowEntryIndex or
// ColumnEntryIndex). Since this is a list, the EntryIndex type is not used.
// The list is compressed using a variable-length encoding, where each element
// is encoded as a delta from the previous element.
// The data are stored in a byte stream, which is addressed byte-by-byte, which
// is necessary to store variable-length integers.
// But before this, we need to forward-declare CompressedStrongListIterator.
template <typename EntryIndex, typename Index>
class CompressedStrongListIterator;

template <typename EntryIndex, typename Index, bool dry_run = false>
class CompressedStrongList {
 public:
  using iterator = CompressedStrongListIterator<EntryIndex, Index>;
  using const_iterator = CompressedStrongListIterator<EntryIndex, Index>;
  using value_type = Index;

  static constexpr uint64_t kMinDelta = 1;

  explicit CompressedStrongList()
      : memorized_value_(0),
        pos_(0),
        num_items_(0),
        size_in_bytes_(0),
        byte_stream_ptr_(nullptr),
        byte_stream_() {}

  // Initializes the compressed list from a strong vector.
  // The strong vector is expected to be sorted in ascending order. This
  // guarantees that the deltas will be strictly increasing, which is necessary
  // for the compressed encoding of EncodeSmallInteger. The vector is traversed
  // once to compute the size of the compressed list, and allocate the memory
  // only once to avoid thrashing. Note that we allocate an extra 7 bytes to
  // read or write a 1-byte value as an 8-byte uint64_t without overflowing the
  // buffer.
  explicit CompressedStrongList(
      const util_intops::StrongVector<EntryIndex, Index>& strong_vector)
      : CompressedStrongList() {
    Load(strong_vector);
  }

  // Same as above, but the vector is provided as a span.
  explicit CompressedStrongList(absl::Span<const Index> span)
      : CompressedStrongList() {
    Load(span);
  }

  void Load(const util_intops::StrongVector<EntryIndex, Index>& strong_vector) {
    Load(strong_vector.get());
  }

  void Load(absl::Span<const Index> span) {
    const uint64_t size_to_reserve = ComputeCompressedSize(span);
    byte_stream_.clear();
    byte_stream_.resize(size_to_reserve + sizeof(memorized_value_));
    for (const Index x : span) {
      UnsafeAppendCompressedInteger(x.value());
    }
  }

  uint64_t ComputeCompressedSize(
      const util_intops::StrongVector<EntryIndex, Index>& strong_vector) const {
    return ComputeCompressedSize(strong_vector.get());
  }

  uint64_t ComputeCompressedSize(absl::Span<const Index> span) const {
    // We use a dry run on another CompressedStrongList to compute the size.
    // This is a bit inefficient, but it avoids having to reallocate memory
    // all the time when doing the load.
    // TODO(user): it would be even more efficient to use a pre-allocated
    // dry-run list that would be reused for all loads.
    CompressedStrongList<EntryIndex, Index, true> dry_run_list;
    for (const Index x : span) {
      dry_run_list.push_back(x);
    }
    return dry_run_list.size_in_bytes();
  }

  // Appends an x to the list in a compressed form.
  void push_back(Index x) { SafeAppendCompressedInteger(x.value()); }

  // Returns a reference to the underlying byte stream.
  const std::vector<uint8_t>& byte_stream() const { return byte_stream_; }

  // Returns the number of bytes needed to store the list.
  size_t size_in_bytes() const { return pos_; }

  // Returns the number of items in the list.
  uint64_t num_items() const { return num_items_; }

  bool empty() const { return byte_stream_.empty(); }

  // Returns true if the lists are equal.
  bool IsEqual(const CompressedStrongList& other) const {
    return byte_stream_ == other.byte_stream_;
  }

  // Returns true if the compressed list contains the same elements as the other
  // list.
  bool IsEqual(absl::Span<const Index> other) const {
    if (num_items() != other.size()) {
      return false;
    }
    uint64_t i = 0;
    for (const Index x : *this) {
      if (x != other[i]) {
        return false;
      }
      ++i;
    }
    return true;
  }

  // Same as above, but for a strong vector.
  bool IsEqual(
      const util_intops::StrongVector<EntryIndex, Index>& other) const {
    return IsEqual(other.get());
  }

  // Reserves space for n bytes.
  void Reserve(size_t n) {
    byte_stream_.reserve(n + 7);  // Add space for the last value.
  }

  util_intops::StrongVector<EntryIndex, Index> ToStrongVector() const {
    util_intops::StrongVector<EntryIndex, Index> result;
    result.reserve(num_items());
    for (const Index x : *this) {
      result.push_back(x);
    }
    return result;
  }

  // For range-for loops.
  iterator begin() { return iterator(*this); }
  iterator end() { return iterator(*this, true); }

  // For const range-for loops.
  const_iterator begin() const { return const_iterator(*this); }
  const_iterator end() const { return const_iterator(*this, true); }

 private:
  // Writes size bytes to the byte stream at the current position pos_. pos_ is
  // incremented by size.
  // This is done by a single 64-bit memory write.
  // It is the responsibility (contract) of the caller to ensure that the there
  // is enough space in the byte stream to write an uint64_t, even though a
  // DCHECK is there to help debug potential issues in the caller code.
  // There is no need for a safe version of this function.
  void UnsafeWriteRawUint64WithSize(uint64_t x, uint32_t size) {
    // Use memcpy to make unaligned writes. On x86 there has not been any
    // penalty for a while, apart maybe when crossing cache line boundaries.
    if constexpr (!dry_run) {
      DCHECK_LE(pos_ + sizeof(x), byte_stream_.size());
      std::memcpy(byte_stream_.data() + pos_, &x, sizeof(x));
    }
    pos_ += size;
    ++num_items_;
  }

  // Writes VonNeumannVarint::kLargeEncodingPrefix followed by a uint64_t to the
  // byte stream.
  void UnsafeWritePrefixAndRawUint64(uint64_t x) {
    DCHECK_LE(pos_ + sizeof(x) + 1, byte_stream_.size());
    byte_stream_[pos_] = VonNeumannVarint::kLargeEncodingPrefix;
    ++pos_;
    UnsafeWriteRawUint64WithSize(x, sizeof(x));
  }

  std::tuple<uint64_t, uint32_t> EncodeSmallInteger(uint64_t x) const {
    DCHECK_GE(x, memorized_value_);
    const uint64_t delta = x - memorized_value_;  // Delta from previous value.
    DCHECK(!VonNeumannVarint::NeedsLargeEncoding(delta))
        << "Delta is too large: " << delta;
    const uint64_t encoded_value = VonNeumannVarint::EncodeSmallVarint(delta);
    const uint32_t size = VonNeumannVarint::EncodingLength(encoded_value);
    return {encoded_value, size};
  }

  void UnsafeAppendCompressedInteger(uint64_t x) {
    const auto [encoded_value, size] = EncodeSmallInteger(x);
    UnsafeWriteRawUint64WithSize(encoded_value, size);
    memorized_value_ = x + kMinDelta;
  }

  void SafeAppendCompressedInteger(uint64_t x) {
    if constexpr (!dry_run) {
      byte_stream_.resize(pos_ + sizeof(x));
    }
    UnsafeAppendCompressedInteger(x);
  }

  // Encodes any integer and writes it to the byte stream in a compressed form.
  // If the delta is larger than kLargeEncodingValue, it is encoded using the
  // WritePrefixAndRawUint64 function. Otherwise, it is encoded using the
  // EncodeSmallInteger function.
  // Note that x MUST be larger than the last value appended to the list.
  void UnsafeAppendAnyCompressedInteger(uint64_t x) {
    // Make sure that there is enough space in the byte stream to write the
    // large encoding.
    DCHECK_LT(pos_ + sizeof(x) + 1, byte_stream_.size());
    // This should almost be a CHECK_GT because it is very important that the
    // encoded value is strictly larger than the previous value.
    // TODO(user): Check the performance hit of using a ZigZag encoding or
    // similar, to enable negative deltas.
    DCHECK_GT(x, memorized_value_);
    const uint64_t delta = x - memorized_value_;  // Delta from previous value.
    if (VonNeumannVarint::NeedsLargeEncoding(delta)) {
      UnsafeWritePrefixAndRawUint64(delta);
    } else {
      const auto [encoded_value, size] = EncodeSmallInteger(x);
      UnsafeWriteRawUint64WithSize(encoded_value, size);
    }
    // Do not forget to remember the last value.
    memorized_value_ = x + kMinDelta;
  }

  // Safe version of UnsafeAppendAnyCompressedInteger, that ensures that there
  // is enough space in the byte stream to write the encoded value.
  void SafeAppendAnyCompressedInteger(uint64_t x) {
    if constexpr (!dry_run) {
      byte_stream_.resize(pos_ + sizeof(x) + 1);
    }
    UnsafeAppendAnyCompressedInteger(x);
  }

  // The last value memorized in the list. It is defined as the last value
  // appended to the list plus a kMinDelta. It starts at zero.
  uint64_t memorized_value_;

  // The current position in the byte stream.
  uint64_t pos_;

  // The number of items in the list. Not named size_ to avoid confusion
  // with size_in_bytes_.
  uint64_t num_items_;

  // The size of the byte stream.
  uint64_t size_in_bytes_;

  // The pointer to the current position in the byte stream.
  // This property must always hold:
  //     byte_stream_ptr_== byte_stream_.data() + pos_.
  // When dereferencing byte_stream_ptr_:
  //     byte_stream_ptr_ < byte_stream_.data() + size_in_bytes.
  uint8_t* byte_stream_ptr_;

  // The byte stream.
  std::vector<uint8_t> byte_stream_;
};

template <typename EntryIndex, typename Index>
class CompressedStrongListIterator {
 public:
  // Make sure that the minimum delta of the iterator (decoder) is the same as
  // the minimum delta of the compressed list (encoder).
  static constexpr uint64_t kMinDelta =
      CompressedStrongList<EntryIndex, Index>::kMinDelta;

  explicit CompressedStrongListIterator(
      const CompressedStrongList<EntryIndex, Index, false>& compressed_vector)
      : compressed_vector_(compressed_vector),
        memorized_value_(0),
        pos_(0),
        last_pos_(0) {}
  explicit CompressedStrongListIterator(
      const CompressedStrongList<EntryIndex, Index, false>& compressed_vector,
      bool at_end)
      : compressed_vector_(compressed_vector),
        memorized_value_(0),
        pos_(0),
        last_pos_(at_end ? compressed_vector.size_in_bytes() : 0) {}

  Index operator*() { return DecodeInteger(); }

  bool operator==(const CompressedStrongListIterator& other) const {
    DCHECK_EQ(&compressed_vector_, &(other.compressed_vector_));
    return last_pos_ == other.last_pos_;
  }

  bool operator!=(const CompressedStrongListIterator& other) const {
    return !(*this == other);
  }

  CompressedStrongListIterator& operator++() {
    last_pos_ = pos_;
    return *this;
  }

 private:
  // Returns the uint64_t at the given byte position.
  // It is the responsibility (contract) of the caller to ensure that the
  // position is valid.
  uint64_t ReadUint64AtByte(uint64_t pos) const {
    uint64_t result;
    // For debugging: Make sure we do not read past the end of the list.
    DCHECK_LE(pos + sizeof(result), compressed_vector_.byte_stream().size());
    std::memcpy(&result, compressed_vector_.byte_stream().data() + pos,
                sizeof(result));
    // Issuing a prefetch instruction can yield a performance gain of 5 to 10%.
    // But this depends on the instance. This needs a lot of tuning and
    // evaluation which is left for a future CL.
    // TODO(user): Add a prefetch instruction such as the following:
    // __builtin_prefetch(compressed_vector_.byte_stream().data() + pos + 64);
    return result;
  }

  // Decodes an integer from the byte stream at the current position.
  // The integer is guaranteed to be in the range [0, 1 << 56).
  Index DecodeInteger() {
    const uint64_t encoded_value = ReadUint64AtByte(pos_);
    const uint64_t first_byte = encoded_value & 0xFF;
    // If the least significant bit is clear, we use the fast path for a single
    // byte. This brings a performance gain of 5-10%.
    if (VonNeumannVarint::UsesOneByte(first_byte)) {
      ++pos_;
      memorized_value_ += (first_byte >> 1) + kMinDelta;
      return Index(memorized_value_ - kMinDelta);
    }
    const uint32_t size = VonNeumannVarint::EncodingLength(encoded_value);
    // Check that the encoding size is smaller than the size of a uint64_t.
    DCHECK_LT(size, sizeof(encoded_value));
    const uint64_t delta = VonNeumannVarint::DecodeSmallVarint(encoded_value);
    pos_ += size;
    memorized_value_ += delta + kMinDelta;
    return Index(memorized_value_ - kMinDelta);
  }

  // Decode an integer from the byte stream at the current position in the
  // general case. The integer can have up to 64 bits.
  Index DecodeAnyInteger() {
    const uint64_t encoded_value = ReadUint64AtByte(pos_);
    // If the encoded value uses the large encoding, we read the uint64_t
    // directly starting at the next byte. Otherwise, we use the small encoding.
    const bool uses_large_encoding =
        VonNeumannVarint::UsesLargeEncoding(encoded_value);
    const uint64_t delta =
        uses_large_encoding
            ? ReadUint64AtByte(pos_ + 1)
            : VonNeumannVarint::DecodeSmallVarint(encoded_value);
    pos_ += uses_large_encoding
                ? sizeof(encoded_value) + 1
                : VonNeumannVarint::EncodingLength(encoded_value);
    memorized_value_ += delta + kMinDelta;
    return Index(memorized_value_ - kMinDelta);
  }

  // The compressed list.
  const CompressedStrongList<EntryIndex, Index>& compressed_vector_;

  // The last value memorized in the decoder. It is defined as the last value
  // appended to the list plus a kMinDelta. It starts at zero.
  uint64_t memorized_value_;

  // The current position in the byte stream.
  uint64_t pos_;

  // The last position in the byte stream.
  uint64_t last_pos_;
};

using CompressedColumn = CompressedStrongList<ColumnEntryIndex, ElementIndex>;
using CompressedRow = CompressedStrongList<RowEntryIndex, SubsetIndex>;

using CompressedColumnView =
    util_intops::StrongVector<SubsetIndex, CompressedColumn>;
using CompressedRowView =
    util_intops::StrongVector<ElementIndex, CompressedRow>;

using CompressedColumnIterator =
    CompressedStrongListIterator<ColumnEntryIndex, ElementIndex>;
using CompressedRowIterator =
    CompressedStrongListIterator<RowEntryIndex, SubsetIndex>;
using CompressedColumnConstIterator =
    CompressedStrongListIterator<ColumnEntryIndex, ElementIndex>;
using CompressedRowConstIterator =
    CompressedStrongListIterator<RowEntryIndex, SubsetIndex>;

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

#endif  // OR_TOOLS_SET_COVER_BASE_TYPES_H_
