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

#ifndef OR_TOOLS_UTIL_ALIGNED_MEMORY_INTERNAL_H_
#define OR_TOOLS_UTIL_ALIGNED_MEMORY_INTERNAL_H_

#include <cstddef>
#include <cstdlib>
#include <memory>

#include "ortools/base/mathutil.h"

namespace operations_research {

namespace internal {

template <typename T, size_t alignment_bytes, size_t misalignment_bytes>
struct AllocatorWithAlignment : public std::allocator<T> {
  // Allocates memory for num_items items of type T. The memory must be freed
  // using deallocate(); using it with free() or `delete` might cause unexpected
  // behavior when misalignment is used.
  T* allocate(size_t num_items) {
    // Having misalignment_bytes >= alignment_bytes is useless, because all
    // misalignments are equivalent modulo `alignment_bytes`. Disallowing it
    // allows us to simplify the code below.
    static_assert(alignment_bytes == 0 || misalignment_bytes < alignment_bytes);

    // `std::aligned_alloc(alignment, size)` requires that `size` is a multiple
    // of `alignment`, and might return a nullptr when this is not respected. To
    // be safe, we round the number of bytes up to alignment.
    const size_t num_required_bytes =
        misalignment_bytes + num_items * sizeof(T);

    const size_t num_allocated_bytes =
        MathUtil::RoundUpTo(num_required_bytes, alignment_bytes);

    std::uintptr_t ptr = reinterpret_cast<std::uintptr_t>(
        std::aligned_alloc(alignment_bytes, num_allocated_bytes));
    return reinterpret_cast<T*>(ptr + misalignment_bytes);
  }
  // A version of allocate() that takes a hint; we just ignore the hint.
  T* allocate(size_t n, const void*) { return allocate(n); }

  // Frees memory allocated by allocate().
  void deallocate(T* p, size_t) {
    std::uintptr_t aligned_pointer =
        reinterpret_cast<std::uintptr_t>(p) - misalignment_bytes;
    free(reinterpret_cast<void*>(aligned_pointer));
  }

  // Rebind must be specialized to produce AllocatorWithAlignment and not
  // std::allocator. It uses the same alignment and misalignment as its source.
  template <typename U>
  struct rebind {
    using other =
        AllocatorWithAlignment<U, alignment_bytes, misalignment_bytes>;
  };
};

}  // namespace internal

}  // namespace operations_research

#endif  // OR_TOOLS_UTIL_ALIGNED_MEMORY_INTERNAL_H_
