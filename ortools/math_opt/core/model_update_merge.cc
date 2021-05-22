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
#include <utility>

#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"

namespace operations_research {
namespace math_opt {

void MergeIntoUpdate(const ModelUpdateProto& from, ModelUpdateProto& into) {
  internal::MergeIntoSortedIds(from.deleted_variable_ids(),
                               *into.mutable_deleted_variable_ids());
  internal::MergeIntoSortedIds(from.deleted_linear_constraint_ids(),
                               *into.mutable_deleted_linear_constraint_ids());

  internal::MergeIntoSparseVector(
      from.variable_updates().lower_bounds(),
      *into.mutable_variable_updates()->mutable_lower_bounds());
  internal::MergeIntoSparseVector(
      from.variable_updates().upper_bounds(),
      *into.mutable_variable_updates()->mutable_upper_bounds());
  internal::MergeIntoSparseVector(
      from.variable_updates().integers(),
      *into.mutable_variable_updates()->mutable_integers());

  internal::MergeIntoSparseVector(
      from.linear_constraint_updates().lower_bounds(),
      *into.mutable_linear_constraint_updates()->mutable_lower_bounds());
  internal::MergeIntoSparseVector(
      from.linear_constraint_updates().upper_bounds(),
      *into.mutable_linear_constraint_updates()->mutable_upper_bounds());

  if (!from.new_variables().ids().empty() &&
      !into.new_variables().ids().empty()) {
    CHECK_GT(*from.new_variables().ids().begin(),
             *into.new_variables().ids().rbegin());
  }
  into.mutable_new_variables()->MergeFrom(from.new_variables());

  if (!from.new_linear_constraints().ids().empty() &&
      !into.new_linear_constraints().ids().empty()) {
    CHECK_GT(*from.new_linear_constraints().ids().begin(),
             *into.new_linear_constraints().ids().rbegin());
  }
  into.mutable_new_linear_constraints()->MergeFrom(
      from.new_linear_constraints());

  if (from.objective_updates().has_direction_update()) {
    into.mutable_objective_updates()->set_direction_update(
        from.objective_updates().direction_update());
  }
  if (from.objective_updates().has_offset_update()) {
    into.mutable_objective_updates()->set_offset_update(
        from.objective_updates().offset_update());
  }
  internal::MergeIntoSparseVector(
      from.objective_updates().linear_coefficients(),
      *into.mutable_objective_updates()->mutable_linear_coefficients());

  internal::MergeIntoSparseDoubleMatrix(
      from.linear_constraint_matrix_updates(),
      *into.mutable_linear_constraint_matrix_updates());
}

namespace internal {

void MergeIntoSortedIds(const google::protobuf::RepeatedField<int64_t>& from,
                        google::protobuf::RepeatedField<int64_t>& into) {
  google::protobuf::RepeatedField<int64_t> result;

  // We don't reserve the sum of the sizes of both repeated fields since they
  // can contain overlapping ids. But we know that we will have at least the max
  // length of either repeated field.
  result.Reserve(std::max(from.size(), into.size()));

  int from_i = 0;
  int into_i = 0;
  while (from_i < from.size() && into_i < into.size()) {
    if (from[from_i] < into[into_i]) {
      result.Add(from[from_i]);
      ++from_i;
    } else if (from[from_i] > into[into_i]) {
      result.Add(into[into_i]);
      ++into_i;
    } else {  // from[from_i] == into[into_i]
      result.Add(from[from_i]);
      ++from_i;
      ++into_i;
    }
  }

  // At this point either from_i == from.size() or to_i == to.size() or
  // both. And the one that is not empty, if it exists, has elements greater
  // than all other elements already inserted.
  result.Reserve(result.size() +
                 std::max(from.size() - from_i, into.size() - into_i));
  for (; from_i < from.size(); ++from_i) {
    result.Add(from[from_i]);
  }
  for (; into_i < into.size(); ++into_i) {
    result.Add(into[into_i]);
  }

  into.Swap(&result);
}

void MergeIntoSparseDoubleMatrix(const SparseDoubleMatrixProto& from,
                                 SparseDoubleMatrixProto& into) {
  SparseDoubleMatrixProto result;
  auto& result_row_ids = *result.mutable_row_ids();
  auto& result_column_ids = *result.mutable_column_ids();
  auto& result_coefficients = *result.mutable_coefficients();

  // We don't reserve the sum of the sizes of both sparse matrices since they
  // can contain overlapping tuples. But we know that we will have at least the
  // max length of either matrix.
  const int max_size = std::max(from.row_ids_size(), into.row_ids_size());
  result_row_ids.Reserve(max_size);
  result_column_ids.Reserve(max_size);
  result_coefficients.Reserve(max_size);

  int from_i = 0;
  int into_i = 0;
  while (from_i < from.row_ids_size() && into_i < into.row_ids_size()) {
    // Matrices are in row-major order and std::pair comparison is
    // lexicographical, thus matrices are sorted in the natural order of pairs
    // of coordinates (row, col).
    const auto from_coordinates =
        std::make_pair(from.row_ids(from_i), from.column_ids(from_i));
    const auto into_coordinates =
        std::make_pair(into.row_ids(into_i), into.column_ids(into_i));
    if (from_coordinates < into_coordinates) {
      result_row_ids.Add(from_coordinates.first);
      result_column_ids.Add(from_coordinates.second);
      result_coefficients.Add(from.coefficients(from_i));
      ++from_i;
    } else if (from_coordinates > into_coordinates) {
      result_row_ids.Add(into_coordinates.first);
      result_column_ids.Add(into_coordinates.second);
      result_coefficients.Add(into.coefficients(into_i));
      ++into_i;
    } else {  // from_coordinates == into_coordinates
      result_row_ids.Add(from_coordinates.first);
      result_column_ids.Add(from_coordinates.second);
      result_coefficients.Add(from.coefficients(from_i));
      ++from_i;
      ++into_i;
    }
  }

  // At this point either from_i == from.row_ids_size() or
  // to_i == to.row_ids_size() (or both). And the one that is not empty, if it
  // exists, has elements greater than all other elements already inserted.
  const int remaining_size =
      std::max(from.row_ids_size() - from_i, into.row_ids_size() - into_i);
  result_row_ids.Reserve(result_row_ids.size() + remaining_size);
  result_column_ids.Reserve(result_column_ids.size() + remaining_size);
  result_coefficients.Reserve(result_coefficients.size() + remaining_size);
  for (; from_i < from.row_ids_size(); ++from_i) {
    result_row_ids.Add(from.row_ids(from_i));
    result_column_ids.Add(from.column_ids(from_i));
    result_coefficients.Add(from.coefficients(from_i));
  }
  for (; into_i < into.row_ids_size(); ++into_i) {
    result_row_ids.Add(into.row_ids(into_i));
    result_column_ids.Add(into.column_ids(into_i));
    result_coefficients.Add(into.coefficients(into_i));
  }

  into.Swap(&result);
}

}  // namespace internal
}  // namespace math_opt
}  // namespace operations_research
