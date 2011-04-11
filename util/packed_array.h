// Copyright 2010-2011 Google
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

#ifndef UTIL_PACKED_ARRAY_H_
#define UTIL_PACKED_ARRAY_H_

#if defined(__APPLE__) && defined(__GNUC__)
#include <machine/endian.h>
#elif !defined(_MSC_VER)
#include <endian.h>
#endif
#include <limits.h>
#include <stdio.h>
#include <limits>
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/scoped_ptr.h"

// An array class for storing arrays of signed integers on NumBytes bytes,
// The range of indices is specified at the construction of the object.
// The minimum and maximum indices are inclusive.
// Think of the Pascal syntax array[min_index..max_index] of ...
// For example PackedArray<5>(-100000,100000) will store 200001 signed
// integers on 5 bytes or 40 bits, and the possible range of indices will be
// -100000..100000.
//
// There is no penalty for using this class for integer sizes of 1,2,4,8 bytes.
// For other sizes the write time penalty ranges from 20% to 100% (for a 7-byte
// integer.) The read time penalty is about 50% for integer sizes different
// from 1,2,4,8 bytes.
//
// WARNING: The implementation of this class (member functions Set and Value)
// assumes the underlying architecture is little-endian.
// TODO(user): make the implementation big-endian compatible.

#if __BYTE_ORDER != __LITTLE_ENDIAN
#error "The implementation of PackedArray assumes a little-endian architecture."
#endif

namespace operations_research {

typedef unsigned char byte;

template<class T, int NumBytes> class PackedArrayAllocator {
 public:
  PackedArrayAllocator()
      : base_(NULL),
        min_index_(0),
        max_index_(0),
        size_in_bytes_(0),
        storage_() {}
  // Reserves memory for new minimum and new maximum indices.
  // Never shrinks the memory allocated.
  void Reserve(int64 new_min_index, int64 new_max_index) {
    DCHECK_LE(new_min_index, new_max_index);
    if (base_ != NULL
        && new_min_index >= min_index_
        && new_max_index <= max_index_) {
      return;
    }
    DCHECK(base_ == NULL || new_min_index <= min_index_);
    DCHECK(base_ == NULL || new_max_index >= max_index_);
    const uint64 max_uint64 = std::numeric_limits<uint64>::max();
    const uint64 new_size = new_max_index - new_min_index + 1;
    DCHECK_GT(max_uint64 / NumBytes, new_size);
    // We need to pad the array to allow reading the last element as an int64.
    const uint64 new_size_in_bytes = new_size * NumBytes
                                   + sizeof(int64) - NumBytes; // NOLINT
    byte* new_storage = new byte[new_size_in_bytes];

    byte* const new_base = new_storage - new_min_index * NumBytes;
    if (base_ != NULL) {
      byte* const destination = new_base + min_index_ * NumBytes;
      memcpy(destination, storage_.get(), size_in_bytes_);
    }

    base_ = reinterpret_cast<T*>(new_base);
    min_index_ = new_min_index;
    max_index_ = new_max_index;
    size_in_bytes_ = new_size_in_bytes;
    storage_.reset(reinterpret_cast<T*>(new_storage));
  }

  int64 min_index() const { return min_index_; }

  int64 max_index() const { return max_index_; }

  T* Base() const {
    DCHECK(base_ != NULL);
    return base_;
  }

 private:
  // Pointer to the element indexed by zero in the array.
  T*                 base_;

  // Minimum index for the array.
  int64              min_index_;

  // Maximum index for the array.
  int64              max_index_;

  // The size of the array in bytes.
  uint64             size_in_bytes_;

  // Storage memory for the array.
  scoped_array<T> storage_;
};

template<int NumBytes> class PackedArray {
 public:
  PackedArray() : allocator_() {}

  // The constructor for PackedArray takes a mininum index and a maximum index.
  // These can be positive or negative, and the value for minimum_index
  // and for maximun_index can be set and read (i.e. the bounds are inclusive.)
  PackedArray(int64 min_index, int64 max_index) : allocator_() {
    Reserve(min_index, max_index);
  }

  ~PackedArray() {}

  // Returns the minimum possible index for the array.
  int64 min_index() const { return allocator_.min_index(); }

  // Returns the maximum possible index for the array.
  int64 max_index() const { return allocator_.max_index(); }

  // Returns the value stored at index.
  // This assumes that the underlying architecture is little-endian. In
  // particular, we select the value to return from the low-address NumBytes
  // bytes of the int64 beginning at the location determined by the given index.
  // To extract those NumBytes from the int64 loaded from memory, we choose the
  // bytes of low arithmetic significance. Code for a big-endian system would
  // need to choose the bytes of high arithmetic significance.
  int64 Value(int64 index) const {
    DCHECK_LE(allocator_.min_index(), index);
    DCHECK_GE(allocator_.max_index(), index);
    int64 value =
        *reinterpret_cast<int64*>(allocator_.Base() + index * NumBytes);
    const int shift = (sizeof(value) - NumBytes) * CHAR_BIT;
    value <<= shift;  // These two lines are
    value >>= shift;  // for sign extension.
    return value;
  }

#if !defined(SWIG)
  // Shortcut for returning the value stored at index.
  int64 operator[](int64 index) const {
    return Value(index);
  }
#endif

// Let n by the number of bytes (i.e. sizeof) of type.
// PACKED_ARRAY_WRITE_IF_ENOUGH_BYTES_AND_ADVANCE writes the first n lowest
// significant bytes of value at address. It then shifts value by n bytes to
// the right, and advances address by n bytes.
// This assumes that the underlying architecture is little-endian, since
// we write the lowest n bytes for value. Code for a big-endian system would
// need to write the bytes of high arithmetic significance.
#define PACKED_ARRAY_WRITE_IF_ENOUGH_BYTES_AND_ADVANCE(type, address, value)  \
{                                                                             \
  const int64 kSize = sizeof(type);                                           \
  if (NumBytes & kSize) {                                                     \
    const int64 k1 = GG_LONGLONG(1);                                          \
    const int64 kSizeInBits = sizeof(type) * CHAR_BIT;                        \
    const int64 kMask = (k1 << kSizeInBits) - 1;                              \
    reinterpret_cast<type*>(address)[0] = value & kMask;                      \
    value >>= kSizeInBits;                                                    \
    address += kSize;                                                         \
  }                                                                           \
}

  // Sets to value the content of the array at index.
  void Set(int64 index, int64 value) {
    DCHECK_LE(allocator_.min_index(), index);
    DCHECK_GE(allocator_.max_index(), index);
    DCHECK_LE(kMinInteger, value);
    DCHECK_GE(kMaxInteger, value);
    byte* current = allocator_.Base() + index * NumBytes;
    PACKED_ARRAY_WRITE_IF_ENOUGH_BYTES_AND_ADVANCE(int32, current, value);
    PACKED_ARRAY_WRITE_IF_ENOUGH_BYTES_AND_ADVANCE(int16, current, value);
    PACKED_ARRAY_WRITE_IF_ENOUGH_BYTES_AND_ADVANCE(byte, current, value);
  }

  // Reserves memory for new minimum and new maximum indices.
  // Never shrinks the memory allocated.
  void Reserve(int64 new_min_index, int64 new_max_index) {
    allocator_.Reserve(new_min_index, new_max_index);
  }

  // Sets all the elements in the array to value. Set is bypassed to maximize
  // performance.
  void Assign(int64 value) {
    DCHECK_LE(kMinInteger, value);
    DCHECK_GE(kMaxInteger, value);
    const byte* end = allocator_.Base() + allocator_.max_index() * NumBytes;
    byte* current = allocator_.Base() + allocator_.min_index() * NumBytes;
    while (current <= end) {
      int64 v = value;  // v is going to be modified by the following macro.
      PACKED_ARRAY_WRITE_IF_ENOUGH_BYTES_AND_ADVANCE(int32, current, v);
      PACKED_ARRAY_WRITE_IF_ENOUGH_BYTES_AND_ADVANCE(int16, current, v);
      PACKED_ARRAY_WRITE_IF_ENOUGH_BYTES_AND_ADVANCE(byte, current, v);
    }
  }
#undef PACKED_ARRAY_WRITE_IF_ENOUGH_BYTES_AND_ADVANCE

 private:
  // The bitmask corresponding to all the bits in Numbytes bytes set.
  static const uint64 kBitMask = (GG_ULONGLONG(1) << (CHAR_BIT * NumBytes)) - 1;

  // The maximum signed integer representable with NumBytes bytes.
  static const int64 kMaxInteger = kBitMask >> 1;

  // The minimum signed integer representable with NumBytes bytes.
  static const int64 kMinInteger = ~kMaxInteger;

  PackedArrayAllocator<byte, NumBytes> allocator_;
};

// A specialization of the template for int32 (NumBytes = 4.)
// TODO(user): also make a specialization for int16 if needed(?).

template<> class PackedArray<4> {
 public:
  PackedArray() : allocator_() {}

  PackedArray(int64 min_index, int64 max_index) : allocator_() {
    Reserve(min_index, max_index);
  }

  int64 min_index() const { return allocator_.min_index(); }

  int64 max_index() const { return allocator_.max_index(); }

  // Returns the value stored at index.
  int64 Value(int64 index) const {
    DCHECK_LE(allocator_.min_index(), index);
    DCHECK_GE(allocator_.max_index(), index);
    return allocator_.Base()[index];
  }

#if !defined(SWIG)
  // Shortcut for returning the value stored at index.
  int64 operator[](int64 index) const {
    return Value(index);
  }
#endif

  // Sets to value the content of the array at index.
  void Set(int64 index, int64 value) {
    DCHECK_LE(allocator_.min_index(), index);
    DCHECK_GE(allocator_.max_index(), index);
    DCHECK_LE(std::numeric_limits<int32>::min(), value);
    DCHECK_GE(std::numeric_limits<int32>::max(), value);
    allocator_.Base()[index] = value;
  }

  // Reserves memory for new minimum and new maximum indices.
  // Never shrinks the memory allocated.
  void Reserve(int64 new_min_index, int64 new_max_index) {
    allocator_.Reserve(new_min_index, new_max_index);
  }

  // Sets all the elements in the array to value.
  void Assign(int64 value) {
    DCHECK_LE(std::numeric_limits<int32>::min(), value);
    DCHECK_GE(std::numeric_limits<int32>::max(), value);
    int32 v = value;  // Do the type conversion only once.
    const int32* end = allocator_.Base() + allocator_.max_index();
    for (int32* current = allocator_.Base() + allocator_.min_index();
         current <= end;
         ++current) {
      *current = v;
    }
  }

 private:
  PackedArrayAllocator<int32, 4> allocator_;
};

//
// A specialization of the template for int64 (NumBytes = 8.)
// There is some duplicated code with PackedArray<4>.
//
template<> class PackedArray<8> {
 public:
  PackedArray() : allocator_() {}

  PackedArray(int64 min_index, int64 max_index) : allocator_() {
    Reserve(min_index, max_index);
  }

  int64 min_index() const { return allocator_.min_index(); }

  int64 max_index() const { return allocator_.max_index(); }

  // Returns the value stored at index.
  int64 Value(int64 index) const {
    DCHECK_LE(allocator_.min_index(), index);
    DCHECK_GE(allocator_.max_index(), index);
    return allocator_.Base()[index];
  }

#if !defined(SWIG)
  // Shortcut for returning the value stored at index.
  int64 operator[](int64 index) const {
    return Value(index);
  }
#endif

  // Sets to value the content of the array at index.
  void Set(int64 index, int64 value) {
    DCHECK_LE(allocator_.min_index(), index);
    DCHECK_GE(allocator_.max_index(), index);
    allocator_.Base()[index] = value;
  }

  // Reserves memory for new minimum and new maximum indices.
  // Never shrinks the memory allocated.
  void Reserve(int64 new_min_index, int64 new_max_index) {
    allocator_.Reserve(new_min_index, new_max_index);
  }

  // Sets all the elements in the array to value.
  void Assign(int64 value) {
    const int64* end = allocator_.Base() + allocator_.max_index();
    for (int64* current = allocator_.Base() + allocator_.min_index();
         current <= end;
         ++current) {
      *current = value;
    }
  }

 private:
  PackedArrayAllocator<int64, 8> allocator_;
};

// Shorthands for all the types of PackedArray's.
typedef PackedArray<1> Int8PackedArray;
typedef PackedArray<2> Int16PackedArray;
typedef PackedArray<3> Int24PackedArray;
typedef PackedArray<4> Int32PackedArray;
typedef PackedArray<5> Int40PackedArray;
typedef PackedArray<6> Int48PackedArray;
typedef PackedArray<7> Int56PackedArray;
typedef PackedArray<8> Int64PackedArray;
}  // namespace operations_research

#endif  // UTIL_PACKED_ARRAY_H_
