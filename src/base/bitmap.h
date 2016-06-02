// Copyright 2010-2014 Google
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

#include <string.h>
#include "base/basictypes.h"
#include "util/bitset.h"

namespace operations_research {

class Bitmap {
 public:
  // Constructor : This allocates on a uint32 boundary.
  // fill: true = initialize with 1's, false = initialize with 0's.
  explicit Bitmap(uint32 size, bool fill = false)
      : max_size_(size),
        array_size_(BitLength64(size)),
        map_(new uint64[array_size_]) {
    // initialize all of the bits
    SetAll(fill);
  }

  // Destructor: clean up.
  ~Bitmap() { delete[] map_; }

  // Resizes the bitmap.
  // If size < bits(), the extra bits will be discarded.
  // If size > bits(), the extra bits will be filled with the fill value.
  void Resize(uint32 size, bool fill = false);

  bool Get(uint32 index) const {
    assert(max_size_ == 0 || index < max_size_);
    return IsBitSet64(map_, index);
  }
  void Set(uint32 index, bool value) {
    assert(max_size_ == 0 || index < max_size_);
    if (value) {
      SetBit64(map_, index);
    } else {
      ClearBit64(map_, index);
    }
  }

  // Sets all the bits to true or false
  void SetAll(bool value) {
    memset(map_, (value ? 0xFF : 0x00), array_size_ * sizeof(*map_));
  }

  // Clears all bits in the bitmap
  void Clear() { SetAll(false); }

 private:
  uint32 max_size_;  // the upper bound of the bitmap
  uint32 array_size_;
  uint64* map_;  // the bitmap
};

}  // namespace operations_research

#endif  // OR_TOOLS_BASE_BITMAP_H_
