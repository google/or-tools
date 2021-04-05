// Copyright 2010-2021 Google LLC
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

#include "ortools/base/bitmap.h"

#include <algorithm>

#include "ortools/base/basictypes.h"

namespace operations_research {

void Bitmap::Resize(uint32_t size, bool fill) {
  const uint32_t new_array_size = internal::BitLength64(size);
  const uint32_t old_max_size = max_size_;
  if (new_array_size <= array_size_) {
    max_size_ = size;
  } else {
    const uint32_t old_array_size = array_size_;
    array_size_ = new_array_size;
    max_size_ = size;
    uint64_t* new_map = new uint64_t[array_size_];
    memcpy(new_map, map_, old_array_size * sizeof(*map_));
    delete[] map_;
    map_ = new_map;
  }
  // TODO(user) : optimize next loop.
  for (uint32_t index = old_max_size; index < size; ++index) {
    Set(index, fill);
  }
}
}  // namespace operations_research
