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

#ifndef ORTOOLS_UTIL_ZVECTOR_H_
#define ORTOOLS_UTIL_ZVECTOR_H_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>

#include "absl/log/check.h"

// An array class for storing arrays of integers.
//
// The range of indices is specified at the construction of the object.
// The minimum and maximum indices are inclusive.
// Think of the Pascal syntax array[min_index..max_index] of ...
//
// For example, ZVector<int32_t>(-100000,100000) will store 200001
// signed integers of 32 bits each, and the possible range of indices
// will be -100000..100000.

namespace operations_research {

template <class T>
class ZVector {
  static_assert(std::is_trivially_default_constructible_v<T>,
                "T must be trivial default constructible to prevent UB because "
                "the array is left uninitialized");

 public:
  // The storage is uninitialized upon construction.
  ZVector(int64_t min_index, int64_t max_index)
      : start_index_(min_index),
        size_(GetSize(min_index, max_index)),
        storage_(new T[size_]),
        base_(storage_.get() - start_index_) {}

  ZVector() : start_index_(0), size_(0), storage_(nullptr), base_(nullptr) {}

  T& operator[](int64_t index) {
    DCheckIndex(index);
    return base_[index];
  }

#if !defined(SWIG)
  T operator[](int64_t index) const {
    DCheckIndex(index);
    return base_[index];
  }

  T& Value(int64_t index) {
    DCheckIndex(index);
    return base_[index];
  }
#endif  // !defined(SWIG)

  void Set(int64_t index, T value) {
    DCheckIndex(index);
    base_[index] = value;
  }

  void SetAll(T value) {
    if (storage_) {
      auto* const ptr = storage_.get();
      std::fill(ptr, ptr + size_, value);
    }
  }

 private:
  static size_t GetSize(int64_t min_index, int64_t max_index) {
    CHECK_LE(min_index, max_index);
    return max_index - min_index + 1;
  }

  void DCheckIndex(int64_t index) const {
    DCHECK_GE(index, start_index_);
    DCHECK_LT(index, start_index_ + static_cast<int64_t>(size_));
    DCHECK(base_ != nullptr);
  }

  int64_t start_index_;           // Minimum index for the array.
  size_t size_;                   // The number of elements in the array.
  std::unique_ptr<T[]> storage_;  // Storage memory for the array.
  T* base_;  // Pointer to the element indexed by zero in the array.
};

}  // namespace operations_research

#endif  // ORTOOLS_UTIL_ZVECTOR_H_
