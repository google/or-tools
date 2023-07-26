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

// Provides functions and data structures that make it easier to work with
// aligned memory:
//
// - AlignedAllocator<T, n>, an extension of std::allocator<T> that also takes
//   an explicit memory alignment parameter. The memory blocks returned by the
//   allocator are aligned to this number of bytes, i.e. the address of the
//   beginning of the block will be N * alignment_bytes for some N.
// - AlignedVector<>, a specialization of std::vector<> that uses the aligned
//   allocator to create blocks with explicit allocations.
//
// - AlignUp and AlignDown are functions that align a pointer to the given
//   number of bytes.

#ifndef OR_TOOLS_UTIL_ALIGNED_MEMORY_H_
#define OR_TOOLS_UTIL_ALIGNED_MEMORY_H_

#include <cstddef>
#include <cstdint>
#include <vector>

#include "ortools/util/aligned_memory_internal.h"

namespace operations_research {

// Functions for working with pointers and rounding them up and down to a given
// alignment.

// Returns the nearest greater or equal address that is a multiple of
// alignment_bytes. When ptr is already aligned to alignment_bytes, returns it
// unchanged.
template <size_t alignment_bytes, typename Value>
inline Value* AlignUp(Value* ptr) {
  const std::uintptr_t int_ptr = reinterpret_cast<std::intptr_t>(ptr);
  const std::uintptr_t misalignment = int_ptr % alignment_bytes;
  if (misalignment == 0) return ptr;
  return reinterpret_cast<Value*>(int_ptr - misalignment + alignment_bytes);
}

// Returns the nearest smaller or equal address that is a multiple of
// alignment_bytes. When ptr is already aligned to alignment_bytes, returns it
// unchanged
template <size_t alignment_bytes, typename Value>
inline Value* AlignDown(Value* ptr) {
  const std::intptr_t int_ptr = reinterpret_cast<std::intptr_t>(ptr);
  const std::intptr_t misalignment = int_ptr % alignment_bytes;
  return reinterpret_cast<Value*>(int_ptr - misalignment);
}

// Returns true when `ptr` is aligned to `alignment_bytes` bytes.
template <size_t alignment_bytes, typename Value>
inline bool IsAligned(Value* ptr) {
  return reinterpret_cast<std::uintptr_t>(ptr) % alignment_bytes == 0;
}

// Support for aligned containers in STL.

// An allocator that always aligns its memory to `alignment_bytes`.
template <typename T, size_t alignment_bytes>
using AlignedAllocator =
    internal::AllocatorWithAlignment<T, alignment_bytes, 0>;

// A version of std::vector<T> whose data() pointer is always aligned to
// `alignment_bytes`.
template <typename T, size_t alignment_bytes>
using AlignedVector = std::vector<T, AlignedAllocator<T, alignment_bytes>>;

// Intentionally misaligned containers for testing correctness and performance
// of code that may depend on a certain alignment.
namespace use_only_in_tests {

// A version of AlignedAllocator for testing purposes that adds intentional
// misalignment. The returned address has the form
// alignment_bytes * N + misalignment_bytes.
template <typename T, size_t alignment_bytes, size_t misalignment_bytes>
using MisalignedAllocator =
    internal::AllocatorWithAlignment<T, alignment_bytes, misalignment_bytes>;

// A specialization of std::vector<> that uses MisalignedAllocator with the
// given parameters.
template <typename T, size_t alignment_bytes, size_t misalignment_bytes>
using MisalignedVector =
    std::vector<T, MisalignedAllocator<T, alignment_bytes, misalignment_bytes>>;

}  // namespace use_only_in_tests

}  // namespace operations_research

#endif  // OR_TOOLS_UTIL_ALIGNED_MEMORY_H_
