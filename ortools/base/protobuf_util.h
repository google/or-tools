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

#ifndef OR_TOOLS_BASE_PROTOBUF_UTIL_H_
#define OR_TOOLS_BASE_PROTOBUF_UTIL_H_

#include <string>

#include "google/protobuf/repeated_field.h"
#include "google/protobuf/text_format.h"
#include "ortools/base/logging.h"

namespace google {
namespace protobuf {
namespace util {
// RepeatedPtrField version.
template <typename T>
inline void Truncate(RepeatedPtrField<T>* array, int new_size) {
  const int size = array->size();
  DCHECK_GE(size, new_size);
  array->DeleteSubrange(new_size, size - new_size);
}

// RepeatedField version.
template <typename T>
inline void Truncate(RepeatedField<T>* array, int new_size) {
  // RepeatedField::Truncate performs size validity checks.
  array->Truncate(new_size);
}

// Removes the elements at the indices specified by 'indices' from 'array' in
// time linear in the size of 'array' (on average, even when 'indices' is a
// singleton) while preserving the relative order of the remaining elements.
// The indices must be a container of ints in strictly increasing order, such
// as vector<int>, set<int> or initializer_list<int>, and in the range [0, N -
// 1] where N is the number of elements in 'array', and RepeatedType must be
// RepeatedField or RepeatedPtrField.
// Returns number of elements erased.
template <typename RepeatedType, typename IndexContainer = std::vector<int>>
int RemoveAt(RepeatedType* array, const IndexContainer& indices) {
  if (indices.size() == 0) {
    return 0;
  }
  const int num_indices = indices.size();
  const int num_elements = array->size();
  DCHECK_LE(num_indices, num_elements);
  if (num_indices == num_elements) {
    // Assumes that 'indices' consists of [0 ... N-1].
    array->Clear();
    return num_indices;
  }
  typename IndexContainer::const_iterator remove_iter = indices.begin();
  int write_index = *(remove_iter++);
  for (int scan = write_index + 1; scan < num_elements; ++scan) {
    if (remove_iter != indices.end() && *remove_iter == scan) {
      ++remove_iter;
    } else {
      array->SwapElements(scan, write_index++);
    }
  }
  DCHECK_EQ(write_index, num_elements - num_indices);
  Truncate(array, write_index);
  return num_indices;
}

template <typename T>
T ParseTextOrDie(const std::string& input) {
  T result;
  CHECK(TextFormat::MergeFromString(input, &result));
  return result;
}

}  // namespace util
}  // namespace protobuf
}  // namespace google

#endif  // OR_TOOLS_BASE_PROTOBUF_UTIL_H_
