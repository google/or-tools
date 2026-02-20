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

#include "ortools/math_opt/solvers/glpk/glpk_sparse_vector.h"

#include <functional>
#include <vector>

#include "absl/log/check.h"

namespace operations_research::math_opt {

GlpkSparseVector::GlpkSparseVector(const int capacity)
    : capacity_(capacity),
      index_to_entry_(capacity + 1, kNotPresent),
      indices_(capacity + 1, -1),
      values_(capacity + 1, 0.0) {
  CHECK_GE(capacity, 0);
}

void GlpkSparseVector::Clear() {
  // Resets the elements of the index_to_entry_ map we have modified.
  for (int i = 1; i <= size_; ++i) {
    index_to_entry_[indices_[i]] = kNotPresent;
  }

  // Cleanup the used items to make sure we don't reuse those values by mistake
  // later.
  for (int i = 1; i <= size_; ++i) {
    indices_[i] = -1;
    values_[i] = 0.0;
  }

  size_ = 0;
}

void GlpkSparseVector::Load(
    std::function<int(int* indices, double* values)> getter) {
  CHECK(getter != nullptr);

  Clear();

  size_ = getter(indices_.data(), values_.data());

  CHECK_GE(size_, 0);
  CHECK_LE(size_, capacity_);

  // We don't know if the GLPK API has written to the first element but we reset
  // those values anyway.
  indices_[0] = -1;
  values_[0] = 0.0;

  // Update index_to_entry_.
  for (int i = 1; i <= size_; ++i) {
    const int index = indices_[i];
    CHECK_GE(index, 1);
    CHECK_LE(index, capacity_);
    CHECK_EQ(index_to_entry_[index], kNotPresent) << "duplicated: " << index;
    index_to_entry_[index] = i;
  }
}

}  // namespace operations_research::math_opt
