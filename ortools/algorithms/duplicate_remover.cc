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

#include "ortools/algorithms/duplicate_remover.h"

#include <cstddef>
#include <cstdint>

#include "absl/log/check.h"
#include "absl/types/span.h"

namespace operations_research {

size_t DenseIntDuplicateRemover::RemoveDuplicatesInternal(
    absl::Span<int> span) {
  // We use vector<uint8_t> because using vector<bool> would be potentially more
  // expensive: writing in vector<bool> involves a read+write, and here we're
  // directly writing.
  int num_unique_kept = -1;
  // Fast track for the leading portion without duplicates.
  while (++num_unique_kept < span.size()) {
    const int x = span[num_unique_kept];
    DCHECK_GE(x, 0);
    DCHECK_LT(x, tmp_mask_.size() * 8);
    // Bit #i = Bit #(i modulo 8) of Byte #(i / 8).
    const uint8_t mask = 1u << (x & 7);      // Bit #(i modulo 8).
    const uint8_t byte = tmp_mask_[x >> 3];  // .. of Byte #(i / 8).
    if (mask & byte) break;                  // Already seen.
    tmp_mask_[x >> 3] = byte | mask;
  }
  // The next portion is exactly the same, except that now we have to shift
  // the elements that we're keeping, making it slightly slower.
  for (int i = num_unique_kept + 1; i < span.size(); ++i) {
    const int x = span[i];
    DCHECK_GE(x, 0);
    DCHECK_LT(x, tmp_mask_.size() * 8);
    const uint8_t mask = 1 << (x & 7);
    const uint8_t byte = tmp_mask_[x >> 3];
    if (mask & byte) continue;  // Already seen.
    tmp_mask_[x >> 3] = mask | byte;
    span[num_unique_kept++] = x;  // Keep x=[i], at its new (shifted) position.
  }
  span.remove_suffix(span.size() - num_unique_kept);
  // Clear the bit mask.
  for (int x : span) tmp_mask_[x >> 3] = 0;
  return num_unique_kept;
}

}  // namespace operations_research
