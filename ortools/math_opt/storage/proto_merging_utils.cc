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

#include "ortools/math_opt/storage/proto_merging_utils.h"

#include <cstdint>
#include <utility>

#include "absl/container/flat_hash_set.h"
#include "ortools/math_opt/sparse_containers.pb.h"

namespace operations_research::math_opt {

void RemoveDeletedIds(google::protobuf::RepeatedField<int64_t>& ids,
                      const google::protobuf::RepeatedField<int64_t>& deleted) {
  int next_insertion_point = 0;
  int deleted_i = 0;
  for (const int64_t id : ids) {
    while (deleted_i < deleted.size() && deleted[deleted_i] < id) {
      ++deleted_i;
    }
    if (deleted_i < deleted.size() && deleted[deleted_i] == id) {
      continue;
    }
    ids[next_insertion_point] = id;
    ++next_insertion_point;
  }
  ids.Truncate(next_insertion_point);
}

void MergeIntoSortedIds(
    const google::protobuf::RepeatedField<int64_t>& from_new,
    google::protobuf::RepeatedField<int64_t>& into_old,
    const google::protobuf::RepeatedField<int64_t>& deleted) {
  google::protobuf::RepeatedField<int64_t> result;

  int from_new_i = 0;
  int into_old_i = 0;
  int deleted_i = 0;

  // Functions that adds the input id to the result if it is not in deleted. It
  // updates deleted_i as a side effect too.
  const auto add_if_not_deleted = [&](const int64_t id) {
    while (deleted_i < deleted.size() && deleted[deleted_i] < id) {
      ++deleted_i;
    }
    if (deleted_i == deleted.size() || deleted[deleted_i] != id) {
      result.Add(id);
    }
  };

  while (from_new_i < from_new.size() && into_old_i < into_old.size()) {
    if (from_new[from_new_i] < into_old[into_old_i]) {
      add_if_not_deleted(from_new[from_new_i]);
      ++from_new_i;
    } else if (from_new[from_new_i] > into_old[into_old_i]) {
      add_if_not_deleted(into_old[into_old_i]);
      ++into_old_i;
    } else {  // from_new[from_new_i] == into_old[into_old_i]
      add_if_not_deleted(from_new[from_new_i]);
      ++from_new_i;
      ++into_old_i;
    }
  }

  // At this point either from_new_i == from_new.size() or to_i == to.size() or
  // both. And the one that is not empty, if it exists, has elements greater
  // than all other elements already inserted.
  for (; from_new_i < from_new.size(); ++from_new_i) {
    add_if_not_deleted(from_new[from_new_i]);
  }
  for (; into_old_i < into_old.size(); ++into_old_i) {
    add_if_not_deleted(into_old[into_old_i]);
  }

  into_old.Swap(&result);
}

void MergeIntoSparseDoubleMatrix(
    const SparseDoubleMatrixProto& from_new, SparseDoubleMatrixProto& into_old,
    const google::protobuf::RepeatedField<int64_t>& deleted_rows,
    const google::protobuf::RepeatedField<int64_t>& deleted_columns) {
  SparseDoubleMatrixProto result;
  auto& result_row_ids = *result.mutable_row_ids();
  auto& result_column_ids = *result.mutable_column_ids();
  auto& result_coefficients = *result.mutable_coefficients();

  // Contrary to rows that are traversed in order (the matrix is using row-major
  // order), columns are not. Thus we would have to start the iteration on
  // deleted_columns for each new row of the matrix if we wanted to use the same
  // approach as with rows. This would be O(num_rows * num_deleted_columns).
  //
  // Here we use a hash-set to be O(num_matrix_elements +
  // num_deleted_columns). The downside is that we consumed
  // O(num_deleted_columns) additional memory.
  //
  // We could have used binary search that would be O(num_matrix_elements *
  // lg(num_deleted_columns)) but without additional memory.
  const absl::flat_hash_set<int64_t> deleted_columns_set(
      deleted_columns.begin(), deleted_columns.end());

  int from_new_i = 0;
  int into_old_i = 0;
  int deleted_rows_i = 0;

  // Functions that adds the input tuple (row_id, col_id, coefficient) to the
  // result if the input row_id and col_id are not in deleted_rows or
  // deleted_columns. It updates deleted_rows_i and deleted_columns_i as a side
  // effect too.
  const auto add_if_not_deleted = [&](const int64_t row_id,
                                      const int64_t col_id,
                                      const double coefficient) {
    while (deleted_rows_i < deleted_rows.size() &&
           deleted_rows[deleted_rows_i] < row_id) {
      ++deleted_rows_i;
    }
    if ((deleted_rows_i != deleted_rows.size() &&
         deleted_rows[deleted_rows_i] == row_id) ||
        deleted_columns_set.contains(col_id)) {
      return;
    }
    result_row_ids.Add(row_id);
    result_column_ids.Add(col_id);
    result_coefficients.Add(coefficient);
  };

  while (from_new_i < from_new.row_ids_size() &&
         into_old_i < into_old.row_ids_size()) {
    // Matrices are in row-major order and std::pair comparison is
    // lexicographical, thus matrices are sorted in the natural order of pairs
    // of coordinates (row, col).
    const auto from_new_coordinates = std::make_pair(
        from_new.row_ids(from_new_i), from_new.column_ids(from_new_i));
    const auto into_old_coordinates = std::make_pair(
        into_old.row_ids(into_old_i), into_old.column_ids(into_old_i));
    if (from_new_coordinates < into_old_coordinates) {
      add_if_not_deleted(from_new_coordinates.first,
                         from_new_coordinates.second,
                         from_new.coefficients(from_new_i));
      ++from_new_i;
    } else if (from_new_coordinates > into_old_coordinates) {
      add_if_not_deleted(into_old_coordinates.first,
                         into_old_coordinates.second,
                         into_old.coefficients(into_old_i));
      ++into_old_i;
    } else {  // from_new_coordinates == into_old_coordinates
      add_if_not_deleted(from_new_coordinates.first,
                         from_new_coordinates.second,
                         from_new.coefficients(from_new_i));
      ++from_new_i;
      ++into_old_i;
    }
  }

  // At this point either from_new_i == from_new.row_ids_size() or
  // to_i == to.row_ids_size() (or both). And the one that is not empty, if it
  // exists, has elements greater than all other elements already inserted.
  for (; from_new_i < from_new.row_ids_size(); ++from_new_i) {
    add_if_not_deleted(from_new.row_ids(from_new_i),
                       from_new.column_ids(from_new_i),
                       from_new.coefficients(from_new_i));
  }
  for (; into_old_i < into_old.row_ids_size(); ++into_old_i) {
    add_if_not_deleted(into_old.row_ids(into_old_i),
                       into_old.column_ids(into_old_i),
                       into_old.coefficients(into_old_i));
  }

  into_old.Swap(&result);
}

}  // namespace operations_research::math_opt
