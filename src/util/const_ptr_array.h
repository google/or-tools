// Copyright 2010-2012 Google
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

#ifndef OR_TOOLS_UTIL_CONST_PTR_ARRAY_H_
#define OR_TOOLS_UTIL_CONST_PTR_ARRAY_H_


using std::string;

#include <stddef.h>
#include <string>
#include <vector>

#include "base/integral_types.h"
#include "base/int-type.h"
#include "base/int-type-indexed-vector.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/stringprintf.h"
#include "base/concise_iterator.h"
#include "util/string_array.h"

namespace operations_research {
// This class is used to store an const array of T*.
//
// This is useful inside constraints and expressions. The constructors,
// except the one with the pointer to a vector of cells will copy the
// data internally and will not take ownership of the data passed in
// argument.
// Its goals are:
// - to unify the construction code across the optimization libraries.
// - to provide one code to modify these mappings and apply
//    transformations like sorting.
// It requires T to implement a method string T::DebugString() const;
// This is linked to @ref ConstIntArray.
template <class T> class ConstPtrArray {
 public:
  // Build from one vector. Copy the data internally.
  explicit ConstPtrArray(const std::vector<T*>& ptrs)
      : data_(new std::vector<T*>(ptrs)) {}

  // Build from one data vector. Takes ownership of the vector.
  explicit ConstPtrArray(std::vector<T*>* const data) : data_(data) {}

  // This code releases the ownership of the data into the returned vector.
  // After this method is called, data_ points to a null vector.
  std::vector<T*>* Release() {
    return data_.release();
  }

  // Size of the array. This is not valid after Release() has been called.
  int size() const {
    if (data_.get() == NULL) {
      return 0;
    }
    return data_->size();
  }

  // Checks for deep equality with other array.
  bool Equals(const ConstPtrArray<T>& other) const {
    const int current_size = size();
    if (current_size != other.size()) {
      return false;
    }
    for (int i = 0; i < current_size; ++i) {
      if (get(i) != other.get(i)) {
        return false;
      }
    }
    return true;
  }

  // Returns the instance of T* at position index. This is not valid
  // after Release() has been called.  @see operator[]().
  T* operator[](int64 index) const {
    CHECK_NOTNULL(data_.get());
    return (*data_)[index];
  }

  // Returns the instance of T* at position index. This is not valid
  // after Release() has been called. @see operator[]().
  T* get(int64 index) const {
    CHECK_NOTNULL(data_.get());
    return (*data_)[index];
  }

  // Returns a copy of the data. Usually used to create a new ConstPtrArray.
  std::vector<T*>* Copy() const {
    CHECK_NOTNULL(data_.get());
    return new std::vector<T*>(*data_);
  }

  // Pretty print.
  string DebugString() const {
    if (data_.get() == NULL) {
      return "Released ConstPtrArray";
    }
    return StringPrintf("[%s]", DebugStringArray(data_->data(),
                                                 data_->size(), ", ").c_str());
  }

  // Access to const raw data.
  // TODO(user) : deprecate API.
  const T* const* RawData() const {
    if (data_.get() == NULL) {
      return data_->data();
    } else {
      return NULL;
    }
  }

 private:
  scoped_ptr<std::vector<T*> > data_;
};

// This class is used to store an const array of T*. Indices are Typed
// integer.
//
// This is useful inside constraints and expressions. The constructors,
// except the one with the pointer to a vector of cells will copy the
// data internally and will not take ownership of the data passed in
// argument.
// Its goals are:
// - to unify the construction code across the optimization libraries.
// - to provide one code to modify these mappings and apply
//    transformations like sorting.
// It requires T to implement a method string T::DebugString() const;
// This is linked to @ref ConstIntArray.
template <class I, class T> class TypedConstPtrArray {
 public:
  // Build from one vector. Copy the data internally.
  explicit TypedConstPtrArray(const ITIVector<I, T*>& ptrs)
      : data_(new ITIVector<I, T*>(ptrs)) {}

  explicit TypedConstPtrArray(const std::vector<T*>& ptrs)
      : data_(new ITIVector<I, T*>(ptrs.begin(), ptrs.end())) {}

  // Build from one data vector. Takes ownership of the vector.
  explicit TypedConstPtrArray(ITIVector<I, T*>* const data) : data_(data) {}

  // This code releases the ownership of the data into the returned vector.
  // After this method is called, data_ points to a null vector.
  ITIVector<I, T*>* Release() {
    return data_.release();
  }

  // Size of the array. This is not valid after Release() has been called.
  int size() const {
    if (data_.get() == NULL) {
      return 0;
    }
    return data_->size();
  }

  // Checks for deep equality with other array.
  bool Equals(const TypedConstPtrArray<I, T>& other) const {
    const int current_size = size();
    if (current_size != other.size()) {
      return false;
    }
    for (int i = 0; i < current_size; ++i) {
      if (get(i) != other.get(i)) {
        return false;
      }
    }
    return true;
  }

  // Returns the instance of T* at position index. This is not valid
  // after Release() has been called.  @see operator[]().
  T* operator[](I index) const {
    CHECK_NOTNULL(data_.get());
    return (*data_)[index];
  }

  // Returns the instance of T* at position index. This is not valid
  // after Release() has been called. @see operator[]().
  T* get(I index) const {
    CHECK_NOTNULL(data_.get());
    return (*data_)[index];
  }

  // Returns a copy of the data. Usually used to create a new ConstPtrArray.
  ITIVector<I, T*>* Copy() const {
    CHECK_NOTNULL(data_.get());
    return new ITIVector<I, T*>(*data_);
  }

  // Pretty print.
  string DebugString() const {
    if (data_.get() == NULL) {
      return "Released ConstPtrArray";
    }
    return StringPrintf("[%s]", DebugStringArray(data_->data(),
                                                 data_->size(), ", ").c_str());
  }

  // Access to const raw data.
  // TODO(user) : deprecate API.
  const T* const* RawData() const {
    if (data_.get() == NULL) {
      return data_->data();
    } else {
      return NULL;
    }
  }

 private:
  scoped_ptr<ITIVector<I, T*> > data_;
};

}  // namespace operations_research
#endif  // OR_TOOLS_UTIL_CONST_PTR_ARRAY_H_
