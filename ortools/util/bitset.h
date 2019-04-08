// Copyright 2010-2018 Google LLC
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

#ifndef OR_TOOLS_UTIL_BITSET_H_
#define OR_TOOLS_UTIL_BITSET_H_

#include <string.h>

#include <algorithm>
#include <vector>

#include "ortools/base/basictypes.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"

namespace operations_research {

// Basic bit operations

// Useful constants: word and double word will all bits set.
static const uint64 kAllBits64 = GG_ULONGLONG(0xFFFFFFFFFFFFFFFF);
static const uint64 kAllBitsButLsb64 = GG_ULONGLONG(0xFFFFFFFFFFFFFFFE);
static const uint32 kAllBits32 = 0xFFFFFFFFU;

// Returns a word with only bit pos set.
inline uint64 OneBit64(int pos) { return GG_ULONGLONG(1) << pos; }
inline uint32 OneBit32(int pos) { return 1U << pos; }

// Returns the number of bits set in n.
inline uint64 BitCount64(uint64 n) {
  const uint64 m1 = GG_ULONGLONG(0x5555555555555555);
  const uint64 m2 = GG_ULONGLONG(0x3333333333333333);
  const uint64 m4 = GG_ULONGLONG(0x0F0F0F0F0F0F0F0F);
  const uint64 h01 = GG_ULONGLONG(0x0101010101010101);
  n -= (n >> 1) & m1;
  n = (n & m2) + ((n >> 2) & m2);
  n = (n + (n >> 4)) & m4;
  n = (n * h01) >> 56;
  return n;
}
inline uint32 BitCount32(uint32 n) {
  n -= (n >> 1) & 0x55555555UL;
  n = (n & 0x33333333) + ((n >> 2) & 0x33333333UL);
  n = (n + (n >> 4)) & 0x0F0F0F0FUL;
  n = n + (n >> 8);
  n = n + (n >> 16);
  return n & 0x0000003FUL;
}

// Returns a word with only the least significant bit of n set.
inline uint64 LeastSignificantBitWord64(uint64 n) { return n & ~(n - 1); }
inline uint32 LeastSignificantBitWord32(uint32 n) { return n & ~(n - 1); }

// Returns the least significant bit position in n.
// Discussion around lsb computation:
// De Bruijn is almost as fast as the bsr/bsf-instruction-based intrinsics.
// Both are always much faster than the Default algorithm.
#define USE_DEBRUIJN true  // if true, use de Bruijn bit forward scanner.
#if defined(__GNUC__) || defined(__llvm__)
#define USE_FAST_LEAST_SIGNIFICANT_BIT true  // if true, use fast lsb.
#endif

#if defined(USE_FAST_LEAST_SIGNIFICANT_BIT)
inline int LeastSignificantBitPosition64Fast(uint64 n) {
  return n == 0 ? 0 : __builtin_ctzll(n);
}
#endif

inline int LeastSignificantBitPosition64DeBruijn(uint64 n) {
  static const uint64 kSeq = GG_ULONGLONG(0x0218a392dd5fb34f);
  static const int kTab[64] = {
      // initialized by 'kTab[(kSeq << i) >> 58] = i
      0,  1,  2,  7,  3,  13, 8,  19, 4,  25, 14, 28, 9,  52, 20, 58,
      5,  17, 26, 56, 15, 38, 29, 40, 10, 49, 53, 31, 21, 34, 59, 42,
      63, 6,  12, 18, 24, 27, 51, 57, 16, 55, 37, 39, 48, 30, 33, 41,
      62, 11, 23, 50, 54, 36, 47, 32, 61, 22, 35, 46, 60, 45, 44, 43,
  };
  return kTab[((n & (~n + 1)) * kSeq) >> 58];
}

inline int LeastSignificantBitPosition64Default(uint64 n) {
  if (n == 0) return 0;
  int pos = 63;
  if (n & 0x00000000FFFFFFFFLL) {
    pos -= 32;
  } else {
    n >>= 32;
  }
  if (n & 0x000000000000FFFFLL) {
    pos -= 16;
  } else {
    n >>= 16;
  }
  if (n & 0x00000000000000FFLL) {
    pos -= 8;
  } else {
    n >>= 8;
  }
  if (n & 0x000000000000000FLL) {
    pos -= 4;
  } else {
    n >>= 4;
  }
  if (n & 0x0000000000000003LL) {
    pos -= 2;
  } else {
    n >>= 2;
  }
  if (n & 0x0000000000000001LL) {
    pos -= 1;
  }
  return pos;
}

inline int LeastSignificantBitPosition64(uint64 n) {
  DCHECK_NE(n, 0);
#ifdef USE_FAST_LEAST_SIGNIFICANT_BIT
  return LeastSignificantBitPosition64Fast(n);
#elif defined(USE_DEBRUIJN)
  return LeastSignificantBitPosition64DeBruijn(n);
#else
  return LeastSignificantBitPosition64Default(n);
#endif
}

#if defined(USE_FAST_LEAST_SIGNIFICANT_BIT)
inline int LeastSignificantBitPosition32Fast(uint32 n) {
  return n == 0 ? 0 : __builtin_ctzl(n);
}
#endif

inline int LeastSignificantBitPosition32DeBruijn(uint32 n) {
  static const uint32 kSeq = 0x077CB531U;  // de Bruijn sequence
  static const int kTab[32] = {// initialized by 'kTab[(kSeq << i) >> 27] = i
                               0,  1,  28, 2,  29, 14, 24, 3,  30, 22, 20,
                               15, 25, 17, 4,  8,  31, 27, 13, 23, 21, 19,
                               16, 7,  26, 12, 18, 6,  11, 5,  10, 9};
  return kTab[((n & (~n + 1)) * kSeq) >> 27];
}

inline int LeastSignificantBitPosition32Default(uint32 n) {
  if (n == 0) return 0;
  int pos = 31;
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
}

inline int LeastSignificantBitPosition32(uint32 n) {
  DCHECK_NE(n, 0);
#ifdef USE_FAST_LEAST_SIGNIFICANT_BIT
  return LeastSignificantBitPosition32Fast(n);
#elif defined(USE_DEBRUIJN)
  return LeastSignificantBitPosition32DeBruijn(n);
#else
  return LeastSignificantBitPosition32Default(n);
#endif
}

// Returns the most significant bit position in n.
#if USE_FAST_LEAST_SIGNIFICANT_BIT
inline int MostSignificantBitPosition64Fast(uint64 n) {
  // __builtin_clzll(1) should always return 63. There is no penalty in
  // using offset, and the code looks more like its uint32 counterpart.
  const int offset = __builtin_clzll(1);
  return n == 0 ? 0 : (offset - __builtin_clzll(n));
}
#endif

inline int MostSignificantBitPosition64Default(uint64 n) {
  int b = 0;
  if (0 != (n & (kAllBits64 << (1 << 5)))) {
    b |= (1 << 5);
    n >>= (1 << 5);
  }
  if (0 != (n & (kAllBits64 << (1 << 4)))) {
    b |= (1 << 4);
    n >>= (1 << 4);
  }
  if (0 != (n & (kAllBits64 << (1 << 3)))) {
    b |= (1 << 3);
    n >>= (1 << 3);
  }
  if (0 != (n & (kAllBits64 << (1 << 2)))) {
    b |= (1 << 2);
    n >>= (1 << 2);
  }
  if (0 != (n & (kAllBits64 << (1 << 1)))) {
    b |= (1 << 1);
    n >>= (1 << 1);
  }
  if (0 != (n & (kAllBits64 << (1 << 0)))) {
    b |= (1 << 0);
  }
  return b;
}

inline int MostSignificantBitPosition64(uint64 n) {
#ifdef USE_FAST_LEAST_SIGNIFICANT_BIT
  return MostSignificantBitPosition64Fast(n);
#else
  return MostSignificantBitPosition64Default(n);
#endif
}

#if USE_FAST_LEAST_SIGNIFICANT_BIT
inline int MostSignificantBitPosition32Fast(uint32 n) {
  // The constant here depends on whether we are on a 32-bit or 64-bit machine.
  // __builtin_clzl(1) returns 63 on a 64-bit machine and 31 on a 32-bit
  // machine.
  const int offset = __builtin_clzl(1);
  return n == 0 ? 0 : (offset - __builtin_clzl(n));
}
#endif

inline int MostSignificantBitPosition32Default(uint32 n) {
  int b = 0;
  if (0 != (n & (kAllBits32 << (1 << 4)))) {
    b |= (1 << 4);
    n >>= (1 << 4);
  }
  if (0 != (n & (kAllBits32 << (1 << 3)))) {
    b |= (1 << 3);
    n >>= (1 << 3);
  }
  if (0 != (n & (kAllBits32 << (1 << 2)))) {
    b |= (1 << 2);
    n >>= (1 << 2);
  }
  if (0 != (n & (kAllBits32 << (1 << 1)))) {
    b |= (1 << 1);
    n >>= (1 << 1);
  }
  if (0 != (n & (kAllBits32 << (1 << 0)))) {
    b |= (1 << 0);
  }
  return b;
}

inline int MostSignificantBitPosition32(uint32 n) {
#ifdef USE_FAST_LEAST_SIGNIFICANT_BIT
  return MostSignificantBitPosition32Fast(n);
#else
  return MostSignificantBitPosition32Default(n);
#endif
}

#undef USE_DEBRUIJN
#undef USE_FAST_LEAST_SIGNIFICANT_BIT

// Returns a word with bits from s to e set.
inline uint64 OneRange64(uint64 s, uint64 e) {
  DCHECK_LE(s, 63);
  DCHECK_LE(e, 63);
  DCHECK_LE(s, e);
  return (kAllBits64 << s) ^ ((kAllBits64 - 1) << e);
}

inline uint32 OneRange32(uint32 s, uint32 e) {
  DCHECK_LE(s, 31);
  DCHECK_LE(e, 31);
  DCHECK_LE(s, e);
  return (kAllBits32 << s) ^ ((kAllBits32 - 1) << e);
}

// Returns a word with s least significant bits unset.
inline uint64 IntervalUp64(uint64 s) {
  DCHECK_LE(s, 63);
  return kAllBits64 << s;
}

inline uint32 IntervalUp32(uint32 s) {
  DCHECK_LE(s, 31);
  return kAllBits32 << s;
}

// Returns a word with the s most significant bits unset.
inline uint64 IntervalDown64(uint64 s) {
  DCHECK_LE(s, 63);
  return kAllBits64 >> (63 - s);
}

inline uint32 IntervalDown32(uint32 s) {
  DCHECK_LE(s, 31);
  return kAllBits32 >> (31 - s);
}

// ----- Bitset operators -----
// Bitset: array of uint32/uint64 words

// Bit operators used to manipulates bitsets.

// Returns the bit number in the word computed by BitOffsetXX,
// corresponding to the bit at position pos in the bitset.
// Note: '& 63' is faster than '% 64'
// TODO(user): rename BitPos and BitOffset to something more understandable.
inline uint32 BitPos64(uint64 pos) { return (pos & 63); }
inline uint32 BitPos32(uint32 pos) { return (pos & 31); }

// Returns the word number corresponding to bit number pos.
inline uint64 BitOffset64(uint64 pos) { return (pos >> 6); }
inline uint32 BitOffset32(uint32 pos) { return (pos >> 5); }

// Returns the number of words needed to store size bits.
inline uint64 BitLength64(uint64 size) { return ((size + 63) >> 6); }
inline uint32 BitLength32(uint32 size) { return ((size + 31) >> 5); }

// Returns the bit number in the bitset of the first bit of word number v.
inline uint64 BitShift64(uint64 v) { return v << 6; }
inline uint32 BitShift32(uint32 v) { return v << 5; }

// Returns true if the bit pos is set in bitset.
inline bool IsBitSet64(const uint64* const bitset, uint64 pos) {
  return (bitset[BitOffset64(pos)] & OneBit64(BitPos64(pos)));
}
inline bool IsBitSet32(const uint32* const bitset, uint32 pos) {
  return (bitset[BitOffset32(pos)] & OneBit32(BitPos32(pos)));
}

// Sets the bit pos to true in bitset.
inline void SetBit64(uint64* const bitset, uint64 pos) {
  bitset[BitOffset64(pos)] |= OneBit64(BitPos64(pos));
}
inline void SetBit32(uint32* const bitset, uint32 pos) {
  bitset[BitOffset32(pos)] |= OneBit32(BitPos32(pos));
}

// Sets the bit pos to false in bitset.
inline void ClearBit64(uint64* const bitset, uint64 pos) {
  bitset[BitOffset64(pos)] &= ~OneBit64(BitPos64(pos));
}
inline void ClearBit32(uint32* const bitset, uint32 pos) {
  bitset[BitOffset32(pos)] &= ~OneBit32(BitPos32(pos));
}

// Returns the number of bits set in bitset between positions start and end.
uint64 BitCountRange64(const uint64* const bitset, uint64 start, uint64 end);
uint32 BitCountRange32(const uint32* const bitset, uint32 start, uint32 end);

// Returns true if no bits are set in bitset between start and end.
bool IsEmptyRange64(const uint64* const bitset, uint64 start, uint64 end);
bool IsEmptyRange32(const uint32* const bitset, uint32 start, uint32 end);

// Returns the first bit set in bitset between start and max_bit.
int64 LeastSignificantBitPosition64(const uint64* const bitset, uint64 start,
                                    uint64 end);
int LeastSignificantBitPosition32(const uint32* const bitset, uint32 start,
                                  uint32 end);

// Returns the last bit set in bitset between min_bit and start.
int64 MostSignificantBitPosition64(const uint64* const bitset, uint64 start,
                                   uint64 end);
int MostSignificantBitPosition32(const uint32* const bitset, uint32 start,
                                 uint32 end);

// Unsafe versions of the functions above where respectively end and start
// are supposed to be set.
int64 UnsafeLeastSignificantBitPosition64(const uint64* const bitset,
                                          uint64 start, uint64 end);
int32 UnsafeLeastSignificantBitPosition32(const uint32* const bitset,
                                          uint32 start, uint32 end);

int64 UnsafeMostSignificantBitPosition64(const uint64* const bitset,
                                         uint64 start, uint64 end);
int32 UnsafeMostSignificantBitPosition32(const uint32* const bitset,
                                         uint32 start, uint32 end);

// Returns a mask with the bits pos % 64 and (pos ^ 1) % 64 sets.
inline uint64 TwoBitsFromPos64(uint64 pos) {
  return GG_ULONGLONG(3) << (pos & 62);
}

// This class is like an ITIVector<IndexType, bool> except that it provides a
// more efficient way to iterate over the positions set to true. It achieves
// this by caching the current uint64 bucket in the Iterator and using
// LeastSignificantBitPosition64() to iterate over the positions at 1 in this
// bucket.
template <typename IndexType = int64>
class Bitset64 {
 public:
  Bitset64() : size_(), data_(), end_(*this, /*at_end=*/true) {}
  explicit Bitset64(IndexType size)
      : size_(Value(size) > 0 ? size : IndexType(0)),
        data_(BitLength64(Value(size_))),
        end_(*this, /*at_end=*/true) {}

  // Returns how many bits this Bitset64 can hold.
  IndexType size() const { return size_; }

  // Appends value at the end of the bitset.
  void PushBack(bool value) {
    ++size_;
    data_.resize(BitLength64(Value(size_)), 0);
    Set(size_ - 1, value);
  }

  // Resizes the Bitset64 to the given number of bits. New bits are sets to 0.
  void Resize(IndexType size) {
    DCHECK_GE(Value(size), 0);
    size_ = Value(size) > 0 ? size : IndexType(0);
    data_.resize(BitLength64(Value(size_)), 0);
  }

  // Changes the number of bits the Bitset64 can hold and set all of them to 0.
  void ClearAndResize(IndexType size) {
    DCHECK_GE(Value(size), 0);
    size_ = Value(size) > 0 ? size : IndexType(0);

    // Memset is 4x faster than data_.assign() as of 19/03/2014.
    // TODO(user): Ideally if a realloc happens, we don't need to copy the old
    // data...
    const size_t bit_length = static_cast<size_t>(BitLength64(Value(size_)));
    const size_t to_clear = std::min(data_.size(), bit_length);
    data_.resize(bit_length, 0);
    memset(data_.data(), 0, to_clear * sizeof(int64));
  }

  // Sets all bits to 0.
  void ClearAll() { memset(data_.data(), 0, data_.size() * sizeof(int64)); }

  // Sets the bit at position i to 0.
  void Clear(IndexType i) {
    DCHECK_GE(Value(i), 0);
    DCHECK_LT(Value(i), Value(size_));
    data_[BitOffset64(Value(i))] &= ~OneBit64(BitPos64(Value(i)));
  }

  // Sets bucket containing bit i to 0.
  void ClearBucket(IndexType i) {
    DCHECK_GE(Value(i), 0);
    DCHECK_LT(Value(i), Value(size_));
    data_[BitOffset64(Value(i))] = 0;
  }

  // Clears the bits at position i and i ^ 1.
  void ClearTwoBits(IndexType i) {
    DCHECK_GE(Value(i), 0);
    DCHECK_LT(Value(i), Value(size_));
    data_[BitOffset64(Value(i))] &= ~TwoBitsFromPos64(Value(i));
  }

  // Returns true if the bit at position i or the one at position i ^ 1 is set.
  bool AreOneOfTwoBitsSet(IndexType i) const {
    DCHECK_GE(Value(i), 0);
    DCHECK_LT(Value(i), Value(size_));
    return data_[BitOffset64(Value(i))] & TwoBitsFromPos64(Value(i));
  }

  // Returns true if the bit at position i is set.
  bool IsSet(IndexType i) const {
    DCHECK_GE(Value(i), 0);
    DCHECK_LT(Value(i), Value(size_));
    return data_[BitOffset64(Value(i))] & OneBit64(BitPos64(Value(i)));
  }

  // Same as IsSet().
  bool operator[](IndexType i) const { return IsSet(i); }

  // Sets the bit at position i to 1.
  void Set(IndexType i) {
    DCHECK_GE(Value(i), 0);
    DCHECK_LT(Value(i), size_);
    data_[BitOffset64(Value(i))] |= OneBit64(BitPos64(Value(i)));
  }

  // If value is true, sets the bit at position i to 1, sets it to 0 otherwise.
  void Set(IndexType i, bool value) {
    if (value) {
      Set(i);
    } else {
      Clear(i);
    }
  }

  // Copies bucket containing bit i from "other" to "this".
  void CopyBucket(const Bitset64<IndexType>& other, IndexType i) {
    const uint64 offset = BitOffset64(Value(i));
    data_[offset] = other.data_[offset];
  }

  // Copies "other" to "this". The bitsets do not have to be of the same size.
  // If "other" is smaller, high order bits are not changed. If "other" is
  // larger, its high order bits are ignored. In any case "this" is not resized.
  template <typename OtherIndexType>
  void SetContentFromBitset(const Bitset64<OtherIndexType>& other) {
    const int64 min_size = std::min(data_.size(), other.data_.size());
    if (min_size == 0) return;
    const uint64 last_common_bucket = data_[min_size - 1];
    memcpy(data_.data(), other.data_.data(), min_size * sizeof(uint64));
    if (data_.size() >= other.data_.size()) {
      const uint64 bitmask = kAllBitsButLsb64
                             << BitPos64(other.Value(other.size() - 1));
      data_[min_size - 1] &= ~bitmask;
      data_[min_size - 1] |= (bitmask & last_common_bucket);
    }
  }

  // Same as SetContentFromBitset where "this" and "other" have the same size.
  template <typename OtherIndexType>
  void SetContentFromBitsetOfSameSize(const Bitset64<OtherIndexType>& other) {
    DCHECK_EQ(Value(size()), other.Value(other.size()));
    memcpy(data_.data(), other.data_.data(), data_.size() * sizeof(uint64));
  }

  // Sets "this" to be the intersection of "this" and "other". The
  // bitsets do not have to be the same size. If other is smaller, all
  // the higher order bits are assumed to be 0.
  void Intersection(const Bitset64<IndexType>& other) {
    const int min_size = std::min(data_.size(), other.data_.size());
    for (int i = 0; i < min_size; ++i) {
      data_[i] &= other.data_[i];
    }
    for (int i = min_size; i < data_.size(); ++i) {
      data_[i] = 0;
    }
  }

  // Sets "this" to be the union of "this" and "other". The
  // bitsets do not have to be the same size. If other is smaller, all
  // the higher order bits are assumed to be 0.
  void Union(const Bitset64<IndexType>& other) {
    const int min_size = std::min(data_.size(), other.data_.size());
    for (int i = 0; i < min_size; ++i) {
      data_[i] |= other.data_[i];
    }
  }

  // Class to iterate over the bit positions at 1 of a Bitset64.
  //
  // IMPORTANT: Because the iterator "caches" the current uint64 bucket, this
  // will probably not do what you want if Bitset64 is modified while iterating.
  class Iterator {
   public:
    explicit Iterator(const Bitset64& data_)
        : bitset_(data_), index_(0), base_index_(0), current_(0) {
      if (bitset_.data_.empty()) {
        index_ = -1;
      } else {
        current_ = bitset_.data_[0];
        Next();
      }
    }

    // Returns true if the Iterator is at a valid position.
    bool Ok() const { return index_ != -1; }

    // Returns the current position of the iterator.
    IndexType Index() const {
      DCHECK(Ok());
      return IndexType(index_);
    }

    // Moves the iterator the the next position at 1 of the Bitset64.
    void Next() {
      DCHECK(Ok());
      if (current_ == 0) {
        int bucket = BitOffset64(base_index_);
        const int size = bitset_.data_.size();
        do {
          bucket++;
        } while (bucket < size && bitset_.data_[bucket] == 0);
        if (bucket == size) {
          index_ = -1;
          return;
        }
        current_ = bitset_.data_[bucket];
        base_index_ = BitShift64(bucket);
      }

      // Computes the index and clear the least significant bit of current_.
      index_ = base_index_ + LeastSignificantBitPosition64(current_);
      current_ &= current_ - 1;
    }

    // STL version of the functions above to support range-based "for" loop.
    Iterator(const Bitset64& data_, bool at_end)
        : bitset_(data_), index_(0), base_index_(0), current_(0) {
      if (at_end || bitset_.data_.empty()) {
        index_ = -1;
      } else {
        current_ = bitset_.data_[0];
        Next();
      }
    }
    bool operator!=(const Iterator& other) const {
      return index_ != other.index_;
    }
    IndexType operator*() const { return IndexType(index_); }
    void operator++() { Next(); }

   private:
    const Bitset64& bitset_;
    int index_;
    int base_index_;
    uint64 current_;
  };

  // Allows range-based "for" loop on the non-zero positions:
  //   for (const IndexType index : bitset) {}
  // instead of:
  //   for (Bitset64<IndexType>::Iterator it(bitset); it.Ok(); it.Next()) {
  //     const IndexType index = it.Index();
  Iterator begin() const { return Iterator(*this); }
  Iterator end() const { return end_; }

  // Cryptic function!
  // This is just an optimized version of a given piece of code and has probably
  // little general use. Sets the bit at position i to the result of
  // (other1[i] && use1) XOR (other2[i] && use2).
  void SetBitFromOtherBitSets(IndexType i, const Bitset64<IndexType>& other1,
                              uint64 use1, const Bitset64<IndexType>& other2,
                              uint64 use2) {
    DCHECK_EQ(data_.size(), other1.data_.size());
    DCHECK_EQ(data_.size(), other2.data_.size());
    DCHECK(use1 == 0 || use1 == 1);
    DCHECK(use2 == 0 || use2 == 1);
    const int bucket = BitOffset64(Value(i));
    const int pos = BitPos64(Value(i));
    data_[bucket] ^= ((1ull << pos) & data_[bucket]) ^
                     ((use1 << pos) & other1.data_[bucket]) ^
                     ((use2 << pos) & other2.data_[bucket]);
  }

  // Returns a 0/1 std::string representing the bitset.
  std::string DebugString() const {
    std::string output;
    for (IndexType i(0); i < size(); ++i) {
      output += IsSet(i) ? "1" : "0";
    }
    return output;
  }

 private:
  // Returns the value of the index type.
  // This function is specialized below to work with IntType and int64.
  int64 Value(IndexType input) const;

  IndexType size_;
  std::vector<uint64> data_;

  // It is faster to store the end() Iterator than to recompute it every time.
  // Note that we cannot do the same for begin().
  const Iterator end_;

  template <class OtherIndexType>
  friend class Bitset64;
  DISALLOW_COPY_AND_ASSIGN(Bitset64);
};

// Specialized version of Bitset64 that allows to query the last bit set more
// efficiently.
class BitQueue64 {
 public:
  BitQueue64() : size_(), top_(-1), data_() {}
  explicit BitQueue64(int size)
      : size_(size), top_(-1), data_(BitLength64(size), 0) {}

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
    uint64 bucket = data_[bucket_index] &= ~OneBit64(BitPos64(top_));
    while (!bucket) {
      if (bucket_index == 0) {
        top_ = -1;
        return;
      }
      bucket = data_[--bucket_index];
    }

    // Note(user): I experimented with reversing the bit order in a bucket to
    // use LeastSignificantBitPosition64() and it is only slightly faster at the
    // cost of a lower Set() speed. So I prefered this version.
    top_ = static_cast<int>(BitShift64(bucket_index) +
                            MostSignificantBitPosition64(bucket));
  }

 private:
  int size_;
  int top_;
  std::vector<uint64> data_;
  DISALLOW_COPY_AND_ASSIGN(BitQueue64);
};

// The specialization of Value() for IntType and int64.
template <typename IntType>
inline int64 Bitset64<IntType>::Value(IntType input) const {
  DCHECK_GE(input.value(), 0);
  return input.value();
}
template <>
inline int64 Bitset64<int64>::Value(int64 input) const {
  DCHECK_GE(input, 0);
  return input;
}

// A simple utility class to set/unset integer in a range [0, size).
// This is optimized for sparsity.
template <typename IntegerType = int64>
class SparseBitset {
 public:
  SparseBitset() {}
  explicit SparseBitset(IntegerType size) : bitset_(size) {}
  IntegerType size() const { return bitset_.size(); }
  void SparseClearAll() {
    for (const IntegerType i : to_clear_) bitset_.ClearBucket(i);
    to_clear_.clear();
  }
  void ClearAll() {
    bitset_.ClearAll();
    to_clear_.clear();
  }
  void ClearAndResize(IntegerType size) {
    // As of 19/03/2014, experiments show that this is a reasonable threshold.
    const int kSparseThreshold = 300;
    if (to_clear_.size() * kSparseThreshold < size) {
      SparseClearAll();
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
    if (DEBUG_MODE) {
      for (IntegerType index : to_clear_) CHECK(!bitset_[index]);
    }
    to_clear_.clear();
  }

 private:
  Bitset64<IntegerType> bitset_;
  std::vector<IntegerType> to_clear_;
  DISALLOW_COPY_AND_ASSIGN(SparseBitset);
};

}  // namespace operations_research

#endif  // OR_TOOLS_UTIL_BITSET_H_
