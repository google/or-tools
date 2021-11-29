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

#ifndef OR_TOOLS_MATH_OPT_CORE_MODEL_UPDATE_MERGE_H_
#define OR_TOOLS_MATH_OPT_CORE_MODEL_UPDATE_MERGE_H_

#include <algorithm>
#include <cstdint>

#include "ortools/base/logging.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"

namespace operations_research {
namespace math_opt {

// Merges the `from` update into the `into` one.
//
// The `from` update must represent an update that happens after the `into` one
// is applied. Thus when the two updates have overlaps, the `from` one overrides
// the value of the `into` one (i.e. the `from` update is expected to be more
// recent).
//
// This function also CHECKs that the ids of new variables and constraints in
// `from` are greater than the ones in `into` (as expected if `from` happens
// after `into`).
//
// Note that the complexity is O(size(from) + size(into)) thus if you need to
// merge a long list of updates this may be not efficient enough. In that case
// an n-way merge would be needed to be implemented here.
void MergeIntoUpdate(const ModelUpdateProto& from, ModelUpdateProto& into);

namespace internal {

// Merges the `from` list of sorted ids into the `into` one. Duplicates are
// removed.
void MergeIntoSortedIds(const google::protobuf::RepeatedField<int64_t>& from,
                        google::protobuf::RepeatedField<int64_t>& into);

// Merges the `from` sparse vector into the `into` one. When the two vectors
// have overlaps, the value in `from` is used to overwrite the one in `into`.
//
// The SparseVector type is either SparseDoubleVectorProto or
// SparseBoolVectorProto.
template <typename SparseVector>
inline void MergeIntoSparseVector(const SparseVector& from, SparseVector& into);

// Merges the `from` sparse matrix into the `into` one. When the two matrices
// have overlaps, the value in `from` is used to overwrite the one in `into`.
void MergeIntoSparseDoubleMatrix(const SparseDoubleMatrixProto& from,
                                 SparseDoubleMatrixProto& into);

}  // namespace internal

////////////////////////////////////////////////////////////////////////////////
// Inline functions implementations.
////////////////////////////////////////////////////////////////////////////////

namespace internal {

template <typename SparseVector>
void MergeIntoSparseVector(const SparseVector& from, SparseVector& into) {
  CHECK_EQ(from.ids_size(), from.values_size());
  CHECK_EQ(into.ids_size(), into.values_size());

  SparseVector result;
  auto& result_ids = *result.mutable_ids();
  auto& result_values = *result.mutable_values();

  // We don't reserve the sum of the sizes of both sparse vectors since they can
  // contain overlapping ids. But we know that we will have at least the max
  // length of either vector.
  const int max_size = std::max(from.ids_size(), into.ids_size());
  result_ids.Reserve(max_size);
  result_values.Reserve(max_size);

  int from_i = 0;
  int into_i = 0;
  while (from_i < from.ids_size() && into_i < into.ids_size()) {
    if (from.ids(from_i) < into.ids(into_i)) {
      result_ids.Add(from.ids(from_i));
      result_values.Add(from.values(from_i));
      ++from_i;
    } else if (from.ids(from_i) > into.ids(into_i)) {
      result_ids.Add(into.ids(into_i));
      result_values.Add(into.values(into_i));
      ++into_i;
    } else {  // from.ids(from_i) == into.ids(into_i)
      result_ids.Add(from.ids(from_i));
      result_values.Add(from.values(from_i));
      ++from_i;
      ++into_i;
    }
  }

  // At this point either from_i == from.ids_size() or to_i == to.ids_size() (or
  // both). And the one that is not empty, if it exists, has elements greater
  // than all other elements already inserted.
  const int remaining_size =
      std::max(from.ids_size() - from_i, into.ids_size() - into_i);
  result_ids.Reserve(result_ids.size() + remaining_size);
  result_values.Reserve(result_values.size() + remaining_size);
  for (; from_i < from.ids_size(); ++from_i) {
    result_ids.Add(from.ids(from_i));
    result_values.Add(from.values(from_i));
  }
  for (; into_i < into.ids_size(); ++into_i) {
    result_ids.Add(into.ids(into_i));
    result_values.Add(into.values(into_i));
  }

  into.Swap(&result);
}

}  // namespace internal
}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_CORE_MODEL_UPDATE_MERGE_H_
