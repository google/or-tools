// Copyright 2010-2022 Google LLC
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

#ifndef OR_TOOLS_UTIL_VECTOR_SUM_INTERNAL_H_
#define OR_TOOLS_UTIL_VECTOR_SUM_INTERNAL_H_

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <numeric>

#include "absl/base/attributes.h"
#include "absl/types/span.h"
#include "ortools/util/aligned_memory.h"

namespace operations_research {
namespace internal {

// A contiguous block of memory that contains `size` values of type `Value`, and
// the first value is aligned to `alignment` bytes.
template <typename Value, size_t size, size_t alignment = sizeof(Value) * size>
struct alignas(alignment) AlignedBlock {
  Value values[size] = {};

  Value Sum() const {
    alignas(alignment) Value sum[size];
    std::copy(std::begin(values), std::end(values), std::begin(sum));
    for (int i = size; i > 1; i /= 2) {
      int middle = i / 2;
      for (int j = 0; j < middle; ++j) {
        sum[j] += sum[middle + j];
      }
    }
    return sum[0];
  }
};

// In-place addition for two AlignedBlock values. Adds `in` to `out`, storing
// the value in `out`. Unless something steps in, this compiles into a single
// `*addp*` instruction.
template <typename Value, size_t size>
void AddInPlace(AlignedBlock<Value, size>& out,
                const AlignedBlock<Value, size>& in) {
  for (int i = 0; i < size; ++i) {
    out.values[i] += in.values[i];
  }
}

// Computes the sum of `num_blocks` aligned blocks. Proceeds in three phases:
// 1. Parallel sum with N = `num_blocks_per_iteration` independent block-sized
//    accumulators. At the end, accumulator i contains partial sums at indices
//    i, i + N, i + 2*N, ..., i + M*N, where M is the largest number of blocks
//    such that i+M*N < num_blocks for all i.
// 2. Parallel addition of remaining blocks. All remaining blocks are added to
//    accumulators 0, ..., num_remaining_blocks - 1.
// 3. Sum of accumulators: all accumulators are added together. The result is a
//    single block-sized accumulator that is returned to the caller.
//
// The code of the function was specifically tuned for 32-bit floating point
// values, and works best with block_size = 4 and num_blocks_per_iteration = 4.
// It will likely work with other types, but may require extra care and possibly
// different parameters.
//
// NOTE(user): As of 2023-04-28, Clang's auto-vectorizer is *very* brittle
// and most attempts to reduce the last accumulator from step 3 into a single
// value prevents the rest of the function from being vectorized. To mitigate
// this behavior we return the whole block (which should normally fit into a
// single vector register). We also mark the function with
// `ABSL_ATTRIBUTE_NOINLINE` to make prevent the inliner from merging this
// function with any additional code that would prevent vectorization.
template <size_t block_size, size_t num_blocks_per_iteration, typename Value>
AlignedBlock<Value, block_size> ABSL_ATTRIBUTE_NOINLINE AlignedBlockSum(
    const AlignedBlock<Value, block_size>* blocks, size_t num_blocks) {
  using Block = AlignedBlock<Value, block_size>;
  static_assert(sizeof(Block[2]) == sizeof(Block::values) * 2,
                "The values in the block are not packed.");

  AlignedBlock<Value, block_size> sum[num_blocks_per_iteration];

  const int leftover_blocks = num_blocks % num_blocks_per_iteration;
  const int packed_blocks = num_blocks - leftover_blocks;

  const AlignedBlock<Value, block_size>* aligned_block_end =
      blocks + packed_blocks;

  // Phase 1: Parallel sum of the bulk of the data.
  if (packed_blocks >= num_blocks_per_iteration) {
    std::copy(blocks, blocks + num_blocks_per_iteration, sum);
  }
  for (int i = num_blocks_per_iteration; i < packed_blocks;
       i += num_blocks_per_iteration) {
    for (int j = 0; j < num_blocks_per_iteration; ++j) {
      AddInPlace(sum[j], blocks[i + j]);
    }
  }

  // Phase 2: Semi-parallel sum of the remaining up to
  // num_blocks_per_iteration - 1 blocks.
  for (int i = 0; i < leftover_blocks; ++i) {
    AddInPlace(sum[i], aligned_block_end[i]);
  }

  // Phase 3: Reduce the accumulator blocks to a single block.
  // NOTE(user): When this code is auto-vectorized correctly, the initial
  // copy is a no-op, and the for loop below translates to
  // num_blocks_per_iteration - 1 vector add instructions. In 2023-05, I
  // experimented with other versions (e.g. using sum[0] as the target or making
  // res a const reference to sum[0], but in most cases they broke vectorization
  // of the whole function).
  AlignedBlock<Value, block_size> res = sum[0];
  for (int i = 1; i < num_blocks_per_iteration; ++i) {
    AddInPlace(res, sum[i]);
  }

  return res;
}

// Computes the sum of values in `values`, by adding `num_blocks_per_iteration`
// blocks of `block_size` values.
// By default, the sum does not make any assumptions about the size or alignment
// of `values`. When the first item of `values` is known to be aligned to
// `block_size * sizeof(Value)` bytes, `assume_aligned_at_start` can be used to
// save a small amount of time.
template <size_t block_size = 4, size_t num_blocks_per_iteration = 4,
          bool assume_aligned_at_start = false, typename Value>
Value VectorSum(absl::Span<const Value> values) {
  using Block = AlignedBlock<Value, block_size>;
  const Value* start_ptr = values.data();
  const int size = values.size();
  // With less than two blocks, there's not a lot to vectorize, and a simple
  // sequential sum is usually faster.
  if (size == 0) return Value{0};
  if (size < 2 * block_size) {
    return std::accumulate(start_ptr + 1, start_ptr + size, *start_ptr);
  }

  if (assume_aligned_at_start) {
    ABSL_ASSUME(reinterpret_cast<std::uintptr_t>(start_ptr) % alignof(Block) ==
                0);
  }
  const Value* aligned_start_ptr =
      assume_aligned_at_start ? start_ptr : AlignUp<alignof(Block)>(start_ptr);
  const Block* blocks = reinterpret_cast<const Block*>(aligned_start_ptr);
  const Value* end_ptr = start_ptr + size;
  const Value* aligned_end_ptr = AlignDown<alignof(Block)>(end_ptr);
  ABSL_ASSUME(aligned_start_ptr <= aligned_end_ptr);
  const size_t num_blocks = (aligned_end_ptr - aligned_start_ptr) / block_size;
  ABSL_ASSUME(
      reinterpret_cast<std::uintptr_t>(aligned_end_ptr) % alignof(Block) == 0);

  Value leading_items_sum{};
  if (!assume_aligned_at_start) {
    leading_items_sum = std::accumulate(start_ptr, aligned_start_ptr, Value{});
  }
  Block res =
      AlignedBlockSum<block_size, num_blocks_per_iteration>(blocks, num_blocks);
  Value block_sum = res.Sum();
  return std::accumulate(aligned_end_ptr, end_ptr,
                         block_sum + leading_items_sum);
}

}  // namespace internal
}  // namespace operations_research

#endif  // OR_TOOLS_UTIL_VECTOR_SUM_INTERNAL_H_
