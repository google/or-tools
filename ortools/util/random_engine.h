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

// Defines the random engine type to use within operations_research code.

#ifndef ORTOOLS_UTIL_RANDOM_ENGINE_H_
#define ORTOOLS_UTIL_RANDOM_ENGINE_H_

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iterator>
#include <limits>
#include <random>

#include "absl/random/bit_gen_ref.h"

namespace operations_research {

using random_engine_t = std::mt19937_64;

namespace random_internal {

// Returns the high 64 bits of the 128-bit product of lhs and rhs.
inline uint64_t MultiplyHigh64(uint64_t lhs, uint64_t rhs) {
  const uint64_t lhs_low = static_cast<uint32_t>(lhs);
  const uint64_t lhs_high = lhs >> 32;
  const uint64_t rhs_low = static_cast<uint32_t>(rhs);
  const uint64_t rhs_high = rhs >> 32;

  const uint64_t low_low = lhs_low * rhs_low;
  const uint64_t low_high = lhs_low * rhs_high;
  const uint64_t high_low = lhs_high * rhs_low;
  const uint64_t high_high = lhs_high * rhs_high;
  const uint64_t carry =
      (low_low >> 32) + static_cast<uint32_t>(low_high) +
      static_cast<uint32_t>(high_low);
  return high_high + (low_high >> 32) + (high_low >> 32) + (carry >> 32);
}

}  // namespace random_internal

// Returns a uniformly distributed value in [0, exclusive_upper_bound). This
// uses Lemire's nearly divisionless algorithm, which is also used by libstdc++.
// Keeping the mapping explicit makes it independent from the standard library
// while preserving the existing libstdc++ search sequence.
inline uint64_t StableUniformIndex(absl::BitGenRef random,
                                   uint64_t exclusive_upper_bound) {
  assert(exclusive_upper_bound > 0);
  const uint64_t rejection_threshold =
      -exclusive_upper_bound % exclusive_upper_bound;
  while (true) {
    const uint64_t value = random();
    const uint64_t low = value * exclusive_upper_bound;
    if (low < rejection_threshold) continue;
    return random_internal::MultiplyHigh64(value, exclusive_upper_bound);
  }
}

// Returns a uniformly distributed double in [0.0, 1.0). This is the conversion
// used by libstdc++ for a full-width 64-bit generator and is independent from
// the standard library implementation.
inline double StableUniformDouble(absl::BitGenRef random) {
  constexpr double kInverseTwoToThe64 = 0x1.0p-64;
  constexpr double kLargestValueBelowOne = 0x1.fffffffffffffp-1;
  return std::min(static_cast<double>(random()) * kInverseTwoToThe64,
                  kLargestValueBelowOne);
}

// A platform-independent shuffle that preserves libstdc++'s optimized shuffle
// sequence for 64-bit generators. The paired swaps reduce random engine calls.
template <typename RandomAccessIterator>
void StableShuffle(RandomAccessIterator first, RandomAccessIterator last,
                   absl::BitGenRef random) {
  using Difference =
      typename std::iterator_traits<RandomAccessIterator>::difference_type;
  const Difference size = last - first;
  if (size <= 1) return;

  const uint64_t unsigned_size = static_cast<uint64_t>(size);
  if (std::numeric_limits<uint64_t>::max() / unsigned_size >= unsigned_size) {
    Difference i = 1;
    if (unsigned_size % 2 == 0) {
      std::iter_swap(first + i,
                     first + static_cast<Difference>(
                                 StableUniformIndex(random, 2)));
      ++i;
    }
    while (i < size) {
      const uint64_t first_range = static_cast<uint64_t>(i) + 1;
      const uint64_t second_range = first_range + 1;
      const uint64_t combined =
          StableUniformIndex(random, first_range * second_range);
      std::iter_swap(first + i,
                     first + static_cast<Difference>(combined / second_range));
      ++i;
      std::iter_swap(first + i,
                     first + static_cast<Difference>(combined % second_range));
      ++i;
    }
    return;
  }

  for (Difference i = 1; i < size; ++i) {
    const Difference selected = static_cast<Difference>(
        StableUniformIndex(random, static_cast<uint64_t>(i) + 1));
    std::iter_swap(first + i, first + selected);
  }
}

}  // namespace operations_research

#endif  // ORTOOLS_UTIL_RANDOM_ENGINE_H_
