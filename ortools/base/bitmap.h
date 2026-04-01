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

#ifndef ORTOOLS_BASE_BITMAP_H_
#define ORTOOLS_BASE_BITMAP_H_

#include <bit>
#include <bitset>
#include <cassert>
#include <cstdint>
#include <utility>
#include <vector>

namespace operations_research {

class Bitmap {
  static constexpr uint32_t kBlockSize = 64;

  static std::pair<uint32_t, uint32_t> DivMod(uint32_t index) {
    return std::make_pair(index / kBlockSize, index % kBlockSize);
  }

  static uint32_t GetNumBlocks(uint32_t size) {
    static_assert(std::has_single_bit(kBlockSize));
    return (size + kBlockSize - 1) / kBlockSize;
  }

 public:
  // Constructor : This allocates on a uint32_t boundary.
  explicit Bitmap(uint32_t size) : bits_(size), map_(GetNumBlocks(size)) {}

  bool Get(uint32_t index) const {
    assert(bits_ == 0 || index < bits_);
    const auto [block, bit] = DivMod(index);
    return map_[block].test(bit);
  }

  // Sets the bit at the given index to the given value.
  void Set(uint32_t index) {
    assert(bits_ == 0 || index < bits_);
    const auto [block, bit] = DivMod(index);
    map_[block].set(bit);
  }

  // Clears all bits in the bitmap
  void Clear() {
    for (auto& bitset : map_) {
      bitset.reset();
    }
  }

 private:
  uint32_t bits_;                             // the upper bound of the bitmap
  std::vector<std::bitset<kBlockSize>> map_;  // the bitmap
};

}  // namespace operations_research

#endif  // ORTOOLS_BASE_BITMAP_H_
