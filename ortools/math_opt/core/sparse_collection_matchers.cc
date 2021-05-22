// Copyright 2010-2021 Google LLC
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

#include "ortools/math_opt/core/sparse_collection_matchers.h"

#include <cstdint>
#include <initializer_list>
#include <tuple>
#include <utility>

#include "ortools/math_opt/sparse_containers.pb.h"

namespace operations_research {
namespace math_opt {

SparseDoubleVectorProto MakeSparseDoubleVector(
    std::initializer_list<std::pair<int64_t, double>> pairs) {
  SparseDoubleVectorProto ret;
  for (const auto [id, value] : pairs) {
    ret.add_ids(id);
    ret.add_values(value);
  }
  return ret;
}

SparseBoolVectorProto MakeSparseBoolVector(
    std::initializer_list<std::pair<int64_t, bool>> pairs) {
  SparseBoolVectorProto ret;
  for (const auto [id, value] : pairs) {
    ret.add_ids(id);
    ret.add_values(value);
  }
  return ret;
}

SparseDoubleMatrixProto MakeSparseDoubleMatrix(
    std::initializer_list<std::tuple<int64_t, int64_t, double>> values) {
  SparseDoubleMatrixProto ret;
  for (const auto [row, col, coefficient] : values) {
    ret.add_row_ids(row);
    ret.add_column_ids(col);
    ret.add_coefficients(coefficient);
  }
  return ret;
}

}  // namespace math_opt
}  // namespace operations_research
