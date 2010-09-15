// Copyright 2010 Google
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

#include "base/bitmap.h"

#include <algorithm>

#include "base/basictypes.h"

namespace operations_research {

void Bitmap::Resize(uint32 size, bool fill) {
  const uint32 new_array_size = BitLength64(size);
  if (size  < array_size_) {
    array_size_ = size;
  } else if (size > array_size_) {
    if (new_array_size > array_size_) {
      const uint32 old_maxsize = maxsize_;
      const uint32 old_array_size = array_size_;
      array_size_ = new_array_size;
      maxsize_ = size;
      uint64* new_map = new uint64[array_size_];
      memcpy(new_map, map_, array_size_ * sizeof(*map_));
      delete [] map_;
      map_ = new_map;
    }
    for (uint32 index = maxsize_; index < size; ++index) {
      Set(index, fill);
    }
    array_size_ = size;
  }
}
}  // namespace operations_research
