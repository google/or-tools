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

#include "ortools/math_opt/core/model_storage.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/memory/memory.h"
#include "absl/meta/type_traits.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/base/map_util.h"
#include "ortools/base/status_macros.h"
#include "ortools/base/strong_int.h"
#include "ortools/math_opt/core/model_update_merge.h"
#include "ortools/math_opt/core/sparse_vector_view.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/solution.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/validators/model_validator.h"

namespace operations_research {
namespace math_opt {

namespace {

template <typename K, typename V>
std::vector<K> MapKeys(const absl::flat_hash_map<K, V>& in_map) {
  std::vector<K> keys;
  keys.reserve(in_map.size());
  for (const auto& key_pair : in_map) {
    keys.push_back(key_pair.first);
  }
  return keys;
}

template <typename K, typename V>
std::vector<K> SortedMapKeys(const absl::flat_hash_map<K, V>& in_map) {
  std::vector<K> keys = MapKeys(in_map);
  std::sort(keys.begin(), keys.end());
  return keys;
}

template <typename T>
std::vector<T> SortedSetKeys(const absl::flat_hash_set<T>& in_set) {
  std::vector<T> keys;
  keys.reserve(in_set.size());
  for (const auto& key : in_set) {
    keys.push_back(key);
  }
  std::sort(keys.begin(), keys.end());
  return keys;
}

// ids should be sorted.
template <typename IdType>
void AppendFromMapOrDefault(const absl::Span<const IdType> ids,
                            const absl::flat_hash_map<IdType, double>& values,
                            SparseDoubleVectorProto& sparse_vector) {
  for (const IdType id : ids) {
    sparse_vector.add_ids(id.value());
    sparse_vector.add_values(gtl::FindWithDefault(values, id));
  }
}

// ids should be sorted.
template <typename IdType, typename IdIterable>
void AppendFromMapIfPresent(const IdIterable& ids,
                            const absl::flat_hash_map<IdType, double>& values,
                            SparseDoubleVectorProto& sparse_vector) {
  for (const IdType id : ids) {
    const double* const double_value = gtl::FindOrNull(values, id);
    if (double_value != nullptr) {
      sparse_vector.add_ids(id.value());
      sparse_vector.add_values(*double_value);
    }
  }
}

template <typename IdType, typename DataType>
void AppendFromMap(const absl::flat_hash_set<IdType>& dirty_keys,
                   const absl::flat_hash_map<IdType, DataType>& values,
                   double DataType::*field,
                   SparseDoubleVectorProto& sparse_vector) {
  for (const IdType id : SortedSetKeys(dirty_keys)) {
    sparse_vector.add_ids(id.value());
    sparse_vector.add_values(values.at(id).*field);
  }
}

template <typename T>
absl::flat_hash_map<T, BasisStatusProto> SparseBasisVectorToMap(
    const SparseBasisStatusVector& sparse_vector) {
  absl::flat_hash_map<T, BasisStatusProto> result;
  CHECK_EQ(sparse_vector.ids_size(), sparse_vector.values_size());
  result.reserve(sparse_vector.ids_size());
  for (const auto [id, value] : MakeView(sparse_vector)) {
    gtl::InsertOrDie(&result, T(id), static_cast<BasisStatusProto>(value));
  }
  return result;
}

// If an element in keys is not found in coefficients, it is set to 0.0 in
// matrix. Keys must be in lexicographic ordering (i.e. sorted).
// NOTE: This signature can be updated to take a Span instead of a vector if
// needed in the future, but it required specifying parameters at the callsites.
template <typename RK, typename CK>
SparseDoubleMatrixProto ExportMatrix(
    const absl::flat_hash_map<std::pair<RK, CK>, double>& coefficients,
    const std::vector<std::pair<RK, CK>>& keys) {
  SparseDoubleMatrixProto matrix;
  matrix.mutable_row_ids()->Reserve(keys.size());
  matrix.mutable_column_ids()->Reserve(keys.size());
  matrix.mutable_coefficients()->Reserve(keys.size());
  for (const auto [row_id, column_id] : keys) {
    matrix.add_row_ids(row_id.value());
    matrix.add_column_ids(column_id.value());
    matrix.add_coefficients(
        gtl::FindWithDefault(coefficients, {row_id, column_id}));
  }
  return matrix;
}

}  // namespace

absl::StatusOr<std::unique_ptr<ModelStorage>> ModelStorage::FromModelProto(
    const ModelProto& model_proto) {
  // We don't check names since ModelStorage does not do so before exporting
  // models. Thus a model built by ModelStorage can contain duplicated
  // names. And since we use FromModelProto() to implement Clone(), we must make
  // sure duplicated names don't fail.
  RETURN_IF_ERROR(ValidateModel(model_proto, /*check_names=*/false));

  auto storage = std::make_unique<ModelStorage>(model_proto.name());

  // Add variables.
  storage->AddVariables(model_proto.variables());

  // Set the objective.
  storage->set_is_maximize(model_proto.objective().maximize());
  storage->set_objective_offset(model_proto.objective().offset());
  storage->UpdateLinearObjectiveCoefficients(
      model_proto.objective().linear_coefficients());
  storage->UpdateQuadraticObjectiveCoefficients(
      model_proto.objective().quadratic_coefficients());

  // Add linear constraints.
  storage->AddLinearConstraints(model_proto.linear_constraints());

  // Set the linear constraints coefficients.
  storage->UpdateLinearConstraintCoefficients(
      model_proto.linear_constraint_matrix());

  return storage;
}

void ModelStorage::UpdateLinearObjectiveCoefficients(
    const SparseDoubleVectorProto& coefficients) {
  for (const auto [var_id, value] : MakeView(coefficients)) {
    set_linear_objective_coefficient(VariableId(var_id), value);
  }
}

void ModelStorage::UpdateQuadraticObjectiveCoefficients(
    const SparseDoubleMatrixProto& coefficients) {
  for (int i = 0; i < coefficients.row_ids_size(); ++i) {
    // This call is valid since this is an upper triangular matrix; there is no
    // duplicated terms.
    set_quadratic_objective_coefficient(VariableId(coefficients.row_ids(i)),
                                        VariableId(coefficients.column_ids(i)),
                                        coefficients.coefficients(i));
  }
}

void ModelStorage::UpdateLinearConstraintCoefficients(
    const SparseDoubleMatrixProto& coefficients) {
  for (int i = 0; i < coefficients.row_ids_size(); ++i) {
    // This call is valid since there are no duplicated pairs.
    set_linear_constraint_coefficient(
        LinearConstraintId(coefficients.row_ids(i)),
        VariableId(coefficients.column_ids(i)), coefficients.coefficients(i));
  }
}

std::unique_ptr<ModelStorage> ModelStorage::Clone() const {
  absl::StatusOr<std::unique_ptr<ModelStorage>> clone =
      ModelStorage::FromModelProto(ExportModel());
  // Unless there is a very serious bug, a model exported by ExportModel()
  // should always be valid.
  CHECK_OK(clone.status());

  // Update the next ids so that the clone does not reused any deleted id from
  // the original.
  CHECK_LE(clone.value()->next_variable_id_, next_variable_id_);
  clone.value()->next_variable_id_ = next_variable_id_;
  CHECK_LE(clone.value()->next_linear_constraint_id_,
           next_linear_constraint_id_);
  clone.value()->next_linear_constraint_id_ = next_linear_constraint_id_;

  return std::move(clone).value();
}

VariableId ModelStorage::AddVariable(const double lower_bound,
                                     const double upper_bound,
                                     const bool is_integer,
                                     const absl::string_view name) {
  const VariableId id = next_variable_id_;
  AddVariableInternal(/*id=*/id,
                      /*lower_bound=*/lower_bound,
                      /*upper_bound=*/upper_bound,
                      /*is_integer=*/is_integer,
                      /*name=*/name);
  return id;
}

void ModelStorage::AddVariableInternal(const VariableId id,
                                       const double lower_bound,
                                       const double upper_bound,
                                       const bool is_integer,
                                       const absl::string_view name) {
  CHECK_GE(id, next_variable_id_);
  next_variable_id_ = id + VariableId(1);

  VariableData& var_data = variables_[id];
  var_data.lower_bound = lower_bound;
  var_data.upper_bound = upper_bound;
  var_data.is_integer = is_integer;
  var_data.name = std::string(name);
  if (!lazy_matrix_columns_.empty()) {
    gtl::InsertOrDie(&lazy_matrix_columns_, id, {});
  }
  if (!lazy_quadratic_objective_by_variable_.empty()) {
    gtl::InsertOrDie(&lazy_quadratic_objective_by_variable_, id, {});
  }
}

void ModelStorage::AddVariables(const VariablesProto& variables) {
  const bool has_names = !variables.names().empty();
  for (int v = 0; v < variables.ids_size(); ++v) {
    // This call is valid since ids are unique and increasing.
    AddVariableInternal(VariableId(variables.ids(v)),
                        /*lower_bound=*/variables.lower_bounds(v),
                        /*upper_bound=*/variables.upper_bounds(v),
                        /*is_integer=*/variables.integers(v),
                        has_names ? variables.names(v) : absl::string_view());
  }
}

void ModelStorage::DeleteVariable(const VariableId id) {
  CHECK(variables_.contains(id));
  EnsureLazyMatrixColumns();
  EnsureLazyMatrixRows();
  linear_objective_.erase(id);
  if (id < variables_checkpoint_) {
    dirty_variable_deletes_.insert(id);
    dirty_variable_lower_bounds_.erase(id);
    dirty_variable_upper_bounds_.erase(id);
    dirty_variable_is_integer_.erase(id);
    dirty_linear_objective_coefficients_.erase(id);
  }
  // If we do not have any quadratic updates to delete, we would like to avoid
  // initializing the lazy data structures. The updates might tracked in:
  //   1. dirty_quadratic_objective_coefficients_ (both variables old)
  //   2. quadratic_objective_ (at least one new variable)
  // If both maps are empty, we can skip the update and initializiation. Note
  // that we could be a bit more clever here based on whether the deleted
  // variable is new or old, but that makes the logic more complex.
  if (!quadratic_objective_.empty() ||
      !dirty_quadratic_objective_coefficients_.empty()) {
    EnsureLazyQuadraticObjective();
    const auto related_variables =
        lazy_quadratic_objective_by_variable_.extract(id);
    for (const VariableId other_id : related_variables.mapped()) {
      // Due to the extract above, the at lookup will fail if other_id == id.
      if (id != other_id) {
        CHECK_GT(lazy_quadratic_objective_by_variable_.at(other_id).erase(id),
                 0);
      }
      const auto ordered_pair = internal::MakeOrderedPair(id, other_id);
      quadratic_objective_.erase(ordered_pair);
      // We can only have a dirty update to wipe clean if both variables are old
      if (id < variables_checkpoint_ && other_id < variables_checkpoint_) {
        dirty_quadratic_objective_coefficients_.erase(ordered_pair);
      }
    }
  }
  for (const LinearConstraintId related_constraint :
       lazy_matrix_columns_.at(id)) {
    CHECK_GT(lazy_matrix_rows_.at(related_constraint).erase(id), 0);
    CHECK_GT(linear_constraint_matrix_.erase({related_constraint, id}), 0);
    if (id < variables_checkpoint_ &&
        related_constraint < linear_constraints_checkpoint_) {
      dirty_linear_constraint_matrix_keys_.erase({related_constraint, id});
    }
  }
  CHECK_GT(lazy_matrix_columns_.erase(id), 0);
  variables_.erase(id);
}

std::vector<VariableId> ModelStorage::variables() const {
  return MapKeys(variables_);
}

std::vector<VariableId> ModelStorage::SortedVariables() const {
  return SortedMapKeys(variables_);
}

LinearConstraintId ModelStorage::AddLinearConstraint(
    const double lower_bound, const double upper_bound,
    const absl::string_view name) {
  const LinearConstraintId id = next_linear_constraint_id_;
  AddLinearConstraintInternal(/*id=*/id, /*lower_bound=*/lower_bound,
                              /*upper_bound=*/upper_bound,
                              /*name=*/name);
  return id;
}

void ModelStorage::AddLinearConstraintInternal(const LinearConstraintId id,
                                               const double lower_bound,
                                               const double upper_bound,
                                               const absl::string_view name) {
  CHECK_GE(id, next_linear_constraint_id_);
  next_linear_constraint_id_ = id + LinearConstraintId(1);

  LinearConstraintData& lin_con_data = linear_constraints_[id];
  lin_con_data.lower_bound = lower_bound;
  lin_con_data.upper_bound = upper_bound;
  lin_con_data.name = std::string(name);
  if (!lazy_matrix_rows_.empty()) {
    gtl::InsertOrDie(&lazy_matrix_rows_, id, {});
  }
}

void ModelStorage::AddLinearConstraints(
    const LinearConstraintsProto& linear_constraints) {
  const bool has_names = !linear_constraints.names().empty();
  for (int c = 0; c < linear_constraints.ids_size(); ++c) {
    // This call is valid since ids are unique and increasing.
    AddLinearConstraintInternal(
        LinearConstraintId(linear_constraints.ids(c)),
        /*lower_bound=*/linear_constraints.lower_bounds(c),
        /*upper_bound=*/linear_constraints.upper_bounds(c),
        has_names ? linear_constraints.names(c) : absl::string_view());
  }
}

void ModelStorage::DeleteLinearConstraint(const LinearConstraintId id) {
  CHECK(linear_constraints_.contains(id));
  EnsureLazyMatrixColumns();
  EnsureLazyMatrixRows();
  linear_constraints_.erase(id);
  if (id < linear_constraints_checkpoint_) {
    dirty_linear_constraint_deletes_.insert(id);
    dirty_linear_constraint_lower_bounds_.erase(id);
    dirty_linear_constraint_upper_bounds_.erase(id);
  }
  for (const VariableId related_variable : lazy_matrix_rows_.at(id)) {
    CHECK_GT(lazy_matrix_columns_.at(related_variable).erase(id), 0);
    CHECK_GT(linear_constraint_matrix_.erase({id, related_variable}), 0);
    if (id < linear_constraints_checkpoint_ &&
        related_variable < variables_checkpoint_) {
      dirty_linear_constraint_matrix_keys_.erase({id, related_variable});
    }
  }
  CHECK_GT(lazy_matrix_rows_.erase(id), 0);
}

std::vector<LinearConstraintId> ModelStorage::linear_constraints() const {
  return MapKeys(linear_constraints_);
}

std::vector<LinearConstraintId> ModelStorage::SortedLinearConstraints() const {
  return SortedMapKeys(linear_constraints_);
}

std::vector<VariableId> ModelStorage::SortedLinearObjectiveNonzeroVariables()
    const {
  return SortedMapKeys(linear_objective_);
}

void ModelStorage::AppendVariable(const VariableId id,
                                  VariablesProto& variables_proto) const {
  const VariableData& var_data = variables_.at(id);
  variables_proto.add_ids(id.value());
  variables_proto.add_lower_bounds(var_data.lower_bound);
  variables_proto.add_upper_bounds(var_data.upper_bound);
  variables_proto.add_integers(var_data.is_integer);
  variables_proto.add_names(var_data.name);
}

void ModelStorage::AppendLinearConstraint(
    const LinearConstraintId id,
    LinearConstraintsProto& linear_constraints_proto) const {
  const LinearConstraintData& con_impl = linear_constraints_.at(id);
  linear_constraints_proto.add_ids(id.value());
  linear_constraints_proto.add_lower_bounds(con_impl.lower_bound);
  linear_constraints_proto.add_upper_bounds(con_impl.upper_bound);
  linear_constraints_proto.add_names(con_impl.name);
}

ModelProto ModelStorage::ExportModel() const {
  ModelProto result;
  result.set_name(name_);
  // Export the variables.
  for (const VariableId variable : SortedMapKeys(variables_)) {
    AppendVariable(variable, *result.mutable_variables());
  }

  // Pull out the objective.
  result.mutable_objective()->set_maximize(is_maximize_);
  result.mutable_objective()->set_offset(objective_offset_);
  AppendFromMapOrDefault<VariableId>(
      SortedMapKeys(linear_objective_), linear_objective_,
      *result.mutable_objective()->mutable_linear_coefficients());
  *result.mutable_objective()->mutable_quadratic_coefficients() =
      ExportMatrix(quadratic_objective_, SortedMapKeys(quadratic_objective_));

  // Pull out the linear constraints.
  for (const LinearConstraintId con : SortedMapKeys(linear_constraints_)) {
    AppendLinearConstraint(con, *result.mutable_linear_constraints());
  }

  // Pull out the constraint matrix.
  *result.mutable_linear_constraint_matrix() =
      ExportMatrix<LinearConstraintId, VariableId>(
          linear_constraint_matrix_, SortedMapKeys(linear_constraint_matrix_));
  return result;
}

std::optional<ModelUpdateProto> ModelStorage::ExportSharedModelUpdate() {
  // We must detect the empty case to prevent unneeded copies and merging in
  // ExportModelUpdate().
  if (variables_checkpoint_ == next_variable_id_ &&
      linear_constraints_checkpoint_ == next_linear_constraint_id_ &&
      !dirty_objective_direction_ && !dirty_objective_offset_ &&
      dirty_variable_deletes_.empty() && dirty_variable_lower_bounds_.empty() &&
      dirty_variable_upper_bounds_.empty() &&
      dirty_variable_is_integer_.empty() &&
      dirty_linear_objective_coefficients_.empty() &&
      dirty_quadratic_objective_coefficients_.empty() &&
      dirty_linear_constraint_deletes_.empty() &&
      dirty_linear_constraint_lower_bounds_.empty() &&
      dirty_linear_constraint_upper_bounds_.empty() &&
      dirty_linear_constraint_matrix_keys_.empty()) {
    return std::nullopt;
  }

  // TODO(b/185608026): these are used to efficiently extract the constraint
  // matrix update, but it would be good to avoid calling these because they
  // result in a large allocation.
  EnsureLazyMatrixRows();
  EnsureLazyMatrixColumns();

  ModelUpdateProto result;

  // Variable/constraint deletions.
  for (const VariableId del_var : SortedSetKeys(dirty_variable_deletes_)) {
    result.add_deleted_variable_ids(del_var.value());
  }
  for (const LinearConstraintId del_lin_con :
       SortedSetKeys(dirty_linear_constraint_deletes_)) {
    result.add_deleted_linear_constraint_ids(del_lin_con.value());
  }

  // Update the variables.
  auto var_updates = result.mutable_variable_updates();
  AppendFromMap(dirty_variable_lower_bounds_, variables_,
                &VariableData::lower_bound,
                *var_updates->mutable_lower_bounds());
  AppendFromMap(dirty_variable_upper_bounds_, variables_,
                &VariableData::upper_bound,
                *var_updates->mutable_upper_bounds());

  for (const VariableId integer_var :
       SortedSetKeys(dirty_variable_is_integer_)) {
    var_updates->mutable_integers()->add_ids(integer_var.value());
    var_updates->mutable_integers()->add_values(
        variables_.at(integer_var).is_integer);
  }
  for (VariableId new_id = variables_checkpoint_; new_id < next_variable_id_;
       ++new_id) {
    if (variables_.contains(new_id)) {
      AppendVariable(new_id, *result.mutable_new_variables());
    }
  }

  // Update the objective
  auto obj_updates = result.mutable_objective_updates();
  if (dirty_objective_direction_) {
    obj_updates->set_direction_update(is_maximize_);
  }
  if (dirty_objective_offset_) {
    obj_updates->set_offset_update(objective_offset_);
  }
  AppendFromMapOrDefault<VariableId>(
      SortedSetKeys(dirty_linear_objective_coefficients_), linear_objective_,
      *obj_updates->mutable_linear_coefficients());
  // TODO(b/182567749): Once StrongInt is in absl, use
  // AppendFromMapIfPresent<VariableId>(
  //      MakeStrongIntRange(variables_checkpoint_, next_variable_id_),
  //      linear_objective_, *obj_updates->mutable_linear_coefficients());
  for (VariableId var_id = variables_checkpoint_; var_id < next_variable_id_;
       ++var_id) {
    const double* const double_value =
        gtl::FindOrNull(linear_objective_, var_id);
    if (double_value != nullptr) {
      obj_updates->mutable_linear_coefficients()->add_ids(var_id.value());
      obj_updates->mutable_linear_coefficients()->add_values(*double_value);
    }
  }
  // If we do not have any quadratic updates to push, we would like to avoid
  // initializing the lazy data structures. The updates might tracked in:
  //   1. dirty_quadratic_objective_coefficients_ (both variables old)
  //   2. quadratic_objective_ (at least one new variable)
  // If both maps are empty, we can skip the update and initializiation.
  if (!quadratic_objective_.empty() ||
      !dirty_quadratic_objective_coefficients_.empty()) {
    EnsureLazyQuadraticObjective();
    // NOTE: dirty_quadratic_objective_coefficients_ only tracks terms where
    // both variables are "old".
    std::vector<std::pair<VariableId, VariableId>> quadratic_objective_updates(
        dirty_quadratic_objective_coefficients_.begin(),
        dirty_quadratic_objective_coefficients_.end());
    // Now, we loop through the "new" variables and track updates involving
    // them. We need to look out for two things:
    //   * The "other" variable in the term can either be new or old.
    //   * We cannot doubly insert terms when both variables are new.
    // Note that this traversal is doing at most twice as much work as
    // necessary.
    for (VariableId new_var = variables_checkpoint_;
         new_var < next_variable_id_; ++new_var) {
      if (variables_.contains(new_var)) {
        for (const VariableId other_var :
             lazy_quadratic_objective_by_variable_.at(new_var)) {
          if (other_var <= new_var) {
            quadratic_objective_updates.push_back(
                internal::MakeOrderedPair(new_var, other_var));
          }
        }
      }
    }
    std::sort(quadratic_objective_updates.begin(),
              quadratic_objective_updates.end());
    *result.mutable_objective_updates()->mutable_quadratic_coefficients() =
        ExportMatrix(quadratic_objective_, quadratic_objective_updates);
  }

  // Update the linear constraints
  auto lin_con_updates = result.mutable_linear_constraint_updates();
  AppendFromMap(dirty_linear_constraint_lower_bounds_, linear_constraints_,
                &LinearConstraintData::lower_bound,
                *lin_con_updates->mutable_lower_bounds());
  AppendFromMap(dirty_linear_constraint_upper_bounds_, linear_constraints_,
                &LinearConstraintData::upper_bound,
                *lin_con_updates->mutable_upper_bounds());

  for (LinearConstraintId new_id = linear_constraints_checkpoint_;
       new_id < next_linear_constraint_id_; ++new_id) {
    if (linear_constraints_.contains(new_id)) {
      AppendLinearConstraint(new_id, *result.mutable_new_linear_constraints());
    }
  }

  // Extract changes to the matrix of linear constraint coefficients
  std::vector<std::pair<LinearConstraintId, VariableId>>
      constraint_matrix_updates(dirty_linear_constraint_matrix_keys_.begin(),
                                dirty_linear_constraint_matrix_keys_.end());
  for (VariableId new_var = variables_checkpoint_; new_var < next_variable_id_;
       ++new_var) {
    if (variables_.contains(new_var)) {
      for (const LinearConstraintId lin_con :
           lazy_matrix_columns_.at(new_var)) {
        constraint_matrix_updates.emplace_back(lin_con, new_var);
      }
    }
  }
  for (LinearConstraintId new_lin_con = linear_constraints_checkpoint_;
       new_lin_con < next_linear_constraint_id_; ++new_lin_con) {
    if (linear_constraints_.contains(new_lin_con)) {
      for (const VariableId var : lazy_matrix_rows_.at(new_lin_con)) {
        // NOTE(user): we will do at most twice as much as needed here.
        if (var < variables_checkpoint_) {
          constraint_matrix_updates.emplace_back(new_lin_con, var);
        }
      }
    }
  }
  std::sort(constraint_matrix_updates.begin(), constraint_matrix_updates.end());
  *result.mutable_linear_constraint_matrix_updates() =
      ExportMatrix(linear_constraint_matrix_, constraint_matrix_updates);

  // Named returned value optimization (NRVO) does not apply here since the
  // return type if not the same type as `result`. To make things clear, we
  // explicitly call the constructor here.
  return {std::move(result)};
}

void ModelStorage::EnsureLazyMatrixColumns() {
  if (lazy_matrix_columns_.empty()) {
    for (const auto& var_pair : variables_) {
      lazy_matrix_columns_.insert({var_pair.first, {}});
    }
    for (const auto& mat_entry : linear_constraint_matrix_) {
      lazy_matrix_columns_.at(mat_entry.first.second)
          .insert(mat_entry.first.first);
    }
  }
}

void ModelStorage::EnsureLazyMatrixRows() {
  if (lazy_matrix_rows_.empty()) {
    for (const auto& lin_con_pair : linear_constraints_) {
      lazy_matrix_rows_.insert({lin_con_pair.first, {}});
    }
    for (const auto& mat_entry : linear_constraint_matrix_) {
      lazy_matrix_rows_.at(mat_entry.first.first)
          .insert(mat_entry.first.second);
    }
  }
}

void ModelStorage::EnsureLazyQuadraticObjective() {
  if (lazy_quadratic_objective_by_variable_.empty()) {
    for (const auto& [var, data] : variables_) {
      lazy_quadratic_objective_by_variable_.insert({var, {}});
    }
    for (const auto& [vars, coeff] : quadratic_objective_) {
      lazy_quadratic_objective_by_variable_.at(vars.first).insert(vars.second);
      lazy_quadratic_objective_by_variable_.at(vars.second).insert(vars.first);
    }
    for (const auto& vars : dirty_quadratic_objective_coefficients_) {
      lazy_quadratic_objective_by_variable_.at(vars.first).insert(vars.second);
      lazy_quadratic_objective_by_variable_.at(vars.second).insert(vars.first);
    }
  }
}

void ModelStorage::SharedCheckpoint() {
  variables_checkpoint_ = next_variable_id_;
  linear_constraints_checkpoint_ = next_linear_constraint_id_;
  dirty_objective_direction_ = false;
  dirty_objective_offset_ = false;

  dirty_variable_deletes_.clear();
  dirty_variable_lower_bounds_.clear();
  dirty_variable_upper_bounds_.clear();
  dirty_variable_is_integer_.clear();

  dirty_linear_objective_coefficients_.clear();
  dirty_quadratic_objective_coefficients_.clear();

  dirty_linear_constraint_deletes_.clear();
  dirty_linear_constraint_lower_bounds_.clear();
  dirty_linear_constraint_upper_bounds_.clear();
  dirty_linear_constraint_matrix_keys_.clear();
}

UpdateTrackerId ModelStorage::NewUpdateTracker() {
  const absl::MutexLock lock(&update_trackers_lock_);

  const UpdateTrackerId update_tracker = next_update_tracker_;
  ++next_update_tracker_;

  CHECK(update_trackers_
            .try_emplace(update_tracker, std::make_unique<UpdateTrackerData>())
            .second);

  CheckpointLocked(update_tracker);

  return update_tracker;
}

void ModelStorage::DeleteUpdateTracker(const UpdateTrackerId update_tracker) {
  const absl::MutexLock lock(&update_trackers_lock_);
  const auto found = update_trackers_.find(update_tracker);
  CHECK(found != update_trackers_.end())
      << "Update tracker " << update_tracker << " does not exist";
  update_trackers_.erase(found);
}

std::optional<ModelUpdateProto> ModelStorage::ExportModelUpdate(
    const UpdateTrackerId update_tracker) {
  const absl::MutexLock lock(&update_trackers_lock_);

  const auto found_data = update_trackers_.find(update_tracker);
  CHECK(found_data != update_trackers_.end())
      << "Update tracker " << update_tracker << " does not exist";
  const std::unique_ptr<UpdateTrackerData>& data = found_data->second;

  // No updates have been pushed, the checkpoint of this tracker is in sync with
  // the shared checkpoint of ModelStorage. We can return the ModelStorage
  // shared update without merging.
  if (data->updates.empty()) {
    return ExportSharedModelUpdate();
  }

  // Find all trackers with the same checkpoint. By construction, all trackers
  // that have the same first update also share all next updates.
  std::vector<UpdateTrackerData*> all_trackers_at_checkpoint;
  for (const auto& [other_id, other_data] : update_trackers_) {
    if (!other_data->updates.empty() &&
        other_data->updates.front() == data->updates.front()) {
      all_trackers_at_checkpoint.push_back(other_data.get());

      // Validate that we have the same updates in debug mode only. In optimized
      // mode, only test the size of the updates vectors.
      CHECK_EQ(data->updates.size(), other_data->updates.size());
      if (DEBUG_MODE) {
        for (int i = 0; i < data->updates.size(); ++i) {
          CHECK_EQ(data->updates[i], other_data->updates[i])
              << "Two trackers have the same checkpoint but different updates.";
        }
      }
    }
  }

  // Possible optimizations here:
  //
  // * Maybe optimize the case where the first update is singly used by `this`
  //   and use it as starting point instead of making a copy. This may be more
  //   complicated if it is shared with multiple trackers since in that case we
  //   must make sure to only update the shared instance if and only if only
  //   trackers have a pointer to it, not external code (i.e. its use count is
  //   the same as the number of trackers).
  //
  // * Use n-way merge here if the performances justify it.
  const auto merge = std::make_shared<ModelUpdateProto>();
  for (const auto& update : data->updates) {
    MergeIntoUpdate(/*from_new=*/*update, /*into_old=*/*merge);
  }

  // Push the merge to all trackers that have the same checkpoint (including
  // this tracker).
  for (UpdateTrackerData* const other_data : all_trackers_at_checkpoint) {
    other_data->updates.clear();
    other_data->updates.push_back(merge);
  }

  ModelUpdateProto update = *merge;
  const std::optional<ModelUpdateProto> pending_update =
      ExportSharedModelUpdate();
  if (pending_update) {
    MergeIntoUpdate(/*from_new=*/*pending_update, /*into_old=*/update);
  }

  // Named returned value optimization (NRVO) does not apply here since the
  // return type if not the same type as `result`. To make things clear, we
  // explicitly call the constructor here.
  return {std::move(update)};
}

void ModelStorage::Checkpoint(const UpdateTrackerId update_tracker) {
  const absl::MutexLock lock(&update_trackers_lock_);

  CheckpointLocked(update_tracker);
}

void ModelStorage::CheckpointLocked(const UpdateTrackerId update_tracker) {
  const auto found_data = update_trackers_.find(update_tracker);
  CHECK(found_data != update_trackers_.end())
      << "Update tracker " << update_tracker << " does not exist";
  const std::unique_ptr<UpdateTrackerData>& data = found_data->second;

  // Optimize the case where we have a single tracker and we don't want to
  // update it. In that case we don't need to update trackers since we would
  // only update this one and clear it immediately.
  if (update_trackers_.size() > 1) {
    std::optional<ModelUpdateProto> update = ExportSharedModelUpdate();
    if (update) {
      const auto shared_update =
          std::make_shared<ModelUpdateProto>(*std::move(update));

      for (const auto& [other_id, other_data] : update_trackers_) {
        other_data->updates.push_back(shared_update);
      }
    }
  }
  SharedCheckpoint();
  data->updates.clear();
}

absl::Status ModelStorage::ApplyUpdateProto(
    const ModelUpdateProto& update_proto) {
  // Check the update first.
  {
    ModelSummary summary;
    // We have to use sorted keys since IdNameBiMap expect Insert() to be called
    // in sorted order.
    for (const auto id : SortedVariables()) {
      summary.variables.Insert(id.value(), variable_name(id));
    }
    summary.variables.SetNextFreeId(next_variable_id_.value());
    for (const auto id : SortedLinearConstraints()) {
      summary.linear_constraints.Insert(id.value(), linear_constraint_name(id));
    }
    summary.linear_constraints.SetNextFreeId(
        next_linear_constraint_id_.value());
    // We don't check the names for the same reason as in FromModelProto().
    RETURN_IF_ERROR(ValidateModelUpdateAndSummary(update_proto, summary,
                                                  /*check_names=*/false));
  }

  // Remove deleted variables and constraints.
  for (const int64_t v_id : update_proto.deleted_variable_ids()) {
    DeleteVariable(VariableId(v_id));
  }
  for (const int64_t c_id : update_proto.deleted_linear_constraint_ids()) {
    DeleteLinearConstraint(LinearConstraintId(c_id));
  }

  // Update existing variables' properties.
  for (const auto [v_id, lb] :
       MakeView(update_proto.variable_updates().lower_bounds())) {
    set_variable_lower_bound(VariableId(v_id), lb);
  }
  for (const auto [v_id, ub] :
       MakeView(update_proto.variable_updates().upper_bounds())) {
    set_variable_upper_bound(VariableId(v_id), ub);
  }
  for (const auto [v_id, is_integer] :
       MakeView(update_proto.variable_updates().integers())) {
    set_variable_is_integer(VariableId(v_id), is_integer);
  }

  // Update existing constraints' properties.
  for (const auto [c_id, lb] :
       MakeView(update_proto.linear_constraint_updates().lower_bounds())) {
    set_linear_constraint_lower_bound(LinearConstraintId(c_id), lb);
  }
  for (const auto [c_id, ub] :
       MakeView(update_proto.linear_constraint_updates().upper_bounds())) {
    set_linear_constraint_upper_bound(LinearConstraintId(c_id), ub);
  }

  // Add the new variables and constraints.
  AddVariables(update_proto.new_variables());
  AddLinearConstraints(update_proto.new_linear_constraints());

  // Update the objective.
  if (update_proto.objective_updates().has_direction_update()) {
    set_is_maximize(update_proto.objective_updates().direction_update());
  }
  if (update_proto.objective_updates().has_offset_update()) {
    set_objective_offset(update_proto.objective_updates().offset_update());
  }
  UpdateLinearObjectiveCoefficients(
      update_proto.objective_updates().linear_coefficients());
  UpdateQuadraticObjectiveCoefficients(
      update_proto.objective_updates().quadratic_coefficients());

  // Update the linear constraints' coefficients.
  UpdateLinearConstraintCoefficients(
      update_proto.linear_constraint_matrix_updates());

  return absl::OkStatus();
}

}  // namespace math_opt
}  // namespace operations_research
