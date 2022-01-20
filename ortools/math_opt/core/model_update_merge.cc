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

#include "ortools/math_opt/core/model_update_merge.h"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <string>
#include <utility>

#include "absl/container/flat_hash_set.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/math_opt/core/sparse_vector_view.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"

namespace operations_research {
namespace math_opt {

void MergeIntoUpdate(const ModelUpdateProto& from_new,
                     ModelUpdateProto& into_old) {
  // Merge the deleted variables. Note that we remove from the merge the
  // variables that were created in `into_old`. Below we will simply remove
  // those variables from the list of new variables in the merge; thus making
  // the update as if those variables never existed.
  internal::MergeIntoSortedIds(from_new.deleted_variable_ids(),
                               *into_old.mutable_deleted_variable_ids(),
                               /*deleted=*/into_old.new_variables().ids());
  internal::MergeIntoSortedIds(
      from_new.deleted_linear_constraint_ids(),
      *into_old.mutable_deleted_linear_constraint_ids(),
      /*deleted=*/into_old.new_linear_constraints().ids());

  // For variables and linear constraints updates, we want to ignore updates of:
  //
  // 1. variable or linear constraints deleted in `from_new` (that could have
  //    been updated in `into_old`).
  //
  // 2. variable or linear constraints created in `into_old`. For those the code
  //    of UpdateNewElementProperty() will use the new value directly as the
  //    value of the created variable.
  //
  // Thus we create here the list of indices to ignore when filtering updates
  // for both variables and linear constraints.
  google::protobuf::RepeatedField<int64_t>
      from_deleted_and_into_new_variable_ids = from_new.deleted_variable_ids();
  from_deleted_and_into_new_variable_ids.MergeFrom(
      into_old.new_variables().ids());

  google::protobuf::RepeatedField<int64_t>
      from_deleted_and_into_new_linear_constraint_ids =
          from_new.deleted_linear_constraint_ids();
  from_deleted_and_into_new_linear_constraint_ids.MergeFrom(
      into_old.new_linear_constraints().ids());

  // Merge updates of variable properties.
  internal::MergeIntoSparseVector(
      from_new.variable_updates().lower_bounds(),
      *into_old.mutable_variable_updates()->mutable_lower_bounds(),
      from_deleted_and_into_new_variable_ids);
  internal::MergeIntoSparseVector(
      from_new.variable_updates().upper_bounds(),
      *into_old.mutable_variable_updates()->mutable_upper_bounds(),
      from_deleted_and_into_new_variable_ids);
  internal::MergeIntoSparseVector(
      from_new.variable_updates().integers(),
      *into_old.mutable_variable_updates()->mutable_integers(),
      from_deleted_and_into_new_variable_ids);

  // Merge updates of linear constraints properties.
  internal::MergeIntoSparseVector(
      from_new.linear_constraint_updates().lower_bounds(),
      *into_old.mutable_linear_constraint_updates()->mutable_lower_bounds(),
      from_deleted_and_into_new_linear_constraint_ids);
  internal::MergeIntoSparseVector(
      from_new.linear_constraint_updates().upper_bounds(),
      *into_old.mutable_linear_constraint_updates()->mutable_upper_bounds(),
      from_deleted_and_into_new_linear_constraint_ids);

  // Merge new variables.
  //
  // The merge occurs in two steps:
  //
  // 1. For each property we remove from the merge the new variables from
  //    `into_old` that are removed in `from_new` since those don't have to
  //    exist. The code above has removed those from the deleted set to).
  //
  //    We also update the value of the property to the one of its update in
  //    `from_new` if it exists. The code above has removed those updates
  //    already.
  //
  // 2. We append all new variables of `from_new` at once by using MergeFrom()
  //    on the VariablesProto. No merges are needed for those since they can't
  //    have been know by `into_old`.
  if (!from_new.new_variables().ids().empty() &&
      !into_old.new_variables().ids().empty()) {
    CHECK_GT(*from_new.new_variables().ids().begin(),
             *into_old.new_variables().ids().rbegin());
  }
  internal::UpdateNewElementProperty(
      /*ids=*/into_old.new_variables().ids(),
      /*values=*/*into_old.mutable_new_variables()->mutable_lower_bounds(),
      /*deleted=*/from_new.deleted_variable_ids(),
      /*updates=*/from_new.variable_updates().lower_bounds());
  internal::UpdateNewElementProperty(
      /*ids=*/into_old.new_variables().ids(),
      /*values=*/*into_old.mutable_new_variables()->mutable_upper_bounds(),
      /*deleted=*/from_new.deleted_variable_ids(),
      /*updates=*/from_new.variable_updates().upper_bounds());
  internal::UpdateNewElementProperty(
      /*ids=*/into_old.new_variables().ids(),
      /*values=*/*into_old.mutable_new_variables()->mutable_integers(),
      /*deleted=*/from_new.deleted_variable_ids(),
      /*updates=*/from_new.variable_updates().integers());
  internal::UpdateNewElementProperty(
      /*ids=*/into_old.new_variables().ids(),
      /*values=*/*into_old.mutable_new_variables()->mutable_names(),
      /*deleted=*/from_new.deleted_variable_ids(),
      // We use an empty view here since names can't be updated.
      /*updates=*/SparseVectorView<std::string>());
  internal::RemoveDeletedIds(
      /*ids=*/*into_old.mutable_new_variables()->mutable_ids(),
      /*deleted=*/from_new.deleted_variable_ids());
  into_old.mutable_new_variables()->MergeFrom(from_new.new_variables());

  // Merge of new linear constraints. The algorithm is similar to variables; see
  // comment above for details.
  if (!from_new.new_linear_constraints().ids().empty() &&
      !into_old.new_linear_constraints().ids().empty()) {
    CHECK_GT(*from_new.new_linear_constraints().ids().begin(),
             *into_old.new_linear_constraints().ids().rbegin());
  }
  internal::UpdateNewElementProperty(
      /*ids=*/into_old.new_linear_constraints().ids(),
      /*values=*/
      *into_old.mutable_new_linear_constraints()->mutable_lower_bounds(),
      /*deleted=*/from_new.deleted_linear_constraint_ids(),
      /*updates=*/from_new.linear_constraint_updates().lower_bounds());
  internal::UpdateNewElementProperty(
      /*ids=*/into_old.new_linear_constraints().ids(),
      /*values=*/
      *into_old.mutable_new_linear_constraints()->mutable_upper_bounds(),
      /*deleted=*/from_new.deleted_linear_constraint_ids(),
      /*updates=*/from_new.linear_constraint_updates().upper_bounds());
  internal::UpdateNewElementProperty(
      /*ids=*/into_old.new_linear_constraints().ids(),
      /*values=*/*into_old.mutable_new_linear_constraints()->mutable_names(),
      /*deleted=*/from_new.deleted_linear_constraint_ids(),
      // We use an empty view here since names can't be updated.
      /*updates=*/SparseVectorView<std::string>());
  internal::RemoveDeletedIds(
      /*ids=*/*into_old.mutable_new_linear_constraints()->mutable_ids(),
      /*deleted=*/from_new.deleted_linear_constraint_ids());
  into_old.mutable_new_linear_constraints()->MergeFrom(
      from_new.new_linear_constraints());

  // Merge the objective.
  if (from_new.objective_updates().has_direction_update()) {
    into_old.mutable_objective_updates()->set_direction_update(
        from_new.objective_updates().direction_update());
  }
  if (from_new.objective_updates().has_offset_update()) {
    into_old.mutable_objective_updates()->set_offset_update(
        from_new.objective_updates().offset_update());
  }
  internal::MergeIntoSparseVector(
      from_new.objective_updates().linear_coefficients(),
      *into_old.mutable_objective_updates()->mutable_linear_coefficients(),
      from_new.deleted_variable_ids());
  internal::MergeIntoSparseDoubleMatrix(
      from_new.objective_updates().quadratic_coefficients(),
      *into_old.mutable_objective_updates()->mutable_quadratic_coefficients(),
      /*deleted_rows=*/from_new.deleted_variable_ids(),
      /*deleted_columns=*/from_new.deleted_variable_ids());

  // Merge the linear constraints coefficients.
  internal::MergeIntoSparseDoubleMatrix(
      from_new.linear_constraint_matrix_updates(),
      *into_old.mutable_linear_constraint_matrix_updates(),
      /*deleted_rows=*/from_new.deleted_linear_constraint_ids(),
      /*deleted_columns=*/from_new.deleted_variable_ids());
}

namespace internal {

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

}  // namespace internal
}  // namespace math_opt
}  // namespace operations_research
