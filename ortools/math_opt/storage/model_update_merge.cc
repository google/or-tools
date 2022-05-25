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

#include "ortools/math_opt/storage/model_update_merge.h"

#include <cstdint>
#include <iterator>
#include <string>

#include "ortools/base/logging.h"
#include "ortools/math_opt/core/sparse_vector_view.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/storage/proto_merging_utils.h"

namespace operations_research::math_opt {

void MergeIntoUpdate(const ModelUpdateProto& from_new,
                     ModelUpdateProto& into_old) {
  // Merge the deleted variables. Note that we remove from the merge the
  // variables that were created in `into_old`. Below we will simply remove
  // those variables from the list of new variables in the merge; thus making
  // the update as if those variables never existed.
  MergeIntoSortedIds(from_new.deleted_variable_ids(),
                     *into_old.mutable_deleted_variable_ids(),
                     /*deleted=*/into_old.new_variables().ids());
  MergeIntoSortedIds(from_new.deleted_linear_constraint_ids(),
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
  MergeIntoSparseVector(
      from_new.variable_updates().lower_bounds(),
      *into_old.mutable_variable_updates()->mutable_lower_bounds(),
      from_deleted_and_into_new_variable_ids);
  MergeIntoSparseVector(
      from_new.variable_updates().upper_bounds(),
      *into_old.mutable_variable_updates()->mutable_upper_bounds(),
      from_deleted_and_into_new_variable_ids);
  MergeIntoSparseVector(
      from_new.variable_updates().integers(),
      *into_old.mutable_variable_updates()->mutable_integers(),
      from_deleted_and_into_new_variable_ids);

  // Merge updates of linear constraints properties.
  MergeIntoSparseVector(
      from_new.linear_constraint_updates().lower_bounds(),
      *into_old.mutable_linear_constraint_updates()->mutable_lower_bounds(),
      from_deleted_and_into_new_linear_constraint_ids);
  MergeIntoSparseVector(
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
  UpdateNewElementProperty(
      /*ids=*/into_old.new_variables().ids(),
      /*values=*/*into_old.mutable_new_variables()->mutable_lower_bounds(),
      /*deleted=*/from_new.deleted_variable_ids(),
      /*updates=*/from_new.variable_updates().lower_bounds());
  UpdateNewElementProperty(
      /*ids=*/into_old.new_variables().ids(),
      /*values=*/*into_old.mutable_new_variables()->mutable_upper_bounds(),
      /*deleted=*/from_new.deleted_variable_ids(),
      /*updates=*/from_new.variable_updates().upper_bounds());
  UpdateNewElementProperty(
      /*ids=*/into_old.new_variables().ids(),
      /*values=*/*into_old.mutable_new_variables()->mutable_integers(),
      /*deleted=*/from_new.deleted_variable_ids(),
      /*updates=*/from_new.variable_updates().integers());
  UpdateNewElementProperty(
      /*ids=*/into_old.new_variables().ids(),
      /*values=*/*into_old.mutable_new_variables()->mutable_names(),
      /*deleted=*/from_new.deleted_variable_ids(),
      // We use an empty view here since names can't be updated.
      /*updates=*/SparseVectorView<std::string>());
  RemoveDeletedIds(
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
  UpdateNewElementProperty(
      /*ids=*/into_old.new_linear_constraints().ids(),
      /*values=*/
      *into_old.mutable_new_linear_constraints()->mutable_lower_bounds(),
      /*deleted=*/from_new.deleted_linear_constraint_ids(),
      /*updates=*/from_new.linear_constraint_updates().lower_bounds());
  UpdateNewElementProperty(
      /*ids=*/into_old.new_linear_constraints().ids(),
      /*values=*/
      *into_old.mutable_new_linear_constraints()->mutable_upper_bounds(),
      /*deleted=*/from_new.deleted_linear_constraint_ids(),
      /*updates=*/from_new.linear_constraint_updates().upper_bounds());
  UpdateNewElementProperty(
      /*ids=*/into_old.new_linear_constraints().ids(),
      /*values=*/*into_old.mutable_new_linear_constraints()->mutable_names(),
      /*deleted=*/from_new.deleted_linear_constraint_ids(),
      // We use an empty view here since names can't be updated.
      /*updates=*/SparseVectorView<std::string>());
  RemoveDeletedIds(
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
  MergeIntoSparseVector(
      from_new.objective_updates().linear_coefficients(),
      *into_old.mutable_objective_updates()->mutable_linear_coefficients(),
      from_new.deleted_variable_ids());
  MergeIntoSparseDoubleMatrix(
      from_new.objective_updates().quadratic_coefficients(),
      *into_old.mutable_objective_updates()->mutable_quadratic_coefficients(),
      /*deleted_rows=*/from_new.deleted_variable_ids(),
      /*deleted_columns=*/from_new.deleted_variable_ids());

  // Merge the linear constraints coefficients.
  MergeIntoSparseDoubleMatrix(
      from_new.linear_constraint_matrix_updates(),
      *into_old.mutable_linear_constraint_matrix_updates(),
      /*deleted_rows=*/from_new.deleted_linear_constraint_ids(),
      /*deleted_columns=*/from_new.deleted_variable_ids());
}

}  // namespace operations_research::math_opt
