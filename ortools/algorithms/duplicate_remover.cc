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
  // We use vector<uint64_t> because using vector<bool> would be potentially
  // more expensive: writing in vector<bool> involves a read+write, and here we
  // clear whole 64-bit words at once.
  const absl::Span<uint64_t> tmp_mask = tmp_mask_;
  int num_unique_kept = -1;
  // Fast track for the leading portion without duplicates.
  while (++num_unique_kept < span.size()) {
    const int x = span[num_unique_kept];
    DCHECK_GE(x, 0);
    DCHECK_LT(x, tmp_mask.size() * 64);
    // Bit #i = Bit #(i modulo 64) of Word #(i / 64).
    const uint64_t mask = uint64_t{1} << (x & 63);  // Bit #(i modulo 64).
    const uint64_t word = tmp_mask[x >> 6];         // .. of Word #(i / 64).
    if (mask & word) break;                         // Already seen.
    tmp_mask[x >> 6] = word | mask;
  }
  // The next portion is exactly the same, except that now we have to shift
  // the elements that we're keeping, making it slightly slower.
  for (int i = num_unique_kept + 1; i < span.size(); ++i) {
    const int x = span[i];
    DCHECK_GE(x, 0);
    DCHECK_LT(x, tmp_mask.size() * 64);
    const uint64_t mask = uint64_t{1} << (x & 63);
    const uint64_t word = tmp_mask[x >> 6];
    if (mask & word) continue;  // Already seen.
    tmp_mask[x >> 6] = mask | word;
    span[num_unique_kept++] = x;  // Keep x=[i], at its new (shifted) position.
  }
  span.remove_suffix(span.size() - num_unique_kept);
  // Clear the bit mask.
  for (int x : span) tmp_mask[x >> 6] = 0;
  return num_unique_kept;
}

}  // namespace operations_research
