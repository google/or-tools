// Copyright 2010-2017 Google
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

#include "ortools/base/commandlineflags.h"
#include "ortools/base/logging.h"

DEFINE_int32(bitset_small_bitset_count, 8,
             "threshold to count bits with buckets");

namespace operations_research {

// ---------- Bit Operations ----------

#define BIT_COUNT_RANGE(size, zero)                                           \
  uint##size BitCountRange##size(const uint##size* const bits,                \
                                 uint##size start, uint##size end) {          \
    if (end - start > FLAGS_bitset_small_bitset_count) {                      \
      const int offset_start = BitOffset##size(start);                        \
      const int pos_start = BitPos##size(start);                              \
      const int offset_end = BitOffset##size(end);                            \
      const int pos_end = BitPos##size(end);                                  \
      if (offset_end == offset_start) {                                       \
        return BitCount##size(bits[offset_start] &                            \
                              OneRange##size(pos_start, pos_end));            \
      } else {                                                                \
        uint##size bit_count = zero;                                          \
        bit_count +=                                                          \
            BitCount##size(bits[offset_start] & IntervalUp##size(pos_start)); \
        for (int offset = offset_start + 1; offset < offset_end; ++offset) {  \
          bit_count += BitCount##size(bits[offset]);                          \
        }                                                                     \
        bit_count +=                                                          \
            BitCount##size(bits[offset_end] & IntervalDown##size(pos_end));   \
        return bit_count;                                                     \
      }                                                                       \
    } else {                                                                  \
      uint##size bit_count = zero;                                            \
      for (uint##size i = start; i <= end; ++i) {                             \
        bit_count += IsBitSet##size(bits, i);                                 \
      }                                                                       \
      return bit_count;                                                       \
    }                                                                         \
  }

BIT_COUNT_RANGE(64, GG_ULONGLONG(0))
BIT_COUNT_RANGE(32, 0U)

#undef BIT_COUNT_RANGE

#define IS_EMPTY_RANGE(size)                                               \
  bool IsEmptyRange##size(const uint##size* const bits, uint##size start,  \
                          uint##size end) {                                \
    const int offset_start = BitOffset##size(start);                       \
    const int pos_start = BitPos##size(start);                             \
    const int offset_end = BitOffset##size(end);                           \
    const int pos_end = BitPos##size(end);                                 \
    if (offset_end == offset_start) {                                      \
      if (bits[offset_start] & OneRange##size(pos_start, pos_end)) {       \
        return false;                                                      \
      }                                                                    \
    } else {                                                               \
      if (bits[offset_start] & IntervalUp##size(pos_start)) {              \
        return false;                                                      \
      }                                                                    \
      for (int offset = offset_start + 1; offset < offset_end; ++offset) { \
        if (bits[offset]) {                                                \
          return false;                                                    \
        }                                                                  \
      }                                                                    \
      if (bits[offset_end] & IntervalDown##size(pos_end)) {                \
        return false;                                                      \
      }                                                                    \
    }                                                                      \
    return true;                                                           \
  }

IS_EMPTY_RANGE(64)
IS_EMPTY_RANGE(32)

#undef IS_EMPTY_RANGE

#define LEAST_SIGNIFCANT_BIT_POSITION(size)                                  \
  int##size LeastSignificantBitPosition##size(                               \
      const uint##size* const bits, uint##size start, uint##size end) {      \
    DCHECK_LE(start, end);                                                   \
    if (IsBitSet##size(bits, start)) {                                       \
      return start;                                                          \
    }                                                                        \
    const int offset_start = BitOffset##size(start);                         \
    const int offset_end = BitOffset##size(end);                             \
    const int pos_start = BitPos##size(start);                               \
    if (offset_start == offset_end) {                                        \
      const int pos_end = BitPos##size(end);                                 \
      const uint##size active_range =                                        \
          bits[offset_start] & OneRange##size(pos_start, pos_end);           \
      if (active_range) {                                                    \
        return BitShift##size(offset_start) +                                \
               LeastSignificantBitPosition##size(active_range);              \
      }                                                                      \
      return -1;                                                             \
    } else {                                                                 \
      const uint##size start_mask =                                          \
          bits[offset_start] & IntervalUp##size(pos_start);                  \
      if (start_mask) {                                                      \
        return BitShift##size(offset_start) +                                \
               LeastSignificantBitPosition##size(start_mask);                \
      } else {                                                               \
        for (int offset = offset_start + 1; offset < offset_end; ++offset) { \
          if (bits[offset]) {                                                \
            return BitShift##size(offset) +                                  \
                   LeastSignificantBitPosition##size(bits[offset]);          \
          }                                                                  \
        }                                                                    \
        const int pos_end = BitPos##size(end);                               \
        const uint##size active_range =                                      \
            bits[offset_end] & IntervalDown##size(pos_end);                  \
        if (active_range) {                                                  \
          return BitShift##size(offset_end) +                                \
                 LeastSignificantBitPosition##size(active_range);            \
        } else {                                                             \
          return -1;                                                         \
        }                                                                    \
      }                                                                      \
    }                                                                        \
  }

LEAST_SIGNIFCANT_BIT_POSITION(64)
LEAST_SIGNIFCANT_BIT_POSITION(32)

#undef LEAST_SIGNIFCANT_BIT_POSITION

#define MOST_SIGNIFICANT_BIT_POSITION(size)                                  \
  int##size MostSignificantBitPosition##size(                                \
      const uint##size* const bits, uint##size start, uint##size end) {      \
    DCHECK_GE(end, start);                                                   \
    if (IsBitSet##size(bits, end)) {                                         \
      return end;                                                            \
    }                                                                        \
    const int offset_start = BitOffset##size(start);                         \
    const int offset_end = BitOffset##size(end);                             \
    const int pos_end = BitPos##size(end);                                   \
    if (offset_start == offset_end) {                                        \
      const int pos_start = BitPos##size(start);                             \
      const uint##size active_range =                                        \
          bits[offset_start] & OneRange##size(pos_start, pos_end);           \
      if (active_range) {                                                    \
        return BitShift##size(offset_end) +                                  \
               MostSignificantBitPosition##size(active_range);               \
      } else {                                                               \
        return -1;                                                           \
      }                                                                      \
    } else {                                                                 \
      const uint##size end_mask =                                            \
          bits[offset_end] & IntervalDown##size(pos_end);                    \
      if (end_mask) {                                                        \
        return BitShift##size(offset_end) +                                  \
               MostSignificantBitPosition##size(end_mask);                   \
      } else {                                                               \
        for (int offset = offset_end - 1; offset > offset_start; --offset) { \
          if (bits[offset]) {                                                \
            return BitShift##size(offset) +                                  \
                   MostSignificantBitPosition##size(bits[offset]);           \
          }                                                                  \
        }                                                                    \
        const int pos_start = BitPos##size(start);                           \
        const uint##size active_range =                                      \
            bits[offset_start] & IntervalUp##size(pos_start);                \
        if (active_range) {                                                  \
          return BitShift##size(offset_start) +                              \
                 MostSignificantBitPosition##size(active_range);             \
        } else {                                                             \
          return -1;                                                         \
        }                                                                    \
      }                                                                      \
    }                                                                        \
  }

MOST_SIGNIFICANT_BIT_POSITION(64)
MOST_SIGNIFICANT_BIT_POSITION(32)

#undef MOST_SIGNIFICANT_BIT_POSITION

#define UNSAFE_LEAST_SIGNIFICANT_BIT_POSITION(size)                       \
  int##size UnsafeLeastSignificantBitPosition##size(                      \
      const uint##size* const bits, uint##size start, uint##size end) {   \
    DCHECK_LE(start, end);                                                \
    DCHECK(IsBitSet##size(bits, end));                                    \
    if (IsBitSet##size(bits, start)) {                                    \
      return start;                                                       \
    }                                                                     \
    const int offset_start = BitOffset##size(start);                      \
    const int offset_end = BitOffset##size(end);                          \
    const int pos_start = BitPos##size(start);                            \
    const uint##size start_mask =                                         \
        bits[offset_start] & IntervalUp##size(pos_start);                 \
    if (start_mask) {                                                     \
      return BitShift##size(offset_start) +                               \
             LeastSignificantBitPosition##size(start_mask);               \
    }                                                                     \
    for (int offset = offset_start + 1; offset <= offset_end; ++offset) { \
      if (bits[offset]) {                                                 \
        return BitShift##size(offset) +                                   \
               LeastSignificantBitPosition##size(bits[offset]);           \
      }                                                                   \
    }                                                                     \
    return -1;                                                            \
  }

UNSAFE_LEAST_SIGNIFICANT_BIT_POSITION(64)
UNSAFE_LEAST_SIGNIFICANT_BIT_POSITION(32)

#undef UNSAFE_LEAST_SIGNIFICANT_BIT_POSITION

#define UNSAFE_MOST_SIGNIFICANT_BIT_POSITION(size)                        \
  int##size UnsafeMostSignificantBitPosition##size(                       \
      const uint##size* const bits, uint##size start, uint##size end) {   \
    DCHECK_GE(end, start);                                                \
    DCHECK(IsBitSet##size(bits, start));                                  \
    if (IsBitSet##size(bits, end)) {                                      \
      return end;                                                         \
    }                                                                     \
    const int offset_start = BitOffset##size(start);                      \
    const int offset_end = BitOffset##size(end);                          \
    const int pos_end = BitPos##size(end);                                \
    const uint##size end_mask =                                           \
        bits[offset_end] & IntervalDown##size(pos_end);                   \
    if (end_mask) {                                                       \
      return BitShift##size(offset_end) +                                 \
             MostSignificantBitPosition##size(end_mask);                  \
    }                                                                     \
    for (int offset = offset_end - 1; offset >= offset_start; --offset) { \
      if (bits[offset]) {                                                 \
        return BitShift##size(offset) +                                   \
               MostSignificantBitPosition##size(bits[offset]);            \
      }                                                                   \
    }                                                                     \
    return -1;                                                            \
  }

UNSAFE_MOST_SIGNIFICANT_BIT_POSITION(64)
UNSAFE_MOST_SIGNIFICANT_BIT_POSITION(32)

#undef UNSAFE_MOST_SIGNIFICANT_BIT_POSITION

}  // namespace operations_research
