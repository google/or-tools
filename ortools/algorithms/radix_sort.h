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

#ifndef OR_TOOLS_ALGORITHMS_RADIX_SORT_H_
#define OR_TOOLS_ALGORITHMS_RADIX_SORT_H_

// This can be MUCH faster than std::sort() on numerical arrays (int32_t, float,
// int64_t, double, ..), when the size is ≥8k:
//   ~10x faster on int32_t or float data
//   ~3-5x faster on int64_t or double data
//
// Unlike std::sort(), it uses extra, temporary buffers: the radix/count-sort
// counters, and a copy of the data, i.e. between 1x and 2x your input size.
//
// RadixSort() falls back to std::sort() for small sizes, so that you get
// the best performance in any case.
//
// CAVEAT: std::sort() is *very* fast when the array is almost-sorted, or
// almost reverse-sorted: in this case, RadixSort() can easily be much slower.
// But the worst-case performance of RadixSort() is much faster than the
// worst-case performance of std::sort().
// To be sure, you should benchmark your use case.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/base/casts.h"
#include "absl/base/log_severity.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/numeric/bits.h"
#include "absl/types/span.h"

namespace operations_research {

// Sorts an array of int, double, or other numeric types. Up to ~10x faster than
// std::sort() when size ≥ 8k: go/radix-sort-bench. See file-level comment.
template <typename T>
void RadixSort(
    absl::Span<T> values,
    // ADVANCED USAGE: if you're sorting nonnegative integers, and suspect that
    // their values use less bits than their full bit width, you may improve
    // performance by setting `num_bits` to a lower value, for example
    // NumBitsForZeroTo(max_value). It might even be faster to scan the values
    // once just to do that, e.g., RadixSort(values,
    // NumBitsForZeroTo(*absl::c_max_element(values)));
    int num_bits = sizeof(T) * 8);

template <typename T>
int NumBitsForZeroTo(T max_value);

// ADVANCED USAGE: For power users who know which radix_width or num_passes
// they need, possibly differing from the canonical values used by RadixSort().
template <typename T, int radix_width, int num_passes>
void RadixSortTpl(absl::Span<T> values);

// TODO(user): Support the user providing already-allocated memory buffers
//              for the radix counts and/or for the temporary vector<T> copy.

// _____________________________________________________________________________
// The rest of this .h is the implementation of the above templates.

namespace internal {
// to_uint<T> converts a numerical type T (int, int64_t, float, double, ...) to
// the unsigned integer of the same bit width.
template <typename T>
struct ToUInt : public std::make_unsigned<T> {};

template <>
struct ToUInt<double> {
  typedef uint64_t type;
};
template <>
struct ToUInt<float> {
  typedef uint32_t type;
};

template <typename T>
using to_uint = typename ToUInt<T>::type;
}  // namespace internal

// The internal template that does all the work.
template <typename T, int radix_width, int num_passes>
void RadixSortTpl(absl::Span<T> values) {
  // Internally we assume our values are unsigned integers. This works for both
  // signed integers and IEEE754 floating-point types, with various twists for
  // negative numbers. In particular, two nonnegative floats compare exactly as
  // their unsigned integer bitcast do.
  typedef internal::to_uint<T> U;

  // NOTE(user): We could support sizes > kint32max if needed. We use uint32_t
  // for sizes, instead of size_t, to spare memory for the large radix widths.
  // We could use uint64_t for sizes > 4G, but until we need it, just using
  // uint32_t is simpler. Using smaller types (uint16_t or uint8_t) for smaller
  // sizes was noticeably slower.
  DCHECK_LE(values.size(),
            static_cast<size_t>(std::numeric_limits<uint32_t>::max()));
  const uint32_t size = values.size();

  // Main Radix/Count-sort counters. Radix sort normally uses several passes,
  // but to speed things up, we compute all radix counters for all passes at
  // once in a single initial sweep over the data.
  //
  // count[] is actually a 2-dimensional array [num_passes][1 << radix_width],
  // flattened for performance and in a vector<> because it can be too big for
  // the stack.
  std::vector<uint32_t> count(num_passes << radix_width, 0);
  uint32_t* const count_ptr = count.data();

  // Perform the radix count all at once, in 'parallel' (the CPU should be able
  // to parallelize the inner loop).
  constexpr uint32_t kRadixMask = (1 << radix_width) - 1;
  for (const T value : values) {
    for (int p = 0; p < num_passes; ++p) {
      ++count_ptr[(p << radix_width) +
                  ((absl::bit_cast<U>(value) >> (radix_width * p)) &
                   kRadixMask)];
    }
  }

  // Convert the counts into offsets via a cumulative sum.
  uint32_t sum[num_passes] = {};
  for (int i = 0; i < (1 << radix_width); ++i) {
    // This inner loop should be parallelizable by the CPU.
    for (int p = 0; p < num_passes; ++p) {
      const uint32_t old_sum = sum[p];
      sum[p] += count_ptr[(p << radix_width) + i];
      count_ptr[(p << radix_width) + i] = old_sum;
    }
  }

  // FIRST-TIME READER: Skip this section, which is only for signed integers:
  // you can go back to it at the end.
  //
  // If T is signed, and if there were any negative numbers, we'll need to
  // account for that. For floating-point types, we do that at the end of this
  // function.
  // For integer types, fortunately, it's easy and fast to do it now: negative
  // numbers were treated as top-half values in the last radix pass. We can poll
  // the most significant count[] bucket corresponding to the min negative
  // number, immediately see if there were any negative numbers, and patch the
  // last count[] offsets in that case.
  // Number of bits of the radix in the last pass. E.g. if U is 32 bits,
  constexpr int kNumBitsInTopRadix =
      std::numeric_limits<U>::digits - (num_passes - 1) * radix_width;
  // TODO: remove the if constexpr so that compilation catches the bad cases.
  if constexpr (std::is_integral_v<T> && std::is_signed_v<T> &&
                kNumBitsInTopRadix > 0 && kNumBitsInTopRadix <= radix_width) {
    uint32_t* const last_pass_count =
        count_ptr + ((num_passes - 1) << radix_width);
    const uint32_t num_nonnegative_values =
        last_pass_count[1 << (kNumBitsInTopRadix - 1)];
    if (num_nonnegative_values != size) {
      // There are some negative values, and they're sorted last instead of
      // first, since we considered them as unsigned so far. E.g., with bytes:
      // 00000000, ..., 01111111, 10000000, ..., 11111111.
      // Fixing that is easy: we take the 10000000..11111111 chunks and shift
      // it before all the 00000000..01111111 ones.
      const uint32_t num_negative_values = size - num_nonnegative_values;
      for (int i = 0; i < (1 << (kNumBitsInTopRadix - 1)); ++i) {
        // Shift non-negatives by +num_negative_values...
        last_pass_count[i] += num_negative_values;
        // ... and negatives by -num_nonnegative_values.
        last_pass_count[i + (1 << (kNumBitsInTopRadix - 1))] -=
            num_nonnegative_values;
      }
    }
  }

  // Perform the radix sort, using a temporary buffer.
  std::vector<T> tmp(size);
  T* from = values.data();
  T* to = tmp.data();
  int radix = 0;
  for (int pass = 0; pass < num_passes; ++pass, radix += radix_width) {
    uint32_t* const cur_count_ptr = count_ptr + (pass << radix_width);
    const T* const from_end = from + size;
    for (T* ptr = from; ptr < from_end; ++ptr) {
      to[cur_count_ptr[(absl::bit_cast<U>(*ptr) >> radix) & kRadixMask]++] =
          *ptr;
    }
    std::swap(from, to);
  }

  // FIRST-TIME READER: Skip this section, which is only for negative floats.
  // We fix mis-sorted negative floating-point numbers here.
  if constexpr (!std::is_integral_v<T> && std::is_signed_v<T> &&
                kNumBitsInTopRadix > 0 && kNumBitsInTopRadix <= radix_width) {
    uint32_t* const last_pass_count =
        count_ptr + ((num_passes - 1) << radix_width);
    const uint32_t num_nonnegative_values =
        last_pass_count[(1 << (kNumBitsInTopRadix - 1)) - 1];
    if (num_nonnegative_values != size) {
      // Negative floating-point numbers are sorted exactly in the reverse
      // order. Unlike for integers, we need to std::reverse() them, and also
      // shift them back before the positive ones.
      const uint32_t num_negative_values = size - num_nonnegative_values;
      if constexpr (num_passes % 2) {
        // If we swapped an odd number of times, we're lucky: we don't need to
        // make an extra copy.
        std::memcpy(values.data() + num_negative_values, tmp.data(),
                    num_nonnegative_values * sizeof(T));
        // TODO(user): See if this is faster than memcpy + std::reverse().
        DCHECK_EQ(from, tmp.data());
        for (uint32_t i = 0; i < num_negative_values; ++i) {
          values[i] = from[size - 1 - i];  // from[] = tmp[]
        }
      } else {
        // We can't move + reverse in-place, so we need the temporary buffer.
        // First, we copy all negative numbers to the temporary buffer.
        std::memcpy(tmp.data(), values.data() + num_nonnegative_values,
                    num_negative_values * sizeof(T));
        // Then we shift the nonnegative.
        // TODO(user): See if memcpy everything + memcpy here is faster than
        // memmove().
        std::memmove(values.data() + num_negative_values, values.data(),
                     num_nonnegative_values * sizeof(T));
        DCHECK_EQ(to, tmp.data());
        for (uint32_t i = 0; i < num_negative_values; ++i) {
          values[i] = to[num_negative_values - 1 - i];  // to[] = tmp[].
        }
      }
      // If there were negative floats, we've done our work and are done. Else
      // we still may need to move the data from the temp buffer to 'values'.
      return;
    }
  }

  // If we swapped an odd number of times, copy tmp[] onto values[].
  if constexpr (num_passes % 2) {
    std::memcpy(values.data(), from, size * sizeof(T));
  }
}

template <typename T>
int NumBitsForZeroTo(T max_value) {
  if constexpr (!std::is_integral_v<T>) {
    return sizeof(T) * 8;
  } else {
    using U = std::make_unsigned_t<T>;
    DCHECK_GE(max_value, 0);
    return std::numeric_limits<U>::digits - absl::countl_zero<U>(max_value);
  }
}

#ifdef NDEBUG
const bool DEBUG_MODE = false;
#else
const bool DEBUG_MODE = true;
#endif

template <typename T>
void RadixSort(absl::Span<T> values, int num_bits) {
  // Debug-check that num_bits is valid w.r.t. the values given.
  if constexpr (DEBUG_MODE) {
    if constexpr (!std::is_integral_v<T>) {
      DCHECK_EQ(num_bits, sizeof(T) * 8);
    } else if (!values.empty()) {
      auto minmax_it = absl::c_minmax_element(values);
      const T min_val = *minmax_it.first;
      const T max_val = *minmax_it.second;
      if (num_bits == 0) {
        DCHECK_EQ(max_val, 0);
      } else {
        using U = std::make_unsigned_t<T>;
        // We only shift by num_bits - 1, to avoid to potentially shift by the
        // entire bit width, which would be undefined behavior.
        DCHECK_LE(static_cast<U>(max_val) >> (num_bits - 1), 1);
        DCHECK_LE(static_cast<U>(min_val) >> (num_bits - 1), 1);
      }
    }
  }

  // This shortcut here is important to have early, guarded by as few "if"
  // branches as possible, for the use case where the array is very small.
  // For larger arrays below, the overhead of a few "if" is negligible.
  if (values.size() < 300) {
    absl::c_sort(values);
    return;
  }

  // TODO(user): More complex decision tree, based on benchmarks. This one
  // is already nice, but some cases can surely be optimized.
  if (num_bits <= 16) {
    if (num_bits <= 8) {
      RadixSortTpl<T, /*radix_width=*/8, /*num_passes=*/1>(values);
    } else {
      RadixSortTpl<T, /*radix_width=*/8, /*num_passes=*/2>(values);
    }
  } else if (num_bits <= 32) {  // num_bits ∈ [17..32]
    if (values.size() < 1000) {
      if (num_bits <= 24) {
        RadixSortTpl<T, /*radix_width=*/8, /*num_passes=*/3>(values);
      } else {
        RadixSortTpl<T, /*radix_width=*/8, /*num_passes=*/4>(values);
      }
    } else if (values.size() < 2'500'000) {
      if (num_bits <= 22) {
        RadixSortTpl<T, /*radix_width=*/11, /*num_passes=*/2>(values);
      } else {
        RadixSortTpl<T, /*radix_width=*/11, /*num_passes=*/3>(values);
      }
    } else {
      RadixSortTpl<T, /*radix_width=*/16, /*num_passes=*/2>(values);
    }
  } else if (num_bits <= 64) {  // num_bits ∈ [33..64]
    if (values.size() < 5000) {
      absl::c_sort(values);
    } else if (values.size() < 1'500'000) {
      if (num_bits <= 33) {
        RadixSortTpl<T, /*radix_width=*/11, /*num_passes=*/3>(values);
      } else if (num_bits <= 44) {
        RadixSortTpl<T, /*radix_width=*/11, /*num_passes=*/4>(values);
      } else if (num_bits <= 55) {
        RadixSortTpl<T, /*radix_width=*/11, /*num_passes=*/5>(values);
      } else {
        RadixSortTpl<T, /*radix_width=*/11, /*num_passes=*/6>(values);
      }
    } else {
      if (num_bits <= 48) {
        RadixSortTpl<T, /*radix_width=*/16, /*num_passes=*/3>(values);
      } else {
        RadixSortTpl<T, /*radix_width=*/16, /*num_passes=*/4>(values);
      }
    }
  } else {
    LOG(DFATAL) << "RadixSort() called with unsupported value type";
    absl::c_sort(values);
  }
}

}  // namespace operations_research

#endif  // OR_TOOLS_ALGORITHMS_RADIX_SORT_H_
