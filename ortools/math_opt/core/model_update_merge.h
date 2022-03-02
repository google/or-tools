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
#include "ortools/base/protobuf_util.h"
#include "ortools/math_opt/core/sparse_vector_view.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"

namespace operations_research {
namespace math_opt {

// Merges the `from_new` update into the `into_old` one.
//
// The `from_new` update must represent an update that happens after the
// `into_old` one is applied. Thus when the two updates have overlaps, the
// `from_new` one overrides the value of the `into_old` one (i.e. the `from_new`
// update is expected to be more recent).
//
// This function also CHECKs that the ids of new variables and constraints in
// `from_new` are greater than the ones in `into_old` (as expected if `from_new`
// happens after `into_old`).
//
// Note that the complexity is O(size(from_new) + size(into_old)) thus if you
// need to merge a long list of updates this may be not efficient enough. In
// that case an n-way merge would be needed to be implemented here.
void MergeIntoUpdate(const ModelUpdateProto& from_new,
                     ModelUpdateProto& into_old);

namespace internal {

// Removes from the sorted list `ids` all elements found in the sorted list
// `deleted`. The elements should be unique in each sorted list.
void RemoveDeletedIds(google::protobuf::RepeatedField<int64_t>& ids,
                      const google::protobuf::RepeatedField<int64_t>& deleted);

// Merges the `from_new` list of sorted ids into the `into_old` one. Elements
// appearing in `from_new` that already exist in `into_old` are ignored.
//
// The input `deleted` should contains a sorted list of ids of elements that
// have been deleted and should be removed from the merge.
//
// The elements should be unique in each sorted list.
void MergeIntoSortedIds(
    const google::protobuf::RepeatedField<int64_t>& from_new,
    google::protobuf::RepeatedField<int64_t>& into_old,
    const google::protobuf::RepeatedField<int64_t>& deleted);

// Merges the `from_new` sparse vector into the `into_old` one. When the two
// vectors have overlaps, the value in `from_new` is used to overwrite the one
// in `into_old`.
//
// The input `deleted` should contains a sorted list of unique ids of elements
// that have been deleted and should be removed from the merge.
//
// The SparseVector type is either SparseDoubleVectorProto or
// SparseBoolVectorProto.
template <typename SparseVector>
void MergeIntoSparseVector(
    const SparseVector& from_new, SparseVector& into_old,
    const google::protobuf::RepeatedField<int64_t>& deleted);

// Merges the `from_new` sparse matrix into the `into_old` one. When the two
// matrices have overlaps, the value in `from_new` is used to overwrite the one
// in `into_old`.
//
// The input `deleted_rows` and `deleted_columns` should contains sorted lists
// of unique ids of rows and cols that have been deleted and should be removed
// from the merge.
void MergeIntoSparseDoubleMatrix(
    const SparseDoubleMatrixProto& from_new, SparseDoubleMatrixProto& into_old,
    const google::protobuf::RepeatedField<int64_t>& deleted_rows,
    const google::protobuf::RepeatedField<int64_t>& deleted_columns);

// Updates a "property" repeated field of a ModelUpdateProto.new_variables or
// ModelUpdateProto.new_linear_constraints.
//
// The `ids` input corresponds to VariablesProto.ids (or
// LinearConstraintsProto.ids), and the values one to one property (for example
// VariablesProto.lower_bounds). Values corresponding to ids in `deleted` are
// removed. For the ids that have a value in `updates`, this value is used to
// replace the existing one.
//
// The type SparseVector can either be a sparse proto like
// SparseDoubleVectorProto or a SparseVectorView. The type RepeatedField is
// usually a google::protobuf::RepeatedField but it can be also a
// RepeatedPtrField<std::string> to deal with the `names` property.
template <typename RepeatedField, typename SparseVector>
void UpdateNewElementProperty(
    const google::protobuf::RepeatedField<int64_t>& ids, RepeatedField& values,
    const google::protobuf::RepeatedField<int64_t>& deleted,
    const SparseVector& updates);

}  // namespace internal

////////////////////////////////////////////////////////////////////////////////
// Inline functions implementations.
////////////////////////////////////////////////////////////////////////////////

namespace internal {

template <typename SparseVector>
void MergeIntoSparseVector(
    const SparseVector& from_new, SparseVector& into_old,
    const google::protobuf::RepeatedField<int64_t>& deleted) {
  CHECK_EQ(from_new.ids_size(), from_new.values_size());
  CHECK_EQ(into_old.ids_size(), into_old.values_size());

  SparseVector result;
  auto& result_ids = *result.mutable_ids();
  auto& result_values = *result.mutable_values();

  int from_new_i = 0;
  int into_old_i = 0;
  int deleted_i = 0;

  // Functions that adds the input pair (id, value) to the result if the input
  // id is not in deleted. It updates deleted_i as a side effect too.
  const auto add_if_not_deleted =
      [&](const int64_t id, const sparse_value_type<SparseVector>& value) {
        while (deleted_i < deleted.size() && deleted[deleted_i] < id) {
          ++deleted_i;
        }
        if (deleted_i == deleted.size() || deleted[deleted_i] != id) {
          result_ids.Add(id);
          result_values.Add(value);
        }
      };

  while (from_new_i < from_new.ids_size() && into_old_i < into_old.ids_size()) {
    if (from_new.ids(from_new_i) < into_old.ids(into_old_i)) {
      add_if_not_deleted(from_new.ids(from_new_i), from_new.values(from_new_i));
      ++from_new_i;
    } else if (from_new.ids(from_new_i) > into_old.ids(into_old_i)) {
      add_if_not_deleted(into_old.ids(into_old_i), into_old.values(into_old_i));
      ++into_old_i;
    } else {  // from_new.ids(from_new_i) == into_old.ids(into_old_i)
      add_if_not_deleted(from_new.ids(from_new_i), from_new.values(from_new_i));
      ++from_new_i;
      ++into_old_i;
    }
  }

  // At this point either from_new_i == from_new.ids_size() or to_i ==
  // to.ids_size() (or both). And the one that is not empty, if it exists, has
  // elements greater than all other elements already inserted.
  for (; from_new_i < from_new.ids_size(); ++from_new_i) {
    add_if_not_deleted(from_new.ids(from_new_i), from_new.values(from_new_i));
  }
  for (; into_old_i < into_old.ids_size(); ++into_old_i) {
    add_if_not_deleted(into_old.ids(into_old_i), into_old.values(into_old_i));
  }

  into_old.Swap(&result);
}

template <typename RepeatedField, typename SparseVector>
void UpdateNewElementProperty(
    const google::protobuf::RepeatedField<int64_t>& ids, RepeatedField& values,
    const google::protobuf::RepeatedField<int64_t>& deleted,
    const SparseVector& updates) {
  int next_insertion_point = 0;
  int deleted_i = 0;
  int updates_i = 0;

  for (int i = 0; i < ids.size(); ++i) {
    const int64_t id = ids[i];

    while (deleted_i < deleted.size() && deleted[deleted_i] < id) {
      ++deleted_i;
    }
    if (deleted_i < deleted.size() && deleted[deleted_i] == id) {
      continue;
    }

    while (updates_i < updates.ids_size() && updates.ids(updates_i) < id) {
      ++updates_i;
    }
    if (updates_i < updates.ids_size() && updates.ids(updates_i) == id) {
      values[next_insertion_point] = updates.values(updates_i);
    } else {
      // Here we use SwapElements() to prevent copies when `values` is a
      // RepeatedPtrField<std::string>.
      values.SwapElements(next_insertion_point, i);
    }
    ++next_insertion_point;
  }

  // We can't use value.Truncate() here since RepeatedPtrField<std::string> does
  // not implement it.
  google::protobuf::util::Truncate(&values, next_insertion_point);
}

}  // namespace internal
}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_CORE_MODEL_UPDATE_MERGE_H_
