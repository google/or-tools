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

// Various utility functions on bitsets.

#ifndef ORTOOLS_UTIL_BITSET_H_
#define ORTOOLS_UTIL_BITSET_H_

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <tuple>
#include <type_traits>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/base/attributes.h"
#include "absl/log/check.h"

namespace operations_research {

// Basic bit operations

// Useful constants: word and double word will all bits set.
static const uint64_t kAllBits64 = uint64_t{0xFFFFFFFFFFFFFFFF};
static const uint64_t kAllBitsButLsb64 = uint64_t{0xFFFFFFFFFFFFFFFE};

// Returns a word with only bit pos set.
inline uint64_t OneBit64(int pos) { return uint64_t{1} << pos; }
inline uint32_t OneBit32(int pos) { return 1U << pos; }

// Returns the word with only the least significant bit of n set.
inline uint64_t LeastSignificantBitWord64(uint64_t n) { return n & ~(n - 1); }

namespace internal {

// Performance comparison of LSB/MSB implementations (BM_BitIndex on x86-64):
//
// Operation | Implementation      | Scalar (kNo)  | Vectorizable loop (kYes)
// ----------+---------------------+---------------+-------------------------
// LSB 32    | kBasic (bsearch)    | ~238M items/s | ~438M items/s
//           | kDeBruijn (lookup)  | ~1.07G items/s| ~3.15G items/s
//           | kStd (countr_zero)  | ~1.08G items/s| ~1.59G items/s
// ----------+---------------------+---------------+-------------------------
// LSB 64    | kBasic (bsearch)    | ~201M items/s | ~365M items/s
//           | kDeBruijn (lookup)  | ~1.08G items/s| ~3.18G items/s
//           | kStd (countr_zero)  | ~1.08G items/s| ~1.07G items/s
// ----------+---------------------+---------------+-------------------------
// MSB 32    | kBasic (bsearch)    | ~247M items/s | ~462M items/s
//           | kStd (countl_zero)  | ~643M items/s | ~800M items/s
// ----------+---------------------+---------------+-------------------------
// MSB 64    | kBasic (bsearch)    | ~207M items/s | ~356M items/s
//           | kStd (countl_zero)  | ~427M items/s | ~639M items/s

// Implementation notes:
// Alternative algorithms are retained here to allow performance evaluation in
// benchmarks across platforms, though the default implementation (kStd) is
// usually the best choice.
// - kStd (C++20 <bit>): Portable and compiles down to hardware bit-scan
//   instructions (tzcnt/bsf and lzcnt/bsr). Guarding n == 0 in
//   MostSignificantBitPosition matches the contract of kBasic and allows the
//   compiler to fold `31 - (31 ^ bsr(n))` into `bsr(n)` on baseline x86-64
//   targets without LZCNT.
// - kDeBruijn (De Bruijn multiplication + 64/32-entry table lookup): Matches
//   hardware instruction latency in scalar dependency chains and achieves ~2x
//   higher throughput in tight independent loops because the
//   multiply-shift-load sequence pipelines across multiple execution ports, at
//   the cost of a static lookup table in L1 cache.
// - kBasic (binary search / shift-and-mask fallback): ~3-5x slower than
//   hardware bit-scan or De Bruijn implementations due to sequential
//   conditional masks and shifts.
enum class BitScanAlgorithm {
  kStd,
  kDeBruijn,
  kBasic,
};

template <BitScanAlgorithm algo = BitScanAlgorithm::kStd, typename T>
inline int LeastSignificantBitPosition(T n) {
  static_assert(std::is_same_v<T, uint32_t> || std::is_same_v<T, uint64_t>,
                "Unsupported integer type");
  if constexpr (algo == BitScanAlgorithm::kStd) {
    return std::countr_zero(n);
  } else if constexpr (algo == BitScanAlgorithm::kDeBruijn) {
    if constexpr (std::is_same_v<T, uint64_t>) {
      static constexpr uint64_t kSeq = uint64_t{0x0218a392dd5fb34f};
      static constexpr int kTab[64] = {
          // initialized by 'kTab[(kSeq << i) >> 58] = i
          0,  1,  2,  7,  3,  13, 8,  19, 4,  25, 14, 28, 9,  52, 20, 58,
          5,  17, 26, 56, 15, 38, 29, 40, 10, 49, 53, 31, 21, 34, 59, 42,
          63, 6,  12, 18, 24, 27, 51, 57, 16, 55, 37, 39, 48, 30, 33, 41,
          62, 11, 23, 50, 54, 36, 47, 32, 61, 22, 35, 46, 60, 45, 44, 43,
      };
      return kTab[((n & (~n + 1)) * kSeq) >> 58];
    } else {
      static constexpr uint32_t kSeq = 0x077CB531U;  // de Bruijn sequence
      static constexpr int kTab[32] = {
          // initialized by 'kTab[(kSeq << i) >> 27] = i
          0,  1,  28, 2,  29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4,  8,
          31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6,  11, 5,  10, 9};
      return kTab[((n & (~n + 1)) * kSeq) >> 27];
    }
  } else if constexpr (algo == BitScanAlgorithm::kBasic) {
    DCHECK_NE(n, 0);
    int pos = sizeof(T) * 8 - 1;
    if constexpr (std::is_same_v<T, uint64_t>) {
      if (n & 0x00000000FFFFFFFFLL) {
        pos -= 32;
      } else {
        n >>= 32;
      }
    }
    if (n & 0x0000FFFFL) {
      pos -= 16;
    } else {
      n >>= 16;
    }
    if (n & 0x000000FFL) {
      pos -= 8;
    } else {
      n >>= 8;
    }
    if (n & 0x0000000FL) {
      pos -= 4;
    } else {
      n >>= 4;
    }
    if (n & 0x00000003L) {
      pos -= 2;
    } else {
      n >>= 2;
    }
    if (n & 0x00000001L) {
      pos -= 1;
    }
    return pos;
  } else {
    static_assert(algo == BitScanAlgorithm::kStd, "Unsupported algorithm");
  }
}

template <BitScanAlgorithm algo = BitScanAlgorithm::kStd, typename T>
inline int MostSignificantBitPosition(T n) {
  static_assert(std::is_same_v<T, uint32_t> || std::is_same_v<T, uint64_t>,
                "Unsupported integer type");
  static_assert(algo != BitScanAlgorithm::kDeBruijn,
                "DeBruijn is not implemented for MostSignificantBitPosition");
  if constexpr (algo == BitScanAlgorithm::kStd) {
    constexpr int kMaxBit = sizeof(T) * 8 - 1;
    return n == 0 ? 0 : (kMaxBit - std::countl_zero(n));
  } else if constexpr (algo == BitScanAlgorithm::kBasic) {
    constexpr T kAllBits = ~T{0};
    int b = 0;
    if constexpr (std::is_same_v<T, uint64_t>) {
      if (0 != (n & (kAllBits << (1 << 5)))) {
        b |= (1 << 5);
        n >>= (1 << 5);
      }
    }
    if (0 != (n & (kAllBits << (1 << 4)))) {
      b |= (1 << 4);
      n >>= (1 << 4);
    }
    if (0 != (n & (kAllBits << (1 << 3)))) {
      b |= (1 << 3);
      n >>= (1 << 3);
    }
    if (0 != (n & (kAllBits << (1 << 2)))) {
      b |= (1 << 2);
      n >>= (1 << 2);
    }
    if (0 != (n & (kAllBits << (1 << 1)))) {
      b |= (1 << 1);
      n >>= (1 << 1);
    }
    if (0 != (n & (kAllBits << (1 << 0)))) {
      b |= (1 << 0);
    }
    return b;
  } else {
    static_assert(algo == BitScanAlgorithm::kStd, "Unsupported algorithm");
  }
}

}  // namespace internal

// Returns the least significant bit position in n.
inline int LeastSignificantBitPosition64(uint64_t n) {
  DCHECK_NE(n, 0);
  return internal::LeastSignificantBitPosition(n);
}

inline int LeastSignificantBitPosition32(uint32_t n) {
  DCHECK_NE(n, 0);
  return internal::LeastSignificantBitPosition(n);
}

// Returns the most significant bit position in n.
inline int MostSignificantBitPosition64(uint64_t n) {
  return internal::MostSignificantBitPosition(n);
}

inline int MostSignificantBitPosition32(uint32_t n) {
  return internal::MostSignificantBitPosition(n);
}

// Returns a word with bits from s to e set.
inline uint64_t OneRange64(uint64_t s, uint64_t e) {
  DCHECK_LE(s, 63);
  DCHECK_LE(e, 63);
  DCHECK_LE(s, e);
  return (kAllBits64 << s) ^ ((kAllBits64 - 1) << e);
}

// Returns a word with s least significant bits unset.
inline uint64_t IntervalUp64(uint64_t s) {
  DCHECK_LE(s, 63);
  return kAllBits64 << s;
}

// Returns a word with the s most significant bits unset.
inline uint64_t IntervalDown64(uint64_t s) {
  DCHECK_LE(s, 63);
  return kAllBits64 >> (63 - s);
}

// ----- Bitset operators -----
// Bitset: array of uint64_t words

// Bit operators used to manipulates bitsets.

// Returns the bit number in the word computed by BitOffset64,
// corresponding to the bit at position pos in the bitset.
// Note: '& 63' is faster than '% 64'
// TODO(user): rename BitPos and BitOffset to something more understandable.
inline uint32_t BitPos64(uint64_t pos) { return (pos & 63); }

// Returns the word number corresponding to bit number pos.
inline uint64_t BitOffset64(uint64_t pos) { return (pos >> 6); }

// Returns the number of words needed to store size bits.
inline uint64_t BitLength64(uint64_t size) { return ((size + 63) >> 6); }

// Returns the bit number in the bitset of the first bit of word number v.
inline uint64_t BitShift64(uint64_t v) { return v << 6; }

// Returns true if the bit pos is set in bitset.
inline bool IsBitSet64(const uint64_t* const bitset, uint64_t pos) {
  return (bitset[BitOffset64(pos)] & OneBit64(BitPos64(pos)));
}

// Sets the bit pos to true in bitset.
inline void SetBit64(uint64_t* const bitset, uint64_t pos) {
  bitset[BitOffset64(pos)] |= OneBit64(BitPos64(pos));
}

// Sets the bit pos to false in bitset.
inline void ClearBit64(uint64_t* const bitset, uint64_t pos) {
  bitset[BitOffset64(pos)] &= ~OneBit64(BitPos64(pos));
}

// Returns the number of bits set in bitset between positions start and end.
uint64_t BitCountRange64(const uint64_t* bitset, uint64_t start, uint64_t end);

// Returns true if no bits are set in bitset between start and end.
bool IsEmptyRange64(const uint64_t* bitset, uint64_t start, uint64_t end);

// Returns the first bit set in bitset between start and max_bit.
int64_t LeastSignificantBitPosition64(const uint64_t* bitset, uint64_t start,
                                      uint64_t end);

// Returns the last bit set in bitset between min_bit and start.
int64_t MostSignificantBitPosition64(const uint64_t* bitset, uint64_t start,
                                     uint64_t end);

// Unsafe versions of the functions above where respectively end and start
// are supposed to be set.
int64_t UnsafeLeastSignificantBitPosition64(const uint64_t* bitset,
                                            uint64_t start, uint64_t end);

int64_t UnsafeMostSignificantBitPosition64(const uint64_t* bitset,
                                           uint64_t start, uint64_t end);

// A dynamically sized bitset indexed by `IndexType` (an integral or strong
// index type) and providing an efficient way to iterate over the true
// positions. It is tuned for maximum speed, bound checking is performed though
// DCHECK and is disabled in optimized build.
template <typename IndexType = int64_t>
class Bitset64 {
  // Stores `size` and checks `0 <= index < size` in debug builds; empty struct
  // in NDEBUG builds so `ViewImpl` below remains a single pointer in size.
  class BoundsChecker {
   public:
#ifndef NDEBUG
    constexpr explicit BoundsChecker(IndexType size) : size_(size) {}
    void Check(IndexType index) const {
      DCHECK_GE(index, IndexType(0));
      DCHECK_LT(index, size_);
    }

   private:
    const IndexType size_;
#else
    constexpr explicit BoundsChecker(IndexType) {}
    constexpr void Check(IndexType) const {}
#endif
  };

  // Lightweight view caching the underlying `buckets_.data()` pointer for tight
  // loops. Performs bounds checking in debug builds and compiles down to a
  // single pointer in optimized (NDEBUG) builds.
  template <typename DataType>
  class ViewImpl {
   public:
    bool operator[](IndexType index) const {
      return GetBucket(index) & GetBitMask(index);
    }
    void Clear(IndexType index) { GetBucket(index) &= ~GetBitMask(index); }
    void Set(IndexType index) { GetBucket(index) |= GetBitMask(index); }
    DataType& GetBucket(IndexType index) const {
      bounds_checker_.Check(index);
      return buckets_[BitOffset64(Value(index))];
    }

   private:
    friend class Bitset64;
    ViewImpl(DataType* buckets, IndexType size)
        : buckets_(buckets), bounds_checker_(size) {}

    static DataType GetBitMask(IndexType index) {
      return OneBit64(BitPos64(Value(index)));
    }

    DataType* const buckets_;
    ABSL_ATTRIBUTE_NO_UNIQUE_ADDRESS BoundsChecker bounds_checker_;
  };

 public:
  using ConstView = ViewImpl<const uint64_t>;
  using View = ViewImpl<uint64_t>;

  // Creates an empty bitset.
  Bitset64() = default;

  // Constructs a bitset of `size` bits, all initialized to `0`.
  explicit Bitset64(IndexType size)
      : size_(CheckAndClampSize(size)), buckets_(BitLength64(Value(size_))) {}

  // Returns a read-only or mutable view caching the underlying bucket pointer.
  ConstView const_view() const { return ConstView(buckets_.data(), size_); }
  View view() { return View(buckets_.data(), size_); }

  // Returns the number of bits in this bitset.
  IndexType size() const { return size_; }

  // Resizes the Bitset64 to the given number of bits. New bits are sets to `0`.
  void resize(int size) { Resize(IndexType(size)); }
  void Resize(IndexType size) {
    const IndexType new_size = CheckAndClampSize(size);
    buckets_.resize(BitLength64(Value(new_size)));
    if (const uint32_t bit_pos = BitPos64(Value(new_size));
        new_size < size_ && bit_pos != 0) {
      buckets_.back() &= OneBit64(bit_pos) - 1;
    }
    size_ = new_size;
  }

  // Resizes the bitset to `size` bits and clears all the bits.
  void ClearAndResize(IndexType size) {
    size_ = CheckAndClampSize(size);
    buckets_.resize(BitLength64(Value(size_)));
    ClearAll();
  }

  // Clears all the bits.
  void ClearAll() {
    // Lowered by the compiler to a single `memset` call.
    absl::c_fill(buckets_, uint64_t{0});
  }

  // Clears the bit at position `index`.
  void Clear(IndexType index) { view().Clear(index); }

  // Clears all the bits in the bucket containing bit `index`.
  void ClearBucket(IndexType index) { view().GetBucket(index) = uint64_t{0}; }

  // Returns whether the bit at position `index` is set.
  bool IsSet(IndexType index) const { return const_view()[index]; }

  // Same as IsSet().
  bool operator[](IndexType index) const { return IsSet(index); }

  // Sets the bit at position `index`.
  void Set(IndexType index) { view().Set(index); }

  // Set the bit at position `ìndex` to 1 when value is `true`, and to 0
  // otherwise.
  void Set(IndexType index, bool value) {
    if (value) {
      view().Set(index);
    } else {
      view().Clear(index);
    }
  }

  // Copies the bucket containing bit `index` from `other` to `*this`.
  void CopyBucket(const Bitset64<IndexType>& other, IndexType index) {
    view().GetBucket(index) = other.const_view().GetBucket(index);
  }

  // Replaces `*this` with the bitwise intersection of `*this` and `other`. The
  // bitsets do not have to be the same size; if `other` is smaller,
  // higher-order bits are cleared to 0.
  void Intersection(const Bitset64& other) {
    const auto it = absl::c_transform(buckets_, other.buckets_,
                                      buckets_.begin(), std::bit_and<>{});
    std::fill(it, buckets_.end(), uint64_t{0});
  }

  // Sets `*this` to the bitwise intersection of `a` and `b`, which must have
  // the same `size()`.
  void SetToIntersectionOf(const Bitset64& a, const Bitset64& b) {
    DCHECK_EQ(a.size(), b.size());
    Resize(a.size());
    absl::c_transform(a.buckets_, b.buckets_, buckets_.begin(),
                      std::bit_and<>{});
  }

  // Replaces `*this` with the bitwise union of `*this` and `other`. The bitsets
  // do not have to be the same size; if `other` is smaller, higher-order bits
  // of `*this` are unchanged, and if `other` is larger, its extra bits are
  // ignored.
  void Union(const Bitset64& other) {
    absl::c_transform(buckets_, other.buckets_, buckets_.begin(),
                      std::bit_or<>{});
  }

  // Returns true if all bits are 0.
  bool IsAllFalse() const {
    return absl::c_all_of(buckets_,
                          [](uint64_t bucket) { return bucket == 0; });
  }

  // Forward iterator over the indices of set bits.
  //
  // IMPORTANT: Because `Iterator` caches the current 64-bit bucket, modifying
  // the bitset while iterating produces undefined results.
  class Iterator {
   public:
    // Make this iterator a std::forward_iterator, so it works with std::sample,
    // std::max_element, etc.
    Iterator() : buckets_(nullptr), size_(0) {}
    Iterator(Iterator&& other) = default;
    Iterator(const Iterator& other) = default;
    Iterator& operator=(const Iterator& other) = default;
    using difference_type = std::ptrdiff_t;
    using iterator_category = std::forward_iterator_tag;
    using value_type = IndexType;
    using size_type = std::size_t;
    using reference = value_type&;
    using pointer = value_type*;

    explicit Iterator(const Bitset64& bitset)
        : buckets_(bitset.buckets_.data()), size_(bitset.buckets_.size()) {
      if (!bitset.buckets_.empty()) {
        current_ = buckets_[0];
        this->operator++();
      }
    }

    static Iterator EndIterator(const Bitset64& bitset) {
      return Iterator(bitset.buckets_.data());
    }

    bool operator==(const Iterator& other) const { return !(*this != other); }
    bool operator!=(const Iterator& other) const {
      if (other.size_ == 0) {
        return size_ != 0;
      }
      return std::tie(index_, current_) !=
             std::tie(other.index_, other.current_);
    }

    IndexType operator*() const { return IndexType(index_); }

    Iterator operator++(int) {
      Iterator other = *this;
      ++(*this);
      return other;
    }

    Iterator& operator++() {
      int bucket = BitOffset64(index_);
      while (current_ == 0) {
        bucket++;
        if (bucket == size_) {
          size_ = 0;
          return *this;
        }
        current_ = buckets_[bucket];
      }

      // Computes the index and clears the least significant bit of current_.
      index_ = BitShift64(bucket) | LeastSignificantBitPosition64(current_);
      current_ &= current_ - 1;
      return *this;
    }

   private:
    explicit Iterator(const uint64_t* buckets) : buckets_(buckets), size_(0) {}

    const uint64_t* buckets_;
    int size_;
    int index_ = 0;
    uint64_t current_ = 0;
  };

  // Returns iterators over the indices of bits set to 1, supporting range-based
  // `for` loops: `for (const IndexType index : bitset)`.
  Iterator begin() const { return Iterator(*this); }
  Iterator end() const { return Iterator::EndIterator(*this); }

 private:
  static constexpr IndexType CheckAndClampSize(IndexType size) {
    DCHECK_GE(size, IndexType(0));
    return size > IndexType(0) ? size : IndexType(0);
  }

  // Returns the underlying integer value of `index` in its native width.
  static constexpr auto Value(IndexType index) {
    if constexpr (std::is_integral_v<IndexType>) {
      return index;
    } else {
      return index.value();
    }
  }

  IndexType size_ = IndexType(0);
  std::vector<uint64_t> buckets_;
};

// Specialized version of Bitset64 that allows to query the last bit set more
// efficiently.
class BitQueue64 {
 public:
  BitQueue64() : size_(), top_(-1), data_() {}
  explicit BitQueue64(int size)
      : size_(size), top_(-1), data_(BitLength64(size), 0) {}

  // This type is neither copyable nor movable.
  BitQueue64(const BitQueue64&) = delete;
  BitQueue64& operator=(const BitQueue64&) = delete;

  void IncreaseSize(int size) {
    CHECK_GE(size, size_);
    size_ = size;
    data_.resize(BitLength64(size), 0);
  }

  void ClearAndResize(int size) {
    top_ = -1;
    size_ = size;
    data_.assign(BitLength64(size), 0);
  }

  void Set(int i) {
    DCHECK_GE(i, 0);
    DCHECK_LT(i, size_);
    top_ = std::max(top_, i);
    data_[BitOffset64(i)] |= OneBit64(BitPos64(i));
  }

  // Sets all the bits from 0 up to i-1 to 1.
  void SetAllBefore(int i) {
    DCHECK_GE(i, 0);
    DCHECK_LT(i, size_);
    top_ = std::max(top_, i - 1);
    int bucket_index = static_cast<int>(BitOffset64(i));
    data_[bucket_index] |= OneBit64(BitPos64(i)) - 1;
    for (--bucket_index; bucket_index >= 0; --bucket_index) {
      data_[bucket_index] = kAllBits64;
    }
  }

  // Returns the position of the highest bit set in O(1) or -1 if no bit is set.
  int Top() const { return top_; }

  // Clears the Top() bit and recomputes the position of the next Top().
  void ClearTop() {
    DCHECK_NE(top_, -1);
    int bucket_index = static_cast<int>(BitOffset64(top_));
    uint64_t bucket = data_[bucket_index] &= ~OneBit64(BitPos64(top_));
    while (!bucket) {
      if (bucket_index == 0) {
        top_ = -1;
        return;
      }
      bucket = data_[--bucket_index];
    }

    // NOTE(user): I experimented with reversing the bit order in a bucket to
    // use LeastSignificantBitPosition64() and it is only slightly faster at the
    // cost of a lower Set() speed. So I preferred this version.
    top_ = static_cast<int>(BitShift64(bucket_index) +
                            MostSignificantBitPosition64(bucket));
  }

 private:
  int size_;
  int top_;
  std::vector<uint64_t> data_;
};

// A simple utility class to set/unset integer in a range [0, size).
// This is optimized for sparsity.
template <typename IntegerType = int64_t>
class SparseBitset {
 public:
  SparseBitset() {}
  explicit SparseBitset(IntegerType size) : bitset_(size) {}

  // This type is neither copyable nor movable.
  SparseBitset(const SparseBitset&) = delete;
  SparseBitset& operator=(const SparseBitset&) = delete;
  IntegerType size() const { return bitset_.size(); }
  void ResetAllToFalse() { ClearAndResize(size()); }
  void ClearAndResize(IntegerType size) {
    // As of 19/03/2014, experiments show that this is a reasonable threshold.
    const int kSparseThreshold = 300;
    if (to_clear_.size() * kSparseThreshold < size) {
      for (const IntegerType i : to_clear_) bitset_.ClearBucket(i);
      to_clear_.clear();
      bitset_.Resize(size);
    } else {
      bitset_.ClearAndResize(size);
      to_clear_.clear();
    }
  }
  void Resize(IntegerType size) {
    if (size < bitset_.size()) {
      int new_index = 0;
      for (IntegerType index : to_clear_) {
        if (index < size) {
          to_clear_[new_index] = index;
          ++new_index;
        }
      }
      to_clear_.resize(new_index);
    }
    bitset_.Resize(size);
  }
  bool operator[](IntegerType index) const { return bitset_[index]; }
  void Set(IntegerType index) {
    if (!bitset_[index]) {
      bitset_.Set(index);
      to_clear_.push_back(index);
    }
  }

  void CopyFrom(const SparseBitset& other) {
    bitset_ = other.bitset_;
    to_clear_.assign(other.to_clear_.begin(), other.to_clear_.end());
  }

  // A bit hacky for really hot loop.
  typename Bitset64<IntegerType>::View BitsetView() { return bitset_.view(); }
  typename Bitset64<IntegerType>::ConstView BitsetConstView() {
    return bitset_.const_view();
  }
  void SetUnsafe(typename Bitset64<IntegerType>::View view, IntegerType index) {
    view.Set(index);
    to_clear_.push_back(index);
  }

  void Clear(IntegerType index) { bitset_.Clear(index); }
  int NumberOfSetCallsWithDifferentArguments() const {
    return to_clear_.size();
  }
  const std::vector<IntegerType>& PositionsSetAtLeastOnce() const {
    return to_clear_;
  }

  // Tells the class that all its bits are cleared, so it can reset to_clear_
  // to the empty vector. Note that this call is "unsafe" since the fact that
  // the class is actually all cleared is only checked in debug mode.
  //
  // This is useful to iterate on the "set" positions while clearing them for
  // instance. This way, after the loop, a client can call this for efficiency.
  void NotifyAllClear() {
#if !defined(NDEBUG)
    for (IntegerType index : to_clear_) CHECK(!bitset_[index]);
#endif
    to_clear_.clear();
  }

  typename Bitset64<IntegerType>::ConstView const_view() const {
    return bitset_.const_view();
  }

 private:
  Bitset64<IntegerType> bitset_;
  std::vector<IntegerType> to_clear_;
};

}  // namespace operations_research

#endif  // ORTOOLS_UTIL_BITSET_H_
