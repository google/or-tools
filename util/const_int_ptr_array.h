// Copyright 2010-2011 Google
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

#ifndef OR_TOOLS_UTIL_CONST_INT_PTR_ARRAY_H_
#define OR_TOOLS_UTIL_CONST_INT_PTR_ARRAY_H_

#include <stddef.h>
#include <algorithm>
#include <functional>
#include "base/hash.h"
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/stringprintf.h"
#include "base/concise_iterator.h"
#include "base/map-util.h"

using std::string;

namespace operations_research {

// This class is used to store pairs of <T*, int64>.
// This is useful inside constraints and expressions. The constructors,
// except the one with the pointer to a vector of cells will copy the
// data internally and will not take ownership of the data passed in
// argument.
// Its goals are:
// - to unify the construction code across the optimization libraries.
// - to provide one code to modify these mappings and apply
//    transformations like sorting, aggregating values per pointer, or
//    removing pointers with zero values.
template <class T> class ConstIntPtrArray {
 public:
  // A cell is used to store pairs of <T, int64>.
  struct Cell {
    Cell(T* p, int64 c) : ptr(p), value(c) {}
    Cell() : ptr(NULL), value(0) {}
    string DebugString() const {
      return StringPrintf("(%lld|%s)", value, ptr->DebugString().c_str());
    }
    T* ptr;
    int64 value;
  };

  // Comparison helpers. This one sorts in a increasing way.
  struct CompareValuesLT {
    bool operator()(const Cell& first, const Cell& second) {
      return first.value < second.value;
    }
  };
  // Comparison helpers. This one sorts in a decreasing way.
  struct CompareValuesGT {
    bool operator()(const Cell& first, const Cell& second) {
      return first.value > second.value;
    }
  };

  // Build from 2 vectors. Copy the data internally.
  template<typename Integer>
  ConstIntPtrArray(const std::vector<T*>& ptrs, const std::vector<Integer>& values)
    : data_(new std::vector<Cell>()) {
    data_->reserve(ptrs.size());
    CHECK_EQ(ptrs.size(), values.size());
    for (int i = 0; i < ptrs.size(); ++i) {
      data_->push_back(Cell(ptrs[i], values[i]));
    }
  }

  // Build from 2 vectors. Copy the data internally.
  template<typename Integer>
  ConstIntPtrArray(T* const* ptrs,
                   const Integer* const values,
                   int size) : data_(new std::vector<Cell>()) {
    data_->reserve(size);
    for (int i = 0; i < size; ++i) {
      data_->push_back(Cell(ptrs[i], values[i]));
    }
  }

  // Build from one data vector. Takes ownership of the vector.
  explicit ConstIntPtrArray(std::vector<Cell>* const data) : data_(data) {}

  // This code releases the ownership of the data into the returned vector.
  // After this method is called, data_ points to a null vector.
  std::vector<Cell>* Release() {
    return data_.release();
  }

  // Size of the array. This is not valid after Release() has been called.
  int size() const {
    CHECK_NOTNULL(data_.get());
    return data_->size();
  }

  // Returns the value at position index.
  int64 value(int64 index) const {
    CHECK_NOTNULL(data_.get());
    return (*data_)[index].value;
  }

  // Returns the instance of T* at position index. This is not valid
  // after Release() has been called.
  T* ptr(int64 index) const {
    CHECK_NOTNULL(data_.get());
    return (*data_)[index].ptr;
  }

  // Returns a copy of the data. Usually used to create a new ConstIntPtrArray.
  std::vector<Cell>* Copy() const {
    CHECK_NOTNULL(data_.get());
    return new std::vector<Cell>(*data_);
  }

  // This will create a new data holder with the mapping sorted by values.
  std::vector<Cell>* SortedCopy(bool increasing) const {
    std::vector<Cell>* new_data = new std::vector<Cell>(*data_);
    Sort(new_data, increasing);
    return new_data;
  }

  // This will create a new data holder with the mapping sorted by
  // values.  Furthermore, it will regroup and sum values
  // attached to the same pointer to T. Finally, it can remove pairs
  // with a value of zero.
  std::vector<Cell>* SortedCopyAggregateValues(bool increasing,
                                          bool remove_zeros) const {
    // Fill a hash_map, aggregating values.
    hash_map<T*, int64> ptr_value_map;
    for (ConstIter<std::vector<Cell> > iter(*data_.get()); !iter.at_end(); ++iter) {
      T* const ptr = iter->ptr;
      const int64 current_value = FindWithDefault(ptr_value_map, ptr, 0);
      ptr_value_map[ptr] = current_value + iter->value;
    }
    // Transfer to a vector, ignoring zeros if need be.
    std::vector<Cell>* const new_data = new std::vector<Cell>();
    for (ConstIter<hash_map<T*, int64> > iter(ptr_value_map);
         !iter.at_end();
         ++iter) {
      if (!remove_zeros || iter->second != 0) {
        new_data->push_back(Cell(iter->first, iter->second));
      }
    }
    // Now we sort the vector.
    Sort(new_data, increasing);
    return new_data;
  }

  // Pretty print.
  string DebugString() const {
    if (data_.get() == NULL) {
      return "Released ConstIntPtrArray";
    }
    string result = "[";
    bool first = true;
    for (ConstIter<std::vector<Cell> > iter(*data_.get()); !iter.at_end(); ++iter) {
      if (first) {
        first = false;
      } else {
        result.append(", ");
      }
      result.append(iter->DebugString());
    }
    result.append("]");
    return result;
  }
 private:
  void Sort(std::vector<Cell>* const data, bool increasing) const {
    if (increasing) {
      std::stable_sort(data->begin(), data->end(), CompareValuesLT());
    } else {
      std::stable_sort(data->begin(), data->end(), CompareValuesGT());
    }
  }
  scoped_ptr<std::vector<Cell> > data_;
};
}  // namespace operations_research
#endif  // OR_TOOLS_UTIL_CONST_INT_PTR_ARRAY_H_

