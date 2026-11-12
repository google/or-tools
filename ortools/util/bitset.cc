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

#include "ortools/util/bitset.h"

#include <bit>
#include <cstdint>

#include "absl/log/check.h"

namespace operations_research {

uint64_t BitCountRange64(const uint64_t* const bits, uint64_t start,
                         uint64_t end) {
  DCHECK_LE(start, end);
  uint64_t offset = BitOffset64(start);
  const uint64_t offset_end = BitOffset64(end);
  uint64_t bucket = bits[offset] & IntervalUp64(BitPos64(start));
  uint64_t bit_count = 0;
  while (offset < offset_end) {
    bit_count += std::popcount(bucket);
    bucket = bits[++offset];
  }
  bucket &= IntervalDown64(BitPos64(end));
  return bit_count + std::popcount(bucket);
}

bool IsEmptyRange64(const uint64_t* const bits, uint64_t start, uint64_t end) {
  DCHECK_LE(start, end);
  uint64_t offset = BitOffset64(start);
  const uint64_t offset_end = BitOffset64(end);
  uint64_t bucket = bits[offset] & IntervalUp64(BitPos64(start));
  while (bucket == 0 && offset < offset_end) {
    bucket = bits[++offset];
  }
  if (offset == offset_end) {
    bucket &= IntervalDown64(BitPos64(end));
  }
  return bucket == 0;
}

int64_t LeastSignificantBitPosition64(const uint64_t* const bits,
                                      uint64_t start, uint64_t end) {
  DCHECK_LE(start, end);
  uint64_t offset = BitOffset64(start);
  const uint64_t offset_end = BitOffset64(end);
  uint64_t bucket = bits[offset] & IntervalUp64(BitPos64(start));
  while (bucket == 0 && offset < offset_end) {
    bucket = bits[++offset];
  }
  if (offset == offset_end) {
    bucket &= IntervalDown64(BitPos64(end));
  }
  if (bucket == 0) return -1;
  return BitShift64(offset) + LeastSignificantBitPosition64(bucket);
}

int64_t MostSignificantBitPosition64(const uint64_t* const bits, uint64_t start,
                                     uint64_t end) {
  DCHECK_LE(start, end);
  const uint64_t offset_start = BitOffset64(start);
  uint64_t offset = BitOffset64(end);
  uint64_t bucket = bits[offset] & IntervalDown64(BitPos64(end));
  while (bucket == 0 && offset > offset_start) {
    bucket = bits[--offset];
  }
  if (offset == offset_start) {
    bucket &= IntervalUp64(BitPos64(start));
  }
  if (bucket == 0) return -1;
  return BitShift64(offset) + MostSignificantBitPosition64(bucket);
}

int64_t UnsafeLeastSignificantBitPosition64(const uint64_t* const bits,
                                            uint64_t start, uint64_t end) {
  DCHECK_LE(start, end);
  DCHECK(IsBitSet64(bits, end));
  uint64_t offset = BitOffset64(start);
  uint64_t bucket = bits[offset] & IntervalUp64(BitPos64(start));
  while (bucket == 0) {
    bucket = bits[++offset];
  }
  return BitShift64(offset) + LeastSignificantBitPosition64(bucket);
}

int64_t UnsafeMostSignificantBitPosition64(const uint64_t* const bits,
                                           uint64_t start, uint64_t end) {
  DCHECK_LE(start, end);
  DCHECK(IsBitSet64(bits, start));
  uint64_t offset = BitOffset64(end);
  uint64_t bucket = bits[offset] & IntervalDown64(BitPos64(end));
  while (bucket == 0) {
    bucket = bits[--offset];
  }
  return BitShift64(offset) + MostSignificantBitPosition64(bucket);
}

}  // namespace operations_research
