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

#include "ortools/math_opt/storage/sparse_coefficient_map.h"

#include <cstdint>

#include "absl/algorithm/container.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/storage/model_storage_types.h"

namespace operations_research::math_opt {

SparseDoubleVectorProto SparseCoefficientMap::Proto() const {
  SparseDoubleVectorProto result;
  result.mutable_ids()->Reserve(static_cast<int>(terms_.size()));
  result.mutable_values()->Reserve(static_cast<int>(terms_.size()));
  for (const auto [var, _] : terms_) {
    result.add_ids(var.value());
  }
  absl::c_sort(*result.mutable_ids());
  for (const int64_t id : result.ids()) {
    result.add_values(terms_.at(VariableId(id)));
  }
  return result;
}

}  // namespace operations_research::math_opt
