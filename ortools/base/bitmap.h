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

#ifndef OR_TOOLS_BASE_BITMAP_H_
#define OR_TOOLS_BASE_BITMAP_H_

#include <cassert>
#include <cstring>

#include "ortools/base/integral_types.h"

namespace operations_research {
namespace internal {
inline uint64_t OneBit64(int pos) { return uint64_t{1} << pos; }
inline uint64_t BitPos64(uint64_t pos) { return (pos & 63); }
inline uint64_t BitOffset64(uint64_t pos) { return (pos >> 6); }
inline uint64_t BitLength64(uint64_t size) { return ((size + 63) >> 6); }
inline bool IsBitSet64(const uint64_t* const bitset, uint64_t pos) {
  return (bitset[BitOffset64(pos)] & OneBit64(BitPos64(pos)));
}
inline void SetBit64(uint64_t* const bitset, uint64_t pos) {
  bitset[BitOffset64(pos)] |= OneBit64(BitPos64(pos));
}
inline void ClearBit64(uint64_t* const bitset, uint64_t pos) {
  bitset[BitOffset64(pos)] &= ~OneBit64(BitPos64(pos));
}
}  // namespace internal

class Bitmap {
 public:
  // Constructor : This allocates on a uint32_t boundary.
  // fill: true = initialize with 1's, false = initialize with 0's.
  explicit Bitmap(uint32_t size, bool fill = false)
      : max_size_(size),
        array_size_(internal::BitLength64(size)),
        map_(new uint64_t[array_size_]) {
    // initialize all of the bits
    SetAll(fill);
  }

  // Destructor: clean up.
  ~Bitmap() { delete[] map_; }

  // Resizes the bitmap.
  // If size < bits(), the extra bits will be discarded.
  // If size > bits(), the extra bits will be filled with the fill value.
  void Resize(uint32_t size, bool fill = false);

  bool Get(uint32_t index) const {
    assert(max_size_ == 0 || index < max_size_);
    return internal::IsBitSet64(map_, index);
  }
  void Set(uint32_t index, bool value) {
    assert(max_size_ == 0 || index < max_size_);
    if (value) {
      internal::SetBit64(map_, index);
    } else {
      internal::ClearBit64(map_, index);
    }
  }

  // Sets all the bits to true or false
  void SetAll(bool value) {
    memset(map_, (value ? 0xFF : 0x00), array_size_ * sizeof(*map_));
  }

  // Clears all bits in the bitmap
  void Clear() { SetAll(false); }

 private:
  uint32_t max_size_;  // the upper bound of the bitmap
  uint32_t array_size_;
  uint64_t* map_;  // the bitmap
};

}  // namespace operations_research

#endif  // OR_TOOLS_BASE_BITMAP_H_
