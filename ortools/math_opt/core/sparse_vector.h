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

#ifndef OR_TOOLS_MATH_OPT_CORE_SPARSE_VECTOR_H_
#define OR_TOOLS_MATH_OPT_CORE_SPARSE_VECTOR_H_

#include <cstdint>
#include <vector>

namespace operations_research::math_opt {

// A sparse representation of a vector of values.
//
// This is equivalent to Sparse(Double|Bool|Int32)VectorProto but for C++.
template <typename T>
struct SparseVector {
  // Should be sorted (in increasing order) with all elements distinct.
  std::vector<int64_t> ids;

  // Must have equal length to ids.
  std::vector<T> values;
};

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_CORE_SPARSE_VECTOR_H_
