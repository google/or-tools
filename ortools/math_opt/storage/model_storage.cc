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

#include "ortools/math_opt/storage/model_storage.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/base/map_util.h"
#include "ortools/base/status_macros.h"
#include "ortools/base/strong_int.h"
#include "ortools/math_opt/core/model_summary.h"
#include "ortools/math_opt/core/sparse_vector_view.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/storage/model_storage_types.h"
#include "ortools/math_opt/storage/sparse_matrix.h"
#include "ortools/math_opt/storage/update_trackers.h"
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
  RETURN_IF_ERROR(ValidateModel(model_proto, /*check_names=*/false).status());

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

std::unique_ptr<ModelStorage> ModelStorage::Clone(
    const std::optional<absl::string_view> new_name) const {
  ModelProto model_proto = ExportModel();
  if (new_name.has_value()) {
    model_proto.set_name(std::string(*new_name));
  }
  absl::StatusOr<std::unique_ptr<ModelStorage>> clone =
      ModelStorage::FromModelProto(model_proto);
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
  gtl::InsertOrDie(&matrix_columns_, id, {});
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
  linear_objective_.erase(id);
  for (const auto& [_, update_tracker] :
       update_trackers_.GetUpdatedTrackers()) {
    if (id < update_tracker->variables_checkpoint) {
      update_tracker->dirty_variable_deletes.insert(id);
      update_tracker->dirty_variable_lower_bounds.erase(id);
      update_tracker->dirty_variable_upper_bounds.erase(id);
      update_tracker->dirty_variable_is_integer.erase(id);
      update_tracker->dirty_linear_objective_coefficients.erase(id);
      for (const VariableId related :
           quadratic_objective_.RelatedVariables(id)) {
        // We can only have a dirty update to wipe clean if both variables are
        // old
        if (related < update_tracker->variables_checkpoint) {
          update_tracker->dirty_quadratic_objective_coefficients.erase(
              internal::MakeOrderedPair(id, related));
        }
      }
    }
  }

  quadratic_objective_.Delete(id);
  for (const LinearConstraintId related_constraint : matrix_columns_.at(id)) {
    CHECK_GT(matrix_rows_.at(related_constraint).erase(id), 0);
    CHECK_GT(linear_constraint_matrix_.erase({related_constraint, id}), 0);
  }
  for (const auto& [_, update_tracker] :
       update_trackers_.GetUpdatedTrackers()) {
    for (const LinearConstraintId related_constraint : matrix_columns_.at(id)) {
      if (id < update_tracker->variables_checkpoint &&
          related_constraint < update_tracker->linear_constraints_checkpoint) {
        update_tracker->dirty_linear_constraint_matrix_keys.erase(
            {related_constraint, id});
      }
    }
  }
  CHECK_GT(matrix_columns_.erase(id), 0);
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
  gtl::InsertOrDie(&matrix_rows_, id, {});
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
  linear_constraints_.erase(id);
  for (const auto& [_, update_tracker] :
       update_trackers_.GetUpdatedTrackers()) {
    if (id < update_tracker->linear_constraints_checkpoint) {
      update_tracker->dirty_linear_constraint_deletes.insert(id);
      update_tracker->dirty_linear_constraint_lower_bounds.erase(id);
      update_tracker->dirty_linear_constraint_upper_bounds.erase(id);
    }
  }
  for (const VariableId related_variable : matrix_rows_.at(id)) {
    CHECK_GT(matrix_columns_.at(related_variable).erase(id), 0);
    CHECK_GT(linear_constraint_matrix_.erase({id, related_variable}), 0);
  }
  for (const auto& [_, update_tracker] :
       update_trackers_.GetUpdatedTrackers()) {
    for (const VariableId related_variable : matrix_rows_.at(id)) {
      if (id < update_tracker->linear_constraints_checkpoint &&
          related_variable < update_tracker->variables_checkpoint) {
        update_tracker->dirty_linear_constraint_matrix_keys.erase(
            {id, related_variable});
      }
    }
  }
  CHECK_GT(matrix_rows_.erase(id), 0);
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
      quadratic_objective_.Proto();

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

std::optional<ModelUpdateProto>
ModelStorage::UpdateTrackerData::ExportModelUpdate(
    const ModelStorage& storage) const {
  // We must detect the empty case to prevent unneeded copies and merging in
  // ExportModelUpdate().
  if (variables_checkpoint == storage.next_variable_id_ &&
      linear_constraints_checkpoint == storage.next_linear_constraint_id_ &&
      !dirty_objective_direction && !dirty_objective_offset &&
      dirty_variable_deletes.empty() && dirty_variable_lower_bounds.empty() &&
      dirty_variable_upper_bounds.empty() &&
      dirty_variable_is_integer.empty() &&
      dirty_linear_objective_coefficients.empty() &&
      dirty_quadratic_objective_coefficients.empty() &&
      dirty_linear_constraint_deletes.empty() &&
      dirty_linear_constraint_lower_bounds.empty() &&
      dirty_linear_constraint_upper_bounds.empty() &&
      dirty_linear_constraint_matrix_keys.empty()) {
    return std::nullopt;
  }

  ModelUpdateProto result;

  // Variable/constraint deletions.
  for (const VariableId del_var : SortedSetKeys(dirty_variable_deletes)) {
    result.add_deleted_variable_ids(del_var.value());
  }
  for (const LinearConstraintId del_lin_con :
       SortedSetKeys(dirty_linear_constraint_deletes)) {
    result.add_deleted_linear_constraint_ids(del_lin_con.value());
  }

  // Update the variables.
  auto var_updates = result.mutable_variable_updates();
  AppendFromMap(dirty_variable_lower_bounds, storage.variables_,
                &VariableData::lower_bound,
                *var_updates->mutable_lower_bounds());
  AppendFromMap(dirty_variable_upper_bounds, storage.variables_,
                &VariableData::upper_bound,
                *var_updates->mutable_upper_bounds());

  for (const VariableId integer_var :
       SortedSetKeys(dirty_variable_is_integer)) {
    var_updates->mutable_integers()->add_ids(integer_var.value());
    var_updates->mutable_integers()->add_values(
        storage.variables_.at(integer_var).is_integer);
  }
  for (VariableId new_id = variables_checkpoint;
       new_id < storage.next_variable_id_; ++new_id) {
    if (storage.variables_.contains(new_id)) {
      storage.AppendVariable(new_id, *result.mutable_new_variables());
    }
  }

  // Update the objective
  auto obj_updates = result.mutable_objective_updates();
  if (dirty_objective_direction) {
    obj_updates->set_direction_update(storage.is_maximize_);
  }
  if (dirty_objective_offset) {
    obj_updates->set_offset_update(storage.objective_offset_);
  }
  AppendFromMapOrDefault<VariableId>(
      SortedSetKeys(dirty_linear_objective_coefficients),
      storage.linear_objective_, *obj_updates->mutable_linear_coefficients());
  // TODO(b/182567749): Once StrongInt is in absl, use
  // AppendFromMapIfPresent<VariableId>(
  //      MakeStrongIntRange(variables_checkpoint, next_variable_id_),
  //      linear_objective_, *obj_updates->mutable_linear_coefficients());
  for (VariableId var_id = variables_checkpoint;
       var_id < storage.next_variable_id_; ++var_id) {
    const double* const double_value =
        gtl::FindOrNull(storage.linear_objective_, var_id);
    if (double_value != nullptr) {
      obj_updates->mutable_linear_coefficients()->add_ids(var_id.value());
      obj_updates->mutable_linear_coefficients()->add_values(*double_value);
    }
  }

  *result.mutable_objective_updates()->mutable_quadratic_coefficients() =
      storage.quadratic_objective_.Update(
          storage.variables_, variables_checkpoint, storage.next_variable_id_,
          dirty_quadratic_objective_coefficients);

  // Update the linear constraints
  auto lin_con_updates = result.mutable_linear_constraint_updates();
  AppendFromMap(dirty_linear_constraint_lower_bounds,
                storage.linear_constraints_, &LinearConstraintData::lower_bound,
                *lin_con_updates->mutable_lower_bounds());
  AppendFromMap(dirty_linear_constraint_upper_bounds,
                storage.linear_constraints_, &LinearConstraintData::upper_bound,
                *lin_con_updates->mutable_upper_bounds());

  for (LinearConstraintId new_id = linear_constraints_checkpoint;
       new_id < storage.next_linear_constraint_id_; ++new_id) {
    if (storage.linear_constraints_.contains(new_id)) {
      storage.AppendLinearConstraint(new_id,
                                     *result.mutable_new_linear_constraints());
    }
  }

  // Extract changes to the matrix of linear constraint coefficients
  std::vector<std::pair<LinearConstraintId, VariableId>>
      constraint_matrix_updates(dirty_linear_constraint_matrix_keys.begin(),
                                dirty_linear_constraint_matrix_keys.end());
  for (VariableId new_var = variables_checkpoint;
       new_var < storage.next_variable_id_; ++new_var) {
    if (storage.variables_.contains(new_var)) {
      for (const LinearConstraintId lin_con :
           storage.matrix_columns_.at(new_var)) {
        constraint_matrix_updates.emplace_back(lin_con, new_var);
      }
    }
  }
  for (LinearConstraintId new_lin_con = linear_constraints_checkpoint;
       new_lin_con < storage.next_linear_constraint_id_; ++new_lin_con) {
    if (storage.linear_constraints_.contains(new_lin_con)) {
      for (const VariableId var : storage.matrix_rows_.at(new_lin_con)) {
        // NOTE(user): we will do at most twice as much as needed here.
        if (var < variables_checkpoint) {
          constraint_matrix_updates.emplace_back(new_lin_con, var);
        }
      }
    }
  }
  std::sort(constraint_matrix_updates.begin(), constraint_matrix_updates.end());
  *result.mutable_linear_constraint_matrix_updates() = ExportMatrix(
      storage.linear_constraint_matrix_, constraint_matrix_updates);

  // Named returned value optimization (NRVO) does not apply here since the
  // return type if not the same type as `result`. To make things clear, we
  // explicitly call the constructor here.
  return {std::move(result)};
}

void ModelStorage::UpdateTrackerData::Checkpoint(const ModelStorage& storage) {
  variables_checkpoint = storage.next_variable_id_;
  linear_constraints_checkpoint = storage.next_linear_constraint_id_;
  dirty_objective_direction = false;
  dirty_objective_offset = false;

  dirty_variable_deletes.clear();
  dirty_variable_lower_bounds.clear();
  dirty_variable_upper_bounds.clear();
  dirty_variable_is_integer.clear();

  dirty_linear_objective_coefficients.clear();
  dirty_quadratic_objective_coefficients.clear();

  dirty_linear_constraint_deletes.clear();
  dirty_linear_constraint_lower_bounds.clear();
  dirty_linear_constraint_upper_bounds.clear();
  dirty_linear_constraint_matrix_keys.clear();
}

UpdateTrackerId ModelStorage::NewUpdateTracker() {
  return update_trackers_.NewUpdateTracker(
      /*variables_checkpoint=*/next_variable_id_,
      /*linear_constraints_checkpoint=*/next_linear_constraint_id_);
}

void ModelStorage::DeleteUpdateTracker(const UpdateTrackerId update_tracker) {
  update_trackers_.DeleteUpdateTracker(update_tracker);
}

std::optional<ModelUpdateProto> ModelStorage::ExportModelUpdate(
    const UpdateTrackerId update_tracker) const {
  return update_trackers_.GetData(update_tracker).ExportModelUpdate(*this);
}

void ModelStorage::Checkpoint(const UpdateTrackerId update_tracker) {
  update_trackers_.GetData(update_tracker).Checkpoint(*this);
}

absl::Status ModelStorage::ApplyUpdateProto(
    const ModelUpdateProto& update_proto) {
  // Check the update first.
  {
    // Do not check for duplicate names, as with FromModelProto();
    ModelSummary summary(/*check_names=*/false);
    // IdNameBiMap requires Insert() calls to be in sorted id order.
    for (const auto id : SortedVariables()) {
      RETURN_IF_ERROR(summary.variables.Insert(id.value(), variable_name(id)))
          << "invalid variable id in model";
    }
    RETURN_IF_ERROR(summary.variables.SetNextFreeId(next_variable_id_.value()));
    for (const auto id : SortedLinearConstraints()) {
      RETURN_IF_ERROR(summary.linear_constraints.Insert(
          id.value(), linear_constraint_name(id)))
          << "invalid linear constraint id in model";
    }
    RETURN_IF_ERROR(summary.linear_constraints.SetNextFreeId(
        next_linear_constraint_id_.value()));
    RETURN_IF_ERROR(ValidateModelUpdate(update_proto, summary))
        << "update not valid";
    // TODO(b/227217735): Remove if block when ModelStorage supports quadratic
    //                    constraints.
    if (update_proto.has_quadratic_constraint_updates()) {
      return absl::UnimplementedError(
          "quadratic constraint updates are not currently supported");
    }
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
