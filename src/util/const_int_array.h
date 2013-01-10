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

#ifndef OR_TOOLS_UTIL_CONST_INT_ARRAY_H_
#define OR_TOOLS_UTIL_CONST_INT_ARRAY_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/scoped_ptr.h"

using std::string;

namespace operations_research {
// This class is used to store constant copies of int64 arrays.
//
// These copies are used inside constraints or expressions. When
// constructed with a C array or a vector, The const int array will
// make a internal copy and own that copy. It will not take ownership
// of the vector/array which can be deleted afterwards. This follows
// the semantics of constraints and expressions which store a
// read-only copy of the data.
//
// Its goals are:
// - to unify the construction code across the optimization libraries.
// - to provide one code to scan these arrays and compute given properties like
//   monotonicity.
//
// As const int arrays provide scanning capabilities, the code inside
// a constraint and its factory looks like:
//
// IntExpr* MakeMyExpr(const std::vector<int64>& values) {
//   ConstIntArray copy(values);
//   if (copy.Status(ConstIntArray::IS_INCREASING)) {
//     return new MyOptimizedExpr(copy.Release());
//   } else {
//    return new MyGenericExpr(copy.Release());
//   }
// }
//
// With:
// class MyOptimizedExpr : IntExpr {
//  public:
//   MyOptimizedExpr(std::vector<int64>* data) : values_(data) {}
//  private:
//   ConstIntArray values_;
// };
class ConstIntArray {
 public:
  // These describe static properties of the int64 array.
  enum Property {
    IS_CONSTANT = 0,
    IS_STRICTLY_INCREASING = 1,
    IS_INCREASING = 2,
    IS_STRICTLY_DECREASING = 3,
    IS_DECREASING = 4,
    IS_BOOLEAN = 5,           // in {0, 1}
    IS_POSITIVE = 6,          // > 0
    IS_NEGATIVE = 7,          // < 0
    IS_POSITIVE_OR_NULL = 8,  // >= 0
    IS_NEGATIVE_OR_NULL = 9,  // <= 0'
    IS_CONTIGUOUS = 10,
    NUM_PROPERTY = 11         // Sentinel.
  };

  // Build from a vector. Copy the data internally.
  explicit ConstIntArray(const std::vector<int64>& data);
  // Build from a vector. Copy the data internally.
  explicit ConstIntArray(const std::vector<int>& data);
  // Build from a C array. Copy the data internally.
  ConstIntArray(const int64* const data, int size);
  // Build from a C array. Copy the data internally.
  ConstIntArray(const int* const data, int size);
  // Build from a vector. Copy the data internally, and sort the data.
  explicit ConstIntArray(const std::vector<int64>& data, bool increasing);
  // Build from a vector. Copy the data internally, and sort the data.
  explicit ConstIntArray(const std::vector<int>& data, bool increasing);
  // Build from a pointer to a vector (usually created by the
  // Release(), or SortedCopy() method).  This call will take ownership of
  // the data and not make a copy.
  explicit ConstIntArray(std::vector<int64>* data);

  ~ConstIntArray();

  // Pretty print.
  string DebugString() const;

  // This code release the ownership of the data into the returned vector.
  // After this method is called, data_ points to a null vector.
  std::vector<int64>* Release();

  // This will create a copy of the data.
  std::vector<int64>* Copy() const;

  // This will create a new data holder with the sorted array.
  std::vector<int64>* SortedCopy(bool increasing) const;

  // This will create a new data holder with the sorted array where
  // the duplicate values have been removed.
  std::vector<int64>* SortedCopyWithoutDuplicates(bool increasing) const;

  // Equality test.
  bool Equals(const ConstIntArray& other) const;

  // Size of the array. This is not valid after Release() has been called.
  int size() const;

  // Operator to access the data at the given index. This is not valid
  // after a release.
  int64 operator[](int64 index) const {
    CHECK_NOTNULL(data_.get());
    return (*data_)[index];
  }

  // Accessor to value in the array.
  int64 get(int64 index) const {
    CHECK_NOTNULL(data_.get());
    return (*data_)[index];
  }

  // Access to const raw data.
  // TODO(user) : deprecate API.
  const int64* RawData() const {
    return data_->data();
  }

  const std::vector<int64>& RawVector() const {
    return *data_.get();
  }

  // Check the status of a given info bit. It will scan the array on demand.
  // This is not valid after Release() has been called.
  bool HasProperty(Property info);

 private:
  void AndProperty(Property info, bool value);
  void Scan();
  scoped_ptr<std::vector<int64> > data_;
  bool scanned_;
  uint64 status_;
  DISALLOW_COPY_AND_ASSIGN(ConstIntArray);
};
}  // namespace operations_research

#endif  // OR_TOOLS_UTIL_CONST_INT_ARRAY_H_
