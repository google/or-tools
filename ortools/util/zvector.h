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


#ifndef OR_TOOLS_UTIL_ZVECTOR_H_
#define OR_TOOLS_UTIL_ZVECTOR_H_

#if defined(__APPLE__) && defined(__GNUC__)
#include <machine/endian.h>
#elif !defined(_MSC_VER)
#include <endian.h>
#endif
#include <climits>
#include <cstdio>
#include <limits>
#include <memory>
#include <string>

#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"

// An array class for storing arrays of integers.
//
// The range of indices is specified at the construction of the object.
// The minimum and maximum indices are inclusive.
// Think of the Pascal syntax array[min_index..max_index] of ...
//
// For example, ZVector<int32>(-100000,100000) will store 200001
// signed integers of 32 bits each, and the possible range of indices
// will be -100000..100000.

namespace operations_research {

template <class T>
class ZVector {
 public:
  ZVector()
      : base_(nullptr), min_index_(0), max_index_(-1), size_(0), storage_() {}

  ZVector(int64 min_index, int64 max_index)
      : base_(nullptr), min_index_(0), max_index_(-1), size_(0), storage_() {
    if (!Reserve(min_index, max_index)) {
      LOG(DFATAL) << "Could not reserve memory for indices ranging from "
                  << min_index << " to " << max_index;
    }
  }

  int64 min_index() const { return min_index_; }

  int64 max_index() const { return max_index_; }

  // Returns the value stored at index.
  T Value(int64 index) const {
    DCHECK_LE(min_index_, index);
    DCHECK_GE(max_index_, index);
    DCHECK(base_ != nullptr);
    return base_[index];
  }

#if !defined(SWIG)
  // Shortcut for returning the value stored at index.
  T& operator[](int64 index) {
    DCHECK_LE(min_index_, index);
    DCHECK_GE(max_index_, index);
    DCHECK(base_ != nullptr);
    return base_[index];
  }

  const T operator[](int64 index) const {
    DCHECK_LE(min_index_, index);
    DCHECK_GE(max_index_, index);
    DCHECK(base_ != nullptr);
    return base_[index];
  }
#endif

  // Sets to value the content of the array at index.
  void Set(int64 index, T value) {
    DCHECK_LE(min_index_, index);
    DCHECK_GE(max_index_, index);
    DCHECK(base_ != nullptr);
    base_[index] = value;
  }

  // Reserves memory for new minimum and new maximum indices.
  // Returns true if the memory could be reserved.
  // Never shrinks the memory allocated.
  bool Reserve(int64 new_min_index, int64 new_max_index) {
    if (new_min_index > new_max_index) {
      return false;
    }
    const uint64 new_size = new_max_index - new_min_index + 1;
    if (base_ != nullptr) {
      if (new_min_index >= min_index_ && new_max_index <= max_index_) {
        min_index_ = new_min_index;
        max_index_ = new_max_index;
        size_ = new_size;
        return true;
      } else if (new_min_index > min_index_ || new_max_index < max_index_) {
        return false;
      }
    }
    T* new_storage = new T[new_size];
    if (new_storage == nullptr) {
      return false;
    }

    T* const new_base = new_storage - new_min_index;
    if (base_ != nullptr) {
      T* const destination = new_base + min_index_;
      memcpy(destination, storage_.get(), size_ * sizeof(*base_));
    }

    base_ = new_base;
    size_ = new_size;
    min_index_ = new_min_index;
    max_index_ = new_max_index;
    storage_.reset(new_storage);
    return true;
  }

  // Sets all the elements in the array to value.
  void SetAll(T value) {
    DLOG_IF(WARNING, base_ == nullptr || size_ <= 0)
        << "Trying to set values to uninitialized vector.";
    for (int64 i = 0; i < size_; ++i) {
      base_[min_index_ + i] = value;
    }
  }

 private:
  // Pointer to the element indexed by zero in the array.
  T* base_;

  // Minimum index for the array.
  int64 min_index_;

  // Maximum index for the array.
  int64 max_index_;

  // The number of elements in the array.
  int64 size_;

  // Storage memory for the array.
  std::unique_ptr<T[]> storage_;
};

// Shorthands for all the types of ZVector's.
typedef ZVector<int8> Int8ZVector;
typedef ZVector<int16> Int16ZVector;
typedef ZVector<int32> Int32ZVector;
typedef ZVector<int64> Int64ZVector;
typedef ZVector<uint8> UInt8ZVector;
typedef ZVector<uint16> UInt16ZVector;
typedef ZVector<uint32> UInt32ZVector;
typedef ZVector<uint64> UInt64ZVector;

}  // namespace operations_research

#endif  // OR_TOOLS_UTIL_ZVECTOR_H_
